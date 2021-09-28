/********************************************************************************************
* Name                   : ns_wan_env.c 
* Purpose                : Holds function related to WAN simulation.
* Author                 : Arun Nishad 
* Intial version date    : Sept 9,08 
* Last modification date : -  
*
*
********************************************************************************************/

#include <sys/ioctl.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "cavmodem.h"
#include "ns_wan_env.h"
#include "nslb_util.h"
#include "util.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

// This is the max modems allowed in the kernel. If kernel is tuned, change this also
#define MAX_CAV_MODEMS 20480

/*

CavModems are two types:
1. Shared
2. Not Shared

Both can be used in many connections

NS uses shared for users which uses shared access

*/

//static int v_cavmodem_fd = 0;
int v_cavmodem_fd = 0;

/*Commented out-5th September,2016---Vivek- No need to check on runtime for kernel 4.2 */
/* Since in Kernel 3.13 TCP_BWEMU_OPT_BASE is set 26 so we need to check on runtime which kernel is using.*/
//static int var_TCP_BWEMU_OPT_BASE = TCP_BWEMU_OPT_BASE;
//static int var_TCP_BWEMU_RPD = TCP_BWEMU_OPT_BASE;
//static int var_TCP_BWEMU_REV_RPD = TCP_BWEMU_OPT_BASE + 1;
//static int var_TCP_BWEMU_DELAY = TCP_BWEMU_OPT_BASE + 2;
//static int var_TCP_BWEMU_REV_DELAY =  TCP_BWEMU_OPT_BASE + 3;
//static int var_TCP_BWEMU_COMPRESS = TCP_BWEMU_OPT_BASE + 4;
//static int var_TCP_BWEMU_REV_COMPRESS = TCP_BWEMU_OPT_BASE + 5;

//static int var_TCP_RTT = TCP_BWEMU_OPT_BASE + 6;
//static int var_TCP_ESTM_SPD = TCP_BWEMU_OPT_BASE + 7;

//static int var_TCP_BWEMU_JITTER = TCP_BWEMU_OPT_BASE + 8;
//static int var_TCP_BWEMU_REV_JITTER = TCP_BWEMU_OPT_BASE + 9;
//static int var_TCP_BWEMU_SET_ALL = TCP_BWEMU_OPT_BASE + 10;   // See the net/sock.h for the all structure 
//static int var_TCP_BWEMU_MODEM = TCP_BWEMU_OPT_BASE + 11;
//static int var_TCP_BWEMU_RELEASE_PAGE = TCP_BWEMU_OPT_BASE + 12;

static int var_TCP_BWEMU_SET_ALL = TCP_BWEMU_SET_ALL;   // See the net/sock.h for the all structure 

// This method will read the data in the modem file created on opening of a mode

// Used to log the information in the event log in case ioctl fails for debugging
#define MAX_MODEM_FILE_DATA 4096

inline void init_wan_setsockopt()
{
  char kernel_version[128 + 1] = "" ;
  char cmd[128 + 1] = "" ;
  char *kv_ptr = NULL;
  FILE *kernel_version_ptr = NULL;
  char kernel_mjv=0;
  char kernel_mnv=0;
  const char *delim = "."; 
  typedef void (*sighandler_t)(int);  

  NSDL2_WAN(NULL, NULL, "Method called");

  sprintf(cmd, "uname -r");
  
  NSDL2_WAN(NULL, NULL, "cmd = [%s]", cmd);

  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, SIG_IGN);

  kernel_version_ptr = popen(cmd, "r");
  if(kernel_version_ptr == NULL)
  {
    NS_EXIT(1, CAV_ERR_1000031, cmd, errno, nslb_strerror(errno));
  }
    
  nslb_fgets(kernel_version, (128 + 1), kernel_version_ptr, 0 );
  NSDL2_WAN(NULL, NULL, "kernel_version = %s", kernel_version);

  pclose(kernel_version_ptr);
  (void) signal( SIGCHLD, prev_handler);
 
  kv_ptr = strtok(kernel_version, delim);
  kernel_mjv = atoi(kv_ptr);
  NSDL2_WAN(NULL, NULL, "kernel_mjv = %d", kernel_mjv);
     
  kv_ptr = strtok(NULL, delim);
  kernel_mnv = atoi(kv_ptr);
  NSDL2_WAN(NULL, NULL, "kernel_mnv = %d", kernel_mnv); 

/*Commented out-5th September,2016---Vivek, no need to set since TCP_BWEMU_OPT_BASE has been set in cavmodem.h */

  if(((kernel_mjv == 3) && (kernel_mnv > 10)))
  {
    //var_TCP_BWEMU_OPT_BASE += 2; 
    //var_TCP_BWEMU_RPD += 2; 
    //var_TCP_BWEMU_REV_RPD += 2; 
    //var_TCP_BWEMU_DELAY  += 2;
   // var_TCP_BWEMU_REV_DELAY  += 2;
   // var_TCP_BWEMU_COMPRESS  += 2;
   // var_TCP_BWEMU_REV_COMPRESS  += 2;

   // var_TCP_RTT  += 2;
   // var_TCP_ESTM_SPD  += 2;

   // var_TCP_BWEMU_JITTER  += 2;
   // var_TCP_BWEMU_REV_JITTER  += 2;
    var_TCP_BWEMU_SET_ALL  += 2;
   // var_TCP_BWEMU_MODEM  += 2;
   // var_TCP_BWEMU_RELEASE_PAGE  += 2;
  }

  NSDL2_WAN(NULL, NULL, "Method Calling Completed");
}


static char *read_modem_file(int modem_id, char *buf, int buf_len)
{
char fname[1024];
int  size, fd;
  
  sprintf(fname, "/proc/driver/cavmodem/%d", modem_id);
  fd = open(fname, O_RDONLY|O_CLOEXEC);
  if(fd < 0)
  {
    sprintf(buf, "Error in opening modem file (%s), error = %s", fname, nslb_strerror(errno));
    return(buf);
  }
  if((size = read(fd, buf, buf_len)) <= 0)
  {
    sprintf(buf, "Error in reading modem file (%s), fd = %d, error = %s", fname, fd, nslb_strerror(errno));
    return(buf);
  }
  buf[size] = '\0'; // Null terminate
  return(buf);
}


int ns_cavmodem_init()
{

  int max = MAX_CAV_MODEMS; 
  char *ptr;
  
  if (!global_settings->wan_env) return 0;

  NSDL2_WAN(NULL, NULL, "Method called");

  ptr = getenv("NS_MAX_MODEM");
  if (ptr) max = atoi(ptr);

   if(max <= 0 || max > MAX_CAV_MODEMS)
  {
    fprintf(stderr, "Environment variable NS_MAX_MODEM value (%d) should be greater than 0 and less than %d for using internet simulation (WAN_ENV) mode. Set its value in %s/sys/site.env and restart tomcat (if using GUI) and logout/login again.\n", max, MAX_CAV_MODEMS, g_ns_wdir);
    return -1;
  }

  // NS_MAX_MODEM is already opened, return.
  if(v_cavmodem_fd)
  {
    NSDL2_WAN(NULL, NULL, "Skipping open. max = %d, v_cavmodem_fd = %d", max, v_cavmodem_fd);
    return 0;
  }

  if ((v_cavmodem_fd = open("/dev/cavmodem", O_RDWR|O_CLOEXEC)) == -1)
  {
    NS_EL_1_ATTR(EID_WAN_ENV, -1, -1,
                 EVENT_CORE, EVENT_CRITICAL, 
                 "open_cavmodem",
                 "Error in opening cavmodem. open(/dev/cavmodem) failed with error = %s", nslb_strerror(errno));
    fprintf(stderr, "Error in opening cavmodem. open(/dev/cavmodem) failed with error = %s", nslb_strerror(errno));
    return -1;
  }
  NSDL2_WAN(NULL, NULL, "Cavmodem open OK. max_modems = %d, cavmodem_fd = %d", max, v_cavmodem_fd);
  return 0;
}

// This is called every time new user is created
// User may be using shared acesss
void cav_open_modem(VUser *vptr)
{
char modem_file_buf[MAX_MODEM_FILE_DATA + 1]; // For reading modem file for logging 

  struct cav_class_params cv_params;

  if (global_settings->wan_env)
  {
    NSDL2_WAN(vptr, NULL, "Method called, NVM = %hd, shared_idx = %d,"
			  " UserId = %u, Profile = %s, Location = %s,"
			  " Access = %s, Browser = %s, Bandwidth (bits/sec)"
                          " (FW=%u, RV=%u), compression = %d",
                          my_child_index, vptr->access->shared_idx,
                          vptr->user_index, vptr->up_ptr->name,
			  vptr->location->name, vptr->access->name,
                          vptr->browser->name, vptr->access->fw_bandwidth,
                          vptr->access->rv_bandwidth, vptr->access->compression); 
			   
    if (vptr->access->shared_idx == -1) // If user is not using shared access
    {
      if (ioctl(v_cavmodem_fd, CAV_OPEN_MODEM, &(vptr->modem_id)))
      {
        NS_EL_1_ATTR(EID_WAN_ENV, vptr->user_index, vptr->sess_inst,
                     EVENT_CORE, EVENT_CRITICAL, 
                     "CAV_OPEN_MODEM",
                     "Error in getting NonShared modem for user. v_cavmodem_fd = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s", v_cavmodem_fd, nslb_strerror(errno));

        fprintf(stderr, "Error in getting NonShared modem for user. v_cavmodem_fd = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s", v_cavmodem_fd, nslb_strerror(errno));

        // Commened abort of test by Neeraj on Feb 5th, 2011 so that test may continue if we run out of modems for some time
        // end_test_run();
        vptr->modem_id = -1; // Force to -1 so that it not closed in close modem
        return;
      }
    
      NSDL2_WAN(vptr, NULL, "NonShared Modem opened. modem_id = %d, NVM = %hd, shared_idx = %d,"
			  " UserId = %u, Profile = %s, Location = %s,"
			  " Access = %s, Browser = %s, Bandwidth (bits/sec)"
                          " (FW=%d, RV=%d), compression = %d",
                          vptr->modem_id,
                          my_child_index, vptr->access->shared_idx,
                          vptr->user_index, vptr->up_ptr->name,
			  vptr->location->name, vptr->access->name,
                          vptr->browser->name, vptr->access->fw_bandwidth,
                          vptr->access->rv_bandwidth, vptr->access->compression); 
      cv_params.modem_id = vptr->modem_id;
      cv_params.params[0].conn_speed = vptr->access->fw_bandwidth;
      cv_params.params[1].conn_speed = vptr->access->rv_bandwidth;
      if (vptr->access->compression)
        cv_params.compression = 1;
      else
        cv_params.compression = 0;

      NSDL2_WAN(vptr, NULL, "Setting class for NonShared modem for user. modem_id = %d",
			    vptr->modem_id);
      if (ioctl(v_cavmodem_fd, CAV_SET_CLASS, &cv_params))
      {
        NS_EL_1_ATTR(EID_WAN_ENV, vptr->user_index, vptr->sess_inst,
                     EVENT_CORE, EVENT_CRITICAL, 
                     "CAV_SET_CLASS",
                     "Error in setting class for NonShared modem. v_cavmodem_fd = %d. "
                     "ioctl(CAV_SET_CLASS) failed with error = %s, "
                     "modem_id = %d, Profile = %s, Location = %s, Access = %s, "
                     "Browser = %s, Bandwidth (bits/sec) (FW=%d, RV=%d), compression = %d.\n"
                     "Modem file contents:\n %s",
                     v_cavmodem_fd, nslb_strerror(errno),
                     vptr->modem_id, vptr->up_ptr->name, vptr->location->name, vptr->access->name, 
                     vptr->browser->name, vptr->access->fw_bandwidth, vptr->access->rv_bandwidth, vptr->access->compression, 
                     read_modem_file(vptr->modem_id, modem_file_buf, MAX_MODEM_FILE_DATA)); 
        fprintf(stderr, "Error in setting class for NonShared modem. v_cavmodem_fd = %d. "
                     "ioctl(CAV_SET_CLASS) failed with error = %s, "
                     "modem_id = %d, Profile = %s, Location = %s, Access = %s, "
                     "Browser = %s, Bandwidth (bits/sec) (FW=%d, RV=%d), compression = %d",
                     v_cavmodem_fd, nslb_strerror(errno),
                     vptr->modem_id, vptr->up_ptr->name, vptr->location->name, vptr->access->name, 
                     vptr->browser->name, vptr->access->fw_bandwidth, vptr->access->rv_bandwidth, vptr->access->compression); 
        // end_test_run();
        return;
      }
      else
      {
        NSDL4_WAN(vptr, NULL, "Modem file contents: %s", read_modem_file(vptr->modem_id, modem_file_buf, MAX_MODEM_FILE_DATA)); 
      }

    }
    // User is using shared access, so use the modem of the shared access
    else
    {
      vptr->modem_id = vptr->access->shared_idx;
      NSDL2_WAN(vptr, NULL, "Using Shared Modem, modem_id = %d, NVM = %hd, shared_idx = %d,"
			  " UserId = %u, Profile = %s, Location = %s,"
			  " Access = %s, Browser = %s, Bandwidth (bits/sec)"
                          " (FW=%u, RV=%u), compression = %d",
                          vptr->modem_id,
                          my_child_index, vptr->access->shared_idx,
                          vptr->user_index, vptr->up_ptr->name,
			  vptr->location->name, vptr->access->name,
                          vptr->browser->name, vptr->access->fw_bandwidth,
                          vptr->access->rv_bandwidth, vptr->access->compression); 
    }

  }
}

// This is called during init time to get modem id for all shared accesses
int cav_open_shared_modem(AccAttrTableEntry *accAttrTable, AccAttrTableEntry_Shr *accattr_table_shr_mem)
{
char modem_file_buf[MAX_MODEM_FILE_DATA + 1]; // For reading modem file for logging 
struct cav_class_params cv_params;
 
  int modem_id; // Must be int

  NSDL2_WAN(NULL, NULL, "Method called. name = %s, accAttrTable->shared_modem = %d", BIG_BUF_MEMORY_CONVERSION(accAttrTable->name), accAttrTable->shared_modem);

  if (accAttrTable->shared_modem) // If access is shared
  {
    if (ioctl(v_cavmodem_fd, CAV_OPEN_SHARED_MODEM, &modem_id))
    {
      NS_EL_1_ATTR(EID_WAN_ENV, -1, -1,
                   EVENT_CORE, EVENT_CRITICAL, 
                   "CAV_OPEN_SHARED_MODEM",
                   "Error in getting Shared modem. v_cavmodem_fd = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s", v_cavmodem_fd, nslb_strerror(errno));
      fprintf(stderr, "Error in getting Shared modem. v_cavmodem_fd = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s", v_cavmodem_fd, nslb_strerror(errno));
      return -1;
    }
    NSDL2_WAN(NULL, NULL, "Shared Modem opened for %s (%d). modem_id = %d", BIG_BUF_MEMORY_CONVERSION(accAttrTable->name), accAttrTable->shared_modem, modem_id);

    accattr_table_shr_mem->shared_idx = modem_id;

    cv_params.modem_id = modem_id;
    cv_params.params[0].conn_speed = accAttrTable->fw_bandwidth;
    cv_params.params[1].conn_speed = accAttrTable->rv_bandwidth;
    if (accAttrTable->compression)
      cv_params.compression = 1;
    else
      cv_params.compression = 0;

    NSDL2_WAN(NULL, NULL, "Setting class for shared modem. modem_id = %d for NVM = %u, Bandwidth (bits/sec) (FW=%d, RV=%d), compression = %d", modem_id, my_child_index, cv_params.params[0].conn_speed, cv_params.params[1].conn_speed, cv_params.compression);
    if (ioctl(v_cavmodem_fd, CAV_SET_CLASS, &cv_params))
    {
      NS_EL_1_ATTR(EID_WAN_ENV, -1, -1,
                   EVENT_CORE, EVENT_CRITICAL, 
                   "CAV_SET_CLASS",
                   "Error in setting class for Shared modem. v_cavmodem_fd = %d. "
                   "ioctl(CAV_SET_CLASS) failed with error = %s, "
                   "modem_id = %d, Bandwidth (bits/sec) (FW=%d, RV=%d), compression = %d.\n" 
                   "Modem file contents:\n %s",
                   v_cavmodem_fd, nslb_strerror(errno),
                   modem_id, cv_params.params[0].conn_speed, cv_params.params[1].conn_speed, cv_params.compression, 
                   read_modem_file(modem_id, modem_file_buf, MAX_MODEM_FILE_DATA));

      // Also show error on console
      fprintf(stderr, "Error in setting class for Shared modem. v_cavmodem_fd = %d. "
                      "ioctl(CAV_SET_CLASS) failed with error = %s, "
                      "modem_id = %d, Bandwidth (bits/sec) (FW=%d, RV=%d), compression = %d", 
                      v_cavmodem_fd, nslb_strerror(errno),
                      modem_id, cv_params.params[0].conn_speed, cv_params.params[1].conn_speed, cv_params.compression);
      return -1;
    }
    else
    {
      NSDL4_WAN(NULL, NULL, "Modem file contents: %s", read_modem_file(modem_id, modem_file_buf, MAX_MODEM_FILE_DATA)); 
    }
  }
  else
  {
    NSDL2_WAN(NULL, NULL, "Not shared %s (%d).", BIG_BUF_MEMORY_CONVERSION(accAttrTable->name), accAttrTable->shared_modem);
    accattr_table_shr_mem->shared_idx = -1;
  }

  return 0;
}


// This is called after user is removed from the system
// If user is using shared access, then ioctl used for close will not do any thing
void cav_close_modem(VUser *vptr)
{
char modem_file_buf[MAX_MODEM_FILE_DATA + 1]; // For reading modem file for logging 
int modem_id = vptr->modem_id;

  if (modem_id != -1 )  // Not non wan, it is -1
  {
    if(vptr->access->shared_idx == -1) // Not Shared
      NSDL2_WAN(vptr, NULL, "Closing NonSharedModem. NVM = %u, UserId = %u, modem id = %d, shared = %d", my_child_index, vptr->user_index, modem_id, vptr->access->shared_idx);
    else
      NSDL2_WAN(vptr, NULL, "Closing SharedModem. It will not be closed by Kernel. NVM = %u, UserId = %u, modem id = %d, shared = %d", my_child_index, vptr->user_index, modem_id, vptr->access->shared_idx);

    // CAV_CLOSE_MODEM: (kernel)
    //   For shared modem, nothing is done and 0 is returned
    //   It marks state to Closed. If refrence count in kernel is 0, it frees cavmodem struct and make point for this modem NULL
    if (ioctl(v_cavmodem_fd, CAV_CLOSE_MODEM, &modem_id))
    {
      NS_EL_1_ATTR(EID_WAN_ENV, vptr->user_index, vptr->sess_inst,
                   EVENT_CORE, EVENT_CRITICAL, 
                   "CAV_CLOSE_MODEM",
                   "Error in closing modem, ignored. v_cavmodem_fd = %d, modem_id = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s.\n"
                   "Modem file contents:\n %s",
                   v_cavmodem_fd, modem_id, nslb_strerror(errno),
                   read_modem_file(vptr->modem_id, modem_file_buf, MAX_MODEM_FILE_DATA));

      fprintf(stderr, "Error in closing modem, ignored. v_cavmodem_fd = %d, modem_id = %d. ioctl(CAV_OPEN_MODEM) failed with error = %s", v_cavmodem_fd, modem_id, nslb_strerror(errno));
    }
    else
    {
      NSDL4_WAN(vptr, NULL, "Modem file contents: %s", read_modem_file(modem_id, modem_file_buf, MAX_MODEM_FILE_DATA)); 
    }
  }
}  

// This is called from parent at the start of the test so that
// all modems which are still open are closed (from prev test)
// PS: NS does not close shared modem as CAV_CLOSE_MODEM does not close shared modem
// Issue - If another test is running, then we will have issue

// Jan 7, 2011 - Added to call this at the end of the test also in 3.7.6

void clear_shared_modems()
{
int modem_id;

  if (!global_settings->wan_env) return;

  NSDL2_WAN(NULL, NULL, "Method called");
  if (ns_cavmodem_init() == -1)
  {
    fprintf(stderr, "Error: ns_cavmodem_init() failed. Ignored\n");
    return;
  }

  for(modem_id = 0; modem_id < MAX_CAV_MODEMS; modem_id++)
  {
    NSDL4_WAN(NULL, NULL, "Closing cavmodem %d", modem_id);

    /*
      This is saftey check if some one has set NS_MAX_MODEM > MAX_CAV_MODEMS, then it will try to close those modem.
      So CAV_CLOSE_MODEM failed. 
      Changed the logic to loop to MAX_CAV_MODEMS
    */

    // CAV_CLOSE_ALL_MODEMS: 
    //   Ignore class of modem. (shared or not). 
    //   If ref count is 0, then modem is released. Else it is marked as CLOSED so it cannot be
    //   used by any other ioctl/setsockopt. Also will not be returned in OPEN modem
    if (ioctl(v_cavmodem_fd, CAV_CLOSE_ALL_MODEMS, &(modem_id)))
    {
      NS_EL_1_ATTR(EID_WAN_ENV, -1, -1,
                   EVENT_CORE, EVENT_CRITICAL, 
                   "CAV_CLOSE_ALL_MODEMS",
                   "Error in closing modem for cleanup hanging modem if any, ignored. v_cavmodem_fd = %d, modem_id = %d. ioctl(CAV_CLOSE_ALL_MODEMS) failed with error = %s", v_cavmodem_fd, modem_id, nslb_strerror(errno));
      fprintf(stderr, "Error in closing modem for cleanup hanging modem if any, ignored. v_cavmodem_fd = %d, modem_id = %d. ioctl(CAV_CLOSE_ALL_MODEMS) failed with error = %s", v_cavmodem_fd, modem_id, nslb_strerror(errno));
    }
  }
}

//Method to get WAN ENV
//WAN_ENV 0
void kw_set_wan_env(char *value)
{
  NSDL2_WAN(NULL, NULL, "Method called. value=%s", value);
  global_settings->wan_env = atoi(value);
}

//ADVERSE_FACTOR LATENCY_FACTOR LOSS_FACTOR 
int kw_set_adverse_factor(char *buff, char *err_msg, int runtime_flag)
{ 
  int num, lat_fact, loss_fact;
  char keyword[512];

  NSDL2_WAN(NULL, NULL, "Method called. buff=%s", buff);
  if ((num = sscanf(buff, "%s %d %d", keyword, &lat_fact, &loss_fact)) != 3)
  {
    NS_KW_PARSING_ERR(buff, runtime_flag, err_msg, ADVERSE_FACTOR_USAGE, CAV_ERR_1011216, CAV_ERR_MSG_1);
  }
  if ((lat_fact < 0) || loss_fact < 0)
  {
    NS_KW_PARSING_ERR(buff, runtime_flag, err_msg, ADVERSE_FACTOR_USAGE, CAV_ERR_1011216, CAV_ERR_MSG_8);
  }
  global_settings->lat_factor = lat_fact;
  global_settings->loss_factor = loss_fact;
 
  return 0;
}

//WAN_JITTER FW_JITTER RV_JITTER 
int kw_set_wan_jitter(char *buff, char *err_msg, int runtime_flag) { 
  int num;
  char keyword[512];

  NSDL2_WAN(NULL, NULL, "Method called. buff=%s", buff);
  if ((num = sscanf(buff, "%s %d %d", keyword, &global_settings->fw_jitter, &global_settings->rv_jitter)) != 3)
  {
    NS_KW_PARSING_ERR(buff, runtime_flag, err_msg, WAN_JITTER_USAGE, CAV_ERR_1011217, CAV_ERR_MSG_1);
  }
  if ((global_settings->fw_jitter < 0) || global_settings->rv_jitter < 0)
  {
    NS_KW_PARSING_ERR(buff, runtime_flag, err_msg, WAN_JITTER_USAGE, CAV_ERR_1011217, CAV_ERR_MSG_8);
  }
 
  return 0;
}

#ifdef ENABLE_WAN_EX
//This method is called if WAN simulation is enabled & Linux is FC9/FC14
void set_socket_for_wan(connection *cptr, PerHostSvrTableEntry_Shr* svr_entry)
{
char modem_file_buf[MAX_MODEM_FILE_DATA + 1]; // For reading modem file for logging 
bwemu_modem_info c_info;

  VUser *vptr = cptr->vptr;

  /*In case of ftp cptr may be a data connection so old server entry can be NULL, so use svr_entry to log server info*/

  NSDL2_WAN(NULL, cptr, "Method called (For FC9) for Server = %s, Connection fd = %d, NVM = %d, UserId = %u, modem_id = %d, fw_drop_rate = %d%%, rv_drop_rate = %d%%, fw_fixed_delay = %d ms, rv_fixed_delay = %d ms, compression = %d, URL (tx_ratio = %d, rx_ratio = %d), fw_jitter = %d, rv_jitter = %d, loss_factor = %d", svr_entry->server_name, cptr->conn_fd, my_child_index, vptr->user_index, vptr->modem_id, vptr->location->linechar_array[svr_entry->loc_idx].fw_loss, vptr->location->linechar_array[svr_entry->loc_idx].rv_loss, vptr->location->linechar_array[svr_entry->loc_idx].fw_lat, vptr->location->linechar_array[svr_entry->loc_idx].rv_lat, vptr->access->compression, cptr->url_num->proto.http.tx_ratio, cptr->url_num->proto.http.rx_ratio, global_settings->fw_jitter, global_settings->rv_jitter, global_settings->loss_factor);

  if(vptr->modem_id < 0) // This can happen if open failed as we are continuing on open fail
  {
    NS_EL_1_ATTR(EID_WAN_ENV, vptr->user_index, vptr->sess_inst,
                 EVENT_CORE, EVENT_MINOR, 
                 "TCP_BWEMU_SET_ALL",
                 "Skipping setting socket option for WAN as modem was not opened sucessfully for this user");
    return;
  }

  c_info.modem_id       = vptr->modem_id;
  c_info.fw_drop_rate   = vptr->location->linechar_array[svr_entry->loc_idx].fw_loss * global_settings->loss_factor;
  c_info.rv_drop_rate   = vptr->location->linechar_array[svr_entry->loc_idx].rv_loss * global_settings->loss_factor;
  c_info.fw_fixed_delay = vptr->location->linechar_array[svr_entry->loc_idx].fw_lat * global_settings->lat_factor;
  c_info.rv_fixed_delay = vptr->location->linechar_array[svr_entry->loc_idx].rv_lat * global_settings->lat_factor;
  if (vptr->access->compression)
  {
    c_info.fw_compress  = cptr->url_num->proto.http.tx_ratio;
    c_info.rv_compress  = cptr->url_num->proto.http.rx_ratio;
  }
  else
  {
    c_info.fw_compress  = 0;
    c_info.rv_compress  = 0;
  } 
  c_info.fw_jitter = global_settings->fw_jitter;
  c_info.rv_jitter = global_settings->rv_jitter;
 
  NSDL2_WAN(NULL, cptr, "Setting socket option for WAN for NVM = %d, UserId = %u, modem_id = %d, fw_drop_rate = %d%%, rv_drop_rate = %d%%, fw_fixed_delay = %d ms, rv_fixed_delay = %d ms, fw_compression = %d, rv_compression = %d, fw_jitter = %d, rv_jitter = %d, var_TCP_BWEMU_SET_ALL = %d", my_child_index, vptr->user_index, vptr->modem_id, c_info.fw_drop_rate, c_info.rv_drop_rate, c_info.fw_fixed_delay, c_info.rv_fixed_delay, c_info.fw_compress, c_info.rv_compress, c_info.fw_jitter, c_info.rv_jitter, var_TCP_BWEMU_SET_ALL);

  if (setsockopt(cptr->conn_fd, SOL_TCP, var_TCP_BWEMU_SET_ALL, (char *) &c_info, sizeof(c_info)) < 0)
  {
    NS_EL_1_ATTR(EID_WAN_ENV, vptr->user_index, vptr->sess_inst,
                 EVENT_CORE, EVENT_CRITICAL, 
                 "var_TCP_BWEMU_SET_ALL",
                 "Error in setting socket option for WAN, modem_id = %d, socket_fd = %d. setsockopt(var_TCP_BWEMU_SET_ALL) failed with error = %s, fw_drop_rate = %d%%, rv_drop_rate = %d%%, fw_fixed_delay = %d ms, rv_fixed_delay = %d ms, fw_compression = %d, rv_compression = %d, fw_jitter = %d, rv_jitter = %d.\n Modem file contents:\n %s",
                 vptr->modem_id, cptr->conn_fd, nslb_strerror(errno), c_info.fw_drop_rate, c_info.rv_drop_rate, c_info.fw_fixed_delay, c_info.rv_fixed_delay, c_info.fw_compress, c_info.rv_compress, c_info.fw_jitter, c_info.rv_jitter, read_modem_file(vptr->modem_id, modem_file_buf, MAX_MODEM_FILE_DATA));

    fprintf(stderr, "Error in setting socket option for WAN, modem_id = %d, socket_fd = %d. setsockopt(var_TCP_BWEMU_SET_ALL) failed with error = %s, fw_drop_rate = %d%%, rv_drop_rate = %d%%, fw_fixed_delay = %d ms, rv_fixed_delay = %d ms, fw_compression = %d, rv_compression = %d, fw_jitter = %d, rv_jitter = %d", 
                 vptr->modem_id, cptr->conn_fd, nslb_strerror(errno), c_info.fw_drop_rate, c_info.rv_drop_rate, c_info.fw_fixed_delay, c_info.rv_fixed_delay, c_info.fw_compress, c_info.rv_compress, c_info.fw_jitter, c_info.rv_jitter);

    end_test_run();
  }
  else
  {
    NSDL4_WAN(vptr, NULL, "Modem file contents: %s", read_modem_file(vptr->modem_id, modem_file_buf, MAX_MODEM_FILE_DATA)); 
  }
}
#endif

//This function will be called form ns_browser_api.c 
//void get_wan_args_for_browser(connection *cptr, char *wan_args)
void get_wan_args_for_browser(VUser *vptr, char *wan_args, char *wan_access)
{
  int fw_bandwidth = 0;
  int rv_bandwidth = 0;
  short fw_drop_rate = 0;   // forword percentege loss
  short rv_drop_rate = 0;   // reverse percentege loss
  short fw_fixed_delay = 0; // forword fixed delay 
  short rv_fixed_delay = 0; // reverse fixed delay
  IW_UNUSED(int fw_compress = 0);    // Tx compression
  IW_UNUSED(int rv_compress = 0);    // Rx compression
  IW_UNUSED(int fw_jitter = 0);      // +- millisec (10 = +- 10MS)
  IW_UNUSED(int rv_jitter = 0);      // +- millisec (10 = +- 10MS)
  int loc_idx = 0;
 
  NSDL1_WAN(vptr, NULL, "Method called, vptr = %p, group_num = %d, default_svr_location = [%s], NVM = %d, UserId = %u", 
                         vptr, vptr->group_num, default_svr_location?default_svr_location:NULL, my_child_index, vptr->user_index);

  connection *cptr = vptr->last_cptr;
  /* Find default server location*/
  loc_idx = find_locattr_shr_idx(default_svr_location);
  NSDL2_WAN(vptr, NULL, "loc_idx = %d", loc_idx);
  if(loc_idx == -1) 
  {
    fprintf(stderr, "----Unknow loaction\n");
    sprintf(wan_args, "%s", "0:0:0:0:0:0:0");
    return;
  }

  //Manish: where should we freed this memory??? In ns_stop_browser() api ????
  //FREE_AND_MAKE_NULL(default_svr_location, "default_svr_location", -1);

  fw_bandwidth   = vptr->access->fw_bandwidth; 
  rv_bandwidth   = vptr->access->rv_bandwidth; 

  strcpy(wan_access , vptr->access->name);

  //If in scenario ADD_RECORDED_HOST keyword not used then use fw_bandwidth and rv_bandwidth only and rest attribute should be zero
  NSDL2_WAN(vptr, NULL, "total_inusesvr_entries = %d", total_inusesvr_entries);
  if(total_inusesvr_entries == 0)
  {
    sprintf(wan_args, "%d:%d:0:0:0:0:0", fw_bandwidth, rv_bandwidth);
    NSDL2_WAN(vptr, NULL, "wan_args = [%s]", wan_args);
    return;
  }

  fw_drop_rate   = vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].fw_loss * global_settings->loss_factor;
  rv_drop_rate   = vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].rv_loss * global_settings->loss_factor;
  fw_fixed_delay = vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].fw_lat * global_settings->lat_factor;
  rv_fixed_delay = vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].rv_lat * global_settings->lat_factor;

  // Changes for the bug 15269. Now we are getting the loc_idx from old_svr_entry

 /* fw_drop_rate   = vptr->location->linechar_array[loc_idx].fw_loss * global_settings->loss_factor;
  rv_drop_rate   = vptr->location->linechar_array[loc_idx].rv_loss * global_settings->loss_factor;
  fw_fixed_delay = vptr->location->linechar_array[loc_idx].fw_lat * global_settings->lat_factor;
  rv_fixed_delay = vptr->location->linechar_array[loc_idx].rv_lat * global_settings->lat_factor;

  */
  NSDL2_WAN(vptr, NULL, "compression = %d, fw_loss = %hd, rv_loss = %hd, loss_factor = %hd, fw_lat = %hd"
                        "rv_lat = %hd, lat_factor = %hd", 
                         vptr->access->compression, vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].fw_loss, 
                         vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].rv_loss, global_settings->loss_factor, 
                         vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].fw_lat, 
                         vptr->location->linechar_array[cptr->old_svr_entry->loc_idx].rv_lat, 
                         global_settings->lat_factor);

  
  if (vptr->access->compression)
  {
    //fw_compress  = cptr->url_num->proto.http.tx_ratio;
    //rv_compress  = cptr->url_num->proto.http.rx_ratio;
    IW_UNUSED(fw_compress  = 0); 
    IW_UNUSED(rv_compress  = 0); 
  }
  else
  {
    IW_UNUSED(fw_compress  = 0);
    IW_UNUSED(rv_compress  = 0);
  } 
  IW_UNUSED(fw_jitter = global_settings->fw_jitter);
  IW_UNUSED(rv_jitter = global_settings->rv_jitter);
 

  NSDL2_WAN(vptr, NULL, "dump:- fw_bandwidth = %d, rv_bandwidth = %d, fw_drop_rate = %hd%%, rv_drop_rate = %hd%%, "
                        "fw_fixed_delay = %hd ms, rv_fixed_delay = %hd ms, fw_compress = %d, rv_compress = %d, " 
                        "fw_jitter = %d, rv_jitter = %d", 
                         fw_bandwidth, rv_bandwidth, fw_drop_rate, rv_drop_rate, fw_fixed_delay, rv_drop_rate,
                         fw_compress, rv_compress, fw_jitter, rv_jitter);
  
  sprintf(wan_args, "%d:%d:0:%hd:%hd:%hd:%hd", fw_bandwidth, rv_bandwidth, fw_fixed_delay, rv_fixed_delay, fw_drop_rate, rv_drop_rate);
 NSDL2_WAN(vptr, NULL, "wan_args = [%s], wan_access = [%s]", wan_args, wan_access);
}
