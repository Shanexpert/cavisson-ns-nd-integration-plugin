#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logging.h"
#include "tmr.h"
#include "ns_msg_def.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "nslb_comp_recovery.h"
#include "ns_monitoring.h"
#include "ns_percentile.h"
#include "ns_license.h"
#include "nia_fa_function.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "wait_forever.h"
#include "nslb_util.h"
#include "nslb_multi_thread_trace_log.h"
#include "ns_exit.h"
#include "ns_alloc.h"
#include "util.h"
#define MAX_RECOVERY_COUNT                5
#define RESTART_TIME_THRESHOLD_IN_SEC   900

int nia_file_aggregator_pid = -1;
ComponentRecoveryData aggregatorRecoveryData;

//Method to recovery nia_file_aggregator only called in controller mode
int nia_file_aggregator_recovery(int loader_opcode)
{
  NSDL2_LOGGING(NULL, NULL, "Method called");
  if (nia_file_aggregator_pid != -1)
    return -1;
  
  if(nslb_recover_component_or_not(&aggregatorRecoveryData) == 0)
  {
    fprintf(stderr, "Main thread, nia_file_aggregator not running. Recovering file aggregator\n");
    NSDL2_LOGGING(NULL, NULL, "Main thread, nia_file_aggregator not running. Recovering file aggregator.");

    if(create_nia_file_aggregator_process(global_settings->num_process, 1, loader_opcode) == 0)
    { 
      NSDL2_LOGGING(NULL, NULL, "Main thread, nia_file_aggregator recovered, Pid = %d.", nia_file_aggregator_pid);
    } 
    else    
    {
      NSDL2_LOGGING(NULL, NULL, "Main thread, nia_file_aggregator recovery failed");
      return -1;
    }
  }
  else
  {
    NSDL2_LOGGING(NULL, NULL, "Main thread, nia_file_aggregator max restart count is over. Cannot recover nia_fa"
          " Retry count = %d, Max Retry count = %d", aggregatorRecoveryData.retry_count, aggregatorRecoveryData.max_retry_count);
    return -1;
  }

  return 0;
}


int kw_set_nifa_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int level;
  int size = 0;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
        
  num = sscanf(buf, "%s %d %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments in line %s.", buf);
  }
  
  if (level < 0 || level > 8)
  {
    fprintf(stderr, "Invaid file aggregator level %d, hence setting to default level 1\n", level);
    level = 1;
  }

  if (size <= 0){
    if(num >= 3) //checking if size is provided by user or not, if yes then show warning
    fprintf(stderr, "Invalid file aggregator file size %d, hence setting to default size 10 MB;\n", size);
    size = 10;
  }

  global_settings->nifa_trace_file_sz = size*1024*1024;
  global_settings->nifa_trace_level = level;

  return 0;
}

int create_nia_file_aggregator_process(int num_child, int recovery_flag, int loader_opcode)
{
  NSDL1_LOGGING(NULL, NULL, "Method called");

  if((nia_file_aggregator_pid = fork()) == 0){
     char tr_num[50];
     char reader_mode[8];
     char test_run_info_shr_mem_key_str[16];

     char fa_bin_path[512 + 1] = "";
     char wdir[512] = "";
     char pct_mode[8];
     char ppid_str[16];
     char dlogBufSize[16];
     char pctBufSize[32];
     char reader_write_time_str[16];
     char dynamic_table_size_str[16];
     char gen_id_str[16];
     char cav_epoch_year_str[16];
     char num_nvm_str[16];
     char num_gen_str[16];
     char logging_reader_trace_level[16];
     char resume_flag[5];
     char loader_opcode_val[5];
     char rbu_flag[2];
     //char rbu_lighthouse_flag[2];
     int ret = 0;
     int ret1=0;
     char err_msg[100];

     if(getenv("NS_WDIR") == NULL)
       strcpy(wdir, "/home/cavisson/work");
     else
       strcpy(wdir, getenv("NS_WDIR"));

     sprintf(tr_num, "%d", testidx);
     sprintf(reader_write_time_str, "%d", global_settings->reader_csv_write_time);
     sprintf(dynamic_table_size_str, "%d", global_settings->url_hash.dynamic_url_table_size);
     sprintf(gen_id_str, "%d", g_generator_idx);
     sprintf(cav_epoch_year_str, "%d", global_settings->cav_epoch_year);
     sprintf(num_nvm_str, "%d", global_settings->max_num_nvm_per_generator);
     sprintf(num_gen_str, "%d", sgrp_used_genrator_entries);
     sprintf(logging_reader_trace_level, "%d", global_settings->nlr_trace_level);

     //sprintf(reader_mode, "%d", global_settings->reader_run_mode);
     sprintf(reader_mode, "%d", 1);
     sprintf(test_run_info_shr_mem_key_str, "%d", test_run_info_shr_mem_key);
     sprintf(fa_bin_path, "%s/bin/nia_file_aggregator", wdir);
     //sprintf(pct_mode, "%d", g_percentile_report);
     sprintf(pct_mode, "%d", g_percentile_report);
     sprintf(ppid_str, "%d", getppid());
     sprintf(pctBufSize, "%d", total_pdf_data_size);
     sprintf(dlogBufSize, "%d", global_settings->log_shr_buffer_size);
     sprintf(resume_flag, "%d", recovery_flag);
     sprintf(loader_opcode_val, "%d", loader_opcode);

     if(global_settings->protocol_enabled & RBU_API_USED) 
       sprintf(rbu_flag, "%d", 1);
     else
       sprintf(rbu_flag, "%d", 0);

     NSDL1_LOGGING(NULL, NULL, "global_settings->protocol_enabled = %0x, rbu_flag = %s,",
                                 global_settings->protocol_enabled, rbu_flag);

     //write nia_file_aggregator pid in get_all_process_pid file.
     ret = nslb_write_all_process_pid(getpid(), "nia file aggregator's pid", wdir, testidx, "a");
     if( ret == -1 )
     {
        fprintf(stderr, "failed to open the nia file aggregator's pid file\n");
        END_TEST_RUN;
     }
     ret1 = nslb_write_process_pid(getpid(), "nia file aggregator's pid",wdir, testidx, "w","NIFA.pid",err_msg);
     if(ret1 == -1) 
     {
        NSTL1(NULL,NULL,"%s",err_msg);
     }

     ret = nslb_write_all_process_pid(getppid(), "nia file aggregator's parent's pid", wdir, testidx, "a");
     if( ret == -1 )
     {
        fprintf(stderr, "failed to open the nia file aggregator's parent's pid file\n");
        END_TEST_RUN;
     }
     NSDL2_LOGGING(NULL, NULL, "Running nia_file_aggregator, %s --testrun %s --shmkey %s --readermode %s --ppid %s"
                               " --dlog_bufsize %s -T %s -D %s -g %s -c %s -n %s, -G %s -o %s -R %s", 
                               fa_bin_path, tr_num, test_run_info_shr_mem_key_str, reader_mode, ppid_str, 
                               dlogBufSize, reader_write_time_str, dynamic_table_size_str, gen_id_str,     
                           cav_epoch_year_str, num_nvm_str, num_gen_str, loader_opcode_val, rbu_flag);

     CLOSE_INHERITED_FD_FROM_CHILD  //closing control and data listen fd from child 

      if (execlp(fa_bin_path, fa_bin_path, "--testrun", tr_num, "--shmkey", test_run_info_shr_mem_key_str, 
                 "--readermode", reader_mode, "--ppid", ppid_str, "--dlog_bufsize", dlogBufSize, 
                 "-T", reader_write_time_str, "-D", dynamic_table_size_str, "-g", gen_id_str, 
                 "-c", cav_epoch_year_str, "-n", num_nvm_str, "-G", num_gen_str,  "-L", logging_reader_trace_level, 
                 "-r", resume_flag, "-o", loader_opcode_val, "-R", rbu_flag, NULL) == -1)
      {
        NS_EXIT(1, "Error in initializing nia_file_aggregator: error in execl");
        perror("execl");
      }
  }else{
    if(nia_file_aggregator_pid < 0){
      NSDL1_LOGGING(NULL, NULL, "Error: Unable to fork nia_file_aggregator");
      if(recovery_flag == 0){
        NS_EXIT(-1, "recovery_flag should not be 0");
      }
      else 
         return -1;
    }
  }

  set_nia_file_aggregator_pid(nia_file_aggregator_pid);
  return 0;
}

void init_component_rec_and_start_nia_fa(int loader_opcode)
{
   if(nslb_init_component_recovery_data(&aggregatorRecoveryData, MAX_RECOVERY_COUNT, (global_settings->progress_secs/1000 + 5), RESTART_TIME_THRESHOLD_IN_SEC) == 0)
     {
       NSDL2_LOGGING(NULL, NULL, "Main thread Recovery data initialized with"
                      "MAX_RECOVERY_COUNT = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                             global_settings->progress_secs , RESTART_TIME_THRESHOLD_IN_SEC);
     }
     else
     {
       NSDL2_LOGGING(NULL, NULL, "Method Called. Component recovery could not be initialized");
     }

     create_nia_file_aggregator_process(global_settings->max_num_nvm_per_generator, 0, loader_opcode);
}
