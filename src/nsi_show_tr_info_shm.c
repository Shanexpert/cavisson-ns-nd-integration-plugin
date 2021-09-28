/***********Header***************
nsi_show_tr_shm: Tool to dump shared memory info for given TR
               

Usage        : nsi_show_tr_shm -c <test_run_num> -m <shm_id>

************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/shm.h>

#include "nslb_util.h"
#include "nslb_partition.h"
#include "logging.h"

// Dump shared memory data
static void dump_shm_data(int shm_id){

  TestRunInfoTable_Shr *testruninfo_table;

  testruninfo_table = (TestRunInfoTable_Shr*)shmat(shm_id, NULL, 0);

  if(testruninfo_table == NULL){
    fprintf(stderr, "Error:Unable to get the shared memory content for id %d\n", shm_id);
    exit(-1);
  }

  fprintf(stdout, "Partition_Name = %s\n"
                  "TR_Num = %d\n"
                  "Partition_Idx = %lld\n"
                  "Absolute_Time = %ld\n"
                  "Cav_epoch_Diff = %ld\n"
                  "Parent_PID = %d\n"
                  "Logging_Writer_PID = %d\n"
                  "Logging_Reader_PID = %d\n"
                  "Test_Status = %d\n"
                  "Num_NVM = %d\n"
                  "Ns_Port = %d\n"
                  "Big_Buff_Shm_ID = %d\n"
                  "NLM_Port = %d\n"
                  "Reader_Run_Mode = %d",
                  testruninfo_table->partition_name,
                  testruninfo_table->tr_number,
                  testruninfo_table->partition_idx,
                  testruninfo_table->absolute_time,
                  testruninfo_table->cav_epoch_diff,
                  testruninfo_table->ns_parent_pid,
                  testruninfo_table->ns_lw_pid,
                  testruninfo_table->ns_lr_pid,
                  testruninfo_table->test_status,
                  testruninfo_table->num_nvm,
                  testruninfo_table->ns_port,
                  testruninfo_table->big_buff_shm_id,
                  testruninfo_table->ns_log_mgr_port,
                  testruninfo_table->reader_run_mode);

}

static void print_usage(char *bin_name)
{
  fprintf(stderr,"Usage: %s -c <test run number> -m <shm_id>\n", bin_name);
  exit(-1);
}

int main(int argc, char *argv[])
{
  int test_run_num = 0;
  int shm_id = -1;
  extern char *optarg;
  int test_run_flag = 0;
  int shm_id_flag = 0;
  int ns_pid;
  int ret;
  int c;

  while ((c = getopt(argc, argv, "c:m:")) != -1)
  {
    switch (c)
    {
      case 'c':
        test_run_num = atoi(optarg);
        test_run_flag++;
        if(test_run_num <= 0){
          fprintf(stderr,"test_run_num %d should be greater than 0\n", test_run_num);
          exit(-1);
        }
        break;

      case 'm':
        shm_id = atoi(optarg);
        shm_id_flag++;
        if(shm_id <= 0){
          fprintf(stderr,"shm_id %d should be greater than 0\n", shm_id);
          exit(-1);
        }
        break;
      case ':':
      case '?':
        print_usage(argv[0]);
        break;
      }
  } /* while */

  if(!(test_run_flag && shm_id_flag)){
    fprintf(stderr,"Missing mandatory argument. All arguments are mandatory\n");
    print_usage(argv[0]);
  }
 
  ns_pid = get_ns_pid(test_run_num); /* This call will exit if failed. */
 
  if(is_test_run_running(ns_pid) != 0) // If test run number is not running then exit
  {
    fprintf(stderr, "Test run number %d is not running\n", test_run_num);
    exit(-1);
  }

  dump_shm_data(shm_id);

  if((ret = shmdt(NULL)) != 0){
    fprintf(stderr, "Error in deattaching shared memory for id = [%d], Error = [%s]\n", shm_id, nslb_strerror(errno)); 
    exit(-1);
  }

  return 0;
}
