/*********************************************************************************************
 * Name                   : ns_sesion_pacing.c
 * Purpose                : This file contains function for
 *                           Parsing
 *                           Runtime changes for these three keywords
 *                           G_SESSION_PACING
 *                           G_FIRST_SESSION_PACING
 *                           G_NEW_USER_ON_SESSION
 *                           Starting of pacing timer based on setting
 * Author                 : Nikita Pandey
 * Intial version date    : Thrusday, November 10 2011
 * *********************************************************************************************/


#include <libgen.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pwd.h>

#include <gsl/gsl_randist.h>

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
#include "ns_trace_level.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

/*
#include "ns_auto_cookie.h"
#include "ns_http_process_resp.h"
#include "ns_percentile.h"
#include "ns_debug_trace.h"
#include "ns_custom_monitor.h"
#include "ns_user_monitor.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_check_monitor.h"
#include "ns_monitor_profiles.h"
#include "ns_sock_list.h"
#include "ns_wan_env.h"
#include "ns_sock_com.h"
#include "ns_cookie.h"
#include "ns_parse_src_ip.h"
*/

#include "ns_alloc.h"
#include "ns_trans_parse.h"
#include "ns_cpu_affinity.h"
#include "ns_event_log.h"
#include "ns_parse_scen_conf.h"
#include "ns_kw_set_non_rtc.h"

#include "ns_replay_access_logs_parse.h"
#include "ns_replay_access_logs.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases_parse_validations.h"
#include "ns_standard_monitors.h"
#include "ns_trans.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_smtp_parse.h"
#include "ns_parse_scen_conf.h"
#include "ns_page.h"
#include "ns_pop3_parse.h"
#include "ns_ftp_parse.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_keep_alive.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_http_cache.h"
#include "init_cav.h"
#include "ns_js.h"
#include "ns_vuser_ctx.h"
#include "ns_script_parse.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif

#include "ns_trans_parse.h"
#include "ns_vuser_trace.h"
#include "tr069/src/ns_tr069_lib.h"
#include "nslb_util.h"
#include "ns_url_hash.h"
//#include "ns_auto_fetch_parse.h"
#include "ns_session_pacing.h"
#include "ns_group_data.h"

/* Description       : Method used to allocate memory to session_pacing table.
 * Input Parameters
 * row_num           : Pointer to row number.
 * Output Parameters : Set total entries of total_pacing_entries.
 * Return            : Return -1 on error in allocating memory to pacing table else return 1.
 */
     
static int create_pacing_table_entry(int *row_num)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  if(total_pacing_entries ==  max_pacing_entries)
  {
    MY_REALLOC_EX(pacingTable, (max_pacing_entries + DELTA_PACING_ENTRIES) * sizeof(PacingTableEntry), (total_pacing_entries) * sizeof(PacingTableEntry), "pacingTable", -1);
    if(!pacingTable)
    {
      fprintf(stderr, "create_pacing_table_entry(): Error allocating more memory for pacing enties\n");
      return FAILURE;
    }else max_pacing_entries += DELTA_PACING_ENTRIES;
  }
  *row_num = total_pacing_entries++;
  return (SUCCESS);
}

/* Description       : Method used to create default pacing  for default value of session pacing keyword.
 * Input Parameters  : None
 * Output Parameters : Set pacing table entries.
 * Return            : None
 */


void create_default_pacing(void) {
  int rnum;

  NSDL2_PARSING(NULL, NULL, "Method called");
  if (create_pacing_table_entry(&rnum) == FAILURE) {
    NS_EXIT(-1, "Error in creating pacing entry");
  }

  assert(rnum == 0);

  pacingTable[0].pacing_mode = SESSION_PACING_MODE_NONE;
  pacingTable[0].refresh = DO_NOT_REFRESH_USER_ON_NEW_SESSION;
  pacingTable[0].first_sess = SESSION_PACING_ON_FIRST_SESSION_OFF;
  pacingTable[0].think_mode = SESSION_PACING_TIME_MODE_CONSTANT;
  pacingTable[0].time = 0;
  pacingTable[0].retain_param_value = 0;
  pacingTable[0].retain_cookie_val = 1;
}

char *session_pacing_mode_to_str(int mode)
{
  NSDL2_PARSING(NULL, NULL, "Method called. mode = %d", mode);
  switch(mode)
  {
    case SESSION_PACING_MODE_NONE:
      return("NoSessionPacing");
    case SESSION_PACING_MODE_AFTER:
      return("AfterCompletionOfPreviousSession");
    case SESSION_PACING_MODE_EVERY:
      return("EveryInterval");
    default:
      return("InvalidPacingMode");
  }    
}

char *session_pacing_time_mode_to_str(int mode)
{
  NSDL2_PARSING(NULL, NULL, "Method called. mode = %d", mode);
  switch(mode)
  {
    case SESSION_PACING_TIME_MODE_CONSTANT:
      return("Constant");
    case SESSION_PACING_TIME_MODE_INTERNET_RANDOM:
      return ("InternetRandom");
    case SESSION_PACING_TIME_MODE_UNIFORM_RANDOM:
      return("UniformRandom");
   default:
      return("InvalidTimeMode");
 }
}


char *first_sess_str(int first_sess)
{
  NSDL2_PARSING(NULL, NULL, "Method called. first_sess = %d", first_sess);
  switch(first_sess)
  {
   case SESSION_PACING_ON_FIRST_SESSION_OFF:
     return("FirstSessionPacingOff");
   case SESSION_PACING_ON_FIRST_SESSION_ON:
     return("FirstSessionPacingOn");
   default:
     return("InvalidFirstSession");
  }
}

char *new_user_on_sess_str(int refresh)
{
  NSDL2_PARSING(NULL, NULL, "Method called. refresh = %d",refresh);
  switch(refresh)
  {
   case DO_NOT_REFRESH_USER_ON_NEW_SESSION:
     return("SameUserContinue");
   case REFRESH_USER_ON_NEW_SESSION:
     return("NewUserOnEveryNewSession");
  default:
     return("InvalidRefreshOption");
  }

}

#define SESS_PACING_TIME_TO_STR(pacing_ptr, buf) \
  if (pacing_ptr->think_mode == SESSION_PACING_TIME_MODE_CONSTANT) \
  { \
    sprintf(buf, "with a fixed delay of %.3f seconds", NS_MS_TO_SEC(pacing_ptr->time)); \
  } \
  else if (pacing_ptr->think_mode == SESSION_PACING_TIME_MODE_INTERNET_RANDOM) \
  { \
    sprintf(buf, "with a random (Internet type distribution) average delay of %.3f seconds", NS_MS_TO_SEC(pacing_ptr->time)); \
  }  \
  else  \
  { \
    sprintf(buf, "with a random (Uniform distribution) delay of %.3f seconds to %.3f seconds", NS_MS_TO_SEC(pacing_ptr->time), NS_MS_TO_SEC(pacing_ptr->max_time)); \
  } \
  return(buf);

char *sess_pacing_time_to_str(PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  SESS_PACING_TIME_TO_STR(pacing_ptr, buf);
}

char *sess_pacing_time_to_str_non_shm(PacingTableEntry *pacing_ptr, char *buf)
{
  SESS_PACING_TIME_TO_STR(pacing_ptr, buf);
}

#define SESSION_GROUP_NAME \
{ \
  if (grp_idx!= -1) \
  { \
    if(runProfTable) \
      strcpy(grp_name, BIG_BUF_MEMORY_CONVERSION(runProfTable[grp_idx].scen_group_name)); \
    else \
      strcpy(grp_name,runprof_table_shr_mem[grp_idx].scen_group_name); \
  } \
}

#ifdef NS_DEBUG_ON
static char *session_pacing_to_str(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  char *ptr;
  char grp_name[256] = "ALL";

  SESSION_GROUP_NAME;

  sprintf(buf, "Group = %s, PacingMode = %s, TimeMode = %s",
          grp_name,
          session_pacing_mode_to_str(pacing_ptr->pacing_mode),
          session_pacing_time_mode_to_str(pacing_ptr->think_mode));
  ptr = buf + strlen(buf);

  sess_pacing_time_to_str(pacing_ptr, ptr);

  return(buf);  // We are returning buf also so that calling function can use this in debug directly
}


static char *first_session_to_str(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  char grp_name[256] = "ALL";
  SESSION_GROUP_NAME;

  sprintf(buf, "Group = %s, First Session = %s", grp_name, first_sess_str(pacing_ptr->first_sess));
  return(buf);
}

static char *new_user_on_session_to_str(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  char grp_name[256] = "ALL";
  SESSION_GROUP_NAME;

  sprintf(buf,"Group = %s, First Session = %s", grp_name, new_user_on_sess_str(pacing_ptr->refresh));
  return(buf);
}
#endif

static char *session_pacing_to_str_runtime(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{ 
  //char *ptr;
  char grp_name[256] = "ALL";
  char buff[4096];

  SESSION_GROUP_NAME;
  //ptr = buf + strlen(buf);
  //sess_pacing_time_to_str(pacing_ptr, ptr);

  switch(pacing_ptr->pacing_mode) {
    
    case SESSION_PACING_MODE_NONE:
      sprintf(buf,"Run time changes applied to Session Pacing for scenario group  '%s'."
                  " \'New setting is As soon as the previous session ends.\'", grp_name);
    break;

    case SESSION_PACING_MODE_AFTER:
      sprintf(buf,"Run time changes applied to Session Pacing for scenario group '%s'."
                  " \'New setting is After the previous session ends is %s\'", grp_name,
                  sess_pacing_time_to_str(pacing_ptr, buff));
    break;
   
    case SESSION_PACING_MODE_EVERY:
      sprintf(buf,"Run time changes applied to Session Pacing for scenario group '%s'."
                  " \'New setting is Once every interval (Provided that the previous session"
                  " ends by that time else next session starts as soon as previous session ends) %s\'",
                  grp_name, sess_pacing_time_to_str(pacing_ptr, buff));
    break;
   
    default:
     sprintf(buf,"Error: Invalid value of session pacing mode (%d). Ignored", pacing_ptr->pacing_mode);
     break;
  }   
  return(buf);  // We are returning buf also so that calling function can use this in debug directly

}

static char *first_session_to_str_runtime(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  char grp_name[256] = "ALL";
  SESSION_GROUP_NAME;
  
  if(pacing_ptr->first_sess == SESSION_PACING_ON_FIRST_SESSION_OFF){
    sprintf(buf, "Run time changes applied to First Session Pacing for Scenario Group = '%s'.\'New setting is First Session is disable \'", grp_name);
  }
  if(pacing_ptr->first_sess == SESSION_PACING_ON_FIRST_SESSION_ON){
    sprintf(buf, "Run time changes applied to First Session Pacing for Scenario Group = '%s'.\'New setting is Introduce delay before first Session too (By randomized mean pacing time)\'", grp_name);
  }

  return(buf);
}
                       
static char *new_user_on_session_to_str_runtime(int grp_idx, PacingTableEntry_Shr *pacing_ptr, char *buf)
{
  char grp_name[256] = "ALL";
  SESSION_GROUP_NAME;

  if(pacing_ptr->refresh == DO_NOT_REFRESH_USER_ON_NEW_SESSION) {
    sprintf(buf,"Run time changes applied to New User On Session Scenario Group = '%s'.\'New setting is New user on every session is disable\'", grp_name);
  }
  if(pacing_ptr->refresh == REFRESH_USER_ON_NEW_SESSION) {
    sprintf(buf,"Run time changes applied to New User On Session Scenario Group = '%s'.\'New setting is Simulate a new user on each session\'", grp_name);
  }
  return(buf);
}

#ifdef NS_DEBUG_ON
static char *session_pacing_to_str_non_shm(int grp_idx, PacingTableEntry *pacing_ptr, char *buf)
{
  char *ptr;
  sprintf(buf, "Group = %s, PacingMode = %s, TimeMode = %s", 
          grp_idx == -1? "ALL": RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name),
          session_pacing_mode_to_str(pacing_ptr->pacing_mode),
          session_pacing_time_mode_to_str(pacing_ptr->think_mode));

  ptr = buf + strlen(buf);
  sess_pacing_time_to_str_non_shm(pacing_ptr, ptr);
  return(buf);  // We are returning buf also so that calling function can use this in debug directly
}

static char *first_session_to_str_non_shm(int grp_idx, PacingTableEntry *pacing_ptr, char *buf)
{
  sprintf(buf, "Group = %s, First Session = %s", grp_idx == -1? "ALL": RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), first_sess_str(pacing_ptr->first_sess));
  return(buf);
}

static char *new_user_on_session_to_str_non_shm(int grp_idx, PacingTableEntry *pacing_ptr, char *buf)
{
  sprintf(buf, "Group = %s, Refresh = %s", grp_idx == -1? "ALL": RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), new_user_on_sess_str(pacing_ptr->refresh));
  return(buf);
}

#endif

/* Description       : Method copy_to_shr() used to copy pacing table in shared memory
 *                     Add pacing value for total number of session across all scenario groups.
 *                     This method is called from copy_structs_into_shared_mem()in util.c.
 * Input Parameters  : None
 * Output Parameters : Set pacing table entries.
 * Return            : None
*/

void copy_session_pacing_to_shr(void)
{
  int i;
#ifdef NS_DEBUG_ON
  char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
#endif

  NSDL2_PARSING(NULL, NULL, "Method called. total_pacing_entries = %d", total_pacing_entries);
  if (!total_pacing_entries) // No session pacing deifned
    return;

  int total_num_session = 0; // Total number of sessions across all scenario groups
  for (i = 0; i < total_runprof_entries; i++)
  {
    total_num_session += runProfTable[i].pacing_idx;
    NSDL4_PARSING(NULL, NULL, "total_num_pages = %d", total_num_session);
  }
  pacing_table_shr_mem = (PacingTableEntry_Shr*) do_shmget(sizeof(PacingTableEntry_Shr) * total_runprof_entries, "pacing table");
  /* For each session we fill the ptr */
  for (i = 0; i < total_runprof_entries; i++) {
    int rp_pt_idx = runProfTable[i].pacing_idx; // Index of pacing table for the current scenario group (run prof)
    pacing_table_shr_mem[i].pacing_mode = pacingTable[rp_pt_idx].pacing_mode;
    pacing_table_shr_mem[i].think_mode = pacingTable[rp_pt_idx].think_mode;
    pacing_table_shr_mem[i].time = pacingTable[rp_pt_idx].time;
    pacing_table_shr_mem[i].max_time = pacingTable[rp_pt_idx].max_time;

    pacing_table_shr_mem[i].first_sess = pacingTable[rp_pt_idx].first_sess;
    pacing_table_shr_mem[i].refresh = pacingTable[rp_pt_idx].refresh;
    pacing_table_shr_mem[i].retain_param_value = pacingTable[rp_pt_idx].retain_param_value;
    pacing_table_shr_mem[i].retain_cookie_val = pacingTable[rp_pt_idx].retain_cookie_val;

    NSDL3_PARSING(NULL, NULL, "SessionPacing: Shared Memory Info - %s", session_pacing_to_str(i, &(pacing_table_shr_mem[i]), sess_pacing_info_buf)); 
    NSDL3_PARSING(NULL, NULL, "SessionPacing: Shared Memory Info - %s", first_session_to_str(i, &(pacing_table_shr_mem[i]), sess_pacing_info_buf)); 
    NSDL3_PARSING(NULL, NULL, "SessionPacing: Shared Memory Info - %s", new_user_on_session_to_str(i, &(pacing_table_shr_mem[i]), sess_pacing_info_buf)); 
  }
}

/*
 * Description       : pacing_usage() method used to print usage for G_SESSION_PACING,G_FIRST_SESSION_PACING,G_NEW_USER_ON_SESSION keyword and exit.
 * Input Parameters
 * err               : Print error message.
 * runtime_flag      : Check for runtime changes
 * err_buf           : Pointer to error buffer
 * Output Parameters : None
 * Return            : None
*/

#define first_session_pacing_usage(err, runtime_flag, err_buff)\
{\
  sprintf(err_buff, "Error: Invalid value of G_FIRST_SESSION_PACING keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_FIRST_SESSION_PACING <group_name> <first_session_pacing_option (0/1)> \n"); \
  strcat(err_buff, "  Where\n"); \
  strcat(err_buff, "    group_name: Scenario group name. It can be ALL or any valid group name\n"); \
  strcat(err_buff, "    first_session_pacing_option: Apply pacing for first session of the user or not\n"); \
  strcat(err_buff, "  For example to enable pacing for first session for all groups, use following keyword:\n"); \
  strcat(err_buff, "    G_FIRST_SESSION_PACING ALL 1\n"); \
  strcat(err_buff, "  Note: Pacing time for first session is by randomized mean pacing time\n"); \
  if(runtime_flag != 1) \
    {NS_EXIT(-1, "%s", err_buff);} \
  else { \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
}

#define new_user_on_session_pacing_usage(err, runtime_flag, err_buff)\
{\
  sprintf(err_buff, "Error: Invalid value of G_NEW_USER_ON_SESSION  keyword: %s\n", err); \
  strcat(err_buff, "  Usage: G_NEW_USER_ON_SESSION <group_name> <refresh_user (0/1)> <refresh_values (0/1)> <refresh_cookie (0/1)>\n"); \
  strcat(err_buff, "  Where\n"); \
  strcat(err_buff, "    group_name: Scenario group name. It can be ALL or any valid group name\n"); \
  strcat(err_buff, "    refresh_user: Create new user on every new session or not\n"); \
  strcat(err_buff, "    refresh_value: Refresh value of paraeters on every session if diasbled. This will only fesiable if refresh_user = 0\n"); \
  strcat(err_buff, "  For example to create new used on every session for groups, use following keyword:\n");\
  strcat(err_buff, "    G_NEW_USER_ON_SESSION ALL 1\n");\
  strcat(err_buff, "    This will simulate a new user on each session");\
  if(runtime_flag != 1) \
    {NS_EXIT(-1, "%s", err_buff);} \
  else { \
    NSTL1_OUT(NULL, NULL, "%s", err_buff); \
    return -1; \
  } \
}

/*

Parsing code is dependent on the order of keywords in KeywordDefinition.dat file
It is assumed that file will have three keywords in the order shown below:
#Session Pacing
G_SESSION_PACING|2700|Scalar|Yes|Yes|Yes|-|ALL 0|NA|NA|NA|TBD|Introduce delay between two sessions of a user.
G_FIRST_SESSION_PACING|2710|Scalar|Yes|Yes|Yes|-|ALL 0|NA|NA|NA|TBD|TBD
G_NEW_USER_ON_SESSION|2720|Scalar|Yes|Yes|Yes|-|ALL 0|NA|NA|NA|TBD|Create a new user after each session if enabled.

Parsing is done in the following:

G_SESSION_PACING ALL
G_FIRST_SESSION_PACING ALL
G_NEW_USER_ON_SESSION ALL

G_SESSION_PACING for all groups for which it is defined
G_FIRST_SESSION_PACING for all groups for which it is defined
G_NEW_USER_ON_SESSION for all groups for which it is defined
*/

static void session_pacing_not_runtime(char *sg_name, short think_mode, int time, 
                    short pacing_mode, int max_time, char *err_msg)
{
  int sg_idx;
  int rnum = 0;
#ifdef NS_DEBUG_ON
  char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
#endif

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, think_mode = %hd, time = %d, pacing_mode = %hd, max_time = %d ", sg_name, think_mode, time, pacing_mode, max_time);

  //Case1 - Group name is ALL
  if (strcasecmp(sg_name, "ALL") == 0)
  {
    pacingTable[0].pacing_mode = pacing_mode;
    pacingTable[0].think_mode = think_mode;
    pacingTable[0].time = time;
    pacingTable[0].max_time = max_time;
    NSDL3_PARSING(NULL, NULL, "SessionPacing: Setting for ALL - %s", session_pacing_to_str_non_shm(-1, &(pacingTable[0]), sess_pacing_info_buf));
    return;
  }
 
// Case2a - Group name is not ALL but is invalid  
  if ((sg_idx = find_sg_idx(sg_name)) == -1)
  {
    NSTL1(NULL, NULL, "Warning: Scenario group (%s) used in session pacing is"
                              " not a valid group name. Group (%s) ignored.\n",
                              sg_name, sg_name);
    return;
  }
   // Case2b - Group name is not ALL and is valid group.
  create_pacing_table_entry(&rnum);

  pacingTable[rnum].pacing_mode = pacing_mode;
  pacingTable[rnum].think_mode = think_mode;
  pacingTable[rnum].time = time;
  pacingTable[rnum].max_time = max_time;

  /* Propogate the defaults for first_pacing and new_user */
  pacingTable[rnum].first_sess = pacingTable[0].first_sess;
  pacingTable[rnum].refresh = pacingTable[0].refresh;
  pacingTable[rnum].retain_param_value = pacingTable[0].retain_param_value;
  pacingTable[rnum].retain_cookie_val = pacingTable[0].retain_cookie_val;

  // Set index of session pacing record in run prof table
  runProfTable[sg_idx].pacing_idx = rnum;

  NSDL3_PARSING(NULL, NULL, "SessionPacing: NonSharedMemory Info - %s. pacing_idx = %d", session_pacing_to_str_non_shm(sg_idx, &(pacingTable[rnum]), sess_pacing_info_buf), runProfTable[sg_idx].pacing_idx);
}

/***************************************************************************************************
 * Description       : RTC_SESSION_PACING_LOG() macro used to print runtime changes and log message
 *                     in debug log.
 * Input Parameter
 *   sp_ptr      : pointer to session pacing table in shared memory
 *   grp_idx      : group index
 *   rtc_msg_buf  : message buffer
 * Output Parameter  : None
 * Return            : None
 ***************************************************************************************************/

#define RTC_SESSION_PACING_LOG(grp_idx, rtc_msg_buf, sp_ptr) \
{ \
  session_pacing_to_str_runtime(grp_idx, sp_ptr, rtc_msg_buf);\
  NSDL3_PARSING(NULL, NULL, "%s", rtc_msg_buf);\
}

#define RTC_FIRST_SESSION_LOG(grp_idx, rtc_msg_buf, sp_ptr) \
{\
char *rtc_msg_ptr; \
 \
 /* if(rtc_msg_buf[0] == '\0')*/ \
   /* sprintf(rtc_msg_buf, "%s", "RunTimeChanges: Changes applied for pacing for first session - "); */\
  int  len = strlen(rtc_msg_buf); \
  rtc_msg_ptr = rtc_msg_buf + len; \
  first_session_to_str_runtime(grp_idx, sp_ptr, rtc_msg_ptr);\
  NSDL3_PARSING(NULL, NULL, "%s", rtc_msg_buf); \
}

#define RTC_NEW_USER_ON_SESSION_LOG(grp_idx, rtc_msg_buf, sp_ptr)\
{\
char *rtc_msg_ptr; \
 \
 /* if(rtc_msg_buf[0] == '\0')*/ \
    /*sprintf(rtc_msg_buf, "%s", "RunTimeChanges: Changes applied for new user on new session - ");*/ \
  int  len = strlen(rtc_msg_buf); \
  rtc_msg_ptr = rtc_msg_buf + len; \
  new_user_on_session_to_str_runtime(grp_idx, sp_ptr, rtc_msg_ptr);\
  NSDL3_PARSING(NULL, NULL, "%s", rtc_msg_buf);\
}

static int session_pacing_runtime(char *buf, char *sg_name, short think_mode, int time,
                                 short pacing_mode, int max_time, char *err_msg)
{
  int  i;
  int grp_idx;
 // char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
  PacingTableEntry_Shr  *pacingTable_ptr;

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, think_mode = %hd, time = %d, pacing_mode = %hd, max_time = %d",sg_name, think_mode, time, pacing_mode, max_time);  

  // Case1: Group is ALL. So set for all groups
  if (strcasecmp(sg_name, "ALL") == 0){
    for (i = 0; i < total_runprof_entries; i++) {
      pacingTable_ptr = (PacingTableEntry_Shr *)runprof_table_shr_mem[i].pacing_ptr;
      pacingTable_ptr->pacing_mode = pacing_mode;
      pacingTable_ptr->think_mode = think_mode;
      pacingTable_ptr->time = time;
      pacingTable_ptr->max_time = max_time;
    }
    RTC_SESSION_PACING_LOG(-1, err_msg, pacingTable_ptr);
    return 0;
  }

  //Case2: Group name is not ALL and not a valid Group name.
  if ((grp_idx = find_sg_idx_shr(sg_name)) == -1)
  {
    NS_KW_PARSING_ERR(buf, 1, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011010, sg_name, "");
  }

  //Case3: Group name is not ALL but valid Group name.
  pacingTable_ptr = (PacingTableEntry_Shr*)runprof_table_shr_mem[grp_idx].pacing_ptr;
  pacingTable_ptr->pacing_mode = pacing_mode;
  pacingTable_ptr->think_mode = think_mode;
  pacingTable_ptr->time = time;
  pacingTable_ptr->max_time = max_time;
  NSDL2_PARSING(NULL, NULL, "Method called in valid group. sg_name = %s, think_mode = %hd, time = %d, pacing_mode = %hd, max_time = %d",sg_name, think_mode, time, pacing_mode, max_time);  
  RTC_SESSION_PACING_LOG(grp_idx, err_msg, pacingTable_ptr);
  return 0;
}

//Example
/* ######## After the previous session end:
 * ### Fixed delay
 * #G_SESSION_PACING ALL 1 0 10000
 *
 * ### Random Internet type
 * #G_SESSION_PACING ALL 1 1 10000
 *       
 * ### Uniform distribution
 * #G_SESSION_PACING ALL 1 2 10000 20000
 *          
 * ####### Once every interval
 * ### Fixed interval
 * #G_SESSION_PACING ALL 2 0 10000
 *              
 * ### Random Internet type
 * #G_SESSION_PACING ALL 2 1 10000
 *                 
 * ### Uniform distribution
 * #G_SESSION_PACING ALL 2 2 10000 20000*/

int kw_set_g_session_pacing(char *buf, char *err_msg, int runtime_flag)
{

  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH], chk_pacing_mode[100], chk_think_mode[100], chk_time[100], chk_max_time[100];
  char tmp[100];
  int max_time = 0, num;
  //int first_sess = 0;
  int pacing_mode = 0, think_mode = 0, time = 0; // default value is 0
  sg_name[0] = chk_pacing_mode[0] = chk_think_mode[0] = chk_time[0] = chk_max_time[0] = '\0';

  NSDL2_PARSING(NULL, NULL, "Method called. buf = %s, runtime_flag = %d", buf, runtime_flag);

 // tmp as last field is to validate if any extra field is given in the keyword
  num = sscanf(buf, "%s %s %s %s %s %s %s", keyword, sg_name, chk_pacing_mode, chk_think_mode, chk_time, chk_max_time,tmp);
  if ((num < 3) || (num > 6))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_1);
  }
  //check for numeric value of  of session_pacing_mode_to_str
  pacing_mode = atoi(chk_pacing_mode);
  if(pacing_mode < 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_8);

  if(ns_is_numeric(chk_pacing_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_2);
  }

  think_mode = atoi(chk_think_mode);
  if(think_mode < 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_8);

  if(ns_is_numeric(chk_think_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_2);
  }

  time = atoi(chk_time);
  if(time < 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_8);

  if(ns_is_numeric(chk_time) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_2);
  }

  max_time = atoi(chk_max_time);
  if(max_time < 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_8);

  if(ns_is_numeric(chk_max_time) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_2);
  }

  if (pacing_mode == SESSION_PACING_MODE_NONE) //pacing mode = 0
  {
    if(num < 3)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_1);
    }
    //first_sess = SESSION_PACING_ON_FIRST_SESSION_OFF;
    think_mode = SESSION_PACING_TIME_MODE_CONSTANT;
    time = 0;
  } 
  else if ((pacing_mode == SESSION_PACING_MODE_AFTER) || (pacing_mode == SESSION_PACING_MODE_EVERY)) 
  {
    if(num < 5)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_1);
    }
    if ((think_mode == SESSION_PACING_TIME_MODE_CONSTANT) || (think_mode == SESSION_PACING_TIME_MODE_INTERNET_RANDOM) || (think_mode == SESSION_PACING_TIME_MODE_UNIFORM_RANDOM)){
      if ((think_mode == SESSION_PACING_TIME_MODE_CONSTANT) || (think_mode == SESSION_PACING_TIME_MODE_INTERNET_RANDOM)){
        if (num  > 5){
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_1);
        }
      } 
      if (think_mode == SESSION_PACING_TIME_MODE_UNIFORM_RANDOM){
        if (num < 6){
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_1);
        }
        if (max_time <= time ){
           NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_5);
        }
      } 
    }
    else
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_3);
    }
  } 
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SESSION_PACING_USAGE, CAV_ERR_1011009, CAV_ERR_MSG_3);
  }
  if (!runtime_flag)
  {
    session_pacing_not_runtime(sg_name, think_mode, time, pacing_mode, max_time, err_msg);
  }
  else
  {
    int ret = session_pacing_runtime(buf, sg_name, think_mode, time, pacing_mode, max_time, err_msg);
    return(ret);
  }
  return 0;
}

static void first_sesion_pacing_not_runtime(char *sg_name, char *err_msg, short first_sess)
{
  int sg_idx;
#ifdef NS_DEBUG_ON
  char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
#endif
  int rnum;
  int i;

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, first_sess = %hd", sg_name, first_sess);

  // Case1 - Group name is ALL
  if (strcasecmp(sg_name, "ALL") == 0)
  {
    pacingTable[0].first_sess = first_sess;
    /* we need to propagate to all groups */
    for (i = 1; i < total_pacing_entries; i++) {
      NSDL2_PARSING(NULL, NULL, "Propagateing for group");
      pacingTable[i].first_sess = first_sess;
    }
    return;
  }
  // Case2a - Group name is not ALL but is invalid
  if ((sg_idx = find_sg_idx(sg_name)) == -1)
  {
    fprintf(stderr, "Warning: Scenario group (%s) used in session pacing is not a valid group name. Group (%s) ignored.\n"                    ,sg_name, sg_name);
    return;
  }

  // Case2b - Group name is not ALL and is valid group
  
  // First we have passing G_SESSION_PACING keyword it fills the pacingTable .
  // and G_FIRST_SESSION_PACING use only those values.
  // G_FIRST_SESSION_PACING update only the value of first_sess and remaining values picked that is filled by G_SESSION_PACING.
  if ((rnum = runProfTable[sg_idx].pacing_idx) == 0)
  {
  // Case2c - Group name is not ALL and is valid group.
    NSDL4_PARSING(NULL, NULL, "Creating for sg_idx = %d, pacing_idx = %d\n", sg_idx, runProfTable[sg_idx].pacing_idx);
    if (create_pacing_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "Error in creating a pacing entry");
    }
    pacingTable[rnum].pacing_mode = pacingTable[0].pacing_mode;
    pacingTable[rnum].think_mode = pacingTable[0].think_mode;
    pacingTable[rnum].time = pacingTable[0].time;
    pacingTable[rnum].max_time = pacingTable[0].max_time;
    pacingTable[rnum].refresh = pacingTable[0].refresh;
    pacingTable[rnum].retain_param_value = pacingTable[0].retain_param_value;
    pacingTable[rnum].retain_cookie_val = pacingTable[0].retain_cookie_val;
    runProfTable[sg_idx].pacing_idx = rnum;
  }

  pacingTable[rnum].first_sess = first_sess;
  NSDL3_PARSING(NULL, NULL, "SessionPacing: Non_Shared Memory Info - %s", first_session_to_str_non_shm(sg_idx, &(pacingTable[rnum]), sess_pacing_info_buf));
}


static int first_sesion_pacing_runtime(char *sg_name, char *err_msg, short first_sess)
{
  //int i;
  int grp_idx;
  PacingTableEntry_Shr *pacingTable_ptr;

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, first_sess = %hd", sg_name, first_sess);

  // Case1: Group is ALL. So set for all groups
  if (strcasecmp(sg_name, "ALL") == 0){
    for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
      pacingTable_ptr = (PacingTableEntry_Shr *)runprof_table_shr_mem[grp_idx].pacing_ptr;
      pacingTable_ptr->first_sess = first_sess;
    }
    RTC_FIRST_SESSION_LOG(-1, err_msg, pacingTable_ptr);
    return 0;
  }

  //Case2: Group name is not ALL and not a valid Group name.
  if ((grp_idx = find_sg_idx_shr(sg_name)) == -1)
  {
    sprintf(err_msg, "Scenario group (%s) used in first session pacing is not a valid group name.", sg_name); 
    return -1;
  }
 
  //Case3: Group name is not ALL but valid Group name.
  pacingTable_ptr = (PacingTableEntry_Shr *)runprof_table_shr_mem[grp_idx].pacing_ptr;
  pacingTable_ptr->first_sess = first_sess;
  RTC_FIRST_SESSION_LOG(grp_idx, err_msg, pacingTable_ptr);
  return 0;
}

// Examples:
//  G_FIRST_SESSION_PACING ALL 0
//  G_FIRST_SESSION_PACING G1 1
int kw_set_g_first_session_pacing(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int num, first_sess = 0;
  char chk_first_sess[100];
  chk_first_sess[0] = '\0';
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields

  NSDL2_PARSING(NULL, NULL, "Method called. buf = %s, runtime_flag = %d", buf, runtime_flag);

  num = sscanf(buf, "%s %s %s %s", keyword, sg_name, chk_first_sess , tmp);//Check for extra arguments.
  if (num != 3)
  {
    first_session_pacing_usage("Invalid number of arguments", runtime_flag, err_msg);
  }
  // check for numeric value of  of first_sess_str
  if(ns_is_numeric(chk_first_sess) == 0){
   first_session_pacing_usage("first session pacing option  is not numeric", runtime_flag, err_msg);
  }
  first_sess = atoi(chk_first_sess);
  if ((first_sess < 0) && (first_sess > 1))
  {
    first_session_pacing_usage("Invalid first session pacing mode. It should be 0 or 1", runtime_flag, err_msg);
  }
  if (!runtime_flag)
  {
    first_sesion_pacing_not_runtime(sg_name, err_msg, first_sess);
  }
  else
  {
    int ret = first_sesion_pacing_runtime(sg_name, err_msg, first_sess); 
    return(ret);
  }

  return 0;
}

  
static void new_user_on_session_not_runtime(char *sg_name, char *err_msg, int refresh, int retain_param_value, int retain_cookie_val) 
{
  int sg_idx, rnum;
  int i;
#ifdef NS_DEBUG_ON
  char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
#endif

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, refresh = %d, retain_param_value = %d, retain_cookie_value = %d", 
                             sg_name, refresh, retain_param_value, retain_cookie_val);
  

  //Case1 - Group name is ALL
  if (strcasecmp(sg_name, "ALL") == 0)
  {
    pacingTable[0].refresh = refresh;
    pacingTable[0].retain_param_value = retain_param_value;
    pacingTable[0].retain_cookie_val = retain_cookie_val;
    NSDL2_PARSING(NULL, NULL, "retain_cookie_val = %d", pacingTable[0].retain_cookie_val);
    /* we need to propagate to all groups */
    // TODO: Check this code. At this point total_pacing_entries will be always 1
    for (i = 1; i < total_pacing_entries; i++) {
      NSDL2_PARSING(NULL, NULL, "propagating refresh to all");
      pacingTable[i].refresh = refresh;
      pacingTable[i].retain_param_value = retain_param_value;
      pacingTable[i].retain_cookie_val = retain_cookie_val;
    }
    return;
  }
  // Case2a - Group name is not ALL but is invalid
  if ((sg_idx = find_sg_idx(sg_name)) == -1)
  {
    fprintf(stderr, "Warning: Scenario group (%s) used in session pacing is not a valid group name. Group (%s) ignored.\n"                    ,sg_name, sg_name);
    return;
  }
   // First we have passing G_SESSION_PACING keyword it fills the pacingTable .
   // and G_new_user_on_SESSION_PACING use only those values.
   //G_FIRST_SESSION_PACING update only the value of first_sess and remaining values picked that is filled by G_SESSION_PACING.
  if ((rnum = runProfTable[sg_idx].pacing_idx) == 0)
  {
    // Case2c - Group name is not ALL and is valid group.     
    NSDL4_PARSING(NULL, NULL, "Creating for sg_idx = %d, pacing_idx = %d\n", sg_idx, runProfTable[sg_idx].pacing_idx);

    if (create_pacing_table_entry(&rnum) != SUCCESS) {
      NS_EXIT(-1, "Error in creating a pacing entry");
    }
         pacingTable[rnum].pacing_mode = pacingTable[0].pacing_mode;
         pacingTable[rnum].think_mode = pacingTable[0].think_mode;
         pacingTable[rnum].time = pacingTable[0].time;
         pacingTable[rnum].max_time = pacingTable[0].max_time;
         pacingTable[rnum].first_sess = pacingTable[0].first_sess;
         runProfTable[sg_idx].pacing_idx = rnum;
  }
  pacingTable[rnum].refresh = refresh;
  pacingTable[rnum].retain_param_value = retain_param_value;
  pacingTable[rnum].retain_cookie_val = retain_cookie_val;
  NSDL3_PARSING(NULL, NULL, "SessionPacing: Non_Shared Memory Info - %s", new_user_on_session_to_str_non_shm(sg_idx, &(pacingTable[rnum]),sess_pacing_info_buf));
}

static int new_user_on_session_runtime(char *sg_name, char *err_msg, char refresh, char retain_param_value, int retain_cookie_val)
{
  int  i;
  PacingTableEntry_Shr *pacingTable_ptr;

  NSDL2_PARSING(NULL, NULL, "Method called. sg_name = %s, refresh = %c, retain_param_value = %c", sg_name , refresh, retain_param_value);  

  // Case1: Group is ALL. So set for all groups
  if (strcasecmp(sg_name, "ALL") == 0) {
    for (i = 0; i < total_runprof_entries; i++) {
      pacingTable_ptr = (PacingTableEntry_Shr *)runprof_table_shr_mem[i].pacing_ptr;
      pacingTable_ptr->refresh = refresh;
      pacingTable_ptr->retain_param_value = retain_param_value;
      pacingTable_ptr->retain_cookie_val = retain_cookie_val;
    }
    RTC_NEW_USER_ON_SESSION_LOG(-1, err_msg, pacingTable_ptr);
    return 0;
  }
//Case2: Group name is not ALL and not a valid Group name.
  int grp_idx;
  if ((grp_idx = find_sg_idx_shr(sg_name)) == -1)
  {
    sprintf(err_msg, "Scenario group (%s) used in new user on session is not a valid group name.", sg_name); 
    return -1;
  }
//Case3: Group name is not ALL but valid Group name.
  pacingTable_ptr = (PacingTableEntry_Shr*)runprof_table_shr_mem[grp_idx].pacing_ptr;
  pacingTable_ptr->refresh = refresh;
  pacingTable_ptr->retain_param_value = retain_param_value;
  pacingTable_ptr->retain_cookie_val = retain_cookie_val;
  RTC_NEW_USER_ON_SESSION_LOG(grp_idx, err_msg, pacingTable_ptr);
  return 0;
}

/*
  There was a requirement from kohls poc to retain parameter and cookie value(s) used  by vuser in previous session 
  if we are continuing with same virtual user and session succeeds . In order to meet the requirement 
  a new argument is added in G_NEW_USER_ON_SESSSION to retain parameter value. 
   
 G_NEW_USER_ON_SESSION <group_name> <refresh_user> <retain_parameter_value> <retain_cookie_value>
   group_name : Group name for which new user option is to be defined. (Must be valid and defined in SGRP)
   refresh_user :
      0: Virtual user continues from the current state (default)
      1: Virtual user would start as a new user for each execution of a test script 
   retain_parameter_value : 
      0: Clean parameter value for virtual user.(default)
      1: Retain parameter value for virtual user. (either session succeed or not)
      2: Retain parameter value for virtual user only if session succeed, if session failed clear parameter value.
   retain_cookie_value :  
      0: Clean cookie value for virtual user.
      1: Retain cookie value for virtual user. (either session succeed or not) (default)
      2: Retain cookie value for virtual user only if session succeed, if session failed clear cookie value.
 
  Examples:
  G_NEW_USER_ON_SESSION ALL 0 0 (same virtual user and uses  different parameter/ cookie value)
  G_NEW_USER_ON_SESSION ALL 0 1 (same virtual user and uses same parameter/cookie values)
  G_NEW_USER_ON_SESSION ALL 1 0/1 ( Different virtual user. Value(s) will be automatically flushed) .

*/

int kw_set_g_new_user_on_session(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int num, refresh = 0;
  char chk_refresh[100]="";
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  char retain_parameter_value[100]=""; 
  char retain_cookie_value[100] = ""; 
  int retain_param_value = 0;
  int retain_cookie_val = 1; 

  NSDL2_PARSING(NULL, NULL, "Method called. buf = %s, runtime_flag = %d", buf, runtime_flag);
  num = sscanf(buf, "%s %s %s %s %s %s", keyword, sg_name, chk_refresh, retain_parameter_value, retain_cookie_value, tmp);//Check for extra arguments. 
  if (num < 3 || num > 5)
  {
    new_user_on_session_pacing_usage("Invalid number of arguments",runtime_flag, err_msg);
  }

  // Check for numeric value of new_user_on_sess_str
  if(!ns_is_numeric(chk_refresh)){
    new_user_on_session_pacing_usage("new_user on session pacing option is not numeric",runtime_flag, err_msg);
  }
  refresh = atoi(chk_refresh);

  if ((refresh != 0) && (refresh != 1)) {
    new_user_on_session_pacing_usage("Invalid new user on session pacing mode. It should be 0 or 1", runtime_flag, err_msg);
  }

  // Check for numeric value of retaining param value on next sesssion
  if(retain_parameter_value[0]) {
    if(!ns_is_numeric(retain_parameter_value)){
      new_user_on_session_pacing_usage("new_user on session parameter cleanup mode is not numeric",runtime_flag, err_msg);
    }
    retain_param_value = atoi(retain_parameter_value);
    if ((retain_param_value != 0) && (retain_param_value != 1) && (retain_param_value != 2)) {
      new_user_on_session_pacing_usage("Invalid new user on session parameter cleanup mode .It should be 0, 1 or 2", runtime_flag, err_msg);
    }
  }

  // Check for numeric value of retaining param value on next sesssion
  if(retain_cookie_value[0]) {
    if(!ns_is_numeric(retain_cookie_value)){
      new_user_on_session_pacing_usage("new_user on session parameter cleanup mode is not numeric",runtime_flag, err_msg);
    }
    retain_cookie_val = atoi(retain_cookie_value);
    if ((retain_cookie_val != 0) && (retain_cookie_val != 1) && (retain_cookie_val != 2)) {
      new_user_on_session_pacing_usage("Invalid new user on session parameter cleanup mode .It should be 0, 1 or 2", runtime_flag, err_msg);
    }
  }
  
  // Parameter value will be flushed if new user is set to 1. Therefore setting retain_param_value to default (0).  
  NSDL2_PARSING(NULL, NULL, "Refresh User =  %d, retain_param_value = %d, retain_cookie_val = %d", refresh, retain_param_value, retain_cookie_val);
  if ( refresh == 1) {
    retain_param_value = 0;
    retain_cookie_val = 0; 
    NSDL2_PARSING(NULL, NULL, "param_clean = [%d] retain_cookie_val = %d", retain_param_value, retain_cookie_val);
  }
    
  if (!runtime_flag)
  { 
    new_user_on_session_not_runtime(sg_name, err_msg, refresh, retain_param_value, retain_cookie_val);
  }
  else
  {
    int ret = new_user_on_session_runtime(sg_name, err_msg, refresh, retain_param_value, retain_cookie_val);
    return(ret);
  }

  return 0;
}

#ifdef NS_DEBUG_ON
/* This will log session pacing time in a data file */
static void  log_pacing_time_for_debug(VUser *vptr, PacingTableEntry_Shr *pacing_ptr, int user_first_sess, int time_to_think, u_ns_ts_t now)
{
char file_name[4096];
//char *sg_name;
char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log


  // In case of debug level 3 or DM_LOGIC3 and Mask schedule, we need to log data in a file
  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC3) && 
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_SCHEDULE)))
    return;

  sprintf(file_name, "%s/logs/TR%d/session_pacing.dat", g_ns_wdir, testidx);

  // Format of file
  // TimeStamp|Group|SessionPacingInfo|FirstSess|PacingTime(ms)|PacingTime(secs)
  // 00:00:10|G1|Group = G1, Mode=Constat..|1|1123|1.123
  ns_save_data_ex(file_name, NS_APPEND_FILE, "%s|%s|%s|%d|%d|%0.3f", get_relative_time(), runprof_table_shr_mem[vptr->group_num].scen_group_name, session_pacing_to_str(vptr->group_num, pacing_ptr, sess_pacing_info_buf), pacing_ptr->first_sess, time_to_think, NS_MS_TO_SEC(time_to_think));
 // ns_save_data_ex(file_name, NS_APPEND_FILE, "%d|%0.3f", time_to_think, NS_MS_TO_SEC(time_to_think));
/*
struct timeval tv;
  gettimeofday(&tv, NULL);
  NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: Starting session pacing timer for time = %u ms, gettimeofday = %llu", time_to_think, (unsigned long long)((tv.tv_sec * 1000) + (tv.tv_usec / 1000)));
*/

}

#endif


// session pacing max time
// the maximum value int can hold is 7fffffff
// so we can give maximum value 7fffffff/(24*3600*1000)ms is approx 24 days 
#define MAX_SESSION_PACING_TIME 24*3600*1000

/*
Function: double gsl_ran_exponential (const gsl_rng * r, double mu)
    This function returns a random variate from the exponential distribution with mean mu. The distribution is,
              p(x) dx = {1 \over \mu} \exp(-x/\mu) dx
    for x >= 0. 
*/

int calc_session_pacing_time_ir(PacingTableEntry_Shr *pacing_ptr, int user_first_sess, double mean_time)
{
  double session_pacing_ir_time;
  int time_to_think; // We need to return think time as int

  NSDL1_SCHEDULE(NULL, NULL, "Method called. User first session is = %d, Mean time = %0.3f ms", user_first_sess, mean_time);
 
  session_pacing_ir_time = gsl_ran_exponential(exp_rangen, mean_time);    
  if(session_pacing_ir_time < 0){
    NSDL1_SCHEDULE(NULL, NULL, "Session pacing time returned by gsl_ran_exponential less then zero.  Returned value = %.3f secs. Forcing it to 0", NS_MS_TO_SEC(session_pacing_ir_time));
    time_to_think = 0;
  }
  else if(session_pacing_ir_time  > MAX_SESSION_PACING_TIME){
    NSDL1_SCHEDULE(NULL, NULL, "Session pacing time returned by gsl_ran_exponential more than max value. Returned value = %.3f secs. Forcing it to %d", NS_MS_TO_SEC(session_pacing_ir_time), NS_MS_TO_SEC(MAX_SESSION_PACING_TIME));
    time_to_think = MAX_SESSION_PACING_TIME;
  }
  else
  {
    time_to_think = (int )session_pacing_ir_time;
    NSDL1_SCHEDULE(NULL, NULL, "Session pacing time returned by gsl_ran_exponential = %.3f secs.", NS_MS_TO_SEC(time_to_think));
  }

  return (time_to_think);
}

/* Name: start_session_pacing_timer
 * Purpose: This function is start session pacing timer for First sesssion pacing & normal session pacing both
 *          Called from new_user & reuse_user both.
 * If Called first time for a new_user then scen_group_num will be -1
 * else will have  group num.
 * Arguments:
 *   vptr
 *   cptr
 *   user_first_sess: 1 - If this is user's first session else 0
 *   start_now: Set to 0 if session pacing timer is started
 *   now
 * Return: None
 */
inline void 
start_session_pacing_timer(VUser *vptr, connection *cptr, int user_first_sess, int *start_now, u_ns_ts_t now, int new_or_reuse) 
{
PacingTableEntry_Shr *pacing_ptr;
ClientData cd;
int pacing_time_mode = -1;
int group_num = vptr->group_num;
int time_to_think = 0;
double mean_time;
#ifdef NS_DEBUG_ON
// char sess_pacing_info_buf[1024]; // use to fill with session pacing detail for debug log
#endif

  NSDL2_SCHEDULE(vptr, NULL, "Method called, vptr = %p, cptr = %p, user_first_sess = %d, new_or_reuse = %d", 
                       vptr, cptr, user_first_sess, new_or_reuse);

  if(vptr->sess_status == NS_SESSION_ABORT) {
    NSDL2_SCHEDULE(vptr, NULL, " At pacing when Session status is abort");
    time_to_think = runprof_table_shr_mem[vptr->group_num].gset.retry_interval_on_page_failure; 
    goto start_timer;
  }

  pacing_ptr = runprof_table_shr_mem[vptr->group_num].pacing_ptr;
  if (pacing_ptr->pacing_mode == SESSION_PACING_MODE_NONE) {  // No Session Pacing
    if (user_first_sess == 0) // It is not first session
      NSDL2_SCHEDULE(vptr, NULL, "SessionPacing: No pacing is to be done.");
    else
      NSDL2_SCHEDULE(vptr, NULL, "SessionPacing: No pacing is to be done for first session of user.");
    return;
  }

  if (user_first_sess == 0) { // It is not first session
    pacing_time_mode = pacing_ptr->think_mode;
  } 
  else 
  { // For first session of the user
    switch (pacing_ptr->first_sess) {
      case SESSION_PACING_ON_FIRST_SESSION_OFF:
        NSDL2_SCHEDULE(vptr, NULL, "SessionPacing: Pacing is OFF for first session. No pacing is to be done for first session of user.");
        return;

      case SESSION_PACING_ON_FIRST_SESSION_ON:  /* Has to be set to random for first pacing mode. */
        NSDL2_SCHEDULE(vptr, NULL, "SessionPacing: Pacing is ON for first session."); 
        pacing_time_mode = SESSION_PACING_TIME_MODE_INTERNET_RANDOM;
        break;
      default:
        fprintf(stderr, "Error: Invalid value of session pacing for first session mode (%d). Ignored\n", pacing_ptr->first_sess);
        return;
    }
  }

  NSDL2_SCHEDULE(vptr, NULL, "SessionPacing: pacing_time_mode = %d", pacing_time_mode);

  switch (pacing_time_mode) {
    case SESSION_PACING_TIME_MODE_CONSTANT:
      time_to_think = pacing_ptr->time;
      break;

    case SESSION_PACING_TIME_MODE_INTERNET_RANDOM:
      mean_time = (double )(pacing_ptr->time);

      // In case of first session, if time mode is uniform random, then we need to use min+max/2 for mean
      if(user_first_sess && pacing_ptr->think_mode == SESSION_PACING_TIME_MODE_UNIFORM_RANDOM){
        mean_time = (double )((pacing_ptr->time + pacing_ptr->max_time)/2);
      }
      time_to_think = calc_session_pacing_time_ir(pacing_ptr, user_first_sess, mean_time);
      break;

    case SESSION_PACING_TIME_MODE_UNIFORM_RANDOM:
      time_to_think = pacing_ptr->time + ns_get_random_max(gen_handle, pacing_ptr->max_time - pacing_ptr->time);
      break;
  }

  // Note: time_to_think can be also 0 in case of unfirm random with min = 0 or in case of internet random
    if (user_first_sess == 0) // It is not first session
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: For next session, Pacing time = %.3f secs", NS_MS_TO_SEC(time_to_think));
    else
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: For first session, Pacing time = %.3f secs", NS_MS_TO_SEC(time_to_think));

  if(SHOW_GRP_DATA)
  {
    if(user_first_sess) {
      vptr->flags |= DO_NOT_INCLU_SPT_IN_TIME_GRAPH;
      NSTL2(vptr, NULL, "SHOW_GRAPH_DATA: First session pacing is going to apply on this vuser. Setting bit.");
    }
  }
  // In case of every interval mode, we need to see how much time session has taken and then
  // reduce this time from pacing time
  // We keep time taken by as sesion in scen_group_adjust and then find out how much more pacing time to apply
  // If net pacing time is >= 0, then we reset scen_group_adjust to 0.
  // If net pacing time is < 0, then we add the loss in the pacing time due to session taken more time than pacing time
  
    if (pacing_ptr->pacing_mode ==  SESSION_PACING_MODE_EVERY) {  // Once every interval
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: scen_group_adjust[%d] = %d", group_num, scen_group_adjust[group_num]);
      time_to_think -= scen_group_adjust[group_num]; 
    if (time_to_think < 0) {
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: Session time was more than pacing time. Loss of time = %d", -time_to_think);
      scen_group_adjust[group_num] = (-1) * (time_to_think); // Save the loss so that we can adjust in next session
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: After adjust. scen_group_adjust[%d] = %d", group_num, scen_group_adjust[group_num]);
      time_to_think = 0;
    } else {
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: Session time was not more than pacing time.");
      scen_group_adjust[group_num] = 0; // Reset to 0 as there is no loss in pacing time
    }
    if (user_first_sess == 0) // It is not first session
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: After adjustment next session, Pacing time = %.3f secs", NS_MS_TO_SEC(time_to_think));
    else
      NSDL1_SCHEDULE(vptr, NULL, "SessionPacing: After adjustment first session, Pacing time = %.3f secs",NS_MS_TO_SEC(time_to_think));
  }
 

#ifdef NS_DEBUG_ON
  log_pacing_time_for_debug(vptr, pacing_ptr, user_first_sess, time_to_think, now);
#endif

  //Added for retry session with interval
  start_timer:

  NSDL2_SCHEDULE(vptr, NULL, "Inside the start_timer, time_to_think - %d", time_to_think);
  if (time_to_think) {
    NSDL2_SCHEDULE(vptr, NULL, "Inside the check");
    *start_now = 0; 
    vptr->timer_ptr->actual_timeout = time_to_think;
    cd.p = vptr;
    cd.timer_started_at = now;
   //Donot allow this if user is paused 
    if(vptr->vuser_state != NS_VUSER_PAUSED){
      VUSER_ACTIVE_TO_WAITING(vptr);
    }
   
    if(new_or_reuse)
      dis_timer_think_add( AB_TIMEOUT_STHINK, vptr->timer_ptr, now, start_new_user_callback, cd, 0);
    else
      dis_timer_think_add( AB_TIMEOUT_STHINK, vptr->timer_ptr, now, start_reuse_user_callback, cd, 0);
  }

  return;
}

