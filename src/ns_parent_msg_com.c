#include <stdio.h>
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
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_gdf.h"
#include "divide_users.h" 
#include "divide_values.h" 
#include "child_init.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_check_monitor.h"
#include "wait_forever.h"
#include "ns_global_dat.h"
#include "ns_trace_level.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_data_handler_thread.h"
#include "ns_nethavoc_handler.h"

extern inline void init_vars_of_ramp_up_down();
extern int NVMIdMapToChildId[];

void send_schedule_phase_start(Schedule *schedule,  int grp_idx, int phase_idx)
{
  parent_child phase_msg;
  int k, ret;
  char time[0xff];
  int num_process; 

  phase_msg.opcode = START_PHASE;
  phase_msg.phase_idx = phase_idx;
  phase_msg.grp_idx = grp_idx;
  
  if ((loader_opcode == MASTER_LOADER) && (global_settings->schedule_by == SCHEDULE_BY_GROUP) ) 
  { 
    num_process = runprof_table_shr_mem[grp_idx].num_generator_per_grp; 
    NSDL2_SCHEDULE(NULL, NULL, "num_process = %d, group_idx = %d, number of generator per group = %d", 
                     num_process, grp_idx, runprof_table_shr_mem[grp_idx].num_generator_per_grp); 
  } else { //For standalone and generator
    num_process = global_settings->num_process; 
  } 

  NSDL2_SCHEDULE(NULL, NULL, "num_process = %d", num_process); 

  init_vars_of_ramp_up_down(schedule);    /* Mark zero for every phase.
                                           * This is for synching RAMP messages */
  for (k = 0; k < num_process; k++)
  {
    /*Bug 65429: bit should set on success only*/
    ret = 1;
    phase_msg.child_id = g_msg_com_con[k].nvm_index;
    if(phase_msg.child_id < 0)
      continue;
    NSDL2_SCHEDULE(NULL, NULL, "Sending START_PHASE to child %d, grp = %d, phase = %d", phase_msg.child_id, grp_idx, phase_idx);
    if (loader_opcode == MASTER_LOADER) 
    { 
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
        phase_msg.child_id = runprof_table_shr_mem[grp_idx].running_gen_value[k].id;

      NSDL2_SCHEDULE(NULL, NULL, "Sending message to child index = %d", phase_msg.child_id);
      CONTINUE_WITH_STARTED_GENERATOR(phase_msg.child_id);
      if ((g_msg_com_con[phase_msg.child_id].con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM) &&
          (g_msg_com_con[phase_msg.child_id].fd != -1))
      { 
        NSTL1(NULL, NULL, "Sending phase index = %d for group id = %d to child index = %d", phase_idx, grp_idx, phase_msg.child_id);
        phase_msg.msg_len = sizeof(phase_msg) - sizeof(int);
	ret = write_msg(&g_msg_com_con[phase_msg.child_id], (char *)&phase_msg, sizeof(phase_msg), 0, CONTROL_MODE); 
      }
    } 
    else 
    {
      //For Standalone and generator 
      //NVM has 0 quantity
      if(((grp_idx >= 0) && !per_proc_runprof_table[(phase_msg.child_id * total_runprof_entries) + grp_idx]) ||
         (g_msg_com_con[k].fd < 0))
        continue;
      phase_msg.msg_len = sizeof(parent_child) - sizeof(int);
      ret = write_msg(&g_msg_com_con[k], (char *)&phase_msg, sizeof(phase_msg), 0, CONTROL_MODE); 
    }
    //Set bitflag when write successful
    if(!ret)
      INC_SCHEDULE_MSG_COUNT(phase_msg.child_id);
    NSDL2_SCHEDULE(NULL, NULL, "Sent START_PHASE to child %d, grp = %d, phase = %d, bitflag = %s",
                               phase_msg.child_id, grp_idx, phase_idx, nslb_show_bitflag(schedule->bitmask));
    if((schedule->phase_array[phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP) ||
       (schedule->phase_array[phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)) {
      INC_RAMPDONE_MSG_COUNT(phase_msg.child_id);
      INC_STATUS_MSG_COUNT(phase_msg.child_id, grp_idx);
    }
  }

  /* Check monitor start */
  start_check_monitor(CHECK_MONITOR_EVENT_START_OF_PHASE, schedule->phase_array[phase_idx].phase_name);

  /*Start Nethavoc scenario*/
  nethavoc_send_api(schedule->phase_array[phase_idx].phase_name, NS_PHASE_START);

  /* Phase Commentary */
  convert_to_hh_mm_ss(schedule->ramp_start_time - global_settings->test_start_time, time);
  log_phase_time(!PHASE_IS_COMPLETED, schedule->phase_array[phase_idx].phase_type, schedule->phase_array[phase_idx].phase_name, time);
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    print2f(rfp, "Starting phase '%s' (phase %d) at %s\n", 
            schedule->phase_array[phase_idx].phase_name, phase_idx, time);
    // This is done so that we can calculete phase timings (Ramp Down) for global.dat
    if(schedule->phase_array[phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
      update_test_runphase_duration();
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    print2f(rfp, "Starting group '%s' (group %d) phase '%s' (phase %d) at %s\n", 
            runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx,
            schedule->phase_array[phase_idx].phase_name, phase_idx, time);
  }

  INC_RAMP_MSG_COUNT(grp_idx);
  schedule->ramp_msg_to_expect = nslb_count_bitflag(g_child_group_status_mask[(grp_idx < 0)?0:grp_idx]);
  NSDL2_SCHEDULE(NULL, NULL, "Group = %d, phase_idx = %d, status mask = %s, ramp mask = %s, ramp done mask = %s,"
                 " ramp_msg_to_expect = %d", grp_idx, phase_idx,
                 nslb_show_bitflag(g_child_group_status_mask[(grp_idx < 0)?0:grp_idx]), nslb_show_bitflag(schedule->ramp_bitmask),
                 nslb_show_bitflag(schedule->ramp_done_bitmask), schedule->ramp_msg_to_expect);
}

//To receive opcode and msg_len from the tools, to process NS/NC http traffic of the running test
// Parent -> Tool
int process_http_test_traffic_stats(void *ptr, Msg_com_con *mccptr)
{
  
  Msg_data_hdr *msg_data_hdr_ptr  = (Msg_data_hdr *) msg_data_ptr;
  TestTrafficStatsReq *test_traffic_req_ptr = (TestTrafficStatsReq*) ptr;
  TestTrafficStatsRes test_traffic_res_obj;
  HttpResponse *http_response_ptr = &(test_traffic_res_obj.http_response);
  Url_hits_gp *url_hits_local_group_ptr;
 
  if( url_hits_gp_ptr != NULL )
  {
    url_hits_local_group_ptr = url_hits_gp_ptr;
  }
  else
  {
    NSTL2(NULL, NULL, "Url_hits_gp structure is null");
    return -1;
  }

  NSTL2(NULL, NULL, "Info regarding http test traffic. Opcode = %d, fd = %d", test_traffic_req_ptr->opcode, mccptr->fd);

  test_traffic_res_obj.progress_interval = global_settings->progress_secs; // Progess interval
  test_traffic_res_obj.abs_timestamp = msg_data_hdr_ptr->abs_timestamp; // RTG packet timestamp
  test_traffic_res_obj.seq_no = msg_data_hdr_ptr->seq_no; //RTG packet sequence number, send along the reponse msg

  http_response_ptr->url_req = url_hits_local_group_ptr->url_req;  //Total HTTP Request Started in the sample period

  http_response_ptr->url_sent = url_hits_local_group_ptr->url_sent;   //Total HTTP Request Sent in the sample period

  http_response_ptr->tries = url_hits_local_group_ptr->tries;  //HTTP Request Completed in the sample period

  http_response_ptr->succ = url_hits_local_group_ptr->succ;  //Total HTTP Request Successful in the sample period

  //Total HTTP Response Time for the sample peeriod
  http_response_ptr->response.avg_time = url_hits_local_group_ptr->response.avg_time;
  http_response_ptr->response.min_time = url_hits_local_group_ptr->response.min_time;
  http_response_ptr->response.max_time = url_hits_local_group_ptr->response.max_time;
  http_response_ptr->response.succ = url_hits_local_group_ptr->response.succ;

  //HTTP Successful Response Time for the sample period
  http_response_ptr->succ_response.avg_time = url_hits_local_group_ptr->succ_response.avg_time;
  http_response_ptr->succ_response.min_time = url_hits_local_group_ptr->succ_response.min_time;
  http_response_ptr->succ_response.max_time = url_hits_local_group_ptr->succ_response.max_time;
  http_response_ptr->succ_response.succ = url_hits_local_group_ptr->succ_response.succ;

  //Total HTTP Failure Response Time for the sample period 
  http_response_ptr->fail_response.avg_time = url_hits_local_group_ptr->fail_response.avg_time;
  http_response_ptr->fail_response.min_time = url_hits_local_group_ptr->fail_response.min_time;
  http_response_ptr->fail_response.max_time = url_hits_local_group_ptr->fail_response.max_time;
  http_response_ptr->fail_response.succ = url_hits_local_group_ptr->fail_response.succ;

  //Total HTTP DNS time of the sample period
  http_response_ptr->dns.avg_time = url_hits_local_group_ptr->dns.avg_time;
  http_response_ptr->dns.min_time = url_hits_local_group_ptr->dns.min_time;
  http_response_ptr->dns.max_time = url_hits_local_group_ptr->dns.max_time;
  http_response_ptr->dns.succ = url_hits_local_group_ptr->dns.succ;

  //Total HTTP Connect Time of the sample period
  http_response_ptr->conn.avg_time = url_hits_local_group_ptr->conn.avg_time;
  http_response_ptr->conn.min_time = url_hits_local_group_ptr->conn.min_time;
  http_response_ptr->conn.max_time = url_hits_local_group_ptr->conn.max_time;
  http_response_ptr->conn.succ = url_hits_local_group_ptr->conn.succ;

  //Total HTTP SSL Time of the sample period
  http_response_ptr->ssl.avg_time = url_hits_local_group_ptr->ssl.avg_time;
  http_response_ptr->ssl.min_time = url_hits_local_group_ptr->ssl.min_time;
  http_response_ptr->ssl.max_time = url_hits_local_group_ptr->ssl.max_time;
  http_response_ptr->ssl.succ = url_hits_local_group_ptr->ssl.succ;

  //Total HTTP First Byte Time in the sample period 
  http_response_ptr->frst_byte_rcv.avg_time = url_hits_local_group_ptr->frst_byte_rcv.avg_time;
  http_response_ptr->frst_byte_rcv.min_time = url_hits_local_group_ptr->frst_byte_rcv.min_time;
  http_response_ptr->frst_byte_rcv.max_time = url_hits_local_group_ptr->frst_byte_rcv.max_time;
  http_response_ptr->frst_byte_rcv.succ = url_hits_local_group_ptr->frst_byte_rcv.succ;
  
  //Total HTTP Download Time in the sample period
  http_response_ptr->dwnld.avg_time = url_hits_local_group_ptr->dwnld.avg_time;
  http_response_ptr->dwnld.min_time = url_hits_local_group_ptr->dwnld.min_time;
  http_response_ptr->dwnld.max_time = url_hits_local_group_ptr->dwnld.max_time;
  http_response_ptr->dwnld.succ = url_hits_local_group_ptr->dwnld.succ;

  http_response_ptr->cum_tries = url_hits_local_group_ptr->cum_tries;  //Total HTTP Requests Completed in the sample period

  http_response_ptr->cum_succ = url_hits_local_group_ptr->cum_succ;  //Total HTTP Requests Successful in the sample period

  http_response_ptr->failure = url_hits_local_group_ptr->failure;  //HTTP Requests Failure (%) in the sample period

  http_response_ptr->http_body_throughput = url_hits_local_group_ptr->http_body_throughput;  //HTTP Body receive throughput in the sample period

  http_response_ptr->tot_http_body = url_hits_local_group_ptr->tot_http_body;  //HTTP Body Total receive bytes in the sample period

  NSDL2_MON(NULL, NULL, "Response buffer for HTTP Test Traffic progress_interval=%d abs_timestamp=%f seq_no=%f url_requested=%f url_sent=%f tries=%f successful=%f response_avg_time=%f response_min_time=%f response_max_time=%f response_succ%f succ_response.avg_time=%f succ_response.min_time=%f succ_response.max_time=%f succ_response.succ=%f fail_response.avg_time=%f fail_response.min_time=%f fail_response.max_time=%f fail_response.succ=%f dns.avg_time=%f dns.min_time=%f dns.max_time=%f dns.succ=%f conn.avg_time=%f conn.min_time=%f conn.max_time=%f conn.succ=%f ssl.avg_time=%f ssl.min_time=%f ssl.max_time=%f ssl.succ=%f frst_byte_rcv.avg_time=%f frst_byte_rcv.min_time=%f frst_byte_rcv.max_time=%f frst_byte_rcv.succ=%f dwnld.avg_time=%f dwnld.min_time=%f dwnld.max_time=%f dwnld.succ=%f cum_tries=%f cum_succ=%f failure=%f http_body_throughput=%f tot_http_body=%f ", test_traffic_res_obj.progress_interval, test_traffic_res_obj.abs_timestamp, test_traffic_res_obj.seq_no, http_response_ptr->url_req, http_response_ptr->url_sent, http_response_ptr->tries, http_response_ptr->succ, http_response_ptr->response.avg_time, http_response_ptr->response.min_time, http_response_ptr->response.max_time, http_response_ptr->response.succ, http_response_ptr->succ_response.avg_time, http_response_ptr->succ_response.min_time, http_response_ptr->succ_response.max_time, http_response_ptr->succ_response.succ, http_response_ptr->fail_response.avg_time, http_response_ptr->fail_response.min_time, http_response_ptr->fail_response.max_time, http_response_ptr->fail_response.succ, http_response_ptr->dns.avg_time, http_response_ptr->dns.min_time, http_response_ptr->dns.max_time, http_response_ptr->dns.succ, http_response_ptr->conn.avg_time, http_response_ptr->conn.min_time, http_response_ptr->conn.max_time, http_response_ptr->conn.succ, http_response_ptr->ssl.avg_time, http_response_ptr->ssl.min_time, http_response_ptr->ssl.max_time, http_response_ptr->ssl.succ, http_response_ptr->frst_byte_rcv.avg_time, http_response_ptr->frst_byte_rcv.min_time, http_response_ptr->frst_byte_rcv.max_time, http_response_ptr->frst_byte_rcv.succ, http_response_ptr->dwnld.avg_time, http_response_ptr->dwnld.min_time, http_response_ptr->dwnld.max_time, http_response_ptr->dwnld.succ, http_response_ptr->cum_tries, http_response_ptr->cum_succ, http_response_ptr->failure, http_response_ptr->http_body_throughput, http_response_ptr->tot_http_body);
 
  test_traffic_res_obj.msg_len = sizeof(test_traffic_res_obj) - sizeof(int);
  test_traffic_res_obj.opcode = test_traffic_req_ptr->opcode;
  
  write_msg(mccptr, (char*)&test_traffic_res_obj, sizeof(test_traffic_res_obj), 0, CONTROL_MODE);
  return 0;
}

