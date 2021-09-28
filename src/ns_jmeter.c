#include <stdio.h>
#include "util.h"
#include "ns_trace_level.h"
#include "ns_log.h"
#include "ns_parse_scen_conf.h"
#include "nslb_util.h"
#include "ns_global_settings.h"
#include "ns_common.h"
#include "ns_msg_def.h"
#include "ns_data_types.h"
#include "netstorm.h"
#include "ns_vuser_tasks.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "ns_data_types.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "netomni/src/core/ni_script_parse.h"
#include "netomni/src/core/ni_user_distribution.h"
#include <libgen.h>
#include "ns_child_msg_com.h"
#include "nslb_sock.h"
#include "output.h"
#include "ns_trans.h"
#include "ns_http_status_codes.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_percentile.h"
#include "ns_network_cache_reporting.h"
#include "logging.h"
#include "ns_url_hash.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_jmeter.h"

#define JMETER_MAX_LINE_SIZE              32000
#define INIT_NORM_PAGE_TABLE_SIZE         1000 
#define DELTA_NORM_PAGE_TABLE_SIZE        1000
//For Tx record 
#define INIT_TX_ENTRY_SIZE                100
#define DELTA_TX_ENTRY_SIZE               10

#define JMETER_TO_NORM_PAGE_ID(pg_idx) {\
  pg_idx = (child_idx<<16) + pg_idx; \
  NSDL4_LOGGING(NULL, NULL, "Normalized page_id = 0x%x",pg_idx); \
}

#define JM_LOG_LEVEL_FOR_DRILL_DOWN_REPORT       \
                                          (log_records &&                                                             \
                                              (runprof_table_shr_mem[sgrp_id].gset.report_level >= 2) &&        \
                                              (run_mode == NORMAL_RUN))
#if 0
int g_jmeter_avgtime_idx = -1;
jmeter_avgtime *g_jmeter_avgtime = NULL;
#endif

static JMeterVuser *jmeter_vuser;
static long g_ts_init_time_in_ms;// Test start timestamp in ms

// jmeter csv and vusers split settings
#define MAX_JMETER_OPT_ARGS 50

// Get Elapsed time for jmeter - to store for different records 
#define CONVERT_ABS_TO_ELAPSED(elapsed_time_ms,abs_time_in_ms)\
	elapsed_time_ms = abs_time_in_ms - g_ts_init_time_in_ms

jmeter_vusers_csv_settings g_jmeter_vuser_csv;

int g_total_jmeter_entries;
int g_jmeter_ports[MAX_FILE_NAME];

static unsigned char url_record_num     =  URL_RECORD;
static const short zero_short = 0;
static const long zero_long = 0;

void jmeter_init()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  MY_MALLOC(g_jmeter_listen_msg_con, total_runprof_entries * sizeof(Msg_com_con), "JMListener", -1);
  MY_MALLOC_AND_MEMSET(jmeter_vuser, total_runprof_entries * sizeof(JMeterVuser), "JMeterVuser", -1);
  NSDL2_MESSAGES(NULL, NULL, "total_runprof_entries = %d, g_jmeter_listen_msg_con = %p, jmeter_vuser = %p", total_runprof_entries, 
                             g_jmeter_listen_msg_con, jmeter_vuser); 
  // Setting start time of ns in milliseconds
  g_ts_init_time_in_ms = g_ts_init_time_in_sec * 1000;
}

void jmeter_per_grp_init(int sgrp_id)
{
  JmeterTxIdMap *tx_map_ptr;
  //NOTE: it is assumption that one Group can have only one JMeter User, 
  // IF more than one user in one group then only one user will work
 // g_jmeter_listen_msg_con[sgrp_id].sgrp_id = sgrp_id; 
  if (runprof_table_shr_mem[sgrp_id].sess_ptr && runprof_table_shr_mem[sgrp_id].sess_ptr->jmeter_sess_name && per_proc_runprof_table[my_port_index * total_runprof_entries + sgrp_id])
    create_jmeter_listen_socket(v_epoll_fd, &g_jmeter_listen_msg_con[sgrp_id], NS_JMETER_DATA_CONN, sgrp_id);
  //intializing 
  MY_MALLOC_AND_MEMSET(tx_map_ptr, sizeof(JmeterTxIdMap), "jmeter_tx_id_mapping", -1);
  g_jmeter_listen_msg_con[sgrp_id].jmeter_tx_id_mapping = tx_map_ptr;
  MY_MALLOC_AND_MEMSET(tx_map_ptr->tx_map, sizeof(int) * INIT_TX_ENTRY_SIZE, "JmeterTxIdMap", -1);
  tx_map_ptr->max_entries = INIT_TX_ENTRY_SIZE;
}

int jmeter_get_running_vusers()
{
  int i; 
  NSDL2_MESSAGES(NULL, NULL, "Method called"); 
  int total_running_vusers = 0;
  for(i =0; i< total_runprof_entries; i++)
  {
    total_running_vusers += jmeter_vuser[i].running_vusers;
  }
  NSDL2_MESSAGES(NULL, NULL, "total_running_vusers =%d", total_running_vusers); 
  return total_running_vusers; 
}

int jmeter_get_running_vusers_by_sgrp(int sgrp_id)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called"); 
  return jmeter_vuser[sgrp_id].running_vusers; 
}

int jmeter_get_active_vusers()
{
  int i; 
  NSDL2_MESSAGES(NULL, NULL, "Method called"); 
  int total_active_vusers = 0;
  for(i =0; i< total_runprof_entries; i++)
  {
    total_active_vusers += jmeter_vuser[i].active_vusers;
  }
  NSDL2_MESSAGES(NULL, NULL, "total_active_vusers =%d", total_active_vusers); 
  return total_active_vusers;
}

int jmeter_get_active_vusers_by_sgrp(int sgrp_id)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called"); 
  return jmeter_vuser[sgrp_id].active_vusers; 
}

void jmeter_set_vuser(int running_vusers, int sgrp_id)
{
  // update running vuser as we get current running vuser from JMeter.
  if (running_vusers >= 0)
  {
    jmeter_vuser[sgrp_id].running_vusers = running_vusers;  
    jmeter_vuser[sgrp_id].active_vusers = running_vusers;
  }
}
 
int kw_set_jmeter_jvm_settings(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[JMETER_MAX_KEYWORD_LEN + 1];
  char group_name[JMETER_MAX_ARGS_LEN + 1];
  char min_heap_size[JMETER_MAX_ARGS_LEN + 1];
  char max_heap_size[JMETER_MAX_ARGS_LEN + 1];
  char optargs[(JMETER_SETTINGS_MAX_OPT_ARGS_LEN * 2) + 1];
  char *buf_ptr;
  char optjavaargs[JMETER_SETTINGS_MAX_OPT_ARGS_LEN + 1]="";

  optargs[0] = 0;

  int num_args = 0;
  
  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group_name, min_heap_size, max_heap_size, optargs);
  if(num_args < 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011282, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(min_heap_size) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011282, CAV_ERR_MSG_2);
  }
  gset->jmeter_gset.min_heap_size = atoi(min_heap_size);

  if(ns_is_numeric(max_heap_size) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011282, CAV_ERR_MSG_2);
  }
  gset->jmeter_gset.max_heap_size = atoi(max_heap_size);

  NSDL1_PARSING(NULL, NULL, "JMETER_JVM_SETTINGS:: group = %s, min heap size = %d, max heap size = %d", 
                             group_name, gset->jmeter_gset.min_heap_size, gset->jmeter_gset.max_heap_size);

  if((gset->jmeter_gset.min_heap_size < 128))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011283, "");
  }

  if((gset->jmeter_gset.min_heap_size) > (gset->jmeter_gset.max_heap_size))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011282, CAV_ERR_MSG_5);
  }

  gset->jmeter_gset.jmeter_java_add_args = -1;
  if(num_args > 4)
  {
    if(!optargs[0])
    {
      NSTL1(NULL, NULL, "G_JMETER_JVM_SETTINGS doesn't contains any additional arguments(%s).. hence returning", optargs);
      return 0;
    }
    else
    {
      //G_JMETER_JVM_SETTINGS <Group> <Min Heap Size in MB> <Max Heap Size in MB> <additional JVM args>
      buf_ptr = strstr(buf, " ");
      buf_ptr++; //Point to group

      buf_ptr = strstr(buf_ptr, " ");
      buf_ptr++; //Point to min_heap_size

      buf_ptr = strstr(buf_ptr, " ");
      buf_ptr++; //Point to max_heap_size

      buf_ptr = strstr(buf_ptr, " ");
      //CLEAR_WHITE_SPACE(buf_ptr); //buf_ptr will point to additional arguments
      CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(buf_ptr);
  
      NSDL4_PARSING(NULL, NULL, "buf_ptr %s", buf_ptr);

      if(strlen(buf_ptr) > JMETER_SETTINGS_MAX_OPT_ARGS_LEN)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_JVM_SETTINGS_USAGE, CAV_ERR_1011373, buf_ptr, strlen(buf_ptr), "");
      }

      char *field[MAX_JMETER_OPT_ARGS];
      int total_flds = get_tokens(buf_ptr, field, " ", MAX_JMETER_OPT_ARGS);
      //optjavargs = xyz abc def ghi
      int javaargslength = 0;
      for(int i=0; i<total_flds; i++)
      {
        javaargslength += snprintf(optjavaargs + javaargslength, JMETER_SETTINGS_MAX_OPT_ARGS_LEN+1, "%s ", field[i]);
      }
      optjavaargs[javaargslength-1]='\0';
      NSDL2_PARSING(NULL, NULL, "JMETER_JVM_SETTINGS Additonal arguments: java: %s", optjavaargs);
      
      gset->jmeter_gset.jmeter_java_add_args = copy_into_big_buf(optjavaargs, 0);     
    }
  } 

  return 0;
}

int kw_set_jmeter_additional_settings(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[JMETER_MAX_KEYWORD_LEN + 1];
  char group_name[JMETER_MAX_ARGS_LEN + 1];
  char jmeter_report[JMETER_MAX_ARGS_LEN + 1];
  char optargs[(JMETER_SETTINGS_MAX_OPT_ARGS_LEN * 2) + 1];
  char *buf_ptr;
  char optjargs[JMETER_SETTINGS_MAX_OPT_ARGS_LEN + 1]="";

  optargs[0] = 0;

  int num_args = 0;
  
  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, jmeter_report, optargs);
  if(num_args < 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_ADD_ARGS_USAGE, CAV_ERR_1011375, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(jmeter_report) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_ADD_ARGS_USAGE, CAV_ERR_1011375, CAV_ERR_MSG_2);
  }

  gset->jmeter_gset.gen_jmeter_report = atoi(jmeter_report);

  NSDL1_PARSING(NULL, NULL, "JMETER_ADD_ARGS:: group = %s, gen_jmeter_report = %d", 
                             group_name, gset->jmeter_gset.gen_jmeter_report);

  if((gset->jmeter_gset.gen_jmeter_report < 0) || (gset->jmeter_gset.gen_jmeter_report > 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_ADD_ARGS_USAGE, CAV_ERR_1011375, "");
  }

  gset->jmeter_gset.jmeter_add_args = -1;

  // If jmeter additional arguments are provied
  if(num_args > 3)
  {
    if(!optargs[0])
    {
      NSTL1(NULL, NULL, "G_JMETER_ADD_ARGS doesn't contains any additional arguments(%s).. hence returning", optargs);
      return 0;
    }
    else
    {
      //G_JMETER_ADD_ARGS <Group> <Generate Jmeter Report>
      buf_ptr = strstr(buf, " ");
      buf_ptr++; //Point to group

      buf_ptr = strstr(buf_ptr, " ");
      buf_ptr++; //Point to generate jmeter report

      buf_ptr = strstr(buf_ptr, " ");
      //CLEAR_WHITE_SPACE(buf_ptr); //buf_ptr will point to additional arguments
      CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(buf_ptr);
  
      NSDL4_PARSING(NULL, NULL, "buf_ptr %s", buf_ptr);

      if(strlen(buf_ptr) > JMETER_SETTINGS_MAX_OPT_ARGS_LEN)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_ADD_ARGS_USAGE, CAV_ERR_1011373, buf_ptr, strlen(buf_ptr), "");
      }

      char *field[MAX_JMETER_OPT_ARGS];
      int total_flds = get_tokens(buf_ptr, field, " ", MAX_JMETER_OPT_ARGS);
      // -JARG1 -JARG2 
      //optargs = -JARG1 -JARG2 -LARG1 -LARG2
      int jargslength = 0;
      for(int i=0; i<total_flds; i++)
      {
        jargslength += snprintf(optjargs + jargslength, JMETER_SETTINGS_MAX_OPT_ARGS_LEN+1, "%s ", field[i]);
      }
      
      optjargs[jargslength-1]='\0';
      NSDL2_PARSING(NULL, NULL, "JMETER_ADD_ARGS Additonal arguments: jmeter: %s", optjargs);
      
      gset->jmeter_gset.jmeter_add_args = copy_into_big_buf(optjargs, 0);
    }
  } 

  return 0;
}

int kw_set_jmeter_vusers_split(char *buf, char *err_msg)
{
  char keyword[JMETER_MAX_KEYWORD_LEN + 1];
  char vusers_split[JMETER_MAX_ARGS_LEN + 1];
  char temp[JMETER_MAX_ARGS_LEN + 1];
  int num_args = 0;

  NIDL(3, "Method Called");

  num_args = sscanf(buf, "%s %s %s", keyword, vusers_split, temp);

  if(num_args != 2)
    NS_KW_PARSING_ERR(buf, 0, err_msg, JMETER_VUSERS_SPLIT_USAGE, CAV_ERR_1011371, CAV_ERR_MSG_1);

  if(ns_is_numeric(vusers_split) == 0) 
    NS_KW_PARSING_ERR(buf, 0, err_msg, JMETER_VUSERS_SPLIT_USAGE, CAV_ERR_1011371, CAV_ERR_MSG_2);

  g_jmeter_vuser_csv.is_vusers_split = atoi(vusers_split);

  if((g_jmeter_vuser_csv.is_vusers_split != 0) && (g_jmeter_vuser_csv.is_vusers_split != 1))
    g_jmeter_vuser_csv.is_vusers_split = 0;

  NIDL(3, "Method exit, jmeter is_jmeter_vusers split mode = %d", g_jmeter_vuser_csv.is_vusers_split);

  return 0;
}


int kw_set_jmeter_csv_files_split_mode(char *buf, char *err_msg)
{
  char keyword[JMETER_MAX_KEYWORD_LEN + 1];
  char tmp_csv_file_split_mode[JMETER_MAX_ARGS_LEN + 1];
  char tmp_csv_split_file_pattern[JMETER_MAX_ARGS_LEN];
  char temp[JMETER_MAX_ARGS_LEN + 1];
  int num_args = 0;

  tmp_csv_split_file_pattern[0]=0;

  num_args = sscanf(buf, "%s %s %s %s", keyword, tmp_csv_file_split_mode, tmp_csv_split_file_pattern, temp);

  if(num_args < 2 && num_args > 3 )
    NS_KW_PARSING_ERR(buf, 0, err_msg, JMETER_CSV_DATA_SET_SPLIT_USAGE, CAV_ERR_1011370, CAV_ERR_MSG_1);

  if(ns_is_numeric(tmp_csv_file_split_mode) == 0)
    NS_KW_PARSING_ERR(buf, 0, err_msg, JMETER_CSV_DATA_SET_SPLIT_USAGE, CAV_ERR_1011370, CAV_ERR_MSG_2);

  g_jmeter_vuser_csv.csv_file_split_mode = atoi(tmp_csv_file_split_mode);

  if((g_jmeter_vuser_csv.csv_file_split_mode < JMETER_CSV_DATA_SET_SPLIT_NO_FILES) ||
     (g_jmeter_vuser_csv.csv_file_split_mode > JMETER_CSV_DATA_SET_SPLIT_FILE_WITH_PATTERN))
  {
    g_jmeter_vuser_csv.csv_file_split_mode = JMETER_CSV_DATA_SET_SPLIT_FILE_WITH_PATTERN;
    NSTL3(NULL, NULL, "Setting Default value of csv file split mode %d", g_jmeter_vuser_csv.csv_file_split_mode);
  }

  if(g_jmeter_vuser_csv.csv_file_split_mode == JMETER_CSV_DATA_SET_SPLIT_FILE_WITH_PATTERN)
  {
     if(!tmp_csv_split_file_pattern[0])
     {
       NS_KW_PARSING_ERR(buf, 0, err_msg, JMETER_CSV_DATA_SET_SPLIT_USAGE, CAV_ERR_1011370, CAV_ERR_MSG_3);
     }
     else
       strcpy(g_jmeter_vuser_csv.csv_file_split_pattern, tmp_csv_split_file_pattern);

     NIDL(3, "jmeter csv_file_split_pattern = %s", g_jmeter_vuser_csv.csv_file_split_pattern);
  }

  NIDL(3, "Method exit, jmeter csv_split mode = %d", g_jmeter_vuser_csv.csv_file_split_mode);

  return 0;
}

// JMETER Schedule setting - To be used while starting jmeter
int kw_set_jmeter_schedule_settings(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[JMETER_MAX_KEYWORD_LEN + 1];
  char group_name[JMETER_MAX_ARGS_LEN + 1];
  char threadnum[JMETER_MAX_ARGS_LEN + 1];
  char ramp_up_time[JMETER_MAX_ARGS_LEN + 1];
  char duration[JMETER_MAX_ARGS_LEN + 1];
  char temp[JMETER_MAX_ARGS_LEN + 1];
  int num_args = 0;
  
  NSDL1_PARSING(NULL, NULL, "Method Called");

  // Keyword definition - ALL NA NA NA
  num_args = sscanf(buf, "%s %s %s %s %s %s", keyword, group_name, threadnum, ramp_up_time, duration, temp);
  
  if(num_args != 5)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_SCHEDULE_SETTING_USAGE, CAV_ERR_1011374, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  // Parse and save ThreadNum
  // IF threadnum is not NA we will keep its default value
  if(strncasecmp(threadnum, "NA", 2))
  {
    if(nslb_atoi(threadnum, &gset->jmeter_schset.threadnum) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_SCHEDULE_SETTING_USAGE, CAV_ERR_1011374, CAV_ERR_MSG_2);
    }
  }
  else
    gset->jmeter_schset.threadnum = -1;

  // Parse and save ramp up time
  if(strncasecmp(ramp_up_time, "NA", 2))
  {
    if(nslb_atoi(ramp_up_time, &gset->jmeter_schset.ramp_up_time) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_SCHEDULE_SETTING_USAGE, CAV_ERR_1011374, CAV_ERR_MSG_2);
    }
  }
  else
    gset->jmeter_schset.ramp_up_time = -1;
 
  // Parse and save duration - shouldn't be 0
  if(strncasecmp(duration, "NA", 2))
  {
    if(nslb_atoi(duration, &gset->jmeter_schset.duration) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_SCHEDULE_SETTING_USAGE, CAV_ERR_1011374, CAV_ERR_MSG_2);
    } 
    if(!gset->jmeter_schset.duration)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JMETER_SCHEDULE_SETTING_USAGE, CAV_ERR_1011374 , CAV_ERR_MSG_4);      
    }
  }
  else
     gset->jmeter_schset.duration = -1; 
  
  NSDL1_PARSING(NULL, NULL, "JMETER SCHEDULE SETTINGS: ThreadNum = %d, ramp_up_time = %d, duration = %d", 
                             gset->jmeter_schset.threadnum, gset->jmeter_schset.ramp_up_time, gset->jmeter_schset.duration);

  return 0;
}

void ns_jmeter_add_new_tx(Msg_com_con *mccptr, char *tx_name, int *norm_id, int tx_length, int tx_id)
{
  VUser *vptr = mccptr->vptr;
  JmeterTxIdMap *tx_map_ptr = mccptr->jmeter_tx_id_mapping;

  NSDL2_MISC(NULL, NULL, "Method called. trans_name = %s, trans_len = %d, tx_id = %d, max_entries = %d, total_enties = %d",
                          tx_name, tx_length, tx_id, tx_map_ptr->max_entries, tx_map_ptr->total_entries);

  if (tx_id < tx_map_ptr->total_entries)  // Already discovered transaction
  {
    NSDL2_MISC(NULL, NULL, "JMeter Dynamic TX already discovered. trans_name = %s, Tx_id = %d, norm_id = %d", tx_name, tx_id, tx_map_ptr->tx_map[tx_id]);
    *norm_id = tx_map_ptr->tx_map[tx_id];
    return ;
  }    
  
  if(tx_id != tx_map_ptr->total_entries)  // JML must return id in sequence
  {
    NSTL1(NULL, NULL, "Error: JMeter tx_id is not coming in sequence. tx_id = %d, total_entries = %d", tx_id, tx_map_ptr->total_entries);
    return;
  }
  
  if(tx_id >= tx_map_ptr->max_entries) // Check if we need to grow array or not
  {
    MY_REALLOC(tx_map_ptr->tx_map, (tx_map_ptr->max_entries + DELTA_TX_ENTRY_SIZE) * sizeof(int), "tx_map_ptr->tx_map", -1);
    tx_map_ptr->max_entries += DELTA_TX_ENTRY_SIZE;
  }
   
  *norm_id = nslb_get_norm_id(&normRuntimeTXTable, tx_name, tx_length);
  
  if (*norm_id >= 0)   //Found
  {
    NSTL1(NULL, NULL, "Warning: New JMeter transaction %s with tx_id %d is already in the normalization table with norm_id = %d", tx_name, tx_id, *norm_id);
  }
  else
    *norm_id = allocate_tx_id_and_send_discovery_msg_to_parent(vptr, tx_name, tx_length);

  tx_map_ptr->tx_map[tx_id] = *norm_id; 
  tx_map_ptr->total_entries++; 
  NSDL2_MISC(NULL, NULL, "JMeter New dynamic TX discovered. tx_name  = %s, Tx_id = %d, norm_id = %d", tx_name, tx_id, tx_map_ptr->tx_map[tx_id]);
}

static void jmeter_get_tx_norm_id(Msg_com_con *mccptr, int *norm_id, int tx_id)
{
  JmeterTxIdMap *tx_map_ptr = mccptr->jmeter_tx_id_mapping; 
  if (tx_id >= tx_map_ptr->total_entries)  // Already discovered transaction
  {
    NSDL2_MISC(NULL, NULL, "Error: Invalid JMeter Tx_id = %d", tx_id);
    *norm_id = -1;
    return;
  }    
  *norm_id = tx_map_ptr->tx_map[tx_id]; 
}

inline static void jmeter_update_pagedata(JMeterPageUrlRec *jm_urc, uint32_t sgrp_id, avgtime *avg, u_ns_ts_t download_time)
{
  
  NSDL4_MISC(NULL, NULL, "Method Called Page Status = %d", jm_urc->status) ;

  avg->pg_tries++;
  avg->pg_fetches_started++; //Page Download Started

  // Now update errro codes
  if ((jm_urc->status >= 0) && (jm_urc->status < TOTAL_PAGE_ERR)) 
    avg->pg_error_codes[jm_urc->status]++;
  else 
    // Unknown Error
    avg->pg_error_codes[NS_REQUEST_ERRMISC]++;

  if(!jm_urc->status)  //Success Page Response time
  {
    NSDL4_MISC(NULL, NULL, "Page is success");
    avg->pg_hits++;
    SET_MIN (avg->pg_succ_min_resp_time, download_time);
    SET_MAX (avg->pg_succ_max_resp_time, download_time);
    avg->pg_succ_tot_resp_time += download_time;

    NSDL4_SCHEDULE(NULL, NULL, "Page is success : average_time->pg_succ_min_resp_time - %d, average_time->pg_succ_max_resp_time - %d, "
                               "average_time->pg_succ_tot_resp_time - %d ",
                                avg->pg_succ_min_resp_time, avg->pg_succ_max_resp_time,
                                avg->pg_succ_tot_resp_time);
  }
  else   //Failure Page Response time
  {
    NSDL4_MISC(NULL, NULL, "Page is failed");
    SET_MIN (avg->pg_fail_min_resp_time, download_time);
    SET_MAX (avg->pg_fail_max_resp_time, download_time);
    avg->pg_fail_tot_resp_time += download_time;

    NSDL4_SCHEDULE(NULL, NULL, "Page is failed: average_time->pg_fail_min_resp_time - %d, average_time->pg_fail_max_resp_time - %d, "
                               "average_time->pg_fail_tot_resp_time - %d",
                                avg->pg_fail_min_resp_time, avg->pg_fail_max_resp_time,
                                avg->pg_fail_tot_resp_time); 
   }

  //Overall Page Response time
  SET_MIN (avg->pg_min_time, download_time);
  SET_MAX (avg->pg_max_time, download_time);
  avg->pg_tot_time += download_time;

  // Page js proc time - Defaulting it 0 as not required in jmeter
  avg->page_js_proc_time_min = 0;
  avg->page_js_proc_time_max = 0;
  avg->page_js_proc_time_tot = 0;
  avg->page_proc_time_min = 0;
  avg->page_proc_time_max = 0;
  avg->page_proc_time_tot = 0;

  if (g_percentile_report == 1)
    update_pdf_data(download_time, pdf_average_page_response_time, 0, 0, 0);

}

inline static void jmeter_update_urldata(JMeterPageUrlRec *jm_urc, uint32_t sgrp_id, avgtime *avg)
{
  //avgtime *avg = average_time;

  NSDL2_MISC(NULL, NULL, "Method called, jm_urc = %p, sgrp_id = %u, avg = %p", jm_urc, sgrp_id, avg);

  // duration - HTTP Response Time 
  int duration = jm_urc->duration;

  // Overall metrics (both successful and failures)
  avg->fetches_started++;
  avg->fetches_sent++;
  avg->url_overall_tot_time += duration;

  SET_MAX(avg->url_overall_max_time, duration);
  SET_MIN(avg->url_overall_min_time, duration);

  avg->num_tries++;   //these are total number of url hits count including succ & Failure
  if((jm_urc->status < 0) || (jm_urc->status >= TOTAL_USED_URL_ERR))
  {
    // NS_REQUEST_CV_FAILURE returned by JMeter is not valid for URL. So mapping to BadResponse
    if(jm_urc->status == NS_REQUEST_CV_FAILURE)
      jm_urc->status = NS_REQUEST_BAD_RESP;
    else
    {
      NSTL1(NULL, NULL, "Warning: JMeter URL record status (%d) is out of range. Setting it to MiscErr", jm_urc->status);
      jm_urc->status = NS_REQUEST_ERRMISC;
    }
  }
  avg->url_error_codes[jm_urc->status]++;  // For URL Failures Group

  if (jm_urc->status == 0) // Successful
  {
    avg->num_hits++;
    /*HTTP Successful Response Time*/
    avg->tot_time += duration;

    SET_MAX(avg->max_time, duration);
    SET_MIN(avg->min_time, duration);
    NSDL2_MISC(NULL, NULL, "Method called, num_hits = %d, max_time = %d, min_time = %d", avg->num_hits, avg->max_time, avg->min_time);
  }
  else
  {
    /*HTTP Failure Response Time*/
    avg->url_failure_tot_time += duration;

    SET_MAX(avg->url_failure_max_time, duration);
    SET_MIN(avg->url_failure_min_time, duration);
  }

  // DNS Time - url_dns_min_time, u_ns_4B_t url_dns_max_time, url_dns_tot_time, url_dns_count
  
  // connect_time - url_conn_min_time, url_conn_max_time, url_conn_tot_time, url_conn_count, 
  if(jm_urc->connect_time > 0) // We are assuming connect is made if connect time > 0
  {
    avg->url_conn_tot_time += jm_urc->connect_time;
    avg->url_conn_count++;

    SET_MAX(avg->url_conn_max_time, jm_urc->connect_time);
    SET_MIN(avg->url_conn_min_time, jm_urc->connect_time);
  }
  
  //TODO
  // SSL - url_ssl_min_time, url_ssl_max_time, url_ssl_tot_time, url_ssl_count;
 
  // First Byte Recvd Time - url_frst_byte_rcv_min_time, url_frst_byte_rcv_max_time, url_frst_byte_rcv_tot_time, u_ns_8B_t url_frst_byte_rcv_count;
  if(jm_urc->first_byte_time > 0) 
  {
    avg->url_frst_byte_rcv_tot_time += jm_urc->first_byte_time;
    avg->url_frst_byte_rcv_count++;

    SET_MAX(avg->url_frst_byte_rcv_max_time, jm_urc->first_byte_time);
    SET_MIN(avg->url_frst_byte_rcv_min_time, jm_urc->first_byte_time);
  }

  // Download Time - url_dwnld_min_time, url_dwnld_max_time, url_dwnld_tot_time, url_dwnld_count;

  if(jm_urc->status_code != -1) // Status code will not come in case like connection error, timeout??
  {
    NSDL2_MISC(NULL, NULL, "Jmeter URL record status_code = %d", jm_urc->status_code);  

    // Increament URL failures (1xx, 2xx...)
    set_http_status_codes_count(NULL, jm_urc->status_code, 1, avg);
  }
  
  if (jm_urc->req_total_size >= 0)
  {
    average_time->tx_bytes += jm_urc->req_total_size;  
  }

  if (jm_urc->rep_total_size >= 0)
  {
    average_time->rx_bytes += jm_urc->rep_total_size;  
    average_time->total_bytes += jm_urc->rep_total_size; 
  }

  // netcache_status


  // Send and Recv throughout
  avg->total_bytes += jm_urc->rep_total_size;

  // Increment failure metrics
 
   
  // Percentile, duration OR download time ??? 
  if (g_percentile_report == 1)
    update_pdf_data(duration, pdf_average_url_response_time, 0, 0, 0);

  NSDL2_MISC(NULL, NULL, "Method end"); 
} 

#define __jmeter_update_overallgrp_pagedata(jm_urc, sgrp_id, download_time)         \
{                                                                                  \
  jmeter_update_pagedata(jm_urc, sgrp_id, average_time, download_time);              \
}

#define __jmeter_update_groupwise_pagedata(jm_urc, sgrp_id, download_time)         \
{                                                                                  \
  avgtime *avg = (avgtime*)((char *)average_time + ((sgrp_id + 1) * g_avg_size_only_grp)); \
  jmeter_update_pagedata(jm_urc, sgrp_id, avg, download_time);                     \
}

#define __jmeter_update_overallgrp_urldata(jm_urc, sgrp_id)                        \
{                                                                                  \
  jmeter_update_urldata(jm_urc, sgrp_id, average_time);                            \
}

#define __jmeter_update_groupwise_urldata(jm_urc, sgrp_id)                         \
{                                                                                  \
  avgtime *avg = (avgtime*)((char *)average_time + ((sgrp_id + 1) * g_avg_size_only_grp)); \
  jmeter_update_urldata(jm_urc, sgrp_id, avg);                                     \
}

int jmeter_log_page_table(Msg_com_con *mccptr, char *page_name, int page_id, int sess_norm_id) {
  char logging_buffer[LOGGING_SIZE + 1];
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  VUser *vptr = mccptr->vptr;

  NSDL2_LOGGING(vptr, NULL, "Method called page_name = %s, page_id = %d, sess_norm_id = %d", page_name, page_id, sess_norm_id);

  // PAGETABLE:page_id,sess_id,page_name,sess_name
  amt_written = snprintf(logging_buffer, LOGGING_SIZE, "%s%d%c%d%c%s%c%s\n", "PAGETABLE:",
			     page_id, DELIMINATOR,
			     sess_norm_id, DELIMINATOR,
			     page_name, DELIMINATOR, runprof_table_shr_mem[mccptr->sgrp_id].sess_ptr->sess_name);

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
  return 0;
}

int jmeter_log_url_record(Msg_com_con *mccptr, JMeterPageUrlRec *jm_urc, int url_id, int tx_norm_id, JMeterMsgHdr *jm_msg) 
{
VUser *vptr = mccptr->vptr;
connection *cptr = vptr->last_cptr; 
int sgrp_id = mccptr->sgrp_id;
char* copy_location;

  NSDL2_LOGGING(vptr, cptr, "Method called, child_idx = %hd, url_id = %d, url = %*.*s", child_idx, url_id, jm_urc->url_len, jm_urc->url_len, jm_urc->url);
  //"Failed to copy to db, Error = ERROR:  value "4294967295" is out of range for type integer" - This error was coming in case url_id = -1,        hence adding a safety check and not logging data in db.
  if(url_id < 0)
  { 
    NSTL1(vptr, cptr, "Not logging url record as url_id = %d", url_id);
    // Not making url log on level 1 as url may contain special characters resulting in core dump
    NSTL2(vptr, cptr, "Url = %*.*s", jm_urc->url_len, jm_urc->url_len, jm_urc->url);
    return -1;
  }
  
  if ((copy_location = get_mem_ptr(url_record_size)) == NULL) 
    return -1;
 
  total_url_records++;
  total_url_record_size += url_record_size;
  memcpy(copy_location, &url_record_num, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  //X-dynatrace header sting - not supported for JMeter so setting len to 0 (see logging.c for details of this field)
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Child Id
  memcpy(copy_location, &child_idx, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  
  // Url Record Url num index
  memcpy(copy_location, &url_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;
  
  // Url Record Session index
  memcpy(copy_location, &runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;
  NSDL2_LOGGING(vptr, cptr, "sess_norm_id = %u", runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id);

  // Session Instance
  memcpy(copy_location, &jm_urc->session_inst, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Current Tx
  memcpy(copy_location, &tx_norm_id, UNSIGNED_INT_SIZE); 
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Current Page
  memcpy(copy_location, &jm_urc->page_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Tx Instance
  // It can be -ve if "Generate parent sample" is not checked in JMeter Tx Controller
  memcpy(copy_location, &jm_urc->tx_inst, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  
  // Url Record Page Instance
  memcpy(copy_location, &jm_urc->page_instance, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  unsigned char bit_mask = 0;
  // Bits (see logging.c file for all bits)
  //   4 - Main Url
  //   5 - Embed Url
  //       If both bit 4 and 5 are set, then it is redirect URL
  // URL type (takes up 4th and 5th bit)
  // Here we are logging embedded redirected url  as embedded url
  // We are logging only main redirected url as redirected url
  // We are doing this because in page component  detail page embedded redirected   URL comes  twice, so the average response time of page is not correct

  if(jm_msg->flags & JMETER_FLAGS_MAIN_URL)
    bit_mask |= 0x10;
  else if(jm_msg->flags & JMETER_FLAGS_INLINE_URL)
    bit_mask |= 0x20;
  else if(jm_msg->flags & JMETER_FLAGS_MAIN_REDURL)
    bit_mask |= 0x30;

  // Connection reuse (takes up 4th bit). Use it as conn reuse
  if(jm_urc->connect_time == 0) // We are assuming  connect time is 0 then connection is reused. This may be incorrect if connect time < 1.
    bit_mask |= 0x08;    

  
  memcpy(copy_location, &bit_mask, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Url Record Url Started at
  long elap_time_ms;
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_urc->start_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // DNS Resolution time (TODOLater - we do get in JMeter)
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Connect time
  memcpy(copy_location, &jm_urc->connect_time, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record SSL Handshake done time (TODOLater - we do get in JMeter)
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Write Complete time (TODOLater - we do get in JMeter)
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record First byte receive time
  memcpy(copy_location, &jm_urc->first_byte_time, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Download Time (TODOLater - we do get in JMeter)
  NSDL2_LOGGING(vptr, cptr, "download time set as 0");
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record this zero copy is for Rendering time
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url End time
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_urc->end_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Url Record Response code
  short status_code = (short)(jm_urc->status_code);
  memcpy(copy_location, &status_code, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // Url Record Http Payload sent
  memcpy(copy_location, &jm_urc->req_body_size, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Tcp bytes sent
  memcpy(copy_location, &jm_urc->req_total_size, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record this zero copy is for Ethernet bytes sent
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record cptr bytes
  // TODO - is this correct. In NS, we use cptr->bytes
  memcpy(copy_location, &jm_urc->rep_body_size, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record TCP bytes receive
  memcpy(copy_location, &jm_urc->rep_total_size, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record this zero copy is for Ethernet bytes recv
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Compression mode
  memcpy(copy_location, &zero_unsigned_char, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Url Record Status 
  // TODO - check what is Pass or Fail. In JMeter, 0 is pass
  memcpy(copy_location, &jm_urc->status, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Url Record Num connection
  // this zero copy is for content verification code
  // Use this place for connection number
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Url Record Connection type (KA or NKA)
  memcpy(copy_location, &zero_unsigned_char, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Url Record this zero copy is for retries
  memcpy(copy_location, &zero_unsigned_char, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Url Record flow path instance
  memcpy(copy_location, &zero_long, UNSIGNED_LONG_SIZE);
  copy_location += UNSIGNED_LONG_SIZE;
  
  // Phase number (TODOLater - we do get in JMeter)
  memcpy(copy_location, &zero_short, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  return 0;
}

int jmeter_log_page_record_v2(Msg_com_con *mccptr, JMeterPageUrlRec *jm_urc, int tx_norm_id, JMeterMsgHdr *jm_msg) {
  char* copy_location;
  unsigned char record_num = PAGE_RECORD_V2;
  int sgrp_id = mccptr->sgrp_id;
  VUser *vptr = mccptr->vptr;

  NSDL1_LOGGING(vptr, NULL, "Method called, child_idx = %hd, page_record_size = %d", child_idx, page_record_size);
  if ((copy_location = get_mem_ptr(page_record_size)) == NULL)
    return -1;

  total_page_records++;
  total_page_record_size += page_record_size;

  memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Page Record Child Id
  memcpy(copy_location, &child_idx, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // Page Record Current Page
  memcpy(copy_location, &jm_urc->page_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Page Record Session Index
  memcpy(copy_location, &runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Page Record Session Instance
  memcpy(copy_location, &jm_urc->session_inst, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Page Record Current Tx
  // This will work for single concurrent Transaction only - TODONJ - verify this. 
  memcpy(copy_location, &tx_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Page Record Tx Instance
  memcpy(copy_location, &jm_urc->tx_inst, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // Page Record Page Instance
  memcpy(copy_location, &jm_urc->page_instance, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // Page Record Page Begin at
  long elap_time_ms;
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_urc->start_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Page Record Page end at
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_urc->end_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Page Record Page Status
  memcpy(copy_location, &jm_urc->status, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  memcpy(copy_location, &zero_short, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  return 0;
}

/* 
This method will record for all page instance executed in context of transaction.
This record fields are of transaction(not of page).
For ex - start time, end time, status are of transaction
*/
static int jmeter_log_tx_pg_record_v2(Msg_com_con *mccptr, JMeterTxRec *jm_txrc, int tx_norm_id, long start_elap, long end_elap, int ins) {
  char* copy_location;
  unsigned char record_num = TX_PG_RECORD_V2;
  int sgrp_id = mccptr->sgrp_id;
  VUser *vptr = mccptr->vptr;

  NSDL2_LOGGING(vptr, NULL, "Method called.");
   
  if ((copy_location = get_mem_ptr(tx_pg_record_size)) == NULL)
    return -1;

  total_tx_pg_records++;
  total_tx_pg_record_size += tx_pg_record_size;

  // Record type
  memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Record Child Id
  memcpy(copy_location, &child_idx, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // TransactionIndex
  memcpy(copy_location, &tx_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // SessionIndex
  memcpy(copy_location, &runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
  copy_location += INDEX_SIZE;

  //Session Instance
  memcpy(copy_location, &jm_txrc->session_instance, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // TxInstance
  memcpy(copy_location, &jm_txrc->tx_instance, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //Page instance
  memcpy(copy_location, &jm_txrc->page_instance[ins], SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // StartTime
  memcpy(copy_location, &start_elap, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // EndTime
  memcpy(copy_location, &end_elap, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Tx Status
  memcpy(copy_location, &jm_txrc->status, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Note - RespTime is not in dlog. It will be calculated by logging_reader when generating CSV file from dlog
    
  // Phase id
  memcpy(copy_location, &zero_short, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  NSDL2_LOGGING(vptr, NULL, "total_tx_pg_record_size = %lld, session instance = %u", total_tx_pg_record_size, jm_txrc->session_instance);
  
  return 0;
}

inline static void jmeter_process_rec_url(JMeterMsgHdr *jm_msg, uint32_t sgrp_id, Msg_com_con *mccptr)
{
  int sample_num = jm_msg->sample_num;
  JMeterPageUrlRec *jm_urc = (char *)jm_msg + sizeof(JMeterMsgHdr); // Point to after header to URL record
  
  char *url = jm_urc->url;
  char *label = url + jm_urc->url_len + 1;  //Point to lable to len

  VUser *vptr = mccptr->vptr;
  connection *cptr = vptr->last_cptr;
  int url_ok = 0;
  u_ns_ts_t download_time;
  

  NSDL2_MISC(NULL, NULL, "JMeterRecord_URL: sample_num = %ld, time_stamp = %ld, "
                         "start_time = %ld, end_time = %ld, page_id = %d, transaction_id = %d, session_instance = %d, "
                         "rxunning_vuser = %d, duration = %d, dns_time = %d, connect_time = %d, ssl_time = %d, first_byte_time = %d, "
                         "download_time = %d, status = %d, netcache_status = 0x%X, edge_time = %d, origin_time = %d, "
                         "req_hdr_size = %d, req_body_size = %d, req_total_size = %d, rep_hdr_size = %d, rep_body_size = %d, "
                         "rep_total_size = %d, error_count = %d, status_code = %d, page_instance = %hd, tx_inst = %hd, url_len = %d, "
                         " sample_label_len = %d, url = %s, label = %s", 
              sample_num, jm_urc->time_stamp, jm_urc->start_time, jm_urc->end_time, 
              jm_urc->page_id, jm_urc->transaction_id, jm_urc->session_inst, jm_urc->running_vusers, jm_urc->duration,
              jm_urc->dns_time, jm_urc->connect_time, jm_urc->ssl_time, jm_urc->first_byte_time, jm_urc->download_time, jm_urc->status, 
              jm_urc->nw_cache_state, jm_urc->edge_time, jm_urc->origin_time, jm_urc->req_hdr_size, jm_urc->req_body_size,
              jm_urc->req_total_size, jm_urc->rep_hdr_size, jm_urc->rep_body_size, jm_urc->rep_total_size, jm_urc->error_count,
              jm_urc->status_code, jm_urc->page_instance, jm_urc->tx_inst, jm_urc->url_len,
              jm_urc->sample_label_len, url, label);

 
  // Getting normalized page_id
  JMETER_TO_NORM_PAGE_ID(jm_urc->page_id);
 
  // update running vuser
  jmeter_set_vuser(jm_urc->running_vusers, sgrp_id);
 
  if (jm_msg->flags & JMETER_FLAGS_ALL_URL)
  {
    if (jm_urc->status == 0)
      url_ok = 1;
    //Set url cache state  
    cptr->cptr_data->nw_cache_state = jm_urc->nw_cache_state; 
    cptr->tcp_bytes_recv = jm_urc->rep_total_size;
    if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(sgrp_id) && cptr->cptr_data->nw_cache_state)
    {
      nw_cache_stats_update_counter(cptr, jm_urc->duration, url_ok);
    }

    // UrlStat - overall of all the groups
    __jmeter_update_overallgrp_urldata(jm_urc, sgrp_id);
  }
 
  if (jm_msg->flags & JMETER_FLAGS_PAGE)
  { 
    // update page stats of all groups
    // Calculate download time
    download_time = jm_urc->end_time - jm_urc->start_time; // in milliseconds
    __jmeter_update_overallgrp_pagedata(jm_urc, sgrp_id, download_time);
  }
  // UrlStat - gorupwise 
  if (SHOW_GRP_DATA)
  {
    if (jm_msg->flags & JMETER_FLAGS_ALL_URL)
      __jmeter_update_groupwise_urldata(jm_urc, sgrp_id);
 
    if (jm_msg->flags & JMETER_FLAGS_PAGE)
      __jmeter_update_groupwise_pagedata(jm_urc, sgrp_id, download_time);
  }
 
  NSDL2_MESSAGES(NULL, NULL, "log_records = %d, run_mode = %d, report_level = %d", log_records, run_mode,
                              runprof_table_shr_mem[sgrp_id].gset.report_level);

  if (JM_LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  {
    int tx_norm_id;   // we need to get tx norm_id from tx_id coming in the message
    if(jm_urc->transaction_id != JMETER_TX_ID_NOT_SET)
      jmeter_get_tx_norm_id(mccptr, &tx_norm_id, jm_urc->transaction_id);
    
    if (tx_norm_id == -1)
    {
      tx_norm_id = 0; 
      NSTL1(NULL, NULL, "Failed to get trans_norm_id for invalid tx_id = %d", jm_urc->transaction_id);
      // TODONJ - what if tx_norm_id is -1.. What will happen in the jmeter_log_url_record()
    }
    
    if (jm_msg->flags & JMETER_FLAGS_ALL_URL)
    {
      // Check for URL and get url_id (existing or new)
      int url_id = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)url, jm_urc->url_len, jm_urc->page_id, 0, 0, label);
      
      if(jmeter_log_url_record(mccptr, jm_urc, url_id, tx_norm_id, jm_msg) == -1)
        NSTL1_OUT(NULL, NULL, "JMeter - Error in logging the url record");
    }
      
    if(jm_msg->flags & JMETER_FLAGS_PAGE)
    {
      if(jmeter_log_page_record_v2(mccptr, jm_urc, tx_norm_id, jm_msg) == -1)
        NSTL1_OUT(NULL, NULL, "JMeter - Error in logging the page record");
    }
  }
}

inline static void jmeter_update_single_txdata(JMeterTxRec *jm_txrc, uint32_t sgrp_id, Msg_com_con *mccptr, u_ns_8B_t sqrt_duration, int tx_hashcode, int duration, int think_time)
{
  TxDataSample *txavg;
  VUser *vptr = mccptr->vptr;

  NSDL2_MISC(NULL, NULL, "Method called, jm_txrc = %p, sgrp_id = %u, jm_txrc->tx_name_len = %d", jm_txrc, sgrp_id, jm_txrc->tx_name_len);

  txavg = &txData[tx_hashcode];
  vptr->tx_hash_code = tx_hashcode;
  // TxStat - tx time of this transaction including success + failure
  txavg->tx_tot_time += duration;
  SET_MIN(txavg->tx_min_time, duration);
  SET_MAX(txavg->tx_max_time, duration);
  txavg->tx_tot_sqr_time += sqrt_duration;

  txavg->tx_fetches_started++;
  txavg->tx_fetches_completed++;

  if (jm_txrc->status == JMETER_TX_SUCCESS)
  {
    // TxStat - tx time of this transaction of success tx only  
    txavg->tx_succ_tot_time += duration;
    SET_MIN(txavg->tx_succ_min_time, duration);
    SET_MAX(txavg->tx_succ_max_time, duration);
    txavg->tx_succ_tot_sqr_time += sqrt_duration;

    txavg->tx_succ_fetches++;
  }
  else
  {
    // TxStat - tx time of this transaction of failure tx only  
    txavg->tx_failure_tot_time += duration;
    SET_MIN(txavg->tx_failure_min_time, duration);
    SET_MAX(txavg->tx_failure_max_time, duration);
    txavg->tx_failure_tot_sqr_time += sqrt_duration;
  }

  txavg->tx_tx_bytes += jm_txrc->req_tot_bytes;  //Transmitted throughput
  txavg->tx_rx_bytes += jm_txrc->rep_tot_bytes;  //Receive throughput

  SET_MIN (txavg->tx_min_think_time, think_time);
  SET_MAX (txavg->tx_max_think_time, think_time);
  txavg->tx_tot_think_time += think_time;
  
  if(jm_txrc->nw_cache_state & TCP_HIT_MASK)
  {
    vptr->flags |= NS_RESP_NETCACHE;
  } else {
    vptr->flags &= ~NS_RESP_NETCACHE;
  }
  //If NS_RESP_NETCACHE flag enable then increment netcache_fetches 
  UPD_TX_NW_CACHE_USED_STATS(vptr, tx_hashcode, duration, sqrt_duration);

  if (g_percentile_report == 1)
    update_pdf_data(duration, pdf_transaction_time, 0, 0, tx_hashcode);

  NSDL2_MISC(NULL, NULL, "Method end");
}

inline static void jmeter_update_tx_avgdata(JMeterTxRec *jm_txrc, uint32_t sgrp_id, avgtime *avg, u_ns_8B_t sqrt_duration, int duration, int think_time)
{

  NSDL2_MISC(NULL, NULL, "Method called, jm_txrc = %p, sgrp_id = %u", jm_txrc, sgrp_id);

  avg->tx_tot_time += duration;
  SET_MIN(avg->tx_min_time, duration);
  SET_MAX(avg->tx_max_time, duration);
  avg->tx_tot_sqr_time += sqrt_duration;

  avg->tx_fetches_started++;
  avg->tx_fetches_completed++;

#if 0
  if((jm_txrc->status < JMETER_TX_SUCCESS) || (jm_txrc->status >= TOTAL_TX_ERR))
  {
    NSTL1(NULL, NULL, "Warning: JMeter Tx record status (%d) is out of range. Setting it to MiscErr", jm_txrc->status);
    jm_txrc->status = NS_REQUEST_ERRMISC;
  }
#endif

  if (jm_txrc->status != JMETER_TX_SUCCESS)
  {
    if(jm_txrc->last_http_status != JMETER_TX_SUCCESS)
    {
      NSTL1(NULL, NULL, "Warning: JMeter Tx record status (%d) is failed. Setting it to last http status (%d)", jm_txrc->status,
                        jm_txrc->last_http_status);
      jm_txrc->status = jm_txrc->last_http_status;
    }
    else
    {
      NSTL1(NULL, NULL, "Warning: JMeter Tx record status (%d) is failed. Setting it to MiscErr", jm_txrc->status);
      jm_txrc->status = NS_REQUEST_ERRMISC;
    }
  }

  if (jm_txrc->status == JMETER_TX_SUCCESS)
  {
    avg->tx_succ_tot_resp_time += duration;
    SET_MIN(avg->tx_succ_min_resp_time, duration); 
    SET_MAX(avg->tx_succ_max_resp_time, duration); 
    avg->tx_succ_fetches++;
  }
  else
  {
    avg->tx_fail_tot_resp_time += duration;
    SET_MIN(avg->tx_fail_min_resp_time, duration); 
    SET_MAX(avg->tx_fail_max_resp_time, duration); 
  }
  avg->tx_error_codes[jm_txrc->status]++;  // For Tx Failures Group
  
  SET_MIN (avg->tx_min_think_time, think_time);
  SET_MAX (avg->tx_max_think_time, think_time);
  avg->tx_tot_think_time += think_time;

  avg->tx_tx_bytes += jm_txrc->req_tot_bytes;  //Transmitted throughput
  avg->tx_rx_bytes += jm_txrc->rep_tot_bytes;  //Receive throughput

  if (g_percentile_report == 1)
    update_pdf_data(duration, pdf_average_transaction_response_time, 0, 0, 0);

  NSDL2_MISC(NULL, NULL, "Method end"); 
}

static void jmeter_update_sess_avgdata(JmeterMsgSessRec *jm_ssr, uint32_t sgrp_id, avgtime *avg, u_ns_ts_t download_time)
{
  NSDL2_MISC(NULL, NULL, "Method Called jm_ssr = %p, sgrp_id = %d, sess_tries = %d, ss_fetches_started = %d", jm_ssr, sgrp_id, 
                   avg->sess_tries, avg->ss_fetches_started);

  avg->sess_tries++;
  avg->ss_fetches_started++; //Sesson Started

  if (!jm_ssr->sess_status)   //Successful  Session 
  {
    NSDL4_MISC(NULL, NULL, "JMeter Session Successful");
    avg->sess_hits++;

    SET_MIN (avg->sess_succ_min_resp_time, download_time);
    SET_MAX (avg->sess_succ_max_resp_time, download_time);

    avg->sess_succ_tot_resp_time += download_time;

    NSDL4_MISC(NULL, NULL, "JMeter Session Successful : average_time->sess_hits - %d, average_time->sess_succ_min_resp_time - %d, "
                               "average_time->sess_succ_max_resp_time - %d, average_time->sess_succ_tot_time - %d",
                                avg->sess_hits, average_time->sess_succ_min_resp_time, avg->sess_succ_max_resp_time,
                                avg->sess_succ_tot_resp_time);
  }
  else // Session Failed
  {
    NSDL4_MISC(NULL, NULL, "JMeter Session Failed");

    SET_MIN (avg->sess_fail_min_resp_time, download_time);
    SET_MAX (avg->sess_fail_max_resp_time, download_time);

    avg->sess_fail_tot_resp_time += download_time;

    NSDL4_MISC(NULL, NULL, "JMeter Session Failed: average_time->sess_hits - %d, average_time->sess_fail_min_resp_time - %d, "
                               "average_time->sess_fail_max_resp_time - %d, average_time->sess_fail_tot_time - %d",
                                avg->sess_hits, avg->sess_fail_min_resp_time, avg->sess_fail_max_resp_time,
                                avg->sess_fail_tot_resp_time);
  }

  //Overall Session Data
  SET_MIN (avg->sess_min_time, download_time);
  SET_MAX (avg->sess_max_time, download_time);

  avg->sess_tot_time += download_time;

  NSDL4_MISC(NULL, NULL, "JMeter Session Overall: average_time->sess_min_time - %d, "
                             "average_time->sess_max_time - %d, average_time->sess_tot_time - %d",
                              avg->sess_min_time, avg->sess_max_time,
                              avg->sess_tot_time);

  if ((jm_ssr->sess_status >= 0) && (jm_ssr->sess_status < TOTAL_SESS_ERR)) 
    avg->sess_error_codes[jm_ssr->sess_status]++;
  else
    avg->sess_error_codes[NS_REQUEST_ERRMISC]++; // MISCERR

  if (g_percentile_report == 1)
    update_pdf_data(download_time, pdf_average_session_response_time, 0, 0, 0);

  NSDL2_MISC(NULL, NULL, "Method End");
 
}

#define __jmeter_update_overall_sessdata(jm_ssr, sgrp_id, download_time)              \
{                                                                                     \
  jmeter_update_sess_avgdata(jm_ssr, sgrp_id, average_time, download_time);             \
}

#define __jmeter_update_groupwise_sessdata(jm_ssr, sgrp_id, download_time)               \
{                                                                                     \
  avgtime *avg = (avgtime*)((char *)average_time + ((sgrp_id + 1) * g_avg_size_only_grp)); \
  jmeter_update_sess_avgdata(jm_ssr, sgrp_id, avg, download_time);                         \
}

#define __jmeter_update_overall_txdata(jm_txrc, sgrp_id, sqrt_duration, duration, think_time)   \
{                                                                                                \
  jmeter_update_tx_avgdata(jm_txrc, sgrp_id, average_time, sqrt_duration, duration, think_time);  \
}

#define __jmeter_update_groupwise_txdata(jm_txrc, sgrp_id, sqrt_duration, duration, think_time)    \
{                                                                                                  \
  avgtime *avg = (avgtime*)((char *)average_time + ((sgrp_id + 1) * g_avg_size_only_grp));         \
  jmeter_update_tx_avgdata(jm_txrc, sgrp_id, avg, sqrt_duration, duration, think_time);            \
}

// We are passing sgrp_id as a part of convention
static int jmeter_log_session_record(JMeterMsgHdr *jm_msg, uint32_t sgrp_idx, Msg_com_con *mccptr)
{

  char* copy_location;
  unsigned char record_num = SESSION_RECORD;
  unsigned char is_run_phase = 1;
  VUser *vptr = mccptr->vptr;

  JmeterMsgSessRec *jm_ssr = (char *)jm_msg + sizeof(JMeterMsgHdr);

  // Getting memory to write in shared buffer
  if ((copy_location = get_mem_ptr(SESSION_RECORD_SIZE)) == NULL)
  {
    NSTL1(vptr, NULL, "Unable to get shared buffer, page dump not recorded");
    return -1;
  }

  total_session_records++;
  total_session_record_size += SESSION_RECORD_SIZE;
  memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Session Record Session Index
  memcpy(copy_location, &runprof_table_shr_mem[mccptr->sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Session Instance
  memcpy(copy_location, &jm_ssr->sess_inst, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record User IndeX
  memcpy(copy_location, &jm_msg->user_index, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Group Num
  memcpy(copy_location, &(runprof_table_shr_mem[mccptr->sgrp_id].grp_norm_id), UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Child id
  memcpy(copy_location, &child_idx, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  // Session Record Is Run Phase
  memcpy(copy_location, &is_run_phase, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Session Record Access
  // Calculting the index by taking the difference of shared memory pointers
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Location
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Browser
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Freq
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Machine Attribute
  memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // Session Record Started at time
  long elap_time_ms;
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_ssr->start_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;
    
  // Session Record End Time
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_ssr->end_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Session Record Think Duration
  long zero_long = 0;
  memcpy(copy_location, &zero_long, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  // Session Record Session Status
  memcpy(copy_location, &jm_ssr->sess_status, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  // Session Record Phase num
  memcpy(copy_location, &zero_short, SHORT_SIZE);

  return 0;
}

static void jmeter_process_rec_sess(JMeterMsgHdr *jm_msg, uint32_t sgrp_id, Msg_com_con *mccptr)
{
  JmeterMsgSessRec *jm_ssr = (char *)jm_msg + sizeof(JMeterMsgHdr);

  VUser *vptr = mccptr->vptr;
  NSDL1_LOGGING(vptr, NULL, "JMeterRecord_SessionRecord Method called, start_time = %ld, end_time = %ld, user_index = %d" , 
                            "sess_inst = %d, sess_status = %hhd",
                             jm_ssr->start_time, jm_ssr->end_time, jm_msg->user_index, jm_ssr->sess_inst, jm_ssr->sess_status);

  // Calculate download time
  u_ns_ts_t download_time;
  download_time = jm_ssr->end_time - jm_ssr->start_time; // in milliseconds

  // Update Overall groups TxStats
  __jmeter_update_overall_sessdata(jm_ssr, sgrp_id, download_time);

  // Update Gropwise TxStats
  if (SHOW_GRP_DATA)
    __jmeter_update_groupwise_sessdata(jm_ssr, sgrp_id, download_time);

  /* For Page dump feature -  We need session record to get session status dumped hence src.csv is required. 
            And also when DRILLDOWN report is enable this src.csv is required */
  if ((JM_LOG_LEVEL_FOR_DRILL_DOWN_REPORT) || (log_records && (get_max_tracing_level() > 0) && (get_max_trace_dest() > 0) && (run_mode == NORMAL_RUN)))
  {
    jmeter_log_session_record(jm_msg, sgrp_id, mccptr);
  }

  NSDL1_LOGGING(vptr, NULL, "JMeterRecord_SessionRecord Method End");
   
}

// We are passing sgrp_id as a part of convention
static int jmeter_log_page_dump_record( JMeterMsgHdr *jm_msg, uint32_t sgrp_idx, Msg_com_con *mccptr)
{
  // We would be having header in the message, So incrementing to get page dump message
  JmeterMsgPageDumpRec *jm_pdr = (char *)jm_msg + sizeof(JMeterMsgHdr);

  char* copy_location;
  VUser *vptr = mccptr->vptr;
  int nvm_id, gen_id = -1;
  int future1 = -1;
  static unsigned char record_num = PAGE_DUMP_RECORD;
  RunProfTableEntry_Shr *rpf_ptr = &(runprof_table_shr_mem[mccptr->sgrp_id]);

  // Point all variable length fields to buffer. Note all are NULL terminated but len is without NULL.
  char *page_name = jm_pdr->page_name;
  char *assertion_results = page_name + jm_pdr->page_name_len + 1;
  char *tx_name = assertion_results + jm_pdr->assertion_results_len + 1;
  char *fetched_param = tx_name + jm_pdr->tx_name_len + 1;
  char *flow_name = fetched_param + jm_pdr->fetched_param_len + 1;
  char *log_file_sfx = flow_name + jm_pdr->flow_name_len + 1;
  char *res_body_orig_name = log_file_sfx + jm_pdr->log_file_sfx_len + 1;
  char *parameter = res_body_orig_name + jm_pdr->res_body_orig_name_len + 1;

  // Debug log for printing page_name, flow_name, etc
  NSDL1_LOGGING(vptr, NULL, "JMeterRecord_PageDump, page_name = %s, assertion_results = %s, " 
                 "tx_name = %s, fetched_param = %s, flow_name = %s, log_file_sfx = %s, "
                 "res_body_orig_name = %s, parameter = %s, " 
                 "user_idx = %d, start_time = %ld, end_time = %ld, partition = %ld, "
                 "sess_instance =  %d, page_index = %d, page_instance = %d, "
                 "page_status = %d, page_response_time = %d, page_name_len = %hd, tx_name_len = %hd, "
                 "log_file_sfx_len = %hd, res_body_orig_name_len = %hd, assertion_results_len = %hd, "
                 "flow_name_len = %hd, parameter_len = %hd, fetched_param_len = %hd",
                 page_name, assertion_results, tx_name,
                 fetched_param, flow_name, log_file_sfx, res_body_orig_name, parameter,
                 jm_msg->user_index, jm_pdr->start_time, jm_pdr->end_time, jm_pdr->partition, 
                 jm_pdr->sess_instance, jm_pdr->page_index, jm_pdr->page_instance,
                 jm_pdr->page_status, jm_pdr->page_response_time, jm_pdr->page_name_len, jm_pdr->tx_name_len, 
                 jm_pdr->log_file_sfx_len, jm_pdr->res_body_orig_name_len, jm_pdr->assertion_results_len,
                 jm_pdr->flow_name_len, jm_pdr->parameter_len, jm_pdr->fetched_param_len);

  // Getting Normalized Page ID
  JMETER_TO_NORM_PAGE_ID(jm_pdr->page_index)

  if(jm_pdr->assertion_results_len > 4096)
  {
    NSDL1_LOGGING(vptr, NULL, "Assertion results length (%d) is more than max limit of 4095. It will be truncated", jm_pdr->assertion_results_len);
    jm_pdr->assertion_results_len = 4096;  // Limit to 4096 as we db table has this limit
    NSTL2(vptr, NULL, "Truncating Assertion result length to %d", jm_pdr->assertion_results_len);
  }
 
  // page_dump_record_size is a constant value PAGE_DUMP_RECORD_SIZE
  int req_size = PAGE_DUMP_RECORD_SIZE + jm_pdr->tx_name_len + jm_pdr->log_file_sfx_len +
                 jm_pdr->res_body_orig_name_len + jm_pdr->assertion_results_len + jm_pdr->flow_name_len + jm_pdr->parameter_len + 
                 jm_pdr->fetched_param_len;


  if (req_size > log_shr_buffer_size)
  {
    NSDL1_LOGGING(vptr, NULL, "Requested size = %d for page dump is more than shared memory block size = %d. Data will be lost", req_size, log_shr_buffer_size);
    NSTL1(vptr, NULL, "Requested size = %d is more than shared memory block size = %d. Hence ignoring this page from logging", req_size, log_shr_buffer_size);
    return -1;
  }

  // Getting memory to write in shared buffer
  if ((copy_location = get_mem_ptr(req_size)) == NULL) 
  {
    NSTL1(vptr, NULL, "Unable to get shared buffer, page dump not recorded");
    return -1;
  }
  total_page_dump_records++;
  total_page_dump_record_size += req_size;

  memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;

  //page dump url record start time
  long elap_time_ms;
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_pdr->start_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  //page dump url record page end time
  CONVERT_ABS_TO_ELAPSED(elap_time_ms, jm_pdr->end_time);
  memcpy(copy_location, &elap_time_ms, NS_TIME_DATA_SIZE);
  copy_location += NS_TIME_DATA_SIZE;

  //page dump url record Generator Id.
  if (send_events_to_master == 1) //In case of NC, calculate generator id whereas in case of standalone send -1
    gen_id = (int)((child_idx & 0xFF00) >> 8);
  memcpy(copy_location, &gen_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record NVM Id
  nvm_id = (int)(child_idx & 0x00FF);
  memcpy(copy_location, &nvm_id, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //page dump url record User Id.
  memcpy(copy_location, &jm_msg->user_index, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record Session Inst
  memcpy(copy_location, &jm_pdr->sess_instance, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  // page_index 
  memcpy(copy_location, &jm_pdr->page_index, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record Page Inst.
  memcpy(copy_location, &jm_pdr->page_instance, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //page dump url record Page Status
  memcpy(copy_location, &jm_pdr->page_status, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //page dump url record Page response time
  memcpy(copy_location, &jm_pdr->page_response_time, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record Group Id
  memcpy(copy_location, &(rpf_ptr->grp_norm_id), UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record Session Id
  memcpy(copy_location, &runprof_table_shr_mem[mccptr->sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //page dump url record Partition.
  memcpy(copy_location, &jm_pdr->partition, UNSIGNED_LONG_SIZE);
  copy_location += UNSIGNED_LONG_SIZE;

  //page dump url record Trace Level.
  memcpy(copy_location, &(rpf_ptr->gset.trace_level), SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //Insert future field as -1 - Taken reference from olde code
  memcpy(copy_location, &future1, UNSIGNED_INT_SIZE);
  copy_location += UNSIGNED_INT_SIZE;

  //TxName 
  memcpy(copy_location, &jm_pdr->tx_name_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  memcpy(copy_location, tx_name, jm_pdr->tx_name_len);
  copy_location += jm_pdr->tx_name_len;


  //Insert fetched param
  memcpy(copy_location, &jm_pdr->fetched_param_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  memcpy(copy_location, fetched_param, jm_pdr->fetched_param_len);
  copy_location += jm_pdr->fetched_param_len ;

  //page dump url record flow name Size.
  memcpy(copy_location, &jm_pdr->flow_name_len, UNSIGNED_CHAR_SIZE);
  copy_location += UNSIGNED_CHAR_SIZE;
  memcpy(copy_location, flow_name, jm_pdr->flow_name_len);
  copy_location += jm_pdr->flow_name_len;

  //page dump url log file.
  memcpy(copy_location, &jm_pdr->log_file_sfx_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  memcpy(copy_location, log_file_sfx, jm_pdr->log_file_sfx_len);
  copy_location += jm_pdr->log_file_sfx_len;
  
  //page dump url res body orig name.
  memcpy(copy_location, &jm_pdr->res_body_orig_name_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  memcpy(copy_location, res_body_orig_name, jm_pdr->res_body_orig_name_len);
  copy_location += jm_pdr->res_body_orig_name_len;
    
  //page dump url record Parameter Size.
  memcpy(copy_location, &jm_pdr->parameter_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;
  //page dump url record Parameter.
  memcpy(copy_location, parameter, jm_pdr->parameter_len);
  copy_location += jm_pdr->parameter_len;

  //page dump assertion failure message length.
  memcpy(copy_location, &jm_pdr->assertion_results_len, SHORT_SIZE);
  copy_location += SHORT_SIZE;

  //page dump record assertion failure
  memcpy(copy_location, assertion_results, jm_pdr->assertion_results_len);
  copy_location += jm_pdr->assertion_results_len;

  return 0;
 
}

// Record format:
// RecordType, TransactionIndex, ChildIndex, PageInstance, SessionInstance, UserId
//
// RecordType, TransactionIndex, SessionIndex, SessionInstance, ChildIndex, TxInstance, PageInstance, StartTime, EndTime, Status, PhaseIndex, 

// Start and End time is already in abs format

int jmeter_log_tx_record_v2(Msg_com_con *mccptr, JMeterTxRec *jm_txrc, int tx_norm_id)
{
  VUser *vptr = mccptr->vptr;
  int sgrp_id = mccptr->sgrp_id;
  char* copy_location;
  unsigned char record_num = TX_RECORD_V2;

  long start_elap_time_ms; // For calculating start elapsed time
  long end_elap_time_ms; // For calculating end elapsed time

  NSDL2_LOGGING(vptr, NULL, "Method called");

  CONVERT_ABS_TO_ELAPSED(start_elap_time_ms, jm_txrc->start_time);
  CONVERT_ABS_TO_ELAPSED(end_elap_time_ms, jm_txrc->end_time);

  for(int i = 0; i < jm_txrc->num_pages; i++)
  {
    if ((copy_location = get_mem_ptr(tx_record_size)) == NULL)
      return -1;

    total_tx_records++;
    total_tx_record_size += tx_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    //  Tx Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Tx Record hash code or tx index  
    memcpy(copy_location, &tx_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //  Tx Record Session Index
    memcpy(copy_location, &runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //  Tx Record Session Instance
    //memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    memcpy(copy_location, &jm_txrc->session_instance, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //  Tx Record Tx Instance
    //memcpy(copy_location, &vptr->tx_instance, SHORT_SIZE);
    memcpy(copy_location, &jm_txrc->tx_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //  Tx Record Tx begin at
    memcpy(copy_location, &start_elap_time_ms, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    memcpy(copy_location, &end_elap_time_ms, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Think Time
    memcpy(copy_location, &zero_long, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Status
    memcpy(copy_location, &jm_txrc->status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Tx Record phase id
    memcpy(copy_location, &zero_short, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Insert transaction Page record corresponding to transaction
    if(jmeter_log_tx_pg_record_v2(mccptr, jm_txrc, tx_norm_id, start_elap_time_ms, end_elap_time_ms, i) < 0)
      NSTL1_OUT(NULL, NULL, "JMeter - Error in logging the tx page record");
  }  
    return 0;
}

inline static void jmeter_process_rec_tx(JMeterMsgHdr *jm_msg, uint32_t sgrp_id, Msg_com_con *mccptr)
{
  int sample_num = jm_msg->sample_num;
  JMeterTxRec *jm_txrc = ++jm_msg;
  VUser *vptr = mccptr->vptr;
  int tx_norm_id;   // we need to get tx norm_id from tx_id coming in the message
  int think_time;
  int duration;
 
  // JmeterTransactionContoller has setting of including thinktime in duration or not.
  // If it is not included then we will calculate thinktime as set in our metrices.
  // This is done to map to netstorm design where thinktime is always included in duration.
  duration = jm_txrc->end_time - jm_txrc->start_time;

  // Computing square duration
  u_ns_8B_t sqrt_duration = duration * duration;
 
  think_time = duration - jm_txrc->duration;

  NSDL2_MISC(NULL, NULL, "JMeterRecord_Tx: sample_num = %ld, running_vusers = %d, req_tot_bytes = %ld, start_time = %ld,"
                         " end_time = %ld, transaction_id = %d, session_instance = %d, duration = %d, req_tot_bytes = %ld, "
                         "rep_tot_bytes = %ld, total_edge_time = %d, total_origin_time = %d, nw_cache_state = 0x%X, error_count = %d, "
                         "status = %hhd, error_code = %hhd, last_http_status = %hhd, pad1 = %hhd, tx_instance = %hd, num_sub_sample = %hd, " 
                         "tx_name_len = %hd, tx_name = %*.*s, duration = %d, think_time = %d, num_pages = %hd",
                          sample_num, jm_txrc->running_vusers, jm_txrc->req_tot_bytes, jm_txrc->start_time, 
                          jm_txrc->end_time, jm_txrc->transaction_id, jm_txrc->session_instance, jm_txrc->duration, jm_txrc->req_tot_bytes,
                          jm_txrc->rep_tot_bytes, jm_txrc->total_edge_time, jm_txrc->total_origin_time, jm_txrc->nw_cache_state, 
                          jm_txrc->error_count, jm_txrc->status, jm_txrc->error_code, jm_txrc->last_http_status, jm_txrc->pad1, 
                          jm_txrc->tx_instance,jm_txrc->num_sub_sample, jm_txrc->tx_name_len, jm_txrc->tx_name_len, 
                          jm_txrc->tx_name_len, jm_txrc->tx_name, duration, think_time, jm_txrc->num_pages);
  
   // Based on test result think_time is always coming in milliseconds so we are forcing it to 0, till the solution is found
   // We have kept it in debug log above to debug the issue later.
   think_time = 0;

  // update running vuser
  jmeter_set_vuser(jm_txrc->running_vusers, sgrp_id);
    
  // Get Transaction norm_id
  jmeter_get_tx_norm_id(mccptr, &tx_norm_id, jm_txrc->transaction_id);
  if(tx_norm_id == -1)
  {
     NSTL1(NULL, NULL, "Failed to get trans_norm_id for tx_id = %d, tx_name = %s", jm_txrc->transaction_id, jm_txrc->tx_name);
     return; // This shouldn't happen
  }

  // Update Single TxStats
  jmeter_update_single_txdata(jm_txrc, sgrp_id, mccptr, sqrt_duration, tx_norm_id, duration, think_time);

  // Update Overall groups TxStats
  __jmeter_update_overall_txdata(jm_txrc, sgrp_id, sqrt_duration, duration, think_time);

  // Update Gropwise TxStats
  if (SHOW_GRP_DATA) 
    __jmeter_update_groupwise_txdata(jm_txrc, sgrp_id, sqrt_duration, duration, think_time);

  if (JM_LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  {
    NSDL3_TRANS(vptr, NULL, "Call log_tx_record_v2 function, tx_norm_id = %d", tx_norm_id);
    // We need record in Transaction Record table for mapping with Transaction table
    /*
       SELECT round(avg(TransactionRecord_138079.ThinkDuration)) AS "Avg Think Time", count(*) AS Total 
       FROM TransactionTable_138079, TransactionRecord_138079 
       WHERE TransactionTable_138079.TransactionName = 'Tx1' 
       AND TransactionRecord_138079.TransactionIndex = TransactionTable_138079.TransactionIndex

    */
    if (jmeter_log_tx_record_v2(mccptr, jm_txrc, tx_norm_id) == -1)
      NSTL1_OUT(NULL, NULL, "JMeter - Error in logging the tx record"); 
 }
}

static void jmeter_process_new_object(JMeterMsgHdr *jm_msg, uint32_t sgrp_id, Msg_com_con *mccptr)
{
  JMeterNewObjRec *jm_newobj = (char *)jm_msg + sizeof(JMeterMsgHdr);
  int32_t tx_hashcode;
  NSTL1(NULL, NULL, "JMeterRecord_NewObjRec, obj_type = %d, obj_name_len = %d, obj_name = %*.*s",
                             jm_newobj->obj_type, jm_newobj->obj_name_len, jm_newobj->obj_name_len, jm_newobj->obj_name_len,
                             jm_newobj->obj_name);

  if(jm_newobj->obj_type == JMETER_OBJECT_PAGE)
  {
    if ((JM_LOG_LEVEL_FOR_DRILL_DOWN_REPORT) || (log_records && (get_max_tracing_level() > 0) && (get_max_trace_dest() > 0) && (run_mode == NORMAL_RUN)))
    {
      // Getting normalized page_id
      JMETER_TO_NORM_PAGE_ID(jm_newobj->obj_id)

      //New page entry writing in slog
      jmeter_log_page_table(mccptr, jm_newobj->obj_name, jm_newobj->obj_id, runprof_table_shr_mem[sgrp_id].sess_ptr->sess_norm_id);
    }
  }
  else if (jm_newobj->obj_type = JMETER_OBJECT_TRANSACTION)
  {
    // Get transaction hash code, by transaction name
    ns_jmeter_add_new_tx(mccptr, jm_newobj->obj_name, &tx_hashcode, jm_newobj->obj_name_len, jm_newobj->obj_id);
  }
  else
  {
    NSTL1(NULL, NULL, "Error: Invalid object (%d) in the message", jm_newobj->obj_type);
  } 
      
}
 
/*********************************************************************************
* Name    :  collect_data_from_jmeter_listener

* Purpose :  This function will read data of JMeter from the Java Listener 
             on a specified port and store the data in a buffer. Then the 
             buffer is passed to process_jmeter_sample() where processing
             of data is done.
             Each line of data will be a complete one stat data.
             For Ex : 0|1534073060982|1|1|0|0|0|0|0|0|0|0|174|2826
             Graph_Num|TimeStamp|Running Vusers|Active Vusers|Thinking Vusers|
             Waiting Vusers|Idling Vusers|Blocked Vusers|Number of Connections|
             SyncPoint Vusers|TCP Connections Open/sec|TCP Connections Close/Sec|
             TCP Send Throughput(kbps)|TCP Receive Throughput(kbps)
***********************************************************************************/
int collect_data_from_jmeter_listener(int accept_fd, struct epoll_event *pfds, Msg_com_con *mccptr)
{
  //char tmpbuf[128];
  NSDL2_MISC(NULL, NULL, "Method called, jmeter_fd = %d, mccptr = %p", accept_fd, mccptr);
  VUser *vptr;
  if (pfds->events & (EPOLLERR | EPOLLHUP))
  {
    NSTL1(NULL, NULL, "EPOLLERR or EPOLLHUP found on Jmeter_fd = %d, Stoping Jmeter.", accept_fd);
    ns_jmeter_stop(vptr);
  }
  if (mccptr->vptr == NULL)
  {
    vptr = get_free_user_slot();
    connection *cptr = get_free_connection_slot(vptr);
    vptr->last_cptr = cptr; 
    mccptr->vptr = vptr;
    MY_MALLOC_AND_MEMSET(cptr->cptr_data, sizeof(cptr_data_t), "cptr->cptr_data", 1); 
  }
  // This is done as vptr is required in MACRO LOG_LEVEL_FOR_DRILL_DOWN_REPORT 
  vptr = mccptr->vptr;

  while(1)
  {
    JMeterMsgHdr *msg;
    int rcv_amt;

    msg = (JMeterMsgHdr *)read_msg(mccptr, &rcv_amt, CONTROL_MODE);

    if (msg == NULL) return 0;
   
    NSDL3_MESSAGES(NULL, NULL, "JMeterMsgHdr: msg_size = %u, opcode = %hd, version = %hhd, "
                   "flags = 0x%02x, user_id = %d, sample_num = %d", 
                   msg->msg_size, msg->opcode, msg->version, msg->flags, msg->user_index, msg->sample_num); 

    switch (msg->opcode)
    {
      case JMETER_OPCODE_URL_REC:  // This is for both page and url
        jmeter_process_rec_url(msg, mccptr->sgrp_id, mccptr);
        break;          
      case JMETER_OPCODE_TX_REC:
        jmeter_process_rec_tx(msg, mccptr->sgrp_id, mccptr);
        break;
      case JMETER_OPCODE_PAGE_DUMP_REC:
        jmeter_log_page_dump_record(msg, mccptr->sgrp_id, mccptr);
        break;
      case JMETER_OPCODE_SESS_REC:
        jmeter_process_rec_sess(msg, mccptr->sgrp_id, mccptr);
       break;
      case JMETER_HEART_BEAT:
        NSDL1_MESSAGES(NULL, NULL,"Received JMETER HEARTBEAT message.. no action to be performed");
        break;
      case JMETER_OPCODE_NEW_OBJECT:  
        jmeter_process_new_object(msg, mccptr->sgrp_id, mccptr);
        break;      
      default:
        NSTL1(NULL, NULL, "Error: Invalid opcode in the message from jmeter. Opcode = %d", msg->opcode);
        // TODO - what to do. Close connect. then jmeter should reconnect??
        break;        
    }
  }
}


 
#if 0 
void update_jmeter_avgtime_size()
{
  int jmeter_avg_size = sizeof(jmeter_avgtime);

  NSDL1_PARENT(NULL, NULL, "Method Called, g_avgtime_size = %d, g_jmeter_avgtime_idx = %d, jmeter_avg_size = %d",
                        g_avgtime_size, g_jmeter_avgtime_idx, jmeter_avg_size);

  if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED) 
  {
    g_jmeter_avgtime_idx = g_avgtime_size;
    g_avgtime_size += jmeter_avg_size;
  } 
  else
    NSDL2_PARENT(NULL, NULL, "JMeter Protocol is disabled");

  NSDL2_PARENT(NULL, NULL, "Method exit, Updated g_avgtime_size = %d, g_jmeter_avgtime_idx = %d",
                        g_avgtime_size, g_jmeter_avgtime_idx); 
}

void ns_jmeter_set_avgtime_ptr()
{
  NSDL2_MISC(NULL, NULL, "g_jmeter_avgtime_idx = %d", g_jmeter_avgtime_idx);

  if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)
  {
    g_jmeter_avgtime = (jmeter_avgtime*)((char *)average_time + g_jmeter_avgtime_idx);
  }

  NSDL2_MISC(NULL, NULL, "g_jmeter_avgtime = %p", g_jmeter_avgtime);
}
#endif

void ns_jmeter_user_cleanup(VUser *vptr)
{
  jmeter_attr_t *jm = vptr->httpData->jmeter;
  NSDL4_API(vptr, NULL, "Method called, vptr = %p", vptr);

  if(jm->cmd_buf != NULL)
  {
    NSDL2_MISC(vptr, NULL, "vptr->httpData->jmeter->cmd_buf");
    FREE_AND_MAKE_NULL_EX(jm->cmd_buf, JMETER_MAX_CMD_LEN + 1, "JMeter cmd buf", -1);
  }
  
  FREE_AND_MAKE_NULL_EX(vptr->httpData->jmeter, sizeof(jmeter_attr_t), "JMeter", -1);
}
// Jmeter script is copied along with all data files including any file to be splited.
// For split files we are making another tar for rel directory.
// This will be untar on the geneartor and overwrite original data files with splited files.
// In case of netstorm scripts, splited files (use once and unique) are not copied with original tar for optimization purpose.
// TODO: We should also find some design to exclude splited file from the script tar.

// data file can be optionally have path relative to the script.
//  For Example:: data_dir/login_ids.csv


static void jmeter_split_data_file(char *script_name, int sgrp_idx, char *data_file)
{
  int  l, gen_idx;
  FILE *orig_file, *divide_file;
  char buffer[JMETER_MAX_LINE_SIZE + 1];
  char comp_file_path[1024 + 1]; 
  char comp_file_path_orig[1024 + 1]; 
  char data_file_with_PS[1024 +1];
  char cmd[1024];
  char err_msg[1024] = "\0";
  int retsys;
  int num_generator = scen_grp_entry[sgrp_idx].num_generator;

 
  NSTL1(NULL, NULL, "Method called for splitting data_file = %s for script = %s", data_file, script_name);

  // TODO : Need to identify the logic how to detect the directory is created
  //if(is_gen_directory_created == 0) // Do mkdir once per JMeter script as many files in one script will be split
  {
    // Loop through all generated used for this script (NOT all generators)
    //PS is poj/sub_proj
    sprintf(data_file_with_PS, "%s/%s/scripts/%s/%s", scen_grp_entry[sgrp_idx].proj_name, scen_grp_entry[sgrp_idx].sub_proj_name, scen_grp_entry[sgrp_idx].sess_name, data_file);
    //dirname return the path of the file and updates the input by replacing last '/' by null
    char *data_file_dir = dirname(data_file_with_PS);


    for(gen_idx = 0; gen_idx < num_generator; gen_idx++)
    {
      int gidx = scen_grp_entry[sgrp_idx].generator_id_list[gen_idx];
      // data_files dir contains files having relative and are per generator
      sprintf(cmd, "mkdir -p %s/logs/TR%d/.controller/%s/rel/%s", g_ns_wdir, testidx, 
                    generator_entry[gidx].gen_name, data_file_dir);

      NSDL2_MISC(NULL, NULL, "Running command to make data_files directory. command = %s", cmd);
      retsys = nslb_system(cmd, 1, err_msg);
      if(retsys != 0)
      {
        NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
      }
    }
    //is_gen_directory_created = 1;
  }
  
  //reading each file and counting total number of lines in it
  int data_file_line_count = 0, data_file_line_count_for_all, left_over;

  // making data_file_with_PS again because got changed while using dirname.
    sprintf(data_file_with_PS, "%s/%s/scripts/%s/%s", scen_grp_entry[sgrp_idx].proj_name, scen_grp_entry[sgrp_idx].sub_proj_name, scen_grp_entry[sgrp_idx].sess_name, data_file);
  sprintf(comp_file_path_orig, "%s/%s/%s",  g_ns_wdir, GET_NS_RTA_DIR(), data_file_with_PS);
  orig_file = fopen(comp_file_path_orig, "r");
  if(orig_file == NULL)
  {
    NS_EXIT(-1, CAV_ERR_1000006, comp_file_path_orig, errno, nslb_strerror(errno));
  }

  // Reading number of Data Lines
  char ch;
  while(!feof(orig_file))
  {
    ch = fgetc(orig_file);
    if(ch == '\n')
    {
      data_file_line_count++;
    }
  }

  // Need to rewind the file as it will required in later part
  rewind(orig_file);

  // Calculate data lines to be distributed and left over
  data_file_line_count_for_all = data_file_line_count/num_generator;
  left_over = data_file_line_count%num_generator;
  
  
  for(gen_idx = 0; gen_idx < num_generator; gen_idx++)
  {
    int line_counts = data_file_line_count_for_all;
    if (gen_idx < left_over)
      line_counts++;
    
    int gidx = scen_grp_entry[sgrp_idx].generator_id_list[gen_idx];

    // Make data file path in generator specific directory
    //data_file contain proj/sub_proj/script/Data/file_name
    sprintf(comp_file_path, "%s/logs/TR%d/.controller/%s/rel/%s", g_ns_wdir, testidx, generator_entry[gidx].gen_name, data_file_with_PS);


    NSDL2_MISC(NULL, NULL, "comp_file_path = %s", comp_file_path);
    
    // If same script is used in mutiple sgrps, then this file may be already available
   #if 0
    int fd = open(comp_file_path,O_CREAT|O_EXCL); //with O_CREAT|O_EXCL
    if(fd < 0)
    {  
      //if already exits trace log with Warning and all details and continue
      if(errno != EEXIST)
      {
        NS_EXIT(-1, CAV_ERR_1000006, comp_file_path, errno, nslb_strerror(errno));
      } 
      NSTL1(NULL, NULL,"File %s already exist. So Skipping......", comp_file_path);
      continue;
      /* TO BE DISCUSS: AS WE ARE NOT DIVIDING THE DATA ON A GENERATOR WHERE THE DATA FILE ALREADY EXISTS.
               THIS WILL CREATE A PROBLEM IN DATA DISTRIBUTION WHERE MULTIPLE GROUPS ARE SHARING
               SAME GENERATOR
         EXAMPLE       SGRP G1 GEN1           SCRIPT(JMX WITH DATA FILE 1)
                       SGRP G2 GEN2           SCRIPT(JMX WITH DATA FILE 2)
                       SGRP G3 GEN1,GEN2,GEN3 SCRIPT(JMX WITH DATA FILE 1,2) 
                        
                       FILE 1 - CONTAINS 100 LINE
                       FILE 2 - CONTAINS 200 LINE

     ACTUAL DIVISION   G1 GEN1 - FILE 1 - 100 LINE
                       G2 GEN2 - FILE 2 - 200 LINE
                       G3 GEN1 - FILE 1-skip, FILE2 - 67 
                       G3 GEN2 - FILE 2-skip, FILE1 - 34
                       G3 GEN3 - FILE 1-34,   FILE2 - 67 */
    }
     
    divide_file = fdopen(fd, "a"); // open in append mode and create is not existing
    #endif
    divide_file = fopen(comp_file_path, "a"); // open in append mode and create is not existing

    if(divide_file == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, comp_file_path, errno, nslb_strerror(errno));
    } 

    for(l = 0; l < line_counts; l++)
    {
      // ERROR CONDITION ON BELOW CALLS
      //    It can impact performance - TBD
      fgets(buffer, JMETER_MAX_LINE_SIZE, orig_file);
      fputs(buffer, divide_file);
    }  
    fclose(divide_file);
  }    
  fclose(orig_file);
 
}
// i is scen grp index
static void jmeter_distribute_threads(int sgrp_idx, char *script_name_orig, int total_thread_grps, int *num_threads)
{

  char script_name[JMETER_MAX_CMD_LEN + 1];
  int tg_idx; // Thread group index in num_theads
  int gen_idx; // Generator index
  int ret;
  char cmd[1024]; 
  int num_generator = scen_grp_entry[sgrp_idx].num_generator;
  char buffer[JMETER_MAX_LINE_SIZE + 1];
  int gen_usr_distribution[num_generator][total_thread_grps]; //to store num threads generator wise for each thread group
  char err_msg[1024] = "\0";
  FILE *orig_jmx_file_fp;
  FILE *new_jmx_file_fp;
  char *tmp_ptr;
  char *ptr; 

 
  memset(gen_usr_distribution, 0, num_generator * total_thread_grps * sizeof(int)); 

  NSDL2_MISC(NULL, NULL, "num_generator = %d, total_thread_grps = %d", num_generator, total_thread_grps);
 
  /* Dividing number of users per thread group according to number of generators*/
  for(tg_idx = 0; tg_idx < total_thread_grps; tg_idx++)
  {
    int thread_count, thread_count_for_all, left_over;

    thread_count = num_threads[tg_idx];
    if(thread_count < num_generator)
    {
      NS_EXIT(1, CAV_ERR_1014023, thread_count, tg_idx, sgrp_used_genrator_entries);
    }

    thread_count_for_all = thread_count / num_generator;

    left_over = thread_count % num_generator;
    NSDL2_MISC(NULL, NULL, "Thread count for all = %d, left_over = %d", thread_count_for_all, left_over);
    
    for(gen_idx = 0; gen_idx < num_generator; gen_idx++)
    {
      gen_usr_distribution[gen_idx][tg_idx] = thread_count_for_all;
      NSDL2_MISC(NULL, NULL, "gen_usr_distribution[%d][%d] = %d", gen_idx, tg_idx, gen_usr_distribution[gen_idx][tg_idx]);

      if(gen_idx < left_over) // Add one more for left over
        gen_usr_distribution[gen_idx][tg_idx]++;
    }
  }
  /*Making new directory inside .controller to store jmx files*/  
  for(gen_idx = 0; gen_idx < num_generator; gen_idx++)
  {
    int gidx = scen_grp_entry[sgrp_idx].generator_id_list[gen_idx];
     
    // Todo : Can we avoid dir if it is already created ???
    sprintf(cmd, "mkdir -p %s/logs/TR%d/.controller/%s/rel/%s/%s/scripts/%s", 
                  g_ns_wdir, testidx, generator_entry[gidx].gen_name,
                  scen_grp_entry[sgrp_idx].proj_name, scen_grp_entry[sgrp_idx].sub_proj_name, scen_grp_entry[sgrp_idx].sess_name);

    NSDL2_MISC(NULL, NULL, "Creating directory to store jmx files generator wise, cmd = %s", cmd);

    ret = nslb_system(cmd, 1, err_msg);
    if(ret != 0)
    {
      NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
    }

    // Need JMX file name. Using basename - Script/JMSFILENAME.jmx
    snprintf(script_name, JMETER_MAX_CMD_LEN, "%s/logs/TR%d/.controller/%s/rel/%s/%s/scripts/%s/%s", 
                          g_ns_wdir, testidx, generator_entry[gidx].gen_name, scen_grp_entry[sgrp_idx].proj_name, 
                          scen_grp_entry[sgrp_idx].sub_proj_name, scen_grp_entry[sgrp_idx].sess_name, basename(script_name_orig));

    NSDL2_MISC(NULL, NULL, "jmx files path inside .controller directory = %s", script_name);

    orig_jmx_file_fp = fopen(script_name_orig, "r");
    if(orig_jmx_file_fp == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, script_name_orig, errno, nslb_strerror(errno));
       
    } 

    /*Opening new jmx file per generator and writing divided user values into it */  
   
   #if 0
   previously we were doing open and then fdopen but we were getting system error 22.
   so for the time being we are using fopen.

    int fd = open(script_name, O_CREAT|O_EXCL, 0666);

    if(fd < 0)
    {
     //if already exits trace log with Warning and all details and continue
      if(errno != EEXIST)
      {
        NS_EXIT(-1, CAV_ERR_1000006, script_name, errno, nslb_strerror(errno));
      } 
      NSTL1(NULL, NULL,"File %s already exist. So Skipping......", script_name);
      continue;
    } 
    new_jmx_file_fp = fdopen(fd, "a"); 
    #endif
    
    new_jmx_file_fp = fopen(script_name, "a"); 
    if(new_jmx_file_fp == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, script_name, errno, nslb_strerror(errno));
    }

    tg_idx = 0;
    while(fgets(buffer, JMETER_MAX_LINE_SIZE, orig_jmx_file_fp) != NULL)
    {
      if((ptr = strstr(buffer, "ThreadGroup.num_threads")) != NULL && (tmp_ptr = strstr(buffer, "</stringProp>")) != NULL)
      {
        //Now update qty of gen on jmx files of generator
        // <stringProp name="ThreadGroup.num_threads">10</stringProp>
        /* Reach to the endof "ThreadGroup.num_threads"> and 
           Put "ThreadGroup.num_threads" in file and save the divided value of threads group
           Append the string </stringProp> at the end */
        ptr = ptr + sizeof("ThreadGroup.num_threads\">");
        *(--ptr) = NULL;
        fputs(buffer, new_jmx_file_fp);
        fprintf(new_jmx_file_fp, "%d", gen_usr_distribution[gen_idx][tg_idx]);
        fputs(tmp_ptr, new_jmx_file_fp); // Ensure we have new line in the file
        NSDL2_MISC(NULL, NULL, "buffer: [%s] tmp_ptr: [%s] gen_usr_distribution: [%d]", buffer, tmp_ptr, gen_usr_distribution[gen_idx][tg_idx]);
        tg_idx++;
      }
      else
        fputs(buffer, new_jmx_file_fp);
    }
    fclose(orig_jmx_file_fp);
    fclose(new_jmx_file_fp);
  }
}

/*****************************************************************************************************
* Name          : ns_copy_jmeter_scripts_to_generator

* Purpose       : This function will find out number of users from provided jmx file, divide it according
                  to number of generators and then write the divided jmx file inside controller's  
                  NS_WDIR/logs/TRXXXX/.controller/gen_name/jmx_files/proj_name/subproj_name/script_name

                  
Case 1 - unique script per sgrp
G1  Gen1   Script1
G1  Gen2   Script1
G2  Gen1   Script2
G2  Gen2   Script2

Case 2 - same script in multiple sgrp
G1  Gen1   Script1
G1  Gen2   Script1
G2  Gen1   Script1
G2  Gen2   Script1

Case 3 - Different group in different generators
G1  Gen1   Script1
G1  Gen2   Script1
G2  Gen3   Script2
G2  Gen4   Script2
******************************************************************************************************/
#define DELTA_THREAD_GROUP_ENTRY 10

//static is_gen_directory_created = 0;
void ns_copy_jmeter_scripts_to_generator()
{
  char buffer[JMETER_MAX_USAGE_LEN + 1];
  char buffer1[JMETER_MAX_USAGE_LEN + 1];
  char script_name_orig[JMETER_MAX_JTL_FNAME_LEN + 1];
  char jmx_session_name[JMETER_MAX_JTL_FNAME_LEN + 1];

  FILE *orig_jmx_file_fp;
  int *num_threads = NULL; // Array of Thread count of each Thread Group used in jmeter script
  char *tmp_ptr;
  char *ptr;
  int  i;
  int total_thread_grps;
  int max_thread_grps = 0;
  int csvdatasetfound = 0;
  jmeter_vusers_csv_settings *jmeter_vusers_csv = &g_jmeter_vuser_csv; 
  
  if((ptr = getenv("JMETER_HOME")) == NULL)
  {
    NSTL1(NULL, NULL, "Error: JMETER_HOME is not set or JMeter is not installed");
    NS_EXIT(1, "Error: JMETER_HOME is not set or JMeter is not installed.");
  }

  //divide every scripts and jxm user values with total generators used.
  for(i = 0; i < total_sgrp_entries; i += scen_grp_entry[i].num_generator)
  {
    total_thread_grps = 0;  // Reset for each JMeter script 
    //is_gen_directory_created = 0;
    NSDL2_MISC(NULL, NULL, "proj_name = %s, sub_proj_name = %s, script_name = %s", scen_grp_entry[i].proj_name,
                            scen_grp_entry[i].sub_proj_name, scen_grp_entry[i].sess_name);

    //skipping group which is non jmx
    if(scen_grp_entry[i].script_or_url != JMETER_TYPE_SCRIPT)
    {
      NSDL2_MISC(NULL, NULL, "Skipping as it not a JMeter Script ,script_name = %s, scen_grp_entry[i].script_or_url = %d", 
                 scen_grp_entry[i].sess_name,scen_grp_entry[i].script_or_url);
      continue;
    }

    // Get JMeter main jmx file (it is taken from the .Main file in the script directory)
    // TODO (later) - why this is NOT same as script name as we are creating script with same name as JMX name
    get_jmeter_script_name(jmx_session_name, scen_grp_entry[i].proj_name, scen_grp_entry[i].sub_proj_name, scen_grp_entry[i].sess_name);

    NSDL2_MISC(NULL, NULL, "Getting jmx_session_name = %s", jmx_session_name);
 
    snprintf(script_name_orig, JMETER_MAX_JTL_FNAME_LEN, "%s/%s/%s/%s/scripts/%s", 
                               g_ns_wdir, GET_NS_RTA_DIR(), scen_grp_entry[i].proj_name, scen_grp_entry[i].sub_proj_name, jmx_session_name);
  
    NSDL2_MISC(NULL, NULL, "Created Script name script_name = %s, sgrp_used_genrator_entries = %d",
                          script_name_orig, sgrp_used_genrator_entries);

    orig_jmx_file_fp = fopen(script_name_orig, "r");    
    if(orig_jmx_file_fp == NULL) 
    {
      NS_EXIT(-1, CAV_ERR_1000006, script_name_orig, errno, nslb_strerror(errno));
    }
  
    while(fgets(buffer, JMETER_MAX_USAGE_LEN, orig_jmx_file_fp) != NULL)
    {
      // TODO P3 BUG (need to handle soon) - what if these are in 2 different lines
      if((ptr = strstr(buffer, "ThreadGroup.num_threads")) != NULL && (tmp_ptr = strstr(buffer, "</stringProp>")) != NULL)
      { 
        // Notes - It is not necessary that ThreadGroup will come before CSV Data Set. So do assume in the code
        //         Also CSVData are used by many thread groups
	// Refer to https://www.perfmatrix.com/sharing-mode-of-csv-data-set-config-in-jmeter/
        if(jmeter_vusers_csv->is_vusers_split)
        {
          // <stringProp name="ThreadGroup.num_threads">${__P(threads,100)}</stringProp> 
          // <stringProp name="ThreadGroup.num_threads">123456</stringProp>
          // Copy number of threads (count) in buffer1
          snprintf(buffer1, (tmp_ptr-(ptr+24)), "%s", ptr+25);
          ptr = buffer1;
          CLEAR_WHITE_SPACE(ptr);// Clearing white spaces if any
          if(total_thread_grps == max_thread_grps)
          {
            max_thread_grps += DELTA_THREAD_GROUP_ENTRY;
            MY_REALLOC(num_threads, max_thread_grps * sizeof(int), "thread group table", 1);
          }
          if(ptr[0] != '\0') 
          {
            int tmpthreadgrp;
            if(nslb_atoi(ptr, &tmpthreadgrp) < 0)
            {
               NSTL1(NULL, NULL, "ThreadGroup num threads should be numeric. Exiting..");
               NS_EXIT(-1, "ThreadGroup.numThread(%s) should be numeric", ptr);
            }
            num_threads[total_thread_grps++] = tmpthreadgrp;      
          }
          else
          {
            NSTL1(NULL, NULL, "ThreadGroup num threads shouldn't be Empty. Exiting..");
            NS_EXIT(-1, "ThreadGroup.numThread(%s) shouldn't be Empty", ptr);
          }
          NSDL2_MISC(NULL, NULL, "thread save value = %d, and total thread groups = %d",
                                num_threads[total_thread_grps-1], total_thread_grps);
        }

      }
      
      // Before April, 2021, we were assuming if  shareMode.thread is set to shareMode.group or shareMode.all, then it unique or use once mode
      // recycle = false is indicating use once
      // But customer are not always using it like this in case and this will not work  in case of generators.
      // So we added split option using keyword
      else if((ptr = strstr(buffer, "testclass=\"CSVDataSet\"")) != NULL && (tmp_ptr = strstr(buffer, "enabled=\"true\"")) != NULL)
      {
        csvdatasetfound = 1;  //Setting if CSVDataSet is found.
      }

      /*If file parameter for a particular thread is found then here we are doing following things
        - Storing file name that is used for file parameter
        - Counting each file data line and storing as per thread 
    
        Example - File name is file1.csv
        buffer1 : file1.csv
        buffer2 : <project>/<subproject>/file_parameter/file1.csv*/

      else if((csvdatasetfound == 1) && ((ptr = strstr(buffer, "filename")) != NULL && (tmp_ptr = strstr(buffer, "</stringProp>")) != NULL))
      {
        snprintf(buffer1, (tmp_ptr-(ptr+9)), "%s", ptr+10); // Copy filename in the buffer1
        NSDL4_MISC(NULL, NULL, "Data File Name retrieve %s, len %d", buffer1, strlen(buffer1));
        ptr = buffer1;
        CLEAR_WHITE_SPACE(ptr); // Clearing white space if any
        if(ptr[0] == '/')
        {
          NSTL1(NULL, NULL, "Error: Absolute Data File Path(%s) not supported in Jmeter", ptr);
          NS_EXIT(-1, CAV_ERR_1011372, ptr);
        }
        // Check if file is to be split. If yes, we need to split and copy in generator specific dir
        // If not to be split, these files will be aleady part of script tar. So no need to copy.
        if((jmeter_vusers_csv->csv_file_split_mode == JMETER_CSV_DATA_SET_SPLIT_ALL_FILES) ||
          ((jmeter_vusers_csv->csv_file_split_mode == JMETER_CSV_DATA_SET_SPLIT_FILE_WITH_PATTERN) &&
           (tmp_ptr = strstr(ptr, jmeter_vusers_csv->csv_file_split_pattern) != NULL)))
        {
          NSDL4_MISC(NULL, NULL, "Data File Name used in JMeter script= [%s]", ptr);
          jmeter_split_data_file(scen_grp_entry[i].sess_name, i, ptr);
        }
        else
        {
          NSDL1_MISC(NULL, NULL, "CSV Data will not be divided as split mode is set to %d",
                     jmeter_vusers_csv->csv_file_split_mode);
        }
        csvdatasetfound = 0;
      }
    }

    fclose(orig_jmx_file_fp);

    if(jmeter_vusers_csv->is_vusers_split)
      jmeter_distribute_threads(i, script_name_orig, total_thread_grps, num_threads);
    else
    {
      // TODO - if we not distributing, do we need to copy jms file or not - SHOULD NOT
      NSDL1_MISC(NULL, NULL, "ThreadGroup NumThreads will not be divided as split mode is set to %d",
                     jmeter_vusers_csv->is_vusers_split);
    }
   
  } // End of For Loop
}

void add_socket_fd(int epfd, char* data_ptr, int fd, int event)
{
  struct epoll_event pfd;
  NSDL1_MESSAGES(NULL, NULL, "Method called. Adding fd = %d for event = %d, epfd  = %d , data_ptr = %p", fd, event, epfd, data_ptr);
  bzero(&pfd, sizeof(struct epoll_event));

  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;
  //pfd.data.fd = fd;

  NSDL1_MESSAGES(NULL, NULL, "pfd.data.ptr = %p", pfd.data.ptr);

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured in child process[%d], add_select_parent() - with fd %d EPOLL_CTL_ADD: err = %s", my_port_index, fd, nslb_strerror(errno));
    NSTL1(NULL, NULL, "\nEPOLL ERROR occured in child process[%d], add_select_parent() - with fd %d EPOLL_CTL_ADD: err = %s\n", my_port_index, fd, nslb_strerror(errno));
    NS_EXIT(-1, "EPOLL ERROR occured in child process[%d], add_select_parent() - with fd %d EPOLL_CTL_ADD: err = %s", my_port_index, fd, nslb_strerror(errno));
  }
}

/**
 * Called from NVM
 * Collect data coming from JMeter (Cavisson Java Agent)
 **/
int create_jmeter_listen_socket(int epfd, Msg_com_con *mccptr, int con_type, int grp_num)
{
  int listen_fd;
  char err_msg[1024]="\0";
  int port = 0;

  NSDL1_MESSAGES(NULL, NULL, "Method Called, group_num = %d, users = %d, jmeter_idx = %d", 
                             grp_num, global_settings->num_connections, global_settings->jmeter_idx);

  if((listen_fd = nslb_tcp_listen(port, 10000, err_msg)) < 0)
  {
    NSTL1_OUT(NULL, NULL, "Error in creating JMeter TCP listen socket.\n");
  }

  if(!port) {
    // Get the JMeter port allocated by the system
    struct sockaddr_in sockname;
    socklen_t socksize = sizeof (sockname);
    if (getsockname(listen_fd, (struct sockaddr *)&sockname, &socksize) < 0)
      {
        NSTL1(NULL, NULL, "getsockname error ");
        perror("getsockname error ");
        NS_EXIT(-1, "getsockname error ");
      }
    port = ntohs(sockname.sin_port);
    //global_settings->jmeter_port[global_settings->jmeter_idx++] = port;
    g_jmeter_ports[grp_num] = port;

    //NSDL2_MESSAGES(NULL, NULL, "JMeter port = %d, gport = %d, group_num = %d, jmeter_idx = %d", 
      //                          port, global_settings->jmeter_port[global_settings->jmeter_idx - 1], my_port_index, global_settings->jmeter_idx);

    NSDL2_MESSAGES(NULL, NULL, "JMeter port = %d, gport = %d, my_port_index = %d, grp_num = %d", 
                                 port, g_jmeter_ports[grp_num], 
                                 my_port_index, grp_num);

  }
  
  memset(mccptr, 0, sizeof(Msg_com_con));
  mccptr->sgrp_id = grp_num;
  mccptr->con_type = con_type;
  mccptr->fd = listen_fd;
  mccptr->write_offset = listen_fd;
  mccptr->flags |= (NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN + NS_MSG_COM_DO_NOT_CALL_EXIT);
  MY_MALLOC(mccptr->read_buf, 4096, "JMeterReadBuf", -1);
  mccptr->read_buf_size = 4096;
  NSDL3_SCHEDULE(NULL, NULL, "mccptr->fd = %d, listen_fd = %d, mccptr->con_type = %c", mccptr->fd , listen_fd, mccptr->con_type);

  add_socket_fd(epfd, (char*)mccptr, listen_fd, EPOLLIN); 
  return listen_fd;
}
