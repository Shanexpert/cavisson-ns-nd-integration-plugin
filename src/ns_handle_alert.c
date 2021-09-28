#include <regex.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include "nslb_dashboard_alert.h"
#include "ns_handle_alert.h"
#include "util.h"
#include "ns_msg_def.h"
#include "wait_forever.h"
#include "ns_log.h"
#include "ns_trace_level.h"
#include "ns_alloc.h"
#include "ns_exit.h"
#include "nslb_alloc.h"
#include "nslb_sock.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"

Alert_Info *alertData;
DashBoard_Info *dashboardInfo = NULL;

NetstormAlert *g_ns_alert;
static char alert_keyword_str[MAX_ALERT_LENGTH]="0";
char *g_alert_info = alert_keyword_str;
static char g_type_str[MAX_NS_VARIABLE_NAME + 1] = "";
#ifndef CAV_MAIN
static AlertInfo alert_info;
static ns_bigbuf_t g_alert_info_idx;
#else
static __thread AlertInfo alert_info;
static __thread ns_bigbuf_t g_alert_info_idx;
#endif

char alert_type_mask[] = {ALERT_MM_NORMAL, ALERT_MM_MINOR, ALERT_MM_MAJOR, ALERT_MM_CRITICAL, 0, 0, ALERT_MM_INFO};
char alert_type_mask_string[][16] = {"NORMAL", "MINOR", "MAJOR", "CRITICAL", "UNDEFINED", "UNDEFINED", "INFO"};

static inline void signal_masking_library()
{
  //set signal mask
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  //Should we block SIGPIPE. because we are writing to socket in thread. 
  sigaddset(&set, SIGPIPE);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGCHLD);
  sigaddset(&set, SIGRTMIN+1);
  set_alert_thread_signal_mask(set);
}

static int get_custom_tokens(char *in_buf, char *sep, int max_col, char **store_buf)
{
  char *first_pos = in_buf;
  char *second_pos = in_buf;
  int val_len = 0;
  int num_val = 0;

  NSDL3_VARS(NULL, NULL, "in_buffer=[%s]", in_buf);
  while(1)
  {
    if(*second_pos == *sep || *second_pos == '\0')
    {
      val_len = second_pos - first_pos;
      NSDL3_VARS(NULL, NULL, "first=[%s], second=[%s],val_len=[%d]", first_pos, second_pos, val_len);
      MY_MALLOC(store_buf[num_val], val_len+1, "store_buf", -1);
      memset(store_buf[num_val], 0, val_len+1);

      NSDL3_VARS(NULL, NULL, "val_len=[%d]", val_len);
      if(val_len > 0 )
       strncpy(store_buf[num_val], (first_pos), val_len);
      else
       strcpy(store_buf[num_val], ""); //TODO: Can we remove this line??


      store_buf[num_val][val_len] = '\0';
      NSDL3_VARS(NULL, NULL, "in_method_val=[%s]", store_buf[num_val]);
      num_val++;

      if(*second_pos == 0) break;

      second_pos  = second_pos + 1;
      first_pos= second_pos;
      continue;

    }
    second_pos  = second_pos + 1;
  }

  NSDL3_VARS(NULL, NULL, "last__val=[%s]", store_buf[num_val -1]);
  return num_val;
}

int parse_alert_info(char *line, char *delimeter)
{
  char *fields[6];
  int total_fields = 0;
  int server_port;

  NSDL3_VARS(NULL, NULL, "Method called, line = %s", line);

  total_fields = get_custom_tokens(line , delimeter, 6, fields);

  if(total_fields != 6)
  {
    NSTL1(NULL, NULL, "Total fields should be 6 in alert configuration");
    return -1;
  }
  //validating read content from file
  if(strcmp(fields[0], "LOCAL") && strcmp(fields[0], "REMOTE"))
  {
    NSTL1(NULL, NULL, "Server mode can be 'LOCAL' or 'REMOTE'");
    return -1;
  }

  if(strlen(fields[1]) > 256)
  {
    NSTL1(NULL, NULL, "Server IP cannot be of length greater than 256");
    return -1;
  }

  if(nslb_atoi(fields[2], &server_port) < 0)
  {
    NSTL1(NULL, NULL, "Server port can only have numeric value");
    return -1;
  }

  if(strcmp(fields[3], "HTTP") &&  strcmp(fields[3], "HTTPS"))
  {
    NSTL1(NULL, NULL, "Protocol can be 'HTTP' or 'HTTPS'");
    return -1;
  }

  if(strcmp(fields[4], "GET") && strcmp(fields[4], "POST"))
  {
    NSTL1(NULL, NULL, "HTTP method can be 'GET' or 'POST'");
    return -1;
  }

  if(strlen(fields[5]) > 256)
  {
    NSTL1(NULL, NULL, "URL length cannot be greater than 256");
    return -1;
  }

  if(!strcmp(fields[3], "HTTP"))
    global_settings->alert_info->protocol = 0;
  else
    global_settings->alert_info->protocol = 1;

  if(!strcmp(fields[4], "GET"))
    global_settings->alert_info->method = HTTP_METHOD_GET;
  else
    global_settings->alert_info->method = HTTP_METHOD_POST;

  strncpy(global_settings->alert_info->server_ip, fields[1], 256);
  global_settings->alert_info->server_ip[256] = '\0';
  global_settings->alert_info->server_port = server_port;
  strncpy(global_settings->alert_info->url, fields[5], 256);
  global_settings->alert_info->url[256] = '\0';

  return 0;
}

int parse_config_file(char *file_name)
{
  FILE *fptr;
  char read_buf[MAX_LINE_LENGTH];
  int line_count = 0;
  struct stat st;
  size_t read_buf_size = MAX_LINE_LENGTH;
  char *ptr = read_buf;

  if(stat(file_name, &st) || (!st.st_size))
  {
    NSTL1(NULL, NULL, "Alert config file is not present, hence using default configuration for sending alert");
    return 0;
  }

  if((fptr = fopen(file_name, "r")) == NULL)
  {
    NSTL1(NULL, NULL, "Alert config file is not present, hence using default configuration for sending alert");
    return 0;
  }

  /*Read line
    - remove \t and blank space
    - skip comment lines
    - skip blank line
    - tokenize fields (= 6)
    - store token*/

  while(getline(&ptr, &read_buf_size, fptr) != -1)
  {
    CLEAR_WHITE_SPACE(ptr);

    if(*ptr == '#' || *ptr == '\n' || *ptr == '\r')
    {
      continue;
    }

    CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(ptr);

    if(parse_alert_info(ptr, "|") < 0)
      return -1;

    if(line_count != 0)
    {
      NSDL3_PARSING(NULL, NULL, "Reading last line only as multi line input is provided");
    }

    line_count++;
  }
  return 0;
}

void inline create_alert_info_line()
{
  char *server_ip;
  
  NSDL2_REPORTING(NULL, NULL, "Method Called");
  
  if(!strcmp(global_settings->alert_info->server_ip, "127.0.0.1") || !strcmp(global_settings->alert_info->server_ip, "0.0.0.0"))
    server_ip = g_cavinfo.NSAdminIP;
  else
    server_ip = global_settings->alert_info->server_ip;

  sprintf(g_alert_info, "%d %s %s %d %d %d %d %d %d %d 1 REMOTE,%s,%d,%s,%s,%s",
                         global_settings->alert_info->enable_alert,
                         g_type_str,
                         global_settings->alert_info->policy,
                         global_settings->alert_info->rate_limit,
                         global_settings->alert_info->max_conn_retry,
                         global_settings->alert_info->retry_timer,
                         global_settings->alert_info->tp_init_size,
                         global_settings->alert_info->tp_max_size,
                         global_settings->alert_info->mq_init_size,
                         global_settings->alert_info->mq_max_size,
                         server_ip,
                         global_settings->alert_info->server_port,
                         global_settings->alert_info->protocol?"HTTPS":"HTTP",
                         global_settings->alert_info->method==HTTP_METHOD_POST?"POST":"GET",
                         global_settings->alert_info->url);
}


int kw_set_enable_alert(char* buf, char *err_msg, int runtime)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "";
  char tmp[MAX_DATA_LINE_LENGTH] = "";
  char mode_str[MAX_NAME_LENGTH + 1] = "";
  char type_str[MAX_NS_VARIABLE_NAME + 1] = "";
  char policy_str[MAX_NAME_LENGTH + 1] = "";
  char rate_limit_str[MAX_NAME_LENGTH + 1] = "";
  char retry_count_str[MAX_NAME_LENGTH + 1] = "";
  char retry_time_str[MAX_NAME_LENGTH + 1] = "";
  char tp_init_size_str[MAX_NAME_LENGTH + 1] = "";
  char tp_max_size_str[MAX_NAME_LENGTH + 1] = "";
  char mq_init_size_str[MAX_NAME_LENGTH + 1] = "";
  char mq_max_size_str[MAX_NAME_LENGTH + 1] = "";
  char alert_config_mode_str[MAX_NAME_LENGTH + 1]="";
  char alert_config_file_str[MAX_FILE_NAME + 1]="";
  int num_args, ret, mode = 0;
  int rate_limit = 0, alert_config_mode = 0;
  int retry_count = 0, retry_time = 0, tp_init_size = 0, tp_max_size = 0;
  int mq_init_size = 0, mq_max_size = 0;
  AlertInfo loc_alert_info;

  NSDL2_REPORTING(NULL, NULL, "Method called, buf = [%s]", buf);

  if(!runtime)
  {
    global_settings->alert_info = &alert_info;
 
    //Default Values in structure
    global_settings->alert_info->enable_alert = 1;
    global_settings->alert_info->type = ALERT_MM_CRITICAL|ALERT_MM_MAJOR|ALERT_MM_MINOR;
    global_settings->alert_info->method = HTTP_METHOD_POST;
    global_settings->alert_info->protocol = 0; //0 - HTTP, 1 - HTTPS
    global_settings->alert_info->alert_config_mode = 0; //0 - alert.conf, 1- memory, 2 - scenario/config_file
    global_settings->alert_info->retry_timer = 60;
    global_settings->alert_info->rate_limit = 60;
    global_settings->alert_info->max_conn_retry = 5;
    global_settings->alert_info->tp_init_size = 5;
    global_settings->alert_info->tp_max_size = 10;
    global_settings->alert_info->mq_init_size = 30;
    global_settings->alert_info->mq_max_size = 60;
    strcpy(global_settings->alert_info->server_ip, "127.0.0.1");
    global_settings->alert_info->server_port = 80;
    strcpy(global_settings->alert_info->url, "/DashboardServer/web/AlertDataService/genCustomAlert");
    strcpy(global_settings->alert_info->policy, "CustomPolicy");
    strcpy(g_type_str, "CRITICAL,MAJOR,MINOR");
  } 
  memcpy(&loc_alert_info, global_settings->alert_info, sizeof(AlertInfo));

  num_args = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s", keyword, mode_str, type_str, policy_str, rate_limit_str, retry_count_str, retry_time_str, tp_init_size_str, tp_max_size_str, mq_init_size_str, mq_max_size_str, alert_config_mode_str, alert_config_file_str, tmp);

  NSDL2_REPORTING(NULL, NULL, "num_args = [%d]", num_args);

  //Only mode is a mandatory argument, if rest not provided then will fill the default values
  if(num_args < 2)
  {
    NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_1);
  }

  if((nslb_atoi(mode_str, &mode)) < 0)
  {
    NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
  } 
  if(mode != 0 && mode != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, "Mode can only be 0 or 1");
  }

  loc_alert_info.enable_alert = mode;

  if(type_str[0])
  {
    char *type_tokens[10];
    strcpy(g_type_str, type_str);
    int num_type_mask = get_tokens(type_str, type_tokens, ",", 10);

    loc_alert_info.type = 0;

    for(int i = 0; i < num_type_mask; i++)
    {
      if(!strcmp(type_tokens[i], "ALL"))
        loc_alert_info.type |= ALERT_MM_ALL;
      else if(!strcmp(type_tokens[i], "NORMAL"))
        loc_alert_info.type |= ALERT_MM_NORMAL;
      else if(!strcmp(type_tokens[i], "MINOR"))
        loc_alert_info.type |= ALERT_MM_MINOR;
      else if(!strcmp(type_tokens[i], "MAJOR"))
        loc_alert_info.type |= ALERT_MM_MAJOR;
      else if(!strcmp(type_tokens[i], "CRITICAL"))
        loc_alert_info.type |= ALERT_MM_CRITICAL;
      else if(!strcmp(type_tokens[i], "INFO"))
        loc_alert_info.type |= ALERT_MM_INFO;
      else
      {
        NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011362, type_tokens[i]);
      }
    }
    NSDL4_PARSING(NULL, NULL, "alert_type = %d", loc_alert_info.type);
  }

  if(policy_str[0])
  {
    strncpy(loc_alert_info.policy, policy_str, 256);
    loc_alert_info.policy[256] = '\0';
  }

  if(rate_limit_str[0])
  {
    if((nslb_atoi(rate_limit_str, &rate_limit)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((rate_limit < 0) || (rate_limit > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    loc_alert_info.rate_limit = rate_limit;
  }

  if(retry_count_str[0])
  {
    if((nslb_atoi(retry_count_str, &retry_count)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((retry_count < 0) || (retry_count > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    loc_alert_info.max_conn_retry = retry_count;
  }

  if(retry_time_str[0])
  {
    if((nslb_atoi(retry_time_str, &retry_time)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((retry_time < 0) || (retry_time > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    loc_alert_info.retry_timer = retry_time;
  }

  if(tp_init_size_str[0])
  {
    if((nslb_atoi(tp_init_size_str, &tp_init_size)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((tp_init_size < 0) || (tp_init_size > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    if(runtime && (tp_init_size != loc_alert_info.tp_init_size))
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, "Thread pool initial size can not be modified");
    }
    else
      loc_alert_info.tp_init_size = tp_init_size;
  }

  if(tp_max_size_str[0])
  {
    if((nslb_atoi(tp_max_size_str, &tp_max_size)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((tp_max_size < 0) || (tp_max_size > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    if(runtime && (tp_max_size != loc_alert_info.tp_max_size))
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, "Thread pool max size can not be modified");
    }
    else
      loc_alert_info.tp_max_size = tp_max_size;
  }

  if(mq_init_size_str[0])
  {
    if((nslb_atoi(mq_init_size_str, &mq_init_size)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((mq_init_size < 0) || (mq_init_size > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    if(runtime && (mq_init_size != loc_alert_info.mq_init_size))
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, "Alert message queue initial size can not be modified");
    }
    else
      loc_alert_info.mq_init_size = mq_init_size;
  }

  if(mq_max_size_str[0])
  {
    if((nslb_atoi(mq_max_size_str, &mq_max_size)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((mq_max_size < 0) || (mq_max_size > 65535))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_10);

    if(runtime && (mq_max_size != loc_alert_info.mq_max_size))
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, "Alert message queue max size can not be modified");
    }
    else
      loc_alert_info.mq_max_size = mq_max_size;
  }

  if(alert_config_mode_str[0])
  {
    if((nslb_atoi(alert_config_mode_str, &alert_config_mode)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    if((alert_config_mode != 0) && (alert_config_mode != 1) && (alert_config_mode != 2))
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);

    if(alert_config_mode && !alert_config_file_str[0])
    {
      NS_KW_PARSING_ERR(buf, runtime, err_msg, ENABLE_ALERT_USAGE, CAV_ERR_1011361, CAV_ERR_MSG_3);
    }
    loc_alert_info.alert_config_mode = alert_config_mode;
  }

  NSDL2_REPORTING(NULL, NULL, "loc_alert_info->enable_alert = %d", loc_alert_info.enable_alert);

  //Updating global structure
  memcpy(global_settings->alert_info, &loc_alert_info, sizeof(AlertInfo));

  if((alert_config_mode == 0) || (alert_config_mode == 2))
  {
    //If config file is provided in keyword then use that file from NS_WDIR/scenarios/proj/subproj/scenario_name/config_file
    //else read system file from NS_WDIR/sys/alert.conf
    if(alert_config_mode == 2)
    {
      char alert_config_file_tmp[MAX_FILE_NAME + 1];
      strcpy(alert_config_file_tmp, alert_config_file_str);
      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scenarios dir*/
      sprintf(alert_config_file_str, "%s/%s/%s/%s/%s/%s", 
		GET_NS_TA_DIR(), g_project_name, g_subproject_name, "scenarios", g_scenario_name, alert_config_file_tmp);
      NSDL2_REPORTING(NULL, NULL, "Mode 2 is provided, alert file = %s", alert_config_file_str);
    }
    else
      sprintf(alert_config_file_str, "%s/sys/alert.conf", g_ns_wdir);

    //Parse and fill all the data provided in file into global structure
    ret = parse_config_file(alert_config_file_str);

    if(ret < 0)
    {
      NSTL1(NULL, NULL, "Alert configuration is not provided properly so using default/existing settings");
    }
  }
  else
  {
    ret = parse_alert_info(alert_config_file_str, ",");
    if(ret < 0)
    {
      NSTL1(NULL, NULL, "Alert configuration is not provided properly so using default/existing settings");
    }
  }

  if(global_settings->alert_info->enable_alert)
  {
    create_alert_info_line();
    ns_alert_config();
  } else
    sprintf(g_alert_info, "0");

  if(!runtime)
  {
    g_alert_info_idx = copy_into_big_buf((char *)global_settings->alert_info, sizeof(AlertInfo));
  }

  return 0;
}

int process_alert_server_config_rtc()
{

  char alert_config_file_str[MAX_FILE_NAME + 1]="";
  int ret; 

  NSDL2_REPORTING(NULL, NULL, "Method Called");

  if(!global_settings->alert_info->enable_alert)   
    return -1;
  
  if(global_settings->alert_info->alert_config_mode)
    return -1;

  sprintf(alert_config_file_str, "%s/sys/alert.conf", g_ns_wdir);
  ret = parse_config_file(alert_config_file_str);
 
  if(ret < 0)
  {
    NSTL1(NULL, NULL, "Alert configuration is not provided properly so using default settings");
  }
 
  create_alert_info_line();
  ns_alert_config();
  return 0;
}
void copy_alert_info_into_shr_mem()
{
  global_settings->alert_info = (AlertInfo *)BIG_BUF_MEMORY_CONVERSION(g_alert_info_idx);
}

int handle_alert_msg(char* msg, int severity, char* policyName)
{
  int ret;

  NSDL2_REPORTING(NULL, NULL, "Method Called, loader_opcode = %d, enable_alert = %d",
                               loader_opcode, global_settings->alert_info->enable_alert);

  //Do not log alert
  if(!global_settings->alert_info->enable_alert)
    return -1;

  MY_MALLOC(alertData, sizeof(Alert_Info), "Alert_Info ptr", 1);
  memset(alertData, 0, sizeof(Alert_Info));
  
  alertData->message = msg;
  strcpy(alertData->policy, policyName);
  alertData->severity=severity;
  NSDL1_REPORTING(NULL, NULL, "alertData->message = %s, alertData->policy = %s, alertData->severity = %d", alertData->message, alertData->policy, alertData->severity);
  
  if (loader_opcode != CLIENT_LOADER) {
    if(dashboardInfo == NULL) {
      MY_MALLOC(dashboardInfo, sizeof(DashBoard_Info), "DashBoard_Info ptr", 1);
    }
    memset(dashboardInfo, 0, sizeof(DashBoard_Info));
    strcpy(dashboardInfo->dashboard_ip, "127.0.0.1");
    dashboardInfo->dashboard_protocol = 0;
    dashboardInfo->nde_testrun = testidx;
    dashboardInfo->dashboard_port = g_tomcat_port;
  }

  signal_masking_library();
  if((ret = nslb_send_alert(alertData, NULL, NULL)) == -1) {
    NSTL1(NULL, NULL, "Dashboard information not loaded correctly");
    return -1;
  } else { 
    NSTL1(NULL, NULL, "Dashboard alert sent successfully, for msg = %s", msg);
    return 0;
  }
}

void ns_alert_config()
{
  NSDL3_PARSING(NULL, NULL, "Method called");

  //Init Alert library with the settings provided
  if(!g_ns_alert && global_settings->alert_info)
    g_ns_alert = nslb_alert_init(global_settings->alert_info->tp_init_size, global_settings->alert_info->tp_max_size,
                                 global_settings->alert_info->mq_init_size, global_settings->alert_info->mq_max_size);

  if(!g_ns_alert)
  {
    NSTL1(NULL, NULL, "Alert library is not initialised successfully, Error: %s", nslb_get_error());
    return;
  }

  //Config alert library
  if(nslb_alert_config(g_ns_alert, global_settings->alert_info->server_ip, global_settings->alert_info->server_port, global_settings->alert_info->protocol, global_settings->alert_info->url, global_settings->alert_info->max_conn_retry, global_settings->alert_info->retry_timer, global_settings->alert_info->rate_limit, ns_event_fd) < 0)
  {
    NSTL1(NULL, NULL, "Failed to configure alert settings, Err: %s", nslb_get_error());
    return;
  }
  NSTL1(NULL, NULL, "Alert library is initialised and configured successfully, g_ns_alert = %p", g_ns_alert);
}

int nsi_send_alert(int alert_type, int alert_method, char *content_type, char *alert_msg, int length)
{
  NSDL2_HTTP(NULL, NULL, "Method called, alert_type = %d, alert_method = %d, alert_msg = %s, length = %d",
                          alert_type, alert_method, alert_msg, length);

  char alert_method_str[][32] = {"NONE", "HTTP_METHOD_GET", "HTTP_METHOD_POST"};
  char hdr[128];
  int ret;

  if(!global_settings->alert_info->enable_alert)
  {
    NSDL1_HTTP(NULL, NULL, "Alert Setting is not enabled.");
    return -1;
  }

  if(!g_ns_alert)
  {
    NSTL1(NULL, NULL, "Failed to send alert as alert object is not initialised");
    return -1;
  }

  if(!alert_msg || !alert_msg[0] || !length )
  {
    NSDL1_HTTP(NULL, NULL, "Failed to send alert, message in null or zero length");
    return -1;
  }

  //Setting alert type to modulemask
  if(alert_type > MAX_ALERT_TYPE)
  {
    NSTL1(NULL, NULL, "Alert level %d is not supported", alert_type);
    return -1;
  }

  if(!(alert_type_mask[alert_type] & global_settings->alert_info->type))
  {
    NSDL1_HTTP(NULL, NULL, "Alert level %s is not configured", alert_type_mask_string[alert_type]);
    return -1;
  }

  if((alert_method != HTTP_METHOD_GET) && (alert_method != HTTP_METHOD_POST))
  {
    NSTL1(NULL, NULL, "Alert method is %s, it should be 'HTTP_METHOD_GET' or 'HTTP_METHOD_POST'", alert_method_str[alert_method]);
    return -1;
  }

  sprintf(hdr, "Content-Type: %s", content_type);
  if((ret = nslb_alert_send(g_ns_alert, alert_method, hdr, alert_msg, length)) < 0)
  {
    NSTL1(NULL, NULL, "Failed to send alert, alert_method = %d, alert_msg = %s, Error: %s",
                       alert_method, alert_msg, nslb_get_error());
    return ret;
  }

  return 0;
}

void ns_process_apply_alert_rtc()
{
  NSDL2_HTTP(NULL, NULL, "Method called");
  if(global_settings->alert_info->enable_alert)
    ns_alert_config();
}
