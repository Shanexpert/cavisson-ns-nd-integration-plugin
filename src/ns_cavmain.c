#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "nslb_util.h"
#include "ns_cavmain.h"

#include "common.h"
#include "ns_common.h"
#include "tmr.h"
#include "ns_global_settings.h"
#include "ns_monitoring.h"
#include "ns_tls_utils.h"
#include "netstorm.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_error_msg.h"
#include "ns_exit.h"
#include "nslb_log.h"
#include "ns_cavmain_child.h"
#include "ns_debug_trace.h"
#include "ns_kw_usage.h"
#include "ns_test_monitor.h"
#include "ns_http_hdr_states.h"
#include "ns_http_process_resp.h"
#include "nslb_cav_conf.h"
#include "ns_http_status_codes.h"
#include "nslb_http_state_transition_init.h"
#include "nslb_time_stamp.h"
#include "ns_parse_scen_conf.h"
/*************************************************/
#define NS_PORT_FILE		"NSPort" 
//#define NSLB_MAP_INIT_SIZE	1024
//#define NSLB_MAP_DELATE_SIZE	128
#define FIL_PLOAD_URL_TXT	"FILE_UPLOAD_URL"
#define CMON_ENV_PATH		"/home/cavisson/monitors/sys/cmon.env"
#define MINIMUM_ARGS		2
#define HTTP_STATE_MODEL_FILE_NAME "etc/ns_http_hdr_state_model.txt"
extern char* argv0;
extern int num_processor;
FileUploadInfo g_file_upload_info;

void cm_init_defaults();
int kw_set_file_upload_settings(char *buf, Global_data* global_settings, char *err_msg, int runtime_flag);
int cm_get_num_processor();

#define SM_FILL_FILE_UPLOAD_INFO()\
{\
 strcpy(g_file_upload_info.server_ip, global_settings->file_upload_info->server_ip);\
 g_file_upload_info.server_port = global_settings->file_upload_info->server_port;\
 g_file_upload_info.protocol = global_settings->file_upload_info->protocol; \
 strcpy(g_file_upload_info.url,  global_settings->file_upload_info->url);\
}

#define CLOSE_FP(fp){ \
 if(fp)\
 {\
   if( fclose(fp) != 0)\
     NSTL1(NULL, NULL, "Error in closing fp, Error: %s\n", nslb_strerror(errno));\
 }\
  fp = NULL;\
}

#define FILE_UPLOAD_SETTINGS_USAGE "Usage: FILE_UPLOAD_SETTINGS <Min Thread> <Max Thread> <Min Queue> <Max Queue> <NUM RETRY> <TIME OUT>"
NSLBMap *sm_map;
FilePath g_file_path;
int parent_listen_fd = -1;
int parent_pid;
unsigned short cm_port_number;
Msg_com_con **g_msg_com_con_arr = NULL;

#define CAVGEN_VERSION_USAGE  "Usage: CAVGEN_VERSION  <version number>"
/*************************************************/

#define CM_IS_PARENT_ID() (parent_pid == getpid())

static char* g_req_arr[SM_REQ_MAX_FIELDS] = { "o", "monId", "gMonId", "name", "monType",
				 "tier", "server", "partitionId", "tr", "operation", "keyword"};

int g_parent_child_port_number;
int ns_get_keyword(char* filePath, char* keyword, char *out)
{
  
  FILE *fp = fopen(filePath, "r");
  if(fp == NULL)
    return CM_ERROR;

  char *ptr;
  char buf[2048+1];

  while(fgets(buf, 1024, fp))
  {
     buf[strlen(buf) - 1] = '\0'; //Removing new lines.
    //ignore emtpy lines.
    if((buf[0] == '\n')||(buf[0] == '#'))
      continue;

    if(!strncasecmp(buf, keyword,strlen(keyword)))
    {
      ptr = strchr(buf, '=');
      if(ptr)
      {
        //move ahead of =
        ptr++;
        strcpy(out, ptr); 
        return CM_SUCCESS;
      }
      fprintf(stderr, "Invalid format of Keyword (%s)\n", keyword);
      return CM_ERROR;
    }
  }
  return CM_ERROR;
}

/*Read following data from cavgen.conf file placed at $NS_WDIR/sys/cavgen.conf
* 
*NUM_CVM	<DEFAULT NUM CPUs>
*DEBUG <ON/OFF> <LEVEL>
*TARCE <ON/OFF> <LEVEL>
*MAX_LOG_FILE_SIZE <FOR BOTH DEBUG AND TRACE>
*MODULE_MASK
*LIB_MODULE_MASK
*/
int ns_read_and_parse_kw(char* file_name, GroupSettings * group_settings)
{
  cm_init_defaults();
  /*NSDL2_SCHEDULE(NULL, NULL, "Method called");*/
  FILE *cavgen_file;
  if ((cavgen_file = fopen(file_name, "r")) == NULL) {
     return CM_ERROR;
  }

  char buf[MAX_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_LINE_LENGTH];
  char err_msg[MAX_ERR_MSG_LENGTH];
 
  while (fgets(buf, MAX_LINE_LENGTH, cavgen_file) != NULL) {
    if ((MINIMUM_ARGS > sscanf(buf, "%s %s", keyword, text))) {
        continue;
    }
    else if(strcasecmp(keyword, "CAVGEN_VERSION") == NS_ZERO){
      kw_set_cavgen_version(buf, &SM_GET_CAVGEN_VERSION(), err_msg, 0);
    }
    else if(strcasecmp(keyword, "DEBUG") == NS_ZERO){
    #ifdef NS_DEBUG_ON
      kw_set_debug(buf, &group_settings->debug, err_msg, 0);
    #endif
    }
    else if(strcasecmp(keyword, "DEBUG_LOGGING_WRITER") == NS_ZERO){
      //global_settings->logging_writer_debug
      kw_logging_writer_debug(buf);
    }
    else if(strcasecmp(keyword, "DEBUG_TRACE") == NS_ZERO){
      //debug_trace_log_value
      kw_set_debug_trace(text, keyword, buf);
    }
    else if(strcasecmp(keyword, "FILE_UPLOAD_SETTINGS") == NS_ZERO){
       kw_set_file_upload_settings(buf, global_settings, err_msg, 0);
    }
    else if(strcasecmp(keyword, "MODULEMASK") == NS_ZERO){
      kw_set_modulemask(buf, &group_settings->module_mask, err_msg, 0);
    }
    else if(strcasecmp(keyword, "LIB_DEBUG") == NS_ZERO){
       set_nslb_debug(buf);
    }
    else if(strcasecmp(keyword, "LIB_MODULEMASK") == NS_ZERO){
      set_nslb_modulemask(buf, err_msg);
    }
    else if(strcasecmp(keyword, "MAX_DEBUG_LOG_FILE_SIZE") == NS_ZERO){
      kw_set_max_debug_log_file_size(buf, err_msg, 0);
    }
    else if(strcasecmp(keyword, "TRACE_LEVEL") == NS_ZERO){
      // global_settings->ns_trace_level
      kw_set_ns_trace_level(buf, err_msg, 0);
    }
    else if(strcasecmp(keyword, "NUM_CVM") == NS_ZERO){
      //global_settings->num_process
      kw_set_num_nvm(buf, global_settings, 0, err_msg);
    }
  }
  
  CLOSE_FP(cavgen_file);  


  return CM_SUCCESS; //SUCCESS;
}


void ns_init_logs(void)
{
  //Create debug log file
  MY_MALLOC(global_settings->tr_or_partition, TR_OR_PARTITION_NAME_LEN + 1, "global_settings->tr_or_partition", -1);
  snprintf(global_settings->tr_or_partition, TR_OR_PARTITION_NAME_LEN, "TR%d", testidx);

  MY_MALLOC(global_settings->tr_or_common_files, TR_OR_PARTITION_NAME_LEN + 1, "global_settings->tr_or_common_files", -1);
  snprintf(global_settings->tr_or_common_files, TR_OR_PARTITION_NAME_LEN, "TR%d/common_files", testidx);

  global_settings->ns_trace_level = 1;
  group_default_settings->debug = 0xFFFFFFFF;
  group_default_settings->module_mask = MM_ALL;
  nslb_util_set_debug_log_level(0xFFFFFFFF);
  nslb_util_set_modulemask(MM_LIB_ALL);

  char log_file[MAX_FILE_NAME];
  char error_log_file[MAX_FILE_NAME];
  sprintf(log_file, "%s/logs/TR%d/debug.log", g_ns_wdir, testidx);
  sprintf(error_log_file, "%s/logs/%s/error.log", g_ns_wdir, global_settings->tr_or_partition);

  nslb_util_set_log_filename(log_file, error_log_file);

  global_settings->script_copy_to_tr = DO_NOT_COPY_SRCIPT_TO_TR;
  set_log_dirs();
}
 
int cm_create_tr_dir(char* ns_wdir)
{  
  //get TR number
  if(CM_ERROR == (testidx = nslb_get_testRun_no())) 
    return CM_ERROR;
  
  //Create a <TR> dir
  char buf[MAX_LINE_LENGTH];
  snprintf(buf, MAX_LINE_LENGTH, "%s/logs/TR%d/", ns_wdir, testidx);
  if (mkdir_ex(buf) != 1)
  {  
    if(errno == ENOSPC)
    {
      NS_EXIT(-1, CAV_ERR_1000008);
    }
    if(errno == EACCES)
    {
      NS_EXIT(-1, CAV_ERR_1000009, ns_wdir);
    }
    //TR dir may exist as user can restart the same test run 
  }
  //Change mode of TR to 777 here.
  sprintf(buf, "logs/TR%d", testidx);
  chmod(buf, S_IRUSR|S_IWUSR|S_IXUSR |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH); 
  return CM_SUCCESS;
}
void cm_continue_with_phase1(void)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called.");
  cm_init_thread_manager(debug_fd);
  ns_register_sm_req();
  cm_wait_forever();
}

void cm_init_defaults()
{
   // IO Vector init size
   io_vector_init_size = 10000;
   io_vector_delta_size = 1000;
   io_vector_max_size = 100000;
 
   //Allocate memory for global_settings->file_upload_info
  // kw_set_test_monitor_config(NS_GET_FILE_UPLOAD_URL(), err_msg, 0);
   global_settings->num_process = NS_ZERO;
   cm_get_num_processor();
   SM_SET_CAVGEN_VERSION(CAVGEN_VERSION_1);
}
void cm_continue_with_phase2(void)
{
   NSDL2_MESSAGES(NULL, NULL, "Method called.my_port_index=%d", my_port_index);
   //ToDo: create_child as per number of NVM in config file
   //ToDo: Check with DJA if it is fine to use existing parent_port_number ??????????????
   if(NS_ZERO == (parent_port_number = cm_init_new_listner_socket(&CM_GET_LISTENER_FOR_CHILD_FD(), NS_STRUCT_TYPE_LISTEN_CHILD)))
     return;
   MY_MALLOC_AND_MEMSET(g_msg_com_con_arr, sizeof (Msg_com_con*) * global_settings->num_process, "g_msg_com_con", -1);

   NSDL2_MESSAGES(NULL, NULL, "cm parent pid=%d parent_child_port_number=%d listner_fd=%d", getpid(), CM_GET_PARENT_PORT_NUM(), CM_GET_LISTENER_FOR_CHILD_FD());
   cm_init_thread_manager_v2(debug_fd);
   ns_register_sm_req_v2();
   // to initialize all default variables if requred
   //cm_init_defaults();
   /*allocate nvm scratch buffer*/
   init_ns_nvm_scratch_buf();
   init_io_vector();
   init_http_response_codes();
   init_status_code_table();
   char state_transition_model_file[MAX_FILE_NAME];
   char log_file_name[1024];
   sprintf(state_transition_model_file, "%s/%s", g_ns_wdir, HTTP_STATE_MODEL_FILE_NAME);
   sprintf(log_file_name, "%s/logs/TR%d/state_transition_table.log", g_ns_wdir, testidx);
  /*State Transition map for HTTP(S)*/
   nslb_init_http_state_transition_model(state_transition_model_file, HdrStateArray, log_file_name, NS_MAX_HDST_ID, HDST_TEXT, ns_get_hdr_callback_fn_address);

   //init_vuser_summary_table(0);

   NSDL2_MESSAGES(NULL, NULL, "global_settings->num_process=%d", global_settings->num_process);
   int count;
   pid_t child_pid;
   char* env_buf;
   for (count = 0; count < global_settings->num_process ; ++count)
   {

     NSDL2_MESSAGES(NULL, NULL, "count=%d", count);
     /* Memory leak - ignoring for now */
     MY_MALLOC(env_buf , 32, "env_buf ", -1);
     //v_port_table[i].env_buf = env_buf;*/
     sprintf(env_buf, "CHILD_INDEX=%d", count);
     putenv(env_buf);
     if ((child_pid = fork()) < 0)
     {
       perror("*** server:  Failed to create child process.  Aborting...\n");
       NSTL1(NULL, NULL, "*** server:  Failed to create child process.  Aborting...");
       NS_EXIT(1,  "");
     }
     if (child_pid > 0)
     {
       NSDL2_MISC(NULL, NULL, "### server:  Created child process with pid = %d.\n", child_pid);
       /*set_cpu_affinity(i, child_pid); // Added by Anuj for CPU_AFFINITY : 25/03/08
       sprintf(tmp_buff, "child_pid[%d]", i);
       ret = nslb_write_all_process_pid(child_pid, tmp_buff, g_ns_wdir, testidx, "a");
       if( ret == -1)
       {
         //NSTL1_OUT(NULL, NULL, "failed to open the child_pid[%d] pid file", i);
         NSTL1(NULL, NULL, "failed to open the child_pid[%d] pid file", i);
         END_TEST_RUN
       }
       char tmp_buff[100];
       sprintf(tmp_buff,"CVM%d.pid",i+1);
       ret1 = nslb_write_process_pid(child_pid,"ns child's pid" ,g_ns_wdir, testidx, "w",tmp_buff,err_msg);
       if( ret1 == -1)
       {
         NSTL1_OUT(NULL, NULL, "failed to open the child_pid[%d] pid file","%s",i,err_msg);
       }*/
     }
     else
     {
       NSDL2_MESSAGES(NULL, NULL, "count=%d break from for loop ", count);
       //INIT_ALL_ALLOC_STATS
       break;
     }
  }

  if (CM_IS_PARENT_ID())
  {
     NSDL2_MESSAGES(NULL, NULL, "calling cm_wait_forever()");
     cm_wait_forever();
  }
  else
  {
     //child
     NSDL2_MESSAGES(NULL, NULL, "calling cm_start_child()");
     cm_start_child();
  }
  //ToDo: update mccptr conn_type in case of connect from child
   //allocate an array of Msg_com_con ptrs  corresponding to the each child/cvm/nvm
}

int cm_init_new_listner_socket(int *listner_fd, int con_type)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, listner_fd=%d con_type=%d", *listner_fd, con_type);
  int port_number;
  if(NS_ZERO == (port_number = init_parent_listner_socket_new(listner_fd, NS_ZERO)))
    return NS_ZERO;
  
  Msg_com_con *cm_listen_msg_com_con;
  MY_MALLOC_AND_MEMSET(cm_listen_msg_com_con, sizeof (Msg_com_con), "cm_msg_com_con", -1);
  cm_listen_msg_com_con->con_type = con_type;
  cm_listen_msg_com_con->fd = *listner_fd;
  NSDL2_MESSAGES(NULL, NULL, "now *listner_fd=%d port_number=%d my_port_index=%d cm_listen_msg_com_con[%p]->con_type=%d cm_listen_msg_com_con->fd=%d", *listner_fd, port_number, my_port_index, cm_listen_msg_com_con, cm_listen_msg_com_con->con_type, cm_listen_msg_com_con->fd);
  ADD_SELECT_MSG_COM_CON((char*)cm_listen_msg_com_con, *listner_fd, EPOLLIN, CONTROL_MODE);
  return port_number;
} 
 
/*******************MAIN METHOD **********************************************************/
int cm_main(int argc, char *argv[]) 
{

  argv0 = argv[0];
  //ToDo: check if ns_trace.log is being generated???=====> Generated
  NS_INIT_GROUP_N_GLOBAL_SETTINGS()
  // Initializing g_tls variable
  ns_tls_init(VUSER_THREAD_LOCAL_BUFFER_SIZE);
  //Set NS_WDIR
  set_ns_wdir();
  
  /*get FILE_UPLOAD_URL*/
  if((CM_ERROR == ns_get_keyword(CMON_ENV_PATH, FIL_PLOAD_URL_TXT, NS_GET_FILE_UPLOAD_URL())) ||
							 (NS_ZERO == strlen(NS_GET_FILE_UPLOAD_URL())) ){
   sprintf(NS_GET_ERR_BUF(), "FILE_UPLOAD_URL reading failed. Unable to read file=(%s)", CMON_ENV_PATH);
   NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
   return CM_ERROR;
  }

  /*Read TR number from $NS_WDIR/webapps/sys/config.ini and create a <TR> dir, if not available.*/
  if(CM_ERROR == cm_create_tr_dir(g_ns_wdir)) {
    sprintf(NS_GET_ERR_BUF(), "TR Number reading failed");
    NS_PUT_ERR_ON_STDERR(NS_GET_ERR_MSG())
    return CM_ERROR;
  }
  /*******************CREATE DEBUG N TRACE FILES **********************************************************/
  ns_init_logs();
  
  ///ToDo: read $NS_WDIR/sys/cavgen.conf
  ///IF FILE NOT PRESENT/ANY ERROR in FILE or data reading  => num_process = 0 i.e version 1
  //Retain this file while Build upgrade
  char temp_buff[1024];

  kw_set_test_monitor_config(NS_GET_FILE_UPLOAD_URL(), temp_buff, 0);

  sprintf(temp_buff, "%s/sys/cavgen.conf", g_ns_wdir);
  if(CM_ERROR == ns_read_and_parse_kw(temp_buff, group_default_settings))
  {
     global_settings->num_process = NS_ZERO;
  }
  else
  {
     SM_FILL_FILE_UPLOAD_INFO();
  } 

  NSDL2_MESSAGES(NULL, NULL, "global_settings->num_process=%d ", global_settings->num_process);
  /*****************************************************************************/
  /*ToDo: currently NS_EPOLL_MAXFD = 1024*32, do we need to update value???*/
  if(CM_ERROR == ns_epoll_init(&(CM_GET_EPOLL_FD())))
    return CM_ERROR;
 
  if(NS_ZERO == (cm_port_number = cm_init_new_listner_socket(&CM_GET_LISTENER_FD(), NS_STRUCT_TYPE_LISTEN))) {
    sprintf(NS_GET_ERR_BUF(), "Error!!! Unable to create socket. cm_port_number=%d", cm_port_number);
    NSDL2_MESSAGES(NULL, NULL, "%s", NS_GET_ERR_MSG());
    NSTL1(NULL, NULL, "%s", NS_GET_ERR_MSG()); 
    return CM_ERROR;
  }
  create_nsport_file(NS_PORT_FILE, cm_port_number);
 
  //initialize MAP for SM Request 
  sm_map = nslb_map_init(NSLB_MAP_INIT_SIZE, NSLB_MAP_DELATE_SIZE);
  g_avgtime_size = READ_BUF_SIZE;

  //ToDo: use MAP instead of below Norm Table
  int is_new;
  nslb_init_norm_id_table_ex(&(req_field_normtbl), SM_REQ_MAX_FIELDS);
  for(int index=0; index < SM_REQ_MAX_FIELDS; ++index) {
   nslb_get_or_set_norm_id(&(req_field_normtbl), g_req_arr[index], strlen(g_req_arr[index]), &is_new);
   NSDL2_MESSAGES(NULL, NULL, "req_arr[%s] index=%d ", g_req_arr[index], index);
  }
  //UT: Stub
  //global_settings->num_process = 1; 
  parent_pid = getpid();
  NSDL2_MESSAGES(NULL, NULL, "global_settings->num_process=%d parent_pid=%d", global_settings->num_process, parent_pid);

  switch(((global_settings->num_process) && (SM_GET_CAVGEN_VERSION() != CAVGEN_VERSION_1))?1:0)
  {
    case NS_ZERO:
    cm_continue_with_phase1();
    break;

    default:
    cm_continue_with_phase2();
  }
  NSDL1_PARENT(NULL, NULL, "Method End.");
  return CM_SUCCESS;
}

//void cm_read_keywords(FILE* fp, int all_keywords)
//{

  /*NUM_CVM	<DEFAULT NUM CPUs>
DEBUG <ON/OFF> <LEVEL>
TARCE <ON/OFF> <LEVEL>
MAX_LOG_FILE_SIZE <FOR BOTH DEBUG AND TRACE>
MODULE_MASK
LIB_MODULEMASK*/
/*  int num;
  char *buf;
  char buff[MAX_MONITOR_BUFFER_SIZE+1];
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  int  line_num = 0;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  set_default_value_for_global_keywords ();
  while (fgets(buff, MAX_MONITOR_BUFFER_SIZE, fp) != NULL) 
  {
    line_num++;
    NSDL2_SCHEDULE(NULL, NULL, "buff = [%s], line_no = %d, len = %d", buff, line_num, strlen(buff));
    buff[strlen(buff)-1] = '\0';  //Removing new line
    
    buf = buff;

    CLEAR_WHITE_SPACE(buf);
    CLEAR_WHITE_SPACE_FROM_END(buf);
    
    NSDL2_SCHEDULE(NULL, NULL, "buf = [%s]", buf);
    //NSTL1_OUT(NULL, NULL,  "Buffer1 = %s \n", buf);
    if((buf[0] == '#') || (buf[0] == '\0'))
      continue;

    if ((num = sscanf(buf, "%s %s", keyword, text)) != 2) 
    {
    	printf("read_keywords(): At least two fields required  <%s>\n", buf);
	    continue;
    } 
    else 
    {
      NSDL3_SCHEDULE(NULL, NULL, "keyword = %s, text = %s", keyword, text);  
      if (all_keywords) 
      {

        if (strcasecmp(keyword, "SCHEDULE") == 0) {
          kw_set_schedule(buf, err_msg, 0);
        }
        else if (strcasecmp(keyword, "NH_SCENARIO") == 0)
        {
          kw_set_nh_scenario(buf, err_msg);
        } 
        else if (strcasecmp(keyword, "SERVER_STATS") == 0) {
	  //printf("Calling function 1 get_server_perf_stats\n");
	  get_server_perf_stats(buf); // Achint 03/01/2007 - Add this function to get all server IP Addresses
	} else if (strcasecmp(keyword, "SERVER_PERF_STATS") == 0) {
	  //printf("Calling function 2 get_server_perf_stats\n");
	  get_server_perf_stats(buf); // Achint 03/01/2007 - Add this function to get all server IP Addresses
        } else if (strcasecmp(keyword, "CUSTOM_MONITOR") == 0 || 
		   strcasecmp(keyword, "SPECIAL_MONITOR") == 0 || 
		   strcasecmp(keyword, "LOG_MONITOR") == 0) {

          //setting unique monitorid for custom monitors added. For DVM we are sperrately incrementing in convert_dvm_to_cm
          //if(custom_config(keyword, buf, NULL, 0, NORMAL_CM_TABLE, err_msg, NULL, 0, -1, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0) >= 0)
          if(custom_monitor_config(keyword, buf, NULL, 0, g_runtime_flag, err_msg, NULL, NULL, 0) >= 0)
          {
            if(strcasecmp(keyword, "LOG_MONITOR") != 0)
              g_mon_id = get_next_mon_id();
            monitor_added_on_runtime = 1;
          }

         else if (strncasecmp(keyword, "LIB_DEBUG", strlen("LIB_DEBUG")) == 0) {
          set_nslb_debug(buf);

      } else if (strcasecmp(keyword, "LIB_MODULEMASK") == 0) {
        if (set_nslb_modulemask(buf, err_msg) != 0)
          NS_EXIT(-1, "Invalid module mask supplied by user, error: %s", err_msg);

          char log_file[1024];
          char error_log_file[1024];

          sprintf(log_file, "%s/logs/TR%d/debug.log", g_ns_wdir, testidx);
          sprintf(error_log_file, "%s/logs/%s/error.log", g_ns_wdir, global_settings->tr_or_partition);
 
          nslb_util_set_log_filename(log_file, error_log_file); 
      }

      }
      }    }//End while loop
  }
 }
}

void cm_init_default_values(Global_data *global_settings)
{

  global_settings->num_process = DEFAULT_PROC_PER_CPU * cm_get_num_processor();
}*/

int cm_get_num_processor()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  char line_buf[MAX_LINE_LENGTH] = "\0";
  FILE* process_ptr = fopen("/proc/cpuinfo", "r");
  if (process_ptr) {
    while (fgets(line_buf, MAX_LINE_LENGTH, process_ptr)) {
        if (strncmp(line_buf, "processor", strlen("processor")) == 0)
            num_processor++;
    }
    CLOSE_FP(process_ptr);
  }
  if (num_processor <= 0) {
        NSTL1_OUT(NULL, NULL, "Unable to determine number of processors. Assuming 1 processor\n");
        NSDL2_MESSAGES(NULL, NULL,"Unable to determine number of processors. Hence, Setting it's value to 1 processor");
        num_processor = 1;
  }
  NSDL2_SCHEDULE(NULL, NULL, "returning..num_processor=%d", num_processor);
  return num_processor;
}

int kw_set_cavgen_version(char *buf, int *to_change, char *err_msg, int runtime_flag)
{   
  int default_version = 0;
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  
  if ((MINIMUM_ARGS > sscanf(buf, "%s %s", keyword, text))) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAVGEN_VERSION_USAGE, CAV_ERR_1011087, CAV_ERR_MSG_1);
  } 
  // Do validations on text
  if(ns_is_numeric(text))
  { 
    default_version = (atoi(text));
  }
  else
  { 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAVGEN_VERSION_USAGE, CAV_ERR_1011087, CAV_ERR_MSG_2);
  }
  
  if (default_version != 1)
  {
    *to_change = 2;
  }
  else
   *to_change = default_version;
  
  return CM_SUCCESS; 
}



int kw_set_file_upload_settings(char *buf, Global_data* global_settings, char *err_msg, int runtime_flag)
{


  char keyword[SGRP_NAME_MAX_LENGTH];
  char tmp[SGRP_NAME_MAX_LENGTH];

  char tp_init_size[6];
  char tp_max_size[6];

  char mq_init_size[6];
  char mq_max_size[6];

  char max_conn_retry[6];
  char retry_timer[6];

  int ret = 0;


  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  ret = sscanf(buf, "%s %s %s %s %s %s %s %s", keyword, tp_init_size, tp_max_size,  mq_init_size, mq_max_size, max_conn_retry, retry_timer, tmp);
  NSDL2_PARSING(NULL, NULL, "keyword = %s, tp_init_size = %s, tp_max_size = %s,  mq_init_size = %s, mq_max_size = %s, max_conn_retry = %s, retry_timer = %s", keyword, tp_init_size, tp_max_size,  mq_init_size, mq_max_size, max_conn_retry, retry_timer);

  if (ret < 2 || ret > 7)
  { 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, FILE_UPLOAD_SETTINGS_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_1);
  }


  global_settings->file_upload_info->tp_init_size = (unsigned short) atoi(tp_init_size); 
  /*if (nslb_atoi(tp_init_size, &(global_settings->file_upload_info->tp_init_size)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  global_settings->file_upload_info->tp_max_size = (unsigned short) atoi(tp_max_size); 
  /*if (nslb_atoi(tp_max_size, &(global_settings->file_upload_info->tp_max_size)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  global_settings->file_upload_info->mq_init_size = (unsigned short) atoi(mq_init_size); 
  /*if (nslb_atoi(mq_init_size, &(global_settings->file_upload_info->mq_init_size)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  global_settings->file_upload_info->mq_max_size = (unsigned short) atoi(mq_max_size); 
  /*if (nslb_atoi(mq_max_size, &(global_settings->file_upload_info->mq_max_size)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  global_settings->file_upload_info->max_conn_retry = (unsigned short) atoi(max_conn_retry); 
  /*if (nslb_atoi(max_conn_retry, &(global_settings->file_upload_info->max_conn_retry)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  global_settings->file_upload_info->retry_timer = (unsigned short) atoi(retry_timer); 
  /*if (nslb_atoi(retry_timer, &(global_settings->file_upload_info->retry_timer)) < 0)
  {
    //ERROR: Invalid Input 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }*/

  return 0;
}
