#include <string.h>

#include "cdr_config.h"
#include "cdr_mem.h"
#include "cdr_log.h"
#include "cdr_utils.h"
#include "cdr_main.h"
#include "nslb_json_parser.h"
#include "nslb_util.h"


#define NUM_FIELDS 1000

struct cdr_config_struct cdr_config; // global var that contains all the configuration settings
static void cdr_config_init()
{
  /* set default values for config
   */
  CDRTL2("Method called");

  cdr_config.enable = CDR_DISABLE;
  cdr_config.mode = MODE_LOCAL;
  cdr_config.tr_num = 0;
  cdr_config.cleanup_flag = CDR_FALSE;
  cdr_config.cleanup_all_tr_flag = CDR_FALSE;
  memset (cdr_config.cleanup_all_tr, -1, sizeof (cdr_config.cleanup_all_tr));
  memset (cdr_config.cleanup, -1, sizeof (cdr_config.cleanup_all_tr));
  cdr_config.custom_cleanup = NULL;
  cdr_config.total_custom_cleanup = 0;

  CDRTL2("Method exit");
}

static inline int convert_to_days(char *time)
{
  // Input: 2d, 2w, 2m
  int len;
  int days;
  char ch;
  
  CDRTL2("Method called");

  len = strlen(time);
  ch = time[len - 1];
  time[len - 1] = '\0'; // removing the last char (d, w, m)

  switch(ch) {
    case 'd':
      days = atoi(time);
      break;
    case 'w':
      days = atoi(time) * 7;
      break;
    case 'm':
      days = atoi(time) * 30;
      break;
    case 'y':
      days = atoi(time) * 365;
      break;
    default:
      days = atoi(time); // TODO: Handle error;
  }
  
  CDRTL2("Method exit");
  return days;
}

int get_component(char *name)
{
  CDRTL2("Method called");

  if(strcmp(name, "raw_data") == 0)
    return RAW_DATA;
  else if(strcmp(name, "csv") == 0)
    return CSV;
  else if(strcmp(name, "logs") == 0)
    return LOGS;
  else if(strcmp(name, "db") == 0)
    return DB;
  else if(strcmp(name, "tr") == 0)
    return PERF_TR;
  else if(strcmp(name, "dbg_tr") == 0)
    return DBG_TR;
  else if(strcmp(name, "arch_tr") == 0)
    return ARCH_TR;
  else if(strcmp(name, "gen_tr") == 0)
    return GEN_TR;
  else if(strcmp(name, "graph_data") == 0)
    return GRAPH_DATA;
  else if(strcmp(name, "har_file") == 0)
    return HAR_FILE;
  else if(strcmp(name, "pagedump") == 0)
    return PAGEDUMP;
  else if(strcmp(name, "test_data") == 0)
    return TEST_DATA;
  else if(strcmp(name, "db_agg") == 0)
    return DB_AGG;
  else if(strcmp(name, "ocx") == 0)
    return OCX;
  else if(strcmp(name, "na_traces") == 0)
    return NA_TRACES;
  else if(strcmp(name, "access_log") == 0)
    return ACCESS_LOG;
  else if(strcmp(name, "reports") == 0)
    return REPORTS;
  else if(strcmp(name, "configs") == 0)
    return CONFIGS;
  else 
    return CDR_ERROR; 

  CDRTL2("Method exit");
}

static void parse_custom_clean_path_array(nslb_jsont *json_tree, char *json_tag)
{
  nslb_jsont *node1;
  nslb_jsont *node2;
  char name[CONF_BUFF_SIZE];
  int retention_time;

  CDRTL2("Method called");

  // getting json array elements 1 by 1
  for(node1 = json_tree->children; node1 != NULL; node1 = node1->next_sib) {
    // gettting name and time (cleanup_info)
    for(node2 = node1->children; node2 != NULL; node2 = node2->next_sib) 
    {
      if(strcmp(node2->name, "name") == 0) {
        strcpy(name, node2->value);
      } 
      else if(strcmp(node2->name, "time") == 0) {
        retention_time = convert_to_days(node2->value);
      }
      else {
        CDRTL1("Error: Wrong json tag in cleanup array, ignoring. tag: %s", node2->name);
      }
    }

    CDR_REALLOC(cdr_config.custom_cleanup, (cdr_config.total_custom_cleanup + 1) * sizeof(struct custom_cleanup_struct), "cdr_config.custom_cleanup");
    CDR_MALLOC(cdr_config.custom_cleanup[cdr_config.total_custom_cleanup].path, strlen(name) + 1, "cdr_config.custom_cleanup[cdr_config.total_custom_cleanup].path");
    strcpy(cdr_config.custom_cleanup[cdr_config.total_custom_cleanup].path, name);
    cdr_config.custom_cleanup[cdr_config.total_custom_cleanup].retention_time = retention_time;
    cdr_config.total_custom_cleanup++;
     
    CDRTL3("Added to '%s' array. Path: '%s', retention_time: '%d' days", json_tag, name, retention_time);
  }

  CDRTL2("Method exit");
}

char component_name[TOTAL_COMPONENTS][32] = {
  "raw_data", "csv", "logs", "db", "tr", "dbg_tr", "arch_tr", "graph_data", "har_file", 
  "pagedump", "test_data", "db_agg", "ocx", "na_traces", "access_log"};

static void parse_cleanup_array(nslb_jsont *json_tree, struct cleanup_struct *cleanup_ptr, char *json_tag)  
{
  nslb_jsont *node1;
  nslb_jsont *node2;
  char name[CONF_BUFF_SIZE];
  int retention_time;
  int recycle_bin_time;
  char *cPtr, *cPtr2;

  CDRTL2("Method called");
  // getting json array elements 1 by 1
  for(node1 = json_tree->children; node1 != NULL; node1 = node1->next_sib) {
    // gettting name and time (cleanup_info)
    for(node2 = node1->children; node2 != NULL; node2 = node2->next_sib) 
    {
      if(strcmp(node2->name, "name") == 0) {
        strcpy(name, node2->value);
      } 
      else if(strcmp(node2->name, "time") == 0) {
        // retention time, recycle bin time
        cPtr = strchr(node2->value, ',');
        if (cPtr){
          cPtr[0] = '\0';
          cPtr++;
          if (cPtr[0] != '\0')
          {
            cPtr2 = strchr(cPtr, ',');
            if (cPtr2)
              cPtr2[0] = '\0';
            recycle_bin_time = convert_to_days(cPtr);
          }
        }
        retention_time = convert_to_days(node2->value);
      }
      else {
        CDRTL1("Error: Wrong json tag in cleanup array, ignoring. tag: %s", node2->name);
      }
    }
  
    // Add to cleanup array
    int cmp = get_component(name);
    if(cmp != CDR_ERROR)
    {
      cleanup_ptr[cmp].retention_time = retention_time;
    }
    else 
      CDRTL1("Error: Wrong component found in json, [%s], Ignoring this component.", name); 

    CDRTL3("Added to %s array. component: %s, retention_time: %d days, recycle_bin_time: %d days",
      json_tag, name, retention_time, recycle_bin_time);
  }

  CDRTL2("Method exit");
}

static void parse_cleanup_ngve_days_array(nslb_jsont *json_tree)
{
  nslb_jsont *node1;
  nslb_jsont *node2;
  char name[CONF_BUFF_SIZE];
  char time[CONF_BUFF_SIZE];
  char *dates[NUM_FIELDS];

  CDRTL2("Method called");
  // getting json array elements 1 by 1
  for(node1 = json_tree->children; node1 != NULL; node1 = node1->next_sib) {
    for(node2 = node1->children; node2 != NULL; node2 = node2->next_sib) {
      if(strcmp(node2->name, "name") == 0) {
        strcpy(name, node2->value);
      } else if(strcmp(node2->name, "time") == 0) {
        strcpy(time, node2->value);
      }else {
        CDRTL1("Error: Wrong json tag in cleanup array, ignoring. tag: %s", node2->name);
        continue;
      }
    }
  
    int cmp = get_component(name);
    if(cmp != CDR_ERROR)
    {
      int total_dates = get_tokens(time, dates, ",", NUM_FIELDS);
      char *c_ptr;

      cdr_config.ngve_cleanup_days[cmp].total_entry = total_dates;
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].start_dates, total_dates * sizeof(char *), "negative cleanup start date");
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].end_dates, total_dates * sizeof(char *), "negative cleanup end date");
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].start_ts, total_dates * sizeof(int), "negative cleanup dates start time stamp");
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].end_ts, total_dates * sizeof(int), "negative cleanup dates end time stamp");
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].start_date_pf, total_dates * sizeof(long long int), "negative cleanup start date");
      CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].end_date_pf, total_dates * sizeof(long long int), "negative cleanup end date");
      
      for(int i = 0; i < total_dates; ++i)
      {
        int len = strlen("MM/DD/YYYY HH:MM:SS") + 1;
        CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].start_dates[i], len, "start date");
        CDR_MALLOC(cdr_config.ngve_cleanup_days[cmp].end_dates[i], len, "end date");

        c_ptr = strchr(dates[i], ':');
        if(c_ptr == NULL) // single date
        {
          sprintf(cdr_config.ngve_cleanup_days[cmp].start_dates[i], "%s 00:00:00", dates[i]);
          sprintf(cdr_config.ngve_cleanup_days[cmp].end_dates[i], "%s 23:59:59", dates[i]);
        }
        else // range date
        {
          *c_ptr = '\0';
          c_ptr++;

          sprintf(cdr_config.ngve_cleanup_days[cmp].start_dates[i], "%s 00:00:00", dates[i]);
          sprintf(cdr_config.ngve_cleanup_days[cmp].end_dates[i], "%s 23:59:59", c_ptr);
        }

        cdr_config.ngve_cleanup_days[cmp].start_ts[i]  = format_date_convert_to_ts(cdr_config.ngve_cleanup_days[cmp].start_dates[i]);
        cdr_config.ngve_cleanup_days[cmp].end_ts[i]  = format_date_convert_to_ts(cdr_config.ngve_cleanup_days[cmp].end_dates[i]);
        change_ts_partition_name_format(cdr_config.ngve_cleanup_days[cmp].start_ts[i], 
                                         cdr_config.ngve_cleanup_days[cmp].start_dates[i], len);
        change_ts_partition_name_format(cdr_config.ngve_cleanup_days[cmp].end_ts[i], 
                                         cdr_config.ngve_cleanup_days[cmp].end_dates[i], len);

        cdr_config.ngve_cleanup_days[cmp].start_date_pf[i] = atoll(cdr_config.ngve_cleanup_days[cmp].start_dates[i]);
        cdr_config.ngve_cleanup_days[cmp].end_date_pf[i] = atoll(cdr_config.ngve_cleanup_days[cmp].end_dates[i]);

        CDRTL2("Negative cleanup date: %s, start = '%s', end = '%s'", dates[i], 
                   cdr_config.ngve_cleanup_days[cmp].start_dates[i], cdr_config.ngve_cleanup_days[cmp].end_dates[i]);
      }
    }
    else
    {
      CDRTL1("Error: Wrong component found in json, [%s], Ignoring this component.", name);
      continue;
    }

    CDRTL3("Added to negative_cleanup_dates . component: %s", name);
  }

  CDRTL2("Method exit");
}

static void parse_ngve_days_array(char * value)
{
  char *dates[NUM_FIELDS];

  CDRTL2("Method called");

  cdr_config.ngve_days.total_entry = 0;
  cdr_config.ngve_days.max_entry = 0;

  int total_dates = get_tokens(value, dates, ",", NUM_FIELDS);
  char *c_ptr;

  for(int i = 0; i < total_dates; ++i)
  {
    if(cdr_config.ngve_days.total_entry >= cdr_config.ngve_days.max_entry)
    {
      cdr_config.ngve_days.max_entry += DELTA_REALLOC_SIZE;
      CDR_MALLOC(cdr_config.ngve_days.start_dates, cdr_config.ngve_days.max_entry * sizeof(char *), "ngve days start dates");
      CDR_MALLOC(cdr_config.ngve_days.end_dates, cdr_config.ngve_days.max_entry * sizeof(char *), "ngve days end dates");
      CDR_MALLOC(cdr_config.ngve_days.start_ts, cdr_config.ngve_days.max_entry * sizeof(int), "ngve days start ts");
      CDR_MALLOC(cdr_config.ngve_days.end_ts, cdr_config.ngve_days.max_entry * sizeof(int), "ngve days end ts");
      CDR_MALLOC(cdr_config.ngve_days.start_date_pf, cdr_config.ngve_days.max_entry * sizeof(long long int), "ngve days start dates");
      CDR_MALLOC(cdr_config.ngve_days.end_date_pf, cdr_config.ngve_days.max_entry * sizeof(long long int), "ngve days end dates");
    }

    int len = (strlen("MM/DD/YYYY HH:MM:SS") + 1024);
    CDR_MALLOC(cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry], len, "start date");
    CDR_MALLOC(cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry], len, "end date");

    c_ptr = strchr(dates[i], ':');
    if(c_ptr == NULL) // single date
    {
      sprintf(cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry], "%s 00:00:00", dates[i]);
      sprintf(cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry], "%s 23:59:59", dates[i]);
    }
    else // range date
    {
      *c_ptr = '\0';
      c_ptr++;
      
      sprintf(cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry], "%s 00:00:00", dates[i]);
      sprintf(cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry], "%s 23:59:59", c_ptr);
    }
    
    cdr_config.ngve_days.start_ts[cdr_config.ngve_days.total_entry] =
        format_date_convert_to_ts(cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry]);
    cdr_config.ngve_days.end_ts[cdr_config.ngve_days.total_entry] = 
        format_date_convert_to_ts(cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry]);

    change_ts_partition_name_format(cdr_config.ngve_days.start_ts[cdr_config.ngve_days.total_entry], 
                                         cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry], len);
    change_ts_partition_name_format(cdr_config.ngve_days.end_ts[cdr_config.ngve_days.total_entry], 
                                         cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry], len);

    cdr_config.ngve_days.start_date_pf[cdr_config.ngve_days.total_entry] = atoll(cdr_config.ngve_days.start_dates[cdr_config.ngve_days.total_entry]);    cdr_config.ngve_days.end_date_pf[cdr_config.ngve_days.total_entry] = atoll(cdr_config.ngve_days.end_dates[cdr_config.ngve_days.total_entry]);

    
    cdr_config.ngve_days.total_entry += 1;
    CDRTL3("Negative days: %s", dates[i]);
  }
  
  CDRTL3("NEGATIVE_DAYS entry done");
  CDRTL2("Method exit");
}

static void parse_ngve_tr_array(char *value)
{
  char *tr_fields[NUM_FIELDS];

  CDRTL2("Method called");
  int n = get_tokens(value, tr_fields, ",", NUM_FIELDS);
  cdr_config.ngve_tr.total_entry = n;
  CDR_MALLOC(cdr_config.ngve_tr.tr, n * sizeof(int), "negative tr");
  for(int i = 0; i < n; ++i)
  {
    cdr_config.ngve_tr.tr[i] = atoi(tr_fields[i]);
    CDRTL3("Negative tr: %s", tr_fields[i]);
  }

  CDRTL3("NEGATIVE_TR entry done");
  CDRTL2("Method exit");
}


static int parse_config_array(nslb_jsont *json_tree)
{
  nslb_jsont *node;

  CDRTL2("Method called");

  for(node = json_tree->children; node != NULL; node = node->next_sib) 
  {
    if(strcmp(node->name, "ENABLED") == 0) 
    {
      if(strcmp(node->value, "true") == 0)
        cdr_config.enable = CDR_ENABLE;
      else
        cdr_config.enable = CDR_DISABLE;

      CDRTL3("cdr_config->enable = %d [%s]", cdr_config.enable, node->value);
    }

    else if(strcmp(node->name, "LOG_FILE_SIZE") == 0)
    {
      cdr_config.log_file_size = atoi(node->value) * 1024 * 1024;
      CDRTL3("cdr_config->log_file_size = %d", cdr_config.log_file_size);
    }

    else if(strcmp(node->name, "AUDIT_LOG_FILE_SIZE") == 0)
    {
      cdr_config.audit_log_file_size = atoi(node->value) * 1024 * 1024;
      CDRTL3("cdr_config->audit_log_file_size = %d", cdr_config.audit_log_file_size);
    }

    else if(strcmp(node->name, "LOG_LEVEL") == 0)
    {
      g_debug_level = atoi(node->value);
      CDRTL1("LOG_LEVEL = '%s'", node->value);
      if(g_debug_level == 1)
        g_debug_level = 0X000000FF;
      else if(g_debug_level == 2)
        g_debug_level = 0X0000FFFF;
      else if(g_debug_level == 3)
        g_debug_level = 0X00FFFFFF;
      else if(g_debug_level == 4)
        g_debug_level = 0XFFFFFFFF;
      else{
        g_debug_level = 0X000000FF;
        CDRTL1("Error: Debug level is not pass correct setting default 1.");
      }
    }

    else if(strcmp(node->name, "MODE") == 0) 
    {
      if(strcmp(node->value, "cloud") == 0)
        cdr_config.mode = MODE_CLOUD;
      else
        cdr_config.mode = MODE_LOCAL;
      CDRTL3("cdr_config->mode = %d [%s]", cdr_config.mode, node->value);
    }

    else if(strcmp(node->name, "TEST_RUN_NO") == 0) 
    {
      cdr_config.tr_num = atoi(node->value);
      CDRTL3("cdr_config->tr_num = %d", cdr_config.tr_num);
    }

    else if(strcmp(node->name, "BACKUP_PATH") == 0) 
    {
      strcpy(cdr_config.backup_path, node->value);
      CDRTL3("cdr_config->backup_path: %s", cdr_config.backup_path);
    }

    else if(strcmp(node->name, "CONTROLLER") == 0) 
    {
      strcpy(cdr_config.controller, node->value);
      CDRTL3("cdr_config->controller: %s", cdr_config.controller);
    }

    else if(strcmp(node->name, "CLEANUP") == 0) 
    {
      CDRTL3("Parsing CLEANUP");
      cdr_config.cleanup_flag = CDR_TRUE;
      parse_cleanup_array(node, cdr_config.cleanup, "CLEANUP`");
    }

    else if(strcmp(node->name, "CLEANUP_ALL_TR") == 0) 
    {
      CDRTL3("Parsing CLEANUP_ALL_TR");
      cdr_config.cleanup_all_tr_flag = CDR_TRUE;
      parse_cleanup_array(node, cdr_config.cleanup_all_tr, "CLEANUP_ALL_TR");  
    }

    else if(strcmp(node->name, "CLEANUP_NEGATIVE_DAYS") == 0) 
    {
      CDRTL3("Parsing CLEANUP_NEGATIVE_DAYS");
      parse_cleanup_ngve_days_array(node);
    }
    else if(strcmp(node->name, "CUSTOM_CLEAN_PATH") == 0) 
    {
      CDRTL3("Parsing CUSTOM_CLEAN_PATH");
      parse_custom_clean_path_array(node, node->name);
    } 

    else if(strcmp(node->name, "NEGATIVE_DAYS") == 0) 
    {
      CDRTL3("Parsing NEGATIVE_DAYS");
      parse_ngve_days_array(node->value);
    }
    
    else if(strcmp(node->name, "NEGATIVE_TR") == 0) 
    {
      CDRTL3("Parsing NEGATIVE_TR");
      parse_ngve_tr_array(node->value);
    }

    else if(strcmp(node->name, "RECYCLE_BIN_TIME") == 0) 
    {
      cdr_config.recyclebin_cleanup = atoi(node->value);
      CDRTL3("cdr_config->recyclebin_time = %d", cdr_config.recyclebin_cleanup);
    }

    else 
    {
      CDRTL1("Error: Wrong json config tag: %s", node->name);
      continue;
      //return CDR_ERROR;
    }
  }

  CDRTL2("Method exit");
  return CDR_SUCCESS;
}

/****************************************************************************************
 * Name: cdr_process_config_file
 *    This will parse the cavisson data retention configuration from the json file
 *    path: <ns_wdir>/sys/data_retention/config.json
 *    after parsing the configurations are store in cdr_config
 ****************************************************************************************/
int cdr_process_config_file()
{
  nslb_json_t *conf_json = NULL;
  nslb_jsont  *conf_json_tree;
  nslb_jsont  *node1;
  nslb_jsont  *node2;
  nslb_json_error j_err;

  CDRTL2("Method called");

  conf_json = nslb_json_init(config_file_path, 0, 0, &j_err);
  if(conf_json == NULL) {
    CDRTL1("Error: config file read failed. config_file: %s, json_error: %s", config_file_path, j_err.str);
    return CDR_ERROR;
  }
  
  CDRTL3("Config file read done");

  conf_json_tree = nslb_json_to_jsont(conf_json);
  if(conf_json_tree == NULL) {
    CDRTL1("Error: Unable to convert json to json tree");
 
  }

  cdr_config_init(); // setting default values

  CDRTL1("Info: Parsing config file started");

  for(node1 = conf_json_tree->children; node1 != NULL; node1 = node1->next_sib) {
    if(strcmp(node1->name, "CONFIG") == 0) {
      for(node2 = node1->children; node2 != NULL; node2 = node2->next_sib) {
        parse_config_array(node2);
      }
    }
    else 
    {
      CDRTL1("Error: Unwanted tag found '%s'", node1->name);
      return CDR_ERROR;
    }
  }

  //TODO: default valuse should set after process complete, otherwise we can not set for all the fields. Currently we are doing in cdr_config_init 

  cdr_config.data_remove_flag = TRUE;

  CDRTL2("Method Exit, Parsing config file done");
  return 0;
}

/*****************************************************************
 * This function will printf the configurations to
 * drm_trace.log
 ****************************************************************/
 #define LOG_MSG_SIZE 1024 * 4
void cdr_print_config_to_log()
{
  CDRTL2("Method called");
  CDRTL3("Printing configurations to log");

  CDRTL4("enable: %d", cdr_config.enable);
  CDRTL4("log_file_size: %d mb", cdr_config.log_file_size);
  CDRTL4("mode: %d", cdr_config.mode);
  CDRTL4("tr_num: %d", cdr_config.tr_num);
  CDRTL4("backup_path: %s", cdr_config.backup_path);
  CDRTL4("controller: %s", cdr_config.controller);
  
  CDRTL4("cleanup:");
  for(int i = 0; i < TOTAL_COMPONENTS; ++i)
    CDRTL4("component: %s, retention_time: %d", component_name[i],
      cdr_config.cleanup[i].retention_time);
  
  CDRTL4("cleanup_all_trs:");
  for(int i = 0; i < TOTAL_COMPONENTS; ++i)
    CDRTL4("component: %s, retention_time: %d", component_name[i],
      cdr_config.cleanup_all_tr[i].retention_time);
  
  CDRTL4("negative_cleanup_days:");
  for(int i = 0; i < TOTAL_COMPONENTS; ++i)
  {
    CDRTL4("component: %s, dates:", component_name[i]);
    for(int j = 0; j < cdr_config.ngve_cleanup_days[i].total_entry; ++j)
      CDRTL4("start_date: %s end_date: %s, ", cdr_config.ngve_cleanup_days[i].start_dates[j],
        cdr_config.ngve_cleanup_days[i].end_dates[j]);  
  }
  
  CDRTL4("negative_days:");
  for(int i = 0; i < cdr_config.ngve_days.total_entry; ++i)
  {
    CDRTL4("start_date: %s end_date: %s, ", cdr_config.ngve_days.start_dates[i],
      cdr_config.ngve_days.end_dates[i]);
  }
  
  CDRTL4("negative_tr");
  for(int i = 0; i < cdr_config.ngve_tr.total_entry; ++i)
    CDRTL4("%d, ", cdr_config.ngve_tr.tr[i]);

  CDRTL2("Method exit");
}
