/**
 * Name             : ns_standard_monitors.c
 * Author           : Shri Chandra
 * Purpose          : This file contains method:
                      - To read standard_monitors.dat file and initilize data structure
                      - To parse standard monitor keyword 
                      - To map with custom monitor keyword. 
 * Modification Date:
 * Initial Version  : 09/09/2009
***********************************************************************************************************/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_custom_monitor.h"
#include "ns_log.h"
#include "ns_mon_log.h"
#include "ns_string.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_schedule_phases.h"
#include "ns_server_admin_utils.h"
#include "ns_standard_monitors.h"
#include "bh_read_conf.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_trace_level.h"
#include "ns_child_msg_com.h"
#include "init_cav.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_lps.h"
#include "ns_custom_monitor.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"
#include "ns_monitoring.h"

#include "ns_monitoring.h"
#define STD_MON_FILE_FIELD 15 // Number of fields in the standard_monitor.dat file

int total_standard_monitors_entries = 0;  // Total Standard monitor enteries
int max_standard_monitors_entries = 0;    // Max Standard monitor enteries
StdMonitor *std_mon = NULL;


/**
Purpose		: This method is display Usages
Arguments	: error message
Return Value	: None
**/

//case 1 json_agent std_mon_agent_type different ->  return(not apply monitor)
//case 2 json_agent std_mon_agent_type same 
   //  ALL ---- ALL
   //  CMON or BCI ---  ALL
   //   ALL --  CMON or BCI
int set_agent_type(JSON_info *json_info_ptr, StdMonitor *std_mon_ptr, char *mon_name)
{
  //Check if agent type is different in standard_monitor.dat and monitor json
  if(json_info_ptr->agent_type & std_mon_ptr->agent_type)
  {
    //Check if monitor_json have ALL agent type then set agent type of standard_monitor.dat.
    if(((json_info_ptr->agent_type & CONNECT_TO_CMON) && (json_info_ptr->agent_type & CONNECT_TO_NDC))) //this case is ALL json agent type
    {
      if(std_mon_ptr->agent_type & CONNECT_TO_CMON)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
           "Monitor is applied on CMON agent type %s", mon_name);
      }
      if(std_mon_ptr->agent_type & CONNECT_TO_NDC)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
           "Monitor is applied on BCI agent type %s", mon_name);
      }
      json_info_ptr->agent_type = std_mon_ptr->agent_type;
    }
    //Send whatever agent type mentioned in monitor json
    else  //this case when std mon is ALL
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
           "Appylinig mon acc to JSON %s", mon_name);
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                "Agent type is not matched in json and standard_monitor.dat file So skipping monitor%s", mon_name);
    return -1;
  }
  return 0;
}


//this will tokenize 7th field of standard_monitor.dat
//7th field format is: <DVM TYPE>:<USE LPS>:<USE ARGS>
static void fill_dvm_related_fields(char *arg, int std_mon_row)
{
  char buff[MAX_BUF_SIZE_FOR_STD_MON] = {0};
  char *fields[4]={0};
  int num_field = 0;
  strcpy(buff, arg);

  num_field = get_tokens(buff, fields, ":", 4);
  NSDL2_MON(NULL, NULL, "Method called, num_field = %d", num_field);

  if(num_field != 0)
  {
    //format of field[7] is DVM2:1:<args>
    if(fields[0])
    {
      NSDL2_MON(NULL, NULL, "fields[0] = %s", fields[0]);

      MY_MALLOC(std_mon[std_mon_row].monitor_type, strlen(fields[0]) + 1, "For Monitor Type", std_mon_row);
      //If monitor type is NA then will consider this as Custom Monitor
      if(!strcmp(fields[0], "NA"))
        strcpy(std_mon[std_mon_row].monitor_type, "CM");
      else
        strcpy(std_mon[std_mon_row].monitor_type, fields[0]);
    }
    if(fields[1])
    {
      NSDL2_MON(NULL, NULL, "fields[1] = %s", fields[1]);
      std_mon[std_mon_row].use_lps = atoi(fields[1]); 
    }
    if(fields[2])
    {
      NSDL2_MON(NULL, NULL, "fields[2] = %s", fields[2]);
      std_mon[std_mon_row].use_args = atoi(fields[2]); 
    }
    if(fields[3])
    {
      NSDL2_MON(NULL, NULL, "fields[3] = %s", fields[3]);
      std_mon[std_mon_row].skip_breadcrumb_creation = atoi(fields[3]);
    }
  }
}

/**
Purpose		: This method is used to read standard monitors details from standard_monitor.dat file, then initialize all the structure fields.
Arguments	: Standard monitor data file
Return Value	: None 
**/

static int init_standard_monitors(FILE *fp, int runtime_flag, char *err_msg)
{
  char buf[MAX_BUF_SIZE_FOR_STD_MON * 10];
  char *fields[STD_MON_FILE_FIELD];
  char xml_file_path[1024] = {0};
  int i = 0, no_fields = 0;
  int std_mon_row = 0;
  int line = 0;
  //ServerCptr server_mon_ptr;

  // this loop is to initialize *fields[] with NULL 
  for (i = 0; i < STD_MON_FILE_FIELD; i++)
    fields[i] = NULL;

  while(fgets(buf, MAX_BUF_SIZE_FOR_STD_MON * 10, fp) != NULL)
  {
    line++;
    buf[strlen(buf)-1] = '\0'; // Remove new line
    CLEAR_WHITE_SPACE_FROM_END(buf);
    if((buf[0] == '#') || (buf[0] == '\0'))   //Check for Comment Line and Blank Line
        continue;
    if((no_fields = get_tokens(buf, fields, "|", STD_MON_FILE_FIELD)) < 9)   // 9 is mandatory fields
    {
      sprintf(err_msg, CAV_ERR_1060067, line, buf);
      CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
    }

    if ((strncmp(fields[7], "CM", 2) != 0) && (strncmp(fields[7], "DVM", 3) != 0) && (strncmp(fields[7], "NA", 2) != 0))
    {
      sprintf(err_msg, CAV_ERR_1060068, fields[7], fields[0]);
      CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
    }

    if(create_table_entry(&std_mon_row, &total_standard_monitors_entries, &max_standard_monitors_entries, (char **)&std_mon, sizeof(StdMonitor), "Standard Monitor Table") == -1)
    {
      sprintf(err_msg, CAV_ERR_1060069);
      CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
    }
    memset((std_mon + std_mon_row), 0, sizeof(StdMonitor)); 
    
    // setting default values for standard monitors
    std_mon[std_mon_row].record_id = -1;
    std_mon[std_mon_row].agent_type = CONNECT_TO_CMON;
    
    MY_MALLOC(std_mon[std_mon_row].monitor_name, strlen(fields[0]) + 1, "Standard Monitor Name", std_mon_row);
    strcpy(std_mon[std_mon_row].monitor_name, fields[0]);

    MY_MALLOC(std_mon[std_mon_row].gdf_name, strlen(fields[1]) + 1, "Gdf File Name", std_mon_row);
    strcpy(std_mon[std_mon_row].gdf_name, fields[1]);
   
    std_mon[std_mon_row].run_option = atoi(fields[2]);
    
    if(std_mon[std_mon_row].run_option < 0) 
    {
      sprintf(err_msg, "Syntax error in standard_monitors.dat file. At Line %d, invalid run option provided is %s.", line, fields[2]);
     // CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0);
    }

    MY_MALLOC(std_mon[std_mon_row].pgrm_name, strlen(fields[3]) + 1, "Program Name", std_mon_row);
    strcpy(std_mon[std_mon_row].pgrm_name, fields[3]);

    MY_MALLOC(std_mon[std_mon_row].pgrm_type, strlen(fields[4]) + 1, "Program Type", std_mon_row);
    strcpy(std_mon[std_mon_row].pgrm_type, fields[4]);

    MY_MALLOC(std_mon[std_mon_row].fixed_args, strlen(fields[5]) + 1, "Fixed Arguments", std_mon_row);
    // To check the arguments field if NA or -
    if((strcmp(fields[5], "NA") == 0) || (strcmp(fields[5], "-") == 0))
      strcpy(std_mon[std_mon_row].fixed_args, "");
    else 
      strcpy(std_mon[std_mon_row].fixed_args, fields[5]);
 
    MY_MALLOC(std_mon[std_mon_row].machine_types, strlen(fields[6]) + 1, "For Server Types", std_mon_row);
    strcpy(std_mon[std_mon_row].machine_types, fields[6]);

    fill_dvm_related_fields(fields[7],std_mon_row);  //function calling
    
    MY_MALLOC(std_mon[std_mon_row].comment, strlen(fields[8]) + 1, "Comments", std_mon_row);
    strcpy(std_mon[std_mon_row].comment, fields[8]);
 
    //Mandatory fields for Mbean monitor
    if(no_fields >= 15)  // It means entry present in standard_monitors.dat file regarding record_id, agent_type and config_file.
    {
      std_mon[std_mon_row].record_id = atoi(fields[12]);
      
      std_mon[std_mon_row].agent_type &= 0;
      if(!strcasecmp(fields[13], "BCI"))
        (std_mon[std_mon_row].agent_type) |= (CONNECT_TO_NDC);
      else if(!strcasecmp(fields[13], "CMON"))
        (std_mon[std_mon_row].agent_type) |= (CONNECT_TO_CMON);
      else if(!strcasecmp(fields[13], "ALL"))
        (std_mon[std_mon_row].agent_type) |= (CONNECT_TO_BOTH_AGENT);
     
      if(strrchr(fields[14], '/'))
        sprintf(xml_file_path, "%s", fields[14]);
      else
        sprintf(xml_file_path, "%s/MBean/XML/%s", g_ns_wdir, fields[14]);

      MY_MALLOC(std_mon[std_mon_row].config_json, strlen(xml_file_path) + 1, "Config JSON", std_mon_row);
      strcpy(std_mon[std_mon_row].config_json, xml_file_path); 
    }
  
    NSDL2_MON(NULL, NULL, "monitor name = %s, gdf name = %s, run option = %d, program name = %s, program type = %s, fixed arguments = %s, server type = %s, monitor_type = %s, comments = %s\n", std_mon[std_mon_row].monitor_name, std_mon[std_mon_row].gdf_name, std_mon[std_mon_row].run_option, std_mon[std_mon_row].pgrm_name, std_mon[std_mon_row].pgrm_type, std_mon[std_mon_row].fixed_args, std_mon[std_mon_row].machine_types, std_mon[std_mon_row].monitor_type, std_mon[std_mon_row].comment);	
  }
return 0;
} 

/**
Purpose		: This method is used to open standard_monitor.dat 
Arguments	: None
Return Value	: None
**/
static int read_std_mon_dat_file(int runtime_flag, char *err_msg)
{
  FILE *fp;
  char file_name[MAX_BUF_SIZE_FOR_STD_MON];
  //ServerCptr server_mon_ptr;

  //If total_standard_monitors_entries = 0 then open file and initialize all entries in the data structure else return. 
  if(total_standard_monitors_entries != 0) return 0; // already loaded, return

  sprintf(file_name, "%s/etc/standard_monitors.dat", g_ns_wdir);
  
  NSDL2_MON(NULL, NULL, "Method called, File Name = %s", file_name);
  if((fp = fopen(file_name, "r")) == NULL)
  {
    sprintf(err_msg, CAV_ERR_1060070, file_name);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
  }

  // To initialize standard monitor data structure
  if(init_standard_monitors(fp, runtime_flag, err_msg) < 0)
  { 
    CLOSE_FP(fp);
    return -1;
  }

CLOSE_FP(fp);
return 0;
}

/**
Purpose		: This method is used to search data structure with standard monitors name. If standard monitor                     name  and machine type present in the list. Then return index to that entry else return NULL.
Arguments	: 
		  - Name of standard monitor   
                  - Name of Machine type which is return by corresponding server from server list 
Return Value	: Index of Standard monitor data structure
**/
StdMonitor  *get_standard_mon_entry(char *monitor_name, char *machine_type, int runtime_flag, char *err_msg)
{
  int i;
  NSDL2_MON(NULL, NULL, "Method called, monitor_name = %s,  machine_type = %s", monitor_name, machine_type);

  //To read standard_monitors.dat and filled structure.
  if(read_std_mon_dat_file(runtime_flag, err_msg) < 0)
    return NULL;

  for(i = 0; i < total_standard_monitors_entries; i++) // Loop for total number of standard monitors
    // To check monitor name and if machine types == ALL or machine types == machine type
    if((strcmp(monitor_name, std_mon[i].monitor_name) == 0) && ((strcasecmp(std_mon[i].machine_types, "ALL") == 0) || (strstr(std_mon[i].machine_types, machine_type)!= NULL)))
      return &std_mon[i];
  NSDL2_MON(NULL, NULL, "Standard monitor entry not found, monitor_name = %s, machine_type = %s", monitor_name, machine_type);
  return NULL;
}

/************************************************************************************************************
 *
 *   get_tokens_with_substring()
 *
 *   this function is written to tokenize a string buffer using a substring as a token unlike the previous
 *   function where the token given were treated as individual tokens.
 *   If the substring given is present in the string then only it will tokenize the string otherwise the 
 *   string will remain untouched.
 *
 ***********************************************************************************************************/
static int get_tokens_with_substring(char *buff, char *final_field[], char *token, int max_fields, int token_len)
{
  char *ptr = buff;
  char *ptr2;
  char *field[max_fields];
  int num_fields = 0;
  int i, j = 0;
  char c;

  field[num_fields] = ptr;
  num_fields++;
  while(*ptr)
  {
    // matches the complete string of tokens in the original string and then tokenize it
    if((ptr2 = strstr(ptr, token)))
    {
      field[num_fields] = ptr2 + token_len;
      *ptr2 = '\0';
      ptr = ptr2 + token_len;
      num_fields++;
      if (num_fields < max_fields)
        continue;
    }
    break;
  }
  // after tokenizing, we need to look for strings within "" and [] and consider them as one
  // if  string starts with '"' then it has to end as '"' and will be treated as a single token
  // same is he case with '[' .
  for(i = 0; i < num_fields; i++)
  {
    final_field[j++] = field[i];
    if(*field[i] == '[' || *field[i] == '"')
    {
      if(*field[i] == '"')
        c = *field[i];
      else
        c = ']';

      while(i < num_fields - 1)
      {
        if(*(field[i+1] - (token_len + 1)) == c)
          break;
        *(field[i+1] - token_len) = ' ';
        i++;

      }
    }
  }

  num_fields = j;
  return num_fields;
}

//Parsing arguments of AppDynamic monitor.
//Calling dynamic vector monitor for each metric given using option -M
int metric_based_mon_arg_parsing(char *arguments, char *server, char *vector_name, StdMonitor *std_mon_ptr, char *server_name, char *dvm_vector_arg_buf, int runtime_flag, char *err_msg, int use_lps_flag, JSON_info *jsonElement,int tier_idx,int server_index)
{
  char metric_buf[MAX_BIG_BUF_SIZE_FOR_STD_MON] = "";
  char save_arg_buf[MAX_BIG_BUF_SIZE_FOR_STD_MON] = "";
  char dyn_vector_mon_buf[4*MAX_BIG_BUF_SIZE_FOR_STD_MON] = "";
  char dyn_vector_gdf_buf[MAX_BUF_SIZE_FOR_STD_MON] = "";
  char dyn_vector_prog_args_buf[MAX_BIG_BUF_SIZE_FOR_STD_MON] = "";
  char *arg_field[2000];
  int num_arg_field = 0;
  int i = 0, ret_flag = 0;

  NSDL2_MON(NULL, NULL, "arguments = %s, server = %s, vector_name = %s", arguments, server, vector_name);
 
  memset(arg_field, 0, sizeof(arg_field));

  /* Program name with arguments:
   * cm_app_dynamics_stats -U AppDynamicsHostPortURL  -u UserName:Password(in base64 encoding) [ -i interval (seconds) -c <config file>]  -M Metric1 -M Metrci2 -M Metric3
   *
   * As standard monitor:
   * STANDARD_MONITOR 192.168.1.66 StandardMonitorNSAppliance AppDynamics -U AppDynamicsHostPortURL -u UserName:Password -M ExternalCalls -M OAP -M BTP -i 1 -c /tmp/abc.txt */

  /*adding space before arguments because now we tokenize with substring " -"
    if we don't add space
    ex- "-a arg1 -b arg2"
    after tokenize in field[0]="-a arg1", field[1]="b arg2"  */
  sprintf(save_arg_buf, " %s", arguments);
  NSDL2_MON(NULL, NULL, "save_arg_buf = %s", save_arg_buf);

  num_arg_field = get_tokens_with_substring(save_arg_buf, arg_field, " -", 500, 2);
  NSDL2_MON(NULL, NULL, "num_arg_field = %d", num_arg_field);

  for(i = 1; i < num_arg_field; i++)
  {
    NSDL2_MON(NULL, NULL, "arg_field[%d] = %s", i, arg_field[i]);

    strcpy(save_arg_buf, arg_field[i]);

    NSDL2_MON(NULL, NULL, "save_arg_buf[0] = %d", save_arg_buf[0]);

    /*switch(save_arg_buf[0])
    {
      case 85: // Comparing ASCII value of options. ascii value of U is 85
        strcat(dyn_vector_prog_args_buf, "-U ");
        strcat(dyn_vector_prog_args_buf, save_arg_buf + 2);
        strcat(dyn_vector_prog_args_buf, " ");
        break;
      case 117: //u
        strcat(dyn_vector_prog_args_buf, "-u ");
        strcat(dyn_vector_prog_args_buf, save_arg_buf + 2);
        strcat(dyn_vector_prog_args_buf, " ");
        break;
      case 77: //M
        strcat(metric_buf, save_arg_buf + 2);
        strcat(metric_buf, " ");
        break; 
      case 99:  //c
        strcat(dyn_vector_prog_args_buf, "-c ");
        strcat(dyn_vector_prog_args_buf, save_arg_buf + 2);
        strcat(dyn_vector_prog_args_buf, " ");
        break;
      case 68:  //D
        strcat(dyn_vector_prog_args_buf, "-D ");
        break;
      case 105:  //i
        strcat(dyn_vector_prog_args_buf, "-i ");
        strcat(dyn_vector_prog_args_buf, save_arg_buf + 2);
        strcat(dyn_vector_prog_args_buf, " ");
        break;
      default:
        fprintf(stderr, "Invalid option %d.\n", save_arg_buf[0]);
        exit(-1);
    }*/

   /* we are putting all the arguments except METRIC in one buffer "dyn_vector_prog_args_buf"
    * Further we will space tokenize this buffer for dynamic vector monitor. */

    if(save_arg_buf[0] == 77)  //Comparing ASCII value of option M. ascii value of M is 77
    {
      strcat(metric_buf, save_arg_buf + 2);
      strcat(metric_buf, " ");
    }
    else                      //saving all other arguments in single buffer
    {
      strcat(dyn_vector_prog_args_buf, "-");
      strcat(dyn_vector_prog_args_buf, save_arg_buf);
      strcat(dyn_vector_prog_args_buf, " ");
    }

    //resetting buffer
    save_arg_buf[0] = '\0';
  }

  NSDL2_MON(NULL, NULL, "dyn_vector_prog_args_buf = %s", dyn_vector_prog_args_buf);
  NSDL2_MON(NULL, NULL, "metric_buf = %s", metric_buf);

  if(metric_buf[0] == '\0')
  {
    sprintf(err_msg, CAV_ERR_1060071);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
  }
   
  num_arg_field = get_tokens(metric_buf, arg_field, " ", 500);
  NSDL2_MON(NULL, NULL, "num_arg_field = %d", num_arg_field);

  /* Here we have 3 different gdf's for 3 different Vector(or Metric).
   * ExternalCalls -  cm_app_dynamics_stats_external_calls.gdf
   * OAP - cm_app_dynamics_stats_oap.gdf
   * BTP- cm_app_dynamics_stats_btp.gdf */

  for(i = 0; i < num_arg_field; i++)
  {
    //Mapping each vector(or Metric) to their respective gdf
    //For AppDynamics monitor
    if(!strcmp(arg_field[i], "ExternalCalls"))
      strcpy(dyn_vector_gdf_buf, "cm_app_dynamics_stats_external_calls.gdf");
    else if(!strcmp(arg_field[i], "OAP"))
      strcpy(dyn_vector_gdf_buf, "cm_app_dynamics_stats_oap.gdf");
    else if(!strcmp(arg_field[i], "BTP"))
      strcpy(dyn_vector_gdf_buf, "cm_app_dynamics_stats_btp.gdf");

    //For oracle awr monitors.
    //TODO: Support for monitor which will write only csv with gdf 'NA'.
    //For oracle awr monitors.
    //as sql report is written to csv, no gdf is required;
    //Writing 'NA_' for gdf becoz 'NA_' is already handled while adding custom monitor
    else if(!strcmp(arg_field[i], "SQL_REPORT"))
    {
      strcpy(dyn_vector_gdf_buf, "NA_SR");
      global_settings->sql_report_mon = 1; 
    }
    else if(!strcmp(arg_field[i], "CACHE_SIZES_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_cache_sizes_stats.gdf");
    else if(!strcmp(arg_field[i], "INSTANCE_EFFICIENCY_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_instance_efficiency_stats.gdf");
    else if(!strcmp(arg_field[i], "LOAD_PROFILE_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_load_profile_stats.gdf");
    else if(!strcmp(arg_field[i], "MEMORY_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_memory_stats.gdf");
    else if(!strcmp(arg_field[i], "SHARED_POOL_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_shared_pool_stats.gdf");
    else if(!strcmp(arg_field[i], "SYSSTAT"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_sysstat.gdf");
    else if(!strcmp(arg_field[i], "TIME_MODEL_STATS"))
      strcpy(dyn_vector_gdf_buf, "cm_oracle_awr_time_model_stats.gdf");
    else
    {
      sprintf(err_msg, CAV_ERR_1060072, arg_field[i]);
      CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
    }

    sprintf(dyn_vector_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s -M %s %s EOC %s %s -M %s %s %s",
                           server, arg_field[i], dyn_vector_gdf_buf,
                           std_mon_ptr->run_option, std_mon_ptr->pgrm_name, dyn_vector_prog_args_buf, arg_field[i], std_mon_ptr->fixed_args,
                           std_mon_ptr->pgrm_name, dyn_vector_prog_args_buf, arg_field[i], std_mon_ptr->fixed_args, dvm_vector_arg_buf);

    NSDL2_MON(NULL, NULL, "dyn_vector_mon_buf = %s", dyn_vector_mon_buf);

    if(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_vector_mon_buf, server_name, use_lps_flag, runtime_flag, NULL, err_msg, NULL, jsonElement, 0) < 0)
      ret_flag = -1; //just for error msg on runtime
  }

  if(ret_flag == -1)
    return ret_flag;
  
  return 0;
}

//set lps flag if DVM monitor is suppose to run with lps also check if 'lps_mode' is enable/disable, set lps flag accordingly
static void set_lps_flag(int *use_lps_flag, StdMonitor *std_mon_ptr)
{ 
  if(std_mon_ptr->use_lps)
  {
    NSDL2_MON(NULL, NULL, "std_mon_ptr->use_lps = %c", std_mon_ptr->use_lps);

    if(global_settings->lps_mode == 1)
      *use_lps_flag = 1;
    else if((global_settings->lps_mode == 0) && (strcasecmp(std_mon_ptr->pgrm_name, "cm_service_stats") == 0))
    {
      fprintf(stderr, "Warning: Log Parsing System (LPS) server name is not configured."
                      " Configuring service stats monitor to server ip 127.0.0.1 port 7892.\n");
      NS_DUMP_WARNING("Log Parsing System (LPS) server name is not configured."
                      " Configuring service stats monitor to server ip 127.0.0.1 port 7892.");
      kw_set_log_server("LPS_SERVER 127.0.0.1 7892 2", 0);

      *use_lps_flag = 1;
    }
    NSDL2_MON(NULL, NULL, "use_lps_flag = [%d]", use_lps_flag);
  }
}

// set vector argument acc to DVM type
static void set_vector_arg(char *dvm_vector_arg_buf, char *dvm_hv_vector_prefix, StdMonitor *std_mon_ptr) 
{ 
  NSDL2_MON(NULL, NULL, " Method called, dvm_vector_arg_buf = %s dvm_hv_vector_prefix =%s, std_mon_ptr->monitor_type = [%s]", dvm_vector_arg_buf, dvm_hv_vector_prefix, std_mon_ptr->monitor_type);

  if(strncmp(std_mon_ptr->monitor_type, "DVM1", 4) == 0)
    sprintf(dvm_vector_arg_buf, "-o header -v %s", dvm_hv_vector_prefix);
  else if(strncmp(std_mon_ptr->monitor_type, "DVM2", 4) == 0)
    sprintf(dvm_vector_arg_buf, "-v %s", dvm_hv_vector_prefix);
  else if(strncmp(std_mon_ptr->monitor_type, "DVM3", 4) == 0)
    sprintf(dvm_vector_arg_buf, "--prefix %s --operation show-vector", dvm_hv_vector_prefix); 
  else if(strcmp(std_mon_ptr->monitor_type, "DVM") == 0)
  {
    if(strcmp(std_mon_ptr->machine_types, "Windows") == 0)
       sprintf(dvm_vector_arg_buf, "/printLevel:header");
    else
      //sprintf(dvm_vector_arg_buf, "-X %s -L -o header", dvm_hv_vector_prefix); 
      sprintf(dvm_vector_arg_buf, "-L header");
  }
  else
    fprintf(stderr,"Invalid Monitor Type %s ", std_mon_ptr->monitor_type);

    NSDL2_MON(NULL, NULL, "dvm_vector_arg_buf = %s", dvm_vector_arg_buf);
}


// set data argument acc to DVM type
static void set_data_args(char *dvm_data_arg_buf, char *dvm_hv_vector_prefix, StdMonitor *std_mon_ptr)
{
  NSDL2_MON(NULL, NULL, " Method called, std_mon_ptr->monitor_type %s", std_mon_ptr->monitor_type);

  if(strncmp(std_mon_ptr->monitor_type, "DVM1", 4) == 0)
    sprintf(dvm_data_arg_buf, "-o data -v %s", dvm_hv_vector_prefix);
  else if(strncmp(std_mon_ptr->monitor_type, "DVM2", 4) == 0)
    sprintf(dvm_data_arg_buf, "-X %s -L data", dvm_hv_vector_prefix);
  else if(strncmp(std_mon_ptr->monitor_type, "DVM3", 4) == 0)
    sprintf(dvm_data_arg_buf, "--prefix %s --operation show-data", dvm_hv_vector_prefix);
  else if(strcmp(std_mon_ptr->monitor_type, "DVM") == 0)
  {
    if(strcmp(std_mon_ptr->machine_types, "Windows") == 0)
       sprintf(dvm_data_arg_buf, "/printLevel:data");
    else
      //sprintf(dvm_data_arg_buf, "-X %s -L -o data", dvm_hv_vector_prefix);
      sprintf(dvm_data_arg_buf, "-L data");
  }
  else
    sprintf(dvm_data_arg_buf, " ");

    NSDL2_MON(NULL, NULL, "dvm_data_arg_buf = %s", dvm_data_arg_buf);
}


// set vector prefix acc to hierarchical view mode enable/disable
static void set_vector_and_data_args(char *vector_name, StdMonitor *std_mon_ptr, char *dvm_vector_arg_buf, char *dvm_data_arg_buf)
{
  char dvm_hv_vector_prefix[MAX_BUF_SIZE_FOR_DYN_MON] = "";      
  NSDL2_MON(NULL, NULL, " Method called");
  sprintf(dvm_hv_vector_prefix, "noprefix");

  NSDL2_MON(NULL, NULL, "dvm_hv_vector_prefix = %s, vector_name = %s",dvm_hv_vector_prefix, vector_name);

  set_vector_arg(dvm_vector_arg_buf, dvm_hv_vector_prefix, std_mon_ptr);

  set_data_args(dvm_data_arg_buf, dvm_hv_vector_prefix, std_mon_ptr);
}


/* This func is to set : 
 *  vector prefix
 *  vector arguments
 *  data arguments
 *  and lps flag*/

void set_dvm_args(char *dvm_vector_arg_buf, char *dvm_data_arg_buf, int *use_lps_flag, char *vector_name, StdMonitor *std_mon_ptr)
{
  NSDL2_MON(NULL, NULL, " Method called..");
  set_vector_and_data_args(vector_name, std_mon_ptr, dvm_vector_arg_buf, dvm_data_arg_buf);

  set_lps_flag(use_lps_flag, std_mon_ptr);
}

void remove_hidden_file(int gdf_flag)
{
 char buff[1024]={0}; 
 for (int k = 0; k <total_hidden_file_entries; k++)
   {
     if(gdf_flag == hidden_file_ptr[k].gdf_flag)
     {
        if(hidden_file_ptr[k].is_file_created)
        { 
          sprintf(buff,"%s/logs/%s/%s",g_ns_wdir,global_settings->tr_or_common_files,hidden_file_ptr[k].hidden_file_name);
          unlink(buff);
          hidden_file_ptr[k].is_file_created = 0;
        }
     } 
   }
}

void create_and_fill_hidden_ex(int gdf_flag, int flag, JSON_info *json_ptr)
{
  
 for (int k = 0; k <total_hidden_file_entries; k++)
   {
     if(gdf_flag == hidden_file_ptr[k].gdf_flag)
     {
        if(!hidden_file_ptr[k].is_file_created || flag)
        {
            make_hidden_file(&hidden_file_ptr[k],global_settings->tr_or_partition , flag);
        }
     } 
   }
   if(json_ptr != NULL)
   {
     json_ptr -> generic_gdf_flag = gdf_flag;
   }
     global_settings->generic_mon_flag = 1; 
}
 



/**
Purpose         : This method is used to parse standard monitor keyword  and map with custom monitors.
Arguments       : STANDARD_MONITOR as keyword, Complete parse line as buf. 
Return Value    : None
**/
int kw_set_standard_monitor(char *keyword, char *buf, int runtime_flag, char *pod_name, char *err_msg, JSON_info *jsonElement)
{
  char key[MAX_BUF_SIZE_FOR_STD_MON];
  char server_name[MAX_BUF_SIZE_FOR_STD_MON];
  char vector_name[MAX_BUF_SIZE_FOR_STD_MON];
  char monitor_name[MAX_BUF_SIZE_FOR_STD_MON];
  char arguments[MAX_BIG_BUF_SIZE_FOR_STD_MON];
  char dyn_mon_buf[2*MAX_BIG_BUF_SIZE_FOR_STD_MON] = "";                //buffer for dynamic vector monitor
  char java_args[MAX_BUF_SIZE_FOR_CUS_MON] = {0};
  int tier_idx=-1;
  int num = 0, i = 0, total_num_args = 0, recv_buf_len = 0, use_lps_flag = 0;
  char custom_monitor_buf[2*MAX_BUF_SIZE_FOR_CUS_MON] = "";
  char *token_arr[2000];
  char pgm_args[32* MAX_BUF_SIZE_FOR_DYN_MON] = "";
  char dvm_vector_arg_buf[MAX_BUF_SIZE_FOR_DYN_MON] = "";    //for vector arguments
  char dvm_data_arg_buf[MAX_BUF_SIZE_FOR_DYN_MON] = "";      //for data arguments
  char mon_type_buf[64];
  //ServerCptr server_mon_ptr;
  StdMonitor *std_mon_ptr = NULL;
  int server_index = -1;
  int ret;
  char java_home_and_classpath[1024]={0}; 
  int len;
  int mbean_mon_idx;
  java_args[0] = 0;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf, "%s %s %s %s %s ", key, server_name, vector_name, monitor_name, arguments);
  NSDL2_MON(NULL, NULL, "key=[%s], server_name=[%s], vector_name=[%s], monitor_name=[%s], args=[%s] ",
                         key, server_name, vector_name, monitor_name, arguments);
/*
  // Validation
  if(num < 4) // All fields except arguments are mandatory.
  {
    sprintf(err_msg, CAV_ERR_1060073, buf);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0);
  }
*/
  recv_buf_len = strlen(buf);
  NSDL2_MON(NULL, NULL, "recv_buf_len = %d", recv_buf_len);

  pgm_args[0] = '\0'; // Make it null string first
  if(num > 4)
  {
    num--; // decrement num as it has first argument also (temp_buf)
    total_num_args = get_tokens(buf, token_arr, " ", 500); //get the total number of tokens
    NSDL2_MON(NULL, NULL, "total_num_args = [%d]", total_num_args);
    for(i = num; i < total_num_args; i++)
    {
      // To concatenate aruguments 
      strcat(pgm_args, token_arr[i]);
      strcat(pgm_args, " ");
    }
  }
  if(strstr(monitor_name,"AccessLog") && !(strstr(pgm_args, "-O ")))
    create_accesslog_id(server_name, pgm_args, vector_name, monitor_name);

  //finding tier_idx and server_idx from vector_name passed Tier>Server or Tier>Server>Monitor: supported after 4.6.0 
  if(find_tier_idx_and_server_idx(server_name, NULL, NULL, global_settings->hierarchical_view_vector_separator, hpd_port,&server_index,&tier_idx) == -1)
  {
    sprintf(err_msg, CAV_ERR_1060059, server_name, monitor_name); 
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
    NSDL1_MON(NULL, NULL, "ServerIndex = %d and Tier Index = %d", server_index,tier_idx);
  }
  NSDL1_MON(NULL, NULL, "ServerIndex = %d and Tier Index = %d", server_index,tier_idx);

  // Validation
  if(num < 4) // All fields except arguments are mandatory.
  {
    sprintf(err_msg, CAV_ERR_1060073, buf);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
  }

  //Decrement this counter here because calling 'find_tier_idx_and_server_idx' everywhere CM/DVM, it get incremented in CM/DVM
  //in order to do only one increment for each monitor decrementing here because we are closing control connection on basis of 'cmon_monitor_count'
  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--;
  NSDL1_MON(NULL, NULL, "Decremented cmon_monitor_count by 1. cmon_monitor_count = %d",topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count);

  // Get std_mon_ptr from jsonElement
  if (jsonElement && (StdMonitor*)jsonElement->std_mon_ptr)
  {
    std_mon_ptr = (StdMonitor*)jsonElement->std_mon_ptr;
    jsonElement->use_lps = std_mon_ptr->use_lps;
  }
  else {
    // Search standard monitor name and validate server types
    if((std_mon_ptr = get_standard_mon_entry(monitor_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->machine_type, runtime_flag, err_msg)) == NULL)
    {
      sprintf(err_msg, "Error: Standard Monitor Name '%s' or server type '%s' is incorrect\n", monitor_name,
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->machine_type);
      strcat(err_msg, "Please provide valid standard monitor name\n");
      return -1;
      //CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0);
    }
   if(jsonElement)
      jsonElement->use_lps = std_mon_ptr->use_lps;
  }
  
  if(!strcmp(std_mon_ptr->gdf_name, "NA_SR"))
  {
    if(runtime_flag && (global_settings->sql_report_mon == 0))
    {
       global_settings->sql_report_mon = 1;
       create_oracle_sql_stats_hidden_file();
    }
    global_settings->sql_report_mon = 1;
  }

  if(!strncasecmp(std_mon_ptr->gdf_name,"NA_SR",5))
    create_and_fill_hidden_ex(NA_SR, 0, jsonElement);  
  else if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_mysql",18))
    create_and_fill_hidden_ex(NA_GENERICDB_MYSQL, 0, jsonElement);   
  else if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_oracle",19))
    create_and_fill_hidden_ex(NA_GENERICDB_ORACLE, 0 , jsonElement);   
  else if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_postgres",21))
    create_and_fill_hidden_ex(NA_GENERICDB_POSTGRES, 0, jsonElement); 
  else if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_mssql",18))
    create_and_fill_hidden_ex(NA_GENERICDB_MSSQL, 0, jsonElement); 
  else if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_mongoDb",20))
    create_and_fill_hidden_ex(NA_GENERICDB_MONGO_DB, 0, jsonElement); 

  if(jsonElement && jsonElement->javaClassPath && jsonElement->javaClassPath[0] != '\0')
  {
    if(jsonElement->javaHome && jsonElement->javaHome[0] != '\0')
    {
      if(jsonElement->javaHome[strlen(jsonElement->javaHome) - 1] == '/' || jsonElement->javaHome[strlen(jsonElement->javaHome) - 1] == '\\')
        jsonElement->javaHome[strlen(jsonElement->javaHome) - 1] = '\0';

      if(strcasecmp(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->machine_type, "Windows"))
        sprintf(java_home_and_classpath,"%s/bin/java -cp %s",jsonElement->javaHome, jsonElement->javaClassPath);
      else
        sprintf(java_home_and_classpath,"%s\\bin\\java -cp %s", jsonElement->javaHome, jsonElement->javaClassPath);
    }
    else
      sprintf(java_home_and_classpath,"java -cp %s", jsonElement->javaClassPath);
  }
  else
    strcpy(java_home_and_classpath, "java");

  len = sprintf(java_args, "%s -DCAV_MON_HOME=%s -DMON_TEST_RUN=%d -DVECTOR_NAME=%s ", java_home_and_classpath,
  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->install_dir, testidx, vector_name);

  NSDL2_MON(NULL, NULL, "java_home_and_classpath=[%s],buf=[%s] ",java_home_and_classpath,buf);

  java_args[len] = '\0';

  NSDL2_MON(NULL, NULL, "java_args = [%s]", java_args);

  /*If buffer length is:  (1) greater than MAX_BUF_SIZE_FOR_DYN_MON
   *                   or (2) greater than MAX_BUF_SIZE_FOR_CUS_MON
   *                   Then we need to exit 
   *
   * In case of DVM, comparing with 2*recv_buf_len because for dynamic monitor we set buffer in format:                       * "buf EOC buf" which is almost 2*(strlen(buf)) */
  if((!strncmp(std_mon_ptr->monitor_type, "DVM", 3) && (2*recv_buf_len > MAX_BIG_BUF_SIZE_FOR_STD_MON)) ||
      (((!strcmp(std_mon_ptr->monitor_type, "CM")) || (!strcmp(std_mon_ptr->monitor_type, "NA"))) && (recv_buf_len > MAX_BUF_SIZE_FOR_CUS_MON)))
  {
    sprintf(err_msg, CAV_ERR_1060074, monitor_name, strcmp(std_mon_ptr->monitor_type, "DVM")?(recv_buf_len):(2*recv_buf_len), strcmp(std_mon_ptr->monitor_type, "DVM")?(MAX_BUF_SIZE_FOR_CUS_MON):(MAX_BIG_BUF_SIZE_FOR_STD_MON));
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
  }

  
  if (strncmp(std_mon_ptr->monitor_type, "DVM", 3) == 0)
  {
    if (!jsonElement)
    {
      MY_MALLOC_AND_MEMSET(jsonElement, sizeof(JSON_info), "Allocating jsonElement inside kw_set_standard_monitor", 0);
      jsonElement->use_lps = std_mon_ptr->use_lps;
 
      if(!strncasecmp(std_mon_ptr->gdf_name,"NA_genericDB_postgres",21))
        jsonElement->generic_gdf_flag = NA_GENERICDB_POSTGRES;
      if (mbean_mon_ptr && (std_mon_ptr->agent_type & CONNECT_TO_NDC)) 
      {
        mbean_mon_idx = search_mon_entry_in_list(monitor_name);
        mbean_mon_ptr[mbean_mon_idx].record_id = std_mon_ptr->record_id;
        MALLOC_AND_COPY(std_mon_ptr->config_json, mbean_mon_ptr[mbean_mon_idx].config_file, (strlen(std_mon_ptr->config_json) + 1),
            "config file", mbean_mon_idx);

        /*
          While applying standard monitor from add_in_cm_table_and_create_msg , jsonElement is NULL. Due to this
          we do not pass agent-type to cm_info structure. Here we are allocating jsonElement and setting agent-type
          agent-type is used while creating cm_info structure.
        */
        jsonElement->agent_type |= CONNECT_TO_NDC;

        sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC %s %s", server_name, vector_name, std_mon_ptr->gdf_name,
        std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args, dvm_data_arg_buf, std_mon_ptr->pgrm_name,
        dvm_vector_arg_buf);

        return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name,
        err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
      }
      else 
      {
	if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
	{
          if(std_mon_ptr->config_json)
          {
            MALLOC_AND_COPY(std_mon_ptr->config_json, jsonElement->config_json, (strlen(std_mon_ptr->config_json) + 1), "config file", -1);
	    jsonElement->agent_type = CONNECT_TO_CMON;
	  }
		              // MBean monitors will also execute this block
		              // java monitors needs program arguments also alongwith -v for vectors i.e. after EO
	  sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s %s EOC %s %s %s %s", server_name, vector_name, std_mon_ptr->gdf_name, std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args, dvm_data_arg_buf, java_args, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
	}
	else
	{
	  if(std_mon_ptr->use_args)
	  {
	    sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "						                                    "%s %s %s",server_name, vector_name, std_mon_ptr->gdf_name, std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args, dvm_data_arg_buf, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
          }
	  else
	  {
	    sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "							                                    "%s %s",server_name, vector_name, std_mon_ptr->gdf_name, std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args, dvm_data_arg_buf, std_mon_ptr->pgrm_name, dvm_vector_arg_buf);
	  }
	}
         return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name,
                                                                               err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
      }
    }
    else 
    { // for CONNECT_TO_CMON and NA
      NSDL2_MON(NULL, NULL, "std_mon_ptr->pgrm_name = %s, std_mon_ptr->pgrm_type = [%s]", std_mon_ptr->pgrm_name, std_mon_ptr->pgrm_type);

      //set following arguments: 
      //(1) vector argument (2) vector prefix (3) lps flag
      set_dvm_args(dvm_vector_arg_buf, dvm_data_arg_buf, &use_lps_flag, vector_name, std_mon_ptr);

      //Parsing arguments of this monitor using special parsing
      if((strstr(std_mon_ptr->pgrm_name, "cm_app_dynamics_stats") != NULL))
      {
        //parse argument and call dynamic vector monitor for each metric passed using option -M
        NSDL2_MON(NULL, NULL, "Completed arguments parsing of monitor = %s, now returning...", std_mon_ptr->pgrm_name);
        return(metric_based_mon_arg_parsing(pgm_args, server_name, vector_name, std_mon_ptr, server_name, dvm_vector_arg_buf, runtime_flag,
          err_msg, use_lps_flag, jsonElement,tier_idx,server_index));
      }

      if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
      {
        // MBean monitors will also execute this block
        // java monitors needs program arguments also alongwith -v for vectors i.e. after EOC
        sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s %s EOC %s %s %s %s",
          server_name, vector_name, std_mon_ptr->gdf_name, std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args,
          std_mon_ptr->fixed_args, dvm_data_arg_buf, java_args, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
      }
      else
      {
        //linux and c based monitors do not need program arguments after EOC except below monitors 
        /*if((strstr(std_mon_ptr->pgrm_name, "cm_service_stats") != NULL) ||
          (strstr(std_mon_ptr->pgrm_name, "cm_jk_apache_stats") != NULL) ||
          (strstr(std_mon_ptr->pgrm_name, "cm_cassandra") != NULL))*/
        if(std_mon_ptr->use_args)
        {
          sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "
                              "%s %s %s",
                              server_name, vector_name, std_mon_ptr->gdf_name,
                              std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                              dvm_data_arg_buf, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
        }
        else
        {
          sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "
                              "%s %s",
                              server_name, vector_name, std_mon_ptr->gdf_name,
                              std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                              dvm_data_arg_buf, std_mon_ptr->pgrm_name, dvm_vector_arg_buf);
        }
      }

      NSDL2_MON(NULL, NULL, "dyn_mon_buf= %s, server_name = %s", dyn_mon_buf, server_name);

      if(jsonElement && jsonElement->init_vector_file && jsonElement->init_vector_file[0] != '\0')
      {
        return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name,
          err_msg, jsonElement->init_vector_file, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
    
      }
      else
      {
        return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name,
          err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
      }
    }
  }
  else if (strcmp(std_mon_ptr->monitor_type, "CM") == 0) 
  {
    // agent_type of monitor_type == "CM" is always COMMECT_TO_CMON

    if(std_mon_ptr->use_lps)
      sprintf(mon_type_buf, "LOG_MONITOR");
    else
      sprintf(mon_type_buf, "CUSTOM_MONITOR");

    // To map with custom monitor keyword
    if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
      sprintf(custom_monitor_buf, "%s %s %s %s %d %s%s %s %s", mon_type_buf, server_name, std_mon_ptr->gdf_name, vector_name,
      std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args);
    else
      sprintf(custom_monitor_buf, "%s %s %s %s %d %s %s %s", mon_type_buf, server_name, std_mon_ptr->gdf_name, vector_name,
      std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args);

    NSDL2_MON(NULL, NULL, "custom_monitor_buf = %s", custom_monitor_buf);

    // To parse custom monitor buffer
    ret = custom_monitor_config(mon_type_buf, custom_monitor_buf, server_name, 0, runtime_flag, err_msg, pod_name, jsonElement,
      std_mon_ptr->skip_breadcrumb_creation);

    if(ret >= 0)
    {
      if(is_outbound_connection_enabled)
        g_mon_id = get_next_mon_id();

      monitor_added_on_runtime = 1;
    }
    return ret;
  }  
  else 
  {
    sprintf(err_msg, CAV_ERR_1060075, std_mon_ptr->monitor_type, vector_name);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
  }

  return 0;
}

#if 0
  /* In standard_monitor.dat, 
   *  monitor_type "DVM" means -> dynamic vector monitor. 
   *  monitor_type "CM"/"NA" means -> custom monitor. */

  /* All "DVM" type monitor will be used as standard monitor in mprof.
   * But internally will execute them as dynamic monitor. */
  if(!strncmp(std_mon_ptr->monitor_type, "DVM", 3))
  {
    NSDL2_MON(NULL, NULL, "std_mon_ptr->pgrm_name = %s, std_mon_ptr->pgrm_type = [%s]", std_mon_ptr->pgrm_name, std_mon_ptr->pgrm_type);

    //set following arguments: 
    //(1) vector argument (2) vector prefix (3) lps flag
    set_dvm_args(dvm_vector_arg_buf, dvm_data_arg_buf, &use_lps_flag, vector_name, std_mon_ptr);

    //Parsing arguments of this monitor using special parsing
    if((strstr(std_mon_ptr->pgrm_name, "cm_app_dynamics_stats") != NULL) || (strstr(std_mon_ptr->pgrm_name, "cm_oracle_stats") != NULL))
    {
      //parse argument and call dynamic vector monitor for each metric passed using option -M
      NSDL2_MON(NULL, NULL, "Completed arguments parsing of monitor = %s, now returning...", std_mon_ptr->pgrm_name);
      return(metric_based_mon_arg_parsing(pgm_args, server_name, vector_name, std_mon_ptr, server_name, dvm_vector_arg_buf, runtime_flag, err_msg, use_lps_flag, server_mon_ptr, jsonElement));
    }

    if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
    {
      //java monitors needs program arguments also alongwith -v for vectors i.e. after EOC
      sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s %s EOC "
                            "%s %s %s %s",
                            server_name, vector_name, std_mon_ptr->gdf_name,
                            std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args, dvm_data_arg_buf,
                            java_args, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
    }
    else
    {
       //linux and c based monitors do not need program arguments after EOC except below monitors 
       /*if((strstr(std_mon_ptr->pgrm_name, "cm_service_stats") != NULL) ||
         (strstr(std_mon_ptr->pgrm_name, "cm_jk_apache_stats") != NULL) ||
         (strstr(std_mon_ptr->pgrm_name, "cm_cassandra") != NULL))*/
      if(std_mon_ptr->use_args)
      {
        sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "
                             "%s %s %s",
                             server_name, vector_name, std_mon_ptr->gdf_name,
                             std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                             dvm_data_arg_buf, std_mon_ptr->pgrm_name, pgm_args, dvm_vector_arg_buf);
      }
      else
      {
        sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "
                             "%s %s",
                             server_name, vector_name, std_mon_ptr->gdf_name,
                             std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                             dvm_data_arg_buf, std_mon_ptr->pgrm_name, dvm_vector_arg_buf);
      }
    }

    NSDL2_MON(NULL, NULL, "dyn_mon_buf= %s, server_name = %s", dyn_mon_buf, server_name);
  
    if(jsonElement && jsonElement->init_vector_file && jsonElement->init_vector_file[0] != '\0')
    {
      return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name, err_msg, jsonElement->init_vector_file, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
  
    }
    else
    {
      return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name, err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
    }

  }
  else if(!strcmp(std_mon_ptr->monitor_type, "CM"))  //other standard monitors will execute as standard monitor only
  {
    if(std_mon_ptr->use_lps)
      sprintf(mon_type_buf, "LOG_MONITOR");
    else
      sprintf(mon_type_buf, "CUSTOM_MONITOR");

    // To map with custom monitor keyword
    if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
      sprintf(custom_monitor_buf, "%s %s %s %s %d %s%s %s %s", mon_type_buf, server_name, std_mon_ptr->gdf_name, vector_name, std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args);
    else
      sprintf(custom_monitor_buf, "%s %s %s %s %d %s %s %s", mon_type_buf, server_name, std_mon_ptr->gdf_name, vector_name, std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args);

    NSDL2_MON(NULL, NULL, "custom_monitor_buf = %s", custom_monitor_buf);

    // To parse custom monitor buffer
    ret = custom_monitor_config(mon_type_buf, custom_monitor_buf, server_name, 0, runtime_flag, err_msg, pod_name, jsonElement, std_mon_ptr->skip_breadcrumb_creation);
    if(ret >= 0)
    {
      //We wont be increasing mon_id for lps_based monitor.
      if(! std_mon_ptr->use_lps)
        g_mon_id = get_next_mon_id();

      monitor_added_on_runtime = 1;
    }
    return ret;
  }
  else if(!strncasecmp(std_mon_ptr->monitor_type, "MBean", 5))
  {
    if(jsonElement && jsonElement->agent_type & CONNECT_TO_CMON)
    {
      if(strcasecmp(std_mon_ptr->pgrm_type, "java") == 0)
      {
        if(!strcasecmp(std_mon_ptr->monitor_type, "MBeanCM"))
        {
          sprintf(mon_type_buf, "CUSTOM_MONITOR");
     
          sprintf(custom_monitor_buf, "%s %s %s %s %d %s%s %s %s", mon_type_buf, server_name, std_mon_ptr->gdf_name, vector_name, std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args);
     
          ret = custom_monitor_config(mon_type_buf, custom_monitor_buf, server_name, 0, runtime_flag, err_msg, pod_name, jsonElement, std_mon_ptr->skip_breadcrumb_creation);
          return ret;
        }
        else 
        { 
          sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s %s EOC %s %s",
                               server_name, monitor_name, std_mon_ptr->gdf_name,
                               std_mon_ptr->run_option, java_args, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                               dvm_data_arg_buf, std_mon_ptr->pgrm_name, dvm_vector_arg_buf);
        
          return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name, err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
        }
      }
      else
      {
        NSTL1(NULL, NULL, "The monitor (%s) used is Invalid. This is Mbean type monitor and it must have 'java' as program type.", monitor_name);
        return -1;
      }
    }
    else if (mbean_mon_ptr)
    {
      mbean_mon_idx = search_mon_entry_in_list(monitor_name);
      mbean_mon_ptr[mbean_mon_idx].record_id = std_mon_ptr->record_id;
      MALLOC_AND_COPY(std_mon_ptr->config_json, mbean_mon_ptr[mbean_mon_idx].config_file, (strlen(std_mon_ptr->config_json) + 1), "config file", mbean_mon_idx);
    }
   
      sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s %s %s %d %s %s %s %s EOC "
                             "%s %s",
                             server_name, vector_name, std_mon_ptr->gdf_name,
                             std_mon_ptr->run_option, std_mon_ptr->pgrm_name, pgm_args, std_mon_ptr->fixed_args,
                             dvm_data_arg_buf, std_mon_ptr->pgrm_name, dvm_vector_arg_buf);

      return(kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, server_name, use_lps_flag, runtime_flag, pod_name, err_msg, NULL, jsonElement, std_mon_ptr->skip_breadcrumb_creation));
 
  }
  else
  {
    sprintf(err_msg, CAV_ERR_1060075, std_mon_ptr->monitor_type, vector_name);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
  }
#endif

#if 0
int main()
{
 // int i;
  char buf[1024]="";
  char key[25]="";
  char text[1000]="";
   FILE *fps;
  
 // i = read_std_mon_dat();
   if((fps = fopen("Scenario.conf", "r")) == NULL)
  {
    fprintf(stderr, "Unable to open config file\n");
    exit(-1);
  }
   while (fgets(buf, MAX_CONF_LINE_LENGTH, fps) != NULL)
   {
     buf[strlen(buf)-1] = '\0';
       if((buf[0] == '#') || (buf[0] == '\0'))
         continue;
       sscanf(buf, "%s %s", key, text);
     //  kw_set_standard_config(key, buf, i); 
   }
#endif
