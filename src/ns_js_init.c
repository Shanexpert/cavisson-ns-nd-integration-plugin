/******************************************************************
 * Name                 : ns_js_init.c 
 * Purpose              : This file holds the keyword parsing & initialisation function.
 * Note                 :
 * Initial Version      : Sun Feb 20 13:01:16 IST 2011
 * Modification History :
 ******************************************************************/

#include <regex.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "decomp.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
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
#include "ns_sock_list.h"
#include "ns_msg_com_util.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "amf.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_sock_com.h"
#include "netstorm_rmi.h"
#include "ns_child_msg_com.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
#include "ns_ssl.h"
#include "ns_wan_env.h"
#include "ns_url_req.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_auto_redirect.h"
#include "ns_replay_access_logs.h"
#include "ns_vuser.h"
#include "ns_schedule_phases_parse.h"
#include "ns_gdf.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_page.h"
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "nslb_util.h"
#include "ns_event_log.h"
#include "ns_keep_alive.h"
#include "ns_event_id.h"
#include "ns_http_process_resp.h"
#include "ns_http_pipelining.h"
#include "ns_js.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"

#define NS_BOOTSTRAP_JS_FILE "ns_bootstrap.js" 

JSRuntime *ns_js_rt = NULL;
char *bootstrap_js_buffer = NULL;
int bootstrap_js_buffer_len = 0;
JSObject *js_bootstrap_script = NULL;

static void load_ns_boostrap_for_js() {

  char err_msg[4096];
  char file_name[2048];

  NSDL2_HTTP(NULL, NULL, "Method called");

  sprintf(file_name, "%s/etc/%s", g_ns_wdir, NS_BOOTSTRAP_JS_FILE);

  if((bootstrap_js_buffer = load_file_into_buf(file_name, err_msg)) == NULL) {
    NSDL2_HTTP(NULL, NULL, "Error: %s", err_msg);
    fprintf(stderr, "Error: %s\n", err_msg);
    return;
  }
  bootstrap_js_buffer_len = strlen(bootstrap_js_buffer);
}

static void kw_usage_java_script_runtime_mem(char *kw, char *error_message) {

  NSTL1_OUT(NULL, NULL, "Invalid use of Keyword %s: %s\n", kw, error_message);
  NSTL1_OUT(NULL, NULL, "Usage:\n");
  NSTL1_OUT(NULL, NULL, "   %s <mode> <runtime_mem_in_MB> [<javascript_stack_size_in_KB>]\n", kw);
  NSTL1_OUT(NULL, NULL, "   Where\n");
  NSTL1_OUT(NULL, NULL, "   mode:\n");
  NSTL1_OUT(NULL, NULL, "       0 Memory specified is per NVM (Default)\n");
  NSTL1_OUT(NULL, NULL, "       1 Memory specified is per Virtual User\n");
  NSTL1_OUT(NULL, NULL, "   runtime_mem_in_MB:\n");
  NSTL1_OUT(NULL, NULL, "       Memory for javascript RunTime in MB\n");
  NSTL1_OUT(NULL, NULL, "   javascript_stack_size_in_KB (optional):\n");
  NSTL1_OUT(NULL, NULL, "       JS context stack size KB, default 4 KB\n");
  NS_EXIT(-1, "%s\nUsage: %s <mode> <runtime_mem_in_MB> [<javascript_stack_size_in_KB>] %s", error_message, kw);
}

/* JAVA_SCRIPT_RUNTIME_MEM <value> */
/* Memory is in MB */

void kw_set_java_script_runtime_mem(char *buf) {     
   char keyword[MAX_DATA_LINE_LENGTH];
   int num;
   unsigned int runtime_mem = 0, stack_size = 0;
   unsigned char mode = 0;
   char str_runtime_mem[100] = "\0", str_mode[100] = "\0", str_js_stack_size[100] = "\0";
              
   NSDL2_PARSING(NULL, NULL, "Method called. buf = %s", buf);

   num = sscanf(buf, "%s %s %s %s", keyword, str_mode, str_runtime_mem, str_js_stack_size);

  /* Validate the keyword and check if integer values */
  if(num < 3 || num > 4) 
    kw_usage_java_script_runtime_mem(keyword, "invalid number of arguments, <mode> and <runtime mem> are mandatory");
  if(ns_is_numeric(str_mode) == 0) 
    kw_usage_java_script_runtime_mem(keyword, "mode can have only integer value");
  if(ns_is_numeric(str_runtime_mem) == 0) 
    kw_usage_java_script_runtime_mem(keyword, "runtime memory can have only integer value");
  if(num>3 && ns_is_numeric(str_js_stack_size) == 0) 
    kw_usage_java_script_runtime_mem(keyword, "stack size can have only integer value");

  /* Convert strings to integer */ 
  runtime_mem = atoi(str_runtime_mem);
  stack_size = atoi(str_js_stack_size);
  mode = (unsigned char) atoi(str_mode); 

  /* validate the integer values */
  if(runtime_mem <= 0 || runtime_mem > (4 * 1024)) 
    kw_usage_java_script_runtime_mem(keyword, "runtime memory can not have value <= 0 or more than 4096 MB");
  if(mode != 0 && mode != 1)
    kw_usage_java_script_runtime_mem(keyword, "mode can either be 0 or 1");
     

   global_settings->js_runtime_mem = runtime_mem * 1024 * 1024;
   global_settings->js_runtime_mem_mode = mode;
   if(num<4)
     global_settings->js_stack_size = 4 * 1024; /* By default 4k */
   else
     global_settings->js_stack_size = stack_size * 1024;

   NSDL2_HTTP(NULL, NULL, "js_runtime_mem = %u Bytes (%u MB), "
                          "js_stack_size = %u Bytes (%u KB), " 
                          "js_runtime_mode = %u", 
                          global_settings->js_runtime_mem, runtime_mem,
                          global_settings->js_stack_size, stack_size,
                          global_settings->js_runtime_mem_mode);
}

/* Keyword Parsing Stuff
 * G_JAVA_SCRIPT <0|1|2> <Save all as JavaScript>
 *      0- Disable
 *      1- Enable but don't use Checkpoints etc ..
 *      2- Use everything
 */

int kw_set_g_java_script(char *buf, GroupSettings *g_set, char *err_msg, int runtime_flag) {

  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_fields;
  int js_mode;
  int js_all = 0;

  num_fields = sscanf(buf, "%s %s %d %d", keyword, grp, &js_mode, &js_all);
  
  if(num_fields < 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JAVA_SCRIPT_USAGE, CAV_ERR_1011083, CAV_ERR_MSG_1);
  }

  if(js_mode < 0 || js_mode > 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JAVA_SCRIPT_USAGE, CAV_ERR_1011083, CAV_ERR_MSG_3);
  }

  if(js_all < 0 || js_all > 1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_JAVA_SCRIPT_USAGE, CAV_ERR_1011083, CAV_ERR_MSG_3);
  }

  if(js_mode != 0 && !global_settings->js_enabled)
     global_settings->js_enabled = 1;

  g_set->js_mode = js_mode;
  g_set->js_all = js_all;

  return 0;
}

/* This function called from all netstorm NVMs which does the allocation
 * for JS Runtime * if any of group has enaled JAVA Script Engine*/
int js_init() {

  NSDL2_HTTP(NULL, NULL, "Method called. JS Enabled = %d, js_runtime_mem = %d, js_runtime_mem_mode = %d",
                          global_settings->js_enabled, 
                          global_settings->js_runtime_mem,
                          global_settings->js_runtime_mem_mode); 

  if(global_settings->js_enabled && global_settings->js_runtime_mem_mode == NS_JS_RUNTIME_MEMORY_MODE_PER_NVM) {
    ns_js_rt = JS_NewRuntime(global_settings->js_runtime_mem);
    if(ns_js_rt == NULL) {
       NSDL2_HTTP(NULL, NULL, "Unable to allocate  memory for Java Run Time.");
       END_TEST_RUN       
    }
  }

  load_ns_boostrap_for_js();

  return 0;
}
