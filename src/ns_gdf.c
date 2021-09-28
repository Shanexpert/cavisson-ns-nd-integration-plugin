//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h> /* not needed on IRIX */
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <limits.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "smon.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_monitor_profiles.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_log.h"
#include "ns_goal_based_sla.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_proxy_server_reporting.h"
//#include "logging.h"
//#include "netstorm.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_page_based_stats.h"
#include "ns_ip_data.h"
#include "ns_svr_ip_normalization.h"
//#include "ns_error_codes.h"
#include "ns_trace_level.h"

#include "nslb_cav_conf.h"
#include "ns_runtime_runlogic_progress.h"
#include "nslb_log.h"
#include "ns_ndc.h"
#include "ns_server_ip_data.h"
#include "ns_trans.h"
#include "ns_websocket_reporting.h"
#include "ns_rbu_domain_stat.h"
#include "ns_script_parse.h"
#include "ns_appliance_health_monitor.h"
#include "ns_monitor_metric_priority.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_check_monitor.h"
#include "ns_h2_reporting.h"
#include "ns_error_msg.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_socket.h"

#define GDF_MAX_GRAPH_LINE 64000         //Max number of Graph lines possible in one group.
#define RTG_MAX_IN_GB 2147483648                   //2*1024*1024*1024
#define GDF_CM_START_RPT_GRP_ID 10001
//#define GDF_UM_START_RPT_GRP_ID 50001

//Netcache graph index in netstorm.gdf is 5
//Graph|Transaction NetCache Hits (Pct)|5|scalar|sample|-|-|0|AS|-1|-1|NA|NA|Percentage of transactions served from network cache based on main URL
#define NS_TX_NETCACHE_GRAPH_IDX (rpGraphID == 5)
#include "wait_forever.h"
#include "ns_parent.h"
#include "ns_mon_log.h"

// Msg Data Variable
char *msg_data_ptr = NULL;            // to point the Msg Data Buffer
ns_ptr_t msg_data_size = 0;                // to store the size of Msg Data Buffer
ns_ptr_t tmp_msg_data_size = 0;                // to store the size of Msg Data Buffer
ns_ptr_t hml_start_rtg_idx = -1;                // to store the size of Msg Data Buffer
int g_gdf_processing_flag = -1;
NormObjKey *g_gdf_hash = NULL;
/* 
   hml_msg_data_size[MAX_METRIC_PRIORITY_LEVELS + 1][2] 

   +--------+-------------+
 H |rtg_idx |tot_msg_size |
   +--------+-------------+
 M |rtg_idx |tot_msg_size |
   +--------+-------------+
 L |rtg_idx |tot_msg_size |
   +--------+-------------+
 D |rtg_idx |tot_msg_size |
   +--------+-------------+
*/
ns_ptr_t hml_msg_data_size[MAX_METRIC_PRIORITY_LEVELS + 1][2];        // To store msg data size for high medium and low for all the monitors

int last_HML_graph_idx[MAX_METRIC_PRIORITY_LEVELS + 1];
int hml_relative_idx[MAX_METRIC_PRIORITY_LEVELS + 1]; 

ns_ptr_t nc_msg_data_size = 0;                // NetCloud:to store the size of Msg Data Buffer
ns_ptr_t ns_gp_msg_data_size = 0;
int cm_index = -1;
static ns_ptr_t old_msg_data_size = 0;

// global variable

char *testrungdf_buffer = NULL; // this will hold testrun.gdf data without monitor data
long fsize = 0;

// Entries Related to Info
char version[6] = "\0";               // Storing version of GDF

// Entries Related to Groups
Group_Info *group_data_ptr = NULL; // Global Pointer for Group Buffer
int total_groups_entries = 0;      // Total No of Group entries.
int max_groups_entries = 0;        // Max No of Group entries.
int group_count = 0;               // to store the group count for testrun.gdf
int ns_gp_end_idx = 0;             // to end index of NS group in group_data_ptr
int ns_gp_count = 0;               // to NS group count

// Entries Related to Graphs
Graph_Info *graph_data_ptr = NULL; // Global Pointer for Graph Buffer
int total_graphs_entries = 0;      // Total No of Graph entries.
int max_graphs_entries = 0;        // Max No of Graph entries.
int element_count = 0;             // to track the no of values required in GDF
int graph_count_mem = 0;           // to store the group count for memory allocation of graph
int ns_graph_end_idx = 0;
int ns_graph_count = 0;
//Using this counter in file name: 'testrun.gdf.<test_run_gdf_count>' & 'rtgMessage.dat.<test_run_gdf_count>'
int test_run_gdf_count = 0;
//taken global var for runtime change, just to avoid passing of runtime_flag to all the functions
char g_runtime_flag = 0;

//this variable is used in test file
FILE *write_gdf_fp;                   // file pointer for file in which we are writing.(temp gdf)

//rtg msg seq
extern int rtg_msg_seq;
// global pointers for each Structure
// Scaler Group
Vuser_gp *vuser_gp_ptr = NULL;
unsigned int vuser_group_idx;
SSL_gp *ssl_gp_ptr = NULL;
unsigned int ssl_gp_idx;
Url_hits_gp *url_hits_gp_ptr = NULL;
unsigned int url_hits_gp_idx;
fc2_gp *fc2_gp_ptr = NULL;
unsigned int fc2_gp_idx;
Page_download_gp *page_download_gp_ptr = NULL;
unsigned int page_download_gp_idx;
Session_gp *session_gp_ptr = NULL;
unsigned int session_gp_idx;
Trans_overall_gp *trans_overall_gp_ptr = NULL;
unsigned int trans_overall_gp_idx;
URL_fail_gp *url_fail_gp_ptr = NULL;
unsigned int url_fail_gp_idx;
Page_fail_gp *page_fail_gp_ptr = NULL;
unsigned int page_fail_gp_idx;
Session_fail_gp *session_fail_gp_ptr = NULL;
unsigned int session_fail_gp_idx;
Trans_fail_gp *trans_fail_gp_ptr = NULL;
unsigned int trans_fail_gp_idx;
NO_system_stats_gp *no_system_stats_gp_ptr = NULL;
unsigned int no_system_stats_gp_idx;
NO_network_stats_gp *no_network_stats_gp_ptr = NULL;
unsigned int no_network_stats_gp_idx;

// Vector Group
Server_stats_gp *server_stats_gp_ptr = NULL;
unsigned int server_stats_gp_idx;
Tunnel_stats_gp *tunnel_stats_gp_ptr = NULL;
unsigned int tunnel_stats_gp_idx;

Trans_stats_gp *trans_stats_gp_ptr = NULL;
unsigned int trans_stats_gp_idx;
Trans_cum_stats_gp *trans_cum_stats_gp_ptr = NULL;
unsigned int trans_cum_stats_gp_idx;

/* HTTP Status Codes */
Http_Status_Codes_gp *http_status_codes_gp_ptr = NULL;
int http_status_codes_gp_idx;

// SMTP Group
SMTP_Net_throughput_gp* smtp_net_throughput_gp_ptr = NULL;
unsigned int smtp_net_throughput_gp_idx;
SMTP_hits_gp* smtp_hits_gp_ptr = NULL;
unsigned int smtp_hits_gp_idx;
SMTP_fail_gp* smtp_fail_gp_ptr = NULL;
unsigned int smtp_fail_gp_idx;


POP3_Net_throughput_gp* pop3_net_throughput_gp_ptr = NULL;
unsigned int pop3_net_throughput_gp_idx;
POP3_hits_gp* pop3_hits_gp_ptr = NULL;
unsigned int pop3_hits_gp_idx;
POP3_fail_gp* pop3_fail_gp_ptr = NULL;
unsigned int pop3_fail_gp_idx;


/* FTP */
FTP_Net_throughput_gp* ftp_net_throughput_gp_ptr = NULL;
unsigned int ftp_net_throughput_gp_idx;
FTP_hits_gp* ftp_hits_gp_ptr = NULL;
unsigned int ftp_hits_gp_idx;
FTP_fail_gp* ftp_fail_gp_ptr = NULL;
unsigned int ftp_fail_gp_idx;

/* LDAP */
LDAP_Net_throughput_gp* ldap_net_throughput_gp_ptr = NULL;
unsigned int ldap_net_throughput_gp_idx;
LDAP_hits_gp* ldap_hits_gp_ptr = NULL;
unsigned int ldap_hits_gp_idx;
LDAP_fail_gp* ldap_fail_gp_ptr = NULL;
unsigned int ldap_fail_gp_idx;

/* IMAP */
IMAP_Net_throughput_gp* imap_net_throughput_gp_ptr = NULL;
unsigned int imap_net_throughput_gp_idx;
IMAP_hits_gp* imap_hits_gp_ptr = NULL;
unsigned int imap_hits_gp_idx;
IMAP_fail_gp* imap_fail_gp_ptr = NULL;
unsigned int imap_fail_gp_idx;

/* JRMI */
JRMI_Net_throughput_gp* jrmi_net_throughput_gp_ptr = NULL;
unsigned int jrmi_net_throughput_gp_idx;
JRMI_hits_gp* jrmi_hits_gp_ptr = NULL;
unsigned int jrmi_hits_gp_idx;
JRMI_fail_gp* jrmi_fail_gp_ptr = NULL;
unsigned int jrmi_fail_gp_idx;

/* DNS */
DNS_Net_throughput_gp* dns_net_throughput_gp_ptr = NULL;
unsigned int dns_net_throughput_gp_idx;
DNS_hits_gp* dns_hits_gp_ptr = NULL;
unsigned int dns_hits_gp_idx;
DNS_fail_gp* dns_fail_gp_ptr = NULL;
unsigned int dns_fail_gp_idx;

DynObjForGDF dynObjForGdf[MAX_DYN_OBJS];
int set_group_idx_for_txn = 0;
int set_group_idx_for_rbu_domain = 0;
#define DYNOBJ_GROUP_INFO_IS_NOT_FILLED             0 
#define DYNOBJ_GROUP_INFO_IS_FILLED                 1 

/* WS */
WSStats_gp *ws_stats_gp_ptr = NULL;
unsigned int ws_stats_gp_idx;
WSStatusCodes_gp *ws_status_codes_gp_ptr = NULL;
unsigned int ws_status_codes_gp_idx;
WSFailureStats_gp *ws_failure_stats_gp_ptr = NULL;
unsigned int ws_failure_stats_gp_idx;

/*** XMPP   ***/
XMPP_gp *xmpp_stat_gp_ptr = NULL;
unsigned int xmpp_stat_gp_idx;

// to save rtg start index of all other groups except monitors because monitors are saving their rtg index in CM_info structure
#define RTG_INDX_ARRAY_SIZE 60
ns_ptr_t rtg_index_array[RTG_INDX_ARRAY_SIZE];

//extern int max_used_generator_entries;
//extern GeneratorEntry* generator_entry;

//buffer for testrun_all_gdf_info.txt
static char testrun_buff[2048];

u_ns_ts_t g_testrun_rtg_timestamp;

static int deleted_monitor_count = 0;
//This variable is to count monitors having "Warning: No vectors." in their vector list, so that it can be handled while writing in testrun.gdf. REFER BUG 21578;
/*** PDF ***/
static int tx_total_pdf_data_size = -1;
static long long int tx_total_pdf_data_size_8B = -1;
int num_graph_to_process = 0;	//GLOBAL - to store actual number of graph to be shown
inline void process_gdf_on_runtime_for_dyn_objs(char *fname, int dyn_obj_idx);
int init_dyn_objs_and_gdf_processing(void);
//char **print_resp_status_code_gdf_grp_vectors(FILE *gdf_file_fp) 
void set_index_for_NA_group(CM_info *local_cm_ptr, int grp_num_monitors, int monitor_idx);
int add_entry_in_mon_id_map_table();

static int prev_version = -1;
static FILE *all_gdf_fp = NULL;

char g_enable_clubing_hml_in_rtg;
/*bug 101437: */
#define NS_AVOID_HTTP2_METRICS_FOR_HTTP1(line) !((strstr(line, "HTTP2")) && (group_default_settings->http_settings.http_mode == HTTP_MODE_HTTP1))
//to initialize all gdf related ptrs/variables
static void init_gdf_ptrs_and_variables()
{
  NSDL2_GDF(NULL, NULL, "Method called.");

  //reset rtg packet count
  rtg_msg_seq = 0;
  //reset gdf related counters
  msg_data_size = total_groups_entries = max_groups_entries = total_graphs_entries = max_graphs_entries = element_count = group_count = nc_msg_data_size = graph_count_mem = 0;
  cm_index = -1;
  memset(version, 0, 6);

  //reset gdf related table ptrs
  vuser_gp_ptr = NULL; 
  ssl_gp_ptr = NULL; 
  url_hits_gp_ptr = NULL;
  fc2_gp_ptr = NULL; 
  page_download_gp_ptr = NULL; 
  session_gp_ptr = NULL; 
  trans_overall_gp_ptr = NULL;
  url_fail_gp_ptr = NULL; 
  page_fail_gp_ptr = NULL; 
  session_fail_gp_ptr = NULL; 
  trans_fail_gp_ptr = NULL; 
  no_system_stats_gp_ptr = NULL; 
  no_network_stats_gp_ptr = NULL; 
  server_stats_gp_ptr = NULL; 
  tunnel_stats_gp_ptr = NULL; 
  trans_stats_gp_ptr = NULL; 
  ip_data_gp_ptr = NULL;
  rbu_page_stat_data_gp_ptr = NULL;
  page_based_stat_gp_ptr = NULL;
  trans_cum_stats_gp_ptr = NULL; 
  group_data_gp_ptr = NULL;
  http_status_codes_gp_ptr = NULL; 
  smtp_net_throughput_gp_ptr = NULL; 
  smtp_hits_gp_ptr = NULL; 
  smtp_fail_gp_ptr = NULL; 
  pop3_net_throughput_gp_ptr = NULL; 
  pop3_hits_gp_ptr = NULL; 
  pop3_fail_gp_ptr = NULL; 
  ftp_net_throughput_gp_ptr = NULL; 
  ftp_hits_gp_ptr = NULL; 
  ftp_fail_gp_ptr = NULL; 
  dns_net_throughput_gp_ptr = NULL; 
  dns_hits_gp_ptr = NULL; 
  dns_fail_gp_ptr = NULL; 
  ldap_net_throughput_gp_ptr = NULL; 
  ldap_hits_gp_ptr = NULL; 
  ldap_fail_gp_ptr = NULL; 
  imap_net_throughput_gp_ptr = NULL; 
  imap_hits_gp_ptr = NULL; 
  imap_fail_gp_ptr = NULL; 
  jrmi_net_throughput_gp_ptr = NULL; 
  jrmi_hits_gp_ptr = NULL; 
  jrmi_fail_gp_ptr = NULL; 
  vuser_flow_gp_ptr = NULL;
  rbu_domain_stat_gp_ptr = NULL;

  //reset gdf related table variables
  vuser_group_idx = ssl_gp_idx = url_hits_gp_idx = page_download_gp_idx = session_gp_idx = trans_overall_gp_idx = url_fail_gp_idx = page_fail_gp_idx = session_fail_gp_idx = trans_fail_gp_idx = no_system_stats_gp_idx = no_network_stats_gp_idx = server_stats_gp_idx = tunnel_stats_gp_idx = trans_stats_gp_idx =trans_cum_stats_gp_idx = http_status_codes_gp_idx = smtp_net_throughput_gp_idx = smtp_hits_gp_idx = smtp_fail_gp_idx = pop3_net_throughput_gp_idx = pop3_hits_gp_idx = pop3_fail_gp_idx = ftp_net_throughput_gp_idx = ftp_hits_gp_idx = ftp_fail_gp_idx = dns_net_throughput_gp_idx = dns_hits_gp_idx = dns_fail_gp_idx = total_pdf_data_size = ldap_net_throughput_gp_idx = ldap_hits_gp_idx = ldap_fail_gp_idx = imap_net_throughput_gp_idx = imap_hits_gp_idx = imap_fail_gp_idx = jrmi_net_throughput_gp_idx = jrmi_hits_gp_idx = jrmi_fail_gp_idx = rbu_domain_stat_data_gp_idx = xmpp_stat_gp_idx = fc2_gp_idx = 0;

  total_pdf_data_size = PDF_DATA_HEADER_SIZE;
  total_pdf_data_size_8B = PDF_DATA_HEADER_SIZE;

  NSDL2_GDF(NULL, NULL, "Method exited. total_pdf_data_size = %d", total_pdf_data_size);
}

void free_gdf_tables()
{
  NSDL2_GDF(NULL, NULL, "Method called.");

  if (!total_um_entries)
    free_gdf();
  else
    free_gdf_data();

  NSDL2_GDF(NULL, NULL, "Method exited.");
}

Graph_Data* get_graph_data_ptr(int gdf_group_vector_idx, int gdf_graph_vector_idx, Graph_Info *local_graph_data_ptr)
{
  NSDL2_GDF(NULL, NULL, "Method called, gdf_group_vector_idx=%d, gdf_graph_vector_idx=%d", gdf_group_vector_idx, gdf_graph_vector_idx);
  return((Graph_Data *)(local_graph_data_ptr->graph_data[gdf_group_vector_idx][gdf_graph_vector_idx]));
} 

static double convert_sec_print (double row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data))/1000.0);
}

// formula for converting the Row data (Periodic) to Per Second (PS)
static double convert_ps_print (double row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data (Periodic) to Per Minute (PM)
static double convert_pm_print (double row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data) * 60)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data from Bits/Sec to Kilo Bits Per Sec (kbps)
static double convert_kbps_print (double row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data))/1024.0);
}

// formula for converting the Row data (multiple of 100) in to Divide By 100 (dbh)
static double convert_dbh_print(double row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data))/100.0);
}

/* This is called for data which is in host format */
inline double convert_data_by_formula_print(char formula, double data)
{
  NSDL2_GDF(NULL, NULL, "Method called, data = %f", data);
  switch(formula)
  {
    case FORMULA_SEC:
      return convert_sec_print(data);
      break;
    case FORMULA_PM:
      return convert_pm_print(data);
      break;
    case FORMULA_PS:
      return convert_ps_print(data);
      break;
    case FORMULA_KBPS:
      return convert_kbps_print(data);
      break;
    case FORMULA_DBH:
      return convert_dbh_print(data);
      break;
    default:
      return (data);
      break;
  }
}

char *get_gdf_group_vector_name(int group_info_idx, int group_vector_idx)
{
  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, group_vector_idx = %d", group_info_idx, group_vector_idx);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;

  if (local_group_data_ptr->vector_names)
    return local_group_data_ptr->vector_names[group_vector_idx];
  else
    return "NA";
}

char *get_gdf_graph_vector_name(int group_info_idx, int graph_num, int graph_vector_idx)
{
  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, graph_vector_idx = %d, graph_num = %d", group_info_idx, graph_vector_idx, graph_num);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);

  if (local_graph_data_ptr->vector_names)
    return local_graph_data_ptr->vector_names[graph_vector_idx];
  else
    return "NA";
}

char get_gdf_graph_formula(int group_info_idx, int graph_num)
{
  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, graph_num = %d", group_info_idx, graph_num);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);

  return local_graph_data_ptr->formula;
}

char *get_gdf_group_name(int group_info_idx)
{
  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d", group_info_idx);
  if (group_info_idx < 0)
     return NO_GROUP_PROCESSED;

  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  return local_group_data_ptr->group_name;
}

char *get_gdf_graph_name(int group_info_idx, int graph_num)
{
  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, graph_num = %d", group_info_idx, graph_num);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);

  return local_graph_data_ptr->graph_name;
}

static inline double calc_gdf_raw_avg_of_c_avg(Graph_Info *local_graph_data_ptr, Graph_Data *gdata)
{
  double c_count;
  NSDL2_GDF(NULL, NULL, "Method called");
  if(local_graph_data_ptr->data_type == DATA_TYPE_CUMULATIVE)
    return(gdata->c_avg);
  else {
    if((local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE) || 
       (local_graph_data_ptr->data_type == DATA_TYPE_RATE) || 
       (local_graph_data_ptr->data_type == DATA_TYPE_SUM) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_2B_100) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_4B_1000) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_RATE_4B_1000) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_1B) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_4B) || 
       (local_graph_data_ptr->data_type == DATA_TYPE_SUM_4B) ||
       (local_graph_data_ptr->data_type == DATA_TYPE_T_DIGEST))
    {
      c_count = ((double )global_settings->test_duration)/((double )global_settings->progress_secs);
    } else 
      c_count = (double )gdata->c_count;

    if(!c_count) c_count = 1; // to avoid divide by 0 exception

    return((double )gdata->c_avg / (double )c_count);
  }
}

static inline double calc_gdf_avg_of_c_avg(Graph_Info *local_graph_data_ptr, Graph_Data *gdata)
{
  double c_count;

  NSDL2_GDF(NULL, NULL, "Method called. test_duration=%u, progress_secs=%lu, c_avg=%llu, c_count=%llu",
                         global_settings->test_duration, global_settings->progress_secs, gdata->c_avg, gdata->c_count);
  // For cumulative last sample is the cumulative value, so no need to divide by count
  if(local_graph_data_ptr->data_type == DATA_TYPE_CUMULATIVE)
    return(convert_data_by_formula_print(local_graph_data_ptr->formula, (double )gdata->c_avg));
  //check for test_duration , because test_duration comes after each sample. It may possible test_duration is not available at the moment       Arun :  07/04/08
  if(((local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE) || 
      (local_graph_data_ptr->data_type == DATA_TYPE_RATE) || 
      (local_graph_data_ptr->data_type == DATA_TYPE_SUM) || 
      (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_2B_100) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_4B_1000) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_RATE_4B_1000) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_4B) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_SAMPLE_1B) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_T_DIGEST) ||
      (local_graph_data_ptr->data_type == DATA_TYPE_SUM_4B)) && global_settings->test_duration != 0 )
    c_count = ((double )global_settings->test_duration)/((double )global_settings->progress_secs);
  else
    c_count = (double )gdata->c_count;

  if(!c_count) c_count = 1; // to avoid divide by 0 exception

  return(convert_data_by_formula_print(local_graph_data_ptr->formula, (double )gdata->c_avg/c_count));
}

void create_gdf_summary_data()
{
  char file_name[1024];
  char buf[4096];
  FILE *fptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  Group_Info *local_group_data_ptr = group_data_ptr;
  Graph_Info *local_graph_data_ptr;
  Graph_Data *gdata;
  
  int i, j, k, l, group_num_vector, graph_num_vector, gdo, ddo;
  
  sprintf(file_name, "%s/logs/TR%d/summary_gdf.data", g_ns_wdir, testidx);
  
  fptr = fopen(file_name, "w");

  strcpy(buf, "Group Display Order|Group Name(Vector Name)|Group ID|Data Display Order|Graph Name(Vector Name)|Avg|Min|Max\n");

  NSDL2_GDF(NULL, NULL, "buf=%s", buf);
  fwrite(buf, strlen(buf), 1, fptr);

  for (i = 0; i < total_groups_entries; i++, local_group_data_ptr++)
  {
    gdo = 10*( i + 1 );
    for (j = 0; j < local_group_data_ptr->num_graphs; j++) 
    {
      local_graph_data_ptr = graph_data_ptr + ( local_group_data_ptr->graph_info_index + j );
      group_num_vector = local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS];
      graph_num_vector = local_graph_data_ptr->num_vectors;
      ddo = 10*( j + 1 );
      NSDL2_GDF(NULL, NULL, "i = %d, j = 0, group_num_vector = %d, graph_num_vector = %d", i, j, group_num_vector, graph_num_vector);
      for (k = 0; k < group_num_vector; k++) 
      {
        for (l = 0; l < graph_num_vector; l++) 
        {
          /* Bug 58677: summary_gdf.data is read by gdata which has original monitor data. 
                        In case of data overflow summary_gdf.data will show some original data value and Dashboard will show -nan because
                        Dashboard read data from RTG and in RTG data dumped according to Graph Data Type. */
          gdata = get_graph_data_ptr(k, l, local_graph_data_ptr);
/*           if(!strstr(local_group_data_ptr->group_name, "Custom Monitor")) */
             sprintf(buf, "%d|%s(%s)|%d|%d|%s(%s)|%.3f|%.3f|%.3f\n", 
                           gdo, local_group_data_ptr->group_name, 
                           local_group_data_ptr->vector_names ? local_group_data_ptr->vector_names[k] : "NA", 
                           local_group_data_ptr->rpt_grp_id, ddo, local_graph_data_ptr->graph_name, 
                           local_graph_data_ptr->vector_names ? local_graph_data_ptr->vector_names[l] : "NA", 
                           calc_gdf_avg_of_c_avg(local_graph_data_ptr, gdata), 
                           convert_data_by_formula_print(local_graph_data_ptr->formula, gdata->c_min), 
                           convert_data_by_formula_print(local_graph_data_ptr->formula, gdata->c_max));
/*           else */
/*           {  */
/*              char server_ip[20]; */
/*              strcpy(server_ip, get_cm_server(local_graph_data_ptr->cm_index));  */
/*              sprintf(buf, "%d|%s(%s)|%d|%d|%s(%s)|%.3f|%.3f|%.3f\n", gdo, local_group_data_ptr->group_name, server_ip, local_group_data_ptr->rpt_grp_id, ddo, local_graph_data_ptr->graph_name, server_ip, calc_gdf_avg_of_c_avg(local_graph_data_ptr, gdata), convert_data_by_formula_print(local_graph_data_ptr->formula, gdata->c_min), convert_data_by_formula_print(local_graph_data_ptr->formula, gdata->c_max)); */
/*           } */
         
          NSDL2_GDF(NULL, NULL, "buf=%s", buf);
          fwrite(buf, strlen(buf), 1, fptr);
        }
      } 
    }
  }
  fclose(fptr);
}

static char* num_to_data_type(int type)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  switch(type)
  {
    case DATA_TYPE_SAMPLE:
      return ("sample"); 
      break;
    case DATA_TYPE_RATE:
      return ("rate"); 
      break;
    case DATA_TYPE_CUMULATIVE:
      return ("cumulative"); 
      break;
    case DATA_TYPE_TIMES:
      return ("times"); 
      break;
    case DATA_TYPE_TIMES_STD:
      return ("timesStd"); 
      break;
    case DATA_TYPE_SUM:
      return ("sum");
      break;
    case DATA_TYPE_SUM_4B:
      return ("sum_4B");
      break;
    case DATA_TYPE_SAMPLE_2B_100:
      return ("sample_2B_100");
      break;
    case DATA_TYPE_SAMPLE_4B_1000:
      return ("sample_4B_1000");
      break;
    case DATA_TYPE_RATE_4B_1000:
      return ("rate_4B_1000");
      break;
    case DATA_TYPE_TIMES_4B_1000:
      return ("times_4B_1000");
      break;
    case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      return ("sample_2B_100_count_4B");
      break;
    case DATA_TYPE_TIMES_STD_4B_1000:
      return ("timesStd_4B_1000");
      break;
    case DATA_TYPE_SAMPLE_4B:
      return ("sample_4B");
      break;
    case DATA_TYPE_SAMPLE_1B:
      return ("sample_1B");
      break;
    case DATA_TYPE_TIMES_4B_10:
      return ("times_4B_10");
      break;
    case DATA_TYPE_T_DIGEST:
      return ("tdigest_4B");
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: dataType {%d} in GDF is not correct\n", type);
      NS_EXIT(-1,CAV_ERR_1013001,type);
  }
  return NULL;
}

static void log_gdf(FILE *fptr, int group_info_idx, int graph_num, int group_num_vectors, int graph_num_vectors)
{
  int i, j;
  
  Graph_Data ***three_d;
  char buf[2048];
  Graph_Data *gdata;

  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, graph_num = %d, group_num_vectors = %d, graph_num_vectors = %d", 
                         group_info_idx, graph_num, group_num_vectors, graph_num_vectors);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);
  
  three_d = (Graph_Data ***)local_graph_data_ptr->graph_data;

/* Calculating diff for cumulative graphs:
 * Issues with this logic
 * 1 – We get first sample at 10 sec. So we will loose data of first 10 secs
 * 2 – If during test, value become lower than first sample, then it may not be correct. */

  for (i = 0; i < group_num_vectors; i++) {
    for (j = 0; j < graph_num_vectors; j++) {
      gdata = (Graph_Data *)three_d[i][j];
      sprintf(buf, "%s,%s,%s,%s,%.3f,%.3f,%.3f,%0.0f\n", 
              local_group_data_ptr->group_name, 
              local_graph_data_ptr->graph_name, 
              local_group_data_ptr->vector_names ? local_group_data_ptr->vector_names[i] : local_graph_data_ptr->vector_names ? local_graph_data_ptr->vector_names[j] : "NA", 
              num_to_data_type(local_graph_data_ptr->data_type), 
              (local_graph_data_ptr->data_type == DATA_TYPE_CUMULATIVE) ? (gdata->c_min != MAX_LONG_LONG_VALUE) ? (gdata->c_max - gdata->c_min) : 0 : calc_gdf_avg_of_c_avg(local_graph_data_ptr, gdata),
              convert_data_by_formula_print(local_graph_data_ptr->formula, (double )gdata->c_min),
              convert_data_by_formula_print(local_graph_data_ptr->formula, (double )gdata->c_max),
              gdata->c_count);

      fwrite(buf, strlen(buf), 1, fptr);
    }
  }
}

/* Wed Dec  2 13:39:43 IST 2009  - Arun Nishad
 * This creates a file named gdfData.#testidx in Test Run Directory.
 * Earliar is was created in /tmp dir named /tmp/TR%d.gdfData.%d.
 * It was created to debug/verify gdf data in case of Goal Based Scenario.
 */
/* Prachi - 19/04/2013
 * Changes file name from gdfData.%d -> TestRunMetricsSummary.csv / TestRunMetricsSummary.%d.csv
 * Changes file path from $NS_WDIR/logs/TR%d/ -> $NS_WDIR/logs/TR%d/ready_reports/
 * Changes file format from "pipe separation" -> "comma separation"
 * Also changed some fields of file.
 */
/* Prachi - 20/04/2013
 * Removing Std-Dev field from file TestRunMetricsSummary.csv because we are getting some junk value for Std-Dev.
 * Using function: get_std_dev() for Std-Dev.
 */
void log_gdf_all_data()
{
  char file_name[1024];
  char buf[2048];
  static int count = 1;
  FILE *fptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  Group_Info *local_group_data_ptr = group_data_ptr;
  Graph_Info *local_graph_data_ptr;
  
  int i, j;

  //if(global_settings->module_mask >= MM_GRAPH) 
    //return;
 
  if(count == 1)
    sprintf(file_name, "%s/logs/TR%d/ready_reports/TestRunMetricsSummary.csv", g_ns_wdir, testidx);
  else
    sprintf(file_name, "%s/logs/TR%d/ready_reports/TestRunMetricsSummary.%d.csv", g_ns_wdir, testidx, count);
   
  count++;
  
  fptr = fopen(file_name, "w");

  /*sprintf(buf, "Test Duration|Progress Secs|Count\n%u|%d|%.3f\n", 
         global_settings->test_duration, global_settings->progress_secs, (double)global_settings->test_duration / global_settings->progress_secs);
  fwrite(buf, strlen(buf), 1, fptr);*/

  //Group Name,Graph Name,Vector Name,GraphType,Avg/Diff,Min,Max,Samples
  strcpy(buf, "Group Name,Graph Name,Vector Name,Graph Type,Avg/Diff,Min,Max,Samples\n");
  fwrite(buf, strlen(buf), 1, fptr);

  for (i = 0; i < total_groups_entries; i++, local_group_data_ptr++) {
    for (j = 0; j < local_group_data_ptr->num_graphs; j++) {
      local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index + j;
      log_gdf(fptr, i, j, local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], local_graph_data_ptr->num_vectors);
    }
  } 
  fclose(fptr);
}

static void init_graph_data_node(Graph_Data *g_data)
{
  g_data->c_avg = 0;
  g_data->c_min = MAX_LONG_LONG_VALUE;
  g_data->c_max = 0;
  g_data->c_count = 0;
  g_data->c_sum_of_sqr = 0;
}

void init_gdf(int group_info_idx, int graph_num, int group_num_vectors, int graph_num_vectors)
{
  int i, j;
  
  Graph_Data ***three_d;

  NSDL2_GDF(NULL, NULL, "Method called, group_info_idx = %d, graph_num = = %d, group_num_vectors = %d, graph_num_vectors = %d", 
                         group_info_idx, graph_num, group_num_vectors, graph_num_vectors);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);
  
  three_d = (Graph_Data ***)local_graph_data_ptr->graph_data;

  for (i = 0; i < group_num_vectors; i++) {
    //three_d[i] = (Graph_Data **) malloc(graph_num_vectors * sizeof(int *));
    for (j = 0; j < graph_num_vectors; j++) {
      //three_d[i][j] = (Graph_Data *)malloc(sizeof(Graph_Data));
      init_graph_data_node(((Graph_Data *)three_d[i][j]));
    }
  }
}

void init_gdf_all_data()
{
  Group_Info *local_group_data_ptr = group_data_ptr;
  Graph_Info *local_graph_data_ptr;
  
  int i, j;

  NSDL2_GDF(NULL, NULL, "Method Called");
  for (i = 0; i < total_groups_entries; i++, local_group_data_ptr++) {
    for (j = 0; j < local_group_data_ptr->num_graphs; j++) {
      local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index + j;
      init_gdf(i, j, local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], local_graph_data_ptr->num_vectors);
    }
  }
}

static void free_graph_data(char ***g_data, int v_num_groups, int v_num_graphs)
{
  int i, j;
  NSDL2_GDF(NULL, NULL, "Method Called, v_num_graphs = %d, v_num_groups = %d", v_num_graphs, v_num_groups);
  //NSTL1_OUT(NULL, NULL, "free_graph_data() , v_num_groups = %d, v_num_graphs = %d\n", v_num_graphs, v_num_groups);
  for (i = 0; i < v_num_groups; i++) {
    for (j = 0; j < v_num_graphs; j++) {
      FREE_AND_MAKE_NULL_EX (g_data[i][j], sizeof(Graph_Data), "g_data[i][j] for j", j);
    }
      FREE_AND_MAKE_NULL_EX (g_data[i], v_num_graphs * sizeof(int *), "g_data[i] for i", i);
  }
  FREE_AND_MAKE_NULL_EX (g_data, v_num_groups * sizeof(int **), "g_data", -1);
}

//static void free_vector_names(char **v_names, int i)
static void free_vector_names(char **v_names, int num_vectors)
{
  NSDL2_GDF(NULL, NULL, "Method Called, v_names = %p, num_vectors = %d", v_names, num_vectors);
  int i;
  //NSDL2_GDF(NULL, NULL, "Method Called, num_vectors = %d", num_vectors);
  //NSTL1_OUT(NULL, NULL, "free_vector_names() num_vectors = %d\n", num_vectors);
  //NSDL2_GDF(NULL, NULL, "Method Called, i = %d", i);
  if (!v_names) return;
  for(i = 0; i < num_vectors; i++){
    if(v_names[i]){
      FREE_AND_MAKE_NULL_EX (v_names[i], strlen(v_names[i]) + 1, "v_names[i]", i);
    }
  }
   
  if(num_vectors != 0)
    FREE_AND_MAKE_NULL_EX (v_names, sizeof(v_names), "v_names", -1);
    //FREE_AND_MAKE_NULL_EX (v_names, strlen(v_names) + 1, "v_names", i);
    //FREE_AND_MAKE_NULL_EX (v_names, sizeof(v_names), "v_names", -1);
}

void free_gdf_data()
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  //Group_Info *local_group_data_ptr = group_data_ptr;
  Group_Info *local_group_data_ptr = group_data_ptr;
  Graph_Info *local_graph_data_ptr = graph_data_ptr;
  int i, j;

  /* Free groups and graph data first */
  for (i = 0; i < total_groups_entries; i++) {
    NSDL2_GDF(NULL, NULL, "graph_name = %s, vector_names = %p, num_vectors = %d", 
                           local_graph_data_ptr->graph_name, 
                           local_group_data_ptr->vector_names, 
                           local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]);
    FREE_AND_MAKE_NULL_EX (local_group_data_ptr->group_name, strlen(local_group_data_ptr->group_name) + 1, "Group Name", i);
    free_vector_names((local_group_data_ptr->vector_names), local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]);
    
    /* Free all graph_data */
    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
    for (j = 0; j < local_group_data_ptr->num_graphs; j++, local_graph_data_ptr++) {
    //for (j = 0; j < local_group_data_ptr->num_graphs; j++) {
      //for(k = 0; k < local_graph_data_ptr->num_vectors; k++, local_graph_data_ptr++)  
      //for(k = 0; k < local_graph_data_ptr->num_vectors; k++)  
      //{
        FREE_AND_MAKE_NULL_EX (local_graph_data_ptr->graph_name, strlen(local_graph_data_ptr->graph_name) + 1, "Graph Name", j);
        free_vector_names((local_graph_data_ptr->vector_names), local_graph_data_ptr->num_vectors);
        //free_vector_names((local_graph_data_ptr->vector_names), k);
  
        //NSDL2_GDF("k = %d, local_graph_data_ptr->num_vectors = [%d].", k, local_graph_data_ptr->num_vectors); 
        ////NSTL1_OUT(NULL, NULL, "k = %d, local_graph_data_ptr->num_vectors = [%d]\n", k, local_graph_data_ptr->num_vectors); 
        //for(k = 0; k < local_graph_data_ptr->num_vectors; k++) 
        //{
        free_graph_data((local_graph_data_ptr->graph_data), local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], local_graph_data_ptr->num_vectors);
        //}
//        local_graph_data_ptr++;
       /* //local_graph_data_ptr->graph_data = NULL;
        //local_graph_data_ptr->vector_names = NULL;
        //local_graph_data_ptr->graph_name = NULL;
        //local_group_data_ptr++;*/
      //}
    }
    local_group_data_ptr++;
  }

  //if(msg_data_ptr)
    //FREE_AND_MAKE_NULL_EX (msg_data_ptr, msg_data_size, "msg_data_ptr", -1);
}

void free_graph_data_ptr(int group_idx)
{
  Graph_Info *local_graph_data_ptr = NULL;
  Group_Info *local_group_data_ptr = NULL;
  int i; 
  int graph_start_count = group_data_ptr[group_idx].graph_info_index;
  local_group_data_ptr = group_data_ptr + group_idx;
  for(i = graph_start_count; i < (group_data_ptr[group_idx].num_graphs + graph_start_count) ; i++)
  {
    NSDL2_GDF(NULL, NULL, "free_graph_data_ptr called %d time", i );
    local_graph_data_ptr = graph_data_ptr + i; 
    FREE_AND_MAKE_NULL_EX ((local_graph_data_ptr->graph_name), strlen(local_graph_data_ptr->graph_name) + 1, "Graph Name", i);
    FREE_AND_MAKE_NULL_EX ((local_graph_data_ptr->derived_formula), strlen(local_graph_data_ptr->derived_formula) + 1, "Graph Derived Formula", i);
    FREE_AND_MAKE_NULL_EX ((local_graph_data_ptr->graph_discription), strlen(local_graph_data_ptr->graph_discription) + 1, "Graph Description", i);

    //free_vector_names((local_graph_data_ptr->vector_names), (local_graph_data_ptr->num_vectors));
    free_graph_data((local_graph_data_ptr->graph_data), (local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]), (local_graph_data_ptr->num_vectors));  
    FREE_AND_MAKE_NULL_EX ((local_graph_data_ptr->gline), strlen(local_graph_data_ptr->gline) + 1, "Graph Line", i);
    memset(&graph_data_ptr[i], 0, sizeof(Graph_Info));
  }
}

void free_group_graph_data_ptr()
{
  int i;
  Group_Info *local_group_data_ptr = NULL;
  // dont free and memset ns gdf
  for(i = ns_gp_end_idx; i < total_groups_entries; i++)
  {
    NSDL2_GDF(NULL, NULL, "free_group_graph_data_ptr called %d time", i );
    local_group_data_ptr = group_data_ptr + i;
    free_graph_data_ptr(i);
    FREE_AND_MAKE_NULL_EX ((local_group_data_ptr->group_name), strlen(local_group_data_ptr->group_name) + 1, "Group Name", i);
    FREE_AND_MAKE_NULL_EX ((local_group_data_ptr->excluded_graph), strlen(local_group_data_ptr->excluded_graph) + 1, "Excluded Graph", i);
    FREE_AND_MAKE_NULL_EX ((local_group_data_ptr->group_description), strlen(local_group_data_ptr->group_description) + 1, "Group Description", i);
    FREE_AND_MAKE_NULL_EX ((local_group_data_ptr->Hierarchy), strlen(local_group_data_ptr->Hierarchy) + 1, "Hierarchy", i);
    FREE_AND_MAKE_NULL_EX ((local_group_data_ptr->groupMetric), strlen(local_group_data_ptr->groupMetric) + 1, "Group Metric", i);
    free_vector_names((local_group_data_ptr->vector_names), (local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]));
    memset(&group_data_ptr[i], 0, sizeof(Group_Info));
  }
  total_graphs_entries = ns_graph_end_idx;
  graph_count_mem = ns_graph_count;
  total_groups_entries = ns_gp_end_idx;
  group_count = ns_gp_count;
}


void free_gdf() /* Free all the groups and graphs and any memory used by them. */
{
  NSDL2_GDF(NULL, NULL, "Method Called");

  free_gdf_data();

  FREE_AND_MAKE_NULL_EX (group_data_ptr, sizeof(Group_Info), "group_data_ptr", -1);
  FREE_AND_MAKE_NULL_EX (graph_data_ptr, sizeof(Graph_Info), "graph_data_ptr", -1);
}

unsigned int get_gdf_group_info_idx(int rpt_grp_id)
{
  NSDL2_GDF(NULL, NULL, "Method Called, rpt_grp_id = %d", rpt_grp_id);
  Group_Info *local_group_data_ptr = group_data_ptr;
  int i;

  for (i = 0; i < total_groups_entries; i++, local_group_data_ptr++) {
    if (local_group_data_ptr->rpt_grp_id == rpt_grp_id)
      return i;
  }
  return -1;
}

int get_gdf_group_graph_info_idx(int rpt_grp_id, int rpt_id, int *group_info_idx, int *graph_num)
{
  NSDL2_GDF(NULL, NULL, "Method Called, rpt_grp_id = %d, rpt_id  = %d", rpt_grp_id, rpt_id);
  Group_Info *local_group_data_ptr = group_data_ptr;
  Graph_Info *local_graph_data_ptr = graph_data_ptr;
  int i, j;
  
  *group_info_idx = -1;
  *graph_num = -1;

  for (i = 0; i < total_groups_entries; i++, local_group_data_ptr++) {
    if (local_group_data_ptr->rpt_grp_id == rpt_grp_id) {
      local_graph_data_ptr += local_group_data_ptr->graph_info_index;

      *group_info_idx = i;
      /* now find rpt_id in graph */
      for (j = 0; j < local_group_data_ptr->num_graphs; j++, local_graph_data_ptr++) {
        if (local_graph_data_ptr->rpt_id == rpt_id) { /* Here we calculate */
          *graph_num = j;
          return 0;
        }
      }
    }
  }
  return -1;
}

int get_gdf_vector_num(int group_info_idx, int graph_num, int *gdf_group_vectors_num, int *gdf_graph_vectors_num)
{
  NSDL2_GDF(NULL, NULL, "Method Called, group_info_idx = %d, graph_num = %d", group_info_idx, graph_num);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);

  *gdf_group_vectors_num = local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS];
  *gdf_graph_vectors_num = local_graph_data_ptr->num_vectors;

  return 0;
}

int get_gdf_vector_idx(int group_info_idx, int graph_num, char *vector_name, int *gdf_group_vector_idx, int *gdf_graph_vector_idx)
{
  NSDL2_GDF(NULL, NULL, "Method Called, group_info_idx = %d, graph_num = %d, vector_name = %s", group_info_idx, graph_num, vector_name);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);
  int i;
  
  *gdf_group_vector_idx = 0;
  *gdf_graph_vector_idx = 0;

  if (local_group_data_ptr->vector_names) {
    for (i = 0; i < local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]; i++) {
      if (strcmp(local_group_data_ptr->vector_names[i], vector_name) == 0) {
        *gdf_group_vector_idx = i;
        return 0;
      }
    }
  }
  
  if (local_graph_data_ptr->vector_names) {
    for (i = 0; i < local_graph_data_ptr->num_vectors; i++) {
      if (strcmp(local_graph_data_ptr->vector_names[i], vector_name) == 0) {
        *gdf_graph_vector_idx = i;
        return 0;
      }
    }
  }
  
  return -1;
}


// here second argument is Long_long_data because we are passing cumulative data which is Long_long_data, rest having Long_data.this may give warning --???
static inline double convert_data_by_formula(char formula, Long_long_data data)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  switch(formula)
  {
    case FORMULA_SEC:
      return convert_sec(data);
      break;
    case FORMULA_PM:
      return convert_pm(data);
      break;
    case FORMULA_PS:
      return convert_ps(data);
      break;
    case FORMULA_KBPS:
      return convert_kbps(data);
      break;
    case FORMULA_DBH:
      return convert_dbh(data);
      break;
    default:
      return (data);
      break;
  }
}

/* convert_data_by_formula clone, it calls convert_* clone functions. */
static inline double convert_data_by_formula_long_long(char formula, char *data)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  switch(formula)
  {
    case FORMULA_SEC:
      return convert_sec_long_long(data);
      break;
    case FORMULA_PM:
      return convert_pm_long_long(data);
      break;
    case FORMULA_PS:
      return convert_ps_long_long(data);
      break;
    case FORMULA_KBPS:
      return convert_kbps_long_long(data);
      break;
    case FORMULA_DBH:
      return convert_dbh_long_long(data);
      break;
    default:
      return (double)(*(Long_data *)(data));
      break;
  }
}

/* This function returns Avg of all vector data of a graph */
double get_gdf_overall_c_avg(int group_info_idx, int graph_num)
{
  NSDL2_GDF(NULL, NULL, "Method Called, group_info_idx = %d, graph_num = %d", group_info_idx, graph_num);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);
  Graph_Data *gdata;
  double  converted;
  double  sum = 0;
  int     count = 0;
  int     i, j;

  for (i = 0; i < local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]; i++) {
    for (j = 0; j < local_graph_data_ptr->num_vectors; j++) {
      gdata = (Graph_Data *)(local_graph_data_ptr->graph_data[i][j]);

      sum += calc_gdf_avg_of_c_avg(local_graph_data_ptr, gdata);      

      NSDL3_GDF(NULL, NULL, "group_info_idx = %d, "
                "graph_num = %d, gdf_group_vector_idx = %d, gdf_graph_vector_idx = %d, "
                "formula = %d, c_avg = %0.0f, sum = %.3f, count = %d", 
                group_info_idx, graph_num, i, 
                j, local_graph_data_ptr->formula, gdata->c_avg,
                sum, count); 
      count++;
    }
  }

  converted = sum / count;
  return converted;
}

double get_gdf_data_c_avg(int group_info_idx, int graph_num, int gdf_group_vector_idx, int gdf_graph_vector_idx, int vector_opt)
{
  NSDL2_GDF(NULL, NULL, "Method Called, group_info_idx = %d, graph_num = %d, gdf_group_vector_idx = %d, gdf_graph_vector_idx = %d, vector_opt = %d", group_info_idx, graph_num, gdf_group_vector_idx, gdf_graph_vector_idx, vector_opt);
  Group_Info *local_group_data_ptr = group_data_ptr + group_info_idx;
  Graph_Info *local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + graph_num);
  Graph_Data *gdata;
  double  converted;

  if (vector_opt == SLA_VECTOR_OPTION_OVERALL) 
    return get_gdf_overall_c_avg(group_info_idx, graph_num);

  gdata = (Graph_Data *)(local_graph_data_ptr->graph_data[gdf_group_vector_idx][gdf_graph_vector_idx]);

  converted = calc_gdf_avg_of_c_avg(local_graph_data_ptr, gdata);

  NSDL3_GDF(NULL, NULL, "Method Called group_info_idx = %d, graph_num = %d, gdf_group_vector_idx = %d, gdf_graph_vector_idx = %d, formula = %d, c_avg = %0.0f, converted = %.3f", 
	    group_info_idx, graph_num, gdf_group_vector_idx, 
	    gdf_graph_vector_idx, local_graph_data_ptr->formula, gdata->c_avg,
	    converted); 
  return converted;
}

void update_gdf_data_times(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, 
                           Long_long_data cur_val, Long_long_data cur_min, Long_long_data cur_max, Long_long_data cur_count)
{
  /**
   * case 1: both salar
   *   graph_data[1][1] is valid
   * case 2: group vector, graph scalar
   *   graph_data[<vary>][1] is valid
   * case 3: both vectors
   *   graph_data[<vary>][<vary>] is valid
   */

  NSDL2_GDF(NULL, NULL, "Method called, cur_val = %f, cur_min = %f, cur_max = %f, cur_count = %f, gdf_group_idx = %d, "
                        "gdf_graph_num = %d, group_vec_idx = %d, graph_vec_idx  = %d", 
                         cur_val, cur_min, cur_max, cur_count, gdf_group_idx, gdf_graph_num, gdf_graph_num, group_vec_idx, graph_vec_idx);

  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;
  Graph_Data *g_data = NULL;

#ifdef NS_DEBUG_ON
  char *group_vec_name = "NA";
  char *graph_vec_name = "NA";
  char formula;
#endif

  if (gdf_group_idx > total_groups_entries || gdf_group_idx < 0) {
    //NSTL1_OUT(NULL, NULL, "Value of Group idx(%d) out of range(%d:%d) for group = %s and graph = %s\n", gdf_group_idx, 0 , total_groups_entries, local_group_data_ptr->group_name, local_graph_data_ptr->graph_name);
    return;
  }

  local_group_data_ptr = group_data_ptr + gdf_group_idx;
  local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + gdf_graph_num);
  
  if (local_graph_data_ptr->graph_state == GRAPH_EXCLUDED)
    return;

  if (gdf_graph_num > local_group_data_ptr->num_graphs || gdf_graph_num < 0) {
    NSTL1_OUT(NULL, NULL, "Value of Num Graph(%d) out of range(%d:%d) for group = %s and graph = %s\n", gdf_graph_num, 0, local_group_data_ptr->num_graphs, local_group_data_ptr->group_name, local_graph_data_ptr->graph_name);
   
    return;
  }

  if(group_vec_idx >= local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS])
   return;

  if (group_vec_idx > local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] || group_vec_idx < 0) {
    
    NSTL1_OUT(NULL, NULL, "Value of Group vec idx(%d) out of range(%d:%d) for group = %s and graph = %s\n", 
                           group_vec_idx, 0, local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], local_group_data_ptr->group_name, 
                           local_graph_data_ptr->graph_name);
    return;
  }

  if (graph_vec_idx > local_graph_data_ptr->num_vectors || graph_vec_idx < 0) {
    NSTL1_OUT(NULL, NULL, "Value of Graph vec idx(%d) out of range(%d:%d) for group = %s and graph = %s\n ", 
                           graph_vec_idx, 0, local_graph_data_ptr->num_vectors, 
                           local_group_data_ptr->group_name, local_graph_data_ptr->graph_name);
    return;
  }

#ifdef NS_DEBUG_ON

  NSDL2_WS(NULL, NULL, "local_group_data_ptr->group_type = %d, local_group_data_ptr->vector_names = %d", 
                                  local_group_data_ptr->group_type, local_group_data_ptr->vector_names);

  if(local_group_data_ptr->group_type == GROUP_GRAPH_TYPE_VECTOR && local_group_data_ptr->vector_names)
    group_vec_name = local_group_data_ptr->vector_names[group_vec_idx];
  if(local_graph_data_ptr->graph_type == GROUP_GRAPH_TYPE_VECTOR && local_graph_data_ptr->vector_names)
    graph_vec_name = local_graph_data_ptr->vector_names[graph_vec_idx];

  NSDL2_WS(NULL, NULL, "group_vec_name = %s, graph_vec_name = %s", group_vec_name, graph_vec_name);
  formula = local_graph_data_ptr->formula;
#endif
  
  g_data = (Graph_Data *)local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx];
  //  NSTL1_OUT(NULL, NULL, "Graph data Add = 0x%x\n", (unsigned int)g_data);

/*   NSDL3_GDF(vptr, cptr, "Method called. Previous values are, groupName = %s(%d), graphName = %s(%d), groupVecName = %s(%d), graphVectorName = %s(%d), PrevCum(avg = %.3f, min = %.3f, max = %.3f, count = %lu, sum_of_sqr = %llu), Cur(avg = %.3f, min = %.3f, max = %.3f, count = %llu, sum_of_sqr = TBD)", */
/*             local_group_data_ptr->group_name, gdf_group_idx, local_graph_data_ptr->graph_name, */
/* 	    gdf_graph_num, group_vec_name, group_vec_idx, graph_vec_name, graph_vec_idx, */
/* 	    convert_data_by_formula_print(formula, g_data->c_avg), */
/* 	    convert_data_by_formula_print(formula, g_data->c_min), */
/* 	    convert_data_by_formula_print(formula, g_data->c_max), */
/* 	    g_data->c_count, g_data->c_sum_of_sqr, */
/*             convert_data_by_formula_print(formula, cur_val), */
/* 	    convert_data_by_formula_print(formula, cur_min), */
/* 	    convert_data_by_formula_print(formula, cur_max), cur_count); */
  NSDL3_GDF(NULL, NULL, "Method called. Previous values are, groupName = %s(%d), graphName = %s(%d), groupVecName = %s(%d), graphVectorName = %s(%d), PrevCum(avg = %0.0f, min = %0.0f, max = %0.0f, count = %0.0f, sum_of_sqr = %0.0f), Cur(avg = %0.0f, min = %0.0f, max = %0.0f, count = %0.0f, sum_of_sqr = TBD) formula = %d",
            local_group_data_ptr->group_name, gdf_group_idx, local_graph_data_ptr->graph_name,
	    gdf_graph_num, group_vec_name, group_vec_idx, graph_vec_name, graph_vec_idx,
	    g_data->c_avg,
	    g_data->c_min,
	    g_data->c_max,
	    g_data->c_count, g_data->c_sum_of_sqr,
            cur_val,
	    cur_min,
	    cur_max, cur_count, formula);
  
  // Calculate min and max
  if(cur_min != cur_min) //for nan this check will fail
  {
    NSDL1_GDF(NULL, NULL, "Received nan in cur_min");
  }
  else
  {
    if(cur_min < g_data->c_min) g_data->c_min = cur_min;
  }

  if(cur_max != cur_max) //for nan this check will fail
  {
    NSDL1_GDF(NULL, NULL, "Received nan in cur_max");
  }
  else
  {  
    if(cur_max > g_data->c_max) g_data->c_max = cur_max;
  }

  // Note - Need to make c_avg 0?? (No)
  if(local_graph_data_ptr->data_type == DATA_TYPE_CUMULATIVE)
  {
    if(cur_val != cur_val) //for nan this check will fail, assuming if cur_count is nan then cur_val will also nan
    {
      NSDL1_GDF(NULL, NULL, "Received nan in cur_count");
    }
    else
    {
      g_data->c_avg = cur_val * cur_count; // For cumulative, we need to keep last value
      g_data->c_count += cur_count;
    }
  }
  // else if((g_data->c_count + cur_count) != 0)
  else 
  {
    if(cur_val != cur_val) //for nan this check will fail, assuming if cur_count is nan then cur_val will also nan
    {
      NSDL1_GDF(NULL, NULL, "Received nan in cur_count");
    }
    else
    {
      g_data->c_avg += cur_val * cur_count; 
      g_data->c_count += cur_count;
    }
    // Averaging will be done when we get the data
    // Long_long_data sum = (g_data->c_avg * g_data->c_count) + (cur_val * cur_count);
    // g_data->c_count += cur_count;
    // g_data->c_avg = sum/g_data->c_count;
  }

/*   NSDL3_GDF(vptr, cptr, "Method done. The updated values are, groupName = %s(%d), graphName = %s(%d), groupVecName = %s(%d), graphVectorName = %s(%d), UpdatedCum(avg = %.3f, min = %.3f, max = %.3f, count = %llu, sum_of_sqr = %llu)", */
/* 	    local_group_data_ptr->group_name, gdf_group_idx, local_graph_data_ptr->graph_name, */
/* 	    gdf_graph_num, group_vec_name, group_vec_idx, graph_vec_name, graph_vec_idx, */
/* 	    convert_data_by_formula_print(formula, g_data->c_avg), */
/* 	    convert_data_by_formula_print(formula, g_data->c_min), */
/* 	    convert_data_by_formula_print(formula, g_data->c_max), */
/* 	    g_data->c_count, g_data->c_sum_of_sqr); */

  NSDL3_GDF(NULL, NULL, "Method done. The updated values are, groupName = %s(%d), graphName = %s(%d), "
                        "groupVecName = %s(%d), graphVectorName = %s(%d), "
                        "PrevCum(avg = %0.0f, min = %0.0f, max = %0.0f, count = %0.0f, sum_of_sqr = %0.0f), "
                        "Cur(avg = %0.0f, min = %0.0f, max = %0.0f, count = %0.0f, sum_of_sqr = TBD) formula = %d",
                           local_group_data_ptr->group_name, gdf_group_idx, local_graph_data_ptr->graph_name,
  		           gdf_graph_num, group_vec_name, group_vec_idx, graph_vec_name, graph_vec_idx,
  		           g_data->c_avg,
  		           g_data->c_min,
  		           g_data->c_max,
  		           g_data->c_count, g_data->c_sum_of_sqr,
                           cur_val,
  		           cur_min,
  		           cur_max, cur_count, formula);

}

void update_gdf_data(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, Long_long_data cur_val)
{
  NSDL2_GDF(NULL, NULL, "Method Called: gdf_group_idx = %d, gdf_graph_num = %d, group_vec_idx = %d, "
                        "graph_vec_idx = %d, cur_val = %f", 
                         gdf_group_idx, gdf_graph_num, group_vec_idx, graph_vec_idx, cur_val);

  update_gdf_data_times(gdf_group_idx, gdf_graph_num, group_vec_idx, graph_vec_idx, cur_val, cur_val, cur_val, 1);
}

void update_gdf_data_times_std(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, Long_long_data cur_val, Long_long_data cur_min, Long_long_data cur_max, Long_long_data cur_count, Long_long_data cur_sum_of_sqr)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  // Use the same code till we add code for generating standard deviation
  update_gdf_data_times(gdf_group_idx, gdf_graph_num, group_vec_idx, graph_vec_idx, cur_val, cur_min, cur_max, cur_count);
}

char **init_2d(int no_of_host)
{
  NSDL2_GDF(NULL, NULL, "Method Called");

  //char **x = NULL;
  char **x ;
  if (no_of_host) {
    MY_MALLOC_AND_MEMSET (x, sizeof(int**) * no_of_host, "x for init_2d", -1);
    return x;
  }
  return NULL;
}

/* call init_2d before this */
void fill_2d(char **TwoD, int i, char *fill_data)
{
  NSDL2_GDF(NULL, NULL, "Method called fill_data = %s", fill_data);
  MY_MALLOC (TwoD[i], strlen(fill_data) +  1, "TwoD[i]", i);
  NSDL2_GDF(NULL, NULL, "**TwoD = %p", TwoD[i]);
  strcpy(TwoD[i], fill_data);
}

// for creating tmp gdf file in /tmp which will use to make testrun.gdf
//this function is not static because used in ns_test_gdf.c for test purpose.
void create_tmp_gdf()
{
  char filename[MAX_LINE_LENGTH];
  NSDL2_GDF(NULL, NULL, "Method Called");
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Creating tmp gdf file");  
  sprintf(filename, "/tmp/%d_tmp.gdf", testidx);     // making name of tmp gdf with testrun no as suffix, for the case of two or testrun running at the same time.
  if ((write_gdf_fp = fopen(filename, "w+")) == NULL)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening tmp gdf file %s\n", filename);
     perror("fopen");
     NS_EXIT(-1,CAV_ERR_1013002, filename);
  }
  NSDL3_GDF(NULL, NULL, "Created a tmp gdf file");
}

// open input gdf for reading
inline FILE *open_gdf(char* fname)
{
  FILE *read_gdf_fp;
  NSDL2_GDF(NULL, NULL, "Method Called, fname = %s", fname);

  //Trim white spaces at the start and end of the string.
  nslb_trim(fname);
  if ((read_gdf_fp = fopen(fname, "r")) == NULL)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening gdf file '%s'\n", fname);
     perror("fopen");
     NS_EXIT(-1,CAV_ERR_1013003, fname);
  }
  return read_gdf_fp;
}

// closing all gdfs.
//this function is not static because used in ns_test_gdf.c for test purpose.
void close_gdf(FILE* fp)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(fp)
    fclose(fp);
}

// this function will read the info line from input gdf fetch up version, numGroup for further use.this will return num of Group in the gdf.
static inline int process_info(char *line)
{
  int numGroup = 0;
  char *buffer[GDF_MAX_FIELDS];
  int i = 0;

  NSDL2_GDF(NULL, NULL, "Method Called, Processing line = %s", line);


  i = get_tokens(line, buffer, "|", GDF_MAX_FIELDS);
  if(i != 8)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error:  Number of fields are not correct in Info line in GDF, line = %s\n", line);
    NS_EXIT(-1,CAV_ERR_1013004, line);
  }

  if (version[0] == '\0')
    strcpy(version, buffer[1]);

  numGroup = atoi(buffer[2]);

  if(msg_data_size == 0)  //set only when processing info from netstorm.gdf
    msg_data_size = sizeof(Msg_data_hdr);

  NSDL3_GDF(NULL, NULL, "version = %s, numGroup = %d, Startindex = %d, msg_data_size = %ld", 
                         version, numGroup, sizeof(Msg_data_hdr), msg_data_size);
  return numGroup;
}

// validation for checking weather the group is scalar or not , using group type
static inline void is_group_scalar(char*grpType, int grpID)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(strcmp(grpType, "scalar"))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: groupType is not scaler for rpGroupID = %d\n", grpID);
    NS_EXIT(-1,CAV_ERR_1013005, grpID);
  }
}
// validation for checking weather the group is vector or not , using group type
static inline void is_group_vector(char*grpType, int grpID)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(strcmp(grpType, "vector"))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: groupType is not vector for rpGroupID = %d\n", grpID);
    NS_EXIT(-1,CAV_ERR_1013006, grpID);
  }
}


// this function will return size on the basis of DataType
inline int getSizeOfGraph(char type, int *num_element)
{
  NSDL2_GDF(NULL, NULL, "Method Called, type = %d, num_element = %d", type, num_element);

  switch(type)
  {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_RATE:
    case DATA_TYPE_SUM:
      *num_element += 1;
      return sizeof(Long_data);
      break;
    case DATA_TYPE_CUMULATIVE:
      *num_element += 1;
      return sizeof(Long_long_data);
      break;
    case DATA_TYPE_TIMES:
      *num_element += 4;
      return sizeof(Times_data);
      break;
    case DATA_TYPE_TIMES_STD:
      *num_element += 5;
      return sizeof(Times_std_data);
      break;
    case DATA_TYPE_SAMPLE_2B_100:
      *num_element += 1;
      return sizeof(Short_data);
      break;
    case DATA_TYPE_SAMPLE_4B:
    case DATA_TYPE_SUM_4B:
    case DATA_TYPE_RATE_4B_1000:
    case DATA_TYPE_SAMPLE_4B_1000:
    case DATA_TYPE_T_DIGEST:
      *num_element += 1;
      return sizeof(Int_data);
      break;
    case DATA_TYPE_TIMES_4B_10:
    case DATA_TYPE_TIMES_4B_1000:
      *num_element += 4;
      return sizeof(Times_data_4B);
      break;
   case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      *num_element += 4;
      return sizeof(SampleCount_data);
      break;
   case DATA_TYPE_SAMPLE_1B:
      *num_element +=1;
      return sizeof(Char_data);
      break;
   case DATA_TYPE_TIMES_STD_4B_1000:
      *num_element += 5;
      return sizeof(Times_std_data_4B);
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType {%d} in GDF is not correct\n", type);
      NS_EXIT(-1, CAV_ERR_1013001, type);
  }
  return 0;
}

// this function is for checking that weather Group is Dynamic or not, and return numVector
// This will return non zero (may be > 1) for scalar group if it is present
static inline int getNSGroupNumVectorByID(int rptGrpId)
{
  NSDL2_GDF(NULL, NULL, "Method Called, rptGrpId = %d", rptGrpId);
  switch(rptGrpId)
  {
    case TRANSDATA_RPT_GRP_ID:  // Overall tansaction data
      return 1;
    case TRANS_STATS_RPT_GRP_ID: 
      NSDL3_GDF(NULL, NULL, "total_tx_entries = %d", total_tx_entries);
      return total_tx_entries;

    case TRANS_CUM_STATS_RPT_GRP_ID: 
      if(global_settings->g_tx_cumulative_graph == 0)
        return 0;
      NSDL3_GDF(NULL, NULL, "total_tx_entries = %d", total_tx_entries);
      return total_tx_entries;
    
    case SRV_IP_STAT_GRP_ID:
      NSDL3_GDF(NULL, NULL, "total_normalized_svr_ips = %d", total_normalized_svr_ips);
      return total_normalized_svr_ips;

    case SERVER_STAT_RPT_GRP_ID:
      return no_of_host;
    case GRP_DATA_STAT_RPT_GRP_ID:
      NSDL3_GDF(NULL, NULL, "total_runprof_entries = %d", total_runprof_entries);
      return total_runprof_entries;
    case RBU_PAGE_STAT_GRP_ID:
      return g_rbu_num_pages;
    case PAGE_BASED_STAT_GRP_ID:
      return g_actual_num_pages ;
    
    case URL_FAIL_RPT_GRP_ID:
    case SMTP_FAIL_RPT_GRP_ID:
    case POP3_FAIL_RPT_GRP_ID:
    case FTP_FAIL_RPT_GRP_ID:
    case DNS_FAIL_RPT_GRP_ID:
    case LDAP_FAIL_RPT_GRP_ID:
    case IMAP_FAIL_RPT_GRP_ID:
    case JRMI_FAIL_RPT_GRP_ID:
    case WS_FAILURE_STATS_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      return TOTAL_USED_URL_ERR - 1 + 1;  // one less because 0th is of success. and plus one for (all) vector name.

    case PAGE_FAIL_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      // Substract URL not used error codes from total used page error
      return TOTAL_USED_PAGE_ERR - (TOTAL_URL_ERR - TOTAL_USED_URL_ERR) - 1  + 1; // one less because 0th is of success.

    case TRANS_FAIL_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      // Substract URL and Page not used error codes from total used tx error
      return TOTAL_TX_ERR - (TOTAL_URL_ERR - TOTAL_USED_URL_ERR) - (TOTAL_PAGE_ERR - TOTAL_USED_PAGE_ERR) - 1  + 1; // one less because 0th is of success.

    case SESSION_FAIL_RPT_GRP_ID:
      return TOTAL_SESS_ERR - 1  + 1; // one less because 0th is of success.

    case SHOW_VUSER_FLOW_STAT_GRP_ID:
      return total_flow_path_entries; // one less because 0th is of success.

    case RBU_DOMAIN_STAT_GRP_ID:
      NSDL3_GDF(NULL, NULL, "total_rbu_domain_entries = %d", total_rbu_domain_entries);
      return total_rbu_domain_entries; 

    case HTTP_STATUS_CODE_RPT_GRP_ID:
      return total_http_resp_code_entries;

    case TCP_CLIENT_FAILURES_RPT_GRP_ID:
      return g_total_tcp_client_errs;

    case UDP_CLIENT_FAILURES_RPT_GRP_ID:
      return g_total_udp_client_errs;
  }
  return 1; // All other groups are not dynamic and hence are to be included
}

//this function will return numVector for the particular graph id.
static inline int getGraphNumVectorByID(int groupid, int graphid)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  switch(groupid)
  {
    case URL_FAIL_RPT_GRP_ID:
    case SMTP_FAIL_RPT_GRP_ID:
    case POP3_FAIL_RPT_GRP_ID:
    case FTP_FAIL_RPT_GRP_ID:
    case DNS_FAIL_RPT_GRP_ID:
    case LDAP_FAIL_RPT_GRP_ID:
    case IMAP_FAIL_RPT_GRP_ID:
    case JRMI_FAIL_RPT_GRP_ID:
    case WS_FAILURE_STATS_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      return TOTAL_USED_URL_ERR - 1;  // one less because 0th is of success.

    case PAGE_FAIL_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      // Substract URL not used error codes from total used page error
      return TOTAL_USED_PAGE_ERR - (TOTAL_URL_ERR - TOTAL_USED_URL_ERR) - 1; // one less because 0th is of success.

    case TRANS_FAIL_RPT_GRP_ID:
      // We are using only used error codes in GDF.
      // Substract URL and Page not used error codes from total used tx error
      return TOTAL_TX_ERR - (TOTAL_URL_ERR - TOTAL_USED_URL_ERR) - (TOTAL_PAGE_ERR - TOTAL_USED_PAGE_ERR) - 1; // one less because 0th is of success.

    case SESSION_FAIL_RPT_GRP_ID:
      return TOTAL_SESS_ERR - 1; // one less because 0th is of success.

    default:
      return 0;
  }
}

// append the num vector lines in the output file.
static inline void append_vector_line(int num, char *buff[])
{
  int i;
  NSDL2_GDF(NULL, NULL, "Method Called");
  for(i = 0; i < num; i++){
    NSDL3_GDF(NULL, NULL, "vector buff = %s", buff[i]);
    fprintf(write_gdf_fp, "%s\n", buff[i]);
  }
}
/*
void getNSPrefix(char *prefix, int gen_entries, char*ang, int runprof_entries, int enable_group_data)
{
  if((enable_group_data == 0) || (loader_opcode == CLIENT_LOADER))
     prefix[0] = '\0';
  else
  {
    //this check is only check for no of sgrp entries and runprof entries
    if(runprof_entries == -1) {
      sprintf(prefix, "Overall%s", ang);
      NSDL2_GDF(NULL, NULL, "Overall%s", ang);
    }
    else
    {
      sprintf(prefix, "%s%s", RETRIEVE_BUFFER_DATA(runProfTable[runprof_entries].scen_group_name), ang);
      NSDL2_GDF(NULL, NULL, "Method Called, %s%s", RETRIEVE_BUFFER_DATA(runProfTable[runprof_entries].scen_group_name), ang);
    }
  }
}
*/

void getNCPrefix(char *prefix, int sgrp_gen_idx, int grp_idx, char*ang, int if_show_group_data_kw_enabled)
{
  NSDL2_GDF(NULL, NULL, "Method called, prefix = %s, sgrp_gen_idx = %d, ang = %s, grp_idx = %d, loader_opcode = %d", prefix, sgrp_gen_idx, ang, grp_idx, loader_opcode);

  //check show group data is disale
  //this is called only for used generatories entries and not for groups
  if(loader_opcode == MASTER_LOADER)
  {
    if(!if_show_group_data_kw_enabled)
    {
      if(sgrp_gen_idx == -1) {
        sprintf(prefix, "Controller>Overall%s", ang);
      }
      else {
        sprintf(prefix, "Controller>%s%s", generator_entry[sgrp_gen_idx].gen_name, ang);
      }
    }
    else
    {
      //this check for no of sgrp entries and runprof entries
      if(sgrp_gen_idx == -1) {
        if(grp_idx == -1) {
          sprintf(prefix, "Controller>Overall>Overall%s", ang);
        }
        else {
          sprintf(prefix, "Controller>Overall>%s%s", RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), ang);
        }
      }
      else if(grp_idx == -1)
      {
        sprintf(prefix, "Controller>%s>Overall%s", generator_entry[sgrp_gen_idx].gen_name, ang);
      }
      else 
      {
        sprintf(prefix, "Controller>%s>%s%s", generator_entry[sgrp_gen_idx].gen_name, RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), ang);
      }
    }
  }
  else 
  {
    //if((if_show_group_data_kw_enabled == 0) || (loader_opcode == CLIENT_LOADER))
    if(!if_show_group_data_kw_enabled)
      prefix[0] = '\0';
    else
    {
      //this check is only check for no of sgrp entries and runprof entries
      if(grp_idx == -1) {
        sprintf(prefix, "Overall%s", ang);
      }
      else
      {
        sprintf(prefix, "%s%s", RETRIEVE_BUFFER_DATA(runProfTable[grp_idx].scen_group_name), ang);
      }
    }
  }
  NSDL2_GDF(NULL, NULL, "Prefix = %s", prefix);
}

// print Vuser entries as vector lines in o/p file only in case of NC
static char **printInCaseOfNc()
{
  int i = 0, j = 0, Idx2d = 0;
  char **TwoD = NULL;
  char prefix[2048];

  TwoD = init_2d(TOTAL_ENTERIES);
  NSDL2_GDF(NULL, NULL, "Method Called, sgrp_used_genrator_entries = %d, total_runprof_entries = %d, TOTAL_ENTERIES = %d "
                        "TOTAL_GRP_ENTERIES_WITH_GRP_KW = %d", sgrp_used_genrator_entries, total_runprof_entries, TOTAL_ENTERIES, TOTAL_GRP_ENTERIES_WITH_GRP_KW);

  for(i = 0; i < sgrp_used_genrator_entries + 1; i++)
  {
    for(j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++)
    {
      getNCPrefix(prefix, i-1, j-1, "", SHOW_GRP_DATA);
      fprintf(write_gdf_fp, "%s\n", prefix);
      fill_2d(TwoD, Idx2d++, prefix);
    }
  } 
  return TwoD;
}


void printTranGraph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId)
{
  char buff[2048]; 
  char vector_name[1024];
  char *name;
  int i = 0, idx;
  int dyn_obj_idx, count;

  NSDL2_GDF(NULL, NULL, " Method called. Idx2d = %d, prefix = %s, genId = %d, groupId = %d", *Idx2d, prefix, genId, groupId);

  if(groupId == TRANS_STATS_RPT_GRP_ID)
  {
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_TX;
    count = dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].startId;
  }
  else if(groupId == TRANS_CUM_STATS_RPT_GRP_ID)
  {
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_TX_CUM;
    count = dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].total + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].startId;
  }

  for(i = 0; i < count; i++)
  {
    idx = i;
    if(genId == 0)
      name = nslb_get_norm_table_data(dynObjForGdf[dyn_obj_idx].normTable, i);
    else {
      idx = g_tx_loc2norm_table[genId-1].nvm_tx_loc2norm_table[i];   
      if(idx != -1) 
        name = nslb_get_norm_table_data(dynObjForGdf[dyn_obj_idx].normTable, idx);
    }

    if(g_runtime_flag == 0)
    {
      dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i] = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (*Idx2d));
      NSDL2_GDF(NULL, NULL, "RTG index set for NS/NC Controller/GeneratorId = %d, and TxnName = %s is %d. Index of DynObjForGdf = %d", genId, name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i], dyn_obj_idx);
    }

    if(idx != -1){
      sprintf(vector_name, "%s%s", prefix, name);
      sprintf(buff, "%s %d", vector_name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i]);
      fprintf(write_gdf_fp, "%s\n", buff);
      fill_2d(TwoD, *Idx2d, vector_name);
    }

    *Idx2d = *Idx2d  + 1;

    NSDL2_GDF(NULL, NULL, "Idx2d = %d", *Idx2d);
  }
}


// print only Transaction entries as vector lines in o/p file
static char **printTrans(int groupId)
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0, dyn_obj_idx = 0;

  NSDL2_GDF(NULL, NULL, "Method called, total_tx_entries = %d", total_tx_entries);

  int total_tx_entry = total_tx_entries * (sgrp_used_genrator_entries + 1);
  TwoD = init_2d(total_tx_entry);
  NSDL2_GDF(NULL, NULL, "Method Called");

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    NSDL2_GDF(NULL, NULL, "in trans prefix is = %s", prefix);
    printTranGraph(TwoD, &Idx2d, prefix, groupId, i);
  }
  if(groupId == TRANS_STATS_RPT_GRP_ID)
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_TX;
  else if(groupId == TRANS_CUM_STATS_RPT_GRP_ID)
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_TX_CUM;

  msg_data_size = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (sgrp_used_genrator_entries));

  return TwoD;
}

// print only Errors entries as vector lines in o/p file, 0 for URL, 1 for Page, 2 for Transaction, 3 for Session
static inline char **printErrors(int arg)
{
  char cmd[MAX_LINE_LENGTH];
  char temp[MAX_LINE_LENGTH + 1] = "";
  char buf[1000][1024];
  FILE *app = NULL;
  int i = 0;
  int j;
  char **TwoD = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called");
  sprintf(cmd, "%s/bin/nsu_get_errors %d %d %d %d", g_ns_wdir, arg, 0, 0, 1);

  /* http://www.lehman.cuny.edu/cgi-bin/man-cgi?popen+3
     The signal handler for SIGCHLD should be set to default when
     using  popen().  If  the  process  has  established a signal
     handler for SIGCHLD, it will be called when the command ter-
     minates.   If  the  signal  handler or another thread in the
     same process issues a wait(3C) call, it will interfere  with
     the  return  value  of  pclose().  If  the  process's signal
     handler for SIGCHLD has  been  set  to  ignore  the  signal,
     pclose() will fail and errno will be set to ECHILD. */

  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, SIG_DFL);
  //prev_handler = signal(SIGCHLD, SIG_IGN);

  app = popen(cmd, "r");

  if (app != NULL)
  {
    NSDL3_GDF(NULL, NULL, "popen the file");
    while (nslb_fgets(temp, 1024, app, 0) != NULL)
    {
      char *p = NULL;
      i++;
      if(i == 1)  // Skip first entry as this is for Success
        continue;
      NSDL3_GDF(NULL, NULL, "error = %s", temp) ;
      fprintf(write_gdf_fp, "%s", temp);
      p = strstr(temp, "\n");
      if (p) *p = '\0';
      strcpy(buf[i], temp);
    }

    TwoD = init_2d(i - 1);
    for (j = 0; j < i - 1; j++) {
      fill_2d(TwoD, j, buf[j + 2]);
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: popen failed for command %s. Error = %s\n", cmd, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1013007, cmd, nslb_strerror(errno));
  }

  if (app != NULL) 
  {
    if (pclose(app) == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: pclose failed for command nsu_get_errors. %s.\n", nslb_strerror(errno));
      NS_EXIT(-1, CAV_ERR_1013008, nslb_strerror(errno));
    }
  }

  (void) signal( SIGCHLD, prev_handler);


  return TwoD;
}

static inline char **printNetStormDiagVector()
{  
  int i = 0, Idx2d = 0;
  char **TwoD = NULL;
  char prefix[2048];

  TwoD = init_2d(TOTAL_ENTERIES);
  NSDL2_GDF(NULL, NULL, "Method Called, sgrp_used_genrator_entries = %d, TOTAL_ENTERIES = %d", sgrp_used_genrator_entries, TOTAL_ENTERIES);

  for(i = 0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, "", 0);
    fprintf(write_gdf_fp, "%s\n", prefix);
    fill_2d(TwoD, Idx2d++, prefix);
  }
  return TwoD;
}

static inline char **printGraphVector(int id, int numVector)
{
  char **TwoD = NULL;
 
  NSDL2_GDF(NULL, NULL, "Method Called");
  NSDL2_GDF(NULL, NULL, "in printGraphVector numVector = %d", numVector);
  switch(id)
  {
    case URL_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case SMTP_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case POP3_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case FTP_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case LDAP_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case IMAP_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case JRMI_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case DNS_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);
      break;
    case PAGE_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(1, numVector);
      break;
    case TRANS_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(2, numVector);
      break;
    case SESSION_FAIL_RPT_GRP_ID:
      TwoD = get_error_codes_ex(3, numVector);
      break;
    case WS_FAILURE_STATS_RPT_GRP_ID:
      TwoD = get_error_codes_ex(0, numVector);      //0 means total no of graphs is 29
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: This graph should not be vector\n rptid = %d", id);
      NS_EXIT(-1,CAV_ERR_1013009, id);
  }

  return TwoD;
}

static char **printCmServers(char *gdf_name, void *info_ptr)
{
  char **TwoD = NULL;
  //int cnt = 0;
  int i, j, no_of_monitors, mon_index, group_vector_index = 0;
  CM_info *local_cm_info = NULL;
  CM_vector_info *local_cm_vector_info = NULL;

  //TwoD = init_2d(cm_get_num_vectors(gdf_name));
  TwoD = init_2d(((CM_info *)info_ptr)->group_num_vectors);

  local_cm_info = (CM_info *)info_ptr;
  mon_index = local_cm_info->monitor_list_idx;
  no_of_monitors = monitor_list_ptr[mon_index].no_of_monitors;

  for (i = mon_index; i < mon_index + no_of_monitors; i++)
  {
    local_cm_info = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    local_cm_vector_info = local_cm_info->vector_list;
    for(j = 0; j <local_cm_info->total_vectors; j++)
    {
      fill_2d(TwoD, group_vector_index, local_cm_vector_info[j].vector_name);

      if(local_cm_info->flags & ALL_VECTOR_DELETED)
      {
        group_vector_index++;
        continue;
      }

      if(local_cm_vector_info[j].flags & RUNTIME_DELETED_VECTOR) //add minus sign before breadcrumb/vector name
      {
        if((global_settings->hierarchical_view) && (local_cm_vector_info[j].flags & MON_BREADCRUMB_SET))
          fprintf(write_gdf_fp, "#-%s %ld\n", local_cm_vector_info[j].mon_breadcrumb, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
        else
          fprintf(write_gdf_fp, "#-%s %ld\n", local_cm_vector_info[j].vector_name, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
      }
      else
      {
        if((global_settings->hierarchical_view) && (local_cm_vector_info[j].flags & MON_BREADCRUMB_SET))
          fprintf(write_gdf_fp, "%s %ld\n", local_cm_vector_info[j].mon_breadcrumb, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
        else
          fprintf(write_gdf_fp, "%s %ld\n", local_cm_vector_info[j].vector_name, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
      }
  
      group_vector_index++;
    }
  }

  return TwoD;
}

static char **create_hml_group_vector_list(int group_idx, char *gdf_name, void *info_ptr, int new_group)
{
  char **TwoD = NULL;
  int i, num_vectors;
  int j;
  int monitor = 0, mon_index = 0, no_of_monitors = 0 ,group_vector_index = 0;
  CM_vector_info *local_cm_vector_info = NULL;
  
  CM_info *local_cm_info = (CM_info *)info_ptr;
  Group_Info *group_ptr = &group_data_ptr[group_idx];

  TwoD = init_2d(((CM_info *)info_ptr)->group_num_vectors);
  
  mon_index = local_cm_info->monitor_list_idx;
  no_of_monitors = monitor_list_ptr[mon_index].no_of_monitors;
  
  for(monitor = 0 ; monitor < no_of_monitors ; monitor++) 
  {
    local_cm_info = monitor_list_ptr[mon_index + monitor].cm_info_mon_conn_ptr;

    num_vectors = local_cm_info->total_vectors; 
 
    NSDL2_GDF(NULL, NULL, "Method Called, group_idx = %d, gdf_name = %s, info_ptr = %p, num_vectors = %d, new_group = %d",
                        group_idx, gdf_name, info_ptr, num_vectors, new_group);
  
    for(i = 0; i < num_vectors; i++)
    {
      local_cm_vector_info = &(local_cm_info->vector_list[i]);
         
      fill_2d(TwoD, group_vector_index, local_cm_vector_info->vector_name);
    
      NSDL2_GDF(NULL, NULL, "i = %d, local_cm_info = %p, metric_priority = %d, GDF name = %s", 
                             i, local_cm_info, local_cm_info->metric_priority, local_cm_info->gdf_name);
      if(new_group && !(local_cm_info->flags & ALL_VECTOR_DELETED) && strncmp(local_cm_info->gdf_name, "NA", 2))
      {
        for(j = 0; j <= local_cm_info->metric_priority; j++)
          group_ptr->num_vectors[j]++;
    
      }
      group_vector_index++;
    }
  }
  return TwoD;
}

//static inline char **printGroupVector(int num, int id, char *gdf_name, void *info_ptr, int is_user_monitor)
static inline char **printGroupVector(int group_idx, int num, int id, char *gdf_name, void *info_ptr, int is_user_monitor, int new_group)
{
  char **TwoD = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called, num = %d, id = %d, gdf_name = %s, is_user_monitor = %d", num, id, gdf_name, is_user_monitor);

  switch(id)
  {
    case VUSER_RPT_GRP_ID:
    case SSL_RPT_GRP_ID:
    case URL_HITS_RPT_GRP_ID:
    case PG_DOWNLOAD_RPT_GRP_ID:
    case SESSION_RPT_GRP_ID:
    case TRANSDATA_RPT_GRP_ID:
    case SMTP_HITS_RPT_GRP_ID:
    case POP3_HITS_RPT_GRP_ID:
    case SMTP_NET_TROUGHPUT_RPT_GRP_ID:
    case POP3_NET_TROUGHPUT_RPT_GRP_ID:   
    case FTP_NET_TROUGHPUT_RPT_GRP_ID:
    case DNS_NET_TROUGHPUT_RPT_GRP_ID:
    case IMAP_NET_TROUGHPUT_RPT_GRP_ID:
  
    case FTP_HITS_RPT_GRP_ID:
    case DNS_HITS_RPT_GRP_ID:
    case DOS_ATTACK_RPT_GRP_ID:
    case HTTP_PROXY_RPT_GRP_ID:
    case HTTP_NETWORK_CACHE_RPT_GRP_ID:
    case DNS_LOOKUP_RPT_GRP_ID:
    case LDAP_NET_TROUGHPUT_RPT_GRP_ID:
    case LDAP_HITS_RPT_GRP_ID:
    case IMAP_HITS_RPT_GRP_ID:
    case JRMI_NET_TROUGHPUT_RPT_GRP_ID:
    case JRMI_HITS_RPT_GRP_ID:
    case HTTP_CACHING_RPT_GRP_ID:
    case WS_RPT_GRP_ID:
    case HTTP2_SERVER_PUSH_ID: /*bug 70480*/
    case WS_STATUS_CODES_RPT_GRP_ID:
    case XMPP_STAT_GRP_ID:
    case TCP_CLIENT_GRP_ID:
    case UDP_CLIENT_GRP_ID:
    case TCP_SERVER_GRP_ID:
    case UDP_SERVER_GRP_ID:
      //if (((loader_opcode == MASTER_LOADER) || (SHOW_GRP_DATA)) && !(loader_opcode == CLIENT_LOADER))
      if ((loader_opcode == MASTER_LOADER) || (SHOW_GRP_DATA))
        TwoD = printInCaseOfNc();
      break; 
      
    case URL_FAIL_RPT_GRP_ID:
    case PAGE_FAIL_RPT_GRP_ID:
    case SESSION_FAIL_RPT_GRP_ID:
    case TRANS_FAIL_RPT_GRP_ID:
    case SMTP_FAIL_RPT_GRP_ID:
    case POP3_FAIL_RPT_GRP_ID:
    case FTP_FAIL_RPT_GRP_ID:
    case LDAP_FAIL_RPT_GRP_ID:
    case JRMI_FAIL_RPT_GRP_ID:
    case DNS_FAIL_RPT_GRP_ID:
    case IMAP_FAIL_RPT_GRP_ID:
    case WS_FAILURE_STATS_RPT_GRP_ID:
      TwoD = printGraphVector(id, num);
      break;

    case NS_DIAGNOSIS_RPT_GRP_ID:
      TwoD = printNetStormDiagVector();
      break;
 
    case SERVER_STAT_RPT_GRP_ID:
      break;

    case TRANS_STATS_RPT_GRP_ID:
    case TRANS_CUM_STATS_RPT_GRP_ID:
      TwoD = printTrans(id);
      break;
 
    case GRP_DATA_STAT_RPT_GRP_ID:
      TwoD = printGroup();
      break;

    case RBU_PAGE_STAT_GRP_ID:
      TwoD = printRBUPageStat();
      break;

    case PAGE_BASED_STAT_GRP_ID:
      TwoD = printPageBasedStat();
      break;

    case RBU_DOMAIN_STAT_GRP_ID:
      TwoD = printRbuDomainStat(id);
      break;
    
    case SRV_IP_STAT_GRP_ID:
      TwoD = printSrvIpStat(id);
      break;

    case SHOW_VUSER_FLOW_STAT_GRP_ID:
      TwoD = printVuserFlowDataStat();
      break;

    case NS_HEALTH_MONITOR_GRP_ID:
      TwoD = printHMVectors();
      break;

    case HTTP_STATUS_CODE_RPT_GRP_ID:
      TwoD = print_resp_status_code_gdf_grp_vectors(id);
      break;

    case TCP_CLIENT_FAILURES_RPT_GRP_ID:
      TwoD = print_tcp_client_failures_grp_vectors(id);
      break;

    case UDP_CLIENT_FAILURES_RPT_GRP_ID:
      TwoD = print_udp_client_failures_grp_vectors(id);
      break;

    default:
      if(id < GDF_CM_START_RPT_GRP_ID)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: This group should not be vector\n rptid = %d", id);
        NS_EXIT(-1, CAV_ERR_1013010, id);
      }
      if(is_user_monitor && (loader_opcode == MASTER_LOADER))
      {
        TwoD = printInCaseOfNc();
        break; 
      }
      /*
      if ((id == SRV_IP_STAT_GRP_ID) && (loader_opcode != MASTER_LOADER)) {   
        TwoD = printSrvIpStat();
        break;
      }*/

      if ((id == IP_BASED_STAT_GRP_ID) && (loader_opcode != MASTER_LOADER)) {
        TwoD = printIpDataStat();
        break;
      }

      if(!IS_HML_APPLIED)
        TwoD = printCmServers(gdf_name, info_ptr);
      else
        TwoD = create_hml_group_vector_list(group_idx, gdf_name, info_ptr, new_group);
      break;
  }

  return TwoD;
}


static char group_graph_type_to_num(char *g_type)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(!strcmp(g_type, "scalar"))
    return ((char)GROUP_GRAPH_TYPE_SCALAR);
  else if(!strcmp(g_type, "vector"))
    return ((char)GROUP_GRAPH_TYPE_VECTOR);
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: groupType {%s} in GDF is not correct\n", g_type);
    NS_EXIT(-1, CAV_ERR_1013011, g_type);
  }
  return '0';
}

inline char data_type_to_num(char *d_type)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(!strcmp(d_type, "sample"))
    return (DATA_TYPE_SAMPLE);
  else if(!strcmp(d_type, "tdigest_4B"))
    return (DATA_TYPE_T_DIGEST);
  else if(!strcmp(d_type, "rate"))
    return (DATA_TYPE_RATE);
  else if(!strcmp(d_type, "cumulative"))
    return (DATA_TYPE_CUMULATIVE);
  else if(!strcmp(d_type, "times"))
    return (DATA_TYPE_TIMES);
  else if(!strcmp(d_type, "timesStd"))
    return (DATA_TYPE_TIMES_STD);
  else if(!strcmp(d_type, "sum"))
    return (DATA_TYPE_SUM);
  else if(!strcmp(d_type, "sum_4B"))
    return (DATA_TYPE_SUM_4B);
  else if(!strcmp(d_type, "sample_2B_100"))
    return (DATA_TYPE_SAMPLE_2B_100);
  else if(!strcmp(d_type, "sample_4B_1000"))
    return (DATA_TYPE_SAMPLE_4B_1000);
  else if (!strcmp(d_type, "sample_4B"))
    return (DATA_TYPE_SAMPLE_4B);
  else if (!strcmp(d_type, "sample_1B"))
    return (DATA_TYPE_SAMPLE_1B);
  else if(!strcmp(d_type, "rate_4B_1000"))
    return (DATA_TYPE_RATE_4B_1000);
  else if(!strcmp(d_type, "times_4B_1000"))
    return (DATA_TYPE_TIMES_4B_1000);
  else if(!strcmp(d_type, "sample_2B_100_count_4B"))
    return (DATA_TYPE_SAMPLE_2B_100_COUNT_4B);
  else if(!strcmp(d_type, "timesStd_4B_1000"))
    return (DATA_TYPE_TIMES_STD_4B_1000);
  else if(!strcmp(d_type, "times_4B_10"))
    return (DATA_TYPE_TIMES_4B_10);
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType {%s} in GDF is not correct\n", d_type);
    NS_EXIT(-1, CAV_ERR_1013001, d_type);
  }
  return '0';
}


static inline char formula_to_num(char *graph_formula)
{
  NSDL2_GDF(NULL, NULL, "Method Called");
  if(!strcmp(graph_formula, "SEC"))
    return (FORMULA_SEC);
  else if(!strcmp(graph_formula, "PM"))
    return (FORMULA_PM);
  else if(!strcmp(graph_formula, "PS"))
    return (FORMULA_PS);
  else if(!strcmp(graph_formula, "KBPS"))
    return (FORMULA_KBPS);
  else if(!strcmp(graph_formula, "DBH"))
    return (FORMULA_DBH);
  else
    return (-1);
}

char *num_to_formula(int fnum)
{
  static char formula[8];

  if(fnum == FORMULA_SEC)
    strcpy(formula, "SEC");
  else if (fnum == FORMULA_PM)
    strcpy(formula, "PM");
  else if (fnum == FORMULA_PS)
    strcpy(formula, "PS");
  else if (fnum == FORMULA_KBPS)
    strcpy(formula, "KBPS");
  else if (fnum == FORMULA_DBH)
    strcpy(formula, "DBH");

  return formula;
}


//This function sets Tier Hierarchy as found in gdf
static void set_breadcrumb_format(Group_Info *local_group_data_ptr, char *groupHierarchy)
{
  if(!strcmp(groupHierarchy, "Tier"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T;
  else if(!strcmp(groupHierarchy, "Tier>Server"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S;
  else if(!strcmp(groupHierarchy, "Tier>Server>App"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_APP;
  else if(!strcmp(groupHierarchy, "Tier>Server>Destination"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_DESTINATION;
  else if(!strcmp(groupHierarchy, "Tier>Server>Device"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_DEVICE;
  else if(!strcmp(groupHierarchy, "Tier>Server>DiskPartition"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_DISKPARTITION;
  else if(!strcmp(groupHierarchy, "Tier>Server>Global"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_GLOBAL;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>AppName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_APPNAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>CacheName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_CACHENAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>ClusterName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_CLUSTERNAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>Condition"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_CONDITION;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>Family"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_FAMILY;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>Method"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_METHOD;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>PoolName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_POOLNAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>ServletName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_SERVLETNAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Instance>ThreadPool"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INSTANCE_THREADPOOL;
  else if(!strcmp(groupHierarchy, "Tier>Server>Interface"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_INTERFACE;
  else if(!strcmp(groupHierarchy, "Tier>Server>PageName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_PAGENAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Processor"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_PROCESSOR;
  else if(!strcmp(groupHierarchy, "Tier>Server>QueueName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_QUEUENAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>ServiceName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_SERVICENAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>TopicName"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_TOPICNAME;
  else if(!strcmp(groupHierarchy, "Tier>Server>Total"))
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_T_S_TOTAL;
  else
    local_group_data_ptr->breadcrumb_format = BREADCRUMB_FORMAT_UNKNOWN;
}

void add_gdf_in_hash_table(char *gdf_name , int row_num)
{
 
  int ret = -1;
  ret = nslb_set_norm_id(g_gdf_hash, gdf_name, strlen(gdf_name), row_num);
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                          "ret from nslb_set_norm_id %d for gdf_name %s and row_num %d", ret, gdf_name, row_num);
  if(ret == row_num)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
	                     "both are equal ret = %d, row_num =%d flag %d", ret, row_num,g_gdf_processing_flag);
  }
  else 
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                    "else case");
}

static int fill_group_info(char *gdf_name , char *groupName, int rpGroupID, char *groupType, int numGraph, int numVector, char **vectorNamesPtr, char *groupHierarchy, char *group_description, char *Hierarchy, char *groupMetric)
{
  Group_Info *local_group_data_ptr = NULL;
  int row_num;
 
  NSDL2_GDF(NULL, NULL, "Method Called, groupName = %s, rpGroupID = %d, groupType = %s, numGraph = %d, "
                        "numVector = %d, groupHierarchy = %s", 
                         groupName, rpGroupID, groupType, numGraph, numVector, groupHierarchy);

  if(create_table_entry(&row_num, &total_groups_entries, &max_groups_entries, (char **)&group_data_ptr, sizeof(Group_Info), "Group Data") == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not create table entry for Group '%s' and Group Id '%d'", groupName, rpGroupID);
    NS_EXIT(-1,CAV_ERR_1013012);
  }

  local_group_data_ptr = &group_data_ptr[row_num];
  memset(local_group_data_ptr, 0, sizeof(Group_Info));
 
  NSDL3_GDF(NULL, NULL, "row_num = %d, groupName = %s, group_count = %d, numVector = %d", 
                         row_num, groupName, group_count, numVector);

  MY_MALLOC(local_group_data_ptr->group_name, strlen(groupName) + 1, "Group Name", -1);
  strcpy(local_group_data_ptr->group_name, groupName);

  //MY_MALLOC(local_group_data_ptr->hml_relative_rtg_idx, numGraph * sizeof(unsigned char), "Group Name", -1);

  local_group_data_ptr->rpt_grp_id = rpGroupID;

  local_group_data_ptr->group_type = group_graph_type_to_num(groupType); // To parse the GDF and set Group Structure with group type as #define variable
  ;
  local_group_data_ptr->num_graphs = numGraph;
  local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] = numVector;
  //local_group_data_ptr->graph_info_index[MAX_METRIC_PRIORITY_LEVELS] = graph_count_mem;
  local_group_data_ptr->graph_info_index = graph_count_mem;
  local_group_data_ptr->vector_names = vectorNamesPtr;
  local_group_data_ptr->mg_gid = -1;

  if(groupHierarchy != NULL)
  {
    //Setting Teired hierarchy format as found in gdf
    set_breadcrumb_format(local_group_data_ptr, groupHierarchy);
  }

  if(group_description)
    MALLOC_AND_COPY (group_description, local_group_data_ptr->group_description, strlen(group_description) + 1, "GDF Group group_description", -1);
  if(Hierarchy)
    MALLOC_AND_COPY (Hierarchy, local_group_data_ptr->Hierarchy, strlen(Hierarchy) + 1, "GDF Group Hierarchy", -1);
  if(groupMetric)
    MALLOC_AND_COPY (groupMetric, local_group_data_ptr->groupMetric, strlen(groupMetric) + 1, "GDF Group groupMetric", -1);

  if (local_group_data_ptr->group_type == GROUP_GRAPH_TYPE_VECTOR)
  {
    if (!local_group_data_ptr->excluded_graph)
    {
      MY_MALLOC(local_group_data_ptr->excluded_graph, local_group_data_ptr->num_graphs * sizeof(char), "excluded_graph_of_group", -1);
    }
    memset(local_group_data_ptr->excluded_graph, '0', local_group_data_ptr->num_graphs * sizeof(char));
  }
 
  if((g_tsdb_configuration_flag & TSDB_MODE))
    add_gdf_in_hash_table(gdf_name, row_num);
 
  NSDL3_GDF(NULL, NULL, "GroupInfo: id = %d, group_name = %s, rpt_grp_id = %d, group_type = %d, "
                        "num_graphs = %d, num_vectors = %d, graph_info_index = %d", 
                         row_num, local_group_data_ptr->group_name, 
                         local_group_data_ptr->rpt_grp_id, local_group_data_ptr->group_type, 
                         local_group_data_ptr->num_graphs, local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], 
                         local_group_data_ptr->graph_info_index);
  return row_num;
}

/**
 * Function to fill the Graph_Data struct
 */
char ***fill_graph_data(int group_num_vectors, int graph_num_vectors) 
{
  int i, j;
  char ***three_d = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called, group_num_vectors = %d, graph_num_vectors = %d", group_num_vectors, graph_num_vectors);
  //NSTL1_OUT(NULL, NULL, "fill_graph_data(), group_num_vectors = %d, graph_num_vectors = %d\n", group_num_vectors, graph_num_vectors);

  MY_MALLOC(three_d, group_num_vectors * sizeof(int **), "three_d for group_num_vectors", group_num_vectors);
  for (i = 0; i < group_num_vectors; i++) {
    MY_MALLOC (three_d[i], graph_num_vectors * sizeof(int *), "three_d[i]", i);
    for (j = 0; j < graph_num_vectors; j++) {
      MY_MALLOC (three_d[i][j], sizeof(Graph_Data), "three_d[i][j] for j", j);
      init_graph_data_node(((Graph_Data *)three_d[i][j]));
    }
  }
  return three_d;
}


static inline void fill_graph_info(char *graphName, int rpGraphID, char *graphType, char *dataType, char *graphFormula,
                                   int numVector, int groupNumVector, char **vectorNamesPtr,
                                   int pdf_info_idx, int create_st_or_not, int rtg_idx, char *line, char exclude_or_not,
                                   int group_idx, void *info_ptr, char *mpriority, char *discription, 
                                   int pdf_id, int graph_seq, int is_user_monitor, int rpGroupID, char *derived_field)
{
  int c_graph;
  Graph_Info *local_graph_data_ptr = NULL;
  int row_num;
  int hml_idx;
  int gsize;
 
  NSDL2_GDF(NULL, NULL, "Method called, graphName = %s, rpGraphID = %d, numVector = %d", 
                         graphName, rpGraphID, numVector);

  for(c_graph = 0; c_graph < numVector; c_graph++)
  {
    //NSTL1_OUT(NULL, NULL, "\n ***** c_graph = [%d], numVector = [%d] ***\n", c_graph, numVector);
    if(create_table_entry(&row_num, &total_graphs_entries, &max_graphs_entries, (char **)&graph_data_ptr, sizeof(Graph_Info), "Graph Data") == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not create table entry for Graph_Info");
      NS_EXIT(-1,CAV_ERR_1013013);
    }

    local_graph_data_ptr = graph_data_ptr + row_num;
    memset(local_graph_data_ptr, 0, sizeof(Graph_Info));

    NSDL2_GDF(NULL, NULL, "cm_index=%u", cm_index);
    local_graph_data_ptr->cm_index = cm_index;

    NSDL3_GDF(NULL, NULL, "row_num = %d, graphName = %s, graph_count_mem = %d, derived_field = %s" , row_num, graphName, graph_count_mem, derived_field);

    // FREE_AND_MAKE_NULL (local_graph_data_ptr->graph_name, "Graph Name", c_graph);
    MY_MALLOC (local_graph_data_ptr->graph_name, strlen(graphName) + 1, "Graph Name", c_graph);
    MY_MALLOC (local_graph_data_ptr->graph_discription, strlen(discription) + 1, "Graph discription", c_graph);
    MY_MALLOC (local_graph_data_ptr->derived_formula, strlen(derived_field) + 1, "Graph derived formula", c_graph);
   
    if(line)
      MALLOC_AND_COPY (line, local_graph_data_ptr->gline, strlen(line) + 1, "Graph line", -1);

    strcpy(local_graph_data_ptr->graph_name, graphName);
    strcpy(local_graph_data_ptr->graph_discription, discription);
    strcpy(local_graph_data_ptr->derived_formula, derived_field);

    local_graph_data_ptr->rpt_id = rpGraphID;

    local_graph_data_ptr->graph_type = group_graph_type_to_num(graphType); // To parse the GDF and set Group Structure with group type as #define variable

    if(!IS_HML_APPLIED)
    {
      hml_idx = local_graph_data_ptr->metric_priority = MAX_METRIC_PRIORITY_LEVELS;
    }
    else
    {
      local_graph_data_ptr->metric_priority = get_metric_priority_id(mpriority, !info_ptr?1:0);
      hml_idx = local_graph_data_ptr->metric_priority;
    }
  

    if((g_runtime_flag) && (create_st_or_not == 0))
      local_graph_data_ptr->graph_msg_index = rtg_idx;
    else
      local_graph_data_ptr->graph_msg_index = msg_data_size;

    local_graph_data_ptr->data_type = data_type_to_num(dataType);
    local_graph_data_ptr->formula = formula_to_num(graphFormula);
    local_graph_data_ptr->num_vectors = numVector;
    local_graph_data_ptr->vector_names = vectorNamesPtr;
    local_graph_data_ptr->graph_data = fill_graph_data(groupNumVector, numVector);
    local_graph_data_ptr->graph_state = exclude_or_not;

    //Fill pdf related info
    local_graph_data_ptr->pdf_id = pdf_id;
    local_graph_data_ptr->pdf_info_idx = pdf_info_idx;
    local_graph_data_ptr->pdf_data_index  = (pdf_id == -1) ? -1 : 
                                            ((rpGroupID != TRANS_STATS_RPT_GRP_ID)?total_pdf_data_size_8B:tx_total_pdf_data_size_8B);

    NSDL3_GDF(NULL, NULL, "hml_idx = %d, exclude_or_not = %d, group_idx = %d, graph_seq = %d", 
                           hml_idx, exclude_or_not, group_idx, graph_seq);
    if(exclude_or_not != GRAPH_EXCLUDED)
    {
      group_data_ptr[group_idx].num_actual_graphs[hml_idx]++;
      // msg_data_size should be increse by the (no of vector). we have to allocate buffer of a size include all vctor entries.
      gsize = getSizeOfGraph(local_graph_data_ptr->data_type, &element_count);
      group_data_ptr[group_idx].tot_hml_graph_size[hml_idx] += gsize;

    }
    else
      group_data_ptr[group_idx].excluded_graph[graph_seq] = GRAPH_EXCLUDED; 

    NSDL2_GDF(NULL, NULL, "GraphInfo: row_num = %d, Graph name= %s, ID= %d, Graph Type= %d, Msg Index (%d)= %lu, "
                          "Data Type= %d\t Formula= %d\t numVector= %d, pdf_id = %d, pdf_info_idx = %d, pdf_data_index = %u, "
                          "hml_msg_data_size[hml_idx][HML_MSG_DATA_SIZE] = %ld, hml_msg_data_size[hml_idx][HML_START_RTG_IDX] = %ld, "
                          "graph_info_index = %d", 
                           row_num, local_graph_data_ptr->graph_name, local_graph_data_ptr->rpt_id, local_graph_data_ptr->graph_type, 
                           hml_idx, local_graph_data_ptr->graph_msg_index, local_graph_data_ptr->data_type, 
                           local_graph_data_ptr->formula, local_graph_data_ptr->num_vectors, local_graph_data_ptr->pdf_id, 
                           local_graph_data_ptr->pdf_info_idx, local_graph_data_ptr->pdf_data_index, 
                           hml_msg_data_size[hml_idx][HML_MSG_DATA_SIZE], hml_msg_data_size[hml_idx][HML_START_RTG_IDX],
                           group_data_ptr[group_idx].graph_info_index);

    graph_count_mem++;
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
  Name          :  update_pdf_size()
  Purpose       :  This function will increase PDF pcaket size.
                   => PDF data size will increase on processing of Graph ONLY when
                      this graph is taking participation in Perentile Graph
                   => In case of Dynamic transactions, PDF packet size will 
                      increase ONLY when NVM/Generator inform Parent/Controller
  PDF Size Cal  : How PDF packet size calculated let have a look -
                  By default percentile graph is supported for followings -
                    1. URL          ==> Number vectors = 1
                    2. Page         ==> Number vectors = 1
                    3. Session      ==> Number vectors = 1
                    4. Tx(Overall)  ==> Number vectors = 1
                    5. Tx(Each)     ==> Number vectors = No. static tx (Say X)

                    Default number of Granules = (max/min + 1) = 1001

                    Size of One bucket = 4 (On Child)
                                         8 (On Parent) 

                    ==> Since pctMessage.dat made by parent so packet size
                        will calculate by Parent bucket size 

                    Packet Header Size = sizeof(pdf_data_hdr) = 32
                    
                    PDF packet size initial = 32 + ((4 + X) * 1001 * 8)
                    
                    PDF packet size on N dynamic Tx -
                    If N == 1 
                       Initial PDF packet size + ((N+16) * 1001 * 8)
                       
                    If N < 16 
                       Initial PDF packet size + (N * 1001 * 8)

                    ==> In case of dynamic tx, memory is grow by 16 entries 

  Input         : @numVector         => Number of Vectors of that Graph
                  @num_granules      => Number of Granules/Buckets
                  @group_num_vectors => Number of Vectors of that Group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static inline void update_pdf_size(int numVector, int num_granules, int group_num_vectors) 
{ 
  NSDL3_GDF(NULL, NULL, "Method Called. total_pdf_data_size = %d, total_pdf_data_size_8B = %d, "
                        "numVector = %d, num_granules = %d, group_num_vectors = %d", 
                         total_pdf_data_size, total_pdf_data_size_8B, numVector, num_granules, group_num_vectors);

  // In case of conrtroller all the graphs become vector,so numVector is changed, but prcentile data packet size will not be changed
  // so we are are dividing numVector by sgrp_used_genrator_entries+1 to get its orignal size
  if(loader_opcode == MASTER_LOADER) {
    NSDL3_GDF(NULL, NULL, "Master Loader case");
    numVector = numVector/(sgrp_used_genrator_entries + 1);
  }
  NSDL3_GDF(NULL, NULL, "numVector = %d", numVector);
  total_pdf_data_size = total_pdf_data_size + ((numVector ? numVector : 1) * (num_granules * sizeof(Pdf_Data))/*  * */
/*                                                  group_num_vectors */);
  total_pdf_data_size_8B = total_pdf_data_size_8B + ((numVector ? numVector : 1) * (num_granules * sizeof(Pdf_Data_8B)));
  NSDL3_GDF(NULL, NULL, "total_pdf_data_size = %d, total_pdf_data_size_8B = %d", total_pdf_data_size, total_pdf_data_size_8B);
   
  NSDL3_GDF(NULL, NULL, "pdf_average_url_response_time  = %d\npdf_average_smtp_response_time  = %d\n"
             "pdf_average_pop3_response_time  = %d\npdf_average_ftp_response_time  = %d\n"
             "pdf_average_dns_response_time  = %d\npdf_average_page_response_time = %d\n"
             "pdf_average_session_response_time = %d\npdf_average_transaction_response_time = %d\n"
             "pdf_transaction_time = %d",
             pdf_average_url_response_time, pdf_average_smtp_response_time, pdf_average_pop3_response_time,
             pdf_average_ftp_response_time, pdf_average_dns_response_time, pdf_average_page_response_time,
             pdf_average_session_response_time, pdf_average_transaction_response_time, pdf_transaction_time);

  NSDL3_GDF(NULL, NULL, "total_pdf_data_size is now = %d, total_pdf_data_size_8B = %d", total_pdf_data_size, total_pdf_data_size_8B);
}

/*********************************************************************
 Name:        save_pdf_data_index
 Purpose:     Whenever dynamic Tx is added, total_pdf_data_size_8B 
              and total_pdf_data_size increaded by percentile code
              It cause issue to get correct tranaction index. 
              To resolve this issue saving store index of first Tx 

                                               PDF Shm 
               pdf_lookup_tabl                 ____________
                _________                     |   URL      |
               |__Url____|                    |____________|
               |__Page___|                    |   Page     |
               |__Sess___|                    |____________|
               |__Tx_____|                    |   Session  |
               |_________|                    |____________|
               |_________|                    | Tx(Overall)|
               |_________|                    |____________|
               |_________|                    |   Tx1      |
               |_________|                    |____________|
                                              |   Tx2      |
                                              |____________|


           
           
 Author/Date: Manish/Tue Aug 22 16:29:35 IST 2017 
*********************************************************************/
static inline void save_pdf_data_index(int rpGroupID)
{
  NSDL3_GDF(NULL, NULL, "Method called, rpGroupID = %d, g_runtime_flag = %d, total_pdf_data_size_8B = %d, total_pdf_data_size = %d", 
                         rpGroupID, g_runtime_flag, total_pdf_data_size_8B, total_pdf_data_size);
  if((rpGroupID == TRANS_STATS_RPT_GRP_ID) && (tx_total_pdf_data_size_8B == -1))
  {
    tx_total_pdf_data_size_8B = total_pdf_data_size_8B; 
    tx_total_pdf_data_size = total_pdf_data_size;
  }
}


//  process graph line and print it into temp file , will return the size of the Graph using data type.

static inline int process_graph_line(char *line, int rpGroupID, int group_num_vectors, int create_st_or_not, int rtg_idx, int group_idx, void *info_ptr, int graph_seq, int is_user_monitor)
{
  int rpGraphID = 0;
  char exclude_or_not;		//stores whether this graph is to be shown or not
  char graphName[MAX_LINE_LENGTH];
  char graphType[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  char printGraphLine[MAX_LINE_LENGTH];
  char dataType[MAX_LINE_LENGTH];
  char graphFormula[MAX_LINE_LENGTH];
  char derived_formula[MAX_LINE_LENGTH];
  int numVector = 0;
  int size = 0;
  int i = 0;
  char graphline[MAX_LINE_LENGTH];
  char **vectorNamesPtr = NULL;
  int um_group;
  int num_granules = 0;
  int row_num = -1; /* must be -1 default */
  int pdf_id = -1; // Must be initialized with -1 as it is checked for pdf data size
  char graph_state[16];  // possible values AS/I/A/NA/E

  NSDL2_GDF(NULL, NULL, "Method called, rpGroupID = %d, group_num_vectors = %d, create_st_or_not = %d, rtg_idx = %d", 
                         rpGroupID, group_num_vectors, create_st_or_not, rtg_idx);
  strcpy(graphline, line);

  i = get_tokens(graphline, buffer, "|", GDF_MAX_FIELDS);

  if(i != 14)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: No. of field in Graph line is not 14. Line = %s\n", line); 
    NS_EXIT(-1, CAV_ERR_1013014, line);
  }

  //TODO - Clean code to remove check for i == 14 in this method

  /**
   * Graph line can be of following types:
   *
   * Graph|Average URL Response Time (Secs)|3|scalar|times|-|SEC|0
   *
   * Graph|graphName|rptId|graphType|dataType|graphDataIndex|formula|numVectors|Future|pdf_id|Percentile_Data_Idx|future1|future2|Graph_Description
   *
   * Graph|Running Vusers|1|scalar|sample|-|-|0|-1|-1|NA|NA|NA
   *
   */

  strcpy(graphName, buffer[1]);
  rpGraphID = atoi(buffer[2]); // Graph ID
  strcpy(graphType, buffer[3]); // Graph type
  strcpy(dataType, buffer[4]); // Data type
  strcpy(graphFormula, buffer[6]);
  strcpy(derived_formula,buffer[12]); //Derived Graph Formula

  //--------------------------fill exclude here
  strcpy(graph_state, "-");

  if(buffer[8])
    strcpy(graph_state, buffer[8]);  // Graph State

  exclude_or_not = is_graph_excluded(buffer[11], graph_state, -1);

  NSDL2_GDF(NULL, NULL, "rpGraphID = %d, exclude_or_not = %d", rpGraphID, exclude_or_not);

  if(g_runtime_flag == 0)
  {
    #ifdef SKIP_NC_GRAPH
    if ((rpGroupID == TRANS_STATS_RPT_GRP_ID) && NS_TX_NETCACHE_GRAPH_IDX && !(global_settings->protocol_enabled & NETWORK_CACHE_STATS_ENABLED)) 
    {
      NSDL2_GDF(NULL, NULL, "sizeof(Long_data) = %d Network cache stats is not enabled. Hence cannot show data of graph Transaction NetCache Hits.", sizeof(Long_data));
      //Incrementing msg_data_size for netcache_pct in case netcache is disabled. 
      //Because we are putting netcache_pct in structure Trans_stats_gp.

      msg_data_size = (msg_data_size + sizeof(Long_data));
      return sizeof(Long_data);
    }
    #endif
  }
 

  pdf_id = atoi(buffer[9]);

  NSDL3_GDF(NULL, NULL, "rpGraphID = %d, graphType = %s, dataType = %s, pdf_id = %d, g_runtime_flag = %d", 
                         rpGraphID, graphType, dataType, pdf_id, g_runtime_flag);

  if(g_runtime_flag == 0)
  {
    if ((um_group = check_if_user_monitor(rpGroupID)) != -1) {
      fill_um_graph_info(graphName, um_group, rpGraphID, data_type_to_num(dataType));
    }
  }

  if(!strcmp(graphType, "vector"))      // graph may be vector or scalar
  {
    NSDL3_GDF(NULL, NULL, " vector rpGroupID = %d", rpGroupID);
    numVector = getGraphNumVectorByID(rpGroupID, rpGraphID);  //get numVector
    if(numVector == 0)
      return 0;   // if numVector is 0 , the line graph should not come in o/p file
    
    /** Save pdf data index **/
    save_pdf_data_index(rpGroupID);

    if(g_runtime_flag == 0)
      size = msg_data_size + group_data_ptr[group_idx].tot_hml_graph_size[MAX_METRIC_PRIORITY_LEVELS];
    else
      size = -1;

    num_granules = chk_and_add_in_pdf_lookup_table(buffer[13], &pdf_id, &row_num, rpGroupID, rpGraphID, g_runtime_flag);

    if(!IS_HML_APPLIED)
    {
      /* is custom monitor and not user monitor */
      if (rpGroupID >= GDF_CM_START_RPT_GRP_ID && check_if_user_monitor(rpGroupID) == -1) {
        sprintf(printGraphLine,"%s|%s|%s|%s|%s|%d|%s|%d|%s|%d|%d|%s|%s|%s", 
                buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], size, buffer[6], numVector, graph_state,
                pdf_id, -1, 
                buffer[11], buffer[12], buffer[13]);  // making graph line
      } else {
        sprintf(printGraphLine,"%s|%s|%s|%s|%s|%d|%s|%d|%s|%d|%lld|%s|%s|%s", 
                buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], size, buffer[6], numVector, graph_state,
                pdf_id, (pdf_id == -1) ? -1 : ((rpGroupID != TRANS_STATS_RPT_GRP_ID)?total_pdf_data_size_8B:tx_total_pdf_data_size_8B),
                buffer[11], buffer[12], buffer[13]);  // making graph line
      }
    
      NSDL3_GDF(NULL, NULL, "printGraphLine = %s", printGraphLine);
      if(exclude_or_not != GRAPH_EXCLUDED)
      {
        fprintf(write_gdf_fp,"%s\n", printGraphLine);
      }
    }
    else
      NSDL3_GDF(NULL, NULL, "Graph is vector, HML group featue is enable");

    if((g_runtime_flag == 0) || ((g_runtime_flag) && (create_st_or_not == 0)))
    {     
      vectorNamesPtr = printGraphVector(rpGroupID, numVector);                  // call for printing vector graph line

      //Atul - we have to take care of vectors in memory because we are filling data in min. max for all vector groups and graphs
      fill_graph_info(graphName, rpGraphID, graphType, dataType, graphFormula, numVector, 
                    group_num_vectors, vectorNamesPtr, row_num, create_st_or_not, rtg_idx, line, exclude_or_not, group_idx, info_ptr, 
                    buffer[11], buffer[13], pdf_id, graph_seq, is_user_monitor, rpGroupID, derived_formula);
    }
  }
  else
  {
    NSDL3_GDF(NULL, NULL, "Graph is scalar");

    /** Save pdf data index **/
    save_pdf_data_index(rpGroupID);

    if(g_runtime_flag == 0)
      size = msg_data_size + group_data_ptr[group_idx].tot_hml_graph_size[MAX_METRIC_PRIORITY_LEVELS];//condition would be for hml :ABHIHML
    else
      size = -1;

    num_granules = chk_and_add_in_pdf_lookup_table(buffer[13], &pdf_id, &row_num, rpGroupID, rpGraphID, g_runtime_flag);

    if(!IS_HML_APPLIED)
    {
      /* is custom monitor and not user monitor */ //we are not going to solve this warning
      if (rpGroupID >= GDF_CM_START_RPT_GRP_ID && check_if_user_monitor(rpGroupID) == -1) {
        sprintf(printGraphLine,"%s|%s|%s|%s|%s|%d|%s|%d|%s|%d|%d|%s|%s|%s", 
                buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], size, buffer[6], numVector, graph_state,
                pdf_id, -1, 
                buffer[11], buffer[12], buffer[13]);  // making graph line
      } else {
        /* is custom monitor and not user monitor */
        sprintf(printGraphLine,"%s|%s|%s|%s|%s|%d|%s|%d|%s|%d|%lld|%s|%s|%s", 
                buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], size, buffer[6], numVector, graph_state,
                pdf_id, (pdf_id == -1) ? -1 : ((rpGroupID != TRANS_STATS_RPT_GRP_ID)?total_pdf_data_size_8B:tx_total_pdf_data_size_8B),
                buffer[11], buffer[12], buffer[13]);  // making graph line
      }
     
      NSDL3_GDF(NULL, NULL, "printGraphLine = %s http_mode = %d", printGraphLine, group_default_settings->http_settings.http_mode);
      if(exclude_or_not != GRAPH_EXCLUDED)
      {
        /*bug 92383: avoid HTTP2 stats Metrics on GUI for HTTP protocol*/
        if(NS_AVOID_HTTP2_METRICS_FOR_HTTP1(printGraphLine)) {
          NSDL3_GDF(NULL, NULL, "Write data");
          fprintf(write_gdf_fp,"%s\n", printGraphLine);
        }
      }
    }
    else
      NSDL3_GDF(NULL, NULL, "Graph is scalar, HML group featue is enable");

    if((g_runtime_flag == 0) || ((g_runtime_flag) && (create_st_or_not == 0)))
    {
      //It will increment the total graph count for the first time when test is started or for a new monitor group added at runtime.
      fill_graph_info(graphName, rpGraphID, graphType, dataType, graphFormula, 1, group_num_vectors, 
                    NULL, row_num, create_st_or_not, rtg_idx, line, exclude_or_not, group_idx, info_ptr, 
                    buffer[11], buffer[13], pdf_id, graph_seq, is_user_monitor, rpGroupID, derived_formula);
    }
  }

  /*=================================================================
    Bug: 93909 - Core dump in percentile module
    RCA: total_pdf_data_size keep increases on every partition switch
         even though NO DYNAMIC Tx found. 
    Action: Adding check of g_rtg_rewrite to resolve bug 93909.
  =================================================================*/
  if ((pdf_id != -1) && (g_runtime_flag == 0) && (g_rtg_rewrite == 0))
  {
    update_pdf_size(numVector, num_granules, group_num_vectors); 
  }
  return size;
}

#if 0
/*this function is basically for processing the vector groups. in case of vector group we have to print the Group once in GDF. but have to store the graph info for all vectors in Graph_Info buffer. so to store these information we are using this function where we are running the loop for groupNumVector - 1 beacuse first vector group is already processed in process_grap_line function and then rest of vector group will be processed and stored in memory in this function.
for static group groupNumVector should be 1 and loop will not run for that.for vector group if groupNumVector = 0 and also loop will not run.*/
void process_rest_graph(int rpGroupID, int groupNumVector, char *buffer[], int num_line)
{
  int i = 0, j, count = 0;
  char *graph_words[GDF_MAX_FIELDS];
  int numVector = 0;
  int rpGraphID = 0;
  char line[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called");
  for(j = 0; j < (groupNumVector - 1); j++)
  {
    for(count = 0; count < (num_line); count++)
    {
      strcpy(line, buffer[count]);
      i = get_tokens(line, graph_words, "|", GDF_MAX_FIELDS);
      rpGraphID = atoi(graph_words[2]);
      numVector = getGraphNumVectorByID(rpGroupID, rpGraphID);  //get numVector
      if(numVector == 0)
        fill_graph_info(graph_words[1], rpGraphID, graph_words[3], graph_words[4], graph_words[6], 1, groupNumVector, NULL, -1 /* TODO */);
      else
        fill_graph_info(graph_words[1], rpGraphID, graph_words[3], graph_words[4], graph_words[6], numVector, groupNumVector, NULL, -1 /* TODO */);
    }
  }
}
#endif


// read the graph line from the file, add the size of all graph line and return size.
static inline int process_graph(FILE *read_fp, int numGraph, int rpGroupID, int groupNumVector, int create_st_or_not, long rtg_idx, int group_idx, void *info_ptr, int is_user_monitor)
{
  int i = 0, j;
  int size = 0;
  char graph_line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called, rpGroupID = %d, numGraph = %d, groupNumVector = %d, "
                        "create_st_or_not = %d, rtg_idx = %d, size = %d, msg_data_size = %d, group_idx = %d", 
                        rpGroupID, numGraph, groupNumVector, create_st_or_not, rtg_idx, size, msg_data_size, group_idx);

  if (numGraph > GDF_MAX_GRAPH_LINE )
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Number of Graphs (%d) in gdf is more than %d.\nExiting..\n", numGraph, GDF_MAX_GRAPH_LINE);
     NS_EXIT(-1, CAV_ERR_1013015, numGraph, GDF_MAX_GRAPH_LINE);
  }

  //Reset 
  for(j = 0; j < MAX_METRIC_PRIORITY_LEVELS; j++)
  {
    last_HML_graph_idx[j] = -1;
    hml_relative_idx[j] = 0;
  }

  while (nslb_fgets(graph_line, MAX_LINE_LENGTH, read_fp, 0) != NULL)
  {
    graph_line[strlen(graph_line) - 1] = '\0';

    if(graph_line[0] == '#' || graph_line[0] == '\0')  // blank line
      continue;

    if(sscanf(graph_line, "%s", tmpbuff) == -1)  // for blank line with some spaces.
      continue;

    if(!(strncasecmp(graph_line, "graph|", strlen("graph|"))))
    {
      NSDL3_GDF(NULL, NULL, "processing graph line = %s", graph_line);

      //size = size + process_graph_line(graph_line, rpGroupID, groupNumVector, create_st_or_not, rtg_idx);
      size += process_graph_line(graph_line, rpGroupID, groupNumVector, create_st_or_not, rtg_idx, group_idx, info_ptr, i, is_user_monitor);
      //NSDL3_GDF(NULL, NULL, "After processing new msg_data_size = %d", size);

/*       //we are saving all graph line is buffer so that we can use it to fill Graph info buffer for each vector group. we are doing this because we have to print the vector group once in GDF and have to process each vector group and saved their values in memory. */
/*       MY_MALLOC (buffer[i], strlen(graph_line) + 1, "Graph Line Buffer", i); */
/*       strcpy(buffer[i], graph_line); */
      i++;
      if(i >= numGraph)
        break;
      else
        continue;

     } 
    else
    {
      
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Invalid line Between Graphs lines {%s} in GDF ID %d \n", graph_line, rpGroupID);       
      NS_EXIT(-1, CAV_ERR_1013016, graph_line, rpGroupID);
    }

    //Set relative Graph index 
  }
  
  if( i != numGraph)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: No.of graphs %d not equals to total no. of graphs %d defined in GDF ID %d.\n",
            i, numGraph, rpGroupID);
    NS_EXIT(-1, CAV_ERR_1013017, i, numGraph, rpGroupID);
  }

  if(!create_st_or_not)
  {
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Graph processing complete for Group[%d]: Group Name = %s, rpGroupID = %d, i = %d, "
                      "tot_hml_graph_size[H,M,L,D] = [%hd,%hd,%hd,%hd], "
                      "num_vectors[H,M,L,D] = [%d,%d,%d,%d], msg_data_size = %d, Is New Group = %d",
                       group_idx, group_data_ptr[group_idx].group_name, rpGroupID, i,
                       group_data_ptr[group_idx].tot_hml_graph_size[METRIC_PRIORITY_HIGH],
                       group_data_ptr[group_idx].tot_hml_graph_size[METRIC_PRIORITY_MEDIUM],
                       group_data_ptr[group_idx].tot_hml_graph_size[METRIC_PRIORITY_LOW],
                       group_data_ptr[group_idx].tot_hml_graph_size[MAX_METRIC_PRIORITY_LEVELS],
                       group_data_ptr[group_idx].num_vectors[METRIC_PRIORITY_HIGH],
                       group_data_ptr[group_idx].num_vectors[METRIC_PRIORITY_MEDIUM],
                       group_data_ptr[group_idx].num_vectors[METRIC_PRIORITY_LOW],
                       group_data_ptr[group_idx].num_vectors[MAX_METRIC_PRIORITY_LEVELS], msg_data_size, !create_st_or_not);     

    if(IS_HML_APPLIED)
    {
      int s = 0;
      for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++)
      {
        NSDL3_GDF(NULL, NULL, "i = %d, num_vectors[%d] = %d, tot_hml_graph_size[%d] = %d, hml_msg_data_size[%d][HML_MSG_DATA_SIZE] = %ld", 
                               i, i, group_data_ptr[group_idx].num_vectors[i], i, group_data_ptr[group_idx].tot_hml_graph_size[i],
                               i, hml_msg_data_size[i][HML_MSG_DATA_SIZE]);
        if(group_data_ptr[group_idx].num_vectors[i] && group_data_ptr[group_idx].tot_hml_graph_size[i])
        {
          s = group_data_ptr[group_idx].tot_hml_graph_size[i] * group_data_ptr[group_idx].num_vectors[i]; 
          hml_msg_data_size[i][HML_MSG_DATA_SIZE] += s; 
          msg_data_size += s; 
 
          //group_data_ptr[group_idx].num_vectors[MAX_METRIC_PRIORITY_LEVELS] += group_data_ptr[group_idx].num_vectors[i];
          group_data_ptr[group_idx].num_actual_graphs[MAX_METRIC_PRIORITY_LEVELS] += group_data_ptr[group_idx].num_actual_graphs[i];
        }
        NSDL3_GDF(NULL, NULL, "After: hml_msg_data_size[%d][HML_MSG_DATA_SIZE] = %d", i, hml_msg_data_size[i][HML_MSG_DATA_SIZE]);
      }
    }
    else
    {
      msg_data_size += (group_data_ptr[group_idx].num_vectors[MAX_METRIC_PRIORITY_LEVELS] * group_data_ptr[group_idx].tot_hml_graph_size[MAX_METRIC_PRIORITY_LEVELS]); 
      fprintf(write_gdf_fp,"\n");
    }
  }

  NSDL2_GDF(NULL, NULL, "Method exit, msg_data_size = %d", msg_data_size);

  return size;
}

// process only scalar groups. keep incrementing group count , which we will print in info line, in netstorm.gdf
static inline void process_scalar_group(char *gdf_name, FILE *read_fp, char*line, char *groupName, int rpGroupID, char *groupType, int numGraph, char *groupMetric, char *group_description, int numVector, char *groupHierarchy)
{
  int group_idx;
  char printLine[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called. http_mode = %d", group_default_settings->http_settings.http_mode);

  is_group_scalar(groupType, rpGroupID);
  NSDL3_GDF(NULL, NULL, "Scaler line =%s", line);

  sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|-|%s", 
          "Group", groupName, rpGroupID, groupType, numGraph, 0, groupMetric, group_description);
  /*bug 101437: avoid writing HTTP2 Group in case of HTTP1 protocol*/
  if(NS_AVOID_HTTP2_METRICS_FOR_HTTP1(printLine)){
    NSDL3_GDF(NULL, NULL, "write data");
    fprintf(write_gdf_fp,"%s\n", printLine);
    fprintf(write_gdf_fp,"\n");
  }

  //calling this function on runtime just to write Group line in testrun.gdf, doing this to avoid code duplicacy
  //If g_rtg_rewrite is set we have to fill all group details and process graph. we are doing this so that we can assign new trg index to all vector 
  if((g_runtime_flag) && (!g_rtg_rewrite))
    return;

  group_idx = fill_group_info(gdf_name, groupName, rpGroupID, groupType, numGraph, 1, NULL, NULL, group_description, groupHierarchy, groupMetric);
  process_graph(read_fp, numGraph, rpGroupID, 1, 0, 0, group_idx, NULL, 0);
  group_count++;
  NSDL3_GDF(NULL, NULL, "group_count = %d", group_count);
}

void get_no_of_elements_of_gdf(char *gdf_name, int *num_data, int *rtg_group_size, int *pdf_id, int *graphId, char *graph_desc)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  char data_type;
  int i = 0, idx = 0, size = 0;

  element_count = 0;

  read_gdf_fp = open_gdf(gdf_name);
  if(read_gdf_fp == NULL)
    return;

  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    if((sscanf(line, "%s", tmpbuff) == -1) || line[0] == '#' || line[0] == '\0' || !(strncasecmp(line, "info|", strlen("info|"))) || !(strncasecmp(line, "group|", strlen("group|"))))
      continue;
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      strcpy(tmpbuff, line);
      i = get_tokens(line, buffer, "|", GDF_MAX_FIELDS);

      if(i != 14)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: No. of field in Graph line is not 14. Line = %s\n", tmpbuff);
        continue;
      }

      //Graph|graphName|rptId|graphType|dataType|graphDataIndex|formula|numVectors|Future|pdf_id|Percentile_Data_Idx|future1|future2|Graph_Description 
      data_type = data_type_to_num(buffer[4]);
   
     if(*(buffer[8]) == 'E')
       continue;

      //Saving pdf id from graph to structure
     if(atoi(buffer[9]) != -1)
     {
       *pdf_id = atoi(buffer[9]);
       MALLOC_AND_COPY(buffer[13], graph_desc, (strlen(buffer[13]) + 1), "Copying graph desc", -1);
       *graphId = atoi(buffer[2]);
     }

      //saving index of min graph of times and timesStd 
      if(IS_TIMES_GRAPH(data_type) || IS_TIMES_STD_GRAPH(data_type))
      {
        if(idx > 127)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Total expected no. of times and timesStd graphs is 128. Exceeding this limit.\n");
        }
        idx++ ;
      }
      size += getSizeOfGraph(data_type, &element_count);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", tmpbuff); 
      continue;
    }
  }
  close_gdf(read_gdf_fp);

  *num_data = element_count;
  *rtg_group_size = size;
}


/*---------------------------------------------------------------------------------------
  Fun. Name     : set_rtg_index_in_cm_info()
  Purpose       : To set rtg_index for CM_info entries. This function is called form 
                  following places -
                  1. From inital gdf parsing i.e. process_all_gdf()
                  2. When any vector comes at runtime i.e. init_cm_table()
----------------------------------------------------------------------------------------*/
void set_rtg_index_in_cm_info(int rptGroupId, void *info_ptr, ns_ptr_t rtg_pkg_size, char runtime_flag)
{
  int idx_mon, mp,idx_vector,no_of_monitors = 1, mon_index , flag=0;
  char metric_priority = 0;
  CM_info *local_cm_info = (CM_info *)info_ptr;;
  CM_vector_info *local_cm_vector_info = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called, rptGroupId = %d, info_ptr = %p, group_num_vectors = %d, "
                        "rtg_pkg_size = %d, for_all = %d, g_rtg_rewrite = %d", 
                         rptGroupId, info_ptr, ((CM_info *)info_ptr)->group_num_vectors, rtg_pkg_size, runtime_flag, g_rtg_rewrite);

  /* Set group_num_vectors more than one only if rtg index need to set for all the vectors */
  if((runtime_flag == FILL_RTG_INDEX_AT_INITTIME) && (rptGroupId >= 0))   
  {
    /* Do not set RTG index for dynamic object as their rtg index is filled by set_rtg_index_for_dyn_objs_discovered() */
    if((rptGroupId == SERVER_STAT_RPT_GRP_ID) || (rptGroupId == TRANS_STATS_RPT_GRP_ID) || (rptGroupId == TRANS_CUM_STATS_RPT_GRP_ID) ||
       (rptGroupId == GRP_DATA_STAT_RPT_GRP_ID) || (rptGroupId == RBU_DOMAIN_STAT_GRP_ID))
    {
      NSDL2_GDF(NULL, NULL, "Since GDF Group rptGroupId '%d' does not lie in monitor range, hence not setting rtg index and returing from here.",rptGroupId);
      return;
    }

    /* All monitors rptGroupId must be greater than GDF_CM_START_RPT_GRP_ID */
    if(rptGroupId < GDF_CM_START_RPT_GRP_ID)
    {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error(set_rtg_index_in_cm_info): This group should not be vector\n rptid = %d", rptGroupId);
      NS_EXIT(-1, CAV_ERR_1013018, rptGroupId);
    }
  }
  
  mon_index = local_cm_info->monitor_list_idx;
  //this will only execute at initial time 
  if(!(rtg_pkg_size && runtime_flag))///ask manish : why check for rtg_pkg_size
  {
    no_of_monitors = monitor_list_ptr[mon_index].no_of_monitors;
    flag=1;
  }

  //It will act as group num vectors of a group. total loop will run for number of vectors in that group.

  NSDL2_GDF(NULL, NULL, "Set RTG index for rptGroupId = %d, gdf_name = %s", 
                         rptGroupId, local_cm_info->gdf_name);

  for(idx_mon = 0; idx_mon < no_of_monitors; idx_mon++)
  {
    local_cm_info = monitor_list_ptr[mon_index + idx_mon].cm_info_mon_conn_ptr;

    idx_vector = local_cm_info->total_vectors -1;

    if(flag)
    {
      idx_vector = 0;
    }

    metric_priority = local_cm_info->metric_priority;

    NSDL2_GDF(NULL, NULL, "Setting CM_info->rtg_index, local_cm_info = %p, idx_mon = %d, "
                          "metric_priority = %d, gdf_name = %s", 
                           local_cm_info, idx_mon, metric_priority, local_cm_info->gdf_name);
     
    if(!strncmp(local_cm_info->gdf_name, "NA", 2) || (local_cm_info->flags & ALL_VECTOR_DELETED))
    { 
      continue;
    }
   

    for(; idx_vector < local_cm_info->total_vectors; idx_vector++)//do not initialize idx_vector since it initialized above
    {
      local_cm_vector_info = &(local_cm_info->vector_list[idx_vector]);
 
      // If enable_hml_group_in_testrun_gdf disable then fill RTG index at index 3rd of rtg_index 
      if(metric_priority == MAX_METRIC_PRIORITY_LEVELS)
        mp = MAX_METRIC_PRIORITY_LEVELS; 
      else
        mp = 0;
    
      for(;mp <= metric_priority; mp++)
      {
        NSDL2_GDF(NULL, NULL, "mp = %d, rtg_index = %d, g_rtg_rewrite = %d, hml_msg_data_size[%d] = %d", 
                             mp, local_cm_vector_info->rtg_index[mp], g_rtg_rewrite, mp, hml_msg_data_size[mp][HML_START_RTG_IDX]);

        if((local_cm_vector_info->rtg_index[mp] == RTG_INDEX_NOT_SET) || g_rtg_rewrite)
        {
          //global_settings->enable_hml_group_in_testrun_gdf is enable
          if(((runtime_flag == FILL_RTG_INDEX_AT_INITTIME) || g_rtg_rewrite) && (mp != MAX_METRIC_PRIORITY_LEVELS))
          {
            local_cm_vector_info->rtg_index[mp] = hml_msg_data_size[mp][HML_START_RTG_IDX];
            hml_msg_data_size[mp][HML_START_RTG_IDX] += group_data_ptr[local_cm_info->gp_info_index].tot_hml_graph_size[mp];

            FILL_HML_VECTOR_LINE(local_cm_vector_info, mp, local_cm_info->metric_priority);

            NSDL2_GDF(NULL, NULL, "After: rtg_index = %ld, hml_msg_data_size[mp][HML_START_RTG_IDX] = %ld, tot_hml_graph_size = %hu", 
                                 local_cm_vector_info->rtg_index[mp], hml_msg_data_size[mp][HML_START_RTG_IDX], 
                                 group_data_ptr[local_cm_info->gp_info_index].tot_hml_graph_size[mp]); 
          }
          else 
          {
            local_cm_vector_info->rtg_index[mp] = rtg_pkg_size; 
            rtg_pkg_size += local_cm_info->group_element_size[mp];
            NSDL2_GDF(NULL, NULL, "After: rtg_index = %ld, rtg_pkg_size = %ld, group_element_size = %hu", 
                                 local_cm_vector_info->rtg_index[mp], rtg_pkg_size, 
                                 local_cm_info->group_element_size[mp]);
          }
        }
        else if(runtime_flag && (local_cm_vector_info->rtg_index[mp] != RTG_INDEX_NOT_SET))
        {
          NSDL2_GDF(NULL, NULL, "RTG index set and RunTime flag on, get HML vectors");
          FILL_HML_VECTOR_LINE(local_cm_vector_info, mp, local_cm_info->metric_priority);
        }
      }
    }
  }
}


static int get_numvector_for_pdf(int numVector){
  NSDL3_GDF(NULL, NULL, "Method called. numVector = %d", numVector);
  if(loader_opcode == MASTER_LOADER){
    numVector = numVector/(sgrp_used_genrator_entries+1);
  }
  NSDL3_GDF(NULL, NULL, "numVector = %d", numVector);
  return numVector;
}
//Anubhav: In case NC, for Tx and Server Ips actual_num_vectors will be calculated based on generator wise vectors
static int get_actual_num_vectors(int dyn_obj_idx, int num_vectors){
  int i, actual_num_vectors = 0; 
  NSDL3_GDF(NULL, NULL, "Method called, dyn_obj_idx = %d, num_vectors = %d"
                        "total_tx_entries = %d total_normalized_svr_ips = %d", dyn_obj_idx, num_vectors, 
                         total_tx_entries, total_normalized_svr_ips);

  if(dyn_obj_idx == NEW_OBJECT_DISCOVERY_TX || dyn_obj_idx == NEW_OBJECT_DISCOVERY_TX_CUM){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_tx_loc2norm_table[i].num_entries; //This will be sum of all vectors on generators
    actual_num_vectors = actual_num_vectors + total_tx_entries; //Sum of vectors on all generators = overall 
  }
  else if(dyn_obj_idx == NEW_OBJECT_DISCOVERY_SVR_IP){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_svr_ip_loc2norm_table[i].num_entries; 
    actual_num_vectors = actual_num_vectors + total_normalized_svr_ips; //Sum of vectors on all generators = overall 
  }
  else if(dyn_obj_idx == NEW_OBJECT_DISCOVERY_STATUS_CODE){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_http_status_code_loc2norm_table[i].num_entries;
    actual_num_vectors = actual_num_vectors + total_http_resp_code_entries; //Sum of vectors on all generators = overall 
  }
  else if(dyn_obj_idx == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_tcp_clinet_errs_loc2normtbl[i].tot_entries;
    actual_num_vectors = actual_num_vectors + g_total_tcp_client_errs; //Sum of vectors on all generators = overall 
  }
  else if(dyn_obj_idx == NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_udp_clinet_errs_loc2normtbl[i].tot_entries;
    actual_num_vectors = actual_num_vectors + g_total_udp_client_errs; //Sum of vectors on all generators = overall 
  }
  else
   actual_num_vectors = num_vectors;
  
  NSDL3_GDF(NULL, NULL, "actual_num_vectors = %d", actual_num_vectors);
  return actual_num_vectors;
}

// process only vector groups. keep incrementing group count , which we will print in info line, in netstorm.gdf
static inline void process_vector_group(FILE *read_fp, char **buffer, char *groupName, int rpGroupID, char *groupType, int numGraph,int numField, int numVector, char *gdf_name, char *groupMetric, char *group_description, void *info_ptr, char *groupHierarchy, int is_user_monitor, int dyn_obj_idx)
{
  char printLine[MAX_LINE_LENGTH];
  //int groupSize = 0;
  char **vectorNamesPtr = NULL;
  int pdf_data_size_prev = 0;
  int pdf_data_size_8B_prev = 0;
  //int prev_msg_data_size;
  int num_actual_data;
  int i, actual_num_vectors = 0;
  int actual_graph_count;
  int group_idx = -1;

  NSDL3_GDF(NULL, NULL, "Method called, gdf_name = %s, groupName = %s, rpGroupID = %d, groupType = %d, numGraph = %d, numVector = %d, "
                         "info_ptr = %p", 
                         gdf_name, groupName, rpGroupID, groupType, numGraph, numVector, info_ptr);

  if(!IS_HML_APPLIED)
  {
    //This method will get actual num of graphs of a group. It will count graphs other than graphs marked with 'E'.
    //int actual_graph_count = get_actual_no_of_graphs(gdf_name, groupName, &num_actual_data, info_ptr);
    actual_graph_count = get_actual_no_of_graphs(gdf_name, groupName, &num_actual_data, info_ptr, NULL);
    
    NSDL3_GDF(NULL, NULL, "actual_graph_count = %d, num_actual_data = %d, deleted_vector_count = %d", 
                           actual_graph_count, num_actual_data, deleted_monitor_count);
    
    actual_num_vectors = (loader_opcode != MASTER_LOADER)?numVector:get_actual_num_vectors(dyn_obj_idx, numVector);
   
    if (loader_opcode != MASTER_LOADER) {// bypass
      is_group_vector(groupType, rpGroupID);
      NSDL2_GDF(NULL, NULL, "**** inside if");
      sprintf(printLine,"%s|%s|%s|%s|%d|%d|%s|%s%s|%s", 
              buffer[0], buffer[1], buffer[2], buffer[3], actual_graph_count, numVector, groupMetric, STORE_PREFIX, groupHierarchy, group_description);
    } else { 
        sprintf(printLine,"%s|%s|%s|%s|%d|%d|%s|%s%s|%s", 
                          buffer[0], buffer[1], buffer[2], "vector", actual_graph_count, actual_num_vectors, groupMetric, STORE_PREFIX, groupHierarchy, group_description);
    }
    
    //  sprintf(printLine,"%s|%s|%s|%s|%s|%d", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], numVector);
    
    NSDL3_GDF(NULL, NULL, "printLine = %s http_mode = %d", printLine, group_default_settings->http_settings.http_mode);
    /*bug 92383: avoid HTTP2 stats Metrics on GUI for HTTP protocol*/
    if(NS_AVOID_HTTP2_METRICS_FOR_HTTP1(printLine)) {
      NSDL3_GDF(NULL, NULL, "Write data");
      fprintf(write_gdf_fp,"%s\n", printLine);
      fprintf(write_gdf_fp,"\n");
    }
  }
  
  NSDL3_GDF(NULL, NULL, "g_runtime_flag = %d, g_rtg_rewrite = %d", g_runtime_flag, g_rtg_rewrite);
  //calling this function on runtime just to write Group line in testrun.gdf, doing this to avoid code duplicacy
  //If g_rtg_rewrite is set we have to fill all group details and process graph. we are doing this so that we can assign new trg index to all vector 
  if((g_runtime_flag) && (!g_rtg_rewrite))
    return;


  if(info_ptr && !global_settings->enable_hml_group_in_testrun_gdf)
    set_rtg_index_in_cm_info(rpGroupID, info_ptr, msg_data_size, FILL_RTG_INDEX_AT_INITTIME);


  group_idx = fill_group_info(gdf_name , groupName, rpGroupID, groupType, numGraph, numVector, vectorNamesPtr, 
                              groupHierarchy, group_description, groupHierarchy, groupMetric);
  
  //Create 2D array to store vector name list for printing purpose 
  // why not used //vectorNamesPtr ???ABHIHML
  group_data_ptr[group_idx].vector_names = printGroupVector(group_idx, numVector, rpGroupID, gdf_name, info_ptr, is_user_monitor, 1);


  pdf_data_size_prev = total_pdf_data_size;
  pdf_data_size_8B_prev = total_pdf_data_size_8B;
  //prev_msg_data_size = msg_data_size;

  //groupSize = process_graph(read_fp, numGraph, rpGroupID, numVector);
  //process_graph(read_fp, numGraph, rpGroupID, numVector, 0, 0);
  process_graph(read_fp, numGraph, rpGroupID, numVector, 0, 0, group_idx, info_ptr, is_user_monitor);

  //groupSize = msg_data_size - prev_msg_data_size;
  group_data_ptr[total_groups_entries - 1].pdf_graph_data_size = (total_pdf_data_size - pdf_data_size_prev)/*  / numVector */;

  //NSTL2(NULL, NULL, "Monitor: %s, Number of total graphs = %d, Number of actual graphs = %d", 
  //                   groupName, group_data_ptr[total_groups_entries - 1].num_graphs, 
  //                   group_data_ptr[total_groups_entries - 1].num_actual_graphs);

  Graph_Info *local_ptr;
  local_ptr = graph_data_ptr + group_data_ptr[total_groups_entries - 1].graph_info_index;

  //int i;
  for(i = 0; i < group_data_ptr[total_groups_entries - 1].num_graphs; i++ )
  {
    if(local_ptr->graph_state == GRAPH_EXCLUDED)
      group_data_ptr[total_groups_entries - 1].excluded_graph[i] = GRAPH_EXCLUDED;

    local_ptr++;
  }
  
  /* Correction */
  // In case of conrtroller all the graphs become vector,so numVector is changed, but prcentile data packet size will not be changed
  // so we are are dividing numVector by sgrp_used_genrator_entries+1 to get its orignal size
  int new_num_vector = get_numvector_for_pdf(numVector);

  total_pdf_data_size += group_data_ptr[total_groups_entries - 1].pdf_graph_data_size * (new_num_vector - 1);

  //total_pdf_data_size += group_data_ptr[group_idx].pdf_graph_data_size * (new_num_vector - 1);
  total_pdf_data_size_8B += (total_pdf_data_size_8B - pdf_data_size_8B_prev) * (new_num_vector - 1);

  //NSDL3_GDF(NULL, NULL, "total_pdf_data_size = %d, total_pdf_data_size_8B = %d", total_pdf_data_size, total_pdf_data_size_8B);

  //msg_data_size = msg_data_size + ( (numVector - 1) * groupSize); // msg_data_size should be increse by the (no of vector -1). we have done this because for 1 vector group, already msg_data_size incremented and for rest of vector group we have to muliply it with size of process vector group.
  //NSDL3_GDF(NULL, NULL, "size of whole graph: %d, msg_data_size =%ld, numVector = %d", groupSize, msg_data_size, numVector);

  group_count++;
  NSDL3_GDF(NULL, NULL, "group_count = %d", group_count);
}

//This is to get no. of monitor whose all_vector_deleted
static int get_deleted_monitor_count(CM_info *local_cm_ptr, int grp_num_monitors, int parent_idx)
{
  int deleted_monitor = 0, i = 0;

  NSDL2_GDF(NULL, NULL, "Method called");

  while(1)
  {
    if(i >= grp_num_monitors)
      break;

    local_cm_ptr = monitor_list_ptr[parent_idx + i].cm_info_mon_conn_ptr;

    if((local_cm_ptr) && (local_cm_ptr->flags & ALL_VECTOR_DELETED))
      deleted_monitor++;      

    i = i + 1;
  }
  NSDL3_GDF(NULL, NULL, "Number of deleted vector: %d", deleted_monitor);
  return deleted_monitor;
}

static inline int getGroupNumVectorByID (int rptGrpId, char *groupHierarchy, char **buffer, char *groupType, void *info_ptr)
{
  int numVector;
  int len =0;
  char local_groupHierarchy[1024] = "";
  char hie_sep[2] = "";

  NSDL2_GDF(NULL, NULL, "Method called, rptGrpId = %d, info_ptr = %p, sgrp_used_genrator_entries = %d", 
                         rptGrpId, info_ptr, sgrp_used_genrator_entries);

  numVector = getNSGroupNumVectorByID(rptGrpId);  // check is Dynamic Group is Present if yes then find its numVector.
  numVector = (sgrp_used_genrator_entries + 1) * numVector;
 
  //TX stats will not come in group based feature, not counting those vectors in group based 
  if (SHOW_GRP_DATA && 
     (rptGrpId != TRANS_STATS_RPT_GRP_ID) && (rptGrpId != TRANS_CUM_STATS_RPT_GRP_ID) && 
     (rptGrpId != SRV_IP_STAT_GRP_ID) && (rptGrpId != RBU_PAGE_STAT_GRP_ID) && 
     (rptGrpId != PAGE_BASED_STAT_GRP_ID) && (rptGrpId != GRP_DATA_STAT_RPT_GRP_ID) && 
     (rptGrpId != NS_DIAGNOSIS_RPT_GRP_ID) && (rptGrpId != SHOW_VUSER_FLOW_STAT_GRP_ID) && 
     (rptGrpId != RBU_DOMAIN_STAT_GRP_ID) && (rptGrpId != NS_HEALTH_MONITOR_GRP_ID) && 
     (rptGrpId != HTTP_STATUS_CODE_RPT_GRP_ID) && (rptGrpId != TCP_CLIENT_FAILURES_RPT_GRP_ID))
  {
    //Show group wise data is enabled. So total vectors will for all grpups + 1 for overall
    //Below statement will set num vectors for custom monitors too which is not required and this will lead to increment of numVector by total_runprof_entries.
    numVector = (total_runprof_entries + 1) * numVector; 
  }
  
  NSDL2_GDF(NULL, NULL, "GroupId = %d, GroupNumVectors = %d", rptGrpId, numVector);
  //This is what goes into group's line in testrun.gdf
  /*if(loader_opcode == MASTER_LOADER) {
    if (!info_ptr || (rptGrpId == SHOW_VUSER_FLOW_STAT_GRP_ID)) {
      strcpy(local_groupHierarchy, "Group>FlowPath");
      sprintf(hie_sep, ">");
    }
  } */

  //This is what goes into group's line in testrun.gdf

  if (loader_opcode == MASTER_LOADER) 
  {
    if (!info_ptr || (rptGrpId == IP_BASED_STAT_GRP_ID) || 
       (rptGrpId == SRV_IP_STAT_GRP_ID) || (rptGrpId == HTTP_STATUS_CODE_RPT_GRP_ID)) 
    { // In case of monitor's gdf do not add Controller>Generator in Hierarchy.
      len = sprintf(local_groupHierarchy, "Controller>Generator");
      sprintf(hie_sep, ">");
    }
  }

  //Show group data is enabled so fill Group in meta data field
  if(SHOW_GRP_DATA && 
    (rptGrpId != TRANS_STATS_RPT_GRP_ID) && (rptGrpId != TRANS_CUM_STATS_RPT_GRP_ID) && 
    (rptGrpId != SRV_IP_STAT_GRP_ID) && (!info_ptr) && (rptGrpId != RBU_PAGE_STAT_GRP_ID) && 
    (rptGrpId != PAGE_BASED_STAT_GRP_ID) && (rptGrpId != GRP_DATA_STAT_RPT_GRP_ID) && 
    (rptGrpId != NS_DIAGNOSIS_RPT_GRP_ID) && (rptGrpId != SHOW_VUSER_FLOW_STAT_GRP_ID) && 
    (rptGrpId != RBU_DOMAIN_STAT_GRP_ID) && (rptGrpId != IP_BASED_STAT_GRP_ID) && 
    (rptGrpId != HTTP_STATUS_CODE_RPT_GRP_ID) && (rptGrpId != TCP_CLIENT_FAILURES_RPT_GRP_ID)) 
  {
    if(loader_opcode != MASTER_LOADER)
      len += sprintf(local_groupHierarchy + len, "Group");
    else
      len += sprintf(local_groupHierarchy + len, ">Group");
    sprintf(hie_sep, ">");
  }

  if(((loader_opcode != MASTER_LOADER) && SHOW_GRP_DATA && 
     (rptGrpId != TRANS_STATS_RPT_GRP_ID) && (rptGrpId != TRANS_CUM_STATS_RPT_GRP_ID) &&
     (rptGrpId != SRV_IP_STAT_GRP_ID) && (rptGrpId != RBU_PAGE_STAT_GRP_ID) && 
     (rptGrpId != PAGE_BASED_STAT_GRP_ID) && (rptGrpId != GRP_DATA_STAT_RPT_GRP_ID) && 
     (rptGrpId != NS_DIAGNOSIS_RPT_GRP_ID) && (rptGrpId != RBU_DOMAIN_STAT_GRP_ID) && 
     (rptGrpId != IP_BASED_STAT_GRP_ID) && (rptGrpId != HTTP_STATUS_CODE_RPT_GRP_ID)) || 
    (loader_opcode == MASTER_LOADER))
  {
    if(strcmp (groupHierarchy, "NA")) //Something is in groupHierarchy
      len += sprintf(local_groupHierarchy + len, "%s%s", hie_sep, groupHierarchy);

    strcpy(groupHierarchy, local_groupHierarchy);
    strcpy(buffer[3], "vector");
    strcpy(groupType, "vector");
  }

  //For health monitor number of vectors will only be generators irrespective of the generator count in case of NC test. It will only be applied on Controller.
  if(rptGrpId == NS_HEALTH_MONITOR_GRP_ID)
    numVector = 1;

  return numVector;
}

// process group line, check the dynamic group, process only thoes are valid.
// Group information line =>
//      Group|groupName|rptGrpId|groupType|numGraphs|numVectors|future1|future2|Group_Description 
// Eg:  Group|Vuser Info|1|scalar|12|0|Test Metrics|NA|Virtual Users information.
//      Group|Domain Stats|109|vector|9|0|RBU Metrics|Group>Page>Domain|Resource timings for each domain
static void process_group(FILE *read_fp, char *grLine, int numGroup, char *gdf_name, int is_user_monitor, void *info_ptr, int *ngraph, int *rpgroup_id, char *gName, char *gType, char *gHierarchy, char *gDescription, char *gMetric, int dyn_obj_idx)
{
  int rpGroupID = 0;
  int numGraph = 0;
  int numVector = 0;
  char groupName [MAX_LINE_LENGTH];
  char groupType[MAX_LINE_LENGTH];
  char groupDescription[MAX_LINE_LENGTH];
  char groupMetric[MAX_LINE_LENGTH];
  char groupHierarchy[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  int numField = 0;
  char line[MAX_LINE_LENGTH];
  Group_Info *local_group_data_ptr = group_data_ptr;
  int i;
  int call_scalar = 1;

  // Storing grLine into local variable beacuse later doing strtok over line and it will put NULL where | is.
  strcpy(line, grLine);
  NSDL2_GDF(NULL, NULL, "Method called, processing Group line = %s, group count = %d", line, group_count);

  numField = get_tokens(line, buffer, "|", GDF_MAX_FIELDS);

  groupDescription[0] = '-';
  groupDescription[1] = '\0';

  if(numField != 6 && numField != 9)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in reading Group line: no. of fields are not correct");
    NS_EXIT(-1,CAV_ERR_1013019);
  }

  strcpy(groupName, buffer[1]);
  if(gName)
    strcpy(gName, groupName);

  rpGroupID = atoi(buffer[2]);
  
  if(rpgroup_id) //need this during runtime graph processing
    *rpgroup_id = rpGroupID;

  numGraph = atoi(buffer[4]);
  if( numGraph == 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: No.of graphs should not be 0 in GDF ID %d.Hence,exiting from the test\n",rpGroupID);
    NS_EXIT(-1, CAV_ERR_1013020, rpGroupID);
  }

  if(ngraph) //need this during runtime graph processing
    *ngraph = numGraph;

  numVector = atoi(buffer[5]);
  strcpy(groupType, buffer[3]);
  if(gType)
    strcpy(gType, groupType);
  
  if (numField == 9)
  {
    if ((rpGroupID == 107 || rpGroupID == 109) && (strcasecmp(g_cavinfo.SUB_CONFIG, "NVSM") == 0))  //RBU Page Stat
    {
      strcpy(groupMetric, "NetVision SM Metrics");
      if(gMetric)
        strcpy(gMetric, groupMetric); 
    }
    else{
      strcpy(groupMetric, buffer[6]);
      if(gMetric)
        strcpy(gMetric, buffer[6]);
    }

    strcpy(groupDescription, buffer[8]);
    if(gDescription)
      strcpy(gDescription, groupDescription);
  }

  //Group hierarchical view format
  strcpy(groupHierarchy, buffer[7]);
  if(gHierarchy)
    strcpy(gHierarchy, groupHierarchy);

  //Maninder:Changes for checking duplicate Group Ids
  if(rpGroupID > GDF_CM_START_RPT_GRP_ID && g_runtime_flag == 0)        //Checking whether it is Netstorm GDF or not.If not, we dont have to check group ids for them.
  {
    for( i=0; i < total_groups_entries; i++, local_group_data_ptr++ )
    {
      if( local_group_data_ptr->rpt_grp_id == rpGroupID)                //Checking whether is there any group with same group id, if yes then exit with Error message
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Found duplicate Group Id [%d] for Group [%s] and Group [%s]  \n", rpGroupID, groupName, local_group_data_ptr->group_name); 
        NS_EXIT(-1, CAV_ERR_1013021, rpGroupID, groupName, local_group_data_ptr->group_name);
      }
    }
  }

  //Check whether Dynamic Group is Present if yes then find its numVector.
  numVector = getGroupNumVectorByID(rpGroupID, groupHierarchy, buffer, groupType, info_ptr);
  if(numVector == 0)
    return;

  //If group wise data need to show then all groups will be vector groups
  if(SHOW_GRP_DATA)
  {
    call_scalar = 0;
  }

  NSDL3_GDF(NULL, NULL, "rpGroupID = %d, groupType = %s, numVector = %d, numGraph =%d, groupMetric = %s, "
                        "groupDescription = %s, groupHierarchy = %s", 
                         rpGroupID, groupType, numVector, numGraph, groupMetric, groupDescription, groupHierarchy);

  switch(rpGroupID)
  {
    case VUSER_RPT_GRP_ID:
      rtg_index_array[VUSER] = msg_data_size;
      vuser_gp_ptr = (Vuser_gp *)msg_data_size;   // using msg_data_size as index.
      vuser_group_idx = group_count;
      break;
    case SSL_RPT_GRP_ID:
      rtg_index_array[NET_THROUGHPUT] = msg_data_size;
      ssl_gp_ptr = (SSL_gp*)msg_data_size;
      ssl_gp_idx = group_count;
      break;
    case URL_HITS_RPT_GRP_ID:
      rtg_index_array[URL_HITS] = msg_data_size;
      url_hits_gp_ptr = (Url_hits_gp*)msg_data_size;
      url_hits_gp_idx = group_count;
      break;
    case FC2_RPT_GRP_ID:
      rtg_index_array[FC2_STATS] = msg_data_size;
      fc2_gp_ptr = (fc2_gp *)msg_data_size;
      fc2_gp_idx = group_count;
      break;
    case PG_DOWNLOAD_RPT_GRP_ID:
      rtg_index_array[PAGE_DOWNLOAD] = msg_data_size;
      page_download_gp_ptr = (Page_download_gp*)msg_data_size;
      page_download_gp_idx = group_count;
      break;
    case SESSION_RPT_GRP_ID:
      rtg_index_array[SESSION_PTR] = msg_data_size;
      session_gp_ptr = (Session_gp*)msg_data_size;
      session_gp_idx = group_count;
      break;
    case TRANSDATA_RPT_GRP_ID:
      rtg_index_array[TRANS_OVERALL] = msg_data_size;
      trans_overall_gp_ptr = (Trans_overall_gp*)msg_data_size;
      trans_overall_gp_idx = group_count;
      break;
    case URL_FAIL_RPT_GRP_ID:
      rtg_index_array[URL_FAIL] = msg_data_size;
      url_fail_gp_ptr = (URL_fail_gp*)msg_data_size;
      url_fail_gp_idx = group_count;
      call_scalar = 0;
      break;
    case PAGE_FAIL_RPT_GRP_ID:
      rtg_index_array[PAGE_FAIL] = msg_data_size;
      page_fail_gp_ptr = (Page_fail_gp*)msg_data_size;
      page_fail_gp_idx = group_count;
      call_scalar = 0;
      break;
    case SESSION_FAIL_RPT_GRP_ID:
      rtg_index_array[SESSION_FAIL] = msg_data_size;
      session_fail_gp_ptr = (Session_fail_gp*)msg_data_size;
      session_fail_gp_idx = group_count;
      call_scalar = 0;
      break;
    case TRANS_FAIL_RPT_GRP_ID:
      rtg_index_array[TRANS_FAIL] = msg_data_size;
      trans_fail_gp_ptr = (Trans_fail_gp*)msg_data_size;
      trans_fail_gp_idx = group_count;
      call_scalar = 0;
      break;
    case SERVER_STAT_RPT_GRP_ID:
      rtg_index_array[SERVER_STATS] = msg_data_size;
      server_stats_gp_ptr = (Server_stats_gp*)msg_data_size;
      server_stats_gp_idx = group_count; 
      call_scalar = 0;
      break;
      /*bug 70480 process  http2 server push group*/
    case HTTP2_SERVER_PUSH_ID:
      rtg_index_array[SERVER_PUSH_STATS] = msg_data_size;
      http2_srv_push_gp_ptr = (Http2SrvPush_gp*)msg_data_size;
      http2_srv_push_gp_idx = group_count;
      break;
    case TRANS_STATS_RPT_GRP_ID:
      if(set_group_idx_for_txn || g_rtg_rewrite)
      {
        rtg_index_array[TRANS_STATS] = msg_data_size;
        trans_stats_gp_ptr = (Trans_stats_gp *)msg_data_size;
        trans_stats_gp_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].gp_info_idx = trans_stats_gp_idx;
      }
      if(!dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].is_gp_info_filled || g_rtg_rewrite)//only dynamic
      {
        trans_stats_gp_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].gp_info_idx = trans_stats_gp_idx;
      }
      call_scalar = 0;
      break;
    case TRANS_CUM_STATS_RPT_GRP_ID:
      if(set_group_idx_for_txn || g_rtg_rewrite)
      {
        rtg_index_array[TRANS_CUM_STATS] = msg_data_size;
        trans_cum_stats_gp_ptr = (Trans_cum_stats_gp *)msg_data_size;
        trans_cum_stats_gp_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].gp_info_idx = trans_cum_stats_gp_idx;
      }
      if(!dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].is_gp_info_filled || g_rtg_rewrite)
      {
        trans_cum_stats_gp_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].gp_info_idx = trans_cum_stats_gp_idx;
      }
      call_scalar = 0;
      break;

    case SMTP_NET_TROUGHPUT_RPT_GRP_ID: 
      rtg_index_array[SMTP_NET_THROUGHPUT] = msg_data_size;
      smtp_net_throughput_gp_ptr = (SMTP_Net_throughput_gp*)msg_data_size;
      smtp_net_throughput_gp_idx = group_count;
      break;
    case SMTP_HITS_RPT_GRP_ID:
      rtg_index_array[SMTP_HITS] = msg_data_size;
      smtp_hits_gp_ptr = (SMTP_hits_gp*)msg_data_size;
      smtp_hits_gp_idx = group_count;
      break;

    case SMTP_FAIL_RPT_GRP_ID:
      rtg_index_array[SMTP_FAIL] = msg_data_size;
      smtp_fail_gp_ptr = (SMTP_fail_gp*)msg_data_size;
      smtp_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

    case POP3_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[POP3_NET_THROUGHPUT] = msg_data_size;
      pop3_net_throughput_gp_ptr = (POP3_Net_throughput_gp*)msg_data_size;
      pop3_net_throughput_gp_idx = group_count;
      break;
    case POP3_HITS_RPT_GRP_ID:
      rtg_index_array[POP3_HITS] = msg_data_size;
      pop3_hits_gp_ptr = (POP3_hits_gp*)msg_data_size;
      pop3_hits_gp_idx = group_count;
      break;
    case POP3_FAIL_RPT_GRP_ID:
      rtg_index_array[POP3_FAIL] = msg_data_size;
      pop3_fail_gp_ptr = (POP3_fail_gp*)msg_data_size;
      pop3_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

      /* FTP */
    case FTP_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[FTP_NET_THROUGHPUT] = msg_data_size;
      ftp_net_throughput_gp_ptr = (FTP_Net_throughput_gp*)msg_data_size;
      ftp_net_throughput_gp_idx = group_count;
      break;
    case FTP_HITS_RPT_GRP_ID:
      rtg_index_array[FTP_HITS] = msg_data_size;
      ftp_hits_gp_ptr = (FTP_hits_gp*)msg_data_size;
      ftp_hits_gp_idx = group_count;
      break;
    case FTP_FAIL_RPT_GRP_ID:
      rtg_index_array[FTP_FAIL] = msg_data_size;
      ftp_fail_gp_ptr = (FTP_fail_gp*)msg_data_size;
      ftp_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

      /* LDAP */
    case LDAP_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[LDAP_NET_THROUGHPUT] = msg_data_size;
      ldap_net_throughput_gp_ptr = (LDAP_Net_throughput_gp*)msg_data_size;
      ldap_net_throughput_gp_idx = group_count;
      break;
    case LDAP_HITS_RPT_GRP_ID:
      rtg_index_array[LDAP_HITS] = msg_data_size;
      ldap_hits_gp_ptr = (LDAP_hits_gp*)msg_data_size;
      ldap_hits_gp_idx = group_count;
      break;
    case LDAP_FAIL_RPT_GRP_ID:
      rtg_index_array[LDAP_FAIL] = msg_data_size;
      ldap_fail_gp_ptr = (LDAP_fail_gp*)msg_data_size;
      ldap_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

      /* IMAP */
    case IMAP_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[IMAP_NET_THROUGHPUT] = msg_data_size;
      imap_net_throughput_gp_ptr = (IMAP_Net_throughput_gp*)msg_data_size;
      imap_net_throughput_gp_idx = group_count;
      break;
    case IMAP_HITS_RPT_GRP_ID:
      rtg_index_array[IMAP_HITS] = msg_data_size; 
      imap_hits_gp_ptr = (IMAP_hits_gp*)msg_data_size;
      imap_hits_gp_idx = group_count;
      break;
    case IMAP_FAIL_RPT_GRP_ID:
      rtg_index_array[IMAP_FAIL] = msg_data_size;
      imap_fail_gp_ptr = (IMAP_fail_gp*)msg_data_size;
      imap_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

      /* JRMI */
    case JRMI_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[JRMI_NET_THROUGHPUT] = msg_data_size;
      jrmi_net_throughput_gp_ptr = (JRMI_Net_throughput_gp*)msg_data_size;
      jrmi_net_throughput_gp_idx = group_count;
      break;
    case JRMI_HITS_RPT_GRP_ID:
      rtg_index_array[JRMI_HITS] = msg_data_size;
      jrmi_hits_gp_ptr = (JRMI_hits_gp*)msg_data_size;
      jrmi_hits_gp_idx = group_count;
      break;
    case JRMI_FAIL_RPT_GRP_ID:
      rtg_index_array[JRMI_FAIL] = msg_data_size;
      jrmi_fail_gp_ptr = (JRMI_fail_gp*)msg_data_size;
      jrmi_fail_gp_idx = group_count;
      call_scalar = 0;
      break;
      /* dns */
    case DNS_NET_TROUGHPUT_RPT_GRP_ID:
      rtg_index_array[DNS_NET_THROUGHPUT] = msg_data_size;
      dns_net_throughput_gp_ptr = (DNS_Net_throughput_gp*)msg_data_size;
      dns_net_throughput_gp_idx = group_count;
      break;
    case DNS_HITS_RPT_GRP_ID:
      rtg_index_array[DNS_HITS] = msg_data_size;
      dns_hits_gp_ptr = (DNS_hits_gp*)msg_data_size;
      dns_hits_gp_idx = group_count;
      break;
    case DNS_FAIL_RPT_GRP_ID:
      rtg_index_array[DNS_FAIL] = msg_data_size;
      dns_fail_gp_ptr = (DNS_fail_gp*)msg_data_size;
      dns_fail_gp_idx = group_count;
      call_scalar = 0;
      break;

    case HTTP_STATUS_CODE_RPT_GRP_ID:
      NSDL2_GDF(NULL, NULL, "RTG index msg_data_size = %d, is_gp_info_filled = %d", 
                             msg_data_size, dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].is_gp_info_filled);
      //if(!dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].is_gp_info_filled)
      if(!http_status_codes_gp_idx || g_rtg_rewrite)
      {
        NSDL2_GDF(NULL, NULL, "RTG index msg_data_size = %d", msg_data_size);
        dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].gp_info_idx = http_status_codes_gp_idx = group_count;

        NSDL2_GDF(NULL, NULL, "HTTP_STATUS_CODE_RPT_GRP_ID: http_status_codes_gp_idx = %u", http_status_codes_gp_idx);
      }
      call_scalar = 0;
      break;


    case HTTP_CACHING_RPT_GRP_ID:  /* HTTP Caching */
      rtg_index_array[HTTP_CACHING] = msg_data_size;
      http_caching_gp_ptr = (HttpCaching_gp *)msg_data_size;
      http_caching_gp_idx = group_count;
      break;

    case NS_DIAGNOSIS_RPT_GRP_ID:  /* Cavisson Diagnostics */
      rtg_index_array[NS_DIAG] = msg_data_size;
      ns_diag_gp_ptr = (NSDiag_gp *)msg_data_size;
      ns_diag_gp_idx = group_count;
      call_scalar = 1;
      break;

    case DOS_ATTACK_RPT_GRP_ID:  /* DOS Attack */
      rtg_index_array[DOS_ATTACK] = msg_data_size;
      dos_attack_gp_ptr = (DosAttack_gp *)msg_data_size;
      dos_attack_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "dos_attack_gp_ptr = %p, dos_attack_gp_idx = %u", dos_attack_gp_ptr, dos_attack_gp_idx);
      break;

    case HTTP_PROXY_RPT_GRP_ID:  /* HTTP Proxy*/
      rtg_index_array[HTTP_PROXY] = msg_data_size;
      http_proxy_gp_ptr = (HttpProxy_gp *)msg_data_size;
      http_proxy_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "http_proxy_gp_ptr= %p, http_proxy_gp_idx= %u", http_proxy_gp_ptr, http_proxy_gp_idx);
      break;


    case HTTP_NETWORK_CACHE_RPT_GRP_ID:  
      rtg_index_array[HTTP_NETWORK_CACHE_STATS] = msg_data_size;
      http_network_cache_stats_gp_ptr = (HttpNetworkCacheStats_gp *)msg_data_size;
      http_network_cache_stats_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "http_network_cache_stats_gp_ptr= %p, http_network_cachestats_gp_idx= %u", http_network_cache_stats_gp_ptr, http_network_cache_stats_gp_idx);
      break;

    case DNS_LOOKUP_RPT_GRP_ID:  
      rtg_index_array[DNS_LOOKUP_STATS] = msg_data_size;
      dns_lookup_stats_gp_ptr = (DnsLookupStats_gp *)msg_data_size;
      dns_lookup_stats_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "dns_lookup_stats_gp_ptr= %p, dns_lookup_stats_gp_idx = %u", dns_lookup_stats_gp_ptr, dns_lookup_stats_gp_idx);
      break;

     /* WS */
     case WS_RPT_GRP_ID:  
      rtg_index_array[WS_STATS] = msg_data_size;
      ws_stats_gp_ptr = (WSStats_gp *)msg_data_size; 
      ws_stats_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "ws_stats_gp_ptr= %p, ws_stats_gp_idx= %u", ws_stats_gp_ptr, ws_stats_gp_idx);
      break;
    
      /* Websocket Resp Status Codes */
    case WS_STATUS_CODES_RPT_GRP_ID:
      rtg_index_array[WS_STATUS_CODES] = msg_data_size;
      ws_status_codes_gp_ptr = (WSStatusCodes_gp *)msg_data_size;
      ws_status_codes_gp_idx = group_count;
      total_ws_status_codes = numGraph;
      NSDL2_GDF(NULL, NULL, "ws_status_codes_gp_ptr = %p, ws_status_codes_gp_idx = %u, ws_status_codes_gp_idx = %d, "
                            "total_ws_status_codes = %d",
                             ws_status_codes_gp_ptr, ws_status_codes_gp_idx, ws_status_codes_gp_idx, total_ws_status_codes);
       /* if numbers are increased in graph change in respective arrays also. */
      if(total_ws_status_codes != 41)
        assert(0);
      break;
   
    /* WebSocket Failure Stats */
    case WS_FAILURE_STATS_RPT_GRP_ID: 
     rtg_index_array[WS_FAILURE_STATS] = msg_data_size;
     ws_failure_stats_gp_ptr = (WSFailureStats_gp *)msg_data_size;
     ws_failure_stats_gp_idx = group_count;
     call_scalar = 0;                        //This represents this graph is vector
     NSDL2_GDF(NULL, NULL, "ws_failure_stats_gp_ptr = %p, ws_failure_stats_gp_idx = %u", ws_failure_stats_gp_ptr, ws_failure_stats_gp_idx);
     break;

    case GRP_DATA_STAT_RPT_GRP_ID:
      rtg_index_array[GROUP_DATA] = msg_data_size;
      group_data_gp_ptr = (Group_data_gp *)msg_data_size;
      group_data_idx = group_count;
      NSDL2_GDF(NULL, NULL, "group_data_gp_ptr= %p, group_data_idx= %u", group_data_gp_ptr, group_data_idx);
      call_scalar = 0;
      break;

    case RBU_PAGE_STAT_GRP_ID:  
      rtg_index_array[RBU_PAGE] = msg_data_size;
      rbu_page_stat_data_gp_ptr = (RBU_Page_Stat_data_gp *)msg_data_size;
      rbu_page_stat_data_idx = group_count; 
      NSDL2_GDF(NULL, NULL, "rbu_page_stat_data_gp_ptr= %p, rbu_page_stat_data_idx= %u", rbu_page_stat_data_gp_ptr, rbu_page_stat_data_idx);
      call_scalar = 0;
      break;
     
    case PAGE_BASED_STAT_GRP_ID:
      rtg_index_array[PAGE_BASED] = msg_data_size;
      page_based_stat_gp_ptr = (Page_based_stat_gp *)msg_data_size;
      page_based_stat_idx = group_count;
      NSDL2_GDF(NULL, NULL, "page_based_stat_gp_ptr = %p, page_based_stat_idx = %u", page_based_stat_gp_ptr, page_based_stat_idx);
      call_scalar = 0;
      break;

    case SHOW_VUSER_FLOW_STAT_GRP_ID: 
      if (numVector) {
        rtg_index_array[VUSER_FLOW_BASED] = msg_data_size;
        vuser_flow_gp_ptr = (VUserFlowBasedGP *)msg_data_size;
        show_vuser_flow_gp_idx = group_count;
        call_scalar = 0;
        NSDL2_GDF(NULL, NULL, "vuser_flow_gp_ptr = %p, show_vuser_flow_idx = %u, numVector = %d",
                               vuser_flow_gp_ptr, show_vuser_flow_idx, numVector);
        break ;
      }
      else {
        NSDL2_GDF(NULL, NULL, "numVector = %d, so returning.", numVector);
        return;
      }
     
     case SRV_IP_STAT_GRP_ID:
      if(!srv_ip_data_gp_idx || g_rtg_rewrite){
        // rtg_index_array[SRV_IP_STAT] = msg_data_size;
         //srv_ip_stat_gp_ptr = (SrvIPStatGP *)msg_data_size;
        srv_ip_data_gp_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].gp_info_idx = srv_ip_data_gp_idx;
      }
      call_scalar = 0;
      break;
    case RBU_DOMAIN_STAT_GRP_ID:
      if(!dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].is_gp_info_filled || g_rtg_rewrite)
      {
        NSDL2_GDF(NULL, NULL, "RTG index msg_data_size = %d", msg_data_size);
        rtg_index_array[RBU_DOMAIN] = msg_data_size;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].gp_info_idx = rbu_domain_stat_data_gp_idx = group_count;

        NSDL2_GDF(NULL, NULL, "RBU_DOMAIN_STAT_GRP_ID: rbu_domain_stat_avg_idx = %u", rbu_domain_stat_data_gp_idx);
      }
      call_scalar = 0;
      break;

    //XMPP
    case XMPP_STAT_GRP_ID:
      rtg_index_array[XMPP_STATS] = msg_data_size;
      xmpp_stat_gp_ptr = (XMPP_gp *)msg_data_size;
      xmpp_stat_gp_idx = group_count;
      NSDL2_GDF(NULL, NULL, "xmpp_stat_gp_ptr = %p, xmpp_stat_gp_idx = %u", xmpp_stat_gp_ptr, xmpp_stat_gp_idx);
      //call_scalar = 1;
      break;

    case TCP_CLIENT_GRP_ID:
      rtg_index_array[TCP_CLIENT_RTG_IDX] = msg_data_size;
      g_tcp_client_rtg_ptr = (TCPClientRTGData *)msg_data_size;
      g_tcp_client_rpt_group_idx = group_count;
      NSDL2_GDF(NULL, NULL, "[SocketStats-TCPClient] msg_data_size = %ld, g_tcp_client_rtg_ptr = %p" 
        "g_tcp_client_rpt_group_idx = %d", 
        msg_data_size, g_tcp_client_rtg_ptr, g_tcp_client_rpt_group_idx); 
      break;

    case TCP_CLIENT_FAILURES_RPT_GRP_ID:
      if (!dynObjForGdf[NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES].gp_info_idx || g_rtg_rewrite)
      {
        rtg_index_array[TCP_CLIENT_FAILURES_RPT_GRP_ID] = msg_data_size;
        g_tcp_clinet_failures_rtg_ptr = (TCPClientFailureRTGData *)msg_data_size;
        g_tcp_clinet_failures_rpt_group_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES].gp_info_idx = g_tcp_clinet_failures_rpt_group_idx;
      }

      NSDL2_GDF(NULL, NULL, "[SocketStats-TCPClientFailures] msg_data_size = %ld, g_tcp_clinet_failures_rtg_ptr = %p" 
        "g_tcp_clinet_failures_rpt_group_idx = %d", 
        msg_data_size, g_tcp_clinet_failures_rtg_ptr, g_tcp_clinet_failures_rpt_group_idx); 

      call_scalar = 0;
      break;

    case UDP_CLIENT_GRP_ID:
      rtg_index_array[UDP_CLIENT_RTG_IDX] = msg_data_size;
      g_udp_client_rtg_ptr = (UDPClientRTGData *)msg_data_size;
      g_udp_client_rpt_group_idx = group_count;
      NSDL2_GDF(NULL, NULL, "[SocketStats-UDPClient] msg_data_size = %ld, g_udp_client_rtg_ptr = %p" 
        "g_udp_client_rpt_group_idx = %d", 
        msg_data_size, g_udp_client_rtg_ptr, g_udp_client_rpt_group_idx); 
      break;

    case UDP_CLIENT_FAILURES_RPT_GRP_ID:
      if (!dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].gp_info_idx || g_rtg_rewrite)
      {
        rtg_index_array[UDP_CLIENT_FAILURES_RPT_GRP_ID] = msg_data_size;
        g_udp_clinet_failures_rtg_ptr = (UDPClientFailureRTGData *)msg_data_size;
        g_udp_clinet_failures_rpt_group_idx = group_count;
        dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].gp_info_idx = g_udp_clinet_failures_rpt_group_idx;
      }

      NSDL2_GDF(NULL, NULL, "[SocketStats-UDPClientFailures] msg_data_size = %ld, g_udp_clinet_failures_rtg_ptr = %p" 
        "g_udp_clinet_failures_rpt_group_idx = %d", 
        msg_data_size, g_udp_clinet_failures_rtg_ptr, g_udp_clinet_failures_rpt_group_idx); 

      call_scalar = 0;
      break;

    default:
      NSDL2_GDF(NULL, NULL, "Default case, rpGroupID = %d, numVector = %d, total_ip_entries = %d, is_user_monitor = %d, "
                            "loader_opcode = %d, groupType = %s, info_ptr = %p", 
                             rpGroupID, numVector, total_ip_entries, is_user_monitor, loader_opcode, groupType, info_ptr);
      if(rpGroupID < GDF_CM_START_RPT_GRP_ID)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Report Group Id is not Correct line = %s, \n", grLine);
        NS_EXIT(-1, CAV_ERR_1013022, grLine);
      }

      if ((rpGroupID == IP_BASED_STAT_GRP_ID) && (loader_opcode != MASTER_LOADER)) {
        numVector = total_group_ip_entries;
        if (numVector) {
          rtg_index_array[IP_BASED] = msg_data_size;
          ip_data_gp_ptr = (IP_based_stat_gp *)msg_data_size;
          ip_data_idx = group_count;
          call_scalar = 0;
          NSDL2_GDF(NULL, NULL, "ip_data_gp_ptr = %p, ip_data_idx = %u, numVector = %d", ip_data_gp_ptr, ip_data_idx, numVector);
          break ;
        } 
        else {
          NSDL2_GDF(NULL, NULL, "total_group_ip_entries = %d, so returning.", total_group_ip_entries);
          return;
        } 
      }

      if (is_user_monitor && (loader_opcode == MASTER_LOADER)) 
      {
        fill_um_group_info(gdf_name, groupName, rpGroupID, numGraph);
        call_scalar = 0;
        break;
      }

      if(!strncasecmp(groupType, "scalar", strlen("scalar")) && !(info_ptr)) 
      {
        if (is_user_monitor) {
          fill_um_group_info(gdf_name, groupName, rpGroupID, numGraph);
          process_scalar_group(gdf_name, read_fp, grLine, groupName, rpGroupID, groupType, numGraph, groupMetric, groupDescription, numVector, groupHierarchy);
        }

      } else if(!strncasecmp(groupType, "vector", strlen("vector")) || (info_ptr))
        {

        if(info_ptr)
          numVector = ((CM_info *)info_ptr)->group_num_vectors;
        process_vector_group(read_fp, buffer, groupName, rpGroupID, groupType, numGraph, numField, numVector, 
                             gdf_name, groupMetric, groupDescription, info_ptr, groupHierarchy, is_user_monitor, dyn_obj_idx);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Group Type is not Correct line = %s\n", grLine);
        NS_EXIT(-1,CAV_ERR_1013023, grLine);
      }
      return;
  }

  if((call_scalar == 1)&&(loader_opcode != MASTER_LOADER))
    process_scalar_group(gdf_name, read_fp, grLine, groupName, rpGroupID, groupType, numGraph, groupMetric, groupDescription, numVector, groupHierarchy);
  else
    process_vector_group(read_fp, buffer, groupName, rpGroupID, groupType, numGraph, numField, numVector,
                             gdf_name, groupMetric, groupDescription, info_ptr, groupHierarchy, is_user_monitor, dyn_obj_idx);
}

//process gdf file , read info and group lines
//this function is not static because used in ns_test_gdf.c for test purpose.
inline void process_gdf(char *fname, int is_user_monitor, void *info_ptr, int dyn_obj_idx)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  int numGroup = 0;
  char tmpbuff[MAX_LINE_LENGTH];
  NSDL2_GDF(NULL, NULL, "Method called, processing gdf: %s", fname);
 
  read_gdf_fp = open_gdf(fname);
  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    NSDL3_GDF(NULL, NULL, "line = %s", line);
    if(sscanf(line, "%s", tmpbuff) == -1)
      continue;
    if(line[0] == '#' || line[0] == '\0')
      continue;
    else if(!(strncasecmp(line, "info|", strlen("info|"))))
    {
      numGroup = process_info(line);
      continue;
    }
    else if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      process_group(read_gdf_fp, line, numGroup, fname, is_user_monitor, info_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, dyn_obj_idx);
      continue;
    }
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      NSTL1_OUT(NULL, NULL, "Number of graphs are greater than no. of graph mentioned in group line of  %s monitor",
                                                                               group_data_ptr[total_groups_entries -1].group_name);
      continue;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", line);
      NS_EXIT(-1, CAV_ERR_1013024, line);
    }
  }
  NSDL3_GDF(NULL, NULL, "Closing gdf");
  close_gdf(read_gdf_fp);

  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
    "GDF variables after parsing the GDF '%s': msg_data_size = %ld, total_groups_entries = %d, "
    "max_groups_entries = %d, total_graphs_entries = %d, max_graphs_entries = %d, element_count = %d, "
    "group_count = %d, nc_msg_data_size = %ld, graph_count_mem = %d", 
     fname, msg_data_size, total_groups_entries, max_groups_entries, total_graphs_entries, 
     max_graphs_entries, element_count, group_count, nc_msg_data_size, graph_count_mem);
}



// allocate buffer of msg_data_size and set pointers for diffrent structures(groups)
//this function is not static because used in ns_test_gdf.c for test purpose.
inline void allocMsgBuffer()
{
  NSDL2_GDF(NULL, NULL, "Method called");
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Method called. tmp_msg_data_size = %d, msg_data_size = %d", tmp_msg_data_size, msg_data_size);

 
  if(tmp_msg_data_size > msg_data_size)
    msg_data_size = tmp_msg_data_size;
  else if(msg_data_size > tmp_msg_data_size)
  {
    if(tmp_msg_data_size > 0)
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Found msg_data_size (%ld) greater than tmp_msg_data_size (%ld). This should not happen.", msg_data_size, tmp_msg_data_size);

    tmp_msg_data_size = msg_data_size;
  }

  //when auto scale cleanup is enable we remove deleted vector form structure,so in this case it is possible that old_msg_data_size is greater then msg_data_size.
  if(msg_data_size > old_msg_data_size)
  {
    MY_REALLOC_AND_MEMSET(msg_data_ptr, msg_data_size, old_msg_data_size, "msg_data_ptr", -1);
    NSDL2_GDF(NULL, NULL, "msg_data_ptr = %p, old_msg_data_size = %d, msg_data_size = %d", msg_data_ptr, old_msg_data_size, msg_data_size);
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Here we are not doing memset for msg_data_ptr because old_msg_data_size [%ld] is greater than msg_data_size [%ld]", old_msg_data_size, msg_data_size);

    MY_REALLOC(msg_data_ptr, msg_data_size,"msg_data_ptr", -1);
  }

  old_msg_data_size = msg_data_size;

  if(g_runtime_flag == 0) // do not memset on runtime
    memset(msg_data_ptr, 0, msg_data_size);  //initilize memory

  if(rtg_index_array[VUSER] > 0)
    vuser_gp_ptr = (Vuser_gp *) (msg_data_ptr + rtg_index_array[VUSER]);
  if(rtg_index_array[NET_THROUGHPUT] > 0)
    ssl_gp_ptr = (SSL_gp*)(msg_data_ptr + rtg_index_array[NET_THROUGHPUT]);
  if(rtg_index_array[URL_HITS] > 0)
    url_hits_gp_ptr = (Url_hits_gp*)(msg_data_ptr + rtg_index_array[URL_HITS]);
  if(rtg_index_array[FC2_STATS] > 0)
    fc2_gp_ptr = (fc2_gp*)(msg_data_ptr + rtg_index_array[FC2_STATS]);
  if(rtg_index_array[PAGE_DOWNLOAD] > 0)
    page_download_gp_ptr = (Page_download_gp*)(msg_data_ptr + rtg_index_array[PAGE_DOWNLOAD]);
  if(rtg_index_array[SESSION_PTR] > 0)
    session_gp_ptr = (Session_gp*)(msg_data_ptr + rtg_index_array[SESSION_PTR]);
  if(rtg_index_array[TRANS_OVERALL] > 0)
    trans_overall_gp_ptr = (Trans_overall_gp*)(msg_data_ptr + rtg_index_array[TRANS_OVERALL]);
  if(rtg_index_array[URL_FAIL] > 0)
    url_fail_gp_ptr = (URL_fail_gp*)(msg_data_ptr + rtg_index_array[URL_FAIL]);
  if(rtg_index_array[PAGE_FAIL] > 0)
    page_fail_gp_ptr = (Page_fail_gp*)(msg_data_ptr + rtg_index_array[PAGE_FAIL]);
  if(rtg_index_array[SESSION_FAIL] > 0)
    session_fail_gp_ptr = (Session_fail_gp*)(msg_data_ptr + rtg_index_array[SESSION_FAIL]);
  if(rtg_index_array[TRANS_FAIL] > 0)
    trans_fail_gp_ptr = (Trans_fail_gp*)(msg_data_ptr + rtg_index_array[TRANS_FAIL]);
  if(rtg_index_array[NO_SYSTEM_STATS] > 0)
    no_system_stats_gp_ptr = (NO_system_stats_gp*)(msg_data_ptr + rtg_index_array[NO_SYSTEM_STATS]);
  if(rtg_index_array[NO_NETWORK_STATS] > 0)
    no_network_stats_gp_ptr = (NO_network_stats_gp*)(msg_data_ptr + rtg_index_array[NO_NETWORK_STATS]);
  if(rtg_index_array[SERVER_STATS] > 0)
    server_stats_gp_ptr = (Server_stats_gp*)(msg_data_ptr + rtg_index_array[SERVER_STATS]);
  if(rtg_index_array[TRANS_STATS] > 0)
    trans_stats_gp_ptr = (Trans_stats_gp*)(msg_data_ptr + rtg_index_array[TRANS_STATS]);
  if(rtg_index_array[TRANS_CUM_STATS] > 0)
    trans_cum_stats_gp_ptr = (Trans_cum_stats_gp*)(msg_data_ptr + rtg_index_array[TRANS_CUM_STATS]);
  if(rtg_index_array[TUNNEL_STATS] > 0) 
    tunnel_stats_gp_ptr = (Tunnel_stats_gp*)(msg_data_ptr + rtg_index_array[TUNNEL_STATS]);
   /* WS */
  if(rtg_index_array[WS_STATS] > 0) {
    ws_stats_gp_ptr = (WSStats_gp *)(msg_data_ptr + rtg_index_array[WS_STATS]);
    NSDL2_GDF(NULL, NULL, "ws_stats_gp_ptr = %p, msg_data_ptr = %p, ws_stats_gp_ptr = %lu", ws_stats_gp_ptr, msg_data_ptr, ws_stats_gp_ptr);
  }
  if(rtg_index_array[WS_STATUS_CODES] > 0) {
    ws_status_codes_gp_ptr = (WSStatusCodes_gp *)(msg_data_ptr + rtg_index_array[WS_STATUS_CODES]);
    NSDL2_GDF(NULL, NULL, "ws_status_codes_gp_ptr = %p", ws_status_codes_gp_ptr);
  }
  
  if(rtg_index_array[WS_FAILURE_STATS] > 0) {
    ws_failure_stats_gp_ptr = (WSFailureStats_gp *)(msg_data_ptr + rtg_index_array[WS_FAILURE_STATS]);
    NSDL2_GDF(NULL, NULL, "ws_failure_stats_gp_ptr = %p", ws_failure_stats_gp_ptr);
  }

  // For SMTP 
  if(total_smtp_request_entries) {
  if(rtg_index_array[SMTP_NET_THROUGHPUT] > 0)
      smtp_net_throughput_gp_ptr = (SMTP_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[SMTP_NET_THROUGHPUT]);
    
  if(rtg_index_array[SMTP_HITS] > 0)
      smtp_hits_gp_ptr = (SMTP_hits_gp*)(msg_data_ptr + rtg_index_array[SMTP_HITS]);
    
    if(smtp_fail_gp_ptr != NULL)
  if(rtg_index_array[SMTP_FAIL] > 0)
      smtp_fail_gp_ptr = (SMTP_fail_gp*)(msg_data_ptr + rtg_index_array[SMTP_FAIL]);
  }

  /* pop3 */
  if(total_pop3_request_entries) {
  if(rtg_index_array[POP3_NET_THROUGHPUT] > 0)
      pop3_net_throughput_gp_ptr = (POP3_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[POP3_NET_THROUGHPUT]);
    
  if(rtg_index_array[POP3_HITS] > 0)
      pop3_hits_gp_ptr = (POP3_hits_gp*)(msg_data_ptr + rtg_index_array[POP3_HITS]);
    
  if(rtg_index_array[POP3_FAIL] > 0)
      pop3_fail_gp_ptr = (POP3_fail_gp*)(msg_data_ptr + rtg_index_array[POP3_FAIL]);
  }

  /* FTP */
  if(total_ftp_request_entries) {
  if(rtg_index_array[FTP_NET_THROUGHPUT] > 0)
      ftp_net_throughput_gp_ptr = (FTP_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[FTP_NET_THROUGHPUT]);
    
  if(rtg_index_array[FTP_HITS] > 0)
      ftp_hits_gp_ptr = (FTP_hits_gp*)(msg_data_ptr + rtg_index_array[FTP_HITS]);
    
  if(rtg_index_array[FTP_FAIL] > 0)
      ftp_fail_gp_ptr = (FTP_fail_gp*)(msg_data_ptr + rtg_index_array[FTP_FAIL]);
  }

  /* LDAP */
  if(total_ldap_request_entries) {
  if(rtg_index_array[LDAP_NET_THROUGHPUT] > 0)
      ldap_net_throughput_gp_ptr = (LDAP_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[LDAP_NET_THROUGHPUT]);
    
  if(rtg_index_array[LDAP_HITS] > 0)
      ldap_hits_gp_ptr = (LDAP_hits_gp*)(msg_data_ptr + rtg_index_array[LDAP_HITS]);
    
  if(rtg_index_array[LDAP_FAIL] > 0)
      ldap_fail_gp_ptr = (LDAP_fail_gp*)(msg_data_ptr + rtg_index_array[LDAP_FAIL]);
  }

  /* IMAP */
  if(total_imap_request_entries) {
  if(rtg_index_array[IMAP_NET_THROUGHPUT] > 0)
      imap_net_throughput_gp_ptr = (IMAP_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[IMAP_NET_THROUGHPUT]);
    
  if(rtg_index_array[IMAP_HITS] > 0)
      imap_hits_gp_ptr = (IMAP_hits_gp*)(msg_data_ptr + rtg_index_array[IMAP_HITS]);
    
  if(rtg_index_array[IMAP_FAIL] > 0)
      imap_fail_gp_ptr = (IMAP_fail_gp*)(msg_data_ptr + rtg_index_array[IMAP_FAIL]);
  }

  /* JRMI */
  if(total_jrmi_request_entries) {
  if(rtg_index_array[JRMI_NET_THROUGHPUT] > 0)
      jrmi_net_throughput_gp_ptr = (JRMI_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[JRMI_NET_THROUGHPUT]);
    
  if(rtg_index_array[JRMI_HITS] > 0)
      jrmi_hits_gp_ptr = (JRMI_hits_gp*)(msg_data_ptr + rtg_index_array[JRMI_HITS]);
    
  if(rtg_index_array[JRMI_FAIL] > 0)
      jrmi_fail_gp_ptr = (JRMI_fail_gp*)(msg_data_ptr + rtg_index_array[JRMI_FAIL]);
  }

   //XMPP
   if(rtg_index_array[XMPP_STATS] > 0) {
    xmpp_stat_gp_ptr = (XMPP_gp *)(msg_data_ptr + rtg_index_array[XMPP_STATS]);
    NSDL2_GDF(NULL, NULL, "Setting xmpp_stat_gp_ptr = %p, rtg_index_array[XMPP_STATS] = %d", 
                            xmpp_stat_gp_ptr, rtg_index_array[XMPP_STATS]);
  }

  if (rtg_index_array[TCP_CLIENT_RTG_IDX] > 0) {
    g_tcp_client_rtg_ptr = (TCPClientRTGData *)(msg_data_ptr + rtg_index_array[TCP_CLIENT_RTG_IDX]);
    NSDL2_GDF(NULL, NULL, "Setting g_tcp_client_rtg_ptr, = %p, rtg_index_array[TCP_CLIENT_RTG_IDX] = %d", 
      g_tcp_client_rtg_ptr, rtg_index_array[TCP_CLIENT_RTG_IDX]);
  }

  if (rtg_index_array[TCP_CLIENT_FAILURES_RTG_IDX] > 0) {
    g_tcp_clinet_failures_rtg_ptr = (TCPClientFailureRTGData *)(msg_data_ptr + rtg_index_array[TCP_CLIENT_FAILURES_RTG_IDX]);
    NSDL2_GDF(NULL, NULL, "Setting g_tcp_client_rtg_ptr, = %p, rtg_index_array[TCP_CLIENT_FAILURES_RTG_IDX] = %d", 
      g_tcp_clinet_failures_rtg_ptr, rtg_index_array[TCP_CLIENT_FAILURES_RTG_IDX]);
  }

  if (rtg_index_array[UDP_CLIENT_RTG_IDX] > 0) {
    g_udp_client_rtg_ptr = (UDPClientRTGData *)(msg_data_ptr + rtg_index_array[UDP_CLIENT_RTG_IDX]);
    NSDL2_GDF(NULL, NULL, "Setting g_udp_client_rtg_ptr, = %p, rtg_index_array[UDP_CLIENT_RTG_IDX] = %d", 
      g_udp_client_rtg_ptr, rtg_index_array[UDP_CLIENT_RTG_IDX]);
  }

  if (rtg_index_array[UDP_CLIENT_FAILURES_RTG_IDX] > 0) {
    g_udp_clinet_failures_rtg_ptr = (UDPClientFailureRTGData *)(msg_data_ptr + rtg_index_array[TCP_CLIENT_FAILURES_RTG_IDX]);
    NSDL2_GDF(NULL, NULL, "Setting g_udp_client_rtg_ptr, = %p, rtg_index_array[UDP_CLIENT_FAILURES_RTG_IDX] = %d", 
      g_udp_clinet_failures_rtg_ptr, rtg_index_array[UDP_CLIENT_FAILURES_RTG_IDX]);
  }

  /* DNS */
  if(total_dns_request_entries) {
  if(rtg_index_array[DNS_NET_THROUGHPUT] > 0)
      dns_net_throughput_gp_ptr = (DNS_Net_throughput_gp*)(msg_data_ptr + rtg_index_array[DNS_NET_THROUGHPUT]);
    
  if(rtg_index_array[DNS_HITS] > 0)
      dns_hits_gp_ptr = (DNS_hits_gp*)(msg_data_ptr + rtg_index_array[DNS_HITS]);
    
  if(rtg_index_array[DNS_FAIL] > 0)
      dns_fail_gp_ptr = (DNS_fail_gp*)(msg_data_ptr + rtg_index_array[DNS_FAIL]);
  }

  if(rtg_index_array[HTTP_STATUS_CODES] > 0)
    http_status_codes_gp_ptr = (Http_Status_Codes_gp*)(msg_data_ptr + rtg_index_array[HTTP_STATUS_CODES]);

  if(rtg_index_array[HTTP_CACHING] > 0)
    http_caching_gp_ptr = (HttpCaching_gp *)(msg_data_ptr + rtg_index_array[HTTP_CACHING]);

  if(rtg_index_array[NS_DIAG] > 0)
    ns_diag_gp_ptr = (NSDiag_gp *)(msg_data_ptr + rtg_index_array[NS_DIAG]);

  //Dos Attack
  if(rtg_index_array[DOS_ATTACK] > 0) {
    dos_attack_gp_ptr = (DosAttack_gp *)(msg_data_ptr + rtg_index_array[DOS_ATTACK]);
    NSDL2_GDF(NULL, NULL, "dos_attack_gp_ptr = %p, msg_data_ptr = %p, dos_attack_gp_ptr = %lu", dos_attack_gp_ptr, msg_data_ptr, dos_attack_gp_ptr);
  }

  //HTTP Proxy
  if(rtg_index_array[HTTP_PROXY] > 0) {
    http_proxy_gp_ptr = (HttpProxy_gp *)(msg_data_ptr + rtg_index_array[HTTP_PROXY]);
    NSDL2_GDF(NULL, NULL, "http_proxy_gp_ptr= %p, msg_data_ptr = %p, http_proxy_gp_ptr= %lu", http_proxy_gp_ptr, msg_data_ptr, http_proxy_gp_ptr);
  }

  if(rtg_index_array[HTTP_NETWORK_CACHE_STATS] > 0) {
    http_network_cache_stats_gp_ptr = (HttpNetworkCacheStats_gp *)(msg_data_ptr + rtg_index_array[HTTP_NETWORK_CACHE_STATS]);
    NSDL2_GDF(NULL, NULL, "http_network_cache_stats_gp_ptr= %p, msg_data_ptr = %p, http_network_cache_stats_gp_ptr= %lu", http_network_cache_stats_gp_ptr, msg_data_ptr, http_network_cache_stats_gp_ptr);
  }

  if(rtg_index_array[DNS_LOOKUP_STATS] > 0) {
    dns_lookup_stats_gp_ptr = (DnsLookupStats_gp *)(msg_data_ptr + rtg_index_array[DNS_LOOKUP_STATS]);
    NSDL2_GDF(NULL, NULL, "dns_lookup_stats_gp_ptr= %p, msg_data_ptr = %p, dns_lookup_stats_gp_ptr= %lu", dns_lookup_stats_gp_ptr, msg_data_ptr, dns_lookup_stats_gp_ptr);
  }

  //Group data
  if(rtg_index_array[GROUP_DATA] > 0){
    group_data_gp_ptr = (Group_data_gp*)(msg_data_ptr + rtg_index_array[GROUP_DATA]);
    NSDL2_GDF(NULL, NULL, "group_data_gp_ptr = %p, msg_data_ptr = %p, group_data_gp_ptr = %lu", 
                           group_data_gp_ptr, msg_data_ptr, group_data_gp_ptr);
  }

  if(rtg_index_array[RBU_PAGE] > 0)
    rbu_page_stat_data_gp_ptr = (RBU_Page_Stat_data_gp*)(msg_data_ptr + rtg_index_array[RBU_PAGE]);

   
  if(rtg_index_array[PAGE_BASED] > 0)
    page_based_stat_gp_ptr = (Page_based_stat_gp*)(msg_data_ptr + rtg_index_array[PAGE_BASED]);
 
  //for ip
  if(rtg_index_array[IP_BASED] > 0)
    ip_data_gp_ptr = (IP_based_stat_gp*)(msg_data_ptr + rtg_index_array[IP_BASED]);

  if(rtg_index_array[VUSER_FLOW_BASED] > 0)
    vuser_flow_gp_ptr = (VUserFlowBasedGP*)(msg_data_ptr + rtg_index_array[VUSER_FLOW_BASED]);

   if(rtg_index_array[RBU_DOMAIN] > 0) {
    rbu_domain_stat_gp_ptr = (Rbu_domain_time_data_gp*)(msg_data_ptr + rtg_index_array[RBU_DOMAIN]);
    NSDL2_GDF(NULL, NULL, "Setting rbu_domain_stat_gp_ptr = %p, rtg_index_array[RBU_DOMAIN] = %d", 
                            rbu_domain_stat_gp_ptr, rtg_index_array[RBU_DOMAIN]);
   }
   /*bug 70480 : for  http2 server push*/
   if(rtg_index_array[SERVER_PUSH_STATS] > 0)
     http2_srv_push_gp_ptr = (Http2SrvPush_gp*)(msg_data_ptr + rtg_index_array[SERVER_PUSH_STATS]);
}

//This function is used to fill the header of testrun_all_gdf_info.txt file
void fill_testrun_gdf_hdr_info()
{
  if(all_gdf_fp)
  {
    fprintf(all_gdf_fp,"#INFO|VERSION|GRP_CNT|SZF_MSG_DATA_HDR|TRUN|SZF_MSG_DATA|PROG MSEC|TR_START_TIME|GDF_CNT|RTG_PKT_TS\n");
    fflush(all_gdf_fp);
  }
}

//This function will create the testrun_all_gdf_info.txt file and append the test run gdf data in the file.
int append_testrun_all_gdf_info()
{
  char file_name[1024];
  int ret = 0;
  MLTL3(EL_D, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Function called to create file testrun_all_gdf_info.txt with test_run_gdf_count %d and rtg_pkt_ts %lld",test_run_gdf_count,rtg_pkt_ts);
  sprintf(file_name,"%s/logs/%s/testrun_all_gdf_info.txt",g_ns_wdir,global_settings->tr_or_partition);
  if(rtg_msg_seq <= 1 )  //This check is code as we want to close testrun_all_gdf_info.txt fp at the end of the current partition.
  {
    if(all_gdf_fp)
    {
      prev_version = -1;
      CLOSE_FP(all_gdf_fp);
    }
    all_gdf_fp = fopen(file_name, "a");
    if(!all_gdf_fp)    //error case if fopen fails
    {
      NSTL1_OUT(NULL, NULL, CAV_ERR_1060081, file_name, strerror(errno));
    }
    else
    {
      fill_testrun_gdf_hdr_info();
    }
  }
  if(test_run_gdf_count != prev_version)//This check is done as we want to create a file and keep the track of previous version of the testrun 
  {                                                                                                                     //gdf file   
    if(all_gdf_fp)
    {
      int testrun_len = strlen(testrun_buff);
      sprintf(testrun_buff + testrun_len,"|%lf\n", rtg_pkt_ts); //done to write data of test run gdf to a file
      fprintf(all_gdf_fp,"%s",testrun_buff);
      ret =  fflush(all_gdf_fp);                                                     
      if(ret != 0)
      {
        NSTL1_OUT(NULL, NULL, CAV_ERR_1060081, file_name, strerror(errno));
      } 
      MLTL3(EL_D, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                   "file testrun_all_gdf_info.txt created sucessfully");
      prev_version = test_run_gdf_count;                           //setting previous version to test run gdf count number
    }
  }
  return 0;
}

//  append all relavent lines
inline void fillTestRunGdf(FILE *testRun_fp)
{
  FILE *read_gdf_fp;
  char printLine[MAX_LINE_LENGTH];
  char line[MAX_LINE_LENGTH];
  char filename[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called");
  sprintf(filename, "/tmp/%d_tmp.gdf",testidx);     // making
  if ((read_gdf_fp = fopen(filename, "r+")) == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening file %s\n", filename);
    perror("fopen");
    NS_EXIT(-1, CAV_ERR_1013003, filename);
  }
  NSDL3_GDF(NULL, NULL, "group_count = %d, StartIndex = %d msg_data_size =%ld",
                         group_count ,sizeof(Msg_data_hdr), msg_data_size);
  //In testrun.gdf now we have increase count to 9 as we assume partition index 
  //will be added in case of continues monitoring 
  //default -1
  sprintf(printLine,"Info|%s|%d|%d|%u|%ld|%d|%s\n", 
                     version, group_count, (unsigned int)sizeof(Msg_data_hdr),
                     testidx, msg_data_size,
                     global_settings->progress_secs, g_test_start_time);
  //making buffer for testrun_all_gdf_info.txt
  sprintf(testrun_buff,"Info|%s|%d|%d|%u|%ld|%d|%s|%d",
                     version, group_count, (unsigned int)sizeof(Msg_data_hdr),
                     testidx, msg_data_size,
                     global_settings->progress_secs, g_test_start_time,test_run_gdf_count); 

  NSDL3_GDF(NULL, NULL, "printLine = %s", printLine);

  fprintf(testRun_fp, "%s\n", printLine);
  while(nslb_fgets(line, sizeof(line), read_gdf_fp, 0) != NULL)
      fputs(line, testRun_fp);
  close_gdf(read_gdf_fp);
  unlink(filename); // to remove the file from temp.
}

// create TestRunGdf
//this function is not static because used in ns_test_gdf.c for test purpose.
inline void create_testrun_gdf(int runtime_flag)
{
  FILE *testRun_fp =  NULL;
  char fname[MAX_LINE_LENGTH];
  char new_buf[MAX_LINE_LENGTH];
  char old_buf[MAX_LINE_LENGTH + 1];
  
  g_testrun_rtg_timestamp = time(NULL) * 1000;

  NSDL2_GDF(NULL, NULL, "Method called, Creating testrun file. runtime_flag = %d ", runtime_flag);

  if(runtime_flag)
  {
    //close old rtgMessage.dat
    if(rtg_fp != NULL) { fclose(rtg_fp); rtg_fp = NULL; }

    //Increment testrun.gdf/rtgMessage.dat name counter
    test_run_gdf_count++;
    NSDL2_GDF(NULL, NULL, "After incrementing counter. test_run_gdf_count = [%d]", test_run_gdf_count);
  }
    
  /*open new testrun.gdf & rtgMessage.dat 
  In non-partition test:
  1> Close old files, Open new files
  2> NS will keep a static counter 'test_run_gdf_count' & will increment this counter on every add/delete.
  3> Will use this counter in file name: 'testrun.gdf.<test_run_gdf_count>' & 'rtgMessage.dat.<test_run_gdf_count>'
  4> If (test_run_gdf_count == 0) then create file without counter i.e.  testrun.gdf & rtgMessage.dat */
  if(test_run_gdf_count == 0)
    sprintf(fname, "%s/logs/%s/testrun.gdf", g_ns_wdir, global_settings->tr_or_partition); 
  else
    sprintf(fname, "%s/logs/%s/testrun.gdf.%d.%llu", g_ns_wdir, global_settings->tr_or_partition, test_run_gdf_count, g_testrun_rtg_timestamp);

  if ((testRun_fp = fopen(fname, "w+")) == NULL)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in opening file %s\n", fname);
     perror("fopen");
     NS_EXIT(-1,CAV_ERR_1013003, fname);
  }
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Going to fill gdf '%s' from '/tmp/%d_tmp.gdf' test_run_gdf_count = %d.", fname, testidx, test_run_gdf_count);
  fillTestRunGdf(testRun_fp);
  close_gdf(testRun_fp);
  NSDL3_GDF(NULL, NULL, "Created a testrun file");

  /*Before sending start msg to server, MUST create testrun.gdf and rtgMessage.dat*/
  if(runtime_flag)
  {
    //open new rtgMessage.dat with name rtgMessage.dat.<counter>
    create_rtg_file_data_file_send_start_msg_to_server(test_run_gdf_count);
  }
 
  if(test_run_gdf_count != 0)
  {
    sprintf(old_buf, "%s", fname);
    sprintf(new_buf, "%s/logs/TR%d/%lld/testrun.gdf.%d", g_ns_wdir, testidx, g_partition_idx, test_run_gdf_count);
    //link(old_buf, new_buf);
    if((link(old_buf, new_buf)) == -1)
    {
    if((symlink(old_buf, new_buf)) == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Unable to create link %s, err = %s", new_buf, nslb_strerror(errno));
      }
    }
    NSDL2_GDF(NULL, NULL, "Created link of %s in %s", old_buf, new_buf);
  }
}

int check_if_cm_vector(const void *cm1, const void *cm2) 
{
  int ret;
  //int temp;

  if((((Monitor_list *)cm1)->cm_info_mon_conn_ptr)->flags & REMOVE_MONITOR)		//To shift the monitor to be removed to the bottom
    return 1;

  ret = strcmp(((Monitor_list *)cm1)->gdf_name, ((Monitor_list *)cm2)->gdf_name);

  return ret;
}

int check_if_all_vectors_new(int mon_index)
{
  int i;
  CM_info *cm_ptr = NULL;
  for(i = 0; i < monitor_list_ptr[mon_index].no_of_monitors; i++)
  {
    cm_ptr = monitor_list_ptr[mon_index + i].cm_info_mon_conn_ptr;
    if(cm_ptr->new_vector_first_index > 0)
      return 0;
  }
  return 1;
}

int handle_testrun_gdf_for_HML(void *mon_ptr, int mon_entries, int runtime)
{
  int i;

  CM_info *info_ptr = NULL;

  Group_Info *lol_group_info_ptr = NULL;
  Graph_Info *lol_graph_info_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called, Write testrun.gdf for HML groups, mon_ptr = %p, mon_entries = %d, runtime = %d, "
                        "enable_hml_group_in_testrun_gdf = %d, g_rtg_rewrite = %d", 
                         mon_ptr, mon_entries, runtime, global_settings->enable_hml_group_in_testrun_gdf, g_rtg_rewrite);

  if(!global_settings->enable_hml_group_in_testrun_gdf)
    return 0;

  if(!runtime || g_rtg_rewrite)
  {
    //Set start index for High, Medium and Low vectors
    hml_msg_data_size[METRIC_PRIORITY_HIGH][HML_START_RTG_IDX] = hml_start_rtg_idx;

    hml_msg_data_size[METRIC_PRIORITY_MEDIUM][HML_START_RTG_IDX] = hml_msg_data_size[METRIC_PRIORITY_HIGH][HML_START_RTG_IDX] + hml_msg_data_size[METRIC_PRIORITY_HIGH][HML_MSG_DATA_SIZE];

    hml_msg_data_size[METRIC_PRIORITY_LOW][HML_START_RTG_IDX] = hml_msg_data_size[METRIC_PRIORITY_MEDIUM][HML_START_RTG_IDX] + hml_msg_data_size[METRIC_PRIORITY_MEDIUM][HML_MSG_DATA_SIZE];

  }

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "RTG packet size [H,M,L,D] = [%d,%d,%d,%d],  start index [H,M,L] = [%d,%d,%d], "
                    "mon_ptr = %p, mon_entries = %d, runtime = %d, g_rtg_rewrite = %d",
                         hml_msg_data_size[METRIC_PRIORITY_HIGH][HML_MSG_DATA_SIZE],
                         hml_msg_data_size[METRIC_PRIORITY_MEDIUM][HML_MSG_DATA_SIZE],
                         hml_msg_data_size[METRIC_PRIORITY_LOW][HML_MSG_DATA_SIZE],
                         hml_msg_data_size[MAX_METRIC_PRIORITY_LEVELS][HML_MSG_DATA_SIZE],
                         hml_msg_data_size[METRIC_PRIORITY_HIGH][HML_START_RTG_IDX],
                         hml_msg_data_size[METRIC_PRIORITY_MEDIUM][HML_START_RTG_IDX],
                         hml_msg_data_size[METRIC_PRIORITY_LOW][HML_START_RTG_IDX],
                         mon_ptr, mon_entries, runtime, g_rtg_rewrite);
 
  //Allocate memory for member of array hml_metrics[]
  CREATE_HML_METRIC_ARR(mon_entries);

  for(i = 0; i < mon_entries;)
  {
    info_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr; 

    NSDL2_GDF(NULL, NULL, "Loop(mon_entries): i = %d, GDF name = %s, gp_info_index = %d, group_num_vectors = %d", 
                           i, info_ptr->gdf_name, info_ptr->gp_info_index, info_ptr->group_num_vectors);

    if(info_ptr->gp_info_index == -1) //Skypping Dummy Vectros
    {
      i++;
      continue;
    }

    //Reset array hml_metrics[] to reuse for this vector 
    RESET_HML_METRIC_ARR

    lol_group_info_ptr = group_data_ptr + info_ptr->gp_info_index;
    lol_graph_info_ptr = graph_data_ptr + lol_group_info_ptr->graph_info_index;

    /* Set RTG index for all vectors of this group, create a vector list according to H/M/L and fill appropriately in hml_metrics[] */
    set_rtg_index_in_cm_info(lol_group_info_ptr->rpt_grp_id, info_ptr, 0, runtime);

    //get_deleted_vector_count(info_ptr, info_ptr->group_num_vectors, i);

    /* Get Group line for H/M/L and fill appropriately in hml_metrics[] */
    FILL_HML_GROUP_LINE(lol_group_info_ptr);

    /* Get Graph line for H/M/L and fill appropriately in hml_metrics[] */
    FILL_HML_GRAPH_LINE(lol_group_info_ptr, lol_graph_info_ptr);

    /* Dump infomation of Group, Graphs and Vectors according to H/M/L in testrun.gdf file. */
    WRITE_HML_IN_TESTRUN_GDF(write_gdf_fp);

    NSDL2_GDF(NULL, NULL, "i = %d, info_ptr = %p, group_num_vectors = %d", i, info_ptr, info_ptr->group_num_vectors);
    i += monitor_list_ptr[i].no_of_monitors;
  }

  //Release memory of all the member of array hml_metrics[]
  FREE_HML_METRIC_ARR

  //Reset hml_msg_data_size  
  init_hml_msg_data_size();

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Method exit, handle_testrun_gdf_for_HML Done!!, mon_entries = %d, runtime = %d", mon_entries, runtime);
  return 0;
}

void process_custom_gdf(int runtime_flag)
{
  char *cm_gdf_name;
  //int cm_gp_info_idx,  //TODO ?? WHY CHANGED EARLIER
  int cm_num_graphs; // For storing in cm_info_ptr
  int save_group_count, save_element_count;
  int cm_num_groups;
  int i = 0, ret;
  CM_info *cm_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  int mon_index = 0;

  /**
   * Sort CM_info to find entries with same GDF. Also mark same entries as vectors.
   */
/*  if(!runtime_flag)
  {
    qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);
  }*/

  /* Initialize vector group stuff. */
  initialize_cm_vector_groups(g_runtime_flag);

  //: Archana
  /* We add dynamic custom monitors after sorting and initialization
   * since the new vectors must be as is and should not be sorted.  */
  //TODO : Remove addition of dynamic monitor  MSR
  /*if(!runtime_flag)
  {
    char err_msg[1024] = {0};
    add_dynamic_custom_monitors(NORMAL_CM_TABLE, err_msg);
  }*/

  if(is_outbound_connection_enabled)
    add_entry_in_mon_id_map_table();

  // Set hml_msg_data_size 
  init_hml_msg_data_size();

  //Save start rtg index of HML 
  if(hml_start_rtg_idx == -1)
  {
    hml_start_rtg_idx = msg_data_size;
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "hml_start_rtg_idx = %d", hml_start_rtg_idx);
  }

  while(1)
  {
    NSDL2_GDF(NULL, NULL, "mon_index = %d, total_monitor_list_entries = %d", mon_index, total_monitor_list_entries);
    if (mon_index == total_monitor_list_entries) 
      break;

    cm_gdf_name = monitor_list_ptr[mon_index].gdf_name;
    cm_ptr = monitor_list_ptr[mon_index].cm_info_mon_conn_ptr;

    save_group_count = group_count;
    save_element_count = element_count;

    //In the case of Log Event Moniter gdf name is NA
    //And in process_gdf(), we open the gdf file
    
    if(strstr(cm_gdf_name,"cm_hm_ns_tier_server_count.gdf") != NULL)
    {
      g_tier_server_count_index=mon_index;
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Index for cm_hm_ns_tier_server_count.gdf = %d", g_tier_server_count_index);
    }    

    if(strncmp(cm_gdf_name, "NA", 2))
    {
      deleted_monitor_count = get_deleted_monitor_count(cm_ptr, monitor_list_ptr[mon_index].no_of_monitors, mon_index);
      if((deleted_monitor_count != monitor_list_ptr[mon_index].no_of_monitors) && cm_ptr->group_num_vectors != 0)
        process_gdf(cm_gdf_name, 0, (void *)(cm_ptr), 0);

      deleted_monitor_count = 0;
    }
    //For Monitors having NA group we need to set cm_index so that after parsing mon_id it can map correclty to parent in cm_info table.
    else if(cm_ptr->flags & OUTBOUND_ENABLED)
      set_index_for_NA_group(cm_ptr, monitor_list_ptr[mon_index].no_of_monitors, mon_index);

    cm_num_groups = (group_count - save_group_count);

    cm_num_graphs = (element_count - save_element_count);

    //for nd backend monitors we need to set the data pointers

    if(cm_ptr->flags & ND_MONITOR)
    {
      set_nd_backend_mon_data_ptrs(mon_index, monitor_list_ptr[mon_index].no_of_monitors);
    }
    //else if((nv_monitor_start_idx == -2) && (strstr(cm_info_ptr[cm_index].gdf_name, "cm_nv_")) && (cm_info_ptr[cm_index].is_dynamic))
    else if((cm_ptr->flags & NV_MONITOR) && (cm_ptr->flags & DYNAMIC_MONITOR))
    {
      // SET DATA POINTER FOR NV MONITORS
      for(i = mon_index; i < monitor_list_ptr[mon_index].no_of_monitors; i++)
        set_nv_mon_data_ptrs(i, monitor_list_ptr[i].cm_info_mon_conn_ptr);
    }

/*
    //Setting Data Pointers for NetCloud monitor
    else if (cm_ptr->cs_port == -1)
      set_netcloud_data_ptrs(mon_index);
*/
    /* function set_cm_info_values() sets gp_info_index and initializes required fields
     * in cm_info_ptr. It loops for num vectors and returns the count with which we update
     * cm_index. */
    // NOTE : IF GDF NAME IS 'NA' THEN ALSO WILL BE SETTING gp_info_index in cm_info structure

    ret = set_cm_info_values(mon_index, cm_num_groups, cm_num_graphs,
                                   cm_ptr->group_num_vectors,
                                   save_group_count);

    if(ret > 0)
      mon_index += ret;
  }
  
  //Create testrun.gdf file for custome monitor for first time only
  handle_testrun_gdf_for_HML(monitor_list_ptr, total_monitor_list_entries, runtime_flag);
}

/************** gdf indexing functions ******************/

void save_testrungdf_in_buffer()
{
  FILE *ftmp = NULL;
  char filename[MAX_LINE_LENGTH];
  long bytes_read = 0;

  NSDL2_GDF(NULL, NULL, "Method Called");

  //open file
  sprintf(filename, "/tmp/%d_tmp.gdf", testidx);   

  if ((ftmp = fopen(filename, "r")) == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in opening file %s\n", filename);
    perror("fopen");
    NS_EXIT(-1, CAV_ERR_1013003, filename);
  }

  //calculate size
  fseek(ftmp, 0, SEEK_END);  
  fsize = ftell(ftmp);
  fseek(ftmp, 0, SEEK_SET);

  //allocate memory & save data in buffer
  MY_MALLOC (testrungdf_buffer, fsize + 1, "testrungdf_buffer", -1);
  
  bytes_read = fread(testrungdf_buffer, sizeof(char), fsize, ftmp);

  if(bytes_read != fsize) 
  {
    if(ferror(ftmp)) // ferror() tests the error indicator for the stream pointed to by stream
    {
      NSTL1_OUT(NULL, NULL, "Found input error.");
      //NS_EXIT(-1,"Default Message"); // TODO DISCUSS ?????????
    }

    if(feof(ftmp)) // feof() tests the end-of-file indicator for the stream pointed to by stream
    {
      NSTL1_OUT(NULL, NULL, "Found end of file.");
      //NS_EXIT(-1,"Default Message");
    }
  }

  fclose(ftmp);
  testrungdf_buffer[fsize] = '\0';  
}

void write_data(void *info_ptr, CM_info *cm_ptr, int fill_2d_flag, char ***TwoD)
{
  int i, j, no_of_monitors = monitor_list_ptr[cm_ptr->monitor_list_idx].no_of_monitors, mon_index = cm_ptr->monitor_list_idx, group_vector_index = 0;
  CM_info *local_cm_info = NULL; 
  CM_vector_info *local_cm_vector_info = NULL;


  for (i = mon_index; i < mon_index + no_of_monitors; i++)
  {
    local_cm_info = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    local_cm_vector_info = local_cm_info->vector_list;
    for(j = 0; j <local_cm_info->total_vectors; j++)
    {
      fill_2d(*TwoD, group_vector_index, local_cm_vector_info[j].vector_name);

      if(local_cm_info->flags & ALL_VECTOR_DELETED)
      {
        group_vector_index++;
        continue;
      }

      if(local_cm_vector_info[j].flags & RUNTIME_DELETED_VECTOR) //add minus sign before breadcrumb/vector name
      {
        if((global_settings->hierarchical_view) && (local_cm_vector_info[j].flags & MON_BREADCRUMB_SET))
          fprintf(write_gdf_fp, "#-%s %ld\n", local_cm_vector_info[j].mon_breadcrumb, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
        else
          fprintf(write_gdf_fp, "#-%s %ld\n", local_cm_vector_info[j].vector_name, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
      }
      else
      {
        if((global_settings->hierarchical_view) && (local_cm_vector_info[j].flags & MON_BREADCRUMB_SET))
          fprintf(write_gdf_fp, "%s %ld\n", local_cm_vector_info[j].mon_breadcrumb, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
        else
          fprintf(write_gdf_fp, "%s %ld\n", local_cm_vector_info[j].vector_name, local_cm_vector_info[j].rtg_index[MAX_METRIC_PRIORITY_LEVELS]);
      }

      group_vector_index++;
    }
  }
}

/*
 - allocate graph data structure for new vectors
 - free array of vector names present in group table
 - create new array of vector names for group table
 - write breadcrumb/vector name in tmp testrun.gdf
   - minus sign for deleted vectors
 - update array of vector names & num vectors in group table
*/
//TM: This function will be called after gdf processing, so gp_info_index will always be set
int write_vectors_with_rtg_index(void *info_ptr, int numGraph, int rpGroupID, char *groupName, char *groupType, 
                                 char *groupHierarchy, int numVector, char *gdf_name, int new_group)
{
  //char **TwoD = NULL;
  int group_vec_idx = 0, graph_vec_idx = 0, graph = 0;
  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;
  CM_info *cm_ptr = (CM_info *)info_ptr;

  if(cm_ptr->gp_info_index < 0)
    return 0;
  
  local_group_data_ptr = group_data_ptr + cm_ptr->gp_info_index;
  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

  NSDL2_GDF(NULL, NULL, "Method called, info_ptr = %p, numGraph = %d, rpGroupID = %d, groupName = %s, numVector = %d, "
                        "gdf_name = %s, new_group = %d", 
                         info_ptr, numGraph, rpGroupID, groupName, numVector, gdf_name, new_group);

  if(!new_group && (numVector >= local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]))
  {
    for(graph = 0; graph < local_group_data_ptr->num_graphs; graph++, local_graph_data_ptr++) 
    {
      MY_REALLOC_AND_MEMSET(local_graph_data_ptr->graph_data, (numVector * sizeof(int **)), 
                              (local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] * sizeof(int **)), 
                              "local_graph_data_ptr->graph_data", -1);
 
      for(group_vec_idx = local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]; group_vec_idx < numVector; group_vec_idx++)  
      {
        MY_MALLOC(local_graph_data_ptr->graph_data[group_vec_idx], local_graph_data_ptr->num_vectors * sizeof(int *), 
                   "local_graph_data_ptr->graph_data[group_vec_idx]", -1);
 
        for(graph_vec_idx = 0; graph_vec_idx < local_graph_data_ptr->num_vectors; graph_vec_idx++) 
        {
          MY_MALLOC(local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx], sizeof(Graph_Data), 
                    "local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx]", -1);
          init_graph_data_node(((Graph_Data *)local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx]));
        } 
      }     
    }

    //free group table vector name list
    if(local_group_data_ptr->vector_names)
      free_vector_names((local_group_data_ptr->vector_names), local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]);

    local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] = numVector;
  }

  local_group_data_ptr->vector_names = printGroupVector(cm_ptr->gp_info_index, numVector, rpGroupID, gdf_name, info_ptr, 0, new_group);

  return 0;
}

/*int get_group_first_rtg_index(int monitor_index)
{
  int i;
  for(i=0;i<monitor_list_ptr[i].no_of_monitors;i++)
    if(monitor_list_ptr[i].cm_info_mon_conn_ptr->total_vectors > 0)
      return (monitor_list_ptr[i].cm_info_mon_conn_ptr)->vector_list[0].rtg_index;
  return 0;
}*/


inline void process_gdf_on_runtime(char *fname, void *info_ptr, int monitor_index)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  int numVector = 1, numGraph = 0, rpGroupID = 0, i = 0;
  char tmpbuff[MAX_LINE_LENGTH];
  char groupName [MAX_LINE_LENGTH];
  char groupType[MAX_LINE_LENGTH];
  char groupHierarchy[MAX_LINE_LENGTH];
  char groupDescription[MAX_LINE_LENGTH];
  char groupMetric[MAX_LINE_LENGTH];
  int cm_num_graphs;
  int save_group_count;
  int cm_num_groups;
  int group_idx;
  int new_group = 0;

  NSDL2_GDF(NULL, NULL, "Method called, processing gdf: %s", fname);

  read_gdf_fp = open_gdf(fname);

  CM_info *cm_info_mon_conn_ptr = (CM_info *)info_ptr;

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                    "Method Called inside process_on_runtime gdf_name %s gp_info_index %d ",cm_info_mon_conn_ptr->gdf_name, cm_info_mon_conn_ptr->gp_info_index);
  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    NSDL3_GDF(NULL, NULL, "line = %s", line);
    if(sscanf(line, "%s", tmpbuff) == -1)
      continue;
    if(line[0] == '#' || line[0] == '\0' || (!(strncasecmp(line, "info|", strlen("info|")))))
      continue;
    else if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      save_group_count = group_count;

      //process Group line
      process_group(read_gdf_fp, line, 0, fname, 0, info_ptr, &numGraph, &rpGroupID, groupName, groupType, groupHierarchy, groupDescription, groupMetric, 0);

      if(cm_info_mon_conn_ptr)
        numVector = cm_info_mon_conn_ptr->group_num_vectors ;
        //numVector = cm_info_mon_conn_ptr->group_num_vectors;

      //for nd backend monitors we need to set the data pointers


      if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
      {
        set_nd_backend_mon_data_ptrs(monitor_index, monitor_list_ptr[monitor_index].no_of_monitors);
      }
      else if((cm_info_mon_conn_ptr->flags & NV_MONITOR) && (cm_info_mon_conn_ptr->flags & DYNAMIC_MONITOR))
      {
        // SET DATA POINTER FOR NV MONITORS
        //TODO MSR Implement loop within the function
        for(i = monitor_index; i < (monitor_index + monitor_list_ptr[monitor_index].no_of_monitors); i++)
          set_nv_mon_data_ptrs(i, monitor_list_ptr[i].cm_info_mon_conn_ptr);
      }
      /*else if ( netcloud )
      {
          TODO
      }*/

      //found new gdf
      if(cm_info_mon_conn_ptr->gp_info_index < 0)
      {
        group_count++;

        group_idx = fill_group_info(fname , groupName, rpGroupID, groupType, numGraph, numVector, NULL, groupHierarchy, groupDescription, groupHierarchy, groupMetric);
        cm_num_groups = (group_count - save_group_count);
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
		                    "Method Called gp_info_index less than 0 gdf_name %s gp_info_index %d ",cm_info_mon_conn_ptr->gdf_name, cm_info_mon_conn_ptr->gp_info_index);
        get_no_of_elements(cm_info_mon_conn_ptr, &cm_num_graphs);

        set_cm_info_values(monitor_index, cm_num_groups, cm_num_graphs, cm_info_mon_conn_ptr->group_num_vectors, save_group_count);
        new_group =1;
      }
      else
      {
        set_cm_info_values(monitor_index, cm_info_mon_conn_ptr->num_group, cm_info_mon_conn_ptr->no_of_element, cm_info_mon_conn_ptr->group_num_vectors, cm_info_mon_conn_ptr->gp_info_index);
      }

      //write breadcrumb/vector in tmp testrun.gdf
      //write_vectors_with_rtg_index(cm_info_mon_conn_ptr, numGraph, rpGroupID, groupName, groupType, groupHierarchy, numVector);
      write_vectors_with_rtg_index(info_ptr, numGraph, rpGroupID, groupName, groupType, groupHierarchy, numVector, fname, new_group);

      //process all the graphs
      if(save_group_count == group_count) //old group
        //process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 1, 0);
        process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 1, 0, ((CM_info *)info_ptr)->gp_info_index, info_ptr, 0);
      else //new group
        //process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 0, get_group_first_rtg_index(monitor_index));
        process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 0, 0, group_idx, info_ptr, 0);

      MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor: %s, Number of total graphs = %d, Number of actual graphs = %d", groupName, group_data_ptr[total_groups_entries - 1].num_graphs, group_data_ptr[total_groups_entries - 1].num_actual_graphs);
 
      Graph_Info *local_ptr;
      local_ptr = graph_data_ptr + group_data_ptr[total_groups_entries - 1].graph_info_index;

      int i;
      for(i = 0; i < group_data_ptr[total_groups_entries - 1].num_graphs; i++ )
      {
        if(local_ptr->graph_state == GRAPH_EXCLUDED)
          group_data_ptr[total_groups_entries - 1].excluded_graph[i] = GRAPH_EXCLUDED;

        local_ptr++;
      }

      break;
    }
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      NSTL1_OUT(NULL, NULL,"Number of graphs are greater than no. of graph mentioned in group line of  %s monitor" ,
                                                                     group_data_ptr[total_groups_entries -1].group_name);
      continue;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", line); 
      NS_EXIT(-1, CAV_ERR_1013024, line);
    }
  }

  NSDL3_GDF(NULL, NULL, "Closing gdf");
  close_gdf(read_gdf_fp);
}


/*int write_rtc_for_ns_health_monitor()
{
  
}*/


/* - At this point cm_info table must be merged and sorted. 
   - Initialize vector group stuff...call  initialize_cm_vector_groups(g_runtime_flag);
   - In loop of total_cm_entries , for each monitor do 
      - parse each gdf except those where gdf_name is 'NA' or whose all vectors are deleted.
      - write Info/Group/Graph from gdf file & 'vector_name rtg_index' from cm_info table
*/
int write_monitor_details()
{
  int idx = 0, j = 0, ret = 0;
  CM_info *cm_info_mon_conn_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method Called  to write in testrun.gdf. total_monitor_list_entries = %d", total_monitor_list_entries);  
  //qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);

  initialize_cm_vector_groups(g_runtime_flag);
  
  //Check and realloc mon_id_map_table 
  if(is_outbound_connection_enabled)
    add_entry_in_mon_id_map_table();

  for (idx = 0; idx < total_monitor_list_entries; idx++)
  {
    cm_info_mon_conn_ptr = monitor_list_ptr[idx].cm_info_mon_conn_ptr;

    if((g_delete_vec_freq_cntr == 0) || !(g_enable_delete_vec_freq))
    {                                                              
      cm_info_mon_conn_ptr->total_deleted_vectors = 0;     //reset 
    }
 
    cm_info_mon_conn_ptr->total_reused_vectors = 0;

    if(cm_info_mon_conn_ptr->cs_port == -1)
      set_netcloud_data_ptrs(idx);

    if(strncmp(monitor_list_ptr[idx].gdf_name, "NA", 2))
    {
      deleted_monitor_count = get_deleted_monitor_count(cm_info_mon_conn_ptr, monitor_list_ptr[idx].no_of_monitors, idx);
      if((deleted_monitor_count != monitor_list_ptr[idx].no_of_monitors) && cm_info_mon_conn_ptr->group_num_vectors > 0)
      {
        process_gdf_on_runtime(monitor_list_ptr[idx].gdf_name, cm_info_mon_conn_ptr, idx);

        idx += monitor_list_ptr[idx].no_of_monitors - 1;

        deleted_monitor_count = 0;
        continue;
      }
      deleted_monitor_count = 0;
    }
    //For Monitors having NA group we need to set cm_index so that after parsing mon_id it can map correclty to parent in cm_info table.
    else if(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED)
      set_index_for_NA_group(cm_info_mon_conn_ptr, cm_info_mon_conn_ptr->group_num_vectors, idx);

    //Assumed that ND or NV monitors will not come with Warning: No vectors. This code is never excuted for ND or NV monitors, its only for future use.

    if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
    {
      set_nd_backend_mon_data_ptrs(idx, cm_info_mon_conn_ptr->group_num_vectors);
    }
    else if((cm_info_mon_conn_ptr->flags & NV_MONITOR) && (monitor_list_ptr[idx].is_dynamic))
    {
      // SET DATA POINTER FOR NV MONITORS
      for(j=idx; j < (idx + monitor_list_ptr[idx].no_of_monitors); j++)
      {
        set_nv_mon_data_ptrs(j, cm_info_mon_conn_ptr);
      }
    }

    //If any new monitor is added then we will increase count of g_monitor_id and simultaneously total_cm_entries will be increased. We are checking here at the last monitor id and if it is NULL then it means new monitor has been added and we need to realloc mon_id_cm_idx_map_table with the updated total_cm_entries count.
  
    ret = set_cm_info_values(idx, cm_info_mon_conn_ptr->num_group, cm_info_mon_conn_ptr->no_of_element, cm_info_mon_conn_ptr->group_num_vectors, cm_info_mon_conn_ptr->gp_info_index);

    if(ret > 0)
    {
      //We will set mon_id_map_table of all the vectors of the monitor.
      idx += ret - 1;
    }
  }

  handle_testrun_gdf_for_HML(monitor_list_ptr, total_monitor_list_entries, 1);
 
   MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method exited");
 
  
   MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "GDF variables after parsing the GDF's : msg_data_size = %ld, total_groups_entries = %d, max_groups_entries = %d, total_graphs_entries = %d, max_graphs_entries = %d, element_count = %d, group_count = %d, nc_msg_data_size = %ld, graph_count_mem = %d", msg_data_size, total_groups_entries, max_groups_entries, total_graphs_entries, max_graphs_entries, element_count, group_count, nc_msg_data_size, graph_count_mem);
  return 0; 
}

void write_rtc_details_for_dyn_objs()
{
 // int i;
  char fname[1024];
 
  NSDL2_GDF(NULL, NULL, "Method called");

  if((dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].startId + dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total) != 0)
  {
    NSDL2_GDF(NULL, NULL, "Processing ns_http_response_codes.gdf, startId = %d, total = %d",
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].startId,
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total);

    sprintf(fname, "%s/etc/ns_http_response_codes.gdf", g_ns_wdir);
    process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_STATUS_CODE);
  }

  if((dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].startId + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total) != 0)
  {
    NSDL2_GDF(NULL, NULL, "Processing ns_trans_stats.gdf, startId = %d, total = %d",
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].startId,
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total);

    sprintf(fname, "%s/etc/ns_trans_stats.gdf", g_ns_wdir);
    process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_TX);
  }

  if(global_settings->g_tx_cumulative_graph)
  {
    if((dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].startId + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].total) != 0)
    {
      sprintf(fname, "%s/etc/ns_trans_cumulative_stats.gdf", g_ns_wdir);
      process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_TX_CUM);
    }
  }

  if(global_settings->rbu_domain_stats_mode)
  {
    if((dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].startId + dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total) != 0)
    {
      NSDL2_GDF(NULL, NULL, "Processing cm_rbu_domain_stat.gdf, startId = %d, total = %d", 
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].startId,
                           dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total);

      sprintf(fname, "%s/sys/cm_rbu_domain_stat.gdf", g_ns_wdir);
      process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_RBU_DOMAIN);
    }
  }

  if(SHOW_SERVER_IP){
    if((dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].startId + dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total) != 0)
    {
      sprintf(fname, "%s/etc/ns_srv_ip_data.gdf", g_ns_wdir);
      process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_SVR_IP);
    }
  }

  if (IS_TCP_CLIENT_API_EXIST){
    if((dynObjForGdf[NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES].startId + 
        dynObjForGdf[NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES].total) != 0)
    {
      sprintf(fname, "%s/etc/ns_socket_tcp_client_failure_stats.gdf", g_ns_wdir);
      process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES);
    }
  }
 
  if (IS_UDP_CLIENT_API_EXIST){
    if((dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].startId + 
        dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].total) != 0)
    {
      sprintf(fname, "%s/etc/ns_socket_udp_client_failure_stats.gdf", g_ns_wdir);
      process_gdf_on_runtime_for_dyn_objs(fname, NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES);
    }
  }

  set_group_idx_for_txn = 0;
}


void process_gdf_at_runtime_for_HM()
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  int numVector = 1, numGraph = 0, rpGroupID = 0; 
  char tmpbuff[MAX_LINE_LENGTH];
  char groupName [MAX_LINE_LENGTH];
  char groupType[MAX_LINE_LENGTH];
  char groupHierarchy[MAX_LINE_LENGTH];
  char groupDescription[MAX_LINE_LENGTH];
  char groupMetric[MAX_LINE_LENGTH];
  char fname[128];

  if(!global_settings->enable_health_monitor)
    return;

  NSDL2_GDF(NULL, NULL, "Method called");

  strcpy(fname, hm_info_ptr->gdf_name);
  read_gdf_fp = open_gdf(fname);

  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) !=NULL)
  {
    line[strlen(line) - 1] = '\0';

    NSDL3_GDF(NULL, NULL, "line = %s", line);

    if(sscanf(line, "%s", tmpbuff) == -1)
      continue;
    if(line[0] == '#' || line[0] == '\0' || (!(strncasecmp(line, "info|", strlen("info|")))))
      continue;
    else if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      //process Group line
      process_group(read_gdf_fp, line, 0, fname, 0, NULL, &numGraph, &rpGroupID, groupName, groupType,
                    groupHierarchy, groupDescription, groupMetric, 0);

      numVector = 1;
      fprintf(write_gdf_fp, "%s %d\n", hm_info_ptr->vector_name, hm_info_ptr->rtg_index);

      //process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 1, 0);
      process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 1, 0, hm_info_ptr->gp_info_idx, NULL, 0);
      fprintf(write_gdf_fp, "\n");

      NSDL3_GDF(NULL, NULL, "numGraph = %d", numGraph);
      break;
    }
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      NSTL1_OUT(NULL, NULL,"Number of graphs are greater than no. of graph mentioned in group line of %s monitor" ,                                                                                                                group_data_ptr[total_groups_entries -1].group_name);
      continue; //should not come here
     }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", line);
 
      NS_EXIT(-1, CAV_ERR_1013024, line);
    }
  }

  NSDL3_GDF(NULL, NULL, "Closing gdf");
  close_gdf(read_gdf_fp);
}

//wrapper of process_all_gdf.
//This is a combine function for both initial gdf processing and runtime gdf processing
void process_gdf_wrapper(int runtime_flag)
{
  g_runtime_flag = runtime_flag;
  
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method called, runtime_flag = %d", runtime_flag);

  if(!runtime_flag) // initial
    process_all_gdf(runtime_flag);
  else // runtime
  {
    /* on every runtime processing of gdf 
        - create temporary testrun.gdf file in tmp
        - dump testrungdf_buffer in tmp file
        - write monitor details in tmp file from CM_info structure
          - parse gdf write Group/Graph information from gdf and vectors with rtg index from cm_info structure
        
     */
    //init_gdf_ptrs_and_variables();

    // create temporary testrun.gdf file in tmp
    create_tmp_gdf();

    //dump testrungdf_buffer in tmp file
    fwrite(testrungdf_buffer, sizeof(char), fsize, write_gdf_fp);  

    fseek(write_gdf_fp, 0, SEEK_END);

    //Write runtime changes applied for netcloud monitors or structures.
    write_rtc_details_for_dyn_objs();
  
    //Write monitor stats in testrun.gdf
    //write_rtc_for_ns_health_monitor();

    //Gdf processing for health monitor when new version of testrun.gdf is created.
    process_gdf_at_runtime_for_HM();

    // write monitor details in tmp file from CM_info structure
    write_monitor_details();

    //close tmp gdf
    close_gdf(write_gdf_fp);

    //realloc and set msg_data_ptr
    allocMsgBuffer();

    //create main testrun.gdf.<count> from /tmp & create rtg & send start msg to gui
   create_testrun_gdf(runtime_flag);
  }
}

/************** gdf indexing functions ******************/


//this function is not static because used in ns_test_gdf.c for test purpose.
void process_all_gdf(int runtime_flag)
{
  char fname[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called. runtime_flag = [%d], g_runtime_flag = [%d], total_pdf_data_size = [%d]", 
                         runtime_flag, g_runtime_flag, total_pdf_data_size);

  //initialize
  init_gdf_ptrs_and_variables();

  g_runtime_flag = runtime_flag;
  
  create_tmp_gdf();
  if(g_dont_skip_test_metrics == 1)
  {
    sprintf(fname, "%s/etc/netstorm.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* Process HTTP Caching Gdf AN-CACHEGDF-TODO*/
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {
    sprintf(fname, "%s/etc/http_cache.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* Process Netstorm Diagnosis gdf*/
  if(global_settings->g_enable_ns_diag) {
    sprintf(fname, "%s/etc/ns_diag.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

   /* Process DOS ATTACK gdf */
  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {
    sprintf(fname, "%s/etc/dos_attack.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

   /* Process Proxy Gdf */
  //Bug 33244 - Not getting the transaction details in progress report when firing test from back-end
  //if(is_proxy_enabled()) 
  if(global_settings->proxy_flag == 1) {
    sprintf(fname, "%s/etc/http_proxy.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

   /* Process Network Cache Gdf */
  if(global_settings->protocol_enabled & NETWORK_CACHE_STATS_ENABLED) {
    sprintf(fname, "%s/etc/http_network_cache_stats.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

   /* Process DNS Lookup Gdf */
  if(IS_DNS_LOOKUP_STATS_ENABLED) {
    sprintf(fname, "%s/etc/dns_lookup_stats.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  // We have to show smtp graphs if at at least one smtp request is exists
  if(total_smtp_request_entries) {
    sprintf(fname, "%s/etc/smtp.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* For POP3 */
  if(total_pop3_request_entries) {
    sprintf(fname, "%s/etc/pop3.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* For ftp */
  if(total_ftp_request_entries) {
    sprintf(fname, "%s/etc/ftp.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* For ldap */
  if(total_ldap_request_entries) {
    sprintf(fname, "%s/etc/ldap.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }
  /* For imap */
  if(total_imap_request_entries) {
    sprintf(fname, "%s/etc/imap.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }
  /* For JRMI */
  if(total_jrmi_request_entries) {
    sprintf(fname, "%s/etc/jrmi.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }
  /* For DNS */
  if(total_dns_request_entries) {
    sprintf(fname, "%s/etc/dns.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /* For WS */
  if(total_ws_request_entries) {
    sprintf(fname, "%s/etc/websocket.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  // [HINT: TCPClient] Parsing GDF of TCP Client and update msg_data_size
  if(IS_TCP_CLIENT_API_EXIST) {
    sprintf(fname, "%s/etc/ns_socket_tcp_client.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  // [HINT: UDPClient] Parsing GDF of UDP Client and update msg_data_size
  if (IS_UDP_CLIENT_API_EXIST) {
    sprintf(fname, "%s/etc/ns_socket_udp_client.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  // Process the ns_grp_data gdf
  if(SHOW_GRP_DATA) {
    sprintf(fname, "%s/etc/ns_grp_data.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }
  
  // Process the ns_rbu_page_stat_data gdf
  if((global_settings->browser_used != -1) && g_rbu_num_pages) {
    sprintf(fname, "%s/sys/cm_ns_rbu_page_stat.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  //Process The ns_page_based_stats.gdf
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) {
    sprintf(fname, "%s/sys/cm_page_based_stats.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }
  
  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS) {
    sprintf(fname, "%s/etc/ns_runtime_runlogic_progress.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  }

  /*if(SHOW_SERVER_IP) {
    sprintf(fname, "%s/etc/ns_srv_ip_data.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL);
  }*/

  //Process The ns_ip_data.gdf and it should always be process in the last. 
  if((loader_opcode != MASTER_LOADER) && (global_settings->show_ip_data == IP_BASED_DATA_ENABLED)) {
    sprintf(fname, "%s/etc/ns_ip_data.gdf", g_ns_wdir);
    process_gdf(fname, 0, NULL, 0);
  } 

  //check if XMPP
  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
      sprintf(fname, "%s/etc/ns_xmpp.gdf", g_ns_wdir);
      process_gdf(fname, 0, NULL, 0);
   }

  //check if FC2
  if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED) {
      sprintf(fname, "%s/etc/ns_fc2.gdf", g_ns_wdir);
      process_gdf(fname, 0, NULL, 0);
   }
  
  //Save size of rtgMessage.dat in nc_msg_data_size, without adding monitor graphs 
  NSDL2_GDF(NULL, NULL, "Before adding monitor graphs, msg_data_size = %ld\n", msg_data_size);
  nc_msg_data_size = msg_data_size;
  ns_gp_msg_data_size = msg_data_size;
  ns_gp_end_idx = total_groups_entries;
  ns_gp_count = group_count;
  ns_graph_end_idx = total_graphs_entries;
  ns_graph_count = graph_count_mem;

  process_user_monitor_gdf();
 
  fflush(write_gdf_fp);

  // save testrun.gdf data except monitor data in a global buffer 
  // TODO: VERIFY IF ANYTHING ABOVE THIS IS RUNTIME CHANGEABLE ??? MUST VERIFY 
  if(testrungdf_buffer == NULL)
    save_testrungdf_in_buffer();

  //We need to set num_element for these graphs
  //Due to percentile issue we need to process gdf for transaction at last.
  if(g_dont_skip_test_metrics == 1)
    init_dyn_objs_and_gdf_processing();
 
  process_hm_gdf();
  process_custom_gdf(runtime_flag);  //Atul - we have to complete this also

  close_gdf(write_gdf_fp);
  allocMsgBuffer();

  create_testrun_gdf(runtime_flag);

  //dump_group_and_graph_info();
}

// filling data in msg_data_ptr which is block_ptr in this function
char* fill_data_by_graph_type(int gp_info_index, int c_graph, int c_group_numvec, int c_graph_numvec, 
				     char type, char *block_ptr, Long_long_data *data, int *indx, char *doverflow)
{
  int i;
  double value = 0;
  double tmp_value = 0;
  int multiplier = 1;
  Times_data tmp_times_data;
  Times_std_data tmp_times_std_data;

  Long_data *ptr;
  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;

  local_group_data_ptr = group_data_ptr + gp_info_index;
  local_graph_data_ptr = graph_data_ptr + (local_group_data_ptr->graph_info_index + c_graph);


  NSDL2_GDF(NULL, NULL, "Method called, gp_info_index = %d, c_graph, c_group_numvec = %d, c_graph_numvec = %d, type = %d, indx = %d", 
                         gp_info_index, c_graph, c_group_numvec, c_graph_numvec, type, *indx);

  switch(type)
  {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_RATE:
    case DATA_TYPE_SUM:
      value =  data[(*indx)]; 
      *((Long_data *)(block_ptr)) = value;
      update_gdf_data(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, value);
      (*indx)++;
      block_ptr += sizeof(Long_data);
      break;
    case DATA_TYPE_CUMULATIVE:
      value =  data[(*indx)]; 
      *(Long_data *)(block_ptr) = value;

      update_gdf_data(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, value);

      (*indx)++;
      block_ptr += sizeof(Long_long_data);
      break;
    case DATA_TYPE_TIMES:
      ptr = (Long_data *)&tmp_times_data;
      for(i = 0; i < TIMES_MAX_NUM_ELEMENTS; i++)
      {
        value =  data[(*indx)];
        *((Long_data *)(block_ptr)) = value;

	*ptr = (Long_data)data[(*indx)];
	ptr++;

        (*indx)++;
        block_ptr += sizeof(Long_data);
      }
      
      update_gdf_data_times(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, 
			    (Long_long_data)tmp_times_data.avg_time, (Long_long_data)tmp_times_data.min_time, 
			    (Long_long_data)tmp_times_data.max_time, (Long_long_data)tmp_times_data.succ);

      break;
    case DATA_TYPE_TIMES_STD:
      ptr = (Long_data *)&tmp_times_std_data;
      for(i = 0; i < TIMES_STD_MAX_NUM_ELEMENTS; i++)
      {
        value =  data[(*indx)];
        *((Long_data *)(block_ptr)) = value;

	*ptr = (Long_data)data[(*indx)];
	ptr++;

        (*indx)++;
        block_ptr += sizeof(Long_data);
      }

      update_gdf_data_times_std(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, 
				(Long_long_data)tmp_times_std_data.avg_time, (Long_long_data)tmp_times_std_data.min_time, 
				(Long_long_data)tmp_times_std_data.max_time, (Long_long_data)tmp_times_std_data.succ, 
				tmp_times_std_data.sum_of_sqr);
      break;
   case DATA_TYPE_SAMPLE_1B:
      if(value != value) //Data is NaN
        *((Char_data *)(block_ptr)) = CHAR_NaN;
      else
      {
        if(value > CHAR_MAX)
        {
          if(!(*doverflow))
          {
             MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                  "DataOverFlow (DATA_TYPE_SAMPLE_1B): occured for Group '%s' and Graph '%s'. "
                              "Data '%d' cann't be fit into 1B (since CHAR_MAX = %d), hence ignoring the data.",
                               local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, value, CHAR_MAX);   
            *doverflow = 1;
          }
          
          hm_data->num_DataOverFlow++;
          *((Char_data *)(block_ptr)) = CHAR_NaN;
        }
        else
          *((Char_data *)(block_ptr)) = ((Char_data)value);
      }

      update_gdf_data(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, value);

      (*indx)++;
      block_ptr += sizeof(Char_data);
      break;
    case DATA_TYPE_SAMPLE_2B_100:
      value = data[(*indx)];
      if(value != value) //Data is NaN
        *((Short_data *)(block_ptr)) = SHORT_NaN;
      else
      {
        tmp_value = value * 100;
        if(tmp_value > SHRT_MAX)
        {
          if(!(*doverflow))
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                  "DataOverFlow (DATA_TYPE_SAMPLE_2B_100): occured for Group '%s' and Graph '%s'. "
                              "Data '%f' cann't be fit into 2B (since SHRT_MAX = %hd), hence ignoring the data.",
                               local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, data[(*indx)], SHRT_MAX);
            *doverflow = 1;
          }
          hm_data->num_DataOverFlow++;                                                        
          *((Short_data *)(block_ptr)) = SHORT_NaN;
        }
        else
          *((Short_data *)(block_ptr)) = (Short_data)tmp_value;
      }

      update_gdf_data(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, value);

      (*indx)++;
      block_ptr += sizeof(Short_data);
      break;
    case DATA_TYPE_SAMPLE_4B:
    case DATA_TYPE_SUM_4B:
    case DATA_TYPE_RATE_4B_1000:
    case DATA_TYPE_SAMPLE_4B_1000:
    case DATA_TYPE_T_DIGEST:
      value = data[(*indx)];
      if(value != value) //Data is NaN
        *((Int_data *)(block_ptr)) = INT_NaN;
      else
      {
        if((type == DATA_TYPE_SUM_4B) || (type == DATA_TYPE_SAMPLE_4B) || (type == DATA_TYPE_T_DIGEST))
        {
          if(value > INT_MAX)
          {
            if(!(*doverflow))
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                      "DataOverFlow (DATA_TYPE_SAMPLE_4B): occured for Group '%s' and Graph '%s'. "
                                "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                 local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, value, INT_MAX);          
              *doverflow = 1;
            }
            
            hm_data->num_DataOverFlow++;
            *((Int_data *)(block_ptr)) = INT_NaN;
          }
          else
            *((Int_data *)(block_ptr)) = ((Int_data)value);
        }
        else
        {
          tmp_value = value * 1000;
          if(tmp_value > INT_MAX)
          {
            if(!(*doverflow))
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                      "DataOverFlow (DATA_TYPE_SAMPLE_4B_1000): occured for Group '%s' and Graph '%s'. "
                                "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                 local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, data[(*indx)], INT_MAX);
              *doverflow = 1;
            }
            
            hm_data->num_DataOverFlow++;
            *((Int_data *)(block_ptr)) = INT_NaN;
          }
          else
            *((Int_data *)(block_ptr)) = ((Int_data)tmp_value);
        }
      }

      update_gdf_data(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec, value);
      (*indx)++;
      block_ptr += sizeof(Int_data);
      break;
   case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      /***************************************************************
       * DATA_TYPE_SAMPLE_2B_100_COUNT_4B has 2 elements avg and count 
         form Times graph (avg min max count) i.e min and max will 
         not be consider here

       * Sample data will be multiplied by 100 and then store into RTG
         on the other hand count data will just put into RTG
 
         Note: 
           * it is assumed CMON will send times type data i.e. 4 
             elements, but min and max is useless.  
           * In future either we have to discard this type or add new
             type so that CMON get flexibility to send graph data having 
             2 elements 
       ***************************************************************/
      ptr = (Long_data *)&tmp_times_data;
      for(i = 0; i < TIMES_MAX_NUM_ELEMENTS; i++) //Number of element is 4
      {
        value = data[(*indx)];
        if(i == TIMES_AVG_DATA)  //0 => sample
        {
          if(value != value) //Data is NaN
            *((Short_data *)(block_ptr)) = SHORT_NaN; 
          else
          {
            tmp_value = value * 100;
            if(tmp_value > SHRT_MAX)
            {
              if(!(*doverflow))
              {
                MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                      "DataOverFlow (DATA_TYPE_SAMPLE_2B_100_COUNT_4B, sample): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since SHRT_MAX = %hd), hence ignoring the data.",
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, data[(*indx)], SHRT_MAX);
                *doverflow = 1;
              }
 
               hm_data->num_DataOverFlow++;
              *((Int_data *)(block_ptr)) = SHORT_NaN;
            }
            else
              *((Short_data *)(block_ptr)) = (Short_data)tmp_value;
          }

          *ptr = (Long_data)data[(*indx)];
          block_ptr += sizeof(Short_data);
        }
        else if(i == TIMES_COUNT_DATA) //3 => count
        {
          if(value != value) //Data is NaN
            *((Int_data *)(block_ptr)) = INT_NaN; 
          else
          {
            if(value > INT_MAX)
            {
              if(!(*doverflow))
              {
               MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "DataOverFlow (DATA_TYPE_SAMPLE_2B_100_COUNT_4B, count): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, value, INT_MAX); 
               *doverflow = 1;
              }
              
              hm_data->num_DataOverFlow++;
              *((Int_data *)(block_ptr)) = INT_NaN;
            }
            else
              *((Int_data *)(block_ptr)) = (Int_data)value;
          }

          *ptr = (Long_data)data[(*indx)];
          block_ptr += sizeof(Int_data);
        }
        else
        {
          *ptr = 0;   //min and max will not be consider in this case
          *((Int_data *)(block_ptr)) = 0;
        }
        ptr++;
        (*indx)++;
      }
      update_gdf_data_times(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec,
                            (Long_long_data)tmp_times_data.avg_time,
                            (Long_long_data)tmp_times_data.min_time,
                            (Long_long_data)tmp_times_data.max_time,
                            (Long_long_data)tmp_times_data.succ);
    break;
   case DATA_TYPE_TIMES_4B_10:
        multiplier = 10;        //Don't add break condtion
   case DATA_TYPE_TIMES_4B_1000:
         if(multiplier == 1)
          multiplier = 1000;
      /***************************************************************
       * DATA_TYPE_TIMES_4B_1000 has 4 elements avg, min, max and count 

       * avg, min and max data will be multiplied by 1000 to convert 
         double (with 3 decimal) into integer  
 
       * count data will not be multiplied by 1000 as it has already 
         too large value
       ***************************************************************/
      ptr = (Long_data *)&tmp_times_data;
      for(i = 0; i < TIMES_MAX_NUM_ELEMENTS; i++) //Number of element in DATA_TYPE_TIMES_4B_1000 = 4
      {
        value = data[(*indx)];
        if(value != value) //Data is NaN
          *((Int_data *)(block_ptr)) = INT_NaN; 
        else
        {
          if(i == TIMES_COUNT_DATA) //count data 
          {
            if(value > INT_MAX)
            {
              if(!(*doverflow))
              {
                MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "DataOverFlow (DATA_TYPE_TIMES_4B_1000, count): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, value, INT_MAX);
                *doverflow = 1;
              }
              
               hm_data->num_DataOverFlow++;
              *((Int_data *)(block_ptr)) = INT_NaN;
            }
            else
              *((Int_data *)(block_ptr)) = (Int_data)value;
          }
          else //avg, min and max data
          {
            tmp_value = value * multiplier;
            if(tmp_value > INT_MAX)
            {
              if(!(*doverflow))
              {
                MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "DataOverFlow (DATA_TYPE_TIMES_4B_1000): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, data[(*indx)], INT_MAX);   
                *doverflow = 1;
                hm_data->num_DataOverFlow++;
              }

              *((Int_data *)(block_ptr)) = INT_NaN;
            }
            else
              *((Int_data *)(block_ptr)) = (Int_data)tmp_value;
          }
        }
            
        *ptr = (Long_data)data[(*indx)];
        ptr++;
        (*indx)++;
        block_ptr += sizeof(Int_data);
      }

      update_gdf_data_times(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec,
                            (Long_long_data)tmp_times_data.avg_time,
                            (Long_long_data)tmp_times_data.min_time,
                            (Long_long_data)tmp_times_data.max_time,
                            (Long_long_data)tmp_times_data.succ);
      break;
   case DATA_TYPE_TIMES_STD_4B_1000:
      /***************************************************************
       * DATA_TYPE_TIMES_STD_4B_1000 has 5 elements avg, min, max, count and  std

       * avg, min, max and std data will be multiplied by 1000 to convert 
         double (with 3 decimal) into integer  
 
       * count data will not be multiplied by 1000 as it has already 
         too large value
       ***************************************************************/
      ptr = (Long_data *)&tmp_times_std_data;
      for(i = 0; i < TIMES_STD_MAX_NUM_ELEMENTS; i++) //Number of elements in DATA_TYPE_TIMES_STD_4B_1000 = 5
      {
        value = data[(*indx)];
        if(value != value) //Data is NaN
          *((Int_data *)(block_ptr)) = INT_NaN; 
        else
        {
          if(i == TIMES_COUNT_DATA) //count data 
          {
            if(value > INT_MAX)
            {
              if(!(*doverflow))
              {
                MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "DataOverFlow (DATA_TYPE_TIMES_STD_4B_1000, count): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.",
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, value, INT_MAX);
                *doverflow = 1;
                hm_data->num_DataOverFlow++;
              }
              
              *((Int_data *)(block_ptr)) = INT_NaN;
            }
            else
              *((Int_data *)(block_ptr)) = (Int_data)value;
          }
          else       
          {
            tmp_value = value * 1000;
            if(tmp_value > INT_MAX)
            {
              if(!(*doverflow))
              {
                MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "DataOverFlow (DATA_TYPE_TIMES_STD_4B_1000): occured for Group '%s' and Graph '%s'. "
                                  "Data '%d' cann't be fit into 4B (since INT_MAX = %d), hence ignoring the data.", 
                                   local_group_data_ptr->group_name, local_graph_data_ptr->graph_name, data[(*indx)], INT_MAX);
                *doverflow = 1;
                hm_data->num_DataOverFlow++;
              }
                   
              *((Int_data *)(block_ptr)) = INT_NaN;
            }
            else
              *((Int_data *)(block_ptr)) = (Int_data)tmp_value;
          }
        }

        *ptr = (Long_data)data[(*indx)];
        ptr++;
        (*indx)++;
        block_ptr += sizeof(Int_data);
      }

      update_gdf_data_times_std(gp_info_index, c_graph, c_group_numvec,  c_graph_numvec,
                                (Long_long_data)tmp_times_std_data.avg_time,
                                (Long_long_data)tmp_times_std_data.min_time,
                                (Long_long_data)tmp_times_std_data.max_time,
                                (Long_long_data)tmp_times_std_data.succ,
                                tmp_times_std_data.sum_of_sqr);
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType (%c)in GDF is not correct\n", type);
      NS_EXIT(-1, CAV_ERR_1013001, type);
  }
  return block_ptr;
}

void check_dup_grp_id_in_CM_and_DVM()
{
  int custom_mon_id = 0;
  Group_Info *cm_group_data_ptr = NULL;

  int dyn_mon_id = 0;
  Group_Info *dyn_group_data_ptr = NULL;

  for (custom_mon_id = 0; custom_mon_id < g_dyn_cm_start_idx;)
  {
    if(!strncmp(monitor_list_ptr[custom_mon_id].gdf_name, "NA", 2))
    {
      custom_mon_id++;
      continue;
    }

    for (dyn_mon_id = g_dyn_cm_start_idx; dyn_mon_id < total_monitor_list_entries;)
    {   
       if (monitor_list_ptr[dyn_mon_id].cm_info_mon_conn_ptr->gp_info_index < 0)
       {
         dyn_mon_id++;
         continue;
       }

      cm_group_data_ptr = group_data_ptr + monitor_list_ptr[custom_mon_id].cm_info_mon_conn_ptr->gp_info_index; //CM group info ptr
      dyn_group_data_ptr = group_data_ptr + monitor_list_ptr[dyn_mon_id].cm_info_mon_conn_ptr->gp_info_index; //DVM group info ptr

      //check if any group used as both custom monitor and dynamic vector monitor
      if(cm_group_data_ptr->rpt_grp_id == dyn_group_data_ptr->rpt_grp_id)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Group ID %u is present in both Custom Monitor (%s) and Dynamic Vector Monitor (%s).\n", cm_group_data_ptr->rpt_grp_id, monitor_list_ptr[custom_mon_id].cm_info_mon_conn_ptr->monitor_name, monitor_list_ptr[dyn_mon_id].cm_info_mon_conn_ptr->monitor_name);
        NS_EXIT(-1, CAV_ERR_1013027, cm_group_data_ptr->rpt_grp_id, monitor_list_ptr[custom_mon_id].cm_info_mon_conn_ptr->monitor_name, monitor_list_ptr[dyn_mon_id].cm_info_mon_conn_ptr->monitor_name);
      }
      dyn_mon_id ++;
    }
    custom_mon_id ++;
  }
}


void update_index_for_filling_data(int *indx, char type)
{

  NSDL2_GDF(NULL, NULL, "Method called, type = %d", type);
  switch(type)
  {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_SAMPLE_2B_100:
    case DATA_TYPE_SAMPLE_4B_1000:
    case DATA_TYPE_RATE:
    case DATA_TYPE_RATE_4B_1000:
    case DATA_TYPE_SUM:
    case DATA_TYPE_SAMPLE_4B:
    case DATA_TYPE_SAMPLE_1B:
    case DATA_TYPE_SUM_4B:
    case DATA_TYPE_T_DIGEST:
      (*indx)++;
      break;
    case DATA_TYPE_CUMULATIVE:
      (*indx)++;
      break;
    case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      *indx += 4; 
      break;
    case DATA_TYPE_TIMES_4B_10:
    case DATA_TYPE_TIMES:
    case DATA_TYPE_TIMES_4B_1000:
      //for(i = 0; i < (sizeof(Times_data)/sizeof(Long_data)); i++)
        *indx += 4;
      break;
    case DATA_TYPE_TIMES_STD:
    case DATA_TYPE_TIMES_STD_4B_1000:
      //for(i = 0; i < (sizeof(Times_std_data)/sizeof(Long_data)); i++)
        *indx += 5;
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType (%c)in GDF is not correct\n", type);
      NS_EXIT(-1, CAV_ERR_1013001, type);
  }
}


void fill_cm_data_ptr(int mon_id)
{
  int c_graph;
  int c_group_numvec = 0;
  int c_graph_numvec = 0;
  int index = 0;
  int i = 0, mp = 0;
  int mon_hml_priority;
  int graph_hml_priority;
  int mon_index, vector_index;
  Graph_Info *local_graph_data_ptr = NULL;
  char *custom_block_local_ptr[MAX_METRIC_PRIORITY_LEVELS + 1];
  int num_times_data = 0;
  int gp_info_index;
  Long_long_data *data;
  CM_info *cm_info_mon_conn_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
  CM_vector_info *vector_list_ptr;
  
  gp_info_index = cm_info_mon_conn_ptr->gp_info_index;
  if (gp_info_index < 0)
  return ;

  NSDL2_GDF(NULL, NULL, "Method called, gp_info_index = %d, num_group = %d", gp_info_index, cm_info_mon_conn_ptr->num_group);

  Group_Info *local_group_data_ptr = group_data_ptr + gp_info_index;

  /* local_graph_data_ptr initialized here to fill custom_block_local_ptr */
  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

//group type should not be scalar.
  if(local_group_data_ptr->group_type == GROUP_GRAPH_TYPE_SCALAR)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: groupType is not scalar for rpGroupID = %d\n", local_group_data_ptr->rpt_grp_id);
    return;
  }

   /* Loop for each group vector */
  for(mon_index = mon_id; mon_index < mon_id + monitor_list_ptr[mon_id].no_of_monitors; mon_index++)
  {
    cm_info_mon_conn_ptr = monitor_list_ptr[mon_index].cm_info_mon_conn_ptr;
    vector_list_ptr = cm_info_mon_conn_ptr->vector_list;
    if(cm_info_mon_conn_ptr->flags & ALL_VECTOR_DELETED)
    {
      continue;
    }

    mon_hml_priority = cm_info_mon_conn_ptr->metric_priority;

    if(mon_hml_priority == MAX_METRIC_PRIORITY_LEVELS)
      mp = MAX_METRIC_PRIORITY_LEVELS;
    else
      mp = 0;

    for(vector_index = 0; vector_index < cm_info_mon_conn_ptr->total_vectors; c_group_numvec++, vector_index++)
    {
      if(vector_list_ptr[vector_index].flags & OVERALL_VECTOR)
      {
	if((cm_info_mon_conn_ptr->flags & ND_MONITOR) && (vector_list_ptr[vector_index].vectorIdx >= 0) && (vector_list_ptr[vector_index].tierIdx >= 0) && (cm_info_mon_conn_ptr->tierNormVectorIdxTable))
        {
          if(((cm_info_mon_conn_ptr->tierNormVectorIdxTable)[vector_list_ptr[vector_index].vectorIdx][vector_list_ptr[vector_index].tierIdx]->got_data_and_delete_flag) & GOT_DATA)
          {
            (cm_info_mon_conn_ptr->tierNormVectorIdxTable)[vector_list_ptr[vector_index].vectorIdx][vector_list_ptr[vector_index].tierIdx]->got_data_and_delete_flag &= ~GOT_DATA;
          }

          else if (!(vector_list_ptr[vector_index].flags & RUNTIME_DELETED_VECTOR ))
          {
            (cm_info_mon_conn_ptr->tierNormVectorIdxTable)[vector_list_ptr[vector_index].vectorIdx][vector_list_ptr[vector_index].tierIdx]->got_data_and_delete_flag |= OVERALL_DELETE;
            delete_entry_for_nd_monitors(cm_info_mon_conn_ptr, vector_index, 0);   //sending 0 to set flag in case of overall
            continue;
          }
        }
      }
    
      gp_info_index = cm_info_mon_conn_ptr->gp_info_index;
      data = vector_list_ptr[vector_index].data;
      local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

      //setting custom_block_ptr index to 
      // 0 for high
      // 1 for medium
      // 2 for low
      for(i = mp; i <= mon_hml_priority; i++)
        custom_block_local_ptr[i] = (char *)(msg_data_ptr + vector_list_ptr[vector_index].rtg_index[i]);

      NSDL3_GDF(NULL, NULL, "num_graphs = %d, num_vectors = %d, graph_info_index = %d",local_group_data_ptr->num_graphs, local_graph_data_ptr->num_vectors, local_group_data_ptr->graph_info_index);

      if(strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_bt.gdf") != NULL)
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "vector name [ %s] , %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                        "%ld %ld %ld %ld %ld %ld %ld %ld ",
                         vector_list_ptr[vector_index].vector_name , vector_list_ptr[vector_index].data[0],
                         vector_list_ptr[vector_index].data[1], vector_list_ptr[vector_index].data[2],
                         vector_list_ptr[vector_index].data[3],vector_list_ptr[vector_index].data[4],vector_list_ptr[vector_index].data[5],
                         vector_list_ptr[vector_index].data[6],vector_list_ptr[vector_index].data[7],vector_list_ptr[vector_index].data[8],
                         vector_list_ptr[vector_index].data[9],vector_list_ptr[vector_index].data[10],vector_list_ptr[vector_index].data[11],
                         vector_list_ptr[vector_index].data[12],vector_list_ptr[vector_index].data[13],vector_list_ptr[vector_index].data[14],
                         vector_list_ptr[vector_index].data[15],vector_list_ptr[vector_index].data[16],vector_list_ptr[vector_index].data[17],
                         vector_list_ptr[vector_index].data[18],vector_list_ptr[vector_index].data[19],vector_list_ptr[vector_index].data[20],
                         vector_list_ptr[vector_index].data[21],vector_list_ptr[vector_index].data[22],vector_list_ptr[vector_index].data[23]);  
      }
      for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
      {
  
        graph_hml_priority = local_graph_data_ptr->metric_priority;

        //Handling graph exlude case. We need to update index accordingly
        if(local_group_data_ptr->excluded_graph[c_graph] == GRAPH_EXCLUDED || (graph_hml_priority > mon_hml_priority))
        { 
          update_index_for_filling_data(&index, local_graph_data_ptr->data_type);
          NSDL2_MON(NULL, NULL, "Skipping data element and proceeding to next. GraphId = %d, GroupId = %d", 
                                 local_graph_data_ptr->rpt_id, local_group_data_ptr->rpt_grp_id);
          local_graph_data_ptr++;
          continue;
        }
    
        for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
        {
          NSDL3_GDF(NULL, NULL, "local_graph_data_ptr = %p, c_group_numvec = %d, c_graph_numvec = %d, num_vectors = %d", 
                        local_graph_data_ptr, c_group_numvec, c_graph_numvec, local_graph_data_ptr->num_vectors);
    
          //for nd backend monitor vector 'overall' we need to calculate avg graph value
          if(vector_list_ptr[vector_index].flags & OVERALL_VECTOR)
          {
            if(IS_TIMES_GRAPH(local_graph_data_ptr->data_type)) //for times type graph  'avg min max count'
            {
              num_times_data = index;
              // gdf graph types are: 'cumulative rate times rate' 0 1 2 3 4 5 6 7 
              // doing avg graph data = avg graph data / count
              if(vector_list_ptr[vector_index].data[num_times_data + 3] > 0)
              {
                vector_list_ptr[vector_index].data[num_times_data] = 
                      vector_list_ptr[vector_index].data[num_times_data] / vector_list_ptr[vector_index].data[num_times_data + 3];
              }
              else
              {
                NSDL3_GDF(NULL, NULL, "Not doing any calculation because received 0 in count.");
              }
            }

            if(strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_bt.gdf") != NULL)
            {
              /*-----------------------------------------------------------------------------
                 Calculate % of slow respose:
                     % slow graph = ((cout_slow_resp + count_very_slow_resp)/count_tot_resp) * 100
                 
                 =>  Graph Indexing:
                     
                        +-+----+-+-+----+----+----+----+-+
                        |s|amMc|s|r|amMc|amMc|amMc|amMc|p|
                        +-+----+-+-+----+----+----+----+-+
                index    0 1    5 6 7    11   15   19   24
                
                here: 
                   *    amMc = avg min max count (i.e Times type graph)
                   *    At index 1, data of graph "Total Respose"
                   *    At index 11, data of graph "Slow Response"
                   *    At index 15, data of graph "Very Slow Response"
                   *    At index 24, data of graph "Percentage of Slow Response"
               ---------------------------------------------------------------------------*/
              // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
              // avg min max count
              // 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
              // r t       s r t         t            t          t           pct
              if(index == 1)
              {
                num_times_data = index;
                if(vector_list_ptr[vector_index].data[num_times_data + 3] > 0)
                //if(cm_info_ptr[custom_mon_id].data[num_times_data + 3] > 0)
                {
                  vector_list_ptr[vector_index].data[num_times_data + 22]   =  ((vector_list_ptr[vector_index].data[num_times_data + 13]  + vector_list_ptr[vector_index].data[num_times_data + 17] ) / (vector_list_ptr[vector_index].data[num_times_data + 3]))  * 100 ;
                }
                else
                {
                  NSDL3_GDF(NULL, NULL, "Not doing any calculation because received 0 in Average Response Time graph's count.");
                }
              } 
            }
          }
   
          /* function fill_data_by_graph_type updates value of custom_block_local_ptr
          * We initialize custom_block_local_ptr once because it is contiguous. */ 
          //In case of hml graph_hml_priority is set based on the prioirity of the graph
          custom_block_local_ptr[graph_hml_priority] = fill_data_by_graph_type(gp_info_index, c_graph, c_group_numvec, c_graph_numvec, local_graph_data_ptr->data_type, custom_block_local_ptr[graph_hml_priority], data, &index, &(cm_info_mon_conn_ptr->is_data_overflowed));
        }
        local_graph_data_ptr++;
      }
    
      //for nd backend monitor vector 'overall' we need to memset the data
      if(vector_list_ptr[vector_index].flags & OVERALL_VECTOR)     
      {
        if(!((vector_list_ptr[vector_index].flags & RUNTIME_DELETED_VECTOR) || (vector_list_ptr[vector_index].flags & WAITING_DELETED_VECTOR)))
        {
          NSDL3_GDF(NULL, NULL, "Memset backend monitor overall data");
          memset(vector_list_ptr[vector_index].data, 0, (cm_info_mon_conn_ptr->no_of_element * sizeof(double)));    
    
          for(i=0; i<128; i++)
          {
            if(cm_info_mon_conn_ptr->save_times_graph_idx[i] == -1)
              break;
            else
              vector_list_ptr[vector_index].data[cm_info_mon_conn_ptr->save_times_graph_idx[i]] = MAX_LONG_LONG_VALUE;
          }
          /*if((cm_info_mon_conn_ptr->flags & ND_MONITOR) && (vector_list_ptr[vector_index].vectorIdx >= 0) && (vector_list_ptr[vector_index].tierIdx >= 0))
          {
            if(cm_info_mon_conn_ptr->tierNormVectorIdxTable)
              (cm_info_mon_conn_ptr->tierNormVectorIdxTable)[vector_list_ptr[vector_index].vectorIdx][vector_list_ptr[vector_index].tierIdx]->got_data_and_delete_flag &= ~GOT_DATA;
          }*/
        }
      }
      index = 0;
      if((cm_info_mon_conn_ptr->flags & ND_MONITOR) && (!(vector_list_ptr[vector_index].flags & OVERALL_VECTOR)) && (vector_list_ptr[vector_index].vectorIdx >= 0 ) && (vector_list_ptr[vector_index].instanceIdx >= 0 ) && ((cm_info_mon_conn_ptr->instanceVectorIdxTable)[vector_list_ptr[vector_index].mappedVectorIdx][vector_list_ptr[vector_index].instanceIdx]))
      {
        (cm_info_mon_conn_ptr->instanceVectorIdxTable)[vector_list_ptr[vector_index].mappedVectorIdx][vector_list_ptr[vector_index].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA ;
      }
      vector_list_ptr[vector_index].flags &= ~RUNTIME_RECENT_DELETED_VECTOR;
    }
  }
}

// printing the custom monitor data into progress and summary report.
// This function will match the data type and keep track the block_ptr (msg_data_ptr) according to data type.

char* print_data_by_graph_type(FILE *fp1, FILE *fp2, Graph_Info *grp_data_ptr, char *block_ptr, int print_mode, Graph_Data *gdata, char *vector_name)
{
    double value = 0;
    double DOUBLE_NaN = 0.0/0.0;
    int dividend = 1;   

  NSDL2_GDF(NULL, NULL, "Method called, print_mode = %d, data_type = %d", print_mode, grp_data_ptr->data_type);

  switch(grp_data_ptr->data_type)
  {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_RATE:
    case DATA_TYPE_SUM:
      if(print_mode == 0)
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\n",grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      else
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\t min=%'6.3f\t max=%'6.3f\t avg=%'6.3f\n",
                 grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))),convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), calc_gdf_avg_of_c_avg(grp_data_ptr, gdata)); 

      block_ptr = (block_ptr + sizeof(Long_data)); 
      break;
    case DATA_TYPE_CUMULATIVE:
      // if(print_mode == 0) // For cumulative, there is no min/max/avg. So both print mode are same
      fprint2f(fp1, fp2,"\t%s (%s): %'.3f\n",grp_data_ptr->graph_name, vector_name,
               convert_data_by_formula_long_long(grp_data_ptr->formula, block_ptr));
      block_ptr = (block_ptr + sizeof(Long_long_data));
      break;
    case DATA_TYPE_TIMES:
      fprint2f(fp1, fp2,"\t%s (%s): avg=%'6.3f\t",grp_data_ptr->graph_name, vector_name,
                    convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"min=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"max=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"succ=%'6.3f\n", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      if(print_mode == 1)
      {
        fprint2f(fp1, fp2, " cum avg=%'6.3f\t cum min=%'6.3f\t cum max=%'6.3f\t cum succ=%'6.3f", 
                            calc_gdf_avg_of_c_avg(grp_data_ptr, gdata), 
                            convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                            convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                            convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_count)); 
      }
      break;
    case DATA_TYPE_TIMES_STD:
      fprint2f(fp1, fp2,"\t%s (%s): avg=%'6.3f\t",grp_data_ptr->graph_name, vector_name,
                    convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"min=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"max=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      fprint2f(fp1, fp2,"succ=%'6.3f\n", convert_data_by_formula(grp_data_ptr->formula, *((Long_data *)(block_ptr))));
      block_ptr = (block_ptr + sizeof(Long_data));

      block_ptr = (block_ptr + sizeof(Long_long_data));

      if(print_mode == 1)
      { 
        fprint2f(fp1, fp2, " cum avg=%'6.3f\t cum min=%'6.3f\t cum max=%'6.3f\t cum succ=%'6.3f", 
                             calc_gdf_avg_of_c_avg(grp_data_ptr, gdata), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_count));
      } 
     break;   
    case DATA_TYPE_SAMPLE_2B_100:
      value = *((Short_data *)(block_ptr));
      if(value == SHORT_NaN)
        value = DOUBLE_NaN;
      else 
         value /=100;
 
      if(print_mode == 0)
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\n",grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value));
      else
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\t min=%'6.3f\t max=%'6.3f\t avg=%'6.3f\n",
                 grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value),
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max),
                 calc_gdf_avg_of_c_avg(grp_data_ptr, gdata));
      block_ptr = (block_ptr + sizeof(Short_data));
      break;
    case DATA_TYPE_SAMPLE_4B:
    case DATA_TYPE_SUM_4B:
    case DATA_TYPE_T_DIGEST:
      value = *((Int_data *)(block_ptr));
      if(value == INT_NaN)
        value = DOUBLE_NaN;

      if(print_mode == 0)
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\n",grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value));
      else
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\t min=%'6.3f\t max=%'6.3f\t avg=%'6.3f\n",
                 grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value),
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                 calc_gdf_avg_of_c_avg(grp_data_ptr, gdata));

      block_ptr = (block_ptr + sizeof(Int_data));
      break;
    case DATA_TYPE_SAMPLE_1B:
      value = *((Char_data *)(block_ptr));
      if(value == CHAR_NaN)
        value = DOUBLE_NaN;

      if(print_mode == 0)
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\n",grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value));
      else
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\t min=%'6.3f\t max=%'6.3f\t avg=%'6.3f\n",
                 grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value),
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min),
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max),
                 calc_gdf_avg_of_c_avg(grp_data_ptr, gdata));

      block_ptr = (block_ptr + sizeof(Char_data));
      break;
    case DATA_TYPE_RATE_4B_1000:
    case DATA_TYPE_SAMPLE_4B_1000:
      value = *((Int_data *)(block_ptr));
      if(value == INT_NaN)
        value = DOUBLE_NaN;
      else 
        value /=1000;
      
      if(print_mode == 0)
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\n",grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value));
      else
        fprint2f(fp1, fp2,"\t%s (%s): %'6.3f\t min=%'6.3f\t max=%'6.3f\t avg=%'6.3f\n",
                 grp_data_ptr->graph_name, vector_name,
                 convert_data_by_formula(grp_data_ptr->formula, value),
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                 convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                 calc_gdf_avg_of_c_avg(grp_data_ptr, gdata));

      block_ptr = (block_ptr + sizeof(Int_data));
      break;
    case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      value = *((Short_data *)(block_ptr));
      if(value == SHORT_NaN)
        value = DOUBLE_NaN;
      else 
        value /=100;

      fprint2f(fp1, fp2,"\t%s (%s): avg=%'6.3f\t",grp_data_ptr->graph_name, vector_name,
               convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Short_data));

      value = *((Int_data *)(block_ptr));
      if(value == INT_NaN)
        value = DOUBLE_NaN;

      fprint2f(fp1, fp2,"succ=%'6.3f\n", convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Int_data));

      if(print_mode == 1)
      {
        fprint2f(fp1, fp2, " cum avg=%'6.3f\t cum min=%'6.3f\t cum max=%'6.3f\t cum succ=%'6.3f", 
                             calc_gdf_avg_of_c_avg(grp_data_ptr, gdata), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_count));
      }

      break;
    case DATA_TYPE_TIMES_4B_10:
      dividend = 10;             //Don't add break statement
    case DATA_TYPE_TIMES_4B_1000:
    case DATA_TYPE_TIMES_STD_4B_1000:
      if(dividend == 1)
        dividend = 1000;
      
      value = *((Int_data *)(block_ptr));

      if(value == INT_NaN)
        value = DOUBLE_NaN;
      else
        value /=dividend;

      fprint2f(fp1, fp2,"\t%s (%s): avg=%'6.3f\t",grp_data_ptr->graph_name, vector_name,
               convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Int_data));
      fprint2f(fp1, fp2,"min=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Int_data));
      fprint2f(fp1, fp2,"max=%'6.3f\t", convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Int_data));
      fprint2f(fp1, fp2,"succ=%'6.3f\n", convert_data_by_formula(grp_data_ptr->formula, value));

      block_ptr = (block_ptr + sizeof(Int_data));

      if(grp_data_ptr->data_type == DATA_TYPE_TIMES_STD_4B_1000)
        block_ptr = (block_ptr + sizeof(Int_data));

      if(print_mode == 1)
      {
        fprint2f(fp1, fp2, " cum avg=%'6.3f\t cum min=%'6.3f\t cum max=%'6.3f\t cum succ=%'6.3f", 
                             calc_gdf_avg_of_c_avg(grp_data_ptr, gdata), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_min), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_max), 
                             convert_data_by_formula_print(grp_data_ptr->formula, gdata->c_count));
      }

      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType {%d} in GDF is not correct\n", grp_data_ptr->data_type); 
      NS_EXIT(-1, CAV_ERR_1013001, grp_data_ptr->data_type);
  }
  return block_ptr;
}

// printing the custom monitor data into progress and summary report.
// This function will match the data type and keep track the block_ptr (msg_data_ptr) according to data type.
char* print_data_by_graph_type_whose_data_not_filled(Graph_Info *grp_data_ptr, char *block_ptr)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  switch(grp_data_ptr->data_type)
  {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_RATE:
    case DATA_TYPE_SUM:
      block_ptr = (block_ptr + sizeof(Long_data)); 
      break;
    case DATA_TYPE_CUMULATIVE:
      block_ptr = (block_ptr + sizeof(Long_long_data));
      break;
    case DATA_TYPE_TIMES:
        block_ptr = (block_ptr + (4 * sizeof(Long_data)));
       /* block_ptr = (block_ptr + sizeof(Long_data));
        block_ptr = (block_ptr + sizeof(Long_data));
        block_ptr = (block_ptr + sizeof(Long_data)); */
      break;
    case DATA_TYPE_TIMES_STD:
      block_ptr = (block_ptr + (5 * sizeof(Long_data)));
      /*block_ptr = (block_ptr + sizeof(Long_data));
      block_ptr = (block_ptr + sizeof(Long_data));
      block_ptr = (block_ptr + sizeof(Long_data));
      block_ptr = (block_ptr + sizeof(Long_long_data)); */
      break;
    case DATA_TYPE_SAMPLE_2B_100:
      block_ptr = (block_ptr + sizeof(Short_data));
      break;
    case DATA_TYPE_SAMPLE_2B_100_COUNT_4B:
      block_ptr = (block_ptr + sizeof(Short_data) + sizeof(Int_data));
      break;
    case DATA_TYPE_SAMPLE_4B:
    case DATA_TYPE_SUM_4B:
    case DATA_TYPE_RATE_4B_1000:
    case DATA_TYPE_SAMPLE_4B_1000:
    case DATA_TYPE_T_DIGEST:
      block_ptr = (block_ptr + sizeof(Int_data));
      break;
    case DATA_TYPE_SAMPLE_1B:
      block_ptr = (block_ptr + sizeof(Char_data));
      break;
    case DATA_TYPE_TIMES_4B_10:
    case DATA_TYPE_TIMES_4B_1000:
      block_ptr = (block_ptr + (4 * sizeof(Int_data)));
      break;
    case DATA_TYPE_TIMES_STD_4B_1000:
      block_ptr = (block_ptr + (5 * sizeof(Int_data)));
      break;
    default:
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: dataType {%d} in GDF is not correct\n", grp_data_ptr->data_type);
      NS_EXIT(-1, CAV_ERR_1013001, grp_data_ptr->data_type);
  }
  return block_ptr;
}

void print_cm_data_ptr(int mon_id, FILE* fp1, FILE* fp2)
{
  int c_graph;
  int i;
  int c_group_numvec = 0;
  int c_graph_numvec = 0;
  int print_mode = 0;
  Graph_Info *local_graph_data_ptr = NULL;
  char *custom_block_local_ptr[MAX_METRIC_PRIORITY_LEVELS + 1];
  int gp_info_index;
  int mon_hml_priority;
  int mon_index, vector_index;
  int graph_hml_priority;
  Graph_Data *gdata;
  char *mon_breadcrumb = NULL;
  CM_info *cm_info_mon_conn_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
  CM_vector_info *vector_list_ptr = NULL;

  gp_info_index = cm_info_mon_conn_ptr->gp_info_index;
  if (gp_info_index < 0)
  return ;
 
  NSDL2_GDF(NULL, NULL, "Method called, gp_info_index = %d, num_group = %d", gp_info_index, cm_info_mon_conn_ptr->num_group);

  Group_Info *local_group_data_ptr = group_data_ptr + gp_info_index;
  print_mode = cm_info_mon_conn_ptr->print_mode;

  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

  /* Loop for each group vector */
  for(mon_index = mon_id; mon_index < mon_id + monitor_list_ptr[mon_id].no_of_monitors; mon_index++)
  {
    cm_info_mon_conn_ptr = monitor_list_ptr[mon_index].cm_info_mon_conn_ptr;
    vector_list_ptr = cm_info_mon_conn_ptr->vector_list;
    for(vector_index = 0; vector_index < cm_info_mon_conn_ptr->total_vectors; c_group_numvec++, vector_index++)
    {
      //skip deleted vectors
      if(( vector_list_ptr[vector_index].flags & RUNTIME_DELETED_VECTOR) || (cm_info_mon_conn_ptr->flags & ALL_VECTOR_DELETED))
      {
        continue;
      }
      NSDL3_GDF(NULL, NULL, "monitor_list_ptr[mon_index].is_dynamic = %d, vector_list_ptr[vector_index].vector_name = %s, "
                          "is_mon_breadcrumb_set = %d", monitor_list_ptr[mon_index].is_dynamic,
                           vector_list_ptr[vector_index].vector_name, (vector_list_ptr[vector_index].flags & MON_BREADCRUMB_SET));

      if((global_settings->hierarchical_view) && (vector_list_ptr[vector_index].flags & MON_BREADCRUMB_SET))
        mon_breadcrumb = vector_list_ptr[vector_index].mon_breadcrumb;   //take breadcrumb for vector name
      else
        mon_breadcrumb = vector_list_ptr[vector_index].vector_name;   //take vector for vector name

      NSDL3_GDF(NULL, NULL, "mon_breadcrumb === %s", mon_breadcrumb);

      mon_hml_priority = cm_info_mon_conn_ptr->metric_priority;
      if(mon_hml_priority == MAX_METRIC_PRIORITY_LEVELS)
        i = MAX_METRIC_PRIORITY_LEVELS;
      else
        i = 0;

      if (vector_list_ptr[vector_index].flags & DATA_FILLED) 
      {
        if(cm_info_mon_conn_ptr->access == RUN_LOCAL_ACCESS)
          fprint2f(fp1, fp2,"    %s (%s):\n",local_group_data_ptr->group_name, mon_breadcrumb);
        if(cm_info_mon_conn_ptr->access == RUN_REMOTE_ACCESS)
          fprint2f(fp1, fp2,"    %s (%s):\n",local_group_data_ptr->group_name, mon_breadcrumb);
    
        local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

        for(; i <= mon_hml_priority; i++)
          custom_block_local_ptr[i] = (char *)(msg_data_ptr + vector_list_ptr[vector_index].rtg_index[i]);
  
        /* If there are two graphs for example, for all group vectors there will be only 
         * two local_graph_data_ptr. So we initialize local_graph_data_ptr here. */
        for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
        {
          graph_hml_priority = local_graph_data_ptr->metric_priority;

          if((local_graph_data_ptr->graph_state == GRAPH_EXCLUDED) || (graph_hml_priority > mon_hml_priority))
          {
            local_graph_data_ptr++;
            continue;
          }
          for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
          {
            NSDL3_GDF(NULL, NULL, "c_group_numvec = %d, c_graph_numvec = %d, local_graph_data_ptr = %p",
                                   c_group_numvec, c_graph_numvec, local_graph_data_ptr);
    
            //Added by Arun to implement print mode 1
            gdata = get_graph_data_ptr(c_group_numvec, c_graph_numvec, local_graph_data_ptr);
            /* function print_data_by_graph_type updates value of custom_block_local_ptr
             * We initialize custom_block_local_ptr once because it is contiguous. */
            custom_block_local_ptr[graph_hml_priority] = print_data_by_graph_type(fp1, fp2, local_graph_data_ptr, custom_block_local_ptr[graph_hml_priority], print_mode, gdata, mon_breadcrumb);
          }
          local_graph_data_ptr++;
        }
      }
      else
      {
        local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
        /* Advance local_graph_data_ptr */
        // Prachi: Fixed Bug - 4843
        for(; i <= mon_hml_priority; i++) 
          custom_block_local_ptr[i] = (char *)(msg_data_ptr + vector_list_ptr[vector_index].rtg_index[i]);

        for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
        {
          graph_hml_priority = local_graph_data_ptr->metric_priority;

          if((local_graph_data_ptr->graph_state == GRAPH_EXCLUDED) || (graph_hml_priority > mon_hml_priority))
          {
            local_graph_data_ptr++;
            continue;
          }
          for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
          {
            NSDL3_GDF(NULL, NULL, "c_group_numvec = %d, c_graph_numvec = %d, local_graph_data_ptr = %p",
                                   c_group_numvec, c_graph_numvec, local_graph_data_ptr);
            /* function print_data_by_graph_type updates value of custom_block_local_ptr
             * We initialize custom_block_local_ptr once because it is contiguous. */
            custom_block_local_ptr[graph_hml_priority] = print_data_by_graph_type_whose_data_not_filled(local_graph_data_ptr, custom_block_local_ptr[graph_hml_priority]);
          }
          local_graph_data_ptr++;
        }
      }
    }
  }
}

//This will write data in gui.data
void log_cm_gp_data_ptr(int custom_mon_id, int gp_info_index, int num_group, int print_mode, char* server_IP)
{
  int i;
  int c_graph;
  int c_group_numvec;
  int c_graph_numvec;
  int mon_hml_priority; 
  int graph_hml_priority; 
  char *custom_block_local_ptr[MAX_METRIC_PRIORITY_LEVELS + 1];
  Graph_Info *local_graph_data_ptr = NULL;
  Graph_Data *gdata;
  char *vector_name = NULL;

  CM_info *cm_ptr = monitor_list_ptr[custom_mon_id].cm_info_mon_conn_ptr;
  CM_vector_info *vector_list = cm_ptr->vector_list;

  Group_Info *local_group_data_ptr = group_data_ptr + gp_info_index;
  num_group = cm_ptr->num_group;
  print_mode = cm_ptr->print_mode;

  if(local_group_data_ptr->rpt_grp_id < GDF_CM_START_RPT_GRP_ID)
    return;

   /* Loop for each group vector */
  for(c_group_numvec = 0, i = 0; c_group_numvec < local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] ; c_group_numvec++, i++)
  {
    if(i > cm_ptr->total_vectors)
    {
      cm_ptr = monitor_list_ptr[++custom_mon_id].cm_info_mon_conn_ptr;
      vector_list = cm_ptr->vector_list;
      i = 0;
    }
    /* Set vector name accordingly
     * If Dynamic vector monitor -> take breadcrumb for vector name,
     * Else take vector_name only 
     */
    if(cm_ptr->flags & MON_BREADCRUMB_SET)
      vector_name = vector_list[i].mon_breadcrumb;   //take breadcrumb for vector name
    else
      vector_name = vector_list[i].vector_name;   //take vector for vector name

    NSDL2_MON(NULL, NULL, "vector_name = %s", vector_name);

    //skip deleted vectors
    if((vector_list[i].flags & RUNTIME_DELETED_VECTOR) || (cm_ptr->flags & ALL_VECTOR_DELETED))
    {
      continue;
    }
    
    mon_hml_priority = cm_ptr->metric_priority;

    if(mon_hml_priority == MAX_METRIC_PRIORITY_LEVELS)
      i = MAX_METRIC_PRIORITY_LEVELS;
    else
      i = 0;

    /* custom_block_local_ptr is initialized once and updated later. it is not reset. */
    for(; i <= mon_hml_priority; i++)
      custom_block_local_ptr[i] = (char *)(msg_data_ptr + vector_list[i].rtg_index[i]);

    //we need to put server name 
    //fprint2f(gui_fp, NULL, "%s:\n",local_group_data_ptr->group_name);
    fprint2f(gui_fp, NULL,"%s - %s:\n",local_group_data_ptr->group_name, server_IP);

    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
    for(c_graph = 0; c_graph < local_group_data_ptr->num_graphs; c_graph++)
    {
      graph_hml_priority = local_graph_data_ptr->metric_priority;

      for(c_graph_numvec = 0; c_graph_numvec < local_graph_data_ptr->num_vectors; c_graph_numvec++)
      {
        //Added by Arun to implement print mode 1
        gdata = get_graph_data_ptr(c_group_numvec, c_graph_numvec, local_graph_data_ptr);
        custom_block_local_ptr[graph_hml_priority] = print_data_by_graph_type(gui_fp, NULL, local_graph_data_ptr, custom_block_local_ptr[graph_hml_priority], print_mode, gdata, vector_name);
      }
      local_graph_data_ptr++;
    }
  }
}

//TM: will be called from runtime_changes_monitor for delete monitor  -done
int get_rpt_grp_id_by_gp_info_index(int gp_info_index)
{
  int group_id;

  NSDL1_MON(NULL, NULL, "Method called. group_num = %d", gp_info_index);

  Group_Info *local_group_data_ptr = NULL;
  local_group_data_ptr = group_data_ptr + gp_info_index;

  group_id = local_group_data_ptr->rpt_grp_id;

  NSDL1_MON(NULL, NULL, "Method End. group_id = %d", group_id);
  return group_id;
}

void check_rtg_size()
{
  FILE *diff_fp =  NULL;
  char next_gdf_file[MAX_LINE_LENGTH];
  char prev_gdf_file[MAX_LINE_LENGTH];
  char next_diff_file[MAX_LINE_LENGTH];
  char cur_rtg_file[MAX_LINE_LENGTH];
  char cmd[2*MAX_LINE_LENGTH + 4];
  char old_buf[MAX_LINE_LENGTH];
  char new_buf[MAX_LINE_LENGTH];
  char printLine[MAX_LINE_LENGTH];
  char err_msg[1024] = "\0";
  struct stat stat_buf;

  if(test_run_gdf_count > 0)
  {
    sprintf(cur_rtg_file, "%s/logs/%s/rtgMessage.dat.%d", g_ns_wdir, global_settings->tr_or_partition, test_run_gdf_count);
    sprintf(prev_gdf_file, "%s/logs/%s/testrun.gdf.%d", g_ns_wdir, global_settings->tr_or_partition, test_run_gdf_count);
  }
  else
  {
    sprintf(cur_rtg_file, "%s/logs/%s/rtgMessage.dat", g_ns_wdir, global_settings->tr_or_partition);
    sprintf(prev_gdf_file, "%s/logs/%s/testrun.gdf", g_ns_wdir, global_settings->tr_or_partition);
  }
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "msg data size =%ld",msg_data_size);
  //if msg_data_size is greter then max_rtg_size then multiplying its size with 5 
  if(msg_data_size > global_settings->max_rtg_size)
  { 
      
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "RTG size is less than (0.005)Mb ,Hence multiplying its size by 5 ");
    global_settings->max_rtg_size= global_settings->max_rtg_size * 5;
  }
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "max rtg size =%lld",global_settings->max_rtg_size);
 //  MLTL1(EL_F, 0, 0, _FLN_,NULL, EID_DATAMON_GENERAL, EVENT_INFO,
   //                      "MAX RTG SIZE definr by yash is=%lld",global_settings->max_rtg_size); 
  //if max_rtg_size is greater then 2GB then setting max_rtg_size to 2GB
  if(global_settings->max_rtg_size > RTG_MAX_IN_GB )
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "RTG size is greater than 2Gb ,Hence resetting it to 2Gb");
    global_settings->max_rtg_size= RTG_MAX_IN_GB;
  }
  
  if ( (stat(cur_rtg_file, &stat_buf) == 0) && ((stat_buf.st_size + msg_data_size) > global_settings->max_rtg_size) )
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Existing rtgMessage.dat size exceed maximum size limit [ %ld (mb) ]. Creating new file." , (global_settings->max_rtg_size/(1024 * 1024)));
    //close old rtgMessage.dat
    if(rtg_fp != NULL) { fclose(rtg_fp); rtg_fp = NULL; }

    //Increment testrun.gdf/rtgMessage.dat name counter
    test_run_gdf_count++;

    g_testrun_rtg_timestamp = time(NULL) * 1000;

    sprintf(next_gdf_file, "%s/logs/%s/testrun.gdf.%d.%llu", g_ns_wdir, global_settings->tr_or_partition, test_run_gdf_count, g_testrun_rtg_timestamp);

    sprintf(next_diff_file, "%s/logs/%s/testrun.gdf.diff.%d", g_ns_wdir, global_settings->tr_or_partition,test_run_gdf_count);
  
    sprintf(cmd, "cp %s %s", prev_gdf_file , next_gdf_file);
   
    int  status = nslb_system(cmd,1,err_msg);
    if(status != 0)
    {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Can not create new rtgmessage.dat due to Error in runing command [%s] ",cmd);
       NS_EXIT(-1, CAV_ERR_1013026,cmd);
    }

    //Making link of testrun.gdf.count.timestamp as testrun.gdf.count
    
    sprintf(old_buf, "%s", next_gdf_file);
    sprintf(new_buf, "%s/logs/TR%d/%lld/testrun.gdf.%d", g_ns_wdir, testidx, g_partition_idx, test_run_gdf_count);
    //link(old_buf, new_buf);
    if((link(old_buf, new_buf)) == -1)
    {
      if((symlink(old_buf, new_buf)) == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Unable to create link %s, err = %s", new_buf, nslb_strerror(errno));
      }
    }
    NSDL2_GDF(NULL, NULL, "Created link of %s in %s", old_buf, new_buf);

    if ((diff_fp = fopen(next_diff_file, "w+")) == NULL)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Unable to open %s \n", next_diff_file); 
      NS_EXIT(-1, CAV_ERR_1013025, next_diff_file);
    }

    sprintf(printLine,"Info|%s|-|%d|%u|%ld|%d|-",
      version, (unsigned int)sizeof(Msg_data_hdr),
      testidx, tmp_msg_data_size,
      global_settings->progress_secs);

    fprintf(diff_fp, "%s\n", printLine); 

    close_gdf(diff_fp);

    create_rtg_file_data_file_send_start_msg_to_server(test_run_gdf_count);
  
 }
  NSDL2_MESSAGES(NULL,NULL, "rtgMessage.dat size including current packe ise (%ld)  and max size limit for rtgMessage.dat is %ld", ((stat_buf.st_size + msg_data_size)/(1024 * 1024)), (global_settings->max_rtg_size/(1024 * 1024)) );  

}

int init_dyn_objs_and_gdf_processing()
{
  char fname[1024];
  int i, j;
  int row_num = -1;
  DynObjForGDF *dyn_obj;
  int num_elements;

  NSDL1_MON(NULL, NULL, "Method called");
  memset(dynObjForGdf, 0, MAX_DYN_OBJS * sizeof(DynObjForGDF));

  //Only setting set_group_idx_for_txn for TX as process_group is called twice if only one is used
  set_group_idx_for_txn = 1;

  /*===========================================================================
      Initialize array of dynObjForGdf[] for - 
        [0] HTTP Status Code
        [1] Transacton Stats
        [2] Transaction Cummulative Stats
        [3] RBU Domain Stats   
        [4] Server IP Stats
        [5] TCP Client Failures 
        [6] UDP Client Failures 
   ==========================================================================*/

  // [0] HTTP Status Code 
  sprintf(fname, "%s/etc/ns_http_response_codes.gdf", g_ns_wdir);

  MALLOC_AND_COPY(fname, dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].gdf_name, (strlen(fname) + 1), "Dyn Objs gdf name for HTTP Status Code stats", -1);

  get_no_of_elements_of_gdf(fname, &num_elements, &dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].rtg_group_size,
                 &dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].pdf_id, &dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].graphId,
                  dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].graphDesc);

  dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total = total_http_resp_code_entries; // Can be 0 if no static tx used 
  dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].normTable = &status_code_normtbl;

  NSDL2_GDF(NULL, NULL, "Status Code Stats: Gdf name = %s, num data element = %d, total_entries = %d",
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].gdf_name, dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].rtg_group_size,
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_STATUS_CODE].total);

  // [1] Transacton Stats 
  sprintf(fname, "%s/etc/ns_trans_stats.gdf", g_ns_wdir);

  MALLOC_AND_COPY(fname, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].gdf_name, (strlen(fname) + 1), "Dyn Objs gdf name for Txn stats", -1);

  get_no_of_elements_of_gdf(fname, &num_elements, &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].rtg_group_size, 
                 &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].pdf_id, &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].graphId, 
                  dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].graphDesc); 

  dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total = total_tx_entries; // Can be 0 if no static tx used 
  dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable = &normRuntimeTXTable;

  NSDL2_GDF(NULL, NULL, "Txn Stats: Gdf name = %s, num data element = %d, total_entries = %d", 
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].gdf_name, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].rtg_group_size, 
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total);

  // [2] Transacton Cum Stats
  sprintf(fname, "%s/etc/ns_trans_cumulative_stats.gdf", g_ns_wdir);

  MALLOC_AND_COPY(fname, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].gdf_name, (strlen(fname) + 1), "Dyn Objs gdf name for Txn stats cumulative", -1);

  get_no_of_elements_of_gdf(fname, &num_elements, &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].rtg_group_size, 
                 &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].pdf_id, &dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].graphId, 
                  dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].graphDesc);


  if(global_settings->g_tx_cumulative_graph != 0)
    dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].total = total_tx_entries;

  dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].normTable = &normRuntimeTXTable;

  NSDL2_GDF(NULL, NULL, "Txn stats Cumulative: Gdf name = %s, num data element = %d, total_entries = %d", 
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].gdf_name, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].rtg_group_size, 
                         dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].total);

  // [3] RBU Domain Stats 
  if(global_settings->rbu_domain_stats_mode)
  {
    sprintf(fname, "%s/sys/cm_rbu_domain_stat.gdf", g_ns_wdir);

    MALLOC_AND_COPY(fname, dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].gdf_name, (strlen(fname) + 1), 
                           "Dyn Objs gdf name for RBU Domain stat", -1);

    get_no_of_elements_of_gdf(fname, &num_elements, &dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].rtg_group_size, 
                                     &dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].pdf_id, 
                                     &dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].graphId, 
                                     dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].graphDesc);

    dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total = total_rbu_domain_entries;
    dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].normTable = &rbu_domian_normtbl;

    NSDL2_GDF(NULL, NULL, "RBU Domain Stat: Gdf name = %s, num data element = %d, total_entries = %d", 
                             dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].gdf_name, 
                             dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].rtg_group_size, 
                             dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total);
  }

  // [4] Server IP Stats 
  if(SHOW_SERVER_IP){
    sprintf(fname, "%s/etc/ns_srv_ip_data.gdf", g_ns_wdir);
    MALLOC_AND_COPY(fname, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].gdf_name, (strlen(fname) + 1), "Dyn Objs gdf name for ServerIp stats", -1);
    get_no_of_elements_of_gdf(fname, &num_elements, &dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_group_size, &dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].pdf_id, &dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].graphId, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].graphDesc);
    dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total = total_normalized_svr_ips;

    dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].normTable = &normServerIPTable;

    NSDL2_GDF(NULL, NULL, "ServerIp Stats: Gdf name = %s, num data element = %d, total_entries = %d", dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].gdf_name, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_group_size, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total);

  }

  // [5] TCP Client Failues Stats 
  if (IS_TCP_CLIENT_API_EXIST)
  {
    dyn_obj = &dynObjForGdf[NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES];
   
    int len = sprintf(fname, "%s/etc/ns_socket_tcp_client_failure_stats.gdf", g_ns_wdir);
   
    MALLOC_AND_COPY(fname, dyn_obj->gdf_name, (len+ 1), "DynObj-TCPClientFailures-GDFName", -1);
   
    get_no_of_elements_of_gdf(fname,&num_elements, &(dyn_obj->rtg_group_size), &(dyn_obj->pdf_id), 
        &(dyn_obj->graphId), dyn_obj->graphDesc);
   
    dyn_obj->total = g_total_tcp_client_errs;
    dyn_obj->normTable = &g_tcp_client_errs_normtbl;
   
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats] DynObj-TCPClientFailuresGdf: "
        "GDFName = %s, num data element = %d, total_entries = %d", 
         dyn_obj->gdf_name, dyn_obj->rtg_group_size, dyn_obj->total);
  }

  // [6] TCP Client Failues Stats 
  if (IS_UDP_CLIENT_API_EXIST)
  {
    dyn_obj = &dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES];
   
    int len = sprintf(fname, "%s/etc/ns_socket_udp_client_failure_stats.gdf", g_ns_wdir);
   
    MALLOC_AND_COPY(fname, dyn_obj->gdf_name, (len+ 1), "DynObj-UDPClientFailures-GDFName", -1);
   
    get_no_of_elements_of_gdf(fname, &num_elements, &(dyn_obj->rtg_group_size), &(dyn_obj->pdf_id), 
        &(dyn_obj->graphId), dyn_obj->graphDesc);
   
    dyn_obj->total = g_total_udp_client_errs;
    dyn_obj->normTable = &g_udp_client_errs_normtbl;
   
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats] DynObj-UDPClientFailuresGdf: "
        "GDFName = %s, num data element = %d, total_entries = %d", 
         dyn_obj->gdf_name, dyn_obj->rtg_group_size, dyn_obj->total);
  }

  // Loop from 1 as 0 index is not used
  for(i = 1; i < MAX_DYN_OBJS; i++)
  {
    NSDL2_GDF(NULL, NULL, "dynObjForGdf[%d], Gdf name = %s, num data element = %d, total_entries = %d", 
                            i, dynObjForGdf[i].gdf_name, dynObjForGdf[i].rtg_group_size, dynObjForGdf[i].total);
 
    MY_MALLOC_AND_MEMSET(dynObjForGdf[i].rtg_index_tbl, ((sgrp_used_genrator_entries + 1)* sizeof(int *)), 
                           "Initial row allocation of DynObjForGdf rtg index table", i);

    dynObjForGdf[i].max_rtg_index_tbl_row_entries = sgrp_used_genrator_entries + 1;
 
    if(dynObjForGdf[i].total != 0)
    {
      dynObjForGdf[i].is_gp_info_filled = 1;
      for(j = 0; j < (sgrp_used_genrator_entries + 1); j++)
      {
        MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(dynObjForGdf[i].rtg_index_tbl[j], ((dynObjForGdf[i]).total * sizeof(int)), 
                   (dynObjForGdf[i].max_rtg_index_tbl_col_entries * sizeof(int)), 
                   "Initial column allocation of Transaction rtg index array", -1);
      }
      dynObjForGdf[i].max_rtg_index_tbl_col_entries = dynObjForGdf[i].total;
      NSDL2_GDF(NULL, NULL, "max_rtg_index_tbl_row_entries = %d, max_rtg_index_tbl_col_entries = %d", 
                             dynObjForGdf[i].max_rtg_index_tbl_row_entries, dynObjForGdf[i].max_rtg_index_tbl_col_entries);
    }

    if((dynObjForGdf[i].gdf_name != NULL) && (dynObjForGdf[i].total != 0))   //TO handle dynObjs allocated but not to be used.
    {
      NSDL2_GDF(NULL, NULL, "Process gdf = %s", dynObjForGdf[i].gdf_name);
      process_gdf(dynObjForGdf[i].gdf_name, 0, NULL, i);
      dynObjForGdf[i].is_gp_info_filled = DYNOBJ_GROUP_INFO_IS_FILLED;
    }

    //Manish: Why we are saving pdf index if no static tx found???
    else if((i == NEW_OBJECT_DISCOVERY_TX) && (dynObjForGdf[i].total == 0))  //This will be in case of transaction stats
    {  
     //Passing entries hard coded to prevent any more entry in structure.
     save_pdf_data_index(102);
     chk_and_add_in_pdf_lookup_table(dynObjForGdf[i].graphDesc, &dynObjForGdf[i].pdf_id, &row_num, 102, dynObjForGdf[i].graphId, g_runtime_flag);  
    }

    dynObjForGdf[i].startId = dynObjForGdf[i].total; // Must be set to total for next run time change
    dynObjForGdf[i].total = 0; // Must set back to 0
  }

  set_group_idx_for_txn = 0;

  return 0;
}


int check_if_realloc_needed_for_dyn_obj(int dyn_obj_idx)
{
  int genId;
  int total_count = dynObjForGdf[dyn_obj_idx].startId + dynObjForGdf[dyn_obj_idx].total;
  
  NSDL2_GDF(NULL, NULL, "Method called, dyn_obj_idx = %d, total_count = %d, start = %d, total Domian discocered = %d", 
                         dyn_obj_idx, total_count, dynObjForGdf[dyn_obj_idx].startId, dynObjForGdf[dyn_obj_idx].total);

  if(total_count > dynObjForGdf[dyn_obj_idx].max_rtg_index_tbl_col_entries)
  {
    for(genId = 0; genId < (sgrp_used_genrator_entries + 1); genId++)
    {
      NSDL2_GDF(NULL, NULL, "Goint to realloc dynObjForGdf. GenId = %d, Old count = %d, New count = %d, "
                             "Max_col_entries = %d, total_count = %d", 
                             genId, dynObjForGdf[dyn_obj_idx].max_rtg_index_tbl_col_entries, 
                             (total_count + DELTA_DYN_OBJ_RTG_IDX_ENTRIES), 
                             dynObjForGdf[dyn_obj_idx].max_rtg_index_tbl_col_entries, total_count);

      MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId], 
             ((total_count + DELTA_DYN_OBJ_RTG_IDX_ENTRIES)* sizeof(int)), 
             (dynObjForGdf[dyn_obj_idx].max_rtg_index_tbl_col_entries * sizeof(int)), 
             "column Reallocation of dynObjForGdf rtg index array", -1);
    }
    dynObjForGdf[dyn_obj_idx].max_rtg_index_tbl_col_entries = total_count + DELTA_DYN_OBJ_RTG_IDX_ENTRIES; // with delta entries
  }
 
  return 0;
}


void set_rtg_index_for_dyn_objs_discovered()
{
  int genId, i, total_count, dyn_obj_idx;

  NSDL2_GDF(NULL, NULL, "Method called, update");

  if(g_rtg_rewrite)
    tmp_msg_data_size = msg_data_size;

  for(dyn_obj_idx = 1; dyn_obj_idx < MAX_DYN_OBJS; dyn_obj_idx++)
  {
    NSDL2_GDF(NULL, NULL, "dyn_obj_idx = %d, total = %d, startId = %d, sgrp_used_genrator_entries = %d", 
                           dyn_obj_idx, dynObjForGdf[dyn_obj_idx].total, dynObjForGdf[dyn_obj_idx].startId,
                           sgrp_used_genrator_entries);
    
    if((!g_rtg_rewrite) && (dynObjForGdf[dyn_obj_idx].total <= 0))
      continue;

    total_count = dynObjForGdf[dyn_obj_idx].startId + dynObjForGdf[dyn_obj_idx].total;
    NSDL2_GDF(NULL, NULL, "RTG index set for Objects discovered runtime. startId = %d, total = %d total_count = %d, rtg_group_size = %d", 
                           dynObjForGdf[dyn_obj_idx].startId, dynObjForGdf[dyn_obj_idx].total, total_count, 
                           dynObjForGdf[dyn_obj_idx].rtg_group_size);

    for(genId = 0; genId < (sgrp_used_genrator_entries + 1); genId++)
    {
      // Fill rtg_index_tbl for newly discovered object only
      if(g_rtg_rewrite)
        i = 0;
      else
        i = dynObjForGdf[dyn_obj_idx].startId;

      for(; i < total_count; i++)
      {
        dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i] = tmp_msg_data_size;
        tmp_msg_data_size += (dynObjForGdf[dyn_obj_idx].rtg_group_size);

        NSDL2_GDF(NULL, NULL, "RTG index set for Objects discovered runtime. ObjType = %d, vector = %d, genId = %d, rtg_Index = %d", 
                               dyn_obj_idx, i, genId,
                               dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i]);
      }
    }
  }
}

void write_data_for_dyn_objs(int idx, char ***TwoD, int numVector)
{
  char *name;
  char buff[2048];
  char vector_buff[1024];
  int id = 0;
  int i, j, index;
  char prefix[1024];

  NSDL2_GDF(NULL, NULL, "Method called, idx = %d, numVector = %d, startId = %d, total = %d", 
                         idx, numVector, dynObjForGdf[idx].startId, dynObjForGdf[idx].total);

  int count = (dynObjForGdf[idx].startId + dynObjForGdf[idx].total);
 
  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    for(j = 0; j < count; j++)
    {  
      index = j;

      if(i == 0 || idx == NEW_OBJECT_DISCOVERY_RBU_DOMAIN) {
        name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, j);
      } else if(idx == NEW_OBJECT_DISCOVERY_TX || idx == NEW_OBJECT_DISCOVERY_TX_CUM){
          index = g_tx_loc2norm_table[i-1].nvm_tx_loc2norm_table[j];
          if(index !=-1)
            name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, index);
      } else if(idx == NEW_OBJECT_DISCOVERY_SVR_IP){
          index = g_svr_ip_loc2norm_table[i-1].nvm_svr_ip_loc2norm_table[j];
          if(index != -1)
            name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, index);
      } else if(idx == NEW_OBJECT_DISCOVERY_STATUS_CODE){
          index = g_http_status_code_loc2norm_table[i-1].nvm_http_status_code_loc2norm_table[j];
          if(index != -1)
            name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, index);
      } else if(idx == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){
          index = g_tcp_clinet_errs_loc2normtbl[i - 1].loc2norm[j];
          if(index != -1)
            name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, index);
      } else if(idx == NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES){
          index = g_udp_clinet_errs_loc2normtbl[i - 1].loc2norm[j];
          if(index != -1)
            name = nslb_get_norm_table_data(dynObjForGdf[idx].normTable, index);
      }

      if(index != -1){
        sprintf(vector_buff, "%s%s", prefix, name);
        sprintf(buff, "%s %d", vector_buff, dynObjForGdf[idx].rtg_index_tbl[i][index]);
        NSDL2_GDF(NULL, NULL, "vector_buff = %s, rtg_index_tbl = %d", vector_buff, dynObjForGdf[idx].rtg_index_tbl[i][j]);
        fprintf(write_gdf_fp, "%s\n", buff);
        fill_2d(*TwoD, index, vector_buff);
      }
      id++;
    }
  }
}

int write_vectors_for_dyn_objs(int idx, int numVector)
{
  char **TwoD = NULL;
  int group_vec_idx = 0, graph_vec_idx = 0, graph = 0;
  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called, idx = %d, numVector = %d", idx, numVector);

  local_group_data_ptr = group_data_ptr + dynObjForGdf[idx].gp_info_idx;
  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

  NSDL2_GDF(NULL, NULL, "gp_info_idx = %d, local_group_data_ptr = %p, local_group_data_ptr->num_vectors = %d, local_graph_data_ptr =  %p, "
                        "local_group_data_ptr->graph_info_index = %d", 
                         dynObjForGdf[idx].gp_info_idx, local_group_data_ptr, local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS], 
                         local_graph_data_ptr, local_group_data_ptr->graph_info_index);

  if(numVector > local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]) //new vector added in this group
  {
    //to reset and set group table vector name list
    for(graph = 0; graph < local_group_data_ptr->num_graphs; graph++, local_graph_data_ptr++)
    {
      MY_REALLOC_AND_MEMSET(local_graph_data_ptr->graph_data, (numVector * sizeof(int **)),
                      (local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] * sizeof(int **)), 
                       "local_graph_data_ptr->graph_data", -1);
 
      for(group_vec_idx = local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]; group_vec_idx < numVector; group_vec_idx++)
      {
        MY_MALLOC(local_graph_data_ptr->graph_data[group_vec_idx], local_graph_data_ptr->num_vectors * sizeof(int *), 
                   "local_graph_data_ptr->graph_data[group_vec_idx]", -1);

        for(graph_vec_idx = 0; graph_vec_idx < local_graph_data_ptr->num_vectors; graph_vec_idx++)
        {
          MY_MALLOC (local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx], sizeof(Graph_Data), 
                   "local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx]", -1);
          init_graph_data_node(((Graph_Data *)local_graph_data_ptr->graph_data[group_vec_idx][graph_vec_idx]));
        }
      }
    }
  }

  if(local_group_data_ptr->vector_names)
    free_vector_names((local_group_data_ptr->vector_names), local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS]);

  //allocate array of vector names for group table
  TwoD = init_2d(numVector * (sgrp_used_genrator_entries + 1));
 
  write_data_for_dyn_objs(idx, &TwoD, numVector);

  //update array of vector names present in group table
  local_group_data_ptr->vector_names = TwoD;

  //update num vectors in group table
  local_group_data_ptr->num_vectors[MAX_METRIC_PRIORITY_LEVELS] = numVector;

  return 0;
}

inline void process_gdf_on_runtime_for_dyn_objs(char *fname, int dyn_obj_idx)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  int numVector = 1, numGraph = 0, rpGroupID = 0;	//int  i = 0;
  char tmpbuff[MAX_LINE_LENGTH];
  char groupName [MAX_LINE_LENGTH];
  char groupType[MAX_LINE_LENGTH];
  char groupHierarchy[MAX_LINE_LENGTH];
  char groupDescription[MAX_LINE_LENGTH];
  char groupMetric[MAX_LINE_LENGTH];

  NSDL2_GDF(NULL, NULL, "Method called, dyn_obj_idx = %d, runtime processing gdf: %s", dyn_obj_idx, fname);

  read_gdf_fp = open_gdf(fname);

  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) !=NULL)
  {
    line[strlen(line) - 1] = '\0';

    NSDL3_GDF(NULL, NULL, "line = %s", line);

    if(sscanf(line, "%s", tmpbuff) == -1)
      continue;
    if(line[0] == '#' || line[0] == '\0' || (!(strncasecmp(line, "info|", strlen("info|")))))
      continue;
    else if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      NSDL2_GDF(NULL, NULL, "group_count = %d, is_gp_info_filled = %d, set_group_idx_for_txn = %d", 
                             group_count, dynObjForGdf[dyn_obj_idx].is_gp_info_filled, set_group_idx_for_txn);

      //process Group line
      process_group(read_gdf_fp, line, 0, fname, 0, NULL, &numGraph, &rpGroupID, groupName, groupType, 
                    groupHierarchy, groupDescription, groupMetric, dyn_obj_idx);

      numVector = ((dynObjForGdf[dyn_obj_idx].startId + dynObjForGdf[dyn_obj_idx].total) * (sgrp_used_genrator_entries + 1));
       
      NSDL2_GDF(NULL, NULL, "dyn_obj_idx = %d, is_gp_info_filled = %d", dyn_obj_idx, dynObjForGdf[dyn_obj_idx].is_gp_info_filled);
      if(!dynObjForGdf[dyn_obj_idx].is_gp_info_filled && (set_group_idx_for_txn != 1))
      {
        NSDL2_GDF(NULL, NULL, "Filling group info for dynobj = %d", dyn_obj_idx);
        fill_group_info(fname, groupName, rpGroupID, groupType, numGraph, numVector, NULL, groupHierarchy, groupDescription, groupHierarchy, groupMetric);
      }

      if(!g_rtg_rewrite)
        write_vectors_for_dyn_objs(dyn_obj_idx, numVector);

      if(set_group_idx_for_txn == 0)
      {
        if(!dynObjForGdf[dyn_obj_idx].is_gp_info_filled)
        {
          process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 0, 0, group_count, NULL, 0);
          dynObjForGdf[dyn_obj_idx].is_gp_info_filled = 1;
          group_count++;
        }
        else
        {
          process_graph(read_gdf_fp, numGraph, rpGroupID, numVector, 1, 0, dynObjForGdf[dyn_obj_idx].gp_info_idx, NULL, 0);
        }
      }

      NSDL3_GDF(NULL, NULL, "numGraph = %d", numGraph);  
      break;
    }
    else if(!(strncasecmp(line, "graph|", strlen("graph|"))))
    {
      NSTL1_OUT(NULL, NULL,"Number of graphs are greater than no. of graph mentioned in group line of %s monitor", 
                                                                             group_data_ptr[total_groups_entries -1].group_name);
      continue; //should not come here
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Invalid line = %s\n", line);
      NS_EXIT(-1, CAV_ERR_1013024, line);
    }
  }

  NSDL3_GDF(NULL, NULL, "Closing gdf");
  close_gdf(read_gdf_fp);
}



//Parsing gdf file for getting number of graphs
int get_actual_no_of_graphs(char *gdf_name, char *groupName, int *num_actual_data, void *cm_info, int *num_hml_graphs)
{
  FILE *read_gdf_fp = NULL;
  int count = 0;
  char line[MAX_LINE_LENGTH + 1];
  char *fields[GDF_MAX_FIELDS];
  int num_fields;
  int group_found = 0;
  int graph_priority;
   
  *num_actual_data = 0;

  NSDL2_GDF(NULL, NULL, "Method called, gdf_name = %s, groupName = %s, cm_info = %p", gdf_name, groupName, cm_info);

  if(!strncmp(gdf_name,"NA", 2))
    return count;

  read_gdf_fp = open_gdf(gdf_name);

  while (nslb_fgets(line, MAX_LINE_LENGTH, read_gdf_fp, 0) != NULL)
  {
    line[strlen(line) - 1] = '\0';
    NSDL3_GDF(NULL, NULL, "line = %s", line);

    num_fields = get_tokens(line, fields, "|", GDF_MAX_FIELDS);

    if(num_fields<=0)
      continue;

    if(fields[0][0] == '#' || fields[0][0] == '\0')
      continue;
    else if(!(strncasecmp(fields[0], "info", strlen("info"))))
      continue;
    else if((!(strncasecmp(fields[0], "group", strlen("group")))))
    {
      //groupName will be NULL, if we are processing gdf to write diff file.
      if (!groupName || !strncasecmp(fields[1], groupName, strlen(groupName)))
      {
        //group_found is taken for the groups present in netstorm.gdf. There are many groups in nettsorm.gdf and it will always process the graphs of first group present. So had to handle it this way.
        group_found = 1;
      }

      if(count != 0)
        break;

      continue;
    }
    else if(group_found && (!(strncasecmp(fields[0], "graph", strlen("graph")))))
    {
      int mp=MAX_METRIC_PRIORITY_LEVELS;
      if(cm_info != NULL)
        mp=(((CM_info *)cm_info)->metric_priority);

      if((graph_priority = is_graph_excluded(fields[11], fields[8], mp)) != GRAPH_EXCLUDED)
      {
        if(num_hml_graphs)
          num_hml_graphs[graph_priority]++; 
        getSizeOfGraph(data_type_to_num(fields[4]), num_actual_data);
        count = count + 1;
      }
      continue;
    }
  }

  NSDL3_GDF(NULL, NULL, "Closing gdf, num_actual_data = %d, count = %d, group_element_size = %d", 
                         *num_actual_data, count, cm_info?((CM_info *)cm_info)->group_element_size[MAX_METRIC_PRIORITY_LEVELS]:-1);

  close_gdf(read_gdf_fp);

  return count;
}

