/*********************************************************************************************
* Name                   : ns_parse_netcloud_keyword.c 
* Purpose                : This file holds the functions to parse scenario keywords used for NetCloud feature. 
* Author                 : Manpreet Kaur
* Intial version date    : Monday, August 26 2013 
* Last modification date :  
*********************************************************************************************/

#define _GNU_SOURCE
#include <libgen.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_sock.h"
#include "smon.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_string.h"
#include "nslb_util.h"
#include "ns_parent.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_data_handler_thread.h"
#include "ns_global_settings.h"

extern int check_pid_timestamp(int pid, char *pid_file);

int num_gen_expected;
int kw_set_ns_gen_fail(char *buf, char *err_msg, int runtime_flag)
{       
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp_data[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int num;
  char tmp_val; 
  char tmp_fail_gen[MAX_DATA_LINE_LENGTH];
  char tmp_start_gen_percent[MAX_DATA_LINE_LENGTH]; //start percent
  char tmp_running_gen_percent[MAX_DATA_LINE_LENGTH]; //running percent
  char tmp_gen_start_timeout[MAX_DATA_LINE_LENGTH];
  char tmp_percent;

  num = sscanf(buf, "%s %s %s %s %s %s %s", keyword, tmp_data, tmp_fail_gen, tmp_start_gen_percent, tmp_running_gen_percent,
                     tmp_gen_start_timeout, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf=%s mode=%s num_progress_fail_gen=%s percent of gen started=%s percent of gen running=%s"
            " gen start timeout=%s", buf, tmp_data, tmp_fail_gen, tmp_start_gen_percent, tmp_running_gen_percent, tmp_gen_start_timeout);

  if (num != 6)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_1);
      
  if(ns_is_numeric(tmp_data) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_2);

  tmp_val = (char)atoi(tmp_data);
  if(tmp_val < 0 || tmp_val > 1)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_3);

  global_settings->con_test_gen_fail_setting.mode = tmp_val;

  if(ns_is_numeric(tmp_fail_gen) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_2);

  tmp_percent = (char)atoi(tmp_fail_gen);
  if(tmp_percent < 1 || tmp_percent > 100)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_6);

  global_settings->con_test_gen_fail_setting.num_sample_delay_allowed = tmp_percent;

  if(ns_is_numeric(tmp_start_gen_percent) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_2);

  tmp_percent = (char)atoi(tmp_start_gen_percent);
  if(tmp_percent < 0 || tmp_percent > 100)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_6);
  
  global_settings->con_test_gen_fail_setting.percent_started = tmp_percent;

  if(ns_is_numeric(tmp_running_gen_percent) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_2);

  tmp_percent = (char)atoi(tmp_running_gen_percent);
  if(tmp_percent < 0 || tmp_percent > 100)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_6);

  if(!tmp_val) //If any generator fails then all generator will be killed
    tmp_percent = 100;
  global_settings->con_test_gen_fail_setting.percent_running = tmp_percent;

  if(ns_is_numeric(tmp_gen_start_timeout) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011263, CAV_ERR_MSG_2);

  num = atoi(tmp_gen_start_timeout);
  if(num < 0 || num >= 3600)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_TEST_ON_GEN_FAILURE_USAGE, CAV_ERR_1011264, "");
  
  //Bug: 71917 (Gaurav) -> Need to change Generators start timeout according to Debug flag,
  //     If test is running in Debug mode then it takes too much time (>900 secs) to start test so continue test update it to 1800 secs.
  #ifdef NS_DEBUG_ON
  if(num == NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_NON_DEBUG) //default value is changed from 900 to 1800 in debug mode
    num = NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_DEBUG; //increasing timeout for processing

  NS_DUMP_WARNING("Generator start test timeout '%d Secs' is not enough to run test in debug mode. Setting timeout to '%d Secs'",
                  NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_NON_DEBUG, NS_GEN_START_TEST_DEFAULT_TIMEOUT_FOR_DEBUG);
  #endif
  global_settings->con_test_gen_fail_setting.start_timeout = num;

  if(num && (num < global_settings->parent_child_con_timeout)) {
    NSTL1(NULL, NULL, "Warning: Generator start test timeout %d is less than parent child connection timeout %d."
                      " Setting timeout as parent child connection timeout", num, global_settings->parent_child_con_timeout);
    NS_DUMP_WARNING("Generator start test timeout %d is less than parent child connection timeout %d."
                    " Setting timeout as parent child connection timeout", num, global_settings->parent_child_con_timeout);
    global_settings->con_test_gen_fail_setting.start_timeout = global_settings->parent_child_con_timeout;
  }

  sgrp_used_genrator_entries = max_used_generator_entries;
  if(global_settings->con_test_gen_fail_setting.percent_started == 100)
    num_gen_expected = sgrp_used_genrator_entries;
  else
    num_gen_expected = (global_settings->con_test_gen_fail_setting.percent_started * sgrp_used_genrator_entries)/100;

  NSDL3_PARSING(NULL, NULL, "Method exited, mode=%d num_progress_fail_gen=%d percent of gen started=%d percent of gen running=%d"
                   " gen start timeout=%d, num_gen_expected = %d", global_settings->con_test_gen_fail_setting.mode,
                  global_settings->con_test_gen_fail_setting.num_sample_delay_allowed,
                global_settings->con_test_gen_fail_setting.percent_started, global_settings->con_test_gen_fail_setting.percent_running,
              global_settings->con_test_gen_fail_setting.start_timeout, num_gen_expected);

  return 0;
}


//For print the use of CONTROLLER_IP keyword
static void ctrl_server_usages(char *err)
{
  fprintf(stderr, "Error: %s\n", err);
  fprintf(stderr, "  Usage: CONTROLLER_IP <SERVER IP> <PORT>\n");
  fprintf(stderr, "  This keyword is use to give controller's ip address and port\n");
  exit(-1);
}

//CONTROLLER_IP <serverip>
void kw_set_controller_server_ip (char *buf, int flag, char *out_buff, int *out_port)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp_buf[MAX_DATA_LINE_LENGTH] = "\0";
  int num;
  char *hptr;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  if ((num = sscanf(buf, "%s %s %s", keyword, text, tmp_buf)) < 2) {
    ctrl_server_usages("Invalid number of fields");
  }

  if((hptr = nslb_split_host_port(text, out_port)) == NULL)
  {
    NS_EXIT(-1, "Invalid input [%s]", buf);
  }
  strncpy(out_buff, hptr, MAX_LPS_SERVER_NAME_SIZE);
  out_buff[MAX_LPS_SERVER_NAME_SIZE] = '\0';

  //strcpy(out_buff, nslb_split_host_port(text, out_port));
  NSDL2_PARSING(NULL, NULL, "Controller ip = %s, port = %d", out_buff, *out_port);
}

static void memory_based_fs_usage(char *buf, char *msg)
{
  NSTL1_OUT(NULL, NULL, "Error: %s\n%s", msg, buf);
  NSTL1_OUT(NULL, NULL, "  Usage: ENABLE_TMPFS <mode> <size in GB>");
  NSTL1_OUT(NULL, NULL, "  This keyword is enable/disable memory based file system to operate on");
  NSTL1_OUT(NULL, NULL, "  mkdir /mnt/tmp; chmod 777 /mnt/tmp; mount -t tmpfs -o size=10G tmpfs /mnt/tmp");
  NSTL1_OUT(NULL, NULL, "  10 GB size is recommended for machines having RAM >= 96 GB");
  NS_EXIT(-1, "Error: %s\n Usage: ENABLE_TMPFS <mode> <size in GB>", msg)
}

static int check_tmpfs_mounted(char *err_msg)
{
  char buf[1024] = {0};
  char cmd_out[1024] = {0};
  int ret;

  NSDL2_PARSING(NULL, NULL, "Method called");

  sprintf(buf, "df -h %s 2>/dev/null| grep -w tmpfs >/dev/null 2>&1; echo $?", memory_based_fs_mnt_path);
  ret = nslb_run_cmd_and_get_last_line (buf, 1024, cmd_out);

  if(ret < 0)
  {
    sprintf(err_msg, "Not able to run command '%s' in function kw_enable_tmpfs", buf);
    return -1;
  }

  if(cmd_out[0] != '0')
  {
    sprintf(err_msg, "tmpfs is not mounted on %s. Please manually mount tmpfs on %s. tmpfs will be mounted by default,"
                     " if RAM size is >= 96GB", memory_based_fs_mnt_path, memory_based_fs_mnt_path);
    return -1;
  }
  return 0;
}

//Bug 79789 - CLean curent ns instance files on completion of test execution
void ns_clean_current_instance()
{
  char buf[MAX_DATA_LINE_LENGTH + 1];
  char mnt_path[MAX_DATA_LINE_LENGTH + 1];
  sighandler_t prev_handler;

  NSDL3_PARENT(NULL, NULL, "Method called.");
  
  snprintf(mnt_path, MAX_DATA_LINE_LENGTH, "%s/%s", memory_based_fs_mnt_path, basename(g_ns_wdir));
  snprintf(buf, MAX_DATA_LINE_LENGTH, "rm -rf %s/.tmp/%s %s/logs/TR%d %s/.tmp/%s", mnt_path, g_test_inst_file,
                mnt_path, testidx, g_ns_wdir, g_test_inst_file);
  NSDL3_PARENT(NULL, NULL, "buf = %s", buf);

  prev_handler = signal(SIGCHLD, SIG_IGN);
  system(buf);
  (void) signal( SIGCHLD, prev_handler);
}

void clean_up_tmpfs_file(int mode)
{
  char buf[MAX_DATA_LINE_LENGTH];
  char tr_path[MAX_DATA_LINE_LENGTH + 1];
  char filename[MAX_DATA_LINE_LENGTH + 1];
  char pid_line[32 + 1];
  char base_tmpfs[MAX_DATA_LINE_LENGTH];
  int pid;
  FILE *pid_fp, *find_cmd_fp;

  NSDL2_PARENT(NULL, NULL, "Method called");
  sprintf(tr_path, "%s/%s/logs", memory_based_fs_mnt_path, basename(g_ns_wdir));
  strcpy(buf, tr_path);
  strcpy(base_tmpfs, dirname(buf)); //as dirname may modify the content of buf provided
  /*Delete old testruns which are accidently stopped in between*/
  NSDL3_PARENT(NULL, NULL, "mode %d, tr_path = %s, base_tmpfs = %s", mode, tr_path, base_tmpfs);
  if(mode == DLT_OLD_TESTRUN)
  {
    snprintf(buf, MAX_DATA_LINE_LENGTH, "find %s/TR* -name nspid 2>/dev/null", tr_path);
    if((find_cmd_fp = popen(buf, "r")))
    {
      while(nslb_fgets(filename, MAX_DATA_LINE_LENGTH, find_cmd_fp, 1))
      {
        filename[strlen(filename) -1] = '\0';
        pid_fp = fopen(filename, "r");
        //unable to open pid file
        if(!pid_fp)
          continue;
 
        if(!nslb_fgets(pid_line, 32, pid_fp, 1)) {
          fclose(pid_fp);
          continue;
        }

        fclose(pid_fp);
        pid = atoi(pid_line);
        sprintf(tr_path, "%s", dirname(filename));
        if(!check_pid_timestamp(pid, tr_path)) //pid is not running
        {
          sprintf(buf, "rm -rf %s %s/.tmp/*_%d", tr_path, base_tmpfs, pid);
          NSDL3_PARENT(NULL, NULL, "buf = %s", buf);
          system(buf);
        }
      }
      pclose(find_cmd_fp); //close running command
    }
  }
  else /*When current TR is successfully completed upload generator data stage */
  {
    sprintf(tr_path, "%s/TR%d", tr_path, testidx);
    sprintf(buf, "rm -f %s; mkdir -p %s; mv -f %s/.controller/*.log %s; rm -rf %s", controller_dir, controller_dir,
                  tr_path, controller_dir, tr_path);
    system(buf);
  }

  NSDL2_PARENT(NULL, NULL, "Method exit");
}

char memory_based_fs_mode;
short memory_based_fs_size;
char memory_based_fs_mnt_path[512];
void kw_enable_tmpfs(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_buf[MAX_DATA_LINE_LENGTH];
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  char size_buf[MAX_DATA_LINE_LENGTH];
  char tmp_fs_path[MAX_DATA_LINE_LENGTH] = "/mnt/tmp";
  char tmp_fs_dir[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  char fname[512];
  int num;
  struct stat st;

  num = sscanf(buf, "%s %s %s %s %s", keyword, mode_buf, size_buf, tmp_fs_path, tmp_buf);

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s, num = %d", buf, num);

  if(num < 2 || num > 4)
    memory_based_fs_usage(buf, "Invalid number of arguments");
  
  if(!ns_is_numeric(mode_buf))
    memory_based_fs_usage(buf, "Mode should be an integer");

  num = atoi(mode_buf);
  if(num < 0 || num > 1)
    memory_based_fs_usage(buf, "Mode should be 0 or 1");
    
  memory_based_fs_mode = num;

  if(tmp_fs_path[0] != '/')
    memory_based_fs_usage(buf, "Memory based file system should be mounted on absolute path. Please provide absolute path");

  strcpy(memory_based_fs_mnt_path, tmp_fs_path);
  /*Mode is enabled and tmpfs disk space is present then move instance to /mnt/tmp */
  if(memory_based_fs_mode && !stat(tmp_fs_path, &st) && (st.st_mode & S_IFDIR))
  {
    if(check_tmpfs_mounted(err_msg) < 0)
    {
      NS_DUMP_WARNING("%s directory is not present, tmpfs may not be mounted on %s, error: %s,"
                      " tmpfs linking is disabled", tmp_fs_path, tmp_fs_path, err_msg);
      memory_based_fs_mode = 0;
      return;
    }
    sprintf(tmp_fs_dir, "%s/%s", tmp_fs_path, basename(g_ns_wdir));
    sprintf(tmp_buf, "mkdir -p %s/.tmp %s/logs/TR%d; mv %s/.tmp/%s %s/.tmp; ln -s %s/.tmp/%s %s/.tmp/%s; echo %d >%s/logs/TR%d/nspid",
                     tmp_fs_dir, tmp_fs_dir, testidx, g_ns_wdir, g_test_inst_file, tmp_fs_dir, tmp_fs_dir, g_test_inst_file,
                     g_ns_wdir, g_test_inst_file, g_parent_pid, tmp_fs_dir, testidx);
    system(tmp_buf);
    //bug 78220: Reopen instance file path as file is changed
    if(!g_enable_test_init_stat)
    {
      fclose(g_instance_fp);
      sprintf(fname, "%s/.tmp/%s/keys", g_ns_wdir, g_test_inst_file);
      if(!(g_instance_fp = fopen(fname, "a")))
        NS_EXIT(-1, "Failed to open instance file %s, error: %s", fname, nslb_strerror(errno));
    }
    //clean_up_tmpfs_file(DLT_OLD_TESTRUN); //Bug-79445: Do not remove instance dir on start
  }
  else
  {
    if(memory_based_fs_mode && (check_tmpfs_mounted(err_msg) < 0))
    {
      NSDL1_PARSING(NULL, NULL, "check_tmpfs_mounted() failed, error: %s", err_msg);
      NS_DUMP_WARNING("%s directory is not present, tmpfs may not be mounted on %s, %s, tmpfs linking is disabled",
                       tmp_fs_path, tmp_fs_path, err_msg);
      memory_based_fs_mode = 0;
      return;
    }
    NS_DUMP_WARNING("ENABLE_TMPFS keyword should be enabled on MASTER to accelerate preprocessing");
  }

  if(!ns_is_numeric(size_buf))
    memory_based_fs_usage(buf, "FS size should be an integer");

  num = atoi(size_buf);
  if(num < 1 || num > 1000)
    memory_based_fs_usage(buf, "FS size should between 1 - 1000 (in GB)");

  memory_based_fs_size = num;
  
  NSDL2_PARSING(NULL, NULL, "Method exited, buf=%s, mode=%d, size=%d", buf, memory_based_fs_mode,
                            memory_based_fs_size);
}

# define DEFAULT_MIN_USER_OR_SESSION 25
// We are not allowing MAX CPU more than 2
#define MAX_PROC_PER_CPU 2

int kw_set_num_nvm(char *buf, Global_data *glob_set, int flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char process_per[MAX_DATA_LINE_LENGTH];
  int num_process, num;
  int per_proc_min_user_session = DEFAULT_MIN_USER_OR_SESSION;
  char per_proc_min_user_session_str[MAX_DATA_LINE_LENGTH]="";

  num = sscanf(buf, "%s %d %s %s", keyword, &num_process, process_per, per_proc_min_user_session_str);

  if ((num > 4) || (num < 3)) {
    NS_KW_PARSING_ERR(buf, flag, err_msg, NUM_NVM_USAGE, CAV_ERR_1011066, CAV_ERR_MSG_1);
  }

  if (num_process <= 0) {
    NSTL1_OUT(NULL, NULL, "read_keywords(): after keyword NUM_NVM< Must have at least one processes\n"
                          "read_keywords(): using default setting of - %d processes per CPU\n", DEFAULT_PROC_PER_CPU);
    NS_DUMP_WARNING("Keyword NUM_NVM must have at least one processes."
                    "Using default setting of keyword NUM_NVM to %d processes per CPU", DEFAULT_PROC_PER_CPU);
  } else {
    if (strncmp(process_per, "CPU", strlen("CPU")) == 0) {
       if(num_process > MAX_PROC_PER_CPU)
       {
          NSTL1_OUT(NULL, NULL, "NUM_NVM PER CPU(%d) must be less than Maximum allowed limit(%d). Setting it to Maximum limit",
                                  num_process, MAX_PROC_PER_CPU);
          NS_DUMP_WARNING("NUM_NVM PER CPU(%d) must be less than Maximum allowed limit (%d). Setting it to Maximum limit",
                                 num_process, MAX_PROC_PER_CPU);
          num_process = MAX_PROC_PER_CPU;
       }
       glob_set->num_process = num_processor * num_process;
        if(per_proc_min_user_session_str[0])
        {
          if(nslb_atoi(per_proc_min_user_session_str, &per_proc_min_user_session) < 0)
            NS_KW_PARSING_ERR(buf, flag, err_msg, NUM_NVM_USAGE, CAV_ERR_1011066, CAV_ERR_MSG_2);
          if(per_proc_min_user_session < 0)
            NS_KW_PARSING_ERR(buf, flag, err_msg, NUM_NVM_USAGE, CAV_ERR_1011066, CAV_ERR_MSG_8);

          glob_set->per_proc_min_user_session = per_proc_min_user_session;
        }

       if(g_per_cvm_distribution) {
        for(int i = 0; i < total_generator_entries; i++) {
          generator_entry[i].num_cvms = generator_entry[i].num_cpus * num_process;
          NIDL(4, "Num_Cpus = %d, num_process = %d, generator_entry[%d].num_cvms = %d",
                   generator_entry[i].num_cvms, num_process, i, generator_entry[i].num_cvms);
        }
      }
    } else if (strncmp(process_per, "MACHINE", strlen("MACHINE")) == 0) {
       glob_set->num_process = num_process;
       g_per_cvm_distribution = 0;
    } else {
      NSTL1_OUT(NULL, NULL, "read_keywords(): Unknown keyword %s, after keyword NUM_NVM (must be CPU or MACHINE). \n"
                            "read_keywords(): using default setting of - %d processes per CPU\n", process_per, DEFAULT_PROC_PER_CPU);
      NS_DUMP_WARNING("Unknown option '%s' to keyword NUM_NVM. It can be only CPU or MACHINE."
                      "using default setting of keyword NUM_NVM to %d processes per CPU", process_per, DEFAULT_PROC_PER_CPU);
    }
  }
  if(glob_set->num_process > MAX_NVM_NUM) {
    glob_set->num_process = MAX_NVM_NUM;
    NSTL1_OUT(NULL, NULL, "read_keywords(): using default setting of NUM_NVM keyword- NUM_NVM 255 MACHINE \n");
    NS_DUMP_WARNING("Number of processes are more than max limit. So, setting its value to 255 MACHINE");
  }
  return 0;

}
