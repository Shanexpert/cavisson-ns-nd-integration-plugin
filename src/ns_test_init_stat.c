/********************************************************************************
 Name    : ns_test_init_stat.c
 Feature : Test Initialization Status
 Purpose : Create stage summary and log files, overall summary etc
 C Date  : 23rd feb 2019
*********************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include "ns_test_init_stat.h"
#include "ns_alloc.h"
#include "util.h"
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_global_settings.h"
#include "ns_msg_def.h"
#include "tmr.h"
#include "timing.h"
#include "user_tables.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "ns_log.h"
#include "divide_users.h"
#include "ns_global_dat.h"
#include "wait_forever.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "output.h"
#include "ns_error_msg.h"
#include "nslb_cav_conf.h"

int max_init_stage_entries;
int total_init_stage_entries;
char testInitPath_buf[512];
TestInitStageEntry *testInitStageTable;
char g_test_init_stage_id = -1;
static char g_test_init_stage_prev[128];
static char scen_type_buf[100 + 1];
pid_t g_parent_pid;
#define PARENT_PID (g_parent_pid == getpid())
//Get duration in secs
static char *convert_time_to_human(int duration_in_secs)
{
  static char time_buf[32 + 1];
  int min, sec;

  min = duration_in_secs / 60; //convert to min
  sec = duration_in_secs % 60; //remaining in sec

  if(min)
    snprintf(time_buf, 32, "%d min %d sec", min, sec);
  else if(sec > 1)
    snprintf(time_buf, 32, "%d secs", sec);
  else
    snprintf(time_buf, 32, "%d sec", sec);
  
  return time_buf;
}

static void get_time_string(time_t time_in_secs, char *time_buf)
{
  struct tm lt;
  localtime_r(&time_in_secs, &lt);
  if(&lt != (struct tm *)NULL)
   sprintf(time_buf, "%02d/%02d/%02d %02d:%02d:%02d", lt.tm_mon + 1, lt.tm_mday, (1900 + lt.tm_year)%2000,
                      lt.tm_hour, lt.tm_min, lt.tm_sec);
  else
   strcpy(time_buf, "Error|Error");

}

void set_scenario_type()
{
  NSDL2_MISC(NULL, NULL, "Method called, testCase.mode = %d", testCase.mode);
  if(global_settings->replay_mode)
    snprintf(scen_type_buf, 100, "RAL");
  else if(testCase.mode == TC_FIX_CONCURRENT_USERS)
    snprintf(scen_type_buf, 100, "FCU - %d VUsers", global_settings->num_connections);
  else if(testCase.mode == TC_FIX_USER_RATE)
    snprintf(scen_type_buf, 100, "FSR - %.2f Sessions/min", global_settings->vuser_rpm/THOUSAND);
  else if(testCase.mode == TC_MIXED_MODE)
    snprintf(scen_type_buf, 100, "Mixed (FCU and FSR)");
  else if(testCase.mode >= TC_FIX_HIT_RATE && testCase.mode <= TC_FIX_MEAN_USERS)
    snprintf(scen_type_buf, 100, "Goal Based");
  else
    snprintf(scen_type_buf, 100, "Unknown");

  NSDL2_MISC(NULL, NULL, "scen_type_buf = %s", scen_type_buf);
}

static FILE *open_test_init_header_file(char *filename)
{
  NSDL2_MISC(NULL, NULL, "Method called");
  FILE *fp = fopen(filename, "w");
  if(!fp)
  {
    fprintf(stderr, "[Test Initialization Status] Not able to open file %s in write mode, err = %s", filename, nslb_strerror(errno));
    exit(-1);
  }

  return fp;
}

static FILE *open_summary_files(char *filename)
{
  char buf[MAX_STAGE_LINE_LENGTH + 5];

  NSDL3_MISC(NULL, NULL, "Method called, testInitPath_buf = %s, summary filename = %s", testInitPath_buf, filename);
  sprintf(buf, "%s%s.summary", testInitPath_buf, filename); //increasing stage num as moving to next stage
  FILE *fp = fopen(buf, "w");
  if(!fp)
    NS_EXIT(-1, "[Test Initialization Status] Not able to open file %s in write mode, error:%s", buf, nslb_strerror(errno));

  return fp;
}

static int open_log_files(char *filename)
{
  char buf[MAX_STAGE_LINE_LENGTH + 5];

  NSDL3_MISC(NULL, NULL, "Method called, testInitPath_buf = %s, log filename = %s", testInitPath_buf, filename);
  sprintf(buf, "%s%s.log", testInitPath_buf, filename); //increasing stage num as moving to next stage
  int fd = open(buf, O_CREAT|O_WRONLY|O_APPEND|O_TRUNC|O_CLOEXEC, 00666);
  if(fd <= 0)
    NS_EXIT(-1, "[Test Initialization Status] Not able to open file %s in write mode, err = %s", buf, nslb_strerror(errno));

  return fd;
}

static void create_init_stages_entry(int *row, char *stagename, char *stagedesc, char *filename)
{
  if(total_init_stage_entries == max_init_stage_entries)
  {
    testInitStageTable = realloc(testInitStageTable, (max_init_stage_entries + DELTA_INIT_STAGE_ENTRIES) * sizeof(TestInitStageEntry));
    max_init_stage_entries += DELTA_INIT_STAGE_ENTRIES;
  }
  *row = total_init_stage_entries++;
  sprintf(testInitStageTable[*row].stageFile, "%s", filename);
  sprintf(testInitStageTable[*row].stageName, "%s", stagename);
  sprintf(testInitStageTable[*row].stageDesc, "%s", stagedesc);
  testInitStageTable[*row].stageLog_fd = -1;
  testInitStageTable[*row].stageStatus = 0;
}

//Create blank files of X.summary and X.log files
void open_test_init_files()
{
  if(testidx <= 0)
    return;
  int stage_id;
  char cmd[MAX_STAGE_LINE_LENGTH];
  char err_msg[1024]= "\0";

  sprintf(testInitPath_buf, "%s/logs/TR%d/ready_reports/TestInitStatus/", g_ns_wdir, testidx);
  if(global_settings->continuous_monitoring_mode)
  {
    sprintf(cmd, "rm -rf %s 2>/dev/null", testInitPath_buf);
    nslb_system(cmd,1,err_msg);
  }
  if(mkdir(testInitPath_buf, 0775) != 0)
  {
    NS_EXIT(-1, CAV_ERR_1000005, testInitPath_buf, errno, nslb_strerror(errno));
  }

  for(stage_id = 0; stage_id < total_init_stage_entries; stage_id++)
  {
    if((stage_id == NS_GOAL_DISCOVERY) || (stage_id == NS_GOAL_STABILIZE))
      continue;
    if((loader_opcode != MASTER_LOADER) && ((stage_id == NS_GEN_VALIDATION) || (stage_id == NS_UPLOAD_GEN_DATA)))
      continue;
    if((loader_opcode == CLIENT_LOADER) && ((stage_id == NS_DB_CREATION) || (stage_id == NS_DIAGNOSTICS_SETUP)))
      continue;
    testInitStageTable[stage_id].stageLog_fd = open_log_files(testInitStageTable[stage_id].stageFile);
    fclose(open_summary_files(testInitStageTable[stage_id].stageFile));
  }
  snprintf(scen_type_buf, 100, "Unknown");
}

void create_test_init_stages()
{
  char file_buf[MAX_STAGE_LINE_LENGTH];
  int row_num = 0;

  g_parent_pid = getpid(); //Saving pid to avoid multiple getpid() calls
  //memset array
  memset(g_test_init_stage_prev, -1, sizeof(g_test_init_stage_prev));

  //Initialization
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_INITIALIZATION);
  create_init_stages_entry(&row_num, INITIALIZATION, DESC_INITIALIZATION, file_buf);
  //Generator Validation
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_CREATE_GEN_TEST_DATA);
  create_init_stages_entry(&row_num, CREATE_GEN_TEST_DATA, DESC_CREATE_GEN_TEST_DATA, file_buf);
  //Scenario Parsing
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_VAL_TEST_DATA);
  create_init_stages_entry(&row_num, VAL_TEST_DATA, DESC_VAL_TEST_DATA, file_buf);
  //Database Creation
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_CREATE_DB_TABLES);
  create_init_stages_entry(&row_num, CREATE_DB_TABLES, DESC_CREATE_DB_TABLES, file_buf);
  //copy scripts
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_CREATE_TEST_RUN_FILES);
  create_init_stages_entry(&row_num, CREATE_TEST_RUN_FILES, DESC_CREATE_TEST_RUN_FILES, file_buf);
  //Monitor Initialization
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_MONITOR_SETUP);
  create_init_stages_entry(&row_num, MONITOR_SETUP, DESC_MONITOR_SETUP, file_buf);
  // Session rate discovery
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_GOAL_DISCOVERY);
  create_init_stages_entry(&row_num, GOAL_DISCOVERY, DESC_GOAL_DISCOVERY, file_buf);
  // Stabilize test run
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_GOAL_STABILIZE);
  create_init_stages_entry(&row_num, GOAL_STABILIZE, DESC_GOAL_STABILIZE, file_buf);
  //netdiagnostics init
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_NETDIAGNOSTICS_SETUP);
  create_init_stages_entry(&row_num, NDC_SETUP, DESC_NETDIAGNOSTICS_SETUP, file_buf);
  //Upload Generator Data
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_UPLOAD_GEN_DATA);
  create_init_stages_entry(&row_num, UPLOAD_GEN_DATA, DESC_UPLOAD_GEN_DATA, file_buf);
  //start instance
  sprintf(file_buf, "%d%s", total_init_stage_entries + 1, F_START_LOAD_GEN);
  create_init_stages_entry(&row_num, START_LOAD_GEN, DESC_START_LOAD_GEN, file_buf);
  
  g_test_init_stage_id = NS_INITIALIZATION;
  time(&testInitStageTable[NS_INITIALIZATION].stageStartTime);
  /*Saving start data and time for overall file*/
  get_time_string(testInitStageTable[NS_INITIALIZATION].stageStartTime, testInitStageTable[NS_INITIALIZATION].stageStartDtTime);
  strcpy(g_test_start_time, testInitStageTable[NS_INITIALIZATION].stageStartDtTime);
  testInitStageTable[NS_INITIALIZATION].stageDuration = 0;
  testInitStageTable[NS_INITIALIZATION].stageStatus = 1;
}

/**********************************************************************************************************
   TestRun|Scenario|TestName|User|StartDateTime|Vusers/SessionRate|Duration|ElapsedDuration|GeneratorCount
      |---------------------------------|             |-------------------------------------| 
              parseArgs                                           parseScenario
**********************************************************************************************************/
void write_test_init_header_file(char *duration)
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;
  char buffer[MAX_STAGE_LINE_LENGTH + 1];
  char file_buf[MAX_STAGE_LINE_LENGTH + 1];
  char elapsedDuration[16];

  convert_to_hh_mm_ss((time(NULL) - testInitStageTable[0].stageStartTime) * 1000, elapsedDuration);
  sprintf(file_buf, "%soverallTestInitStat.info", testInitPath_buf);
  snprintf(buffer, MAX_STAGE_LINE_LENGTH, "%d|%s/%s/%s|%s|%s|%s|%s|%s|%s|%d", testidx, g_project_name,
                   g_subproject_name, g_scenario_name, global_settings->testname, g_test_user_name,
                   testInitStageTable[NS_INITIALIZATION].stageStartDtTime, scen_type_buf,
                   duration?duration:"00:00:00", elapsedDuration, sgrp_used_genrator_entries);

  FILE *fp = open_test_init_header_file(file_buf);
  fprintf(fp, "%s\n", buffer);
  fclose(fp);
  #endif
}

void inline append_summary_file(short stage_idx, char *buffer, FILE **sum_fp)
{
  if(testidx <= 0)
    return;

  char buf[MAX_STAGE_LINE_LENGTH + 5];

  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d, buffer = %s, sum_fp = %p", stage_idx, buffer, *sum_fp);
  if(!*sum_fp)
  {
    sprintf(buf, "%s%s.summary", testInitPath_buf, testInitStageTable[stage_idx].stageFile);
    *sum_fp = fopen(buf, "a");
    if(!*sum_fp)
      NS_EXIT(-1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }
  fprintf(*sum_fp, "%s", buffer);
}

void inline write_summary_file(short stage_idx, char *buffer)
{
  if(testidx <= 0)
    return;

  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d, buffer = %s", stage_idx, buffer);
  FILE *fp = open_summary_files(testInitStageTable[stage_idx].stageFile);
  fprintf(fp, "%s\n", buffer);
  fclose(fp);
}

void inline write_log_file(short stage_idx, char *format, ...)
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;
  char time_str[100];
  int amt_written, buf_len;
  va_list ap;

  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d, buffer = %p", stage_idx, g_tls.buffer);

  amt_written = snprintf(g_tls.buffer, g_tls.buffer_size, "%s|", nslb_get_cur_date_time(time_str, 1));
  va_start (ap, format);
  buf_len = vsnprintf(g_tls.buffer + amt_written, g_tls.buffer_size - amt_written, format, ap);
  va_end(ap);

  //format wrong then vsnprintf returns -1
  if(buf_len < 0)
    buf_len = strlen(g_tls.buffer);
  else
    buf_len += amt_written;
  /* This condition is to bound g_tls.buffer under the size VUSER_THREAD_BUFFER_SIZE-2 */   
  if(buf_len >= VUSER_THREAD_BUFFER_SIZE-2)
    buf_len = VUSER_THREAD_BUFFER_SIZE-2;
  g_tls.buffer[buf_len] = '\n';
  g_tls.buffer[buf_len + 1] = '\0';

  NSDL2_MISC(NULL, NULL, "stage_idx = %d, buffer = %s", stage_idx, g_tls.buffer);
  /* current time|message  */

  if(testInitStageTable[stage_idx].stageLog_fd < 0)
     testInitStageTable[stage_idx].stageLog_fd = open_log_files(testInitStageTable[stage_idx].stageFile);

  write(testInitStageTable[stage_idx].stageLog_fd, g_tls.buffer, buf_len+1);
  /* last stage and stage status is finished */
  /* As g_tls is getting freed during thread exit, so commented below  */
 // if((stage_idx == NS_START_INST) && (testInitStageTable[stage_idx].stageStatus == TIS_FINISHED))
 //   FREE_AND_MAKE_NOT_NULL(buffer, "stage log buffer", -1);
 #endif
}

void write_init_stage_files(short stage_idx)
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;

  char buffer[MAX_STAGE_LINE_LENGTH + 1];
  snprintf(buffer, MAX_STAGE_LINE_LENGTH, "%s|%s|%s|%s|%d", testInitStageTable[stage_idx].stageName,
                  testInitStageTable[stage_idx].stageStartDtTime, testInitStageTable[stage_idx].stageDesc,
                  convert_time_to_human(testInitStageTable[stage_idx].stageDuration),
                  testInitStageTable[stage_idx].stageStatus);
  write_summary_file(stage_idx, buffer);
  sprintf(buffer, "Stage [%s] started", testInitStageTable[stage_idx].stageName);
  write_log_file(stage_idx, "%s", buffer);
  write_test_init_header_file(NULL);
  #endif
}

//stage id is number but index should be stage id - 1
void init_stage(short stage_idx)
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;
  char buffer[MAX_STAGE_LINE_LENGTH + 1];
  if((stage_idx != NS_DB_CREATION) && (stage_idx != NS_COPY_SCRIPTS))
  {
    g_test_init_stage_prev[stage_idx] = g_test_init_stage_id;
    g_test_init_stage_id = stage_idx;
  }
  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d", stage_idx);
  time(&testInitStageTable[stage_idx].stageStartTime); //saving in secs
  get_time_string(testInitStageTable[stage_idx].stageStartTime, testInitStageTable[stage_idx].stageStartDtTime);
  testInitStageTable[stage_idx].stageDuration = 0;
  testInitStageTable[stage_idx].stageStatus = ((stage_idx == NS_COPY_SCRIPTS)?TIS_FINISHED:TIS_RUNNING);
  snprintf(buffer, MAX_STAGE_LINE_LENGTH, "%s|%s|%s|%s|%d", testInitStageTable[stage_idx].stageName,
                  testInitStageTable[stage_idx].stageStartDtTime, testInitStageTable[stage_idx].stageDesc,
                  convert_time_to_human(testInitStageTable[stage_idx].stageDuration),
                  testInitStageTable[stage_idx].stageStatus);
  write_summary_file(stage_idx, buffer);
  sprintf(buffer, "Stage [%s] started", testInitStageTable[stage_idx].stageName);
  write_log_file(stage_idx, "%s", buffer);
  write_test_init_header_file(NULL);
  #endif
}

//stage id is number but index should be stage id - 1
void end_stage(short stage_idx, char stagestatus, char *format, ...)
{
  #ifndef CAV_MAIN
  if((testidx <= 0) || !PARENT_PID)
    return;
  int amt_written = 0, amt_written1;
  char time_str[100];
  va_list ap;

  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d, stagestatus = %d, g_stage_id = %d, g_prev_id = %d",
                          stage_idx, stagestatus, g_test_init_stage_id, g_test_init_stage_prev[stage_idx]);
  /* Both NS_DB_CREATION and NS_COPY_SCRIPTS are parallel stages as they take time for completion
     Other stages should end */
  if((stage_idx != NS_DB_CREATION) && (stage_idx != NS_COPY_SCRIPTS) &&
     (g_test_init_stage_id == -1 || g_test_init_stage_prev[stage_idx] == -2))
  {
    NSDL2_MISC(NULL, NULL, "Unable to end stage as stage not yet started or all stages ended");
    return;
  }
  testInitStageTable[stage_idx].stageDuration = (int) time(NULL) - testInitStageTable[stage_idx].stageStartTime;
  testInitStageTable[stage_idx].stageStatus = stagestatus;
  amt_written = snprintf(g_tls.buffer, g_tls.buffer_size, "%s|%s|%s|%s|%d", testInitStageTable[stage_idx].stageName, testInitStageTable[stage_idx].stageStartDtTime, testInitStageTable[stage_idx].stageDesc, convert_time_to_human(testInitStageTable[stage_idx].stageDuration),testInitStageTable[stage_idx].stageStatus);

  if(stagestatus == TIS_ERROR)
  {
    strcat(g_tls.buffer, "\n");
    amt_written++;
    va_start (ap, format);
    amt_written1 = vsnprintf(g_tls.buffer + amt_written, g_tls.buffer_size - amt_written, format, ap);
    va_end(ap);

    if(amt_written1 <= 0)
      amt_written1 = strlen(g_tls.buffer+amt_written);
    write(testInitStageTable[stage_idx].stageLog_fd, g_tls.buffer+amt_written, amt_written1);
  }

  write_summary_file(stage_idx, g_tls.buffer);
  amt_written = snprintf(g_tls.buffer, g_tls.buffer_size, "%s|Stage [%s] finished\n", nslb_get_cur_date_time(time_str, 1),
                         testInitStageTable[stage_idx].stageName);
  write(testInitStageTable[stage_idx].stageLog_fd, g_tls.buffer, amt_written);
  close(testInitStageTable[stage_idx].stageLog_fd);
  testInitStageTable[stage_idx].stageLog_fd = -1;

  if((stage_idx != NS_DB_CREATION) && (stage_idx != NS_COPY_SCRIPTS))
  {
    g_test_init_stage_id = g_test_init_stage_prev[stage_idx];
    g_test_init_stage_prev[stage_idx] = -2;
  }

  if(stagestatus == TIS_ERROR)
  {
    int err_stage = 0;
    if(stage_idx == NS_DB_CREATION)
      err_stage = NS_SCENARIO_PARSING;
    else if(stage_idx == NS_SCENARIO_PARSING)
      err_stage = NS_DB_CREATION;

    //we are finishing stage db when db got started and exitted on parsing
    if(err_stage && testInitStageTable[err_stage].stageStatus == TIS_RUNNING)
    {
      snprintf(g_tls.buffer, g_tls.buffer_size, "%s|%s|%s|%s|%d", testInitStageTable[err_stage].stageName,
                      testInitStageTable[err_stage].stageStartDtTime, testInitStageTable[err_stage].stageDesc,
                      convert_time_to_human(testInitStageTable[err_stage].stageDuration), TIS_FINISHED);
      write_summary_file(err_stage, g_tls.buffer);
    }
  }

  if(stage_idx == NS_START_INST)
    write_test_init_header_file(target_completion_time);
  else if(stage_idx != NS_COPY_SCRIPTS) //since COPY_SCRIPT is now parallel
    write_test_init_header_file(NULL);
  #endif
}

void update_summary_desc(short stage_idx, char *desc)
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;
  char buffer[MAX_STAGE_LINE_LENGTH + 1];
  NSDL2_MISC(NULL, NULL, "Method called, stage_idx = %d, desc = %s", stage_idx, desc);
  testInitStageTable[stage_idx].stageDuration = (int) time(NULL) - testInitStageTable[stage_idx].stageStartTime;
  testInitStageTable[stage_idx].stageStatus = 1; //progress
  snprintf(buffer, MAX_STAGE_LINE_LENGTH, "%s|%s|%s|%s|%d", testInitStageTable[stage_idx].stageName,
           testInitStageTable[stage_idx].stageStartDtTime, desc, convert_time_to_human(testInitStageTable[stage_idx].stageDuration),
           testInitStageTable[stage_idx].stageStatus);

  write_summary_file(stage_idx, buffer);
 #endif
}

void rem_invalid_stage_files()
{
  #ifndef CAV_MAIN
  if(testidx <= 0)
    return;
  char buf[MAX_STAGE_LINE_LENGTH];
  if (global_settings->script_copy_to_tr == DO_NOT_COPY_SRCIPT_TO_TR)
  {
    sprintf(buf, "%s/%s.summary", testInitPath_buf, testInitStageTable[NS_COPY_SCRIPTS].stageFile);
    unlink(buf);
    sprintf(buf, "%s/%s.log", testInitPath_buf, testInitStageTable[NS_COPY_SCRIPTS].stageFile);
    unlink(buf);
  }

  //If not generator and DB is disable then delete file
  if((loader_opcode != CLIENT_LOADER) && !global_settings->reader_run_mode)
  {
    sprintf(buf, "%s/%s.summary", testInitPath_buf, testInitStageTable[NS_DB_CREATION].stageFile);
    unlink(buf);
    sprintf(buf, "%s/%s.log", testInitPath_buf, testInitStageTable[NS_DB_CREATION].stageFile);
    unlink(buf);
  }
  
  //If not generator and nde mode disable then delete file
  if((loader_opcode != CLIENT_LOADER) && !global_settings->net_diagnostics_mode)
  {
    sprintf(buf, "%s/%s.summary", testInitPath_buf, testInitStageTable[NS_DIAGNOSTICS_SETUP].stageFile);
    unlink(buf);
    sprintf(buf, "%s/%s.log", testInitPath_buf, testInitStageTable[NS_DIAGNOSTICS_SETUP].stageFile);
    unlink(buf);
  }
  #endif
}
