/********************************************************************************
 * File Name            : ns_param_override.c 
 *
 * Purpose              : Contains parsing function for G_SCRIPT_PARAM keyword, 
 *                        and overrides the declare parameter value with the
 *                        values given by user.
 *
 ********************************************************************************/

#include <stdio.h>
#include <string.h>

#include <sys/wait.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#ifdef USE_EPOLL
  #include <sys/epoll.h>
#endif
#include <regex.h>
#include <libpq-fe.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_msg_def.h"
#include "ns_static_vars.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "ns_string.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h" 
#include "divide_values.h" 
#include "child_init.h"
#include "eth.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "server_stats.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_child.h"
#include "ns_parent.h"
#include "ns_health_monitor.h"
#include "ns_summary_rpt.h"
#include "ns_goal_based_sla.h"
#include "ns_goal_based_run.h"
#include "ns_wan_env.h"
#include "ns_url_req.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_debug_trace.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_event_log.h"
#include "ns_custom_monitor.h"
#include "smon.h"
#include "ns_parse_scen_conf.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_sock_com.h"
#include "ns_schedule_phases_parse_validations.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases.h"
#include "ns_global_dat.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_server_admin_utils.h"
#include "ns_smtp_parse.h"
#include "ns_keep_alive.h"
#include "ns_static_use_once.h"
#include "nslb_hash_code.h"
#include "nslb_date.h"
#include "nslb_cav_conf.h"
#include "ns_event_filter.h"
#include "ns_event_id.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_http_cache_table.h"
#include "ns_vuser_trace.h"

#include "tr069/src/ns_tr069_lib.h"
#include "tr069/src/ns_tr069_data_file.h"
#include "ns_auto_fetch_parse.h"
#include "ns_url_hash.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_runtime_changes.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_lps.h"
#include "deliver_report.h"
#include "ns_http_hdr_states.h"

#include "nslb_http_state_transition_init.h"
#include "nslb_db_util.h"
//#include "nslb_license.h"
#include "ns_http_process_resp.h"
#include "ns_dynamic_hosts.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_page_dump.h"
#include "nslb_hm_disk_space.h"
#include "nslb_hm_disk_inode.h"
#include "ns_proxy_server.h"
#include "ns_proxy_server_reporting.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_monitoring.h"
#include "ns_license.h"
#include "ns_njvm.h"
#include "ns_trace_level.h"
#include "ns_ndc.h"
#include "nslb_signal.h"
#include "ns_session.h"
#include "nia_fa_function.h"
#include "nslb_db_upload_common.h"
#include "ns_send_mail.h"
#include "ns_rbu.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_runtime_changes_quantity.h"
#include "v1/topolib_structures.h"
#include "ns_ip_data.h"
#include "ns_inline_delay.h"
#include "ns_page_think_time.h"
#include "ns_trans_parse.h"
#include "nslb_netcloud_util.h"
#include "db_aggregator.h"
#include "ns_replay_db_query.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_ndc_outbound.h"
#include "ns_param_override.h"
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif


ParamOverrideTable_t *paramOverrideTable = NULL;

int total_paramoverride_entries = 0;
int max_paramoverride_entries = 0;

int create_paramoveride_table_entry(int *row_num) {
  NSDL1_PARSING(NULL, NULL, "Method called");
  if (total_paramoverride_entries == max_paramoverride_entries) {
    MY_REALLOC_EX(paramOverrideTable, (max_paramoverride_entries + DELTA_PARAM_OVERRIDE_ENTRIES) * sizeof(ParamOverrideTable_t), max_paramoverride_entries * sizeof(ParamOverrideTable_t), "ParamOverrideTable", -1);
    if (!paramOverrideTable) {
      fprintf(stderr,"create_paramoveride_table_entry(): Error allocating more memory for ParamOverrideTable entries\n");
      return(FAILURE);
    } else max_paramoverride_entries += DELTA_PARAM_OVERRIDE_ENTRIES;
  }               
  *row_num = total_paramoverride_entries++;
  return (SUCCESS);
}      

//Parsing the G_SCRIPT_PARAM keyword
//return 0 if successfully parsed keyword and -1 if parsing not successful
int kw_set_g_script_param(char *buf, char *err_msg){
  char dummy[MAX_DATA_LINE_LENGTH] = {0};
  char sg_name[MAX_DATA_LINE_LENGTH] = {0};
  char param_name[MAX_DATA_LINE_LENGTH] = {0};
  char param_value[MAX_DATA_LINE_LENGTH] = {0};
  int row_num;
  char *ptr = NULL, *str = NULL;
  int count=0;

  NSDL1_PARSING(NULL, NULL, "Method Called, buf = %s", buf);
  
  str=strdup(buf);
  CLEAR_WHITE_SPACE(str);
  while((ptr=strchr( str, ' ')) != NULL)
  {
    count++;
    *ptr='\0';
    ptr++;
    CLEAR_WHITE_SPACE(ptr);

    if(count == 1) {
      sprintf(dummy, "%s", str);
    } else if(count == 2) {
      sprintf(sg_name, "%s", str);
    } else if(count == 3) {
      sprintf(param_name, "%s", str);
      break;
    }
    str= ptr;
  }

  //Check if Keyword has less arguments than required
  if((ptr == NULL) || (count == 2) || (*ptr == '\0'))
  {
    if ((*str != '\0') && (count == 2))
    {
      sprintf(param_name, "%s", str);
    }
    printf("Invalid number of arguments for Keyword G_SCRIPT_PARAM \n");
    return -1;
  }
  else
  {
    if(strlen(ptr) > MAX_DATA_LINE_LENGTH)
    {
      printf("Param value is more than max length, So ignoring remaining value.\n");
    }
    snprintf(param_value, MAX_DATA_LINE_LENGTH, "%s", ptr);
  }

  //create paramoveride table and filling group_name, parameter_name, parameter_value, in table
  create_paramoveride_table_entry(&row_num);
  paramOverrideTable[row_num].grp_idx = find_group_idx(sg_name);
  strcpy(paramOverrideTable[row_num].grp_name, sg_name);
  strcpy(paramOverrideTable[row_num].param_name, param_name);
  strcpy(paramOverrideTable[row_num].param_value, param_value);

  NSDL2_PARSING(NULL, NULL, "sg_name = %s, param_name = %s, param_value = %s", paramOverrideTable[row_num].grp_name,
                             paramOverrideTable[row_num].param_name, paramOverrideTable[row_num].param_value);
  return 0;
}

int  g_max_script_decl_param = 0;
static void replaces_from_group(int param_override_id, int grp_id)
{
  int total_script_decl_param;
  int start_idx, j;
  NsVarTableEntry* loc_nsVarTable;

  //Total declared parameter count in that group
  total_script_decl_param = gSessionTable[runProfTable[grp_id].sessprof_idx].num_nslvar_entries;
  start_idx = gSessionTable[runProfTable[grp_id].sessprof_idx].nslvar_start_idx;

  //First time malloc loc_nsVarTable equal to total_script_decl_param used in group to making group based
  if (runProfTable[grp_id].grp_ns_var_start_idx == -1)
  {
    runProfTable[grp_id].grp_ns_var_start_idx = g_max_script_decl_param;
    MY_REALLOC_EX(grpNsVarTable, (g_max_script_decl_param + total_script_decl_param) * sizeof(NsVarTableEntry), g_max_script_decl_param * sizeof(NsVarTableEntry), "grpNsVarTable", -1);
    for (int i = g_max_script_decl_param; i < g_max_script_decl_param + total_script_decl_param; i++)
      grpNsVarTable[i].default_value = -1;

    g_max_script_decl_param += total_script_decl_param;
  }

  loc_nsVarTable = grpNsVarTable + runProfTable[grp_id].grp_ns_var_start_idx;

  NSDL2_PARSING(NULL, NULL, "grp_idx = %d, total_script_decl_param = %d, start_idx = %d, param_override_id = %d",
                             grp_id, total_script_decl_param, start_idx, param_override_id);
  for(j = 0; j < total_script_decl_param; j++)
  {
    if(!strcmp(RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[start_idx + j].name), paramOverrideTable[param_override_id].param_name))
    {
      nsVarTable[start_idx + j].default_value = copy_into_big_buf(paramOverrideTable[param_override_id].param_value, 0);
      loc_nsVarTable[j].default_value = nsVarTable[start_idx + j].default_value;
      loc_nsVarTable[j].type = runProfTable[grp_id].sessprof_idx;
      int var_idx = gSessionTable[runProfTable[grp_id].sessprof_idx].var_hash_func(paramOverrideTable[param_override_id].param_name, strlen(paramOverrideTable[param_override_id].param_name));
      loc_nsVarTable[j].length = gSessionTable[runProfTable[grp_id].sessprof_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
      break;
    }
  }
}

void replace_overide_values ()
{
  int i, k;
  int grp_idx;

  NSDL1_PARSING(NULL, NULL, "Method called");
  for(i = 0; i < total_paramoverride_entries; i++)
  {
    grp_idx = paramOverrideTable[i].grp_idx;
    //for ALL
    if(grp_idx == -1)
    {
      for(k = 0; k < total_runprof_entries; k++)
      {
        replaces_from_group(i, k);
      }
    }
    else 
    {
      //for particular grp
      replaces_from_group(i, grp_idx);
    }
  }
}
