// This is c file for Chrome Browser
#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include<strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#include "nslb_util.h"
#include "nslb_json_parser.h"
#include "ns_cache_include.h"
#include "ns_vuser_thread.h"
#include "ns_url_req.h"
#include "ns_page_dump.h"
#include "ns_script_parse.h"
#include "ns_http_script_parse.h"
#include "ns_vuser_tasks.h"
#include "ns_error_codes.h"
#include "ns_http_process_resp.h" 
#include "ns_vuser_ctx.h" 
#include "ns_alloc.h"
#include "ns_rbu.h"
#include "nslb_json_parser.h"
#include "ns_rbu_api.h"
#include "url.h"
#include "ns_click_script.h"
#include "ns_network_cache_reporting.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_child_thread_util.h"
#include "ns_trace_level.h"

/*{"opcode": "1001", "params": {"url": "http://www.google.com/", "postHeader": [{"name": "firstHeader", "value": "value"}], "header": [{"name": "firstHeader", "value": "value"}], "page": "home_page","newtab" : true, "captureHar" :{ "enable": true, "path": "download/prof1/"}, "resolution" : {"height" : 100, "width" : 200}, "snapshot" : {"enable" : true, "file": "/screen_shot/<ss>"}}}
*/
/* Messages Parts-
 * Msg (1) - {"opcode": "1001", "params": {"newtab" : true,
 * Msg (2) - "url": "http://www.google.com/", 
 * Msg (3) - "resolution": {"height" : 100, "width" : 200}, 
 * Msg (4) - "snapshot": {"enable" : true, "file": "/screen_shot/<ss.jpeg>"}
 * Msg (5) - 
 */
static int ns_rbu_chrome_make_message(VUser *vptr)
{
  char user_agent_str[RBU_USER_AGENT_LENGTH + 1];
  char *cmd_buf_ptr = NULL;                            /* This pointer is responsible to fill chrome command buffer */
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  int clear_cache = 0; //Default is false
  int timeout_for_next_req = 0;   //setting default valur of varibale 'timeout_for_next_req' and 'G_RBU_PAGE_LOADED_TIMEOUT'
  int phase_interval_for_page_load = 0;

  #ifdef NS_DEBUG_ON
    connection *cptr = vptr->last_cptr;
  #endif
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, NULL, "Method Called, enable_ns_chrome = %d, g_ns_chrome_binpath = [%s]",
                       global_settings->enable_ns_chrome, g_ns_chrome_binpath[0]?g_ns_chrome_binpath:NULL);

  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  /* Move all files from har log dir before executing new Page so that if due to some reason
     har file left then not distrub Renaming of others.
     Here using same buffer as we are using in firefox command i.e. rbu_resp_attr->firefox_cmd_buf so that stack size not increases*/
  
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "mv %s/%s/*.har %s/logs/%s/rbu_logs/harp_files >/dev/null 2>&1",
                           rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir,
                           g_ns_wdir, global_settings->tr_or_partition);
  NSDL2_RBU(vptr, NULL, "Running command to move old Har file in archive: cmd_buf = [%s]", rbu_resp_attr->firefox_cmd_buf);

  nslb_system2(rbu_resp_attr->firefox_cmd_buf);

  //clear buffer so that there is no junk data in memory
  //memset(rbu_resp_attr->firefox_cmd_buf, 0, RBU_MAX_CMD_LENGTH);
  memset(rbu_resp_attr->firefox_cmd_buf, 0, 2*cmd_write_idx + 1);
  cmd_write_idx = 0;

  /* Making chrome message */

  /* Msg: start - {"opcode": "1001", "params": {"newtab" : true, */
  NSDL2_RBU(vptr, NULL, "Chrome Message: (Start): opcode and param");
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                              "{\"opcode\": \"1001\", "
                              " \"params\": "
                                   "{"
                                     "\"newtab\" : true, "
                                     "\"captureHar\": {\"enable\": true, \"path\": \"\", \"threshold\": %d}, ",
                                                                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.har_threshold);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
  /* Capture Har */
  /*cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"captureHar\" : {\"enable\": true, \"path\": \"chrome/logs/%s/\"}, ", 
                            rbu_resp_attr->profile); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);*/
  
  /* Page Name */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"page\" : \"%s\", ", vptr->cur_page->page_name); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Test Id */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"testRun\" : \"%d\", ", testidx); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: add fully qualified urls  - "url": "http://www.google.com/", */
  ns_rbu_make_full_qal_url(vptr);

  NSDL2_RBU(vptr, NULL, "Chrome Message: Passing URL - %s", vptr->httpData->rbu_resp_attr->url);
  //url part.
  cmd_write_idx = sprintf(cmd_buf_ptr, "\"url\": \"%s\", ", vptr->httpData->rbu_resp_attr->url) ;
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /*Msg: (Authentication popup credential) - "authCredential": "{username:\"cavisson\", password:\"Cav@123\"}", */
  if(vptr->first_page_url->proto.http.rbu_param.auth_username && vptr->first_page_url->proto.http.rbu_param.auth_password)
  {
    char *username = vptr->first_page_url->proto.http.rbu_param.auth_username;
    char *password = vptr->first_page_url->proto.http.rbu_param.auth_password;

    NSDL4_RBU(vptr, NULL, "auth_username = %s, auth_password = %s", username, password);

    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"authCredential\" : {");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    int len = strlen(username);
    if(username[0] == '{' && username[len - 1] == '}')
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"username\": \"%s\", ", ns_eval_string(username));
    else
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"username\": \"%s\", ", username);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    len = strlen(password); 
    if(password[0] == '{' && password[len - 1] == '}')
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"password\": \"%s\"}, ", ns_eval_string(password));
    else
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"password\": \"%s\"}, ", password);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  
  /*Msg: (Proxy Authentication) - "proxyAuthCredential": "{username:\"cavisson\", password:\"Cav@123\"}", */
  if(global_settings->proxy_flag)  //Proxy is used
  {
    ProxyServerTable_Shr *proxy_ptr =  runprof_table_shr_mem[vptr->group_num].proxy_ptr;
    if(proxy_ptr && proxy_ptr->username && proxy_ptr->password)
    {
      NSDL4_RBU(vptr, NULL, "proxy auth_username = %s, proxy auth_password = %s", proxy_ptr->username, proxy_ptr->password);
     
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"proxyAuthCredential\" : {"
                                                                  "\"username\": \"%s\", \"password\": \"%s\"}, ",
                                                                     proxy_ptr->username, proxy_ptr->password);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
  }

  /* Msg: (Screen Resolution) - "resolution": {"height" : 100, "width" : 200},  */
  NSDL2_RBU(vptr, NULL, "global_settings->rbu_screen_sim_mode = [%d]", global_settings->rbu_screen_sim_mode);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim == 0)  // If screen resolution off 
  {
    NSDL2_RBU(vptr, NULL, "Setting chrome screen resolution from user");
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"resolution\" : {\"width\" : %d, \"height\" : %d}, ", 0, 0); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    NSDL2_RBU(vptr, NULL, "Setting chrome default screen resolution");
  }
  else/* if(global_settings->rbu_screen_sim_mode == 1) */
  {
    NSDL2_RBU(vptr, NULL, "Setting chrome screen resolution from user");
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"resolution\" : {\"width\" : %d, \"height\" : %d}, ", 
                                          vptr->screen_size->width, vptr->screen_size->height);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /* Msg: (Capture Clips) - "captureClips" : { "enable" : true, "clipDir": "/home/<user_name>/.rbu/.chrome/logs/profiles/clips", "frequency" : 2000 , "quality" : 100 } */

  NSDL2_RBU(vptr, NULL, "Setting chrome captureClips ");
  
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip)
  {
    if(rbu_resp_attr->page_capture_clip_file == NULL)
      MY_MALLOC(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH + 1, "vptr->httpData->rbu_resp_attr->page_capture_clip_file", 1);

    snprintf(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH, "%s/%s/clips/video_clip_%hd_%u_%u_%d_0_%d_%d_%d_0_",
                       rbu_resp_attr->har_log_path, rbu_resp_attr->profile, child_idx, vptr->user_index, vptr->sess_inst,
                       vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    NSDL2_RBU(vptr, NULL, "Chrome Message: capture_clip_file_name = %s", rbu_resp_attr->page_capture_clip_file);

    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                             "\"captureClips\" : {\"enable\" : %d, \"clipDir\" : \"%s\", \"frequency\" : %d, \"quality\" : %d, \"domload_th\": %d, \"onload_th\": %d}, ",
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip, 
                             (rbu_resp_attr->page_capture_clip_file + strlen(rbu_resp_attr->har_log_path) + strlen(rbu_resp_attr->profile) + 2),
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_frequency,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_quality, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_domload_th,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_onload_th);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else
  {
    //send false if captureClips is not enable.
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                             "\"captureClips\": {\"enable\" : false, \"clipDir\": \"\" , \"frequency\" : 100, \"quality\" : 100, \"domload_th\": 0, \"onload_th\": 0, \"pageload_th\": 0}, ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
 
  /* Msg: (Screen shot) -  "snapshot": {"enable" : true, "file": "~/.rbu/.chrome/logs/prof/screen_shot/"}, */
  NSDL2_RBU(vptr, NULL, "trace_on_fail = %d, trace_level = %d", runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail,
                         runprof_table_shr_mem[vptr->group_num].gset.trace_level);
  //enable_screen_shot
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot &&
       NS_IF_PAGE_DUMP_ENABLE && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
  {
    if(rbu_resp_attr->page_screen_shot_file == NULL)
      MY_MALLOC(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH + 1,
                              "vptr->httpData->rbu_resp_attr->page_screen_shot_file", 1);

    //MM: TODO- what was the exact path of chrome extension
    // Path- /home/<user>/Downloads/chrome/logs/<profile_name>/.TRxx/scree_shot/
    //TODO: check if we can use hidden directories in relative path of snapshot.
    snprintf(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH,
                    "%s/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0",
                     rbu_resp_attr->har_log_path, rbu_resp_attr->profile, child_idx, vptr->user_index, vptr->sess_inst,
                     vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    NSDL2_RBU(vptr, NULL, "Chrome Message: screen_shot_file_name = %s", rbu_resp_attr->page_screen_shot_file);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"snapshot\": {\"enable\" : true, \"file\": \"%s\"}, "
                                        , (rbu_resp_attr->page_screen_shot_file + strlen(rbu_resp_attr->har_log_path) +
                                          strlen(rbu_resp_attr->profile) + 2));
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
    // Since in function ns_rbu_wait_for_har_file screen shots file needs with absolute path Hence update this  
  }
  else 
  {
    //send false if screenshot is not enable.
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"snapshot\": {\"enable\" : false, \"file\": \"\"}, ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  
  //G_RBU_PAGE_LOADED_TIMEOUT > 500ms, So WaitForNextReq > 500ms
  //By Default, G_RBU_PAGE_LOADED_TIMEOUT = 5000ms
  /* Msg: Add pageLoadedTimeout : "pageLoadedTimeout": 1200 */
  timeout_for_next_req  = ((vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req > 500)?
                              vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req :
                              runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.page_loaded_timeout);

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"pageLoadedTimeout\": \"%d\", ",
                                         timeout_for_next_req);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  NSDL2_RBU(vptr, NULL, "Setting pageLoadedTimeout = %d ", timeout_for_next_req);

  /*Bug-73558:G_RBU_PAGE_LOADED_TIMEOUT(phase_interval) > 2000ms , So PhaseInterval > 2000ms
   *By Default, G_RBU_PAGE_LOADED_TIMEOUT - phase_interval = 4000ms
   *Msg: Add pageLoadPhaseInterval : "pageLoadPhaseInterval": 4000 */
  phase_interval_for_page_load = ((vptr->first_page_url->proto.http.rbu_param.phase_interval_for_page_load > 2000)?
                                     vptr->first_page_url->proto.http.rbu_param.phase_interval_for_page_load :
                                     runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.pg_load_phase_interval);

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"pageLoadPhaseInterval\": \"%d\", ",
                                         phase_interval_for_page_load);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  NSDL2_RBU(NULL, NULL, "Setting pageLoadPhaseInterval = %d ", phase_interval_for_page_load);

  /* Msg: Add timeout : "timeout": 60000 */
  unsigned short har_timeout;
  HAR_TIMEOUT
  int timeout = (1000 * har_timeout) - 5000;

  int browser_version = 0;
  nslb_atoi(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn, &browser_version);
  if(browser_version >= 78)
    timeout = (1000 * har_timeout) - 10000;

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"timeout\": \"%d\", ", timeout);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  NSDL2_RBU(vptr, NULL, "Setting timeout page_load_wait_time = %d", timeout);

  /*Bug 59603: Performance Trace Dump
   *Message Format::
   *"performanceTrace": {"enable": true, "timeout": 10000, "memoryTrace": 1, "screenshot": 0, "filename": "performance_trace/P_index+NSAppliance-work-cavisson-TR5627-1-1+kohls.com+0_0_0_1_0_0_0_1_0.json"}
   */

  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_mode == 1) || 
       (vptr->first_page_url->proto.http.rbu_param.performance_trace_mode == 1))
  {
    char host[RBU_MAX_BUF_LENGTH] = {0};
    char *ptr = NULL;
    rbu_resp_attr->performance_trace_flag = 1;

    NSDL2_RBU(vptr, NULL, "Setting performanceTrace");

    if(rbu_resp_attr->performance_trace_filename == NULL)
      MY_MALLOC(rbu_resp_attr->performance_trace_filename, RBU_MAX_TRACE_FILE_LENGTH + 1,
                              "vptr->httpData->rbu_resp_attr->performance_trace_filename", 1);

    PerHostSvrTableEntry_Shr* svr_entry = get_svr_entry(vptr, vptr->cur_page->first_eurl->index.svr_ptr);
    snprintf(host, RBU_MAX_BUF_LENGTH, "%s", svr_entry->server_name);

    ptr = host;
    while(*ptr)
    {
      if(*ptr == ':')
      {
        *ptr = '-';
        break;
      }
      ptr++;
    }

    /*Performance Trace Dump file name format:
     *P_<page_name>+<Chrome-Profile-Name>+<Host>+<child_idx>_<user_index>_<sess_inst>_<page_instance>_0_<group_num>_<sess_id>_<page_id>_0.json
     */

    snprintf(rbu_resp_attr->performance_trace_filename, RBU_MAX_TRACE_FILE_LENGTH, "%s+%s+%hd_%u_%u_%d_0_%d_%d_%d_0.json",
               rbu_resp_attr->profile, host, child_idx, vptr->user_index, vptr->sess_inst,
               vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr)); 

    if(vptr->first_page_url->proto.http.rbu_param.performance_trace_mode == 1)
    {
      rbu_resp_attr->performance_trace_timeout = vptr->first_page_url->proto.http.rbu_param.performance_trace_timeout;
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"performanceTrace\": {\"enable\": true, \"timeout\": %d, \"memoryTrace\": %d, "
                                                                   "\"screenshot\": %d, \"durationLevel\": %d, "
                                                                   "\"filename\": \"performance_trace/%s\"}, ",
                                                   vptr->first_page_url->proto.http.rbu_param.performance_trace_timeout,
                                                   vptr->first_page_url->proto.http.rbu_param.performance_trace_memory_flag,
                                                   vptr->first_page_url->proto.http.rbu_param.performance_trace_screenshot_flag,
                                                   vptr->first_page_url->proto.http.rbu_param.performance_trace_duration_level,
                                                   rbu_resp_attr->performance_trace_filename); 
    }
    else
    {
      rbu_resp_attr->performance_trace_timeout = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_timeout;
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"performanceTrace\": {\"enable\": true, \"timeout\": %d, \"memoryTrace\": %d, "
                                                                   "\"screenshot\": %d, \"durationLevel\": %d, "
                                                                   "\"filename\": \"performance_trace/%s\"}, ",
                                                   runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_timeout,
                                                   runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_memory_flag,
                                                   runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_screenshot_flag,
                                                   runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_duration_level,
                                                   rbu_resp_attr->performance_trace_filename);
    }
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else
    rbu_resp_attr->performance_trace_flag = 0;

  /* Msg: Add Headers - User-Agent, Cookie, Transaction etc...
      Note - All Header shuld be in one place because Header is an array in json message 
      "headers": [{"name": "FirstHeader", "value": "FirstHeaderValue"}, {"name": "secondHeader", "value": "secondHeaderValue"}, ...],*/
  
  // Start of Headers -
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"headers\": ["); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* User Agent - "User-Agent": "<string>", */
  //enable_ns_chrome 0 -> don't set chrome set its default
  //enable_ns_chrome 1 -> take from User profile
  //enable_ns_chrome 2 -> take from user provided string
  int header_count = 0;  //This variable has been used in macros, don't remove this.
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  NSDL2_RBU(vptr, NULL, "global_settings->rbu_user_agent = [%d]", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 0)
  {
    NSDL2_RBU(vptr, NULL, "Setting firefox default user agent");
  }
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 1)
  {
    NSDL2_RBU(vptr, NULL, "Setting user agent from user profile");
    if (!(disable_headers & NS_UA_HEADER))
    {
      if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
          && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
      {
        NSDL2_RBU(vptr, NULL, "Chrome Message: User-Agent = %s", vptr->browser->UA);
        strcpy(user_agent_str, vptr->browser->UA);
        ADD_USER_AGENT_IN_CHROME_MSG
      }
      else
      {
        NSDL2_RBU(vptr, NULL, "Chrome Message: User-Agent = %s", vptr->httpData->ua_handler_ptr->ua_string);
        strcpy(user_agent_str, vptr->httpData->ua_handler_ptr->ua_string);
        ADD_USER_AGENT_IN_CHROME_MSG
      }
    }
  }
  //else if(global_settings->rbu_user_agent == 2)
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 2)
  {
    //NSDL2_RBU(vptr, NULL, "Setting user agent provide by user, User-Agent = %s", g_rbu_user_agent_str);
    NSDL2_RBU(vptr, NULL, "Setting user agent provide by user, User-Agent = %s", 
                           runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
    ADD_HEADERS_IN_CHROME_MSG("User-Agent", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name); 
    //ADD_HEADERS_IN_CHROME_MSG("User-Agent", g_rbu_user_agent_str); 
  }

  //MM: TODO- what is in network_cache_stats_header_buf_ptr????
  /* Msg: Set Network Cache Stats header -  - */
  // Pragma: akamai-x-cache-on, akamai-x-check-cacheable, akamai-x-cache-remote-on
  if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    NSDL3_RBU(vptr, cptr, "Network Cache Stats Headers = [%s], len =[%d]", network_cache_stats_header_buf_ptr, 
                                network_cache_stats_header_buf_len);
    network_cache_stats_header_buf_ptr[network_cache_stats_header_buf_len - 2] = '\0';
    ADD_HEADERS_IN_CHROME_MSG("Pragma", network_cache_stats_header_buf_ptr + strlen("Pragma: "));
  }  

  /* Msg: Set CavTxName header - */ 
  // ADD CavTxName or customized header, if G_SEND_NS_TX_HTTP_HEADER is on
  // CavTxName: IndexPage
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode)
  {
    NSDL3_RBU(vptr, cptr, "G_SEND_NS_TX_HTTP_HEADER is enabled. Going to prepare transaction header");
    TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
    if(node_ptr == NULL) 
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                  __FILE__, (char*)__FUNCTION__, "No transaction is running for this user, so netcache transaction header will not be send.");
    } 
    else 
    {
      char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code);
      // Adding HTTP Header name is the name of HTTP header. Default value of http header name is "CavTxName".
      //cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"%s",
      //                                      runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name);
      //RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
      int len = strlen(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name) - 2;
      strncpy(user_agent_str, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name, len);
      user_agent_str[len] = 0;

      // Adding Transaction name, http req will have this header with the last tx started before this URL was send.
      if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable[0] == '\0')
      {
        NSDL3_RBU(vptr, cptr, "Tx var is null, tx name = %s", tx_name);
        ADD_HEADERS_IN_CHROME_MSG(user_agent_str, tx_name);
        // We are sending head node of link list, that is the last transaction started before ns_web_url
        //cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "%s\", ", tx_table_shr_mem[node_ptr->hash_code].name);
        //RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
      }
      else
      {
        NSDL3_RBU(vptr, cptr, "tx var is not null...");
        ADD_HEADERS_IN_CHROME_MSG(user_agent_str, ns_eval_string(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable));
        //cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "%s\", ", 
          //                       ns_eval_string(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable));
        //RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
      }
    }
  } // End of CavTxName hdr

  /* Added Custom headers */
  char *req_headers_buf = NULL;
  char *headers_fields[128];
  char *sep_fields[128];
  int req_headers_size = 0;
  int num_headers = 0;
  int count_fields = 0, i;
  //Send Header in Main as Well Inline url  
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 2)
  {
    NSDL2_RBU(vptr, NULL, "Make Req Headers from segment table");
    if(ns_rbu_make_req_headers(vptr, &req_headers_buf, &req_headers_size) < 0)
      return -1;
    NSDL2_RBU(vptr, NULL, "req_headers_buf = [%s], req_headers_size = %d", req_headers_buf, req_headers_size);

    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_headers_buf);
    NSDL2_RBU(vptr, NULL, "After clear, req_headers_buf = [%s]", req_headers_buf);

    num_headers = get_tokens_ex2(req_headers_buf, headers_fields, "\r\n", 128);
    NSDL2_RBU(vptr, NULL, "num_headers = %d", num_headers);

    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "Chrome Args: headers_fields[%d] = [%s]", i, headers_fields[i]);
      count_fields = get_tokens_ex2(headers_fields[i], sep_fields, ":", 128);    
      if(count_fields == 2)
        ADD_HEADERS_IN_CHROME_MSG(sep_fields[0], sep_fields[1]);     
    }
  } /* End Custom headers */

  //Add NVSM header which will go in main and inline urls both. 
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode != 0) && (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_send_mode == 1))
  {
    ADD_ND_FPI_HEADER_IN_CHROME_MSG();
    NSDL2_RBU(vptr, NULL,"rbu_gset.rbu_nd_fpi_mode = %d, Added cav_post_hdr CavNDFPInstance: sucessfully",
                                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode);
  }

  /* End of Headers: Headers end - }], */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "], "); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
 
  header_count = 0; //Resetting Header count for postHeaders
 
  /* Msg: Add post Header */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"postHeaders\" : [ "); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  //We have to send header in main url only
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 1)
  {
    NSDL2_RBU(vptr, NULL, "Make Req Headers from segment table");
    if(ns_rbu_make_req_headers(vptr, &req_headers_buf, &req_headers_size) < 0)
      return -1;
    NSDL2_RBU(vptr, NULL, "req_headers_buf = [%s], req_headers_size = %d", req_headers_buf, req_headers_size);

    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_headers_buf);
    NSDL2_RBU(vptr, NULL, "After clear, req_headers_buf = [%s]", req_headers_buf);

    num_headers = get_tokens_ex2(req_headers_buf, headers_fields, "\r\n", 128);
    NSDL2_RBU(vptr, NULL, "num_headers = %d", num_headers);

    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "Chrome Args: headers_fields[%d] = [%s]", i, headers_fields[i]);
      count_fields = get_tokens_ex2(headers_fields[i], sep_fields, ":", 128);    
      if(count_fields == 2)
        ADD_HEADERS_IN_CHROME_MSG(sep_fields[0], sep_fields[1]);     
    }
  }
  // send NVSM header to main url only.
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode != 0) && (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_send_mode == 0))
  {
    ADD_ND_FPI_HEADER_IN_CHROME_MSG();
    NSDL2_RBU(vptr, NULL,"rbu_gset.rbu_nd_fpi_mode = %d, Added cav_post_hdr CavNDFPInstance: sucessfully",
                                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode);
  }
  /* End of the PostHeaders   */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "], ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

 /* Msg: Cache Related setting */
  NSDL3_RBU(vptr, cptr, "Adding enable cache attribute: enable_cache = %d", 
                                   runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_cache);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"disableCache\" : %s, ", 
                                        !runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_cache?"true":"false");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
 
  //Resolved Bug 26385 - RBU- G_RBU_CACHE_SETTING mode 2 is not getting cleared the cache on each page. 
  int cache_val = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clear_cache_on_start;
  NSDL3_RBU(vptr, cptr, "Adding clear cache attribute: clear_cache_on_start = %d, first_pg = %d", cache_val, rbu_resp_attr->first_pg);

  if((rbu_resp_attr->first_pg && (cache_val == 1))|| (cache_val == 2))
    clear_cache = cache_val;
  else
    clear_cache = 0;

  NSDL3_RBU(vptr, cptr, "clear_cache = %d", clear_cache);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"clearCache\" : %s, ", (clear_cache != 0)?"true":"false");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /*Clear Cache on Click events*/
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"clearCacheOnClick\" : %s, ", ((cache_val == 3)?"true":"false" ));
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  NSDL3_RBU(vptr, cptr, "clear_cache_mode = %d", cache_val);

  /*Cookie Clear on start of session*/
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clear_cookie_on_start && rbu_resp_attr->first_pg ){
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"clearCookie\" : %s, ", 
                              runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clear_cookie_on_start?"true":"false");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  
  /* TTI */

  //TTI Profile name i.e., home1 of "PrimaryContentProfile=home1"
  NSDL3_RBU(vptr, cptr, "Adding tti profile name = %s", vptr->first_page_url->proto.http.rbu_param.tti_prof); 
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"primaryContentProfName\" : \"%s\", ",
                                                         (vptr->first_page_url->proto.http.rbu_param.tti_prof ? 
                                                          vptr->first_page_url->proto.http.rbu_param.tti_prof : "false")); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
  NSDL3_RBU(vptr, cptr, "Adding primary content profile = %s", vptr->first_page_url->proto.http.rbu_param.primary_content_profile); 
  if(vptr->first_page_url->proto.http.rbu_param.primary_content_profile != NULL)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"primaryContentList\" : %s, "
                                                                   , vptr->first_page_url->proto.http.rbu_param.primary_content_profile); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  //Added for scroll Event
  char *scroll_enable = "false";

  long long scroll_page_x = vptr->first_page_url->proto.http.rbu_param.scroll_page_x;
  long long scroll_page_y = vptr->first_page_url->proto.http.rbu_param.scroll_page_y;

  if((scroll_page_x != 0) && (scroll_page_y != 0))
    scroll_enable = "true";
  
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"scrollPage\": {\"enable\": \"%s\", \"x\": \"%lld\", \"y\": \"%lld\"},",
                           scroll_enable, scroll_page_x, scroll_page_y);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); 

  NSDL3_RBU(vptr, cptr, "scroll_page_enable= [%s], scroll_page_x =[%lld], scroll_page_y = [%lld] ", scroll_enable, scroll_page_x, scroll_page_y);

  //render enable if on then value should be >=100
  //if rbu_settings and capture_clip both is enable then capture_clip is on high priority
  NSDL3_RBU(vptr, cptr, "Adding render enable = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
  if ((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings >= 100) 
      && !(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip)) 
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"renderEnable\" : {\"value\": %d}" , 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }  
  else 
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"renderEnable\" : {\"value\": 0}" );
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  //set rbu_enable_tti
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"rbuEnableTTI\" : {\"value\" : %d}", 
                                                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.tti_mode);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //set get_marks_measures
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"get_marks_measures\" : {\"value\" : %d}", 
                                                          global_settings->rbu_enable_mark_measure);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /*Msg: Add rbu_har_setting */
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"rbuHARSetting\" :{\"enable\": %d, \"compression\": %d, "
                             "\"request\": %d, \"response\": %d, \"jsProcAndUri\": %d}",
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_compression,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_request, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_response,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_js_proc_tm_and_dt_uri);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /*Msg : Domain List which will be ignored or not dump in HAR file */
  NSDL3_RBU(vptr, cptr, "Adding Domain Name = %s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list);
  if( strlen(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list) != 0)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"domainIgnoreList\" : \"%s\""
                                                             , runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
   /*Msg : URL List which will be blocked before going over Network */
  NSDL3_RBU(vptr, cptr, "Adding URLs to block = %s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list);
  if( runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list != NULL)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"blockUrlList\" : \"%s\""
                                                             , runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
   
  /* Msg: Set Cookie header - "Cookie": "<string>", */
  char *req_cookies_buf = NULL;
  int req_cookies_size = 0;

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"cookies\" : [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  char *ptr = NULL;
 
  NSDL2_RBU(vptr, NULL, "Make Req Cookie form cookie table");
  ns_rbu_make_req_cookies(vptr, &req_cookies_buf, &req_cookies_size);
  NSDL2_RBU(vptr, NULL, "req_cookies_buf = [%s], req_cookies_size = %d", req_cookies_buf, req_cookies_size);
  if(req_cookies_buf != NULL)
  {
    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_cookies_buf);
    ptr = strstr(req_cookies_buf, "Cookie:"); 
    if(ptr != NULL) 
    {
      ptr = req_cookies_buf + strlen("Cookie:");
      BRU_CLEAR_WHITE_SPACE(ptr);
    }
    NSDL2_RBU(vptr, NULL, "After clear , req_cookies_buf = [%s]", ptr);
    
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"flag\": 1, \"pairs\": [ ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    //make_msg_for_cookies_in_chrome(vptr, ptr, &cmd_buf_ptr, &cmd_write_idx, &free_cmd_buf_len);
    char *cookies_fields[128];
    char *sep_cookie_fields[3];
    int num_cookies = 0;
    int i, cookie_count = 0;

    num_cookies = get_tokens_ex2(ptr, cookies_fields, ";", 128);
    NSDL2_RBU(NULL, NULL, "num_cookies = %d", num_cookies);

    if(num_cookies == 0)
    {
      NSDL2_RBU(NULL, NULL, "num_cookies = %d", num_cookies);
      get_tokens_ex2(ptr, sep_cookie_fields, "=", 2);
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]);
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]);
      ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], NULL);
    }
    else
    {
      for(i = 0; i < num_cookies; i++)
      {
        NSDL2_RBU(NULL, NULL, "cookies_fields[%d] = [%s]", i, cookies_fields[i]);
        get_tokens_ex2(cookies_fields[i], sep_cookie_fields, "=", 2);
        BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]);
        BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]);
        ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], NULL);
      }
    }

    // ']}' is for "pairs and flag" end in cookies msg 
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]}");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); 
  }
  // ']' is for "cookies" end in cookies msg
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //Sending extension tracing mode i.e RBU_EXT_TRACING
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"ext_tracing\" : %d", global_settings->rbu_ext_tracing_mode);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: End -  */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "}}\n");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  // Null terminate cmd buf
  *cmd_buf_ptr = '\0';

  // Free memory malloced for body and header
  FREE_AND_MAKE_NULL(req_cookies_buf, "req_cookies_buf", -1);

  NSDL3_RBU(vptr, cptr, "Chrome MSG: %s", rbu_resp_attr->firefox_cmd_buf);

  return 0;
}

/*
 * Bug-62245: NetTest | Deprecated selector mapping to new selector
 * {"opcode": "1000", "params": {"selectorMapping" : [{"url": "http://127.0.0.1:81/tours/index.html",  "oldSelector": "HTML/BODY[1]/TABLE[1]/TBODY[1]/TR[2]/TD[1]/TABLE[1]/TBODY[1]/TR[1]/TD[1]/FORM[1]/CENTER[1]/TABLE[1]/TBODY[1]/TR[2]/TD/INPUT", "newSelector": "HTML/BODY[1]/TABLE[1]/TBODY[1]/TR[2]/TD[1]/TABLE[1]/TBODY[1]/TR[1]/TD[1]/FORM[1]/CENTER[1]/TABLE[1]/TBODY[1]/TR[2]/TD[1]/INPUT[1]"},{...}, {...}]}
*/
static int ns_rbu_chrome_make_message_1000(VUser *vptr)
{
  char *cmd_buf_ptr = NULL;                            /* This pointer is responsible to fill chrome command buffer */
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  FILE *fp = NULL;
  char temp[RBU_MAX_BUF_LENGTH + 1] = "";
  char *pos = NULL;
  struct stat stat_st;
  long file_size = 0;

  #ifdef NS_DEBUG_ON
    connection *cptr = vptr->last_cptr;
  #endif
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  /*Finding the size of data file*/
  if(stat(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.selector_mapping_file, &stat_st) == -1)
  {
     fprintf(stderr, "Selector mappig file does not exists. Exiting.");
     NSDL1_RBU(NULL, NULL, "Selector mapping file does not exists");
     return -1;
  }
  else
  {
    if(stat_st.st_size == 0)
    {
      fprintf(stderr, "Selector mapping file is of zero size. Exiting.");
      NSDL1_RBU(NULL, NULL, "Selector mapping file is of zero size. Exiting.");
      return -1;
    }
  }
  file_size = stat_st.st_size;
  if (RBU_MAX_CMD_LENGTH < file_size)
  {
    NSDL3_RBU(NULL, NULL, "malloced_sized = %d, file_size = %ld", RBU_MAX_CMD_LENGTH, file_size);
    MY_REALLOC_EX(rbu_resp_attr->firefox_cmd_buf, file_size + RBU_MAX_64BYTE_LENGTH, RBU_MAX_CMD_LENGTH, "firefox_cmd_buf", -1);
    free_cmd_buf_len = file_size + RBU_MAX_64BYTE_LENGTH;
  }

  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  /* Making chrome message 1000*/

  /* Msg: start - {"opcode": "1000", "params": {"selectorMapping" : [{},{}],} */
  NSDL2_RBU(vptr, cptr, "Chrome Message: (Start): opcode and param");
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                              "{\"opcode\": \"1000\", "
                                "\"params\": ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
 
  fp = fopen(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.selector_mapping_file, "r");
  if(fp)
  {
    while(fgets(temp, RBU_MAX_BUF_LENGTH, fp) != NULL )
    {
      pos = strpbrk(temp, "\r\n");
      if(*pos)
        *pos = '\0';
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "%s", temp);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
  }
  else
  {
    fprintf(stderr, "Failed to open Selector mapping file.");
    NSDL1_RBU(NULL, NULL, "Failed to open Selector mapping file.");
    return -1;    
  }
  fclose(fp);

  /* Msg: End - */ 
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "}\n");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //Null terminate cmd buf
  *cmd_buf_ptr = '\0';

  NSDL3_RBU(vptr, cptr, "Chrome MSG 1000: %s", rbu_resp_attr->firefox_cmd_buf);

  return 0;
}

static int ns_rbu_chrome_make_lh_message(VUser *vptr)
{
  char user_agent_str[RBU_USER_AGENT_LENGTH + 1] = "";
  char *cmd_buf_ptr = NULL;                            /* This pointer is responsible to fill chrome command buffer */
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;

  unsigned short har_timeout;
  HAR_TIMEOUT
  unsigned short timeout = (1000 * har_timeout) - 5000;

  #ifdef NS_DEBUG_ON
    connection *cptr = vptr->last_cptr;
  #endif
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, cptr, "Method Called, vptr = %p", vptr);

  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  snprintf(rbu_resp_attr->rbu_light_house->lighthouse_filename, RBU_MAX_NAME_LENGTH,
                        "P_%s+lighthouse+%hd_%u_%u_%d_0_%d_%d_%d_0",
                        vptr->cur_page->page_name, child_idx, vptr->user_index, vptr->sess_inst,
                        vptr->page_instance, vptr->group_num, vptr->sess_ptr->sess_norm_id, vptr->cur_page->page_norm_id);
  NSDL1_RBU(vptr, cptr, "lighthouse_filename = %s", rbu_resp_attr->rbu_light_house->lighthouse_filename);

  // Handling for User agent. 
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  NSDL2_RBU(vptr, NULL, "global_settings->rbu_user_agent = [%d]", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 0)
  {
    NSDL2_RBU(vptr, NULL, "Setting firefox default user agent");
  }
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 1)
  {
    NSDL2_RBU(vptr, NULL, "Setting user agent from user profile");
    if (!(disable_headers & NS_UA_HEADER))
    {
      if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
          && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
      {
        NSDL2_RBU(vptr, NULL, "Chrome Message: User-Agent = %s", vptr->browser->UA);
        strcpy(user_agent_str, vptr->browser->UA);
      }
      else
      {
        NSDL2_RBU(vptr, NULL, "Chrome Message: User-Agent = %s", vptr->httpData->ua_handler_ptr->ua_string);
        strcpy(user_agent_str, vptr->httpData->ua_handler_ptr->ua_string);
      }
    }
  }
  //else if(global_settings->rbu_user_agent == 2)
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 2)
  {
    //NSDL2_RBU(vptr, NULL, "Setting user agent provide by user, User-Agent = %s", g_rbu_user_agent_str);
    NSDL2_RBU(vptr, NULL, "Setting user agent provide by user, User-Agent = %s", 
                           runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
    strcpy(user_agent_str, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
  }


  char *req_headers_buf = NULL;
  char *headers_fields[128];
  char *sep_fields[128];
  int req_headers_size = 0;
  int num_headers = 0;
  int count_fields = 0, i;
  char custom_headers[4096] = "";
  //Send Header in Main as Well Inline url  
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 2)
  {
    NSDL2_RBU(vptr, NULL, "Make Req Headers from segment table");
    if(ns_rbu_make_req_headers(vptr, &req_headers_buf, &req_headers_size) < 0)
      return -1;
    NSDL2_RBU(vptr, NULL, "req_headers_buf = [%s], req_headers_size = %d", req_headers_buf, req_headers_size);

    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_headers_buf);
    NSDL2_RBU(vptr, NULL, "After clear, req_headers_buf = [%s]", req_headers_buf);

    num_headers = get_tokens_ex2(req_headers_buf, headers_fields, "\r\n", 128);
    NSDL2_RBU(vptr, NULL, "num_headers = %d", num_headers);

   
    int headerBufOffset = 0;
    int estimateLen = 0; 
    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "Chrome Args: headers_fields[%d] = [%s]", i, headers_fields[i]);
      count_fields = get_tokens_ex2(headers_fields[i], sep_fields, ":", 128);    
      if(count_fields == 2)
      {
        // Check if sufficient space left. 
        // format - {"name": "$name", "value": "$value"} 
        estimateLen = strlen(sep_fields[0]) + strlen(sep_fields[1]) + 26;
        if (headerBufOffset + estimateLen + 10 < sizeof(custom_headers)) {
          if (custom_headers[0])
            headerBufOffset += sprintf(&custom_headers[headerBufOffset], ",{\"name\": \"%s\", \"value\": \"%s\"}", sep_fields[0], sep_fields[1]);
          else 
            headerBufOffset += sprintf(&custom_headers[headerBufOffset], "{\"name\": \"%s\", \"value\": \"%s\"}", sep_fields[0], sep_fields[1]);
        }  else {
          break;
        }
      }
    }
  } /* End Custom headers */

  cmd_write_idx = 0;

  /* Making chrome message */

  NSDL2_RBU(vptr, cptr, "Chrome Message: (Start): opcode and param");
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                              "{\"opcode\": \"2001\", "
                              " \"params\": "
                                   "{"
                                     "\"newtab\" : true, \"lightHouse\": {\"enable\": true, \"filename\": \"%s\"}, ",
                                                 rbu_resp_attr->rbu_light_house->lighthouse_filename);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
  /* Page Name */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"page\" : \"%s\", ", vptr->cur_page->page_name); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Test Id */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"testRun\" : \"%d\", ", testidx); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: add fully qualified urls  - "url": "http://www.google.com/", */
  ns_rbu_make_full_qal_url(vptr);

  NSDL2_RBU(vptr, cptr, "Chrome Message: Passing URL - %s", vptr->httpData->rbu_resp_attr->url);
  //url part.
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"url\": \"%s\", ", vptr->httpData->rbu_resp_attr->url) ;
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /*Msg: Add networkThrottling: {"enable": true, "downloadThroughput": "1474", "uploadThroughput": "675", "requestLatency": "562"}*/
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.ntwrk_throttling_mode == 1)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"networkThrottling\": {\"enable\": true, "
                                                            "\"downloadThroughput\": %d, \"uploadThroughput\": %d, \"requestLatency\": %d}, ",
                                                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.ntwrk_down_tp,
                                                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.ntwrk_up_tp,
                                                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.ntwrk_latency);
  }
  else
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"networkThrottling\": {\"enable\": false}, ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.cpu_throttling_mode == 1){
     cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"cpuThrottling\": {\"enable\": true, \"cpuSlowDownMultiplier\": %d},", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.cpuSlowDownMultiplier);
  }
  else
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"cpuThrottling\": {\"enable\": false}, ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //Bug 85317
  /*Msg: Add device: "device": "desktop"*/
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.lh_device_mode == 1)
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"device\": \"desktop\", ");
  else
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"device\": \"mobile\", ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    
  /* Msg: Add timeout : "timeout": 60000 */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"timeout\": \"%hu\"", timeout);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  NSDL2_RBU(vptr, cptr, "Setting timeout page_load_wait_time = %hu", timeout);

  /*Msg: User Agent: "userAgent": $userAgent*/
  if (user_agent_str[0]) {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"userAgent\": \"%s\"", user_agent_str);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    NSDL2_RBU(vptr, cptr, "Setting userAgent = %s", user_agent_str);
  }

  /*Msg: Headers: "headers": $headerArray*/
  if (custom_headers[0]) {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"headers\": [%s]", custom_headers);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    NSDL2_RBU(vptr, cptr, "Setting headers - %s", custom_headers);
  }

  /* Msg: End -  */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "}}\n");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //at last \n is not needed
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu == 2)
    cmd_buf_ptr -= 1;

  // Null terminate cmd buf
  *cmd_buf_ptr = '\0';

  NSDL3_RBU(vptr, cptr, "Chrome MSG: %s", rbu_resp_attr->firefox_cmd_buf);

  return 0;
}

int get_debug_level(VUser *vptr)
{
  int d_level = runprof_table_shr_mem[vptr->group_num].gset.debug; 
  
  if(d_level == 0x000000FF) return 1;
  else if(d_level == 0x0000FFFF) return 2;
  else if(d_level == 0X00FFFFFF) return 3;
  else if(d_level == 0xFFFFFFFF) return 4;
 
  return 0;
}

// chromium-browser --user-data-dir="~/home/...." --controller_name=<work> --test_run=7877
static int ns_rbu_chrome_start_browser(VUser *vptr, int mode)
{ 
  char wan_args[512 + 1] = "";
  char wan_access[512 + 1];
  char read_buf[1024 + 1] = "0";
  char controller_name[1024  + 1] = "";
  char file_name[512 + 1] = "";
  char *controller_name_ptr = NULL;
  char *cmd_buf_ptr = NULL;                            /* This pointer is responsilbe to fill firefox command buffer */
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  typedef void (*sighandler_t)(int);
  PerGrpStaticHostTable *per_grp_static_host = NULL;
  int static_host_flag = 0;

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = %p, rbu_resp_attr = %p, enable_ns_chrome = %d", vptr, rbu_resp_attr, 
                         global_settings->enable_ns_chrome);

  if(rbu_resp_attr == NULL)
  {
    NSTL1(vptr, NULL, "Error: ns_rbu_chrome_start_browser() - rbu_resp_attr is NULL\n");
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }

  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
    strcpy(controller_name, controller_name_ptr + 1);

  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  //Setting ns_logs_file_path, and rbu_logs_file_path 
  RBU_NS_LOGS_PATH

  /* Chrome out file */
  snprintf(file_name, RBU_MAX_FILE_LENGTH, "%s/logs/%s/ns_rbu_chrome_out_prof_%s.log", 
                                            g_ns_wdir, rbu_logs_file_path, rbu_resp_attr->profile);
  NSDL2_RBU(vptr, NULL, "Chrome_out_file = %s", file_name);

  /* Making Chrome command */
  /* For Bug Id : 18086 - Adding '--ignore-certificate-errors' to chrome cammand line, to ingore certificate errors. */
  if(global_settings->enable_ns_chrome) /* NS Chrome */
  {
    #ifndef CAV_MAIN
    cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH,
                                         "nohup %s/chromium-browser --user-data-dir=%s/.rbu/.chrome/profiles/%s --display=:%d "
                                         "--cav_testrun=%d  --controller_name=%s --debug_level=%d --ns_wdir=%s --cav_partition=%lld ",
                                         g_ns_chrome_binpath, g_home_env, rbu_resp_attr->profile, rbu_resp_attr->vnc_display,
                                         testidx, controller_name, get_debug_level(vptr), g_ns_wdir, vptr->partition_idx); 
    #else
    cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH,
                                         "nohup %s/chromium-browser --user-data-dir=%s/.rbu/.chrome/profiles/%s --display=:%d "
                                         "--cav_testrun=%d --controller_name=%s --debug_level=%d --ns_wdir=%s --cav_partition=%s ",
                                         g_ns_chrome_binpath, g_home_env, rbu_resp_attr->profile, rbu_resp_attr->vnc_display,
                                         testidx, controller_name, get_debug_level(vptr), g_ns_wdir, vptr->sess_ptr->sess_name);
    #endif
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    if(global_settings->wan_env)
    {
      get_wan_args_for_browser(vptr, wan_args, wan_access);
      NSDL2_RBU(vptr, NULL, "wan_args = %s", wan_args);

      cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--cav_wan_setting=%s ", wan_args);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    //Bug 76346 - Support of G_STATIC_HOST keyword in RBU for Chrome browser
    //Group specific settings
    if(get_is_static_host_shm_created() == 0)
      per_grp_static_host = &runProfTable[vptr->group_num].gset.per_grp_static_host_settings;
    else
      per_grp_static_host = &runprof_table_shr_mem[vptr->group_num].gset.per_grp_static_host_settings;

    for(int i = 0; i < per_grp_static_host->total_static_host_entries; i++)
    {
      if(static_host_flag == 0)
      {
        cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--host-resolver-rules=\"MAP %s %s\"",
                                 per_grp_static_host->static_host_table[i].host_name, per_grp_static_host->static_host_table[i].ip);
        static_host_flag = 1;
      }
      else
      {
        cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, ",\"MAP %s %s\"",
                                 per_grp_static_host->static_host_table[i].host_name, per_grp_static_host->static_host_table[i].ip);
      }
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    //ALL Group settings
    per_grp_static_host = &group_default_settings->per_grp_static_host_settings;
    for(int i = 0; i < per_grp_static_host->total_static_host_entries; i++)
    {
      if(static_host_flag == 0)
      {
        cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--host-resolver-rules=\"MAP %s %s\"",
                                 per_grp_static_host->static_host_table[i].host_name, per_grp_static_host->static_host_table[i].ip);
        static_host_flag = 1;
      }
      else
      {
        cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, ",\"MAP %s %s\"",
                                 per_grp_static_host->static_host_table[i].host_name, per_grp_static_host->static_host_table[i].ip);
      }
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
    if(static_host_flag)
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, " ");
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(global_settings->proxy_flag)  //Proxy is used 
    {
      ProxyServerTable_Shr *proxy_ptr =  runprof_table_shr_mem[vptr->group_num].proxy_ptr;
      if(proxy_ptr)
      {
        //Format: --proxy-server="http=proxy1:80;https=proxy2:1080"
        if(proxy_ptr->http_proxy_server != NULL && proxy_ptr->https_proxy_server != NULL)  //HTTP & HTTPS
        {
          cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--proxy-server=\"http=%s:%d;https=%s:%d\" ",
                                    proxy_ptr->http_proxy_server, proxy_ptr->http_port,
                                    proxy_ptr->https_proxy_server, proxy_ptr->https_port);
      
          RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
        }
        else if(proxy_ptr->http_proxy_server != NULL) //HTTP
        {
          cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--proxy-server=\"http=%s:%d\" ", proxy_ptr->http_proxy_server, proxy_ptr->http_port);
          RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
        }
        else if(proxy_ptr->https_proxy_server != NULL)  //HTTPS
        {
          cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--proxy-server=\"https=%s:%d\" ", proxy_ptr->https_proxy_server, proxy_ptr->https_port);
          RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
        }
      }
    }
 
    cmd_write_idx = snprintf(cmd_buf_ptr, RBU_MAX_CMD_LENGTH, "--disable-web-security --ignore-certificate-errors about:blank >> %s 2>&1 & echo -n $!", file_name);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else /* System chrome browser */
  {
    // BugID: 8517, 8518 "--disable-web-security : To disable security issues of js"
    cmd_write_idx = sprintf(cmd_buf_ptr, "nohup chromium-browser --user-data-dir=%s/.rbu/.chrome/profiles/%s --display=:%d"
                                         " --cav_testrun=%d  --controller_name=%s --debug_level=%d --ns_wdir=%s --cav_partition=%lld "
                                         " --disable-web-security --ignore-certificate-errors  about:blank >> %s 2>&1 & echo -n $!",
                                         g_home_env, rbu_resp_attr->profile, rbu_resp_attr->vnc_display, testidx, controller_name,
                                         get_debug_level(vptr), g_ns_wdir, vptr->partition_idx, file_name);

    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  // Null terminate cmd buf 
  *cmd_buf_ptr = '\0';

  NSDL2_RBU(vptr, NULL, "Running command to start Chrome Browser = [%s]", rbu_resp_attr->firefox_cmd_buf);

  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, handler_sigchild_ignore);

  FILE *fp = popen(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, "r");
  //Need to check if browser was started successfully or not.(It may through some error messages.)
  if(fp == NULL)
  {
    NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4, 
                            vptr->sess_ptr->sess_name, vptr->cur_page->page_name, "Error in starting Chrome browser. Error = %s", 
                            nslb_strerror(errno));

    NSTL1(vptr, NULL, "Error in starting Chrome browser. Error = %s", nslb_strerror(errno)); 
    strncat(rbu_resp_attr->access_log_msg, "Error: Chrome Browser not started", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Chrome Browser not started, Error: %s", nslb_strerror(errno));
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }

  fgets(read_buf, 1024, fp);
  NSDL2_RBU(vptr, NULL, "Chrome Browser started succesfully, Chrome proc id = [%s]", read_buf);

  vptr->httpData->rbu_resp_attr->browser_pid = atoi(read_buf);
    
  (void) signal( SIGCHLD, prev_handler);
  pclose(fp);
  VUSER_SLEEP_RBU_START_BROWSER(vptr, 10000, mode);
     
  return 0;
}

//nslb_tcp_client_ex(svr_ip, svr_port, 10, err_msg);
// int nslb_tcp_client_ex(char *server_name, int default_port, TCP_CLIENT_CON_TIMEOUT, char *err_msg)
// This function is used for making TCP connection
static int ns_rbu_chrome_make_connection(VUser *vptr)
{
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  if(rbu_resp_attr == NULL) 
  {
    NSTL1(vptr, NULL, "Error: ns_rbu_chrome_make_connection() - rbu_resp_attr is NULL");
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }
 
  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p, rbu_resp_attr = %p, browser_pid = %d", vptr, rbu_resp_attr, 
                         vptr->httpData->rbu_resp_attr->browser_pid);

  /* (1) check chrome process is running or not if not then start again*/
  /* (2) Get port form port_info.txt */
  /* (3) If chrome process is running then make TCP connection to cav-listner*/
  /* (4) If connection is not made successfully then retry to NUM_RETRY number of times finally through error and return*/

  /* (1) check chrome process is running or not if not then start again*/
  /* If chrome is running then store their pid to kill on session end */
  if(vptr->httpData->rbu_resp_attr->browser_pid != 0)
  {
    NSDL3_RBU(vptr, NULL, "Browser_pid = %d", vptr->httpData->rbu_resp_attr->browser_pid);
    if(kill(vptr->httpData->rbu_resp_attr->browser_pid, 0) != 0)
    {
      NSTL1(vptr, NULL, "Chrome browser with proc id '%d' is not running, retry to invoke...", 
                             vptr->httpData->rbu_resp_attr->browser_pid);
      if(ns_rbu_chrome_start_browser(vptr, 1) != 0)
      {
        NSTL1(vptr, NULL, "Unable to invoke Chrome browser, Hence returning...");
        return -1;
      }
    }
    else
    {
     
      int ret = make_rbu_connection(vptr); // We already have browser running
      if(ret != 1 && ret != 0)//make rbu_connection internally calls wait_for_har_file which calls web_url_end which can return 1
        HANDLE_RBU_PAGE_FAILURE(ret)
       return ret;
    }
  }
  return 0;
      
}

// This is a callback function for making rbu_connection
void make_rbu_connection_callback(ClientData client_data)
{
  VUser *vptr = (VUser *)client_data.p;
  NSDL3_RBU(vptr, NULL, "Timer Expired - Method called - Callback function");
  int ret = make_rbu_connection(vptr);
  if((ret != 1) && (ret != 0))
    HANDLE_RBU_PAGE_FAILURE(ret)
}

// This is a common function called after making chrome connection or fireforx connection
int wait_for_lighthouse_or_make_har_file(VUser *vptr)
{
  int ret;
  NSDL3_RBU(vptr, NULL, "Method Called");
 
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
 
  // In main function ns_rbu_execute_page_via_firefox or ns_rbu_execute_page_via_chrome rbu_resp_attr has been malloced 
  // and all its member has been filled
  rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  
  //Dumping Header For RBU Access Log
  //Right now, due to NIFA we will dump header whenever RBU is enabled and even if rbu_acc_log is disabled
  ns_rbu_log_access_log(vptr, rbu_resp_attr, RBU_ACC_LOG_DUMP_HEADER);

  /* Wait till either har file not generated or timeout */
  unsigned short har_timeout;
  HAR_TIMEOUT
  
  time_t start_time = time(NULL);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.lighthouse_mode)
  { 
    ret = ns_rbu_wait_for_lighthouse_report(vptr, har_timeout, start_time);
  }
  else
  {
    ret = ns_rbu_wait_for_har_file(vptr->cur_page->page_name, rbu_resp_attr->url, rbu_resp_attr->profile, har_timeout, rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir, rbu_resp_attr->firefox_cmd_buf, vptr->first_page_url->proto.http.rbu_param.har_rename_flag, vptr, start_time);
  }
 
  return ret; 
}

// Function to make RBU Connection. Old Function has been removed
int make_rbu_connection(VUser *vptr)
{

  char *cav_listner_ip = "127.0.0.1";
  char port_file[512 + 1] = "";   //save the port no
  char read_buf[512 + 1] = "";
  char err_msg[1024 + 1] = "";
  int cav_listner_port = 9080;
  int con_fd = -1;

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  int retry = rbu_resp_attr->retry_count;

  ContinueOnPageErrorTableEntry_Shr *ptr;
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];
 
  /* (2) Get port form port_info.txt */
  snprintf(port_file, 512, "%s/.rbu/.chrome/profiles/%s/port_info.txt", g_home_env, rbu_resp_attr->profile);  
  NSDL2_RBU(vptr, NULL, "Port file = [%s]", port_file);

  FILE *port_fp = fopen(port_file, "r");
  if(port_fp == NULL)
  {
    NSTL1(vptr, NULL, "Error: Failed to open port file = %s, errno = %d, error = %s \n", port_file, errno, nslb_strerror(errno));
    strncat(rbu_resp_attr->access_log_msg, "Error: Failed to open port file", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Failed to open Netstorm Browser Extension(i.e. CavService) port file.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    return -1;
  }
  
  fgets(read_buf, 512, port_fp);
  fclose(port_fp);

  cav_listner_port = atoi(read_buf);
  
  NSDL2_RBU(vptr, NULL, "cav_listner_port = %d, rbu_com_setting_max_retry = %d", 
                           cav_listner_port, global_settings->rbu_com_setting_max_retry);

  /* (3) If chrome process is running then make TCP connection to cav-listner*/
  NSDL2_RBU(vptr, NULL, "retry = %d, Making connection to cav-listner cav_listner_ip = %s, cav_listner_port = %d", 
                                                                             retry, cav_listner_ip, cav_listner_port);

  NSTL1(vptr, NULL, "retry = %d, Making connection to cav-listner cav_listner_ip = %s, cav_listner_port = %d", 
                                                                             retry, cav_listner_ip, cav_listner_port);
  con_fd = nslb_tcp_client_ex(cav_listner_ip, cav_listner_port, TCP_CLIENT_CON_TIMEOUT, err_msg);
  NSDL2_RBU(vptr, NULL, "con_fd = %d", con_fd);
  // Check connection made or not? If made then break loop
  // If Retry cout greater than provided then return -1
  if(con_fd > 0){
    NSTL1(vptr, NULL, "Connection made successfully on port = %d, cav_fd = %d, con_retry_count = %d", cav_listner_port, con_fd, retry);
    NSDL2_RBU(vptr, NULL, "Connection made successfully on port = %d, cav_fd = %d, con_retry_count = %d", cav_listner_port, con_fd, retry);
    NSDL2_RBU(vptr, NULL, "Connection to cav-listner made succefully returning con_fd. retry cout = %d, con_fd = %d", retry, con_fd);
  //  return con_fd;
  }
  else 
  {

    retry++;
    
    if((retry > global_settings->rbu_com_setting_max_retry) && (con_fd == -1))
    {
      NSTL1(vptr, NULL, "Error: Connection is not made as retry count %d exceed to max %d. con_fd = %d, err_msg = %s \n", retry, 
                              global_settings->rbu_com_setting_max_retry, con_fd, err_msg);

      strcpy(rbu_resp_attr->access_log_msg, "Error: Connection is not made Successfully");
      snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to create connection with NetStorm Browser Extension (i.e CavService). Error: %s", err_msg);
      
      //We are aborting session is G_CONTINUE_ON_PAGE_ERR 
      if(ptr->continue_error_value == 0)
      {
        vptr->page_status = NS_REQUEST_ERRMISC;
        vptr->sess_status = NS_REQUEST_ERRMISC;
        vptr->last_cptr->req_ok = NS_REQUEST_ERRMISC;

        NSDL3_RBU(vptr, NULL, "'G_CONTINUE_ON_PAGE_ERR = [%d]' and page_status = [%d], sess_status = [%d], req_ok = [%d]", 
                              ptr->continue_error_value, vptr->page_status, vptr->sess_status, vptr->last_cptr->req_ok);
        //Add vptr->operation to VUT_RBU_WEB_URL_END and then switch to ns_rbu_handle_web_url_end()
        vut_add_task(vptr, VUT_RBU_WEB_URL_END);
      }
      else
      {
        vptr->page_status = NS_REQUEST_ERRMISC;
        vptr->sess_status = NS_REQUEST_ERRMISC;
        vptr->last_cptr->req_ok = NS_REQUEST_ERRMISC;
        NSDL3_RBU(vptr, NULL, "'G_CONTINUE_ON_PAGE_ERR = [%d]' and page_status = [%d], sess_status = [%d], req_ok = [%d]", 
                                ptr->continue_error_value, vptr->page_status, vptr->sess_status, vptr->last_cptr->req_ok);
      }
      return -1;
    }
    rbu_resp_attr->retry_count = retry;
    VUSER_SLEEP_MAKE_RBU_CONNECTION(vptr, global_settings->rbu_com_setting_interval);
    return 0;
  }
  // Initialize it 0 so that it can be used later
  rbu_resp_attr->retry_count = 0;

  int ret;
  if(con_fd > 0)
  {

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.selector_mapping_mode == 1)
    {
      if(ns_rbu_chrome_make_message_1000(vptr) != 0)
        return -1;
      if((ns_rbu_chrome_send_message(vptr, con_fd)) == -1)
        return -1;
      if((ns_rbu_chrome_read_message(vptr, con_fd)) == -1)
        return -1;
    }

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.lighthouse_mode)
    {
      if(ns_rbu_chrome_make_lh_message(vptr) != 0)
        return -1;
    }
    else
    {
      if(ns_rbu_chrome_make_message(vptr) != 0)
        return -1;
    }

    if((ns_rbu_chrome_send_message(vptr, con_fd)) == -1)
      return -1;

    if((ns_rbu_chrome_read_message(vptr, con_fd)) == -1)
      return -1;

    if(close(con_fd) == -1)
    {
      NSTL1(vptr, NULL, "Error: Unable to close connection to native host , conn_fd =  %d, errno = %d, errmsg = %s",
                       con_fd, errno, nslb_strerror(errno));
      strncat(vptr->httpData->rbu_resp_attr->access_log_msg, "Error: Unable to close connection to native host",
                  RBU_MAX_ACC_LOG_LENGTH);
      con_fd = -1;
    }

    vptr->httpData->rbu_resp_attr->first_pg = 0;
    NSDL2_RBU(vptr, NULL, "Set first_pg = %d", vptr->httpData->rbu_resp_attr->first_pg);
    // Common function for light house or make har file
    ret = wait_for_lighthouse_or_make_har_file(vptr); 

  }
  if(ret != 0)
    return ret;
  
  return 0;
}

// Send JSON message to CavService
int ns_rbu_chrome_send_message(VUser *vptr, int fd)
{ 
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  int len =  strlen(rbu_resp_attr->firefox_cmd_buf);
 
  NSDL1_RBU(vptr, NULL, "Method called, fd = %d, len = %d, msg = %s", fd, len, rbu_resp_attr->firefox_cmd_buf);

  if(send(fd, rbu_resp_attr->firefox_cmd_buf, len, 0) != len)
  {
    NSTL1(vptr, NULL, "Method called : Failed in sending message to CavService, errno = %d, errmsg = %s, msg(NS->CavService) = %s", 
                       errno, nslb_strerror(errno), rbu_resp_attr->firefox_cmd_buf);
    NSDL2_RBU(vptr, NULL, "Failed in sending message to CavService, errno = %d, errmsg = %s, msg(NS->CavService) = %s", 
                       errno, nslb_strerror(errno), rbu_resp_attr->firefox_cmd_buf);
    return -1;
  }
  return 0;
}

//Read response mesgase from CavService
int ns_rbu_chrome_read_message(VUser *vptr, int con_fd)
{
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, NULL, "Method called, con_fd = %d", con_fd);

  //Read msg to cav-listner
  if(read(con_fd, rbu_resp_attr->firefox_cmd_buf, 512) <= 0) 
  {
    NSDL1_RBU(vptr, NULL, "Error: (ns_rbu_execute_page_via_chrome), netstorm failed to read response from browser. = %s", nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_execute_page_via_chrome), netstorm failed to read response from browser, error = %s.\n", nslb_strerror(errno));
    strncat(rbu_resp_attr->access_log_msg, "Error: netstorm failed to read response from browser", 
                  RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Netstorm failed to read response from browser. Error: %s", nslb_strerror(errno));
    close(con_fd);
    return -1; 
  }
  if(strncasecmp(rbu_resp_attr->firefox_cmd_buf, "success", 7))  
  {
    NSDL3_RBU(vptr, NULL, "Error: Chrome failed to process ns_web_url req.");
    strncat(rbu_resp_attr->access_log_msg, "Error: Chrome failed to process ns_web_url req", 
                  RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Chrome failed to process ns_web_url request.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    return -1;
  }
  return 0;
}
int ns_rbu_execute_page_via_node(VUser *vptr, int page_id)
{
  char cmd[RBU_MAX_CMD_LENGTH + RBU_MAX_FIREFOX_CMD_LENGTH];
  char log_file_name[512 + 1] = "";

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = %p, page_id = %d, rbu_resp_attr = %p, sess_start_flag = %d",
                         vptr, page_id, vptr->httpData->rbu_resp_attr, vptr->httpData->rbu_resp_attr->sess_start_flag);

  RBU_NS_LOGS_PATH

  snprintf(log_file_name, RBU_MAX_FILE_LENGTH, "%s/logs/%s/ns_rbu_chrome_out_prof_%s.log", 
                                            g_ns_wdir, rbu_logs_file_path, rbu_resp_attr->profile);
  if(vptr->httpData->rbu_resp_attr->sess_start_flag == 0)
  {
    NSDL2_RBU(vptr, NULL, "First page of session - Invoke Chrome Browser to start cav-listner");

    vptr->httpData->rbu_resp_attr->sess_start_flag = 1;
    vptr->httpData->rbu_resp_attr->first_pg = 1;
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.lighthouse_mode)
  {
    if(ns_rbu_chrome_make_lh_message(vptr) != 0)
    {
      HANDLE_RBU_PAGE_FAILURE(-1)
      return -1;
    }
  }
 
  if(global_settings->enable_ns_chrome) 
  {
    snprintf(cmd, RBU_MAX_CMD_LENGTH + RBU_MAX_FIREFOX_CMD_LENGTH, "nohup node %s/rbu/ns_rbu_node_manager.js '%s' '%s' '%s/chrome' >> %s 2>&1 &",
                g_ns_wdir, rbu_resp_attr->firefox_cmd_buf, rbu_resp_attr->profile, g_ns_chrome_binpath, log_file_name);
  } else {
    snprintf(cmd, RBU_MAX_CMD_LENGTH + RBU_MAX_FIREFOX_CMD_LENGTH, "nohup node %s/rbu/ns_rbu_node_manager.js '%s' '%s' >> %s 2>&1 &",
                g_ns_wdir, rbu_resp_attr->firefox_cmd_buf, rbu_resp_attr->profile, log_file_name);
  }

  //snprintf(cmd, RBU_MAX_CMD_LENGTH + RBU_MAX_FIREFOX_CMD_LENGTH, "nohup node %s/rbu/ns_rbu_node_manager.js '%s' '%s' >> %s 2>&1 &",
  //              g_ns_wdir, rbu_resp_attr->firefox_cmd_buf, rbu_resp_attr->profile, log_file_name);
  NSDL2_RBU(vptr, NULL, "cmd = %s", cmd);

  //VIKAS: Send Message to Node
  nslb_system2(cmd);

  vptr->httpData->rbu_resp_attr->first_pg = 0;
  NSDL2_RBU(vptr, NULL, "Set first_pg = %d", vptr->httpData->rbu_resp_attr->first_pg);

  int ret = wait_for_lighthouse_or_make_har_file(vptr);
  if((ret != 1) && (ret != 0))// We can have one case where wait_for_har_file calls rbu_web_url_end which returns 1
    HANDLE_RBU_PAGE_FAILURE(ret)

  return ret;
}

int ns_rbu_execute_page_via_chrome(VUser *vptr, int page_id)
{

  NSDL1_RBU(vptr, NULL, "Method called, vptr = %p, page_id = %d, rbu_resp_attr = %p, sess_start_flag = %d",
                         vptr, page_id, vptr->httpData->rbu_resp_attr, vptr->httpData->rbu_resp_attr->sess_start_flag);

  /* In chrome there is no concept of dummy but there is concept of listener So, on start of the session we always start a listener
     just like dummy page in firefox */ 
  if(vptr->httpData->rbu_resp_attr->sess_start_flag == 0)
  {
    NSDL2_RBU(vptr, NULL, "First page of session - Invoke Chrome Browser to start cav-listner");
    
    vptr->httpData->rbu_resp_attr->sess_start_flag = 1;
    vptr->httpData->rbu_resp_attr->first_pg = 1;

     if(ns_rbu_chrome_start_browser(vptr, 0) != 0)
       return -1;
  }
  else
  {
    ns_rbu_browser_after_start(vptr);
  }

  return 0;
}
void ns_rbu_action_after_browser_start_callback(ClientData client_data)
{

  NSDL4_RBU(NULL, NULL, "Timer Expired:: Method Called - Callback Function");
  VUser *vptr = (VUser *) client_data.p;
  NSDL4_RBU(NULL, NULL, "Timer Expired:: Method Called - Callback Function Exit");
  ns_rbu_browser_after_start(vptr);
   
}

int ns_rbu_browser_after_start(VUser *vptr)
{
  int con_fd = -1;
  char log_path[1024 + 1];
  
  NSDL2_RBU(vptr, NULL, "Method Called - vptr(%p)", vptr);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_rm_unwntd_dir_mode == 2)
  {
    snprintf(log_path, 1024, "%s/.rbu/.chrome/profiles/%s/%s",
                             g_home_env, vptr->httpData->rbu_resp_attr->profile, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_rm_unwntd_dir_list);

    NSDL2_RBU(NULL, NULL, "Unwanted Directory : log_path = %s", log_path);

    if(remove_directory(log_path) != 0 )
      NSDL1_RBU(NULL,NULL, "Unable to delete directory");
    else
      NSDL1_RBU(NULL, NULL, "Directory sucessfully deleted");
  }

  if((con_fd = ns_rbu_chrome_make_connection(vptr)) == -1)
    return -1;

  return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------ 
 * Purpose    : This function will remove_cookies 
 *    
 * Input      : vptr, name, path, domain and free_for_next_req 
 *    
 * Output     : On error    -1
 *              On success   0
 *
 * Chrome Msg : {"opcode": "1002",  "params": {"cookies": [{"flag": 0, "pairs": [{"name": "fsr.s.session", "domain": "NULL"}]}]}}
 *              Here: flag: 0 -> remove_cookie
 *                    flag: 1 -> add_cookie 
 *
 * Build_ver  : 4.1.7# 18
 *------------------------------------------------------------------------------------------------------------------------------------*/
int ns_rbu_remove_cookies(VUser *vptr, char *name, char *path, char *domain, int free_for_next_req)
{
  char *cmd_buf_ptr = NULL;                            /* This pointer is responsible to fill chrome command buffer */
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  int cookie_count = 0;
  int con_fd = -1;

  NSDL2_RBU(NULL, NULL, "Method called, name = %s, path = %s, free_for_next_req = %d", name, path, domain, free_for_next_req);

  if((con_fd = ns_rbu_chrome_make_connection(vptr)) == -1)
    return -1;
 
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  // Make msg for remove_cookie
  NSDL2_RBU(NULL, NULL, "Chrome CA Message: (Start): opcode and param");
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                              "{\"opcode\": \"1002\", "
                              " \"params\": "
                                   "{\"cookies\": [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"flag\": 0, \"pairs\": [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
 
  REMOVE_COOKIES_IN_CHROME_MSG(name, domain);  

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]}]}}\n");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  // Null terminate cmd buf
  *cmd_buf_ptr = '\0';

  NSDL3_RBU(vptr, NULL, "Request to send = %s", rbu_resp_attr->firefox_cmd_buf);

  if((ns_rbu_chrome_send_message(vptr, con_fd)) == -1)
    return -1;

  //Read msg to cav-listner
  if(read(con_fd, vptr->httpData->rbu_resp_attr->firefox_cmd_buf, 512)  <= 0)
  {
    NSDL1_RBU(vptr, NULL, "Error: (ns_rbu_execute_page_via_chrome), netstorm failed to read response from browser. = %s", nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_execute_page_via_chrome), netstorm failed to read response from browser, error = %s.\n", nslb_strerror(errno));
    strncat(rbu_resp_attr->access_log_msg, "Error: netstorm failed to read response from browser", 
                     RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Netstorm failed to read response from browser, Error: %s.", nslb_strerror(errno));
    close(con_fd);
    return -1;
  }

  if(close(con_fd) == -1)
  {
    NSDL3_RBU(vptr, NULL, "Error: Unable to close connection to native host , conn_fd =  %d", con_fd );
    con_fd = -1;
  }

  if(strncasecmp(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, "success", 7))
  {
    NSDL3_RBU(vptr, NULL, "Error: Chrome failed to process ns_web_url req.");
    strncat(rbu_resp_attr->access_log_msg, "Error: Chrome failed to process ns_web_url req", 
                     RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Chrome failed to process ns_web_url request.",MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    return -1;
  }

  vptr->httpData->rbu_resp_attr->first_pg = 0;
  NSDL2_RBU(vptr, NULL, "Set first_pg = %d", vptr->httpData->rbu_resp_attr->first_pg);

  return 0;
}
