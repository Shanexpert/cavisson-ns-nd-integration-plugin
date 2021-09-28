#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include "v1/topolib_structures.h"
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "url.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_parse_scen_conf.h"
#include "ns_ssl.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_log.h"
#include "ns_kw_set_non_rtc.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "nslb_util.h"
#include "ns_auto_fetch_parse.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_runtime_changes_monitor.h"
#include <error.h>
#include "ns_session_pacing.h"
#include "ns_page_think_time_parse.h"
#include "ns_sync_point.h"
#include "ns_monitor_profiles.h"
#include "ns_master_agent.h"
#include "ns_trace_level.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_page_dump.h"
#include "ns_custom_monitor.h"
#include "ns_inline_delay.h"
#include "ns_continue_on_page.h"
#include "ns_nd_kw_parse.h"
#include "ns_runtime_changes.h"
#include "ns_auto_scale.h" 
#include "ns_svr_ip_normalization.h"
#include "ns_server_ip_data.h"
#include "ns_runtime.h"
#include "ns_handle_alert.h"
#include "ns_socket.h"
#include "ns_socket_tcp_client_failures_rpt.h"
#include "ns_monitor_init.h"
 
int kw_set_nd_enable_data_validation(char *keyword, char *buf, char *err_msg, int runtime_flag); 
double round(double x);
FILE *rtc_log_fp = NULL;
//static FILE *runtime_all_fp = NULL;
//extern u_ns_ts_t local_rtc_epoll_start_time;
int first_time = -1;
int runtime_id = -1;
int nc_flag_set = 0;
//int for_quantity_rtc = 0;
//char qty_msg_buff[RTC_QTY_BUFFER_SIZE];
//char nc_rtc_msg[RTC_QTY_BUFFER_SIZE]; //In case of generator we need to create rtc message
/* 
 * NOTE: All run time changes MUST NOT have exit as it will stop the test
 */

/* Returns:
 *   -1 for ALL 
 *   -2 for Invalid group 
 *  group id for grp */

#define NS_GRP_IS_ALL      -1
#define NS_GRP_IS_INVALID  -2

#define SESSION_SCHEDULE_RTC 4
#define PER_USER_SESSION_SCHEDULE_RTC 5

void set_rtc_info(Msg_com_con *mccptr, int rtc_type)
{
  NSDL2_RUNTIME(NULL, NULL, "Method called, rtc_type = %d, mccptr = %p", rtc_type, mccptr);

  rtcdata->type = rtc_type;
  SET_RTC_FLAG(RUNTIME_PROGRESS_FLAG);
  rtcdata->invoker_mccptr = mccptr;

  switch(rtc_type) {
    case APPLY_FPARAM_RTC:
      SET_RTC_FLAG(RUNTIME_FPARAM_FLAG);
      break;
    
    case APPLY_CAVMAIN_RTC:
      SET_RTC_FLAG(RUNTIME_CAVMAIN_FLAG);
      break;

    case APPLY_QUANTITY_RTC:
      SET_RTC_FLAG(RUNTIME_QUANTITY_FLAG);
      break;

    case APPLY_PHASE_RTC:
      SET_RTC_FLAG(RUNTIME_SCHEDULE_FLAG);
      break;

    case APPLY_MONITOR_RTC:
    case TIER_GROUP_RTC:
      SET_RTC_FLAG(RUNTIME_MONITOR_FLAG);
      break;

    case RESET_RTC_INFO:
      RESET_RTC_FLAG(RUNTIME_SET_ALL_FLAG);
      rtcdata->cur_state = RESET_RTC_STATE;
      break;
   
    case APPLY_ALERT_RTC:
      SET_RTC_FLAG(RUNTIME_ALERT_FLAG);
      break;

    default:
      NSTL1(NULL, NULL, "Unknown RTC type is passed from %s", loader_opcode == CLIENT_LOADER?"Master":"Tool");
      break;
   }
  
  NSDL1_RUNTIME(NULL, NULL, "Method exit, RTC flags = [%x]", rtcdata->flags);
}

int
find_grp_idx_from_kwd(char *buf, char *err_msg)
{
  int num;
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int idx;
  
  if ((num = sscanf(buf, "%s %s %s", keyword, grp, text)) < 3) {
    sprintf(err_msg, "Invalid number of fields in keyword '%s'", keyword);
    return NS_GRP_IS_INVALID;
  }
  
  if (strcasecmp(grp, "ALL") == 0) {
    return NS_GRP_IS_ALL;
  } else {
    idx = find_sg_idx_shr(grp);
    if (idx == -1) {
      sprintf(err_msg, "Group '%s' used in keyword '%s' is not a valid group", grp, keyword);
      return NS_GRP_IS_INVALID;
    }
    return idx;
  }
}

int
find_grp_idx_from_kwd_line(char *buf)
{
char err_msg[1024];

 return(find_grp_idx_from_kwd(buf, err_msg));
}
#if 0
static char *get_cur_time()
{
  time_t    tloc;
  struct  tm *lt;
  static  char cur_time[100];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_time, "Error");
  else
    sprintf(cur_time, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_time);
}
#endif

extern void
handle_parent_sickchild( int sig );




/* This function is called for Advanced scenario,
 * it returns the index of phase which matches with the phase name
 * if return value is -1 must be checked
 * */
static int get_phase_index_by_phase_name(int grp_idx, char *phase_name) {

  Schedule *ptr;
  int i;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, phase_name = %s", grp_idx, phase_name);

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    ptr = scenario_schedule;
  } else {
    ptr = &group_schedule[grp_idx];
  }

  for(i = 0; i< ptr->num_phases; i++) {
    if(strcmp(ptr->phase_array[i].phase_name, phase_name) == 0)
       return i;
  }

  return -1;
}

static inline int is_active_gen_on_grp(int gen_idx, int grp_idx)
{ 
  int i;
  NSDL2_RUNTIME(NULL, NULL, "Method called, gen_idx = %d, grp_idx = %d", gen_idx, grp_idx);
  for (i = 0; i < runprof_table_shr_mem[grp_idx].num_generator_per_grp; i++)
  {
    if (runprof_table_shr_mem[grp_idx].running_gen_value[i].id == gen_idx)
    {
      return 1;
    }
  }
  return 0;
}

schedule *get_gen_grp_schedule(int gen_idx, int grp_idx)
{
  int gidx = 0;
  int j, k = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method called, gen_idx = %d, grp_idx = %d", gen_idx, grp_idx);

  while(gidx < total_runprof_entries)
  {
    if(gidx == grp_idx)
    {
      for( j = 0 ; j < scen_grp_entry[k].num_generator; j++)
      {
        NSDL2_RUNTIME(NULL, NULL, "k = %d, j = %d, total gens = %d, gen on sgrp = %s, gen on table = %s", k, j,
            scen_grp_entry[k].num_generator, scen_grp_entry[k + j].generator_name, generator_entry[gen_idx].gen_name);
        if(!strcmp(scen_grp_entry[k + j].generator_name, (char *)generator_entry[gen_idx].gen_name))
          return &group_schedule_ptr[k + j];
        else
          continue;
        return NULL; //It should not come here
      }
    } 
    k += scen_grp_entry[k].num_generator;
    gidx++;
  }
  NSDL1_RUNTIME(NULL, NULL, "Method exited, gidx = %d, total_runprof_entries = %d. It must not come here", gidx, total_runprof_entries);
  return NULL;
}

/**********************************************************************************************
  Purpose:  This function will modify generator buffer which will be transferred to generators
            based on ramp up/down phase generator specific users on advance type schedule
  Bug:      50330
  Fixed by: Gaurav
  Mod Date: 29/11/2019
  is_ramp_up  - 1 RAMP_UP phase, 0 - RAMP_DOWN_PHASE
**********************************************************************************************/
void update_advance_ramp_up_down_users(int grp_idx, int phase_idx, char *full_buf, int is_ramp_up_phase)
{
  char buf[MAX_DATA_LINE_LENGTH];
  char *fields[20];
  int vuser_or_sess;
  int gen_idx;
  int offset;
  int i, num_fields;
  int num_phases;
  int replace_vuser_flag = 1;
  ni_Phases *ph;
  schedule *schedule;

  strcpy(buf, full_buf);
  num_fields = get_tokens(buf, fields, " ", 20);

  NSDL2_RUNTIME(NULL, NULL, "Method called, buf = %s, num_fields = %d", full_buf, num_fields);

  for(gen_idx = 0; gen_idx < sgrp_used_genrator_entries; gen_idx++)
  {
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      ph = &generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx];
      num_phases = generator_entry[gen_idx].scenario_schedule_ptr->num_phases;
    }
    else {
      if(!is_active_gen_on_grp(gen_idx, grp_idx)) //if generator does not have grp_idx provided
        continue;
      if(!(schedule = get_gen_grp_schedule(gen_idx, grp_idx))) //no data for gen_idx and grp_idx
      {
        NS_EXIT(-1, "Invalid scheduling details for group %s generator %s",
                    runprof_table_shr_mem[grp_idx].scen_group_name, generator_entry[gen_idx].gen_name);
      }
      NSDL2_RUNTIME(NULL, NULL, "schedule group idx = %d, grp_idx", schedule->group_idx, grp_idx);
      ph = &schedule->phase_array[phase_idx];
      num_phases = schedule->num_phases;
    }
    //which phase
    if(is_ramp_up_phase)
    {
      vuser_or_sess = ph->phase_cmd.ramp_up_phase.num_vusers_or_sess;  
    }
    else //ramp_down_phase
    {
      vuser_or_sess = ph->phase_cmd.ramp_down_phase.num_vusers_or_sess;  
      if((!is_ramp_up_phase && (phase_idx == num_phases - 1)))
        replace_vuser_flag = 0;
    }

    NSDL1_RUNTIME(NULL, NULL, "grp_idx = %d, gen_idx = %d, vuser_or_sess = %d", grp_idx, gen_idx, vuser_or_sess);
    //make phase buffer here and copy to gen buffer
    offset = 0; //reset offset before making phase msg
    for(i = 0; i < num_fields; i++)
    {  
      if((i == 4) && replace_vuser_flag && (get_grp_mode(grp_idx) == TC_FIX_USER_RATE))
        offset += sprintf(generator_entry[gen_idx].gen_keyword + offset, "%0.3f", vuser_or_sess/SESSION_RATE_MULTIPLIER);
      else if((i == 4) && replace_vuser_flag)
        offset += sprintf(generator_entry[gen_idx].gen_keyword + offset, "%d", vuser_or_sess);
      else
        offset += sprintf(generator_entry[gen_idx].gen_keyword + offset, "%s",  fields[i]);
      if(i != num_fields - 1) //add space
        generator_entry[gen_idx].gen_keyword[offset++] = ' ';
    }
    NSDL1_RUNTIME(NULL, NULL, "grp_idx = %d, gen_idx = %d, buf = %s", grp_idx, gen_idx, generator_entry[gen_idx].gen_keyword);
  }
  NSDL2_RUNTIME(NULL, NULL, "Method exit");
}

int parse_runtime_schedule_phase_start(int grp_idx, char *full_buf, char *phase_name, char *err_msg) {
  int i, dependent_grp, phase_index;
  int dependent_grp_idx, num_fields;
  u_ns_ts_t time;
  int buf_len;
  char *fields[20];
  char *buf, *buf_more;
  char buf_backup[MAX_DATA_LINE_LENGTH + 1];

  Phases *tmp_ph; /**parent_tmp_ph*/
  Schedule *cur_schedule;
  s_child_ports *v_port_entry_ptr;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

  if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
    phase_index = 0; 
  else {
    phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
    if(phase_index == -1) {
      sprintf(err_msg, "Phase '%s' does not exists.", phase_name);
      return -1;
    }
  }

  buf_len = strlen(full_buf);
  if (full_buf[buf_len - 1] == '\n') {
    full_buf[buf_len - 1] = '\0';
  }

  strncpy(buf_backup, full_buf, MAX_DATA_LINE_LENGTH);      /* keep a backup to print */

  buf = strstr(full_buf, "START");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += 5; //strlen("START");
    buf_more = strstr(buf_more, "START");
    if (buf_more) buf = buf_more;
  }

  num_fields = get_tokens(buf, fields, " ", 20);
  
  if (num_fields < 2) {
    sprintf(err_msg, "%s", "Atleast one argument required after START");
    return -1;
    //usage_parse_schedule_phase_start(NULL);
  }
  
  if (strcmp(fields[1], "IMMEDIATELY") == 0) {
    time = 0;
    dependent_grp = -1;
  } else if (strcmp(fields[1], "AFTER") == 0) {
    
    if (num_fields < 3) {
      sprintf(err_msg, "Atleast one argument required after AFTER in [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_start(NULL);
    }
    
    if (strcmp(fields[2], "TIME") == 0) {

      if (num_fields < 4) {
        sprintf(err_msg, "Time in format HH:MM:SS required after TIME in [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_start(NULL);
      }
    
      time = get_time_from_format(fields[3]);
      if (time < 0) {
        sprintf(err_msg, "Invalid time format in %s", buf_backup);
        return -1;
        //usage_parse_schedule_phase_start(NULL);
        
      }
      dependent_grp = -1;
      NSDL4_RUNTIME(NULL, NULL, "parsing grp time  = %u\n", 
                     time);

    } else if (strcmp(fields[2], "GROUP") == 0) {
      
      if (num_fields < 4) {
        sprintf(err_msg, "Group must be specified after GROUP in [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_start(NULL);
      }
      if ((dependent_grp_idx = find_sg_idx_shr(fields[3])) != -1) {
        dependent_grp = dependent_grp_idx;

        if (num_fields > 5) {
           sprintf(err_msg, "%s", "num_fields is > 5 for START phase");
           return -1;
          //usage_parse_schedule_phase_start(NULL);
        }

        if (num_fields == 5) { /* time */
          time = get_time_from_format(fields[4]);
          if (time < 0) {
            sprintf(err_msg, "Invalid time format in %s", buf_backup);
            return -1;
          }
        } else {
          time = 0;
        }

      } else {
        sprintf(err_msg, "Invalid group in %s", buf_backup);
        return -1;
        //usage_parse_schedule_phase_start(NULL);
      }
    } else {
      sprintf(err_msg, "Invalid option with START (%s)", buf_backup);
      return -1;
      //usage_parse_schedule_phase_start(NULL);
    }
  }
  /*NC: In case of controller we need to by-pass the code to update nvm structure*/
  if(loader_opcode != MASTER_LOADER) 
  {
    // fill for all childs
    for(i = 0; i < global_settings->num_process; i++) {
      /* Start phase will always be there. */
      v_port_entry_ptr = &v_port_table[i];
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        cur_schedule = v_port_entry_ptr->scenario_schedule;
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
        //parent_tmp_ph = &(scenario_schedule->phase_array[phase_index]);
      } else {
        cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
        //parent_tmp_ph = &(group_schedule[grp_idx].phase_array[phase_index]);
      }
  
      /* It may possible that few childs have updated their data because of check PHASE_IS_COMPLETED*/
      if(tmp_ph->phase_status == PHASE_IS_COMPLETED) {
        NSTL1(NULL, NULL, "Phase '%s' is already completed for child %d", phase_name, i);
        sprintf(err_msg, "Phase '%s' is already completed.\n", phase_name);
        fprintf(stderr, "Warning: Phase '%s' is already completed.\n", phase_name);
        return -1;
      }
   
      /* Actual fill stuff here*/
      tmp_ph->phase_cmd.start_phase.time = time;
      tmp_ph->phase_cmd.start_phase.dependent_grp = dependent_grp;
      if(tmp_ph->phase_status == PHASE_RUNNING) // update phase runtime flag when this phase is running only
        tmp_ph->runtime_flag = 1;  // Marked phase as changes has been applied 
/*    
    // Parent Data Updation
    parent_tmp_ph->phase_cmd.start_phase.time = time;
    parent_tmp_ph->phase_cmd.start_phase.dependent_grp = dependent_grp;
*/
    }
    if (!strcmp(fields[1], "IMMEDIATELY")) {
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is start with mode '%s'", phase_name, fields[1]);
    }
    else if(!strcmp(fields[1], "AFTER")) {
      if (!strcmp(fields[2], "TIME"))
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is start with mode '%s' '%s' '%s'(HH:MM:SS)", phase_name, fields[1], fields[2], fields[3]);
      else if(!strcmp(fields[2], "GROUP"))
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is start with mode '%s' '%s' '%s' '%s'(HH:MM:SS)", phase_name, fields[1], fields[2], fields[3], fields[4]);
    }
  }

  return 0;
}

int parse_runtime_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int *flag, int rtc_idx, char *quantity, int runtime_id) {
  int i, phase_index;
  Phases *tmp_ph /**parent_tmp_ph*/;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char buf_backup_ex[MAX_DATA_LINE_LENGTH];
  int group_mode;
  Schedule *cur_schedule;
  char *buf, *buf_more;
  int num_fields;
  unsigned char ramp_up_mode;
  int ramp_up_step_users, ramp_up_step_time, ramp_up_time;
  double ramp_up_rate;
  char ramp_up_pattern;
  int num_vusers_or_sess;
  int tot_num_steps_for_sessions;
  s_child_ports *v_port_entry_ptr;
  char *users_or_sess_rate;
  char *ramp_up_mode_str;;
  double rpc = 0;;
  
  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);
  
  if (rtc_idx == -1)
  {
    if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
      phase_index = 1; 
    else {
      if(runtime_id == -1) {
        phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
        if(phase_index == -1) {
          sprintf(err_msg, "Phase '%s' does not exists.", phase_name);
          return -1;
        }
      }
    }
  }
  num_vusers_or_sess = ramp_up_mode = ramp_up_pattern = ramp_up_rate = ramp_up_step_time = 0;
  ramp_up_step_users = ramp_up_time = tot_num_steps_for_sessions = 0;

  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
 
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "RAMP_UP");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("RAMP_UP");
    buf_more = strstr(buf_more, "RAMP_UP");
    if (buf_more) buf = buf_more;
  }
  
  num_fields = get_tokens(buf, fields, " ", 20);
  group_mode = get_group_mode(grp_idx);

  if (num_fields < 3) {
    sprintf(err_msg, "Atleast two arguments required with RAMP_UP, in line [%s]", buf_backup);
    return -1;
    //usage_parse_schedule_phase_ramp_up(NULL);
  }


  /* Validation */
  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") &&
        strcmp(fields[2], "RATE") && strcmp(fields[2], "TIME")) {
      sprintf(err_msg, "%s", "Unknown Ramp Up mode for Fixed Concurrent Users scenarios. "
                             "It can be : IMMEDIATELY, STEP, RATE or TIME");
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
      sprintf(err_msg, "%s", "Unknown Ramp Up mode for Fixed Sessions Rate scenarios. It can be : IMMEDIATELY or TIME_SESSIONS");
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
  }

  users_or_sess_rate = fields[1];
  ramp_up_mode_str = fields[2];

  /* Users_or_sess_rate can only be ALL in Simple mode and > 0 for others */
  if ((global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users_or_sess_rate, "ALL")) {
    sprintf(err_msg, "%s", "For Simple Schedule, users_or_sess_rate can only be ALL");
    return -1;
    //usage_parse_schedule_phase_ramp_up(NULL);
  } else if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if (!strcmp(users_or_sess_rate, "ALL")) {
      sprintf(err_msg, "For Advanced Schedule, users_or_sess_rate can not be ALL");
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    } else if (atoi(users_or_sess_rate) <= 0) {
      sprintf(err_msg, "Users_or_sess_rate specified for Advanced Schedule can not be less than or equal to zero");
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
  }
  
  /* Fill mode */
  if (strcmp(ramp_up_mode_str, "IMMEDIATELY") == 0) {
    ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
  } else if (strcmp(ramp_up_mode_str, "STEP") == 0) {
    
    if (num_fields < 5) {
      sprintf(err_msg, "Atleast two arguments required with STEP, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }

    ramp_up_mode = RAMP_UP_MODE_STEP;
    ramp_up_step_users = atoi(fields[3]);
    ramp_up_step_time = get_time_from_format(fields[4]);
    if(ramp_up_step_users <= 0) {
      sprintf(err_msg, "In STEP mode, number of step users cannot be less than or equal to 0. Give correct number of users.");
      return -1;
    }
    if (ramp_up_step_time < 0) {
      sprintf(err_msg, "Time specified with STEP is invalid. Line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
  } else if (strcmp(ramp_up_mode_str, "RATE") == 0) {
 
    //In case of ramp up rate we need to call scenario distribution tool
    if (loader_opcode == MASTER_LOADER) {
      strcpy(buf_backup_ex, buf_backup); //retain buf_backup
      if(!(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)))
        init_scenario_distribution_tool(buf_backup_ex, 4, testidx, 1, 0, 0, err_msg);
      *flag = 1;
      NSTL1(NULL, NULL, "RTC: In case of scheduling call scenario_distribution_tool, flag = %d", *flag);
    } else {
      char tmp_buf[1024];
      ramp_up_mode = RAMP_UP_MODE_RATE;
  
      if (num_fields < 5) {
        sprintf(err_msg, "Atleast two arguments required with RATE, in line [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_up(NULL);
      }

      sprintf(tmp_buf, "DUMMY_KW %s %s", fields[3], fields[4]); /* Sending dummy kw */
      ramp_up_rate = convert_to_per_minute(tmp_buf); /* Decimal howto ?? */

      /* pattern */
      if (num_fields < 6)
      {
        sprintf(err_msg, "<ramp_up_pattern>  not specified. It should be LINEARLY or RANDOMLY, in line [%s]", buf_backup);
        return -1;
      }
      else if (strcmp(fields[5], "LINEARLY") == 0) {
        ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
      } else if (strcmp(fields[5], "RANDOMLY") == 0) {
        ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
      } else {                    /* Unknown pattern */
        sprintf(err_msg, "Unknown Pattern %s, in line [%s]", fields[5], buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_up(NULL);
      }
    }
  } else if (strcmp(ramp_up_mode_str, "TIME") == 0) {

    if (num_fields < 5) {
      sprintf(err_msg, "Atleast two arguments required with TIME, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }

    ramp_up_mode = RAMP_UP_MODE_TIME;
    ramp_up_time = get_time_from_format(fields[3]);

    if (ramp_up_time < 0) {
      sprintf(err_msg, "Time specified is invalid in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    } else if (ramp_up_time == 0)  // Ramp up time is 0 then do immediate
      ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;

    if (strcmp(fields[4], "LINEARLY") == 0) {
      ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
    } else if (strcmp(fields[4], "RANDOMLY") == 0) {
      ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
    } else {                    /* Unknown pattern */
      sprintf(err_msg, "Unknown Pattern %s, in line [%s]", fields[4], buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
    
  } else if (strcmp(ramp_up_mode_str, "TIME_SESSIONS") == 0) {
    
    //In case of ramp up rate we need to call scenario distribution tool
    if (loader_opcode == MASTER_LOADER) {
      strcpy(buf_backup_ex, buf_backup); //retain buf_backup
      if(!(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)))
        init_scenario_distribution_tool(buf_backup_ex, 4, testidx, 1, 0, 0, err_msg);
      *flag = 1;
      NSTL1(NULL, NULL, "RTC: In case of scheduling call scenario_distribution_tool, flag = %d", *flag);
    } else {
      if (num_fields < 5) {
        sprintf(err_msg, "Atleast two arguments required with TIME_SESSIONS, in line [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_up(NULL);
      }

      /* TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
         <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
      ramp_up_time = get_time_from_format(fields[3]);
      if (ramp_up_time < 0) {
        sprintf(err_msg, "Time specified is invalid in line [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_up(NULL);
      } else if (ramp_up_time == 0)  { // Ramp up time is 0 then do immediate
        ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
      }
/* ramp_up_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
      if (ramp_up_time > 0) {
        double temp;
        int step_mode = atoi(fields[4]);
        if (step_mode == 0) {
          tot_num_steps_for_sessions = 2;
          int floor = ramp_up_time/(1000*60); // chk it once
          if (floor > 1)
            tot_num_steps_for_sessions = floor;
          temp = (double)(ramp_up_time/1000)/(double)tot_num_steps_for_sessions;
          ramp_up_step_time = round(temp);
        }
        else if (step_mode == 1) {
          if (num_fields < 6) {
            sprintf(err_msg, "Step time or num steps should be specified, in line [%s]", buf_backup);
            return -1;
            //usage_parse_schedule_phase_ramp_up(NULL);
          }
          ramp_up_step_time = atoi(fields[5]);
          if(ramp_up_step_time <= 0){
            sprintf(err_msg, "Invalid step time '%d' at line [%s]", ramp_up_step_time, buf_backup);
            return -1;
          }          
          if((ramp_up_time/1000) < ramp_up_step_time){
            sprintf(err_msg, "Ramp Up time can not be less then step time at line [%s]", buf_backup);
            return -1;
          }          
          temp = (double)(ramp_up_time/1000)/(double)ramp_up_step_time;
          tot_num_steps_for_sessions = round(temp);
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            sprintf(err_msg, "Step time or num steps should be specified, in line [%s]", buf_backup);
 	    return -1;
            //usage_parse_schedule_phase_ramp_up(NULL);
          }
          tot_num_steps_for_sessions = atoi(fields[5]);
          if(tot_num_steps_for_sessions <= 0){
            sprintf(err_msg, "Invalid num steps '%d' at line [%s]", tot_num_steps_for_sessions, buf_backup);
            return -1;
          }          
          temp = (double)(ramp_up_time/1000)/(double)tot_num_steps_for_sessions;
          // step time can not be 0 
          ramp_up_step_time = (round(temp))?round(temp):1;
        }
        else {
          sprintf(err_msg, "Invalid step mode specified in line [%s]", buf_backup);
	  return -1;
          //exit(-1);
        }
      }    
    } 
  }

  NSDL2_RUNTIME(NULL, NULL, "users_or_sess_rate = %s, num_vusers_or_sess = %d, rtc_idx = %d, quantity = %s", 
                             users_or_sess_rate, num_vusers_or_sess, rtc_idx, quantity);

  /* Fill users_or_sess_rate */
  if (strcmp(users_or_sess_rate, "ALL") == 0) {
    num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
  } else if (group_mode == TC_FIX_USER_RATE) { /* FSR */
    num_vusers_or_sess = (int)(atof(users_or_sess_rate) * SESSION_RATE_MULTIPLE); 
  } else {
    num_vusers_or_sess = atoi(users_or_sess_rate);
  }

  if (rtc_idx != -1) {
    if (group_mode == TC_FIX_CONCURRENT_USERS)
      num_vusers_or_sess = atoi(quantity);
    else
      num_vusers_or_sess = (int)(atof(quantity) * SESSION_RATE_MULTIPLE);
  }

  int tot_users = num_vusers_or_sess;
  // its a total number if users to calculate rpc for FCU
  if (rtc_idx == -1)
  {
    int local_vusers_or_sess;
    if (group_mode == TC_FIX_CONCURRENT_USERS) {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        tot_users = group_schedule[grp_idx].phase_array[phase_index].phase_cmd.ramp_up_phase.num_vusers_or_sess;
      else
        tot_users = scenario_schedule->phase_array[phase_index].phase_cmd.ramp_up_phase.num_vusers_or_sess;
      local_vusers_or_sess = tot_users;
    }
    /* Bug 34047 where on Phase rtc, Users_or_sess_rate validations */
    else {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        local_vusers_or_sess = group_schedule[grp_idx].phase_array[phase_index].phase_cmd.ramp_up_phase.num_vusers_or_sess;
      else
        local_vusers_or_sess = scenario_schedule->phase_array[phase_index].phase_cmd.ramp_up_phase.num_vusers_or_sess;
    }
    if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
      /* on master, ramp up/down should be transferred as generator ramp phase users*/ 
      if(loader_opcode == MASTER_LOADER)
      {
        update_advance_ramp_up_down_users(grp_idx, phase_index, buf_backup, 1);
        *flag = 1;
      }
      if(num_vusers_or_sess != local_vusers_or_sess) {
        sprintf(err_msg, "For Advanced Schedule, Users_or_sess_rate cannot be changed");
        return -1;
      }
    }
  }

  NSDL2_RUNTIME(NULL, NULL, "Phase '%s' tot_users = %d, phase_idx = %d", phase_name, tot_users, phase_index);

  if(loader_opcode != MASTER_LOADER) 
  {
    // fill for all childs
    for(i = 0; i < global_settings->num_process; i++) {
      /* Start phase will always be there. */
      v_port_entry_ptr = &v_port_table[i];
      void *shr_mem_ptr = v_port_entry_ptr->runtime_schedule;
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        if (rtc_idx != -1) /*here RTC and phase index is same*/
        { 
          cur_schedule = shr_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
          cur_schedule->type = 1; //Runtime schedule
          cur_schedule->phase_idx = runtime_schedule[rtc_idx].phase_idx; //Runtime phase index
          cur_schedule->rtc_idx = rtc_idx;
          cur_schedule->rtc_id = runtime_id;
          cur_schedule->group_idx = grp_idx; //Runtime phase index
          tmp_ph = &cur_schedule->phase_array[cur_schedule->phase_idx];
          tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = num_vusers_or_sess;  
          strncpy(runtime_schedule[rtc_idx].phase_array[runtime_schedule[rtc_idx].phase_idx].phase_name, phase_name, PHASE_NAME_SIZE);  
          strncpy(tmp_ph->phase_name, phase_name, PHASE_NAME_SIZE);  
          NSDL2_RUNTIME(NULL, NULL, "Phase filled at index = %d, tmp_ph =%p", rtc_idx, tmp_ph);
        } else {
          cur_schedule = v_port_entry_ptr->scenario_schedule;
          tmp_ph = &(cur_schedule->phase_array[phase_index]);
          //parent_tmp_ph = &(scenario_schedule->phase_array[phase_idx]);
        }
      } else {
        if (rtc_idx != -1) { 
          cur_schedule = shr_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
          cur_schedule->type = 1; //Runtime schedule
          cur_schedule->phase_idx = runtime_schedule[rtc_idx].phase_idx; //Runtime phase index
          cur_schedule->rtc_idx = rtc_idx;
          cur_schedule->rtc_id = runtime_id;
          cur_schedule->group_idx = grp_idx; //Runtime phase index
          tmp_ph = &cur_schedule->phase_array[cur_schedule->phase_idx];
          tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = num_vusers_or_sess;  
          strncpy(runtime_schedule[rtc_idx].phase_array[runtime_schedule[rtc_idx].phase_idx].phase_name, phase_name, PHASE_NAME_SIZE);  
          strncpy(tmp_ph->phase_name, phase_name, PHASE_NAME_SIZE);  
          NSDL2_RUNTIME(NULL, NULL, "Phase filled at index = %d, tmp_ph =%p", rtc_idx, tmp_ph);
        } else {
          cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
          tmp_ph = &(cur_schedule->phase_array[phase_index]);
        //parent_tmp_ph = &(group_schedule[grp_idx].phase_array[phase_index]);
        }
      }
    
      //tmp_ph->default_phase = 0;
      if (rtc_idx == -1) { 
        if(tmp_ph->phase_status == PHASE_IS_COMPLETED) {
          NSTL1(NULL,NULL, "Phase '%s' is already completed for child %d", phase_name, i);
          sprintf(err_msg, "Phase '%s' is already completed.\n", phase_name);
          fprintf(stderr, "Warning: Phase '%s' is already completed.\n", phase_name);
          return -1;
        }
      }
      if(group_mode == TC_FIX_CONCURRENT_USERS) {
        if(ramp_up_mode == RAMP_UP_MODE_RATE) {
          if (tot_users <= 0) {
            rpc = 0;
          } else  {
            rpc = ((((double) ramp_up_rate * (double) (tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess - tmp_ph->phase_cmd.ramp_up_phase.ramped_up_vusers)) / (double) tot_users));
          }
        } else if( ramp_up_mode == RAMP_UP_MODE_TIME) {
          /*BugFix#13128: If we applied Phase RTC like: The time for ramp up phase was increased from 00:01:30 to 00:02:00.
                          Means, the phase should complete in 2 mins. But, the ramp up phase is taking less time to complete. 
                          The phase is getting complete in 1 mins 44 secs rather it should have taken 2 mins to complete.
          */
          u_ns_ts_t now;
          now = get_ms_stamp();
          int time_val = ramp_up_time;
          NSDL4_RUNTIME(NULL, NULL, "time val = %u, current phase = %d, phase_status = %d, phase_name = %s", time_val, cur_schedule->phase_array[cur_schedule->phase_idx].phase_type, cur_schedule->phase_array[cur_schedule->phase_idx].phase_status, cur_schedule->phase_array[cur_schedule->phase_idx].phase_name);
          if((cur_schedule->phase_array[cur_schedule->phase_idx].phase_status == PHASE_RUNNING) && 
            !(strcmp(cur_schedule->phase_array[cur_schedule->phase_idx].phase_name, phase_name))) {
            time_val = time_val - (now - tmp_ph->phase_start_time);
            NSDL4_RUNTIME(NULL, NULL, "time_val = %u, now = %llu, phase_start_time = %llu, ramp_up_time = %d", time_val, now, tmp_ph->phase_start_time, ramp_up_time);
            //Check should only for scheduling RTC
            if((rtc_idx == -1) && (time_val < 0)) {
              sprintf(err_msg, "Phase '%s' time provided is less than executed ramp time for child '%d'", phase_name, i);
              return -1;
            }
            rpc = (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess - tmp_ph->phase_cmd.ramp_up_phase.ramped_up_vusers) / (double)(time_val/(1000.0*60)); 
          } else {
            time_val = ramp_up_time;
            rpc = (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess) / (double)(time_val/(1000.0*60));
          }
        }
      }

      NSDL2_RUNTIME(NULL, NULL, "Phase name = %s, ramp_up_mode = %d, tot_users = %d, "
                                "num_vusers_or_sess = %d, rpc = %lf, ramped_up_vusers = %d", 
                                 phase_name, ramp_up_mode, tot_users, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, 
                                 rpc, tmp_ph->phase_cmd.ramp_up_phase.ramped_up_vusers);

      /* Actual fill stuff here*/
      //tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess         = num_vusers_or_sess;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode               = ramp_up_mode;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern            = ramp_up_pattern; 
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate               = ramp_up_rate;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time          = ramp_up_step_time;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users         = ramp_up_step_users; 
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time               = ramp_up_time;
      tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = tot_num_steps_for_sessions;
      tmp_ph->phase_cmd.ramp_up_phase.rpc                        = rpc;
      if (rtc_idx != -1)
        tmp_ph->runtime_flag = 1;
      else
        if(tmp_ph->phase_status == PHASE_RUNNING) // update phase runtime flag when this phase is running only
          tmp_ph->runtime_flag = 1;  // Marked phase as changes has been applied

      NSDL2_RUNTIME(NULL, NULL, "Runtime changes are updated for phase name = %s, ramp_up_rate =%d, rpc=%d", 
                                 phase_name, tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate, tmp_ph->phase_cmd.ramp_up_phase.rpc);
/*
    // Parent updation
    //parent_tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess         = num_vusers_or_sess;
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode               = ramp_up_mode;
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern            = ramp_up_pattern; 
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate               = ramp_up_rate;
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time          = ramp_up_step_time;
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users         = ramp_up_step_users; 
    parent_tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time               = ramp_up_time;
    parent_tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = tot_num_steps_for_sessions;
*/
    }

    if (!strcmp(ramp_up_mode_str, "IMMEDIATELY"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s'", phase_name, fields[2]);
    else if (!strcmp(ramp_up_mode_str, "STEP")) 
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' step users '%s' '%s'(HH:MM:SS)", phase_name, fields[2], fields[3], fields[4]);
    else if (!strcmp(ramp_up_mode_str, "TIME"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' '%s'(HH:MM:SS) pattern '%s'", phase_name, fields[2], fields[3], fields[4]);
    else if (!strcmp(ramp_up_mode_str, "RATE"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' '%s' users per '%s' pattern '%s'", phase_name, fields[2], fields[3], fields[4], fields[5]);
    else if (!strcmp(ramp_up_mode_str, "TIME_SESSIONS")) {
      if(atoi(fields[4]) == 1)
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' '%s'(HH:MM:SS) step mode '%s' step time '%s'", phase_name, fields[2], fields[3], fields[4], fields[5]);
      else if(atoi(fields[4]) == 2)
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' '%s'(HH:MM:SS) step mode '%s' steps '%s'", phase_name, fields[2], fields[3], fields[4], fields[5]);
      else
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampup with mode '%s' '%s'(HH:MM:SS) step mode '%s'", phase_name, fields[2], fields[3], fields[4]);
    }    
  }
  return 0;
}

int parse_runtime_schedule_phase_stabilize(int grp_idx, char *full_buf, char *phase_name, char *err_msg) {
  int i, phase_index;
  Phases *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  Schedule *cur_schedule;
  char *buf;
  char *buf_more;
  int num_fields;
  u_ns_ts_t time;
  s_child_ports *v_port_entry_ptr;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

  if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
    phase_index = 2; 
  else {
    phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
    if(phase_index == -1) {
      sprintf(err_msg, "Phase '%s' does not exists in '%s'", phase_name, full_buf);
      return -1;
    }
  }
  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "STABILIZATION");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("STABILIZATION");
    buf_more = strstr(buf_more, "STABILIZATION");
    if (buf_more) buf = buf_more;
  }

  num_fields = get_tokens(buf, fields, " ", 20);

  if (num_fields < 3) {
    sprintf(err_msg, "Atleast two fields reqiured with STABILIZATION, in line [%s]", buf_backup); 
    return -1;
      //usage_parse_schedule_phase_stabilize(NULL);
  }

  if (strcmp(fields[1], "TIME") == 0) {
    time = get_time_from_format(fields[2]);
    if (tmp_ph->phase_cmd.stabilize_phase.time < 0) {
      sprintf(err_msg, "Invalid time specified with TIME, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_stabilize(NULL);
    }
  } else {
    sprintf(err_msg, "Invalid format for STABILIZATION (%s)", buf_backup);
    return -1;
  }

  /*NC: In case of controller we need to by-pass the code to update nvm structure*/
  if(loader_opcode != MASTER_LOADER) 
  {
    // fill for all childs
    for(i = 0; i < global_settings->num_process; i++) {
      /* Start phase will always be there. */
      v_port_entry_ptr = &v_port_table[i];
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        cur_schedule = v_port_entry_ptr->scenario_schedule;
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
      } else {
        cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
      }
  
      //tmp_ph->default_phase = 0;

      if(tmp_ph->phase_status == PHASE_IS_COMPLETED) {
        NSTL1(NULL,NULL, "Phase '%s' is already completed for child %d", phase_name, i);
	sprintf(err_msg, "Phase '%s' is already completed.\n", phase_name);
        fprintf(stderr, "Warning: Phase '%s' is already completed.\n", phase_name);
        return -1;
      }
   
      /* Actual fill stuff here*/
      tmp_ph->phase_cmd.stabilize_phase.time = time;
      if(tmp_ph->phase_status == PHASE_RUNNING) // update phase runtime flag when this phase is running only 
        tmp_ph->runtime_flag = 1;  // Marked phase as changes has been applied 
    }
    
    sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is stabilization with mode '%s' '%s'(HH:MM:SS)",
                      phase_name, fields[1], fields[2]);
  }
  return 0;
}

inline void get_total_num_users_and_num_fetches(char *nvm_ids, int grp_idx, int *total_users, int *total_fetches)
{
  // Initialising to default value 
  *total_users = 0;
  *total_fetches = 0;

  // Case when Group = ALL
  if(grp_idx == -1)
  {
    int i;
    for (i = 0; i < global_settings->num_process; i++){
       //Skip if nvm is not active and active nvm does not have users.
       if(!(nvm_ids[i] && v_port_table[i].num_vusers)){
          NSDL2_SCHEDULE(NULL, NULL, "skipping nvm as nvm_ids[%d] = %d, v_port_table[%d].num_vusers tal_users = %d",
                                              i, nvm_ids[i], i, v_port_table[i].num_vusers);
          continue;
      }
      *total_users += v_port_table[i].num_vusers;
      *total_fetches += v_port_table[i].num_fetches; 
      NSDL2_SCHEDULE(NULL, NULL, "nvm_id[%d].num_vusers = %d, nvm_id[%d].num_fetches = %d, total_users = %d, total_fetches = %d",
                                              i, v_port_table[i].num_vusers, i, v_port_table[i].num_fetches, *total_users, *total_fetches);
    }
  }
  else // In Schedule Duration, currently we are not handling group based scenario.
  {
     //TODO
  }  
}


// This is a common method used during session increment/decrement through QUANTITY/SCHEDULE keyword
int update_runtime_sessions(char mode, int num_fetches, int grp_idx, char *err_msg)
{
   int ret = RUNTIME_SUCCESS;
   char nvm_ids[MAX_NVM_NUM] = {0};
   int nvm_count = 0;

   NSDL4_RUNTIME(NULL, NULL, "Method called mode = %d, num_fetches = %d, grp_idx = %d", mode, num_fetches, grp_idx);

   if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
   {
      int i;
      for(i = 0; i < global_settings->num_process; i++)
      {
        if(is_process_still_active(grp_idx, i))
        {
           nvm_ids[i] = 1;
        }
      }
      if((mode == SESSION_SCHEDULE_RTC) || (mode == PER_USER_SESSION_SCHEDULE_RTC))
      {
       // Get total number of users and sessions
        int total_users;
        int total_fetches;
        get_total_num_users_and_num_fetches(nvm_ids, grp_idx, &total_users, &total_fetches);
        
        if(mode == PER_USER_SESSION_SCHEDULE_RTC)
        { 
          num_fetches *= total_users; 
        }
        global_settings->num_fetches = num_fetches; 
        num_fetches -= total_fetches;

        NSDL4_RUNTIME(NULL, NULL, " num_fetches = %d, total_fetches = %d, total_users = %d", num_fetches, total_fetches, total_users);
      }
      else if(mode == USER_SESSION_RTC)
      {
        if(scenario_schedule->phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
          num_fetches *= scenario_schedule->phase_array[3].phase_cmd.duration_phase.num_fetches;
      }
      ret = runtime_distribute_fetches(nvm_ids, num_fetches, err_msg);
   }
   else
   {
      runtime_get_process_cnt_serving_grp(nvm_ids, &nvm_count, grp_idx);
      NSDL2_RUNTIME(NULL, NULL, "nvm_count = %d", nvm_count);

      if(nvm_count <= 0)
      {
        NSDL2_RUNTIME(NULL, NULL, "Cannot apply runtime changes to group=%s as NVM's running this group are over OR group is over",
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        sprintf(err_msg, "Cannot apply runtime changes to group=%s as NVM's running this group are over OR group is over",
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        return RUNTIME_ERROR;
      }
      if(mode==USER_SESSION_RTC)
      {
        if(group_schedule[grp_idx].phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
          num_fetches *= group_schedule[grp_idx].phase_array[3].phase_cmd.duration_phase.num_fetches;
      }
      ret = runtime_distribute_fetchs_over_nvms_and_grps(nvm_ids, num_fetches, nvm_count, grp_idx, err_msg);
   }
    return ret;
}

int parse_runtime_schedule_phase_duration(int grp_idx, char *full_buf, char *phase_name, char *err_msg) {
  int i, phase_index;
  Phases *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  Schedule *cur_schedule;
  char *buf;
  char *buf_more;
  int num_fields;
  s_child_ports *v_port_entry_ptr;
  int duration_mode;            /* 0 == Indefinite, 1 = Time, 2 = Sessions */
  int seconds;
  int num_fetches = 0;
  int per_user_session = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

  if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
    phase_index = 3; 
  else {
    phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
    if(phase_index == -1) {
      sprintf(err_msg, "Phase '%s' does not exists in '%s'", phase_name, full_buf);
      return -1;
    }
  }
  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }

  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "DURATION");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("DURATION");
    buf_more = strstr(buf_more, "DURATION");
    if (buf_more) buf = buf_more;
  }
  
  num_fields = get_tokens(buf, fields, " ", 20);

  if (num_fields < 2) {
    sprintf(err_msg, "Atleast one field required with DURATION, in line [%s]", buf_backup);
    return -1;
    //usage_parse_schedule_phase_duration(NULL);
  }

  if (!strcmp(fields[1], "SESSIONS")) {
    if (global_settings->schedule_type != SCHEDULE_TYPE_SIMPLE ||
        global_settings->schedule_by   != SCHEDULE_BY_SCENARIO) {
      sprintf(err_msg, "SESSIONS (fetches) are only applicable with simple and scenario based schedule.");
      return -1;
      //exit(-1);
    }

    if (get_group_mode(grp_idx) == TC_FIX_USER_RATE) {
      sprintf(err_msg, "SESSIONS (fetches) are not allowed in fixed session rate scenarios.");
      return -1;
      //exit(-1);
    }
  }

  if (strcmp(fields[1], "INDEFINITE") == 0) {
    duration_mode = DURATION_MODE_INDEFINITE;
  } else if (strcmp(fields[1], "TIME") == 0) {
    if (num_fields < 3) {
      sprintf(err_msg, "Time in format HH:MM:SS must be specified with TIME, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_duration(NULL);
    }

    duration_mode = DURATION_MODE_TIME;
    seconds = get_time_from_format(fields[2]);
    NSDL3_RUNTIME(NULL, NULL, "seconds = %d\n", seconds);
    if (seconds < 0) {
      sprintf(err_msg, "Invalid time specified in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_duration(NULL);
    }
  } else if (strcmp(fields[1], "SESSIONS") == 0) {

    if (num_fields < 3) {
      sprintf(err_msg, "Sessions must be specified with SESSIONS, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_duration(NULL);
    }

    // Get number of SESSIONS to be incremented or decremented
    num_fetches = atoi(fields[2]);

    // Get per user session flag
    per_user_session = atoi(fields[3]);

    NSDL2_RUNTIME(NULL, NULL, "per_user_session = %d, num_fetches = %d", per_user_session,num_fetches);

    if(num_fetches < 0)
    {
       sprintf(err_msg, "Number of users/sessions to cannot be < 0. Sessions given = %d", num_fetches);
       return RUNTIME_ERROR;
    }

   if (loader_opcode != MASTER_LOADER) {
      // PER_USER_SESSION_SCHEDULE_RTC = total_users * sessions to be updated 
      if(update_runtime_sessions(per_user_session?PER_USER_SESSION_SCHEDULE_RTC:SESSION_SCHEDULE_RTC, num_fetches, grp_idx, err_msg) == RUNTIME_ERROR)
         return RUNTIME_ERROR;
      }
    
  } else {
    sprintf(err_msg, "Invalid format for DURATION, in line [%s]", buf_backup);
    return -1;
  }

  /*NC: In case of controller we need to by-pass the code to update nvm structure*/
  if (loader_opcode != MASTER_LOADER) {
    // fill for all childs
    for(i = 0; i < global_settings->num_process; i++) {
      /* Start phase will always be there. */
      v_port_entry_ptr = &v_port_table[i];
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        cur_schedule = v_port_entry_ptr->scenario_schedule;
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
      } else {
        cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
        tmp_ph = &(cur_schedule->phase_array[phase_index]);
      }  
  
      if(cur_schedule->cur_vusers_or_sess && (tmp_ph->phase_status == PHASE_IS_COMPLETED)) {
        NSTL1(NULL, NULL, "Phase '%s' is already completed for child %d", phase_name, i);
        sprintf(err_msg, "Phase '%s' is already completed.\n", phase_name);
        fprintf(stderr, "Warning: Phase '%s' is already completed.\n", phase_name);
        return -1;
      }
   
      //We are not allowing to change run time settings from DURATION SESSIONS to DURATION TIME.
      if((tmp_ph->phase_cmd.duration_phase.duration_mode == DURATION_MODE_SESSION) && (duration_mode == DURATION_MODE_TIME)){ 
        sprintf(err_msg, "For 'Duration' scheduling runtime changes is not allowed from SESSIONS to TIME");
        return -1;
      }
      /* Actual fill stuff here*/
      tmp_ph->phase_cmd.duration_phase.duration_mode = duration_mode;
      tmp_ph->phase_cmd.duration_phase.seconds       = seconds;
      if(tmp_ph->phase_status == PHASE_RUNNING) // update phase runtime flag when this phase is running only
        tmp_ph->runtime_flag = 1;  // Marked phase as changes has been applied
      tmp_ph->phase_cmd.duration_phase.num_fetches = num_fetches;
      tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag = per_user_session;   
    }
    if (!strcmp(fields[1], "INDEFINITE"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is duration with mode '%s'.", phase_name, fields[1]);
    else if (!strcmp(fields[1], "TIME"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is duration with mode '%s' '%s'(HH:MM:SS).", phase_name, fields[1], fields[2]);
  }
  return 0;
}

int parse_runtime_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int *flag, int rtc_idx, char *quantity, int runtime_id) {
  int i, phase_index;			
  Phases *tmp_ph;
  char *fields[20];
  char *buf;
  char *buf_more;
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char buf_backup_ex[MAX_DATA_LINE_LENGTH];
  int group_mode;
  Schedule *cur_schedule;
  int num_fields;
  char *users;
  char *mode;
  short ramp_down_mode;
  int ramp_down_pattern;
  int ramp_down_time;
  int tot_num_steps_for_sessions;
  int ramp_down_step_time;
  int num_vusers_or_sess;
  int ramp_down_step_users; 
  s_child_ports *v_port_entry_ptr;
  double rpc;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s, rtc_idx = %d, runtime_id = %d", 
                             grp_idx, full_buf, phase_name, *flag, runtime_id);
  if (rtc_idx == -1)
  {
    if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
      phase_index = 4; 
    else {
      if(runtime_id == -1) {
        phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
        if(phase_index == -1) {
          sprintf(err_msg, "Phase '%s' does not exists in '%s'", phase_name, full_buf);
          return -1;
        }
      }
    }
  }
  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "RAMP_DOWN");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("RAMP_DOWN");
    buf_more = strstr(buf_more, "RAMP_DOWN");
    if (buf_more) buf = buf_more;
  }

  num_fields = get_tokens(buf, fields, " ", 20);
  
  group_mode = get_group_mode(grp_idx);

  //tmp_ph->default_phase = 0;

  /* Validation */
  if (num_fields < 3) {
    sprintf(err_msg, "Atleast two field required after RAMP_DOWN, in line [%s]", buf_backup);
    return -1;
    //usage_parse_schedule_phase_ramp_down(NULL);
  }


  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") && strcmp(fields[2], "TIME")) {
      sprintf(err_msg, "%s", "Unknown Ramp Down mode. It can be : IMMEDIATELY, STEP or TIME");
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
     if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
       sprintf(err_msg, "%s", "Unknown Ramp Down mode for Fixed Sessions Rate scenarios. It can be : IMMEDIATELY or TIME_SESSIONS");
       return -1;
       //usage_parse_schedule_phase_ramp_up(NULL);
     }
  }

  /* RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args> */
  users = fields[1];
  mode = fields[2];
  
  /* Users can only be ALL in Simple mode */
  if ((global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users, "ALL")) {
    sprintf(err_msg, "%s", "For Simple Scenario Types, Ramp Down users can only be ALL");
    return -1;
    //usage_parse_schedule_phase_ramp_down(NULL);
  } /* All for Advanced should work */
/*  else if ((global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) && */
/*              !strcmp(users, "ALL")) { */
/*     NSTL1_OUT(NULL, NULL, "For Advanced Scenario Types, Ramp Down users can not be ALL\n"); */
/*       usage_parse_schedule_phase_ramp_down(NULL); */
/*   } */

  /* Fill mode */
  if (strcmp(mode, "IMMEDIATELY") == 0) {
    ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;

  } else if (strcmp(mode, "STEP") == 0) {
    if (num_fields < 4) {
      sprintf(err_msg, "Atleast one argument required with STEP, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }

    ramp_down_mode = RAMP_DOWN_MODE_STEP;
    ramp_down_step_users = atoi(fields[3]);
    if (ramp_down_step_users <= 0) {
      sprintf(err_msg, "In STEP mode Users cannot be less than or equal to zero. So, passed correct Users.., in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }
    ramp_down_step_time = get_time_from_format(fields[4]);
    if (ramp_down_step_time < 0) {
      sprintf(err_msg, "Invalid time with STEPS, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }

  } else if (strcmp(mode, "TIME") == 0) {
    if (num_fields < 5) {
      sprintf(err_msg, "Atleast Two argument required after TIME, in line [%s]", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }

    ramp_down_mode = RAMP_DOWN_MODE_TIME;
    ramp_down_time = get_time_from_format(fields[3]);
    if (ramp_down_time < 0) {
      sprintf(err_msg, "Invalid time specified in line %s", buf_backup);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }else if (ramp_down_time == 0) {
     ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    }

    if (strcmp(fields[4], "LINEARLY") == 0) {
      ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;
    } else if (strcmp(fields[4], "RANDOMLY") == 0) {
      ramp_down_pattern = RAMP_DOWN_PATTERN_RANDOM;
    } else {                    /* Unknown pattern */
      sprintf(err_msg, "Unknown Pattern %s, in RAMP_DOWN", fields[4]);
      return -1;
      //usage_parse_schedule_phase_ramp_down(NULL);
    }
  } else if (strcmp(mode, "TIME_SESSIONS") == 0) {
    //In case of ramp up rate we need to call scenario distribution tool
    if (loader_opcode == MASTER_LOADER) {
      strcpy(buf_backup_ex, buf_backup); //retain buf_backup
      if(!(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)))
        init_scenario_distribution_tool(buf_backup_ex, 4, testidx, 1, 0, 0, err_msg);
      *flag = 1;
      NSTL1(NULL, NULL, "RTC: In case of scheduling call scenario_distribution_tool, flag = %d", *flag);
    } else {
      if (num_fields < 5) {
        sprintf(err_msg, "Atleast Two argument required after TIME, in line [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_down(NULL);
      }

    /* TIME_SESSIONS <ramp_down_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
      ramp_down_time = get_time_from_format(fields[3]);
      if (ramp_down_time < 0) {
        sprintf(err_msg, "Time specified is invalid in line [%s]", buf_backup);
        return -1;
        //usage_parse_schedule_phase_ramp_down(NULL);
      }else if (ramp_down_time == 0) {
       ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
      }

/* ramp_down_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
      if (ramp_down_time > 0) {
        double temp;
        int step_mode = atoi(fields[4]);
        if (step_mode == 0) {
          tot_num_steps_for_sessions = 2;
          int floor = ramp_down_time/(1000*60); // chk it once
          if (floor > 1)
            tot_num_steps_for_sessions = floor;
          temp = (double)(ramp_down_time/1000)/(double)tot_num_steps_for_sessions;
          ramp_down_step_time = round(temp);
        }
        else if (step_mode == 1) {
          if (num_fields < 6) {
            sprintf(err_msg, "Step time or num steps should be specified, in line [%s]", buf_backup);
            return -1;
            //usage_parse_schedule_phase_ramp_down(NULL);
          }
          ramp_down_step_time = atoi(fields[5]);
          if(ramp_down_step_time <= 0){
            sprintf(err_msg, "Invalid step time '%d' at line [%s]", ramp_down_step_time, buf_backup);
            return -1;
          }          
          if((ramp_down_time/1000) < ramp_down_step_time){
            sprintf(err_msg, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
            return -1;
          }          
          temp = (double)(ramp_down_time/1000)/(double)ramp_down_step_time;
          tot_num_steps_for_sessions = round(temp);
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            sprintf(err_msg, "Step time or num steps should be specified, in line [%s]", buf_backup);
            return -1;
            //usage_parse_schedule_phase_ramp_down(NULL);
          }
          tot_num_steps_for_sessions = atoi(fields[5]);
          if(tot_num_steps_for_sessions <= 0){
            sprintf(err_msg, "Invalid num steps '%d' at line [%s]", tot_num_steps_for_sessions, buf_backup);
            return -1;
          }          
          temp = (double)(ramp_down_time/1000)/(double)tot_num_steps_for_sessions;
          // step time can not be 0 
          ramp_down_step_time = (round(temp))?round(temp):1; 
        }
        else {
          sprintf(err_msg, "Invalid step mode specified in line [%s]", buf_backup);
	  return -1;
        }
      }
    }
  }
  /* Fill users */
  if (strcmp(users, "ALL") == 0) {
    num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
  } else if (group_mode == TC_FIX_USER_RATE) { /* FSR */
    num_vusers_or_sess = (int)(atof(users) * SESSION_RATE_MULTIPLE); 
  } else {
    num_vusers_or_sess = atoi(users);
    if (num_vusers_or_sess < 0) {
      sprintf(err_msg, "Quantity can not be less than zero, in line [%s]", buf_backup);
      return -1;
    }
  }

  if (rtc_idx != -1) {
    if (group_mode == TC_FIX_CONCURRENT_USERS)
      num_vusers_or_sess = atoi(quantity);
    else
      num_vusers_or_sess = (int)(atof(quantity) * SESSION_RATE_MULTIPLE);
  }
  /* Bug 34047 where on Phase rtc, Users_or_sess_rate validations */
  else
  {
    int local_vusers_or_sess;
    if (group_mode == TC_FIX_CONCURRENT_USERS) {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        local_vusers_or_sess = group_schedule[grp_idx].phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
      else
        local_vusers_or_sess = scenario_schedule->phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
    }
    else {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        local_vusers_or_sess = group_schedule[grp_idx].phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
      else
        local_vusers_or_sess = scenario_schedule->phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
    }
    if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
      /* In NC, ramp up/down should be transferred as generator ramp phase users*/
      if(loader_opcode == MASTER_LOADER)
      {
        update_advance_ramp_up_down_users(grp_idx, phase_index, buf_backup, 0);
        *flag = 1;
      }
      if((num_vusers_or_sess != -1) && num_vusers_or_sess != local_vusers_or_sess) {
        sprintf(err_msg, "For Advanced Schedule, Users_or_sess_rate cannot be changed");
        return -1;
      }
    }
  }
  /*int tot_users = num_vusers_or_sess;
  // its a total number if users to calculate rpc for FCU
  if (rtc_idx == -1) {
    if (group_mode == TC_FIX_CONCURRENT_USERS) {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        tot_users = group_schedule[grp_idx].phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
      else
        tot_users = scenario_schedule->phase_array[phase_index].phase_cmd.ramp_down_phase.num_vusers_or_sess;
    }  
  }*/
  /*NC: In case of controller we need to by-pass the code to update nvm structure*/
  if (loader_opcode != MASTER_LOADER) {
    for(i = 0; i < global_settings->num_process; i++) {
      /* Start phase will always be there. */
      v_port_entry_ptr = &v_port_table[i];
      void *shr_mem_ptr = v_port_entry_ptr->runtime_schedule;
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        if (rtc_idx != -1) /*here RTC and phase index is same*/
        { 
          cur_schedule = shr_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
          cur_schedule->type = 1; //Runtime schedule
          cur_schedule->phase_idx = runtime_schedule[rtc_idx].phase_idx; //Runtime phase index
          cur_schedule->group_idx = grp_idx; //Runtime phase index
          cur_schedule->rtc_idx = rtc_idx; //Runtime phase index
          cur_schedule->rtc_id = runtime_id; //Runtime phase index
          tmp_ph = &cur_schedule->phase_array[cur_schedule->phase_idx];
          tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = num_vusers_or_sess;  
          strcpy(runtime_schedule[rtc_idx].phase_array[runtime_schedule[rtc_idx].phase_idx].phase_name, phase_name);  
          strcpy(tmp_ph->phase_name, phase_name);
          NSDL2_RUNTIME(NULL, NULL, "Phase filled at index = %d, tmp_ph =%p", rtc_idx, tmp_ph);
        } else {
          cur_schedule = v_port_entry_ptr->scenario_schedule;
          tmp_ph = &(cur_schedule->phase_array[phase_index]);
        }
      } else {
        if (rtc_idx != -1) { 
          cur_schedule = shr_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
          cur_schedule->type = 1; //Runtime schedule
          cur_schedule->phase_idx = runtime_schedule[rtc_idx].phase_idx; //Runtime phase index
          cur_schedule->group_idx = grp_idx; //Runtime phase index
          cur_schedule->rtc_idx = rtc_idx; //Runtime phase index
          cur_schedule->rtc_id = runtime_id; //Runtime phase index
          tmp_ph = &cur_schedule->phase_array[cur_schedule->phase_idx];
          tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = num_vusers_or_sess;  
          strcpy(runtime_schedule[rtc_idx].phase_array[runtime_schedule[rtc_idx].phase_idx].phase_name, phase_name);
          strcpy(tmp_ph->phase_name, phase_name);
          NSDL2_RUNTIME(NULL, NULL, "Phase filled at index = %d, tmp_ph =%p", rtc_idx, tmp_ph);
        } else {
          cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
          tmp_ph = &(cur_schedule->phase_array[phase_index]);
        }
      }
  
      if (rtc_idx == -1) { 
        if(tmp_ph->phase_status == PHASE_IS_COMPLETED) {
          NSTL1(NULL, NULL, "Phase '%s' is already completed for child %d", phase_name, i);
          sprintf(err_msg, "Phase '%s' is already completed.\n", phase_name);
          fprintf(stderr, "Warning: Phase '%s' is already completed.\n", phase_name);
          return -1;
        }
      }

      if(group_mode == TC_FIX_CONCURRENT_USERS) {
        if(ramp_down_mode == RAMP_DOWN_MODE_TIME) {
          /*BugFix#13128: If we applied Phase RTC like: The time for ramp up phase was increased from 00:01:30 to 00:02:00.
                          Means, the phase should complete in 2 mins. But, the ramp up phase is taking less time to complete. 
                          The phase is getting complete in 1 mins 44 secs rather it should have taken 2 mins to complete.
          */
          u_ns_ts_t now = 0;
          now = get_ms_stamp();
          int time_val = ramp_down_time;
          NSDL4_RUNTIME(NULL, NULL, "time_val = %d, now = %u, phase_status = %d, phase_name = %s", time_val, now,
                        cur_schedule->phase_array[cur_schedule->phase_idx].phase_status, cur_schedule->phase_array[cur_schedule->phase_idx].phase_name);
          if((cur_schedule->phase_array[cur_schedule->phase_idx].phase_status == PHASE_RUNNING) && 
            !(strcmp(cur_schedule->phase_array[cur_schedule->phase_idx].phase_name, phase_name))) {
            time_val = time_val - (now - tmp_ph->phase_start_time);
            //Check should only for scheduling RTC
            if((rtc_idx == -1) && (time_val < 0)) {
              sprintf(err_msg, "Phase '%s' time provided is less than executed ramp time for child '%d'", phase_name, i);
              return -1;
            }
            rpc = (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess - tmp_ph->phase_cmd.ramp_down_phase.max_ramp_down_vuser_or_sess) /(double)(time_val/(1000.0*60));
          } else {
              time_val = ramp_down_time;
              rpc = (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess) / (double)(time_val/(1000.0*60)); 
          }
          NSDL2_RUNTIME(NULL, NULL, "time val = %u, num_vusers_or_sess = %d, max_ramp_down_vuser_or_sess = %d", time_val,
                        tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess, tmp_ph->phase_cmd.ramp_down_phase.max_ramp_down_vuser_or_sess);
        }
      }
      // currently we are not allowing to update users at run time
      //tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess         = num_vusers_or_sess;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode             = ramp_down_mode;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern          = ramp_down_pattern;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users       = ramp_down_step_users;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time             = ramp_down_time;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time        = ramp_down_step_time;
      tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = tot_num_steps_for_sessions;
      tmp_ph->phase_cmd.ramp_down_phase.rpc                        = rpc;
      if (rtc_idx != -1)
        tmp_ph->runtime_flag = 1;
      else
        if(tmp_ph->phase_status == PHASE_RUNNING) // update phase runtime flag when this phase is running only
          tmp_ph->runtime_flag = 1;  // Marked phase as changes has been applied
      NSDL2_RUNTIME(NULL, NULL, "For Child: %d, Runtime changes are updated for phase name = %s", i, phase_name);
      //In case of 
      if ((global_settings->runtime_decrease_quantity_mode != 0) && (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode != RAMP_DOWN_MODE_IMMEDIATE))
      {
        sprintf(err_msg, "Phase Settings Mode 1 and 2 are not applicable for quantity decrease mode is other than immediate");
        return -1;     
      }    
    }
    if (!strcmp(mode, "IMMEDIATELY"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s'", phase_name, fields[2]);
    else if (!strcmp(mode, "STEP"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s' step users '%s' '%s'(HH:MM:SS).", phase_name, fields[2], fields[3], fields[4]);
    else if (!strcmp(mode, "TIME"))
      sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s' '%s'(HH:MM:SS) pattern '%s'.", phase_name, fields[2], fields[3], fields[4]);
    else if (!strcmp(mode, "TIME_SESSIONS")){
    if(atoi(fields[4]) == 1)
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s' '%s'(HH:MM:SS) step mode %s step time '%s'.",
                         phase_name, fields[2], fields[3], fields[4], fields[5]);
    else if(atoi(fields[4]) == 2)
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s' '%s'(HH:MM:SS) step mode '%s' steps '%s'.",
                         phase_name, fields[2], fields[3], fields[4], fields[5]);
      else
        sprintf(err_msg, "Runtime changes applied to phase '%s'. New setting is rampdown with mode '%s' '%s'(HH:MM:SS) step mode '%s'.", phase_name, fields[2], fields[3], fields[4]);
    }
  }
  return 0;
}

int kw_set_runtime_schedule(char *buf, char *err_msg, int *flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  char phase_name[MAX_DATA_LINE_LENGTH];
  char phase_type[MAX_DATA_LINE_LENGTH];
  int grp_idx;
  int num, ret;
  
  NSDL2_RUNTIME(NULL, NULL, "buf = %s\n", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, grp, phase_name, phase_type);
  if (num < 4) {
    strcpy(err_msg, "Invalid SCHEDULE entry\nSyntax: SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>\n");
    return -1;
    //usage_kw_set_schedule(NULL);
  }

  grp_idx = find_sg_idx_shr(grp);

  NSDL2_RUNTIME(NULL, NULL, "Group id = %d group quantity = %d", grp_idx, ((grp_idx > 0)?runprof_table_shr_mem[grp_idx].quantity:0));

  if((grp_idx > -1) && !runprof_table_shr_mem[grp_idx].quantity && (loader_opcode == CLIENT_LOADER))
  {
    NSTL1(NULL, NULL, "Group (%s) is not participating in generator (%s). Hence, setting "
		                  "runtime changes status as success", grp, global_settings->event_generating_host);
    sprintf(err_msg, "Group (%s) is not participating in generator (%s)", grp, global_settings->event_generating_host);

    // setting flag = GROUP_IS_NOT_IN_GEN to show generator error and flag = 1 may be used
    *flag = GROUP_IS_NOT_IN_GEN;

    // If group is not running on generator, rtc should not be applied
    // return success
    return 0;
  }
	  
  
  NSDL2_RUNTIME(NULL, NULL, "grp_idx = %d, schedule_by = %d", grp_idx, global_settings->schedule_by);
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    if (grp_idx != -1)   /* i.e  not ALL */ {
      strcpy(err_msg, "In Schedule Based Scenario, group field can only be 'ALL'");
      return -1;
      //usage_kw_set_schedule(NULL);
    }
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    if (grp_idx == -1)   /* i.e ALL */ {
      strcpy(err_msg, "In Group Based Scenario, group field can not be 'ALL'"); 
      return -1;
      //usage_kw_set_schedule(NULL);
    }
  }

  //check_duplicate_phase_name(grp_idx, phase_name);

  NSDL2_RUNTIME(NULL, NULL, "phase_type = %s", phase_type);

  if (strcmp(phase_type, "START") == 0) {
    ret = parse_runtime_schedule_phase_start(grp_idx, buf, phase_name, err_msg);
  } else if (strcmp(phase_type, "RAMP_UP") == 0) {
    ret = parse_runtime_schedule_phase_ramp_up(grp_idx, buf, phase_name, err_msg, flag, -1, "NA", -1);
  } else if (strcmp(phase_type, "STABILIZATION") == 0) {
    ret = parse_runtime_schedule_phase_stabilize(grp_idx, buf, phase_name, err_msg);
  } else if (strcmp(phase_type, "DURATION") == 0) {
    ret = parse_runtime_schedule_phase_duration(grp_idx, buf, phase_name, err_msg);
  } else if (strcmp(phase_type, "RAMP_DOWN") == 0) {
    ret = parse_runtime_schedule_phase_ramp_down(grp_idx, buf, phase_name, err_msg, flag, -1, "NA", -1);
  } else {
    sprintf(err_msg, "Invalid Phase %s given in scenario file. Returning..", phase_name);
    ret = -1; 
  }

  return ret;
}
/*
 * This macro is for all group based keyowrd having no argument
 */
#define RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, func_name, flag) \
{ \
  grp_idx = find_grp_idx_from_kwd(buf, err_msg); \
  if(grp_idx == NS_GRP_IS_INVALID) ret = -1; \
  \
  else { \
    ret = func_name(buf, err_msg, flag); \
  } \
}


/* 
 * This macro is for all group based keywords whose value is to be set in one varible
 */
#define RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, func_name, shr_var_name, flag) \
{ \
  grp_idx = find_grp_idx_from_kwd(buf, err_msg); \
  if(grp_idx == NS_GRP_IS_INVALID) ret = -1; \
  \
  else { \
    if(grp_idx == NS_GRP_IS_ALL) \
    { \
      for (i = 0; i < total_runprof_entries && (ret == 0); i++) \
        ret = func_name(buf, &runprof_table_shr_mem[i].gset.shr_var_name, err_msg, flag); \
    } \
    else { \
      ret = func_name(buf, &runprof_table_shr_mem[grp_idx].gset.shr_var_name, err_msg, flag); \
    } \
  } \
}

#define RTC_GENERIC_GROUP_BASED_EX(grp_idx, buf, err_msg, func_name, shr_var_name, flag) \
{ \
  grp_idx = find_grp_idx_from_kwd(buf, err_msg); \
  if(grp_idx == NS_GRP_IS_INVALID) ret = -1; \
  \
  else { \
    if(grp_idx == NS_GRP_IS_ALL) \
    { \
      for (i = 0; i < total_runprof_entries && (ret == 0); i++) \
        ret = func_name(buf, &runprof_table_shr_mem[i].gset, err_msg, flag); \
    } \
    else { \
      ret = func_name(buf, &runprof_table_shr_mem[grp_idx].gset, err_msg, flag); \
    } \
  } \
}

#define RTC_GENERIC_GROUP_BASED_TWO_ARGS(grp_idx, buf, err_msg, func_name, shr_var_name1, shr_var_name2, flag) \
{ \
  grp_idx = find_grp_idx_from_kwd(buf, err_msg); \
  if(grp_idx == NS_GRP_IS_INVALID) ret = -1; \
  \
  else { \
    if(grp_idx == NS_GRP_IS_ALL) \
    { \
      for (i = 0; i < total_runprof_entries && (ret == 0); i++) \
        ret = func_name(buf, &runprof_table_shr_mem[i].gset.shr_var_name1, &runprof_table_shr_mem[i].gset.shr_var_name2, err_msg, flag); \
    } \
    else { \
      ret = func_name(buf, &runprof_table_shr_mem[grp_idx].gset.shr_var_name1, &runprof_table_shr_mem[grp_idx].gset.shr_var_name2, err_msg, flag); \
    } \
  } \
}

int kw_set_add_server(char *buff)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char argument[MAX_DATA_LINE_LENGTH];
  char err_buf[MAX_LINE_SIZE];

  err_buf[0]='\0';
  int i = 0;

  sscanf(buff, "%s %s", keyword, argument);
  NSDL2_RUNTIME(NULL, NULL, "Going to add new server with details = %s\n", argument);
  
  /*ServerCptr server_mon_ptr; (need to tokenize and get server name)
    int index;
    if(search_in_server_list(server_name, &server_mon_ptr, NULL, NULL, global_settings->hierarchical_view_vector_separator, 0, &index) == -1)
  */ 
  //replace $ with |
  while(argument[i] != '\0')
  {
    if(argument[i] == '$')
    {
      argument[i] = '|';
    }
 
    i++;
  }
  topolib_fill_server_structure(argument,err_buf,topo_idx);
  
  return 0;
}

int rtc_failed_details(char *keyword, char *buf_backup, char *err_msg, int *any_update_failed, char *args, char *curr_time_buffer, int grp_idx)
{
  int err_msg_len;
  // TODO: Add new level so that is does not get filtered
  if (loader_opcode != CLIENT_LOADER)
  {
    *any_update_failed = 1;
    NSTL1_OUT(NULL, NULL, "Error in applying runtime changes for Setting = %s \"%s\"\n Error = %s\n", 
                           keyword, args, err_msg);
    err_msg_len = snprintf(rtcdata->err_msg, 1024, "Error in applying runtime changes for Setting = %s %s, "
                                                       "Error %s", keyword, args, err_msg);
    if(CHECK_RTC_FLAG(RUNTIME_MONITOR_FLAG))
      write_msg(rtcdata->invoker_mccptr, rtcdata->err_msg, err_msg_len, 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE); 
  } 
  else 
  { /*For Generator, create error message add current timestamp and send failure message to controller*/
    if(!strstr(err_msg, "is already completed"))
    {
      snprintf(rtcdata->msg_buff, RTC_QTY_BUFFER_SIZE, "For generator %s at %s: Error in applying runtime changes for Setting = %s %s, Error = %s\n", 
                 global_settings->event_generating_host, nslb_get_cur_date_time(curr_time_buffer, 0), keyword, args, err_msg);
      send_rtc_msg_to_controller(master_fd, NC_RTC_FAILED_MESSAGE, rtcdata->msg_buff, grp_idx);
      return 0;
    }
  }    
  return *any_update_failed;
}

int rtc_success_details(char *keyword, int do_not_distribute_kw, char *buf_backup, int nc_flag_set, int grp_idx, int runtime_id, int runtime_idx, char *err_msg, char *curr_time_buffer, int err_msg_has_applied_value, char *args, char *owner_name, int is_rtc_for_qty, int rtc_send_counter, int *rtc_rec_counter, int *rtc_msg_len)
{
  NSDL2_RUNTIME(NULL, NULL, "Method Called, msg_len = %d, keyword = %s, args = %s", *rtc_msg_len, keyword, args);

  if (loader_opcode == MASTER_LOADER && !do_not_distribute_kw)
  {
   
    NSTL1(NULL, NULL, "(Master -> Generator) Sending keyword = %s, nc_flag_set = %d", buf_backup, nc_flag_set);
    char *ptr;
    if((ptr = strchr(buf_backup, '\n')) != NULL) //Replace newline with NULL char 
    *ptr = '\0';
    NSDL2_RUNTIME(NULL, NULL, "runtime_id = %d", runtime_id);
    //int ret = send_rtc_settings_to_generator(buf_backup, nc_flag_set, runtime_id);
    send_nc_apply_rtc_msg_to_all_gen(NC_APPLY_RTC_MESSAGE, buf_backup, nc_flag_set, runtime_id);
    fprintf(rtcdata->runtime_all_fp, "%s|%s|%s\n", get_relative_time(), nslb_get_owner(owner_name), buf_backup);
    //if(ret) return ret; //In case of failure we need to stop RTC and send resume message to all generators
    return 0;
  }

  if(err_msg_has_applied_value)
  {
    // TODO: Add new level so that is does not get filtered
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_OK, -1,
                            -1, EVENT_CORE, EVENT_INFORMATION,
                            keyword,
                            buf_backup,
                           "%s", err_msg);
    if (loader_opcode != CLIENT_LOADER) {
      NSTL1_OUT(NULL, NULL, "%s", err_msg);
      RUNTIME_UPDATE_LOG(err_msg);           
    } 
    else {
      snprintf(rtcdata->msg_buff, RTC_QTY_BUFFER_SIZE, "For generator %s at %s: Runtime changes applied for Keyword %s %s\n", 
                  global_settings->event_generating_host, nslb_get_cur_date_time(curr_time_buffer, 0), keyword, err_msg);
      send_rtc_msg_to_controller(master_fd, NC_RTC_APPLIED_MESSAGE, rtcdata->msg_buff, grp_idx);
      return 0;
    }
  }
  else
  {
    // TODO: Add new level so that is does not get filtered
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_OK, -1,
                            -1, EVENT_CORE, EVENT_INFORMATION,
                            keyword,
                            buf_backup,
                            "Runtime changes applied successfully for -------------->%s", buf_backup);
    if (loader_opcode != CLIENT_LOADER) {
      NSTL1_OUT(NULL, NULL, "Runtime changes applied for Keyword = %s \"%s\"\n", keyword, args);
      // In case of SYNC_POINT RTC got success, we were not writing in runtime_changes_all.log.
      if(strncmp(keyword,"SYNC_POINT",strlen("SYNC_POINT")) == 0)
      {
         RUNTIME_UPDATE_LOG(buf_backup);
      }
      else
      {
         RUNTIME_UPDATE_LOG(err_msg);
      }
    } 
    else {
      NSDL2_RUNTIME(NULL, NULL, "rtc_rec_counter = %d, rtc_send_counter = %d", *rtc_rec_counter, rtc_send_counter);
      //This is for generator only.
      if(*rtc_rec_counter) {
        if(*rtc_rec_counter == 1)
          *rtc_msg_len = snprintf(rtcdata->msg_buff, RTC_QTY_BUFFER_SIZE, "For generator %s at %s: "
                                   "Runtime changes applied for Keyword\n", global_settings->event_generating_host, 
                                   nslb_get_cur_date_time(curr_time_buffer, 0));
         
        *rtc_msg_len += snprintf(rtcdata->msg_buff + *rtc_msg_len, (RTC_QTY_BUFFER_SIZE - *rtc_msg_len), "%s %s\n", keyword, args);
        NSDL2_RUNTIME(NULL, NULL, "rtc_msg_len = %d, rtcdata->msg_buff = %s", *rtc_msg_len, rtcdata->msg_buff);
      }

      if(((is_rtc_for_qty == 1) && (*rtc_rec_counter == rtc_send_counter)) && (loader_opcode == CLIENT_LOADER)) {
        //sprintf(rtcdata->msg_buff, "For generator %s Runtime changes successfully Applied at %s\n", 
        //                     global_settings->event_generating_host, nslb_get_cur_date_time(curr_time_buffer, 0));
        send_rtc_msg_to_controller(master_fd, NC_RTC_APPLIED_MESSAGE, rtcdata->msg_buff, grp_idx);
        memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
      } else if(is_rtc_for_qty != 1) {

	// If group is not running in gen, don't set rtcdata->msg_buff
        if(nc_flag_set != GROUP_IS_NOT_IN_GEN)
          sprintf(rtcdata->msg_buff, "For generator %s at %s: Runtime changes applied for Keyword = %s %s\n",
                 global_settings->event_generating_host, nslb_get_cur_date_time(curr_time_buffer, 0), keyword, args);
        send_rtc_msg_to_controller(master_fd, NC_RTC_APPLIED_MESSAGE, rtcdata->msg_buff, grp_idx);
       
      }
      return 0; 
    } 
  }
  // Append Ok keyword in this file
  if (loader_opcode != CLIENT_LOADER)
  {
    //fprintf(runtime_all_fp, "%s\n", buf_backup);
    //log with data/time/username
    fprintf(rtcdata->runtime_all_fp, "%s|%s|%s\n", get_relative_time(), nslb_get_owner(owner_name), buf_backup);
  }
  return 0;
}

void fill_data_in_gen_keyword_buffer(char *line)
{
  int i;
  NSDL3_MESSAGES(NULL, NULL, "Method Called, line = %s", line);
  for(i = 0; i < sgrp_used_genrator_entries; i++) 
  {
    if (g_msg_com_con[i].fd == -1) {
      if (g_msg_com_con[i].ip)
        NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));
    } else {
      strcpy(generator_entry[i].gen_keyword, line);
      generator_entry[i].flags |= SCEN_DETAIL_MSG_SENT;
    }
    NSDL3_MESSAGES(NULL, NULL, "gen_idx = %d, gen_keyword = %s, flags = %0x", i, generator_entry[i].gen_keyword, generator_entry[i].flags);
  }
}

int g_rtc_start_idx;
int read_runtime_keyword(char *line, char *err_msg, int *mon_status, int *first_time, int id, int is_rtc_for_qty, int rtc_send_counter, int *rtc_rec_counter, int *rtc_msg_len)
{
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[1024];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char *buf;
  char *args;
  int num, len, ret, i, val;
  int grp_idx = -1;
  int err_msg_has_applied_value = 0, any_update_failed = 0;
  //char nc_rtc_msg[1024]; //In case of generator we need to create rtc message
  int nc_flag_set = 0; //nc_flag_set: need to set flag were we call scenario distribution tool 
  int do_not_distribute_kw = 0; //do_not_distribute_kw: need to set flag in case of keyword that we dont want to distribute
  char curr_time_buffer[100];
  char owner_name[256] = {0};
  int runtime_idx = 0;
  static int runtime_id = -1;
  if (id != -1)
    runtime_id = id;
 
  NSDL1_RUNTIME(NULL, NULL, "Method called, msg_len = %d", *rtc_msg_len);

  //For debugging purpose
  strcpy(buf_backup, line);
  if ((num = sscanf(line, "%s %s", keyword, text)) < 2) {
    return 0;
  } 
  buf = line;      
  ret = 0;
  err_msg_has_applied_value = 0;
  NSDL2_RUNTIME(NULL, NULL, "line = %s, keyword=%s", line, keyword);

  if (strncasecmp(keyword, "SCHEDULE", strlen("SCHEDULE")) == 0) {
    //grp_idx = find_grp_idx_from_kwd(buf, err_msg);
    ret = kw_set_runtime_schedule(line, err_msg, &nc_flag_set);
  } else if (strncasecmp(keyword, "G_SESSION_PACING", strlen("G_SESSION_PACING")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_g_session_pacing, 1);
    err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "SYNC_POINT", strlen("SYNC_POINT")) == 0) {
    NSDL2_RUNTIME(NULL, NULL, "after SYNC_POINT check .....  line == %s", line);
    ret = kw_set_sync_point(line, err_msg, 1);
    NSDL2_RUNTIME(NULL, NULL, "end of if SYNC_POINT check .... line === %s", line);
    //kw_set_sync_point(buf, 0, 1);
    //ret = kw_set_sync_point(buf, err_msg, 1);
    //err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "CMON_SETTINGS", strlen("CMON_SETTINGS")) == 0) {
    ret = kw_set_cmon_settings(buf, err_msg, 1);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strcasecmp(keyword, "G_FIRST_SESSION_PACING") == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_g_first_session_pacing, 1);
    err_msg_has_applied_value = 1;
  } else if (strcasecmp(keyword, "G_NEW_USER_ON_SESSION") == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_g_new_user_on_session, 1);
    err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "G_PAGE_THINK_TIME", strlen("G_PAGE_THINK_TIME")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_g_page_think_time, 1);
    err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "G_INLINE_DELAY", strlen("G_INLINE_DELAY")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_g_inline_delay, 1);
    err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "REPLAY_FACTOR", strlen("REPLAY_FACTOR")) == 0) {
    ret = kw_set_replay_factor(buf, err_msg, 1);
    err_msg_has_applied_value = 1;
  } else if (strncasecmp(keyword, "G_USER_CLEANUP_MSECS", strlen("G_USER_CLEANUP_MSECS")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_user_cleanup_msecs, user_cleanup_time, 1);
  } else if (strncasecmp(keyword, "G_IDLE_MSECS", strlen("G_IDLE_MSECS")) == 0) {
    RTC_GENERIC_GROUP_BASED_EX(grp_idx, buf, err_msg, kw_set_idle_msecs, idle_secs, 1);
  } else if (strncasecmp(keyword, "G_MAX_URL_RETRIES", strlen("G_MAX_URL_RETRIES")) == 0) {
    RTC_GENERIC_GROUP_BASED_TWO_ARGS(grp_idx, buf, err_msg, kw_set_max_url_retries, max_url_retries, retry_on_timeout, 1);
  } else if (strncasecmp(keyword, "G_NO_VALIDATION", strlen("G_NO_VALIDATION")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_no_validation, no_validation, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_REUSEADDR", strlen("G_DISABLE_REUSEADDR")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_reuseaddr, disable_reuseaddr, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_HOST_HEADER", strlen("G_DISABLE_HOST_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_host_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_UA_HEADER", strlen("G_DISABLE_UA_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_ua_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_ACCEPT_HEADER", strlen("G_DISABLE_ACCEPT_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_accept_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_ACCEPT_ENC_HEADER", strlen("G_DISABLE_ACCEPT_ENC_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_accept_enc_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_KA_HEADER", strlen("G_DISABLE_KA_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_ka_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_CONNECTION_HEADER", strlen("G_DISABLE_CONNECTION_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_connection_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_DISABLE_ALL_HEADER", strlen("G_DISABLE_ALL_HEADER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_disable_all_header, disable_headers, 1);
  } else if (strncasecmp(keyword, "G_USE_RECORDED_HOST_IN_HOST_HDR", strlen("G_USE_RECORDED_HOST_IN_HOST_HDR")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_use_recorded_host_in_host_hdr, use_rec_host, 1);
/*     } else if (strncasecmp(keyword, "AUTO_REDIRECT", strlen("AUTO_REDIRECT")) == 0) { */
/*       //for (i = 0; i < total_runprof_entries && (ret == 0); i++) */
/*       ret = kw_set_auto_redirect(buf, err_msg); */
  } else if (strncasecmp(keyword, "G_AVG_SSL_REUSE", strlen("G_AVG_SSL_REUSE")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_avg_ssl_reuse, avg_ssl_reuse, 1);
  } else if (strncasecmp(keyword, "G_SSL_CLEAN_CLOSE_ONLY", strlen("G_SSL_CLEAN_CLOSE_ONLY")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_ssl_clean_close_only, ssl_clean_close_only, 1);
    //if ((grp_idx = find_grp_idx_from_kwd_line(buf)) == -1) {
       // for (i = 0; i < total_runprof_entries && (ret == 0); i++)
         // ret = kw_set_ssl_clean_close_only(keyword, text, &runprof_table_shr_mem[grp_idx].gset.ssl_clean_close_only, err_msg);
     // } else {
       // ret = kw_set_ssl_clean_close_only(keyword, text,  &runprof_table_shr_mem[grp_idx].gset.ssl_clean_close_only, err_msg);
     // }
/*     } else if (strncasecmp(keyword, "DISABLE_COOKIES", strlen("DISABLE_COOKIES")) == 0) { */
/*       ret = kw_set_cookies(keyword, text, err_msg); */
/*     } else if (strncasecmp(keyword, "AUTO_COOKIE", strlen("AUTO_COOKIE")) == 0) { */
/*       ret = kw_set_auto_cookie(keyword, text, err_msg); */
  } else if (strncasecmp(keyword, "G_ON_EURL_ERR", strlen("G_ON_EURL_ERR")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_on_eurl_err, on_eurl_err, 1);
  } else if (strncasecmp(keyword, "G_ERR_CODE_OK", strlen("G_ERR_CODE_OK")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_err_code_ok, errcode_ok, 1);
/*     } else if (strncasecmp(keyword, "G_LOGGING", strlen("G_LOGGING")) == 0) { */
/*       if ((grp_idx = find_grp_idx_from_kwd_line(buf)) == -1) { */
/*         for (i = 0; i < total_runprof_entries && (ret == 0); i++) */
/*           ret = kw_set_logging(buf, &runprof_table_shr_mem[i].gset.log_level, &runprof_table_shr_mem[i].gset.log_dest, err_msg); */
/*         //kw_set_logging(buf, &group_default_settings->log_level, &group_default_settings->log_dest); */
/*       } else { */
/*         ret = kw_set_logging(buf, &runprof_table_shr_mem[grp_idx].gset.log_level, &runprof_table_shr_mem[grp_idx].gset.log_dest, err_msg); */
/*       } */
  } else if (strncasecmp(keyword, "G_TRACING", strlen("G_TRACING")) == 0) { 
    if ((grp_idx = find_grp_idx_from_kwd_line(buf)) == -1) { 
      for (i = 0; i < total_runprof_entries; i++) 
        ret = kw_set_tracing(buf, &runprof_table_shr_mem[i].gset.trace_level, 
                             &runprof_table_shr_mem[i].gset.max_trace_level, 
                             &runprof_table_shr_mem[i].gset.trace_dest, 
                             &runprof_table_shr_mem[i].gset.max_trace_dest, 
                             &runprof_table_shr_mem[i].gset.trace_on_fail, 
                             &runprof_table_shr_mem[i].gset.max_log_space, 
                             &runprof_table_shr_mem[i].gset.trace_inline_url, 
                             &runprof_table_shr_mem[i].gset.trace_limit_mode, 
                             &runprof_table_shr_mem[i].gset.trace_limit_mode_val, 
                             err_msg, 1); 
    } else { 
      ret = kw_set_tracing(buf, &runprof_table_shr_mem[grp_idx].gset.trace_level, 
                           &runprof_table_shr_mem[grp_idx].gset.max_trace_level, 
                           &runprof_table_shr_mem[grp_idx].gset.trace_dest, 
                           &runprof_table_shr_mem[grp_idx].gset.max_trace_dest, 
                           &runprof_table_shr_mem[grp_idx].gset.trace_on_fail, 
                           &runprof_table_shr_mem[grp_idx].gset.max_log_space, 
                           &runprof_table_shr_mem[grp_idx].gset.trace_inline_url, 
                           &runprof_table_shr_mem[grp_idx].gset.trace_limit_mode, 
                           &runprof_table_shr_mem[grp_idx].gset.trace_limit_mode_val, 
                           err_msg, 1); 
    } 
/*     } else if (strncasecmp(keyword, "G_REPORTING", strlen("G_REPORTING")) == 0) { */
/*       if ((grp_idx = find_grp_idx_from_kwd_line(buf)) == -1) { */
/*         for (i = 0; i < total_runprof_entries && (ret == 0); i++) */
/*           ret = kw_set_reporting(buf, &runprof_table_shr_mem[i].gset.report_level, err_msg); */
/*       } else { */
/*         ret = kw_set_reporting(buf, &runprof_table_shr_mem[grp_idx].gset.report_level, err_msg); */
/*       } */
  } else if (strncasecmp(keyword, "G_MODULEMASK", strlen("G_MODULEMASK")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_modulemask, module_mask, 1);
  } else if (strncasecmp(keyword, "G_DEBUG", strlen("G_DEBUG")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_debug, debug, 1);
  } else if (strncasecmp(keyword, "MAX_DEBUG_LOG_FILE_SIZE", strlen("MAX_DEBUG_LOG_FILE_SIZE")) == 0) {
    ret = kw_set_max_debug_log_file_size(buf, err_msg, 1);
  } else if (strncasecmp(keyword, "MAX_ERROR_LOG_FILE_SIZE", strlen("MAX_ERROR_LOG_FILE_SIZE")) == 0) {
    ret = kw_set_max_error_log_file_size(buf, err_msg, 1);
  #ifdef RMI_MODE
  } else if (strncasecmp(keyword, "G_URL_IDLE_SECS", strlen("G_URL_IDLE_SECS")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_url_idle_secs, url_idle_secs, 1);
  #endif
  } else if (strncasecmp(keyword, "G_KA_PCT", strlen("G_KA_PCT")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_ka_pct, ka_pct, 1);
  } else if (strncasecmp(keyword, "G_NUM_KA", strlen("G_NUM_KA")) == 0) {
    RTC_GENERIC_GROUP_BASED_TWO_ARGS(grp_idx, buf, err_msg, kw_set_num_ka, num_ka_min, num_ka_range, 1);
  } else if (strncasecmp(keyword, "G_ENABLE_REFERER", strlen("G_ENABLE_REFERER")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_enable_referer, enable_referer, 1);
    NSDL2_RUNTIME(NULL, NULL, "Method Called, enable_referer = %d",runprof_table_shr_mem[0].gset.enable_referer);
  } else if (strncasecmp(keyword, "G_MAX_USERS", strlen("G_MAX_USERS")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_g_max_users, grp_max_user_limit, 1);
  } else if (strncasecmp(keyword, "MAX_USERS", strlen("MAX_USERS")) == 0) {
    ret = kw_set_max_users(buf, err_msg, 1);
  } else if (strncasecmp(keyword, "G_GET_NO_INLINED_OBJ" ,strlen("G_GET_NO_INLINED_OBJ")) == 0) {
    RTC_GENERIC_GROUP_BASED(grp_idx, buf, err_msg, kw_set_get_no_inlined_obj, get_no_inlined_obj, 1);
  } else if (strncasecmp(keyword, "G_AUTO_FETCH_EMBEDDED", strlen("G_AUTO_FETCH_EMBEDDED")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_auto_fetch_embedded, 1);
  } else if (strncasecmp(keyword, "G_CONTINUE_ON_PAGE_ERROR", strlen("G_CONTINUE_ON_PAGE_ERROR")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_continue_on_page_error, 1);
  } else if(strncasecmp(keyword, "G_OVERRIDE_RECORDED_THINK_TIME", strlen("G_OVERRIDE_RECORDED_THINK_TIME")) == 0) {
    RTC_GENERIC_GROUP_WITH_NO_ARGS(grp_idx, buf, err_msg, kw_set_override_recorded_think_time, 1);
  } else if (strncasecmp(keyword, "RUNTIME_CHANGE_QUANTITY_SETTINGS", strlen("RUNTIME_CHANGE_QUANTITY_SETTINGS")) == 0) {
    NSDL2_RUNTIME(NULL, NULL, "line = %s, keyword=%s", line, keyword);
    ret = kw_set_runtime_change_quantity_settings(line, err_msg);
    if(is_rtc_for_qty && (loader_opcode == CLIENT_LOADER))
    return ret;
  } else if (strncasecmp(keyword, "QUANTITY", strlen("QUANTITY")) == 0) {
    (*first_time)++;
    NSDL2_RUNTIME(NULL, NULL, "Keyword parsing of QUANTITY: first_time = %d, runtime_id = %d", *first_time, runtime_id);
    if (loader_opcode != CLIENT_LOADER)
    {
      if (!(*first_time))
        runtime_id++;
      NSDL2_RUNTIME(NULL, NULL, "runtime_id = %d", runtime_id);
    }
    ret = kw_set_runtime_users_or_sessions(line, err_msg, runtime_id, *first_time);
    if((ret == RUNTIME_SUCCESS) && (loader_opcode == CLIENT_LOADER))
      (*rtc_rec_counter)++;
  } else if (strncasecmp(keyword, "START_MONITOR", strlen("START_MONITOR")) == 0) {    
    ret = kw_set_runtime_monitors(line, err_msg);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strncasecmp(keyword, "STOP_MONITOR", strlen("STOP_MONITOR")) == 0) {    
    ret = kw_set_runtime_monitors(line, err_msg);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strncasecmp(keyword, "RESTART_MONITOR", strlen("RESTART_MONITOR")) == 0) {   
    ret = kw_set_runtime_monitors(line, err_msg);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strncasecmp(keyword, "MONITOR_PROFILE", 15) == 0) { 
    NSDL2_RUNTIME(NULL, NULL, "line = %s, keyword=%s", line, keyword);
    ret = kw_set_monitor_profile(text, num, err_msg, 1, mon_status);
    NSDL2_RUNTIME(NULL, NULL, "ret from kw_set_monitor_profile = %d", ret);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
    if(ret == 0) //atleast one monitor parsing successfull
    {
      monitor_runtime_changes_applied = 1; //set in both the cases 'ADD/DELETE'
    }
  } else if (strncasecmp(keyword, "DELETE_MONITOR", strlen("DELETE_MONITOR")) == 0) { //not called after json is supported
    ret = kw_set_runtime_monitors(line, err_msg);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
    if(ret == 0) //atleast one monitor parsing successfull
    {
      monitor_runtime_changes_applied = 1;  //set in both the cases 'ADD/DELETE'
      monitor_deleted_on_runtime = 2;  //set in both the cases 'ADD/DELETE'
    }
  } else if (strcasecmp(keyword, "ENABLE_AUTO_JSON_MONITOR") == 0) {
    ret = kw_set_enable_auto_json_monitor(keyword, buf, 1, err_msg); 
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strcasecmp(keyword, "NS_TRACE_LEVEL") == 0) {
    kw_set_ns_trace_level(buf, err_msg, 1);
  } else if (strcasecmp(keyword, "SECONDARY_GUI_SERVER") == 0) {
    kw_set_ns_server_secondary(buf, 1);
  } else if(strcmp(keyword, "ENABLE_MONITOR_DATA_LOG") == 0){
    ret = kw_set_nd_monitor_log(buf, err_msg);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if (strncasecmp(keyword, "ND_DATA_VALIDATION", strlen("ND_DATA_VALIDATION")) == 0) {
    ret = kw_set_nd_enable_data_validation(keyword, buf, err_msg, 1);
    err_msg_has_applied_value = 1;
    do_not_distribute_kw = 1;
  } else if(strcmp(keyword, "ADD_SERVER") == 0){
    kw_set_add_server(buf);
  } else if(strcmp(keyword, "SSL_KEY_LOG") == 0){
    kw_set_ssl_key_log(buf);
  }else if(strcmp(keyword, "CONTROLLER_NAME") == 0){
    NSTL1(NULL, NULL, "Ignoring keyword CONTROLLER_NAME.");
    return 0;
  } 
  else if(strcmp(keyword, "ENABLE_ALERT") == 0){
    ret = kw_set_enable_alert(buf, err_msg, 1);
    if(!ret)
    { 
      if(loader_opcode == MASTER_LOADER)
        sprintf(buf_backup, "%s %s", keyword, g_alert_info);
      else
        send_rtc_msg_to_all_clients(APPLY_ALERT_RTC, NULL, 0);
    }
    err_msg_has_applied_value = 1;
  }
 /*
  else if(strcmp(keyword, "ALERT_SERVER_CONFIG") == 0){
    ret = kw_set_alert_server_config(buf, err_msg, 1);
    err_msg_has_applied_value = 1;
    sprintf(buf_backup, "%s %s", keyword, g_alert_info);
  }*/
  else
  {
    ret = -1;
    strcpy(err_msg, "Keyword cannot be changed at runtime");
  }
 
  args = buf_backup + strlen(keyword) + 1;
  len = strlen(args) - 1;
  
  if (args[len] == '\n') args[len] = '\0';

  if (ret != 0) /* failed while consuming */
  { 
    val = rtc_failed_details(keyword, buf_backup, err_msg, &any_update_failed, args, curr_time_buffer, grp_idx); 
    return(val);
  } 
  else  /* Applied successfully */               
  {                   
    val = rtc_success_details(keyword, do_not_distribute_kw, buf_backup, nc_flag_set, grp_idx, runtime_id, runtime_idx, err_msg, curr_time_buffer, err_msg_has_applied_value, args, owner_name, is_rtc_for_qty, rtc_send_counter, rtc_rec_counter, rtc_msg_len);
    return(val);
  }
  return any_update_failed; 
}  
// --- Run Time Changes  -- END


//This function is to just check if any monitor related runtime changes are done or not
int is_runtime_changes_for_monitor(char *inbuff)
{
  //char text[MAX_DATA_LINE_LENGTH];
  //char keyword[MAX_DATA_LINE_LENGTH];
  //char line[MAX_DATA_LINE_LENGTH];
  //char new_file_name[MAX_DATA_LINE_LENGTH];
  //FILE *conf_file = NULL;
  //char err_msg[64000];
  char flag = 0;
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, inbuff = %s", inbuff); 
  //sprintf(new_file_name, "%s/logs/TR%d/runtime_changes/%s", g_ns_wdir, testidx, SORTED_RUNTIME_CHANGES_CONF);
  //conf_file = fopen(new_file_name, "r");
  //if (!conf_file) 
  //{
   // sprintf(err_msg, "Error in opening file %s. Errno=%s", new_file_name, nslb_strerror(errno));
    //RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES(err_msg);
    //return -1;
  //}

  //err_msg[0] = '\0'; // Make it NULL as functions append to this buffer

  //while (nslb_fgets(line, MAX_DATA_LINE_LENGTH, conf_file, 0) != NULL)
  {
    
   // sscanf(line, "%s %s", keyword, text);

    if((strstr(inbuff, "START_MONITOR") == 0) || (strstr(inbuff, "STOP_MONITOR") == 0) || (strstr(inbuff, "RESTART_MONITOR") == 0) || (strstr(inbuff, "MONITOR_PROFILE") == 0) || (strstr(inbuff, "DELETE_MONITOR") == 0) || (strstr(inbuff, "ND_DATA_VALIDATION") == 0) || (strcmp(inbuff, "ENABLE_MONITOR_DATA_LOG") == 0))
    {
      //continue;
    }
    else
    {
      flag = 1;
      //break;
    }
  }

  NSDL2_RUNTIME(NULL, NULL, "flag = %d", flag);
  return (!flag);
}


void parse_runtime_changes()
{
  char file_name[MAX_DATA_LINE_LENGTH];
  char file_name_all[MAX_DATA_LINE_LENGTH];
  struct stat st;
  char err_msg[64000];
  int any_update_failed = 0;
  int mon_status = 0;
  int first_time = -1;
  int rtc_rec_counter = 0, rtc_msg_len = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method Called");
    
  /* open log file */
  if(!rtcdata->rtclog_fp && (!CHECK_RTC_FLAG(RUNTIME_MONITOR_FLAG)))
    RUNTIME_UPDATION_OPEN_LOG

  sprintf(file_name, "%s/logs/TR%d/runtime_changes/%s", g_ns_wdir, testidx, RUNTIME_CHANGES_CONF);
  if (stat(file_name, &st) == -1) {
    sprintf(err_msg, "Error in opening file %s. Errno=%s", file_name, nslb_strerror(errno));
    RUNTIME_UPDATION_LOG_ERROR(err_msg, 0);
    return;
  }

  sprintf(file_name_all, "%s/logs/TR%d/runtime_changes/%s", g_ns_wdir, testidx, RUNTIME_CHANGES_CONF_ALL);
  if ((rtcdata->runtime_all_fp = fopen(file_name_all, "a+")) == NULL) {
    sprintf(err_msg, "Error in opening file %s. Errno=%s", file_name_all, nslb_strerror(errno));
    RUNTIME_UPDATION_LOG_ERROR(err_msg, 0);
    return;
  }

  err_msg[0] = '\0'; // Make it NULL as functions append to this buffer
  NSTL1(NULL, NULL, "Going to distruibute RTC Quantity on Controller/Parent where rtc_qty_flag = %d",
                    (CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)?1:0));

  if((loader_opcode == MASTER_LOADER) && CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)) {
    send_nc_apply_rtc_msg_to_all_gen(NC_APPLY_RTC_MESSAGE, rtcdata->msg_buff, nc_flag_set, runtime_id);
    dump_rtc_log_in_runtime_all_conf_file(rtcdata->runtime_all_fp);
    return;
  } 
  else 
  {
    /* In case of NS/Generator Parent RTC data parsed from rtcdata->msg_buff.*/
    char *line = rtcdata->msg_buff;
    char *start_ptr = NULL;
    while (*line != '\0') 
    {
      NSDL2_RUNTIME(NULL, NULL, "RTC: where line = %s", line);
      if((start_ptr = strchr(line, '\n')) != NULL)
      {
        *start_ptr = '\0';
        start_ptr++;
        // Reading keyword
        fill_data_in_gen_keyword_buffer(line);
        any_update_failed = read_runtime_keyword(line, err_msg, &mon_status, &first_time, -1, 0, 0, &rtc_rec_counter, &rtc_msg_len);
        if(any_update_failed)
        {
          /* In case of NS/Generator Parent RTC apply on phase is already completed. */ 
          if(strstr(err_msg, "is already completed"))
          {
            if(start_ptr != NULL)
            {
              line = start_ptr;
              continue;
            }
          }       
          break; //In error case we dont process anymore
          err_msg[0]='\0';
        }
        line = start_ptr;
      }
    }
  }

  NSDL2_RUNTIME(NULL, NULL, "mon_status = %d", mon_status);
  // any_update_failed is not really needed as we stop on first error. Keep it for future
  if (any_update_failed != 0 || mon_status != 0) {
    NSTL1_OUT(NULL, NULL, "Error in applying runtime changes in one or more settings");
    RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
    RUNTIME_UPDATION_CLOSE_FILES
    if((loader_opcode != CLIENT_LOADER) && (rtcdata->type != APPLY_FPARAM_RTC) && (rtcdata->type != TIER_GROUP_RTC))\
      delete_runtime_changes_conf_file();\
  }
  else
  {
    if(loader_opcode == STAND_ALONE)
    {
      if(CHECK_RTC_FLAG(RUNTIME_MONITOR_FLAG)){
        strcpy(rtcdata->err_msg, "Runtime changes applied Successfully");
        write_msg(rtcdata->invoker_mccptr, rtcdata->err_msg, strlen(rtcdata->err_msg), 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE);
      }
      else
      {
        RUNTIME_UPDATE_LOG("Runtime changes applied Successfully");
      }
      NS_EL_2_ATTR(EID_RUNTIME_CHANGES_OK, -1,
                                    -1, EVENT_CORE, EVENT_INFORMATION,
                                    "NA",
                                    "NA",
                                    "Runtime changes applied Successfully");
      NSTL1(NULL, NULL, "Runtime changes applied Successfully");
      RUNTIME_UPDATION_CLOSE_FILES
    }
    if(CHECK_RTC_FLAG(RUNTIME_ALERT_FLAG)){
      rtcdata->cur_state = RTC_START_STATE;   
    }
  }
}

//this method will move the the runtime_changes.conf file after changes are applied as runtime_changes.conf.bkp
//it will remove runtime_changes.conf.bkp using unlink and then rename it using rename method
int delete_runtime_changes_conf_file()
{
  char file_name[MAX_DATA_LINE_LENGTH]; 
  char bkp_file_name[MAX_DATA_LINE_LENGTH];
  int ret; 
  sprintf(bkp_file_name, "%s/logs/TR%d/runtime_changes/%s.bak", g_ns_wdir, testidx, RUNTIME_CHANGES_CONF);
  ret = unlink(bkp_file_name);
  if(ret < 0)
  {
    if(errno != ENOENT)
    {
      NSTL1_OUT(NULL, NULL, "Error in removing runtime changes backup file. Error = %s", nslb_strerror(errno));
      return 0;
    }
  } 

  sprintf(file_name, "%s/logs/TR%d/runtime_changes/%s", g_ns_wdir, testidx, RUNTIME_CHANGES_CONF);
  if((rename(file_name, bkp_file_name)) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Error in renaming runtime changes to backup file. Error = %s", nslb_strerror(errno));
  }
  return 0;
}

void apply_resume_rtc(int only_monitor_runtime_change)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called.");

  //flushing all fds to dump rtc messages
  fflush(NULL);
 /*PageDump: Need to divide number of sessions among NVMs*/
  if (!only_monitor_runtime_change)
    divide_session_per_proc();

  if ((loader_opcode == MASTER_LOADER) && (!only_monitor_runtime_change))
  {
    //Once runtime changes applied successfully on generator send resume message to all generator, even in case of failure
    NSTL1(NULL, NULL, "(Master -> Generator) RTC_RESUME(139) for resume processing");
    //remove_epoll_from_controller();
    if(rtc_resume_from_pause() != 0)
      return;
    rtcdata->cur_state = RTC_RESUME_STATE;
    send_msg_to_all_clients(RTC_RESUME, 0);
    if(CHECK_ALL_RTC_MSG_DONE)
    {
      //TODO: RTC will take sometime to process
      //No message was sent from here
      (rtcdata->msg_seq_num)--;
      return;
    }
    check_and_send_next_phase_msg();
    //Resetting bit  mask
    /*for (i=0; i < sgrp_used_genrator_entries; i++)
    {
      strcpy(gen_ip_local, g_msg_com_con[i].ip);
      gen_id=get_gen_id_from_ip(gen_ip_local);
      if (gen_id != -1)
        generator_entry[gen_id].flags &= ~IS_GEN_ACTIVE;
    }*/
  }
  else
  {
    if(!only_monitor_runtime_change)
      process_resume_from_rtc();
    else{
      rtcdata->cur_state = RESET_RTC_STATE;
      RUNTIME_UPDATION_RESPONSE
    }
  }

  /* In run time changes for page dump, if test is started with level 0 and 
   * after some time page dump is eanbled in online mode, then we need to show 
   * page dump link in reports. So need to updatea summary.top file
   * to show page_dump*/
  if ((get_max_tracing_level() > TRACE_DISABLE) && (get_max_trace_dest() > 0)) {
    NSDL2_MESSAGES(NULL, NULL, "Need to update summary.top");
    NSTL1(NULL, NULL, "Going to update summary.top file");
    update_summary_top_field(4, "Available_New", 0);
    NSTL1(NULL, NULL, "Completed updation of summary.top file");
  }
}

void apply_runtime_changes(char only_monitor_runtime_change)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called.");

  parse_runtime_changes();
  if(CHECK_RTC_FLAG(RUNTIME_FAIL))
  {
    apply_resume_rtc(only_monitor_runtime_change);
    return;
  }

  if((loader_opcode == MASTER_LOADER) && !only_monitor_runtime_change)
    return;

  //for success
  SET_RTC_FLAG(RUNTIME_PASS);
  apply_resume_rtc(only_monitor_runtime_change);
}


void get_monitor_status(int mon_info_index, char *send_msg)
{
  MonConfig *mon_config;
  int mon_id; 
 
  mon_config = mon_config_list_ptr[mon_info_index].mon_config;
 
  for(mon_id=0; mon_id< mon_config->total_mon_id_index;mon_id++)
  {
    //save the msg in send_msg so that we can send the msg to mccptr
    if(mon_config->mon_id_struct[mon_id].status == MJ_QUEUED)
      snprintf(send_msg, 2200, "monitorName=%s,status=%d,monSuccess=0,monFailure=%d,queueOrd=0,estTime=5,msg=NA",
                               mon_config->g_mon_id, mon_config->mon_id_struct[mon_id].status, mon_config->mon_err_count);
    else if(mon_config->mon_id_struct[mon_id].status == MJ_SUCCESS)
      snprintf(send_msg, 2200, "monitorName=%s,status=%d,monSuccess=%d,monFailure=%d,queueOrd=0,estTime=5,msg=%s",
                               mon_config->g_mon_id, mon_config->mon_id_struct[mon_id].status, mon_config->count, mon_config->mon_err_count,
                               mon_config->mon_id_struct[mon_id].message);
    else
      snprintf(send_msg, 2200, "monitorName=%s,status=%d,monSuccess=%d,monFailure=%d,queueOrd=0,estTime=0,msg=%s",
                               mon_config->g_mon_id, mon_config->mon_id_struct[mon_id].status, mon_config->count, mon_config->mon_err_count,
                               mon_config->mon_id_struct[mon_id].message); 
  }
  return;
}


void process_and_extract_rtc_data(char **data, int num_fields, char *send_msg)
{
  char operation[128] = "\0";
  char g_mon_id[128] = "\0";
  char json_file_path[512] = "\0";
  char keys[256] = "\0";
  char mon_args[512] = "\0";

  int i;
  int mon_info_index;
  
  for(i = 0;i < num_fields;i++)
  {
    if(data[i])
    {
      if(!strncmp(data[i],"o=",2))
      {
        if(strlen(data[i] + 2) > 0)
          strcpy(operation, (data[i] + 2));
      }
      else if(!strncmp(data[i],"m=",2))
      {
        if(strlen(data[i] + 2) > 0)
          strcpy(g_mon_id, (data[i] + 2));
      }
      else if(!strncmp(data[i],"f=",2))
      {
        if(strlen(data[i] + 2) > 0)
          strcpy(json_file_path, (data[i] + 2));
      }
      else if(!strncmp(data[i],"k=",2))
      {
        if(strlen(data[i] + 2) > 0)
          strcpy(keys, (data[i] + 2));
      }
    }
  }
  
  if(!(strcmp(operation,"cm_status")))
  {
    mon_info_index = nslb_get_norm_id(g_monid_hash, g_mon_id, strlen(g_mon_id));
    if(mon_info_index == -2)
    {
      snprintf(send_msg, MAX_LINE_SIZE,
      "monitorName=%s,status=4,monSuccess=0,monFailure=0,queueOrd=0,estTime=0,msg=Received INVALID mon_info_index for this monitor", g_mon_id);
    }
    else
      get_monitor_status(mon_info_index, send_msg);
  }
  else if(!(strcmp(operation,"cm_add_mon")))
  {
    mj_read_json_and_save_struct(json_file_path, 1);
    mon_info_index = nslb_get_norm_id(g_monid_hash, g_mon_id, strlen(g_mon_id));
    //save the msg in send_msg so that we can send the msg to mccptr
    if(mon_info_index == -2)
    {
      snprintf(send_msg, MAX_LINE_SIZE,
      "monitorName=%s,status=4,monSuccess=0,monFailure=0,queueOrd=0,estTime=0,msg=Monitor is not getting applied due to json is not correct",
      g_mon_id);
    }
    else
    {
      snprintf(send_msg, MAX_LINE_SIZE, "monitorName=%s,status=1,monSuccess=0,monFailure=0,queueOrd=0,estTime=10,msg=NA", g_mon_id);
    }
  }
  // cm_synthetic_mon -o opcode -k “<comma separated keyword>” [opcode can be cm_update_monitor, cm_pause_monitor, cm_resume_monitor]
  // In case of cm_update keyword will be mandatory [program name]
  else if(!(strcmp(operation,"cg_pause")))
  {
    mon_info_index = nslb_get_norm_id(g_monid_hash, g_mon_id, strlen(g_mon_id));
    if(mon_info_index == -2)
    {
      NSTL1_OUT(NULL, NULL, "Received INVALID g_mon_id %s", g_mon_id);
    }
    else
    {
      sprintf(mon_args,"cm_synthetic_mon -o cm_pause_monitor");
      make_cm_update_msg_and_send_msg_to_ndc(mon_args, mon_info_index);
    }
  }
  else if(!(strcmp(operation,"cg_resume")))
  {
    mon_info_index = nslb_get_norm_id(g_monid_hash, g_mon_id, strlen(g_mon_id));
    if(mon_info_index == -2)
    {
      NSTL1_OUT(NULL, NULL, "Received INVALID g_mon_id %s", g_mon_id);
    }
    else
    {
      sprintf(mon_args,"cm_synthetic_mon -o cm_resume_monitor");
      make_cm_update_msg_and_send_msg_to_ndc(mon_args, mon_info_index);
    }
  }
  else if(!(strcmp(operation,"cg_update")))
  {
    mon_info_index = nslb_get_norm_id(g_monid_hash, g_mon_id, strlen(g_mon_id));
    if(mon_info_index == -2)
    {
      NSTL1_OUT(NULL, NULL, "Received INVALID g_mon_id %s", g_mon_id);
    }
    else
    {
      sprintf(mon_args,"cm_synthetic_mon -o cm_update_monitor -k %s", keys);
      make_cm_update_msg_and_send_msg_to_ndc(mon_args, mon_info_index);
    }
  }
  //else if(!(strcmp(operation,"cm_update")))

  NSTL1_OUT(NULL, NULL, "Message send to UI %s", send_msg);
  return;
}

//nsi_rtc_invoker -t <tr no> -u cavisson -r admin -o 176 -m "o=cm_add_mon;m=1599657379855;f=<filepath>"
//nsi_rtc_invoker -t <tr no> -u cavisson -r admin -o 176 -m "o=cm_status;m=1599657379855"
void parse_sm_monitor_rtc_data(char *msg, char *send_msg)
{
  int len;
  int num_fields;
  char *tool_msg;
  char *data[30]; // assuming values will not exceed 30.

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  memcpy(&len, msg, 4);
  NSDL2_MESSAGES(NULL, NULL, "Method called, msg size = %d", len);
  memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
  tool_msg = msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
  strncpy(rtcdata->msg_buff, tool_msg, (len - 4));   //Message will be like "o=<operation>;m=<g_mon_id>;f=<filepath>;k=<key1,key2>"
   
  NSDL2_MESSAGES(NULL, NULL, "rtcdata->msg_buff = %s", rtcdata->msg_buff);
  
  num_fields = get_tokens(rtcdata->msg_buff, data, ";", 20);

  process_and_extract_rtc_data(data, num_fields, send_msg);
 
  return;
}
void parse_monitor_rtc_data(char *msg)
{
  int len;
  char *tool_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  
  memcpy(&len, msg, 4);
  NSDL2_MESSAGES(NULL, NULL, "Method called, msg size = %d", len);
  memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
  tool_msg = msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
  strncpy(rtcdata->msg_buff, tool_msg, (len - 4));

  NSDL2_MESSAGES(NULL, NULL, "rtcdata->msg_buff = %s", rtcdata->msg_buff);
  apply_runtime_changes(1);
}

static inline void set_pause_done_in_case_of_rtc_failure()
{
  NSTL1(NULL, NULL, "Method Called");
  global_settings->pause_done = 0;
}

/*
  Purpose :      This function parse quantity keyword and send APPLY_QUANTITY_RTC
                 to all clients
  return  :     -1, Parsing Error
                 1, Generator Delay 
                 0, Success
*/  
int parse_rtc_quantity_keyword1(char *tool_msg)
{
  char buff[MAX_DATA_LINE_LENGTH] = {0};
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[1024];
  char err_msg[MAX_DATA_LINE_LENGTH];
  char group[1024];
  int grp_idx = -1, num;
  int i, ret;
  char *next_ptr;
  char *msg = tool_msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
  int tool_msg_len = 0;
  Schedule *schedule;
  //Null terminate messgae at max len
  msg[(*((int *)tool_msg)) - 4] = '\0';

  NSDL2_RUNTIME(NULL, NULL, "Method Called, msg = %s", msg);
 
  runtime_id++;
  memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
  for(i = 0; i < sgrp_used_genrator_entries; i++)
    generator_entry[i].send_buff[0] = '\0'; 

  while(*msg != '\0')
  {
    if((next_ptr = strchr(msg, '\n')) != NULL)
    {
      *next_ptr = '\0';
      next_ptr++;
      if ((num = sscanf(msg, "%s %s %s", keyword, group, text)) < 2) {
        NSDL2_RUNTIME(NULL, NULL, "code return from there");
        RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Invalid quantity keyword, group is not provided")
        return -1;
      }
      NSDL2_RUNTIME(NULL, NULL, "msg = %s, keyword = %s", msg, keyword);
      if (strncasecmp(keyword, "RUNTIME_CHANGE_QUANTITY_SETTINGS", strlen("RUNTIME_CHANGE_QUANTITY_SETTINGS")) == 0) {
        tool_msg_len += snprintf(rtcdata->msg_buff + tool_msg_len, (RTC_QTY_BUFFER_SIZE - tool_msg_len), "%s\n", msg);
      } else if (strncasecmp(keyword, "QUANTITY", strlen("QUANTITY")) == 0) {
        grp_idx = find_grp_idx_from_kwd(msg, err_msg);
        //Bug 37401: Wrong group name handling for Netcloud
        if(grp_idx < 0) {
          if(grp_idx == NS_GRP_IS_ALL) 
            sprintf(err_msg, "Group cannot be ALL for QUANTITY keyword");
          RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES(err_msg)
          return -1;
        }
        if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
          schedule = scenario_schedule;
        else
          schedule = &group_schedule[grp_idx];
        if((schedule->phase_idx == (schedule->num_phases - 1)) &&
           (schedule->phase_array[schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN))
        {
          if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
          { 
            msg = next_ptr;
            continue;
          }
          RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Quantity RTC is not allowed on last phase of test");
          return -1;
        }
        tool_msg_len += snprintf(rtcdata->msg_buff + tool_msg_len, (RTC_QTY_BUFFER_SIZE - tool_msg_len), "%s\n", msg);
  
        //added code to make buffer in generator table for that prticular generator which is used in that group
        sprintf(buff, "%s:", group);
        //how to get grp_idx
        NSDL2_RUNTIME(NULL, NULL, "grp_idx = %d, num_generator_per_grp = %d", grp_idx, runprof_table_shr_mem[grp_idx].num_generator_per_grp);
        for(idx = 0; idx < runprof_table_shr_mem[grp_idx].num_generator_per_grp; idx++) 
        {
          NSDL3_MESSAGES(NULL, NULL,"idx = %d, grp_idx = %d, id = %d, flag = %d", idx, grp_idx, 
                                     runprof_table_shr_mem[grp_idx].running_gen_value[idx].id, 
                                     generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[idx].id].flags);
          if((generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[idx].id].flags) & (IS_GEN_ACTIVE))
          {
            strcat(generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[idx].id].send_buff, buff);
            NSDL2_RUNTIME(NULL, NULL, "send_buff = %s", generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[idx].id].send_buff);
          }
        }
      }else if (strncasecmp(keyword, "CONTROLLER_NAME", strlen("CONTROLLER_NAME")) == 0){
        NSTL1(NULL, NULL, "Ignoring 'CONTROLLER_NAME' keyword form RTC conf file"); 
      }
      else
      {
        NSTL1_OUT(NULL, NULL, "Error: Wrong Keyword '%s' found in RTC conf file", keyword);
        sprintf(err_msg, "Error: Wrong Keyword '%s' found in RTC conf file", keyword);
        //RUNTIME_UPDATION_FAILED(err_msg, 1)
        RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES(err_msg)
        return -1;
      }
      msg = next_ptr;
    }
  }
  SET_RTC_FLAG(RUNTIME_QUANTITY_FLAG);
  NSDL2_RUNTIME(NULL, NULL, "Complete rtcdata->msg_buff = %s", rtcdata->msg_buff);
 
  ret = send_qty_pause_msg_to_all_clients();
  if(ret)
    return ret;

  return 0;
}

int parse_qty_buff_and_distribute_on_gen()
{
  char *start_ptr = NULL;
  char *line = NULL;
  char qty_buf[RTC_QTY_BUFFER_SIZE] = {0};
  char err_msg[1024] = {0};
  int ret, runtime_idx = 0;
  int i;

  //for_quantity_rtc = 1;
  strcpy(qty_buf, rtcdata->msg_buff);
  //Resetting gen_keyword buffer.
  //TODO: do we need memset
  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    //memset(generator_entry[i].gen_keyword, 0, RTC_QTY_BUFFER_SIZE);
    generator_entry[i].gen_keyword[0] = '\0';
    generator_entry[i].msg_len = 0;
  }
  NSTL1(NULL, NULL, "Distribute quantity among generators and create keyword");
  NSDL2_RUNTIME(NULL, NULL, "Method called, In qty_buf = %s", qty_buf);

  line = qty_buf;
  while (*line != '\0')
  {
    if(!(start_ptr = strchr(line, '\n')))
    {
      NSDL2_RUNTIME(NULL, NULL, "In line = '%s' newline not found..", line);
      break;
    }

    *start_ptr = '\0';
    start_ptr++;
    if((strstr(line, "RUNTIME_CHANGE_QUANTITY_SETTINGS")))
    {
      NSDL2_RUNTIME(NULL, NULL, "RUNTIME_CHANGE_QUANTITY_SETTINGS keyword found");
      ret = kw_set_runtime_change_quantity_settings(line, err_msg);
      if(ret == RUNTIME_ERROR) {
        NSTL1_OUT(NULL, NULL, "Failed to parse keyword %s, error = \n%s", line, err_msg);
        RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Failed to parse keyword RUNTIME_CHANGE_QUANTITY_SETTINGS");
        return 1;
      }
      line = start_ptr;
      continue; 
    }
    ret = distribute_quantity_among_generators(line, err_msg, runtime_id, first_time, &runtime_idx);
    if(ret == RUNTIME_ERROR) {
      NSTL1_OUT(NULL, NULL, "ERROR: RTC - Unable to distribute quantity = %s", err_msg);
      RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES(err_msg);
      return 1;
    }
    line = start_ptr;
    nc_flag_set = 1;
    runtime_idx = 0;
  }
  return RUNTIME_SUCCESS;
}


int is_rtc_applied_for_dyn_objs()
{
  int i, j;
 
  NSDL2_RUNTIME(NULL, NULL, "Method called");
  for(i = 1; i < MAX_DYN_OBJS; i++)
  {
    NSDL2_RUNTIME(NULL, NULL, "i = %d, total = %d", i, dynObjForGdf[i].total);
    if(dynObjForGdf[i].total > 0)
    {
      //We will not process structure cleanup if any dynamic objects are involved. We are setting g_enable_new_gdf_on_partition_switch to 0 to make sure it won't happen. This function will be called for first time at 1st progress interval and check if there any dynamic objects involved, if there is we set g_enable_new_gdf_on_partition_switch to 0.
      //g_enable_new_gdf_on_partition_switch = 0;
      return 1;
    }
    if(loader_opcode == MASTER_LOADER)
    {
      if(i == NEW_OBJECT_DISCOVERY_STATUS_CODE){
        if(g_http_status_code_loc2norm_table && (total_http_resp_code_entries > 0)){ 
          for(j = 0; j < sgrp_used_genrator_entries; j++){
            if(g_http_status_code_loc2norm_table[j].dyn_total_entries) 
              return 1;
          }
        }
      }
      if(i == NEW_OBJECT_DISCOVERY_TX || i == NEW_OBJECT_DISCOVERY_TX_CUM){
        if(g_tx_loc2norm_table && (total_tx_entries > 0)){ // g_tx_loc2norm_table is malloced even if static tx count = 0 as there can be DynTx
          for(j = 0; j < sgrp_used_genrator_entries; j++){
            if(g_tx_loc2norm_table[j].dyn_total_entries[i-1]) 
              return 1;
          }
        }
      }
      if(i == NEW_OBJECT_DISCOVERY_SVR_IP){
        if(SHOW_SERVER_IP){
          for(j = 0; j < sgrp_used_genrator_entries; j++){
            if(g_svr_ip_loc2norm_table[j].dyn_total_entries > 0)
              return 1;
          }
        }
      }
    }
  }
  return 0;
}


void reset_dynamic_obj_structure()
{
  int i, j;
  
  for(i = 1; i < MAX_DYN_OBJS; i++)
  {
    dynObjForGdf[i].startId += dynObjForGdf[i].total;
    dynObjForGdf[i].total = 0;
    if(loader_opcode == MASTER_LOADER && dynObjForGdf[i].startId > 0)
    {
      if(i == NEW_OBJECT_DISCOVERY_TX || i == NEW_OBJECT_DISCOVERY_TX_CUM){
        if(g_tx_loc2norm_table && (total_tx_entries > 0)){
          for(j = 0; j < sgrp_used_genrator_entries; j++)
            g_tx_loc2norm_table[j].dyn_total_entries[i-1] = 0;
        }
      }
      else if(i == NEW_OBJECT_DISCOVERY_SVR_IP){
        if(SHOW_SERVER_IP){
          for(j = 0; j < sgrp_used_genrator_entries; j++)
            g_svr_ip_loc2norm_table[j].dyn_total_entries = 0;
        }
      } 
      else if(i == NEW_OBJECT_DISCOVERY_STATUS_CODE){
        for(j = 0; j < sgrp_used_genrator_entries; j++)
          g_http_status_code_loc2norm_table[j].dyn_total_entries = 0;
      } 
      else if(i == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){
        for(j = 0; j < sgrp_used_genrator_entries; j++)
          g_tcp_clinet_errs_loc2normtbl[j].tot_dyn_entries[i - 1] = 0;
      } 
    }  
  }
}

void handle_rtc_child_failed(int child_id)
{
  char time[0xff];
  NSDL1_RUNTIME(NULL, NULL, "Method called, child_id = %d", child_id);
  if(rtcdata == NULL)
    return;

  switch(rtcdata->opcode)
  {
    case RTC_PAUSE_DONE:
      if(CHECK_ALL_RTC_MSG_DONE)
      {
        if(rtcdata->cur_state == RESET_RTC_STATE)
        {
          NSTL1(NULL, NULL, "ERROR: RTC_PAUSE_DONE(145), rtc state = %d, which is always"
                            " be set in case of failure. This will not processed", rtcdata->cur_state);
          return;
        }
        rtcdata->cur_state = RESET_RTC_STATE;//Reset the runtime change state
        NSTL1(NULL, NULL, "Got all RTC_PAUSE_DONE messages from generators/child");
        if (loader_opcode == CLIENT_LOADER)
        {
          if(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG))
            process_rtc_qty_schedule_detail(rtcdata->opcode, rtcdata->msg_buff);
          else
            send_msg_to_master(master_fd, rtcdata->opcode, CONTROL_MODE);
        } 
        else //Controller and NS (standalone) parent 
        {
          if((loader_opcode == MASTER_LOADER) && CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG))
          {
            if((parse_qty_buff_and_distribute_on_gen()))
            {
              global_settings->pause_done = 0;
              rtcdata->cur_state = RTC_RESUME_STATE;
              send_msg_to_all_clients(RTC_RESUME, 0);
              return;
            }
          }
          NSDL3_MESSAGES(NULL, NULL, "Got all RTC_PAUSE_DONE messages from generators/child");
          NSTL1(NULL, NULL, "Applying runtime changes.");
          apply_runtime_changes(0);
        }
      }
      break;
    case NC_RTC_APPLIED_MESSAGE:
      if(CHECK_ALL_RTC_MSG_DONE)
      {
        if(CHECK_RTC_FLAG(RUNTIME_FAIL))
        {
          NSTL1_OUT(NULL, NULL, "Runtime changes not applied");
          NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, \
          "NA", "NA", \
          "Error in applying runtime changes");
          //RUNTIME_UPDATION_CLOSE_FILES
          RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Runtime changes not applied")
          RESET_RTC_FLAG(RUNTIME_FAIL);
        }
        else
        {
          RUNTIME_UPDATE_LOG("Runtime changes applied Successfully") 
          NS_EL_2_ATTR(EID_RUNTIME_CHANGES_OK, -1,
                                      -1, EVENT_CORE, EVENT_INFORMATION,
                                      "NA",
                                      "NA",
                                      "Runtime changes applied Successfully");
          RUNTIME_UPDATION_CLOSE_FILES;
          NSTL1(NULL, NULL, "Runtime changes applied Successfully");
          SET_RTC_FLAG(RUNTIME_PASS);
        }
        apply_resume_rtc(0);
      }
      break;
    case RTC_RESUME_DONE:
      if(rtcdata->cur_state == RESET_RTC_STATE) {
        NSTL1(NULL, NULL, "ERROR: RTC_RESUME_DONE(146): Here rtc state will be = %d, which is always "
                          "be set in case of failure. So Sending QUANTITY_RESUME_RTC(155) and returning", rtcdata->cur_state );
 
        return;
      }
      rtcdata->cur_state = RESET_RTC_STATE;//Reset the runtime change state
      NSTL1(NULL, NULL, "Got all RTC_RESUME_DONE messages from generators/child");
      if (loader_opcode == CLIENT_LOADER)
        send_msg_to_master(master_fd, rtcdata->opcode, CONTROL_MODE);
      int rtc_idx, rtc_flag;
      char phase_name[50];
      for(rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
      {
        NSDL3_MESSAGES(NULL, NULL, "rtc_idx = %d, rtc_state = %d", rtc_idx, runtime_schedule[rtc_idx].rtc_state);
        if(runtime_schedule[rtc_idx].rtc_state == RTC_NEED_TO_PROCESS)
        {
          convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
          sprintf(phase_name, "RTC_PHASE_%d", runtime_schedule[rtc_idx].rtc_id);
          NSDL3_MESSAGES(NULL, NULL, "***rtc_idx = %d, rtc_id = %d", rtc_idx, runtime_schedule[rtc_idx].rtc_id);
          runtime_schedule[rtc_idx].rtc_state = RTC_RUNNING;
          rtc_flag = 1;
        }
      }
      if(rtc_flag)
        log_phase_time(!PHASE_IS_COMPLETED, 6, phase_name, time);
 
      RUNTIME_UPDATION_RESET_FLAGS
      break;
    default:
      break;
  }
}

void process_alert_rtc(Msg_com_con *mccptr, char *msg)
{
  if(global_settings->pause_done)
  {
    NSTL1(NULL, NULL, "RTC cannot be applied as test run is already in schedule paused state.");
    SET_RTC_FLAG(RUNTIME_FAIL);
    return;
  }

  if((ns_parent_state == NS_PARENT_ST_INIT) || (ns_parent_state == NS_PARENT_ST_TEST_OVER))
  {
    flag_run_time_changes_called = 1;
    NSTL1(NULL, NULL, "RTC cannot be applied as test run is not running.");
    SET_RTC_FLAG(RUNTIME_FAIL);
    return;
  }

  rtcdata->epoll_start_time = get_ms_stamp();
 
  if(process_alert_server_config_rtc() < 0)
  {
    NSTL1(NULL, NULL, "RTC cannot be applied as alert is not configured.");
    SET_RTC_FLAG(RUNTIME_FAIL);
    return;
  }
  if (loader_opcode == MASTER_LOADER)
  {
    char buf[2048];
    sprintf(buf, "ENABLE_ALERT %s", g_alert_info);
    NSTL1(NULL, NULL, "(Master -> Generator) Sending keyword = %s", buf);
    send_nc_apply_rtc_msg_to_all_gen(NC_APPLY_RTC_MESSAGE, buf, 0, 0);
  }else
  {  
    //Send signal to all child/genrator
    send_rtc_msg_to_all_clients(APPLY_ALERT_RTC, NULL, 0);
    send_rtc_msg_to_invoker(mccptr, APPLY_ALERT_RTC, NULL, 0);
    RUNTIME_UPDATION_RESET_FLAGS
  }
  return;
}
