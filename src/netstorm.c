/************************************************************************
* netstorm.c *
*
* HISTORY
* 8/28/01	0.51		Report generation Added
* 		0.60		Efficieny added by reducing user time
*				renew_connection
*				time calculations bug in us from ms
*		0.70		SSL support Added
*		0.71		select failied (Bad file descriptor) fixed
*				Support multiple server addresses
*		0.71		PROGRESS_SECS->PROGRESS_MSECS
*				WAIT_PROGRESS_SECS->WAIT_PROGRESS_MSECS
* 03/06/03	0.72		Time drift problem by compiling the sample
*				numbers. Calvulation problem wrt to finding
*				average solved
* 03/10/03	0.75		epoll added, asymmetric speed,loss & delay
* 03/12/03	0.76		problem with epoll version removed
* 				num_hits =0 case fixed
* 				masked connection RST errors
* 				bad_bytes count fixed
* 				case of EOF indication by server close
* 				connection added
* 				Unsolved - why Reset received even in case
* 				of first KA and NKS connectiosn -logs are in
* 				rst.txt and rst.dmp
* 03/19/03      0.77 		tx & pg times
* 				create user concept
* 				Different ramp ups (NUM_USER_MODE, USER_REUSE_MODE)
* 				non-linear distributions
* 				granules for pg & tx
* 				different granule specifications for hit,pg & tx
* 				create module for %ile report for easy maintenance
* 04/30/03	0.78		Uses modem ioctls
* 				keep track of users & states
* 				keep track of connections
* 				deliver_report() moved to util.c
* 				USE_HOST : CONFIG => use from config file else from host hdr
* 				USE_DNS: NO_DNS, DYNAMIC or SIMULATE timing
* 				parallel connection implementaion
* 				modularize close_fd & url connction functions
* 07/29/03	0.80		Merge with variablized recorded file
* 				Implement Session, User & Run profile
* 				Cookies, cookie variable, dynamic vars based on hidden vars
* 				Cleanup for close_connection (End of message processing)
* 				It is possible that abort page would close an fd and Main
* 				loop already has its fd in its set. To avoid that, close_fd
* 				maintains a list of fd's that need to be simply ignored by
* 				main loop.
* 08/19/03	0.90		Test Run execution for differnt test modes (targets)
* 				User slot calculation based on cleanup time
* 10/07/03      0.91            Test Logging implemented
*                               Standard Headers removed from capture file and inserted
*                               during runtime.
*                               SSL is per request, instead of per test run.
*                               Weiball Think time supported.
* 10/07/03      0.92            Capture program now captures the v.42 compression ratio and
*                               netstorm will alter speed to take compression into account
*                               Shared memory cleared in netstorm
*                               Latency and drop rate is now per connection
*                               Netstorm will wait for TIME_WAIT sockets to clear before
*                               every test run
*                               Close modems are done in netstorm
* 11/17/03      0.95            Per page think time implemented
*                               FIX_MEAN_USERS run mode implemented
*                               Run start time synchronized
*                               Server capacity check implemented
* 12/26/03      0.99            Fixed the multiple-value variable problem (when variable is a UTF or LONG variable)
*				Put in the keyword LOAD_KEY so user can set the run mode for the test
*				criteria_met modified for tri-state
*************************************************************************/
//TODO: look at the case of num_dir (aftermath of user simulation)
//TODO: Currently, it is assumed the host adress (from host address) would
//	resolve to same IP address (during a session).

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/prctl.h>
#ifdef SLOW_CON
  #include <linux/socket.h>
  #include <netinet/tcp.h>
  #define TCP_BWEMU_REV_DELAY 16
  #define TCP_BWEMU_REV_RPD 17
  #define TCP_BWEMU_REV_CONSPD 18
#endif
#ifdef NS_USE_MODEM
  #include <linux/socket.h>
  //#include <linux/cavmodem.h>
  #include <netinet/tcp.h>
  #include <regex.h>

  #include "url.h"
  #include "ns_byte_vars.h"
  #include "ns_nsl_vars.h"
  #include "nslb_util.h"
  #include "ns_search_vars.h"
  #include "ns_cookie_vars.h"
  #include "ns_check_point_vars.h"
  #include "nslb_time_stamp.h"
  #include "ns_static_vars.h"
  #include "ns_msg_def.h"
  #include "ns_check_replysize_vars.h"
  #include "ns_error_codes.h"
  #include "user_tables.h"
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
  #include "cavmodem.h"
  #include "ns_wan_env.h"
  
#endif

#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef USE_EPOLL
  //#include <asm/page.h>
  // This code has been commented for FC8 PORTING
  //#include <linux/linkage.h>
  #include <linux/unistd.h>
  #include <sys/epoll.h>
  #include <asm/unistd.h>
#endif

#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
//#include "logging.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include "netstorm.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "amf.h"
#include "eth.h"
#include "timing.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "server_stats.h"
#include "ns_trans.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_parent.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_url_resp.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_auto_fetch_embd.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_page.h"
#include "ns_vuser.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3_send.h"
#include "ns_pop3.h"
#include "ns_ftp_send.h"
#include "ns_dns.h"
#include "ns_ftp.h"
#include "ns_http_pipelining.h"

#include "ns_server_mapping.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_http_cache.h"
#include "ns_http_cache_store.h"
#include "ns_http_cache_reporting.h"
#include "ns_vuser_trace.h"
#include "nslb_date.h"
#include "ns_dynamic_hosts.h" //Added for macro ERR_MAIN_URL_ABORT and ERR_EMBD_URL_ABORT
#include "ns_http_auth.h"
#include "ns_socket_api_int.h"

#include <libgen.h> // For basename()
#include "ns_network_cache_reporting.h"

#include "ns_url_hash.h"
#include <grp.h>
#include "ns_trace_level.h"

#include "nslb_cav_conf.h"
#define CPU_HEALTH_IDX 0
#include "ns_monitoring.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_inline_delay.h"
#include "ns_page_based_stats.h"
#include "ns_ip_data.h"
#include "ns_group_data.h"
#include "ns_ndc.h"
#include "ns_server_ip_data.h"
#include "ns_websocket_reporting.h"
#include "ns_websocket.h"
#include "ns_exit.h"
#include "ns_http_status_codes.h"
#include "ns_test_init_stat.h"
#include "ns_error_msg.h"
#include "ns_log_req_rep.h"
#include "ns_h2_reporting.h"
#include "ns_socket.h"
#include "ns_cavmain.h"
#include "ns_test_monitor.h"

// This code has been commented for FC8 PORTING
//_syscall1(int, epoll_create, int, size);
//_syscall4(int, epoll_ctl, int, epfd, int, op, int, fd, struct epoll_event*, event);
//_syscall4(int, epoll_wait, int, epfd, struct epoll_event*, events, int, maxevents, int, timeout);
extern void inline update_proxy_counters(VUser *vptr, int status);
static int memperf_for_conn = 0; //Used for MEMPERF keyword
int ns_parent_state=0; //0: init, 1= started, 2 = ending
#ifndef CAV_MAIN
s_child_ports* v_port_table = NULL;
s_child_ports* v_port_entry = NULL;
#else
__thread s_child_ports* v_port_table = NULL;
__thread s_child_ports* v_port_entry = NULL;
#endif
int ns_sickchild_pending=0; /*TST */
int sigterm_received = 0;
int end_test_run_mode = 0; 
int num_set_select=0; /*TST */
int num_reset_select=0; /*TST */
__thread int http_resp_code = 0;
static char version_buf[128 + 1] = "Version not available"; // Version without new line

#define MAX_BANDWIDTH_REACHED (70 * 1024 * 1024)    /* 70 megabits/sec */
#define CLEANUP_NUMBER_RUNS 10
#define LOWER_BOUND_FACTOR 0.95
#define UPPER_BOUND_FACTOR 1.05
#define MEET_RATE_HIGH_PCT 5
#define MEET_RATE_LOW_PCT 5
//#define WITHIN_TARGET(avg, target, lower_pct, upper_pct) ((avg >= (target * (100 - lower_pct))/100) && (avg <= (target*(100 + upper_pct))/100))

#if 0
/*  in case of continuous mon, dir may exist as test may restart; hence error msg is not displayed. */
#define MKDIR_ERR_MSG(dir, buf){ \
      if(!(EEXIST == errno && global_settings->continuous_monitoring_mode)) \
      { \
        NSTL1_OUT(NULL, NULL, "Unable to Create %s directory '%s'. Error: '%s'\n", dir, buf, nslb_strerror(errno)); \
        exit(-1); \
      } \
    }
     
#endif
   
char interactive_buf[4096];

char ns_target_buf[64];
//3 for Host + 2 for User-Agent + 4 std  : Accept, Accept-Encoding, Keep-Alive, Connection

//int end_fetches;/*, end_seconds;*/
//#define END_NONE 0
//#define END_FETCHES 1
//#define END_SECONDS 2

int msg_num = 0;
//char netstorm_usr_and_grp_name[258];

//int g_follow_redirects = 0; /* Depth of how many redirects to follow for one URL */
//int g_auto_redirect_use_parent_method = 0; /* Used with auto redirect */

int inline
ns_strncpy(char* dest, char* source, int num) {
  int i;

  if (!source)
    return 0;

  for (i = 0; i < num; i++, dest++, source++) {
    *dest = *source;

    if (*source == '\0')
      return i;
  }

  return -1;
}

#ifndef CAV_MAIN
int max_var_table_idx;
#else
__thread int max_var_table_idx;
#endif

const char* (*get_key_parse)(unsigned int);

int max_dynvar_hash_code;
unsigned int (*dynvar_hash)(const char*, unsigned int);
int (*in_dynvar_hash)(const char*, unsigned int);

const char* (*bytevar_get_key)(int);

int max_tagattr_hash_code;

//static char compass_ver[]="netstorm: Version 1.4.1 (build# %d) Cavisson Systems Inc.\n\n";
// static int build_num;

int ns_ramp_timeout_idx;
int ns_end_timeout_idx;
int ns_progress_timeout_idx;
unsigned int v_cur_progress_num = 0;

//Add 02/09/07 Atul -- Helper function url_prepost_cb.c--------

// Buffer for complete URL repsone (includes header)
char *url_resp_buff = NULL;
int url_resp_size = 0;
__thread char compression_type = 0;
__thread int http_header_size = 0;
// Buffer for URL header repsone
char *url_resp_hdr_buff = NULL;
int url_resp_hdr_size = 0;

connection *cur_cptr = NULL;

struct sockaddr_in parent_addr;

char ip_alias[] = "alias.txt";
FILE *ip_alias_fp;
//FILE* ssl_logs;

int run_mode;
#ifndef CAV_MAIN
VUser* gBusyVuserHead = NULL;
VUser* gBusyVuserTail = NULL;
#else
__thread VUser* gBusyVuserHead = NULL;
__thread VUser* gBusyVuserTail = NULL;
#endif
int gNumVuserActive=0;
int gNumVuserThinking=0;
int gNumVuserBlocked=0;
int gNumVuserWaiting=0;
int gNumVuserSPWaiting=0;
int gNumVuserCleanup=0;
int gNumVuserPaused=0;
short gRunPhase = NS_RUN_PHASE_RAMP;
short g_ramp_down_completed = 0;

u_ns_ts_t cum_timestamp = 0;
u_ns_ts_t interval_start_time = 0;

u_ns_ts_t cur_time;

#define MAX_CNUM_NUMBER_LENGTH 30


/* added variable <ultimate_max_connections>*/
int ultimate_max_connections;
//int max_connections;
//static int num_not_ready;
//int max_vusers;
//int max_session;   //added because this is needed to send ramp up done msg in case of session rate 

#define NO_LENGTH_ERROR 1

#if 0
#define REQ_NEW 0
#define REQ_OK 1
#define REQ_TIMEOUT 2
#define BAD_BYTES 3
#define ERR_ERR 4
#define REQ_CON_TIMEOUT 5
#endif

char* argv0;
int do_checksum, do_throttle, do_verbose;

time_t g_start_time;

//int warmup_session_done=1;
//int warmup_seconds_done=1;

/* Forwards. */
void user_cleanup_timer( ClientData client_data, u_ns_ts_t now );
//static void http_close_connection( connection* cptr, action_request_Shr *url_num, int chk, u_ns_ts_t now );
//static void non_http_close_connection( connection* cptr, action_request_Shr *url_num, int chk, u_ns_ts_t now );
void http_close_connection( connection* cptr, int chk, u_ns_ts_t now );
static void non_http_close_connection( connection* cptr, int chk, u_ns_ts_t now );
void* my_malloc( size_t size );
void* my_realloc( void *ptr, size_t size );
void inline on_request_write_done ( connection* cptr);

//int ns_iid_handle;

extern int log_run_profile(const TestCaseType_Shr* test_case);
extern int log_user_profile(const UserIndexTableEntry* test_case_shr);
extern int log_session_profile(const SessProfIndexTableEntry_Shr* sessprofindex_table);
extern int log_session_table(const SessTableEntry_Shr* sess_table);
extern int log_page_table(const SessTableEntry_Shr* session_table);
extern int log_tx_table_record_v2(char *tx_name, int tx_len, unsigned int tx_index, int nvm_id);
extern int log_svr_tables(const SvrTableEntry_Shr* svr_table);
extern int log_url_table(const SessTableEntry_Shr* sess_table);
extern int log_data_record(unsigned int sess_inst, http_request_Shr* url, char* buf, int buf_length);
extern int log_message_record(unsigned int msg_num, u_ns_ts_t now, char* buf, int buf_length);
extern void flush_logging_buffers(void);
extern void dump_URL_SHR(PageTableEntry_Shr*);

gsl_rng* weib_rangen;
gsl_rng* exp_rangen;

int log_records = 1;

int port_udp_fd = 0;
int runq_udp_fd = 0;

int cpu_min_checks;
int port_min_checks;
int runq_min_checks;
int decrease_connections;

int testidx = -1;
__thread int g_monitor_status;
int start_testidx = -1;  
int g_ns_instance_id = -1;
/* -- This variable stores Session/Testrun and TR number
   -- Found bug on buffer overflow hps went down 10% of current hps
   -- buffer overflow corrupts global variables which leads to unnecessary load on child
*/
char g_test_or_session[32];

inline SvrTableEntry_Shr* get_svr_ptr(action_request_Shr* request, VUser* vptr) {
  NSDL3_SCHEDULE(vptr, NULL, "Method called, vptr = %p", vptr);
  if (request->server_base == NULL)
    return request->index.svr_ptr;
  else
    return (request->server_base + get_value_index(request->index.group_ptr, vptr, "Server Variable"))->server_ptr;
}

#ifdef REQUEST_STITCHING
static void
stitch_request(char* buf, const SegTab_Shr* seg_tab_ptr, VUser* vptr)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called");
  int seg_num = 0, var_num = 0;
  short next = seg_tab_ptr->start;
  int num_seg = seg_tab_ptr->num_segments;
  int num_var = seg_tab_ptr->num_vars;
  PointerTableEntry_Shr *segment_ptr = seg_tab_ptr->segment_start;
  VarOrderTableEntry_Shr *var_ptr = seg_tab_ptr->variable_start;

  if (abs(num_seg-num_var) > 1) {
	NSTL1(NULL, NULL, "STITCH REQUEST(): ERROR, NUM OF SEGMENTS AND VARIABLES NOT CORRECT\n");
	NS_EXIT(-1, "STITCH REQUEST(): ERROR, NUM OF SEGMENTS AND VARIABLES NOT CORRECT");
  }

  do {
    switch (next) {
    case SEG:
      strcat(buf, segment_ptr->big_buf_pointer);
      next = VAR;
      seg_num++;
      segment_ptr++;
      break;
    case VAR:
      strcat(buf, get_var_val(var_ptr->var_ptr, vptr)->big_buf_pointer, 1);
      next = SEG;
      var_num++;
      var_ptr++;
      break;
    }
  } while ((seg_num < num_seg) || (var_num < num_var));
}
#endif /* REQUEST_STITCHING */

inline void
hurl_done(VUser *vptr, action_request_Shr* url_num, HostSvrEntry *hptr, u_ns_ts_t now)
{

  NSDL2_HTTP(vptr, NULL, "Method called for cur_url = %p, vptr->urls_left = %d, hptr->cur_url = %p, hptr->hurl_left = %d", 
                          url_num, vptr->urls_left, hptr->cur_url, hptr->hurl_left);

  vptr->urls_left--;
  NSDL1_HTTP(vptr, NULL, "urls_left dec at hurl_done: %d", vptr->urls_left);
  hptr->cur_url++;
  hptr->hurl_left--;
  NSDL2_HTTP(vptr, NULL, "vptr = %p hptr->hurl_left[%d] ",vptr, hptr, hptr->hurl_left);
}

inline void
repeat_hurl(VUser *vptr, action_request_Shr* url_num, HostSvrEntry *hptr, u_ns_ts_t now)
{

  NSDL2_HTTP(vptr, NULL, "Method called for cur_url = %p, vptr->urls_left = %d, hptr->cur_url = %p, hptr->hurl_left = %d", 
                          url_num, vptr->urls_left, hptr->cur_url, hptr->hurl_left);

  vptr->urls_left++;
  NSDL1_HTTP(vptr, NULL, "urls_left dec at hurl_done: %d", vptr->urls_left);
  //hptr->cur_url--;
  //hptr->hurl_left++;
  NSDL2_HTTP(vptr, NULL, "Exiting repeat_hurl");
}

#if 0
inline void
reset_udp_array() {
  int i;
  int udp_array_idx;
  if (testcase_shr_mem->mode == TC_MEET_SERVER_LOAD)
    for (i = 0; i < total_metric_entries; i++) {
      udp_array_idx = metric_table_shr_mem[i].udp_array_idx;
      udp_array[udp_array_idx].samples_awaited = metric_table_shr_mem[i].min_samples + 1;
      udp_array[udp_array_idx].value_rcv = -1;
    }
/*   if (global_settings->health_monitor_on) { */
/*     udp_array[CPU_HEALTH_IDX].samples_awaited = 2; */
/*     udp_array[CPU_HEALTH_IDX].value_rcv = -1; */
/*   } */
}
#endif


//Need this during auto monitors setup for postgres monitor, because we setup postgres monitor only if reporting is 2
int get_max_report_level_from_non_shr_mem()
{
  int i;
  int max = 0;

  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runProfTable[i].gset.report_level ? max : runProfTable[i].gset.report_level;
  }
  
  return max;
}

int get_max_report_level()
{
  int i;
  int max = 0;

  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runprof_table_shr_mem[i].gset.report_level ? max : runprof_table_shr_mem[i].gset.report_level;
  }
  
  return max;
}

int get_max_log_level()
{
  int i;
  int max = 0;
  
  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runprof_table_shr_mem[i].gset.log_level ? max : runprof_table_shr_mem[i].gset.log_level;
  }
  return max;
}

int get_max_log_dest()
{
  int i;
  int max = 0;
  
  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runprof_table_shr_mem[i].gset.log_dest ? max : runprof_table_shr_mem[i].gset.log_dest;
  }
  
  return max;
}

char* get_version(char* component)
{
  static int version_buf_set = 0;
  FILE *app = NULL;
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
 
  if(version_buf_set == 1)
    return version_buf;

  // Added argument -n so that it gets versin of only netstorm (For 3.2.2 - UserAdmin feature)
  // Note - We can only get version of NS/GUI/CMON as these are on same machine
  // Do not get version of HPD as it can be on NO machine and password less ssh is not for all users
  app = popen("nsu_get_version -n", "r");

  if(app == NULL)
  {
    NS_EXIT(1, CAV_ERR_1000031, "nsu_get_version -n", errno, nslb_strerror(errno));
  }

  while(fgets(version_buf, 128, app))
  {
    if(strstr(version_buf, component) != NULL)
    {
      version_buf[strlen(version_buf) - 1] = '\0';  // Remove new line
      break;
    }
  }
  if(pclose(app) == -1)
    NS_EXIT(1, CAV_ERR_1000030, "nsu_get_version -n", errno, nslb_strerror(errno));

  version_buf_set = 1;
  return version_buf;
}

void create_summary_top(char *testrun)
{
  char buf[600];
  FILE *tfp;

  NSDL2_PARENT(NULL, NULL, "Method called, Testrun = %s", testrun); 
  sprintf(buf, "logs/TR%s/summary.top", testrun);
  if ((tfp = fopen(buf, "w")) == NULL) {
    NSTL1(NULL, NULL, "error in creating the file '%s'\n", buf);
    perror("netstorm");
    NS_EXIT(1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }
  

  int need_to_write_page_dump = 0;
  /* In case of online, we need to show page dump link in report gui therefore we need to
   * make pagedump field enable in summary.top. Whereas in case of offline, page dump link created
   * at post processing
   * Need to add check for online mode if trace level 1 and destination is 0, then page_dump.txt should not be created*/
  if(global_settings->reader_run_mode > 0 && (get_max_tracing_level() != 0) && (get_max_trace_dest() != 0)){
    need_to_write_page_dump = 1;
  }

  char *slash_ptr = NULL;
  char tmp_tr_buff[100 + 1] = {0};
  strcpy(tmp_tr_buff, testrun);

  if((slash_ptr = strstr(tmp_tr_buff, "/")) != NULL)
    *slash_ptr = '\0';

  // Abhishek - 11/10/06
  // Two Additional column are added in summary.top file as Run Time and Virtual Users data
  // Function is called here to convert Run Time from seconds to HH:MM:SS format
  if (global_settings->testname[0] == '\0') {
/*
1.  default/default/<scenario name> should use instead of <scenario name> in summary.top when use 'Absolute Path'
  For Example:
  If execute test withTR Absolute path: 
    nsu_start_test -n test
  Format of summary.top file should be:
    24426|default/default/test|2/9/09  20:07:44|Y|Unavailable|Y|	Unavailable|	Unavailable|Unavailable|	Unavailable|0|1|Archana - Just For Test|W|	00:00:00|1
2.  Project/Subproject/<Scenario Name> should use instead of <scenario name> in summary.top when use 'Relative Path'
  For Example:
  If execute test with Relative path:
    nsu_start_test -n ut_proj_test/ut_subproj_test/test
  Format of summary.top file should be:
    24426|ut_proj_test/ut_subproj_test/test|2/9/09  20:07:44|Y|Unavailable|Y|	Unavailable|Unavailable|Unavailable|	Unavailable|0|1|Archana - Just For Test|W|	00:00:00|1
*/
     fprintf (tfp, "%s|%s/%s/%s|%s|Y|%s|%s|N|N|N|N|%d|%d|%s|W|%s|%d\n", tmp_tr_buff, g_project_name, g_subproject_name, g_testrun, g_test_start_time, need_to_write_page_dump?"Available_New":"N", g_test_user_name,global_settings->wan_env, get_max_report_level(), tmp_tr_buff, (char *)get_time_in_hhmmss((int)(global_settings->test_duration/1000)), global_settings->num_connections);
  } else {
      fprintf (tfp, "%s|%s/%s/%s|%s|Y|%s|%s|N|N|N|N|%d|%d|%s|W|%s|%d\n", tmp_tr_buff, g_project_name, g_subproject_name, g_testrun, g_test_start_time, need_to_write_page_dump?"Available_New":"N", g_test_user_name, global_settings->wan_env, get_max_report_level(), global_settings->testname, (char *)get_time_in_hhmmss((int)(global_settings->test_duration/1000)), global_settings->num_connections);
  }
  fclose(tfp);
}
/* Added flag create_summary_top_file for switching test run,
 * in case of NS_MODE we need to create summary.top file of each new test run
 * */
  
void
create_report_file(int testidx, int create_summary_top_in_partition) {
  char buf[600];
  char symlink_buf[600];
  //time_t tt;
  //struct tm *time_ptr;
  static int first_time = 1;
  struct stat s;
  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  sprintf(buf, "logs/%s/progress.report", global_settings->tr_or_partition);
  //printf("Creating Progress Report file %s\n", buf); //TODO: Printing 2 times on console..need to fix this
  NSTL1(NULL,NULL, "Creating Progress Report file %s", buf);

  //In case of new TR do not print this line
  if(first_time)
    //printf("Initialing VUsers...\n");
    NSTL1(NULL,NULL, "Initialing VUsers...");

  first_time = 0;
  fflush(NULL);
  if ((rfp = fopen(buf, "w")) == NULL) {
    NSTL1(NULL, NULL, "Error in creating the report file\n");
    NS_EXIT(1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }

  /************************************************************************
   [BugId:94911] Getting -ve Values for Vusers in Generator's progress report.

  => From release 4.4.0 Progress Report on generator has been disabled because
  => we are send compressed progress report generator to controller.
  => Using compressed report so progress report can not be printed.
  *************************************************************************/
  if (loader_opcode == CLIENT_LOADER)
  {
    fprintf(rfp, "Progress report is no longer supported in generator!!!\n");
    fflush(rfp);
  }

  //Creating symbolic link to 'partition/progress.report' in TR directory
  sprintf(buf, "%s/logs/%s/progress.report", g_ns_wdir, global_settings->tr_or_partition);
  sprintf(symlink_buf, "logs/TR%d/progress.report", testidx);
  NSDL2_PARENT(NULL, NULL, "Creating symbolic link %s to %s", symlink_buf, buf);

  if((remove(symlink_buf) < 0) && (errno != ENOENT)) //removing existing link
  {
    //ENOENT -> file or directory doesn't exist
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                     "Could not remove symbolic link %s", symlink_buf);  
  }
  if(symlink(buf, symlink_buf) < 0)  //creating link
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                     "Could not create symbolic link %s to %s", symlink_buf, buf);  
  }
  sprintf(buf, "logs/TR%d/summary.report", testidx);
  if ((srfp = fopen(buf, "w")) == NULL) {
    NSTL1(NULL, NULL, "error in creating the summary report file\n");
    NS_EXIT(1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }
  //In case of switching test run we need to create summary top file irrespective of reset test time stamp flag
  if (create_summary_top_in_partition) { 
    NSDL2_PARENT(NULL, NULL, "If NS_MODE is enable then create summary top file, irrespective of reset test time stamp flag");
    //Skipping TR (initial 2 characters)
    create_summary_top(global_settings->tr_or_partition + 2); 
  } else if (global_settings->reset_test_start_time_stamp == 0) { //If TIME stamp reset is off then create summary top here as previous.
    //fixed bug 8114
    //previously if test exited after first partition creation and before summary.top creation,
    //then summary.top was not created on restart of test.
    //now check if summary.top exists and create if doesn't exist on test start or restart
    sprintf(buf, "logs/TR%d/summary.top", testidx);
    //call this function testrun id is created first time or summary.top doesn't exists
    if((g_first_partition_idx == g_partition_idx) || stat(buf, &s) != 0)
    //if(stat(buf, &s) != 0) //summary.top doesn't exist
    //do not call this function when 'nde mode & testrun restarted'
    //if ((global_settings->partition_creation_mode == 0) || (g_first_partition_idx == g_partition_idx))
    {
      NSDL2_PARENT(NULL, NULL, "Reset test time stamp flag is not set, creating summary top before starting monitors");
      sprintf(buf, "%d", testidx);
      create_summary_top(buf);
    }
  }
   
  //abhay :- update scenario file name in TR summary.top BUGID #45027
  if(global_settings->continuous_monitoring_mode)
  {  
    char partition_scen_file[512];
    sprintf(partition_scen_file, "%s/%s/%s", g_project_name, g_subproject_name, g_testrun);
    update_summary_top_field(1, partition_scen_file, 0); 
  } 


  // Start: This code has been moved to just before calling create_report() : Anuj 16/04/08
  //time(&tt);
  //time_ptr = localtime(&tt);
  //sprintf(g_test_start_time, "%d/%d/%2.2d  %02d:%02d:%02d",
	//(time_ptr->tm_mon + 1), time_ptr->tm_mday, (time_ptr->tm_year-100),
	//time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);
  // End : Anuj 16/04/08


  //fprint3f(rfp, srfp, NULL, get_version("NetStorm"));
  fprint3f(rfp, srfp, NULL, "%s\n", version_buf);
  fprint3f(rfp, srfp, NULL, "Test Case Name: %s\n", g_testrun);
  fprint3f(rfp, srfp, NULL, "Test Run Number: %d\n", testidx);
  fprint3f(rfp, srfp, NULL, "Test Started At: %s\n", g_test_start_time);
  fprint3f(rfp, srfp, NULL, "Test Configuration\n\n");

  if(g_debug_script)
  {
    char *version_ptr = version_buf;
    if((version_ptr = strchr(version_buf, ':')) != NULL)
      version_ptr++;
    
    NS_DT1(NULL, NULL, DM_L1, MM_PARENT, "Starting Test Run %d", testidx);
    NS_DT1(NULL, NULL, DM_L1, MM_PARENT, "Cavisson Debugger %s", version_ptr);
  }

  switch (testcase_shr_mem->mode) {
  case TC_FIX_CONCURRENT_USERS:
    fprint3f(rfp, srfp, NULL, "Test Mode: Fix Concurrent Users\n");
    //fprint3f(rfp, srfp, NULL, "Number of Users: %d\n", global_settings->num_connections);
    break;
  case TC_FIX_USER_RATE:
    if(global_settings->replay_mode == 0)
      fprint3f(rfp, srfp, NULL, "Test Mode: Fix Session Rate\n");
    else
      fprint3f(rfp, srfp, NULL, "Test Mode: Replay Access Logs\n");
    //fprint3f(rfp, srfp, NULL, "User Rate: %d per minute\n", global_settings->vuser_rpm);
    break;
  case TC_MIXED_MODE:
    fprint3f(rfp, srfp, NULL, "Test Mode: Mixed Mode\n");
    //fprint3f(rfp, srfp, NULL, "User Rate: %d per minute\n", global_settings->vuser_rpm);
    break;
  case TC_FIX_HIT_RATE:
    fprint3f(rfp, srfp, NULL, "Test Mode: Fix Hit Rate\n");
    //fprint3f(rfp, srfp, NULL, "Target URL Hit Rate : %d per minute\n", testcase_shr_mem->target_rate);
    break;
  case TC_FIX_PAGE_RATE:
    fprint3f(rfp, srfp, NULL, "Test Mode: Fix Page Rate\n");
    //fprint3f(rfp, srfp, NULL, "Target Page Hit Rate : %d per minute\n", testcase_shr_mem->target_rate);
    break;
  case TC_FIX_TX_RATE:
    fprint3f(rfp, srfp, NULL, "Test Mode: Fix Transaction Users\n");
    //fprint3f(rfp, srfp, NULL, "Target Transaction Rate : %d per minute\n", testcase_shr_mem->target_rate);
    break;
  case TC_MEET_SLA:
    fprint3f(rfp, srfp, NULL, "Test Mode: Meet SLA\n");
    break;
  case TC_MEET_SERVER_LOAD:
    fprint3f(rfp, srfp, NULL, "Test Mode: Meet Server Load\n");
    break;
  case TC_FIX_MEAN_USERS:
    fprint3f(rfp, srfp, NULL, "Test Mode: Fix mean users\n");
    //fprint3f(rfp, srfp, NULL, "Number of Users: %d\n", global_settings->num_connections);
    break;
  }
/*
  if (global_settings->gui_server_addr[0]) {
	fprint3f(rfp, srfp, NULL, "GUI SERVER: %s:%hd\n", global_settings->gui_server_addr, global_settings->gui_server_port);
  } else {
	fprint3f(rfp, srfp, NULL, "GUI SERVER: None\n");
  }

  if ((run_mode == FIND_NUM_USERS) && (global_settings->load_key == 1)) {
    fprint3f(rfp, srfp, NULL, "Run Mode: Number of Users \n");
  }

*/

  if (run_mode == FIND_NUM_USERS) {
    fprint3f(rfp, srfp, NULL, "Each Discovery Run would be %d seconds\n", global_settings->test_stab_time);
  }
/*
  if ((testcase_shr_mem->mode == TC_FIX_CONCURRENT_USERS) || (testcase_shr_mem->mode == TC_FIX_USER_RATE)) {
    char buf[1024]="\0";
    get_ramp_up_info(buf);
    fprint3f(rfp, srfp, NULL, "Ramp Up: %s\n", buf);
  } else {
    fprint3f(rfp, srfp, NULL, "User Rate Mode: %s\n", global_settings->user_rate_mode?"Random User Creation iid":"Constant User Creation id");
  }
  // Abhishek (12/19/06)- Now Test Duration in HH:MM:SS format will return by get_time_in_hhmmss()
  // change global_settings->test_stab_time to (int)(global_settings->test_duration/1000)
  char completion_time_buf[1024]="\0";
  get_test_completion_time_detail(completion_time_buf);
  fprint3f(rfp, srfp, NULL, "Target Completion: %s\n", completion_time_buf);
*/
  if (group_default_settings->idle_secs != DEFAULT_IDLE_MSECS) {
    fprint3f(rfp, srfp, NULL, "Client TimeOut (MilliSeconds): %d\n", group_default_settings->idle_secs);
  }
/*
#ifndef RMI_MODE
  fprint3f(rfp, srfp, NULL, "Percentage of Keep-Alive requests: %d\n", group_default_settings->ka_pct);
  fprint3f(rfp, srfp, NULL, "Min & Max Number of Keep-Alive requests on a Keep-Alive connection: %d %d\n", 
           group_default_settings->num_ka_min, group_default_settings->num_ka_range + group_default_settings->num_ka_min);
#endif
  //fprintf(rfp, "Percentage of Click Away requests )(without any wait): %d\n", global_settings->clickaway_pct);
  if (total_thinkprof_entries) {
    if (thinkprof_table_shr_mem[0].mode != 0) {
      fprint3f(rfp, srfp, NULL, "Global Average Think Time (MilliSeconds): %d\n", thinkprof_table_shr_mem[0].avg_time);
    } else {
      fprint3f(rfp, srfp, NULL, "Average Think Time (MilliSeconds): 0\n");
    }
  }
*/
/*
  if (global_settings->num_dirs) {
    fprint3f(rfp, srfp, NULL, "Using SpecWeb file set with %d directories\n\n", global_settings->num_dirs);
  } else {
    fprint3f(rfp, srfp, NULL, "Using %d URL's\n\n", total_request_entries);
  }
*/
}

static void write_cur_instance(int trnum)
{
  FILE *fp;
  char buff[0xff];

  sprintf(buff, "%s/logs/TR%d/.curInstance", g_ns_wdir, trnum);
  if(!(fp = fopen(buff, "w")))
  {
    //Not exiting here as this is not mandatory
    fprintf(stderr, "Failed to write instance %s inside testrun file %s", g_test_inst_file, buff);
    return;
  }
  fprintf(fp, "%s", g_test_inst_file);
  fclose(fp);
}

static void create_inst_file(int trnum)
{
  char fname[512];
  char buf[1024];
  char err_msg[100];
  int max, ret, cur_running_inst_cnt,ret1;
  FILE *fp;
  TestRunList *test_run_info;
  test_run_info = (TestRunList *)malloc(128 * sizeof(TestRunList)); //alloc
  memset(test_run_info, 0, (128 * sizeof(TestRunList)));
  if(!g_test_inst_file[0]) //if -i is not provided then create urself
    sprintf(g_test_inst_file, "ns_inst%lld", nslb_get_cur_time_in_ms());

  sprintf(g_test_inst_file, "%s_%d", g_test_inst_file, g_parent_pid);
  write_cur_instance(trnum);
  if ((fp = fopen(".tmp/.max", "r+")) && (fgets(buf, 1024, fp))) {
    max = atoi(buf);
  } else
    max = 100;
  //Find current running instances from nsu_show_test_logs functions
  cur_running_inst_cnt = get_running_test_runs_ex(test_run_info, NULL);
  if(cur_running_inst_cnt >= max)
  {
    NS_EXIT (1, "Max Concurrent NetStorm instances allowed %d are used. Please stop some test and restart a new one", max);
  }
  free(test_run_info); //free

  sprintf(fname, "%s/.tmp/%s", g_ns_wdir, g_test_inst_file);
  if(mkdir(fname, 0755))
  {
    if(errno == ENOSPC)
    {
      NS_EXIT(-1, "Disk running out of space, can not start new test run\n Please release some space and try again");
    }
    else if(errno == EACCES)
    {
      NS_EXIT(-1, "Error: %s/webapps/logs do not have write permission to create test run directory\n"
                      "       Please correct permission attributes, and try again", g_ns_wdir);
    }
    else
    {
      NS_EXIT(-1, "Failed to create instance directory %s, error:%s", fname, nslb_strerror(errno));
    }
  }
  sprintf(fname, "%s/.tmp/%s/keys", g_ns_wdir, g_test_inst_file);
  //if not then create file
  if((g_instance_fp = fopen(fname, "w")))
  {
    //fprintf(g_instance_fp, "%d\n", g_parent_pid);
    fprintf(g_instance_fp, "%d\n", trnum);
    fflush(g_instance_fp);  //as we need testrun on parsing also to stop test
    if(g_enable_test_init_stat){
      WRITE_TR_STATUS 
    }
  }
  sprintf(g_ns_tmpdir, ".tmp/%s", g_test_inst_file);
  
  ret = nslb_write_all_process_pid(g_parent_pid, "netstorm parent's pid", g_ns_wdir, trnum, "w");
  if( ret == -1 )
  {
    NSTL1_OUT(NULL, NULL, "failed to open the netstorm parent's pid file\n");
    END_TEST_RUN;
  }
 //char tmp_cavmain[20];
 //sprintf(tmp_cavmain,"cavmain.pid");
 ret1 = nslb_write_process_pid(getpid(), "netstorm parent's pid", g_ns_wdir, trnum, "w","CavMain.pid",err_msg);
  if( ret1 == -1 )
  {
    NSTL1_OUT(NULL, NULL, "failed to open the netstorm parent's pid file\n");
    END_TEST_RUN;
  }
}

//This method checks if test can run in cont mon mode
//All of below 4 conditions must be true to run test in cont mon mode
//1. Machine must be in nde mode (/home/cavisson/etc/cav.conf file)
//2. nde.testRunNum keyword must be specified in webapps/sys/config.ini file with value >1000
//3. test must be run with ndeadmin user
//4. test must be run in gui mode

static int check_cont_mon_mode()
{
  int test_id = -1;

  //nde_get_testid() returns negative value in case of error(eg nde.testRunNum keyword not found)
  //In case of error test will run in NDE mode without showing warning.
  //if user has provided TR num and if it is between 1 and 1000, test will run in NDE mode, warning will be shown.
  //pw = getpwuid(getuid());
  //cont mon test can be run by ndeadmin user in gui mode only
  if (!strcmp(g_test_user_role, "admin"))
  {
    if(!global_settings->gui_server_addr[0])
    {
      NSTL1(NULL, NULL, "Warning: continuous monitoring test can only be allowed to user who has capailities to rum the CM test in GUI mode. "
                        "Test is not running in GUI mode. Hence continuing test in NDE mode.\n");
      return -1;
    }

    if((strcmp(g_cavinfo.config, "NDE") != 0) && (strcmp(g_cavinfo.config, "NV") != 0) && (strcmp(g_cavinfo.config, "NO") != 0) && (strcmp(g_cavinfo.config, "NF") != 0) && (strcmp(g_cavinfo.config, "NCH") != 0) && (strcmp(g_cavinfo.config, "SM") != 0)) // /home/cavisson/etc/cav.conf must have NDE/NV/NO/NF/NCH mode
    {
      NSTL1(NULL, NULL, "Warning: continuous monitoring test can only be allowed in NDE/NV/NO/NF/NVSM/NCH mode. "
                      "To run continuous monitoring test change mode in /home/cavisson/etc/cav.conf to NDE/NV/NO/NF/NVSM/NCHM/NCH.\n");
      return -1;
    }

    test_id = nde_get_testid(); //getting test id from config.ini, testid must be > 1000 

    if(test_id <= 0)
    {
      NSTL1(NULL, NULL, "Warning: No testrun Num is provided in continuous monitoring test . "
                      "Provide test run in work/webapps/sys/config.ini using nde.testRunNum keyword\n");
      return -1;
    }
    else if(test_id == 999 && (strcmp(g_cavinfo.config, "NCH") != 0)) 
    {
      global_settings->continuous_monitoring_mode = 1;
      return test_id;
    }
    else if(test_id <= 1000) //if TR num is in between 1 and 1000 display warning and run NDE mode.
    {
      NSTL1(NULL, NULL, "Warning: Test Run Num %d in config.ini file is <= 1000, "
                        "cannot run test in continuous monitoring mode\n", test_id);
      return -1;
    }
    else if(test_id > 1000) //if TR num is >1000; run continuous monitoring.
    {
      global_settings->continuous_monitoring_mode = 1;
    }
  }
  return test_id;
}

int get_testid(void) 
{  
  int return_test_id = -1, status = 1;
  char buf[MAX_LINE_LENGTH];
  static int create_ns_inst = 1;
  static int flag; 

  NSDL2_SCHEDULE(NULL, NULL, "Method called");  
  do
  {
    //checking in which mode test should run(cont mon mode or other mode)
    return_test_id = check_cont_mon_mode();
    //if test is not in continuous monitoring mode; generate TR number
    if(return_test_id <= 0)
      return_test_id = nslb_get_testid(); //Code has been moved to library

    NSDL2_SCHEDULE(NULL, NULL, "Test Run ID = %d", return_test_id);  
  
    //Create the logs/TRXXX/ns_logs directory
    // sprintf(buf, "%s/logs/TR%d/ns_logs/", g_ns_wdir, return_test_id); CHECK ???????
    sprintf(buf, "%s/logs/TR%d/", g_ns_wdir, return_test_id);
    //This is not doing here since we are setting umask in netstorm.env
    //umask(0);
    if (mkdir_ex(buf) != 1) {  
      //perror("mkdir");
      if(errno == ENOSPC)
      {
        NS_EXIT(-1, CAV_ERR_1000008);
      }
      if(errno == EACCES)
      {
        NS_EXIT(-1, CAV_ERR_1000009, getenv("NS_WDIR"));
      }
      MKDIR_ERR_MSG("logs/TR/ns_logs", buf)

      /*  In case of continuous monitoring, testidx dir may exist as user can restart the same test run.
       *  Then test data will be stored in same dir */
      if(global_settings->continuous_monitoring_mode)
        break;
      else
      {
        fprintf(stderr, "Test run id %d already used, getting a new test run id\n", return_test_id);
        continue;
      }
    }
    else
      status = 0;
  }while(status);

  //For NDE we need to only update netstorm.tid file
  if(create_ns_inst)
  {
    create_inst_file(return_test_id);
    create_ns_inst = 0;
  }

  if(check_if_repo_testidx())
  {
    sprintf(buf, "%s/logs/TR%d/.nsrepo", g_ns_wdir, return_test_id);
    FILE *fp = fopen(buf, "w+");
    if(fp)
    {
      fprintf(fp, "%d|0", return_test_id);
      fclose(fp);
    }
  }
  
  fprintf(stderr, "Starting Test run %d\n", return_test_id);
  
  //we are changing mode of TR to 777 here, as in CHECK_MONITOR it FTP to a dir using netstorm id.
  sprintf(buf, "logs/TR%d", return_test_id);
  int ret = chmod(buf, S_IRUSR|S_IWUSR|S_IXUSR |S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
  //TODO: just added to avoid erro log
  //need to check again why return type is error for system
  if(!flag) {
    if(ret == -1) 
    {
      NS_EXIT(-1, "Unable to change mode of logs/TR%d, error = %s", return_test_id, nslb_strerror(errno));
    }
  }
  
  //Creating ns_files dir in TR/ for some files used by gui: eg. netstorm_hosts.dat & for state_transition.dat
  //sprintf(buf, "%s/logs/TR%d/ns_files", g_ns_wdir, testidx);
  // Create ns_file so that we can copy files into it 
  /*if (mkdir(buf, 0775) != 0) 
  {
    MKDIR_ERR_MSG("logs", buf) //in case of continuous mon, dir may exist as test may restart; hence error msg is not displayed
  }*/

  // Create runtime_changes directory which will contain all runtime_changes information
  sprintf(buf, "%s/logs/TR%d/runtime_changes", g_ns_wdir, return_test_id);
  if (mkdir(buf, 0775) != 0) 
  {
    MKDIR_ERR_MSG("runtime_changes", buf)
  }

  /*gaurav: creating directory ready report for test initialization status feature*/
  sprintf(buf, "%s/logs/TR%d/ready_reports", g_ns_wdir, return_test_id);
  if (mkdir(buf, 0775) != 0) 
  {
    MKDIR_ERR_MSG("ready_reports", buf)
  }

  sprintf(g_test_or_session, "%s %d", (global_settings->continuous_monitoring_mode?"Session":"Testrun"), return_test_id);
  return return_test_id;
}

//This mehtod to check user is a authorized 'Active' netstorm user or not
int check_authenticate_user(char *username, char *cmd_name, char *err_msg)
{
  char cmd[1024];
  int return_status;
  sprintf(cmd, "nsu_check_user %s %s", username,  cmd_name);
  return_status = nslb_system(cmd,1,err_msg);
  if(return_status != 0)
  {
    //NSTL1(NULL, NULL, "Error in running command '%s' to check authorized netstorm user", cmd);
    //NS_EXIT(-1, "Error in running command '%s' to check authorized netstorm user", cmd);
    sprintf(err_msg, "%s username is not authentic user", username);
    return -1;
  }
  if (return_status > 0)
  {
    //NS_EXIT (1, "Failed to run system command.");
    sprintf(err_msg, "%s username is not authentic user", username);
    return -1;
  }
  return 0;
}

int confirm_netstorm_uid(char *cmd_name, char *err_msg)
{
  struct passwd *pw;
  //struct group *gr; 
  //int ret;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  /* The last three arguments are just padding, because the
  * system call requires five arguments.
  */
  //prctl(PR_SET_DUMPABLE,1,42,42,42);

  pw = getpwuid(getuid());
  if (pw == NULL) {
    NS_EXIT(-1, CAV_ERR_1000026, errno, nslb_strerror(errno));
  }
  if(strcmp(pw->pw_name, "cavisson"))
  {
    NS_EXIT(-1, CAV_ERR_1031001, pw->pw_name);
  }
  strcpy(g_ns_login_user, pw->pw_name); //saving linux login user
#if 0
  if((ret = check_authenticate_user(pw->pw_name, cmd_name, err_msg)) == -1)
    return ret;
  /* NetCloud: We require user name and group name for changing the ownership of test runs coming
  * from generator*/
  gid_t group_id = pw->pw_gid;
  gr = getgrgid (group_id);     
  if (gr == NULL) {
    //NS_EXIT (1, "Error: Unable to get the group name");
    sprintf(err_msg, "Failed to get the group name");
    return -1;
  }    
  sprintf(netstorm_usr_and_grp_name, "%s:%s", pw->pw_name, gr->gr_name);
  NSDL2_SCHEDULE(NULL, NULL, "Current test is running from following user = %s", netstorm_usr_and_grp_name);
/*	if (!strcmp(pw->pw_name, "root")) {
		printf("Error: netstorm can not be run as '%s'\n", pw->pw_name);
		//printf("Error: netstorm must be run as 'netstorm' user only. Currently being run as '%s'\n", pw->pw_name);
		exit(1);
	}
*/
#endif
  return 0;
}

// This method to get scenario name without .conf 
void
set_logfile_names() {
  char* file_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, g_conf_file = %s", g_conf_file);
//  strcpy(g_prefix, "-");
  strcpy(g_testrun, g_conf_file);

  //Take the file component of g_conf_file
  file_ptr = strrchr(g_conf_file, '/');
  if (file_ptr) {
      file_ptr++;
      strcpy(g_testrun, file_ptr);
  } else
      strcpy(g_testrun, g_conf_file);
  file_ptr = strrchr(g_testrun, '.');
  if (!file_ptr || strcmp(file_ptr, ".conf")) {
    NS_EXIT(-1, CAV_ERR_1011170);
  }
  *file_ptr = '\0';
  if (strchr(g_testrun, ',')) {
    NS_EXIT(-1, CAV_ERR_1011167);
  }

  strcpy(g_url_file, "-.capture");
/*#ifndef RMI_MODE
  sprintf(g_det_url_file, "%s.detail", g_prefix);
#endif*/
}

void make_ns_common_files_dir_or_link(char *csv_path)
{
  char buf1[1024] = "";
  char buf2[1024] = "";
    
  /* If multidisk keyword is on, then create csv directory at user given path */
  if(global_settings->multidisk_nscsv_path && global_settings->multidisk_nscsv_path[0])
  {
    /* Create reports directory */
    sprintf(buf1, "%s/logs/%s/reports/", g_ns_wdir, csv_path);
    mkdir_ex(buf1);

    /* Crreate csv directory on muldisk path */
    sprintf(buf1, "%s/%s/reports/csv/", global_settings->multidisk_nscsv_path, csv_path);
    mkdir_ex(buf1);

    /* Make soft link */
    sprintf(buf2, "%s/logs/%s/reports/csv", g_ns_wdir, csv_path);
    if(symlink(buf1, buf2) < 0)
    {
      //NSTL1_OUT(NULL, NULL, "Error = %s", nslb_strerror(errno));
      NSTL1(NULL, NULL, "Error = %s", nslb_strerror(errno));
    }   
  }
  else
  {
    sprintf(buf1, "%s/logs/%s/reports/csv/", g_ns_wdir, csv_path);
    mkdir_ex(buf1);
  }
}

void make_ns_raw_data_dir_or_link(char *raw_data_path)
{
  char buf1[1024] = "";
  char buf2[1024] = "";
    
  /* If multidisk keyword is on, then create csv directory at user given path */
  if(global_settings->multidisk_ns_rawdata_path && global_settings->multidisk_ns_rawdata_path[0])
  {
    /* Create reports directory */
    sprintf(buf1, "%s/logs/%s/reports/", g_ns_wdir, raw_data_path);
    mkdir_ex(buf1);

    /* Crreate csv directory on muldisk path */
    sprintf(buf1, "%s/%s/reports/raw_data/", global_settings->multidisk_ns_rawdata_path, raw_data_path);
    mkdir_ex(buf1);

    /* Make soft link */
    sprintf(buf2, "%s/logs/%s/reports/raw_data", g_ns_wdir, raw_data_path);
    if(symlink(buf1, buf2) < 0)
    {
      //NSTL1_OUT(NULL, NULL, "Error = %s", nslb_strerror(errno));
      NSTL1(NULL, NULL, "Error = %s", nslb_strerror(errno));
    }   
  }
  else
  {
    sprintf(buf1, "%s/logs/%s/reports/raw_data/", g_ns_wdir, raw_data_path);
    mkdir_ex(buf1);
  }
}


void set_log_dirs() {
char buf[1024];
char symlink_buf[1024];
char ready_reports_path[600];
char ns_log_path[600];//Create directory ns_log_path in TR 
//char reports_dir_path[1024];
struct stat s;
int ret;

  NSDL1_SCHEDULE(NULL, NULL, "Method called");  

  //ssl_logs = fopen("logs/ssl_log", "w+");
  //Bug-94145: Need to copy .rbu_parameter.csv inside TR script, it will be used to stop VNC and clean profile
  if ((global_settings->script_copy_to_tr != DO_NOT_COPY_SRCIPT_TO_TR) || (global_settings->protocol_enabled & RBU_API_USED))
  {
    //Creating symbolic link to 'partition/scripts' in TR directory
    sprintf(buf, "%s/logs/%s/scripts", g_ns_wdir, global_settings->tr_or_partition);
    sprintf(symlink_buf, "logs/TR%d/scripts", testidx);
    NSDL2_PARENT(NULL, NULL, "Creating symbolic link %s to %s", symlink_buf, buf);

    if((remove(symlink_buf) < 0) && (errno != ENOENT)) //removing existing link
    {
      //ENOENT -> file or directory doesn't exist
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                       "Could not remove symbolic link %s", symlink_buf);
    }
    if(symlink(buf, symlink_buf) < 0)  //creating link
    { 
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                       "Could not create symbolic link %s to %s", symlink_buf, buf);
    }
  }

  if (global_settings->interactive) {
      sprintf(buf, "logs/%s/data", global_settings->tr_or_partition);
      if (mkdir(buf, 0775) != 0) {
        NS_EXIT(-1, CAV_ERR_1000005, buf, errno, nslb_strerror(errno));
      }
  }
  #if 0
  //create ns_logs dir in TR also in partition mode
  if(global_settings->partition_creation_mode > 0)
  {
    sprintf(buf, "mkdir -p -m 777 logs/TR%d/ns_logs", testidx);
    system(buf);
  }
  #endif
  //sprintf(buf, "mkdir -m 777 logs/%s/ns_logs", global_settings->tr_or_partition);
  //system(buf);

  //Create req_rep directory for URL request response files 
  sprintf(buf, "mkdir -p -m 777 logs/%s/ns_logs/req_rep", global_settings->tr_or_partition);
  ret = system(buf);
  
  if (WEXITSTATUS(ret) == 1){
    NSTL1(NULL, NULL, "Unable to create url request response directory\n");
    NS_EXIT(-1, CAV_ERR_1000019, buf, errno, nslb_strerror(errno));
  }

  //Create logging directory for njvm. 
  {
    sprintf(buf, "mkdir -p -m 777 logs/%s/ns_logs/njvm", global_settings->tr_or_common_files);
    ret = system(buf);
   
    if (WEXITSTATUS(ret) == 1){
      NSTL1(NULL, NULL, "Unable to create njvm gc logging directory\n");
      NS_EXIT(-1, CAV_ERR_1000019, buf, errno, nslb_strerror(errno));
    }
  }
  //Create rbu_logs/snap_shots dir
  if(global_settings->protocol_enabled & RBU_API_USED)
  {
    make_rbu_logs_dir_and_link();
  }
  /*set nd log directories if nd is enable. Moving to url.c
  if(global_settings->net_diagnostics_server[0] != '\0') 
  {
    make_ndlogs_dir_and_link();
  }*/
/*
  sprintf(buf, "mkdir -p -m 0755 logs/TR%d/scenarios/%s/%s", testidx, g_project_name, g_subproject_name); 
  if(system(buf) == -1 ) //done by arun 
  {
    NSTL1_OUT(NULL, NULL, "'%s' failed.\n", buf);
    exit (-1);
  }
  if (mkdir(buf, 0775) != 0) 
  {
    perror("mkdir");
    printf("Unable to Create directory %s \n", buf);
    exit(1);
  }
*/

  /* Added reports directory in TR to store csv and slog and dlog files in csv and raw_data directories. */
  
  make_ns_common_files_dir_or_link(global_settings->tr_or_partition);
  make_ns_raw_data_dir_or_link(global_settings->tr_or_partition);
  
  make_ns_common_files_dir_or_link(global_settings->tr_or_common_files);
  make_ns_raw_data_dir_or_link(global_settings->tr_or_common_files);

  // Added in 3.8.2 for keeping dyn host data file
  // Bug fixed 3804 Changing the permission of Ready report dir
  sprintf(ready_reports_path, "logs/TR%d/ready_reports", testidx); 
  sprintf(buf, "mkdir -p -m 777 %s; touch %s/drill_down_query.log %s/drill_down_query_err.log; chmod 777 %s/*", ready_reports_path, ready_reports_path, ready_reports_path, ready_reports_path); 
  system(buf);
  //Create blank generator_data.csv file, will fill generator details in case of MASTER_LOADER
  sprintf(buf, "touch logs/%s/reports/csv/generator_table.csv", global_settings->tr_or_common_files); 
  system(buf);
  /* Added ns_log directory in TR to store ns_trace.log file*/
  sprintf(ns_log_path, "logs/%s/ns_logs/http_req_rep_body", global_settings->tr_or_common_files);
  sprintf(buf, "mkdir -p -m 777 %s", ns_log_path); 
  system(buf);
 
  #if 0
  //Shift url_req.. and url_resp.. files into ns_logs/ns_req_rep
  ns_log_path[0] = 0;
  sprintf(ns_log_path, "logs/TR%d/ns_logs/ns_req_rep", testidx);
  sprintf(buf, "mkdir -p -m 777 %s", ns_log_path); 
  system(buf);
  #endif

  #ifndef CAV_MAIN
  //sprintf(buf, "cp %s logs/TR%d/scenarios/%s/%s", g_conf_file, testidx, g_project_name, g_subproject_name);
  sprintf(buf, "cp %s logs/TR%d/", g_conf_file, testidx); // : arun
  system(buf);
  sprintf(buf, "cp %s logs/%s/", g_conf_file, global_settings->tr_or_partition);
  system(buf);
  sprintf(buf, "cp %s logs/%s/sorted_scenario.conf", g_sorted_conf_file, global_settings->tr_or_partition);
  system(buf);
  sprintf(buf, "%s/logs/TR%d/sorted_scenario.conf", g_ns_wdir, testidx);
  //if(lstat(buf, &s) == 0)
  if(stat(buf, &s) == 0)
  {
    sprintf(buf, "rm %s/logs/TR%d/sorted_scenario.conf", g_ns_wdir, testidx);
    system(buf);
  }
  sprintf(buf, "%s/logs/TR%d/scenario", g_ns_wdir, testidx);
  if(lstat(buf, &s) == 0)
  {
    sprintf(buf, "unlink %s/logs/TR%d/scenario", g_ns_wdir, testidx);
    system(buf);
  }
  sprintf(buf, "ln %s/logs/%s/sorted_scenario.conf logs/TR%d/sorted_scenario.conf  > /dev/null 2>&1", 
                g_ns_wdir, global_settings->tr_or_partition, testidx); 
  system(buf);
  
  //sprintf(buf, "ln logs/TR%d/scenarios/%s/%s/%s.conf logs/TR%d/scenario", testidx, g_project_name, g_subproject_name, g_testrun, testidx);
  sprintf(buf, "ln %s/logs/%s/%s logs/TR%d/scenario  > /dev/null 2>&1", 
               g_ns_wdir, global_settings->tr_or_partition, basename(g_conf_file), testidx);
  system(buf);
  /*bug id: 101320: ToDo: TBD with DJA*/
  sprintf(buf, "cp scenarios/ctl_config logs/TR%d >/dev/null 2>&1 ", testidx);  
  system(buf);
  #endif
  /*
  if (global_settings->exclude_failed_agg) {
      sprintf(buf, "touch logs/TR%d/exclude_failed_agg", testidx);
      system(buf);
  } */

  if(debug_trace_log_value != 0 && (global_settings->protocol_enabled & RBU_API_USED))
  {
    sprintf(buf, "mkdir -p -m 777 %s/logs/%s/execution_log", g_ns_wdir, global_settings->tr_or_partition);
    nslb_system2(buf);
    NSTL1(NULL, NULL, "Created execution_log directory '%s/logs/%s/execution_log'", g_ns_wdir, global_settings->tr_or_partition);
  }
}

void *
copy_scripts_to_tr() {
  char buf[1024];
  int i;
  SessTableEntry_Shr* sess_table;
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);
  init_stage(NS_COPY_SCRIPTS);
  NSDL2_SCHEDULE(NULL, NULL, "Method called");  
  sess_table = session_table_shr_mem;

  for (i = 0; i < total_sess_entries; i++, sess_table++) {
      //Manish: we dont wan't to copy .version dir into logs because this dir have huge data and copying so much huge data int log is costly so first we make a dir in TRxx and copy by *, Note cp can copy hidden files or dir
      
      write_log_file(NS_COPY_SCRIPTS, "Copying script %s to partition (%d out of %d)",
              get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"), i+1, total_sess_entries);
      sprintf(buf, "mkdir -p %s/logs/%s/scripts/%s >/dev/null 2>&1", g_ns_wdir, global_settings->tr_or_partition, 
                    get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"));
      if(system(buf))
      {
        NSTL1(NULL, NULL, "Error: runing cmd '%s'", buf);
      }

      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
      if(global_settings->script_copy_to_tr == COPY_SRCIPT_DIR_TO_TR)
      {
        sprintf(buf, "find %s/%s -mindepth 1 -maxdepth 1 \\! -name \".version\" \\! -name \"%s\" -exec cp -r {} %s/logs/%s/scripts/%s \\; >/dev/null 2>&1", GET_NS_TA_DIR(), get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"), sess_table->sess_name, g_ns_wdir, global_settings->tr_or_partition, get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"));
      }
      else if(global_settings->script_copy_to_tr == COPY_SCRIPT_NOT_SUBDIR_TO_TR){
       sprintf(buf, "find %s/%s -mindepth 1 -maxdepth 1 -type f \\! -name \".version\" \\! -name \"%s\" -exec cp {} %s/logs/%s/scripts/%s \\; >/dev/null 2>&1", GET_NS_TA_DIR(), get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"), sess_table->sess_name, g_ns_wdir, global_settings->tr_or_partition, get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"));
      }
      else if(global_settings->protocol_enabled & RBU_API_USED) //Copy .rbu_parameter.csv to TR, it will be used to stop VNC and clean profile
      {
        sprintf(buf, "cp %s/%s/.rbu_parameter.csv %s/logs/%s/scripts/%s/ >/dev/null 2>&1",
                      GET_NS_TA_DIR(), get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"),
                      g_ns_wdir, global_settings->tr_or_partition,
                      get_sess_name_with_proj_subproj_int(sess_table->sess_name, sess_table->sess_id, "/"));
      }
      NSDL1_SCHEDULE(NULL, NULL, "buf with NS_TA_DIR = %s", buf);  
      if(system(buf))
      {
        NSTL1(NULL, NULL, "Error: runing cmd '%s'", buf);
      }
      //Add - Achint 12/14/2006
      //Check if its url based script the delete it
      /*bug id: 101320: ToDo: TBD with DJA*/
      if(!is_script_url_based(sess_table->sess_name)) {
        sprintf(buf, "rm -r %s/%s  >/dev/null 2>&1 ", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_table->sess_name, -1 , "/"));
        if(system(buf))
          NSTL1_OUT(NULL, NULL, "Can not remove url based script with ta_dir( %s) %s\n", GET_NS_RTA_DIR(),
                                 get_sess_name_with_proj_subproj_int(sess_table->sess_name, -1, "/"));
      }

      //TODO: Sleep should be calculated value based on size of script
      sleep(1); //sleep for 1 seconds
    }
  //}

  //since owner and same group member can remove test run, for that need to make logs/TRxxxx/scripts dir as 775
  //Issue: since we are doing -R 775 that means change files and directories recursively
  //So all file will be as executable that may create problem when user by mistake use as ./<filename>
  //We will fix it later.
  // Discuss with Neeraj
  //Scripts have 775 permission as default directories have
  //remove existing link
  sprintf(buf, "rm -f %s/logs/%s/scripts", g_ns_wdir, global_settings->tr_or_common_files);
  system(buf);

  write_log_file(NS_COPY_SCRIPTS, "Creating script link on common_files from partition");
  //in partition mode make hard link of scripts in common_files/
  //make script directory soft link in common files
  sprintf(buf, "ln -s %s/logs/%s/scripts %s/logs/%s/scripts > /dev/null 2>&1", g_ns_wdir, global_settings->tr_or_partition, g_ns_wdir, global_settings->tr_or_common_files);
  system(buf);

  create_scripts_list();
  end_stage(NS_COPY_SCRIPTS, TIS_FINISHED, NULL);
  TLS_FREE_AND_EXIT(NULL);
}


void 
create_summary_report() {

  NSDL2_SCHEDULE(NULL, NULL, "Method called");  


  // init_test_start_time();
 
  get_version("NetStorm");

  //In case of RESET_TEST_START_TIME_STAMP enable then do not create summary.top file 
  //In case of switching test runs, summary.top file should be created for all test runs  
  //Create in TR dir
  create_report_file(testidx, 0);

  //Create in  partition dir
  create_summary_top(global_settings->tr_or_partition + 2);

  /*if (((get_max_log_level() == 0) || (get_max_log_dest() == 0)) && //logs need no file logging
      ((get_max_tracing_level() == 0) || (get_max_trace_dest() == 0)) && //trace need no file logging
      (get_max_report_level() < 2)) {//report need no file logging
    log_records = 0;
    global_settings->log_postprocessing = 0; //If nothing to log, force post-proceesing to 0
  }*/

  /*PageDump: In case for making G_TRACING kw runtime changeable we need to start
   *          logging process*/
  //if (start_logging(global_settings, testidx, 1) == -1)
   // exit(-1);
  //if (log_records) {
  // In release 3.9.7: Page dump feature has been redesign therefore update slog file 
  // if either reporting or tracing is enable
  if((get_max_report_level() >= 2) || ((get_max_tracing_level() > 0) && (get_max_trace_dest()) > 0))
  {
    if (log_run_profile(testcase_shr_mem) == -1)
      NS_EXIT(-1, "log_run_profile() failed.");
    if (log_user_profile(userIndexTable) == -1)
      NS_EXIT(-1,"log_user_profile() failed." );
    if (log_session_profile(sessprofindex_table_shr_mem) == -1)
      NS_EXIT(-1, "log_session_profile() failed.");
     #if 0
     if (log_tx_table(tx_table_shr_mem) == -1)
       exit(-1);
     if (log_session_table(session_table_shr_mem) == -1)
        exit(-1);
     if (log_page_table(session_table_shr_mem) == -1)
        exit(-1);
     #endif
    //Bug: 59378 - to resolve this bug adding check of USER_SOCKET_PROTOCOL_ENABLED
    if(!(global_settings->protocol_enabled & USER_SOCKET_PROTOCOL_ENABLED))
    {
      if(log_url_table(session_table_shr_mem) == -1)
        NS_EXIT(-1, "log_url_table() failed.");
    }
    if (log_svr_tables(gserver_table_shr_mem) == -1)
      NS_EXIT(-1, "log_url_tables() failed.");
    if (log_error_code() == -1)
      NS_EXIT(-1, "log_error_code() failed.");
  }
  //}
}

int
main( int argc, char** argv )
{
  #ifndef CAV_MAIN
    netstorm_parent(argc, argv);
  #else
    cm_main(argc, argv);
  #endif
  return 0;
}

/*  For Error Cases, we have 3 different error handling levels:
    Level 1:  We close the fd, and we retry the connection (retry_connection()) upto MAX_RETRIES
    Level 2:  We set the req_ok of the connection and call close_connection on the cptr
    Level 3:  We tell the parent that we have failed and we kill ourself.  The parent should then stop the whole test run
*/

#if 0
void
dump_con(VUser *vptr, char *msg, connection* cptr)
{
  connection* next;
  int ii;
  HostSvrEntry *hptr;

  printf("\ncnum=%d cid=%d msg=%s\n",  vptr->cnum_parallel, cptr?cptr - (vptr->first_cptr):-1, msg );

  next = vptr->head_creuse;
  //printf("Global reuse list: ");
  while (next) {
    printf("cptr=0x%x cid=%d fd=%d state=%d started=%u connected=%u\n", (unsigned int)next, (next-(vptr->first_cptr)), next->conn_fd, next->conn_state, next->started_at, next->connect_done_time);
    next = (connection*)next->next_reuse;
  }
  printf("\n");

}
#endif

void inline on_request_write_done ( connection* cptr) {
  u_ns_ts_t now;
  VUser *vptr = cptr->vptr;
  stream *sptr = NULL; // For HTTP2 response timeout  

  NSDL2_CONN(NULL, cptr, "Method called, cptr=[%p]: Request successfully sent", cptr);
  
  now = get_ms_stamp();
  //Calculate write complete time diff
  cptr->write_complete_time = now - cptr->ns_component_start_time_stamp;
  cptr->ns_component_start_time_stamp = now;//Update NS component start time stamp

  cptr->request_sent_timestamp = now;       //Update Request time stamp
  
  if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2){
    sptr = cptr->http2->http2_cur_stream;
    if(sptr){
      sptr->request_sent_timestamp = now;
      NSDL2_CONN(NULL, NULL, "sptr->request_sent_timestamp = %lld", sptr->request_sent_timestamp);
    }
  }
     
  //Reset Idle Timer, when timer is active i.e., timer_type is present
  //if((cptr->request_type == HTTP_REQUEST) || (cptr->request_type == HTTPS_REQUEST))
  //When Idle Timer then only reset
  if((cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE))
    dis_idle_timer_reset( now, cptr->timer_ptr);

  NSDL2_CONN(NULL, cptr, " Request write complete done = %d, Update NS component start time stamp = %u", cptr->write_complete_time, cptr->ns_component_start_time_stamp);
 
  //Storing the request time in cache entry		#Shilpa 22Dec10
  if(NS_IF_CACHING_ENABLE_FOR_USER && cptr->cptr_data != NULL && cptr->cptr_data->cache_data != NULL) 
  {
     //Adding request time in cache entry, used for calculating age of the document 
     ((CacheTable_t *)(cptr->cptr_data->cache_data))->request_ts = 
                         get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt(); 
  }

  /* This has to be reset so we know there was no partial write */
  cptr->bytes_left_to_send = 0;
  NSDL2_CONN(NULL, cptr, "cptr->request_type = %d, cptr->conn_state = %d, global_settings->show_ip_data = %d, total_ip_entries = %d", 
             cptr->request_type, cptr->conn_state, global_settings->show_ip_data, total_ip_entries);
  if(((cptr->request_type == HTTP_REQUEST) || (cptr->request_type == HTTPS_REQUEST)) && 
     ((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && total_ip_entries)) {

    //Bug 43707-Request served form admin/primary IP is getting merged with the requests served from assigned IP Requests Sent/Sec graph
    if(vptr->user_ip->ip_id != -1) {
      NSDL2_CONN(NULL, cptr,"nslb_get_src_addr(cptr->conn_fd), = %s, ", nslb_get_src_addr(cptr->conn_fd));
      int ip_index = group_ip_entries_index_table[(vptr->group_num * total_ip_entries) + vptr->user_ip->ip_id];
      ip_avgtime[ip_index].cur_url_req++;
      NSDL2_CONN(vptr, cptr,"vptr->user_ip->ip_id = %d, ip_avgtime[vptr->user_ip->ip_id].cur_url_req = %d, ip_avgtime = %p",
                 vptr->user_ip->ip_id, ip_avgtime[vptr->user_ip->ip_id].cur_url_req, ip_avgtime);

      NSDL2_CONN(vptr, cptr,"group_ip_entries_index_table: index = %d, ip_index = %d, ", 
                              ((vptr->group_num * total_ip_entries) + vptr->user_ip->ip_id), ip_index);
    }
    else 
      NSDL2_CONN(NULL, cptr,"vptr->user_ip->ip_id = %d, ip_avgtime = %p", vptr->user_ip->ip_id, ip_avgtime);
  }
  switch(cptr->url_num->request_type) {
    case SMTP_REQUEST:
    case SMTPS_REQUEST:
      cptr->conn_state = CNST_READING; /* Since we have to now wait for next response. */
      // Add timer on the basis of smtp stat
      if(cptr->proto_state == ST_SMTP_DATA_BODY)
        delete_smtp_timeout_timer(cptr);
      add_smtp_timeout_timer(cptr, now);
      return;
      break;
    case POP3_REQUEST:
    case SPOP3_REQUEST:
      cptr->conn_state = CNST_READING; /* Since we have to now wait for next response. */
      // Add timer on the basis of pop3 stat
      add_pop3_timeout_timer(cptr, now);
      return;
      break;
    case FTP_REQUEST:
      cptr->conn_state = CNST_READING; /* Since we have to now wait for next response. */
      add_ftp_timeout_timer(cptr, now);
      return;
      break;
    case DNS_REQUEST:
      cptr->proto_state = ST_DNS_HEADER;
      cptr->conn_state = CNST_READING; /* we will be reading now for +ve response from server. */
      return;
      break;
    case LDAP_REQUEST:
    case LDAPS_REQUEST:
      cptr->conn_state = CNST_READING;  // TODO: do we need ro add timeout timer
      return;
      break;
    case IMAP_REQUEST:
    case IMAPS_REQUEST:
      cptr->conn_state = CNST_READING;
    //add_imap_timeout_timer(cptr, now);
      return;
      break;
    case JRMI_REQUEST:
        cptr->conn_state = CNST_READING;
        if(!(cptr->proto_state == ST_JRMI_STATIC_CALL || cptr->proto_state == ST_JRMI_NSTATIC_CALL) && !(cptr->flags & NS_CPTR_JRMI_REG_CON))
          add_jrmi_timeout_timer(cptr, now);
      return;
      break;
    case WS_REQUEST:
    case WSS_REQUEST:
      //cptr->conn_state = CNST_READING;  // TODO: do we need ro add timeout timer
      if(cptr->conn_state == CNST_WS_FRAME_WRITING)
        cptr->conn_state = CNST_WS_IDLE;    
      return;
      break;
    case XMPP_REQUEST:
    case XMPPS_REQUEST:
        cptr->conn_state = CNST_READING;    
      return;
      break;
    case SOCKET_REQUEST:
    case SSL_SOCKET_REQUEST:
      cptr->conn_state = CNST_IDLE;

      INC_OTHER_TYPE_TX_BYTES_COUNTER(vptr, cptr->tcp_bytes_sent);  // Overall througthput

      if (IS_TCP_CLIENT_API_EXIST)
      {
        fill_tcp_client_avg(vptr, SEND_TIME, cptr->write_complete_time);
        fill_tcp_client_avg(vptr, SEND_THROUGHPUT, cptr->tcp_bytes_sent);
      }

      if (IS_UDP_CLIENT_API_EXIST)
      {
        fill_udp_client_avg(vptr, SEND_TIME, cptr->write_complete_time);
        fill_udp_client_avg(vptr, SEND_THROUGHPUT, cptr->tcp_bytes_sent);
      }
      return;
  }

  //Code will here only for HTTP and HTTP2

#ifndef USE_EPOLL
  FD_SET( cptr->conn_fd, &g_rfdset );
#endif
  /* set hdr_state only if  */
  if (!runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining) {
    cptr->header_state = HDST_BOL;
  } else if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && ((cptr->num_pipe == 1) || (cptr->num_pipe == -1)))  {
    cptr->header_state = HDST_BOL;
  }
  
  /* Pipeline only for non-MAIN, HTTP(S) and non-POST */
  if ((cptr->url_num->proto.http.type != MAIN_URL) &&
      runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && 
      (cptr->url_num->request_type == HTTP_REQUEST ||
       cptr->url_num->request_type == HTTPS_REQUEST) &&
      (cptr->url_num->proto.http.http_method != HTTP_METHOD_POST &&
       cptr->url_num->proto.http.http_method != HTTP_METHOD_PUT)) {
    NSDL4_HTTP(NULL, NULL, "trying to pipeline, cptr = %p",
               cptr);
    pipeline_connection((VUser *)cptr->vptr, cptr, now);
  }

  if(SHOW_SERVER_IP)
  {
    action_request_Shr *request = cptr->url_num;
    int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
    PerHostSvrTableEntry_Shr* svr_entry;
    char *host_name = NULL; // Can be www.test1.com:9014 in actual host 
    char host_name_buf[256];
    char *host_name_token = NULL; //only www.test1.com
    int host_name_len;
    int host_name_token_len;

    if (use_rec_host == 0) { //Send actual host (mapped)
      //svr_entry = get_svr_entry(vptr, request->index.svr_ptr);
      svr_entry = cptr->old_svr_entry;
      host_name = svr_entry->server_name;
      host_name_len = svr_entry->server_name_len;
      NSDL4_HTTP(NULL, NULL, "Use recorded host = 0, host_name = %s, host_name_len = %d ", host_name, host_name_len);

    } else { //Send recorded host
      host_name = request->index.svr_ptr->server_hostname;
      host_name_len = request->index.svr_ptr->server_hostname_len;
      NSDL4_HTTP(NULL, NULL, "Use recorded host = 1, host_name = %s, host_name_len = %d ", host_name, host_name_len);
    }

    host_name_token = host_name;
    host_name_token_len = host_name_len;
    //tokenize if port is there
    if(strchr(host_name, ':')){ 
      strcpy(host_name_buf, host_name);
      host_name_token = strtok(host_name_buf, ":"); 
      host_name_token_len = strlen(host_name_token);
    } 
    update_counters_for_this_server((VUser *)cptr->vptr, &(cptr->cur_server), host_name_token, host_name_token_len);
  }
}

static inline int
check_compress(char* buf, int size) {
  int filepipe[2];
  int compressionpipe[2];
  char read_file_fd[8];
  char write_file_fd[8];
  char write_compression_fd[8];
  char comp_buff[16];
  int child_pid;
  int ratio;
  int status;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  if (pipe(filepipe) == -1) {
    NSTL1(NULL, NULL, "check_compress: getting pipe failed\n");
    perror("./netstorm");
    NS_EXIT(-1, "check_compress: getting pipe failed");
  }

  if (pipe(compressionpipe) == -1) {
    NSTL1(NULL, NULL, "check_compress: getting pipe failed\n");
    perror("./netstorm");
    NS_EXIT(-1, "check_compress: getting pipe failed\n");
  }

  child_pid = fork();

  if (child_pid == -1) {
    NSTL1(NULL, NULL, "check_compress: forking child failed\n");
    perror("./netstorm");
    NS_EXIT(-1, "check_compress: forking child failed");
  }

  if (child_pid == 0) {
    sprintf(read_file_fd, "%d", filepipe[0]);
    sprintf(write_file_fd, "%d", filepipe[1]);
    sprintf(write_compression_fd, "%d", compressionpipe[1]);
    execl("./compact", "./compact", "-T", "10", "-i", read_file_fd, "-g", write_file_fd, "-h", write_compression_fd, "-q", "-f", "/dev/null", NULL);
  } else {
    write(filepipe[1], buf, size);
    close(filepipe[1]);
    close(filepipe[0]);
    read(compressionpipe[0], comp_buff, 16);
    close(compressionpipe[0]);
    close(compressionpipe[1]);
    ratio = atoi(comp_buff);
    if (ratio < 0)
      ratio = 0;
    waitpid(child_pid, &status, 0);
  }

  return ratio;
}

#if 0
static void
free_cptr_bufs( connection *cptr ) {
  struct copy_buffer* buffer = cptr->buf_head;
  struct copy_buffer* old_buffer;

  while (buffer) {
    buffer = buffer->next;
    old_buffer = buffer;
    free(old_buffer);
  }

  cptr->buf_head = cptr->cur_buf = NULL;
}
#endif



/* Return 0: if any error or 100 Continue response code   
 * Return 1: else (i.e. normal case of http without expect 100 continue)
 */
static char handle_100_continue(connection *cptr, u_ns_ts_t now, int chk, int req_type) {
  
  NSDL2_HTTP(NULL, cptr, "Method Called, req_code = %d, chk = %d, req_type = %d",
					 cptr->req_code, chk, req_type);

  if(chk) {

    update_http_status_codes(cptr, average_time);
    /*
  
    if(SHOW_GRP_DATA)
    {
      VUser *vptr = cptr->vptr;
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      update_http_status_codes(cptr, lol_average_time);
    }
*/
 
    cptr->conn_state = CNST_WRITING;
    cptr->flags  &= ~NS_HTTP_EXPECT_100_CONTINUE_HDR;  // Unset as we got the 100 Continue now expecting for Response to come
    cptr->req_code_filled = -2;  // Reset as we will expect for next response code
    cptr->content_length = -1;
    cptr->req_code = 0;
    NSDL2_HTTP(NULL, cptr, "We got 100 Continue now sending rest message (Body) and expecting for response");
    if(req_type == HTTP_REQUEST)
      handle_write(cptr, now);
    else{
      handle_ssl_write(cptr, now);
      if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->url_num->proto.http.type == EMBEDDED_URL){
        HTTP2_SET_INLINE_QUEUE
      }
    }
    return 0;
  } else {
    // We must reset is as we do not exepct 100 continue now
    cptr->flags  &= ~NS_HTTP_EXPECT_100_CONTINUE_HDR;  // Unset as we got the 100 Continue now expecting for Response to come
    free_all_vectors(cptr);
    return 1;     
  }
}

inline int
get_req_status (connection *cptr)
{
//int status;
int hcode;
int req_code = cptr->req_code;

	hcode = req_code/100;
  	NSDL2_HTTP(NULL, cptr, "Method called. req_code = %d, hcode = %d", req_code, hcode);
	if (hcode > 5)
        {
  	    NSDL3_HTTP(NULL, cptr, "req_status = NS_REQUEST_5xx");
	    return NS_REQUEST_5xx; //Any code more than 5xx mapped tp 5xx
        }
        //For NTLM Authentication Handshake Failure    
        if(cptr->proto_state == ST_AUTH_HANDSHAKE_FAILURE)
        {
  	  NSDL3_HTTP(NULL, cptr, "Authentication Handshake Failure, req_code = %d", req_code);
          // Must clear flag and state
          if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
            cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_BASIC;
          if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
            cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_DIGEST;
          if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)
            cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_NTLM;
          if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
            cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_KERBEROS;
          if(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)
            cptr->flags = cptr->flags & ~NS_CPTR_FLAGS_CON_PROXY_AUTH;
/*
          if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT) //Resetting Proxy Connect flag in case
                                                                  //handshake failure
          {  
            //In case of proxy connect requests, updating progress/graph counters
            cptr->flags = cptr->flags & ~NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT;
            NSDL2_PROXY(NULL, cptr, "Unsetting NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT");
          }
*/
          cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 
          cptr->proto_state = 0; // Clear state
          return NS_REQUEST_AUTH_FAIL;
        }

        /*We are expecting for 100-continue but got a diffrent response code*/
        if(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) {

  	   NSDL3_HTTP(NULL, cptr, "Expecting for 100 response code, req_code = %d", req_code);
           if(req_code == 100) 
	     return NS_REQUEST_OK;

           // We can get 401 in case of expect 100 continue
           if((req_code == 401) && (cptr->flags & NS_CPTR_AUTH_MASK))
            {
  	     NSDL3_HTTP(NULL, cptr, "Got 401 when expect 100 continue was set and auth is in progress. So discarding the body of request as we do not need to send it");
             // This will free Body vectors which were not sent
             // NOTE -> Since now is not used in case chk is 0, passing now as 0
             handle_100_continue(cptr, 0, 0, cptr->url_num->request_type);
	     return NS_REQUEST_OK;
           }
           
           return NS_REQUEST_BAD_HDR;
        }

        // Bug:36326| when status code is 401 and cptr flag for authentication is not set, then AUTH_FAIL will set
        if ((req_code == 401) && !(cptr->flags & NS_CPTR_AUTH_HDR_RCVD)) {
          cptr->flags &= ~NS_CPTR_AUTH_HDR_RCVD;              //Reset NS_CPTR_AUTH_HDR_RCVD flag here.
          return NS_REQUEST_AUTH_FAIL;
        }

        // Added 401 for TR069 as a temp fix
        // Adding status code 204, as it should also treated as success
      	if ((req_code == 200) || (req_code == 304) || (req_code == 301) || (req_code == 302) || 
                (req_code == 401) || (req_code == 307) || (req_code == 407) || (req_code == 204)
||            (runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.errcode_ok && (hcode > 0) && (hcode < 4) && req_code != 100 ))
        {
  	    NSDL3_HTTP(NULL, cptr, "req_status = NS_REQUEST_OK");
	    return NS_REQUEST_OK;
        }
	else 
        {
  	    NSDL3_HTTP(NULL, cptr, "req_status = %d", hcode+1);
	    return hcode+1; //Assumption: 1xx to 5xx is defined 2 to 6
	}
}



//Parse MEMPERF keyword which have the two possible values 0 or 1
void kw_set_mem_perf(char *buf)
{  
  NSDL2_CONN(NULL, NULL, "Method called. buf = %s", buf);
  sscanf(buf, "%*s %d", &memperf_for_conn);
}

static inline char *get_request_status(int request_status)
{
  switch (request_status) {
    //case NS_REQUEST_OK: return "NS_REQUEST_OK";
  case NS_REQUEST_ERRMISC: return "NS_REQUEST_ERRMISC";
  case NS_REQUEST_1xx: return "1xx";
  case NS_REQUEST_2xx: return "2xx";
  case NS_REQUEST_3xx: return "3xx";
  case NS_REQUEST_4xx: return "4xx";
  case NS_REQUEST_5xx: return "5xx";
  case NS_REQUEST_BADBYTES: return "PartialBody";
  case NS_REQUEST_TIMEOUT: return "T.O";
  case NS_REQ_PROTO_NOT_SUPPORTED_BY_SERVER: return "ProtoNotSupported";

  case NS_REQUEST_CONFAIL: return "ConFail";

  case NS_REQUEST_SSLWRITE_FAIL: return "SSLWriteFail";
  case NS_REQUEST_SSL_HSHAKE_FAIL: return "SSLHshakeFail";

  case NS_REQUEST_CLICKAWAY: return "ClickAway";
  case NS_REQUEST_WRITE_FAIL: return "WriteFail";

  case NS_REQUEST_INCOMPLETE_EXACT: return "IncompleteExact";
  case NS_REQUEST_NO_READ: return "NoRead";
  case NS_REQUEST_BAD_HDR: return "PartialHdr";
  case NS_REQUEST_BAD_BODY_NOSIZE: return "BadBodyNoSize";
  case NS_REQUEST_BAD_BODY_CHUNKED: return "BadBodyChunked";
  case NS_REQUEST_BAD_BODY_CONLEN: return "BadBodyConLen";
  case NS_UNCOMP_FAIL: return "UnCompFail";
  case NS_REQUEST_STOPPED: return "Stopped";
  case NS_REQUEST_SIZE_TOO_SMALL: return "SizeTooSmall";
  case NS_REQUEST_SIZE_TOO_BIG: return "SizeTooBig";
  case NS_REQUEST_URL_FAILURE: return "NS_REQUEST_URL_FAILURE";
  case NS_REQUEST_CV_FAILURE: return "NS_REQUEST_CV_FAILURE";
  case NS_USEONCE_ABORT: return "DataExhausted";
  case NS_UNIQUE_RANGE_ABORT_SESSION : return "RangeExhausted";
  case NS_REDIRECT_EXCEED: return "RedirectLimit";
  case NS_SOCKET_BIND_FAIL : return "BindFail";
  case NS_DNS_LOOKUP_FAIL : return "DNSLookUpFail";
  default:
    return "Unknown";
  }
}

static inline void log_action_err_event(connection *cptr) {

  VUser *vptr = cptr->vptr;
  char srcip[128];
  char cptr_to_str[MAX_LINE_LENGTH + 1];
  /*bug 52092: get request_type direct from cptr, instead of url_num*/
  int request_type = cptr->request_type;

  NSDL2_CONN(NULL, cptr, "Method Called, request_type = %d", request_type);

  strcpy(srcip, nslb_get_src_addr_ex(cptr->conn_fd, 0)); 

  /* We are not using this anywhere so why to copy
  if (global_settings->num_dirs) {
    sprintf(spec_buf, "dir=%d class=%d file=%d", cptr->dir, cptr->class, cptr->file);
  }
  */
  
  /*Copy all useful info of cptr into string*/
  cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
  
  switch(request_type) {
   case HTTP_REQUEST:
   case HTTPS_REQUEST:
     NS_EL_4_ATTR(EID_HTTP_URL_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),  // 1 give port
                                 get_request_string(cptr), 
                                 "Url failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case SMTP_REQUEST:
   case SMTPS_REQUEST:
     NS_EL_4_ATTR(EID_SMTP_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),  // 1 give port
                                 get_request_string(cptr), 
                                 "SMTP request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case POP3_REQUEST:
   case SPOP3_REQUEST:
     NS_EL_4_ATTR(EID_POP3_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),  // 1 give port
                                 get_request_string(cptr), 
                                 "POP3 request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case DNS_REQUEST:
     NS_EL_4_ATTR(EID_DNS_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),  // 1 give port
                                 get_request_string(cptr), 
                                 "DNS request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case FTP_REQUEST:
   case FTP_DATA_REQUEST:
     NS_EL_4_ATTR(EID_FTP_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr), 
                                 "FTP request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case LDAP_REQUEST:
   case LDAPS_REQUEST:
     NS_EL_4_ATTR(EID_FTP_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr), 
                                 "LDAP request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case IMAP_REQUEST:
   case IMAPS_REQUEST:
     NS_EL_4_ATTR(EID_FTP_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr), 
                                 "IMAP request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case WS_REQUEST:
   case WSS_REQUEST:
     NS_EL_4_ATTR(EID_WS_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr), 
                                 "WebSocket Connect request failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
     break;
   case SOCKET_REQUEST:
   case SSL_SOCKET_REQUEST:
     NS_EL_4_ATTR(EID_WS_ERR_START + cptr->req_ok, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name, 
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr), 
                                 "Socket read/write failed with status %s.\nEvent Data:\nsrcip = %s %s",
				 get_error_code_name(cptr->req_ok),
				 srcip, cptr_to_str); 
    default:
        NSDL2_CONN(NULL, cptr, "Invalid request type %d", request_type);
	break;
  }
}

static void update_pipelining_data(connection *cptr) {

  pipeline_page_list *pplist = NULL;
  pipeline_page_list *pplist_prev = NULL;

  NSDL2_CONN(NULL, cptr, "Method Called, num_pipe = %d", cptr->num_pipe);

  if (cptr->num_pipe > 0) {/* can never be == 0; it can be either -1 or > 0 */
    cptr->num_pipe--;

    NSDL4_HTTP(NULL, cptr, "decremented num_pipe = %d", 
               cptr->num_pipe);
    /* in following error cases we are not going to expect any more response
     * on this connections make num_pipe = 0 */
    if (cptr->req_ok >= NS_REQUEST_BADBYTES && cptr->req_ok <= NS_REQUEST_STOPPED) {
      NSDL3_HTTP(NULL, cptr, "setting num_pipe = 0, req_ok = %d", cptr->req_ok);
      /* we have to loose all the URL heads */
      pplist = (pipeline_page_list *)(cptr->data);
        
      if (pplist) {
      /* save the top one. */
        action_request_Shr *top_url_num = pplist->url_num;
        
        /* Detach all others and free */
        while (pplist) {
          pplist_prev = pplist;
          pplist = pplist->next;
          FREE_AND_MAKE_NOT_NULL_EX(pplist_prev, sizeof(pipeline_page_list), "pplist_prev", -1); // Added size of struct 
        }

        cptr->data = NULL;
        cptr->url_num = top_url_num;
      }
      //((VUser *)(cptr->vptr))->urls_awaited -= (cptr->num_pipe);
      //((VUser *)(cptr->vptr))->urls_awaited++;
      /* incrementing once since we are processing atleast one. */
      ((VUser *)(cptr->vptr))->urls_awaited = 
        ((VUser *)(cptr->vptr))->urls_left + 1;
      NSDL3_HTTP(NULL, cptr, "urls_awaited = %d, num_pipe = %d, urls_left = %d", 
                 ((VUser *)(cptr->vptr))->urls_awaited, cptr->num_pipe,
                 ((VUser *)(cptr->vptr))->urls_left);
      cptr->num_pipe = 0;
    }
  } else {            /* POST */
    cptr->num_pipe = 0;
  }
}

inline void
Close_connection( connection* cptr, int chk, u_ns_ts_t now, int req_ok, int completion) {
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(NULL, cptr, "Method Called, chk = %d, now = %u, req_ok = %d, completion = %d", chk, now, req_ok, completion);
  //action_request_Shr *url_num = get_top_url_num(cptr);
  //int request_type = url_num->request_type;
  
  cptr->req_ok = req_ok;
  cptr->completion_code = completion;
  if (req_ok != NS_REQUEST_OK) {
    log_action_err_event(cptr);
  }
  
  if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining) {
    if (cptr->request_type == HTTP_REQUEST || cptr->request_type == HTTPS_REQUEST) {
      /* Update pipelining specific data*/
      update_pipelining_data(cptr);
    }
  }

  NSDL2_CONN(NULL, cptr, "cptr->request_type = %d, cptr->flags = 0X%x, global_settings->protocol_enabled = %x", 
                          cptr->request_type, cptr->flags, global_settings->protocol_enabled);
  //Bug 33900 - WebSocket: Getting Error on console if we are running test with page think time API.
  if((global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) && (cptr->flags & NS_CPTR_DO_NOT_CLOSE_WS_CONN))
  {
    NSDL2_CONN(NULL, cptr, "Switching to VUser context as connection do not close on websocket connect API");
    cptr->flags &= ~NS_CPTR_DO_NOT_CLOSE_WS_CONN;
    NSDL2_CONN(NULL, cptr, "cptr->conn_state = %d", cptr->conn_state);
    if((cptr->conn_state == CNST_WS_READING))
    {
      switch_to_vuser_ctx(vptr, "WebUrlOver");
      return;
    }
  }

  cptr->num_retries = 0;
  switch(cptr->request_type) {
   case HTTP_REQUEST:
   case HTTPS_REQUEST:
       http_close_connection(cptr, chk, now);
     break;
   case WS_REQUEST:
   case WSS_REQUEST:
     if(cptr->flags & NS_WEBSOCKET_UPGRADE_DONE)
       websocket_close_connection(cptr, 1, now, req_ok, completion);
     else
       http_close_connection(cptr, chk, now);
     break;
   case SOCKET_REQUEST:
   case SSL_SOCKET_REQUEST:
     socket_close_connection(cptr, chk, now, req_ok, completion);
     break;
   case SMTP_REQUEST:
   case SMTPS_REQUEST:
   case POP3_REQUEST:
   case SPOP3_REQUEST: //SPOP3 connection
   case DNS_REQUEST:
   case FTP_REQUEST:
   case LDAP_REQUEST:
   case LDAPS_REQUEST:
   case FTP_DATA_REQUEST:
   case USER_SOCKET_REQUEST:
   case IMAP_REQUEST:
   case IMAPS_REQUEST:
   case JRMI_REQUEST:
   case XMPP_REQUEST:
   case XMPPS_REQUEST:
   case FC2_REQUEST:
      non_http_close_connection(cptr, chk, now);
      break;
   default:
      // Error log as its an invalid request_type 
      NSDL2_CONN(NULL, cptr, "Invalid request type. Request type is  %d", cptr->request_type);
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Invalid request type");
      break;
  }
}


static inline void http_xml_parse(connection* cptr) {

  char buf[16];
  htmlDocPtr doc = NULL;
  htmlParserCtxtPtr ctxt = cptr->ctxt;

  NSDL2_CONN(NULL, cptr, "Method called. ctxt = %d", ctxt); 

  {
    if (!htmlParseChunk(ctxt, buf, 0, 1)) {
      // DL_ISSUE
#ifdef NS_DEBUG_ON
      // if (global_settings->debug >= 3)
	xmlParserWarning(cptr->ctxt, "Handle_read parsing error\n");
#endif
    }
    doc = ctxt->myDoc;
    htmlFreeParserCtxt(ctxt);
    if (doc != NULL)
      xmlFreeDoc(doc);
    cptr->ctxt = NULL;
  }
}

inline void http_do_checksum(connection *cptr, action_request_Shr *url_num) {

  NSDL2_CONN(NULL, cptr, "Method called, do_checksum = %d", do_checksum);

  // Anil - What is this checksum stuff?
  {
    if ( ! url_num->proto.http.got_checksum ) {
      url_num->proto.http.checksum = cptr->checksum;
      url_num->proto.http.got_checksum = 1;
    } else {
      if ( cptr->checksum != url_num->proto.http.checksum ) {
        ++total_badchecksums;
      }
    }
  }
}

inline void abort_page_based_on_type(connection *cptr, VUser *vptr, int url_type,
                                     int redirect_flag, int status) {

  NSDL2_CONN(vptr, NULL, "Method called. url_type = %d, status = %d, redirect_flag = %p", 
			  url_type, status, redirect_flag);
  
  switch (url_type) {
    case REDIRECT_URL: // URL will never be REDIRECT_URL if auto redirect is used
    // No need to check NS_REQUEST_3xx as we have added 301/301 in OK list
    case MAIN_URL:
    /* page_status is set in on_page_start to NS_REQUEST_OK
       We need to set it only if MAIN URL or Main URL Redirected TO URL fails
       This is conditionaly done as we have CVFail in Search Var, XML on 
       different redirection depth of Main url. So we need to preserve the CVFail 
       unless there are HTTP level errors on fetching of redirected URL
       For example, Main URL -> R1 -> R2. If Action of not found is continue
        If search var on R1 causes CVFail and
        R2 fetching is succesful, then page status will be CVFail
        However if R2 fetching is Not succesful, then page status will be R2 status
    */
      abort_bad_page(cptr, status, redirect_flag);
      break;
    default:
      if (runprof_table_shr_mem[vptr->group_num].gset.on_eurl_err) {
        if (vptr->page_status == NS_REQUEST_OK)
          vptr->page_status = NS_REQUEST_URL_FAILURE;

        if (vptr->sess_status == 0)
          vptr->sess_status = NS_REQUEST_ERRMISC;
      }
  }
}

inline void reset_cptr_bits (connection *cptr)
{
  cptr->flags &= NS_CPTR_FLAGS_RESET_ON_CLOSE_MASK;
}

//For dynamic host, funct was created to process next url request in erronous embd case
static inline void process_next_http_request(connection* cptr, VUser *vptr, int done, int url_type, u_ns_ts_t now)
{
  NSDL2_CONN(NULL, cptr, "Method called. cptr = %p, vptr = %p, done = %d, url_type = %d", cptr, vptr, done, url_type);

  if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && cptr->num_pipe == 0 && !done && url_type == MAIN_URL) {
    /* First entry into pipeline after MAIN */
    int flg = pipeline_connection(vptr, cptr, now);
    NSDL2_CONN(NULL, cptr, "flg = %d", flg);
    next_connection(vptr, flg?cptr:NULL, now);
  } else if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && cptr->num_pipe > 0){
    /* Here we call next_connection() since in this particular case we will
     * go back to handle_read loop and read more. Here we just try to make
     * parallel conns if possible (which will most likeley not). This also
     * avoids the bug:
     *            ~~~
     * sending more on pipe when last last send was partial will cause corruption
     * on the pipe.
     */
    //next_connection(vptr, NULL, now);  
  } else {
    next_connection(vptr, done?NULL:cptr, now);
  }
  NSDL2_CONN(NULL, cptr, "Exiting method");
}

void dns_http_close_connection(connection* cptr, int chk, u_ns_ts_t now, int req_ok, int completion){ //TODO: set cptr->req_ok as failure
  int done = NS_FD_CLOSE_AS_PER_RESP;
  VUser *vptr;
  int url_ok = 0;
  int status =0;
  int url_type;
  int redirect_flag = 0;
  char *redirected_url = NULL; // We will set it only for HTTP(S)
  int type;
  int request_type;
  char taken_from_cache = 0; // No 
  action_request_Shr *url_num;
  int reload_page;
  int url_id;

  const int con_num = cptr->conn_fd;
  ns_8B_t flow_path_instance = cptr->nd_fp_instance;
  vptr = cptr->vptr;
  NSDL2_CONN(NULL, cptr, "Method called. chk = %d, cptr = %p, urls_awaited = %d",
			  chk, cptr, vptr->urls_awaited);

  //TODO: if caching is enabled and we are here with dns failure, how to handle this case, need to close with caching flow       
  cptr->req_ok = req_ok;
  cptr->completion_code = completion;
  if (req_ok != NS_REQUEST_OK) {
    log_action_err_event(cptr);
  }   

  if (!(cptr->data)) {
    NSDL2_CONN(vptr, cptr, "Returning url_num from cptr");
    url_num =  cptr->url_num;
  }else{
    NSDL2_CONN(vptr, cptr, "Returning url_num from cptr->data");
    url_num =  ((pipeline_page_list *)(cptr->data))->url_num;
  }

  if(cptr->completion_code == NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE)  {
    /* There is two possibility here: 
     *      o   Connection was Reused 
     *      	o	So we need not to close_fd as it will be reused	
     *      o   Got new cptr but did not made the connection
     *      	o	So we have to save cptr into free connection slots
     *      		but we did not make connection so we can not close
     *      		connection.
     */ 
    if(cptr->conn_state == CNST_REUSE_CON){
       NSDL2_CONN(vptr, cptr, "Connection was Reused hence making done to 0");
       done = NS_DO_NOT_CLOSE_FD;
    }
    taken_from_cache = 1; 
  }
  
  url_type = url_num->proto.http.type;
  request_type = url_num->request_type;
  //Retrying connections may not not produce connection and would close
  //assert (cptr->conn_state != CNST_FREE);

  /* we have just read the 100-continue header*/
  if(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) {
     if(handle_100_continue(cptr, now, chk, request_type) == 0) {
       /*return as we have done with the body write*/
       return;
     } /*Else we are expecting 100-continue & we got something else*/
  }
  //TODO: What if response is not OK ???
  //For status we need to move status code before PROXY_CONNECT bit check
  status = cptr->req_ok;
  url_ok = !status;

  /* In case of HTTPs request if proxy bit is set, 
   * then read response from proxy and make ssl connection
   * Do not make ssl connection in case proxy authentication handshake is in progress */
  //Here we dont need to check for PROXY_ENABLED
  NSDL3_CONN(vptr, cptr, "status = %d, cptr->flags = %x", status, cptr->flags);

  if((cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT) &&
        !(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)) {
    if((status == NS_REQUEST_OK))
    { 
      //Must reset here as we are calling handle_connect() and in handle_connect()
      //we are checking for NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT() if bit is set then
      //we are calling proxy_make_and_send_connect () method
    
      UPDATE_CONNECT_SUCCESS_COUNTERS(vptr);
      cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT;
      NSDL2_PROXY(NULL, cptr, "Unsetting NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT");
      handle_proxy_connect(cptr, now);
      return;
    }
    else
    {
      NSDL3_CONN(vptr, cptr, "NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT Bit is still set.");
      //In case of proxy connect requests, updating progress/graph counters
      //Since we are in auth, we dont know whether its CONNECT success or failure,
      //hence incrementing CONNECT failure counters only after auth is complete
        //Updating the proxy failures graph counters.
      UPDATE_CONNECT_FAILURE_COUNTERS(vptr);
    }
  }

  NSDL3_CONN(vptr, cptr, "After handle proxy connect.");

  if(cptr->ctxt) http_xml_parse(cptr);

  /* page reload has higher priority than click away so it has to be called first */
  // If page has to be reloaded then close fd, so that new connection could be made
  // Cache - Review if this will have any impact
  if(runprof_table_shr_mem[vptr->group_num].page_reload_table) {
    is_reload_page(vptr, cptr, &reload_page);
    if(reload_page) {
      NSDL3_CONN(vptr, cptr, "Page is set to reload.");
      done = NS_FD_CLOSE_AS_PER_RESP;
    }
  }

   
  /*this function setup auto redirection by checking location_url, 
    redirect_count, g_follow_redirects & fills redirect_flag
  */
  http_setup_auto_redirection(cptr, &status, &redirect_flag);
  /* save redirected host and port if the user wants to change the recorded
   * server to this one for a certain page and thereafter, at this
   * redirection depth - save is done conditionally inside.
   */

#ifdef NS_DEBUG_ON
  char request_buf[MAX_LINE_LENGTH];
  int request_buf_len;
#endif
  NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed fetching of page %s URL(%s) on fd = %d. Request line is %s",
          url_num->proto.http.type == MAIN_URL ? "main" : "inline",
          get_req_type_by_name(url_num->request_type),cptr->conn_fd,
          get_url_req_line(cptr, request_buf, &request_buf_len, MAX_LINE_LENGTH));

  //free kernel memory for each KEEP ALIVE socket connection
  if (memperf_for_conn && !done)
  {
    NSDL3_CONN(vptr, cptr, "Freeing socket memory");
    free_sock_mem(cptr->conn_fd);
  }


  if(cptr->conn_state == CNST_REUSE_CON) {
    NSDL2_SOCKETS(vptr, cptr, "cptr conn state is CNST_REUSE_CON returning");
    vptr->last_cptr = cptr;
    return;
  } else {
    NSDL2_SOCKETS(vptr, cptr, "Setting connection state to CNST_FREE");
    cptr->conn_state = CNST_FREE;
    cptr->conn_fd = -1;
    update_parallel_connection_counters(cptr, now);
    vptr->last_cptr = NULL;
  }
  //cache_close_fd (cptr, now);   // This must be called if response is from cache

  if(status != NS_REQUEST_OK) {
    // This handles bad page if URL fails (4xx etc)
    abort_page_based_on_type(cptr, vptr, url_type, redirect_flag, status);
    //redirect_flag  will be 0 always as status != NS_REQUEST_OK -- see http_setup_auto_redirection
    //redirect_flag &= ~NS_HTTP_REDIRECT_URL;  
  }

  NSDL2_CONN(vptr, cptr, "vptr=%p status=%d, CONNECT bit=0X%x",vptr, status, cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT);

  handle_url_complete(cptr, request_type, now, url_ok, redirect_flag, status, taken_from_cache);


  if(!taken_from_cache && LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  {
    NSDL4_HTTP(vptr, cptr, "Call log_url_record function");
    if((global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED) && (cptr->url_num->proto.http.url_index == -1))
      url_id = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)cptr->url,
                                  cptr->url_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    else
      url_id = cptr->url_num->proto.http.url_index;

    if (log_url_record(vptr, cptr, status, cptr->request_complete_time, redirect_flag, con_num, flow_path_instance, url_id) == -1)
      NSTL1_OUT(NULL, NULL, "Error in logging the url record\n");
  }

  // Must be called after log_url_record as we are  using flags in that method
  reset_cptr_bits(cptr);

  cptr->req_ok = NS_REQUEST_OK;

  type = url_num->proto.http.type;

  /* cptr->url is freed in 3 cases/places
     1. Here if its not a redirected url or any url failure.
     2. a. In auto redirect connection as we need cptr->url there thats why we did not freed in case: 1
        b. If page failed in do_data_process in case of auto_redirect then in that case we will 
           not go to auto redirect connection so we need to free it there
  */ 
  NSDL2_CONN(vptr, cptr, "Going to free cptr");
  if (!(redirect_flag & NS_HTTP_REDIRECT_URL) || !vptr->urls_awaited) {
    FREE_CPTR_URL(cptr);
  }

  if (!vptr->urls_awaited) {
    handle_page_complete(cptr, vptr, done, now, request_type);
   /*redirect_flag should be updated if page_status fail and action is
    stop then abort the page and set the redirect_flag by zero fo the case of
    M-->R1-->R2-->R3, in this case redirect_flag=3, if the CVFail is
    on R2 then, FREE_REDIRECT_URL() called two time, One time from handle_page_complete()
    and one time [due to redirect_flag is on] from the end of close_connection()
    and hence this causes the double free and gives core dump. Therfore we are avoiding
    this by updating redirect_flag  by zero. 
  */
    redirect_flag &= ~NS_HTTP_REDIRECT_URL;  
  } else {
    
    NSDL4_CONN(vptr, cptr, "url_type = %d, redirect_flag = 0x%x, global_settings->g_follow_redirects = %d, redirect_url = %s",
           url_type, redirect_flag, global_settings->g_follow_redirects, cptr->location_url);

    // Auto Redirection Case
    if (redirect_flag & NS_HTTP_REDIRECT_URL) {
      int ret = auto_redirect_connection(cptr, vptr, now, done, redirect_flag);
      if (ret == ERR_MAIN_URL_ABORT) {
        abort_bad_page(cptr, NS_REQUEST_ERRMISC, redirect_flag);
        handle_page_complete(cptr, vptr, done, now, request_type);
      } else if (ret == ERR_EMBD_URL_ABORT) {
        // Since we are not doing redirection due to error, we need to reduce urls_awaited
        vptr->urls_awaited--;
        if (!vptr->urls_awaited)
          handle_page_complete(cptr, vptr, done, now, request_type);
        else
          process_next_http_request(cptr, vptr, done, url_type, now);  
      } 

    } else if (url_type == REDIRECT_URL) {
        // Manual redirection case
    	redirect_connection(vptr, done?NULL:cptr, now, url_num);
    } else {
        // Next Url case
        process_next_http_request(cptr, vptr, done, url_type, now);
    }
  } 

  // Free redirect URL for all non Main URLs and Main URL which is redirected
  // as we need this final URL of Main URL redirection chain in do_data_processing() which is stored in vptr by copy_cptr_to_vptr()
  // Final Main URL of redirection chain is freed in handle_page_compelete()
  if ((type != MAIN_URL || (type == MAIN_URL && (redirect_flag & NS_HTTP_REDIRECT_URL)))) {
    FREE_REDIRECT_URL(cptr, redirected_url, url_num);
  } else {
    NSDL3_HTTP(vptr, cptr, "Not calling FREE_REDIRECT_URL, url is main url and is final url");
  }
  if (cptr->conn_state == CNST_FREE) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
    }
  }
}


/*bug 68963:  added debug_log_connection_failure()*/
/*************************************************************************************/
/* Name         : debug_log_connection_failure
*  Description  : It logs connection error in request file, on the basis of Protocol type
*  Arguments    : cptr
*  Result       : NA
*  Return       : NA
*/
/**************************************************************************************/
void debug_log_connection_failure(connection* cptr)
{
  NSDL1_CONN(NULL, cptr, "Method called.cptr->http_protocol=%d",cptr->http_protocol);
  char* buffer = ns_nvm_scratch_buf;
  memset(buffer, '\0', ns_nvm_scratch_buf_size);
  sprintf(buffer,"Erorr in making connection to host[%s] port[%d], error =%d for Connection Failed", cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_port, cptr->req_ok);
  switch(cptr->http_protocol)
  {
    case HTTP_MODE_AUTO:
    case HTTP_MODE_HTTP1:
     debug_log_http_req(cptr, buffer, strlen(buffer), 0, 0);
     debug_log_http_res(cptr, buffer, 0);
    break;

    case HTTP_MODE_HTTP2:
     NS_RESET_IOVEC(g_scratch_io_vector);
     NS_FILL_IOVEC(g_scratch_io_vector, buffer, strlen(buffer));
     debug_log_http2_dump_req(cptr, g_scratch_io_vector.vector, 0, NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector));
     debug_log_http2_res(cptr, (unsigned char *)buffer, 0);
    break;
  }
} 
/* In case of entry taken from cache (not a validation case) we pass chk 0,
 * so num_ka will not be decremented*/
void
//http_close_connection( connection* cptr, action_request_Shr *url_num, int chk, u_ns_ts_t now) {
http_close_connection( connection* cptr, int chk, u_ns_ts_t now) {

  char buf[16];
  int done = NS_FD_CLOSE_AS_PER_RESP;
  VUser *vptr;
  int url_ok = 0;
  int status =0;
  int url_type;
  int redirect_flag = 0;
  char *redirected_url = NULL; // We will set it only for HTTP(S)
  int type;
  int request_type;
  char taken_from_cache = 0; // No 
  action_request_Shr *url_num;
  int reload_page;
  int url_id;

  /*Atul: HLS is consider iff in response either Header proc_hls_hdr_content_type_mpegurl or proc_hls_hdr_content_type_media found */
  long long int hls_flag = cptr->flags & (NS_CPTR_CONTENT_TYPE_MPEGURL | NS_CPTR_CONTENT_TYPE_MEDIA);

   /* 3.9.0 Changes:
    * Before connection pool was implemented, we were using cptr index as con_num to report in drill down
    * Now we do not have cptr index as we are using pool. So we will use cptr->conn_fd
    * Need to preserve connection fd before calling close_fd()
    */
  const int con_num = cptr->conn_fd;
  ns_8B_t flow_path_instance = cptr->nd_fp_instance;
  vptr = cptr->vptr;
  NSDL2_CONN(NULL, cptr, "Method called. chk = %d, cptr = %p, urls_awaited = %d, hls_flag = %0x",
			  chk, cptr, vptr->urls_awaited, hls_flag);

  if (!(cptr->data)) {
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr");
    url_num =  cptr->url_num;
  }else{
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr->data");
    url_num =  ((pipeline_page_list *)(cptr->data))->url_num;
  }

  // Cache is coming fresh without making a connection
  if(cptr->completion_code == NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE)  {
    /* There is two possibility here: 
     *      o   Connection was Reused 
     *      	o	So we need not to close_fd as it will be reused	
     *      o   Got new cptr but did not made the connection
     *      	o	So we have to save cptr into free connection slots
     *      		but we did not make connection so we can not close
     *      		connection.
     */ 
    if(cptr->conn_state == CNST_REUSE_CON){
       NSDL2_CONN(NULL, cptr, "Connection was Reused hence making done to 0");
       done = NS_DO_NOT_CLOSE_FD;
    }
    taken_from_cache = 1; 
  }
  
  url_type = url_num->proto.http.type;
  request_type = url_num->request_type;
  //Retrying connections may not not produce connection and would close
  //assert (cptr->conn_state != CNST_FREE);

  /* we have just read the 100-continue header*/
  if(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) {
     if(handle_100_continue(cptr, now, chk, request_type) == 0) {
       /*return as we have done with the body write*/
       return;
     } /*Else we are expecting 100-continue & we got something else*/
  }
  /* Connection upgraded from http to http2
   *  We will proceed with http2 in case of NS_REQUEST_OK only. */
  if ((cptr->flags & NS_HTTP2_UPGRADE_DONE) && (cptr->req_ok == NS_REQUEST_OK)) {
    if (handle_http2_upgraded_connection(cptr, now, chk, request_type) == 0) {
      return;
    }
  }
  //TODO: What if response is not OK ???
  //For status we need to move status code before PROXY_CONNECT bit check
  status = cptr->req_ok;
 
 /*bug 68963 : log Error in case of ConFail*/
  if(NS_REQUEST_CONFAIL == cptr->req_ok) {
     debug_log_connection_failure(cptr);
  }

  // Bugid = 45391, fill cptr->req_code to http_resp_code. http_resp_code variable is used by ns_url_get_http_status_code API 
  if(url_num->proto.http.type == MAIN_URL)
    http_resp_code = cptr->req_code;

  /* In case of HTTPs request if proxy bit is set, 
   * then read response from proxy and make ssl connection
   * Do not make ssl connection in case proxy authentication handshake is in progress */
  //Here we dont need to check for PROXY_ENABLED
  NSDL3_CONN(vptr, cptr, "status = %d, cptr->flags = %x", status, cptr->flags);

  if((cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT) &&
        !(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)) {
    if((status == NS_REQUEST_OK))
    { 
      //Must reset here as we are calling handle_connect() and in handle_connect()
      //we are checking for NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT() if bit is set then
      //we are calling proxy_make_and_send_connect () method
    
      UPDATE_CONNECT_SUCCESS_COUNTERS(vptr);
      cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT;
      NSDL2_PROXY(NULL, cptr, "Unsetting NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT");
      handle_proxy_connect(cptr, now);
      return;
    }
    else
    {
      NSDL3_CONN(vptr, cptr, "NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT Bit is still set.");
      //In case of proxy connect requests, updating progress/graph counters
      //Since we are in auth, we dont know whether its CONNECT success or failure,
      //hence incrementing CONNECT failure counters only after auth is complete
        //Updating the proxy failures graph counters.
      UPDATE_CONNECT_FAILURE_COUNTERS(vptr);
    }
  }

  NSDL3_CONN(vptr, cptr, "After handle proxy connect.");

  if(cptr->ctxt) http_xml_parse(cptr);

  // Since HTTP Authentication need to hanppen on same connection, NS should not close connection if
  // HTTP authentication is in progress
  // In case of http2, connection need not to be closed for success cases as connection is keep alive
  if (((chk == 1) && (cptr->flags & NS_CPTR_AUTH_MASK)) || (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && (cptr->num_pipe > 0)) ||((chk) && (cptr->http_protocol == HTTP_MODE_HTTP2))) {
    /* Pipelining is enabled and there is more requests to be sent on the 
     * pipe. "> 0". Here we are by-passing KA_PCT/NUM_KA */
    NSDL3_CONN(NULL, NULL, " here done = NS_DO_NOT_CLOSE_FD");
    done = NS_DO_NOT_CLOSE_FD;
  } else if (chk && (cptr->connection_type == KA) && (--cptr->num_ka)) {
      if(global_settings->high_perf_mode == 0)  { // Anil - Do we need to do this? 
        int ret = 0;
       /* if (cptr->http_protocol == HTTP_MODE_HTTP2){
          NSDL3_CONN(NULL, NULL, " here done = NS_DO_NOT_CLOSE_FD In case of HTTP2");
          done = NS_DO_NOT_CLOSE_FD;
        } else {*/
          ret = read( cptr->conn_fd, buf, 16 );
          if (ret < 0)
          {
            if(errno == EAGAIN)
            {
             done = NS_DO_NOT_CLOSE_FD;
             NSDL3_CONN(vptr, cptr, "Got EGAIN on connection. Setting done to %d", done);
            }
            else
              NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, 
               (char*)__FUNCTION__,
               "Got error other than EGAIN on connection. Error = %s", nslb_strerror(errno));
          }
          else if(ret == 0 )
          {
            NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, 
            (char*)__FUNCTION__,
                     "Connection close from server.");
          }
          else
          {
            NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, 
            (char*)__FUNCTION__,
                     "Got message on the socket. Message is [%*.*s]. Length = %d", ret, ret, buf, ret);
            if (cptr->http_protocol == HTTP_MODE_HTTP2) {
              http2_copy_data_to_buf(cptr, (unsigned char *)buf, ret);
              done = NS_DO_NOT_CLOSE_FD;
            }
          } 
       // }
      }
      else {
        done = NS_DO_NOT_CLOSE_FD;
     }
  }

  NSDL3_CONN(vptr, cptr, "done=%d chk=%d, KA=%d, num_ka=%d, num_pipe = %d, "
  			 "urls_awaited before decrementing status = %d, "
			 "urls_awaited = %d, req_ok = %d, req_code = %d", 
			 done, chk, (cptr->connection_type == KA)?1:0,
			 cptr->num_ka, cptr->num_pipe,
              		 status, vptr->urls_awaited,  cptr->req_ok, cptr->req_code);

  /* page reload has higher priority than click away so it has to be called first */
  // If page has to be reloaded then close fd, so that new connection could be made
  // Cache - Review if this will have any impact
  if(runprof_table_shr_mem[vptr->group_num].page_reload_table) {
    is_reload_page(vptr, cptr, &reload_page);
    if(reload_page) {
      NSDL3_CONN(vptr, cptr, "Page is set to reload.");
      done = NS_FD_CLOSE_AS_PER_RESP;
    }
  }

   
  /*this function setup auto redirection by checking location_url, 
    redirect_count, g_follow_redirects & fills redirect_flag
  */
  http_setup_auto_redirection(cptr, &status, &redirect_flag);
  url_ok = !status;
  
  
  if ((redirect_flag & NS_HTTP_REDIRECT_URL))
  {
    cptr->flags &= ~NS_CPTR_FINAL_RESP;
  }
  else
  {
    cptr->flags |= NS_CPTR_FINAL_RESP;
  }
    
  /* save redirected host and port if the user wants to change the recorded
   * server to this one for a certain page and thereafter, at this
   * redirection depth - save is done conditionally inside.
   */

  // TODO: Cache - Review if this will have any impact as we may not make connection
  // if URL is coming from cache. Currently, we make connection (if not already done)
  // and then check if URL is in cache or not
  if(vptr->svr_map_change) save_current_url(cptr);
 
#ifdef NS_DEBUG_ON
  char request_buf[MAX_LINE_LENGTH];
  int request_buf_len;
#endif 
  NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed fetching of page %s URL(%s) on fd = %d. Request line is %s",
          url_num->proto.http.type == MAIN_URL ? "main" : "inline",
          get_req_type_by_name(url_num->request_type),cptr->conn_fd,
          get_url_req_line(cptr, request_buf, &request_buf_len, MAX_LINE_LENGTH));

  //free kernel memory for each KEEP ALIVE socket connection
  if (memperf_for_conn && !done)
  {
    NSDL3_CONN(vptr, cptr, "Freeing socket memory");
    free_sock_mem(cptr->conn_fd);
  }


  if(!taken_from_cache)
    close_fd (cptr, done, now);
  else
    cache_close_fd (cptr, now);   // This must be called if response is from cache

  NSDL2_CONN(NULL, cptr, "vptr=%p status=%d, CONNECT bit=0X%x",vptr, status, vptr->flags);
  if(status != NS_REQUEST_OK) {
    // This handles bad page if URL fails (4xx etc)
    abort_page_based_on_type(cptr, vptr, url_type, redirect_flag, status);
    //redirect_flag  will be 0 always as status != NS_REQUEST_OK -- see http_setup_auto_redirection
    //redirect_flag &= ~NS_HTTP_REDIRECT_URL;  
  }

  NSDL2_CONN(NULL, cptr, "vptr=%p status=%d, CONNECT bit=0X%x",vptr, status, cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT);

  handle_url_complete(cptr, request_type, now, url_ok, redirect_flag, status, taken_from_cache);

  NSDL2_CONN(NULL, cptr, "cptr->req_ok = %d", cptr->req_ok);
  
  // excluding the failed url statistics. check for macro NS_EXCLUDE_STOPPED_STATS_FROM_URL
  // If exclude_stopped_stats is on and page status is stopped, hence excluding the stopped stats from page dump, hits, drilldown database, response time & tracing 
  if(!taken_from_cache && LOG_LEVEL_FOR_DRILL_DOWN_REPORT && !NS_EXCLUDE_STOPPED_STATS_FROM_URL)
  {
    NSDL4_HTTP(vptr, cptr, "Call log_url_record function");
    if((global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED) && (cptr->url_num->proto.http.url_index == -1))
      url_id = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)cptr->url,
                                  cptr->url_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    else
      url_id = cptr->url_num->proto.http.url_index;

    if (log_url_record(vptr, cptr, status, cptr->request_complete_time, redirect_flag, con_num, flow_path_instance, url_id) == -1)
      NSTL1_OUT(NULL, NULL, "Error in logging the url record\n");
  }

//  if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM || cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST || cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
  if (cptr->flags & NS_CPTR_AUTH_MASK) 
  {
    // We need to free cptr url as in case of cahing enabled, this was not getting freed
    // This need to be freed before next request is sent
    // we need are not freeing cptr for replau mode main url as we take url in sending autherizaion header from cptr
    NSDL2_CONN(NULL, cptr, "Going to free cptr");
    //Bug :65096| In case of Authentication, Parameterized URL need not to be freed
    if(!(cptr->url_num->is_url_parameterized) && ((global_settings->replay_mode == 0) || (cptr->url_num->proto.http.type != MAIN_URL)))
      FREE_CPTR_URL(cptr);
    int ret = auth_handle_response(cptr, status, now);
    if(!ret) // request sent, so return
    {
      return;
    }
    // In case request is not sent, we need to fall thru
  }

  // Must be called after log_url_record as we are  using flags in that method
  reset_cptr_bits(cptr);

  /* Initialization for Auto fetching embedded URLs */
  int get_no_inlined_obj = runprof_table_shr_mem[vptr->group_num].gset.get_no_inlined_obj;
  /* Call populate_auto_fetch_embedded_urls() only if 
       1. embd  urls are not set by API c ns_set_embd_objects() 
       2. Main url response is OK (HTTP Ok and no CVFail etc)
  */
  AutoFetchTableEntry_Shr *ptr = NULL;
  ptr = runprof_table_shr_mem[vptr->group_num].auto_fetch_table[vptr->cur_page->page_number]; 
  
  //API is set but only APIs URLs will be fetached
  if ((vptr->flags & NS_EMBD_OBJS_SET_BY_API) && (!(vptr->flags & NS_BOTH_AUTO_AND_API_EMBD_OBJS_NEED_TO_GO)) 
        && (status == NS_REQUEST_OK)) 
  {
    int i;
    NSDL3_CONN(vptr, cptr, "Embd urls are set by API, going to add urls in main");
    set_embd_objects(cptr, g_num_eurls_set_by_api, eurls_prop);
    for(i = 0; i < g_num_eurls_set_by_api; i++)
    {
      FREE_AND_MAKE_NULL_EX(eurls_prop[i].embd_url, strlen(eurls_prop[i].embd_url), "eurls_prop[i].embd_url", i);
    }
    FREE_AND_MAKE_NOT_NULL_EX(eurls_prop, sizeof(eurls_prop), "EmbdUrlsProp", -1);
    vptr->flags &= ~NS_EMBD_OBJS_SET_BY_API;
  }
  else if ((status == NS_REQUEST_OK)) 
  { 
    //Both API and Autofeteched URLs will be featched if autofetch is ON
    /* If main url is m3u8 url OR INLINE url fetching is enable 
       and if autofetch is enable or it is m3u8 url response then populate embaded urls ,
       m3u8 url -> A url is called m3u8 url if and only if in its response content type is "mpegurl" */
    if(!get_no_inlined_obj || hls_flag) // InLine to be fetched 
    {
      int pattern_table_index = runprof_table_shr_mem[vptr->group_num].gset.pattern_table_start_idx;
      NSDL3_CONN(vptr, cptr, "hls_flag = 0x%x, cptr->url = %s" , hls_flag, cptr->url);

      if(((url_num->proto.http.type == MAIN_URL) && !(redirect_flag & NS_HTTP_REDIRECT_URL)) 
                                               || (hls_flag & NS_CPTR_CONTENT_TYPE_MPEGURL)) // Final Main URL response
      {
        if(ptr->auto_fetch_embedded || hls_flag) // Auto fetch is enabled or hls is enabled
        {
          NSDL3_CONN(vptr, cptr, "Auto fetch value = %d",ptr->auto_fetch_embedded);
          populate_auto_fetch_embedded_urls(vptr, cptr, pattern_table_index, hls_flag);
        }
        else if (global_settings->replay_mode == REPLAY_USING_ACCESS_LOGS) // InLine to be fetched for replay as per log
        {
          NSDL3_CONN(vptr, cptr, "Auto fetch value = %d",ptr->auto_fetch_embedded);
          populate_replay_embedded_urls(vptr, cptr);
        }
        if(vptr->flags & NS_VPTR_FLAGS_INLINE_DELAY) 
          set_inline_schedule_time(vptr, now);
      } 
    }

    // Added for deciding whether or not to get embedded objects based on the
    // keyword (get_no_inlined_obj) on a per group basis, at run time
    NSDL4_HTTP(vptr, cptr, "get_no_inlined_obj %d grp %d redirect_flag 0x%x",
     		         get_no_inlined_obj, vptr->group_num, redirect_flag);
    if (get_no_inlined_obj && !hls_flag ) {
       if (url_num->proto.http.type == MAIN_URL && !(redirect_flag & NS_HTTP_REDIRECT_URL)) {
          NSDL4_HTTP(vptr, cptr, "Get no inlined object is true for group %d,"
               		         " so skipping all embedded objects",vptr->group_num);
          vptr->urls_awaited = vptr->urls_left = 0;
       } 
    }
  }

  cptr->req_ok = NS_REQUEST_OK;

  if(do_checksum) http_do_checksum(cptr, url_num); 

  redirected_url = url_num->proto.http.redirected_url;
  type = url_num->proto.http.type;

  /* Loose the head of pipelined URLs */
  if (cptr->data) {
    pipeline_page_list *pplist_remove;
    pplist_remove = (pipeline_page_list *)(cptr->data);
    cptr->data = (void *)((pipeline_page_list *)(cptr->data))->next;
    
    FREE_AND_MAKE_NOT_NULL_EX(pplist_remove, sizeof(pipeline_page_list), "pplist_remove", -1);//added size of struct
  }

  /* Free cptr url in every case except redirection and enable pipelining(Bug 48032) as we need cptr->url there*/
  /* cptr->url is freed in 3 cases/places
     1. Here if its not a redirected url or any url failure.
     2. a. In auto redirect connection as we need cptr->url there thats why we did not freed in case: 1
        b. If page failed in do_data_process in case of auto_redirect then in that case we will 
           not go to auto redirect connection so we need to free it there
  */ 
  if (!(redirect_flag & NS_HTTP_REDIRECT_URL || runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining) || !vptr->urls_awaited) {
    NSDL2_CONN(NULL, cptr, "Going to free cptr = %p, redirect_flag = %0x, vptr->urls_awaited = %d", cptr, redirect_flag, vptr->urls_awaited);
    FREE_CPTR_URL(cptr);
  }

   
  if (!vptr->urls_awaited) {
    handle_page_complete(cptr, vptr, done, now, request_type);
   /*redirect_flag should be updated if page_status fail and action is
    stop then abort the page and set the redirect_flag by zero fo the case of
    M-->R1-->R2-->R3, in this case redirect_flag=3, if the CVFail is
    on R2 then, FREE_REDIRECT_URL() called two time, One time from handle_page_complete()
    and one time [due to redirect_flag is on] from the end of close_connection()
    and hence this causes the double free and gives core dump. Therfore we are avoiding
    this by updating redirect_flag  by zero. 
  */

    redirect_flag &= ~NS_HTTP_REDIRECT_URL;  
  } else {
    
    NSDL4_CONN(vptr, cptr, "url_type = %d, redirect_flag = 0x%x, global_settings->g_follow_redirects = %d, redirect_url = %s",
           url_type, redirect_flag, global_settings->g_follow_redirects, cptr->location_url);

    // Auto Redirection Case
    if (redirect_flag & NS_HTTP_REDIRECT_URL) {
      int ret = auto_redirect_connection(cptr, vptr, now, done, redirect_flag);
      if (ret == ERR_MAIN_URL_ABORT) {
        abort_bad_page(cptr, NS_REQUEST_ERRMISC, redirect_flag);
        handle_page_complete(cptr, vptr, done, now, request_type);
      } else if (ret == ERR_EMBD_URL_ABORT) {
        // Since we are not doing redirection due to error, we need to reduce urls_awaited
        vptr->urls_awaited--;
        if (!vptr->urls_awaited)
          handle_page_complete(cptr, vptr, done, now, request_type);
        else
          process_next_http_request(cptr, vptr, done, url_type, now);  
      } 

    } else if (url_type == REDIRECT_URL) {
        // Manual redirection case
    	redirect_connection(vptr, done?NULL:cptr, now, url_num);
    } else {
        // Next Url case
        process_next_http_request(cptr, vptr, done, url_type, now);
    }
  }
  // Free redirect URL for all non Main URLs and Main URL which is redirected
  // as we need this final URL of Main URL redirection chain in do_data_processing() which is stored in vptr by copy_cptr_to_vptr()
  // Final Main URL of redirection chain is freed in handle_page_compelete()
  
  if (( ((type == REDIRECT_URL) || (type == EMBEDDED_URL)) && !hls_flag) || (type == MAIN_URL && (redirect_flag & NS_HTTP_REDIRECT_URL))) {  
    FREE_REDIRECT_URL(cptr, redirected_url, url_num);
  } else {
    NSDL3_HTTP(vptr, cptr, "Not calling FREE_REDIRECT_URL, url is main url and is final url");
  }
  if (cptr->conn_state == CNST_FREE) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
    }
  }
}

static void non_http_close_connection( connection* cptr, int chk, u_ns_ts_t now) {
  VUser *vptr;
  int url_ok = 0;
  int status;
  //int url_type;
  int redirect_flag = 0;
  int request_type;
  char taken_from_cache = 0; // No

  NSDL2_CONN(NULL, cptr, "Method called. chk = %d, cptr = %p, now = %llu", chk, cptr, now);

  //action_request_Shr *url_num = get_top_url_num(cptr);
  vptr = cptr->vptr;
  //url_type = url_num->proto.http.type;
  request_type = cptr->request_type;
  
  if (!cptr->request_complete_time) {
    cptr->request_complete_time = now;
    if(cptr->bytes > 0) {
      SET_MIN (average_time->url_dwnld_min_time, cptr->request_complete_time);
      SET_MAX (average_time->url_dwnld_max_time, cptr->request_complete_time);
      average_time->url_dwnld_tot_time += cptr->request_complete_time;
      average_time->url_dwnld_count++;
      UPDATE_GROUP_BASED_NETWORK_TIME(request_complete_time, url_dwnld_tot_time, url_dwnld_min_time, url_dwnld_max_time, url_dwnld_count);
    }
  }

  status = cptr->req_ok;
  url_ok = !status;
  NSDL2_CONN(NULL, cptr, "request_type %d",request_type);

  if (request_type == FTP_DATA_REQUEST) {
    /* Unset connection link for control connection */
    ((connection *)(cptr->conn_link))->conn_link = NULL;
    NSDL3_FTP(vptr, cptr, "Freeing stuff for DATA_REQUEST");
  } else { /* other protocols*/
      vptr->urls_awaited--;
      NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed %s session with server %s",
             get_req_type_by_name(request_type),
             nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

      if(request_type == DNS_REQUEST) {
        FREE_AND_MAKE_NULL(DNSDATA->dns_resp.buf, "Freeing dns_resp.buf", -1); // previous size (sizeof(DnsData)) 
      }

      if(request_type == USER_SOCKET_REQUEST) {
        FREE_AND_MAKE_NULL(((userSocketData*)cptr->data)->recvBuf, "freeing receive buffer for user socket data", -1);
      }
      FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
  }

  //In case of LDAP, close the connection only in case of LOGOUT opreation.
  if((request_type == JRMI_REQUEST || request_type == FC2_REQUEST || 
                  ((request_type == LDAP_REQUEST || request_type == LDAPS_REQUEST) && cptr->url_num->proto.ldap.operation != LDAP_LOGOUT)) 
                  && status == NS_REQUEST_OK)
    close_fd (cptr, 0, now);
  else
    close_fd (cptr, 1, now);
  
  if (status != NS_REQUEST_OK) { //if page not success
     NSDL3_HTTP(vptr, cptr, "aborting on main status=%d", status);
     abort_bad_page(cptr, status, redirect_flag);
     if (request_type == XMPP_REQUEST)
     {
       vptr->xmpp_status = status;
     }
  }

  handle_url_complete(cptr, request_type, now, url_ok,
		      redirect_flag, status, taken_from_cache); 

  //dont get into handle_page_complete as it switches back to user context, as it assumes that
  //we're called frm the nvm context , which is not true 
  if(request_type == USER_SOCKET_REQUEST) {
    return;
  } 

// fix the bug 3974
// When we are using smtp api then do not log url. That why we are commenting the code
/*
  if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT){
    NSDL2_CONN(NULL, NULL, "Call log_url_record Function");
    if (log_url_record(my_port_index, vptr, cptr, status, cptr->request_complete_time, redirect_flag) == -1)
      NSTL1_OUT(NULL, NULL, "Error in logging the url record\n");
  }*/
  
  cptr->req_ok = NS_REQUEST_OK;

  /* Only Last will be handled here */
  if (!vptr->urls_awaited) {
    handle_page_complete(cptr, vptr, 1, now, request_type);
  } else {
    NSDL2_CONN(NULL, cptr, "Something wrong here");  // Error LOG
  }

  if (cptr->conn_state == CNST_FREE) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSTL1(vptr, cptr, "Connection slot is already in free connection list");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
    }
  }
}

void close_accounting( u_ns_ts_t now )
{
  #ifndef CAV_MAIN
  ClientData client_data;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  fflush(stdout);

/* Child should not update following  
  global_settings->test_duration = now - global_settings->test_start_time;
  update_test_runphase_duration();
*/

  client_data.i = 1;
  progress_report( client_data , now);

  if (do_verbose)
  (void) dis_timer_del( progress_tmr );

  //Taken care in the above call to progress_report
  //average_time->c_tot_time += average_time->tot_time;

  NSDL3_MESSAGES(NULL, NULL, "num_connections = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserSPWaiting = %d, gNumVuserBlocked = %d \n", num_connections, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserSPWaiting, gNumVuserBlocked);

  NSDL3_MESSAGES(NULL, NULL, "average_time->num_connections = %d, average_time->cur_vusers_active = %d, average_time->cur_vusers_thinking = %d, average_time->cur_vusers_waiting = %d, average_time->cur_vusers_cleanup = %d, average_time->cur_vusers_in_sp = %d average_time->cur_vusers_blocked = %d, average_time->cur_vusers_paused = %d\n", average_time->num_connections, average_time->cur_vusers_active, average_time->cur_vusers_thinking, average_time->cur_vusers_waiting, average_time->cur_vusers_cleanup, average_time->cur_vusers_in_sp, average_time->cur_vusers_blocked, average_time->cur_vusers_paused);

  //executed in Ramp down phase
  average_time->num_connections = num_connections;
  average_time->smtp_num_connections = smtp_num_connections;
  average_time->cur_vusers_active = gNumVuserActive;
  average_time->cur_vusers_thinking = gNumVuserThinking;
  average_time->cur_vusers_waiting = gNumVuserWaiting;
  average_time->cur_vusers_cleanup = gNumVuserCleanup;
  average_time->cur_vusers_in_sp = gNumVuserSPWaiting;   
  average_time->cur_vusers_blocked = gNumVuserBlocked;
  average_time->cur_vusers_paused = gNumVuserPaused;


  FILL_GRP_BASE_VUSER
  // This will set cumulative fields int the avgtime except error code which is done above
  if (cum_timestamp > now) cum_timestamp = now;
  average_time->cum_user_ms += ((now - cum_timestamp) * RUNNING_VUSERS);
  average_time->total_cum_user_ms += average_time->cum_user_ms;

  NSDL3_MESSAGES(NULL, NULL, "total_cum_user_ms = %llu", average_time->total_cum_user_ms);

  //average_time->avg_users = (double)(average_time->total_cum_user_ms)/(double)(now - g_start_time);
  average_time->running_users = RUNNING_VUSERS;

  FILL_GRP_BASED_DATA;
// Added if 0 on date jun 9, 2008, as part of debug logging
#if 0

#ifndef NS_PROFILE
  if (global_settings->debug) {
#endif
    sprintf(heading, "END * (cur=%u) (pid:%d)" , get_ms_stamp(), getpid());
    print_report(stdout, NULL, URL_REPORT, 0, average_time, heading);
#ifdef NS_TIME
    sprintf(heading, "END * Pages   (pid:%d)" ,getpid());
    print_report(stdout, NULL, PAGE_REPORT, 0, average_time, heading);

    sprintf(heading, "END * Transactions   (pid:%d)" ,getpid());
    print_report(stdout, NULL, TX_REPORT, 0, average_time, heading);

    (void) printf("Info: (pid:%d) Vusers: Active %d, Thinking %d, Waiting %d, Idling %d, Connections %d Bytes/Sec: Rx %llu Tx %llu Payload Rx %llu\n\n",
		  getpid(),
		  gNumVuserActive,
		  gNumVuserThinking,
		  gNumVuserWaiting,
		  gNumVuserCleanup,
		  num_connections,
		  average_time->c_tot_tx_bytes*1000/(global_settings->test_duration),
		  average_time->c_tot_rx_bytes*1000/(global_settings->test_duration),
		  average_time->c_tot_total_bytes*1000/(global_settings->test_duration));
#endif
    printf("pid: %d\n", getpid());
    fflush(stdout);
#ifndef NS_PROFILE
  }
#endif

#endif

  average_time->opcode = FINISH_REPORT;
  average_time->elapsed = v_cur_progress_num;
  if(run_mode == NORMAL_RUN)
    flush_logging_buffers();
  dns_resolve_log_close_file();
  log_records = 0;
  #endif
}

void*
my_malloc( size_t size )
{
  void* new;
  NSDL1_MISC(NULL, NULL, "Method called. size=%u", size);
  if (size < 0) {
    NSTL1_OUT(NULL, NULL, "Trying to malloc w/ a negative or 0 size\n");
    end_test_run();
  }
  //KNQ: Is there a reason for avoiding 0 size alloc
  if (size == 0) {
    //NSTL1_OUT(NULL, NULL, "Trying to malloc 0 size\n");
    //sleep(300);
    return NULL;
  }
  new = malloc( size );
  if ( new == (void*) 0 )
    {
      (void) fprintf( stderr, "%s: out of memory\n", argv0 );
      fflush(stderr);
      end_test_run();
    }
  return new;
}

inline void copy_cptr_to_vptr(VUser *vptr, connection *cptr) 
{
  NSDL3_HTTP(NULL, cptr, "Method called, cptr->buf_head=%s, cptr->bytes = %d", cptr->buf_head, cptr->bytes);
  vptr->buf_head = cptr->buf_head;
  vptr->body_offset = cptr->body_offset; //Add for body
  vptr->cur_buf = cptr->cur_buf;
  cptr->buf_head = NULL;
  cptr->cur_buf = NULL;
  if(!cptr->url_num->is_url_parameterized)
    vptr->url_num = cptr->url_num;
  
  NSDL3_HTTP(NULL, cptr, "vptr->url_num = %p", vptr->url_num);
  vptr->bytes = cptr->bytes;
  vptr->compression_type = cptr->compression_type;

  /* Change done for 4.1.15 to save http_resp_code in vptr also */
  vptr->http_resp_code = cptr->req_code;

  if(cptr->flags & NS_CPTR_FLAGS_AMF) {
     cptr->flags &= ~NS_CPTR_FLAGS_AMF;
     vptr->flags |= NS_RESP_AMF;
  }

  if(cptr->flags & NS_CPTR_FLAGS_HESSIAN) {
     cptr->flags &= ~NS_CPTR_FLAGS_HESSIAN;
     vptr->flags |= NS_RESP_HESSIAN;
  }

  if(cptr->flags & NS_CPTR_CONTENT_TYPE_PROTOBUF) {
     cptr->flags &= ~NS_CPTR_CONTENT_TYPE_PROTOBUF;
     vptr->flags |= NS_RESP_PROTOBUF;  //Manish: This flag is used to decide whether protobuf has to be decoded or not
  }
  
  //Copy grpc flag to vptr 
  if(cptr->flags & NS_CPTR_CONTENT_TYPE_GRPC_PROTO) {
     cptr->flags &= ~NS_CPTR_CONTENT_TYPE_GRPC_PROTO;
     vptr->flags |= NS_VPTR_CONTENT_TYPE_GRPC_PROTO;   
  }
   //for future use=>  do not delete
  /*if(cptr->flags & NS_CPTR_CONTENT_TYPE_GRPC_JSON) {
     cptr->flags &= ~NS_CPTR_CONTENT_TYPE_GRPC_JSON;
     vptr->flags |= NS_VPTR_CONTENT_TYPE_GRPC_JSON;
  }

  if(cptr->flags & NS_CPTR_CONTENT_TYPE_GRPC_CUSTOM) {
     cptr->flags &= ~NS_CPTR_CONTENT_TYPE_GRPC_CUSTOM;
     vptr->flags |= NS_VPTR_CONTENT_TYPE_GRPC_CUSTOM;
  }*/
  /*bug 93672: gRPC NS_CPTR_GRPC_ENCODING added*/
  if(cptr->flags & NS_CPTR_GRPC_ENCODING) {
     cptr->flags &= ~NS_CPTR_GRPC_ENCODING;
     vptr->flags |= NS_VPTR_GRPC_ENCODING;
  }
 
 
  NSDL3_SCHEDULE(NULL, cptr, "global_settings->use_java_obj_mgr = %d", global_settings->use_java_obj_mgr);

  // TODO: change macro name NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY to NS_CPTR_FLAGS_JAVA_OBJ
  if(cptr->flags & NS_CPTR_FLAGS_JAVA_OBJ && global_settings->use_java_obj_mgr) {
    NSDL3_SCHEDULE(NULL, cptr, "Setting NS_CPTR_FLAGS_JAVA_OBJ bit into vptr");
     cptr->flags &= ~NS_CPTR_FLAGS_JAVA_OBJ;
     vptr->flags |= NS_RESP_JAVA_OBJ;
  }

  if(cptr->flags & NS_CPTR_RESP_FRM_CACHING){
    NSDL3_SCHEDULE(NULL, cptr, "Setting NS_RESP_FRM_CACHING bit into vptr");
    vptr->flags |= NS_RESP_FRM_CACHING;
  }

  if(cptr->cptr_data)
  {
    // NSDL3_HTTP(NULL, cptr, "cptr->cptr_data = %p", cptr->cptr_data);

    if(cptr->cptr_data->nw_cache_state & TCP_HIT_MASK)
    {
      NSDL3_HTTP(NULL, cptr, "url is of main url type");
      vptr->flags |= NS_RESP_NETCACHE;
    } else {
      vptr->flags &= ~NS_RESP_NETCACHE;
    } 
  }

  memcpy ((char *)&(vptr->sin_addr), (char *) &(cptr->cur_server), sizeof(struct sockaddr_in6));
}

static inline void calc_avg_time(connection* cptr, VUser *vptr, int request_type, 
			  u_ns_ts_t now, int url_ok, int status) {

  unsigned int download_time;

  NSDL2_SCHEDULE(NULL, cptr, "url_ok = %d , status=%d cptr->req_ok = %d",url_ok, status, cptr->req_ok);

  //  excluding the failed url statistics. Refer the debug log below for more detail
  if(NS_EXCLUDE_STOPPED_STATS_FROM_URL)
  {
    NSDL2_SCHEDULE(NULL, vptr, "exclude_stopped_stats is on and url status is stopped, hence excluding the stopped stats from page dump, hits, drilldown database, response time & tracing");
    return; 
  }

  //Logging url sent byte and recv byte throughput for tx 
  tx_logging_on_url_completion(vptr, cptr, now);

  /*Moved from http_close_connection in Caching implementation*/
  cptr->request_complete_time = now - cptr->ns_component_start_time_stamp; 
  cptr->ns_component_start_time_stamp = now;

  if(cptr->bytes > 0) {
    SET_MIN (average_time->url_dwnld_min_time, cptr->request_complete_time);
    SET_MAX (average_time->url_dwnld_max_time, cptr->request_complete_time);
    average_time->url_dwnld_tot_time += cptr->request_complete_time;
    average_time->url_dwnld_count++;
    UPDATE_GROUP_BASED_NETWORK_TIME(request_complete_time, url_dwnld_tot_time, url_dwnld_min_time, url_dwnld_max_time, url_dwnld_count);
  }

  //Shilpa 16Feb2011 - Commented as the function calc_avg_time is called only if response is not taken from cache
  //if(!taken_from_cache) {// Not taken from cache
    /*Moved from http_close_connection in Caching implementation*/
    switch(request_type) {
      case HTTP_REQUEST:
      case HTTPS_REQUEST:
         INC_HTTP_HTTPS_NUM_TRIES_COUNTER(vptr);
         /*
         //In case of proxy connect requests, updating progress/graph counters
         NSDL2_SCHEDULE(NULL, cptr, "status=%d, cptr flags = %x ,CONNECT bit=0X%x",status, cptr->flags ,NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT);
         if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)
           update_proxy_counters(status);
*/
         break;
      case SMTP_REQUEST:
      case SMTPS_REQUEST:
         INC_SMTP_SMTPS_NUM_TRIES_COUNTER(vptr);
         break;
      case POP3_REQUEST:
      case SPOP3_REQUEST:
         INC_POP3_SPOP3_NUM_TRIES_COUNTER(vptr);
         break;
      /*case SPOP3_REQUEST:
         average_time->pop3_num_tries++;
         average_time->pop3_error_codes[status]++;
         break;*/
      case DNS_REQUEST:
         INC_DNS_NUM_TRIES_COUNTER(vptr);
         break;
      case LDAP_REQUEST:
      case LDAPS_REQUEST:
         INC_LDAP_NUM_TRIES_COUNTER(vptr);
         break;
      case JRMI_REQUEST:
         INC_JRMI_NUM_TRIES_COUNTER(vptr);
         break;
      case FTP_REQUEST:
        INC_FTP_NUM_TRIES_COUNTER(vptr); 
        break;
      case IMAP_REQUEST:
      case IMAPS_REQUEST:
        INC_IMAP_IMAPS_NUM_TRIES_COUNTER(vptr); 
        break;
      case FTP_DATA_REQUEST:
      case XHTTP_REQUEST:
      case CPE_RFC_REQUEST:
         break;
      case USER_SOCKET_REQUEST:
         break;
      case WS_REQUEST:
      case WSS_REQUEST:
         INC_WS_WSS_NUM_TRIES_COUNTER(vptr);
         break;
      case SOCKET_REQUEST:
      case SSL_SOCKET_REQUEST:
         // Alread handle on ns_socket_recv
         break;
      case FC2_REQUEST:   
         INC_FC2_NUM_TRIES_COUNTER(vptr);         
         break;

      default:
       NSEL_MAJ(vptr, NULL, ERROR_ID, ERROR_ATTR, 
                            "Invalid request type (%d)",
                            request_type);
       break;
    }
  //}


  // Moved up as now we need this for updation of nw_cache_stats
  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
    download_time = cptr->ns_component_start_time_stamp - cptr->started_at;
  else
    // In case of RBU, we cannot use time stamp as there is time spent in waiting for har file
    // Issue  we are setting page time to URL time
    // TODO - We need to get time of each URL and add in NS
    download_time = (vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0;
  
  if (cptr->url_num->request_type == HTTP_REQUEST || cptr->url_num->request_type == HTTPS_REQUEST) 
  {
      update_http_status_codes(cptr, average_time);
/*
      if(SHOW_GRP_DATA)
      {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        update_http_status_codes(cptr, lol_average_time);
      }
*/
 
    // Newtork cache: If network cache is enable then increment the counters
    // Issue - How average_time->rx_bytes is used?
    if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(((VUser *)(cptr->vptr))->group_num))
      nw_cache_stats_update_counter(cptr, download_time, url_ok);
  }

 
  if (url_ok) {

    // Not needed. Avearge where it is needed.
    // average_time->avg_time = average_time->tot_time / average_time->num_hits;

    if (cptr->url_num->request_type == HTTP_REQUEST ||
        cptr->url_num->request_type == HTTPS_REQUEST) {

      SET_MIN (average_time->min_time, download_time);
      SET_MAX (average_time->max_time, download_time);
      average_time->tot_time += download_time;
      average_time->num_hits++;
    
      FILL_HTTP_TOT_TIME_FOR_GROUP_BASED(vptr, min_time, max_time, tot_time, 1);

      NSDL4_HTTP(NULL, NULL, "Success URL : average_time->min_time - %u, average_time->max_time - %d, "
                           "average_time->tot_time - %d",
                            average_time->min_time , average_time->max_time, average_time->tot_time);
        
      //if (g_percentile_report == 1)
      //  update_pdf_data(download_time, pdf_average_url_response_time, 0, 0, 0);
    }else if(cptr->url_num->request_type == FC2_REQUEST) {
      FC2AvgTime *loc_fc2_avgtime; 
      loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx); 
      SET_MIN (loc_fc2_avgtime->fc2_min_time, download_time);
      SET_MAX (loc_fc2_avgtime->fc2_max_time, download_time);
      loc_fc2_avgtime->fc2_tot_time += download_time;
      loc_fc2_avgtime->fc2_num_hits++;

      FILL_FC2_TOT_TIME_FOR_GROUP_BASED(vptr, fc2_min_time, fc2_max_time, fc2_tot_time, 1);

      NSDL4_HTTP(NULL, NULL, "Success URL : loc_fc2_avgtime->min_time - %u, loc_fc2_avgtime->max_time - %d, "
                           "loc_fc2_avgtime->tot_time - %d",
                            loc_fc2_avgtime->fc2_min_time , loc_fc2_avgtime->fc2_max_time, loc_fc2_avgtime->fc2_tot_time);

     }
     else if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST) { /* For SMTP */
      NSDL3_SMTP(NULL, cptr, "For smtp: download_time = %u, min = %llu", download_time, average_time->smtp_min_time);
      
      if (download_time < average_time->smtp_min_time) {
        average_time->smtp_min_time = download_time;
      }
      
      if (download_time > average_time->smtp_max_time) {
        average_time->smtp_max_time = download_time;
      }

      average_time->smtp_tot_time += download_time;
      average_time->smtp_num_hits++;

      FILL_SMTP_TOT_TIME_FOR_GROUP_BASED(vptr); 
       
      if (g_percentile_report == 1)
        update_pdf_data(download_time, pdf_average_smtp_response_time, 0, 0, 0);
    } else if (cptr->url_num->request_type == POP3_REQUEST || cptr->url_num->request_type == SPOP3_REQUEST) {
      NSDL3_POP3(NULL, cptr, "For pop3: download_time = %u, min = %llu", download_time, average_time->pop3_min_time);
      
      SET_MIN (average_time->pop3_min_time, download_time);
      SET_MAX (average_time->pop3_max_time, download_time);

      average_time->pop3_tot_time += download_time;
      average_time->pop3_num_hits++;
     
      FILL_POP3_TOT_TIME_FOR_GROUP_BASED(vptr, pop3_min_time, pop3_max_time, pop3_tot_time, 1);
      
      if (g_percentile_report == 1)
        update_pdf_data(download_time, pdf_average_pop3_response_time, 0, 0, 0);
    } else if (cptr->url_num->request_type == FTP_REQUEST) {
      NSDL3_FTP(NULL, cptr, "For ftp: download_time = %u, min = %llu", download_time, ftp_avgtime->ftp_min_time);

      SET_MIN (ftp_avgtime->ftp_min_time, download_time);
      SET_MAX (ftp_avgtime->ftp_max_time, download_time);

      ftp_avgtime->ftp_tot_time += download_time;
      ftp_avgtime->ftp_num_hits++;

      FILL_FTP_TOT_TIME_FOR_GROUP_BASED(vptr, ftp_min_time, ftp_max_time, ftp_tot_time, 1);
 
      if (g_percentile_report == 1)
        update_pdf_data(download_time, pdf_average_ftp_response_time, 0, 0, 0);
    } else if (cptr->url_num->request_type == DNS_REQUEST) {
      NSDL3_DNS(NULL, cptr, "For dns: download_time = %u, min = %llu", download_time, average_time->dns_min_time);
      
      SET_MIN (average_time->dns_min_time, download_time);
      SET_MAX (average_time->dns_max_time, download_time);

      average_time->dns_tot_time += download_time;
      average_time->dns_num_hits++;
   
      FILL_DNS_TOT_TIME_FOR_GROUP_BASED(vptr, dns_min_time, dns_max_time, dns_tot_time, 1);
    
      if (g_percentile_report == 1)
        update_pdf_data(download_time, pdf_average_dns_response_time, 0, 0, 0);
    }else if (cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST) {
      NSDL3_LDAP(NULL, cptr, "For ldap: download_time = %u, min = %llu", download_time, ldap_avgtime->ldap_min_time);

      if (download_time < ldap_avgtime->ldap_min_time) {
        ldap_avgtime->ldap_min_time = download_time;
      }

      if (download_time > ldap_avgtime->ldap_max_time) {
        ldap_avgtime->ldap_max_time = download_time;
      }

      ldap_avgtime->ldap_tot_time += download_time;
      ldap_avgtime->ldap_num_hits++;
     
      FILL_LDAP_TOT_TIME_FOR_GROUP_BASED(vptr); 
       
      // if (g_percentile_report == 1) // To check do we need this?
      // update_pdf_data(download_time, pdf_average_ldap_response_time, 0, 0, 0);
    } else if (cptr->url_num->request_type == IMAP_REQUEST || cptr->url_num->request_type == IMAPS_REQUEST) {
      NSDL3_IMAP(NULL, cptr, "For imap: download_time = %u, min = %llu", download_time, imap_avgtime->imap_min_time);

      SET_MIN (imap_avgtime->imap_min_time, download_time);
      SET_MAX (imap_avgtime->imap_max_time, download_time);
      imap_avgtime->imap_tot_time += download_time;
      imap_avgtime->imap_num_hits++;

      FILL_IMAP_TOT_TIME_FOR_GROUP_BASED(vptr, imap_min_time, imap_max_time, imap_tot_time, 1);

      // if (g_percentile_report == 1) // To check do we need this?
      // update_pdf_data(download_time, pdf_average_imap_response_time, 0, 0, 0);
    } else if (cptr->url_num->request_type == JRMI_REQUEST) {
      NSDL3_IMAP(NULL, cptr, "For imap: download_time = %u, min = %llu", download_time, jrmi_avgtime->jrmi_min_time);

      if (download_time < jrmi_avgtime->jrmi_min_time) {
        jrmi_avgtime->jrmi_min_time = download_time;
      }

      if (download_time > jrmi_avgtime->jrmi_max_time) {
        jrmi_avgtime->jrmi_max_time = download_time;
      }

      jrmi_avgtime->jrmi_tot_time += download_time;
      jrmi_avgtime->jrmi_num_hits++;

      FILL_JRMI_TOT_TIME_FOR_GROUP_BASED(vptr);
      // if (g_percentile_report == 1) // To check do we need this?
      // update_pdf_data(download_time, pdf_average_jrmi_response_time, 0, 0, 0);
    } else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST)) {
      NSDL3_WS(NULL, cptr, "For ws: download_time = %u, min = %llu", download_time, ws_avgtime->ws_min_time);

      if (download_time < ws_avgtime->ws_min_time) {
        ws_avgtime->ws_min_time = download_time;
      }

      if (download_time > ws_avgtime->ws_max_time) {
        ws_avgtime->ws_max_time = download_time;
      }

      ws_avgtime->ws_tot_time += download_time;
      ws_avgtime->ws_num_hits++;

      NSDL3_WS(NULL, cptr, "ws_avgtime->ws_num_hits = [%d], ws_avgtime->ws_min_time = [%d], ws_avgtime->ws_max_time = [%d]", ws_avgtime->ws_num_hits, ws_avgtime->ws_min_time, ws_avgtime->ws_max_time);

      FILL_WS_TOT_TIME_FOR_GROUP_BASED(vptr);
      //if (g_percentile_report == 1)
        //update_pdf_data(download_time, pdf_average_ws_response_time, 0, 0, 0);
     }
  }
  else
  {
    if (cptr->url_num->request_type == HTTP_REQUEST ||
        cptr->url_num->request_type == HTTPS_REQUEST) {

      SET_MIN (average_time->url_failure_min_time, download_time);
      SET_MAX (average_time->url_failure_max_time, download_time);
      average_time->url_failure_tot_time += download_time;
 
      FILL_HTTP_TOT_TIME_FOR_GROUP_BASED(vptr, url_failure_min_time, url_failure_max_time, url_failure_tot_time, 0);
      
      NSDL4_HTTP(NULL, NULL, "Failed URL : average_time->url_failure_min_time - %u, average_time->url_failure_max_time - %d, "
                           "average_time->url_failure_tot_time - %d",
                            average_time->url_failure_min_time , average_time->url_failure_max_time, average_time->url_failure_tot_time);


    } else if (cptr->url_num->request_type == FC2_REQUEST) {
      FC2AvgTime *loc_fc2_avgtime;
      loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx);
      
      SET_MIN (loc_fc2_avgtime->fc2_failure_min_time, download_time);
      SET_MAX (loc_fc2_avgtime->fc2_failure_max_time, download_time);
      loc_fc2_avgtime->fc2_failure_tot_time += download_time;

      FILL_FC2_TOT_TIME_FOR_GROUP_BASED(vptr, fc2_failure_min_time, fc2_failure_max_time, fc2_failure_tot_time, 0);

      NSDL4_HTTP(NULL, NULL, "Failed FC2 : loc_fc2_avgtime->fc2_failure_min_time - %u, loc_fc2_avgtime->fc2_failure_max_time - %d, "
                           "loc_fc2_avgtime->fc2_failure_tot_time - %d",
                            loc_fc2_avgtime->fc2_failure_min_time , loc_fc2_avgtime->fc2_failure_max_time, loc_fc2_avgtime->fc2_failure_tot_time);
    } else if (cptr->url_num->request_type == POP3_REQUEST ||
        cptr->url_num->request_type == SPOP3_REQUEST) {
 
      SET_MIN (average_time->pop3_failure_min_time, download_time);
      SET_MAX (average_time->pop3_failure_max_time, download_time);
      average_time->pop3_failure_tot_time += download_time;
 
      FILL_POP3_TOT_TIME_FOR_GROUP_BASED(vptr, pop3_failure_min_time, pop3_failure_max_time, pop3_failure_tot_time, 0);

      NSDL4_HTTP(NULL, NULL, "Failed POP3 : average_time->pop3_failure_min_time - %u, average_time->pop3_failure_max_time - %d, "
                           "average_time->pop3_failure_tot_time - %d",
                            average_time->pop3_failure_min_time , average_time->pop3_failure_max_time, average_time->pop3_failure_tot_time);
    } else if (cptr->url_num->request_type == FTP_REQUEST ){
 
      SET_MIN (ftp_avgtime->ftp_failure_min_time, download_time);
      SET_MAX (ftp_avgtime->ftp_failure_max_time, download_time);
      ftp_avgtime->ftp_failure_tot_time += download_time;
 
      FILL_FTP_TOT_TIME_FOR_GROUP_BASED(vptr, ftp_failure_min_time, ftp_failure_max_time, ftp_failure_tot_time, 0);

      NSDL4_HTTP(NULL, NULL, "Failed FTP : ftp_avgtime->ftp_failure_min_time - %u, ftp_avgtime->ftp_failure_max_time - %d, "
                           "ftp_avgtime->ftp_failure_tot_time - %d",
                            ftp_avgtime->ftp_failure_min_time, ftp_avgtime->ftp_failure_max_time, ftp_avgtime->ftp_failure_tot_time);
    } else if (cptr->url_num->request_type == DNS_REQUEST){
 
      SET_MIN (average_time->dns_failure_min_time, download_time);
      SET_MAX (average_time->dns_failure_max_time, download_time);
      average_time->dns_failure_tot_time += download_time;
 
      FILL_DNS_TOT_TIME_FOR_GROUP_BASED(vptr, dns_failure_min_time, dns_failure_max_time, dns_failure_tot_time, 0);

      NSDL4_HTTP(NULL, NULL, "Failed DNS : average_time->dns_failure_min_time - %u, average_time->dns_failure_max_time - %d, "
                             "average_time->dns_failure_tot_time- %d",
                              average_time->dns_failure_min_time, average_time->dns_failure_max_time, average_time->dns_failure_tot_time);
    } else if (cptr->url_num->request_type == IMAPS_REQUEST ||
                cptr->url_num->request_type == IMAP_REQUEST){
 
      SET_MIN (imap_avgtime->imap_failure_min_time, download_time);
      SET_MAX (imap_avgtime->imap_failure_max_time, download_time);
      imap_avgtime->imap_failure_tot_time += download_time;
 
      FILL_IMAP_TOT_TIME_FOR_GROUP_BASED(vptr, imap_failure_min_time, imap_failure_max_time, imap_failure_tot_time, 0);

      NSDL4_HTTP(NULL, NULL, "Failed FTP : imap_avgtime->imap_failure_min_time - %u, imap_avgtime->imap_failure_max_time - %d, "
                           "imap_avgtime->imap_failure_tot_time - %d",
                            imap_avgtime->imap_failure_min_time, imap_avgtime->imap_failure_max_time, imap_avgtime->imap_failure_tot_time);
    }
 }

  //For Overall URL Response Time
  if (cptr->url_num->request_type == HTTP_REQUEST ||
      cptr->url_num->request_type == HTTPS_REQUEST) {
    SET_MIN (average_time->url_overall_min_time, download_time);
    SET_MAX (average_time->url_overall_max_time, download_time);
    average_time->url_overall_tot_time += download_time;

    FILL_HTTP_TOT_TIME_FOR_GROUP_BASED(vptr, url_overall_min_time, url_overall_max_time, url_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall URL : average_time->url_overall_min_time - %u, average_time->url_overall_max_time - %d, "
                           "average_time->url_overall_tot_time - %d",
                            average_time->url_overall_min_time, average_time->url_overall_max_time, average_time->url_overall_tot_time);
    if (g_percentile_report == 1)
      update_pdf_data(download_time, pdf_average_url_response_time, 0, 0, 0);
  } else if (cptr->url_num->request_type == FC2_REQUEST) {
    FC2AvgTime *loc_fc2_avgtime;
    loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx);
    
    SET_MIN (loc_fc2_avgtime->fc2_overall_min_time, download_time);
    SET_MAX (loc_fc2_avgtime->fc2_overall_max_time, download_time);
    
    loc_fc2_avgtime->fc2_overall_tot_time += download_time;

    FILL_FC2_TOT_TIME_FOR_GROUP_BASED(vptr, fc2_overall_min_time, fc2_overall_max_time, fc2_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall FC2 : loc_fc2_avgtime->fc2_overall_min_time - %u, loc_fc2_avgtime->fc2_overall_max_time - %d, "
                           "loc_fc2_avgtime->fc2_overall_tot_time - %d",
                            loc_fc2_avgtime->fc2_overall_min_time, loc_fc2_avgtime->fc2_overall_max_time, 
                            loc_fc2_avgtime->fc2_overall_tot_time);

  } else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST)) {
    SET_MIN (average_time->pop3_overall_min_time, download_time);
    SET_MAX (average_time->pop3_overall_max_time, download_time);
    average_time->pop3_overall_tot_time += download_time;

    FILL_POP3_TOT_TIME_FOR_GROUP_BASED(vptr, pop3_overall_min_time, pop3_overall_max_time, pop3_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall POP3 : average_time->pop3_overall_min_time - %u, average_time->pop3_overall_max_time - %d, "
                           "average_time->pop3_overall_tot_time - %d",
                            average_time->pop3_overall_min_time, average_time->pop3_overall_max_time, average_time->pop3_overall_tot_time);

  } else if (cptr->url_num->request_type == FTP_REQUEST)  {
    SET_MIN (ftp_avgtime->ftp_overall_min_time, download_time);
    SET_MAX (ftp_avgtime->ftp_overall_max_time, download_time);
    ftp_avgtime->ftp_overall_tot_time += download_time;

    FILL_FTP_TOT_TIME_FOR_GROUP_BASED(vptr, ftp_overall_min_time, ftp_overall_max_time, ftp_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall FTP : ftp_avgtime->ftp_overall_min_time - %u, ftp_avgtime->ftp_overall_max_time - %d, "
                           "ftp_avgtime->ftp_overall_tot_time - %d",
                            ftp_avgtime->ftp_overall_min_time, ftp_avgtime->ftp_overall_max_time, ftp_avgtime->ftp_overall_tot_time);

  } else if (cptr->url_num->request_type == DNS_REQUEST) {
    SET_MIN (average_time->dns_overall_min_time, download_time);
    SET_MAX (average_time->dns_overall_max_time, download_time);
    average_time->dns_overall_tot_time += download_time;

    FILL_DNS_TOT_TIME_FOR_GROUP_BASED(vptr, dns_overall_min_time, dns_overall_max_time, dns_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall URL : average_time->dns_overall_min_time - %u, average_time->dns_overall_max_time - %d, "
                           "average_time->dns_overall_tot_time - %d",
                            average_time->dns_overall_min_time, average_time->dns_overall_max_time, average_time->dns_overall_tot_time);

  } else if ((cptr->url_num->request_type == IMAP_REQUEST) ||(cptr->url_num->request_type == IMAPS_REQUEST)) {
    SET_MIN (imap_avgtime->imap_overall_min_time, download_time);
    SET_MAX (imap_avgtime->imap_overall_max_time, download_time);
    imap_avgtime->imap_overall_tot_time += download_time;

    FILL_IMAP_TOT_TIME_FOR_GROUP_BASED(vptr, imap_overall_min_time, imap_overall_max_time, imap_overall_tot_time, 0);

    NSDL4_HTTP(NULL, NULL, "Overall URL : imap_avgtime->imap_overall_min_time - %u, imap_avgtime->imap_overall_max_time - %d, "
                           "imap_avgtime->imap_overall_tot_time - %d",
                            imap_avgtime->imap_overall_min_time, imap_avgtime->imap_overall_max_time, imap_avgtime->imap_overall_tot_time);

  }
}

void handle_url_complete(connection* cptr, int request_type,
			 u_ns_ts_t now, int url_ok, int redirect_flag,
			 int status, char taken_from_cache) {
  VUser *vptr;

  NSDL1_HTTP(NULL, cptr, "Method called, url OK = %d, redirect_flag = 0x%x, "
			     " status = %d, taken_from_cache = %d",
			      url_ok, redirect_flag, status, taken_from_cache);
  vptr = cptr->vptr;

  if(cptr->req_ok == NS_USEONCE_ABORT)
  {
     INC_HTTP_FETCHES_STARTED_COUNTER(vptr);
  }

  //#Shilpa 16Feb2011 - The function calc_avg_time will be called only if response is not taken from cache
  if(!(taken_from_cache) && !(cptr->http_protocol == HTTP_MODE_HTTP2 && runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push 
                             && (cptr->flags & NS_CPTR_HTTP2_PUSH))) {// Not taken from cache
    calc_avg_time(cptr, vptr, request_type, now, url_ok, status);
  }
  compression_type = cptr->compression_type;
  http_header_size = cptr->body_offset; 
  if(global_settings->monitor_type == HTTP_API)
  {
    //Total Time = Resolve(DNS) Time + Connect Time + SSL Handshake Time + Send(Write Complete) Time + First Byte Received Time + Download Time
    int total_time = cptr->dns_lookup_time + cptr->connect_time + cptr->ssl_handshake_time + cptr->write_complete_time
                                           + cptr->first_byte_rcv_time + cptr->request_complete_time;
    //Fill Send Time
    SET_MIN (cavtest_http_avg->send_min_time, cptr->write_complete_time);
    SET_MAX (cavtest_http_avg->send_max_time, cptr->write_complete_time);
    cavtest_http_avg->send_tot_time += cptr->write_complete_time;
    cavtest_http_avg->send_time_count++;

    //Fill Redirection Time
    if(vptr->redirect_count > 0)
    {
      SET_MIN (cavtest_http_avg->redirect_min_time, total_time);
      SET_MAX (cavtest_http_avg->redirect_max_time, total_time);
      cavtest_http_avg->redirect_tot_time += total_time;
      cavtest_http_avg->redirect_time_count++;
    }

    //Total time
    SET_MIN (cavtest_http_avg->total_min_time, total_time);
    SET_MAX (cavtest_http_avg->total_max_time, total_time);
    cavtest_http_avg->total_tot_time += total_time;
    cavtest_http_avg->total_time_count++;

    //Status Code
    cavtest_http_avg->status_code = cptr->req_code;
    cavtest_log_rep(vptr, NULL, 0, 1);
  }

  on_url_done(cptr, redirect_flag, taken_from_cache, now);

  if(NS_IF_TRACING_ENABLE_FOR_USER){
    NSDL2_USER_TRACE(vptr, cptr, "Method called, User tracing enabled");
    ut_update_http_status_code(cptr);
  }

  /* Add checks for RedirectionDepth = ALL or 1,2,3,..n 
  * do not access redirect_count using cptr since its value is cleared 
  * Use always vptr 
  * subtract -1 from redirect_count since its incrementing above in close_connection().
  */
  if ((((cptr->url_num->request_type == HTTP_REQUEST) ||
       (cptr->url_num->request_type == HTTPS_REQUEST)) && (cptr->url_num->proto.http.type == MAIN_URL)) || 
      (cptr->url_num->request_type == JNVM_JRMI_REQUEST) || (cptr->url_num->request_type == JRMI_REQUEST) || 
      ((cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST) && 
       (cptr->url_num->proto.ldap.type == MAIN_URL)) || 
      (cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) ||
      (cptr->url_num->request_type == SOCKET_REQUEST || cptr->url_num->request_type == SSL_SOCKET_REQUEST))
  {
    // It is final URL of redirection chain OR
    // at least one variable is be processed on this redirection depth
    // Added for HTTP Auth NTLM in 3.8.2. We should not copy in case HTTP auth is in progress
    if((!(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM) || !(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC) || !(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST) || !(cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)) && 
         ((!(redirect_flag & NS_HTTP_REDIRECT_URL)) ||
           (vptr->redirect_count > 0 && 
           (vptr->cur_page->redirection_depth_bitmask & (1 << (vptr->redirect_count -1))))
      ))
    {
      NSDL4_VARS(vptr, cptr, "redirect_flag = 0x%x, redirection_depth_bitmask = %d, "
                 "redirect_count = %d",
                 redirect_flag, vptr->cur_page->redirection_depth_bitmask,
                 vptr->redirect_count);
      copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 
    }


    // If at least one variable is be processed on this redirection depth
    if(vptr->redirect_count > 0 && 
         vptr->cur_page->redirection_depth_bitmask & (1 << (vptr->redirect_count -1)))
    {
      NSDL4_VARS(vptr, cptr, "Do data processing");
      /*Call do_data_processing() for RedirectionDepth=1,2,3, ..n or ALL
        * Redirection=Last will be handled in handle_page_complete()-->do_data_processing().
        * special case: for example RedirectionDepth=5 and this is the last, then handle this case here.
        * we are not calling this for embedded urls
      */
      if(redirect_flag & NS_HTTP_REDIRECT_URL)  // If Main url is redirect to, then we need to free buffers (arg4 = 1)
      { 
        do_data_processing(cptr, now, vptr->redirect_count, 1);
      } else { // If Main url is NOT redirect to, then we will not free here. It will be freed once page is complete
        do_data_processing(cptr, now, vptr->redirect_count, 0);
      }

     // vptr->redirect_count = 0; //Must reset this for next call
    /* if page_status fail and action is stop then abort the current page else do nothing
     This is conditionaly done as we have CVFail in Search Var, XML on
     different redirection depth of Main url. So we need to preserve the CVFail
     unless there are HTTP level errors on fetching of redirected URL
     For example, Main URL -> R1 -> R2. If Action On Fail is continue
      If search var on R1 causes CVFail and
         R2 fetching is succesful, then page status will be CVFail
         However if R2 fetching is Not succesful, then page status will be R2 status
    */
      if ((vptr->page_status != NS_REQUEST_OK) && (!(vptr->flags & NS_ACTION_ON_FAIL_CONTINUE)))
      {
        /* assuming copy_cptr_to_vptr() is not changing some of the value of cptr
           that are required in abort_bad_page()
        */
        abort_bad_page(cptr, vptr->page_status, redirect_flag);
      }
    }

  }

  // Free location url is not redirect or auto redirect is off 
  // This must be freed after on_url_done as we have a API
  // ns_get_redirect_url() which can be called from URL POST_CB method
  if(!(redirect_flag & NS_HTTP_REDIRECT_URL))
    FREE_LOCATION_URL(cptr, redirect_flag);
  
  /*Doing here because above this we need cptr_data*/
  if(cptr->cptr_data)
  {
    FREE_AND_MAKE_NULL_EX(cptr->cptr_data->buffer, cptr->cptr_data->len, "Freeing cptr data buffer", -1);
    cptr->cptr_data->len = 0;
    //bug id  78144 : make free only when cptr->cptr_data is available
    FREE_AND_MAKE_NULL_EX(cptr->cptr_data, sizeof(cptr_data_t), "Freeing cptr data", -1);
  }
  
  /* Bug#2186 
     1. Main Url is cachable
     2. Embedded url is same as main url
     3. Embedded url is picked from cache and cptr->buf_head will point start pointing to cacheptr->buf_head for main
     4. cptr is now reused for another embedded url which is non-cacheable and hence cptr->buf_head is freed

    Solution: Making cptr->buf_head = NULL here as doesn't require cptr->buf_head anywhere after this point and
    We have already stored cptr->buf_head in vptr->buf_head whereever required.
  */
   cptr->buf_head = NULL;

   /*Bug 67499: Incase of Redirection all headers till Redirection_Depth
     was getting added and due to this if search will be on header for a 
     particular depth, it was always showing value of first occurence for that
     LB and RB and not for that depth.
     
     Solution: Now, in case of Redirection, setting header len to 0 after each URL get 
     processed.*/
   if((redirect_flag & NS_HTTP_REDIRECT_URL) && vptr->response_hdr)
   {
     NSDL3_HTTP(NULL, NULL, "Going to make response header length to 0");
     vptr->response_hdr->used_hdr_buf_len = 0;
   }
}

//Called once the page is co  mplete

/* Following function in to get request type
 * We are using this function so that we can 
 * control request type from here, As we have 
 * request type in url_num also but it got freed 
 * some times before close_fd called, so we will 
 * use request type directly from cptr, one another
 * reason of this is that we can control request type
 * from here without doing much changes in future.
 */

inline unsigned char get_request_type(connection *cptr) {

  NSDL2_HTTP(NULL, cptr, "Method called, request_type = %d", cptr->request_type);
  return (cptr->request_type);
}

//scripts.list is created for script UI to get lists of scripts with proj/subproj used in scenario
void create_scripts_list()
{
  char buf[2048];
  FILE *scripts_list_fp;
  int i;

  sprintf(buf, "%s/logs/TR%d/scripts/scripts.list", g_ns_wdir, testidx);

  if ((scripts_list_fp = fopen(buf, "w")) == NULL) {
    NS_EXIT(1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }

  for(i = 0; i < total_runprof_entries; i++) {
    fprintf(scripts_list_fp, "%s\n", get_sess_name_with_proj_subproj_int(runprof_table_shr_mem[i].sess_ptr->sess_name,
                                     runprof_table_shr_mem[i].sess_ptr->sess_id, "/"));

  }
  if(fclose(scripts_list_fp) != 0)
  {
    //file is not closed successfully
    NS_EXIT(1, CAV_ERR_1000021, buf, errno, nslb_strerror(errno));
  }
}
