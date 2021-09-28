/* File Name: ns_njvm.c
 * Author: Shalu/Kamlesh
 * Purpose: parse java type script and start njvm
 **/

#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include <sys/un.h>

#include "ns_script_parse.h"
#include "util.h"
#include "tr069/src/ns_tr069_script_parse.h"
#include "ns_child_thread_util.h"
#include "ns_njvm.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_server_admin_utils.h"
#include "ns_trace_level.h"
#include "ns_monitor_profiles.h"
#include "nslb_cav_conf.h"
#include "wait_forever.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_ip_data.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_appliance_health_monitor.h"

#define MAX_HEAP_SIZE 12*1024
#define MAX_VAL_LENGTH 1024
#define NS_INIT_SCRIPT_FILENAME_JAVA "init_script.java"
#define NS_EXIT_SCRIPT_FILENAME_JAVA "exit_script.java"
#define LOOPBACK_IP "127.0.0.1"
#define JAR_NAME_LEN   35 * 1024
#define MAX_NUM_JARS       100

// Global variables for njvm
int njvm_total_free_thread = 0; // no of free thread  
int njvm_total_busy_thread = 0; // no of bsy thread
int njvm_num_ceased_thread = 0; // No of ceased thread 
int njvm_resize_req_done = 0; // Resize request is send or not
int njvm_total_accepted_thread = 0; // tolal thread connection accepted
int njvm_total_requested_thread = 0; // total thread connection requested

unsigned short njvm_listen_port = 0;
int njvm_control_con_fd;
int njvm_listen_fd;
int njvm_epoll_fd;
Msg_com_con njvm_msg_com_con_listen;
Msg_com_con njvm_msg_com_con_control;
//If busy thread list reached to this count then increase threads.
int njvm_num_threshold_thread = 0;

char listen_socket_address[256] = "";

// Thread free link list head
Msg_com_con *njvm_free_thread_pool = NULL;
//int g_rtc_msg_seq_num = 0;
extern int g_rtc_msg_seq_num;

static int create_jtmp_dir; //flag to ensure that .tmp/user/ns_instance/scripts directory will be created only once for whole test

static char script_libs_jars[JAR_NAME_LEN + 1];
 
void njvm_handler_ignore(int data)
{
}

// Method to get default java home path, called from ns_parse_scen_conf.c
void get_java_home_path()
{
  char *ptr;
  global_settings->njvm_settings.njvm_java_home = NULL;

  NSDL2_PARSING(NULL, NULL, "Method called");

  ptr = getenv("JAVA_HOME");
  if(ptr != NULL){
    MY_MALLOC(global_settings->njvm_settings.njvm_java_home, strlen(ptr) + 1, "njvm_java_home", -1);
    strcpy(global_settings->njvm_settings.njvm_java_home, ptr);
  }
}

// Method to create jar file from compiled java files, called fron url.c
void make_scripts_jar(){
  char cmd_buff[MAX_LINE_LENGTH];
  sighandler_t prev_handler;
  FILE *cp = NULL;

  NSDL2_PARSING(NULL, NULL, "Method called");
  sprintf(cmd_buff, "cd %s/scripts && jar -cf ../scripts.jar * && cd %s", g_ns_tmpdir, g_ns_wdir);
  NSDL2_PARSING(NULL, NULL, "command to create jar = %s", cmd_buff);
  prev_handler = signal(SIGCHLD, njvm_handler_ignore);
  cp = popen(cmd_buff, "r");
  if(cp == NULL){
    NS_EXIT(1, CAV_ERR_1000031, cmd_buff, errno, nslb_strerror(errno));
  }   
  pclose(cp);

  (void) signal( SIGCHLD, prev_handler);
}

/* Nisha(8 Feb 2018): 
      - Reading file rdt_sys.conf if script is of type RDT 
      - Get all RDT jars and set in classpath
*/

void get_rdt_type_jar_names(char *jar_classpath)
{
  char *jar_ptr = NULL;
  char abs_jar_path[MAX_VAL_LENGTH + 1] = "";
  char jar_names[JAR_NAME_LEN + 1] = "";
  char *fields[MAX_NUM_JARS];
  int num_field = 0;
  struct stat st;
  FILE *fp;
  int len,i;

  jar_ptr = jar_names;
  sprintf(jar_ptr, "%s/RDT/config/rdt_sys.conf", g_ns_wdir);
  NSTL1(NULL, NULL, "Going to read file %s to set CLASS_PATH for NJVM", jar_ptr);

  stat(jar_ptr, &st);

  if(S_ISREG(st.st_mode) && st.st_size)
  {
    if((fp = fopen(jar_ptr, "r")) != NULL)
    {
      while(nslb_fgets(jar_names, JAR_NAME_LEN, fp, 0) != NULL)
      {
        len = strlen(jar_names);

        //Handle Unix as well as DOS file format
        if(jar_names[len - 1] == '\n')
        {
          len--;
          if(jar_names[len - 1] == '\r')
            len--;

          jar_names[len] = '\0';
        }

        if((jar_ptr = strstr(jar_names, "RDT_JARS=")) != NULL)
        {
          jar_ptr += 9; // move ptr1 after RDT_JARS= ie 9 bytes
          num_field = get_tokens(jar_ptr, fields, ":", MAX_NUM_JARS);
          break;
        }
      }
      NSTL1(NULL, NULL, "Number of RDT jars = %d", num_field);
   
      // Make absolute jar path and append into classpath
      for(i = 0; i < num_field; i++)
      {
        sprintf(abs_jar_path, ":%s/webapps/netstorm/lib/%s", g_ns_wdir, fields[i]);
        strcat(jar_classpath, abs_jar_path);
      }

      fclose(fp);
    }
    else
    {
      NSTL1(NULL, NULL, "Error: unable to open file. errno = %d, strerror = %s", errno, nslb_strerror(errno));
    }
  }
  else
  {
    NSTL1(NULL, NULL, "Error: either file is not a regural file or its size is 0 and hence ignore to read file");
  }

  NSTL1(NULL, NULL, "RDT jar classpath = %s", jar_classpath);
}

static void copy_runlogic_files(int sess_idx, char *script_filepath)
{  
  int i;
  char tmp_buf[MAX_VAL_LENGTH + 1] = "\0";
  NSDL2_PARSING(NULL, NULL, "Method called script_filepath= %s",script_filepath);
 
  // check if script is old or new format by checking runlogic directory
  if (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
  {
    for(i = 0; i <total_runprof_entries; i++)
    {
      if( runProfTable[i].sessprof_idx == sess_idx)
      {
        sprintf(tmp_buf,"cp %s/runlogic/%s.java  %s/scripts_java/%s/runlogic", script_filepath, runProfTable[i].runlogic,
                        g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

        NSDL2_PARSING(NULL, NULL, "copy cmd=%s", tmp_buf);
        system(tmp_buf);
      }
    }
  }
}
// Creating classes from java files present in script
int create_classes_for_java_type_script( int sess_idx, char* script_filepath, char* flow_file, int file_count, FlowFileList_st *script_filelist){
  char tmp_buf[MAX_LINE_LENGTH];
  char *cp_buf  = tmp_buf;
  char class_path[2*MAX_LINE_LENGTH];
  int return_value;
  int i;
  int amt_wrt = 0;
  char jar_classpath[JAR_NAME_LEN + 1] = "";
  char file_name[1024];
  //char temp_jar_buf[MAX_LINE_LENGTH];
  FILE *fp;  
  char *ptr;
  char *jar_ptr;
  int widx = 0, wbytes = 0;
  static int wjaridx = 0;
  int len = 0;
  char package_path[1024];
  struct stat st;


  /*************************************START*************************************/
  //Read user defined script jar file in .script.libs
  sprintf(file_name, "%s/%s/%s/.script.libs", g_ns_wdir, GET_NS_RTA_DIR(),
                      get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));

  NSDL2_PARSING(NULL, NULL, "Method called %s", file_name);

  fp = fopen(file_name, "r");
  if(fp != NULL)
  {
    //Read file that contains jar files i.e file_name
    while(nslb_fgets(tmp_buf, MAX_LINE_LENGTH, fp, 0) != NULL)
    {
      //TODO: need to handle DOS format i.e. \r\n
      if((tmp_buf[0] == '#') || tmp_buf[0] == '\0' || tmp_buf[0] == '\n') //Skip commented and Blank line
        continue;
     
      len = strlen(tmp_buf);

      if(tmp_buf[len - 1] == '\n')
        tmp_buf[len - 1] = '\0';  // Replace new line by Null

      ptr = tmp_buf;
  
      //Tokenise tmp_buf and make absolute path for jar files
      while((jar_ptr = strtok(ptr, ";")) != NULL)
      {
        if(wjaridx < JAR_NAME_LEN)
        {
          wbytes = snprintf(script_libs_jars + wjaridx, JAR_NAME_LEN - wjaridx, "/home/cavisson/thirdparty/.script_lib/%s:", jar_ptr);
          wjaridx += wbytes;
          widx = wjaridx;         //Using widx for checking classpath size
          ptr = NULL;
        }
        else
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012012_ID, CAV_ERR_1012012_MSG, wjaridx, JAR_NAME_LEN);
        }
      }
    }

    NSTL1(NULL, NULL, "Content of jar files in script libs = %s", script_libs_jars);

    /*if(wjaridx)
    {
      script_libs_jars[widx - 1] = 0;  //Removing ":" from last jar file
      NSTL1(NULL, NULL, "Content of jar files in script libs = %s", script_libs_jars);
    }*/
  }
  else
    NSTL1(NULL, NULL, "User defined jar file %s not found", file_name);
  
  /*************************************END****************************************/


  if(!create_jtmp_dir)
  {
    sprintf(tmp_buf, "mkdir -p %s/scripts", g_ns_tmpdir);
    NSDL2_PARSING(NULL, NULL, "mkdir cmd=%s", tmp_buf);
    system(tmp_buf);
    create_jtmp_dir = 1;
  }
 
  //to remove previously copied java type scripts
  // BUG ID : 73999 | While we are creating flow inside a java type script and compiling it then it is giving compilation error  
  sprintf(tmp_buf, "rm -rf %s/scripts_java/*",g_ns_tmpdir);
  NSDL2_PARSING(NULL, NULL, "removing old java type scripts  cmd=%s", tmp_buf);
  system(tmp_buf);
 
  if (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
    sprintf(tmp_buf,"mkdir -p %s/scripts_java/%s/runlogic", g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  else
    sprintf(tmp_buf,"mkdir -p %s/scripts_java/%s", g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  NSDL2_PARSING(NULL, NULL, "mkdir cmd=%s", tmp_buf);
  system(tmp_buf);

  sprintf(tmp_buf,"cp %s/*.java  %s/scripts_java/%s", script_filepath, g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  NSDL2_PARSING(NULL, NULL, "copy cmd=%s", tmp_buf);
  system(tmp_buf);

  //Copy all runlogic java files
  copy_runlogic_files(sess_idx, script_filepath);
  //save runlogic on gset
  runprof_save_runlogic(sess_idx, NULL);

  //copy all temprory flow file
  amt_wrt = sprintf(cp_buf, "cd %s; cp", g_ns_tmpdir); 
  cp_buf = cp_buf + amt_wrt;

  for(i = 0; i < file_count; i++){
    amt_wrt = sprintf(cp_buf, " %s", script_filelist[i].orig_filename);
    cp_buf = cp_buf + amt_wrt;
  }

  amt_wrt = sprintf(cp_buf, " scripts_java/%s ;cd %s",  RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), g_ns_wdir);
  
  NSDL2_PARSING(NULL, NULL, "tmp_copy_cmd = %s", tmp_buf);
  system(tmp_buf);

  //Set class path
  sprintf(class_path, "%s/webapps/netstorm/lib/jnvmApi.jar", g_ns_wdir);

  /*Setting classpath for RDT type during script compilation*/
  sprintf(jar_classpath,":%s/RDT/lib/*",g_ns_wdir);
  strcat(class_path, jar_classpath);

  NSDL2_PARSING(NULL, NULL, "Classpath after appending rdt type jars = %s", class_path);  

  if(global_settings->njvm_settings.njvm_class_path) {
    sprintf(class_path, "%s:%s", class_path, global_settings->njvm_settings.njvm_class_path);
  } 
  if(global_settings->njvm_settings.njvm_system_class_path){
    sprintf(class_path, "%s:%s", class_path, global_settings->njvm_settings.njvm_system_class_path); 
  }

  widx += strlen(class_path);
  if(widx >= (2*MAX_LINE_LENGTH))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012013_ID, CAV_ERR_1012013_MSG, widx, (2*MAX_LINE_LENGTH));
  }

  //Setting classpath for .script.libs jar files during script compilation.
  if(script_libs_jars[0] != '\0')
    sprintf(class_path, "%s:%s", class_path, script_libs_jars);

  NSDL2_PARSING(NULL, NULL, "Classpath = %s", class_path);

  if (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
    sprintf(tmp_buf, "javac -cp %s -d %s/scripts %s/scripts_java/%s/*.java  %s/scripts_java/%s/runlogic/*.java",
                      class_path, g_ns_tmpdir, g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
                      g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  else
    sprintf(tmp_buf, "javac -cp %s -d %s/scripts %s/scripts_java/%s/*.java",
                      class_path, g_ns_tmpdir, g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

  NSDL2_PARSING(NULL, NULL, "java compile cmd=%s", tmp_buf);

  return_value = system(tmp_buf);
  
  if (WEXITSTATUS(return_value) == 1)
  {
    NSDL2_PARSING(NULL, NULL, "Error in compiling the script files");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012014_ID, CAV_ERR_1012014_MSG);
  }
  else if (WEXITSTATUS(return_value) == 0 && (run_mode_option & RUN_MODE_OPTION_COMPILE))
  {
    NS_EXIT(0, "Successfully compiled script files");
  }

  sprintf(package_path, "%s/scripts/%s", g_ns_tmpdir, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
  if((!stat(package_path, &st)) && (S_ISDIR(st.st_mode)))
    gSessionTable[sess_idx].flags |= ST_FLAGS_SCRIPT_NEW_JAVA_PKG;
  else
    gSessionTable[sess_idx].flags |= ST_FLAGS_SCRIPT_OLD_JAVA_PKG;
  
  NSDL2_PARSING(NULL, NULL, "WEXITSTATUS(return_value) = %d", WEXITSTATUS(return_value));
  return NS_PARSE_SCRIPT_SUCCESS;
}


/********************keyword parsing methods start *******************************/
 

int  kw_set_njvm_system_class_path(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[1024];
  char tmp_data[MAX_LINE_LENGTH];
  char tmp[1024];
  int num;
  int len = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  if (num != 2){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_SYSTEM_CLASS_PATH_USAGE, CAV_ERR_1011125, CAV_ERR_MSG_1);
  }

  if(tmp_data == NULL) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_SYSTEM_CLASS_PATH_USAGE, CAV_ERR_1011125, CAV_ERR_MSG_1);
  }

  if(*tmp_data != '/')
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_SYSTEM_CLASS_PATH_USAGE, CAV_ERR_1011226, "");
   
  len = strlen(tmp_data);
  MY_MALLOC(global_settings->njvm_settings.njvm_system_class_path, len + 1, "global_settings->njvm_settings.njvm_system_class_path", -1);
  strncpy(global_settings->njvm_settings.njvm_system_class_path, tmp_data, len); 

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_system_class_path = %s", global_settings->njvm_settings.njvm_system_class_path);
  return 0;
}


int  kw_set_njvm_class_path(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[1024];
  char tmp_data[MAX_LINE_LENGTH];
  char tmp[1024];
  int num;
  int len = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  if (num != 2){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_CLASS_PATH_USAGE, CAV_ERR_1011127, CAV_ERR_MSG_1);
  }

  if(tmp_data == NULL) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_CLASS_PATH_USAGE, CAV_ERR_1011127, CAV_ERR_MSG_1);
  }

  len = strlen(tmp_data);

  MY_MALLOC(global_settings->njvm_settings.njvm_class_path, len + 1, "global_settings->njvm_settings.njvm_class_path", -1);
  strncpy(global_settings->njvm_settings.njvm_class_path, tmp_data, len);
  global_settings->njvm_settings.njvm_class_path[len] = '\0';

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_class_path = %s", global_settings->njvm_settings.njvm_class_path);
  return 0;
}


int  kw_set_njvm_java_home_path(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[1024];
  char tmp_data[1024];
  char tmp[1024];
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  if (num != 2){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_JAVA_HOME_USAGE, CAV_ERR_1011126, CAV_ERR_MSG_1);
  }

  if(tmp_data == NULL) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_JAVA_HOME_USAGE, CAV_ERR_1011126, CAV_ERR_MSG_1);
  }

  MY_MALLOC(global_settings->njvm_settings.njvm_java_home, strlen(tmp_data) + 1, "njvm_java_home_path", -1);
  strcpy(global_settings->njvm_settings.njvm_java_home, tmp_data);

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_java_home_path = %s", global_settings->njvm_settings.njvm_java_home);
  return 0;
}


//Changed to global variable.
int kw_set_njvm_thread_pool(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_VAL_LENGTH]; // Neeraj: Do not initialize as there is not need, otherwise there are performance issues
  char init_size[MAX_VAL_LENGTH];
  char incremental_size[MAX_VAL_LENGTH];
  char threshold_pct[MAX_VAL_LENGTH];
  char max_size[MAX_VAL_LENGTH];
  char tmp_buf[MAX_VAL_LENGTH];
  char *val;
  int num;
  NSDL3_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %s %s %s %s", keyword, init_size, incremental_size, max_size, threshold_pct, tmp_buf)) != 5) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_1);
  }

  val = init_size;
  CLEAR_WHITE_SPACE(val);

  if(ns_is_numeric(val) == 0)
   NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_2);

  num = atoi(val);

  if(num <= 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_9);

  global_settings->njvm_settings.njvm_init_thrd_pool_size = num;
  NSDL3_PARSING(NULL, NULL, "Setting global_settings->njvm_settings.njvm_init_thrd_pool_size = %d", global_settings->njvm_settings.njvm_init_thrd_pool_size);

  val = incremental_size;
  CLEAR_WHITE_SPACE(val);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_2);

  num = atoi(val);

  if(num <= 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_9);

  global_settings->njvm_settings.njvm_increment_thrd_pool_size = num;

  val = max_size;
  CLEAR_WHITE_SPACE(val);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_2);

  num = atoi(val);

  if(num <= 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_9);

  global_settings->njvm_settings.njvm_max_thrd_pool_size = num;

  val = threshold_pct;
  CLEAR_WHITE_SPACE(val);
 
  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_2);

  num = atoi(val);

  if(num <= 0 || num > 100)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_VUSER_THREAD_POOL_USAGE, CAV_ERR_1011128, CAV_ERR_MSG_6);

  global_settings->njvm_settings.njvm_thrd_threshold_pct = num;
   
#if 0
  if(global_settings->njvm_settings.njvm_init_thrd_pool_size > global_settings->njvm_settings.njvm_max_thrd_pool_size)
    njvm_thread_pool_usage("Init thread can't more the max thread");

  if(global_settings->njvm_settings.njvm_init_thrd_pool_size < global_settings->njvm_settings.njvm_max_thrd_pool_size
     &&
     global_settings->njvm_settings.njvm_increment_thrd_pool_size == 0)
    njvm_thread_pool_usage("Incremental thread can't 0, because max thread is greater than init thread");
#endif

  if(global_settings->njvm_settings.njvm_max_thrd_pool_size < (global_settings->njvm_settings.njvm_init_thrd_pool_size + global_settings->njvm_settings.njvm_increment_thrd_pool_size)) {
    global_settings->njvm_settings.njvm_max_thrd_pool_size = global_settings->njvm_settings.njvm_init_thrd_pool_size + global_settings->njvm_settings.njvm_increment_thrd_pool_size;

    NSTL1(NULL, NULL, "Warning: Max thread is less then sum of init and incremantal thread so setting it to init+incremental. global_settings->njvm_settings.njvm_max_thrd_pool_size = %d", global_settings->njvm_settings.njvm_max_thrd_pool_size);
    NS_DUMP_WARNING("Max thread is less then sum of init and incremantal thread so setting it to init+incremental. global_settings->njvm_settings.njvm_max_thrd_pool_size = %d", global_settings->njvm_settings.njvm_max_thrd_pool_size);
  
  }

  NSDL3_PARSING(NULL, NULL, "Method Exiting, init_thread = %d, incremental_thread = %d, max_thread = %d, threshold_pct = %d",
                              global_settings->njvm_settings.njvm_init_thrd_pool_size, 
                              global_settings->njvm_settings.njvm_increment_thrd_pool_size,
                              global_settings->njvm_settings.njvm_max_thrd_pool_size,
                              global_settings->njvm_settings.njvm_thrd_threshold_pct);

  return 0;
}

#define MAX_VAL_LENGTH 1024


int kw_set_njvm_std_args(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_VAL_LENGTH] = "\0";
  char njvm_min_heap_size[MAX_VAL_LENGTH] = "\0";
  char njvm_max_heap_size[MAX_VAL_LENGTH] = "\0";
  int njvm_gc_logging_mode = 0;
  char tmp_buf[MAX_VAL_LENGTH] = "\0";
  char *val;
  int num = 0;
  int num_fields;
  NSDL3_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  num_fields = sscanf(buf, "%s %s %s %d %s", keyword, njvm_min_heap_size, njvm_max_heap_size, &njvm_gc_logging_mode, tmp_buf);

  NSDL3_PARSING(NULL, NULL, "num_fields = %d", num_fields);
  if(num_fields < 3 || num_fields > 4){
    NS_KW_PARSING_ERR(buf, runtime_flag , err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011122, CAV_ERR_MSG_1);
  }

  // Validate and set minimum heap size
  val = njvm_min_heap_size;
  CLEAR_WHITE_SPACE(val);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011122, CAV_ERR_MSG_2);

  num = atoi(val);
  if(num < 2)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011223, "");
  
  global_settings->njvm_settings.njvm_min_heap_size = num;
  NSDL3_PARSING(NULL, NULL, "Setting global_settings->njvm_settings.njvm_min_heap_size = %d", global_settings->njvm_settings.njvm_min_heap_size);

  // Validate and set max heap size
  val = njvm_max_heap_size;
  CLEAR_WHITE_SPACE(val);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011122, CAV_ERR_MSG_2);

  num = atoi(val);

  if(num > MAX_HEAP_SIZE)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011123, "");

  global_settings->njvm_settings.njvm_max_heap_size = num;

  if(global_settings->njvm_settings.njvm_min_heap_size > global_settings->njvm_settings.njvm_max_heap_size)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011122, CAV_ERR_MSG_5);

  if(num_fields == 4){
     NSDL3_PARSING(NULL, NULL, "gc_logmode = %d", njvm_gc_logging_mode);

  if(njvm_gc_logging_mode != 0 && njvm_gc_logging_mode != 1)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_STD_ARGS_USAGE, CAV_ERR_1011122, CAV_ERR_MSG_3);

    global_settings->njvm_settings.njvm_gc_logging_mode = (char)njvm_gc_logging_mode; 
  }

  NSDL3_PARSING(NULL, NULL, "Method Exiting, min_heap_size = %d, max_heap_size = %d, njvm_gc_logging_mode = %d"
                            , global_settings->njvm_settings.njvm_min_heap_size, global_settings->njvm_settings.njvm_max_heap_size
                            , global_settings->njvm_settings.njvm_gc_logging_mode);

  return 0;
}


int  kw_set_njvm_custom_args(char *buf, char *err_msg, int runtime_flag)
{
  char* tmp_ptr;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  tmp_ptr = strchr(buf, ' ');
  if(tmp_ptr == NULL)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, NJVM_CUSTOM_ARGS_USAGE, CAV_ERR_1011124, CAV_ERR_MSG_1);
  }

  CLEAR_WHITE_SPACE(tmp_ptr);
  CLEAR_WHITE_SPACE_FROM_END(tmp_ptr);

  MY_MALLOC(global_settings->njvm_settings.njvm_custom_config, strlen(tmp_ptr) + 1, "njvm_custom_config", -1);
  strcpy(global_settings->njvm_settings.njvm_custom_config, tmp_ptr);

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_custom_config = %s", global_settings->njvm_settings.njvm_custom_config);
  return 0;
}


int  kw_set_njvm_conn_timeout(char *buf, unsigned long* global_set, char *err_msg, int runtime_flag)
{
  char keyword[1024];
  char tmp_data[1024];
  char tmp[1024];
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);
  NSDL2_PARSING(NULL, NULL, "num =%d", num);
  if (num != 2){
    NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_CONN_TIMEOUT_USAGE, CAV_ERR_1011130, CAV_ERR_MSG_1);
  }

  if(tmp_data == NULL) {
     NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_CONN_TIMEOUT_USAGE, CAV_ERR_1011130, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(tmp_data) == 0) {
   NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_CONN_TIMEOUT_USAGE, CAV_ERR_1011130, CAV_ERR_MSG_2);
  }
 
  *global_set = atol(tmp_data);

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_conn_timeout = %lu", global_settings->njvm_settings.njvm_conn_timeout);
  return 0;
}

// Function to set njvm simulator mode
static void  kw_set_njvm_simulator_mode_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of NJVM_SIMULATOR_MODE keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: NJVM_SIMULATOR_MODE <mode> <flow-file>\n");
  NSTL1_OUT(NULL, NULL, "  Where:\n");
  NSTL1_OUT(NULL, NULL, "         mode : mode can be 0, 1, 2\n");
  NSTL1_OUT(NULL, NULL, "         flow file : NA or c file should have a function named flow\n");
  NS_EXIT(-1, "%s\nUsage: NJVM_SIMULATOR_MODE <mode> <flow-file>", err_msg);
}

int  kw_set_njvm_simulator_mode(char *buf)
{
  char keyword[1024];
  char mode_str[1024];
  char flow_file[1024];
  int mode;
  char tmp[1024];
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, mode_str, flow_file, tmp);
  NSDL2_PARSING(NULL, NULL, "num =%d", num);
  if (num != 3){
     kw_set_njvm_simulator_mode_usage("Invaid number of arguments", buf);
  }

  if(ns_is_numeric(mode_str) == 0) {
    kw_set_njvm_simulator_mode_usage("Invalid mode given", buf);
  }
 
  mode = atoi(mode_str);

  if(mode < 0 && mode > 2) {
    kw_set_njvm_simulator_mode_usage("Invalid mode given", buf);
  }
  
  global_settings->njvm_settings.njvm_simulator_mode = mode;
  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_simulator_mode = %d", mode);
 
  if(strcasecmp(flow_file, "NA")) {
    MY_MALLOC(global_settings->njvm_settings.njvm_simulator_flow_file, strlen(flow_file + 1), "global_settings->njvm_simulator_flow_file", -1);
    strcpy(global_settings->njvm_settings.njvm_simulator_flow_file, flow_file);
    NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_simulator_flow_file = %s", global_settings->njvm_settings.njvm_simulator_flow_file);
  }
  return 0;
}


// Function to set njvm connection mode
static void  kw_set_njvm_con_mode_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of NJVM_CON_MODE keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: NJVM_CON_MODE <mode>\n");
  NSTL1_OUT(NULL, NULL, "  Where:\n");
  NSTL1_OUT(NULL, NULL, "         mode : mode can be 0(tcp socket), 1(unix domain socket)\n");
  NS_EXIT(-1, "%s\nUsage: NJVM_CON_MODE <mode>", err_msg);
}

int  kw_set_njvm_con_mode(char *buf, char* global_set)
{
  char keyword[1024];
  char tmp_data[1024];
  char tmp[1024];
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);
  NSDL2_PARSING(NULL, NULL, "num =%d", num);
  if (num != 2){
     kw_set_njvm_con_mode_usage("Invaid number of arguments", buf);
  }

  if(tmp_data == NULL) {
     kw_set_njvm_con_mode_usage("mode is not given", buf);
  }

  if(ns_is_numeric(tmp_data) == 0) {
    kw_set_njvm_con_mode_usage("Invalid mode given", buf);
  }
 
  *global_set = atoi(tmp_data);

  if(*global_set < 0 && *global_set > 1) {
    kw_set_njvm_con_mode_usage("Invalid mode given", buf);
  }

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_con_type = %d", *global_set);
  return 0;
}


int  kw_set_njvm_msg_timeout(char *buf, unsigned long* global_set, char *err_msg, int runtime_flag)
{
  char keyword[1024];
  char tmp_data[1024];
  char tmp[1024];
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  NSDL2_PARSING(NULL, NULL, "num =%d", num);
  if (num != 2){
    NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_MESSAGE_TIMEOUT_USAGE, CAV_ERR_1011129, CAV_ERR_MSG_1);
  }

  if(tmp_data == NULL) {
    NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_MESSAGE_TIMEOUT_USAGE, CAV_ERR_1011129, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(tmp_data) == 0) {
   NS_KW_PARSING_ERR(buf,runtime_flag, err_msg, NJVM_MESSAGE_TIMEOUT_USAGE, CAV_ERR_1011129, CAV_ERR_MSG_2);
  }
 
  *global_set = atol(tmp_data);

  NSDL2_PARSING(NULL, NULL, "global_settings->njvm_settings.njvm_msg_timeout = %lu", global_settings->njvm_settings.njvm_msg_timeout);
  return 0;
}
/********************keyword parsing methods end *******************************/


int njvm_is_debug_on(){
  int i, debug_level = 0, debug_hex = 0x0;
  for(i = 0; i < total_runprof_entries; i++){
    NSDL4_NJVM(NULL, NULL, "Method called. debug_level = %8x, modulemask = %8x", runprof_table_shr_mem[i].gset.debug, runprof_table_shr_mem[i].gset.module_mask & MM_NJVM);
   if(runprof_table_shr_mem[i].gset.debug && (runprof_table_shr_mem[i].gset.module_mask & MM_NJVM || runprof_table_shr_mem[i].gset.module_mask & MM_ALL) )
     debug_hex |= runprof_table_shr_mem[i].gset.debug; 
  }
  if(debug_hex == 0x000000FF)
    debug_level = 1;
  else if(debug_hex == 0x0000FFFF)
    debug_level = 2;
  else if(debug_hex == 0x00FFFFFF)
    debug_level = 3;
  else if(debug_hex == 0xFFFFFFFF)
    debug_level = 4;

  NSDL2_NJVM(NULL, NULL, "debug_level = %d, total_runprof_entries = %d", debug_level, total_runprof_entries);
  return debug_level;    
}


int njvm_is_trace_on(){

  int i, trace_level;
  for(i = 0; i < total_runprof_entries; i++){
    NSDL4_NJVM(NULL, NULL, "Method called. debug_level = %d", runprof_table_shr_mem[i].gset.vuser_trace_enabled);

    if(trace_level < runprof_table_shr_mem[i].gset.vuser_trace_enabled)
      trace_level = runprof_table_shr_mem[i].gset.vuser_trace_enabled;
  }
  NSDL2_NJVM(NULL, NULL, "trace_level = %d, total_runprof_entries = %d", trace_level, total_runprof_entries);
  return trace_level;
}

// This method will start njvm and will be called by nvms whih have java type script running
// njvm will run by java specified by user in keyword JAVA_HOME, if this keyword is not present then java will be taken from env variable 
// Parameter to passes to njvm are:
// -t testidx
// -u user name (which user is running the test)
// -n nvm_id
// system class path (optional)
// scenario classpath (optional)
// standarad args: min heap, max heap size, gc logging file
// Custom args: all
// -p <init:inc:max> thread pool args: initial, incremental, max
// -i ip:port /listner port
// -d debug_level
// $NS_WDIR/logs/TRXXXX/ns_logs/njvm/gc_<nvm_id>.log
static inline void start_njvm(){

  static char cmd_buf[10 * 1024] = ""; 
  static char classpath[JAR_NAME_LEN + 1] = ""; 
  static char java_args[5 * 1024] = "";
  char jar_classpath[JAR_NAME_LEN + 1] = "";
  int debug_level = 0;
  int trace_level = 0;
  FILE *fp = NULL;
  sighandler_t prev_handler;
  /* For njvm process id */
  char njvm_pids[50] = {0};
  int njvm_pid = 0;
  char err_msg[1024] = {0};

  NSDL2_NJVM(NULL, NULL, "Method called. my_port_index = %d, script_libs_jars = %s", my_port_index, script_libs_jars[0]?script_libs_jars:"");
  
  //add script jar and other needed jars path as class path. 
  sprintf(classpath, "-cp %s/scripts.jar:%s/webapps/netstorm/lib/jnvm.jar:"
                     "%s/webapps/netstorm/lib/jnvmApi.jar:"
                     "%s/webapps/netstorm/lib/protobuf-java-3.13.0.jar:"
                     "%s/webapps/netstorm/WEB-INF/lib/netstorm_bean.jar:"
                     "%s/webapps/netstorm/WEB-INF/lib/java-getopt-1.0.9.jar:%s",
                     g_ns_tmpdir, g_ns_wdir, g_ns_wdir, g_ns_wdir, g_ns_wdir, g_ns_wdir, script_libs_jars[0]?script_libs_jars:"");

//Appending jar path form rdt_sys.conf file for RDT type into classpath
  if(g_script_or_url)
  {
    //Changing rdt jar path
    sprintf(jar_classpath,"%s/RDT/lib/*",g_ns_wdir);
    strcat(classpath, jar_classpath);
  }  

  // add addition class path
  if(global_settings->njvm_settings.njvm_system_class_path != NULL)
    sprintf(classpath, "%s:%s", classpath, global_settings->njvm_settings.njvm_system_class_path);

  if(global_settings->njvm_settings.njvm_class_path != NULL)
    sprintf(classpath, "%s:%s", classpath, global_settings->njvm_settings.njvm_class_path);
 
  NSDL2_NJVM(NULL, NULL, "njvm classpath  = %s", classpath);

  // if gc logging is enable, pass gc logging file to njvm 
  if(global_settings->njvm_settings.njvm_gc_logging_mode >= 0)
    sprintf(java_args, "-Xloggc:%s/logs/%s/ns_logs/njvm/gc_%d.log", g_ns_wdir, global_settings->tr_or_common_files, my_port_index); 


  if(global_settings->njvm_settings.njvm_custom_config != NULL)
    sprintf(java_args, "%s %s", java_args, global_settings->njvm_settings.njvm_custom_config);

  // Module mask and debug level
  /*if(!((gset->debug == 0) || ((gset->module_mask & MM_NJVM) == 0)))
    debug_level = gset->debug;   */

  debug_level = njvm_is_debug_on();
 
  //trace_level = njvm_is_trace_on();
  if(global_settings->ns_trace_level == 0x000000FF)
    trace_level = 1; 
  else if(global_settings->ns_trace_level == 0x0000FFFF)
    trace_level = 2;

//redirect output of java process to a file njvm_<nvm-id>.out
  char out_redirect_file[512];
  struct stat st;
  // Change for Bug 31613: Here we are redirect pacJNVM.JavaNVM output to netstorm.netstorm.log which will be moved to TestrunOutput.log
  // by nsu_start_test, do not change this file name as this is used by nsu_start_test.
  //sprintf(out_redirect_file, "%s/webapps/logs/%s/ns_logs/jnvm_%d.out", g_ns_wdir, global_settings->tr_or_common_files, my_port_index);
  sprintf(out_redirect_file, "%s/logs/TR%d/TestRunOutput.log", g_ns_wdir, testidx);
  if(stat(out_redirect_file, &st))
    sprintf(out_redirect_file, "%s/webapps/netstorm/logs/%s.netstorm.log", g_ns_wdir,g_test_user_name);
  //Check tool type.
  if(!global_settings->njvm_settings.njvm_simulator_mode) {
    sprintf(cmd_buf, "%s/bin/java -Djava.library.path=%s/bin -DNS_WDIR=%s %s -Xms%dm -Xmx%dm %s pacJNVM.JavaNVM  -i %s:%d -t %d" 
      " -d %d:%d -u %s -n %d:%d -p %d:%d:%d -x %d 1>>%s 2>&1 < /dev/null & echo -n $!",
      global_settings->njvm_settings.njvm_java_home, g_ns_wdir, g_ns_wdir, classpath, global_settings->njvm_settings.njvm_min_heap_size, 
       global_settings->njvm_settings.njvm_max_heap_size, java_args, njvm_listen_port?"127.0.0.1":listen_socket_address, 
       njvm_listen_port, testidx, debug_level, trace_level, g_ns_login_user, my_port_index, getpid(), 
       global_settings->njvm_settings.njvm_init_thrd_pool_size, 
       global_settings->njvm_settings.njvm_increment_thrd_pool_size, global_settings->njvm_settings.njvm_max_thrd_pool_size,
       global_settings->num_process, out_redirect_file); 
  }
  else {
    //for java simulator
    printf("Running with njvm simulator\n");
#ifndef NS_DEBUG_ON
    sprintf(cmd_buf, "%s/bin/njvm_simulator_tool -i %s -p %d -t %d -n %d -m %d -s %s -N %d &", getenv("NS_WDIR"), 
             (*listen_socket_address)?listen_socket_address:"127.0.0.1", njvm_listen_port,
             testidx, global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->njvm_settings.njvm_simulator_mode,
             global_settings->njvm_settings.njvm_simulator_flow_file?global_settings->njvm_settings.njvm_simulator_flow_file:"NA", my_port_index);
#else 
    sprintf(cmd_buf, "%s/bin/njvm_simulator_tool -i %s -p %d -t %d -d -n %d -m %d -s %s -N %d &", getenv("NS_WDIR"), 
          (*listen_socket_address)?listen_socket_address:"127.0.0.1", njvm_listen_port,
          testidx, global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->njvm_settings.njvm_simulator_mode,
          global_settings->njvm_settings.njvm_simulator_flow_file?global_settings->njvm_settings.njvm_simulator_flow_file:"NA", my_port_index);
#endif
  }
  NSTL1(NULL, NULL, "Njvm started for nvm %d. Command = %s", my_port_index, cmd_buf);
  // set njvm_total_requested_thread, it will be used in resize thread pool
  njvm_total_requested_thread = global_settings->njvm_settings.njvm_init_thrd_pool_size;
  NSDL2_NJVM(NULL, NULL, "cmd_buf = %s", cmd_buf);

  prev_handler = signal(SIGCHLD, njvm_handler_ignore);
  fp = popen(cmd_buf, "r");
  if(fp == NULL)
  {
    NSDL2_NJVM(NULL, NULL, "failed to start njvm");
    NSTL1_OUT(NULL, NULL, "failed to start njvm. Error = %s.\n", nslb_strerror(errno));
    end_test_run();
  }

  /* Get the njvm id */
  nslb_fgets(njvm_pids, 50, fp, 0);
  njvm_pid = atoi(njvm_pids);
 
  pclose(fp);

  sprintf(njvm_pids,"CJVM%d.pid",my_port_index+1);
  njvm_pid = nslb_write_process_pid(njvm_pid,"ns njvm's pid" ,g_ns_wdir, testidx, "a",njvm_pids,err_msg);
  if(njvm_pid == -1)
  {
    NSTL1_OUT(NULL, NULL, "failed to open the CJVM%d pid file","%s",my_port_index,err_msg);
  }


  NSDL2_NJVM(NULL, NULL, "cmd sucessfully executed.");
  (void) signal( SIGCHLD, prev_handler);  
} 

void add_jts_auto_monitors(char* server, char *vector, int nvm_id){
 
  char buffer[4096] = "\0"; 
  char* vector_name = vector;
  char err_msg[MAX_DATA_LINE_LENGTH + 1] = "\0";

  vector += strlen(vector);
  sprintf(vector, "%cNJVM_%d", global_settings->hierarchical_view_vector_separator, nvm_id); 

  if(global_settings->njvm_settings.njvm_gc_logging_mode){
      sprintf(buffer, "STANDARD_MONITOR %s %s  JavaGCExtended -f %s/logs/%s/ns_logs/njvm/gc_%d.log", server, vector_name, g_ns_wdir, global_settings->tr_or_common_files, nvm_id);
      kw_set_standard_monitor("STANDARD_MONITOR", buffer, 0, NULL, err_msg, NULL); 
    }
}

inline void njvm_add_auto_monitors(){

  int java_script_found = 0;
  char err_msg[1024] = "\0";
  char vector_prefix[1024] = "\0";
  char tier_server[4*MAX_NAME_LEN + 1] = {0};
  int i,j;

  //we are not using is_java_type_script because at that time shared memory is not created.
  NSDL2_NJVM(NULL, NULL, "Method called. my_port_index = %d", my_port_index);
  for(i = 0; i < total_sess_entries; i++) {
    //Check if anyscript is java type.
    if(gSessionTable[i].script_type == NS_SCRIPT_TYPE_JAVA) {
      java_script_found = 1;
      break;
    }
  }  

  if(!java_script_found) {
    return;
  }

  snprintf(tier_server, 2*MAX_NAME_LEN, "%s%c%s", "Cavisson", global_settings->hierarchical_view_vector_separator, g_machine);
  //add '127.0.0.1' in server.dat
  //topolib_fill_server_for_njvm_add_auto_monitors(LOOPBACK_IP, tier_server,topo_idx); 
  add_server_in_server_list(LOOPBACK_IP, tier_server,topo_idx);

//  global_settings->hierarchical_view == 1?(vector_separator = global_settings->hierarchical_view_vector_separator): (vector_separator = '_');

  for(i = 0; i< global_settings->num_process; i++) {
    if(loader_opcode == STAND_ALONE) //Netstorm standalone mode
    {
      if(topolib_generate_vector_auto_monitor(IS_NS, NULL, g_cavinfo.config, vector_prefix, global_settings->hierarchical_view_vector_separator, err_msg) == -1)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
      add_jts_auto_monitors(LOOPBACK_IP, vector_prefix, i);

    }else{   //NETCLOUD

     //CONTROLLER MODE
     if(topolib_generate_vector_auto_monitor(IS_CONTROLLER, NULL, NULL, vector_prefix, global_settings->hierarchical_view_vector_separator, err_msg) == -1)
     {
         NS_EXIT(-1, "%s", err_msg);
      }
     add_jts_auto_monitors(LOOPBACK_IP, vector_prefix, i);

     //GENERATOR MODE
      for(j = 0; j < sgrp_used_genrator_entries; j++){
                        
        if(topolib_generate_vector_auto_monitor(IS_GENERATOR, (char *)generator_entry[j].gen_name, NULL, vector_prefix, global_settings->hierarchical_view_vector_separator, err_msg) == -1)
        {
           NS_EXIT(-1, "%s", err_msg);
        }
        add_jts_auto_monitors(generator_entry[j].IP, vector_prefix, i);
      } 
    }
  }
}

// This method check nvm idtribution and if  mode is 1 or 2 then checks if any sripts assgined to this grp is java type or not  
int is_java_type_script()
{
  int i;
  int java_type = 0;

  for(i = 0; i < total_sess_entries; i++)
  {
    if(session_table_shr_mem[i].script_type == NS_SCRIPT_TYPE_JAVA)
    {
      java_type = 1;
      break;
    }
  }
  NSDL3_SCHEDULE(NULL, NULL, "java type script = %d", java_type);
  return java_type;
}

/********************keyword parsing methods end*******************************/

/******************** Thread code start *****************************/


//this function will create unix domain listen socket.
int vutd_create_listen_fd_unixs(Msg_com_con *mccptr, int con_type, unsigned short *listen_port)
{
  int lfd;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called");
/********************listener creation *******************/
  lfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(lfd == -1) {
    NSTL1_OUT(NULL, NULL, "Error: Failed to create socket, error msg = %s\n", nslb_strerror(errno));
    return -1;
  }
  
  //now bind to an address.
  //currently we have taken address from .tmp directory with test run number.
  char addr_path[128];
  sprintf(addr_path, "%s/.tmp/%d", getenv("NS_WDIR") ,testidx);
  if(mkdir(addr_path, 0777) != 0) {
    if(errno != EEXIST) {
      NSTL1_OUT(NULL, NULL, "Error: failed to create socket directory, error msg = %s\n", nslb_strerror(errno));
      return -1;
    }
  }
  sprintf(addr_path, "%s/.tmp/%d/%d", getenv("NS_WDIR") ,testidx, my_port_index);
  
  struct sockaddr_un sock_addr;
  //memset structure
  memset(&sock_addr, 0, sizeof(struct sockaddr_un));
  sock_addr.sun_family = AF_UNIX;
  strcpy(sock_addr.sun_path, addr_path);
  int len = strlen(sock_addr.sun_path) + sizeof(sock_addr.sun_family);
   
  //unlink path just for safty.
  unlink(addr_path);
  
  //bind to specified path.
  if(bind(lfd, (struct sockaddr *)&sock_addr, len) < 0) {
    NSTL1_OUT(NULL, NULL, "Error: Failed to create socket, error msg = %s\n", nslb_strerror(errno));
    return -1;
  }

  //now set to listen.
  if(listen(lfd, 1000) < 0) {
    NSTL1_OUT(NULL, NULL, "Error: Failed to listen, error msg = %s\n", nslb_strerror(errno));
    return -1;
  }
 
/***********************listener created*************************/   
 
  memset(mccptr, 0, sizeof(Msg_com_con));
  mccptr->con_type = con_type;
  NSDL3_SCHEDULE(NULL, NULL, "NVM (Unix socket) Listener address = %s", addr_path);
  
  //this holding address for unix socket(it's in file path format, so no listen port).
  strcpy(listen_socket_address, addr_path);
  *listen_port = 0;
  mccptr->fd = lfd;
  NSDL3_SCHEDULE(NULL, NULL, "mccptr->fd = %d,  fd = %d", mccptr->fd , lfd);
  NSDL3_SCHEDULE(NULL, NULL, "Method Exiting, mccptr->con_type = %d", mccptr->con_type);
  return lfd;
}



// Add fd to njvm epoll
void njvm_add_select_fd(int fd, int event){
     
  struct epoll_event pfd;
  pfd.events = event;
  pfd.data.fd = fd;
  NSDL2_NJVM(NULL, NULL, "Method, called");
  if (epoll_ctl(njvm_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured while adding listen fd to epoll for njvm, njvm_add_select_fd() - EPOLL_CTL_ADD: err = %s",
                                                                                                               nslb_strerror(errno));
    NS_EXIT(-1, "EPOLL ERROR occured in parent process, while adding listen fd to epoll for njvm, njvm_add_select_fd() - "
                                                                                          "EPOLL_CTL_ADD: err = %s", nslb_strerror(errno));
  }
}

void remove_select_fd(int fd)
{
  struct epoll_event pfd;
  bzero(&pfd, sizeof(pfd));
  if(epoll_ctl(njvm_epoll_fd, EPOLL_CTL_DEL, fd, &pfd) == -1){
     NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured while removing from epoll, error = %s", nslb_strerror(errno)); 
     NS_EXIT(-1, "EPOLL ERROR occured while removing from epoll, error = %s", nslb_strerror(errno));
  }
}

/* 1: Add new connection to free thread list
   2: This method malloc node for newly accepted connections and init Msg_com_con structure
   3: Here we are making data structure to maintain free thread pool list. Node prev and next are 
      used when NVM received closed connection from NJVM and one node get fetched from the
      list the link should not get break*/
inline void njvm_add_thrd_to_free_list(int thrd_fd, int epoll_add_flag){

  Msg_com_con *node;
  Msg_com_con *next_node;
  
  NSDL2_NJVM(NULL, NULL, "Method, called. njvm_total_free_thread = %d", njvm_total_free_thread);
  next_node = njvm_free_thread_pool;  
  MY_MALLOC(node, sizeof(Msg_com_con), "njvm_free_thread_pool", -1);
  memset(node, 0, sizeof(Msg_com_con));
  init_msg_con_struct(node, thrd_fd, CONNECTION_TYPE_CHILD_OR_CLIENT, "127.0.0.1", NS_STRUCT_TYPE_NJVM_THREAD);
  NSDL2_NJVM(NULL, NULL, "added in free list and epoll fd, node = %p", node);

  if(epoll_add_flag) 
    add_select_msg_com_con((char*)node, thrd_fd, EPOLLIN); // add njvm_listen fd to nvm epoll
 
  njvm_free_thread_pool = node;
  node->next = next_node;
  if(next_node)
    next_node->prev = node;
  njvm_total_free_thread++; 
  njvm_total_accepted_thread++;
  NSDL2_NJVM(NULL, NULL, "After adding thread to free list: njvm_total_free_thread = %d, njvm_total_accepted_thread = %d", 
                          njvm_total_free_thread, njvm_total_accepted_thread);
  if(njvm_total_requested_thread == njvm_total_accepted_thread){
    NSDL2_NJVM(NULL, NULL, "All requested connection are made so setting njvm_resize_req_done to 0");
    //reset threshold thread count.
    njvm_num_threshold_thread = (njvm_total_accepted_thread * global_settings->njvm_settings.njvm_thrd_threshold_pct)/100; 
    njvm_resize_req_done = 0;
    NSDL2_NJVM(NULL, NULL, "njvm_total_requested_thread = %d, njvm_num_threshold_thread = %d, njvm_total_accepted_thread = %d", 
                            njvm_total_requested_thread, njvm_num_threshold_thread, njvm_total_accepted_thread);
  }
} 


// This method checks for java type script, create listner if any and creae epoll for njvm and add njvm_listen_fd to njvm epoll
static inline void njvm_check_and_create_listner(){

  NSDL2_NJVM(NULL, NULL, "Method called");

  // Create listner for njvm
  //If simulator is on and conn type is 1 then create unix socket otherwise tcp socket
  if(global_settings->njvm_settings.njvm_simulator_mode == 0 || global_settings->njvm_settings.njvm_con_type == 0) {
		if((njvm_listen_fd = vutd_create_listen_fd(&njvm_msg_com_con_listen, NS_STRUCT_TYPE_NJVM_LISTEN, &njvm_listen_port)) == -1)
		{
			// TODO - send end parent
			NS_EXIT(0, "Error: Unable to create Listener for threads");
		}
 
    //Add some tcp socket options for improving performance
  
    int flag = 1;
    //we have set TCP_NODELAY to remove Nagle's algorithm, because in nvm-njvm we don't want any delay in messages.
    //In cps test we were facing performance issue. Because of this.
    //after sending response of end_session, nvm was waiting for ack from njvm to send start_user msg. (this dealy can be upto 40milli sec.)
    if(setsockopt(njvm_listen_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) )  < 0) {
      NSTL1_OUT(NULL, NULL, "Error: Unable to set TCP_NODELAY on njvm listener");
    }
  }
  else {
		if((njvm_listen_fd = vutd_create_listen_fd_unixs(&njvm_msg_com_con_listen, NS_STRUCT_TYPE_NJVM_LISTEN, &njvm_listen_port)) == -1)
		{
			// TODO - send end parent
			NS_EXIT(0, "Error: Unable to create Listener for threads");
		}
  }

  NSTL1(NULL, NULL, "Nvm listner created for nvm %d with port = %d", my_port_index, njvm_listen_port);
  NSDL2_NJVM(NULL, NULL, "Njvm listner created. njvm_listen_fd = %d", njvm_listen_fd);
 
  // Create njvm epoll, this epoll will be used for listening for control connection and thrd connection
  njvm_epoll_fd = epoll_create(10); 
  if(njvm_epoll_fd == -1){
    NS_EXIT(-1, "Failed while creating nvm,s epoll for java type script. Error = %s", nslb_strerror(errno));
  }  
  NSTL2(NULL, NULL, "Nvm epoll created for nvm %d with with fd = %d", my_port_index, njvm_epoll_fd);

  // Add njvm_listen_fd to njvm epoll
  njvm_add_select_fd(njvm_listen_fd, EPOLLIN);
} 

// This method accpet thread connection from njvm
// This method will be called form njvm epoll and nvm epoll
inline void njvm_accept_thrd_con(int event_fd, int epoll_add_flag){

  int thread_con_fd;
  NSDL2_NJVM(NULL, NULL, "Method, called. njvm_total_free_thread = %d", njvm_total_free_thread);
  thread_con_fd = accept(event_fd, NULL, 0);
  if(thread_con_fd == -1){
    NSTL1_OUT(NULL, NULL, "Error in accepting thread connection. Error = %s\n", nslb_strerror(errno));
    end_test_run();
  }
  NSTL1(NULL, NULL, "Thread connection accepted for nvm %d. fd = %d", my_port_index, thread_con_fd);
  NSDL2_NJVM(NULL, NULL, "Thread connection accepted for nvm %d. fd = %d", my_port_index, thread_con_fd);
  njvm_add_thrd_to_free_list(thread_con_fd, epoll_add_flag); 
}

// This method is used only to send message which need only opcode
inline void send_msg_from_nvm_to_parent(int opcode, int th_flag)
{
  parent_child send_msg;

  NSDL2_MESSAGES(NULL, NULL, "Sending message to master. opcode = %d, g_rtc_msg_seq_num = %d", opcode, g_rtc_msg_seq_num);
  send_msg.opcode = opcode;
  send_msg.gen_rtc_idx = g_rtc_msg_seq_num;
  send_msg.testidx = testidx;
  //send_msg.child_id = my_port_index;
  send_msg.child_id = my_child_index;
  send_msg.avg_time_size = g_avgtime_size;
  send_msg.msg_len = sizeof(send_msg) - sizeof(int);
 
  if(write_msg((th_flag == DATA_MODE)?&g_dh_child_msg_com_con:&g_child_msg_com_con, (char *)(&send_msg), sizeof(send_msg), 0, th_flag) != 0)
  {
    NSTL1_OUT(NULL, NULL, "NVM %d, failed to send opcode %d to %s connection\n"
                          , my_child_index, opcode, (th_flag == DATA_MODE)?"DATA":"CONTROL");
    end_test_run();
  }
}

// nvm will wait in its njvm epoll and wait for control connection and njvm thread connection  
static int epoll_wait_and_accept_con(){
  struct epoll_event events[NS_EPOLL_MAXFD];
  int ret;
  int i;
  int bind_msg = 0;

  NSDL2_NJVM(NULL, NULL, "Method, called. global_settings->njvm_settings.njvm_conn_timeout in miliseconds = %d", (global_settings->njvm_settings.njvm_conn_timeout * 1000));
  ret = epoll_wait(njvm_epoll_fd, &events[0], 1, (global_settings->njvm_settings.njvm_conn_timeout * 1000));

  NSDL2_NJVM(NULL, NULL, " ret = %d, events[0].data.fd = %d", ret, events[0].data.fd);

  if (ret == 0){ // Time out
    NSTL1_OUT(NULL, NULL, "Njvm timeout while waiting for control and thread connection. nvm_id = %d\n", my_port_index);
    end_test_run();
  }

  if(ret < 0){ // Epoll error
    NSTL1_OUT(NULL, NULL, "Njvm epoll error for nvm_id %d . Error = %s\n", my_port_index, nslb_strerror(errno)); 
    end_test_run(); 
  }
  if(events[0].data.fd == njvm_listen_fd) {
    njvm_control_con_fd = accept(events[0].data.fd, NULL, 0);
    init_msg_con_struct(&njvm_msg_com_con_control, njvm_control_con_fd, CONNECTION_TYPE_CHILD_OR_CLIENT, "127.0.0.1", NS_STRUCT_TYPE_NJVM_CONTROL); 
    NSTL1(NULL, NULL, "Control connection accepted for nvm %d. njvm_control_con_fd = %d", my_port_index, njvm_control_con_fd);
    NSDL2_NJVM(NULL, NULL, "Control connection accepted, adding its fd to njvm epoll");
    njvm_add_select_fd(njvm_control_con_fd, EPOLLIN); 
  } 
  NSDL2_NJVM(NULL, NULL, "Control connection is made going to wait for thread connections. thread connection timeout = %d", (global_settings->njvm_settings.njvm_conn_timeout * 1000));


  while(1) {
    ret = epoll_wait(njvm_epoll_fd, events, NS_EPOLL_MAXFD, (global_settings->njvm_settings.njvm_conn_timeout * 1000));

    NSDL2_NJVM(NULL, NULL, " ret = %d", ret);
    if (ret == 0){ // Time out
      NSTL1_OUT(NULL, NULL, "Error: NVM(%02d) Connection timeout while waiting for control and thread connection\n"
              "Total connections  received are %02d out of %02d connections and Bind message is %sreceived\n", 
              my_port_index, njvm_total_free_thread, global_settings->njvm_settings.njvm_init_thrd_pool_size, bind_msg?"":"not ");
      end_test_run();
    }

    if(ret < 0){ // Epoll error
      NSTL1(NULL, NULL, "Njvm epoll error. Error = %s\n", nslb_strerror(errno)); 
      end_test_run();
      NS_EXIT(-1, "Njvm epoll error. Error = %s", nslb_strerror(errno));
    }

    // Process events. These events can be for thread connections or BIND requuest on control connection
    for(i = 0; i <ret; i++){
      if(events[i].data.fd == njvm_listen_fd){ // Thread connection
        NSDL2_NJVM(NULL, NULL, "Got thread connection on listen fd");
        njvm_accept_thrd_con(events[i].data.fd, 0);
      } else if(events[i].data.fd == njvm_control_con_fd){ // BIND_REQUEST on control connection
        NSTL1(NULL, NULL, "Got BIND_MESSAGE on control connection for nvm %d. fd = %d", my_port_index, njvm_control_con_fd);
        NSDL2_NJVM(NULL, NULL, "Got BIND_REQUEST on control connection");
        bind_msg = 1;
        if(handle_msg_from_njvm(&njvm_msg_com_con_control, events[i].events != 0)){
          NSTL1_OUT(NULL, NULL, "NJVM %s, invalid bind message recieved\n", msg_com_con_to_str(&njvm_msg_com_con_control));  
          end_test_run();
        }
        //log trace for bind message.
        if(njvm_total_free_thread == global_settings->njvm_settings.njvm_init_thrd_pool_size) {
          NSTL1(NULL, NULL, "NVM(%02d): Bind message received after all connections from NJVM threads. "
                "Total connections received are %02d out of %02d connections.", my_port_index, njvm_total_free_thread, 
                global_settings->njvm_settings.njvm_init_thrd_pool_size);
        } else {
          NSTL1(NULL, NULL, "NVM(%02d): Bind message received before all connections from NJVM threads. Total connections received are "
                            "%02d out of %02d connections. Continuing to wait for connections for %02d seconds", my_port_index,
                            njvm_total_free_thread, global_settings->njvm_settings.njvm_init_thrd_pool_size, 
                            global_settings->njvm_settings.njvm_conn_timeout);
        }
      }
    }

    //log trace epoll_wait loop report 
    NSTL2(NULL, NULL, "NVM(%02d): Total connections  received are %02d out of %02d connections and Bind message is %sreceived",
          my_port_index, njvm_total_free_thread, global_settings->njvm_settings.njvm_init_thrd_pool_size, bind_msg?"":"not ");

    //Check if total connection received 
    if(bind_msg && (njvm_total_free_thread == global_settings->njvm_settings.njvm_init_thrd_pool_size))  {
      NSDL2_NJVM(NULL, NULL, "NVM(%d), Bind message and Total thread connections received, breaking from epoll_wait loop", my_port_index);
      break;
    }
  }
  
  //Now no need for this check.
/*
  // TODO: check for no of thread connetions made
  if(njvm_total_free_thread != global_settings->njvm_settings.njvm_init_thrd_pool_size) {
    NSDL2_NJVM(NULL, NULL, "Failed to recieve njvm init connection(%d), total recieved = %d, exiting.", 
       global_settings->njvm_settings.njvm_init_thrd_pool_size, njvm_total_free_thread);
    NSTL1_OUT(NULL, NULL, "Failed to recieve njvm init connection(%d), total recieved = %d, exiting.", 
       global_settings->njvm_settings.njvm_init_thrd_pool_size, njvm_total_free_thread);
    end_test_run();
  } 
*/  
 
  // if we got bind msg from njvm, we need to prcoceed
  if(bind_msg){
    NSDL2_NJVM(NULL, NULL, "Going to add njvm_listen_fd, control_fd, and all the thread fd to nvm epoll");
    Msg_com_con *node = njvm_free_thread_pool;    
    //remove from local epoll fd.
    remove_select_fd(njvm_listen_fd);
    remove_select_fd(njvm_control_con_fd);  
    close(njvm_epoll_fd); // close njvm epoll

    add_select_msg_com_con((char*)&njvm_msg_com_con_listen, njvm_listen_fd, EPOLLIN); // add njvm_listen fd to nvm epoll
    add_select_msg_com_con((char*)&njvm_msg_com_con_control, njvm_control_con_fd, EPOLLIN); // add control connection fd to nvm epoll
    // for debuging purpose
#ifdef NS_DEBUG_ON
    while(node != NULL){ // add all thread connection fd to nvm epoll
      NSDL2_NJVM(NULL, NULL, "con = %s", msg_com_con_to_str(node));
      node = node->next;
    }
#endif
    node = njvm_free_thread_pool; 
    while(node != NULL){ // add all thread connection fd to nvm epoll
      NSDL2_NJVM(NULL, NULL, "con = %s", msg_com_con_to_str(node));
      add_select_msg_com_con((char *)node, node->fd, EPOLLIN);
      node = node->next;
    }
    // This message is now send from netstorm child in all cases
    // Send start phase to parent, so that parent can start scheduling
    //send_msg_from_nvm_to_parent(START_MSG_BY_CLIENT);
    NSTL2(NULL, NULL, "All fd added to nvm eopll for nvm %d", my_port_index);
  }
  return 0;
}

// This method will check if we need to send thread pool increment request, and if yes then sends thread pool lincrement request and 
// sets njvm_total_requested_thread counter
static inline void njvm_send_resize_request(){

  int inc_thrd = 0;
  NSDL2_NJVM(NULL, NULL, "Method called. njvm_resize_req_done = %d, njvm_total_busy_thread = %d, njvm_num_threshold_thread = %d",
           njvm_resize_req_done, njvm_total_busy_thread, njvm_num_threshold_thread);
   
  //Now check max thread count. 
  //if(njvm_num_threshold_thread < global_settings->njvm_settings.njvm_max_thrd_pool_size){
  if(njvm_total_accepted_thread < global_settings->njvm_settings.njvm_max_thrd_pool_size){
 
    // if threshold_value and incr thrd is becoming greater than max then we should increment max - threshold only so that we can keep max
    if((njvm_total_accepted_thread + global_settings->njvm_settings.njvm_increment_thrd_pool_size) 
                                                            <= global_settings->njvm_settings.njvm_max_thrd_pool_size)
      inc_thrd = global_settings->njvm_settings.njvm_increment_thrd_pool_size;
    else
      inc_thrd = global_settings->njvm_settings.njvm_max_thrd_pool_size - njvm_total_accepted_thread;
    NSDL2_NJVM(NULL, NULL, "Sending increment thrd messgae inc_thrd = %d", inc_thrd);
    send_msg_to_njvm(&njvm_msg_com_con_control, NS_NJVM_INCREASE_THREAD_POOL_REQ, inc_thrd);
    NSTL1(NULL, NULL, "NS_NJVM_INCREASE_THREAD_POOL_REQ send to njvm. inc_thrd = %d, nvm = %d",inc_thrd, my_port_index);
    njvm_total_requested_thread += inc_thrd; 
    njvm_resize_req_done = 1; // set resize flag to indictae resize request is send
  }
}

// This method return free thread node if present and decrement njvm_total_free_thread
// If free list is empty, then it will return NULL
Msg_com_con *njvm_get_thread(){
  Msg_com_con *node;
  Msg_com_con *next_node;
  
  NSDL2_NJVM(NULL, NULL, "Method called. njvm_total_free_thread = %d, Total accepted thread = %d\n",
                            njvm_total_free_thread, njvm_total_accepted_thread);

  // to increase thread pool size if it is reached to threshold
  if(!njvm_resize_req_done && (njvm_total_busy_thread >= njvm_num_threshold_thread))
    njvm_send_resize_request();

  if(njvm_free_thread_pool != NULL){
    NSDL2_NJVM(NULL, NULL, "Free thread is present in free list");
    next_node = njvm_free_thread_pool->next;
    node = njvm_free_thread_pool;
    njvm_free_thread_pool = next_node;
    if(next_node)
      next_node->prev = NULL;
    njvm_total_free_thread--;
    njvm_total_busy_thread++;
    return(node);
  } else { // there is no threade in free list
    NSDL2_NJVM(NULL, NULL, "Free thread is not present in free list");
    return NULL;
  }  
}

// This method takes a node as an args and add that node to free thread list and increment free counter and decrement bsy count 
inline void njvm_free_thread(Msg_com_con *node){

  Msg_com_con *next_node;

  NSDL2_NJVM(NULL, NULL, "Method called. njvm_total_free_thread = %d, njvm_total_busy_thread = %d", njvm_total_free_thread, njvm_total_busy_thread);
  next_node = njvm_free_thread_pool;
  njvm_free_thread_pool = node;
  node->next = next_node; 
  njvm_total_free_thread++;
  njvm_total_busy_thread--;

}

inline void check_and_init_njvm(){

  if(is_java_type_script()){
    njvm_check_and_create_listner();
    start_njvm();
    epoll_wait_and_accept_con();
  }
}

void divide_thrd_among_nvm(){
  //int init_thread_pool_size = global_settings->njvm_settings.njvm_init_thrd_pool_size;
  //int max_thread_pool_size = global_settings->njvm_settings.njvm_max_thrd_pool_size;

  /* Fix for bug 31417. Issue while running java type script test was not started if user provided njvm_init_thrd_pool_size less than 
     num_process and users are less than njvm_init_thrd_pool_size. 
     To fix this issue an additional check for total users. i.e test will not be executed if njvm_init_thrd_pool_size is less than num_process and total_users. 
  */


  /* Bug 58943 in 4.1.13. Also check for total user is removed as this cannot be done for FSR and also if concurrent users are more, then session will fail with Error and then user need to tune the max value */
  //if(global_settings->njvm_settings.njvm_init_thrd_pool_size < global_settings->num_process){
    //global_settings->njvm_settings.njvm_init_thrd_pool_size = global_settings->num_process;
    //NSTL1(NULL, NULL, "Initial number of threads for Java type script are less than number of NVM's so setting it equal to NVMs. Now, Init_thread = %d and num_process = %d", global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->num_process);
  //}

  NSDL2_NJVM(NULL, NULL, "Before divide init_thrd_pool_size = %d, incremental thread = %d, max_thrd_pool_size = %d, num_process = %d", 
    			  global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->njvm_settings.njvm_increment_thrd_pool_size
                          , global_settings->njvm_settings.njvm_max_thrd_pool_size, global_settings->num_process);

  global_settings->njvm_settings.njvm_init_thrd_pool_size /= global_settings->num_process;
  global_settings->njvm_settings.njvm_max_thrd_pool_size /= global_settings->num_process;
  global_settings->njvm_settings.njvm_increment_thrd_pool_size /= global_settings->num_process;

  /* Fix for bug 31417. njvm_init_thrd_pool_size and njvm_max_thrd_pool_size is divided equally among total nvms. 
     If value of njvm_init_thrd_pool_size or njvm_max_thrd_pool_size is less than num_process it becomes 0 on diving and no thread was created      by njvm. So if it is getting 0 after diving reassigning value to its original value. 
  */
  NSDL2_NJVM(NULL, NULL, "After divide init_thrd_pool_size = %d,incremental thread = %d, max_thrd_pool_size = %d", 
    			  global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->njvm_settings.njvm_increment_thrd_pool_size                          , global_settings->njvm_settings.njvm_max_thrd_pool_size);

#if 0
  if (global_settings->njvm_settings.njvm_init_thrd_pool_size == 0) // TODO - due to check above, this can never be 0 and code can be deleted
  {
    //Bug 41490 - RDT || With selenium JAVA script sessions are not generating and Test is getting stuck in NC mode and terminating in NS mode
    if((global_settings->njvm_settings.njvm_max_thrd_pool_size) && (global_settings->njvm_settings.njvm_max_thrd_pool_size <= init_thread_pool_size))
      global_settings->njvm_settings.njvm_init_thrd_pool_size = global_settings->njvm_settings.njvm_max_thrd_pool_size;
    else
      global_settings->njvm_settings.njvm_init_thrd_pool_size = init_thread_pool_size;
  }
#endif
  if (global_settings->njvm_settings.njvm_init_thrd_pool_size == 0){
    NSTL1(NULL, NULL, "Warning: No of init thread are less than number of NVM. So, setting it equals to no. NVM's\n");
    global_settings->njvm_settings.njvm_init_thrd_pool_size = 1;
  }
  if (global_settings->njvm_settings.njvm_increment_thrd_pool_size == 0){
    NSTL1(NULL, NULL, "Warning: No of incremental thread are less than number of NVM. So, setting it equals to no. NVM's\n");
    global_settings->njvm_settings.njvm_increment_thrd_pool_size = 1;
  }
  if ((global_settings->njvm_settings.njvm_max_thrd_pool_size == 0) || (global_settings->njvm_settings.njvm_max_thrd_pool_size < (global_settings->njvm_settings.njvm_init_thrd_pool_size + global_settings->njvm_settings.njvm_increment_thrd_pool_size))){
    NSTL1(NULL, NULL, "Warning: No of Max thread are less than number of NVM. So, setting it equals to (init + incremental)\n");
    global_settings->njvm_settings.njvm_max_thrd_pool_size = global_settings->njvm_settings.njvm_init_thrd_pool_size + global_settings->njvm_settings.njvm_increment_thrd_pool_size;
  }
  NSDL2_NJVM(NULL, NULL, "init_thrd_pool_size = %d, incr_thrd_pool_size = %d, max_thrd_pool_size = %d", 
    global_settings->njvm_settings.njvm_init_thrd_pool_size, global_settings->njvm_settings.njvm_increment_thrd_pool_size, global_settings->njvm_settings.njvm_max_thrd_pool_size);
}


/******************** Thread code end *****************************/
