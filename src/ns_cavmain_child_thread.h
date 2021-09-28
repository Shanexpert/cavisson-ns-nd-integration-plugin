#ifndef CAV_MAIN_CHILD_THREAD_H 
#define CAV_MAIN_CHILD_THREAD_H

#include "ns_msg_def.h"
#include "ns_msg_com_util.h"
#include "nslb_map.h"
#include "url.h"
#include "util.h"
#include "ns_http_script_parse.h"
#include "ns_embd_objects.h"
#include "ns_trans_parse.h"
#include "netstorm.h"
#include "ns_random_vars.h"
#include "ns_date_vars.h"
#include "ns_random_string.h"
#include "ns_unique_numbers.h" 
#include "ns_index_vars.h"
#include "ns_rbu_domain_stat.h"
#include "ns_http_status_codes.h"
#include "ns_trans_normalization.h"
#include "ns_user_monitor.h"
#include "ns_http_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_xmpp.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_rbu_page_stat.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_network_cache_reporting.h"
#include "ns_socket_tcp_client_rpt.h"
#include "ns_group_data.h"
#include "ns_page_based_stats.h"
#include "ns_socket_udp_client_failures_rpt.h"
#include "ns_socket_tcp_client_failures_rpt.h"
#include "ns_test_monitor.h"
#include "ns_page.h"
#include "unique_vals.h"
#include "ns_cavmain.h"

/*****************************************************************************/

#define SM_MON_ID_MAX_SIZE 256 // Scenario name
extern NSLBMap *sm_mon_map;

#ifdef CAV_MAIN
extern __thread int user_svr_table_size;
extern __thread int user_group_table_size;
extern __thread int user_cookie_table_size;
extern __thread int user_dynamic_vars_table_size;
extern __thread int user_var_table_size;
extern __thread int user_order_table_size;
extern __thread SSL_CTX **g_ssl_ctx;
extern __thread int g_cache_avgtime_idx;
extern __thread int g_network_cache_stats_avgtime_idx;
extern __thread int dns_lookup_stats_avgtime_idx;
extern __thread int g_ftp_avgtime_idx;
extern __thread int g_imap_avgtime_idx;
extern __thread int g_jrmi_avgtime_idx;
extern __thread int g_ws_avgtime_idx;
extern __thread int g_xmpp_avgtime_idx;
extern __thread int g_tcp_client_avg_idx;
extern __thread int g_udp_client_avg_idx;
extern __thread int g_avg_um_data_idx;

extern __thread unsigned int group_data_gp_idx;
extern __thread unsigned int rbu_page_stat_data_gp_idx;
extern __thread unsigned int page_based_stat_gp_idx;
extern __thread unsigned int show_vuser_flow_idx;
extern __thread int g_udp_client_failures_avg_idx;
extern __thread NormObjKey g_udp_client_errs_normtbl;
extern __thread int g_total_udp_client_errs;
extern __thread int g_tcp_client_failures_avg_idx;
extern __thread NormObjKey g_tcp_client_errs_normtbl;
extern __thread int g_total_tcp_client_errs;
extern __thread NormObjKey normRuntimeTXTable;
extern __thread int g_ldap_avgtime_idx;

extern __thread int total_pagereloadprof_entries;
extern __thread int max_pagereloadprof_entries;
extern __thread int total_pageclickawayprof_entries;
extern __thread int max_pageclickawayprof_entries;
extern __thread int g_auto_fetch_info_total_size;
extern __thread int total_cookie_entries;
extern __thread int total_reqcook_entries;
extern __thread int max_cookie_hash_code;
extern __thread int cur_post_buf_len;
extern __thread int rbu_web_url_host_id;
extern __thread int end_inline_url_count;
extern __thread int static_host_table_shm_size;
extern __thread int total_vendor_locations;
extern __thread int used_tempbuffer_space;
extern __thread int total_xmpp_request_entries;
extern __thread int total_fc2_request_entries;
extern __thread ns_bigbuf_t max_buffer_space;
extern __thread int total_userprofshr_entries;
extern __thread ProfilePctCountTable *prof_pct_count_table;
extern __thread int num_dyn_host_left;
extern __thread PerHostSvrTableEntry_Shr* actsvr_table_shr_mem;
extern __thread int is_static_host_shm_created;
extern __thread int num_dyn_host_add;
extern __thread int used_buffer_space;
extern __thread int total_randomvar_entries;
extern __thread int total_randomstring_entries;
extern __thread int total_uniquevar_entries;
extern __thread int total_datevar_entries;
extern __thread int total_serverorder_entries;
extern __thread int total_post_entries;
extern __thread int total_http_method;
extern __thread int total_jsonvar_entries;
extern __thread int total_perpagejsonvar_entries;
extern __thread int total_checkpoint_entries;
extern __thread int total_perpagechkpt_entries;
extern __thread int total_checkreplysize_entries;
extern __thread int total_perpagechkrepsize_entries;
extern __thread int g_static_vars_shr_mem_size;
extern __thread int total_searchvar_entries;
extern __thread int total_perpageservar_entries;
extern __thread CheckPointTableEntry_Shr *checkpoint_table_shr_mem;
extern __thread CheckReplySizeTableEntry_Shr *checkReplySizeTable_shr_mem;
#else
extern int g_auto_fetch_info_total_size;
extern int total_cookie_entries;
extern int total_reqcook_entries;
extern int max_cookie_hash_code;
extern int rbu_web_url_host_id;
extern int end_inline_url_count;
extern int static_host_table_shm_size;
extern int g_ws_avgtime_idx;
extern int total_vendor_locations;
extern int used_tempbuffer_space;
extern int total_xmpp_request_entries;
extern int total_fc2_request_entries;
extern ns_bigbuf_t max_buffer_space;
extern int total_userprofshr_entries;
extern ProfilePctCountTable *prof_pct_count_table;
extern int num_dyn_host_left;
//static int is_static_host_shm_created;
//static int num_dyn_host_add;
//static PerHostSvrTableEntry_Shr* actsvr_table_shr_mem;
extern int user_svr_table_size;
extern int user_group_table_size;
extern int user_cookie_table_size;
extern int user_dynamic_vars_table_size;
extern int user_var_table_size;
extern int user_order_table_size;
extern SSL_CTX **g_ssl_ctx;
extern int g_ws_avgtime_idx;
extern NormObjKey normRuntimeTXTable;
extern int used_buffer_space;
//extern  int total_randomvar_entries;
//extern __thread int total_randomstring_entries;
//extern __thread int total_uniquevar_entries;
//extern __thread int total_datevar_entries;
extern int total_serverorder_entries;
extern int total_post_entries;
//extern __thread int total_http_method;
//extern __thread int total_jsonvar_entries;
//extern __thread int total_perpagejsonvar_entries;
//extern __thread int total_checkpoint_entries;
//extern __thread int total_perpagechkpt_entries;
//extern __thread int total_checkreplysize_entries;
//extern __thread int total_perpagechkrepsize_entries;
extern int g_static_vars_shr_mem_size;
//extern __thread int total_searchvar_entries;
//extern __thread int total_perpageservar_entries;
extern CheckPointTableEntry_Shr *checkpoint_table_shr_mem;
extern CheckReplySizeTableEntry_Shr *checkReplySizeTable_shr_mem;
#endif

typedef enum {
  SM_STOP = -2,
  SM_ERROR = -1,
  SM_SUCCESS
} SmStatus;

//extern EventDefinitionShr *event_def_shr_mem;
#ifndef CAV_MAIN
extern ClustValTableEntry* clustValTable;
extern GroupValTableEntry* groupValTable;
extern char* g_buf_ptr;
extern char* g_temp_ptr;
extern ReqDynVarTableEntry_Shr *reqdynvar_table_shr_mem;
extern ReqCookTableEntry_Shr *reqcook_table_shr_mem;
extern int debug_trace_log_value;
extern InuseSvrTableEntry* inuseSvrTable;
extern InuseUserTableEntry* inuseUserTable;
//static http_method_t     *http_method_table = NULL;
extern PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem;
extern PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem;
extern int * my_runprof_table;
extern PerProcVgroupTable * my_vgroup_table;
extern PerProcVgroupTable *g_static_vars_shr_mem;
extern int *seq_group_next;
#else
extern __thread ClustValTableEntry* clustValTable;
extern __thread GroupValTableEntry* groupValTable;
extern __thread char* g_buf_ptr;
extern __thread char* g_temp_ptr;
extern __thread ReqDynVarTableEntry_Shr *reqdynvar_table_shr_mem;
extern __thread ReqCookTableEntry_Shr *reqcook_table_shr_mem;
extern __thread int debug_trace_log_value;
extern __thread InuseSvrTableEntry* inuseSvrTable;
extern __thread InuseUserTableEntry* inuseUserTable;
//static __thread http_method_t     *http_method_table = NULL;
// This is required
extern __thread PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem;
extern __thread PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem;
extern __thread int * my_runprof_table;
extern __thread PerProcVgroupTable * my_vgroup_table;
extern __thread PerProcVgroupTable *g_static_vars_shr_mem;
extern __thread int *seq_group_next;
#endif



typedef struct SMMonSessionInfo
{ 
  int status;
  
  char mon_id[SM_MON_ID_MAX_SIZE + 1];
  char mon_name[SM_MON_ID_MAX_SIZE + 1];
  int mon_type;
  int mon_index;
  char tier_name[SM_MON_ID_MAX_SIZE + 1];
  char srv_name[SM_MON_ID_MAX_SIZE + 1];
  /* Below all are for session and scenario settings */
  char* big_buf_shr_mem;
  PointerTableEntry_Shr* pointer_table_shr_mem;
  WeightTableEntry* weight_table_shr_mem;
  GroupTableEntry_Shr* group_table_shr_mem;
  VarTableEntry_Shr* variable_table_shr_mem;
  VarTableEntry_Shr* index_variable_table_shr_mem;
  RepeatBlock_Shr* repeat_block_shr_mem;
  RandomVarTableEntry_Shr *randomvar_table_shr_mem;
  RandomStringTableEntry_Shr *randomstring_table_shr_mem;
  UniqueVarTableEntry_Shr *uniquevar_table_shr_mem;
  DateVarTableEntry_Shr *datevar_table_shr_mem;
  SvrTableEntry_Shr* gserver_table_shr_mem;
  SegTableEntry_Shr *seg_table_shr_mem;
  ServerOrderTableEntry_Shr* serverorder_table_shr_mem;
  StrEnt_Shr* post_table_shr_mem;
  ReqCookTableEntry_Shr *reqcook_table_shr_mem;
  ReqDynVarTableEntry_Shr *reqdynvar_table_shr_mem;
  ClickActionTableEntry_Shr *clickaction_table_shr_mem;
  action_request_Shr* request_table_shr_mem;
  HostTableEntry_Shr* host_table_shr_mem;
  ThinkProfTableEntry_Shr *thinkprof_table_shr_mem;
  InlineDelayTableEntry_Shr *inline_delay_table_shr_mem;
  AutoFetchTableEntry_Shr *autofetch_table_shr_mem;
  PacingTableEntry_Shr *pacing_table_shr_mem;
  ContinueOnPageErrorTableEntry_Shr *continueOnPageErrorTable_shr_mem;
  PerPageChkPtTableEntry_Shr *perpagechkpt_table_shr_mem;
  PageTableEntry_Shr* page_table_shr_mem;
  SessTableEntry_Shr* session_table_shr_mem;
  LocAttrTableEntry_Shr *locattr_table_shr_mem;
  AccAttrTableEntry_Shr *accattr_table_shr_mem;
  BrowAttrTableEntry_Shr *browattr_table_shr_mem;
  ScreenSizeAttrTableEntry_Shr *scszattr_table_share_mem;
  SessProfTableEntry_Shr *sessprof_table_shr_mem;
  SessProfIndexTableEntry_Shr *sessprofindex_table_shr_mem;
  RunProfTableEntry_Shr *runprof_table_shr_mem;
  ProxyServerTable_Shr *proxySvr_table_shr_mem;
  ProxyExceptionTable_Shr *proxyExcp_table_shr_mem;
  MetricTableEntry_Shr *metric_table_shr_mem;
  InuseSvrTableEntry_Shr *inusesvr_table_shr_mem;
  ErrorCodeTableEntry_Shr *errorcode_table_shr_mem;
  UserProfIndexTableEntry_Shr *userprofindex_table_shr_mem;
  RunProfIndexTableEntry_Shr *runprofindex_table_shr_mem; 
  TestCaseType_Shr *testcase_shr_mem;
  TxTableEntry_Shr* tx_table_shr_mem;
  http_method_t_shr *http_method_table_shr_mem;
  PatternTable_Shr *pattern_table_shr;

  GroupSettings *group_default_settings;
  Global_data *global_settings;
  PerProcVgroupTable *per_proc_vgroup_table;
  PointerTableEntry_Shr* fparamValueTable_shr_mem;
  NslVarTableEntry_Shr* nsl_var_table_shr_mem;

  s_child_ports* v_port_entry;
  s_child_ports* v_port_table;
  avgtime *average_time;
  
  int g_cur_server;
  int user_svr_table_size;
  int user_group_table_size;
  int user_cookie_table_size;
  int user_dynamic_vars_table_size;
  int user_var_table_size;
  int user_order_table_size;

  SSL_CTX **g_ssl_ctx;

  // Required for avgtime
 
  int g_avgtime_size;
  int g_cache_avgtime_idx;
  int g_proxy_avgtime_idx;
  int g_network_cache_stats_avgtime_idx;
  int dns_lookup_stats_avgtime_idx;
  int g_ftp_avgtime_idx;
  int g_ldap_avgtime_idx;
  int g_imap_avgtime_idx;
  int g_jrmi_avgtime_idx;
  int g_ws_avgtime_idx;
  int g_xmpp_avgtime_idx;
  int g_fc2_avgtime_idx;
  //int g_jmeter_avgtime_idx;
  int g_tcp_client_avg_idx;
  int g_udp_client_avg_idx;
  int g_avg_size_only_grp;
  int g_avg_um_data_idx;
  unsigned int group_data_gp_idx;
  unsigned int rbu_page_stat_data_gp_idx;
  unsigned int page_based_stat_gp_idx;
  int g_cavtest_http_avg_idx;
  int g_cavtest_web_avg_idx;
//  unsigned int (*tx_hash_func)(const char*, unsigned int); - TBD
  unsigned int show_vuser_flow_idx;
  int g_static_avgtime_size;
  int g_udp_client_failures_avg_idx;
  NormObjKey g_udp_client_errs_normtbl;
  int g_total_udp_client_errs;
  int g_tcp_client_failures_avg_idx;
  NormObjKey g_tcp_client_errs_normtbl;
  int g_total_tcp_client_errs;
  int http_resp_code_avgtime_idx;
  int total_http_resp_code_entries;
  HTTP_Status_Code_loc2norm_table *g_http_status_code_loc2norm_table;
  int total_tx_entries;
  TxDataSample *txData;
  int g_trans_avgtime_idx;
  TxLoc2NormTable *g_tx_loc2norm_table;
  NormObjKey rbu_domian_normtbl;
  int rbu_domain_stat_avg_idx;
  Rbu_domain_loc2norm_table *g_domain_loc2norm_table;
  NormObjKey normRuntimeTXTable ;

  UM_info *um_info;
  CacheAvgTime *cache_avgtime;
  ProxyAvgTime *proxy_avgtime;
  NetworkCacheStatsAvgTime *network_cache_stats_avgtime;
  DnsLookupStatsAvgTime *dns_lookup_stats_avgtime;
  FTPAvgTime *ftp_avgtime;
  LDAPAvgTime *ldap_avgtime;
  IMAPAvgTime *imap_avgtime;
  JRMIAvgTime *jrmi_avgtime;
  XMPPAvgTime *xmpp_avgtime;
  Rbu_domain_stat_avgtime *rbu_domain_stat_avg;
  HTTPRespCodeAvgTime *http_resp_code_avgtime;
  TCPClientFailureAvgTime *g_tcp_client_failures_avg;
  UDPClientFailureAvgTime *g_udp_client_failures_avg;
  NSDiagAvgTime *ns_diag_avgtime;
  SocketClientAvgTime *g_tcp_client_avg;
  SocketClientAvgTime *g_udp_client_avg;
  GROUPAvgTime *grp_avgtime;
  RBUPageStatAvgTime *rbu_page_stat_avg;
  PageStatAvgTime *page_stat_avgtime;
  VUserFlowAvgTime *vuser_flow_avgtime;
  CavTestHTTPAvgTime *cavtest_http_avg;
  CavTestWebAvgTime *cavtest_web_avg; 
  //int test_run;
  //long part_id;
  //jmeter_avgtime *g_jmeter_avgtime;
  int num_users;
  unsigned int sess_inst_num;
 
 // Data for file uploading
  char *cm_nvm_scratch_buf;
  int cm_nvm_scratch_buf_size;
  int amt_written;
  int total_runprof_entries;

  // For Managing Vuser List
  VUser* gBusyVuserHead;
  VUser* gBusyVuserTail;
  VUser* gFreeVuserHead;
  int gFreeVuserCnt; 
  int gFreeVuserMinCnt;
  int * my_runprof_table;
  PerProcVgroupTable * my_vgroup_table;
  UniqueRangeVarPerProcessTable *unique_range_var_table;
  PerHostSvrTableEntry_Shr* actsvr_table_shr_mem;
  unique_group_table_type* unique_group_table;
  int num_dyn_host_left;
  int is_static_host_shm_created;
  int num_dyn_host_add;
  SearchVarTableEntry_Shr *searchvar_table_shr_mem;
  int *seq_group_next;
  int used_buffer_space;
  int total_pointer_entries;
  int total_weight_entries;
  int total_group_entries;
  int total_var_entries;
  int total_index_var_entries;
  int total_repeat_block_entries;
  int total_nsvar_entries;
  int total_randomvar_entries;
  int total_randomstring_entries;
  int total_uniquevar_entries;
  int total_datevar_entries;
  int total_svr_entries;
  int total_seg_entries;
  int total_serverorder_entries;
  int total_post_entries;
  int total_reqcook_entries;
  int total_reqdynvar_entries;
  int total_clickaction_entries;
  int total_request_entries;
  int total_http_method;
  int total_host_entries;
  int total_jsonvar_entries;
  int total_perpagejsonvar_entries;
  int total_checkpoint_entries;
  int total_perpagechkpt_entries;
  int total_checkreplysize_entries;
  int total_perpagechkrepsize_entries;
  int total_page_entries;
  int total_sess_entries;
  int total_locattr_entries;
  int total_sessprof_entries;
  int total_inusesvr_entries;
  int total_errorcode_entries;
  int total_clustvar_entries;
  int total_clust_entries;
  int total_groupvar_entries;
  int total_userindex_entries;
  int total_userprofshr_entries;
  long g_static_vars_shr_mem_size;
  int total_fparam_entries;
  int total_searchvar_entries;
  int total_perpageservar_entries;
  int unique_group_id;
}SMMonSessionInfo;


/*****************************************************************************/


/********************  Functions Decalartion *********************************/
extern int  sm_init_thread_manager(int debug_fd);
extern int  sm_run_command(void *args);
extern int  sm_run_command_in_thread(char *mon_id, int mon_type, char *mon_name, int mon_index, char *tier, char *srv);
//extern int sm_run_command_in_thread(CM_MON_REQ *rcv_msg, NSLBMap * nslb_map_ptr);
extern void sm_init_all_mon_global_mem_with_defaults();
extern void sm_init_all_mon_shared_mem_with_defaults();
extern void sm_init_all_mon_global_vars_with_defaults();
extern int  sm_parse_script_scenario_for_mon();
extern void sm_map_shared_mem_with_structs();
extern void sm_override_mon_global_settings();
extern int  sm_mon_init_after_parsing_args();
extern void sm_initialize_req_vars_for_exe();
extern void sm_send_msg_thread_to_nvm();
extern int sm_make_connection_thread_child(void *args);
extern void sm_free_monitor_data(SMMonSessionInfo *sm_mon_info_var);
extern void sm_free_shared_memory_structs();
extern void  sm_stop_monitor(int opcode, SMMonSessionInfo *tmsg);
extern void sm_process_monitor_thread_msg(SMMonSessionInfo *tmsg);
/*****************************************************************************/

#endif
