/********************************************************************************
 * File Name            : ns_dynamic_hosts.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 15 February 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing function for MAX_DYNAMIC_HOST keyword,
 *                        to add dynamic host entries.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
#include "timing.h"
#include "tmr.h"

#include "url.h"
#include "nslb_cav_conf.h"
#include "nslb_sock.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "tr069/src/ns_tr069_lib.h"
#include "nslb_util.h"
#include "ns_dynamic_hosts.h"
#include "nslb_time_stamp.h"
#include "ns_dns_reporting.h"
#include "wait_forever.h"
#include "ns_group_data.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_cavmain_child_thread.h"

extern int host_id;
static int last_rec_svr_idx; //To save last recorded svr idx
// We are using _Shr struct, here it is'nt part of shared memory, but for making dynamic host 
// design compatible with netstorm.
#ifndef CAV_MAIN
static PerHostSvrTableEntry_Shr* actsvr_table_shr_mem = NULL;
int num_dyn_host_left;//Intialize number of dynamic host left.
#else
__thread PerHostSvrTableEntry_Shr* actsvr_table_shr_mem = NULL;
__thread int num_dyn_host_left;//Intialize number of dynamic host left.
#endif

//Debug Functions:
#ifdef NS_DEBUG_ON
static void actual_svr_dump_msg (PerHostSvrTableEntry_Shr *actsvr_table_shr_mem)
{
  NSDL3_HTTP(NULL, NULL, "Name = [%s]", actsvr_table_shr_mem->server_name);
  NSDL3_HTTP(NULL, NULL, "Host_id = [%d]", actsvr_table_shr_mem->host_id);
  NSDL3_HTTP(NULL, NULL, "Address = [%p]", actsvr_table_shr_mem->saddr);
  //NSDL3_HTTP(NULL, NULL, "Server already resolved = [%d]", totsvr_table_shr_mem->server_already_resolved);
}

static void rec_svr_dump_msg (SvrTableEntry_Shr *gserver_table_shr_mem)
{
  int i;
  NSDL2_HTTP(NULL, NULL, "Method called, total recorded server = %d, total_totsvr_entries = %d", total_svr_entries, total_totsvr_entries);
  for (i = 0; i < total_svr_entries; i++)
  {
    NSDL3_HTTP(NULL, NULL, "Index = [%d], Name = [%s]", i, gserver_table_shr_mem[i].server_hostname);
    NSDL3_HTTP(NULL, NULL, "Index = [%d], Length = [%d]", i, gserver_table_shr_mem[i].server_hostname_len);
    NSDL3_HTTP(NULL, NULL, "Index = [%d], idx = [%d]", i, gserver_table_shr_mem[i].idx);
    NSDL3_HTTP(NULL, NULL, "Index = [%d], Port = [%d]", i, gserver_table_shr_mem[i].server_port);
    NSDL3_HTTP(NULL, NULL, "Index = [%d], Type = [%d]", i, gserver_table_shr_mem[i].type);
    //NSDL3_HTTP(NULL, NULL, "Index = [%d], num_svrs = [%d]", i, gserver_table_shr_mem[i].num_svrs);
    NSDL3_HTTP(NULL, NULL, "Index = [%d], Actual Server = [%p]",i, gserver_table_shr_mem[i].totsvr_table_ptr);
  }
}
#endif

/*******************************************************************************************
 * Description        : kw_set_max_dyn_host() method used to parse MAX_DYNAMIC_HOST keyword,
 *                      This method is called from read_scripts_glob_vars() in ns_parse_scen_conf.c.
 * Format             : MAX_DYNAMIC_HOST <number of host>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 *           err_msg  : Error message.
 * Output Parameters  : Set max_dyn_host in struct GlobalData.
 * Return             : Retuns 0 for success and exit if fails.
 **************************************************************************************************************/

int kw_set_max_dyn_host(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num, dyn_host;
  int threshold_dns_time = 0;
  char num_dyn_host[MAX_DATA_LINE_LENGTH];
  char dns_threshold_time[MAX_DATA_LINE_LENGTH];
  num_dyn_host[0] = 0;
 
  //Fill default value 
  strcpy(dns_threshold_time, "2000");

  NSDL4_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, num_dyn_host, dns_threshold_time, tmp); // This is used to check number of arguments

  if((num < 2) || (num > 3)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_DYNAMIC_HOST_USAGE, CAV_ERR_1011075, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(num_dyn_host) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_DYNAMIC_HOST_USAGE, CAV_ERR_1011075, CAV_ERR_MSG_2);
  }
  dyn_host = atoi(num_dyn_host);

  if(ns_is_numeric(dns_threshold_time) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_DYNAMIC_HOST_USAGE, CAV_ERR_1011075, CAV_ERR_MSG_2);
  }

  threshold_dns_time = atoi(dns_threshold_time);
  
  global_settings->max_dyn_host = dyn_host;
  global_settings->dns_threshold_time_reporting = threshold_dns_time;
  num_dyn_host_left = global_settings->max_dyn_host;
  NSDL2_PARSING(NULL, NULL, "Exiting method, global_settings->max_dyn_host = %d", global_settings->max_dyn_host);

  return 0;
}

/******************************************************************
 * Function is called before forking nvms.  
 * 
 * If max_dyn_host > 0,then malloc gserver_table_shr_mem pointer 
 * with total_svr_entries + max_dyn_host
 * 
 * memcopy gserver_table_shr_mem
 * memset new entries(max_dyn_host)
 * 
 * change total_svr_entries and g_cur_server
 ******************************************************************/

void setup_rec_tbl_dyn_host(int max_dyn_host)
{
  int i, idx;
  int size;
  SvrTableEntry_Shr* dyn_gserver_table_shr_mem;

  NSDL1_HTTP(NULL, NULL, "Method called, max_dyn_host = %d", max_dyn_host);
  
  if (max_dyn_host == 0)
  {
    NSDL1_HTTP(NULL, NULL, "Returning as keyword disabled");
    return;
  }

  size = total_svr_entries + max_dyn_host;

  //Malloc gserver_table_shr_mem with number of dynamic host + total recorded server entries.
  MY_MALLOC(dyn_gserver_table_shr_mem, sizeof(SvrTableEntry_Shr) * size, "Dynamic RecSvr Tbl", -1);
  
  //Malloc actual host table
  MY_MALLOC(actsvr_table_shr_mem, sizeof(PerHostSvrTableEntry_Shr) * max_dyn_host, "Dynamic ActualSvr Tbl", -1);
    
  //memcopy shared recorded host as per total recorded host entries(init time). 
  memcpy(dyn_gserver_table_shr_mem, gserver_table_shr_mem, sizeof(SvrTableEntry_Shr) * total_svr_entries);

  /* Updating gserver_table_shr_mem.
   * TODO: Free shared memory struct gserver_table_shr_mem
   * Currently we are using same name as shared memory var, makes design compatible with current netstorm design 
   * */
  gserver_table_shr_mem = dyn_gserver_table_shr_mem;

  last_rec_svr_idx = total_svr_entries; // Here total_svr_entries is incremented
  idx = total_svr_entries;

  //memset dynamic host entries.
  memset(&gserver_table_shr_mem[total_svr_entries], 0, sizeof(SvrTableEntry_Shr) * max_dyn_host);
  memset(actsvr_table_shr_mem, 0, sizeof(PerHostSvrTableEntry_Shr) * max_dyn_host);

  NSDL2_HTTP(NULL, NULL, "Save last recorded host index, last_rec_svr_idx - 1 = %d", last_rec_svr_idx - 1);

  NSDL2_HTTP(NULL, NULL, "total_svr_entries = %d, g_cur_server = %d", total_svr_entries, g_cur_server);

  for(i = 0; i < max_dyn_host; i++)
  {  
    gserver_table_shr_mem[idx + i].totsvr_table_ptr = &actsvr_table_shr_mem[i];
    NSDL2_HTTP(NULL, NULL, "gserver_table_shr_mem[%d + %d].totsvr_table_ptr = [%p]", idx, i, gserver_table_shr_mem[idx + i].totsvr_table_ptr);
    total_svr_entries ++; //Increase total_svr_entries.
    g_cur_server ++; //Increase recorded host index for HostSvrEntry
  }

  //dump svr information
#ifdef NS_DEBUG_ON
  rec_svr_dump_msg(gserver_table_shr_mem);
  actual_svr_dump_msg(actsvr_table_shr_mem);
#endif

  NSDL2_HTTP(NULL, NULL, "Increment total_svr_entries = %d, g_cur_server = %d", total_svr_entries, g_cur_server);  
}

/*********************************************************************************** 
 * Function used to create dynamic_host.dat file to print dynamic hosts for each NVM
 * File is created in logs/TRxx/ready_reports directory
 * Format: TIME-STAMP|NVM_ID|URL|RESOLVED URL|HOST:PORT
 * Returns 0 in success case and -1 for failure
 ***********************************************************************************/

static int write_dynamic_host_file(unsigned char nvm_id, char *url, char *resolved_svr_name, char *hostname, short port, char *session_name, char *page_name, int user_id, char *scenario_group)
{
  FILE *fp;
  //char wdir[1024];
  char dynhostpath[2024];
  char file_name[1024];
  static int print_dyn_host_file_header = 0; //To print header in dynamic host file

  NSDL2_HTTP(NULL, NULL, "Method called, print_dyn_host_file_header = %d", print_dyn_host_file_header);

#if 0
  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
    sprintf(dynhostpath, "%s/logs/TR%d", wdir, testidx);
  }
  else
  {
    fprintf(stderr, "Error in getting the NS_WDIR\n");
    return(-1); 
  }
#endif
  sprintf(dynhostpath, "%s/logs/TR%d", g_ns_wdir, testidx);
  sprintf(file_name, "%s/ready_reports/dynamic_host.dat", dynhostpath);

  if((fp = fopen(file_name, "a")) == NULL)
  {
    fprintf(stderr, "Error in opening %s file.\n", file_name);
    return -1;
  }
  if (!print_dyn_host_file_header)
  {
    fprintf(fp, "TIME_STAMP|NVM_ID|USER_ID|GROUP|SCRIPT|PAGE|URL|RESOLVED HOST|HOST:PORT\n");
    print_dyn_host_file_header = 1;
  }  
  //Format: TIME_STAMP|NVM_ID|USER_ID|GROUP|SCRIPT|PAGE|URL|RESOLVED HOST|HOST:PORT
  fprintf(fp, "%s|%d|%d|%s|%s|%s|%s|%s|%s:%d\n", get_relative_time(), nvm_id, user_id, scenario_group, session_name, page_name, url, resolved_svr_name, hostname, port);
  fclose(fp);
  return 0;
}

static int write_dynamic_host_in_slog(char *svr_name, int host_id)
{
  int d_host_index = my_port_index + 1;
  d_host_index <<= 24;
  int new_host_id = d_host_index |= host_id;

  NSDL1_HTTP(NULL, NULL, "Method Called, svr_name = %s, host_id = %d, new_host_id = %d", svr_name, host_id, new_host_id); 
  fprintf(static_logging_fp, "HOSTTABLE:%s,%d,%d,%d\n", svr_name, new_host_id, ((g_generator_idx == -1)?0:g_generator_idx), my_port_index);
  fflush(static_logging_fp);

  return 0;
}

#ifndef CAV_MAIN
static int num_dyn_host_add = 0;
#else
__thread int num_dyn_host_add = 0;
#endif
 
/*******************************************************************************
 * Description      : Function used to
 *                    1)  Check keyword enable/disable
 *                    2)  Before adding new host, check whether we have exceeded
 *                        number_of_host(mentioned in keyword). 
 *                    3)  add new host entry in recorded and actual host
 *                        tables.
 * Input Parameters 
 *       hostname   : new host name
 *       port       : port number
 *                    Rest arguments are for debugging purpose
 *
 * Output Parameters: Update num_dyn_host_left used for searching host in recorded 
 *                    table
 *
 * Returns          : Success case: returns recorded host index, 
 *                    Error case  : -1 Host limit exceeded the number_of_host 
 *                                     or big_buf tbl is full   
 *                                  -2 IP is not resolved
 *                                  -3 Keyword disable
 ******************************************************************************/

int add_dynamic_hosts (VUser *vptr, char *hostname, short port, int main_or_inline, int redirect_flag, int request_type, char *url, char *session_name, char *page_name, int user_id, char *scenario_group)
{
  int rec_host_idx;
  char *host_name;
  unsigned int start_time, diff_timestamp;
  unsigned int end_time;
  char err_msg[1024]; 
  //int local_max_id = 0;

  NSDL1_HTTP(NULL, NULL, "Method called. hostname = [%s], port = [%d], num_dyn_host_add = [%d], request_type = [%d], url = [%s], main_or_inline = [%d], redirect_flag = [%d], session_name = [%s], page_name = [%s]", hostname, port, num_dyn_host_add, request_type, url, main_or_inline, redirect_flag, session_name, page_name);

  //Check whether max_dyn_host keyword enable
  if (global_settings->max_dyn_host == 0)
  {
    NSDL1_HTTP(NULL, NULL, "Returning as keyword disable.");
    NS_EL_3_ATTR(EID_DYNAMIC_HOST, -1, -1, EVENT_CORE, EVENT_WARNING, 
                         session_name, page_name, hostname,
                         "Dynamic host table is not enabled. Cannot process request for %s%sURL %s."
                         "Enable dynamic host table by setting maximum dynamic hosts run time settings and run test again.",
                          main_or_inline == 1 ? "MAIN" : "INLINE", redirect_flag == 0 ? " " : " redirected ", url);
    return(ERR_DYNAMIC_HOST_DISABLE);
  }

  //Check whether we have exceeded number_of_host(mentioned in keyword)
  if (num_dyn_host_add >= global_settings->max_dyn_host)
  {
    NSDL1_HTTP(NULL, NULL, "Host limit exceeded the number_of_host mentioned in keyword.");

    NS_EL_3_ATTR(EID_DYNAMIC_HOST,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         session_name, page_name, hostname,
                         "Host table is full for host %s:%s. Cannot process request for %s%sURL %s. "
                         "Increase maximum dynamic hosts run time settings and run test again.",
                         hostname, request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP", main_or_inline == 1 ? "MAIN" : "INLINE",
                         redirect_flag == 0 ? " " : " redirected ", url);
    return(ERR_DYNAMIC_HOST_TBL_FUL);
  }
  
  //Malloc host name
  int host_name_len = strlen(hostname);
  MY_MALLOC (host_name, host_name_len + 1, "Host name", -1);
  strcpy(host_name, hostname);
  NSDL2_HTTP(NULL, NULL, "host_name = %s", host_name); 

  start_time = get_ms_stamp(); //time before resolving host
  NSDL2_HTTP(NULL, NULL, "Time before resolving dynamic host, start_time = %d", start_time); 
  
  INCREMENT_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr);
  int grp_idx = vptr->group_num;
  if (ns_get_host(grp_idx, &actsvr_table_shr_mem[num_dyn_host_add].saddr, hostname, port, err_msg) == -1)
  {
    end_time = get_ms_stamp();
    diff_timestamp = end_time - start_time;//time taken while resolving host
    INCREMENT_DNS_LOOKUP_FAILURE_COUNTER(vptr); 
    UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, diff_timestamp);
    NSDL2_HTTP(NULL, NULL, "Error is resolving host %s:%s , threshold time given by user %d ms whereas total time taken %d ms. HostErr %s", hostname, request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP", global_settings->dns_threshold_time_reporting, diff_timestamp, err_msg);
    NS_EL_3_ATTR(EID_DYNAMIC_HOST,  -1, -1, EVENT_CORE, EVENT_WARNING,
                      session_name, page_name, hostname,
                         "Error is resolving host %s:%s, total time taken %d ms. Cannot process request for %s%sURL %s. HostErr %s",
                          hostname, request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP", diff_timestamp, main_or_inline == 1 ? "MAIN" : "INLINE",
                         redirect_flag == 0 ? " " : " redirected ", url, err_msg);
    FREE_AND_MAKE_NULL_EX(host_name, host_name_len + 1, "host_name", 1); //added struct size.
    return(ERR_IP_NOT_RESOLVED);
  }
  
  end_time = get_ms_stamp();//time taken to resolve host
  actsvr_table_shr_mem[num_dyn_host_add].last_resolved_time = end_time;
  diff_timestamp = end_time - start_time;
  UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, diff_timestamp);
  NSDL2_HTTP(NULL, NULL, "Time taken after resolving dynamic host, end_time = %d, diff_timestamp = %d, threshold_time = %d", end_time, diff_timestamp, global_settings->dns_threshold_time_reporting); 
  //Report event if time taken to resolve host is greater than threshold time set for DNS lookup
  if(diff_timestamp > global_settings->dns_threshold_time_reporting)
  {
    NSDL2_HTTP(NULL, NULL, "Total time taken to resolve host %s:%s was %d ms whereas threshold time given by user was %d ms", hostname, request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP", diff_timestamp, global_settings->dns_threshold_time_reporting);
    NS_EL_3_ATTR(EID_DYNAMIC_HOST,  -1, -1, EVENT_CORE, EVENT_WARNING,
                    session_name, page_name, hostname,
                    "Total time taken to resolve host %s:%s is %d ms.",
                    hostname, request_type == HTTPS_REQUEST ? "HTTPS" : "HTTP", diff_timestamp);
    
  }
  strcpy(actsvr_table_shr_mem[num_dyn_host_add].server_name, host_name);  
  /* Changes done for bug#5079, added server name length struct member to optimize 
   * strlen in make_request()  while sending host name header*/
  actsvr_table_shr_mem[num_dyn_host_add].server_name_len = host_name_len;  
  //actsvr_table_shr_mem[num_dyn_host_add].server_already_resolved = 1;
  actsvr_table_shr_mem[num_dyn_host_add].server_flags |= NS_SVR_FLAG_SVR_ALREADY_RESOLVED;
  
  host_id++;

  //Manish: 16 April 2013: Setting flag to know host is with port or without port
  //This flag will used in ns_proxy_server.c file when making CONNECT Method 
  NSDL2_HTTP(NULL, NULL, "Checking host [%s] contain port or not?", host_name);
  if(!index (host_name, ':')) //If Port not found in server host
  {
     NSDL2_HTTP(NULL, NULL, "Setting server_flag for hostname '%s' is NS_SVR_FLAG_SVR_WITHOUT_PORT: actsvr_table_shr_mem[%d] = %p", 
                             actsvr_table_shr_mem[num_dyn_host_add].server_name, num_dyn_host_add, actsvr_table_shr_mem[num_dyn_host_add]); 
     actsvr_table_shr_mem[num_dyn_host_add].server_flags |= NS_SVR_FLAG_SVR_WITHOUT_PORT; 
  }

  // Inc index before adding host
  NSDL2_HTTP(NULL, NULL, "Last recorded server index = %d", last_rec_svr_idx);
  rec_host_idx = last_rec_svr_idx + num_dyn_host_add;
  //add new entry into recorded host table
  NSDL2_HTTP(NULL, NULL, "Adding recorded host at incremented index = %d", rec_host_idx);
  gserver_table_shr_mem[rec_host_idx].server_hostname = host_name;
  gserver_table_shr_mem[rec_host_idx].server_hostname_len = host_name_len;
  gserver_table_shr_mem[rec_host_idx].idx = rec_host_idx; //index in the gserver_table_shr_mem
  gserver_table_shr_mem[rec_host_idx].server_port = port;
  gserver_table_shr_mem[rec_host_idx].type = SERVER_ANY;
  //gserver_table_shr_mem[rec_host_idx].num_svrs =  1;  // mapped to itself

  //Debugging functions
#ifdef NS_DEBUG_ON
  rec_svr_dump_msg(gserver_table_shr_mem);
  actual_svr_dump_msg(actsvr_table_shr_mem);
#endif
  #ifndef CAV_MAIN
  write_dynamic_host_file(my_port_index, url, nslb_sockaddr_to_ip((struct sockaddr *)&actsvr_table_shr_mem[num_dyn_host_add].saddr, port), hostname, port, session_name, page_name, user_id, scenario_group);

  write_dynamic_host_in_slog(nslb_sockaddr_to_ip((struct sockaddr *)&actsvr_table_shr_mem[num_dyn_host_add].saddr, port), host_id);
  #endif
  //Increase dynamic host limit and decrement num_dyn_host
  num_dyn_host_add ++;
  num_dyn_host_left --;
  #ifdef CAV_MAIN 
  vptr->sm_mon_info->num_dyn_host_add = num_dyn_host_add;
  vptr->sm_mon_info->num_dyn_host_left = num_dyn_host_left;
  #endif
  
  NSDL1_HTTP(NULL, NULL, "num_dyn_host_add = %d, num_dyn_host_left = %d", num_dyn_host_add, num_dyn_host_left);
  NSDL1_HTTP(NULL, NULL, "Exiting method. host_index =%d", rec_host_idx);

  return rec_host_idx;

}
