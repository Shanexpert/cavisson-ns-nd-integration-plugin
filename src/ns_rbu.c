/************************************************************************************
 * Name	     : ns_rbu.c 

 * Purpose   : This file contains functions related to RBU(Real Browser User Support) feature 
 *             eg: parsing function, page execution function
 *             All these function will run on NVM context 

 * Author(s) : Sunil Kumar Mishra and
 *             Manish Kumar Mishra

 * Date      : 4 Jan 2014

 * Copyright : (c) Cavisson Systems

 * Modification History :
 * SM, 11-02-2014: Function kw_set_rbu_screen_size_sim() to handle RBU_SCREEN_SIZE_SIM keyword
 * SM, 04-04-2014: Handling for deleting POST request files from ns_logs directory.
 ***********************************************************************************/

#define _GNU_SOURCE 

#include <stdio.h>
#include <unistd.h>

#include "ns_script_parse.h"
#include "ns_url_req.h"
#include "ns_vuser_trace.h"
#include "ns_vuser_tasks.h"
#include "ns_url_hash.h"
#include "ns_trace_log.h"
#include "ns_child_thread_util.h"
#include "ns_rbu_api.h"
#include "ns_rbu.h"
#include "ns_session.h"
#include "ns_rbu_page_stat.h"
#include "ns_trace_level.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include <curl/curl.h>
#include "ns_exit.h"
#include "ns_rbu_domain_stat.h"
#include "ns_kw_usage.h"

/* Global variable - Declaration*/
/* This globle g_ns_firefox_binpath  is update at parsing time so no thred will update it so we can use it*/
char g_ns_firefox_binpath[RBU_MAX_PATH_LENGTH];
char g_rbu_user_agent_str[RBU_USER_AGENT_LENGTH + 1];
char g_rbu_dummy_url[RBU_MAX_DEFAULT_PAGE_LENGTH + 1];
char g_ns_chrome_binpath[RBU_MAX_PATH_LENGTH + 1];
char g_home_env[] = "/home/cavisson";                 //only cavisson user can run test
#ifndef CAV_MAIN
char g_rbu_create_performance_trace_dir = 0; //Bug-59603: Flag for creating performance_trace dir inside TR
#else
__thread char g_rbu_create_performance_trace_dir = 0; //Bug-59603: Flag for creating performance_trace dir inside TR
#endif
extern int loader_opcode;


/*--------------------------------------------------------------------------------
  Function      =   move_to_tr_as_harp
  Purpose       =   This function will move har and harp to TRXX/partition at the time of postprocessing
  Input         =   Har file path eg. /home/cavisson/.rbu/.chrome/logs/ 
--------------------------------------------------------------------------------*/
static void move_to_tr_as_harp(char *hpath)
{
  char buf[1024 + 1];
  #ifdef NS_DEBUG_ON
    int debug_flag = 1;
  #else
    int debug_flag = 0;
  #endif
    
  NSDL2_RBU(NULL, NULL, "Method called, hpath = [%s], rbu_har_rename_info_file = [%s], debug_flag = %d", 
                            hpath, global_settings->rbu_har_rename_info_file, debug_flag);
  

  snprintf(buf, 1024, "chm_har_file -o moveToTRAsHarp -t %d -f %s -p %s -D %d -i %lld 1>/dev/null 2>&1",
        testidx, global_settings->rbu_har_rename_info_file, hpath, debug_flag, g_partition_idx);

  NSDL2_RBU(NULL, NULL, "RBU Post Processing, Command = %s", buf);
  nslb_system2(buf);
}

//This function will close all open csv_fd
static int ns_rbu_close_csv_fd()
{
  int fd = -1;
  int i;

  NSDL2_RBU(NULL, NULL, "Mathod Called, total_page_entries = %d", total_page_entries);
  for(i = 0; i < total_page_entries; i++)
  {
    fd = (page_table_shr_mem[i].first_eurl)->proto.http.rbu_param.csv_fd;
    NSDL2_RBU(NULL, NULL, "Closing fd = %d", fd);
    if(fd > 0)
    {
      if(close(fd) != 0)
      {
        NSDL3_RBU(NULL, NULL, "Error in closing fd %d file", fd);
        NSTL1_OUT(NULL, NULL, "Error in closing fd %d. error = %s \n", fd, nslb_strerror(errno));
        return -1;
      }
    }  
    (page_table_shr_mem[i].first_eurl)->proto.http.rbu_param.csv_fd = -1;
  }
  NSDL2_RBU(NULL, NULL, "Successfully closed all fd");
  return 0;
}

/*-------------------------------------------------------------------------------------------------
* Function name  - ns_rbu_stop_vnc_and_delete_profiles
*                 
* Purpose        - This function will stop vnc and delete profiles at the time of post processing.
*
* Output         - On success:  0
*          	 - On failure: -1
* Changing rbu_parameter.csv to .rbu_parameter.csv at script path
---------------------------------------------------------------------------------------------------*/
static int ns_rbu_stop_vnc_and_delete_profiles()
{
  char cmd_buf [3072 + 1];
  char rbu_param_path [512 + 1];
  char controller_name[1024  + 1] = "";
  char *controller_name_ptr = NULL;
  int gp_idx;
  int ret;

  cmd_buf [0] = 0;
  rbu_param_path [0] = 0;

  //SessTableEntry_Shr* sess_table;
  //sess_table = session_table_shr_mem; 

  NSDL2_RBU(NULL, NULL, "Method called");

  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
  strcpy(controller_name, controller_name_ptr + 1);

  NSDL2_RBU(NULL, NULL, "controller_name = %s", controller_name);

  for(gp_idx = 0; gp_idx < total_runprof_entries; gp_idx++)
  {
    sprintf(rbu_param_path, "%s/logs/TR%d/scripts/%s/.rbu_parameter.csv", g_ns_wdir, testidx,
                             get_sess_name_with_proj_subproj_int(runprof_table_shr_mem[gp_idx].sess_ptr->sess_name,
                                                                 runprof_table_shr_mem[gp_idx].sess_ptr->sess_id, "/"));

    NSDL3_RBU(NULL, NULL, "rbu_param_path = %s, scen_group_name = %s, sess_name = %s", rbu_param_path,
                           runprof_table_shr_mem[gp_idx].scen_group_name, runprof_table_shr_mem[gp_idx].sess_ptr->sess_name);

    if(runprof_table_shr_mem[gp_idx].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    {
      sprintf(cmd_buf, "vid=`cat %s |awk -F',' '{printf $3\",\"}'` "
                       " && nsu_auto_gen_prof_and_vnc -o stop -N $vid -P %s-%s-%s-TR%d-0- -w -B 0 >/dev/null 2>&1",
                        rbu_param_path, global_settings->event_generating_host, controller_name, g_ns_login_user, testidx);
    }
    else if(runprof_table_shr_mem[gp_idx].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    {
      sprintf(cmd_buf, "vid=`cat %s 2>/dev/null |awk -F',' '{printf $3\",\"}'` "
                       " && nsu_auto_gen_prof_and_vnc -o stop -N $vid -P %s-%s-%s-TR%d-1- -w -B 1 >/dev/null 2>&1",
                        rbu_param_path, global_settings->event_generating_host, controller_name, g_ns_login_user, testidx);
    }
    else
    {
      NSDL3_RBU(NULL, NULL, "Invalid Browser mode");
      return -1;
    }

    NSDL2_RBU(NULL, NULL, "cmd_buf = %s", cmd_buf);
    NSTL1(NULL, NULL, "ns_rbu_stop_vnc_and_delete_profiles, cmd_buf = %s", cmd_buf);

    ret = nslb_system2(cmd_buf);
    if (WEXITSTATUS(ret) == 1)
    {
      NSTL1(NULL, NULL, "Unable to stop VNC");
    }
    else
    {
      NSTL1(NULL, NULL, "VNC stopped successfully");
    }
  }
  
 return 0;
}

/* Following task will handle here -
   (1) If run time renaming is off (i.e HAR_RENAME_FLAG = 0) then rename it on post processing and move HAR files into TRxx
   (2) Close all open csv files
*/
inline int ns_rbu_post_proc()
{
  //if post processing is on then we have to move all har and harp file at post processing
  NSDL1_RBU(NULL, NULL, "Method called" );
  if ((global_settings->rbu_har_rename_info_file))
  {
    char h_path[1024 + 1];

    //Atul- move all har made by firefox
    NSDL1_RBU(NULL, NULL, "browser_used = %d",global_settings->browser_used );
    if(global_settings->browser_used == FIREFOX || global_settings->browser_used == -1 ) {
      snprintf(h_path, 1024, "%s/.rbu/.mozilla/firefox/logs", g_home_env);
      move_to_tr_as_harp(h_path);
    } 
    else if (global_settings->browser_used == CHROME ) {
      //move all har made by chrome
      snprintf(h_path, 1024, "%s/.rbu/.chrome/logs", g_home_env);
      move_to_tr_as_harp(h_path);
    }
    else if ( global_settings->browser_used == FIREFOX_AND_CHROME ) {
      //In this case we used both firefox and chrome in different groups
      snprintf(h_path, 1024, "%s/.rbu/.mozilla/firefox/logs", g_home_env);
      move_to_tr_as_harp(h_path);
      snprintf(h_path, 1024, "%s/.rbu/.chrome/logs", g_home_env);
      move_to_tr_as_harp(h_path);
    }
  }

  if (global_settings->rbu_move_downloads_to_TR_enabled == 1)
  {
    char logs_path[RBU_MAX_128BYTE_LENGTH + 1];
    char buf[RBU_MAX_256BYTE_LENGTH + 1];

    #ifdef NS_DEBUG_ON
      int debug_flag = 1;
    #else
      int debug_flag = 0;
    #endif

    NSDL1_RBU(NULL, NULL, "browser_used = %d, debug_flag = %d", global_settings->browser_used, debug_flag);
    switch(global_settings->browser_used)
    {
      case FIREFOX :
                     snprintf(logs_path, RBU_MAX_128BYTE_LENGTH, "%s/.rbu/.mozilla/firefox/logs", g_home_env);
                     snprintf(buf, RBU_MAX_256BYTE_LENGTH, "nsu_rbu_post_proc -o moveDownloadsToTR -t %d -l %s -D %d -i %lld 1>/dev/null 2>&1",
                                   testidx, logs_path, debug_flag, g_partition_idx);
                     nslb_system2(buf);
                     break;

      case CHROME : 
                    snprintf(logs_path, RBU_MAX_128BYTE_LENGTH, "%s/.rbu/.chrome/logs", g_home_env);
                    snprintf(buf, RBU_MAX_256BYTE_LENGTH, "nsu_rbu_post_proc -o moveDownloadsToTR -t %d -l %s -D %d -i %lld 1>/dev/null 2>&1",
                                  testidx, logs_path, debug_flag, g_partition_idx);
                    nslb_system2(buf);
                    break;

      case FIREFOX_AND_CHROME : 
                    snprintf(logs_path, RBU_MAX_128BYTE_LENGTH, "%s/.rbu/.mozilla/firefox/logs", g_home_env);
                    snprintf(buf, RBU_MAX_256BYTE_LENGTH, "nsu_rbu_post_proc -o moveDownloadsToTR -t %d -l %s -D %d -i %lld 1>/dev/null 2>&1",
                                  testidx, logs_path, debug_flag, g_partition_idx);
                    nslb_system2(buf); 

                    snprintf(logs_path, RBU_MAX_128BYTE_LENGTH, "%s/.rbu/.chrome/logs", g_home_env);
                    snprintf(buf, RBU_MAX_256BYTE_LENGTH, "nsu_rbu_post_proc -o moveDownloadsToTR -t %d -l %s -D %d -i %lld 1>/dev/null 2>&1",
                                  testidx, logs_path, debug_flag, g_partition_idx);
                    nslb_system2(buf);
                    break;
    }
 
    NSDL2_RBU(NULL, NULL, "RBU Post Processing, Command = %s", buf);
  }

  //RBU Automation: This will delete profiles and stop vnc.
  if(global_settings->rbu_enable_auto_param)
  {
    if(ns_rbu_stop_vnc_and_delete_profiles() == 0)
      NSDL3_RBU(NULL, NULL, "Vnc and profiles deleted successfully.");
  }

  //call function ns_rbu_close_csv_fd only if RBU_ENABLE_CSV is on i.e 1
  if (global_settings->rbu_enable_csv)
  {
    if(ns_rbu_close_csv_fd() == -1)
      return -1;
  }
  return 0;
}

/*------------------------------------------------------------------------------------
  Function name       :    ns_rbu_set_csv_file_name
  Description         :    This function will calculate host name from url and create rbu csv file name
  Input               :    URL index and host name 
  Output              :    save rbu csv file name into bigbuffer
-------------------------------------------------------------------------------------*/ 
int ns_rbu_set_csv_file_name(int url_idx, char *hostname)
{
  char csv_file_name[1024 + 1];
  char host[128 + 1];
  char *host_ptr;
  int pos = 0;
  int dot_count = 0;
  int hostname_len;

  NSDL2_RBU(NULL, NULL, "Method Called, url_idx = %d, hostname = %s", url_idx, hostname?hostname:"NULL");

  //if hostname is null then return, no need to go further....
  if(hostname[0]  == '\0')
  {
    NSDL2_RBU(NULL, NULL, "Null hostname found, hence returning");
    return -1;
  }
  hostname_len = strlen(hostname);
  for(pos = 0; pos < hostname_len; pos++)
  {
    if(hostname[pos] == '.')
      dot_count++;
  }
  pos = 0; //resetting pos so we can use it further
  if(dot_count == 2) //host name is like www.hostname.com or m.hostname.com so pick only hostname
  {
    host_ptr = strchr(hostname, '.');
    host_ptr++;

    while(host_ptr[pos] != '.')
    {
      host[pos] = host_ptr[pos];
      pos++;
    }
    host[pos] = '\0';
  }
#if 0
  else if(dot_count == 3) //host is IP address like 192.168.1.77:8080 then pick ip address only
  {
    while(hostname[pos] != ':') //TODO if port is not given in ns_web_url
    {
      host[pos] = hostname[pos];
      pos++;
    }
    host[pos] = '\0';
  }
#endif
  else //Unknown host
  {  
    NSDL2_RBU(NULL, NULL, "Unknown host found");
    sprintf(host, "%s", hostname); 
  }

  sprintf(csv_file_name, "%s_%s.csv", host, RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));      

  NSDL2_RBU(NULL, NULL, "Fill csv name '%s' in RBU_Param struct at url id '%d'", csv_file_name, url_idx);
  
  requests[url_idx].proto.http.rbu_param.csv_file_name = copy_into_big_buf(csv_file_name, 0);

  return 0;
}

inline void ns_rbu_stop_browser_on_sess_end(VUser *vptr)
{
  NSDL2_RBU(vptr, vptr->last_cptr, "Method Called, vptr = %p", vptr);
  if(kill(vptr->httpData->rbu_resp_attr->browser_pid, 0) == 0) //If browser is running
  {
    NSDL4_RBU(vptr, vptr->last_cptr, "Session completed - Killing Browser pid %d", vptr->httpData->rbu_resp_attr->browser_pid);

    //Deepika : many times browser not properly shutdown by SIGTERM command so we need to wait after sending signal SIGTERM  
    kill(vptr->httpData->rbu_resp_attr->browser_pid, SIGTERM);
    usleep(300000);  //Sleep for 300 ms
    
    /* If browser is not normally killed then kill forcefully */
    if(kill(vptr->httpData->rbu_resp_attr->browser_pid, 0) == 0) //Browser is running
    {
      NSDL4_RBU(vptr, vptr->last_cptr, "Browser pid %d is not killed normally and hence killing it forcefully", 
                                        vptr->httpData->rbu_resp_attr->browser_pid);
      kill(vptr->httpData->rbu_resp_attr->browser_pid, SIGKILL);
      usleep(300000);  //Sleep for 300 ms

      if(kill(vptr->httpData->rbu_resp_attr->browser_pid, 0) == 0) //Browser is still running 
      {
         NSDL4_RBU(vptr, vptr->last_cptr, "Browser pid %d is not killed forcefully", vptr->httpData->rbu_resp_attr->browser_pid);
      }
      else
      {
        NSDL4_RBU(vptr, vptr->last_cptr, "Browser pid %d is forcefully killed, successfully", vptr->httpData->rbu_resp_attr->browser_pid);
      }
    }
    else
    { 
      NSDL4_RBU(vptr, vptr->last_cptr, "Session completed - Browser pid %d normally killed successfully", 
                                           vptr->httpData->rbu_resp_attr->browser_pid);
    }
  }
  else
  {
    NSDL4_RBU(vptr, vptr->last_cptr, "Session completed - Browser pid %d not running", vptr->httpData->rbu_resp_attr->browser_pid);
  }

  //Set browser_pid = 0 because on start of session it checks browser_pid is 0 or not 
  vptr->httpData->rbu_resp_attr->browser_pid = 0;
}

inline void ns_rbu_on_sess_end(VUser *vptr)
{
  
  NSDL3_RBU(vptr, vptr->last_cptr, "vptr->httpData->rbu_resp_attr[%p] vptr->httpData[%p]", vptr->httpData->rbu_resp_attr, vptr->httpData );
  if(!vptr->httpData->rbu_resp_attr)
  {
    return ;
  }

  NSDL3_RBU(vptr, vptr->last_cptr, "Method called, stop_browser_on_sess_end_flag = %d, browser_pid = %d", 
                                         runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.stop_browser_on_sess_end_flag, 
                                         runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu?
                                         vptr->httpData->rbu_resp_attr->browser_pid:0);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.stop_browser_on_sess_end_flag 
                 && (vptr->httpData->rbu_resp_attr->browser_pid != 0))
    ns_rbu_stop_browser_on_sess_end(vptr);

  vptr->httpData->rbu_resp_attr->sess_start_flag = 0;
  vptr->httpData->rbu_resp_attr->first_pg = 1;

  /* Get data for graph - sess_success, sess_success_completed */
  vptr->httpData->rbu_resp_attr->sess_completed++;

  if (!vptr->sess_status)
    vptr->httpData->rbu_resp_attr->sess_success++;
  
  set_rbu_page_stat_data_avgtime_data_sess_only(vptr, NULL);

  NSDL3_RBU(vptr, vptr->last_cptr, "sess_completed = %d, sess_success = %d", 
                                    vptr->httpData->rbu_resp_attr->sess_completed,
                                    vptr->httpData->rbu_resp_attr->sess_success);
}

inline void kill_browsers(char *user, char *browser, char *controller, char *ignore_tr)
{
  char cmd[1024  + 1];
  char con_name[512 +1];

  NSDL2_RBU(NULL, NULL, "user = [%s], browser = [%s], controller = [%s], ignore_tr = [%s]", user, browser, controller, ignore_tr);

  if(!strcmp(browser, "firefox"))
  {
    sprintf(con_name, "--controller_name %s", controller); 
  }
  else if(!strcmp(browser, "chromium-browser|chrome"))
  {
    sprintf(con_name, "--controller_name=%s", controller); 
  }
  else
  {
    NSDL2_RBU(NULL, NULL, "Unspecifed browser [%s].", browser); 
    return;
  }

  if(ignore_tr != NULL && ignore_tr[0])
    snprintf(cmd, 1024, "kill -9 `ps -ef | grep \"^%s.*%s\" | egrep \"%s\" | grep -v grep | grep -v -E %s | awk -F' ' '{printf $2\" \"}'` >/dev/null 2>&1", user, con_name, browser, ignore_tr);
    //snprintf(cmd, 1024, "kill -9 `ps -ef | grep \"^%s.*%s.*%s\" | grep -v grep | grep -v -E %s | awk -F' ' '{printf $2\" \"}'` >/dev/null 2>&1", user, browser, con_name, ignore_tr);
  else
    snprintf(cmd, 1024, "kill -9 `ps -ef | grep \"^%s.*%s\" | egrep \"%s\" | grep -v grep | awk -F' ' '{printf $2\" \"}'` >/dev/null 2>&1", 
                         user, con_name, browser);
    //snprintf(cmd, 1024, "kill -9 `ps -ef | grep \"^%s.*%s.*%s\" | grep -v grep | awk -F' ' '{printf $2\" \"}'` >/dev/null 2>&1", user, browser, con_name);

  NSDL2_RBU(NULL, NULL, "Run command to clear Browser process before starting test : [%s]", cmd);

  nslb_system2(cmd);
}

inline int ns_rbu_check_for_browser_existence()
{
  char cmd[1024 + 8];
  struct stat stat_buf;

  NSDL2_RBU(NULL, NULL, "Method called, Browser used = %d", global_settings->browser_used);
  
  // Check firefox command exist or not and browser_used is added to check which browser is running.
  if((*g_ns_firefox_binpath != '\0') && (global_settings->browser_used == FIREFOX_AND_CHROME || global_settings->browser_used == FIREFOX))
  {
    sprintf(cmd, "%s/firefox", g_ns_firefox_binpath);
    NSDL3_RBU(NULL, NULL, "firefox_binpath = %s", g_ns_firefox_binpath);

    if(stat(cmd, &stat_buf) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000038, cmd, errno, nslb_strerror(errno)); 
    }
  }
  //System Firefox
  else if(global_settings->browser_used == FIREFOX)
  {
    strcpy(cmd, "which firefox; echo $?");
    FILE *fp = nslb_popen(cmd, "re");

    if(fp)
    {
      int status = -1;
      char buff[512] = {0};

      while((fgets(buff, 512, fp)) != NULL);

      sscanf(buff, "%d", &status);
      if(status != 0)
      {
        NSTL1_OUT(NULL, NULL, "Error: firefox not found.\n");
         return -1;
      }
    }
    else
    {
      NS_EXIT(-1, CAV_ERR_1000031, cmd, errno, nslb_strerror(errno)); 
    }
  }

  // Check chrome 40 command exist or not and browser_used is added to check which browser is running.
  if((*g_ns_chrome_binpath != '\0') && (global_settings->browser_used == FIREFOX_AND_CHROME || global_settings->browser_used == CHROME))
  {
    sprintf(cmd, "%s/chrome", g_ns_chrome_binpath);
    NSDL3_RBU(NULL, NULL, "chrome_binpath = %s", g_ns_chrome_binpath);

    if(stat(cmd, &stat_buf) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1000038, cmd, errno, nslb_strerror(errno)); 
    }
  }
  //Chrome 34
  else if(global_settings->browser_used == CHROME)
  {
    /*strcpy(cmd, "/home/cavisson/thirdparty/chrome"); 
     NSDL3_RBU(NULL, NULL, "chrome_binpath = %s", g_ns_chrome_binpath);
  
    if(stat(cmd, &stat_buf) == -1)
    {
      NSTL1_OUT(NULL, NULL, "Error: command '%s' not found.\n", cmd);
      return -1;
    }*/
 
    strcpy(cmd, "which chromium-browser; echo $?");
    FILE *fp = nslb_popen(cmd, "re");

    if(fp)
    {
      int status = -1;
      char buff[512] = {0};
      
      while((fgets(buff, 512, fp)) != NULL);

      sscanf(buff, "%d", &status);
      if(status != 0)
      {
        NSTL1_OUT(NULL, NULL, "Error: chromium-browser not found.\n");
        return -1;
      }
    }
    else
    {
      NS_EXIT(-1, CAV_ERR_1000031, cmd, errno, nslb_strerror(errno)); 
    }
  }
  
  return 0;
}

inline void ns_rbu_kill_browsers_before_start_test()
{
  struct passwd *pw;
  char cmd[1024  + 1];
  char controller_name[1024  + 1] = "";
  char ignore_trun[1024  + 8];
  char buf[1024  + 1] = "";
  char *controller_name_ptr = NULL;

  ignore_trun[0] = 0;

  NSDL2_RBU(NULL, NULL, "Method Called.");

  pw = getpwuid(getuid());

  if (pw == NULL)
  {
    NS_EXIT(1, "Error: Unable to get the real user name");
  }
 
  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
    strcpy(controller_name, controller_name_ptr + 1);

  //Reslove BUg: 7459 - Get id of running test and ignore that 
  sprintf(cmd, "%s", "nsu_show_test_logs -RL | awk '!/TestRun/{printf(\"\\--cav_testrun*.*\%s|\",$1)}' 2>/dev/null");
  NSDL3_RBU(NULL, NULL, "cmd = [%s]", cmd);
 
  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, SIG_IGN);

  FILE *cmd_fp = nslb_popen(cmd, "re");
  if(cmd_fp != NULL)
  {
    if(fgets(buf, 1024, cmd_fp) != NULL)
    {
      char *ptr = NULL;
      if ((ptr = strrchr(buf, '|')) != NULL)
        *ptr = 0;

      NSDL3_RBU(NULL, NULL, "buf = [%s]", buf);
      sprintf(ignore_trun, "'(%s)'", buf);
    }
    (void) signal( SIGCHLD, prev_handler);
    pclose(cmd_fp);
  }

  NSDL3_RBU(NULL, NULL, "ignore_trun = [%s]", ignore_trun);

  /* Kill firefox browsers */
  kill_browsers(pw->pw_name, "firefox", controller_name, ignore_trun);

  /* Kill chromium-browser browsers */
  kill_browsers(pw->pw_name, "chromium-browser|chrome", controller_name, ignore_trun);
}

static int remove_all_files(char *dir_name)
{
  DIR *dir = NULL;
  char abs_filename[RBU_MAX_FILE_LENGTH];
  struct dirent *dptr = NULL;

  NSDL2_RBU(NULL, NULL,"Methd Called, remove_all_files(): dir_name = [%s]", dir_name);

  if ((dir = opendir(dir_name)) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: Unable to open directory %s. errno = %d(%s)", dir_name, errno, nslb_strerror(errno));
    return -1;
  }
    
  while ((dptr = readdir(dir)) != NULL)
  {
    NSDL2_RBU(NULL, NULL, "dptr->d_name = %s, dptr->d_type = %s", dptr->d_name, (nslb_get_file_type(dir_name, dptr) == DT_DIR)?"Directory":"File");
  
    if (nslb_get_file_type(dir_name, dptr) == DT_REG)
    {
      snprintf(abs_filename, RBU_MAX_FILE_LENGTH, "%s/%s", dir_name, dptr->d_name);
      if(unlink(abs_filename) != 0)
        NSDL2_RBU(NULL, NULL,"Error: unable to removed file %s error is = %s ", abs_filename, nslb_strerror(errno));
      else
        NSDL2_RBU(NULL, NULL,"Sucessful remove file %s", dptr->d_name);
    }
  }

  closedir(dir);
  return 0;
}

static int clean_profile_logs(char *dir_path, int mode)
{
  char abs_filename[RBU_MAX_FILE_LENGTH];
  
  NSDL2_RBU(NULL, NULL,"Methd Called, clean_profile_logs(): dir_path = [%s], delete root directory flag = %d", dir_path, mode);

  // Removes clips files
  sprintf(abs_filename, "%s/clips", dir_path);
  remove_all_files(abs_filename);

  //Removes snap_shot files
  sprintf(abs_filename, "%s/snap_shot", dir_path);
  remove_all_files(abs_filename);
 
  //Removes csv files
  sprintf(abs_filename, "%s/csv", dir_path);
  remove_all_files(abs_filename);

  //Removes performance_trace files
  sprintf(abs_filename, "%s/performance_trace", dir_path);
  remove_all_files(abs_filename);

  //Remove extra files present in profile logs path
  remove_all_files(dir_path);

  if(mode)
  {
    if(remove(dir_path) != 0)
      NSDL2_RBU(NULL, NULL,"Error: could not removed directory %s error = %s ", dir_path, nslb_strerror(errno));
    else
      NSDL2_RBU(NULL, NULL,"sucessful remove directory %s ",dir_path);
  }
  return 0;
}

inline void ns_rbu_user_cleanup(VUser *vptr)
{
  NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr exists");
  if(vptr->httpData->rbu_resp_attr->resp_body != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->ua_handler_ptr->resp_body exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->resp_body, vptr->httpData->rbu_resp_attr->last_malloced_size, "Har Resp Body", -1);
    vptr->httpData->rbu_resp_attr->resp_body_size = 0;
    vptr->httpData->rbu_resp_attr->last_malloced_size = 0;
  }

  if(vptr->httpData->rbu_resp_attr->page_dump_buf != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->ua_handler_ptr->page_dump_buf exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->page_dump_buf, vptr->httpData->rbu_resp_attr->last_page_dump_buf_malloced, "Screen Shot Buffer", -1);
    vptr->httpData->rbu_resp_attr->page_dump_buf_len = 0;
    vptr->httpData->rbu_resp_attr->last_page_dump_buf_malloced = 0;
  }

  if(vptr->httpData->rbu_resp_attr->url != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->ua_handler_ptr->url exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->url, RBU_MAX_URL_LENGTH, "url", -1);
  }

  if(vptr->httpData->rbu_resp_attr->firefox_cmd_buf != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->ua_handler_ptr->firefox_cmd_buf exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, RBU_MAX_CMD_LENGTH, "Firefox command buf", -1);
  }

  if(vptr->httpData->rbu_resp_attr->post_req_filename != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->post_req_filename exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->post_req_filename, RBU_MAX_FILE_LENGTH + 1, "POST req file name", -1);
  }

  if(vptr->httpData->rbu_resp_attr->page_screen_shot_file != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->ua_handler_ptr->page_screen_shot_file exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH, "Screen shot", -1);
  }

  if(vptr->httpData->rbu_resp_attr->page_capture_clip_file != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->page_capture_clip_file exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH, "Clip File", -1);
  } 
  if(vptr->httpData->rbu_resp_attr->rbu_hartime != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->rbu_hartime exists");
    //we realloc this memory hence initial size is updated 
    FREE_AND_MAKE_NULL(vptr->httpData->rbu_resp_attr->rbu_hartime, "rbu_hartime", -1);
  } 

  if(vptr->httpData->rbu_resp_attr->profile != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->profile = [%s] exists", vptr->httpData->rbu_resp_attr->profile);
    if ((global_settings->protocol_enabled & RBU_API_USED) && !global_settings->rbu_har_rename_info_file && !global_settings->rbu_move_downloads_to_TR_enabled)
    {
      //we have already moved all har files and data at runtime when post proc keyword is off 
      //here we have to remove all  directories in .rbu/.chrome/logs/<profile> 
      char log_path[1024 + 1];
      NSDL1_RBU(NULL, NULL, "browser_used = %d", global_settings->browser_used );
      if(global_settings->browser_used == FIREFOX || global_settings->browser_used == -1 ) {
        snprintf(log_path, 1024, "%s/.rbu/.mozilla/firefox/logs/%s", g_home_env, vptr->httpData->rbu_resp_attr->profile);
        if(clean_profile_logs(log_path, 0) != 0 )
          NSDL1_RBU(NULL,NULL, "Unable to clear log file");
        else
          NSDL1_RBU(NULL, NULL, "Clean log directory sucessfully");
      }
      else if(global_settings->browser_used == CHROME) {
        snprintf(log_path, 1024, "%s/.rbu/.chrome/logs/%s", g_home_env, vptr->httpData->rbu_resp_attr->profile);
        if(clean_profile_logs(log_path, 0) != 0 )
          NSDL1_RBU(NULL,NULL, "Unable to clear log file");
        else
          NSDL1_RBU(NULL, NULL, "Clean log directory sucessfully");
      }
    }
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->profile, RBU_MAX_PROFILE_NAME_LENGTH, "profile", -1);
  }

  if(vptr->httpData->rbu_resp_attr->har_log_path != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->har_log_path exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->har_log_path, RBU_MAX_PATH_LENGTH, "har_log_path", -1);
  }

  if(vptr->httpData->rbu_resp_attr->har_log_dir != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->har_log_dir exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->har_log_dir, RBU_MAX_HAR_DIR_NAME_LENGTH, "har_log_dir", -1);
  }

  if(vptr->httpData->rbu_resp_attr->date_time_str != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->date_time_str exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->date_time_str, RBU_MAX_256BYTE_LENGTH, "date_time_str,", -1);
  }

  if(vptr->httpData->rbu_resp_attr->har_name != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->har_name exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->har_name, RBU_HAR_FILE_NAME_SIZE, "har_name,", -1);
  }

  //Cleanup for RBU Device Info
  if(vptr->httpData->rbu_resp_attr->dvc_info != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->dvc_info exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->dvc_info, RBU_HAR_FILE_NAME_SIZE, "Device Info", -1);
  }

  if(vptr->httpData->rbu_resp_attr->performance_trace_filename != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->performance_trace_filename exists");
    FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr->performance_trace_filename, RBU_HAR_FILE_NAME_SIZE, "performance_trace_filename", -1);
  }

  if(vptr->httpData->rbu_resp_attr->rbu_light_house != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->rbu_light_house exists");
    //we realloc this memory hence initial size is updated 
    FREE_AND_MAKE_NULL(vptr->httpData->rbu_resp_attr->rbu_light_house , "rbu_light_house", -1);
  } 

  if(vptr->httpData->rbu_resp_attr->mark_and_measures != NULL)
  {
    NSDL1_RBU(vptr, NULL, "vptr->httpData->rbu_resp_attr->mark_and_measures exists");
    //we realloc this memory hence initial size is updated 
    FREE_AND_MAKE_NULL(vptr->httpData->rbu_resp_attr->mark_and_measures , "mark_and_measures", -1);
  } 

  if(vptr->httpData->rbu_resp_attr->timer_ptr != NULL)
  {
    FREE_AND_MAKE_NULL(vptr->httpData->rbu_resp_attr->timer_ptr, "rbu_resp_attr->timer_ptr", -1);
  }

  FREE_AND_MAKE_NULL_EX(vptr->httpData->rbu_resp_attr, sizeof(RBU_RespAttr), "RBU_RespAttr structure", -1);
}

static inline void ns_rbu_advance_param(char *param_string)
{
  char *start;
  char *end;
  int len;
  char param_name[512];

  start = strchr(param_string, '{');
  if(start) {
    end = strchr(start, '}');
    if(end) { 
      len = (end - start);
      strncpy(param_name, start+1, len-1);
      param_name[len - 1] = 0;
      ns_advance_param(param_name);
    }
  }
}


/*--------------------------------------------------------------------------------
  Function      =   make_rbulogs_dir_and_link
  Purpose       =   This function will create rbu_logs and its sub directories and in case of Multidisk it will creates symlink also 
 
  Return status =   None
--------------------------------------------------------------------------------*/
void make_rbu_logs_dir_and_link()
{

  char buf[1024 + 1] = "";
  int ret; 
 
  NSDL1_RBU(NULL, NULL, "Method called, multidisk_rbu_logs_path = %s", 
                                        global_settings->multidisk_ns_rbu_logs_path?global_settings->multidisk_ns_rbu_logs_path:NULL);

  if(global_settings->multidisk_ns_rbu_logs_path && global_settings->multidisk_ns_rbu_logs_path[0])
  {
    
    char symlink_buf[1024 + 1] = "";
    //Create rbu_logs dir on multidisk
    sprintf(symlink_buf , "%s/%s/rbu_logs/harp_files/", global_settings->multidisk_ns_rbu_logs_path, global_settings->tr_or_partition);
    mkdir_ex(symlink_buf);
    //This will create the path till rbu_logs.
    sprintf(buf , "%s/logs/%s/rbu_logs/harp_files", g_ns_wdir , global_settings->tr_or_partition);
    mkdir_ex(buf);

    NSDL1_RBU(NULL, NULL, "Created directory = %s, har_tr_directory = %s", symlink_buf, buf);
    if(symlink(symlink_buf, buf) < 0)  //creating link of harp_files
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                      "Could not create symbolic link %s to %s .Error = %s ", symlink_buf, buf, nslb_strerror(errno));  
    }

  }
  //Create rbu_logs dir
  NSDL1_RBU(NULL, NULL, "global_setting->create_screen_shot_dir = %d ", global_settings->create_screen_shot_dir);
  if( global_settings->create_screen_shot_dir == SCREEN_SHOT_DIR_FLAG )
    sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/screen_shot && " 
                 "mkdir -p -m 777 logs/%s/rbu_logs/harp_files",
                  global_settings->tr_or_partition, global_settings->tr_or_partition);
  else
  if(global_settings->create_screen_shot_dir == SNAP_SHOT_DIR_FLAG )
    sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/snap_shots && "
                 "mkdir -p -m 777 logs/%s/rbu_logs/harp_files",
                  global_settings->tr_or_partition, global_settings->tr_or_partition);
 
  else
  if(global_settings->create_screen_shot_dir == ALL_DIR_FLAG )
    sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/snap_shots && "
                 "mkdir -p -m 777 logs/%s/rbu_logs/screen_shot && "
                 "mkdir -p -m 777 logs/%s/rbu_logs/harp_files",
                  global_settings->tr_or_partition, global_settings->tr_or_partition, global_settings->tr_or_partition);
 
   else
     sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/harp_files", global_settings->tr_or_partition);
  ret = nslb_system2(buf);
  if (WEXITSTATUS(ret) == 1){
    NS_EXIT(-1, "Failed to create RBU logs directory (snap_shots, screen_shot, harp_files) in %s/rbu_logs",
                   global_settings->tr_or_partition);
  }
  else
    NSDL1_RBU(NULL, NULL, "Created directory = %s", buf);

  //Create lighthouse directory
  if(global_settings->create_lighthouse_dir == LIGHTHOUSE_DIR_FLAG){
    sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/lighthouse", global_settings->tr_or_partition);

    ret =  nslb_system2(buf);
    if (WEXITSTATUS(ret) == 1){
      NS_EXIT(-1, "Failed to create RBU logs directory (lighthouse) in %s/rbu_logs",
                   global_settings->tr_or_partition);
    }
    else 
      NSTL1(NULL, NULL, "Created lighthouse dir \'logs/%s/rbu_logs/lighthouse\'", global_settings->tr_or_partition);
  }

  //Create performance_trace directory
  if(g_rbu_create_performance_trace_dir == PERFORMANCE_TRACE_DIR_FLAG){
    sprintf(buf, "mkdir -p -m 777 logs/%s/rbu_logs/performance_trace", global_settings->tr_or_partition);

    ret = nslb_system2(buf);
    if (WEXITSTATUS(ret) == 1){
      NS_EXIT(-1, "Failed to create RBU logs directory (performance_trace) in %s/rbu_logs",
                   global_settings->tr_or_partition);
    }
    else 
      NSTL1(NULL, NULL, "Created performance_trace dir \'logs/%s/rbu_logs/performance_trace\'", global_settings->tr_or_partition);
  }
}

/*--------------------------------------------------------------------------------
  Function      =   make_dir
  Purpose       =   This function will create directory only if parent directory exist
  Input         =   Absolute path where new directory will be created 
                    For ex : /home/cavisson/work/logs/TRxx/Reports/harp_csv
                    on the basis of above example, 'harp_csv' directory will be created if 'reports' directory exist 
  Return status =   0 sucess
                   -1 failure
--------------------------------------------------------------------------------*/
int make_dir(char *create_dir)
{

  NSDL2_RBU(NULL, NULL, "Method called, directory name with full path = %s", create_dir?create_dir:NULL);
  if(mkdir(create_dir, 0777))
  {
    //Ignore if directory already exist.
    if(errno == EEXIST)
    {
      NSDL2_RBU(NULL, NULL, "Directory %s already exist", create_dir);
    }
    if(errno != EEXIST)
    {
     NSTL1_OUT(NULL, NULL, "Failed to create directory [%s], error = %s\n",create_dir, nslb_strerror(errno));
     return -1;
    }
  }
  NSDL2_RBU(NULL, NULL, "Directory [%s] created successfully, returning", create_dir);
  return 0;
}

/*-------------------------------------------------------------------------------- 
  Function Name  :  ns_rbu_generate_csv_file
  Purpose        :  This function will do the following things
                      1- Open csv file name at required path
                      2- Put header in the file and save fd in request table
                      Header format is -
                      Date,Time,Dom_Load_Time(Sec),OnLoad_Time(Sec),Page_Load_Time(Sec),Requests,Browser_Cache,Bytes_Recieved(KB),Bytes_Send(KB) 
  Input          :  NO
  Output         :  Store fd of open file in request table

  Return status  :  0  Sucess
                 :  -1 Error          
---------------------------------------------------------------------------------*/
int ns_rbu_generate_csv_file()
{
  char csv_abs_file_name[RBU_MAX_PATH_LENGTH + 8];
  char csv_dir_path[RBU_MAX_PATH_LENGTH + 1];
  char csv_header[] = "Date,Time,Dom_Load_Time(Sec),OnLoad_Time(Sec),Page_Load_Time(Sec),Requests,Browser_Cache,Bytes_Recieved(KB),Bytes_Send(KB)\n";
  char *csv_file_name = NULL;
  int csv_fd = -1;
  int i = 0;

  NSDL2_RBU(NULL, NULL, "Method called, g_ns_wdir = [%s], testidx = [%d], total_page_entries = [%d]", 
                                                             g_ns_wdir, testidx, total_page_entries);

  //If report/harp_csv directory not exist then first create it then open csv file
  sprintf(csv_dir_path, "%s/logs/TR%d/reports", g_ns_wdir, testidx);
  if(make_dir(csv_dir_path) != 0)
    return -1;
 
  sprintf(csv_dir_path, "%s/logs/TR%d/reports/harp_csv", g_ns_wdir, testidx);
  if(make_dir(csv_dir_path) != 0)
    return -1;

  for(i = 0; i < total_page_entries; i++)
  {
    csv_file_name = RETRIEVE_BUFFER_DATA(requests[gPageTable[i].first_eurl].proto.http.rbu_param.csv_file_name);
    sprintf(csv_abs_file_name, "%s/%s", csv_dir_path, csv_file_name); 
    NSDL3_RBU(NULL, NULL, "Open csv_file_name = %s, csv_abs_file_name = %s, "
                          "Index of request table is [gPageTable[%d].first_eurl] = %lu", 
                           csv_file_name, csv_abs_file_name, i, gPageTable[i].first_eurl);

    //Create File in append mode 
    //TODO: fd cav be 0????
    if(requests[gPageTable[i].first_eurl].proto.http.rbu_param.csv_fd > 0)
    {
      NSDL3_RBU(NULL, NULL, "csv file '%s' is alreday opened.", csv_file_name); 
      continue;
    }

    
    if((csv_fd = open(csv_abs_file_name, O_CREAT|O_WRONLY|O_CLOEXEC, 00666)) == -1)
    {
      NSDL3_RBU(NULL, NULL, "Error in opening file [%s]", csv_abs_file_name);
      NSTL1(NULL, NULL, "Error in opening file [%s]. error = %s",  csv_file_name, nslb_strerror(errno));
      NSTL1_OUT(NULL, NULL, "Error in opening file [%s]. error = %s \n", csv_abs_file_name, nslb_strerror(errno));
      return -1;
    }

    requests[gPageTable[i].first_eurl].proto.http.rbu_param.csv_fd = csv_fd;

    NSDL3_RBU(NULL, NULL, "Write Header on csv_fd = %d", csv_fd);
    if(write(csv_fd, csv_header, strlen(csv_header)) == -1)
    {
      NSDL3_RBU(NULL, NULL, "Error in writing header in csv file [%s]", csv_abs_file_name);
      NSTL1(NULL, NULL, "Error in writing header in csv file [%s] on fd = %d.Error = %s",  csv_file_name, csv_fd, nslb_strerror(errno));
      NSTL1_OUT(NULL, NULL, "Error in writing header in csv file [%s]. error = %s \n", csv_abs_file_name, nslb_strerror(errno));
      return -1;
    }
  }
  
  return 0;
}

static int move_to_archive(VUser *vptr, int testidx, char *prof_name, char *har_log_dir)
{
  char browser_base_log_path[512 + 1];

  NSDL1_RBU(vptr, NULL,"Function called, testidx = %d, profile_name = %s, har_log_dir = %s", testidx, prof_name, har_log_dir);
  //validate profile

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    snprintf(browser_base_log_path, 512, "%s/.rbu/.mozilla/firefox", g_home_env);
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    snprintf(browser_base_log_path, 512, "%s/.rbu/.chrome", g_home_env);
  else
  {
    //TODO: not supported yet so return form here
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_on_session_start() - Browser mode %d not supported yet. Try in next release.\n",
                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode);
    return -1;
  }
  NSDL1_RBU(NULL,NULL,"browser_base_log_path = %s", browser_base_log_path);
  //valaditing profile 
  if(ns_rbu_validate_profile(vptr, browser_base_log_path, prof_name) == -1)
    return -1;

  //Make archive
  char log_tr_dir[512 + 1];
  sprintf(log_tr_dir, "%s/%s/archive/", har_log_dir, prof_name);
  NSDL1_RBU(vptr, NULL,"Making archive directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;

  //make snap_shot_directory
  sprintf(log_tr_dir, "%s/%s/snap_shot/", har_log_dir, prof_name);
  NSDL1_RBU(vptr, NULL,"Making snap_shot directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;

  //make clips directory
  sprintf(log_tr_dir, "%s/%s/clips/", har_log_dir, prof_name);
  NSDL1_RBU(vptr, NULL,"Making clips directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;

  //make csv directory
  sprintf(log_tr_dir, "%s/%s/csv/", har_log_dir, prof_name);
  NSDL1_RBU(vptr, NULL,"Making csv directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;

  //make performance_trace directory
  sprintf(log_tr_dir, "%s/%s/performance_trace/", har_log_dir, prof_name);
  NSDL1_RBU(vptr, NULL,"Making performance_trace directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;

  //make .TRxxx directory only if post_proc is on in scenario file
  if(global_settings->rbu_har_rename_info_file){
  sprintf(log_tr_dir, "%s/%s/.TR%d/", har_log_dir, prof_name, testidx);
  NSDL1_RBU(vptr, NULL,"Making hidden TRxxx directory at path = %s ", log_tr_dir);
  if(make_dir(log_tr_dir) != 0)
    return -1;
  }
  //move har to archive
  //Bug-79445: Remove system commmand
  //Temporary adding system commmand here remove it late and find alternative
  //sprintf(log_tr_dir,"mv %s/%s/*.har %s/%s/archive/ 2>/dev/null", har_log_dir, prof_name, har_log_dir, prof_name);
  //NSDL1_RBU(vptr, NULL,"running command to move files to archive =  %s ", log_tr_dir);
  /*
  if(system(log_tr_dir) != 0);
  {
    NSDL1_RBU(vptr, NULL, "Unable to move files to archive error = %s ", nslb_strerror(errno));
    return -1;
  }
  */

  char archive_dir[512 + 1];
  snprintf(log_tr_dir, 512, "%s/%s", har_log_dir, prof_name);

  struct dirent *de; //Pointer for directory entry

  // opendir() returns a pointer of DIR type.
  DIR *dr = opendir(log_tr_dir);

  if (dr == NULL) //opendir returns NULL if couldn't open directory
  {
    NSDL1_RBU(vptr, NULL, "Unable to open dir '%s' error = %s ", log_tr_dir, nslb_strerror(errno));
    return 0;
  }

  while ((de = readdir(dr)) != NULL)
  {
    if(strstr(de->d_name, ".har"))
    {
      snprintf(log_tr_dir, 512, "%s/%s/%s", har_log_dir, prof_name, de->d_name);
      snprintf(archive_dir, 512, "%s/%s/archive/%s", har_log_dir, prof_name, de->d_name);
      if(rename(log_tr_dir, archive_dir) < 0)
      {
        NSDL1_RBU(vptr, NULL, "Unable to move files to archive, error = %s ", nslb_strerror(errno));
        return -1;
      }
    }
  }

  closedir(dr);

  return 0;

}

/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : ns_rbu_cleanup_prof_on_session_start 
 *
 * Purpose   : This function will delete and create profiles, using cleanup option and user must 
 *             provide sample_profile. It will call on start of each session for cleanup profiles.
 *
 * Input     : vptr, prof_name(profile_name with which prof), sample_profile (This is the profile
 *             from where profiles are copied.)
 *
 * Output    : On error     -1
 *             On success    0
 *        
 * Build_v   : 4.1.5 #6 
 *------------------------------------------------------------------------------------------------------------------*/
static int ns_rbu_cleanup_prof_on_session_start(VUser *vptr, char *prof_name, char *sample_profile)
{
  char profile_buf [1024 + 1];

  NSDL2_RBU(NULL, NULL, "Method Called, prof_name = %s, sample_profile = %s", prof_name, sample_profile);

  #ifdef NS_DEBUG_ON
    sprintf(profile_buf, "nsu_auto_gen_prof_and_vnc -o cleanup -P %s -s %s -w -B %d -D ", prof_name,
                          sample_profile, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode);
  #else
    sprintf(profile_buf, "nsu_auto_gen_prof_and_vnc -o cleanup -P %s -s %s -w -B %d ", prof_name,
                          sample_profile, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode);
  #endif

  NSDL2_RBU(NULL, NULL, "Cleaning profile by running command = %s", profile_buf);

  nslb_system2(profile_buf);

  NSDL2_RBU(NULL, NULL, "Successfully running cmd '%s'\n", profile_buf);

  return 0;
}

//This function will call from on_session_start 
int ns_rbu_on_session_start(VUser *vptr)
{
  char prof_name[1024 + 1] = "";
  char har_log_path[1024 + 1] = "";  // For firefox /home/<user_name>/.rbu/.mozilla/firefox/logs and For Chrome /home/<user_name>/.rbu/.chrome/logs

  NSDL1_RBU(vptr, NULL,"total_runprof_entries = %d, enable_auto_param = %d, enable_rbu = %d", total_runprof_entries, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_auto_param,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu);

  /* Set har logs path for Firefox */
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    snprintf(har_log_path, 1024, "%s/.rbu/.mozilla/firefox/logs", g_home_env);
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    snprintf(har_log_path, 1024, "%s/.rbu/.chrome/logs", g_home_env);
  else if (vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    NSDL4_RBU(vptr, NULL, "RBU is running with Java Type Script. Don't check for browser_mode as we are using selenium.");
  }
  else
  {
    //TODO: not supported yet so return form here
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_on_session_start() - Browser mode %d not supported yet. Try in next release.\n", 
                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode);
    return -1;
  }

  if(global_settings->rbu_enable_auto_param)
  {
    NSDL2_RBU(vptr, NULL, "Auto parameter is on.");
    ns_advance_param("cav_browser_user_profile");

    strcpy(prof_name, ns_eval_string("{cav_browser_user_profile}"));
  }
  else if (vptr->cur_page != NULL)
  {
    RBU_Param_Shr *rbu_param = &vptr->cur_page->first_eurl->proto.http.rbu_param;
    NSDL2_RBU(vptr, NULL, "Advance param group: cur_page = %s, rbu_param = %p", vptr->cur_page->page_name, rbu_param);
    if(rbu_param->browser_user_profile != NULL)
    {
      ns_rbu_advance_param(rbu_param->browser_user_profile);
      strcpy(prof_name, ns_eval_string(rbu_param->browser_user_profile));
      NSDL4_RBU(vptr, NULL, "Profile Name = %s", prof_name);
    }
    //Now we are supporting RBU Page Stat without running RBU in Java type script using selenium. 
    //So, for now, we check if G_RBU is enabled and we have Java-Type script, we by-pass the flow of RBU 
    //This was Done to run PMS application.
    //TODO: Revisit this code later.
    else if (vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)    
    {
      NSDL4_RBU(vptr, NULL, "RBU is running with Java Type Script. Don't check fror profile as we are not using profiles.");
    }
    else
    {
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_on_session_start() - failed to getting browser user profile name.\n");
      return -1;
    }
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.prof_cleanup_flag)
  {
    if((ns_rbu_cleanup_prof_on_session_start(vptr, prof_name, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.sample_profile)) == -1)
    {
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_cleanup_prof_on_session_start() - Unable to cleanup profiles.\n");
      vptr->page_status = NS_REQUEST_ERRMISC;
      vptr->sess_status = NS_REQUEST_ERRMISC;
      return -1;
    }
  }

  NSDL4_RBU(vptr, NULL, "rbu_screen_sim_mode = %d, browser_mode = %d", 
                         global_settings->rbu_screen_sim_mode, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode);

  //Add screen_size in firefox.
  //In Firefox 42, extension is unable to simulate screen size of browser Hence adding manually in perf.js
  //if((global_settings->rbu_screen_sim_mode) && (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == 0))
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim) && 
     (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == 0))
    ns_rbu_set_screen_size(vptr, prof_name);
  
  //Move old data in archive if exist
  //Call this function only if prof_name exist.
  if(*prof_name)
  {
    if(move_to_archive(vptr, testidx, prof_name, har_log_path) == 0)
      NSDL4_RBU(vptr, NULL, "Sucessfully returned form move to archive function");
    else
      NSDL4_RBU(vptr, NULL, "Error occured at move to archive function not taking any action");
  }

  vptr->sess_ptr->netTest_start_time = get_ms_stamp();

  NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Progress; Total Page=%d; Executed Page=%d",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
                 vptr->sess_ptr->num_pages, vptr->sess_ptr->netTest_page_executed);
  NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Execution Started",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"));

 return 0;
}

// Read screen shot file and store this into vptr->httpData->rbu_resp_att->resp_body
long ns_rbu_read_ss_file(VUser *vptr)
{
  struct stat stat_buf;
  int read_data_fd = 0;
  long read_bytes;
  char ss_file[1024 + 1];

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL2_RBU(vptr, NULL, "Method called, vptr = %p, rbu_resp_attr = %p, page_screen_shot_file = %s,  last_malloced_size = %d", 
                             vptr, rbu_resp_attr, rbu_resp_attr->page_screen_shot_file, rbu_resp_attr->last_malloced_size);

  // Since extention added .jpeg so add 
  snprintf(ss_file, 1024, "%s.jpeg", rbu_resp_attr->page_screen_shot_file);
  NSDL2_RBU(vptr, NULL, "ss_file = %s", ss_file);

  rbu_resp_attr->screen_shot_file_flag = 0;  // Set to 0 first

  if(stat(ss_file, &stat_buf) == -1)
  {
    NSDL2_RBU(vptr, NULL, "Warrning: RBU screen shot file '%s' not present", ss_file);
    NSTL1_OUT(NULL, NULL, "Warrning: RBU screen shot file '%s' not present.\n", ss_file); 
    return -1;
  }

  if ((read_data_fd = open(ss_file, O_RDONLY|O_CLOEXEC)) < 0)
  {
    NSDL2_RBU(vptr, NULL, "Warrning: Failed  in RBU screen shot opening file (%s). Error = %s", rbu_resp_attr->page_screen_shot_file,
                            nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "Warrning: Failed  in RBU screen shot opening file (%s). Error = %s\n", rbu_resp_attr->page_screen_shot_file, 
                     nslb_strerror(errno));
    return -1;
  }

  NSDL2_RBU(vptr, NULL, "old full buff size = %d, new full buf size = %d", rbu_resp_attr->last_page_dump_buf_malloced, stat_buf.st_size);
  if((rbu_resp_attr->last_page_dump_buf_malloced == 0) || (stat_buf.st_size > (rbu_resp_attr->last_page_dump_buf_malloced - 1))) // -1 as we are adding null at the end
  {
    MY_REALLOC(rbu_resp_attr->page_dump_buf, stat_buf.st_size + 1, "Realloc page_dump_buf for screen shot", -1);
    rbu_resp_attr->last_page_dump_buf_malloced = stat_buf.st_size + 1;
  }
  rbu_resp_attr->page_dump_buf_len = stat_buf.st_size;

  read_bytes = nslb_read_file_and_fill_buf(read_data_fd, rbu_resp_attr->page_dump_buf, rbu_resp_attr->page_dump_buf_len);
  NSDL2_RBU(vptr, NULL, "read_bytes = %d, screen shot file size = %d", read_bytes, stat_buf.st_size);

  if(read_bytes == stat_buf.st_size)
    rbu_resp_attr->screen_shot_file_flag = 1;

  close(read_data_fd);
  
  return(read_bytes); // can be -1 if error if above method
}


/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will called from function vut_execute() when task VUT_RBU_WEB_URL_END found.
 *             This function behave like ns_http_close_connection() for RBU
 *             Here -
 *             (1) Handle url complete  
 *             (2) Handle page complete
 *
 * Input     : vptr    - to point current vptr 
 *             now     - to provide time  
 *
 * Output    : 
 *--------------------------------------------------------------------------------------------*/
void ns_rbu_handle_web_url_end(VUser *vptr, u_ns_ts_t now)
{
  char taken_from_cache = 0;                  // No 
  action_request_Shr *url_num;
  connection *cptr = vptr->last_cptr;  
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  //What ever '0' or '1' will come is_incomplete_har_file, will be used for log_page_rbu_detail_record
  char is_page_detail_done = rbu_resp_attr->is_incomplete_har_file;

  int done = 1;
  int url_ok = 0;
  int status =0;
  int url_type;
  int redirect_flag = 0;
  int request_type;
  int url_id;
  const int con_num = cptr->conn_fd;
  ns_8B_t flow_path_instance = cptr->nd_fp_instance;
  char reset_last_cptr = 0;

  NSDL4_RBU(vptr, cptr, "Method Called, vptr = %p, now = %u", vptr, now);

  url_num =  cptr->url_num;
  url_type = url_num->proto.http.type;
  request_type = url_num->request_type;
  status = cptr->req_ok;
  url_ok = !status;

  NSDL4_RBU(vptr, cptr, "status = %d, url_ok = %d", status, url_ok);

  //Filling graphs
  //For Bug ID: 17133, Not save data if HAR file is incomplete.
  //ns_rbu_log_access_log: called here for negative case only, for positive it is done while processing HAR
  if(is_page_detail_done == 1) 
  {
    rbu_fill_page_status(vptr, NULL);

    //In Case of JS Check-point, when abort is applied, we need to log access_log from here.
    //Otherwise, Already dumped for failure cases, from nsi_rbu_handle_page_failure 
    //if(status == NS_REQUEST_CV_FAILURE || ptr->continue_error_value == 0) 
    //  ns_rbu_log_access_log(vptr, NULL, RBU_ACC_LOG_DUMP_LOG);   
  }else{
    set_rbu_page_stat_data_avgtime_data(vptr, NULL);
    set_rbu_domain_stat_data_avgtime_data(vptr);       //if able to access rbu_domain_timings here.
  }

  //set_rbu_page_status_data_avgtime_data(vptr, NULL);  //Bug - 83012

  //Need to reset each time on web url end,
  rbu_resp_attr->is_incomplete_har_file = 0;

  if(status != NS_REQUEST_OK) 
    abort_page_based_on_type(cptr, vptr, url_type, redirect_flag, status);
  
  handle_url_complete(cptr, request_type, now, url_ok, redirect_flag, status, taken_from_cache);

  if(!taken_from_cache && LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  {
    NSDL4_RBU(vptr, cptr, "Call log_url_record function");
    if((global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED) && (cptr->url_num->proto.http.url_index == -1))
      url_id = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)cptr->url, cptr->url_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    else
      url_id = cptr->url_num->proto.http.url_index;

    if (log_url_record(vptr, cptr, status, cptr->request_complete_time, redirect_flag, con_num, flow_path_instance, url_id) == -1)
      NSTL1_OUT(NULL, NULL, "Error in logging the url record\n");
  }

  //cptr->req_ok = NS_REQUEST_OK;
  
  //vptr->last_cptr get reset in case 'Page failed & continue on page error is false and done is true', so storing it.
  connection *tmp = vptr->last_cptr;
  handle_page_complete(cptr, vptr, done, now, request_type);

  set_rbu_page_status_data_avgtime_data(vptr, NULL);

  //For NetTest
  vptr->sess_ptr->netTest_page_executed++;
  global_settings->netTest_total_executed_page++;

  NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Progress; Total Page=%d; Executed Page=%d",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
                 vptr->sess_ptr->num_pages, vptr->sess_ptr->netTest_page_executed);
  NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=OverallExecutedPage; Total Executed Page=%d",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"), 
                 global_settings->netTest_total_executed_page);

  NSDL4_RBU(vptr, NULL, "is_page_detail_done = %d", is_page_detail_done);

  if(vptr->last_cptr == NULL)  //If last_cptr has been reset then restore it, as it is needed in logging records into CSV.
  {
    vptr->last_cptr = tmp;
    reset_last_cptr = 1;
  }

  if(rbu_resp_attr->rbu_light_house)
    log_rbu_light_house_detail_record(vptr, rbu_resp_attr->rbu_light_house);

  if (rbu_resp_attr->mark_and_measures)
  {
    log_rbu_mark_and_measure_record(vptr, rbu_resp_attr);
  }

  //calling log_page_rbu_detail_record() to dump data into csv
  if((rbu_resp_attr) && ((is_page_detail_done == 0) && strcmp(rbu_resp_attr->har_name, "")))
  {
    log_page_rbu_detail_record(vptr, rbu_resp_attr->har_name, rbu_resp_attr->har_date_and_time, rbu_resp_attr->speed_index,
                                 rbu_resp_attr->cav_nv_val, NULL, rbu_resp_attr->profile);
  }
  
  if(reset_last_cptr)         //Reset the last_cptr
    vptr->last_cptr = NULL;

//SM: remove POST req file only if NS_DEBUG_ON is not defined
#ifndef NS_DEBUG_ON
  if(vptr->httpData->rbu_resp_attr->post_req_filename && *(vptr->httpData->rbu_resp_attr->post_req_filename))
  {
    if(unlink(vptr->httpData->rbu_resp_attr->post_req_filename) == -1)
    {
      NSTL1_OUT(NULL, NULL, "\nPOST req file %s could not be deleted, error: %s\n", vptr->httpData->rbu_resp_attr->post_req_filename, nslb_strerror(errno));
    }
  }
#endif
//
  free_connection_slot(cptr, now);
//  if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
//    is_rbu_web_url_end_done = 1;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will called form function nsi_web_url_int().
 *             Here -
 *             (1) Make dummy connection to store data only , In RBU connection making is done by 
 *                 Firefox so cptr is not participating in making connection.  
 *
 *             (2) Set this connection to vptr->last_cptr, Since In RBU we have only one url MAIN  
 *                 URL so here vptr and cptr has one to one mapping and hence we can switch from cptr 
 *                 to vptr and vise versa But in case of Normal script not because vptr 
 *                 has one to many mapping to cptr hence we can switch from cptr to vptr but not vptr to cptr.
 *
 *             (3) Now copy data from vptr to cptr, following are main -
 *                 (a) cptr->url_num 
 *                 (b) cptr->gServerTable_idx
 *                 (c) cptr->request_type 
 *                 (d) cptr->num_retries 
 *                 (e) cptr->url        --> by function http_make_url_and_check_cache() 
 *                 (f) cptr->url_len 
 *                 (g) cptr->old_svr_entry
 *
 *             (5) Switch context: NVM --> User
 *
 * Input     : vptr    - to point current vptr 
 *             page_id - to provide page id of that PAGE
 *
 * Output    : 
 *--------------------------------------------------------------------------------------------*/
void ns_rbu_setup_for_execute_page(VUser *vptr, int page_id, u_ns_ts_t now)
{
  int ret = 0;
  connection* cptr = vptr->last_cptr;
  action_request_Shr *url_num = (vptr->first_page_url? vptr->first_page_url: vptr->cur_page->first_eurl) ;
  PerHostSvrTableEntry_Shr* svr_entry;
  
  NSDL1_RBU(vptr, cptr, "Method called: vptr=%p, cptr=%p, page_id = %d, now = %u", vptr, cptr, page_id, now);

  NSDL2_RBU(vptr, NULL, "Before urls_left at new_user = %d, partition_idx = %lld", vptr->urls_left, vptr->partition_idx);

  vptr->urls_left--;
  vptr->cnum_parallel = 1;

  /* Setting partition_idx for each page */
  vptr->partition_idx = g_partition_idx; 


  NSDL2_RBU(vptr, NULL, "After urls_left at new_user = %d, partition_idx = %lld", vptr->urls_left, vptr->partition_idx);

  /* (1) Making Dummy connection to store data only */
  NSDL2_RBU(vptr, NULL, "Before get_free_connection_slot cptr=%p", cptr);

  cptr = get_free_connection_slot(vptr);

  NSDL2_RBU(vptr, NULL, "After get_free_connection_slot cptr=%p", cptr);

  /* (2) Point this cptr to vptr->last_cptr */
  vptr->last_cptr = cptr; // Set it so that in API we can get cptr

  /* (3) Now set cpoy data from cptr to vptr */
  SET_URL_NUM_IN_CPTR(cptr, url_num);
  cptr->num_retries = 0;

  // To init used_param table for page dump and user trace 
  init_trace_up_t(vptr); 

  reset_cptr_attributes(cptr);

  //Make : getserver entry to map server host 2171 - 22 , 
  /* (4) Map Server Host */
  if((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) 
  {
    NSTL1(NULL, NULL, "Start Socket: Unknown host\n");
    //end_test_run();
    NS_EXIT(-1, "Start Socket: Unknown host");
  } 
  else 
  {
    cptr->old_svr_entry = svr_entry;
  }

  /* Make url point it into cptr->url */
  http_make_url_and_check_cache(cptr, now, &ret);
  
  /* Info: set_cptr_for_new_req(cptr, vptr, now); we no need to set timeout so just increase average_time->fetches_started*/
  average_time->fetches_started++;  

  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->fetches_started++;
  }

  if(NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) 
  {
    NSDL2_USER_TRACE(vptr, cptr, "Method called, User tracing enabled");
    ut_update_url_values(cptr, ret); // It will save the first URL of page as there is check in the function
  }

  // Switch Context to Vuser or send control to user thread for actual execution of the page using exteranl browser (firefox)
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    //send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0); // TODO- Check what is done ..
    return;
  }
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
//    NSDL2_API(vptr, NULL, "RBU is enabled with Java-type Script vptr->page_instance is [%hu].", vptr->page_instance);
//    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_URL_REP, 0);
    return;
  }
  else
  {
    NSDL3_RBU(vptr, cptr, "WebUrlOnPageStartOver...");
//    switch_to_vuser_ctx(vptr, "WebUrlOnPageStartOver");
    return;
  }
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse registration.spces and create corresponding tables.
 *             (1) Make .registrations.spec
 *             (2) Add file parameter API - 
 *                  nsl_static_var(cav_browser_user_profile:1, cav_har_log_dir:2, cav_vnc_display_id:3, File=/etc/rbu_parameter.csv, Refresh=SESSION, Mode=UNIQUE);
 *
 * Input     : script_filepath   - to provide script path where registration.spec for RBU have to make 
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int ns_rbu_parse_registrations_file(char *script_filepath, int sess_idx, int *reg_ln_no)
{
  //FILE *reg_fp = NULL;
  char registration_file[1024];
  //int len;
  int i;
  //buf[0] = 0;
  registration_file[0] = 0;
  
  NSDL1_PARSING(NULL, NULL, "Method Called, script_filepath = %s, sess_idx = %d, reg_ln_no = %d", script_filepath, sess_idx, reg_ln_no);

  //Bug 89992
  //check rbu is enabled or not for sess_idx(script). If script is not used for rbu then do not parse .registrations.spec
  for(i = 0; i < total_runprof_entries; i++)
  {
    if((runProfTable[i].sessprof_idx == sess_idx) && !runProfTable[i].gset.rbu_gset.enable_rbu)
      return NS_PARSE_SCRIPT_SUCCESS;
  }

  sprintf(registration_file, "%s/etc/%s", g_ns_wdir, RBU_REGISTRATION_FILENAME);
 
  //sprintf(buf, "cp %s/etc/rbu_parameter.csv %s/%s 2>/dev/null", g_ns_wdir, g_ns_wdir, script_filepath);
  
  //system(buf);

  NSDL1_PARSING(NULL, NULL, "registration_file = [%s]", registration_file);

  if (ns_parse_registration_file(registration_file, sess_idx, reg_ln_no) == NS_PARSE_SCRIPT_ERROR)
  {
    return NS_PARSE_SCRIPT_ERROR;
  }

  return NS_PARSE_SCRIPT_SUCCESS; 
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword ENABLE_NS_FIREFOX and set flag enable_ns_firefox 
 *             in group based setting structure 
 *
 * Input     : buf   : ENABLE_NS_FIREFOX <mode> <firefox bin path>  
 *                     mode  - 0 mean NS firefox is disable (default)
 *                     mode  - 1 mean NS firefox is enable 
 *                     path  - if mode 1 then one can set firefox bin path (optional)
 *                             if path is not given then use default path "/home/cavisson/thirdparty/firefox/ffx_32" 
 *                     Eg: ENABLE_NS_FIREFOX 1 /home/cavisson/.ns_mozilla/firefox
 *                         ENABLE_NS_FIREFOX 1 (here default path will be used)
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_ns_firefox(char *buf, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char firefox_binpath[RBU_MAX_PATH_LENGTH];
  char usages[RBU_MAX_USAGE_LENGTH];
  char num_args = 0;
  int imode = 0;  /* Take integer value of mode. */

  /* Init */
  keyword[0] = 0;
  mode[0] = 0;
  firefox_binpath[0] = 0;
  usages[0] = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s], runtime_changes = %d, protocol_enabled = %x", buf, runtime_changes, 
                             global_settings->protocol_enabled & RBU_API_USED);

  /* Making usages */
  sprintf(usages, "Usages:\n"
                  "Syntax - ENABLE_NS_FIREFOX <mode> <path>\n"
                  "Where:\n"
                  "  mode - provide netstorm firefox is enable or not.\n"
                  "         0 - disabled (Default)\n"
                  "         1 - enabled\n"
                  "  path - provide netstorm firefox binary path.\n"
                  "       - applicable only with mode 1\n"
                  "       - Eg: /home/cavisson/.ns_mozilla/firefox\n"
                  "       -     /home/cavisson/thirdparty/firefox/ffx_43 (Default path).\n");

  //Commented becuase we are now parse ENABLE_NS_CHROME this keyword befor G_RBU
  #if 0
  /* This keyword used only when browser api used */
  if(!(global_settings->protocol_enabled & RBU_API_USED))
  {
     NSDL1_API(NULL, NULL, "Since in script is running in pure non-RBU mode hence not parsing keyword ENABLE_NS_FIREFOX."); 
     return 0;
  }
  #endif

  num_args = sscanf(buf, "%s %s %s", keyword, mode, firefox_binpath);

  NSDL2_PARSING(NULL, NULL, "Method called, keyword = [%s], mode = [%s], firefox_binpath = [%s]", keyword, mode, firefox_binpath);  
  
  /* Validation */
  if(num_args < 2 && num_args > 3) 
  {
    NSTL1_OUT(NULL, NULL, "Error: provided number of argument (%d) is wrong. At least 2 and at most 3 argumnet expected.\n%s", num_args, usages);
    return -1;
  }
  
  imode = atoi(mode); 
  NSDL2_PARSING(NULL, NULL, "imode = %d", imode);

  /* mode should be 0 or 1 only */ 
  if(imode != 0 && imode != 1) 
  {
    NSTL1_OUT(NULL, NULL, "Error: mode should be 0 or 1 only.\n%s", usages); 
    return -1;
  }

  #if 0
  /* in BBT wan can be used only with netstorm firefox */
  if(!global_settings->wan_env) //WAN ENV off 
  {
    if(imode) //NS Firefox enable 
    {
      NSTL1_OUT(NULL, NULL, "Error: Netstorm firefox used only for Browser Based Testing if WAN_ENV = 1. Hence disable Netstorm Firefox.\n%s", 
                       usages);
      return -1;
    }
    //Wan env is off so dont need to set firefox path
    NSDL2_PARSING(NULL, NULL, "wan_env = %d", global_settings->wan_env);
    global_settings->enable_ns_firefox = imode; 
    return 0;
  }
  #endif

  if(imode == 0)
  {
    global_settings->enable_ns_firefox = imode;
    return 0;
  }

  /* According to bug 23122, default path of NS Firefox change from /home/cavisson/thirdparty/firefox to 
       /home/cavisson/thirdparty/firefox/ffx_32 
  */
  if(firefox_binpath[0] == 0) // Set default path 
      strcpy(firefox_binpath, "/home/cavisson/thirdparty/firefox/ffx_43");

  /*if(imode == 1)
  {
    //If mode is 1 and chrome bin path is not provided then set it to default
    if(firefox_binpath[0] == 0) // Set default path 
      strcpy(firefox_binpath, "/home/cavisson/thirdparty/firefox");
  }*/

  NSDL2_PARSING(NULL, NULL, "Browser used = %d", global_settings->browser_used);

  //Chrome binary check should be after parsing and before start test 
  #if 0
  /* Check firefox command exist or not and browser_used is added to check which browser is running. */
  if((firefox_binpath[0] != 0) && (global_settings->browser_used == FIREFOX_AND_CHROME || global_settings->browser_used == FIREFOX)) 
  {
    char cmd[1024];
    sprintf(cmd, "%s/firefox", firefox_binpath);
    if (stat(cmd, &stat_buf) == -1)
    {
      NSTL1_OUT(NULL, NULL, "Error: command '%s' not found.\n%s", cmd, usages);
      return -1;
    }
  }
  #endif
    
  strcpy(g_ns_firefox_binpath, firefox_binpath);

  /* Store mode in Globle data structure as it will be used when we call apis */
  global_settings->enable_ns_firefox = imode; 
 
  NSDL2_PARSING(NULL, NULL, "On end - g_ns_firefox_binpath = [%s], enable_ns_firefox = [%d]", 
                                 g_ns_firefox_binpath, global_settings->enable_ns_firefox);
  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_USER_AGENT and set flag enable_ns_firefox 
 *             in group based setting structure 
 *
 * Input     : buf   : RBU_USER_AGENT <mode> <user agent path>  
 *                     mode  - 0 provide Firefox default user agent
 *                     mode  - 1 provide user agent from Internet user profile
 *                     mode  - 2 provide user agent string along with mode
 *                     path  - if mode 2 , then path need to be provided
 *                     Eg: RBU_USER_AGENT 2 Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; HTC_Amaze_4G Build/GRJ22) AppleWebKit/533.1
 *
 * Output    : On error    -1
 *             On success   0 
 *Changes    : Converting RBU_USER_AGENT as group based G_RBU_USER_AGENT, due to design changes
 *--------------------------------------------------------------------------------------------*/
int kw_set_rbu_user_agent(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char user_agent[RBU_MAX_USAGE_LENGTH];
  int num_args = 0;
  int imode = 0;
  char* buf_ptr;

  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";

  
  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], runtime_flag = %d", buf, runtime_flag);

  sscanf(buf, "%s", keyword);

  if((strcasecmp(keyword, "RBU_USER_AGENT")) == 0)
  {
    num_args = sscanf(buf, "%s %s %s", keyword, mode, user_agent);
    NSDL2_PARSING(NULL, NULL, "num_args = [%d], keyword = [%s]", num_args, keyword);

    if(ns_is_numeric(mode) == 1)
    {
      imode = atoi(mode);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_2);
    }

    if((imode == 1) || (imode == 2))
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_3);
    }  
    return 1;       //Return 1, as RBU_USER_AGENT has been deprecated
  }

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, user_agent);
  NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], mode = [%s], group = [%s], user_agent = [%s]", num_args, keyword, mode, group_name, user_agent);

  if(num_args < 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_1);
  }
 
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode))
  {
    imode=atoi(mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_2);
  }
   
  if(imode != 0 && imode != 1 && imode != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_3);
  }

  global_settings->rbu_user_agent = imode;

  if(imode == 2 && num_args < 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011188, CAV_ERR_MSG_1);
  }
  else if(imode == 2)
  {
    buf_ptr = strstr(buf, " ");
    buf_ptr++; //Point to group

    buf_ptr = strstr(buf_ptr, " ");
    buf_ptr++; //Point to mode

    buf_ptr = strstr(buf_ptr, " ");
    CLEAR_WHITE_SPACE(buf_ptr); //buf_ptr will point to user_agent_string
    CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(buf_ptr);
    NSDL2_PARSING(NULL, NULL, "UA = %s", buf_ptr);

    if(strlen(buf_ptr) > RBU_USER_AGENT_LENGTH)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_USER_AGENT_USAGE, CAV_ERR_1011248, buf_ptr, strlen(buf_ptr), "");
    }

    //CLEAR_WHITE_SPACE_FROM_END(buf_ptr);

    NSDL2_PARSING(NULL, NULL, "The Value of buf_ptr = %s", buf_ptr);
 
    snprintf(g_rbu_user_agent_str, RBU_USER_AGENT_LENGTH, "%s", buf_ptr);
  }

  gset->rbu_gset.user_agent_mode = imode;
  strcpy(gset->rbu_gset.user_agent_name, g_rbu_user_agent_str);

  NSDL2_PARSING(NULL, NULL, "At end - rbu_user_agent = %d, g_rbu_user_agent_str = %s", global_settings->rbu_user_agent, 
                                      g_rbu_user_agent_str[0]?g_rbu_user_agent_str:NULL);
  
 return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU and set rbu_flag in group based setting structure 
 *
 * Input     : buf   :  G_RBU <group_name> <page_all> <rbu_mode> <ss_mode> <auto_param_mode> <stop_browser_on_sess_end> <browser_mode> 
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_g_rbu(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char page_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char mode[RBU_MAX_KEYWORD_LENGTH] = "0";
  char ss_mode[RBU_MAX_KEYWORD_LENGTH] = "0";
  char param_mode[RBU_MAX_KEYWORD_LENGTH] = "0";
  char stop_browser_on_sess_end_flag[RBU_MAX_KEYWORD_LENGTH] = "1"; //stop_browser_flag
  char browser_mode[RBU_MAX_KEYWORD_LENGTH] = "0"; //browser_mode
  char lighthouse_mode[RBU_MAX_KEYWORD_LENGTH] = "0";               //lighthouse plugin mode
  int imode = 0;        /* Take integer value of mode. */
  int iss_mode = 0;     /* Take integer value of ss_mode. */
  int iparam_mode = 0;   /* Take integer value of param_mode. */
  int istop_browser_on_sess_end_flag = 1;
  int ibrowser_mode = 0;
  int ilighthouse_mode = 0;
  char temp[RBU_MAX_USAGE_LENGTH] = "";
  char *ptr = NULL;
  int chrome_version = 0;
  
  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], gset = [%p], runtime_changes = %d", buf, gset, runtime_changes);



  int count_rbu_arg = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s", keyword, group_name, page_name, mode, ss_mode, param_mode, 
                                                 stop_browser_on_sess_end_flag, browser_mode, lighthouse_mode, temp);
 
  NSDL2_PARSING(NULL, NULL, "Total number of arguments = [%d]", count_rbu_arg);

  if ((count_rbu_arg < 8) || (count_rbu_arg > 9))
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_1);
  }


  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s],  page_name = [%s], mode = [%s],ss_mode = [%s], "
                            "param_mode = [%s], stop_browser_on_sess_end_flag = [%s], browser_mode = [%s], lighthouse_mode = [%s]" , 
                             keyword, group_name, page_name, mode, ss_mode, param_mode, stop_browser_on_sess_end_flag,
                             browser_mode, lighthouse_mode);  
  
  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
   
  if(ns_is_numeric(mode))
  {
    imode = atoi(mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(ss_mode))
  {
    iss_mode = atoi(ss_mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(param_mode))
  {
    iparam_mode = atoi(param_mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
  }
  
  if(ns_is_numeric(stop_browser_on_sess_end_flag))
  {
    istop_browser_on_sess_end_flag = atoi(stop_browser_on_sess_end_flag);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(browser_mode))
  {
    ibrowser_mode = atoi(browser_mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
  }

  if(count_rbu_arg == 9 )
  {
    if(ns_is_numeric(lighthouse_mode))
      ilighthouse_mode = atoi(lighthouse_mode);
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_2);
    }
  }

  NSDL2_PARSING(NULL, NULL, "imode = %d, iss_mode= %d, iparam_mode = %d, stop_browser_on_sess_end_flag = %d, ibrowser_mode = %d, "
                "ilighthouse_mode = %d",
                imode, iss_mode, iparam_mode,istop_browser_on_sess_end_flag, ibrowser_mode, ilighthouse_mode);

  /* mode should be 0 or 1 only */ 
  if((imode != 0 && imode != 1 && imode != 2) || (iss_mode != 0 && iss_mode != 1) || (iparam_mode != 0 && iparam_mode != 1) || 
         (istop_browser_on_sess_end_flag != 0 && istop_browser_on_sess_end_flag != 1) || (ibrowser_mode != 0 && ibrowser_mode != 1) ||
         (ilighthouse_mode != 0 && ilighthouse_mode != 1)) 
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011166, CAV_ERR_MSG_3);
  }

  /* If At least one group has RBU script then set bit RBU_API_USED in flag protocol_enabled */
  if(imode != 0)
  {
    NSDL4_MISC(NULL, NULL, "Setting flag protocol_enabled to RBU_API_USED");
    global_settings->protocol_enabled |= RBU_API_USED;
  }
  //Checking for default case Atul Sharma 
  if ( !(imode || iss_mode || iparam_mode || istop_browser_on_sess_end_flag || ibrowser_mode) )
    global_settings->browser_used = -1; //ignore default case 
  else if ( global_settings->browser_used != FIREFOX_AND_CHROME ) {
  //Setting browser_mode here which is used in post proc
  if ( ibrowser_mode == FIREFOX ) 
  {
    if ( global_settings-> browser_used != CHROME )
      global_settings-> browser_used = FIREFOX;
    else
      global_settings->browser_used = FIREFOX_AND_CHROME;
  }
  else if ( ibrowser_mode == CHROME ) 
  {
    if ( global_settings-> browser_used != FIREFOX )
      global_settings-> browser_used = CHROME;
    else  
      global_settings->browser_used = FIREFOX_AND_CHROME;
  }
  
  }
  //setting create_screen_shot_dir variable here Atul Sharma
  if ( !(imode || iss_mode || iparam_mode || istop_browser_on_sess_end_flag || ibrowser_mode) )
    global_settings->create_screen_shot_dir = NONE_DIR_FLAG; //for default value
  else if ( global_settings->create_screen_shot_dir != ALL_DIR_FLAG ) {
  if ( iss_mode == 1 ) 
  {
    if ( global_settings->create_screen_shot_dir != SNAP_SHOT_DIR_FLAG )
      global_settings->create_screen_shot_dir = SCREEN_SHOT_DIR_FLAG;
    else 
      global_settings->create_screen_shot_dir = ALL_DIR_FLAG;
      
  }
  else if ( global_settings->create_screen_shot_dir != SNAP_SHOT_DIR_FLAG && global_settings->create_screen_shot_dir != SCREEN_SHOT_DIR_FLAG )
    global_settings->create_screen_shot_dir = NONE_DIR_FLAG;
  }

  if (ilighthouse_mode == 1)
  {
    if(ibrowser_mode != CHROME)
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011249, "");
    }

    ptr = index(g_ns_chrome_binpath, '_');
    ptr++;
    chrome_version = atoi(ptr);

    if(chrome_version < 93)
    { 
      //NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_USAGE, CAV_ERR_1011250, "");
      strcpy(g_ns_chrome_binpath, "/home/cavisson/thirdparty/chrome/chrome_93");
      chrome_version = 93;
      fprintf(stderr, "Lighthouse Reporting is best supported for Chrome Browser version 93 or above. So internally using Chrome Browser Version 93.");
    }
    imode = 2; //setting node mode default

    global_settings->create_lighthouse_dir = LIGHTHOUSE_DIR_FLAG;
  }
  else if (global_settings->create_lighthouse_dir != LIGHTHOUSE_DIR_FLAG)
   global_settings->create_lighthouse_dir = NONE_DIR_FLAG;

  NSDL2_PARSING(NULL, NULL, "global_settings->create_screen_shot_dir = %d " , global_settings->create_screen_shot_dir);
  NSDL2_PARSING(NULL, NULL, "global_settings->browser_used = %d " , global_settings->browser_used);
  NSDL2_PARSING(NULL, NULL, "global_settings->create_lighthouse_dir = %d " , global_settings->create_lighthouse_dir);
  gset->rbu_gset.enable_rbu = imode;
  gset->rbu_gset.enable_screen_shot = iss_mode;
  gset->rbu_gset.enable_auto_param = iparam_mode;
  gset->rbu_gset.stop_browser_on_sess_end_flag = istop_browser_on_sess_end_flag;
  gset->rbu_gset.browser_mode = ibrowser_mode;
  gset->rbu_gset.lighthouse_mode = ilighthouse_mode;

  //Bug 47773 : Only cavisson User can run test so, setting it globally "/home/cavisson"
  //set g_home_env
  //strcpy(g_home_env, "/home/cavisson");

  NSDL2_PARSING(NULL, NULL, "On end - gset->rbu_gset.enable_rbu = %d, gset->rbu_gset.enable_screen_shot = %d, "
                            "gset->rbu_gset.enable_auto_param = %d, gset->rbu_gset.stop_browser_on_sess_end_flag = %d, "
                            "gset->rbu_gset.browser_mode = %d, gset->rbu_gset.lighthouse_mode = %d, g_home_env = %s", 
                gset->rbu_gset.enable_rbu, gset->rbu_gset.enable_screen_shot, gset->rbu_gset.enable_auto_param, 
                gset->rbu_gset.stop_browser_on_sess_end_flag, gset->rbu_gset.browser_mode, gset->rbu_gset.lighthouse_mode, g_home_env);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will parse keyword G_RBU_CAPTURE_CLIPS <group_name> <mode> [<frequency>] [<quality>] 
 *             [domload_threshold] [onload_threshold]
 *
 * Input     : buf   : G_RBU_CAPTURE_CLIPS <group_name> <mode> [<frequency>] [<quality>] [domload_threshold] [onload_threshold] 
 *                     Group_name        - ALL (Default)
 *                     mode              - disable/enable
 *                                         - 0 - disable
 *                                         - 1 - enable
 *                                         - 2 - enable, with [domload_threshold] [onload_threshold] (By default 0 0 ) 
 *                     frequency         - 3000 (By default 100 milliseconds) 
 *                     quality           - 50 (Default 100)
 *                     domload_threshold - 90 (By default 0 milliseconds)
 *                     onload_threshold  - 100 (By default 0 milliseconds)                   
 *                     Eg: G_RBU_CAPTURE_CLIPS ALL 1 3000 50  <or>
 *                     Eg: G_RBU_CAPTURE_CLIPS ALL 1 3000 50 90 100 
 *
 * Output    : On error    -1
 *             On success   0
 *-----------------------------------------------------------------------------------------------------------------------*/


int kw_set_g_rbu_capture_clips(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char mode[RBU_MAX_KEYWORD_LENGTH] = "0";  
  char frequency[RBU_MAX_KEYWORD_LENGTH] = "100";
  char quality[RBU_MAX_KEYWORD_LENGTH] = "100";
  char domload_th[11 + 1] = "0";
  char onload_th[11 + 1] = "0";
  int imode = 0;
  int ifrequency = 0;
  int iquality = 0;
  int idomload_th = 0;
  int ionload_th = 0;
  int num_args = 0;
 
  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], gset = [%p], runtime_changes = %d", buf, gset, runtime_flag);

  num_args = sscanf(buf, "%s %s %s %s %s %s %s", keyword, group_name, mode, frequency, quality, domload_th, onload_th);

  if(num_args > 7)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], frequency = [%s], quality = [%s]",
                keyword, group_name, mode, frequency, quality);

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode))
  {
    imode = atoi(mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_2);
  }

  // mode should be 0, 1 or 2 only 
  if(match_pattern(mode, "^[0-2]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_3);
  }

  if((imode == 1) && (num_args != 5))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_1);
  }

  /* Resolved Bug 26867 - RBU-Core | Test should not be started, if we run the test with mode 2 in "G_RBU_CAPTURE_CLIPS"
     without mentioning the threshold values. */
  if((imode == 2) && (num_args < 7))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(frequency))
  {
    ifrequency = atoi(frequency);
    if(ifrequency < 5 || ifrequency > 10000)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011182, "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(quality))
  {
    iquality = atoi(quality);
    if(iquality < 0 || iquality > 100)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011184, "Quality", "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(domload_th))
  {
    idomload_th = atoi(domload_th);
    if(idomload_th < 0 || idomload_th > 120000)   //2min
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011183, "DOM Threshold", "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(onload_th))
  {
    ionload_th = atoi(onload_th);
    if(ionload_th < 0 || ionload_th > 120000)  //2min
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011183, "Onload Threshold", "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_CLIPS_USAGE, CAV_ERR_1011181, CAV_ERR_MSG_2);
  }

  NSDL2_PARSING(NULL, NULL, "imode = %d, ifrequency = %d, iquality = %d, idomload_th = %d, ionload_th = %d", 
                             imode, ifrequency, iquality, idomload_th, ionload_th);

  gset->rbu_gset.enable_capture_clip = imode;
  gset->rbu_gset.clip_frequency = ifrequency;
  gset->rbu_gset.clip_quality = iquality;
  gset->rbu_gset.clip_domload_th = idomload_th;
  gset->rbu_gset.clip_onload_th = ionload_th;

  NSDL2_PARSING(NULL, NULL, "On end - gset->rbu_gset.enable_capture_clip = %d, gset->rbu_gset.clip_frequency = %d, "
                            "gset->rbu_gset.clip_quality = %d, clip_domload_th = %d, clip_onload_th = %d",
                             gset->rbu_gset.enable_capture_clip, gset->rbu_gset.clip_frequency, gset->rbu_gset.clip_quality,
                             gset->rbu_gset.clip_domload_th, gset->rbu_gset.clip_onload_th);
  NSDL2_PARSING(NULL, NULL, "On starting - global_setting->create_screen_shot_dir = %d", global_settings->create_screen_shot_dir);

  if ( !(imode || ifrequency || iquality) )
    global_settings->create_screen_shot_dir = NONE_DIR_FLAG; //for default value
  else if ( global_settings->create_screen_shot_dir != ALL_DIR_FLAG ) {
  //setting create_dir_flag here which is used in creating directory
  if (imode) 
  {
    if ( global_settings->create_screen_shot_dir != SCREEN_SHOT_DIR_FLAG )
      global_settings->create_screen_shot_dir = SNAP_SHOT_DIR_FLAG;
    else 
      global_settings->create_screen_shot_dir = ALL_DIR_FLAG;

  }
  else if ( global_settings->create_screen_shot_dir != SCREEN_SHOT_DIR_FLAG && global_settings->create_screen_shot_dir != SNAP_SHOT_DIR_FLAG )
    global_settings->create_screen_shot_dir = NONE_DIR_FLAG;
  }
  NSDL2_PARSING( NULL, NULL, "On end - global_setting->create_screen_shot_dir = %d", global_settings->create_screen_shot_dir);
  return 0;
}


/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_SCREEN_SIZE_SIM 
 *
 * Input     : buf   : RBU_SCREEN_SIZE_SIM <mode> <screen_width> <screen_height>  
 *                     mode  - 0 - disable
 *                     mode  - 1 - enable
 *                     screen_width - width part of screen resolution
 *                     screen_height - height part of screen resolution
 *                     Eg: RBU_SCREEN_SIZE_SIM 1 320 480
 *
 * Output    : On error    -1
 *             On success   0 
 * Changes regarding Group based G_RBU_SCREEN_SIZE_SIM
 *--------------------------------------------------------------------------------------------*/
int kw_set_rbu_screen_size_sim (char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_USAGE_LENGTH];
  char tmp[RBU_MAX_USAGE_LENGTH];   //No use of tmp
  int imode = 0;
  int num_args = 0;

  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";

  sscanf(buf, "%s", keyword);
  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], runtime_flag = %d", buf, runtime_flag);

  if((strcasecmp(keyword, "RBU_SCREEN_SIZE_SIM")) == 0)
  {
    num_args = sscanf(buf, "%s %s %s", keyword, mode, tmp);
    NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], mode = [%s]", num_args, keyword, mode);

    if(ns_is_numeric(mode) == 1)
    {
      imode = atoi(mode);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SCREEN_SIZE_SIM_USAGE, CAV_ERR_1011195, CAV_ERR_MSG_2);
    }

    if(imode != 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SCREEN_SIZE_SIM_USAGE, CAV_ERR_1011195, CAV_ERR_MSG_3);
    }
    return 1;       //Returns 1, as RBU_SCREEN_SIZE_SIM has been deprecated
  }

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, tmp);

  if((num_args < 2) || (num_args > 3))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SCREEN_SIZE_SIM_USAGE, CAV_ERR_1011195, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], group_name = [%s], mode = [%s]", num_args, keyword, group_name, mode);

  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 1)
  {
    imode = atoi(mode);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg,G_RBU_SCREEN_SIZE_SIM_USAGE, CAV_ERR_1011195, CAV_ERR_MSG_2);
  }

  if(imode != 0 && imode != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg,G_RBU_SCREEN_SIZE_SIM_USAGE, CAV_ERR_1011195, CAV_ERR_MSG_3);
  }

  global_settings->rbu_screen_sim_mode = imode;
  gset->rbu_gset.screen_size_sim = imode;

  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_POST_PROC
 *
 * Input     : buf   : RBU_POST_PROC <mode> <file_name> 
 *                     mode  - 0 - disable
 *                     mode  - 1 - It is enable, but you should provide keyword with file name
 *                                 Eg: RBU_POST_PROC 1 /home/<username>/<controller_name>/<file_name>
 *                     mode  - 2 - To move downloaded files to $NS_WDIR/<TR>/<Partition>/rbu_logs/downloads
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_rbu_post_proc_parameter(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode_str[RBU_MAX_USAGE_LENGTH];
  char spec_file[RBU_MAX_USAGE_LENGTH];
  int num_args = sscanf(buf, "%s %s %s", keyword, mode_str, spec_file);

  if(num_args < 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_POST_PROC_USAGE, CAV_ERR_1011203, CAV_ERR_MSG_1);
  }

  if(!ns_is_numeric(mode_str))  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_POST_PROC_USAGE, CAV_ERR_1011203, CAV_ERR_MSG_2);
  }

  int mode = atoi(mode_str);
  if(mode != 0 && mode != 1 && mode != 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_POST_PROC_USAGE, CAV_ERR_1011203, CAV_ERR_MSG_3);
  }
  if(mode == 1) {
    if(num_args < 3) {
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg,RBU_POST_PROC_USAGE, CAV_ERR_1011203, CAV_ERR_MSG_1);
    }
    MY_MALLOC(global_settings->rbu_har_rename_info_file, strlen(spec_file) + 1, "rbu_har_rename_info_file", -1);
    strcpy(global_settings->rbu_har_rename_info_file, spec_file);
    NSDL2_PARSING(NULL, NULL, "RBU_POST_PROC, mode = %d, har_rename_info_file = %s", 
                                                      mode, global_settings->rbu_har_rename_info_file);
  }
  else if(mode == 2) {
   global_settings->rbu_move_downloads_to_TR_enabled = 1;
   if(num_args == 3) {
      MY_MALLOC(global_settings->rbu_har_rename_info_file, strlen(spec_file) + 1, "rbu_har_rename_info_file", -1);
      strcpy(global_settings->rbu_har_rename_info_file, spec_file);
      NSDL2_PARSING(NULL, NULL, "RBU_POST_PROC, mode = %d, har_rename_info_file = %s",
                                                      mode, global_settings->rbu_har_rename_info_file);
    }
  }
  else
    global_settings->rbu_move_downloads_to_TR_enabled = 0;

  NSDL2_PARSING(NULL, NULL, "RBU_POST_PROC, mode = %d, rbu_move_downloads_to_TR_enabled = %d",
                      mode, global_settings->rbu_move_downloads_to_TR_enabled);
  return 0;
}

/* keyword to enable rbu settings */
int kw_set_rbu_settings_parameter(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode_str[RBU_MAX_USAGE_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
 
  NSDL2_PARSING(NULL, NULL, "Method called.buf = [%s]", buf);
  int num_args = 0;
  int mode = 0;

  sscanf(buf, "%s", keyword);
 
  if((strcasecmp(keyword, "RBU_SETTINGS")) == 0)
  {
    num_args = sscanf(buf, "%s %s", keyword, mode_str);

    if(ns_is_numeric(mode_str) == 1)
    {
      mode = atoi(mode_str);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SETTINGS_USAGE, CAV_ERR_1011185, CAV_ERR_MSG_2);
    }

    if(mode != 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SETTINGS_USAGE, CAV_ERR_1011284, keyword, "");
    }
    return 1;       //Return 1, as RBU_SETTINGS has been deprecated 
  }

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, mode_str);

  if(num_args < 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SETTINGS_USAGE, CAV_ERR_1011185, CAV_ERR_MSG_1);
  }

  val_sgrp_name(buf, group_name, 0);

  if(!ns_is_numeric(mode_str))  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SETTINGS_USAGE, CAV_ERR_1011185, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  NSDL2_PARSING(NULL, NULL, "mode = %d", mode);

  //mode should be 0 or >= 100 
  if(mode == 0 || (mode >= 100 && mode <=10000)) {
    gset->rbu_gset.rbu_settings = mode;
    NSDL2_PARSING(NULL, NULL, "G_RBU_SETTINGS, gset->rbu_gset.rbu_settings = %d", gset->rbu_gset.rbu_settings);
  }else {
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_SETTINGS_USAGE, CAV_ERR_1011186, "");
  }

  return 0;
}

/*---------------------------------------------------------------------------------------------
 * Purpose   : This function will parse keyword RBU_ENABLE_CSV
 *
 * Input     : buf   : RBU_ENABLE_CSV <mode> 
 *                     mode  - 0 - disable
 *                     mode  - 1 - enable, it will enable csv file generation at path 
 *                     $NS_WDIR/logs/TRXX/reports/harp_csv
 *
 * Output    : On error    -1
 *             On success   0 
 *-------------------------------------------------------------------------------------------*/
int kw_set_rbu_enable_csv(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode_str[RBU_MAX_USAGE_LENGTH];
  char tmp_str[RBU_MAX_USAGE_LENGTH];
  
  NSDL2_PARSING(NULL, NULL, "Method called. buf = [%s]", buf);
  int num_args = sscanf(buf, "%s %s %s", keyword, mode_str, tmp_str);

  if(num_args < 2 || num_args > 2) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_CSV_USAGE, CAV_ERR_1011206, CAV_ERR_MSG_1);
  }

  if(!ns_is_numeric(mode_str))  
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_CSV_USAGE, CAV_ERR_1011206, CAV_ERR_MSG_2);
  }

  int mode = atoi(mode_str);
  NSDL2_PARSING(NULL, NULL, "RBU_ENABLE_CSV mode = %d", mode);

  //mode should be 0 or 1 
  if(mode == 0 || mode == 1) 
  {
    global_settings->rbu_enable_csv = mode;
    NSDL2_PARSING(NULL, NULL, "RBU_ENABLE_CSV, global_settings->rbu_enable_csv = %d", global_settings->rbu_enable_csv);
  }
  else 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_CSV_USAGE, CAV_ERR_1011206, CAV_ERR_MSG_3);
  }

  return 0;
}

/*---------------------------------------------------------------------------------------------
 * Purpose   : This function will parse keyword RBU_BROWSER_COM_SETTINGS
 *
 * Input     : buf   : RBU_BROWSER_COM_SETTING <mode> <frequency> <Interval>
 *                     mode       :  0 - disable
 *                                :  1 - enable
 *                     frequency  :  positive integer less then 257 
 *                     Interval   :  interval time in milli second to reconnect 
 *
 * Output    : On error    -1 exit with -1 
 *             On success   0 return 
 *-------------------------------------------------------------------------------------------*/
int kw_set_rbu_browser_com_settings(char *buf, char *err_msg, int runtime_flag)
{

  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode_str[RBU_MAX_USAGE_LENGTH];
  char frequency_str[RBU_MAX_USAGE_LENGTH];
  char interval_str[RBU_MAX_USAGE_LENGTH];
  char tmp_str[RBU_MAX_USAGE_LENGTH]; 
  int mode, frequency, interval; 
 
  NSDL2_PARSING(NULL, NULL, "Method called. buf = [%s]", buf);

  int num_args = sscanf(buf, "%s %s %s %s %s", keyword, mode_str, frequency_str, interval_str, tmp_str);

  if(num_args != 4) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011207, CAV_ERR_MSG_1);
  }

  //setting mode
  if(ns_is_numeric(mode_str))
  {
    mode = atoi(mode_str);
    if(mode == 0 || mode == 1)
    {
      global_settings->rbu_com_setting_mode = (char) (mode);
      NSDL2_PARSING(NULL, NULL, "global_settings->rbu_com_setting_mode= %d", global_settings->rbu_com_setting_mode);
    }
    else 
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011207, CAV_ERR_MSG_3);
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011207, CAV_ERR_MSG_2);
  }

  //setting up frequency
  if(ns_is_numeric(frequency_str))
  {
    frequency = atoi(frequency_str);
    if(frequency >= 0 && frequency <= 256)
    {
      global_settings->rbu_com_setting_max_retry = frequency;
      NSDL2_PARSING(NULL, NULL, "global_settings->rbu_com_setting_max_retry = %d", global_settings->rbu_com_setting_max_retry);
    }
    else 
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011208, "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011207, CAV_ERR_MSG_2);
  }

  //setting up interval
  //300 milli second is default interval set in Keyword_definition.dat
  if(ns_is_numeric(interval_str))
  {
    interval = atoi(interval_str);
    //interval should be vary in range 3000 to 120000
    if(interval >= 3000 && interval <= 120000)
    {
      global_settings->rbu_com_setting_interval = (unsigned int) (interval);
      NSDL2_PARSING(NULL, NULL, "global_settings->rbu_com_setting_interval = %d", global_settings->rbu_com_setting_interval);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011209, "");
    }
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_BROWSER_COM_SETTINGS_USAGE, CAV_ERR_1011207, CAV_ERR_MSG_2);
  }

  return 0;
}

/* RBU_ENABLE_DUMMY_PAGE <mode> <url> */
int kw_set_rbu_enable_dummy_page(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_MODE_LENGTH];
  char dummy_url[RBU_MAX_DEFAULT_PAGE_LENGTH];
  int imode = -1;

  dummy_url[0] = 0;

  NSDL1_PARSING(NULL, NULL, "Method Called., buf = [%s], runtime_flag = %d", buf, runtime_flag); 

  int num_args = sscanf(buf, "%s %s %s", keyword, mode, dummy_url);

  NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], mode = [%s], url = [%s]", num_args, keyword, mode, dummy_url); 

  if(num_args < 2) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_DUMMY_PAGE_USAGE, CAV_ERR_1011210, CAV_ERR_MSG_1);
  }

  if(!ns_is_numeric(mode))  
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_DUMMY_PAGE_USAGE, CAV_ERR_1011210, CAV_ERR_MSG_2);
  }

  imode = atoi(mode);

  global_settings->rbu_enable_dummy_page = imode;

  if(imode != 0 && imode != 1) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_ENABLE_DUMMY_PAGE_USAGE, CAV_ERR_1011210, CAV_ERR_MSG_3);
  }
  
  NSDL2_PARSING(NULL, NULL, "imode = %d, dummy_url = %s", imode, *dummy_url?dummy_url:"NULL");

  if(imode == 1 && dummy_url[0] != 0)
    strcpy(g_rbu_dummy_url, dummy_url);
  else
    strcpy(g_rbu_dummy_url, "about:blank");
  
  NSDL2_PARSING(NULL, NULL, "rbu_enable_dummy_page = %d, g_rbu_dummy_url = [%s]", global_settings->rbu_enable_dummy_page, g_rbu_dummy_url);

  return 0;
}

//Keyword Parsing of Chrome
int kw_set_ns_chrome(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char chrome_binpath[RBU_MAX_PATH_LENGTH];
  char num_args = 0;
  int imode = 0;  // Take integer value of mode. 
  //struct stat stat_buf;

  // Init 
  keyword[0] = 0;
  mode[0] = 0;
  chrome_binpath[0] = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s], err_msg = [%s], runtime_changes = %d, protocol_enabled = %x", buf, err_msg, runtime_flag,
                             global_settings->protocol_enabled & RBU_API_USED);


  //Commented becuase we are now parse ENABLE_NS_CHROME this keyword befor G_RBU
  #if 0
  // This keyword used only when browser api used 
  if(!(global_settings->protocol_enabled & RBU_API_USED))
  {
     NSDL1_API(NULL, NULL, "Since in script no RBU API is used hence not parsing keyword ENABLE_NS_CHROME.");
     return 0;
  }
  #endif

  num_args = sscanf(buf, "%s %s %s", keyword, mode, chrome_binpath);

  NSDL2_PARSING(NULL, NULL, "Method called, keyword = [%s], mode = [%s], chrome_binpath = [%s]", keyword, mode, chrome_binpath);

  // Validation 
  if(num_args < 2 && num_args > 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_NS_CHROME_USAGE, CAV_ERR_1011179, CAV_ERR_MSG_1);
  }

  imode = atoi(mode);
  NSDL2_PARSING(NULL, NULL, "imode = %d", imode);

  // mode should be 0 or 1 only 
  if(imode != 0 && imode != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_NS_CHROME_USAGE, CAV_ERR_1011179, CAV_ERR_MSG_3);
  }

  if(imode == 0)
  {
    global_settings->enable_ns_chrome = imode;
    return 0;
  }

  /* According to bug 23122, default path of NS chromium is changed from /home/cavisson/thirdparty/chrome to 
     /home/cavisson/thirdparty/chrome/chrome_40 
  */
  if(chrome_binpath[0] == 0) // Set default path 
    strcpy(chrome_binpath, "/home/cavisson/thirdparty/chrome/chrome_78");

 /* if(imode == 1)
  {
    //If mode is 1 and chrome bin path is not provided then set it to default
    if(chrome_binpath[0] == 0) // Set default path 
      strcpy(chrome_binpath, "/home/cavisson/thirdparty/chrome");
  }*/

  //Chrome binary check should be after parsing and before start test 
  #if 0
  NSDL2_PARSING(NULL, NULL, "Browser used = %d", global_settings->browser_used);
  // Check chrome command exist or not and browser_used is added to check which browser is running.
  if((chrome_binpath[0] != 0) && (global_settings->browser_used == FIREFOX_AND_CHROME || global_settings->browser_used == CHROME))
  {
    char cmd[1024];
    sprintf(cmd, "%s/chrome", chrome_binpath);
    if (stat(cmd, &stat_buf) == -1)
    {
      NSTL1_OUT(NULL, NULL, "Error: command '%s' not found.\n%s", cmd, usages);
      return -1;
    }
  }
  #endif

  strcpy(g_ns_chrome_binpath, chrome_binpath);

  // Store mode in Globle data structure as it will be used when we call apis
  global_settings->enable_ns_chrome = imode;

  NSDL2_PARSING(NULL, NULL, "On end - g_ns_chrome_binpath = [%s], enable_ns_chrome = [%d]",
                                 g_ns_chrome_binpath, global_settings->enable_ns_chrome);
  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_ENABLE_AUTO_PARAM
 *
 * Input     : buf          : RBU_ENABLE_AUTO_PARAM  <mode>
 *                            mode can be 0 or 1.
 *			      0 - Disbale (Default)
 *			      1 - Enable  
 *
 * Output    : On success    0
 *             On error     -1 
 *--------------------------------------------------------------------------------------------*/
int kw_set_rbu_enable_auto_param(char *buf)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char usages[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_USAGE_LENGTH];

  NSDL1_PARSING(NULL, NULL, "Method called buf = [%s], protocol_enabled = %x", buf, global_settings->protocol_enabled & RBU_API_USED);

  sprintf(usages, "Usages:\n"
                  "Syntax - RBU_ENABLE_AUTO_PARAM <mode>\n"
                  "Where:\n"
                  "  mode - 0 or 1\n"
                  "         0 - disabled (Default)\n"
                  "         1 - enabled\n");

  NSDL2_RBU(NULL, NULL, "Method called.buf = [%s]", buf);

  int num_args = sscanf(buf, "%s %s", keyword, mode);

  if(num_args < 2) {
    NSTL1_OUT(NULL, NULL, "Error: mode is Missing.\n%s", usages);
    exit(-1);
  }

  if(!ns_is_numeric(mode))  {
    NSTL1_OUT(NULL, NULL, "Error: mode \'%s\' is Invalid.\n%s", mode, usages);
    exit(-1);
  }

  global_settings->rbu_enable_auto_param = atoi(mode);

  NSDL2_RBU(NULL, NULL, "On end rbu_enable_auto_param = %d", global_settings->rbu_enable_auto_param); 

  return 0; 
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_CACHE_SETTING 
 *
 * Input     : buf          : G_RBU_CACHE_SETTING  <enable_cache> <clear_cache_on_sess_start> 
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_cache_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  int mode = 1;
  int cache_mode = 0;
  int cookie_mode = 0;
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s], runtime_changes = %d, protocol_enabled = %x", buf, runtime_changes,
                             global_settings->protocol_enabled & RBU_API_USED);

  num_args = sscanf(buf, "%s %s %d %d %d", keyword, group_name, &mode, &cache_mode, &cookie_mode);

  if(num_args > 5)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_SETTING_USAGE, CAV_ERR_1011190, CAV_ERR_MSG_1);
  }
  
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%d], cache_mode = %d, cookie_mode = %d", 
                              keyword, group_name, mode, cache_mode, cookie_mode);

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if((mode != 0 && mode != 1) || (cache_mode != 0 && cache_mode != 1 && cache_mode != 2 && cache_mode != 3) || (cookie_mode != 0 && cookie_mode != 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_SETTING_USAGE, CAV_ERR_1011190, CAV_ERR_MSG_3);
  }

  gset->rbu_gset.enable_cache = mode;
  gset->rbu_gset.clear_cache_on_start = cache_mode;
  gset->rbu_gset.clear_cookie_on_start = cookie_mode;

  NSDL2_PARSING(NULL, NULL, "End: enable_cache = %d, clear_cache_on_start = %d, clear_cookie_on_start = %d", 
                             gset->rbu_gset.enable_cache, gset->rbu_gset.clear_cache_on_start,  gset->rbu_gset.clear_cookie_on_start);

  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_PAGE_LOADED_TIMEOUT 
 *
 * Input     : buf          : G_RBU_PAGE_LOADED_TIMEOUT <group_name> <timeout> <phase_interval> 
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_page_loaded_timeout(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  int timeout = 5000;
  int phase_interval = 4000;

  NSDL1_PARSING(NULL, NULL, "buf = [%s], runtime_changes = %d, protocol_enabled = %x", buf, runtime_changes,
                             global_settings->protocol_enabled & RBU_API_USED);

  sscanf(buf, "%s %s %d %d", keyword, group_name, &timeout, &phase_interval);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], time = [%d], phase_interval = [%d]", 
                              keyword, group_name, timeout, phase_interval);

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(timeout < 500) 
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_PAGE_LOADED_TIMEOUT_USAGE, CAV_ERR_1011212, "");
  }
  if(phase_interval < 2000) 
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg,G_RBU_PAGE_LOADED_TIMEOUT_USAGE, CAV_ERR_1011213, "");
  }
  gset->rbu_gset.page_loaded_timeout = timeout;
  gset->rbu_gset.pg_load_phase_interval = phase_interval;

  NSDL2_PARSING(NULL, NULL, "End: timeout = %d, phase_interval = %d", gset->rbu_gset.page_loaded_timeout,
                 gset->rbu_gset.pg_load_phase_interval); 

  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_ADD_HEADER
 *
 * Input     : buf          : G_RBU_ADD_HEADER <group_name> <mode>
 *                            mode can be 0, 1 or 2
 *                            0) Customize headers are disable in chrome and firefox.
 *
 *                            For Firefox, mode can be
 *                            1) If we are sending header with body (postheader) then, it will add it to main url only not in inline urls.
 *                            2) If we are sending headers without body then, it will be shown in both main and inline urls.
 *
 *                            For Chrome, mode can be
 *                            1) We cannot send with body (postheader) request.
 *                            2) If we are sending headers without body then, it will be shown in both main and inline urls. 
 *
 *             err_msg         : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/

int kw_set_g_rbu_add_header(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char mode[RBU_MAX_KEYWORD_LENGTH];
  int imode;
  int num_args = 0;
  
  NSDL1_PARSING(NULL, NULL, "Method called, buf = [%s], runtime_changes = %d, protocol_enabled = %x", buf, runtime_changes, 
                             global_settings->protocol_enabled & RBU_API_USED);

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, mode);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], num_args = [%d]", 
                             keyword, group_name, mode, num_args);

  // Validation 
  if(num_args < 2 && num_args > 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ADD_HEADER_USAGE, CAV_ERR_1011191, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
 
  if(match_pattern(mode, "^[0-2]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ADD_HEADER_USAGE, CAV_ERR_1011191, CAV_ERR_MSG_3);
  }
 
  imode = atoi(mode);
  NSDL2_PARSING(NULL, NULL, "imode = %d", imode); 
  if(imode != 0 && imode != 1 && imode != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ADD_HEADER_USAGE, CAV_ERR_1011191, CAV_ERR_MSG_3);
  }
  
  gset->rbu_gset.rbu_header_flag = imode;

  NSDL2_PARSING(NULL, NULL, "End: imode = %d", gset->rbu_gset.rbu_header_flag);

  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_CLEAN_UP_PROF_ON_SESSION_START
 *
 * Input     : buf          : G_RBU_CLEAN_UP_PROF_ON_SESSION_START <group_name> <mode> <sample_profile>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_clean_up_prof_on_session_start(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char sample_profile[RBU_MAX_KEYWORD_LENGTH];
  int imode;
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "Method called, buf = [%s], runtime_changes = %d, protocol_enabled = %x", buf, runtime_changes,
                             global_settings->protocol_enabled & RBU_API_USED);

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, sample_profile);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], sample_profile = [%s], num_args = [%d]",
                             keyword, group_name, mode, sample_profile, num_args);

  // Validation 
  if(num_args < 3 && num_args > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CLEAN_UP_PROF_ON_SESSION_START_USAGE, CAV_ERR_1011192, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CLEAN_UP_PROF_ON_SESSION_START_USAGE, CAV_ERR_1011192, CAV_ERR_MSG_3);
  }

  imode = atoi(mode);
  gset->rbu_gset.prof_cleanup_flag = imode;

  if(gset->rbu_gset.prof_cleanup_flag)
  {
    if(strcmp(sample_profile, ""))  //If it doesn't have sample_profiles it will throw error.
    {
      strcpy(gset->rbu_gset.sample_profile, sample_profile);
      NSDL2_PARSING(NULL, NULL, "sample_profile = %s", gset->rbu_gset.sample_profile);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CLEAN_UP_PROF_ON_SESSION_START_USAGE, CAV_ERR_1011192, CAV_ERR_MSG_1);
    }
  }
  return 0;
}

int kw_set_tti(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  int imode = 0;
  int num_args = 0;  
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";

  sscanf(buf, "%s", keyword);

  if((strcasecmp(keyword, "RBU_ENABLE_TTI_MATRIX")) ==0 )
  {
    sscanf(buf, "%s %s", keyword, mode);
    NSDL2_PARSING(NULL, NULL, "keyword = %s, mode = %s ", keyword, mode);

    if(ns_is_numeric(mode) == 1)
    {
      imode = atoi(mode);
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_ENABLE_TTI_MATRIX_USAGE, CAV_ERR_1011196, CAV_ERR_MSG_2);
    }

    if(imode == 1)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_ENABLE_TTI_MATRIX_USAGE, CAV_ERR_1011287, keyword, "");
    }
    return 1;       //Return 1, as RBU_ENABLE_TTI_MATRIX has been deprecated
  }

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, mode);
  NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], mode = [%s], group = [%s]", num_args, keyword, mode, group_name);

  if(num_args < 3)
  {
    NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, G_RBU_ENABLE_TTI_MATRIX_USAGE, CAV_ERR_1011196, CAV_ERR_MSG_1);
  }    

  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_ENABLE_TTI_MATRIX_USAGE, CAV_ERR_1011196, CAV_ERR_MSG_2); 
  }

  imode = atoi(mode);
  if(imode != 0 && imode != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_ENABLE_TTI_MATRIX_USAGE, CAV_ERR_1011196, CAV_ERR_MSG_3);  
  }
  global_settings->rbu_enable_tti = imode;
  gset->rbu_gset.tti_mode = imode;
  NSDL2_RBU(NULL, NULL, "Method ends, global_settings->rbu_enable_tti = [%d]", global_settings->rbu_enable_tti);
  return 0;  
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_HAR_SETTING <GRP_NAME> <MODE> <COMPRESSION> <REQ> <RESP> <JS_PROC_TIME_AND_DATA_URI>
 *
 * Input     : buf          : G_RBU_HAR_SETTING <GRP_NAME> <MODE> <COMPRESSION> <REQ> <RESP> <JS_PROC_TIME_AND_DATA_URI>
 *                            Where:
 *                                 group_name    - G1, RBU etc
 *                                 mode          - 0 Disable (Default)
 *                                               - 1 Enable
 *                                 compression   - 0 Disable (This will not compress HAR)
 *                                               - 1 Enable (This will compress HAR)
 *                                 request       - 0 Disable (This will not dump body in HAR in request)
 *                                               - 1 Enable (This will dump body in HAR in request)
 *                                 response      - 0 Disbale (This will not dump body in HAR in response)
 *                                               - 1 Enable (This will dump body in HAR in response)
 *                                 JS processing time and data URI      - 0 Disable (This will not dump JS processing time and data URI in HAR)
 *                                                                      - 1 Enable (This will dump JS processing time)
 *                                                                      - 2 Enable (This will data URI in HAR)
 *                                                                      - 3 Enable (This will dump both JS processing time and data URI in HAR)
 *
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *
 * Output    : On error    -1
 *             On success   0
 * 
 * Build_ver : 4.1.5#16 
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_har_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char compression[RBU_MAX_KEYWORD_LENGTH];
  char request[RBU_MAX_KEYWORD_LENGTH];
  char response[RBU_MAX_KEYWORD_LENGTH];
  char js_proc_and_data_uri[RBU_MAX_KEYWORD_LENGTH] = {0};
  char temp[RBU_MAX_KEYWORD_LENGTH] = {0};
  int num_args = 0;

  NSDL3_RBU(NULL, NULL, "Method called buf = %s", buf);

   num_args = sscanf(buf, "%s %s %s %s %s %s %s %s", keyword, group_name, mode, compression, request, response, js_proc_and_data_uri, temp);
   if(num_args < 6 || num_args > 7)
   {
     NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_1);
   }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_3);

  if(ns_is_numeric(compression) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_2);
  }

  if(match_pattern(compression, "^[0-1]$") == 0)
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_3);

  if(ns_is_numeric(request) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_2);
  }

  if(match_pattern(request, "^[0-1]$") == 0)
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_3);
  
  if(ns_is_numeric(response) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_2);
  }

  if(match_pattern(response, "^[0-1]$") == 0)
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_3);

  if(num_args == 7)
  {
    if(ns_is_numeric(js_proc_and_data_uri) == 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_2);
    }
    if(match_pattern(js_proc_and_data_uri, "^[0-3]$") == 0)
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_SETTING_USAGE, CAV_ERR_1011193, CAV_ERR_MSG_3);

    gset->rbu_gset.rbu_js_proc_tm_and_dt_uri = atoi(js_proc_and_data_uri);
  }

  gset->rbu_gset.rbu_har_setting_mode = atoi(mode);
  gset->rbu_gset.rbu_har_setting_compression = atoi(compression);
  gset->rbu_gset.rbu_har_setting_request = atoi(request);
  gset->rbu_gset.rbu_har_setting_response = atoi(response);
  
  NSDL3_RBU(NULL, NULL, "Method end mode = %d, compression = %d, request = %d, response = %d, js_proc_time_and_data_uri = %d", 
                         gset->rbu_gset.rbu_har_setting_mode, gset->rbu_gset.rbu_har_setting_compression, 
                         gset->rbu_gset.rbu_har_setting_request, gset->rbu_gset.rbu_har_setting_response,
                         gset->rbu_gset.rbu_js_proc_tm_and_dt_uri); 
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_CACHE_DOMAIN <GRP_NAME> <MODE> <DOMAIN_LIST> 
 *    
 * Input     : buf          : G_RBU_CACHE_DOMAIN <GRP_NAME> <MODE> <DOMAIN_LIST>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.5#26
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_cache_domain(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{     
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char domain_list[1024 + 1] = "";
  int num_args = 0, i;
  char *domain_fields[50 + 1] = {0};

  NSDL3_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, domain_list);
  if(num_args < 3 && num_args > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_DOMAIN_USAGE, CAV_ERR_1011187, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_DOMAIN_USAGE, CAV_ERR_1011187, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_DOMAIN_USAGE, CAV_ERR_1011187, CAV_ERR_MSG_3);
  }

  gset->rbu_gset.rbu_cache_domain_mode = atoi(mode);
  strcpy(gset->rbu_gset.domain_list, domain_list);

  int domain_num_tokens = get_tokens(domain_list, domain_fields, ";",  51);
  NSDL4_RBU(NULL, NULL, "num_tokens = %d", domain_num_tokens);
  
  if(domain_num_tokens > 50)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_CACHE_DOMAIN_USAGE, CAV_ERR_1011288, "");
  }
  else
  {
    for(i=0 ; i < domain_num_tokens ; i++)
    {
      NSDL4_RBU(NULL, NULL, "domain_fields[%d] = %s", i, domain_fields[i]);
    }
  }

  NSDL3_RBU(NULL, NULL, "Method end, mode = %d", gset->rbu_gset.rbu_cache_domain_mode);
  
  return 0;
}


/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_ADD_ND_FPI <GRP_NAME> <MODE>
 *    
 * Input     : buf          : G_RBU_ADD_ND_FPI <GRP_NAME> <MODE>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.5#
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_add_nd_fpi(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{     
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char usages[RBU_MAX_USAGE_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char send_mode[RBU_MAX_KEYWORD_LENGTH];
  char tmp_str[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;

  NSDL3_RBU(NULL, NULL, "Method called buf = %s", buf);

  sprintf(usages, "Usages:\n"
                  "Syntax - G_RBU_ADD_ND_FPI <GRP_NAME> <MODE> <SEND_MODE main/All urls>\n"
                  "Where:\n"
                  "group_name    - G1, RBU etc \n"
                  "mode          - 0 Disable (Default) \n"
                  "              - 1 Enable and set 'f' as header value \n"
                  "              - 2 Enable and set 'F' as header value \n"
                  "send mode     - 0 send request to main url only\n"
                  "              - 1 send request to main as well as inline urls(Default)\n");


  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group_name, mode, send_mode, tmp_str);
  if(num_args != 4)
  {
    NSTL1_OUT(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args, usages);
    return -1;
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if((ns_is_numeric(mode) == 0) || (match_pattern(mode, "^[0-2]$") == 0))
  {
    NSTL1_OUT(NULL, NULL, "Error: Mode %s is not numeric or mode is not 0 or 1 or 2.\n%s", mode, usages);
    return -1;
  }

  gset->rbu_gset.rbu_nd_fpi_mode = atoi(mode);
  
  if((ns_is_numeric(send_mode) == 0) || (match_pattern(send_mode, "^[0-1]$") == 0))
  {
    NSTL1_OUT(NULL, NULL, "Error: send_mode %s is not numeric or send_mode is not 0 or 1 or 2 .\n%s", send_mode, usages);
    return -1;
  }

  gset->rbu_gset.rbu_nd_fpi_send_mode = atoi(send_mode); 
  return 0;

}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_ALERT_SETTING <GRP_NAME> <MODE>
 *    
 * Input     : buf          : G_RBU_ALERT_SETTING <GRP_NAME> <MODE>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.7#27
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_alert_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{     
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char tmp_str[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;
  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s ", keyword, group_name, mode, tmp_str);
  if(num_args != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ALERT_SETTING_USAGE, CAV_ERR_1011194, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ALERT_SETTING_USAGE, CAV_ERR_1011194, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_ALERT_SETTING_USAGE, CAV_ERR_1011194, CAV_ERR_MSG_3);
  }

  gset->rbu_gset.rbu_alert_setting_mode = atoi(mode);

  NSDL2_RBU(NULL, NULL, "Method ends with rbu_alert_setting_mode = %d", gset->rbu_gset.rbu_alert_setting_mode);  
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_DOMAIN_IGNORE_LIST <GRP_NAME> <DOMAIN_LIST>
 *    
 * Input     : buf          : G_RBU_DOMAIN_IGNORE_LIST <GRP_NAME> <DOMAIN_LIST>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.7B# 4.1.8B#
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_domain_ignore_list(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char str[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;

  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, str);
  if(num_args != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_DOMAIN_IGNORE_LIST_USAGE, CAV_ERR_1011201, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
  if(strcasecmp(str, "NA") == 0)
  {
    NSDL2_RBU(NULL, NULL, "No domain provided in 'G_RBU_DOMAIN_IGNORE', So Ignoring 'G_RBU_DOMAIN_IGNORE'");
  }
  else
  {
    strcpy(gset->rbu_gset.rbu_domain_ignore_list, str);
    NSDL2_RBU(NULL, NULL, "Method ends with rbu_domain_ignore = %s", gset->rbu_gset.rbu_domain_ignore_list);
  }
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_BLOCK_URL_LIST <GRP_NAME> <URL_LIST>
 *    
 * Input     : buf             : G_RBU_BLOCK_URL_LIST <GRP_NAME> <URL LIST>
 *             err_msg         : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 *------------------------------------------------------------------------------------------------------------*/


int kw_set_g_rbu_block_url_list(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char url_list[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;
  char temp[RBU_MAX_USAGE_LENGTH];

  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);
  gset->rbu_gset.rbu_block_url_list = NULL;

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, url_list,temp);
  if(num_args != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_BLOCK_URL_LIST_USAGE, CAV_ERR_1011200, CAV_ERR_MSG_1);
  }
  
  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
  if(strcasecmp(url_list, "NA") == 0 || ((*url_list == '"') && (*(url_list + 1) == '"')))
  {
    NSDL2_RBU(NULL, NULL, "No Url is provided in 'G_RBU_BLOCK_URL_LIST', Hence ignoring");
  }
  else
  {
    MY_MALLOC_AND_MEMSET(gset->rbu_gset.rbu_block_url_list, strlen(url_list), "rbu_block_url_list", -1);
    strcpy(gset->rbu_gset.rbu_block_url_list, url_list);
    NSDL2_RBU(NULL, NULL, "Method ends with rbu_block_url = %s", gset->rbu_gset.rbu_block_url_list);
  }
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse G_RBU_HAR_TIMEOUT, this keyword is used to provide HAR file collection timeout
 * Previously: Only PageLoadWaitTime is provided from script for HAR file collection timeout 
 * From Now  : Using scenario also, we can provide HAR timeout for a group
 *    
 * Input     : buf          : G_RBU_HAR_TIMEOUT <group_name> <time (Sec)>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.8 B#5
 * Design : If script contains PageLoadWaitTime then overwrite G_RBU_HAR_TIMEOUT with PageLoadWaitTime for that specific page.
 *     
 * if(requests[url_idx].proto.http.rbu_param.page_load_wait_time < RBU_DEFAULT_PAGE_LOAD_WAIT_TIME){
     timeout = requests[url_idx].proto.http.rbu_param.page_load_wait_time;
 * else
 *   timeout = gset->rbu_gset.har_timeout; 
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_har_timeout(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  unsigned short har_timeout = RBU_DEFAULT_PAGE_LOAD_WAIT_TIME;   //As defined in page_load_wait_time of script

  NSDL1_PARSING(NULL, NULL, "buf = [%s], runtime_changes = %d", buf, runtime_changes);

  sscanf(buf, "%s %s %hu", keyword, group_name, &har_timeout);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], har_timeout = [%hu]",
                              keyword, group_name, har_timeout);

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(har_timeout < RBU_DEFAULT_PAGE_LOAD_WAIT_TIME)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_HAR_TIMEOUT_USAGE, CAV_ERR_1011198, "");
  }

  gset->rbu_gset.har_timeout = har_timeout;

  NSDL2_PARSING(NULL, NULL, "End: har_timeout = [%d] sec.", gset->rbu_gset.har_timeout);

  return 0;
}

typedef struct 
{
  int malloc_size;
  int free_size;
  int written_size;
}script_policy;

/*--------------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_ALERT_POLICY <MODE> <CUSTOM_POLICY1:SCRIPT_NAME1;CUSTOM_POLICY2:SCRIPT_NAME2>
 *    
 * Input     : buf             : RBU_ALERT_POLICY <MODE> <CUSTOM_POLICY1:SCRIPT_NAME1;CUSTOM_POLICY2:SCRIPT_NAME2>
 *             err_msg         : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 4.1.8 #37, 4.1.9 #4
 *-----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_rbu_alert_policy(char *buf, char *err_msg, int runtime_flag)
{
  char mode[25 + 1] = "";
  int num_args = 0;
  int num_policy_list = 0;
  int num_policy_colon = 0;
  int num_policy_fields_comma = 0;
  int imode = 0;
  int j = 0;
  char usages[RBU_MAX_USAGE_LENGTH + 1] = "";
  char policy_buf[RBU_MAX_USAGE_LENGTH + 1] = "";	//This buf contains whole string eg. P1:S1,S3;P2:S4
  char *policy_fields_buf[RBU_MAX_KEYWORD_LENGTH + 1];	//This buf is semicolon separated  
  char *policy_fields_colon[RBU_MAX_KEYWORD_LENGTH + 1];//This buf is colon separated 
  char *policy_fields_comma[RBU_MAX_KEYWORD_LENGTH + 1];//This buf is comma separated
  char keyword[RBU_MAX_KEYWORD_LENGTH + 1] = "";
  script_policy per_script_policy[256];

  NSDL2_RBU(NULL, NULL, "Method called, buf = [%s], err_msg = [%s], runtime_chages = [%d]", buf, err_msg, runtime_flag);

  sprintf(usages, "RBU_ALERT_POLICY <mode> <policy_and_script_name>\n"
                 "where: \n"
                 "mode                  : It can be 0/1 \n"
                 "                             0 - Disable (Default, No Policy is applied) \n"
                 "                             1 - Enable  (Policy is provided with policy_name:script_name)\n"
                 "policy_and_script_name: These are the custom policy provided with script_name\n"
                                          "Policy1:Script1,Script3;Policy2:Script4 \n\n"
                  "Note: Here no policy applied for Script3, that means if G_RBU_ALERT_SETTING" 
                  " is enable then it will show alerts otherwise not.\n\n");

  num_args = sscanf(buf, "%s %s %s", keyword, mode, policy_buf);

  imode = atoi(mode);
  NSDL2_RBU(NULL, NULL, "imode = [%d]", imode);
  if(imode != 0 && imode != 1)
  {
    NSTL1_OUT(NULL, NULL, "Error: mode [%d] should be 0 or 1 only.\n%s", imode, usages);
    return -1;
  }

  if(imode && (num_args != 3))
  {
    NSTL1_OUT(NULL, NULL, "Error: provided number of argument is [%d], with mode 1 policy_name:script_name is mandatory.\n%s", 
                           num_args, usages);
    return -1;
  }

  global_settings->rbu_alert_policy_mode = imode;

  NSDL2_RBU(NULL, NULL, "rbu_alert_policy_mode = [%d], policy_buf = [%s]", global_settings->rbu_alert_policy_mode, policy_buf);

  num_policy_list = get_tokens(policy_buf, policy_fields_buf, ";", 126);

  NSDL4_RBU(NULL, NULL, "num_policy_list = [%d], total_sess_entries = [%d]", num_policy_list, total_sess_entries);

  if(num_policy_list > 125)
  {
    NSTL1_OUT(NULL, NULL, "Error: Policy_list having equal or more than 125 tokens, it should be <= 125.\n%s", usages);
    NSDL2_RBU(NULL, NULL, "Error: Policy_list having equal or more than 125 tokens, it should be <= 125.\n%s", usages);
    return -1;
  }
  else if((num_policy_list == 0) && (imode == 1))
  {
    NSTL1_OUT(NULL, NULL, "Error: No policy is provided.\n%s", usages);
    NSDL4_RBU(NULL, NULL, "Error: No policy is provided.\n%s", usages);
    return -1;
  }
  int i = 0, sess_idx = 0;
  while(i < num_policy_list)   //loop running of ';'
  {
    //policy_fields_buf[0] = P1:S1,S2,S3 => policy_fields_colon[0] = P1, policy_fields_colon[1] = S1,S2,S3
    num_policy_colon = get_tokens(policy_fields_buf[i], policy_fields_colon, ":", 3);
   
    NSDL4_RBU(NULL, NULL, "num_policy_colon = [%d]", num_policy_colon); 
    if(num_policy_colon != 2)
    {
      NSTL1_OUT(NULL, NULL, "Error: Syntax of providing policy is wrong, policy_fields_buf = [%s]\n%s\n", policy_fields_buf[i], usages);
      return -1;
    }
    else
    {
      //policy_fields_colon[0] = P1, policy_fields_colon[1] = S1,S2,S3, policy_fields_comma[0] = S1 
      num_policy_fields_comma = get_tokens(policy_fields_colon[1], policy_fields_comma, ",", 125);

      NSDL4_RBU(NULL, NULL, "num_policy_fields_comma = [%d]", num_policy_fields_comma);
      j = 0;
      while(j < num_policy_fields_comma)
      {
        NSDL4_RBU(NULL, NULL, "policy_fields_comma[%d] = [%s]", j, policy_fields_comma[j]);

        sess_idx = 0;
        while(sess_idx < total_sess_entries)
        {
          NSDL4_RBU(NULL, NULL, "j = [%d], sess_idx = [%d], policy_fields_comma[%d] = [%s], gSessionTable[%d].sess_name = [%s]", 
                                 j, sess_idx, j, policy_fields_comma[j], sess_idx, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

          if(strcmp(policy_fields_comma[j], RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)))
          {
            sess_idx++;
            continue;
          }
          if(gSessionTable[sess_idx].rbu_alert_policy_ptr == NULL)
          {
            MY_MALLOC(gSessionTable[sess_idx].rbu_alert_policy_ptr, RBU_MAX_BUF_LENGTH, "rbu_alert_policy_ptr", -1);
            memset(gSessionTable[sess_idx].rbu_alert_policy_ptr, 0, 1025);
            per_script_policy[sess_idx].free_size = 1024;
            per_script_policy[sess_idx].written_size = 0;
            per_script_policy[sess_idx].malloc_size = 1024;

            NSDL4_RBU(NULL, NULL, "rbu_alert_policy_ptr is malloced to 1024 bytes, free_size = [%d], written_size = [%d], malloc_size = [%d]",
                                   per_script_policy[sess_idx].free_size, per_script_policy[sess_idx].written_size, 
                                   per_script_policy[sess_idx].malloc_size);
          }
          else if(per_script_policy[sess_idx].free_size <= strlen(policy_fields_colon[0]))
          {
            MY_REALLOC_EX(gSessionTable[sess_idx].rbu_alert_policy_ptr, 2*RBU_MAX_BUF_LENGTH, RBU_MAX_BUF_LENGTH, "gSessionTable[sess_idx].rbu_alert_policy_ptr", -1);
            NSDL4_RBU(NULL, NULL, "rbu_alert_policy_ptr is realloced to 1024 bytes,free_size = [%d], written_size = [%d], malloc_size = [%d]",
                                   per_script_policy[sess_idx].free_size, per_script_policy[sess_idx].written_size, 
                                   per_script_policy[sess_idx].malloc_size);
          }
  
          per_script_policy[sess_idx].written_size += strlen(policy_fields_colon[0]);
          per_script_policy[sess_idx].free_size -= strlen(policy_fields_colon[0]);
          NSDL4_RBU(NULL, NULL, "free_size = [%d], written_size = [%d], malloc_size = [%d]", per_script_policy[sess_idx].free_size, 
                                 per_script_policy[sess_idx].written_size, per_script_policy[sess_idx].malloc_size);

          if(strcmp(gSessionTable[sess_idx].rbu_alert_policy_ptr, ""))
            strcat(gSessionTable[sess_idx].rbu_alert_policy_ptr, ",");
          strcat(gSessionTable[sess_idx].rbu_alert_policy_ptr, policy_fields_colon[0]);
          NSDL4_RBU(NULL, NULL, "gSessionTable[%d].rbu_alert_policy_ptr = [%s]", sess_idx, gSessionTable[sess_idx].rbu_alert_policy_ptr);
          sess_idx++;
        }
        j++;
      }
    }
    i++;
  }
  NSDL2_RBU(NULL, NULL, "Method end, mode = [%d]", imode);
 
  return 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_ACCESS_LOG
 * Input     : G_RBU_ACCESS_LOG <Grp> <Mode> [<acc_log_code>]   
 *             Grp          : Any valid Group or ALL
 *             Mode         : mode for logging access_log
 *                            0 : Disable
 *                            1 : Enable, only Main URL data will be dumped (By Default)
 *                            2 : Enable, Inline data will also be dumped
 *             acc_log_code : Code for which access log will be logged
 *                            Example : 1xx;3xx
 *-----------------------------------------------------------------------------------------------------------------------------*/

int kw_set_g_rbu_access_log(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char usages[RBU_MAX_USAGE_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char code_list[RBU_MAX_KEYWORD_LENGTH] = "";
  char *code_fields[50 + 1] = {0};
 
  int code_num_tokens;

  int num_args = 0;
  int i;
 
  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  sprintf(usages, "Usages:\n"
                  "Syntax - G_RBU_ACCESS_LOG <Grp> <Mode> [<acc_log_code>]\n"
                  "Where:\n"
                  "group_name   - Any valid Group or ALL \n"
                  "               Example - G1, RBU etc \n"
                  "mode         - mode for logging access_log\n"
                  "               0 : Disable access log \n"
                  "               1 : Enable for Main URL only (By Default) \n"
		  "               2 : Enable for Main as well as inline URL. \n"
		  "acc_log_code - Code for which access log will be logged \n"
		  "               Example :1xx;2xx;3xx;4xx \n"
		  "               - If nothing provided in acc_log_code, then 4xx;5xx will be default.\n");
			

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, code_list);

  if(num_args < 3 && num_args > 4)
  {
    //NSDL3_RBU(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args, usages);
    sprintf(err_msg, "Error: provided number of argument (%d) is wrong.\n%s", num_args, usages);
    return -1;
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if((ns_is_numeric(mode) == 0) || (match_pattern(mode, "^[0-2]$") == 0))
  {
    sprintf(err_msg, "Error: Mode (%s) it is not numeric or mode is not valid.\n%s", mode, usages);
    return -1;
  }

  gset->rbu_gset.rbu_access_log_mode = atoi(mode);

  //TOKENIZE on the basis of ; 
  NSDL4_RBU(NULL, NULL, "code_list - %s", code_list);
  strcpy(gset->rbu_gset.rbu_acc_log_status, code_list);
  code_num_tokens = get_tokens(code_list, code_fields, ";",  10);
 
  NSDL4_RBU(NULL, NULL, "num_tokens = %d", code_num_tokens);
  if(code_num_tokens >= 10)
  {
    sprintf(err_msg, "Error: More then 10 status code at a time is not acceptable\n%s", usages);
    return -1;
  }
  else
  {
    if (code_num_tokens == 0)
    {
      strcpy(gset->rbu_gset.rbu_acc_log_status, "4xx;5xx");
      NSDL4_RBU(NULL, NULL, "gset->rbu_gset.rbu_acc_log_status = [%s]", gset->rbu_gset.rbu_acc_log_status);
    }
    else
    {
    
      for(i=0 ; i < code_num_tokens ; i++)
      {
         NSDL4_RBU(NULL, NULL, "Access Log :code_fields[%d] - %s",i, code_fields[i]);
        //TODO set flag bit according to 
    
        if((!(strcmp(code_fields[i], "1xx"))) || (!(strcmp(code_fields[i], "2xx"))) || (!(strcmp(code_fields[i], "3xx"))) ||
            (!(strcmp(code_fields[i], "4xx"))) || (!(strcmp(code_fields[i], "5xx"))))
        {
          NSDL4_RBU(NULL, NULL, "rbu_acc_log_status  = %s, code_fields[i] = %s", gset->rbu_gset.rbu_acc_log_status, code_fields[i]);
        }
        else
        {
          NSDL4_RBU(NULL, NULL, "Invalid Token Provided");
          sprintf(err_msg, "Error: Invalid Token Provided [%s]. Please provide valid status code \n%s", code_fields[i], usages);
          return -1;
        }
      }
    }
  }

  NSDL4_RBU(NULL, NULL, "rbu_acc_log_status  = %s", gset->rbu_gset.rbu_acc_log_status);
  return 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_EXT_TRACING
 * Input     : buf              : RBU_EXT_TRACING <Level>
 *           : Level            : level for logging extension trace log
 *                                Level can be 0(Disable), 1(By Default), 2, 3 and 4
 * Output    : On error    -1
 *             On success   0
 *-----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_rbu_ext_tracing(char *buf, char *err_msg)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char usages[RBU_MAX_USAGE_LENGTH] = "";
  char trace_val[RBU_MAX_KEYWORD_LENGTH] = "";
  char temp[RBU_MAX_USAGE_LENGTH] = "";

  int num_args = 0;
  int trace_level = 0;

  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  sprintf(usages, "Usages:\n"
                  "Syntax - RBU_EXT_TRACING <Level> \n"
                  "Where:\n"
                  "Level        - level for logging extension trace log\n"
                  "               Level can be 0(Disable), 1(By Default), 2, 3 and 4 \n");

  num_args = sscanf(buf, "%s %s %s", keyword, trace_val, temp);
  NSDL2_PARSING(NULL, NULL, "num_args = %d, keyword = [%s], level = [%s]", num_args, keyword, trace_val);

  if(num_args != 2)
  {
    sprintf(err_msg, "Error: Provided number of argument is wrong.\n%s", usages);
    return -1;
  }

  if(ns_is_numeric(trace_val) == 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: Value %s is not numeric .\n%s", trace_val, usages);
    return -1;
  }

  trace_level = atoi(trace_val);
  if(trace_level < 0 || trace_level > 4)
  {
    NSTL1_OUT(NULL, NULL, "Error: Value [%d] is not valid .\n%s", trace_level, usages);
    return -1;
  }

  global_settings->rbu_ext_tracing_mode = trace_level;
  NSDL2_RBU(NULL, NULL, "Method ends, global_settings->rbu_ext_tracing_mode = [%d]", global_settings->rbu_ext_tracing_mode);
  return 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword G_RBU_RM_PROF_SUB_DIR 
 * Input     : G_RBU_RM_PROF_SUB_DIR <Grp> <Mode> <profile relative path>   
 *             Grp          : Any valid Group or ALL
 *             Mode         : mode for logging access_log
 *                            0 : Disable
 *                            1 : Enable, delete directory on session start
 *                            2 : Enable, delete directory on page start
 *             profile relative path : Provide relative path of directory to be deleted from profile path.
                                   Example : If need to delete Service Worker directory which exist at path : 
 *                                           /home/cavisson/.rbu/.chrome/profiles/new_shikha_satish/Default/Service Worker
 *                                           then provide Default/Service Worker
 *-----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_rm_prof_sub_dir(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  char dir_path[RBU_MAX_KEYWORD_LENGTH] = "";
  char *path = "";
 
  int num_args = 0;
 
  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, dir_path);

  if(num_args < 3 && num_args > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_RM_PROF_SUB_DIR_USAGE, CAV_ERR_1011197, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_RM_PROF_SUB_DIR_USAGE, CAV_ERR_1011197, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-2]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RBU_RM_PROF_SUB_DIR_USAGE, CAV_ERR_1011197, CAV_ERR_MSG_3);
  }

  gset->rbu_gset.rbu_rm_unwntd_dir_mode = atoi(mode);

  NSDL4_RBU(NULL, NULL, "Directroy List - %s", dir_path);

  path = curl_unescape(dir_path, 0);
  strcpy(gset->rbu_gset.rbu_rm_unwntd_dir_list, path);
  curl_free(path);

  NSDL4_RBU(NULL, NULL, "End : Directroy List - %s", gset->rbu_gset.rbu_rm_unwntd_dir_list);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_DOMAIN_STAT <mode>
 *    
 * Input     : RBU_DOMAIN_STAT <Mode>   
 *             Mode         : mode for enabling/disabling dynamic domain stats
 *                        0 : Disable
 *                        1 : Enable
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_rbu_domain_stats(char *buf, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;

  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s", keyword, mode);
  if(num_args != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, RBU_DOMAIN_STAT_USAGE, CAV_ERR_1011202, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, RBU_DOMAIN_STAT_USAGE, CAV_ERR_1011202, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, RBU_DOMAIN_STAT_USAGE, CAV_ERR_1011202, CAV_ERR_MSG_3);
  }

  if(global_settings->protocol_enabled & RBU_API_USED)
  {
    global_settings->rbu_domain_stats_mode = atoi(mode);
  }
  else 
  {
    global_settings->rbu_domain_stats_mode = 0;
  }
  
  NSDL2_RBU(NULL, NULL, "RBU_DOMAIN_STAT | rbu_domain_stats_mode = %d", global_settings->rbu_domain_stats_mode);
  return 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will delete directory, empty non-empty both
 * Input     : We will provide absolute path of directory to be removed
 *            Example : If need to delete Service Worker directory, which is non-empty directory from path - 
 *                                           /home/cavisson/.rbu/.chrome/profiles/prof1/Default/Service Worker
 *                      We need to provide full path(above) to this function                     
 * Output    : Success - 0
 * checks recursively for directory
 *                                     Directory                   : Checks for next directory or file
 *                                      /     \
 *                               Directory   File                  : On file, unlink
 *                                   |
 *                                Directory                        : On dirctory, Checks for next directory or file
 *                                   |
 *                                  File                           : On file, unlink
 *
 * When all file from directory got unlinked(removed),  then directory will be removed using rmdir 
 *-----------------------------------------------------------------------------------------------------------------------------*/
int remove_directory(const char *path)
{
  NSDL4_RBU(NULL, NULL, "Method Called Path = %s", path);

  DIR *drctry = opendir(path);
  NSDL4_RBU(NULL, NULL, "drctry = %p, err = %s", drctry, nslb_strerror(errno));

  size_t path_len = strlen(path);
  int ret = -1;


  if(drctry)                                             //If directory stream pointer found
  {
    struct dirent *p;
    ret = 0;

    while (!ret && (p=readdir(drctry)))                  //readdir, checks for next dir in path
    {
      int ret_2 = -1;
      char *buf;
      size_t len;

      /* Skip the names "." and ".." as we don't want to recurse on them. */
      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
      {
        NSDL4_RBU(NULL, NULL, "Found Directory with . n ..");
        continue;
      }

      len = path_len + strlen(p->d_name) + 2;
      buf = malloc(len);

      if (buf)
      {
        struct stat statbuf;

        snprintf(buf, len, "%s/%s", path, p->d_name);    //making path for newly found directory on path

        if (!stat(buf, &statbuf))
        {
          if (S_ISDIR(statbuf.st_mode))                  //if directory, recursion starts
          {
            ret_2 = remove_directory(buf);
          }
          else
          {
            ret_2 = unlink(buf);                         //when file found, unlink the file
          }
        }
        free(buf);
      }
      ret = ret_2;
    }
    closedir(drctry);
  }

  if (!ret)
  {
    ret = rmdir(path);                                   //removes directory
  }

  return ret;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse G_RBU_WAITTIME_AFTER_ONLOAD, this keyword is used to provide time to wait for HAR file after onload
 *    
 * Input     : buf          : G_RBU_WAITTIME_AFTER_ONLOAD <group_name> <time (MilliSeconds)>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_waittime_after_onload(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char usages[RBU_MAX_USAGE_LENGTH];
  char waittime_after_onload[RBU_MAX_KEYWORD_LENGTH] = "0";
  char temp[RBU_MAX_KEYWORD_LENGTH] = {0};
  int waittime = 0;
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s], runtime_changes = %d", buf, runtime_changes);

  // Making usages 
  sprintf(usages, "\nUsages:\n"
                  "G_RBU_WAITTIME_AFTER_ONLOAD <group_name> <time (MilliSeconds)>\n"
                  "Where:\n"
                  "group_name            - group name can be ALL or any valid group\n"
                  "                      - Default is ALL\n"
                  "time                  - provide time to wait for HAR file after onload, in miliseconds\n"
                  "                      - 0    - disabled(Default)\n");

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, waittime_after_onload, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], wait_time = [%s]",
                              keyword, group_name, waittime_after_onload);

  if(num_args != 3)
  {
    NSDL3_RBU(NULL, NULL, "Error: provided number of argument is wrong. It should be 2.%s", usages);
    sprintf(err_msg, "Error: provided number of argument is wrong. It should be 2.%s", usages);
    return -1;
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(waittime_after_onload))  {
    waittime = atoi(waittime_after_onload);
  } else {
    NSDL2_RBU(NULL, NULL, "Error: time \'%s\' should be numeric.%s", waittime_after_onload, usages);
    sprintf(err_msg, "Error: time \'%s\' should be numeric.%s", waittime_after_onload, usages);
    return -1;
  }  

  gset->rbu_gset.har_threshold = waittime;

  NSDL2_PARSING(NULL, NULL, "End: waittime_after_onload = [%d] miliseconds.", gset->rbu_gset.har_threshold);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To set page/session status on failure.
 *    
 * Input     : VUser : to set and retrieve data  
 *             ptr   : pointer of ContinueOnPageErrorTableEntry_Shr, for continue of page error
 *             pg_execution_status : if '0' : Success 
 *                                     '-1' : On failure like, connection fail 
 *                                     '-2' : On abort  when retry is applied 
 * Called from ns_string_api.c ns_vuser_tasks.c 
 *------------------------------------------------------------------------------------------------------------*/
void nsi_rbu_handle_page_failure(VUser *vptr, ContinueOnPageErrorTableEntry_Shr *ptr, int pg_execution_status)
{
  NSDL2_RBU(vptr, NULL, "Method called, pg_execution_status = %d, num_retry_on_page_failure = %d, retry_count_on_abort = %d", 
                             pg_execution_status, runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure, 
                             vptr->retry_count_on_abort);

  //In All error cases we will dump access log once, from here. 
  //For all return types -1,-2,-3
  //form 4.1.14, will dump access log msg at error, in case of different values of Continue on page error, failure access log messages
  //i.e., pg_execution_status == -1, and in all those error cases where flow will not come here
  //ns_rbu_log_access_log(vptr, NULL, RBU_ACC_LOG_DUMP_LOG);

  //Handling T.O./Abort Case
  //Checking again for remaining retries, as a safety check
  if((pg_execution_status == -2) && 
       (runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure > vptr->retry_count_on_abort))
  {
    strncpy(script_execution_fail_msg, "Session is Aborted", MAX_SCRIPT_EXECUTION_LOG_LENGTH); 
    NSDL2_RBU(vptr, NULL, "At retry, for count = %d", vptr->retry_count_on_abort);

    //strncpy(vptr->httpData->rbu_resp_attr->access_log_msg, "session get Aborted, at count", RBU_MAX_ACC_LOG_LENGTH);
    ns_rbu_log_access_log(vptr, NULL, RBU_ACC_LOG_DUMP_LOG);

    nsi_retry_session(vptr, NS_SESSION_ABORT);

    NSDL2_RBU(vptr, NULL, "Retry Done.");
  }
  else
  {
    if(pg_execution_status == -1)
    {
      vptr->httpData->rbu_resp_attr->is_incomplete_har_file = 1; 
      vptr->page_status = NS_REQUEST_ERRMISC;
      vptr->sess_status = NS_REQUEST_ERRMISC;
      vptr->last_cptr->req_ok = NS_REQUEST_ERRMISC;

      rbu_fill_page_status(vptr, NULL);
      set_rbu_page_status_data_avgtime_data(vptr, NULL);

      if(ptr->continue_error_value == 0)
      {
        NSDL3_RBU(vptr, NULL, "HAR not made successfully and G_CONTINUE_ON_PAGE_ERROR is 0, hence session aborted.");
        vptr->next_pg_id = -1;   // vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION
        vptr->pg_think_time = 0; // Force think time to 0 so that it does not start think time
  
/*        if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
        {
          NSDL2_RBU(vptr, NULL, "Java Type Script:: Calling ns_rbu_handle_web_url_end ",
                           vptr->cur_page->page_name, vptr->cur_page->page_id);
          if(!is_rbu_web_url_end_done) 
          {        
            u_ns_ts_t now = get_ms_stamp();
            ns_rbu_handle_web_url_end(vptr, now);          
          }
        } */
      //(  else
      //  {
          //Add vptr->operation to VUT_RBU_WEB_URL_END and then switch to ns_rbu_handle_web_url_end()
          vut_add_task(vptr, VUT_RBU_WEB_URL_END);
      //    switch_to_nvm_ctx(vptr, "RBUWebUrlEnd");
      //  }
      } 
    }
  }  
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword RBU_MARK_MEASURE_MATRIX <mode>
 *    
 * Input     : RBU_MARK_MEASURE_MATRIX <Mode>   
 *             Mode         : mode for enabling/disabling dynamic domain stats
 *                        0 : Disable
 *                        1 : Enable
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 * Build_ver : 
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_rbu_mark_measure_matrix(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;

  NSDL2_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s", keyword, mode);
  if(num_args != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_MARK_MEASURE_MATRIX_USAGE, CAV_ERR_1011205, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_MARK_MEASURE_MATRIX_USAGE, CAV_ERR_1011205, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, RBU_MARK_MEASURE_MATRIX_USAGE, CAV_ERR_1011205, CAV_ERR_MSG_3);
  }

  if(global_settings->protocol_enabled & RBU_API_USED)
  {
    global_settings->rbu_enable_mark_measure = atoi(mode);
  }
  else 
  {
    global_settings->rbu_enable_mark_measure = 0;
  }
  
  NSDL2_RBU(NULL, NULL, "RBU_MARK_MEASURE_MATRIX | rbu_enable_mark_measure = %d", global_settings->rbu_enable_mark_measure);
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse keyword G_RBU_ENABLE_AUTO_SELECTOR, this keyword is used to control autoSelector feature of RBU.
               If cssPath for an element provided in script is not found then generate equivalent cssPath(selector) at runtime.
 *    
 * Input     : buf          : G_RBU_ENABLE_AUTO_SELECTOR <group_name> <mode>
 *             err_msg      : buffer to fill error message
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_enable_auto_selector(char *buf, GroupSettings *gset, char *err_msg)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char usages[RBU_MAX_USAGE_LENGTH];
  char mode[RBU_MAX_8BYTE_LENGTH] = "0";
  char temp[RBU_MAX_8BYTE_LENGTH] = {0};
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  //Making usages 
  sprintf(usages, "\nUsages:\n"
                  "G_RBU_ENABLE_AUTO_SELECTOR <group_name> <mode>\n"
                  "Where:\n"
                  "group_name            - Group name can be ALL or any valid group\n"
                  "                      - Default is ALL\n"
                  "mode                  - Control autoSelector feature of RBU\n"
                  "                      - 0    - Disabled(Default)\n"
                  "                      - 1/2  - Enabled\n"
                  "                             - 1    - Search from Last index value\n"
                  "                             - 2    - Search from First index value\n");

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s]",
                              keyword, group_name, mode);

  if(num_args != 3)
  {
    NSDL3_RBU(NULL, NULL, "Error: provided number of argument is wrong. It should be 2.%s", usages);
    sprintf(err_msg, "Error: provided number of argument is wrong. It should be 2.%s", usages);
    return -1;
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if((ns_is_numeric(mode) == 0) || (match_pattern(mode, "^[0-2]$") == 0))
  {
    sprintf(err_msg, "Error: Mode (%s) is not valid.\n%s", mode, usages);
    return -1;
  }

  gset->rbu_gset.rbu_auto_selector_mode = atoi(mode);

  NSDL2_PARSING(NULL, NULL, "End: auto_selector_mode = [%d].", gset->rbu_gset.rbu_auto_selector_mode);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse keyword G_RBU_CAPTURE_PERFORMANCE_TRACE, this keyword is used to enable/disable capturing of performance stats.
 *    
 * Input     : buf          : G_RBU_CAPTURE_PERFORMANCE_TRACE <Group> <Mode> <Timeout> <Enable Memory> <Enable Screenshot> 
 *             err_msg      : buffer to fill error message
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_capture_performance_trace(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char mode[RBU_MAX_8BYTE_LENGTH] = {0};
  char timeout[RBU_MAX_8BYTE_LENGTH] = {0};
  char memory[RBU_MAX_8BYTE_LENGTH] = {0};
  char screenshot[RBU_MAX_8BYTE_LENGTH] = {0};
  char duration_level[RBU_MAX_8BYTE_LENGTH] = {0};
  char temp[RBU_MAX_8BYTE_LENGTH] = {0};
  int num_args = 0;

  int imode = 0;
  int itimeout =  RBU_DEFAULT_PERFORMANCE_TRACE_TIMEOUT;
  int imemory = 1;
  int iscreenshot = 0;
  int iduration_level = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  num_args = sscanf(buf, "%s %s %s %s %s %s %s %s", keyword, group, mode, timeout, memory, screenshot, duration_level, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], timeout = [%s], memory = [%s], screenshot = [%s]"
                            ", duration_level = [%s]",
                            keyword, group, mode, timeout, memory, screenshot, duration_level);

  if(num_args < 3 || num_args > 7)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_1);
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group, 0);

  switch(num_args)
  {
    case 7:
            if(ns_is_numeric(duration_level) == 0)
            {
              NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_2);
            }
            else if(match_pattern(duration_level, "^[0-1]$") == 0)
            {
              NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_3);
            }
            else
              iduration_level = atoi(duration_level);
    case 6:
            if(ns_is_numeric(screenshot) == 0)
            {
 	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_2);
            }
            else if(match_pattern(screenshot, "^[0-1]$") == 0)
            {
 	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_3);
            }
            else
              iscreenshot = atoi(screenshot);
    case 5:
            if(ns_is_numeric(memory) == 0)
            {
	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_2);
            }
            else if(match_pattern(memory, "^[0-1]$") == 0)
            {
	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_3);
            }
            else
              imemory = atoi(memory);
    case 4:
            if(ns_is_numeric(timeout) == 0)
            {
	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_2);
            }
            else
            {
              itimeout = atoi(timeout);
              if(itimeout < 10000 || itimeout > 120000)
              {
	        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011199, "");
              }
            }
    case 3:
            if(ns_is_numeric(mode) == 0)
            {
	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_2);
            }
            else if(match_pattern(mode, "^[0-1]$") == 0)
            {
	      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CAPTURE_PERFORMANCE_TRACE_USAGE, CAV_ERR_1011189, CAV_ERR_MSG_3);
            }
            else
              imode = atoi(mode);
  }
  
  gset->rbu_gset.performance_trace_mode = imode;
  gset->rbu_gset.performance_trace_timeout = itimeout; 
  gset->rbu_gset.performance_trace_memory_flag = imemory;
  gset->rbu_gset.performance_trace_screenshot_flag = iscreenshot;
  gset->rbu_gset.performance_trace_duration_level = iduration_level;

  if(gset->rbu_gset.performance_trace_mode == 1)
    g_rbu_create_performance_trace_dir = PERFORMANCE_TRACE_DIR_FLAG;

  NSDL2_PARSING(NULL, NULL, "End: mode = [%d], timeout = [%d], memory = [%d], screenshot = [%d], duration_level = [%d].", 
                 gset->rbu_gset.performance_trace_mode, gset->rbu_gset.performance_trace_timeout, gset->rbu_gset.performance_trace_memory_flag,
                 gset->rbu_gset.performance_trace_screenshot_flag, gset->rbu_gset.performance_trace_duration_level);

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse keyword G_RBU_SELECTOR_MAPPING_PROFILE, this keyword is used for deprecated selector mapping.
 *    
 * Input     : buf          : G_RBU_SELECTOR_MAPPING_PROFILE <Group> <Mode> <Profile Path>  
 *             err_msg      : buffer to fill error message
 *    
 * Output    : On error    -1
 *             On success   0
 *    
 *------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_selector_mapping_profile(char *buf, GroupSettings *gset, char *err_msg)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char group[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char usages[RBU_MAX_USAGE_LENGTH];
  char mode[RBU_MAX_8BYTE_LENGTH] = {0};
  char profile_path[RBU_MAX_PATH_LENGTH] = {0};
  char temp[RBU_MAX_USAGE_LENGTH] = {0};
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  sprintf(usages, "\nUsages:\n"
                  "G_RBU_SELECTOR_MAPPING_PROFILE <Group> <Mode> <Profile Path>\n"
                  "Where:\n"
                  "Group                 - Group name can be ALL or any valid group\n"
                  "                        - Default is ALL\n"
                  "Mode                  - It may be 0 or 1\n"
                  "                        - 0 - Disabled(Default)\n"
                  "                        - 1 - Enabled\n"
                  "Profile Path          - Absolute Path of Seletor Mapping Profile File\n"
                  "                        - Default Path : $NS_WDIR/webapps/sys/smp/\n"
                  "                        - File Format  : JSON\n"
                  "                        - Example      : /home/cavisson/work/webapps/sys/smp/profile1.json\n");

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group, mode, profile_path, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], profile_path = [%s]",
                              keyword, group, mode, profile_path);

  if(num_args != 4)
  {
    NSDL3_RBU(NULL, NULL, "Error: valid number of argument is not provided.%s", usages);
    sprintf(err_msg, "Error: valid number of argument is not provided.%s", usages);
    return -1;
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group, 0);

  if((ns_is_numeric(mode) == 0) || (match_pattern(mode, "^[0-1]$") == 0))
  {
    sprintf(err_msg, "Error: Mode (%s) is not valid.\n%s", mode, usages);
    return -1;
  }
  else
    gset->rbu_gset.selector_mapping_mode = atoi(mode);

  if(gset->rbu_gset.selector_mapping_mode == 1)
  {
    if(strcasecmp(profile_path, "NA") == 0 || (profile_path[0] == '"' && profile_path[1] == '"'))
    {
      NSDL2_RBU(NULL, NULL, "No profile path is provided in 'G_RBU_SELECTOR_MAPPING_PROFILE'");
      sprintf(err_msg, "Error: No profile path is provided in 'G_RBU_SELECTOR_MAPPING_PROFILE'.%s\n", usages);
      return -1;
    }
    else if(profile_path[0] != '/')
    {
      snprintf(temp, RBU_MAX_USAGE_LENGTH, "%s/webapps/sys/smp/%s", g_ns_wdir, profile_path);
      strncpy(profile_path, temp, RBU_MAX_PATH_LENGTH);
    }

    if(access(profile_path, F_OK) != 0 )
    {
      NSDL2_RBU(NULL, NULL, "Deprecated selector mapping profile file '%s' does not exits.", profile_path);
      sprintf(err_msg, "Error: Deprecated selector mapping profile file '%s' does not exits.", profile_path);
      return -1;
    }
    MY_MALLOC_AND_MEMSET(gset->rbu_gset.selector_mapping_file, strlen(profile_path) + 1, "selector mapping file path", -1);
    strcpy(gset->rbu_gset.selector_mapping_file, profile_path);
  }
                  
  NSDL2_PARSING(NULL, NULL, "End: mode = [%d], profile_path = [%s].", 
                 gset->rbu_gset.selector_mapping_mode, gset->rbu_gset.selector_mapping_file);
  return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------ 
 * Purpose   : To parse keyword G_RBU_THROTTLING_SETTING, this keyword is used for Netwtork/Bandwidth throttling in RBU(Lighthouse).
 *    
 * Input     : buf        : G_RBU_THROTTLING_SETTING <Group> <Mode> <Download Throughput> <Upload Throughput> <Request Latency>
 *             err_msg    : buffer to fill error message
 *    
 * Output    : On error    -1
 *             On success   0
 *
 * Bug Id    : 62805 - Mobile||Need Bandwidth simulation in Lighthouse Feature   
 *----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_throttling_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char group[RBU_MAX_NAME_LENGTH] = "";
  char mode[RBU_MAX_8BYTE_LENGTH] = "";
  char down_throughput[RBU_MAX_NAME_LENGTH] = "";
  char up_throughput[RBU_MAX_NAME_LENGTH] = "";
  char latency[RBU_MAX_NAME_LENGTH] = "";
  char temp[RBU_MAX_NAME_LENGTH] = "";
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  num_args = sscanf(buf, "%s %s %s %s %s %s %s", keyword, group, mode, down_throughput, up_throughput, latency, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], down_throughput = [%s], up_throughput = [%s], latency = [%s]",
                             keyword, group, mode, down_throughput, up_throughput, latency);
  if(num_args != 3 && num_args != 6)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_1);
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group, 0);

  if(gset->rbu_gset.enable_rbu && gset->rbu_gset.lighthouse_mode != 1)
  {
    NSDL2_PARSING(NULL, NULL, "Warning:Lighthouse reporting is not enabled for group '%s'. Hence Ignoring Network Throttling Configuration.",
                               group);
  }

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_2);
  }
  else if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_3); 	
  }
  else
    gset->rbu_gset.ntwrk_throttling_mode = atoi(mode);

  if(num_args == 6)
  {
    if(nslb_atoi(down_throughput, &(gset->rbu_gset.ntwrk_down_tp)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_2);
    }
    if(nslb_atoi(up_throughput, &(gset->rbu_gset.ntwrk_up_tp)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_2);
    }
    if(nslb_atoi(latency, &(gset->rbu_gset.ntwrk_latency)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_THROTTLING_SETTING_USAGE, CAV_ERR_1011180, CAV_ERR_MSG_2);
    }
  }
                  
  NSDL2_PARSING(NULL, NULL, "End: mode = [%d], down_throughput = [%d], up_throughput = [%d], latency = [%d].", 
                 gset->rbu_gset.ntwrk_throttling_mode, gset->rbu_gset.ntwrk_down_tp, gset->rbu_gset.ntwrk_up_tp, gset->rbu_gset.ntwrk_latency);
  return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------- 
 * Purpose: Used for providing cpu throtting in lighthouse (RBU)
   G_RBU_CPU_THROTTLING <group> <mode> <slowdown multiplier>
	
 * Here - 
	group - ALL or any specific group. 
	mode - 0 - disable (default) and 1 (enable)
	slowdown multiplier - any number greater than 1 

 * Output    : On error    -1
 *             On success   0
 *----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_cpu_throttling_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char group[RBU_MAX_NAME_LENGTH] = "";
  char mode[RBU_MAX_8BYTE_LENGTH] = "";
  char cpuSlowDownMultiplier[RBU_MAX_NAME_LENGTH] = "";
  char temp[RBU_MAX_NAME_LENGTH] = "";
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group, mode, cpuSlowDownMultiplier, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s], cpuSlowDownMultiplier= [%s]",
                             keyword, group, mode, cpuSlowDownMultiplier);
  if(num_args != 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CPU_THROTTLING_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_1);
  }  

  /* Validate group Name */
  val_sgrp_name(buf, group, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CPU_THROTTLING_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_2);
  }
  else if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CPU_THROTTLING_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_3); 	
  }
  else
    gset->rbu_gset.cpu_throttling_mode = atoi(mode);

  if(gset->rbu_gset.cpu_throttling_mode == 1){
    if(nslb_atoi(cpuSlowDownMultiplier, &(gset->rbu_gset.cpuSlowDownMultiplier)) < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CPU_THROTTLING_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_2);
    }
    if(gset->rbu_gset.cpuSlowDownMultiplier <= 1){
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RBU_CPU_THROTTLING_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_3);
    }
  }
  NSDL2_PARSING(NULL, NULL, "End: mode = [%d], cpuSlowDownMultiplier= [%d]", 
                 gset->rbu_gset.cpu_throttling_mode, gset->rbu_gset.cpuSlowDownMultiplier);
  return 0;

}

/*----------------------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To parse keyword G_RBU_LIGHTHOUSE_SETTING, this keyword is used for Device Simulation in Lighthouse.
 *    
 * Input     : buf        : G_RBU_LIGHTHOUSE_SETTING <Group> <Device Mode>
 *             err_msg    : buffer to fill error message
 *    
 * Output    : On error    -1
 *             On success   0
 *
 * Bug Id    : 85317 - Need lighthouse in desktop mode also.
 *----------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_lighthouse_setting(char *buf, GroupSettings *gset, char *err_msg)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char group[RBU_MAX_NAME_LENGTH] = "";
  char usages[RBU_MAX_USAGE_LENGTH] = "";
  char device_mode[RBU_MAX_8BYTE_LENGTH] = "";
  char temp[RBU_MAX_NAME_LENGTH] = "";
  int num_args = 0;

  NSDL1_PARSING(NULL, NULL, "buf = [%s] = %d", buf);

  sprintf(usages, "\nUsages:\n"
                  "G_RBU_LIGHTHOUSE_SETTING <Group> <Device Mode>\n"
                  "Where:\n"
                  "Group                 - Group name can be ALL or any valid group\n"
                  "                        - Default is ALL\n"
                  "Device Mode           - It may be 0 or 1\n"
                  "                        - 0 - Mobile(Default)\n"
                  "                        - 1 - Desktop\n");

  num_args = sscanf(buf, "%s %s %s %s", keyword, group, device_mode, temp);
  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], device mode = [%s]",
                             keyword, group, device_mode);
  if(num_args != 3)
  {
    NSDL3_RBU(NULL, NULL, "Error: valid number of argument is not provided.%s", usages);
    sprintf(err_msg, "Error: valid number of argument is not provided (%s). %s", buf, usages);
    return -1;
  }

  /* Validate group Name */
  val_sgrp_name(buf, group, 0);

  if(gset->rbu_gset.enable_rbu && gset->rbu_gset.lighthouse_mode != 1)
  {
    NSDL2_PARSING(NULL, NULL, "Warning:Lighthouse reporting is not enabled for group '%s'. Hence Lighthouse Setting will be ignored.",
                               group);
  }

  if((ns_is_numeric(device_mode) == 0) || (match_pattern(device_mode, "^[0-1]$") == 0))
  {
    sprintf(err_msg, "Error: Mode (%s) is not valid.\n%s", device_mode, usages);
    return -1;
  }
  else
    gset->rbu_gset.lh_device_mode = atoi(device_mode);

  NSDL2_PARSING(NULL, NULL, "End: device mode = [%d]", 
                 gset->rbu_gset.lh_device_mode);
  return 0;
}

/*-----------------------------------------------------------------------------------------------------------------------------------
 * Purpose	: To parse keyword G_RBU_RELOAD_HAR, this keyword is used to disable HAR creation.
 *    
 * Input        : buf        : G_RBU_RELOAD_HAR <Group> <Mode>
 *                err_msg    : buffer to fill error message
 *
 * Output	: On error   : -1
 *		  On success : 0
 *
 * Bug Id       : 72608
 *
 *----------------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_reload_har(char *buf, GroupSettings *gset, char *err_msg){
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char usages[RBU_MAX_USAGE_LENGTH] = "";
  char mode[RBU_MAX_8BYTE_LENGTH] = "0";
  char tmp[RBU_MAX_8BYTE_LENGTH] = "0";
  int num_args = 0;
  
  NSDL1_PARSING(NULL, NULL, "buf = [%s]", buf);

  sprintf(usages, "\nUsages:\n"
                  "G_RBU_RELOAD_HAR <Group> <Mode>\n"
                  "Where:\n"
                  "Group                 - Group name can be ALL or any valid group\n"
                  "                        - Default is ALL\n"
                  "Mode                  - It may be 0 or 1\n"
                  "                        - 0 - Disabled\n"
                  "                        - 1 - Enabled (default)\n"
                 );

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, mode, tmp);

  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], mode = [%s]",
                             keyword, group_name, mode);
  if (num_args != 3){
    //invalid arg count
    sprintf(err_msg, "Error: valid number of argument is not provided (%s). %s", buf, usages);
    NSDL3_RBU(NULL, NULL, "Error: valid number of argument is not provided.%s", usages);
    return -1;
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
 
  if (match_pattern(mode, "^(0|1)$") == 0){
    sprintf(err_msg, "Error: Mode (%s) is not valid.\n%s", mode, usages);
    return -1;
  }

  gset->rbu_gset.reload_har = atoi(mode);

  NSDL2_PARSING(NULL, NULL, "End: reload_har mode for group [%s] =  [%d]", group_name, gset->rbu_gset.reload_har);
  return 0;
}

/*-----------------------------------------------------------------------------------------------------------------------------------
 * Purpose     : To parse keyword G_RBU_WAIT_UNTIL, this keyword is used to retry click action on failure.
 *    
 * Input        : buf        : G_RBU_WAIT_UNTIL <group> <timeout>
 *                err_msg    : buffer to fill error message
 *
 * Output      : On error   : -1
 *               On success : 0
 *
 * Bug Id       : 87108
 *
 *----------------------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_rbu_wait_until(char *buf, GroupSettings *gset){
  char keyword[RBU_MAX_KEYWORD_LENGTH] = "";
  char group_name[RBU_MAX_KEYWORD_LENGTH] = "ALL";
  char usages[RBU_MAX_USAGE_LENGTH] = "";
  char timeout[RBU_MAX_8BYTE_LENGTH] = "0";
  char tmp[RBU_MAX_8BYTE_LENGTH] = "0";
  int num_args = 0;
  
  NSDL1_PARSING(NULL, NULL, "buf = [%s]", buf);

  sprintf(usages, "\nUsages:\n"
                  "G_RBU_WAIT_UNTIL <Group> <Timeout>\n"
                  "Where:\n"
                  "Group                 - Group name can be ALL or any valid group\n"
                  "                        - Default is ALL\n"
                  "Timeout               - It is time for wait in seconds\n"
                  "                        - 0 - Disabled (default)\n"
                 );

  num_args = sscanf(buf, "%s %s %s %s", keyword, group_name, timeout, tmp);

  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], timeout = [%s]",
                             keyword, group_name, timeout);
  if (num_args != 3){
    //invalid arg count
    NSDL3_RBU(NULL, NULL, "Error: valid number of argument is not provided.%s", usages);
    NS_EXIT(-1, "Error: valid number of argument is not provided (%s). %s", buf, usages);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
 
  if(ns_is_numeric(timeout) == 0){
    NS_EXIT(-1, "Error: Timeout (%s) is not numeric.\n%s", timeout, usages);
  }

  gset->rbu_gset.wait_until_timeout = atoi(timeout);

  NSDL2_PARSING(NULL, NULL, "End: Wait Until Timeout for group [%s] =  [%d]", group_name, gset->rbu_gset.wait_until_timeout);
  return 0;
}
