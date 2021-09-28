/********************************************************************************************************************
 * File Name      : nsi_rtc_invoker.c                                                                               |
 |                                                                                                                  | 
 * Purpose        : Apply RunTimeChanges for NetStorm, Monitor and File Parameter Keywords                          |
 |                   Usage:                                                                                         |
 |                     nsi_rtc_invoker -t <TestRun_Number>                                                          |
 |                                                                                                                  |
 * HLD            : [1] Take input Test Run number (say xxx) form user                                              |
 |                                                                                                                  |
 |                  [2] Check test is running or not?                                                               |
 |                      - check ns-inst directory exist or not in .tmp dir 'ls -td $NS_WDIR/.tmp/user/ns-inst*'     |
 |                      - If dir exist then check provided test run number is same as in netstorm.tid or not?       |
 |                      - if provided test run number is same as netstorm.tid then check pid(netstorm.pid)          |
 |                        is running or not, If not running through error message on console and exist              |
 |                                                                                                                  |
 |                  [3] Open file in write mode for debug log on path                                               | 
 |                        $NS_WDIR/logs/TRxxx/runtime_changes/nsi_rtc_invoker.log                                   |
 |                                                                                                                  |
 |                  [4] Check ownership - rtc can be applied by test run user or root user                          |
 |                                                                                                                  |
 |                  [5] Check for which RTC is applied (i.e. NS, Monitor or File param), only one can be applied    | 
 |                  at the same time.                                                                               |
 |                                                                                                                  |
 |                  [6] Make Message if following format                                                            |
 |                                                                                                                  |
 |                  0 1 2 3 4 5 6 7                                                                                 |
 |                  +-------+-------+---------+                                                                     |
 |                  | lenght| opcode| payload |                                                                     |
 |                  |  (4)  |  (4)  |         |                                                                     |
 |                  +-+-+-+-+-------+---------+                                                                     |
 |                                                                                                                  |
 |                  Where:                                                                                          |
 |                    length   - provide lenght include header(i.e. opcode + ..) + payload (i.e. send message)      |
 |                                                                                                                  |
 |                    opcode   - provide opcode                                                                     |
 |                               149 - for File Parameter RTC                                                       |
 |                               150 - for NetStorm RTC                                                             |
 |                               151 - for Monitor RTC                                                              |
 |                                                                                                                  |
 |                    payload  - provide send message                                                               |
 |                                                                                                                  |
 * Author(s)      : Tanmay Prakash                                                                                  |
 * Date           : 27 Aug 2016                                                                                     |
 * Copyright      : (c) Cavisson Systems,2016                                                                       |
 * Mod. History   : Mod. History   : Add support for VUser Management.(Atul Sharma)                                 |
 *
 * GCC Command    : gcc nsi_rtc_invoker.c -fgnu89-inline -I./libnscore -I./topology/src/topolib -I/usr/include/postgresql/ -DUbuntu -DRELEASE=1604 -DENABLE_TCP_STATES -DPG_BULKLOAD -lodbc -o ../bin/nsi_rtc_invoker ../obj/topology_debug/src/libnstopo/libnstopo_debug.a ../obj/nscore_debug/libnscore/libnscore_debug.a -L/usr/lib64 -lm -g
 *******************************************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <stdarg.h>

#include "nslb_sock.h"
#include "nslb_util.h"
#include "nslb_log.h"

#include "ns_alloc.h"
#include "nsi_rtc_invoker.h"
#include "ns_msg_def.h"
#define  MSG_FROM_CMD 99
/* Global variables */
char g_ns_wdir[512 + 1];

char runtime_conf_file[512] = {0};
char sorted_rtc_file[512] = {0};
char rtc_log_file[512] = {0};

static FILE *dlog_fp = NULL;

int trnum = 0;
unsigned short debug = 0x0000;
int ns_pid = 0;

uid_t test_run_uid;
gid_t test_run_gid;

int check_user(char *err_msg);
char cur_date_time[64 + 1];
char rtc_type[15] = "Unknown";
char current_user[64] = "";
char role[32 + 1] = "";
char group[128 + 1] = "";
char test_run_user[128 + 1] = ""; //Either gui or normal user
char *keyword = NULL;
int g_time_out = 60; //Initial value of tcp connnection timeout will be 60 seconds
int time_out = 150; //Initial value of time_out will be 150 seconds
char read_buf[1024] = {0};
int rbytes = -1;

#define CHECK_REPLY_STATUS_FROM_NS \
  if(rbytes == 0) \
    sprintf(err_msg, "FAILURE\nError: Connection closed while waiting for reply"); \
  else \
  { \
    if(errno == 11) \
       sprintf(err_msg, "FAILURE\nError: Runtime change request timeout"); \
    else \
       sprintf(err_msg, "FAILURE\nError: Failed to read runtime change reply"); \
  } 


/*--------------------------------------------------------------------------------
   Name		: create_log_file()

   Purpose	: Its purpose is to create a debug log file under same TRxx
  		  directory, with the purpose of debugging


   Output	: Will open the global file pointer and create the RTC debug_log
                  file. 
---------------------------------------------------------------------------------*/
static int open_debug_log(char *err_msg)
{
  char dlog_file[1024 + 1];
  int fd;
  sprintf(dlog_file, "%s/logs/TR%d/runtime_changes/nsi_rtc_invoker.log", g_ns_wdir, trnum);

  umask(0);
  fd = open(dlog_file, O_WRONLY|O_CLOEXEC|O_CREAT|O_APPEND, 0666);
  if(ENOENT == errno)
  {
    sprintf(err_msg, "Error:'%s', [System Exception] = %s", dlog_file, nslb_strerror(errno));
    return FAILURE;
  }
  if(fd < 0 )
  {
    sprintf(err_msg, "Error: Unable to open debug log file '%s', [System Exception-(%d)] = %s", dlog_file, errno, nslb_strerror(errno));
    return FAILURE;
  }
  
  dlog_fp = fdopen(fd, "a");
  if(dlog_fp == NULL) 
  {
    sprintf(err_msg, "Error: Unable to open debug log file '%s', [System Exception-(%d)] = %s", dlog_file, errno, nslb_strerror(errno));
    return FAILURE;
  }
  
  return INV_SUCCESS;
}

/*---------------------------------------------------------------------------------
   Name         : debug_log

   Input        : log_check = to check if debug log flag is set or not; 
		  file name;
		  line number; 
		  function name; 
		  format

   Output       : write the debug log message to rtc_log_file file pointer
                  in the following format

                    "nsi_rtc_invoker.c|182|is_digit|Method is_dir called"
                         
   Purpose      : Its purpose is to write to a debug log file, if the user have 
                  provided '-d' argument i.e user have enable the debugging, in
                  order to track any bug easily. 
----------------------------------------------------------------------------------*/
static inline void debug_log(int level, char *file, int line, char *fname, char *format, ...)
{
  va_list ap;
  int wbytes = 0, log_wbytes = 0;
  char buffer[1024 + 1] = "\0";
  char time_buf[100];

  if((debug & level) == 0) return;

  wbytes = sprintf(buffer, "\n%s|%s|%d|%s|", nslb_get_cur_date_time(time_buf, 0), file, line, fname);
  va_start(ap, format);
  log_wbytes = vsnprintf(buffer + wbytes , 1024 - wbytes, format, ap);
  va_end(ap);
  buffer[1024] = 0;

  if(wbytes < 0)
    log_wbytes = strlen(buffer) - wbytes;

  if(dlog_fp)
  {
    if((fwrite(buffer, log_wbytes + wbytes, 1, dlog_fp)) < 0)
    {
      fprintf(stderr, "Error: [Write Operation Failure] - Unable to write to debug log file for nsi_rtc_invoker .\n");
      exit (EXIT_FAILURE);
    }
  } 
}
//debug_log(DLOG_L1, _NLF_,"<message>\n")

char *get_gui_test_owner(int trnum)
{  
  static char user_name[128] = "";
  char user_file[256 + 1] = "";
  char pid_dir[256] = "";
  int len;
  FILE *fp = NULL;
  struct passwd *pw;
  struct stat file_info;

  if((trnum < 1) || (ns_pid < 1))
  {
    debug_log(DLOG_L1, _NLF_, "Error: Unable to get owner of testrun %d", trnum);
    return "InvalidTestRun";
  }
  
  debug_log(DLOG_L2, _NLF_, "Method called, testrun %d", trnum);

  sprintf(user_file, "%s/logs/TR%d/.user", g_ns_wdir, trnum);
  fp = fopen(user_file, "r");
  if(fp)
  { 
     len = fread(user_name, 1, 128, fp);
     if(len < 1)
     {
       //Error
       return "Unavailable";
     }
     if(user_name[len-1] == '\n')
       user_name[len-1] = '\0';

     return user_name;
  }
  else
  {
    sprintf(pid_dir, "/proc/%d/", ns_pid); //User who run the test
    stat(pid_dir, &file_info);
    pw = getpwuid(file_info.st_uid);
    if(pw)
      return pw->pw_name;
    else
      return "Unavailable";
  }

  return NULL;
}

/*---------------------------------------------------------------------------------
   Name		: is_digit

   Input	: char string

   Output	: return SUCCESS - on successful execution
			 FAILURE - on Error
   
   Purpose	: Its purpose is to check each every character of string is a digit. 
----------------------------------------------------------------------------------*/
#if 0
static int is_digit(char *line)
{
  int i;
  debug_log(DLOG_L1, _NLF_,"Method is_dir() called for string %s", line);
  for(i=0; i < strlen(line); i++)
    if(!isdigit(line[i]) ) 
      return FAILURE;

  return INV_SUCCESS;
}
#endif

/*---------------------------------------------------------------------------------
   Name		: read_file

   Input	: File full path and file name

   Output	: return SUCCESS - on successful execution
			 FAILURE - on Error
                  Fill the global variable test_pid if test run number provided by
 		  user is found in any netstorm.tid file
   
   Purpose	: Its purpose is the open the file provided as input and check
                  netstorm.tid value is equal to the test run number given by users
                  if match found the store the netstorm pid from netstorm.pid
----------------------------------------------------------------------------------*/
#if 0
static int read_file(const char *dir_name, char *file_name)
{ 
  char line[256 + 1];
  int value = 0;
  FILE *fp;
  char path[MAX_PATH_LEN + 1] = "";
  int len ;
  
  debug_log(DLOG_L2, _NLF_, "Method read_file() called for file = %s, absolute path = %s", file_name, path);
  len = snprintf(path, MAX_PATH_LEN, "%s/%s", dir_name, file_name);
  path[len] = 0;
 
  if((fp = fopen(path, "r")) == NULL)
  {
    debug_log(DLOG_L1, _NLF_, "Error: Error in opening file (%s). Error = %s", path, nslb_strerror(errno));
    exit (EXIT_FAILURE);
  }

  if(fgets(line, 128, fp) == NULL) 
    return FAILURE;

  //Trim white spaces.It will modify the original string.
  nslb_clear_white_space_from_string(line, END);
  debug_log(DLOG_L1, _NLF_, "After trim string is = %s and length is = %d", line, (int) strlen(line));

  if (is_digit(line) != INV_SUCCESS)
  {
    debug_log(DLOG_L1, _NLF_, "Error: testrun number should be Integer");
    exit (EXIT_FAILURE);
  }
  value = atoi(line);

  debug_log(DLOG_L1, _NLF_,"Inside read_file and value = %d", value);
  if(strncasecmp(file_name, "netstorm.tid", 12) == 0)
  {
    debug_log(DLOG_L1, _NLF_, "Netstorm TID = %d", value);
    if(trnum == value)
      return INV_SUCCESS;
    else
      return FAILURE;
  }
  if(strncasecmp(file_name, "netstorm.pid", 12) == 0)
  { //ns_pid will set if PID of given testrun is alive.
    //EPERM means Operation not permitted. As RTC can be applied by any user, so if error of type 'EPERM' comes, then ns_pid will be set.     
    if((kill(value, 0)) == -1){
      if(errno == EPERM)
        ns_pid = value;
    }//Enter to else block when kill() is success.
    else{
      ns_pid = value;
   }
    debug_log(DLOG_L1, _NLF_, "Netstorm PID = %d Value = %d", ns_pid, value);
  }
   
  debug_log(DLOG_L1, _NLF_, "Netstorm PID = %d Value = %d", ns_pid, value);
  fclose(fp);
  return INV_SUCCESS; 
}
#endif

/*-------------------------------------------------------------------------------------
   Name 	: read_dir

   Input	: Input will be directory name, initially path will be "NS_WDIR/.tmp/"
  
   Output	: Return SUCCESS on successful completion
		  and FAILURE on error

   Purpose	: The purpose of this function is to open the directory recurrsively
                  until regular file found and call the fuction read_file to read 
                  netstorm.tid and netstorm.pid

   Example	:/home/cavisson/Controller_harshit/.tmp
                 <------------------------------------>
                    opendir() =  g_ns_wdir + /.tmp      /ndeadmin
                                                       <-------->
                                                         opendir   /ns-inst0
                                                                   <-------->
                                                                  (regular file)
--------------------------------------------------------------------------------------*/
#if 0
static int read_dir(const char *dir_name)
{
  char *file_name = NULL;
  char path[MAX_PATH_LEN + 1] = "";
  int len = 0;
  DIR *dir;
  struct dirent *entry;

  debug_log(DLOG_L1, _NLF_, "Method called read_dir dir_name = %s", dir_name);
  if (!(dir = opendir(dir_name)))
  {
    fprintf(stderr, "Error: [Operation not permitted] - access denied to open directory = %s", dir_name);
    exit (EXIT_FAILURE);
  }   

  if (!(entry = readdir(dir)))
  {
    fprintf(stderr,"Error: [Operation not permitted] - access denied in reading directory = %s\n", dir_name);
    exit (EXIT_FAILURE);
  }

  do 
  {
    if(ns_pid != 0)
      break;

    debug_log(DLOG_L1, _NLF_, "directory/file name = %s" , entry->d_name);
    if(nslb_get_file_type(dir_name, entry) == DT_DIR) //handle dir 
    {
      len = snprintf(path, MAX_PATH_LEN, "%s/%s", dir_name, entry->d_name);
      path[len] = 0;

      if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
         continue;

      read_dir(path);
    }
    else if (nslb_get_file_type(dir_name, entry) == DT_REG)//handle inner file
    {
      debug_log(DLOG_L1, _NLF_, "directory %s found again going listing its file" , entry->d_name);
      file_name = entry->d_name;
      if ( strncmp(file_name, "netstorm.tid", strlen("netstorm.tid") ) == 0)
      {
        debug_log(DLOG_L3, _NLF_, "netstorm.tid file found = %s, going to read file, path = %s", file_name, path);
        if(read_file(dir_name, file_name) == INV_SUCCESS)
        {
	  read_file(dir_name, "netstorm.pid");
	  //return INV_SUCCESS;
	}
        return INV_SUCCESS;
      }
    }
  } while ((entry = readdir(dir)));
  
  closedir(dir);
  return INV_SUCCESS;
}
#endif

/*----------------------------------------------------------------------
  Name 		: check_test_running

  Input		: test run number provided by user

  output	: Running - 0
                  Not runing - 1

  purpose	: The purpose of this  function is check whether the
		  test is still running or not before applying RTC
------------------------------------------------------------------------*/
static int check_test_running(char *err_msg)
{
  char path[256] = "";
  char *ptr1 = NULL;
  struct passwd *pw;
  int tr;

#if 0
  sprintf(path, "%s/.tmp/cavisson", g_ns_wdir);
  if(read_dir(path) != INV_SUCCESS)
  {
    sprintf(err_msg, "Error:[Path(%s) not found] - Environment variable NS_WDIR not set", path);
    return FAILURE;
  }
#endif
  //TODO: check current running testruns
  TestRunList *test_run_info;
  test_run_info = (TestRunList *)malloc(128 * sizeof(TestRunList)); //alloc
  memset(test_run_info, 0, (128 * sizeof(TestRunList)));
  for(tr = 0; tr < (get_running_test_runs_ex(test_run_info, NULL)); tr++)
  {
    if(trnum == test_run_info[tr].test_run_list)
    {
      debug_log(DLOG_L1, _NLF_, "trnum = %d, tid_path = %s", trnum, test_run_info[tr].tid_file_path);
      strcpy(path, test_run_info[tr].tid_file_path);
      if((ptr1 = strrchr(path, '_')))
      {
        ptr1++;
        //if((ptr2 = strrchr(ptr1, '/')))
        //  *ptr2 = '\0';   atoi can handle it: "converts the initial portion of the string pointed to by nptr to int"
        ns_pid = atoi(ptr1);
        break;
      }
    }
  }
  free(test_run_info); //free

  if(ns_pid == 0)
  {
    sprintf(err_msg, "Error:[Test %d is not running]", trnum);
    debug_log(DLOG_L1, _NLF_, "Error: Unable to fetch process PID of Test run %d; ns pid = %d", trnum, ns_pid);
    return TEST_NOT_RUNNING;
  }
  else if((kill(ns_pid, 0)) == -1)
  {
    if (errno == ESRCH){   // check to proper description to user ESRCH = No such Process
      sprintf(err_msg, "Error:[Test %d is stopped]", trnum);
      debug_log(DLOG_L1, _NLF_, "Error: [No Running Process(%d)] - Make sure TR%d is still running in order to apply RTC", ns_pid, trnum);
    } 
    if(errno == EPERM)  /*EPERM = Operation not permitted -> In this case we need to check if RTC is trying to be applied by different
                                                               user  */
    {           
      /* Any user can now apply RTC 
      if(check_user(err_msg) != INV_SUCCESS)
        PROC_RETURN(1, err_msg);
      */
      snprintf(test_run_user, 128, "%s", get_gui_test_owner(trnum));
      if(!strcmp(current_user, ""))
      {
        pw = getpwuid(geteuid());
        if(pw)
           strcpy(current_user, pw->pw_name); 
        else
        {
          sprintf(err_msg, "Error: In fetching Information regarding Current User[%s]", current_user);
          return FAILURE;
        }
      }
      debug_log(DLOG_L1, _NLF_, "Current user who run rtc tool = %s, User who run the test = %s, Netstorm PID = %d ",
                                 current_user, test_run_user, ns_pid);
      return INV_SUCCESS;
    }
    debug_log(DLOG_L1, _NLF_, "Error: Make sure test[%d] is still|process pid = %d|errno(%d) = (%s)", trnum, ns_pid, errno, nslb_strerror(errno));
    return FAILURE;
  }
  return INV_SUCCESS;
}

/*----------------------------------------------------------------------------------
   Name		: check_user()
   
   Input	:

   Output	: 

   Purpose	: The purpose of this function is to check if the user running this
                  tool and the user who ran the test(Running) are same of not
                  if not same it will exit giving out an error 
----------------------------------------------------------------------------------*/
/* From 4.1.10 GUI users will be given by -u option
int check_user(char *err_msg)
{
  char pid_dir[256] = "";
  struct passwd *pw;
  struct group  *grp;
  struct stat file_info;

  debug_log(DLOG_L2, _NLF_, "Method called.");

  pw = getpwuid(geteuid());
  if(pw)
    strcpy(current_user, pw->pw_name); 
  else
  {
    sprintf(err_msg, "Error: In fetching Information regarding Current User[%s]", current_user);
    return FAILURE;
  }
  
  debug_log(DLOG_L1, _NLF_, "Current user who run rtc tool = %s; Netstorm PID = %d ", current_user, ns_pid);
  if(!ns_pid)
  {
    return FAILURE;
  }

  sprintf(pid_dir, "/proc/%d/", ns_pid); //User who run the test
  debug_log(DLOG_L1, _NLF_, "PID directory path under /proc = %s", pid_dir);

  stat(pid_dir, &file_info);
  pw = getpwuid(file_info.st_uid);
  if(pw)
  {
    strcpy(test_run_user, pw->pw_name);
    debug_log(DLOG_L1, _NLF_, "User who run the test = %s",  test_run_user);
  }
  else
  {
    sprintf(err_msg, "Error: Unable to fetch information of TR%d Owner", trnum);
    return FAILURE;
  }
  test_run_uid = pw->pw_uid;
  debug_log(DLOG_L1, _NLF_, "user id who ran the test = %ld", test_run_uid);
  grp = getgrgid(file_info.st_gid);
  if (grp != NULL)
    test_run_gid = grp->gr_gid;
    debug_log(DLOG_L1, _NLF_, "group id who ran the test = %ld", test_run_gid);

  if(!test_run_uid && !test_run_gid)
     debug_log(DLOG_L3, _NLF_, "Warning: User Id & Group Id of TR%d owner not set");
  
  debug_log(DLOG_L1, _NLF_, "Test run user = %s, Tool user = %s. Going to compare users",  test_run_user, current_user);
  if((strcmp(current_user, test_run_user)) != 0)
  {
    fprintf(stderr, "Error: [User Unauthenticated] - Make sure to login with TR%d owner = %s, presently you are login with %s\n",
                                trnum, test_run_user, current_user);
    return FAILURE;
  }
  

  return INV_SUCCESS;
}
*/

#define PORT_LEN   5
/*--------------------------------------------------------------------------
  Name 		: make_connection_with_ns

  Input		: -xx-

  Output	: It will return the fd to which connection with NS is made

  Purpose	: It will use nslb_tcp_client_Ex API to make connection with 
		  NS, this function take out NS port from 
                  NS_WDIR/logs/TRxx/NSPort which is required in making 
                  connection with NS
---------------------------------------------------------------------------*/
int make_connection_with_ns(char *err_msg, int conn_mode)
{
  char ns_port_buf[PORT_LEN + 1] = "";
  char ns_port_file[MAX_PATH_LEN + 1] = "";
  char err_buf[1024] = "";
  FILE *fp;
  int fd = -1;
  int ns_port = 0;

  debug_log(DLOG_L1, _NLF_, "Method called");
  if (conn_mode == CONTROL_MODE) 
    sprintf(ns_port_file, "%s/logs/TR%d/NSPort", g_ns_wdir, trnum);
  else
    sprintf(ns_port_file, "%s/logs/TR%d/NSDHPort", g_ns_wdir, trnum);
 
  debug_log(DLOG_L1, _NLF_, "ns_port_file = %s", ns_port_file);
  
  if(!(fp = fopen(ns_port_file, "r")))
  {
    sprintf(err_msg, "Error: [Connection Failure] - In making connection with NS, unable to opening NSPort file %s", ns_port_file); 
    return FAILURE;
  }

  if(fgets(ns_port_buf,  PORT_LEN + 1, fp) != NULL) // should we != NULL for success ?
    ns_port = atoi(ns_port_buf);

  debug_log(DLOG_L1, _NLF_, "path of ns port file = %s, ns_port = %d, g_time_out = %d", ns_port_file, ns_port, g_time_out);
 
  /* Increase timeout value from 10 to 60 seconds  */ 
  if((fd = nslb_tcp_client_ex("127.0.0.1", ns_port, g_time_out, err_buf)) <= 0)
  {
    sprintf(err_msg, "Error: %s", err_buf);
    return FAILURE;
  }

  debug_log(DLOG_L1, _NLF_, "Returning fd = %d , after nslb_tcp_client", fd);

  fclose(fp);
  return fd;
}

/*-------------------------------------------------------------------------------
   Name		: write_msg_on_connection

   Input	: It will take following inputs
                    o message buffer
                    o fd [which is return from make_connection_with_ns()]
                    o Lenght of message buffer

   Output	: SUCCESS i.e. 0 -> on successful execution
   		  FAILURE i.e. -1 -> on failure

   Purpose	: The purpose of the function is to write message buffer on the 
		   fd(file descriptor) return from make_connetion_with_ns()
-------------------------------------------------------------------------------*/
int write_msg_on_connection(char *msg_buf, int fd, int msg_buf_len, char *err_msg)
{
  int ret = 0;

  debug_log(DLOG_L1, _NLF_, "Method called, fd = %d, msg_buf = [%s], msg_buf_len = %d", fd, msg_buf + 8, msg_buf_len);
  
  if(fd < 0)
  {
   sprintf(err_msg,"Error: [Bad file descriptor(%d)] - Unable to write message on NS connection ", fd);
   return FAILURE;
  }

  ret = write(fd, msg_buf, msg_buf_len);
  debug_log(DLOG_L1, _NLF_, "Return bytes after write = %d", ret);

  if(ret < 0)
  {
    sprintf(err_msg,"Error: [Write Operation Fail] - Unable to send RTC message to NS connection\n"
                                                    "Tip: Make sure NS is ready before you apply RTC");
    return FAILURE;
  }
  return INV_SUCCESS;  
}


void set_debug(int level)
{
  if(level == 1) 
    debug = 0x000F;
  else if(level == 2)
    debug = 0x00FF;  
  else if(level == 3)
    debug = 0x0FFF;
  else if(level == 4)
    debug = 0xFFFF;
  else
    debug = 0x0000;
}

void show_and_append_all_log(int opcode)
{
  FILE *fp, *fp1;
  char rtc_all_log_file[256] = {0};
  char buf[1024 + 1] = {0};
  char cmd[512] = {0};
  char *ptr = NULL;
  int status;
  char err_msg[1024] = "\0";

  debug_log(DLOG_L1, _NLF_, "Method Called, rtc_log_file = %s, opcode = %d", rtc_log_file, opcode); 

  sprintf(rtc_all_log_file, "%s/logs/TR%d/runtime_changes/runtime_changes_all.log", g_ns_wdir, trnum);
  fp = fopen(rtc_log_file, "r");
  fp1 = fopen(rtc_all_log_file, "a");
  if(fp1 == NULL) {
    debug_log(DLOG_L1, _NLF_, "Error: in opening file = %s in append mode where error = %s", rtc_all_log_file, nslb_strerror(errno));
    fprintf(stderr, "Error: in opening file = %s in append mode where error = %s\n", rtc_all_log_file, nslb_strerror(errno));
    exit(-1);
  }
  
  sprintf(cmd, "tail -1 %s | grep \"Successful\" >/dev/null 2>&1", rtc_log_file);
  if((status = nslb_system(cmd,1,err_msg)) != 0)
    if((opcode != APPLY_FPARAM_RTC) && (opcode != TIER_GROUP_RTC) && (opcode != APPLY_MONITOR_RTC) && (opcode != APPLY_CAVMAIN_RTC))
       fprintf(stderr, "Runtime changes failed.\n");

  nslb_get_cur_date_time(cur_date_time, 0);

  if(fp == NULL) {
    debug_log(DLOG_L1, _NLF_, "Error: in opening file = %s, error = %s", rtc_log_file, nslb_strerror(errno));
    fprintf(stderr, "Error: in opening file = %s, error = %s\n", rtc_log_file, nslb_strerror(errno));
    exit(-1);
  } else {
    while(fgets(buf, 1024, fp)) {
      debug_log(DLOG_L1, _NLF_, "buff =  %s", buf);
      if(opcode != PAUSE_VUSER && opcode != RESUME_VUSER && opcode != STOP_VUSER){
        fprintf(stdout, "%s", buf);
      }
      //Skip all logs contains "Successful" or "\n"
      //Eg. "All runtime changes applied Successful"
      if(((ptr = strstr(buf, "applied Successfully")) != NULL) || ((ptr = strstr(buf, "not applied")) != NULL) || (buf[0] == '\n') )
        continue;
    
      if(keyword && (ptr = strchr(keyword, '\n'))) {
        *ptr = '\0';
        ptr++;
      }

      /*Bug:31901, Gaurav: Runtime history - [Type|Status|datetime|owner|appliedBy|Keyword|Description] */
      fprintf(fp1, "%s|%s|%s|%s|%s|%s|%s", rtc_type, ((status == 0)?"Success":"Failed"), cur_date_time, test_run_user,
                    current_user,"-", buf);
      debug_log(DLOG_L1, _NLF_, "Going to be write '%s' is on '%s', keyword '%s'", buf, rtc_all_log_file, keyword);

      keyword = ptr;
    }
  }

  /*if(chown(rtc_all_log_file, test_run_uid, -1) == -1) {
    debug_log(DLOG_L1, _NLF_, "Chown command failed for '%s'", rtc_all_log_file);
    fprintf(stderr, "Chown command failed for '%s'\n", rtc_all_log_file);
    exit(-1);
  }*/
 
  debug_log(DLOG_L1, _NLF_, "--------------------runtime changes end----------------------");
  fclose(fp);
  fclose(fp1);
  //unlink(rtc_log_file);
  truncate(rtc_log_file, 0);
}

static void apply_runtime_changes()
{
  int retryCount=1;
  struct stat s;
  struct stat s1;

  debug_log(DLOG_L1, _NLF_, "Method Called, where runtime_conf_file = %s, rtc_log_file = %s", runtime_conf_file, rtc_log_file);
  for( ;retryCount < 60; retryCount++)
  {
    if(stat(runtime_conf_file, &s) == 0) {
      sleep(1);
      if(retryCount > 60) {
        debug_log(DLOG_L1, _NLF_, "Runtime changes cannot be applied for test run '%d' as netstorm could not finish applying changes in 60 seconds", trnum);
        exit(-1);
      }
    }
  }

  if(stat(rtc_log_file, &s1) != 0) {
    debug_log(DLOG_L1, _NLF_, "Runtime changes cannot be applied for test run '%d' as netstorm did not create runtime changes log file", trnum);
    exit(-1);
  } 
  show_and_append_all_log(0);
}

void make_sorted_file()
{
  char cmd[1024] = {0}; 
  struct stat s;
  FILE *log_fp = NULL;
  char err_msg[1024] = "\0";

  debug_log(DLOG_L1, _NLF_, "Method Called, where runtime_conf_file = %s", runtime_conf_file);
 
  if(stat(runtime_conf_file, &s) != 0) {
    debug_log(DLOG_L1, _NLF_, "Error: Runtime conf file = [%s] not exist, errror = [%s]", runtime_conf_file, nslb_strerror(errno));
    fprintf(stderr, "Error: Runtime conf file = [%s] not exist, errror = [%s]", runtime_conf_file, nslb_strerror(errno));
    exit(-1);
  }
  if((log_fp = fopen(rtc_log_file, "w")) == NULL)
  {
    fprintf(stderr, "Unable to create file = %s, error = %s\n", rtc_log_file, nslb_strerror(errno));
    exit(-1);
  }

  /*if(chown(rtc_log_file, test_run_uid, -1) == -1) {
    debug_log(DLOG_L1, _NLF_, "Chown command failed");
    fprintf(stderr, "Chown command failed for '%s', error = '%s'\n", rtc_log_file, nslb_strerror(errno));
    exit(-1);
  }*/
  sprintf(cmd, "%s/bin/nsi_merge_sort_scen 1 %s %s 1>%s 2>&1", g_ns_wdir, runtime_conf_file, sorted_rtc_file, rtc_log_file); 
  if(nslb_system(cmd,1,err_msg) != 0) {
    debug_log(DLOG_L1, _NLF_, "Error in executing command = %s", cmd); 
    debug_log(DLOG_L1, _NLF_, "Runtime changes cannot be applied for test run '%d' as runtime change configuration file is not valid", trnum);
    fprintf(stdout, "%s\n", runtime_conf_file);
    show_and_append_all_log(0);
    exit(-1);
  }
  
  debug_log(DLOG_L1, _NLF_, "Method make_sorted_file() end.");
}

/*This function will do following task 
  1. read sorted_scenario.conf for provided test run
  2. Parse PROGRESS_MSECS Keyword to found progress interval time 
  3. set g_time_out  
*/
void read_scenario_and_fill_timeout(int test_idx)
{
  char sorted_scenario_file[512 + 1];
  char keyword_buf[1024 + 1];
  char scen_keyword[64 + 1];
  char s_rtc_timeout_val[64 + 1];
  char text[64 + 1];
  FILE *fp = NULL;
  char *ptr = NULL;
  int num;

  debug_log(DLOG_L1, _NLF_,"Method called, tets idx =  %d", test_idx);

  sprintf(sorted_scenario_file, "%s/logs/TR%d/sorted_scenario.conf", g_ns_wdir, test_idx);
  fp = fopen(sorted_scenario_file, "r");
  if(fp == NULL){
    debug_log(DLOG_L1, _NLF_, "sorted_scenario file not found a path %s", sorted_scenario_file);
    fprintf(stderr, "sorted_scenario file not found a path %s", sorted_scenario_file);
    exit(-1);
  }
  while(fgets(keyword_buf, 1024, fp)) 
  {
    if((ptr = strstr(keyword_buf, "RUNTIME_CHANGE_TIMEOUT")))
    {
      debug_log(DLOG_L1, _NLF_, "RUNTIME_CHANGE_TIMEOUT found in %s", sorted_scenario_file);
      num = sscanf(keyword_buf, "%s %s %s", scen_keyword, text, s_rtc_timeout_val);
      if((num == 3) && atoi(text) && (s_rtc_timeout_val[0] != '\0'))
      {
        debug_log(DLOG_L1, _NLF_, "mode = %s, timout val = %s", text, s_rtc_timeout_val);
        time_out = (atoi(s_rtc_timeout_val) + 30); //30 extra to handle processing from controller
        break;
      }
    }
    /*
    else if((ptr = strstr(keyword_buf, "PROGRESS_MSECS")) != NULL)
    {
      debug_log(DLOG_L1, _NLF_,"PROGRESS_MSECS found in sorted_scenario.conf", sorted_scenario_file);
      sscanf(keyword_buf, "%s %s", scen_keyword, text); 
      if(text)//text should never be null.
        progress_msecs = atoi(text);

      time_out = (3 * (progress_msecs/1000)) + 20; //+20 bcs in NC parent_epoll t.o is (3*progress_msec)+10000 hence taking 10 sec extra.
    }
    */
  }
  debug_log(DLOG_L1, _NLF_, "set time_out to %d Sec.", time_out);
}

//This function is to just check if any monitor related runtime changes are done or not
static int is_runtime_changes_for_monitor(char *ibuff)
{
  if((strstr(ibuff, "START_MONITOR") != NULL) || (strstr(ibuff, "STOP_MONITOR") != NULL) || (strstr(ibuff, "RESTART_MONITOR") != NULL) || (strstr(ibuff, "MONITOR_PROFILE") != NULL) || (strstr(ibuff, "DELETE_MONITOR") != NULL) || (strstr(ibuff, "ND_DATA_VALIDATION") != NULL) || (strstr(ibuff, "ENABLE_MONITOR_DATA_LOG") != NULL) || (strstr(ibuff, "ENABLE_AUTO_JSON_MONITOR") != NULL))
  {
    //Got monitor related kw.
    return 1;
  }

  return 0;
}

static inline int fill_buffer_and_write_msg(int length, char *msg_buf, int opcode, int con_fd, char *err_msg)
{
  int wlen = 0;
  int name_len = 0;
 
  debug_log(DLOG_L1, _NLF_, "Method Called, length = %d, msg_buf  = %s, opcode = %d, con_fd = %d", length, msg_buf, opcode, con_fd);
  wlen += length;

  //Fill opcode in msg_buf
  memcpy(msg_buf + MSG_LEN, &opcode, OPCODE_LEN);
  wlen += OPCODE_LEN;

  //Fill length (payload + opcode) in msg_buf
  memcpy(msg_buf, &wlen, MSG_LEN);
  wlen += MSG_LEN;

  if(write_msg_on_connection(msg_buf, con_fd, wlen, err_msg) == FAILURE)
    PROC_RETURN(1, err_msg);

  /* In case of Sync point RTC, message contains the owner of RTC of size 64 bytes */
  if(opcode == APPLY_PHASE_RTC)
    name_len = 64;

  debug_log(DLOG_L1, _NLF_, "<----Send MSG to NS---> len = %d, opcode = %d, msg = %s, wlen = %d",
                             *((int *)msg_buf), *((int *)msg_buf + 1), (msg_buf + 8 + name_len), wlen);
/*  do
  {
    rbytes = read(con_fd, read_buf, 1024);
    debug_log(DLOG_L1, _NLF_,"Trying to receive message from NS. Read bytes = %d", 
                      rbytes);
  } while(rbytes < 0); */
  if((rbytes = read(con_fd, read_buf, 1024)) <= 0)
  {
      CHECK_REPLY_STATUS_FROM_NS
      //sprintf(err_msg, "FAILURE\nError: [Read Operation Failure] - Unable to receive message from NS. read bytes = %d, errno(%d) = (%s)", 
      //                  rbytes, errno, nslb_strerror(errno));
      debug_log(DLOG_L1, _NLF_,"Error: Message received from NS. read bytes = %d, errno(%d) = (%s)", 
                        rbytes, errno, nslb_strerror(errno));
      show_and_append_all_log(opcode);
      PROC_RETURN(1, err_msg);
      if(dlog_fp)
        fclose(dlog_fp);
      if(msg_buf)
      {
        free(msg_buf);
        msg_buf = NULL;
      } 
      exit(1);
  }

  close(con_fd);
  
  debug_log(DLOG_L1, _NLF_, "Message received from NS = %s, rbytes = %d\n", read_buf, rbytes);
  
  if((rbytes > 8) && !strcmp(read_buf + 4, "REJECT"))
  {
    debug_log(DLOG_L1, _NLF_,"rbytes = %d, read_buf = %s, opcode = %d", 
                      rbytes, read_buf + 4, *((int *)read_buf));
    set_rtc_type(*((int*)read_buf), rtc_type);
    printf("%s RTC is already in progress. Please retry after sometime!", rtc_type);
  }
  else
  {
    apply_runtime_changes();
    free(msg_buf);
    fclose(dlog_fp);
  }
  return 0;
}

void set_rtc_type(int opcode, char *buf)
{
  if(opcode == APPLY_FPARAM_RTC)
    sprintf(buf, "FileParameter");
  else if(opcode == APPLY_MONITOR_RTC)
    sprintf(buf, "Monitors");
  else if(opcode == APPLY_QUANTITY_RTC)
    sprintf(buf, "Quantity");
  else if(opcode == APPLY_PHASE_RTC)
    sprintf(buf, "Scenario");
  else if(opcode == APPLY_CAVMAIN_RTC)
    sprintf(buf, "CAV Main");
  else if(opcode == TIER_GROUP_RTC)
    sprintf(buf, "Tier Group");
  else if(opcode == PAUSE_VUSER || opcode == RESUME_VUSER || opcode == STOP_VUSER)
    sprintf(buf, "VUser Manager");

  else
    sprintf(buf, "Unknown");
}


static int get_connection_mode(int opcode)
{
  switch(opcode){
  case APPLY_MONITOR_RTC:
  case TIER_GROUP_RTC:
  case APPLY_CAVMAIN_RTC:
  case CAV_MEMORY_MAP: 
   return DATA_MODE;
   break;   
  }
  return CONTROL_MODE;
}

// Below function read_msg_from_fd and handler exit_on_sig_alarm are specific to PAUSE_SCHEDULE AND RESUME_SCHEDULE message
int read_msg_from_fd(int my_fd)
{
  char ns_msg[MAX_MSG_SIZE + 1] = "\0";
  char *cmd_line, *line_ptr;
  int bytes_read = 0, total_read = 0;

  while(1)
  {
    if((bytes_read = read (my_fd, ns_msg + total_read, MAX_MSG_SIZE - total_read)) < 0)
      continue;
    else if(bytes_read == 0)
      return 1;

    total_read += bytes_read;
    ns_msg[total_read] = '\0';

    line_ptr = ns_msg;
    char *tmp_ptr;
    
    while ((tmp_ptr = strstr(line_ptr, "\n")))
    {
      int msg_len = 0;
      *tmp_ptr = 0; // NULL terminate 
      cmd_line = line_ptr;
      msg_len = strlen(cmd_line);
      line_ptr = tmp_ptr + 1;
      total_read -= msg_len + 1;
      fprintf(stderr, "%s\n", cmd_line);
      return 0;
    }
    bcopy(line_ptr, ns_msg, total_read + 1); // Must be NULL terminated
  }
  return 0;
}

void exit_on_sig_alarm(int sig)
{
  fprintf(stderr, "Exiting on time out occurred during pause/resume test.\n");
  exit(-1);
}

int pause_resume_test(char *current_user, int alarm_time, int server_port, int duration, int opcode)
{
   Pause_resume msg;
   int sock_fd;
   char err_msg[1024]="\0";
   memset(&msg, 0, sizeof(Pause_resume));
   msg.child_id = 255;
   msg.opcode = opcode;
   if(opcode == PAUSE_SCHEDULE)
      msg.time = duration/1000; 
   strncpy(msg.cmd_owner, current_user, 64);
   msg.msg_from = MSG_FROM_CMD;

   if(alarm_time > 0)
   {
     alarm(alarm_time);
     (void) signal( SIGALRM, exit_on_sig_alarm);
   }

   sock_fd = nslb_tcp_client_ex("127.0.0.1", server_port, 30, err_msg);
   if(sock_fd < 0)
   {
     fprintf(stderr, "%s\n", err_msg);
     exit(-1);
   }
   msg.msg_len = sizeof(Pause_resume) - sizeof(int);
   if (write(sock_fd, &msg, sizeof(Pause_resume)) == -1)
   {
      fprintf(stderr, "Error: Unable to send message to netstorm.\n");
      exit (-1);
   }

   read_msg_from_fd(sock_fd);
   close(sock_fd);
   return 0;
}

int main(int argc, char *argv[])
{
  char argv_flag = 0x00;
  int rtc_flag = 0x0000;
  char err_msg[2 * 1024 + 1] = "";
  //char usage[2*1024 + 1];
  char *msg_buf = NULL;
  char *home_env = NULL;

  int opcode, opt;
  int len = 0;
  int wlen = 0;
  int con_fd = 0;
  long length;
  int fd;
  int offset = -1;
  int limit = -1;
  int quantity = -1;
  char group[128 +1] = "";
  char status[128 + 1] = "";
  char vuser_id[10 * 1024 +1];
  char user_vptr[10 * 1024 +1];
  char gen_id[512 +1] = "";

  struct stat conf_stat;
  int need_to_malloc;
  char *buf_ptr;
  struct passwd *pw;
  int conn_mode = CONTROL_MODE;
  int duration, alarm_time = 15, u_flag = 0, r_flag = 0, my_user_id, ns_uid;

  const char *usage =  "Usage: \n"
           "  nsi_rtc_invoker -t <tr_num> -o <opcode> -m <message> -d -u username -g group -O offset -L list -q quantity -D <duration> -i <alarm_time>\n"
           "Where:\n"
           "  tr_num      - to provide test run number\n"
           "                  Note:- test run number should be numeric\n" 
           "  opcode      - to provide operation for rtc type. RTC can be following four types only -\n"
           "                  149 - for File parameter RTC\n"
           "                  150 - for NetStorm RTC\n"
           "                  151 - for Monitor RTC\n"
           "                  164 - for vuser RTC\n"
           "                  164 - for vuser summary|165: vuser list|166 - vuser pause|167 - resume vuser\n"
           "                  120 - for pause test | 130 - for resume test\n" 
           "  message     - to provide message according to rtc opcode. For example File param RTC messgae will be -\n"
           "                message - \"<mode>|<script_name>:<param_name>|<comma seperated file list>\" \n"
           "                Eg:  0|S1:S1v1|emp_info\n"
           "  username    - to provide gui user who is applying changes\n"
           "  duration    - duration of pausing test <HH:MM:SS>(for pausing test)\n"
           "  alarm_time  - in seconds(for pausing and resuming test. Default - 15 seconds)\n";
  if (argc < 2) 
  {
    sprintf(err_msg, "Error: Arguments are missing.\n%s", usage);
    PROC_RETURN(1, err_msg);
  }

  while((opt = getopt(argc, argv, "t:o:m:d:u:r:g:O:L:s:l:q:w:v:G:D:i:")) != -1 )
  {
    switch(opt)
    {
      case 't':
        if(!ns_is_numeric(optarg))
        {
	  sprintf(err_msg, "Error: test run number should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        trnum = atoi(optarg);
        argv_flag |= ARGV_TRNUM;  
        break;

      case 'o':
        if(!ns_is_numeric(optarg)) 
        {
          sprintf(err_msg, "Error: opcode should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        opcode = atoi(optarg);
        argv_flag |= ARGV_OPCODE;  
        break;

      case 'm':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: message is empty.");
          PROC_RETURN(1, err_msg);
        }

        len = strlen(optarg);
        wlen += len;
        len += MSG_LEN + OPCODE_LEN + 1;
        msg_buf = (char *)malloc(len);                            

        //Fill payload in msg_buf 
        strcpy(msg_buf + (MSG_LEN + OPCODE_LEN), optarg);
        argv_flag |= ARGV_MESG;  
        break;

      case 'd':
        if(!ns_is_numeric(optarg)) 
        {
          sprintf(err_msg, "Error: debug level should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        set_debug(atoi(optarg));
        break;

      case 'u':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: username not given.");
          PROC_RETURN(1, err_msg);
        }
        strncpy(current_user, optarg, 64);
        u_flag++;
        break;
   
      case 'r':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: role not given.");
          PROC_RETURN(1, err_msg);
        }
        strncpy(role, optarg, 32);
        r_flag++;
        break;

     case 'g':
        if(!*optarg)
        {
	  sprintf(err_msg, "Error: group not given.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & NS_VUSER_OPTION_GROUP)
        {
          sprintf(err_msg, "Error: Multiple Group is not allowed.");
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_GROUP;
        strncpy(group, optarg, 128);
        group[128] = '\0';

        break;
      
     case 'O':
        if(!ns_is_numeric(optarg))
        {
	  sprintf(err_msg, "Error: Offset should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & ( NS_VUSER_OPTION_OFFSET | 
	                 NS_VUSER_OPTION_QUANTITY | 
                         NS_VUSER_OPTION_USER_LIST | NS_VUSER_OPTION_USER_VPTR))
        {
          sprintf(err_msg, "Error: Invalid option with offset. \n%s", usage);
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_OFFSET;
        offset = atoi(optarg);

        break;

     case 'L':
        if(!ns_is_numeric(optarg))
        {
	  sprintf(err_msg, "Error: Limit should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & ( NS_VUSER_OPTION_LIMIT | 
			 NS_VUSER_OPTION_QUANTITY | 
  		         NS_VUSER_OPTION_USER_LIST | NS_VUSER_OPTION_USER_VPTR))
        {
          sprintf(err_msg, "Error: Invalid option with Limit. \n%s", usage);
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_LIMIT;
        limit = atoi(optarg);

        break;

     case 'q':
        if(!ns_is_numeric(optarg))
        {
	  sprintf(err_msg, "Error: quantity should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & ( NS_VUSER_OPTION_QUANTITY | 
                         NS_VUSER_OPTION_LIMIT | NS_VUSER_OPTION_OFFSET | NS_VUSER_OPTION_STATUS | 
                         NS_VUSER_OPTION_USER_LIST | NS_VUSER_OPTION_USER_VPTR))
        {
          sprintf(err_msg, "Error: Invalid option with Quantity.\n%s", usage);
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_QUANTITY;
        quantity = atoi(optarg);

        break;

     case 's':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: status not given.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & NS_VUSER_OPTION_STATUS)
        {
          sprintf(err_msg, "Error: Multiple status list is not allowed.");
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_STATUS;
        strncpy(status, optarg, 128); 
        status[128] = '\0';

        break;

     case 'l':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: VUser-id is not given.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & ( NS_VUSER_OPTION_USER_LIST | 
                         NS_VUSER_OPTION_LIMIT | NS_VUSER_OPTION_OFFSET | NS_VUSER_OPTION_STATUS | 
                         NS_VUSER_OPTION_QUANTITY))
        {
          sprintf(err_msg, "Error: Invalid option with user list. \n%s", usage);
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_USER_LIST;
        strncpy(vuser_id, optarg, 10240);
        vuser_id[10240] = '\0';

        break;

     case 'w':
        if(!ns_is_numeric(optarg))
        {
	  sprintf(err_msg, "Error: timeout should be numeric.");
          PROC_RETURN(1, err_msg);
        }
        g_time_out = atoi(optarg); 
        break;

     case 'v':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: Vptr is not given.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & ( NS_VUSER_OPTION_USER_VPTR | 
                         NS_VUSER_OPTION_OFFSET | NS_VUSER_OPTION_STATUS |
                         NS_VUSER_OPTION_QUANTITY))
        {
          sprintf(err_msg, "Error: Invalid option with -v option.\n %s", usage);
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_USER_VPTR;
        strncpy(user_vptr, optarg, 10240);
        user_vptr[10240] = '\0';
        break;

     case 'G':
        if(!*optarg)
        {
          sprintf(err_msg, "Error: Genid is not given.");
          PROC_RETURN(1, err_msg);
        }
        if (rtc_flag & NS_VUSER_OPTION_GEN)
        {
          sprintf(err_msg, "Error: Multiple generator id is not allowed.");
          PROC_RETURN(1, err_msg);
        }
        rtc_flag |= NS_VUSER_OPTION_GEN;
        strncpy(gen_id, optarg, 512);
        gen_id[512] = '\0';

        break;
    // Below two cases are added to handle request of pause and resume test
     case 'D':
       duration = get_time_from_format(optarg);
       if(duration <= 0)
       {
          sprintf(err_msg, "Pause is not allowed for time '%s'\n", optarg);
          PROC_RETURN(1, err_msg);
       }
       break;

     case 'i':
       alarm_time = atoi(optarg);
       break;

     default:
        sprintf(err_msg, "Error: incorrect usages\n%s", usage);
        PROC_RETURN(1, err_msg);
    }
  }

  if((my_user_id = getuid()) == 0)
  {
    char name[65]="";
    cuserid(name);
    sprintf(err_msg, "Error: Cannot Apply RTC through %s user..", name);
    PROC_RETURN(1, err_msg);
  }

  // Check if user or flag flag are given more than once 
  if(u_flag > 1)
  {
     sprintf(err_msg, "Error: Only one user must be given.\n");
     PROC_RETURN(1, err_msg);
  }

  if(r_flag > 1)
  {
     sprintf(err_msg, "Error: Only one role must be given.\n");
     PROC_RETURN(1, err_msg);
  }

// we have to handle case of PAUSE_SCHEDULE and RESUME_SCHEDULE also through rtc_invoker only
  if((opcode == PAUSE_SCHEDULE) || (opcode == RESUME_SCHEDULE))
  {
     if(argv_flag & ARGV_TRNUM)
     {
        ns_pid = get_ns_pid(trnum);

        if(is_test_run_running(ns_pid) != 0)
        {
           sprintf(err_msg, "Error: Test run %d is not running.\n", trnum);
           PROC_RETURN(1, err_msg);
        }
     }
     else
     {
        sprintf(err_msg, "Error: One test run must be given.\n");
        PROC_RETURN(1, err_msg);
     }

     if (( r_flag && !u_flag ) || (!r_flag && u_flag ))
     {
        fprintf(stderr, "Role(-r) should be provided with User(-u) option\n");
        exit(-1);
     }
     if (r_flag && strcmp(role,"admin") && strcmp(role,"normal"))
     {
        fprintf(stderr, "Role %s is not authorized to pause the test\n", role);
        exit(-1);
     }

     ns_uid = get_process_uid(ns_pid);
     if(current_user[0] == '\0')
     {
        pw = getpwuid(my_user_id);
        strcpy(current_user, pw->pw_name);
     }

     char *ptr = get_test_owner(trnum);
     // root check is done because GUI runs all cmd using root
     if(strcmp(current_user, "admin") && strcmp(current_user, "root") && (!r_flag || strcmp(role,"admin")) && (my_user_id != ns_uid || strcmp(current_user,ptr)))
     {
       fprintf(stderr, "User '%s' is not authorized to pause/resume test TR%d\n", current_user,trnum);
       exit(-1);
     }

     int server_port = get_server_port(trnum);
     if(server_port == -1)
     {
       fprintf(stderr, "Error: NetStorm Port is not available. It may possible test is in before start phase.\n");
       exit(-1);
     }
     pause_resume_test(current_user, alarm_time, server_port, duration, opcode);
     return 0;
  }
  // End of PAUSE_SCHEDULE and RESUME_SCHEDULE handling

  if (( current_user[0] == '\0') && (role[0] == '\0') )
  {
    pw = getpwuid(geteuid());
    if(pw)
       strcpy(current_user, pw->pw_name);
    else
    {
      sprintf(err_msg, "Error: In fetching Information regarding Current User[%s]", current_user);
      PROC_RETURN(1, err_msg);
    }
    strncpy(role, "normal", 32);
  }
  else if ( (current_user[0] == '\0') || (role[0] == '\0') )
  {
    printf("Role(-r) should be provided with User(-u) option\n");
    exit(-1);
  }
  else
  {
    char *ptr = get_test_owner(trnum);
    if(!ptr){
      printf("Error:[Test %d is not valid]\n", trnum);
      exit (-1);
    }
    if((strcmp(role, "admin")) && (strcmp(role, "normal")) && strcmp(current_user, ptr))
    {
      //In case of summary and vuser list, all users allowed to apply VUser RTC of any role
      if(!(opcode == GET_VUSER_SUMMARY || opcode == GET_VUSER_LIST))
      {
        printf("user with role %s is not allowed to apply RTC\n", role);
        exit(-1);
      }
    }
  }

  //HANDLE CAVMEMORYMAP
  if(opcode == CAV_MEMORY_MAP || opcode == APPLY_ALERT_RTC)
  {
    if(!(argv_flag & ARGV_TRNUM)) {
      sprintf(err_msg, "Error: Invalid Arguments.\n%s", usage);
      PROC_RETURN(1, err_msg);
    }
    set_debug(1);
  }
  /*else if (opcode == APPLY_CAVMAIN_RTC)
  {
    if(!(argv_flag & ARGV_MESG)) {
      sprintf(err_msg, "Error: Arguments '-m' is mandatory in CAVMAIN.\n%s", usage);
      PROC_RETURN(1, err_msg);
    }
    set_debug(1);
  }*/
  //HandleVUserRunTimeControl
  else if(opcode >= GET_VUSER_SUMMARY && opcode <= STOP_VUSER)
  {
    //VUser RunTimeControl
    if(!(argv_flag & ARGV_TRNUM)) {
      sprintf(err_msg, "Error: Arguments '-t' is mandatory in VUser.\n%s", usage);
      PROC_RETURN(1, err_msg);
    }
    if(((opcode == GET_VUSER_SUMMARY) && (rtc_flag )) ||
       ((opcode == GET_VUSER_LIST) && (rtc_flag & (NS_VUSER_OPTION_USER_LIST | NS_VUSER_OPTION_USER_VPTR | NS_VUSER_OPTION_QUANTITY))) ||
       ((opcode == PAUSE_VUSER || opcode == RESUME_VUSER || opcode == STOP_VUSER ) && (rtc_flag & ( NS_VUSER_OPTION_LIMIT | NS_VUSER_OPTION_OFFSET | NS_VUSER_OPTION_STATUS))))
    {
      sprintf(err_msg, "Error: Invalid operation provided with opcode %d", opcode);
      PROC_RETURN(1, err_msg);
    }
    debug_log(DLOG_L1, _NLF_, "VUser RunTimeControl:");
  }
  else if(opcode != APPLY_FPARAM_RTC && opcode != TIER_GROUP_RTC && opcode != APPLY_CAVMAIN_RTC) 
  {
    if((argv_flag & ARGV_MASK) != ARGV_TRNUM) {
      sprintf(err_msg, "Error: Invalid Arguments.\n%s", usage);
      PROC_RETURN(1, err_msg);
    }
    set_debug(1);
  }
  else if((argv_flag & ARGV_MASK) != ARGV_MASK)
  {
    sprintf(err_msg, "Error: All arguments '-t, -o and -m' are mandatory.\n%s", usage); 
    PROC_RETURN(1, err_msg);
  }

  if(!(home_env = getenv("NS_WDIR")))
  {
    sprintf(err_msg, "Error: Environment variable NS_WDIR not set");
    PROC_RETURN(1, err_msg);
  }

  strcpy(g_ns_wdir, home_env);

  if((open_debug_log(err_msg)) == FAILURE)
    PROC_RETURN(1, err_msg);
    
  debug_log(DLOG_L1, _NLF_, "<----- RTC Applying -----> Input: trnum = %d, opcode = %d", 
                             trnum, opcode);
  int ret = check_test_running(err_msg);

  if (ret == TEST_NOT_RUNNING){
    PROC_RETURN(-2, err_msg); //exit with 2 in this case
  }
  else if (ret == FAILURE ){
    PROC_RETURN(1, err_msg);
  }
  /* Any user can now apply RTC 
  if(check_user(err_msg) != INV_SUCCESS)
    PROC_RETURN(1, err_msg);
  */
  //Gui user is saved in test_run_user when test ran using gui mode
  snprintf(test_run_user, 128, "%s", get_gui_test_owner(trnum));
  if(!strcmp(current_user, ""))
  {
    pw = getpwuid(geteuid());
    if(pw)
      strcpy(current_user, pw->pw_name); 
    else
    {
      sprintf(err_msg, "Error: In fetching Information regarding Current User[%s]", current_user);
      PROC_RETURN(1, err_msg);
    }
  }

  debug_log(DLOG_L1, _NLF_, "Testrun = %d, test_run_user = %s, current user = %s", trnum, test_run_user, current_user);

  //Resolve bug 26444 - Unable to apply RTC file parameter in any other phase of test after applying it in start phase
  char file_path[512] = {0};
  struct stat parent_nvm_con_stat;
  sprintf(file_path, "%s/logs/TR%d/.parent_nvm_con.status", g_ns_wdir, trnum);

  if((stat(file_path, &parent_nvm_con_stat)) != 0)
  {
    sprintf(err_msg, "Error: Test run is in initialization state. Run time changes cannot be applied. Please try after sometime");
    debug_log(DLOG_L1, _NLF_, "stat() failed for file %s. Error: %s", file_path, nslb_strerror(errno));
    PROC_RETURN(1, err_msg);
  }
  sprintf(rtc_log_file, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, trnum);

  //This function will read sorted_scenario.conf and set tcp_connection timeout
  read_scenario_and_fill_timeout(trnum);
  debug_log(DLOG_L1, _NLF_, "time_out %d Sec and g_time_out = %d.", time_out, g_time_out);
  if(g_time_out < time_out) //tcp_connection timeout should be greater then parent epoll timeout.
    g_time_out = time_out;

    
  /*Create sorted file for NetStorm/NetCloud RTC
    If We apply RTC for NS/NC then opcode will be 150
  */
  if(!(argv_flag & ARGV_OPCODE))
  {
    sprintf(runtime_conf_file, "%s/logs/TR%d/runtime_changes/runtime_changes.conf", g_ns_wdir, trnum);
    sprintf(sorted_rtc_file, "%s/logs/TR%d/runtime_changes/sorted_runtime_changes.conf", g_ns_wdir, trnum);
    make_sorted_file();
    stat(sorted_rtc_file, &conf_stat);
    fd = open(sorted_rtc_file, O_RDONLY|O_CLOEXEC);
    length = conf_stat.st_size;
    need_to_malloc = 4 + 4 + 64 + (length + 1); //Size + opcode + username + payload length
    debug_log(DLOG_L1, _NLF_, "length = %d, need_to_malloc = %d", length, need_to_malloc);
    
    msg_buf = (char *)malloc(need_to_malloc);
    if(msg_buf == NULL) {
      fprintf(stdout, "Error in allocating memory\n");
      exit(-1);
    }
    buf_ptr = msg_buf + 8;
    
    nslb_read_file_and_fill_buf (fd, buf_ptr, length);
    if((strstr(buf_ptr, "RUNTIME_CHANGE_QUANTITY_SETTINGS")) != NULL) {
      opcode = APPLY_QUANTITY_RTC;
    } else if((strstr(buf_ptr, "QUANTITY")) != NULL){
      opcode = APPLY_QUANTITY_RTC;
    } else if(is_runtime_changes_for_monitor(buf_ptr)) {
      opcode = APPLY_MONITOR_RTC;
    } else {
      opcode = APPLY_PHASE_RTC;
      memmove(buf_ptr+64, buf_ptr, length);// Fill username who applied RTC
      memcpy(buf_ptr,current_user,64);
      length += 64;
      buf_ptr += 64;
    }
    debug_log(DLOG_L1, _NLF_, "opcode = %d, complete file msg_buf = %s", opcode, buf_ptr);

    //Gaurav: Malloc and pointing keyword to content of runtime_changes.conf
    keyword = strdup(buf_ptr);
    keyword[strlen(keyword) - 1] = '\0';
    debug_log(DLOG_L1, _NLF_, "cur_date_time = %s, keyword = %s", cur_date_time, keyword);
  }

  set_rtc_type(opcode, rtc_type);
  conn_mode = get_connection_mode(opcode);
  /* Make blocking connection with NetStorm Parent */
  //TODO: making connetion both data & control conn 
  if((con_fd = make_connection_with_ns(err_msg, conn_mode)) == FAILURE)
    PROC_RETURN(1, err_msg);

  //Handling CAV_MEMORY_MAP
  if(opcode == CAV_MEMORY_MAP || opcode == APPLY_ALERT_RTC)
  {
    /* msg format - nsi_rtc_invoker -t 1234 -o CAV_MEMORY_MAP 
    *  message will be 
    *  ----------------------------
    *  MSG_LEN|OPCODE
    *  ----------------------------
    *  3|174
    */
 
    int buff_len = 0;
    int msg_len = 0;
    int read_buf_size = 0;
    int buf_size = 0;
    char msg_buff[10] = "";   //3|174
 
    //Fill opcode in msg_buf
    memcpy(msg_buff + MSG_LEN, &opcode, OPCODE_LEN);
    buff_len += OPCODE_LEN; 

    debug_log(DLOG_L1, _NLF_, "opcode = [%d]\n", opcode);
    memcpy(msg_buff, &buff_len, MSG_LEN);
    buff_len += MSG_LEN; 
    debug_log(DLOG_L1, _NLF_, "rtc invoker buffer is %s ", msg_buff);
    //buffer is ready

    if(write_msg_on_connection(msg_buff, con_fd, buff_len, err_msg) == FAILURE)
      PROC_RETURN(1, err_msg);
   
    int read_buf_int = 4; //1st four bytes of message is length.
    buff_len = 0;
    while(read_buf_int > 0)
    {
      if((rbytes = read(con_fd, read_buf + buff_len, read_buf_int)) <= 0) 
      {
        if(errno == ENETRESET || errno == ECONNRESET ) //In case parent dies after accepting the connection.
        {
          sprintf(err_msg, "FAILURE\nError: [Test is Stopped]");
          debug_log(DLOG_L1, _NLF_,"Error: Test is Stopped. read bytes = %d, buff_len = %d, read_buf_int = %d, errno(%d) = (%s)", 
                                    rbytes, buff_len, read_buf_int, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
        else if(errno == EAGAIN ) //In case parent is not responding.
        {
          sprintf(err_msg, "FAILURE\nError: [TimeOut: Netstorm is not responding]");
          debug_log(DLOG_L1, _NLF_,"Error: TimeOut: Netstorm is not responding read bytes = %d"
                                   "buff_len = %d, read_buf_int = %d, errno(%d) = (%s)", 
                                    rbytes, buff_len, read_buf_int, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
        else 
        {
          sprintf(err_msg, "FAILURE\nError: [Peer Connection Reset]. read bytes = %d, errno(%d) = (%s)", msg_len, errno, nslb_strerror(errno));
          debug_log(DLOG_L1, _NLF_,"Error: [Peer Connection Reset]. read bytes = %d, errno(%d) = (%s)", msg_len, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
      }
      buff_len += rbytes;
      read_buf_int -= rbytes;
    }
    read_buf[buff_len] = '\0'; 
    memcpy(&read_buf_size, read_buf, 4); 
  
    debug_log(DLOG_L1, _NLF_, "First 4 bytes of Message = [%d]", read_buf_size);

    while(read_buf_size > 0)
    {
      if( read_buf_size > 1024)
        buf_size = 1024;
      else
        buf_size = read_buf_size;

      if((rbytes = read(con_fd, read_buf, buf_size)) <= 0) 
      {
        /*sprintf(err_msg, "FAILURE\nError: [Read Operation Failure] - Unable to receive message from NS. read bytes = %d, errno(%d) = (%s)",
                          rbytes, errno, nslb_strerror(errno));*/
        CHECK_REPLY_STATUS_FROM_NS

        debug_log(DLOG_L1, _NLF_,"Error: Message received from NS. read bytes = %d, errno(%d) = (%s)",
                          rbytes, errno, nslb_strerror(errno));
        PROC_RETURN(1, err_msg);
      }
      read_buf[rbytes] = '\0';
      debug_log(DLOG_L1, _NLF_, "Message recived from NS = %s\n", read_buf);
      printf("%s", read_buf);
      if(!(strncmp(read_buf, "Error:", 6)))
        return FAILURE;
 
      read_buf_size -= rbytes;
    }
    rtc_flag = 0;
    close(con_fd);
 
    return 0; 
  }
  //Handling of VUser RunTimeControl
  if (opcode == GET_VUSER_SUMMARY || opcode == GET_VUSER_LIST || opcode == PAUSE_VUSER || 
      opcode == RESUME_VUSER || opcode == STOP_VUSER )
  {
    /*  make buffer and send 
    *  message format will be like 
    *  e.g nsi_rtc_invoker -t 1111 -o GET_VUSER_LIST -g 1 -O 10 -L 10
    *  message will be 
    *  ----------------------------
    *  |len|opcode|g=1;O=10;L=10|
    *  ----------------------------
    */
    int buff_len = 0;
    int msg_len = 0;
    int read_buf_size = 0;
    int buf_size = 0;
    char msg_buff[20 * 1024 + 1] = "";
    int size = 20 * 1024;
    char *buff;

    buff = msg_buff + OPCODE_LEN + MSG_LEN; 
    
    //switch(opcode)
    //{

      //case GET_VUSER_SUMMARY:
      //  buff_len = 0;
     // break;
      //case GET_VUSER_LIST:
        //In case group is given as ALL then it is consider as -1 
        if(rtc_flag & NS_VUSER_OPTION_GROUP)
        { 
          if (strncasecmp(group, "ALL", 3))
            buff_len += snprintf(buff + buff_len , (size - buff_len), "g=%s;", group);
          else
            buff_len += snprintf(buff + buff_len ,(size - buff_len), "g=-1%s;", group + 3);
        }
        else
          buff_len += snprintf(buff + buff_len ,(size - buff_len), "g=-1;");
 
        if(rtc_flag & NS_VUSER_OPTION_STATUS)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "s=%s;", status);
  
        if(rtc_flag & NS_VUSER_OPTION_USER_LIST)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "l=%s;", vuser_id);
      
        if(rtc_flag & NS_VUSER_OPTION_LIMIT)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "L=%d;", limit);
            
        if(rtc_flag & NS_VUSER_OPTION_OFFSET)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "O=%d;", offset);

        if(rtc_flag & NS_VUSER_OPTION_QUANTITY)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "q=%d;", quantity);

        if(rtc_flag & NS_VUSER_OPTION_USER_VPTR)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "v=%s;", user_vptr);

        if(rtc_flag & NS_VUSER_OPTION_GEN)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "G=%s;", gen_id);
 
        //buff_len--; //TODO need to send extra null bute to NS as read_msg do not terminate buffer to  null
        buff[buff_len - 1] = '\0'; 
      //break;
      
      //case PAUSE_VUSER:
      //case RESUME_VUSER:
      //case STOP_VUSER:

/*      if(rtc_flag & NS_VUSER_OPTION_GROUP)
        { 
          if (strncasecmp(group, "ALL", 3))
            buff_len += snprintf(buff + buff_len , (size - buff_len), "g=%s;", group);
          else
            buff_len += snprintf(buff + buff_len ,(size - buff_len), "g=-1%s;", group + 3);
        }
        else
          buff_len += snprintf(buff + buff_len ,(size - buff_len), "g=-1;");
 
  
       if(rtc_flag & NS_VUSER_OPTION_USER_LIST)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "l=%s;", vuser_id);
      
        if(rtc_flag & NS_VUSER_OPTION_LIMIT)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "L=%d;", limit);
            
        if(rtc_flag & NS_VUSER_OPTION_OFFSET)
          buff_len += snprintf(buff + buff_len , (size - buff_len), "O=%d;", offset);


        buff_len--;
        buff[buff_len] = '\0'; 
      break;


      default :
        sprintf(err_msg, "Error: incorrect opcode %d", opcode);
        return -1; 
 
    }
*/
    //Fill opcode in msg_buf in File Parameter Case
    memcpy(msg_buff + MSG_LEN, &opcode, OPCODE_LEN);
    buff_len += OPCODE_LEN; 

    debug_log(DLOG_L1, _NLF_, "opcode = [%d]\n", opcode);
    //Fill lenght (payload + opcode) in msg_buf
    memcpy(msg_buff, &buff_len, MSG_LEN);
    buff_len += MSG_LEN; 
    debug_log(DLOG_L1, _NLF_, "rtc invoker buffer is %s ", msg_buff + 8);
    //buffer is ready

    if(write_msg_on_connection(msg_buff, con_fd, buff_len, err_msg) == FAILURE)
      PROC_RETURN(1, err_msg);

    int read_buf_int = 4; //1st four bytes of message is length.
    buff_len = 0;
    while(read_buf_int > 0)
    {
      if((rbytes = read(con_fd, read_buf + buff_len, read_buf_int)) <= 0) 
      {
        if(errno == ENETRESET || errno == ECONNRESET ) //In case parent dies after accepting the connection.
        {
          sprintf(err_msg, "FAILURE\nError: [Test is Stopped]");
          debug_log(DLOG_L1, _NLF_,"Error: Test is Stopped. read bytes = %d, buff_len = %d, read_buf_int = %d, errno(%d) = (%s)", 
                                    rbytes, buff_len, read_buf_int, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
        else if(errno == EAGAIN ) //In case parent is not responding.
        {
          sprintf(err_msg, "FAILURE\nError: [TimeOut: Netstorm is not responding]");
          debug_log(DLOG_L1, _NLF_,"Error: TimeOut: Netstorm is not responding read bytes = %d"
                                   "buff_len = %d, read_buf_int = %d, errno(%d) = (%s)", 
                                    rbytes, buff_len, read_buf_int, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
        else 
        {
          sprintf(err_msg, "FAILURE\nError: [Peer Connection Reset]. read bytes = %d, errno(%d) = (%s)", msg_len, errno, nslb_strerror(errno));
          debug_log(DLOG_L1, _NLF_,"Error: [Peer Connection Reset]. read bytes = %d, errno(%d) = (%s)", msg_len, errno, nslb_strerror(errno));
          PROC_RETURN(1, err_msg);
        }
      }
      buff_len += rbytes;
      read_buf_int -= rbytes;
    }
    read_buf[buff_len] = '\0'; 
    memcpy(&read_buf_size, read_buf, 4); 
  
    debug_log(DLOG_L1, _NLF_, "First 4 bytes of Message = [%d]", read_buf_size);

    while(read_buf_size > 0)
    {
      if( read_buf_size > 1024)
        buf_size = 1024;
      else
        buf_size = read_buf_size;

      if((rbytes = read(con_fd, read_buf, buf_size)) <= 0) 
      {
        //sprintf(err_msg, "FAILURE\nError: [Read Operation Failure] - Unable to receive message from NS. read bytes = %d, errno(%d) = (%s)",
        //                  rbytes, errno, nslb_strerror(errno));
        CHECK_REPLY_STATUS_FROM_NS
        debug_log(DLOG_L1, _NLF_,"Error: Message received from NS. read bytes = %d, errno(%d) = (%s)",
                          rbytes, errno, nslb_strerror(errno));
        PROC_RETURN(1, err_msg);
      }
      read_buf[rbytes] = '\0';
      debug_log(DLOG_L1, _NLF_, "Message recived from NS = %s\n", read_buf);
      printf("%s", read_buf);
      if(!(strncmp(read_buf, "Error:", 6)))
        return FAILURE;
 
      read_buf_size -= rbytes;
    }
    rtc_flag = 0;
    close(con_fd);
  

    if(opcode == PAUSE_VUSER || opcode == RESUME_VUSER || opcode == STOP_VUSER){
      set_rtc_type(opcode, rtc_type);
      show_and_append_all_log(opcode);
    }

    return 0;
  }
  else if(opcode != APPLY_FPARAM_RTC && opcode != TIER_GROUP_RTC && opcode != APPLY_MONITOR_RTC && opcode != APPLY_CAVMAIN_RTC)
    fill_buffer_and_write_msg(length, msg_buf, opcode, con_fd, err_msg);
  else {
    if(opcode == APPLY_MONITOR_RTC)
    {
      wlen += length;
    }  
    //Fill opcode in msg_buf in File Parameter Case
    memcpy(msg_buf + MSG_LEN, &opcode, OPCODE_LEN);
    wlen += OPCODE_LEN; 

    //Fill lenght (payload + opcode) in msg_buf
    memcpy(msg_buf, &wlen, MSG_LEN);
    wlen += MSG_LEN;
  
    debug_log(DLOG_L1, _NLF_, "<----Send MSG to NS---> len = %d, opcode = %d, msg = %s, wlen = %d", 
                               *((int *)msg_buf), *((int *)msg_buf + 1), (msg_buf + 8), wlen);
 
    if(write_msg_on_connection(msg_buf, con_fd, wlen, err_msg) == FAILURE)
      PROC_RETURN(1, err_msg);
  

    if((rbytes = read(con_fd, read_buf, 1024)) <= 0)
    {
      //sprintf(err_msg, "FAILURE\nError: [Read Operation Failure] - Unable to receive message from NS. read bytes = %d, errno(%d) = (%s)", 
      //                  rbytes, errno, nslb_strerror(errno));
      CHECK_REPLY_STATUS_FROM_NS
      debug_log(DLOG_L1, _NLF_,"Error: Unable to receive message from NS. read bytes = %d, errno(%d) = (%s)", 
                        rbytes, errno, nslb_strerror(errno));
      PROC_RETURN(1, err_msg);
    }
    close(con_fd);
    
    debug_log(DLOG_L1, _NLF_, "Message received from NS = %s, rbytes = %d\n", read_buf, rbytes);
    
    if((rbytes > 8) && !strcmp(read_buf + 4, "REJECT"))
    {
      debug_log(DLOG_L1, _NLF_,"rbytes = %d, read_buf = %s, opcode = %d", 
                      rbytes, read_buf + 4, *((int *)read_buf));
      set_rtc_type(*((int *)read_buf), rtc_type);
      printf("%s RTC is already in progress. Please try after sometime!\n", rtc_type);
    }
    else
    {
      printf("%s\n", read_buf);
      if(opcode != APPLY_CAVMAIN_RTC)
        show_and_append_all_log(opcode);
    }
   /* if(strcmp(read_buf, "SUCCESS"))
      PROC_RETURN(1, read_buf);
    
    PROC_RETURN(0, err_msg);*/
  }
}
