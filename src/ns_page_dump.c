/********************************************************************************
 * File Name            : ns_page_dump.c
 * Author(s)            : Achint Agarwal
 *                        Manpreet Kaur
 * Date                 : 18/06/2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains page dump functions
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <libgen.h>
#include <sys/stat.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_tag_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h"
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_goal_based_sla.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_monitor_profiles.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_wan_env.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_debug_trace.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_child_msg_com.h"
#include "ns_page.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_index_vars.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_error_codes.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_event_log.h"
#include "ns_data_types.h"
#include "ns_parse_scen_conf.h"
#include "ns_event_id.h"
#include "ns_string.h"

#include "ns_vuser_trace.h"
#include "ns_trace_log.h"
#include "ns_url_req.h"
#include "nslb_encode.h"

#include "nslb_util.h"
#include "ns_http_cache.h"

#include "ns_global_settings.h"
#include "ns_page_dump.h"
#include "ns_url_resp.h"
#include "wait_forever.h"
#include "nslb_time_stamp.h"
#include "ns_log_req_rep.h"
#include "nslb_cav_conf.h"

PerProcSessionTable *per_proc_sess_table;

extern void log_session_status(VUser *vptr);

/*Create share memory for keeping number of sessions per nvm*/
void create_per_proc_sess_table() 
{
  int i, j;
  NSDL1_LOGGING(NULL, NULL, "Creating share memory for having number of sessions per nvm");
  per_proc_sess_table = (PerProcSessionTable *) do_shmget(sizeof(PerProcSessionTable) * global_settings->num_process * total_runprof_entries, "PerProcSessionTable");
  /*Initialize num_sess to 0*/
  NSDL2_LOGGING(NULL, NULL, "Initialize num_sess to 0");
  for (i = 0 ; i < total_runprof_entries; i++) { 
    for (j = 0; j < global_settings->num_process; j++) {
      per_proc_sess_table[(j * total_runprof_entries) + i].num_sess = 0;
    }
  }
  NSDL1_LOGGING(NULL, NULL, "Exiting method");
}

/*Function to log Session Distribution per nvm*/
#ifdef NS_DEBUG_ON
static void dump_trace_log()
{
  int i, j; 
  NSDL1_LOGGING(NULL, NULL, "Method called.");

  for (i = 0; i < total_runprof_entries; i++) {
    if (runprof_table_shr_mem[i].gset.trace_limit_mode != 2) {
      NSDL2_LOGGING(NULL, NULL, "Trace limit mode not equal to 2, hence continuing.");
      continue;
    }
    for (j = 0; j < global_settings->num_process; j++) {
      NSDL3_LOGGING(NULL, NULL, "Session Distribution: Group id = %d, NVM = %d, SESS = %d", i, j,
                     per_proc_sess_table[(j * total_runprof_entries) + i].num_sess);
    }
  }
}
#endif  /*NS_DEBUG_ON */ 

static void distribute_sess(int sessions, int group_id)
{
  int j, k, leftover, for_all;
  int cur_proc = 0;
  int count_used_nvm = 0;

  NSDL1_LOGGING(NULL, NULL, "Method called, sessions= %d, group_id = %d",
                 sessions, group_id);

  for (j = 0; j < global_settings->num_process; j++) {
    if (per_proc_runprof_table[(j * total_runprof_entries) + group_id] > 0) {
      /*Count of used nvms*/
      count_used_nvm ++;
    } else
      continue;
  }
  NSDL2_LOGGING(NULL, NULL, "count_used_nvm = %d", count_used_nvm);

  for_all = sessions / count_used_nvm;
  leftover = sessions % count_used_nvm;
  NSDL2_LOGGING(NULL, NULL, "for_all = %d, leftover = %d", for_all, leftover);

  /*Calculate session per nvm*/
  for (j = 0; j < global_settings->num_process; j++) {
    if (per_proc_runprof_table[(j * total_runprof_entries) + group_id] > 0)
      per_proc_sess_table[(j * total_runprof_entries) + group_id].num_sess = for_all;
  }
  /*Distributing leftover sesions*/
  for (k = 0; k < leftover; k++) {
    per_proc_sess_table[(cur_proc * total_runprof_entries) + group_id].num_sess = per_proc_sess_table[(cur_proc * total_runprof_entries) + group_id].num_sess + 1;
    cur_proc++;
    if (cur_proc >= count_used_nvm) cur_proc = 0; /* This ensures that we fill left over for*/
  }
}

/***************************************************************************************************
 * Description     : This function is used to divide session among nvm,
 * 		     Checks:
 *                   If trace_limit_mode == 2 then distribute session similar to user distribution 
 *                   among nvm.Here we fill per_proc_sess_table, struct contains distributed sessions
 *                   per nvm with respect to number of SGRP group 
 * Input Parameter : None
 * Output Parameter: set per_proc_sess_table struct pointer
 * Return          : None
 ****************************************************************************************************/

static void divide_sess_among_nvm (int group_id)
{
  NSDL1_LOGGING(NULL, NULL, "Method called, total_runprof_entries = %d, group_id = %d", total_runprof_entries, group_id); 

  /*Check whether tracing enable for particular group*/
  if (runprof_table_shr_mem[group_id].gset.trace_limit_mode == 0) {
    NSDL2_LOGGING(NULL, NULL, "Trace limit mode equal to 0 no nvm distribution required, hence continuing.");
    return;
  } else if (runprof_table_shr_mem[group_id].gset.trace_limit_mode == 1) {
    NSDL2_LOGGING(NULL, NULL, "Trace limit mode equal to 1, hence dividing pct among nvms.");
    cal_freq_to_dump_session (group_id, runprof_table_shr_mem[group_id].gset.trace_limit_mode_val); 
    return;
  } else { 
    distribute_sess((int)runprof_table_shr_mem[group_id].gset.trace_limit_mode_val, group_id);
    return;
  } 
}

/*************************************************************************************************** 
 * Description     : This function is used to call divide_sess_among_nvm() if keyword
 * 		     G_TRACING enable for particular SGRP group. This function is called from 
 * 		     function setup_schedule_for_nvm() ns_schedule_phases.c  
 * Input Parameter : None
 * Output Parameter: None
 * Return          : None
 ****************************************************************************************************/

void divide_session_per_proc() 
{
  int i;

  NSDL1_LOGGING(NULL, NULL, "Method called, total_runprof_entries = %d, loader_opcode = %d", total_runprof_entries, loader_opcode);
  /* NetCloud: In case of tracing enable with logging page dump with respect to number mode, 
   * here we divide sessions among NVM. In netcloud in case of controller we need not to divide 
   * sessions among nvms, hence returning from the function*/
  if (loader_opcode == MASTER_LOADER)
    return;
  for (i = 0 ; i < total_runprof_entries; i++) {
    NSDL1_LOGGING(NULL, NULL, "group_id = %d, trace_level = %d", i, runprof_table_shr_mem[i].gset.trace_level);
    if (runprof_table_shr_mem[i].gset.trace_level == TRACE_DISABLE) {   
      NSDL2_LOGGING(NULL, NULL, "G_TRACING keyword disable");
      continue;
    } else { 
      NSDL2_LOGGING(NULL, NULL, "G_TRACING keyword enabled");
      divide_sess_among_nvm(i);  
    }
  }
/* Debugging purpose*/
#ifdef NS_DEBUG_ON
  dump_trace_log();
#endif /* NS_DEBUG_ON */ 
}


/*This function is for enabling page dump on useres*/
int need_to_enable_page_dump(VUser *vptr, int mode, int group_idx)
{

  NSDL1_LOGGING(vptr, NULL, "Method called, mode = %d", mode);
  /*if(mode == PAGE_DUMP_MODE_PERCENTAGE)
  {
    //Check for percenatge, if percenatge mode than
    //enable page dump on all vptr, we will check at last if
    //need to dump or not 
    vptr->flags |= NS_PAGE_DUMP_ENABLE;
  }
  else*/
  if(mode == PAGE_DUMP_MODE_NUMBER)
  {
    //Number mode, check for total dumped 
    //If condition does not macth at last and we dont dump the session then 
    if(per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess > 0)
    {
      NSDL2_LOGGING(vptr, NULL, "Remaining number of sessions %d for NVM %d", 
               per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess, my_port_index);
      vptr->flags |= NS_PAGE_DUMP_ENABLE;
      NSDL2_LOGGING(vptr, NULL, "vptr %p is enabled for page dump", vptr);
      return 1;
    }
    else
    {
      NSDL2_LOGGING(vptr, NULL, "Required number of sessions are dumped no need to dump more sessions");
      RESET_ALL_PAGE_DUMP_FLAGS
      //vptr->flags &= ~NS_PAGE_DUMP_ENABLE;
      return 0;
    }
  }
  
  /*In Percentage mode and default mode we will enable page dump on all
 * users */
  
  NSDL2_LOGGING(vptr, NULL, "Dump all sessions");
  vptr->flags |= NS_PAGE_DUMP_ENABLE;
  return 1; //Need to dump all sessions default case or percenatge case
}

int cal_multiple_factor (double pct)
{
  int count = 0;
  int multiplier;
  for (; pct != floor(pct); pct *= 10)  {
    NSDL2_LOGGING(NULL, NULL, "pct = %lf", pct);
    /*Inc count to check significant digit*/
    count++;
  }
  NSDL2_LOGGING(NULL, NULL, "Determine multiple factor, count =%d", count);
  switch (count)
  {
    case 1:
    multiplier = 10;
    break;
    case 2:
    multiplier = 100;
    break;
    case 3:
    multiplier = 1000;
    break;
    case 4:
    multiplier = 10000;
    break;
    default:
    multiplier = 100;
  }
  NSDL2_LOGGING(NULL, NULL, "Calculated multiplier = %d", multiplier);
  return(multiplier);
}

void cal_freq_to_dump_session(int group_num, double pct) 
{
  int multiplier, session_freq, j;
  double modify_pct;
  NSDL1_LOGGING(NULL, NULL, "Method called, group_num = %d, pct = %f", group_num, pct);
  modify_pct = pct / 100;
  NSDL3_LOGGING(NULL, NULL, "modify_pct = %lf", modify_pct);
 
  /*Find multiplier, need to be multiply with both freq and running sessions*/
  multiplier =  cal_multiple_factor(modify_pct);   
  NSDL3_LOGGING(NULL, NULL, "multiplier = %d", multiplier);

  session_freq = (int)((100 * 100 * multiplier) / pct);
  NSDL2_LOGGING(NULL, NULL, "Frequency calculated = %d", session_freq);
  for (j = 0; j < global_settings->num_process; j++) {
    per_proc_sess_table[(j * total_runprof_entries) + group_num].freq = session_freq;
    NSDL2_LOGGING(NULL, NULL, "Frequency updated = %d for nvm_id = %d", per_proc_sess_table[(j * total_runprof_entries) + group_num].freq, j);
    per_proc_sess_table[(j * total_runprof_entries) + group_num].multiplier = multiplier;
    NSDL2_LOGGING(NULL, NULL, "Update multiplier= %d for nvm_id = %d", per_proc_sess_table[(j * total_runprof_entries) + group_num].multiplier, j);
  }
} 


/**/
int need_to_dump_session(VUser *vptr, int mode, int group_idx)
{
  int rem, inc_counter;

  NSDL1_LOGGING(vptr, NULL, "Method called");

  //Disable page dump on all users
  //vptr->flags &= ~NS_PAGE_DUMP_ENABLE;

  /*If mode is ALL then dump all sessions*/
  if(mode == PAGE_DUMP_MODE_ALL) {
    vptr->flags |= NS_PAGE_DUMP_ENABLE;
    return 1; 
  }
  
  if(mode == PAGE_DUMP_MODE_PERCENTAGE)
  {
    /* Intially increment num_sess (running session count) 
     * Nxt we need to decide whether we need to dump session or not
     * If running_counter % frequency now remainder is 1 then we need to dump session
     * otherwise we return
     * */
    per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess ++;
    NSDL2_LOGGING(vptr, NULL, "num_sess = %d, freq = %d", 
                   per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess, 
                    per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].freq);
    /*Increment counter with multiplier*/
    inc_counter = per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess * 
                    per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].multiplier;
    NSDL1_LOGGING(vptr, NULL, "inc_counter = %d", inc_counter);
    /*Find remainder*/ 
    rem = inc_counter % per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].freq;
    NSDL1_LOGGING(vptr, NULL, "remainder = %d", rem);

    if ((per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess == 1) || (rem < per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].multiplier)) {
      vptr->flags |= NS_PAGE_DUMP_ENABLE;
      return 1; //Need to dump this session 
    } else {
      //vptr->flags &= ~NS_PAGE_DUMP_ENABLE;
      RESET_ALL_PAGE_DUMP_FLAGS
      return 0; //no need to dump this session
    }
  }
  else if(mode == PAGE_DUMP_MODE_NUMBER)
  {
    //Number mode, check for total dumped 
    //We will decrease in every condition.
    //If condition does not macth at last and we dont dump the session then 
    //we will increment the num of sessions at the end.
    if(per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess > 0)
    {
      per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess--;
      NSDL2_LOGGING(vptr, NULL, "Remaining number of sessions %d for NVM %d", 
               per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess, my_port_index);
      NSDL2_LOGGING(vptr, NULL, "vptr %p is enabled for page dump", vptr);
      vptr->flags |= NS_PAGE_DUMP_ENABLE;
      return 1;
    } else {
      NSDL2_LOGGING(vptr, NULL, "Required number of sessions are dumped no need to dump more sessions");
      /* Need to reset it otherwise if same user is continuing then flag is remaining set
       * TODO: We can move all thses bits in a MACRO
       * TODO: can we move these bits to ns_session.c when scenario is complete or 
       * in user_cleanup () function.*/
      RESET_ALL_PAGE_DUMP_FLAGS
      return 0;
    }
  }
  
  //Flow should not come here
  //If coming here then some problem
  NSDL2_LOGGING(vptr, NULL, "FLOW SHOULD NOT COME HERE, INVALID PAGE DUMP MODE");
  //vptr->flags &= ~NS_PAGE_DUMP_ENABLE;
  return 0;
}


void init_page_dump_data_and_add_in_list(VUser *vptr)
{
  userTraceData *tmp_user_trace_data_ptr = NULL;

  NSDL1_LOGGING(vptr, NULL, "Method called");

  MY_MALLOC_AND_MEMSET(tmp_user_trace_data_ptr, sizeof(userTraceData) , "User trace data allocated", 0);

  tmp_user_trace_data_ptr->vptr = vptr;

  if(vptr->pd_head == NULL)
  {
    vptr->pd_head = tmp_user_trace_data_ptr;
  }
  else
  {
    fprintf(stderr, "Error: User trace header node is not NULL. There is some problem." 
                    "It should always NULL while calling this method.\n");
    return;
  }

  NSDL1_LOGGING(vptr, NULL, "Method Exiting");
}

/*If we dont dump session then, free nodes and increment session count*/
inline void free_nodes(VUser *vptr)
{
  NSDL1_LOGGING(vptr, NULL, "Method called");
  UT_FREE_ALL_NODES(vptr)
  //ut_free_all_nodes (vptr->pd_head);
  if (vptr->pd_head != NULL) {
  FREE_AND_MAKE_NULL_EX(vptr->pd_head, sizeof(userTraceData), "vptr->pd_head", -1);
  RESET_ALL_PAGE_DUMP_FLAGS
  //per_proc_sess_table[(my_port_index * total_runprof_entries)+ group_idx].num_sess++;
  NSDL2_LOGGING(vptr, NULL, "Total sessions = %d", per_proc_sess_table[(my_port_index * total_runprof_entries)+ vptr->group_num].num_sess);
  }
}
#define FILE_SUFFIX \
sprintf(log_file, "%hd_%u_%u_%d_0_%d_%d_%d_0",\
            child_idx, vptr->user_index, sess_inst, page_inst,\
            vptr->group_num, sess_id, page_id);\


/**/
static int get_hdrs_frm_node(char* log_space, int max_bytes, VUser *vptr, unsigned int sess_id, unsigned int sess_inst, unsigned int page_id, unsigned int page_inst, UserTracePageInfo *page_info, int log_size)
{
  //VUser* vptr = cptr->vptr; get the url_num from vptr
  int amt_written = 0;
  int amt_left = max_bytes;
  int total_written = 0;
  /*Added for paremeterization*/
  int complete_flag = 0;
  UserTraceParamUsed *tmp_param_used_node = page_info->param_used_head;

  if (runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_ONLY_REQ_RESP) {
    NSDL2_LOGGING(NULL, NULL, "Given trace-level is 2,"
             "hence returning total_written = %d", total_written);
    return total_written;
  }

  /*If page failed because of connection making, 
   *parametrization substitution might not have been done*/
  if (!(vptr->url_num) || (page_info->page_status == NS_REQUEST_CONFAIL ))
  {
    NSDL1_LOGGING(vptr, NULL, "Connection Failed");
    return total_written;
  }

  int first_parameter = 0;
  /*BEGIN: Parametrization block*/
  while(tmp_param_used_node != NULL)
  {
    if(amt_left) {
      complete_flag = 0;
      amt_written = encode_parameter_value(tmp_param_used_node->name, tmp_param_used_node->value, log_space+total_written, amt_left, &complete_flag, first_parameter); 
      NSDL4_LOGGING(vptr, NULL, "amt_left = %d, amt_written = %d, log_space = [%s]",
                        amt_left, amt_written, log_space);
      amt_left -= amt_written;
      total_written += amt_written;
      first_parameter = 1;
    } else {
      NSDL1_LOGGING(NULL, NULL, "No data available to read");
      break;
    }
    /*Next node*/
    tmp_param_used_node = tmp_param_used_node->next;
  }
  //if complete parameter list not copied then add ... at the end
  /* Bug#7986
     We make buffer for used parameters and put dots if buffer gets truncated. 
     In case of no parameters used in page the code was into truncation flow which 
     corrupted buffer and core dump was created.*/
  if (!complete_flag && total_written) {
    char num_dotts = (max_bytes - 3)>=0?3:max_bytes;
    memset(log_space + (total_written - num_dotts), '.', num_dotts);//Removed amt_written as we are using total_written which includes amt_written
  }

/*END: Parametrization block*/

  return total_written;
}


void dump_req_resp(VUser *vptr, char *file_name_str, unsigned int sess_inst, unsigned int page_inst, unsigned int page_id, unsigned int sess_id, unsigned int bytes_to_log, char *buf)
{
  NSDL1_LOGGING(NULL, NULL, "Method called");
  
  if((global_settings->replay_mode)) {
    NSDL1_LOGGING(NULL, NULL, "global_settings->replay_mode = %d", global_settings->replay_mode);
    return;
  }
  {
    char log_file[4096] = "\0";
    FILE *log_fp;
    char line_break[] = "\n------------------------------------------------------------\n";
    int create_empty_resp_body_file = 0;
    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if((buf == NULL) || (bytes_to_log == 0)) {
      if (!strcmp(file_name_str, "url_rep_body"))
      {
        create_empty_resp_body_file = 1;
      } else {
        NSDL1_LOGGING(NULL, NULL, "bytes_to_log = %d, returning", bytes_to_log);
        return;  
      }
    }
    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    /* In release 3.9.7, create directory in TR or partition directory(NDE-continues monitoring) for request and response files
     * path:  logs/TRxx/ns_logs/req_rep/
     * or
     * logs/TRxx/<partition>/ns_logs/req_rep/
     * */
    SAVE_REQ_REP_FILES

    sprintf(log_file, "%s/logs/%s/%s_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
                      g_ns_wdir, req_rep_file_path, file_name_str, child_idx, vptr->user_index,
                      sess_inst, page_inst, vptr->group_num, sess_id, page_id);
                      

    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL)
    {
      NSDL1_LOGGING(NULL, NULL, "Unable to open file %s. err = %s", log_file, nslb_strerror(errno));
      fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
      return;
    }
    NSDL1_LOGGING(NULL, NULL, "File created %s", log_file);
    if (create_empty_resp_body_file == 1)
    {
      NSDL1_LOGGING(NULL, NULL, "Created empty file %s in case of confail", log_file);
      fclose(log_fp);
      return;
    }
    //write for both ssl and non ssl url
    if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
    {
      NSDL1_LOGGING(NULL, NULL, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
      return;
    }

    fwrite(line_break, strlen(line_break), 1, log_fp);

    if(fclose(log_fp) != 0)
    {
      NSDL1_LOGGING(NULL, NULL, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
      fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
      return;
    }
  }
  NSDL1_LOGGING(NULL, NULL, "Method exiting");
}

/* Function is used to write response body of page into files stored in docs folder
 *  */
int write_into_rep_body_file(char *path, char *filename, char *buff_data, int buff_len)
{
  struct stat st1;
  FILE *fp;
  char file[1024];

  if (stat(path, &st1) < 0)
  {
    fprintf(stderr,"path [%s] is not correct\n", path);
    return -1;
  }

  sprintf(file, "%s%s", path, filename);

  fp = fopen(file, "w");
  if (fp == NULL)
  {
    fprintf(stderr, "Error cannot open [%s] file\n", file);
    return 0;
  }
#if 0
  if((fwrite(buff_data, 1, buff_len, fp)) <= 0)
  {
    fprintf(stderr, "Error in writting into [%s] file\n", file);
  }
#endif
  fclose(fp);
  return 0;
}

/****************************************************************************
 * Description     : This function used to determine request,response and 
 *                   parameter substitution. Determine trace-on-fail option, 
 *                   in case of 0 we need to get buffer from do_data_processing
 *                   else from vuser trace node. Next call do_trace_log     
 * Input Parameter : vptr and timestamp
 * Output Parameter: None
 * Return          : None
 ***************************************************************************/

void dump_whole_session(VUser* vptr, u_ns_ts_t now)
{
  char *log_space = ns_nvm_scratch_buf;
  int max_buf_len = runprof_table_shr_mem[vptr->group_num].gset.max_trace_param_value_size;
  char resp_body_file_name[MAX_LOG_SPACE_FOR_SNAPSHOT + 1]; //Added to make file name of response body
  char docs_path[1024];
  char url_resp_body_path[4096];
  int total_bytes_copied = 0;
  unsigned int sess_id;
  unsigned int sess_inst;
  unsigned int page_id;
  unsigned int page_inst;
  UserTraceNode *pd_node_tmp = NULL;
  UserTraceNode *pd_node = NULL;
  int need_to_write_session_status_file = 0;
  char log_file[1024];
  char *res_body_without_orig;
  int res_body_file_name_len = 0;
  NSDL1_LOGGING(vptr, NULL, "Method called");

  if((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_FAILED_PG) ||
          (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_WHOLE_IF_PG_TX_FAIL)) 
  {
    /*function to dump req/resp from user trace linklist*/
    userTraceData* utd_node = NULL;
    GET_UTD_NODE
    pd_node = (UserTraceNode*)utd_node->ut_head;
    NSDL2_LOGGING(vptr, NULL, "pd_node = %p", pd_node);
    /* Here we need to traverse UserTraceNode link-list, 
     * Save utd_node->head and update pd_node with next 
     * pointer */
    while(pd_node != NULL)
    {
      pd_node_tmp = pd_node->next; /*Assign temp pointer with pd_node->next*/
      NSDL2_LOGGING(vptr, NULL, "Node type = %d", pd_node->type);
      if(pd_node->type == NS_UT_TYPE_START_SESSION || pd_node->type == NS_UT_TYPE_END_SESSION)
      {
        /* If node is session type no need to log this, just take session id and instance
         * and continue for next node.*/
        sess_id = pd_node->sess_or_page_id;
        sess_inst = pd_node->sess_or_page_inst;
        //If node is end session node, it means we have traveersed full linkedlist
        //now dump the session status after checking flag if we have dump any page
	#if 0
        if(pd_node->type == NS_UT_TYPE_END_SESSION && need_to_write_session_status_file != 0)
        {
          log_session_status(vptr);
        }
	#endif
        NSDL2_LOGGING(vptr, NULL, "sess_id = %d, sess_inst = %d", sess_id, sess_inst);
        pd_node = pd_node_tmp;
        continue;
      }
      //We are here as this node type is page
      if(pd_node->page_info == NULL){
        //TODO: Log event
        fprintf(stderr, "Page info node is NULL. It should not happen\n");
        pd_node = pd_node_tmp;
        continue;
      }
      //On trace_on_failure == 1 then only failed page will be logged.
      //trace_on_failure == 2 then on failing page, Tx or session whole session will be logged
      if (((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_FAILED_PG) && 
         (pd_node->page_info->page_status == NS_REQUEST_OK) && (pd_node->dump_on_tx_fail == 0)))
      {
        NSDL2_LOGGING(vptr, NULL, " Page status %d, hence continuing", pd_node->page_info->page_status);  
        pd_node = pd_node_tmp; 
        continue;
      }
      NSDL2_LOGGING(vptr, NULL, "pd_node = %p", pd_node);
      need_to_write_session_status_file++;
      page_id = pd_node->sess_or_page_id;
      page_inst = pd_node->sess_or_page_inst;
      if (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP) {
        total_bytes_copied = get_hdrs_frm_node(log_space, max_buf_len, vptr, sess_id, sess_inst, page_id, page_inst, pd_node->page_info, pd_node->page_info->rep_body_size);
      } 
      else {
        total_bytes_copied = get_hdrs_frm_node(log_space, max_buf_len, vptr, sess_id, sess_inst, page_id, page_inst, pd_node->page_info, 0);
      }
      NSDL2_LOGGING(NULL, NULL, "total_bytes_copied = %d", total_bytes_copied);
      //if (total_bytes_copied > 0)
        //log_space[total_bytes_copied++] = '\n';
      log_space[total_bytes_copied] = '\0';
      /*First dump req file */
      NSDL2_LOGGING(NULL, NULL, "pd_node->page_info->req_size = %d", pd_node->page_info->req_size);
      dump_req_resp(vptr, "url_req", sess_inst, page_inst, page_id, sess_id, pd_node->page_info->req_size, pd_node->page_info->req);
      /*dump resp file*/
      NSDL2_LOGGING(NULL, NULL, "pd_node->page_info->rep_size = %d", pd_node->page_info->rep_size);
      dump_req_resp(vptr, "url_rep", sess_inst, page_inst, page_id, sess_id, pd_node->page_info->rep_size, pd_node->page_info->rep);
      /*dump rep body*/
      NSDL2_LOGGING(NULL, NULL, "pd_node->page_info->rep_size = %d", pd_node->page_info->rep_size);
      dump_req_resp(vptr, "url_rep_body", sess_inst, page_inst, page_id, sess_id, pd_node->page_info->rep_body_size, pd_node->page_info->rep_body);
      if (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP){ //If trace level is greater than 2
        /* In release 3.9.7,
         * Make response body file name and pass as an argument in log_message_record2
         * nvm_id:sess_instance:script_name:page_name:page_id*/ 
        sprintf(resp_body_file_name, "%hd:%d:%s:%s:%d.orig", 
                   child_idx, sess_inst, get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "-"),
                   pd_node->page_info->page_name, page_inst);
        /* Write page response body into file which is stored in docs directory*/
        if (vptr->partition_idx <= 0)
          sprintf(docs_path, "logs/TR%d", testidx);
        else
          sprintf(docs_path, "logs/TR%d/%lld/", testidx, vptr->partition_idx);
        NSDL2_LOGGING(NULL, NULL, "Writing response body into file = %s at docs folder = %s, ", resp_body_file_name, docs_path);

        sprintf(url_resp_body_path, "%s/ns_logs/req_rep/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                   docs_path, child_idx, vptr->user_index,
                   sess_inst, page_inst, vptr->group_num, sess_id, page_id);
        //Create orig file path 
        sprintf(docs_path, "%s/page_dump/docs/%s", docs_path, resp_body_file_name);

        //Hard link response body with orig file, hence need to create file path
        if ((link(url_resp_body_path, docs_path)) == -1)
        {
          //fprintf(stderr, "Error: Unable to create hard link for orig file. %s", nslb_strerror(errno));
          if((symlink(url_resp_body_path, docs_path)) == -1)
            NSDL2_LOGGING(NULL, NULL, "Error: Unable to create link for orig file. %s", nslb_strerror(errno));
        }
        NSDL2_LOGGING(NULL, NULL, "Created link of %s in %s", url_resp_body_path, docs_path);
        // removing the .orig file with NULL
        if ((res_body_without_orig = strrchr(resp_body_file_name, '.')) != NULL) 
        {
          *res_body_without_orig = '\0'; 
          NSDL2_LOGGING(vptr, NULL, "After removing orig extension orig_file_name = %s", resp_body_file_name);
          res_body_file_name_len = strlen(resp_body_file_name);
        }
      }
      //If total_bytes_copied is zero then send NULL as parameterization buffer
      FILE_SUFFIX; 
      log_page_dump_record(vptr, child_idx, pd_node->page_info->page_start_time, pd_node->page_info->page_end_time, sess_inst, sess_id, page_id, page_inst, (total_bytes_copied != 0)?log_space:NULL, total_bytes_copied, pd_node->page_info->page_status, pd_node->page_info->flow_name?pd_node->page_info->flow_name:"NA", strlen(log_file), log_file, res_body_file_name_len, (res_body_file_name_len == 0)?NULL:resp_body_file_name, pd_node->page_info->page_name?pd_node->page_info->page_name:"NA", pd_node->page_info->page_rep_time, -1, 0, NULL, 0, NULL); //Added page response time and three future fields

      pd_node = pd_node_tmp;
      //TODO: Optimization we can free nodes here itself insted of calling ut_free_all_nodes();
   } //While
   NSDL2_LOGGING(vptr, NULL, "LAST utd_node->ut_head = %p", utd_node->ut_head);
  }
}

void start_dumping_page_dump (VUser *vptr, u_ns_ts_t now)
{
  int ret;
  NSDL1_LOGGING(vptr, NULL, "Method called");
  //First check if session status is failed or not 
  //If status is failed then only we will dump the session
  //Otherwise just return because we are dumping from here
  //only for failure case 
  //First check status of session 
  //Case1: If Pass then update the number of sessions and return.

  /* If bit is not set then dont dump this session
   * This NS_PAGE_DUMP_CAN_DUMP bit gets set 
   * in following cases:
   * 		        Page fails
   *                    Transaction fails
   *                    Page Dump API set 
   * */
   if (vptr->flags & NS_PAGE_DUMP_CAN_DUMP)
     NSDL2_LOGGING(vptr, NULL, "NS_PAGE_DUMP_CAN_DUMP enable");
 
  if((vptr->flags & NS_PAGE_DUMP_CAN_DUMP) && (ret = need_to_dump_session (vptr, runprof_table_shr_mem[vptr->group_num].gset.trace_limit_mode, vptr->group_num)))
  {     
    //Case1: If failure then check trace_on_fail 
    dump_whole_session(vptr, now);
  }
  else 
  {
    NSDL2_LOGGING(vptr, NULL, "Every thing is OK(passed) or limit to dump sessions is over. No need to do page dump");
  }

  free_nodes (vptr);
}

/* Function used to log message in case of trace level 1
 * print url details on screen, log data in 
 * dlog and log files and create url_req file with request 
 * line
 * */
void dump_url_details(connection *cptr, u_ns_ts_t now)
{
  VUser* vptr = cptr->vptr;
#if 0
  char log_space[MAX_LOG_SPACE_FOR_SNAPSHOT + 1];
  int total_bytes_copied;
#endif
  char request_buf[MAX_LINE_LENGTH];
  int request_buf_len;
  char log_file[1024];
  NSDL1_LOGGING(vptr, NULL, "Method called");

  sprintf(log_file, "%hd_%u_%u_%d_0_%d_%d_%d_0",
            child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, vptr->sess_ptr->sess_id, vptr->cur_page->page_id);

  action_request_Shr *request = cptr->url_num;
  PerHostSvrTableEntry_Shr* svr_entry;
  svr_entry = get_svr_entry(vptr, request->index.svr_ptr);

  /*For trace on destination 0 or 2*/
  if(runprof_table_shr_mem[vptr->group_num].gset.trace_dest == 0 || 
       runprof_table_shr_mem[vptr->group_num].gset.trace_dest == 2) 
  {
    NSDL1_LOGGING(vptr, NULL, "Print url detail on console, cptr->req_ok = %d", cptr->req_ok);
    printf("\nTimeStamp=%s; URL=%s; SessInstance=%u; UserId=%u; Group=%s; Script=%s:%s; Page=%s;"
           " URLStatus=%s; PageInstance=%d; SessId=%d; PageId=%d; GroupNum=%d; MyPortIndex=%d;" 
           " ServerName=%s; ServerPort=%hd; sess_status=NA \n",
           get_relative_time(), get_url_req_url(cptr), vptr->sess_inst, vptr->user_index, 
           runprof_table_shr_mem[vptr->group_num].scen_group_name,
           vptr->sess_ptr->sess_name, vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA",
           vptr->cur_page->page_name, get_error_code_name(cptr->req_ok), vptr->page_instance,
           GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr), vptr->group_num, my_port_index, 
    //     cptr->url_num->index.svr_ptr->totsvr_table_ptr->server_name, 
           svr_entry->server_name,
   //      ntohs(cptr->url_num->index.svr_ptr->totsvr_table_ptr->saddr.sin6_port));
           ntohs(svr_entry->saddr.sin6_port)); 
  }
  /*For trace on destination 1 or 2*/ 
  if(runprof_table_shr_mem[vptr->group_num].gset.trace_dest == 1 ||
             runprof_table_shr_mem[vptr->group_num].gset.trace_dest == 2) 
  { 
    NSDL1_LOGGING(vptr, NULL, "Print url detail in log file");
#if 0
    int page_status_point;
    total_bytes_copied = get_parameters(cptr, log_space, vptr, MAX_LOG_SPACE_FOR_SNAPSHOT, 0, &page_status_point);    
    log_space[total_bytes_copied++] = '\n';
    log_space[total_bytes_copied] = '\0';
    NSDL4_LOGGING(vptr, NULL, "log_space = %s", log_space);
#endif
    log_page_dump_record(vptr, child_idx, vptr->pg_begin_at, cptr->ns_component_start_time_stamp, vptr->sess_inst,
           vptr->sess_ptr->sess_id, vptr->cur_page->page_id, vptr->page_instance, NULL, 0, vptr->page_status, vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA", strlen(log_file), log_file, 0, NULL, vptr->cur_page->page_name?vptr->cur_page->page_name:"NA", -1, -1, 0, NULL, 0, NULL);//Here page response time will be -1 and three future fields
    /*Create request file to show URL name*/
    get_url_req_line(cptr, request_buf, &request_buf_len, MAX_LINE_LENGTH);
    NSDL4_LOGGING(vptr, NULL, "request_buf = %s, request_buf_len = %d", request_buf, request_buf_len);
    dump_req_resp(vptr, "url_req", vptr->sess_inst, vptr->page_instance, GET_PAGE_ID_BY_NAME(vptr), 
                   GET_SESS_ID_BY_NAME(vptr), request_buf_len, request_buf);  
  }
  NSDL1_LOGGING(vptr, NULL, "Exiting method");
}

/* PAGE DUMP API: ns_trace_log_current_sess()
 * Input parameters:  vptr
 * Output parameters: Enable vptr->flag, need to dump session
 * Return value: 
 *   0: success case 
 * -ve: failure case
 * +ve: Session already dump
 * */
int trace_log_current_sess(VUser* vptr)
{
  NSDL1_LOGGING(vptr, NULL, "Method called");

  /* Verify whether page dump enable for vptr
   * Check page already dump or not
   * */
  if (!(vptr->flags & NS_PAGE_DUMP_ENABLE)) {
    NSDL1_LOGGING(vptr, NULL, "Page dump disable for vptr, hence returning");
    return -1;
  } else if ((vptr->flags & NS_PAGE_DUMP_ENABLE) && runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS) {
    NSDL1_LOGGING(vptr, NULL, "Error case, page dump enabled for all sessions");
    return 1;
  } else {
    NSDL1_LOGGING(vptr, NULL, "Page dump enabled for vptr=%p", vptr);
    /* Set page dump flag, bit gets set in case of page or tx failure*/
    vptr->flags |= NS_PAGE_DUMP_CAN_DUMP;
  }
  return 0;
}

static void mark_page_to_dump(TxInfo *node_ptr, UserTraceNode *pd_node, VUser *vptr)
{
  int i;

  NSDL1_LOGGING(vptr, NULL, "Method called, number of pages in transaction %d", node_ptr->num_pages);
  for (i = 0; (i < node_ptr->num_pages && pd_node != NULL); i++)
  {
    NSDL2_LOGGING(vptr, NULL, "User Trace node = %p, Tx Page instance %d," 
                   "Page dump node instance %d", 
                   pd_node, node_ptr->page_instance[i], pd_node->sess_or_page_inst);

    if (node_ptr->page_instance[i] == pd_node->sess_or_page_inst) //Safety Check
      pd_node->dump_on_tx_fail = 1;
    else //This case should not happen, here we assume all pages will have continues instances 
    {
      fprintf(stderr, "Page instance[%d] miss matched with page instance[%d] given in transaction. It should not happen.\n", pd_node->sess_or_page_inst, node_ptr->page_instance[i]);
      NS_EL_1_ATTR(EID_TRANS, vptr->user_index, vptr->sess_inst, 
                   EVENT_CORE, EVENT_CRITICAL,
                    vptr->sess_ptr->sess_name,
                    "Page instance[%d] miss matched with page instance[%d] given in transaction."
                    " It should not happen.\n", pd_node->sess_or_page_inst, node_ptr->page_instance[i]);
    }
    NSDL2_LOGGING(NULL, NULL, "Set flag %d for page instance %d which need" 
                     " to be dump on transaction failure", pd_node->dump_on_tx_fail, pd_node->sess_or_page_inst);
    pd_node = pd_node->next;
  }  
}

 
/*Function used to mark page instance of transaction node*/
void mark_pg_instace_on_tx_failure(TxInfo *node_ptr, VUser *vptr)
{
  UserTraceNode *pd_node_tmp = NULL;
  UserTraceNode *pd_node = NULL;
  NSDL1_LOGGING(NULL, NULL, "Method called");

  userTraceData* utd_node = NULL;
  GET_UTD_NODE
  if (utd_node == NULL)
  { 
    NSDL2_LOGGING(vptr, NULL, "User trace node is NULL hence returning...");
    return;
  }
  pd_node = (UserTraceNode*)utd_node->ut_head;
  NSDL2_LOGGING(vptr, NULL, "pd_node = %p", pd_node);

  while(pd_node != NULL)
  {
    pd_node_tmp = pd_node->next; /*Assign temp pointer with pd_node->next*/
    NSDL2_LOGGING(vptr, NULL, "Node type = %d", pd_node->type);
    if (pd_node->type == NS_UT_TYPE_PAGE || pd_node->type == NS_UT_TYPE_PAGE_REQ)
    {
      if (node_ptr->page_instance[0] == pd_node->sess_or_page_inst)
      {
        mark_page_to_dump(node_ptr, pd_node, vptr);
        break;
      }
    }
    pd_node = pd_node_tmp;
  }
  NSDL1_LOGGING(vptr, NULL, "Exiting method");
}
