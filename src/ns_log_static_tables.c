/******************************************************************
 * Name    : ns_log_static_tables.c
 * Author  : Neeraj Jain
 * Purpose : To log all static table data in slog file. 
 *           Called from parent
 *           In case of URL, it any new url is found due to redirecrt/auto fetch
 *           then NVM also log this URL
 *
 * Following are the static tables:
 *   URL, Page, Transaction, Session
 *   TestCase RunProfile UserProfile SessionProfile ServerTable
 *
 * Modification History: Code splitted from logging.c on Dec 21, 11 (Release 3.8.1)
 *
 *
*****************************************************************/

//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE

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
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "logging.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_trans.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "ns_url_resp.h"
#include "ns_url_hash.h"
#include "ns_schedule_phases_parse.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "wait_forever.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nslb_get_norm_obj_id.h"

static inline char* forward_buffer(char* buf, int amt_written, int max_size, int* space_left) {

  NSDL2_LOGGING(NULL, NULL, "Method called. amt_written = %d, max_size = %d, space_left = %d", amt_written, max_size, (max_size - amt_written));

  NSDL4_LOGGING(NULL, NULL, "buf = %s", buf);

  if (amt_written > max_size) {
    fprintf(stderr, "forward_buffer: overwriting buffer\n");
    return NULL;
  }
  *space_left = max_size - amt_written;
  return buf + amt_written;
}


#if 0
comented code in rel 3.8.2
static void log_static_data_record(char *rec_type, char *rec_buf, int rec_len)
{
int bytes_written;

  NSDL2_LOGGING(NULL, NULL, "Method called. Rec_type = %s, Rec_len = %d, Record = %s", rec_type, rec_len, rec_buf);

  if ((bytes_written = (write(static_logging_fd, rec_buf, rec_len))) != rec_len) {
    fprintf(stderr, "Error in writing to static data record in static logging file. Record type = %s. Record = %s\n", rec_type, rec_buf);
    exit(-1);
  }
}

int log_test_case(int test_idx, const TestCaseType_Shr* test_case,
                  const Global_data* gdata, GroupSettings *global_gset,
                  const SLATableEntry_Shr* sla_table, const MetricTableEntry_Shr* metric_table,
                  const ThinkProfTableEntry_Shr* think_table,
                  u_ns_ts_t start, u_ns_ts_t end) {

  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written = 0;   /* amount of bytes written NOT include trailing NULL */
  int space_left = LOGGING_SIZE; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;
  char* sla_type_string_conv[7] = {"ALL", "URL", "PAGE", "TX_WITH_WAIT", "TX_WO_WAIT", "SESS_WITH_WAIT", "SESS_WO_WAIT"};
  //char* sla_prop_string_conv[8] = {"AVG", "MEAN", "MIN", "MAX", "80%", "90%", "99%", "FAILURE"};
  char* metric_string_conv[5] = {"CPU", "PORT", "RUN_QUEUES", "LESS_THAN", "GREATER_THAN"};
  int i;
 
  NSDL2_LOGGING(NULL, NULL, "Method called, test_idx = %d, start = %u, end = %u", test_idx, start, end);
  write_location = logging_buffer;

  //Added new line for TESTCASE
  //In reader mode 2, reader was not able to read last line from slog file
  //this is because there was no new line at the end of line and file was not closed
  amt_written += snprintf(write_location, space_left+1, "%s%d%c%s%c", "TESTCASE:", test_idx, DELIMINATOR, g_testrun, DELIMINATOR);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  switch (test_case->mode) {
  case TC_FIX_CONCURRENT_USERS:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "FIX CONCURRENT USERS", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_USER_RATE:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "FIX USER RATE", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_HIT_RATE:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "FIX HIT RATE", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_PAGE_RATE:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "FIX PAGE RATE", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_TX_RATE:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "FIX TRANSACTION RATE", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_MEET_SLA:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "MEET SLA", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_MEET_SERVER_LOAD:
    amt_written += snprintf(write_location, space_left+1, "%s%c", "MEET SERVER LOAD", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  }

  amt_written += snprintf(write_location, space_left+1, "%d%c", gdata->wan_env, DELIMINATOR);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  switch (test_case->mode) {
  case TC_FIX_USER_RATE:
    amt_written += snprintf(write_location, space_left+1, "%d%c", gdata->vuser_rpm, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_CONCURRENT_USERS:
  case TC_FIX_MEAN_USERS:
    amt_written += snprintf(write_location, space_left+1, "%d%c", gdata->num_connections, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_PAGE_RATE:
  case TC_FIX_HIT_RATE:
  case TC_FIX_TX_RATE:
    amt_written += snprintf(write_location, space_left+1, "%d%c", test_case->target_rate, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_MEET_SERVER_LOAD:
  case TC_MEET_SLA:
    amt_written += snprintf(write_location, space_left+1, "%d%c", 0, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  }
  int avg_time, median_time, var_time;
  short mode;
  // If no url in script think_table ma be NULL
  if(think_table)
  {
    avg_time = think_table[GLOBAL_THINK_IDX].avg_time;
    median_time = think_table[GLOBAL_THINK_IDX].median_time;
    var_time = think_table[GLOBAL_THINK_IDX].var_time;
    mode = think_table[GLOBAL_THINK_IDX].mode;
  }
  else
  {
    avg_time = 0;
    median_time = 0;
    var_time = 0;
    mode = 0;
  }

  amt_written += snprintf(write_location, space_left+1, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c%s%c%d%c",
			  gdata->num_process, DELIMINATOR,
			  gdata->ramp_up_rate, DELIMINATOR,
			  gdata->progress_secs, DELIMINATOR,
			  gdata->test_stab_time, DELIMINATOR,
			  global_gset->idle_secs, DELIMINATOR,
			  gdata->ssl_pct, DELIMINATOR,
			  group_default_settings->ka_pct, DELIMINATOR,
			  (group_default_settings->num_ka_min + group_default_settings->num_ka_range/2), 
                          DELIMINATOR,
			  avg_time, DELIMINATOR,
			  median_time, DELIMINATOR,
			  var_time, DELIMINATOR,
			  mode, DELIMINATOR,
			  gdata->user_reuse_mode, DELIMINATOR,
			  gdata->user_rate_mode, DELIMINATOR,
			  gdata->ramp_up_mode, DELIMINATOR,
			  group_default_settings->user_cleanup_time, DELIMINATOR,
			  gdata->max_con_per_vuser, DELIMINATOR,
			  g_url_file, DELIMINATOR, //Eralier, it used to be session recording name. Now -
                          0, DELIMINATOR
			  //,gdata->health_monitor_on, DELIMINATOR
			  );

  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;
  switch(test_case->mode) {
  case TC_FIX_HIT_RATE:
  case TC_FIX_PAGE_RATE:
  case TC_FIX_TX_RATE:
  case TC_MEET_SLA:
  case TC_MEET_SERVER_LOAD:
  case TC_FIX_MEAN_USERS:
    amt_written += snprintf(write_location, space_left+1, "%d%c%d%c%d%c%d%c%d%c",
			    test_case->guess_num, DELIMINATOR,
			    test_case->guess_prob, DELIMINATOR,
			    test_case->stab_num_success, DELIMINATOR,
			    test_case->stab_max_run, DELIMINATOR,
			    test_case->stab_run_time, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_FIX_CONCURRENT_USERS:
  case TC_FIX_USER_RATE:
    amt_written += snprintf(write_location, space_left+1, "%d%c%d%c%d%c%d%c%d%c",
			    0, DELIMINATOR,
			    0, DELIMINATOR,
			    0, DELIMINATOR,
			    0, DELIMINATOR,
			    0, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  }

  switch (test_case->mode) {
  case TC_FIX_CONCURRENT_USERS:
  case TC_FIX_HIT_RATE:
  case TC_FIX_PAGE_RATE:
  case TC_FIX_TX_RATE:
  case TC_FIX_MEAN_USERS:
  case TC_FIX_USER_RATE:
    amt_written += snprintf(write_location, space_left+1, "%d%c", 0, DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_MEET_SLA:
    if (sla_table_shr_mem) {
      for (i = 0; i < total_sla_entries; i++) {
        // wht do we pass here now for SLA: Anuj
	amt_written += snprintf(write_location, space_left+1, "%s %d %d %d %d %d %d %.3f %.3f %d %d | ", 
                                sla_type_string_conv[sla_table_shr_mem->user_id], 
                                sla_table_shr_mem->gdf_rpt_gp_num_idx,
                                sla_table_shr_mem->gdf_rpt_graph_num_idx,
                                (int)(sla_table_shr_mem->vector_option),
                                sla_table_shr_mem->gdf_group_vector_idx,
                                sla_table_shr_mem->gdf_graph_vector_idx,
                                sla_table_shr_mem->relation,
                                sla_table_shr_mem->value,
                                sla_table_shr_mem->pct_variation,
                                sla_table_shr_mem->gdf_group_num_vectors,
                                sla_table_shr_mem->gdf_graph_num_vectors);

	if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
	  return -1;
      }
    }
    amt_written += snprintf(write_location, space_left+1, "%c", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  case TC_MEET_SERVER_LOAD:
    if (metric_table_shr_mem) {
      for (i = 0; i < total_metric_entries; i++) {
	amt_written += snprintf(write_location, space_left+1, "%s %d %s %d %d | ", metric_string_conv[metric_table_shr_mem->name], metric_table_shr_mem->port,
			       metric_string_conv[metric_table_shr_mem->relation], metric_table_shr_mem->target_value, metric_table_shr_mem->min_samples);
	if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
	  return -1;
      }
    }
    amt_written += snprintf(write_location, space_left+1, "%c", DELIMINATOR);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;
    break;
  }

  amt_written += snprintf(write_location, space_left+1, "%u%c%u\n", start, DELIMINATOR, end);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  log_static_data_record("TestCaseRecord", logging_buffer, amt_written);

  return 0;
}

#endif

#ifndef CAV_MAIN
NormObjKey ParentnormRunProfTable;
NormObjKey ParentnormGroupRunProfTable;
#else
__thread NormObjKey ParentnormRunProfTable;
__thread NormObjKey ParentnormGroupRunProfTable;
#endif

int log_run_profile(const TestCaseType_Shr* test_case) {
  char logging_buffer[LOGGING_SIZE + 1];
  char combined_buf[LOGGING_SIZE + 1];
  RunProfTableEntry_Shr* runprof_ptr = runprof_table_shr_mem;
  int i;
  int old_quantity = 0;

  NSDL2_LOGGING(NULL, NULL, "Method called. Run profile (sgrp) shr mem start address = %p (as lu = %lu). Size of shared memory record = %d", runprof_table_shr_mem, runprof_table_shr_mem, sizeof(RunProfTableEntry_Shr));

  int userprofidx = 0;//TODO: making it zero . Need to check how to get it
  int len = 0;
  int norm_id, grp_norm_id;
  int is_new;

  for (i = 0; i < total_runprof_entries; i++, runprof_ptr++) {
    
    NSDL4_LOGGING(NULL, NULL, "group_num = %d, runprof_ptr->sess_ptr->sess_id = %lu, quantity = %d", runprof_ptr->group_num, runprof_ptr->sess_ptr->sess_id, runprof_ptr->quantity);

    grp_norm_id = nslb_get_or_gen_norm_id(&ParentnormGroupRunProfTable, runprof_ptr->scen_group_name, strlen(runprof_ptr->scen_group_name), &is_new);
    //Fill group's norm id into runprof table
    runprof_ptr->grp_norm_id = grp_norm_id;

    //First check if need to write into csv. Check with combitination of group:userprof:session
    //If already there then no need to write into csv
   
    len = sprintf(combined_buf, "%s:%s:%s", runprof_ptr->scen_group_name, "Internet", get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runprof_ptr->sess_ptr->sess_id].sess_name), runprof_ptr->sess_ptr->sess_id, "/"));
    norm_id = nslb_get_or_gen_norm_id(&ParentnormRunProfTable, combined_buf, len, &is_new);

    if(is_new)
    {
      //New entry get norm id of group and write into csv file
      len = sprintf(logging_buffer, "%d,%d,%d,%d,%hd,%s,%d,%s\n", testidx, grp_norm_id, userprofidx, gSessionTable[runprof_ptr->sess_ptr->sess_id].sess_norm_id, runprof_ptr->quantity - old_quantity, runprof_ptr->scen_group_name, norm_id, combined_buf);
      write_buffer_into_csv_file("rpf.csv", logging_buffer, len);
    }

    old_quantity = runprof_ptr->quantity;
  }

  return 0;
}

static int log_user_profile_attribute(int userprof_id, char *userprof_name, char *attribute_type, int attribute_idx, char *attribute_value, int pct)
{
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;
  int space_left;
  // int max_size = LOGGING_SIZE;

  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;

  // USERPROFILE:userprof_id,userprof_name,attr_type,attr_id,attr_value,pct
  // userprof_id:
  // userprof_name:
  // attr_type: LOCATION or ACCESS OR BROWSER
  // attr_id: Index in attribute table
  // attr_value: Attribute value (e.g. NewDelhi, DSL, IE)
  // pct:
  amt_written = snprintf(write_location, space_left+1, "%s%d%c%s%c%s%c%d%c%s%c%d\n", "USERPROFILE:",
                         userprof_id, DELIMINATOR,//For writing the index in slog 
                         userprof_name, DELIMINATOR,
                         attribute_type, DELIMINATOR,
                         attribute_idx, DELIMINATOR,  //For writing the index in slog for population table in nsu_logging_reader
                         attribute_value, DELIMINATOR,
                         pct);

  // if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    // return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
  return 0;
}

int log_user_profile(const UserIndexTableEntry* userindextable) {
  UserProfTableEntry *userprof_ptr;
  AccLocTableEntry* accloc_ptr;
  int i, j;
  int total_locacc_pct;

  NSDL2_LOGGING(NULL, NULL, "Method called");

  for (i = 0; i < total_userindex_entries; i++, userindextable++) 
  {
    total_locacc_pct = 0;
    
    // Co-Located User Location/Access
    accloc_ptr = &accLocTable[userindextable->UPAccLoc_start_idx];
    for (j = 0; j < userindextable->UPAccLoc_length; j++, accloc_ptr++) 
    {

      log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "LOCATION", accloc_ptr->location, RETRIEVE_BUFFER_DATA(locAttrTable[accloc_ptr->location].name), accloc_ptr->pct);

      log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "ACCESS", accloc_ptr->access, RETRIEVE_BUFFER_DATA(accAttrTable[accloc_ptr->access].name), accloc_ptr->pct);

      total_locacc_pct += accloc_ptr->pct;
    }
    // End of co located user location/accesses

    if (total_locacc_pct < 100) 
    {
      userprof_ptr = &userProfTable[userindextable->UPLoc_start_idx];

      for (j = 0; j < userindextable->UPLoc_length; j++, userprof_ptr++) 
      {
        log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "LOCATION", userprof_ptr->attribute_idx, RETRIEVE_BUFFER_DATA(locAttrTable[userprof_ptr->attribute_idx].name), ((100 - total_locacc_pct) * userprof_ptr->pct)/100);
      }

      userprof_ptr = &userProfTable[userindextable->UPAcc_start_idx];
      for (j = 0; j < userindextable->UPAcc_length; j++, userprof_ptr++) {
        log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "ACCESS", userprof_ptr->attribute_idx, RETRIEVE_BUFFER_DATA(accAttrTable[userprof_ptr->attribute_idx].name), ((100 - total_locacc_pct) * userprof_ptr->pct)/100);
      }
    }

    userprof_ptr = &userProfTable[userindextable->UPBrow_start_idx];
    for (j = 0; j < userindextable->UPBrow_length; j++, userprof_ptr++) {
      log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "BROWSER", userprof_ptr->attribute_idx, RETRIEVE_BUFFER_DATA(browAttrTable[userprof_ptr->attribute_idx].name), userprof_ptr->pct);
    }

   /* userprof_ptr = &userProfTable[userindextable->UPFreq_start_idx];
    for (j = 0; j < userindextable->UPFreq_length; j++, userprof_ptr++) {
      log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "FREQUENCY", userprof_ptr->attribute_idx, RETRIEVE_BUFFER_DATA(freqAttrTable[userprof_ptr->attribute_idx].name), userprof_ptr->pct);
    }

    userprof_ptr = &userProfTable[userindextable->UPMach_start_idx];
    for (j = 0; j < userindextable->UPMach_length; j++, userprof_ptr++) {
      log_user_profile_attribute(i, RETRIEVE_BUFFER_DATA(userindextable->name), "MACHINE", userprof_ptr->attribute_idx, RETRIEVE_BUFFER_DATA(machAttrTable[userprof_ptr->attribute_idx].name), userprof_ptr->pct);
    }*/
  }

  return 0;
}

int log_session_profile(const SessProfIndexTableEntry_Shr* sessprofindex_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;
  int space_left;
  int max_size = LOGGING_SIZE;
  //  RunProfTableEntry_Shr* runprof_ptr = test_case->runindex_ptr->runprof_start;
  SessProfTableEntry_Shr* sessprof_ptr;
  int i, j;

  NSDL2_LOGGING(NULL, NULL, "Method called");
  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;

#if 0
  amt_written = snprintf(write_location, space_left+1, "%s%lu\n", "SESSPROFSTART:", (u_ns_ptr_t) sessprofindex_table);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
#endif

  for (i = 0; i < total_sessprofindex_entries; i++, sessprofindex_table++) {
    for (j = 0, sessprof_ptr = sessprofindex_table->sessprof_start; j < sessprofindex_table->length; j++, sessprof_ptr++) {
      memset(logging_buffer, 0, LOGGING_SIZE + 1);
      write_location = logging_buffer;
      space_left = LOGGING_SIZE;
      
      // SESSIONPROFILE:sessprof_id,sess_id,sessprof_name,pct
      // sessprof_id:
      // sess_id:
      // sessprof_name:
      // pct:
      // NOT CLEAR - Need to review again
      amt_written = snprintf(write_location, space_left+1, "%s%i%c%d%c%s%c%hd\n", "SESSIONPROFILE:",
			     //(u_ns_ptr_t) sessprofindex_table, DELIMINATOR,
			     i, DELIMINATOR,
			     //(u_ns_ptr_t) sessprof_ptr->session_ptr, DELIMINATOR,
			     j, DELIMINATOR,
			     sessprofindex_table->name, DELIMINATOR,
			     sessprof_ptr->pct);
      if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
	return -1;

      if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
	fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
	return -1;
      }
    }
  }

  return 0;
}

#if 0
int log_session_table(const SessTableEntry_Shr* sess_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;
  int i;

  NSDL2_LOGGING(NULL, NULL, "Method called total_sess_entries=%d", total_sess_entries);
  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;

#if 0
  amt_written = snprintf(write_location, space_left+1, "%s%lu\n", "SESSTABLESTART:", (u_ns_ptr_t) sess_table);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
#endif

  for (i = 0; i < total_sess_entries; i++, sess_table++) {
    memset(logging_buffer, 0, LOGGING_SIZE + 1);
    write_location = logging_buffer;
    space_left = LOGGING_SIZE;
    amt_written = snprintf(write_location, space_left+1, "%s%u%c%s\n", "SESSIONTABLE:",
// TODO
			   // (u_ns_ptr_t) sess_table, DELIMINATOR,
			   sess_table->sess_id, DELIMINATOR,
			   sess_table->sess_name);

    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;

    if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
      fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
      return -1;
    }
  }
  return 0;
}


int log_page_table(const SessTableEntry_Shr* session_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;
  int i, j;
  PageTableEntry_Shr* page_ptr;

  NSDL2_LOGGING(NULL, NULL, "Method called");
  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;

#if 0
  amt_written = snprintf(write_location, space_left+1, "%s%lu\n", "PAGETABLESTART:", (u_ns_ptr_t) page_table_shr_mem);
  if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
    return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
#endif

  for (i = 0; i < total_sess_entries; i++, session_table++) {
    page_ptr = session_table->first_page;
    for (j = 0; j < session_table->num_pages; j++, page_ptr++) {
      memset(logging_buffer, 0, LOGGING_SIZE + 1);
      write_location = logging_buffer;
      space_left = LOGGING_SIZE;
      amt_written = snprintf(write_location, space_left+1, "%s%u%c%u%c%s%c%s\n", "PAGETABLE:",
//			     (u_ns_ptr_t) page_ptr, DELIMINATOR,
//			     (u_ns_ptr_t) session_table, DELIMINATOR,
			     page_ptr->page_id, DELIMINATOR,
			     session_table->sess_id, DELIMINATOR,
			     page_ptr->page_name, DELIMINATOR, session_table->sess_name);

      if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
	return -1;

      if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
	fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
	return -1;
      }
    }
  }
  return 0;
}
#endif

#if 0
int log_tx_table(const TxTableEntry_Shr* tx_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;
  int i;

  NSDL2_LOGGING(NULL, NULL, "Method called");

  for (i = 0; i < total_tx_entries; i++, tx_table++) {
    memset(logging_buffer, 0, LOGGING_SIZE + 1);
    write_location = logging_buffer;
    space_left = LOGGING_SIZE;
    amt_written = snprintf(write_location, space_left+1, "%s%lu%c%s\n", "TXTABLE:",
			   (u_ns_ptr_t) tx_table->tx_hash_idx, DELIMINATOR,
			   tx_table->name);

    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;

    if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
      fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
      return -1;
    }
  }
  return 0;
}
#endif

int log_tx_table_record_v2(char *tx_name, int tx_len, unsigned int tx_index, int nvm_id)
{
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */

  NSDL2_LOGGING(NULL, NULL, "Method called");

  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;
  amt_written = snprintf(write_location, space_left+1, "%s%u%c%d%c%d%c%d%c%s\n", "TXTABLEV2:",
      		   tx_index, DELIMINATOR, 
      		   nvm_id, DELIMINATOR, 
      		   ((g_generator_idx == -1)?0:g_generator_idx), DELIMINATOR,
                         tx_len, DELIMINATOR,
                         tx_name);


  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
    return -1;
  }
  return 0;
}

/*
Added By Nikita
Date: Mon Apr  2 10:43:32 IST 2012
 
This function will create ect.csv file in TRxxx. This file will contain error code in following formate..
obj_type,err_code,err_msg
ex:
0,0,Success
0,1,MiscErr
0,2,1xx
*/
int log_error_code()
{
  char cmd_buf[1024];
  sprintf(cmd_buf, "bin/nsi_create_error_codes_csv %s", global_settings->tr_or_common_files);
  if(system(cmd_buf) != 0)
  {
    printf("commad 'cmd_buf' not execute successfully.");
    return -1; 
  }
  return 0; //Success
}

//int log_url(char* write_location, char *logging_buffer,  int space_left, char *url_str, action_request_Shr* action_ptr, PageTableEntry_Shr* page_ptr, int max_size)
//Make name better

/*Changes function name from replace_comma_and_single_quote_with_underscore to replace_special_characters_with_underscore.
 *Replacing backslash(\) with underscore because '\' creates problem while importing data from csv to database.
 *Issue is:
 *        In DBTable, '\' gets changed to '\x0C' which further gets changed to '^L' when we query and store data in url_ids.dat file. */
inline void replace_special_characters_with_underscore(char *str, int skip_count) 
{
  char *ptr;
  int char_cnt = 0;

  NSDL3_LOGGING(NULL, NULL, "Before Replacing comma in str=%s,  skip_count=%d", str, skip_count);
  ptr = str;

  while(*ptr != '\0')
  {
    if(*ptr == ',' || *ptr == '\'' || *ptr == '\\' || *ptr == '|')
    {
      char_cnt++;
      if(char_cnt > skip_count)
        *ptr = '_';
    }
    ptr++;
  }
  NSDL3_LOGGING(NULL, NULL, "After Replacing comma in str=%s", str);

}
// In database the length of url name is 4096, and in NS we are support any length of urlname,
// Now NS will truncate the url name if exceed.
#define MAX_URL_LEN_IN_DB_TABLE 4096

// Nikita Added comments below
// URL's are sorted by qsort on the basis of hostname, in urt.csv wr are getting sorted url.
// For refence please see the arrange_page_urls function in  url.c file
int log_url(char *url_str, int len, unsigned int url_index, unsigned int page_id, unsigned int url_hash_index, unsigned int url_hash_code, char *page_name)
{
  char logging_buffer[LOGGING_SIZE + 1];
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;

  NSDL3_LOGGING(NULL, NULL, "Writing url into slog, url_index=%u, url=%*.*s", url_index, 0, 50, url_str);
  space_left = LOGGING_SIZE;
  memset(logging_buffer, 0, LOGGING_SIZE + 1);

  // Format:
  //   URLTABLE:TestRun, UrlIndex, PageIndex, UrlHashIndex, UrlHashCode, UrlLen, UrlName

  if(len > MAX_URL_LEN_IN_DB_TABLE)
  len = MAX_URL_LEN_IN_DB_TABLE;
  // Note -> URL to be logged can be smaller than complete URL. So Must used *.* when logging URL
  amt_written = snprintf(logging_buffer, space_left+1, "%s%u%c%u%c%u%c%d%c%s%c%d%c%*.*s\n", "URLTABLE:",
                               url_index, DELIMINATOR,
                               page_id, DELIMINATOR,
                               url_hash_index, DELIMINATOR,
                               ((g_generator_idx == -1)?0:g_generator_idx), DELIMINATOR,
                               page_name, DELIMINATOR,
                               len, DELIMINATOR,
                               len, len, url_str);

  //Replacing comma by underscore(_) in url so as to avoid any conflict in csv files
  replace_special_characters_with_underscore(logging_buffer + 9, 6);
  
  if ((forward_buffer(logging_buffer, amt_written, max_size, &space_left)) == NULL)
  return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    //fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__); /*bug 78764 avoid printing unwanted  trace*/
    NSDL3_LOGGING(NULL, NULL, "error in writing to logging file"); /*bug 78764 - put trace in debug.log*/
    return -1;
  }
  return 0;
} 

static inline char* get_url_str(http_request_Shr* url_ptr, int *param_flag, const SessTableEntry_Shr* sess_table) {
  static char url_buf[URL_BUFFER_SIZE];
  SegTableEntry_Shr * seg_table_ptr = url_ptr->url.seg_start;
  int written;
  int left = URL_BUFFER_SIZE;
  int i;
  char* url_buf_ptr = url_buf;
  int idx;
  VarTableEntry_Shr *fparam_var = NULL; 

  url_buf[0]='\0';
  NSDL2_HTTP(NULL, NULL, "Method called. Number of URL entries = %d, param_flag =  %d", url_ptr->url.num_entries, *param_flag);

  for (i = 0; i < url_ptr->url.num_entries; i++, seg_table_ptr++) {
    switch (seg_table_ptr->type) {
    case STR: //no param
      NSDL2_HTTP(NULL, NULL, "STR: seg_table_ptr->type = %d", seg_table_ptr->type);
      if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.str_ptr->big_buf_pointer, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      break;
     case TAG_VAR:
     case SEARCH_VAR:
     case NSL_VAR:
      idx = sess_table->vars_rev_trans_table_shr_mem[seg_table_ptr->seg_ptr.var_idx];
      NSDL2_HTTP(NULL, NULL, "SEARCH VAR: seg_table_ptr->type = %d, seg_table_ptr->seg_ptr.var_idx = %d, sess_table->var_get_key(seg_table_ptr->seg_ptr.var_idx) = %s", seg_table_ptr->type, idx, sess_table->var_get_key(idx));
      if ((written = ns_strncpy(url_buf_ptr, ((char *)sess_table->var_get_key(idx)), left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    case VAR:
      NSDL2_HTTP(NULL, NULL, "VAR: seg_table_ptr->type = %d", seg_table_ptr->type);
      fparam_var = get_fparam_var(NULL, sess_table->sess_id, seg_table_ptr->seg_ptr.fparam_hash_code);
      //if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.var_ptr->name_pointer, left)) == -1) {
      NSDL2_HTTP(NULL, NULL, "name_pointer = %s", fparam_var->name_pointer);
      if ((written = ns_strncpy(url_buf_ptr, fparam_var->name_pointer, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    case RANDOM_VAR:
      NSDL2_HTTP(NULL, NULL, "RANDOM VAR: seg_table_ptr->type = %d", seg_table_ptr->type);
      if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.random_ptr->var_name, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    case RANDOM_STRING:
      NSDL2_HTTP(NULL, NULL, "RANDOM STRING VAR: seg_table_ptr->type = %d", seg_table_ptr->type);
      if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.random_str->var_name, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    case UNIQUE_VAR:
      NSDL2_HTTP(NULL, NULL, "UNIQUE VAR: seg_table_ptr->type = %d", seg_table_ptr->type);
      if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.unique_ptr->var_name, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    case DATE_VAR:
      NSDL2_HTTP(NULL, NULL, "DATE VAR: seg_table_ptr->type = %d", seg_table_ptr->type);
      if ((written = ns_strncpy(url_buf_ptr, seg_table_ptr->seg_ptr.date_ptr->var_name, left)) == -1) {
        fprintf(stderr, "get_url_str(): url_buf is too small\n");
        return NULL;
      }
      url_buf_ptr += written;
      left -= written;
      *param_flag = 1; //Param is used
      break;
    default:
      NSDL2_HTTP(NULL, NULL, "Used parameter type %d is going in default case.",seg_table_ptr->type);
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                __FILE__, (char*)__FUNCTION__,
      "Used parameter type %d is going in default case."
      "It should not happened.", seg_table_ptr->type);

      *param_flag = 1; //Param is used
      break;
    }
  }
  NSDL3_HTTP(NULL, NULL, "Returning URL buffer %s", url_buf);

  return url_buf;
}

int MaxStaticUrlIds = 0;
int log_url_table(const SessTableEntry_Shr* sess_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  //int space_left; /* amt bytes left NOT including trailing NULL */
  //int amt_written = 0;   /* amount of bytes written NOT include trailing NULL */
  int i,j,k;
  //int max_size = LOGGING_SIZE;
  PageTableEntry_Shr* page_ptr;
  action_request_Shr* action_ptr;
  http_request_Shr* url_ptr;
  char* url_str;
  int url_cnt;
#ifndef RMI_MODE
  //char* newline_ptr;
#else
  int url_num = 0;
  char url_buf[LOGGING_SIZE];
#endif
  int param_flag = 0;

  NSDL2_LOGGING(NULL, NULL, "Method called. Url shr mem start address = %p (as lu = %lu). Size of shared memory record = %d, "
                            "total_sess_entries = %d",
                             request_table_shr_mem, request_table_shr_mem, sizeof(action_request_Shr),
                             total_sess_entries);

  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  //space_left = LOGGING_SIZE;

  // It will write in slog like
  // URLTABLESTART:3264512

#if 0
  amt_written = snprintf(logging_buffer, space_left+1, "%s%lu\n", "URLTABLESTART:", (u_ns_ptr_t) request_table_shr_mem);
  if ((forward_buffer(logging_buffer, amt_written, max_size, &space_left)) == NULL)
    return -1;

  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
    fprintf(stderr, "%s: error in writing to logging file\n",  (char*)__FUNCTION__);
    return -1;
  }
#endif

  for (i = 0; i < total_sess_entries; i++, sess_table++) {
    page_ptr = sess_table->first_page;
    for ( j = 0; j < sess_table->num_pages; j++, page_ptr++) {
      action_ptr = page_ptr->first_eurl;

      /* bypass for for other protocols type */
      if (action_ptr->request_type != HTTP_REQUEST && 
          action_ptr->request_type != HTTPS_REQUEST) continue;

      for (k = 0; k < page_ptr->num_eurls; k++, action_ptr++) {
        url_ptr = &(action_ptr->proto.http);
	memset(logging_buffer, 0, LOGGING_SIZE + 1);
	//space_left = LOGGING_SIZE;
#ifdef RMI_MODE
	sprintf(url_buf, "Url%d", url_num++);
	url_str = url_buf;
#else
  param_flag = 0;
	url_str = get_url_str(url_ptr, &param_flag, sess_table); // Get Url has only url line
  NSDL2_LOGGING(NULL, NULL, "URL is %s", url_str);
  if(!url_str)
  {
    fprintf(stderr, "log_url_table: Error: Empty URL.\n");
    return -1;
  }
#endif
        NSDL4_LOGGING(NULL, NULL, "action_ptr = %p, page_ptr = %p, url = %s, url_index=%d", action_ptr, page_ptr, url_str, action_ptr->proto.http.url_index);

        //If url is using param and mode is to treat paramterized URL as dyn URL

        NSDL4_LOGGING(NULL, NULL, "param_flag = %d, global_settings->static_parm_url_as_dyn_url_mode = %d, NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED = %d",param_flag, global_settings->static_parm_url_as_dyn_url_mode, NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED); 
        if((param_flag == 1) && (global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED)) 
        {
          action_ptr->proto.http.url_index = -1;
        }
        else
        {
          // For static URL, we need to pass 0 for hash value and hash index
          if (log_url(url_str, strlen(url_str), action_ptr->proto.http.url_index, page_ptr->page_id, 0 , 0, page_ptr->page_name) == -1)
            return -1;
        }
        NSDL4_LOGGING(NULL, NULL, "action_ptr->proto.http.url_index = %d \n", action_ptr->proto.http.url_index);
        url_cnt = action_ptr->proto.http.url_index;
        MaxStaticUrlIds = url_cnt;
     }
    }
  }

  if(global_settings->dynamic_url_hash_mode == NS_DYNAMIC_URL_HASH_DISABLED)
  {
    // Add dynanic url so that we can this url id for all dyanmic url in case of url hash table is disabled
    // Since there is no page associated with this, we are using page id of 0 
    // For dummy dynamic URL, we need to pass 0 for hash value and hash index
    if (log_url("Dynamic_Url", strlen("Dynamic_Url"), ++url_cnt, 0, 0, 0, "NA") == -1)  return -1;

    global_settings->url_hash.dummy_dynamic_url_index = url_cnt;  //Storing Dynamic url index in dynamic_url_table_size in case url_hash is disabled
    MaxStaticUrlIds = url_cnt;
  }

  return 0;
}


int log_svr_tables(const SvrTableEntry_Shr* svr_table) {
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;
  int g_svr_idx, grp_idx, s;
  PerGrpHostTableEntry_Shr *grp_host_ptr;

  NSDL2_LOGGING(NULL, NULL, "Method called");
  memset(logging_buffer, 0, LOGGING_SIZE + 1);
  write_location = logging_buffer;
  space_left = LOGGING_SIZE;

  for(g_svr_idx=0; g_svr_idx < total_svr_entries; g_svr_idx++)	//loop for script
  {
    memset(logging_buffer, 0, LOGGING_SIZE + 1);
    write_location = logging_buffer;
    space_left = LOGGING_SIZE;

    //Dump data in slog file for the entries of RECSVRTABLE
    amt_written = snprintf(write_location, space_left+1, "%s%d%c%d%c%s%c%hd%c%s%c%d\n", "RECSVRTABLE:",g_svr_idx, 
                           DELIMINATOR,g_svr_idx, DELIMINATOR,
                           gserver_table_shr_mem[g_svr_idx].server_hostname, DELIMINATOR, 
                           gserver_table_shr_mem[g_svr_idx].server_port, DELIMINATOR,
                           (gserver_table_shr_mem[g_svr_idx].type==SERVER_ANY)?"0":"1",DELIMINATOR, 0);

    NSDL2_LOGGING(NULL, NULL, "write_location = %s", write_location);
    if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
      return -1;

    if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
      fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
      return -1;
    }
    
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
    {
      FIND_GRP_HOST_ENTRY_SHR(grp_idx, g_svr_idx, grp_host_ptr);
      if(!grp_host_ptr || grp_host_ptr->grp_dynamic_host)
        continue;
      
      PerHostSvrTableEntry *svr_table_ptr = grp_host_ptr->server_table;
      for(s = 0; s < grp_host_ptr->total_act_svr_entries; s++,svr_table_ptr++) 
      {
        memset(logging_buffer, 0, LOGGING_SIZE + 1); 
        write_location = logging_buffer;
        space_left = LOGGING_SIZE;
        int user_idx = svr_table_ptr->loc_idx; 
        NSDL2_LOGGING(NULL, NULL, "user_idx = %d", user_idx);
        //Dump data in slog file for the entries of ACTSVRTABLE
        amt_written = snprintf(write_location, space_left+1, "%s%d%c%d%c%s%c%hd%c%s\n", "ACTSVRTABLE:",g_svr_idx, DELIMINATOR,
                               grp_idx, DELIMINATOR, 
                               grp_host_ptr->server_table[s].server_name, DELIMINATOR,
                               ntohs(grp_host_ptr->server_table[s].saddr.sin6_port), DELIMINATOR, 
                               locattr_table_shr_mem[inusesvr_table_shr_mem[user_idx].location_idx].name);
	
        NSDL2_LOGGING(NULL, NULL, "write_location = %s", write_location);
        if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
          return -1;

        if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
          fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
          return -1;
        }
      }
    }
  }
  return 0;
}

/************************************************* 
 * Added By: Nikita Pandey
 * Date    : Fri Jul 13 14:28:03 EDT 2012 
 * Input   : no input
 * Output  : Log Phase Table info into slog file in format -
 *           PHASETABLE:phase_id,group_name,phase_type,phase_name
 * Return V: On success - (0)
 *           On failue  - (-1)
 *
 * TODO: This function uses fwrite(). We need to do that same for others
 ************************************************/
int log_phase_table()
{
  char logging_buffer[LOGGING_SIZE + 1];
  int amt_written;               /* amount of bytes written NOT include trailing NULL */
  int phase_id, grp_idx;
  Schedule *schedule;  
  Phases *phase_ptr;
  RunProfTableEntry_Shr* runprof_ptr = runprof_table_shr_mem;

  NSDL2_LOGGING(NULL, NULL, "Method called");

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    schedule = scenario_schedule;
    for(phase_id = 0; phase_id < schedule->num_phases; phase_id++)
    {
      phase_ptr = &(schedule->phase_array[phase_id]);
      amt_written = snprintf(logging_buffer, LOGGING_SIZE, "PHASETABLE:%d%c%s%c%d%c%s\n",
			     phase_id, DELIMINATOR, "ALL", DELIMINATOR, 
                             phase_ptr->phase_type, DELIMINATOR, phase_ptr->phase_name);
//fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

      if (fwrite(logging_buffer, 1, amt_written, static_logging_fp) != amt_written) {
	fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
	return -1;
      }
    }
  }
  else
  {
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++, runprof_ptr++)
    {    
      schedule = &(group_schedule[grp_idx]);
      NSDL4_SCHEDULE(NULL,NULL, "Number of phase in grp=%d  are=[%d]\n", grp_idx, schedule->num_phases);
      for(phase_id = 0; phase_id < schedule->num_phases; phase_id++)
      {
        phase_ptr = &(schedule->phase_array[phase_id]);
        NSDL4_SCHEDULE(NULL,NULL, "Phase_type=%d", phase_ptr->phase_type);
        amt_written = snprintf(logging_buffer, LOGGING_SIZE, "PHASETABLE:%d%c%s%c%d%c%s\n",
                             phase_ptr->phase_num, DELIMINATOR, runprof_ptr->scen_group_name, DELIMINATOR,
                             phase_ptr->phase_type, DELIMINATOR, phase_ptr->phase_name);

      //  if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
       if (fwrite(logging_buffer, 1, amt_written, static_logging_fp) != amt_written) {
          fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
          return -1;
        }
      }
    }
  }
  fflush(static_logging_fp); 
  return 0; 
}

/*******************************************************************************************
This code create static HostTable.csv with unique id: 

For example Recorded Host table contain server name like: 	
        m.jcpenney.com
	m.macys.com
	www.jcpenney.com
	www.jcpenney.com
	m.homedepot.com
	www.target.com
	www.macys.com
	www.kohls.com
	m.sears.com
        www.jcpenney.com

Logic:	 ____________________________________________________________________
	|Host Name		| Initial Value	|change Intial| Actual HostId|
        |                       |               |as per HostId|              |
        ---------------------------------------------------------------------
	|m.jcpenney.com		|	-1	|     0      |	    0	     |
	|m.macys.com            |       -1      |     1      |      1        |
	|www.jcpenney.com       |       -1      |     2      |      2        |
	|www.jcpenney.com       |       -1      |     2      |      -        |
	|m.homedepot.com        |       -1      |     3      |      3        |		
	|www.target.com         |       -1      |     4      |      4        |
	|www.macys.com          |       -1      |     5      |      5        |
	|www.kohls.com          |       -1      |     6      |      6        |
	|m.sears.com            |       -1      |     7      |      7        |
	|www.jcpenney.com       |       -1      |     2      |      -        |
	 --------------------------------------------------------------------

Result: In HostTable.csv
	----------------
	0,m.jcpenney.com
        1,m.macys.com
        2,www.jcpenney.com
        3,m.homedepot.com
        4,www.target.com
        5,www.macys.com
        6,www.kohls.com
        7,m.sears.com
********************************************************************************************/
int host_id = -1;
//int max_static_host_id = -1; //global
int log_host_table()
{
  int i, j;
  char logging_buffer[LOGGING_SIZE + 1];
  char* write_location;
  int amt_written;   /* amount of bytes written NOT include trailing NULL */
  int space_left; /* amt bytes left NOT including trailing NULL */
  int max_size = LOGGING_SIZE;

  NSDL2_LOGGING(NULL, NULL, "Method called, total_totsvr_entries = %d", total_totsvr_entries);

  for (i = 0; i < total_totsvr_entries; i++) {
    NSDL4_LOGGING(NULL, NULL, "server_name = %s, host_id = %d", totsvr_table_shr_mem[i].server_name, totsvr_table_shr_mem[i].host_id);

    if(totsvr_table_shr_mem[i].host_id == -1)
    {
      write_location = logging_buffer;
      space_left = LOGGING_SIZE; 

      host_id++;
      totsvr_table_shr_mem[i].host_id = host_id;
      if(totsvr_table_shr_mem[i].server_name[0]) {
      amt_written = snprintf(write_location, space_left+1, "HOSTTABLE:%s%c%d%c%d%c%d\n",
                             totsvr_table_shr_mem[i].server_name, DELIMINATOR,
                             totsvr_table_shr_mem[i].host_id, DELIMINATOR,
                             ((g_generator_idx == -1)?0:g_generator_idx), DELIMINATOR, 0);
      }
      NSDL2_LOGGING(NULL, NULL, "write_location = %s", write_location);
      if (!(write_location = forward_buffer(logging_buffer, amt_written, max_size, &space_left)))
        return -1;

      if (write(static_logging_fd, logging_buffer, amt_written) != amt_written) {
        fprintf(stderr, "%s: error in writing to logging file\n", (char*)__FUNCTION__);
        return -1;
      }
 
      for(j = i + 1; j < total_totsvr_entries; j++) 
        if(totsvr_table_shr_mem[j].host_id == -1)
          if(!strcmp(totsvr_table_shr_mem[i].server_name, totsvr_table_shr_mem[j].server_name))
            totsvr_table_shr_mem[j].host_id = host_id;
    }
  }
  //max_static_host_id = host_id + 1;
  //NSDL4_LOGGING(NULL, NULL, "max_static_host_id = %d", max_static_host_id);

  return 0;
}

void log_generator_table()
{
  int j;
  char ptr[128];
  char *token_ptr;

  NSDL1_LOGGING(NULL, NULL, "Method called");
  for (j = 0; j < sgrp_used_genrator_entries; j++)
  {
    strcpy(ptr, generator_entry[j].resolved_IP);
    if((token_ptr = strtok(ptr, ":")) != NULL) {
      token_ptr = strtok(NULL, " ");
    }
    fprintf(static_logging_fp, "GENERATORTABLE:%s%c%d%c%s%c%s%c%s%c%s\n", generator_entry[j].gen_name, DELIMINATOR, j, DELIMINATOR, generator_entry[j].work, DELIMINATOR, generator_entry[j].IP, DELIMINATOR, token_ptr, DELIMINATOR, generator_entry[j].agentport);
  }
 
  fflush(static_logging_fp);
}
