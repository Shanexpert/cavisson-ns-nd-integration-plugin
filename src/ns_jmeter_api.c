/************************************************************************************
 * Name	         : ns_jmeter_api.c 

 * Purpose       :  

 * Author(s)     : Manish 

 * Date          : 5 April 2018 

 * Copyright     : (c) Cavisson Systems

 * Mod. History  : 
***********************************************************************************/

#include "util.h"
#include "netstorm.h"

#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_trace_level.h"

#include "ns_vuser_ctx.h"
#include "ns_rbu_api.h"
#include "ns_jmeter_api.h"
#include "ns_vuser_tasks.h"
#include "ns_script_parse.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "ns_schedule_phases_parse.h"
#include "ns_jmeter.h"

#define MAX_SCRIPT_PROJ_SUBPROJ_LEN 2048
#define MAX_JMX_SCRIPT_PATH 512
void ns_jm_handler_sigchild_ignore(int data)
{
  NSDL2_RBU(NULL, NULL, "Ignoring signal %d to run JMETER command", data);  
}

#define JMETER_SCHEDULE_SETTINGS 100
#define JMETER_GEN_LOC_SETTINGS 100
#define JMETER_GEN_REPORT_ARGS 1024
void ns_jmeter_init(VUser *vptr, char *ptr)
{
  jmeter_attr_t *jmeter = NULL;
  FILE *fp;
  char read_buf[MAX_SCRIPT_PROJ_SUBPROJ_LEN + 1];
  char jmx_session_name[JMETER_MAX_JTL_FNAME_LEN + 1];
  char script_path[MAX_JMX_SCRIPT_PATH + 1];
  char jmeter_schedule_setting[JMETER_SCHEDULE_SETTINGS + 1] = "";
  char jmeter_loc_gen_idx[JMETER_GEN_LOC_SETTINGS];
  char jmeter_generate_report[JMETER_GEN_REPORT_ARGS];
  char jmeter_report_html[512 + 1];
  char cmd[1024 + 1];
  char err_msg[1024] = "\0";
  char *cmd_ptr = jmeter_schedule_setting;
  char tmp[2]="";
  int len = 0;
  
  // Initializing local buffer
  jmeter_loc_gen_idx[0] = 0;
  jmeter_generate_report[0] = 0;

  NSDL2_HTTP(vptr, NULL, "vptr = %p, group_num = %d", vptr, vptr->group_num);
  
  //Allocate memory for jmeter atrributes
  if(vptr->httpData->jmeter == NULL) 
  { 
    MY_MALLOC_AND_MEMSET(vptr->httpData->jmeter, sizeof(jmeter_attr_t),  "vptr->httpData->jmeter", -1);

    jmeter = vptr->httpData->jmeter;

    MY_MALLOC_AND_MEMSET(jmeter->cmd_buf, JMETER_MAX_CMD_LEN + 1,  "vptr->httpData->jmeter.cmd_buf", -1);
  }
  else
    jmeter = vptr->httpData->jmeter; 

  vptr->partition_idx = g_partition_idx;

  JMETER_NS_LOGS_PATH

  jmeter->cmd_buf[0] = 0;
  jmeter->jmeter_pid = -1;

  NSDL2_HTTP(vptr, NULL, "testid = %d, debug = %d, port = %d, vptr->group_num = %d, group_name = %s", 
                          testidx, debug_log_value, global_settings->jmeter_port[vptr->group_num], vptr->group_num,
                          runprof_table_shr_mem[vptr->group_num].scen_group_name);

  // We dont need now to remove dependency of nsi_attach_jmeter_listener.sh to create and get jmx.bak file from TR.
  get_jmeter_script_name(jmx_session_name, session_table_shr_mem[vptr->sess_ptr->sess_id].proj_name,
           session_table_shr_mem[vptr->sess_ptr->sess_id].sub_proj_name,
           vptr->sess_ptr->sess_name);
 
  snprintf(read_buf, MAX_SCRIPT_PROJ_SUBPROJ_LEN, "%s/%s/%s/%s/scripts/%s",
           g_ns_wdir, GET_NS_RTA_DIR(), session_table_shr_mem[vptr->sess_ptr->sess_id].proj_name,
           session_table_shr_mem[vptr->sess_ptr->sess_id].sub_proj_name, 
           jmx_session_name);
  
  //./jmeter -n -t /home/netstorm/work/scripts/ns_ut/ns_ut/jm1.jmx -l /tmp/jm1.jtl
  #if 0
  snprintf(jmeter->cmd_buf, JMETER_MAX_CMD_LEN, "nsi_start_jmeter_test.sh -f %s/logs/TR%d/scripts/%s", 
           g_ns_wdir, testidx, get_sess_name_with_proj_subproj_int(vptr->sess_ptr->jmeter_sess_name, vptr->sess_ptr->sess_id, "/"));

  NSDL2_HTTP(NULL, NULL, "New cmd buf for making bak file = %s", jmeter->cmd_buf);

  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, ns_jm_handler_sigchild_ignore);

  fp = popen(jmeter->cmd_buf, "r");

  if(fp == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: unable to make copy of jmeter script %s. errno = %d, errstr = %s",
                           vptr->sess_ptr->sess_name, errno, nslb_strerror(errno));
  }

  fgets(read_buf, 1024, fp);
  pclose(fp);
  (void) signal( SIGCHLD, prev_handler);
  #endif

  //read_buf[strlen(read_buf)] = '\0';

  NSDL2_HTTP(vptr, NULL, "read_buf = [%s]", read_buf); 

 // This data is required in cmd buff for response file name
  snprintf(script_path, MAX_JMX_SCRIPT_PATH, "%s/%s/scripts/%s",
           session_table_shr_mem[vptr->sess_ptr->sess_id].proj_name, 
           session_table_shr_mem[vptr->sess_ptr->sess_id].sub_proj_name,
           vptr->sess_ptr->sess_name);  
  NSDL2_MISC(vptr, NULL, "Script Path = [%s]", script_path);

  // It is not neccessary all schedule settings of jmeter would have some valid value
  // In case of invalid value don't copy them
  if(runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.threadnum != -1)
  {
    len = snprintf(cmd_ptr, JMETER_SCHEDULE_SETTINGS, "-Jthreads=%d ", runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.threadnum);
    cmd_ptr = cmd_ptr + len;
  }
  if(runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.ramp_up_time != -1)
  {
    len = snprintf(cmd_ptr, JMETER_SCHEDULE_SETTINGS, "-Jrampup=%d ", runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.ramp_up_time);
    cmd_ptr = cmd_ptr + len;
  }
  if(runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.duration != -1)
  {
    len = snprintf(cmd_ptr, JMETER_SCHEDULE_SETTINGS, "-Jduration=%d", runprof_table_shr_mem[vptr->group_num].gset.jmeter_schset.duration);
    cmd_ptr = cmd_ptr + len;
  }
 
  NSDL2_MISC(vptr, NULL, "JMETER SCHEDULE_SETTINGS = [%s]", jmeter_schedule_setting);
 
  // File required to log jmeter logs and need to pass with -j option 
  char jmeter_log_file[512 + 1];
  snprintf(jmeter_log_file, 512, "%s/logs/TR%d/ns_logs/JMeter_%s_%d.log",g_ns_wdir,testidx, vptr->sess_ptr->sess_name, vptr->group_num);

  // Created temporary pointer to runprof ptr to access TRACING data
  RunProfTableEntry_Shr *tmprprof_ptr = &(runprof_table_shr_mem[vptr->group_num]); 

  // In case of Netcloud, we need to send CAV location and Generator Index
  // Location index is hardcoded to 0
  if(send_events_to_master == 1)
    sprintf(jmeter_loc_gen_idx, " -Jcav_loc_idx=0 -Jcav_gen_idx=%d",(int)((child_idx & 0xFF00) >> 8));

  NSDL2_MISC(vptr, NULL, "JMETER LOC GEN IDX = [%s] send_events_to_master = %d", jmeter_loc_gen_idx, send_events_to_master);

  // Check If generate report level is ON OR OFF. If ON create html directory under report/jmeter.
  // Currently this has been done only for Netstorm, Netcloud mode would be handled later.
  if((runprof_table_shr_mem[vptr->group_num].gset.jmeter_gset.gen_jmeter_report) &&
     (!send_events_to_master))
  {
    snprintf(jmeter_report_html, 512, "%s/logs/TR%d/reports/jmeter/JMeter_%s_%d/html", 
                                       g_ns_wdir, testidx, vptr->sess_ptr->sess_name, vptr->group_num);
   
    snprintf(cmd, 1024, "mkdir -p %s", jmeter_report_html);

    NSDL2_MISC(NULL, NULL, "Running command to make Jmeter html report directory. command = %s", cmd);
    int retsys = nslb_system(cmd, 1, err_msg);
    if(retsys != 0)
    {               
      NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
    }
 
    snprintf(jmeter_generate_report, JMETER_GEN_REPORT_ARGS, " -l %s/logs/TR%d/reports/jmeter/JMeter_%s_%d/JMeter_%s_%d.csv"
                                                             " -e -o %s", 
                                                            g_ns_wdir, testidx, vptr->sess_ptr->sess_name, vptr->group_num,
                                                            vptr->sess_ptr->sess_name,vptr->group_num,
                                                            jmeter_report_html);
  }
  NSDL2_MISC(vptr, NULL, "JMETER GENERATOR REPORT = [%s]", jmeter_generate_report);


  // TODO: CHECK tmprprof_ptr->gset.max_log_space
  /* In the below command -DcavTracing 4th argument, max_log_space has been hardcoded to 0 due to the reason:
     In ns_trace_log.c, check for max_log_space has to be 0 is present but if its 0 then it is getting 
     updated with 100000000 in the same file */

  // Creating JMETER CMD BUF with additional schedule settings and additional arguments 
  snprintf(jmeter->cmd_buf, JMETER_MAX_CMD_LEN, "nohup java -Xms%dm -Xmx%dm %s "
                           "\"-Dplugin_dependency_paths=%s/webapps/netstorm/lib/CavJMeterListener.jar\""
                           " -DdebugLevel=%d -DlistenerPort=%d -DlistenerSampleInterval=10000 -DtrNum=%d -DgroupName=%s"
                           " -DcavSgrpIndex=%d -DcavChildIndex=%hd -DcavSessIndex=%u -DcavPartition=%lld"
                           " -DcavTracing=%hd-%hd-%hd-0-%hd-%hd-%d"
                           " -DcavSessName=%s"
                           " -DcavPgAsTx=%hd-%hd-%hd"
                           " -jar %s/bin/ApacheJMeter.jar -n -t %s %s %s %s %s -j %s"
                           " >/dev/null 2>&1 </dev/null & echo -n $!", group_default_settings->jmeter_gset.min_heap_size,
                           group_default_settings->jmeter_gset.max_heap_size,
                           (runprof_table_shr_mem[vptr->group_num].gset.jmeter_gset.jmeter_java_add_args == -1)?tmp:BIG_BUF_MEMORY_CONVERSION(runprof_table_shr_mem[vptr->group_num].gset.jmeter_gset.jmeter_java_add_args),
                           g_ns_wdir, debug_log_value, g_jmeter_ports[vptr->group_num], testidx, 
                           runprof_table_shr_mem[vptr->group_num].scen_group_name,
                           vptr->group_num, child_idx, GET_SESS_ID_BY_NAME(vptr), g_partition_idx,
                           tmprprof_ptr->gset.trace_level, tmprprof_ptr->gset.trace_dest, tmprprof_ptr->gset.trace_on_fail,
                           tmprprof_ptr->gset.trace_inline_url, tmprprof_ptr->gset.trace_limit_mode,
                           (int)tmprprof_ptr->gset.trace_limit_mode_val,
                           script_path,global_settings->pg_as_tx, global_settings->pg_as_tx_name,
                           global_settings->page_as_tx_jm_parent_sample_mode,
                           ptr, read_buf, jmeter_schedule_setting,
                           (runprof_table_shr_mem[vptr->group_num].gset.jmeter_gset.jmeter_add_args == -1)?tmp:BIG_BUF_MEMORY_CONVERSION(runprof_table_shr_mem[vptr->group_num].gset.jmeter_gset.jmeter_add_args), jmeter_loc_gen_idx, jmeter_generate_report, jmeter_log_file); 

  NSTL1(NULL, NULL, "JMeter cmd = %s", jmeter->cmd_buf);

  NSDL2_HTTP(vptr, NULL, "JMeter cmd = %s", jmeter->cmd_buf);
}

void ns_jmeter_handler_sigchild_ignore(int data)
{
  NSDL2_HTTP(NULL, NULL, "Method called, data = %d", data);
}

static int ns_jmeter_wait_to_complete(VUser *vptr)
{
  jmeter_attr_t *jmeter = vptr->httpData->jmeter;

  NSDL2_HTTP(vptr, NULL, "Method called, vptr = %p", vptr);

  while(1)
  {
    //checking jmeter_pid is running or not
    // Removed the check if elapsed time is more than 1 hour
    if((jmeter->jmeter_pid) && kill(jmeter->jmeter_pid, 0) == 0)
    {
      NSDL2_HTTP(vptr, NULL, "JMeter with pid %d is running ...", jmeter->jmeter_pid);
      NSTL2(vptr, NULL, "JMeter with pid %d is running ...", jmeter->jmeter_pid);
      if(sigterm_received)
      {
        NSTL1(vptr, NULL, "SIGTERM received. So going to stop Jmeter process pid = [%d]", jmeter->jmeter_pid);
        break;
      }
      // Sleeping Vuser for 10 sec - implemented with page_think_time
      VUSER_SLEEP(vptr, 10000);
      continue; 
    }
    else
    {
      NSTL1(vptr, NULL, "Jmeter process with pid %d done", jmeter->jmeter_pid);
      NSDL2_HTTP(vptr, NULL, "JMeter with pid %d done!", jmeter->jmeter_pid); 
      break;
    } 
  }
  return 0;
}

void ns_jmeter_stop(VUser *vptr)
{
  // There is a bug in the below code.
  // If a NVM is handling multiple script of same type - jmeter, we are deallocating
  // global g_jmeter_listen_msg_con without considering the index where jmeter script is allocated.
  // Also we have already handled the case of connection close with jmeter so no need to perform below operation here.

  // TODO: This will be corrected LATER

  jmeter_attr_t *jmeter = vptr->httpData->jmeter;
/*
  NSDL2_HTTP(vptr, NULL, "Method called, vptr = %p, jmeter_pid = %d", vptr, jmeter->jmeter_pid);

  NSDL2_MISC(vptr, NULL, " Freeing g_jmeter_listen_msg_con"); 
  FREE_AND_MAKE_NULL(g_jmeter_listen_msg_con, "JMeter Listener conn", -1);
  //NSDL2_MISC(vptr, NULL, " Freeing JMeter Listener Port"); 
  //FREE_AND_MAKE_NULL(global_settings->jmeter_port, "JMeter Listener port", -1);
*/
  if(jmeter->jmeter_pid)
  {
    NSTL1(vptr, NULL, "Sending SIGKILL to Jmeter process pid = [%d]", jmeter->jmeter_pid);
    kill(jmeter->jmeter_pid, SIGKILL);
    jmeter->jmeter_pid = 0;
  }
}

int ns_jmeter_start_internal(VUser *vptr, char *ptr)
{
  FILE *fp = NULL;
  jmeter_attr_t *jmeter = NULL; 
  char read_buf[32 + 1];
  char jmeter_pid_file[ 32 + 1];
  char err_msg[1024 +1]; 

  typedef void (*sighandler_t)(int);
  
  NSDL2_MISC(vptr, NULL, "Method called, vptr = %p", vptr);

  ns_jmeter_init(vptr, ptr);

  jmeter = vptr->httpData->jmeter;

  //Is need??
  if((jmeter->jmeter_pid != -1) && (kill(jmeter->jmeter_pid, 0) == 0)) 
  {
    NSTL1(NULL, NULL, "JMeter process is already running with pid = %d", jmeter->jmeter_pid);
    NSDL2_MISC(vptr, NULL, "JMeter process is already running with pid = %d", jmeter->jmeter_pid); 
    return 0;
  }

  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, ns_jmeter_handler_sigchild_ignore);

  NSTL1(NULL, NULL, "cmd_buf while running = %s", jmeter->cmd_buf);
  NSDL2_MISC(vptr, NULL, "cmd_buf while running = %s", jmeter->cmd_buf);

  fp = popen(jmeter->cmd_buf, "r");
  if(fp == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: unable to execute jmeter script %s. errno = %d, errstr = %s", 
                           vptr->sess_ptr->sess_name, errno, nslb_strerror(errno));
    return -1;
  }

  fgets(read_buf, 32, fp);

  jmeter->jmeter_pid = atoi(read_buf); 
  NSTL1(NULL, NULL, "JMeter started successfully. JMeter pid= %d", jmeter->jmeter_pid);
  NSDL2_MISC(vptr, NULL, "ret_buf = [%d]", jmeter->jmeter_pid);
 
  sprintf(jmeter_pid_file, "JMeter_%s_%d.pid", vptr->sess_ptr->sess_name, vptr->group_num);
  
  if(nslb_write_process_pid(jmeter->jmeter_pid, "jmeter_pid", g_ns_wdir, testidx, "w", jmeter_pid_file, err_msg) < 0)
  {
    NS_EXIT(-1, "Error in saving JMeter process pid in file %s, %s", jmeter_pid_file, err_msg);
  }

//my_port_index, sgrp index -> put correct variable

  pclose(fp);
  (void) signal( SIGCHLD, prev_handler);

  ns_jmeter_wait_to_complete(vptr);
  
  ns_jmeter_stop(vptr);

  return 0;
}

int ns_jmeter_start()
{
  char *ptr;
  
  VUser *vptr = TLS_GET_VPTR();
  

  ptr = getenv("JMETER_HOME");
  if(ptr == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: JMETER_HOME is not set or JMeter is not installed");
    NS_EXIT(1, "Error: JMeter not installed.");
  }
  return ns_jmeter_start_internal(vptr, ptr);
}

