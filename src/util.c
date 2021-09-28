/************************************************************
		
* Utility functions
* parser of http_load's configuration file
*
* HISTORY
* 8/28/01	get_report_file_extn added
* 		get_ms_stamp modified to run for extended time
* 8/29/01	SSL_PCT added
* 05/01/3	deliver_report() added here to merge the
* 		changes of netstorm.c & storm_coordinator.c
**************************************************************/

/*************************************************************
Notes:
NS Variables:
Each static var declaration add an entry into groupvars table.
*************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
//#include <linux/cavmodem.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>
#include <libgen.h>
#include <sys/stat.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_json_vars.h"  //For JSON Var
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_tag_vars.h"
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
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "nslb_cav_conf.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
//#include "ns_handle_read.h"
#include "ns_goal_based_sla.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_monitor_profiles.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_wan_env.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_debug_trace.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_child_msg_com.h"
#include "ns_page.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_unique_range_var.h"
#include "ns_index_vars.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_error_codes.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_event_log.h"
#include "ns_schedule_phases_parse.h"
#include "ns_http_cache.h"
#include "ns_http_script_parse.h"
#include "ns_auto_fetch_parse.h"
#include "ns_url_hash.h"
#include "ns_session_pacing.h" 
#include "ns_click_script.h" 
#include "ns_page_think_time_parse.h"

#include "ns_user_profile.h"
#include "ns_proxy_server.h"
#include "ns_websocket.h"

//#include "nslb_uservar_table.h"
#include "nslb_search_vars.h" 
#include "ns_sync_point.h"
#include "ns_monitoring.h"
#include "ns_string.h"
#include "ns_continue_on_page.h"
#include "nslb_big_buf.h"
#include "ns_inline_delay.h"
#include "ns_static_vars_rtc.h"
#include "ns_replay_db_query.h"
#include "ns_exit.h"
#include "ns_sockjs.h"
#include "ns_static_files.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_handle_alert.h"
#include "ns_socket.h"
#include "ns_rdp_api.h" /*bug 79149*/
#include "ns_param_override.h"

//extern int g_follow_redirects;

//extern int ns_weibthink_calc(int mean, int median, int variance, double* a, double* b);

#ifndef CAV_MAIN
GroupSettings *group_default_settings = NULL;
int g_avg_um_data_idx; /* This marks where UM_data array starts in average_time */
int g_cavg_um_data_idx; /* This marks where UM_data array starts in cum average_time */
int g_avgtime_size;
int g_static_avgtime_size;
int g_avg_size_only_grp;
int g_cavgtime_size;
int g_cavg_size_only_grp;
short* large_location_array;
int total_vendor_locations;
unsigned int (*page_hash_func)(const char*, unsigned int);
#else
__thread GroupSettings *group_default_settings = NULL;
__thread int g_avg_um_data_idx; /* This marks where UM_data array starts in average_time */
__thread int g_cavg_um_data_idx; /* This marks where UM_data array starts in cum average_time */
__thread int g_avgtime_size;
__thread int g_static_avgtime_size;
__thread int g_avg_size_only_grp;
__thread int g_cavgtime_size;
__thread int g_cavg_size_only_grp;
__thread short* large_location_array;
__thread int total_vendor_locations;

__thread unsigned int (*page_hash_func)(const char*, unsigned int);
#endif

FILE *console_fp = NULL;  //to replace NULL with stdout when netstorm is running as Client or from gui.
int num_processor = 0;

// Added May/29/2007 For response hit parcentile data

FILE *resp_hits_fp=NULL;

AddHeaderTableEntry* addHeaderTable = NULL;
int total_hdr_entries;
int max_hdr_entries;

//User_data *users;
#ifndef CAV_MAIN
extern Global_data *global_settings;
IP_data *ips;
Client_data *clients;
SessTableEntry *gSessionTable;
PageTableEntry *gPageTable;
action_request* requests;
CheckPageTableEntry* checkPageTable;
CheckReplySizePageTableEntry* checkReplySizePageTable;
SvrTableEntry *gServerTable;
ReqCookTableEntry* reqCookTable;
SegTableEntry* segTable;
PointerTableEntry *pointerTable;
StrEnt* postTable;
VarTableEntry *varTable;
GroupTableEntry *groupTable;
RepeatBlock *repeatBlock;
PointerTableEntry *fparamValueTable;
WeightTableEntry *weightTable;
LocAttrTableEntry *locAttrTable;
AccLocTableEntry *accLocTable;
ScreenSizeAttrTableEntry *scSzeAttrTable;
PfBwScSzTableEntry *pfBwScSzTable;
LineCharTableEntry *lineCharTable;
AccAttrTableEntry *accAttrTable;
BRScSzMapTableEntry *brScSzTable;
BrowAttrTableEntry *browAttrTable;
SessProfTableEntry *sessProfTable;
SessProfIndexTableEntry *sessProfIndexTable;
UserProfTableEntry *userProfTable;
UserIndexTableEntry *userIndexTable;
RunProfTableEntry *runProfTable;
RunIndexTableEntry *runIndexTable;
ServerOrderTableEntry *serverOrderTable;
TestCaseType testCase;
MetricTableEntry *metricTable;
HostElement* hostTable;
avgtime* reportTable;
DynVarTableEntry* dynVarTable;
ReqDynVarTableEntry* reqDynVarTable;
InuseSvrTableEntry* inuseSvrTable; //in ns_user_profile.c
InuseUserTableEntry* inuseUserTable;//in ns_user_profile.c
ThinkProfTableEntry* thinkProfTable;
InlineDelayTableEntry* inlineDelayTable;
ContinueOnPageErrorTableEntry* continueOnPageErrorTable;
OverrideRecordedThinkTime* overrideRecordedThinktimeTable;
AutoFetchTableEntry* autofetchTable; // for auto fetch embedded
ClickActionTableEntry* clickActionTable; // for click script user actions
PacingTableEntry* pacingTable;
#ifdef RMI_MODE
ByteVarTableEntry* byteVarTable;
ReqByteVarTableEntry* reqByteVarTable;
#endif
ErrorCodeTableEntry* errorCodeTable;
NsVarTableEntry* nsVarTable;
NsVarTableEntry* grpNsVarTable;
TagTableEntry* tagTable;
AttrQualTableEntry* attrQualTable;
TagPageTableEntry* tagPageTable;
AttrQualTableEntry* attrQualTable;
TagPageTableEntry* tagPageTable;
SearchVarTableEntry* searchVarTable;
SearchPageTableEntry* searchPageTable;
UniqueRangeVarTableEntry *uniquerangevarTable; // For Unique range var
JSONVarTableEntry* jsonVarTable;  //For JSON Var
JSONPageTableEntry* jsonPageTable;  //For JSON Var
RandomVarTableEntry* randomVarTable;
UniqueVarTableEntry* uniqueVarTable;
RandomStringTableEntry* randomStringTable;
DateVarTableEntry* dateVarTable;
PerPageSerVarTableEntry* perPageSerVarTable;
PerPageJSONVarTableEntry* perPageJSONVarTable;  //For JSON Var
PerPageChkPtTableEntry* perPageChkPtTable;
PerPageCheckReplySizeTableEntry* perPageChkRepSizeTable;
ClustVarTableEntry* clustVarTable;
ClustValTableEntry* clustValTable;
ClustTableEntry* clustTable;
GroupVarTableEntry* groupVarTable;
GroupValTableEntry* groupValTable;
ProxyServerTable *proxySvrTable;
ProxyExceptionTable *proxyExcpTable;
ProxyNetPrefix *proxyNetPrefixId;
status_codes status_code[STATUS_CODE_ARRAY_SIZE];
//ClickawayProfTableEntry* clickawayProfTable;
char* g_big_buf;
char* g_buf_ptr;

//---- Temp Buffer -----------
char* g_temp_buf;
char* g_temp_ptr;
int used_tempbuffer_space;
int max_tempbuffer_space;
//-----------------------------
#else
extern __thread  Global_data *global_settings;
__thread IP_data *ips;
__thread Client_data *clients;
__thread SessTableEntry *gSessionTable;
__thread PageTableEntry *gPageTable;
__thread action_request* requests;
__thread CheckReplySizePageTableEntry* checkReplySizePageTable;
__thread CheckPageTableEntry* checkPageTable;
__thread SvrTableEntry *gServerTable;
__thread ReqCookTableEntry* reqCookTable;
__thread SegTableEntry* segTable;
__thread PointerTableEntry *pointerTable;
__thread StrEnt* postTable;
__thread VarTableEntry *varTable;
__thread GroupTableEntry *groupTable;
__thread RepeatBlock *repeatBlock;
__thread PointerTableEntry *fparamValueTable;
__thread WeightTableEntry *weightTable;
__thread LocAttrTableEntry *locAttrTable;
__thread AccLocTableEntry *accLocTable;
__thread ScreenSizeAttrTableEntry *scSzeAttrTable;
__thread PfBwScSzTableEntry *pfBwScSzTable;
__thread LineCharTableEntry *lineCharTable;
__thread AccAttrTableEntry *accAttrTable;
__thread BRScSzMapTableEntry *brScSzTable;
__thread BrowAttrTableEntry *browAttrTable;
__thread SessProfTableEntry *sessProfTable;
__thread SessProfIndexTableEntry *sessProfIndexTable;
__thread UserProfTableEntry *userProfTable;
__thread UserIndexTableEntry *userIndexTable;
__thread RunProfTableEntry *runProfTable;
__thread RunIndexTableEntry *runIndexTable;
__thread ServerOrderTableEntry *serverOrderTable;
__thread TestCaseType testCase;
__thread MetricTableEntry *metricTable;
__thread HostElement* hostTable;
__thread avgtime* reportTable;
__thread DynVarTableEntry* dynVarTable;
__thread ReqDynVarTableEntry* reqDynVarTable;
__thread InuseSvrTableEntry* inuseSvrTable; //in ns_user_profile.c
__thread InuseUserTableEntry* inuseUserTable;//in ns_user_profile.c
__thread ThinkProfTableEntry* thinkProfTable;
__thread InlineDelayTableEntry* inlineDelayTable;
__thread ContinueOnPageErrorTableEntry* continueOnPageErrorTable;
__thread OverrideRecordedThinkTime* overrideRecordedThinktimeTable;
__thread AutoFetchTableEntry* autofetchTable; // for auto fetch embedded
__thread ClickActionTableEntry* clickActionTable; // for click script user actions
__thread PacingTableEntry* pacingTable;
#ifdef RMI_MODE
__thread ByteVarTableEntry* byteVarTable;
__thread ReqByteVarTableEntry* reqByteVarTable;
#endif
__thread ErrorCodeTableEntry* errorCodeTable;
__thread NsVarTableEntry* nsVarTable;
__thread NsVarTableEntry* grpNsVarTable;
__thread TagTableEntry* tagTable;
__thread AttrQualTableEntry* attrQualTable;
__thread TagPageTableEntry* tagPageTable;
__thread AttrQualTableEntry* attrQualTable;
__thread TagPageTableEntry* tagPageTable;
__thread SearchVarTableEntry* searchVarTable;
__thread SearchPageTableEntry* searchPageTable;
__thread UniqueRangeVarTableEntry *uniquerangevarTable; // For Unique range var
__thread JSONVarTableEntry* jsonVarTable;  //For JSON Var
__thread JSONPageTableEntry* jsonPageTable;  //For JSON Var
__thread RandomVarTableEntry* randomVarTable;
__thread UniqueVarTableEntry* uniqueVarTable;
__thread RandomStringTableEntry* randomStringTable;
__thread DateVarTableEntry* dateVarTable;
__thread PerPageSerVarTableEntry* perPageSerVarTable;
__thread PerPageJSONVarTableEntry* perPageJSONVarTable;  //For JSON Var
__thread PerPageChkPtTableEntry* perPageChkPtTable;
__thread PerPageCheckReplySizeTableEntry* perPageChkRepSizeTable;
__thread ClustVarTableEntry* clustVarTable;
__thread ClustValTableEntry* clustValTable;
__thread ClustTableEntry* clustTable;
__thread GroupVarTableEntry* groupVarTable;
__thread GroupValTableEntry* groupValTable;
__thread ProxyServerTable *proxySvrTable;
__thread ProxyExceptionTable *proxyExcpTable;
__thread ProxyNetPrefix *proxyNetPrefixId;
__thread status_codes status_code[STATUS_CODE_ARRAY_SIZE];
//ClickawayProfTableEntry* clickawayProfTable;
__thread char* g_big_buf;
__thread char* g_buf_ptr;

//---- Temp Buffer -----------
__thread char* g_temp_buf;
__thread char* g_temp_ptr;
__thread int used_tempbuffer_space;
__thread int max_tempbuffer_space;
//-----------------------------
#endif

#ifndef CAV_MAIN
CacheTable_t **g_master_cache_tbl_ptr;

DB* var_hash_table;

char *url_errorcode_table[TOTAL_URL_ERR];
char g_url_file[MAX_FILE_NAME];
char g_conf_file[MAX_SCENARIO_LEN];
char g_sorted_conf_file[MAX_SCENARIO_LEN];
char g_var_file[MAX_FILE_NAME];
char g_c_file[MAX_FILE_NAME];
char g_testrun[MAX_SCENARIO_LEN];
char g_groupvar_filename[MAX_FILE_NAME];
char g_tmp_fname[MAX_FILE_NAME];
char g_ns_tmpdir[MAX_FILE_NAME];
char g_ns_login_user[MAX_FILE_NAME];

char g_project_name[MAX_FILE_NAME];    // this is add to implement for project dir
char g_subproject_name[MAX_FILE_NAME]; // this is add to implement for sub project dir
char g_scenario_name[MAX_SCENARIO_LEN];   // this is add to implement scenario name
char g_proj_subproj_name[MAX_FILE_NAME];

int max_user_entries;
int total_user_entries;
int cur_ip_entry;
int max_ip_entries;
int total_ip_entries;
int total_group_ip_entries;
int max_client_entries;
int total_client_entries;
int cur_server_entry;
int max_sess_entries;
int total_sess_entries;

//int max_page_entries;
//int total_page_entries; // Total number of pages of all used scripts (in case of two grp using same script, it will be counted once)
int g_actual_num_pages;   //Actual number of pages (in case of two group using same script, it will count for both group)

int g_rbu_num_pages;   //Number of pages Used in RBU

int max_svr_entries;
int total_svr_entries;
int max_var_entries;
int total_var_entries;
int total_index_var_entries;
int max_group_entries;
int total_group_entries;
int max_fparam_entries;
int total_fparam_entries;
int max_weight_entries;
int total_weight_entries;
int max_locattr_entries;
int total_locattr_entries;
int max_linechar_entries;
int total_linechar_entries;
int max_accattr_entries;
int total_accattr_entries;
int max_br_sc_sz_entries;
int total_br_sc_sz_map_entries;
int max_accloc_entries;
int total_accloc_entries;
int max_browattr_entries;
int total_browattr_entries;
int max_screen_size_entries;
int total_screen_size_entries;
int max_pf_bw_screen_size_entries;
int total_pf_bw_screen_size_entries;
int max_machattr_entries;
int total_machattr_entries;
int max_freqattr_entries;
int total_freqattr_entries;
int max_sessprof_entries;
int total_sessprof_entries;
int max_sessprofindex_entries;
int total_sessprofindex_entries;
int max_userprof_entries;
int total_userprof_entries;
int max_userindex_entries;
int total_userindex_entries;
int max_runprof_entries;
int total_runprof_entries;
int max_runindex_entries;
int total_runindex_entries;
int max_serverorder_entries;
int total_serverorder_entries;
int max_metric_entries;
int total_metric_entries;
int max_request_entries;
int total_request_entries;
int max_checkpage_entries;
int total_checkpage_entries;
int max_check_replysize_page_entries; //can be moved to ns_check_reply_size.c
int total_check_replysize_page_entries; //can be moved to ns_check_reply_size.c
int max_pointer_entries;
int total_pointer_entries;
int max_seg_entries;
int total_seg_entries;
int max_post_entries;
int total_post_entries;
/* It has only SMTP requests used as we need not to load smtp gdf group
 * if no smtp request exists, smpt gdf group is ignored in getGroupNumVectorByID
 */
int total_smtp_request_entries;
int total_pop3_request_entries;
int total_ftp_request_entries;
int total_dns_request_entries;
int total_imap_request_entries;
int total_jrmi_request_entries;
int total_ws_request_entries;
int total_xmpp_request_entries;
int total_fc2_request_entries;
int max_host_entries;
int total_host_entries;
int max_report_entries;
int total_report_entries;
int max_dynvar_entries;
int total_dynvar_entries;
int max_reqdynvar_entries;
int total_reqdynvar_entries;
int max_inusesvr_entries;
int total_inusesvr_entries;
int max_inuseuser_entries;
int total_inuseuser_entries;
int max_thinkprof_entries;
int total_thinkprof_entries;
int max_inline_delay_entries;
int total_inline_delay_entries;
int max_pacing_entries;
int total_pacing_entries;
int max_autofetch_entries; // maximum entries for auto fetch embedded
int max_cont_on_err_entries; // maximum entries for auto fetch embedded
int max_recorded_think_time_entries; // maximum entries for override recorded think time;
int total_autofetch_entries; // total entries for auto fetch embedded
int total_cont_on_err_entries; // total entries for continue on err
int total_recorded_think_time_entries;// total entries for override recorded think time
int total_socket_request_entries;
#ifdef RMI_MODE
int max_bytevar_entries;
int total_bytevar_entries;
int max_reqbytevar_entries;
int total_reqbytevar_entries;
#endif
int max_errorcode_entries;
int max_nsvar_entries;
int total_nsvar_entries;
int max_tag_entries;
int total_tag_entries;
int max_tagpage_entries;
int total_tagpage_entries;
int max_attrqual_entries;
int total_attrqual_entries;
int max_perpageservar_entries;
int max_perpagejsonvar_entries; //Max JSON Vars per page
int max_clustvar_entries;
int total_clustvar_entries;
int max_clustval_entries;
int total_clustval_entries;
int max_clust_entries;
int total_clust_entries;
int max_groupvar_entries;
int total_groupvar_entries;
int max_groupval_entries;
int max_repeat_block_entries;
int total_repeat_block_entries;
int total_groupval_entries;
int max_clickaction_entries;    // for Click Script Actions
int total_clickaction_entries;  // for Click Script Actions
int total_proxy_svr_entries;
int total_proxy_excp_entries;
int total_proxy_ip_interfaces;
int max_proxy_svr_entries;
int max_proxy_excp_entries;
int max_proxy_ip_interfaces;
ns_bigbuf_t max_buffer_space;
ns_bigbuf_t used_buffer_space;


int max_rbu_domain_entries;           //max domain entry for RBU Domain Stat
int total_rbu_domain_entries;         //total domain entry for RBU Domain Stat

//initialisation at init_http_response_codes()
int max_http_resp_code_entries;           //max entry for Status Code
int total_http_resp_code_entries;         //total entry for Status Code

int default_userprof_idx;

int config_file_server_base = -1;
int config_file_server_idx = -1;

int unique_group_id = 0;

int max_ssl_cert_key_entries;           //max ssl cert key entries 
int total_ssl_cert_key_entries;         //total ssl cert key entries
int total_add_rec_host_entries;         //ADD_RECORDED_HOST

int total_active_runprof_entries;      //on group schedule, number of grps with qty > 0

#else

__thread CacheTable_t **g_master_cache_tbl_ptr;

__thread DB* var_hash_table;

__thread char *url_errorcode_table[TOTAL_URL_ERR];
__thread char g_url_file[MAX_FILE_NAME];
__thread char g_conf_file[MAX_SCENARIO_LEN];
__thread char g_sorted_conf_file[MAX_SCENARIO_LEN];
__thread char g_var_file[MAX_FILE_NAME];
__thread char g_c_file[MAX_FILE_NAME];
__thread char g_testrun[MAX_SCENARIO_LEN];
__thread char g_groupvar_filename[MAX_FILE_NAME];
__thread char g_tmp_fname[MAX_FILE_NAME];
__thread char g_ns_tmpdir[MAX_FILE_NAME];
__thread char g_ns_login_user[MAX_FILE_NAME];

__thread char g_project_name[MAX_FILE_NAME];    // this is add to implement for project dir
__thread char g_subproject_name[MAX_FILE_NAME]; // this is add to implement for sub project dir
__thread char g_scenario_name[MAX_SCENARIO_LEN];   // this is add to implement scenario name
__thread char g_proj_subproj_name[MAX_FILE_NAME];

__thread int max_user_entries;
__thread int total_user_entries;
__thread int cur_ip_entry;
__thread int max_ip_entries;
__thread int total_ip_entries;
__thread int total_group_ip_entries;
__thread int max_client_entries;
__thread int total_client_entries;
__thread int cur_server_entry;
__thread int max_sess_entries;
__thread int total_sess_entries;

//__thread int max_page_entries;
//__thread int total_page_entries; // Total number of pages of all used scripts (in case of two grp using same script, it will be counted once)
__thread int g_actual_num_pages;   //Actual number of pages (in case of two group using same script, it will count for both group)

__thread int g_rbu_num_pages;   //Number of pages Used in RBU

__thread int max_svr_entries;
__thread int total_svr_entries;
__thread int max_var_entries;
__thread int total_var_entries;
__thread int total_index_var_entries;
__thread int max_group_entries;
__thread int total_group_entries;
__thread int max_fparam_entries;
__thread int total_fparam_entries;
__thread int max_weight_entries;
__thread int total_weight_entries;
__thread int max_locattr_entries;
__thread int total_locattr_entries;
__thread int max_linechar_entries;
__thread int total_linechar_entries;
__thread int max_accattr_entries;
__thread int total_accattr_entries;
__thread int max_br_sc_sz_entries;
__thread int total_br_sc_sz_map_entries;
__thread int max_accloc_entries;
__thread int total_accloc_entries;
__thread int max_browattr_entries;
__thread int total_browattr_entries;
__thread int max_screen_size_entries;
__thread int total_screen_size_entries;
__thread int max_pf_bw_screen_size_entries;
__thread int total_pf_bw_screen_size_entries;
__thread int max_machattr_entries;
__thread int total_machattr_entries;
__thread int max_freqattr_entries;
__thread int total_freqattr_entries;
__thread int max_sessprof_entries;
__thread int total_sessprof_entries;
__thread int max_sessprofindex_entries;
__thread int total_sessprofindex_entries;
__thread int max_userprof_entries;
__thread int total_userprof_entries;
__thread int max_userindex_entries;
__thread int total_userindex_entries;
__thread int max_runprof_entries;
__thread int total_runprof_entries;
__thread int max_runindex_entries;
__thread int total_runindex_entries;
__thread int max_serverorder_entries;
__thread int total_serverorder_entries;
__thread int max_metric_entries;
__thread int total_metric_entries;
__thread int max_request_entries;
__thread int total_request_entries;
__thread int max_checkpage_entries;
__thread int total_checkpage_entries;
__thread int max_check_replysize_page_entries; //can be moved to ns_check_reply_size.c
__thread int total_check_replysize_page_entries; //can be moved to ns_check_reply_size.c
__thread int max_pointer_entries;
__thread int total_pointer_entries;
__thread int max_seg_entries;
__thread int total_seg_entries;
__thread int max_post_entries;
__thread int total_post_entries;
/* It has only SMTP requests used as we need not to load smtp gdf group
 * if no smtp request exists, smpt gdf group is ignored in getGroupNumVectorByID
 */
__thread int total_smtp_request_entries;
__thread int total_pop3_request_entries;
__thread int total_ftp_request_entries;
__thread int total_dns_request_entries;
__thread int total_imap_request_entries;
__thread int total_jrmi_request_entries;
__thread int total_ws_request_entries;
__thread int total_xmpp_request_entries;
__thread int total_fc2_request_entries;
__thread int max_host_entries;
__thread int total_host_entries;
__thread int max_report_entries;
__thread int total_report_entries;
__thread int max_dynvar_entries;
__thread int total_dynvar_entries;
__thread int max_reqdynvar_entries;
__thread int total_reqdynvar_entries;
__thread int max_inusesvr_entries;
__thread int total_inusesvr_entries;
__thread int max_inuseuser_entries;
__thread int total_inuseuser_entries;
__thread int max_thinkprof_entries;
__thread int total_thinkprof_entries;
__thread int max_inline_delay_entries;
__thread int total_inline_delay_entries;
__thread int max_pacing_entries;
__thread int total_pacing_entries;
__thread int max_autofetch_entries; // maximum entries for auto fetch embedded
__thread int max_cont_on_err_entries; // maximum entries for auto fetch embedded
__thread int max_recorded_think_time_entries; // maximum entries for override recorded think time;
__thread int total_autofetch_entries; // total entries for auto fetch embedded
__thread int total_cont_on_err_entries; // total entries for continue on err
__thread int total_recorded_think_time_entries;// total entries for override recorded think time
__thread int total_socket_request_entries;
#ifdef RMI_MODE
__thread int max_bytevar_entries;
__thread int total_bytevar_entries;
__thread int max_reqbytevar_entries;
__thread int total_reqbytevar_entries;
#endif
__thread int max_errorcode_entries;
__thread int max_nsvar_entries;
__thread int total_nsvar_entries;
__thread int max_tag_entries;
__thread int total_tag_entries;
__thread int max_tagpage_entries;
__thread int total_tagpage_entries;
__thread int max_attrqual_entries;
__thread int total_attrqual_entries;
__thread int max_perpageservar_entries;
__thread int max_perpagejsonvar_entries; //Max JSON Vars per page
__thread int max_clustvar_entries;
__thread int total_clustvar_entries;
__thread int max_clustval_entries;
__thread int total_clustval_entries;
__thread int max_clust_entries;
__thread int total_clust_entries;
__thread int max_groupvar_entries;
__thread int total_groupvar_entries;
__thread int max_groupval_entries;
__thread int max_repeat_block_entries;
__thread int total_repeat_block_entries;
__thread int total_groupval_entries;
__thread int max_clickaction_entries;    // for Click Script Actions
__thread int total_clickaction_entries;  // for Click Script Actions
__thread int total_proxy_svr_entries;
__thread int total_proxy_excp_entries;
__thread int total_proxy_ip_interfaces;
__thread int max_proxy_svr_entries;
__thread int max_proxy_excp_entries;
__thread int max_proxy_ip_interfaces;
__thread ns_bigbuf_t max_buffer_space;
__thread ns_bigbuf_t used_buffer_space;


__thread int max_rbu_domain_entries;           //max domain entry for RBU Domain Stat
__thread int total_rbu_domain_entries;         //total domain entry for RBU Domain Stat

//initialisation at init_http_response_codes()
__thread int max_http_resp_code_entries;           //max entry for Status Code
__thread int total_http_resp_code_entries;         //total entry for Status Code

__thread int default_userprof_idx;

__thread int config_file_server_base = -1;
__thread int config_file_server_idx = -1;

__thread int unique_group_id = 0;

__thread int max_ssl_cert_key_entries;           //max ssl cert key entries 
__thread int total_ssl_cert_key_entries;         //total ssl cert key entries
__thread int total_add_rec_host_entries;         //ADD_RECORDED_HOST

__thread int total_active_runprof_entries;      //on group schedule, number of grps with qty > 0
#endif

int g_last_acked_sample = 0;
u_ns_ts_t g_tcpdump_started_time = 0;

#ifndef CAV_MAIN
char* big_buf_shr_mem = NULL;
PointerTableEntry_Shr* pointer_table_shr_mem = NULL;
WeightTableEntry* weight_table_shr_mem = NULL;
GroupTableEntry_Shr* group_table_shr_mem = NULL;
VarTableEntry_Shr* variable_table_shr_mem = NULL;
VarTableEntry_Shr* index_variable_table_shr_mem = NULL;
RepeatBlock_Shr* repeat_block_shr_mem = NULL;
RandomVarTableEntry_Shr *randomvar_table_shr_mem = NULL;
RandomStringTableEntry_Shr *randomstring_table_shr_mem = NULL;
UniqueVarTableEntry_Shr *uniquevar_table_shr_mem = NULL ;
DateVarTableEntry_Shr *datevar_table_shr_mem = NULL; 
SvrTableEntry_Shr* gserver_table_shr_mem = NULL;
SegTableEntry_Shr *seg_table_shr_mem = NULL;
ServerOrderTableEntry_Shr* serverorder_table_shr_mem = NULL;
StrEnt_Shr* post_table_shr_mem = NULL;
ReqCookTableEntry_Shr *reqcook_table_shr_mem = NULL;
ReqDynVarTableEntry_Shr *reqdynvar_table_shr_mem = NULL;
ClickActionTableEntry_Shr *clickaction_table_shr_mem = NULL;
action_request_Shr* request_table_shr_mem = NULL;
HostTableEntry_Shr* host_table_shr_mem = NULL;
ThinkProfTableEntry_Shr *thinkprof_table_shr_mem = NULL;
InlineDelayTableEntry_Shr *inline_delay_table_shr_mem = NULL;
AutoFetchTableEntry_Shr *autofetch_table_shr_mem = NULL;
PacingTableEntry_Shr *pacing_table_shr_mem = NULL;
ContinueOnPageErrorTableEntry_Shr *continueOnPageErrorTable_shr_mem = NULL;
PerPageSerVarTableEntry_Shr *perpageservar_table_shr_mem = NULL;
PerPageJSONVarTableEntry_Shr *perpagejsonvar_table_shr_mem = NULL; //JSON var 
PerPageChkPtTableEntry_Shr *perpagechkpt_table_shr_mem = NULL;
PerPageCheckReplySizeTableEntry_Shr *perpagechk_replysize_table_shr_mem= NULL;
PageTableEntry_Shr* page_table_shr_mem = NULL;
SessTableEntry_Shr* session_table_shr_mem = NULL;
LocAttrTableEntry_Shr *locattr_table_shr_mem = NULL;
AccAttrTableEntry_Shr *accattr_table_shr_mem = NULL;
BrowAttrTableEntry_Shr *browattr_table_shr_mem = NULL;
FreqAttrTableEntry_Shr *freqattr_table_shr_mem = NULL;
MachAttrTableEntry_Shr *machattr_table_shr_mem = NULL;
ScreenSizeAttrTableEntry_Shr *scszattr_table_share_mem = NULL; 
SessProfTableEntry_Shr *sessprof_table_shr_mem = NULL;
SessProfIndexTableEntry_Shr *sessprofindex_table_shr_mem = NULL;
RunProfTableEntry_Shr *runprof_table_shr_mem = NULL;
ProxyServerTable_Shr *proxySvr_table_shr_mem = NULL;
ProxyExceptionTable_Shr *proxyExcp_table_shr_mem = NULL;
ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem = NULL;
MetricTableEntry_Shr *metric_table_shr_mem = NULL;
InuseSvrTableEntry_Shr *inusesvr_table_shr_mem = NULL;
ErrorCodeTableEntry_Shr *errorcode_table_shr_mem = NULL;
UserProfTableEntry_Shr *userprof_table_shr_mem = NULL;
UserProfIndexTableEntry_Shr *userprofindex_table_shr_mem = NULL;
RunProfIndexTableEntry_Shr *runprofindex_table_shr_mem = NULL;
TestCaseType_Shr *testcase_shr_mem;
char* default_svr_location = NULL;
int max_svr_group_num = -1;
ProfilePctCountTable *prof_pct_count_table = NULL; 
PerHostSvrTableEntry_Shr *totsvr_table_shr_mem = NULL;
char *file_param_value_big_buf_shr_mem = NULL;
PointerTableEntry_Shr* fparamValueTable_shr_mem = NULL;
/*Manish: create static buffer for data file */
char *data_file_buf = NULL;
long malloced_sized = 0;
NslVarTableEntry_Shr* nsl_var_table_shr_mem = NULL;
#else
__thread char* big_buf_shr_mem = NULL;
__thread PointerTableEntry_Shr* pointer_table_shr_mem = NULL;
__thread WeightTableEntry* weight_table_shr_mem = NULL;
__thread GroupTableEntry_Shr* group_table_shr_mem = NULL;
__thread VarTableEntry_Shr* variable_table_shr_mem = NULL;
__thread VarTableEntry_Shr* index_variable_table_shr_mem = NULL;
__thread RepeatBlock_Shr* repeat_block_shr_mem = NULL;
__thread RandomVarTableEntry_Shr *randomvar_table_shr_mem = NULL;
__thread RandomStringTableEntry_Shr *randomstring_table_shr_mem = NULL;
__thread UniqueVarTableEntry_Shr *uniquevar_table_shr_mem = NULL ;
__thread DateVarTableEntry_Shr *datevar_table_shr_mem = NULL; 
__thread SvrTableEntry_Shr* gserver_table_shr_mem = NULL;
__thread SegTableEntry_Shr *seg_table_shr_mem = NULL;
__thread ServerOrderTableEntry_Shr* serverorder_table_shr_mem = NULL;
__thread StrEnt_Shr* post_table_shr_mem = NULL;
__thread ReqCookTableEntry_Shr *reqcook_table_shr_mem = NULL;
__thread ReqDynVarTableEntry_Shr *reqdynvar_table_shr_mem = NULL;
__thread ClickActionTableEntry_Shr *clickaction_table_shr_mem = NULL;
__thread action_request_Shr* request_table_shr_mem = NULL;
__thread HostTableEntry_Shr* host_table_shr_mem = NULL;
__thread ThinkProfTableEntry_Shr *thinkprof_table_shr_mem = NULL;
__thread InlineDelayTableEntry_Shr *inline_delay_table_shr_mem = NULL;
__thread AutoFetchTableEntry_Shr *autofetch_table_shr_mem = NULL;
__thread PacingTableEntry_Shr *pacing_table_shr_mem = NULL;
__thread ContinueOnPageErrorTableEntry_Shr *continueOnPageErrorTable_shr_mem = NULL;
__thread PerPageSerVarTableEntry_Shr *perpageservar_table_shr_mem = NULL;
__thread PerPageJSONVarTableEntry_Shr *perpagejsonvar_table_shr_mem = NULL; //JSON var 
__thread PerPageChkPtTableEntry_Shr *perpagechkpt_table_shr_mem = NULL;
__thread PerPageCheckReplySizeTableEntry_Shr *perpagechk_replysize_table_shr_mem= NULL;
__thread PageTableEntry_Shr* page_table_shr_mem = NULL;
__thread SessTableEntry_Shr* session_table_shr_mem = NULL;
__thread LocAttrTableEntry_Shr *locattr_table_shr_mem = NULL;
__thread AccAttrTableEntry_Shr *accattr_table_shr_mem = NULL;
__thread BrowAttrTableEntry_Shr *browattr_table_shr_mem = NULL;
__thread FreqAttrTableEntry_Shr *freqattr_table_shr_mem = NULL;
__thread MachAttrTableEntry_Shr *machattr_table_shr_mem = NULL;
__thread ScreenSizeAttrTableEntry_Shr *scszattr_table_share_mem = NULL; 
__thread SessProfTableEntry_Shr *sessprof_table_shr_mem = NULL;
__thread SessProfIndexTableEntry_Shr *sessprofindex_table_shr_mem = NULL;
__thread RunProfTableEntry_Shr *runprof_table_shr_mem = NULL;
__thread ProxyServerTable_Shr *proxySvr_table_shr_mem = NULL;
__thread ProxyExceptionTable_Shr *proxyExcp_table_shr_mem = NULL;
__thread ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem = NULL;
__thread MetricTableEntry_Shr *metric_table_shr_mem = NULL;
__thread InuseSvrTableEntry_Shr *inusesvr_table_shr_mem = NULL;
__thread ErrorCodeTableEntry_Shr *errorcode_table_shr_mem = NULL;
__thread UserProfTableEntry_Shr *userprof_table_shr_mem = NULL;
__thread UserProfIndexTableEntry_Shr *userprofindex_table_shr_mem = NULL;
__thread RunProfIndexTableEntry_Shr *runprofindex_table_shr_mem = NULL;
__thread TestCaseType_Shr *testcase_shr_mem;
__thread char* default_svr_location = NULL;
__thread int max_svr_group_num = -1;
__thread ProfilePctCountTable *prof_pct_count_table = NULL; 
__thread PerHostSvrTableEntry_Shr *totsvr_table_shr_mem = NULL;
__thread char *file_param_value_big_buf_shr_mem = NULL;
__thread PointerTableEntry_Shr* fparamValueTable_shr_mem = NULL;
/*Manish: create static buffer for data file */
__thread char *data_file_buf = NULL;
__thread long malloced_sized = 0;
__thread NslVarTableEntry_Shr* nsl_var_table_shr_mem = NULL;

#endif
#ifdef RMI_MODE
ReqByteVarTableEntry_Shr *reqbytevar_table_shr_mem = NULL;
#endif
PageTransTableEntry_Shr *page_trans_table_shr_mem = NULL;
ClustValTableEntry_Shr* clust_table_shr_mem = NULL;
GroupValTableEntry_Shr* rungroup_table_shr_mem = NULL;
char** clust_name_table_shr_mem = NULL;
char** rungroup_name_table_shr_mem = NULL;
int* vars_rev_trans_table_shr_mem = NULL;
char* var_type_table_shr_mem = NULL;
//ClickawayProfTableEntry_Shr *clickawayprof_table_shr_mem = NULL;
PerGrpSrcIPTable *per_proc_src_ip_table_shr_mem = NULL;
Master_Src_Ip_Table *master_src_ip_table_shr_mem = NULL;
IP_data *ips_table_shr_mem = NULL;
BodyEncryptionArgs_Shr *body_encryption_shr_mem = NULL; 

key_t shm_base;
int total_num_shared_segs = 0;


#define CAV_MEM_MAP_DONE_ERR_MSG "[Memory Map Dump Request not Processed]"

//pass request type & get string
char *get_req_type_by_name(int type)
{
  switch(type)
  {
    case HTTP_REQUEST:
      return("HTTP");
    case HTTPS_REQUEST:
      return("HTTPS");
    case SMTP_REQUEST:
      return("SMTP");
    case POP3_REQUEST:
      return("POP3");
    case FTP_REQUEST:
      return("FTP");
    case DNS_REQUEST:
      return("DNS");
#ifdef RMI_MODE
    case RMI_REQUEST:
      return("RMI");
    case JBOSS_CONNECT_REQUEST:
      return("Jboss");
    case RMI_CONNECT_REQUEST:
      return("RMI Connect");
    case PING_ACK_REQUEST:
      return("Ping Ack");
#endif
    case XMPP_REQUEST:
      return("XMPP");
    case SOCKET_REQUEST:
      return("SOCKET");
    default:
      return("NA");
  }
}

int is_ip_numeric (char *ipaddr)
{
  while (*ipaddr)
  {
    if(isdigit(*ipaddr) || (*ipaddr == '-') || (*ipaddr == '.') || (*ipaddr == ':') || (*ipaddr == '[') || (*ipaddr == ']'))
	  ipaddr++;
    else
      return 0;
  }
  return 1;
}

u_int32_t hash_function(DB* hash_table, const void *data, u_int32_t data_size) {
  int i;
  u_int32_t key = 0;

  NSDL2_MISC(NULL, NULL, "Method called");
  for (i = 0; i < data_size; i++)
    key += ((char *)data)[0];

  key %= 30;
  return key;
}
#if 0
void*
My_malloc( size_t size )
{
  void* new;
  NSDL2_MISC(vptr, cptr, "Method called");
  if (size < 0) {
    fprintf(stderr, "Trying to malloc w/ a negative or 0 size\n");
    exit(1);
    // Anil - Why it was exit1(); - Neeraj??
  }
  //KNQ: Is there a reason for avoiding 0 size alloc
  if (size == 0) {
    //fprintf(stderr, "Trying to malloc 0 size\n");
    //sleep(300);
    return NULL;
  }
  new = malloc( size );
  if ( new == (void*) 0 )
    {
      (void) fprintf( stderr, "out of memory\n");
      fflush(stderr);
      exit(1);
    }
  return new;
}
#endif

/* debug log and NS_EXIT will not work, removed*/
int ns_init_path(char *bin_path, char *err_msg)
{
  NSDL2_MISC(NULL, NULL, "Method called");
  if (nslb_init_cav_ex(err_msg) == -1) //Code moved after test  TODO: check error on return 
   return -1; 
  NSDL2_MISC(NULL, NULL, "CAV Conf : g_cavinfo.config = [%s] g_cavinfo.SUB_CONFIG = [%s]"
                        " g_cavinfo.NSLoadIF = [%s], g_cavinfo.SRLoadIF = [%s], g_cavinfo.NSAdminIP = [%s]"
                        " g_cavinfo.SRAdminIP = [%s], g_cavinfo.NSAdminGW = [%s], g_cavinfo.SRAdminGW = [%s]"
                        " g_cavinfo.NSAdminIF = [%s], g_cavinfo.SRAdminIF = [%s], g_cavinfo.NSAdminNetBits = [%d]"
                        " g_cavinfo.SRAdminNetBits = [%d]",
                        g_cavinfo.config, g_cavinfo.SUB_CONFIG, g_cavinfo.NSLoadIF, g_cavinfo.SRLoadIF,
                        g_cavinfo.NSAdminIP, g_cavinfo.SRAdminIP, g_cavinfo.NSAdminGW, g_cavinfo.SRAdminGW,
                        g_cavinfo.NSAdminIF, g_cavinfo.SRAdminIF, g_cavinfo.NSAdminNetBits, g_cavinfo.SRAdminNetBits);
  if (chdir(g_ns_wdir)) {
    perror("could not change dir to $NS_WDIR");
    NS_EXIT(-1, CAV_ERR_1000025, g_ns_wdir, errno, nslb_strerror(errno));
  }
  return 0;
}

static inline
char* get_seg_name(SegTableEntry_Shr* seg_ptr) {
  char *var_name = NULL;
  //VarTableEntry_Shr *fparam_var = NULL;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, seg_ptr->type = [%d]", seg_ptr->type);

  switch(seg_ptr->type) 
  {
    case STR:
      var_name = seg_ptr->seg_ptr.str_ptr->big_buf_pointer;
      break;

    //TODO:
    /*case VAR:
      fparam_var = get_fparam_var(NULL, -1, seg_ptr->seg_ptr.fparam_hash_code);
      var_name = fparam_var->name_pointer;
      break;*/
  }

  return var_name;
}

void fprint3f(FILE *fp1, FILE* fp2, FILE *fp3,char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];

  NSDL2_MISC(NULL, NULL, "Method called");
  va_start(ap, format);
  amt_written = vsnprintf(buffer, 4095, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;

  if (fp1)
     fprintf(fp1, "%s", buffer);
  if (fp2)
     fprintf(fp2, "%s", buffer);
  if (fp3)
     fprintf(fp3, "%s", buffer);
}

void fprint2f(FILE *fp1, FILE* fp2,char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];

  NSDL2_MISC(NULL, NULL, "Method called");
  va_start(ap, format);
  amt_written = vsnprintf(buffer, 4095, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;

  if (fp1)
     fprintf(fp1, "%s", buffer);
  if (fp2)
     fprintf(fp2, "%s", buffer);
}

void print2f(FILE *fp, char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];

  NSDL2_MISC(NULL, NULL, "Method called");
  va_start(ap, format);
  amt_written = vsnprintf(buffer, 4095, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;
  if(!g_debug_script)
  {
    if (console_fp != NULL)
      printf("%s", buffer);
  }
  if (fp)
     fprintf(fp, "%s", buffer);
}

void print2f_always(FILE *fp, char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];

  NSDL2_MISC(NULL, NULL, "Method called");
  va_start(ap, format);
  amt_written = vsnprintf(buffer, 4095, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;
  printf("%s", buffer);
 
  if (fp)
     fprintf(fp, "%s", buffer);
  fflush(NULL); // this will flush all open streams
}


static void init_g_temp_buf()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  used_tempbuffer_space = 0;
  MY_MALLOC (g_temp_buf, INIT_TEMPBUFFER, "g_temp_buf", -1);
  g_temp_ptr = g_temp_buf;
  NSDL2_SCHEDULE(NULL, NULL, "Exiting Method");
}

static int create_temp_buf_space(void) {
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  char* old_temp_buf_ptr = g_temp_buf;
  MY_REALLOC_EX (g_temp_buf, max_tempbuffer_space + DELTA_TEMPBUFFER, max_tempbuffer_space, "g_temp_buf", -1);
  if (!g_temp_buf){
    //fprintf(stderr, "create_temp_buf_space(): Error allocating more memory for g_temp_buf\n");
    return(FAILURE);
  } else {
    if (old_temp_buf_ptr != g_temp_buf)
      g_temp_ptr = g_temp_buf + used_tempbuffer_space;
    max_tempbuffer_space += DELTA_TEMPBUFFER;
  }
  return (SUCCESS);
}

static int enough_temp_memory(int space) {
  NSDL2_MISC(NULL, NULL, "Method called, space = %d", space);
  return (g_temp_ptr+space < g_temp_buf+max_tempbuffer_space);
}

int copy_into_temp_buf(char* data, int size) {
  NSDL2_MISC(NULL, NULL, "Method called, data = %s, size = %d", data, size);
  int data_loc = used_tempbuffer_space;

  if (size == 0)
    size = strlen(data);

  while (!enough_temp_memory(size+1))
    if (create_temp_buf_space() != SUCCESS)
      return -1;

  memcpy(g_temp_ptr, data, size);
  g_temp_ptr[size] = '\0';

  g_temp_ptr += size +1;
  used_tempbuffer_space += size +1;

  return data_loc;
}

static void free_g_temp_buf()
{
  FREE_AND_MAKE_NULL_EX (g_temp_buf, max_tempbuffer_space, "g_temp_buf", -1);
}

int init_userinfo() {
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  total_user_entries = 0;
  cur_ip_entry = 0;
  total_ip_entries = 0;
  total_group_ip_entries = 0;
  total_client_entries = 0;
  cur_server_entry = 0;
  //total_server_entries = 0;
  total_sess_entries = 0;
  total_page_entries = 0;
  total_svr_entries = 0;
  total_var_entries = 0;
  total_index_var_entries = 0;
  total_group_entries = 0;
  total_repeat_block_entries = 0;
  total_pointer_entries = 0;
  total_fparam_entries = 0;
  total_locattr_entries = 0;
  total_linechar_entries = 0;
  total_accattr_entries = 0;
  total_accloc_entries = 0;
  total_browattr_entries = 0;
  total_machattr_entries = 0;
  total_freqattr_entries = 0;
  total_sessprof_entries = 0;
  total_sessprofindex_entries = 0;
  total_userprof_entries = 0;
  total_userindex_entries = 0;
  total_runprof_entries = 0;
  total_runindex_entries = 0;
  total_weight_entries = 0;
  total_serverorder_entries = 0;
  total_metric_entries = 0;
  total_post_entries = 0;
  total_request_entries = 0;
  total_smtp_request_entries = 0;
  total_pop3_request_entries = 0;
  total_ftp_request_entries = 0;
  total_jrmi_request_entries = 0; // for jrmi
  total_dns_request_entries = 0;
  total_ldap_request_entries = 0; //for ldap
  total_imap_request_entries = 0;
  total_ws_request_entries = 0;  //for ws
  total_socket_request_entries = 0;
  total_host_entries = 0;
  total_report_entries = 0;
  total_reqcook_entries = 0;
  total_dynvar_entries = 0;
  total_reqdynvar_entries = 0;
  total_seg_entries = 0;
  //total_totsvr_entries = 0;
  total_inusesvr_entries = 0;
  total_inuseuser_entries = 0;
  total_thinkprof_entries = 0;
  total_inline_delay_entries = 0;
  total_pacing_entries = 0;
  total_autofetch_entries = 0; // for auto fetch embedded
  total_cont_on_err_entries = 0;
  total_recorded_think_time_entries = 0;
#ifdef RMI_MODE
  total_bytevar_entries = 0;
  total_reqbytevar_entries = 0;
#endif
  total_errorcode_entries = 0;
  total_nsvar_entries = 0;
  total_tag_entries = 0;
  total_tagpage_entries = 0;
  total_attrqual_entries = 0;
  total_clustvar_entries = 0;
  total_clustval_entries = 0;
  total_clust_entries = 0;
  total_groupvar_entries = 0;
  total_groupval_entries = 0;
  total_proxy_svr_entries = INIT_PROXY_SVR_ENTRIES;
  total_proxy_excp_entries = 0;
  total_proxy_ip_interfaces = 0;
  //  total_clickawayprof_entries = 0;
  used_buffer_space = 0;
  total_rbu_domain_entries = 0;
  total_ssl_cert_key_entries = 0;
  total_add_rec_host_entries = 0;

  //  users = (User_data *)malloc(INIT_USER_ENTRIES * sizeof(User_data));
  MY_MALLOC (ips, INIT_IP_ENTRIES * sizeof(IP_data), "ips", -1);
  MY_MALLOC (clients, INIT_CLIENT_ENTRIES * sizeof(Client_data), "clients", -1);
  //servers = (Server_data *)malloc(INIT_SERVER_ENTRIES * sizeof(Server_data));
  MY_MALLOC (gSessionTable, INIT_SESSION_ENTRIES * sizeof(SessTableEntry), "gSessionTable", -1);
  MY_MALLOC (gPageTable, INIT_PAGE_ENTRIES * sizeof(PageTableEntry), "gPageTable", -1);
  alloc_mem_for_txtable ();  // Added by anuj (ns_trans_parse.c)
  MY_MALLOC (gServerTable, INIT_SVR_ENTRIES * sizeof(SvrTableEntry), "gServerTable", -1);
  MY_MALLOC (varTable, INIT_VAR_ENTRIES * sizeof(VarTableEntry), "varTable", -1);
  MY_MALLOC (groupTable, INIT_GROUP_ENTRIES * sizeof(GroupTableEntry), "groupTable", -1);
  MY_MALLOC (repeatBlock, INIT_REPEAT_BLOCK_ENTRIES * sizeof(RepeatBlock), "repeatBlock", -1);
  MY_MALLOC (pointerTable, INIT_POINTER_ENTRIES * sizeof(PointerTableEntry), "pointerTable", -1);
  MY_MALLOC (fparamValueTable, INIT_POINTER_ENTRIES * sizeof(PointerTableEntry), "fparamValueTable", -1);
  MY_MALLOC (weightTable, INIT_WEIGHT_ENTRIES * sizeof(WeightTableEntry), "weightTable", -1);
  MY_MALLOC (locAttrTable, INIT_LOCATTR_ENTRIES * sizeof(LocAttrTableEntry), "locAttrTable", -1);
  MY_MALLOC (lineCharTable, INIT_LINECHAR_ENTRIES * sizeof(LineCharTableEntry), "lineCharTable", -1);
  MY_MALLOC (accAttrTable, INIT_ACCATTR_ENTRIES * sizeof(AccAttrTableEntry), "accAttrTable", -1);
  MY_MALLOC (accLocTable, INIT_ACCLOC_ENTRIES * sizeof(AccLocTableEntry), "accLocTable", -1);
  MY_MALLOC (browAttrTable, INIT_BROWATTR_ENTRIES * sizeof(BrowAttrTableEntry), "browAttrTable", -1);
  //MY_MALLOC (machAttrTable, INIT_MACHATTR_ENTRIES * sizeof(MachAttrTableEntry), "machAttrTable", -1);
  //MY_MALLOC (freqAttrTable, INIT_FREQATTR_ENTRIES * sizeof(FreqAttrTableEntry), "freqAttrTable", -1);
  MY_MALLOC (sessProfTable, INIT_SESSPROF_ENTRIES * sizeof(SessProfTableEntry), "sessProfTable", -1);
  MY_MALLOC (sessProfIndexTable, INIT_SESSPROFINDEX_ENTRIES * sizeof(SessProfIndexTableEntry), "sessProfIndexTable", -1);
  MY_MALLOC (userProfTable, INIT_USERPROF_ENTRIES * sizeof(RunProfTableEntry), "userProfTable", -1);
  MY_MALLOC (userIndexTable, INIT_USERINDEX_ENTRIES * sizeof(UserIndexTableEntry), "userIndexTable", -1);
  MY_MALLOC (runProfTable, INIT_RUNPROF_ENTRIES * sizeof(RunProfTableEntry), "runProfTable", -1);
  MY_MALLOC (runIndexTable, INIT_RUNINDEX_ENTRIES * sizeof(RunIndexTableEntry), "runIndexTable", -1);
  MY_MALLOC (serverOrderTable, INIT_SERVERORDER_ENTRIES * sizeof(ServerOrderTableEntry), "serverOrderTable", -1);
  alloc_mem_for_sla_table(); // Added by anuj (ns_goal_based_sla.c)
  MY_MALLOC (metricTable, INIT_METRIC_ENTRIES * sizeof(MetricTableEntry), "metricTable", -1);
  MY_MALLOC (requests, INIT_REQUEST_ENTRIES * sizeof(action_request), "requests", -1);
  MY_MALLOC (postTable, INIT_POST_ENTRIES * sizeof(StrEnt), "postTable", -1);
  MY_MALLOC (hostTable, INIT_HOST_ENTRIES * sizeof(HostElement), "hostTable", -1);
  MY_MALLOC (reportTable, INIT_REPORT_ENTRIES * sizeof(avgtime), "reportTable", -1);
  MY_MALLOC (reqCookTable, INIT_REQCOOK_ENTRIES * sizeof(ReqCookTableEntry), "reqCookTable", -1);
  MY_MALLOC (dynVarTable, INIT_DYNVAR_ENTRIES * sizeof(DynVarTableEntry), "dynVarTable", -1);
  MY_MALLOC (reqDynVarTable, INIT_REQDYNVAR_ENTRIES * sizeof(ReqDynVarTableEntry), "reqDynVarTable", -1);
  MY_MALLOC (segTable, INIT_SEG_ENTRIES * sizeof(SegTableEntry), "segTable", -1);
  MY_MALLOC (inuseSvrTable, INIT_INUSESVR_ENTRIES * sizeof(InuseSvrTableEntry), "inuseSvrTable", -1);
  MY_MALLOC (inuseUserTable, INIT_INUSEUSER_ENTRIES * sizeof(InuseUserTableEntry), "inuseUserTable", -1);
  MY_MALLOC (thinkProfTable, INIT_THINKPROF_ENTRIES * sizeof(ThinkProfTableEntry), "thinkProfTable", -1);
  MY_MALLOC (inlineDelayTable, INIT_INLINE_DELAY_ENTRIES * sizeof(InlineDelayTableEntry), "inlineDelayTable", -1);
  MY_MALLOC (autofetchTable, INIT_AUTOFETCH_ENTRIES * sizeof(AutoFetchTableEntry), "autofetchTable", -1); // for auto fetch embedded
  MY_MALLOC (continueOnPageErrorTable, INIT_CONT_ON_ERR_ENTRIES * sizeof(ContinueOnPageErrorTableEntry), "continue on page error table", -1); // for override recorded think time table
  MY_MALLOC (overrideRecordedThinktimeTable, INIT_OVERRIDE_RECORDED_THINK_ENTRIES * sizeof(OverrideRecordedThinkTime), "override recorded think time", -1); 
  MY_MALLOC (clickActionTable, INIT_CLICKACTION_ENTRIES * sizeof(ClickActionTableEntry), "clickActionTable", -1); // for click script user actions
  MY_MALLOC (pacingTable, INIT_PACING_ENTRIES * sizeof(PacingTableEntry), "pacingTable", -1);
#ifdef RMI_MODE
  MY_MALLOC (byteVarTable, INIT_BYTEVAR_ENTRIES * sizeof(ByteVarTableEntry), "byteVarTable", -1);
  MY_MALLOC (reqByteVarTable, INIT_REQBYTEVAR_ENTRIES * sizeof(ReqByteVarTableEntry), "reqByteVarTable", -1);
#endif
  MY_MALLOC (errorCodeTable, INIT_ERRORCODE_ENTRIES * sizeof(ErrorCodeTableEntry), "errorCodeTable", -1);
  MY_MALLOC (nsVarTable, INIT_NSVAR_ENTRIES * sizeof(NsVarTableEntry), "nsVarTable", -1);
  MY_MALLOC (tagTable, INIT_TAG_ENTRIES * sizeof(TagTableEntry), "tagTable", -1);
  MY_MALLOC (tagPageTable, INIT_TAGPAGE_ENTRIES * sizeof(TagPageTableEntry), "tagPageTable", -1);
  MY_MALLOC (attrQualTable, INIT_ATTRQUAL_ENTRIES * sizeof(AttrQualTableEntry), "attrQualTable", -1);
  //Manish: MY_MALLOC (searchVarTable, INIT_SEARCHVAR_ENTRIES * sizeof(SearchVarTableEntry), "searchVarTable", -1);
  MY_MALLOC (searchPageTable, INIT_SEARCHPAGE_ENTRIES * sizeof(SearchPageTableEntry), "searchPageTable", -1);
  MY_MALLOC (jsonPageTable, INIT_JSONPAGE_ENTRIES * sizeof(JSONPageTableEntry), "jsonPageTable", -1);
  //Manish: MY_MALLOC (randomVarTable, INIT_RANDOMVAR_ENTRIES * sizeof(RandomVarTableEntry), "randomVarTable", -1);
  //Manish: MY_MALLOC (randomStringTable, INIT_RANDOMSTRING_ENTRIES * sizeof(RandomStringTableEntry), "randomStringTable", -1);
  //Manish: MY_MALLOC (dateVarTable, INIT_DATEVAR_ENTRIES * sizeof(DateVarTableEntry), "dateVarTable", -1);
  //Manish: MY_MALLOC (uniqueVarTable, INIT_UNIQUEVAR_ENTRIES * sizeof(UniqueVarTableEntry), "uniqueVarTable", -1);
  MY_MALLOC (perPageSerVarTable, INIT_PERPAGESERVAR_ENTRIES * sizeof(PerPageSerVarTableEntry), "perPageSerVarTable", -1);
  MY_MALLOC (perPageJSONVarTable, INIT_PERPAGEJSONVAR_ENTRIES * sizeof(PerPageJSONVarTableEntry), "perPageJSONVarTable", -1);
  MY_MALLOC (perPageChkPtTable, INIT_PERPAGECHKPT_ENTRIES * sizeof(PerPageChkPtTableEntry), "perPageChkPtTable", -1);
  MY_MALLOC (perPageChkRepSizeTable, INIT_PERPAGECHK_REPSIZE_ENTRIES * sizeof(PerPageCheckReplySizeTableEntry), "PerPageCheckReplySizeTableEntry", -1);
  MY_MALLOC (clustVarTable, INIT_CLUSTVAR_ENTRIES * sizeof(ClustVarTableEntry), "clustVarTable", -1);
  MY_MALLOC (clustValTable, INIT_CLUSTVAL_ENTRIES * sizeof(ClustValTableEntry), "clustValTable", -1);
  MY_MALLOC (clustTable, INIT_CLUST_ENTRIES * sizeof(ClustTableEntry), "clustTable", -1);
  MY_MALLOC (groupVarTable, INIT_GROUPVAR_ENTRIES * sizeof(GroupVarTableEntry), "groupVarTable", -1);
  MY_MALLOC (groupValTable, INIT_GROUPVAL_ENTRIES * sizeof(GroupValTableEntry), "groupValTable", -1);
  MY_MALLOC (proxySvrTable, INIT_PROXY_SVR_ENTRIES * sizeof(ProxyServerTable), "proxySvrTable", -1);
  memset(proxySvrTable, -1, INIT_PROXY_SVR_ENTRIES * sizeof(ProxyServerTable));
  //  clickawayProfTable = (ClickawayProfTableEntry *)malloc(INIT_CLICKAWAYPROF_ENTRIES * sizeof(ClickawayProfTableEntry));

  // Big buf must be allocated before using from any method
  MY_MALLOC (g_big_buf, INIT_BIGBUFFER, "g_big_buf", -1);
  g_buf_ptr = g_big_buf;
  init_searchvar_info();
  init_jsonvar_info();  //for JSON Var
  init_checkpoint_info();
  init_check_replysize_info();
  init_cookie_info();
  init_randomvar_info();
  init_randomstring_info();
  init_uniquevar_info();
  init_unique_range_var_info();
  init_datevar_info();
  init_http_method_table();
  init_g_temp_buf();
  nslb_init_temparray(); // added for temp array for search var
  
  if (db_create(&var_hash_table, NULL, 0)) {
    printf("Error in init_userinfo(): Failed in creating the variable hash table\n");
    return (FAILURE);
  }

  if (var_hash_table->set_h_hash(var_hash_table, hash_function)) {
    printf("Error in init_userinfo(): Failed in setting the hash function for the variable hash table\n");
    return (FAILURE);
  }

/*#if (Fedora && RELEASE == 4)
  if (var_hash_table->open(var_hash_table, NULL, NULL, DB_HASH, 0, 0)) {
    printf("Error in init_userinfo(): Failed in opening the  variable hash table\n");
    return (FAILURE);
  }
#else*/
  if (var_hash_table->open(var_hash_table, NULL, NULL, NULL, DB_HASH, DB_CREATE, 0)) {
    printf("Error in init_userinfo(): Failed in opening the  variable hash table\n");
    return (FAILURE);
  }
//#endif

  if (ips && clients && //servers && && users &&
      gSessionTable /*&& txTable*/ && gPageTable && gServerTable &&
      varTable && groupTable && pointerTable && fparamValueTable &&
      weightTable && locAttrTable && lineCharTable &&
      accAttrTable && accLocTable && browAttrTable &&
      //machAttrTable && freqAttrTable &&
      sessProfTable && sessProfIndexTable && userProfTable &&
      userIndexTable && runProfTable && runIndexTable &&
      serverOrderTable && slaTable && metricTable && requests && postTable &&
      hostTable && reportTable && reqCookTable &&
      dynVarTable && reqDynVarTable && segTable && /*totSvrTable &&*/
#ifdef RMI_MODE
      byteVarTable && reqByteVarTable &&
#endif
      errorCodeTable && inuseSvrTable && inuseUserTable && thinkProfTable && inlineDelayTable &&
      pacingTable && nsVarTable && tagTable && tagPageTable && attrQualTable &&
      searchVarTable && searchPageTable && jsonVarTable && jsonPageTable &&  dateVarTable && randomVarTable && randomStringTable  && uniqueVarTable && checkPageTable && checkReplySizePageTable &&
      perPageSerVarTable && perPageChkPtTable && perPageChkRepSizeTable &&
      clustVarTable && clustValTable && clustTable &&
      groupVarTable && groupValTable &&
      //      && clickawayProfTable
      g_big_buf && g_temp_buf && var_hash_table && autofetchTable && proxySvrTable && continueOnPageErrorTable) {
    max_user_entries = INIT_USER_ENTRIES;
    max_ip_entries = INIT_IP_ENTRIES;
    max_client_entries = INIT_CLIENT_ENTRIES;
    //max_server_entries = INIT_SERVER_ENTRIES;
    max_sess_entries = INIT_SESSION_ENTRIES;
    max_page_entries = INIT_PAGE_ENTRIES;
    max_svr_entries = INIT_SVR_ENTRIES;
    max_var_entries = INIT_VAR_ENTRIES;
    max_group_entries = INIT_GROUP_ENTRIES;
    max_repeat_block_entries = INIT_REPEAT_BLOCK_ENTRIES;
    max_pointer_entries = INIT_POINTER_ENTRIES;
    max_fparam_entries = INIT_POINTER_ENTRIES;
    max_weight_entries = INIT_WEIGHT_ENTRIES;
    max_locattr_entries = INIT_LOCATTR_ENTRIES;
    max_linechar_entries = INIT_LINECHAR_ENTRIES;
    max_accattr_entries = INIT_ACCATTR_ENTRIES;
    max_accloc_entries = INIT_ACCLOC_ENTRIES;
    max_browattr_entries = INIT_BROWATTR_ENTRIES;
    max_machattr_entries = INIT_MACHATTR_ENTRIES;
    max_freqattr_entries = INIT_FREQATTR_ENTRIES;
    max_sessprof_entries = INIT_SESSPROF_ENTRIES;
    max_sessprofindex_entries = INIT_SESSPROFINDEX_ENTRIES;
    max_userprof_entries = INIT_USERPROF_ENTRIES;
    max_userindex_entries = INIT_USERINDEX_ENTRIES;
    max_runprof_entries = INIT_RUNPROF_ENTRIES;
    max_runindex_entries = INIT_RUNINDEX_ENTRIES;
    max_serverorder_entries = INIT_SERVERORDER_ENTRIES;
    max_metric_entries = INIT_METRIC_ENTRIES;
    max_request_entries = INIT_REQUEST_ENTRIES;
    max_post_entries = INIT_POST_ENTRIES;
    max_host_entries = INIT_HOST_ENTRIES;
    max_report_entries = INIT_REPORT_ENTRIES;
    max_reqcook_entries = INIT_REQCOOK_ENTRIES;
    max_dynvar_entries = INIT_DYNVAR_ENTRIES;
    max_reqdynvar_entries = INIT_REQDYNVAR_ENTRIES;
    max_seg_entries = INIT_SEG_ENTRIES;
    max_inusesvr_entries = INIT_INUSESVR_ENTRIES;
    max_inuseuser_entries = INIT_INUSEUSER_ENTRIES;
    max_thinkprof_entries = INIT_THINKPROF_ENTRIES;
    max_inline_delay_entries = INIT_INLINE_DELAY_ENTRIES;
    max_pacing_entries = INIT_PACING_ENTRIES;
    max_autofetch_entries = INIT_AUTOFETCH_ENTRIES; // for auto fetch embedded
    max_cont_on_err_entries = INIT_CONT_ON_ERR_ENTRIES; 
    max_recorded_think_time_entries = INIT_OVERRIDE_RECORDED_THINK_ENTRIES; 
    max_proxy_svr_entries = INIT_PROXY_SVR_ENTRIES;
#ifdef RMI_MODE
    max_bytevar_entries = INIT_BYTEVAR_ENTRIES;
    max_reqbytevar_entries = INIT_REQBYTEVAR_ENTRIES;
#endif
    max_errorcode_entries = INIT_ERRORCODE_ENTRIES;
    max_nsvar_entries = INIT_NSVAR_ENTRIES;
    max_tag_entries = INIT_TAG_ENTRIES;
    max_tagpage_entries = INIT_TAGPAGE_ENTRIES;
    max_attrqual_entries = INIT_ATTRQUAL_ENTRIES;
    max_perpageservar_entries = INIT_PERPAGESERVAR_ENTRIES;
    max_perpagejsonvar_entries = INIT_PERPAGEJSONVAR_ENTRIES; //For JSON Var
    max_clustvar_entries = INIT_CLUSTVAR_ENTRIES;
    max_clustval_entries = INIT_CLUSTVAL_ENTRIES;
    max_clust_entries = INIT_CLUST_ENTRIES;
    max_groupvar_entries = INIT_GROUPVAR_ENTRIES;
    max_groupval_entries = INIT_GROUPVAL_ENTRIES;
    //    max_clickawayprof_entries = INIT_CLICKAWAYPROF_ENTRIES;
    max_buffer_space = INIT_BIGBUFFER;
    max_tempbuffer_space = INIT_TEMPBUFFER;

    memset(&testCase, 0, sizeof(testCase));

    return(SUCCESS);
  } else {
    fprintf(stderr, CAV_ERR_1031014);

    max_user_entries = 0;
    max_ip_entries = 0;
    max_client_entries = 0;
    //max_server_entries = 0;
    max_sess_entries = 0;
    max_page_entries = 0;
    max_svr_entries = 0;
    max_var_entries = 0;
    max_group_entries = 0;
    max_pointer_entries = 0;
    max_fparam_entries = 0;
    max_weight_entries = 0;
    max_locattr_entries = 0;
    max_linechar_entries = 0;
    max_accattr_entries = 0;
    max_accloc_entries = 0;
    max_browattr_entries = 0;
    max_machattr_entries = 0;
    max_freqattr_entries = 0;
    max_sessprof_entries = 0;
    max_sessprofindex_entries = 0;
    max_userprof_entries = 0;
    max_userindex_entries = 0;
    max_runprof_entries = 0;
    max_runindex_entries = 0;
    max_serverorder_entries = 0;
    max_metric_entries = 0;
    max_request_entries = 0;
    max_post_entries = 0;
    max_host_entries = 0;
    max_report_entries = 0;
    max_reqcook_entries = 0;
    max_dynvar_entries = 0;
    max_reqdynvar_entries = 0;
    max_seg_entries = 0;
    //max_totsvr_entries = 0;
    max_inusesvr_entries = 0;
    max_inuseuser_entries = 0;
    max_thinkprof_entries = 0;
    max_inline_delay_entries = 0;
    max_pacing_entries = 0;
    max_autofetch_entries = 0; // for auto fetch embedded
    max_cont_on_err_entries = 0; // for auto fetch embedded
    max_recorded_think_time_entries = 0;
#ifdef RMI_MODE
    max_bytevar_entries = 0;
    max_reqbytevar_entries = 0;
#endif
    max_errorcode_entries = 0;
    max_nsvar_entries = 0;
    max_tag_entries = 0;
    max_tagpage_entries = 0;
    max_attrqual_entries = 0;
    max_perpageservar_entries = 0;
    max_perpagejsonvar_entries = 0;  //For JSON Var
    max_clustvar_entries = 0;
    max_clustval_entries = 0;
    max_clust_entries = 0;
    max_groupvar_entries = 0;
    max_groupval_entries = 0;
    //    max_clickawayprof_entries = 0;
    max_buffer_space = 0;
    max_tempbuffer_space = 0;
    max_proxy_svr_entries = 0;
    max_proxy_excp_entries = 0;
    max_proxy_ip_interfaces = 0;
    max_rbu_domain_entries = 0;
    max_ssl_cert_key_entries = 0;
    return(FAILURE);
  }

  return(FAILURE);
}

/*static int create_user_table_entry(int *row_num) {
  if (total_user_entries == max_user_entries) {
    users = (User_data *) realloc ((char *)users,
				   (max_user_entries + DELTA_USER_ENTRIES) *
				   sizeof(User_data));
    if (!users) {
      fprintf(stderr,"create_user_table_entry(): Error allocating more memory for user entries\n");
      return(FAILURE);
    } else max_user_entries += DELTA_USER_ENTRIES;
  }
  *row_num = total_user_entries++;
  return (SUCCESS);
  }*/

int create_sess_table_entry(int *row_num) {
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  if (total_sess_entries == max_sess_entries) {
    MY_REALLOC_EX (gSessionTable, (max_sess_entries + DELTA_SESSION_ENTRIES) * sizeof(SessTableEntry), max_sess_entries * sizeof(SessTableEntry), "gSessionTable", -1);
    if (!gSessionTable) {
      fprintf(stderr,"create_sess_table_entry(): Error allocating more memory for session entries\n");
      return(FAILURE);
    } else max_sess_entries += DELTA_SESSION_ENTRIES;
  }
  *row_num = total_sess_entries++;
  
  gSessionTable[*row_num].sp_grp_tbl_idx = -1;   //default value for sync point

  gSessionTable[*row_num].tagpage_start_idx = -1;
  gSessionTable[*row_num].num_tagpage_entries = 0;
  gSessionTable[*row_num].tag_start_idx = -1;
  gSessionTable[*row_num].num_tag_entries = 0;
  gSessionTable[*row_num].nslvar_start_idx = -1;
  gSessionTable[*row_num].num_nslvar_entries = 0;
  gSessionTable[*row_num].var_start_idx = -1;
  gSessionTable[*row_num].num_var_entries = 0;
  gSessionTable[*row_num].index_var_start_idx = -1;
  gSessionTable[*row_num].num_index_var_entries = 0;
  gSessionTable[*row_num].searchvar_start_idx = -1;
  gSessionTable[*row_num].jsonvar_start_idx = -1;
  gSessionTable[*row_num].num_jsonvar_entries= 0;
  gSessionTable[*row_num].num_searchvar_entries = 0;
  gSessionTable[*row_num].checkpoint_start_idx = -1;
  gSessionTable[*row_num].num_checkpoint_entries = 0;
  gSessionTable[*row_num].searchpagevar_start_idx = -1;
  gSessionTable[*row_num].num_searchpagevar_entries = 0;
  gSessionTable[*row_num].checkpage_start_idx = -1;
  gSessionTable[*row_num].num_checkpage_entries = 0;
  gSessionTable[*row_num].cookievar_start_idx = -1;
  gSessionTable[*row_num].num_cookievar_entries = 0;
  gSessionTable[*row_num].first_page = -1;
  gSessionTable[*row_num].num_pages = 0;
  gSessionTable[*row_num].pacing_idx = -1;
  gSessionTable[*row_num].randomvar_start_idx = -1;
  gSessionTable[*row_num].num_randomvar_entries = 0;

  gSessionTable[*row_num].randomstring_start_idx = -1;
  gSessionTable[*row_num].num_randomstring_entries = 0;

  gSessionTable[*row_num].uniquevar_start_idx = -1;
  gSessionTable[*row_num].num_uniquevar_entries = 0;
  
  gSessionTable[*row_num].datevar_start_idx = -1;
  gSessionTable[*row_num].num_datevar_entries = 0;

  gSessionTable[*row_num].unique_range_var_start_idx = -1;
  gSessionTable[*row_num].num_unique_range_var_entries = 0;
  // -- Add Achint- For global cookie - 10/04/2007
  gSessionTable[*row_num].cookies.cookie_start = -1;
  gSessionTable[*row_num].cookies.num_cookies = 0;
  //for checkreplysize
  gSessionTable[*row_num].checkreplysize_start_idx = -1;
  gSessionTable[*row_num].num_checkreplysize_entries = 0;
  gSessionTable[*row_num].checkreplysizepage_start_idx = -1;
  gSessionTable[*row_num].num_checkreplysizepage_entries = 0;
  gSessionTable[*row_num].sess_id = *row_num;
  gSessionTable[*row_num].rbu_tti_prof_tree = NULL;
  gSessionTable[*row_num].host_table_entries = NULL;
  gSessionTable[*row_num].total_sess_host_table_entries = 0;
  gSessionTable[*row_num].max_sess_host_table_entries = 0;
  gSessionTable[*row_num].dyn_norm_ids = NULL;
  gSessionTable[*row_num].num_dyn_entries = 0;
  gSessionTable[*row_num].max_dyn_entries = 0;
  gSessionTable[*row_num].save_url_body_head_resp = 0;
  gSessionTable[*row_num].jmeter_sess_name = -1;
  gSessionTable[*row_num].exit_func_ptr = NULL;
  gSessionTable[*row_num].init_func_ptr = NULL;
  gSessionTable[*row_num].user_test_init = NULL;
  gSessionTable[*row_num].user_test_exit = NULL;
  gSessionTable[*row_num].flags = 0;
  NSDL2_SCHEDULE(NULL, NULL, "gSessionTable[*row_num].sess_id=%u", gSessionTable[*row_num].sess_id);
  
  return (SUCCESS);
}

#ifndef CAV_MAIN
int total_page_entries = 0;
int max_page_entries = 0;
#else
__thread int total_page_entries = 0;
__thread int max_page_entries = 0;
#endif

int create_page_table_entry(int *row_num) {
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  if (total_page_entries == max_page_entries) {
    MY_REALLOC_EX (gPageTable, (max_page_entries + DELTA_PAGE_ENTRIES) * sizeof(PageTableEntry), max_page_entries * sizeof(PageTableEntry), "gPageTable", -1);
    if (!gPageTable) {
      fprintf(stderr,"create_page_table_entry(): Error allocating more memory for page entries\n");
      return(FAILURE);
    } else max_page_entries += DELTA_PAGE_ENTRIES;
  }
  *row_num = total_page_entries++;
  gPageTable[*row_num].sp_grp_tbl_idx = -1;      // default value for sync point

  gPageTable[*row_num].head_hlist = -1;
  gPageTable[*row_num].tail_hlist = -1;
  //gPageTable[*row_num].think_prof_idx = -1;
#ifndef RMI_MODE
  gPageTable[*row_num].tag_root_idx = -1;
#endif
  gPageTable[*row_num].first_searchvar_idx = -1;
  gPageTable[*row_num].first_jsonvar_idx = -1;
  gPageTable[*row_num].num_searchvar = 0;
  gPageTable[*row_num].first_checkpoint_idx = -1;
  gPageTable[*row_num].num_checkpoint = 0;

  gPageTable[*row_num].first_check_replysize_idx = -1;
  gPageTable[*row_num].num_check_replysize = 0;
  gPageTable[*row_num].redirection_depth_bitmask = 0; //initialize depth bitmask
  gPageTable[*row_num].save_headers = 0; //initialize save header  
  gPageTable[*row_num].flow_name = -1;
  gPageTable[*row_num].page_id = *row_num;
  gPageTable[*row_num].flags = 0;
  return (SUCCESS);
}

int find_locattr_idx(char* name) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, name = %s, total_locattr_entries = %d", name, total_locattr_entries);
  for (i = 0; i < total_locattr_entries; i++) {
    //printf("---- i = %d, Name = [%s]\n", i, RETRIEVE_BUFFER_DATA(locAttrTable[i].name));
    if (!strncmp(RETRIEVE_BUFFER_DATA(locAttrTable[i].name), name, strlen(name))) {
      return i;
    }
  }
  return -1;
}

/* This function is used in Browser Based script only
 * In function find_locattr_idx() - we are getting name form talbe locAttrTable which is local and freed But we need name at runtime so we 
 * have to use shared memory table locattr_table_shr_mem
 */
int find_locattr_shr_idx(char* name) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, name = %s, total_locattr_entries = %d", name, total_locattr_entries);

  for (i = 0; i < total_locattr_entries; i++) {
    NSDL4_MISC(NULL, NULL, "i = %d, Name = [%s]", i, locattr_table_shr_mem[i].name);
    if (!strncmp(locattr_table_shr_mem[i].name, name, strlen(name))) {
      return i;
    }
  }
  return -1;
}

int create_sessprof_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_sessprof_entries == max_sessprof_entries) {
    MY_REALLOC_EX (sessProfTable, (max_sessprof_entries + DELTA_SESSPROF_ENTRIES) * sizeof(SessProfTableEntry), max_sessprof_entries * sizeof(SessProfTableEntry), "sessProfTable", -1);
    if (!sessProfTable) {
      fprintf(stderr,"create_sessprof_table_entry(): Error allocating more memory for sessprof entries\n");
      return(FAILURE);
    } else max_sessprof_entries += DELTA_SESSPROF_ENTRIES;
  }
  *row_num = total_sessprof_entries++;
  return (SUCCESS);
}

int create_sessprofindex_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_sessprofindex_entries == max_sessprofindex_entries) {
    MY_REALLOC_EX (sessProfIndexTable, (max_sessprofindex_entries + DELTA_SESSPROFINDEX_ENTRIES) * sizeof(SessProfIndexTableEntry), max_sessprofindex_entries * sizeof(SessProfIndexTableEntry), "sessProfIndexTable", -1);
    if (!sessProfIndexTable) {
      fprintf(stderr,"create_sessprofindex_table_entry(): Error allocating more memory for sessprofindex entries\n");
      return(FAILURE);
    } else max_sessprofindex_entries += DELTA_SESSPROFINDEX_ENTRIES;
  }
  *row_num = total_sessprofindex_entries++;
  sessProfIndexTable[*row_num].sessprof_start = -1;
  sessProfIndexTable[*row_num].sessprof_length = 0;
  return (SUCCESS);
}

int create_runindex_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_runindex_entries == max_runindex_entries) {
    MY_REALLOC_EX (runIndexTable, (max_runindex_entries + DELTA_RUNINDEX_ENTRIES) * sizeof(RunIndexTableEntry), max_runindex_entries * sizeof(RunIndexTableEntry), "runIndexTable", -1);
    if (!runIndexTable) {
      fprintf(stderr,"create_runindex_table_entry(): Error allocating more memory for runindex entries\n");
      return(FAILURE);
    } else max_runindex_entries += DELTA_RUNINDEX_ENTRIES;
  }
  *row_num = total_runindex_entries++;
  runIndexTable[*row_num].runprof_start = -1;
  runIndexTable[*row_num].runprof_length = 0;
  return (SUCCESS);
}

int create_serverorder_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_serverorder_entries == max_serverorder_entries) {
    MY_REALLOC_EX (serverOrderTable, (max_serverorder_entries + DELTA_SERVERORDER_ENTRIES) * sizeof(ServerOrderTableEntry), max_serverorder_entries * sizeof(ServerOrderTableEntry), "serverOrderTable", -1);
    if (!serverOrderTable) {
      return(FAILURE);
    } else max_serverorder_entries += DELTA_SERVERORDER_ENTRIES;
  }
  *row_num = total_serverorder_entries++;
  return (SUCCESS);
}

int create_smtp_requests_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_request_entries == max_request_entries) {
    MY_REALLOC_EX (requests, (max_request_entries + DELTA_REQUEST_ENTRIES) * sizeof(smtp_request), max_request_entries * sizeof(smtp_request),"smtp requests", -1);
    if (!requests) {
      fprintf(stderr,"create_requests_table_entry(): Error allocating more memory for http_request entries\n");
      return(FAILURE);
    } else max_request_entries += DELTA_REQUEST_ENTRIES;
  }
  *row_num = total_request_entries++;
  return (SUCCESS);
}

int create_requests_table_entry(int *row_num) 
{
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_request_entries == max_request_entries) 
  {
    MY_REALLOC_EX (requests, (max_request_entries + DELTA_REQUEST_ENTRIES) * sizeof(action_request), max_request_entries * sizeof(action_request), "requests", -1);
    if (!requests) {
      fprintf(stderr,"create_requests_table_entry(): Error allocating more memory for http_request entries\n");
      return(FAILURE);
    } else max_request_entries += DELTA_REQUEST_ENTRIES;
  }
  *row_num = total_request_entries++;
  requests[*row_num].server_base = -1;
  requests[*row_num].proto.http.url_index = *row_num;
  requests[*row_num].proto.http.tx_idx = -1;
  requests[*row_num].proto.http.tx_prefix = -1;
  requests[*row_num].index.svr_idx = -1;
  requests[*row_num].index.svr_idx = -1;
  requests[*row_num].schedule_time = 0;
  requests[*row_num].is_url_parameterized = 0;
  requests[*row_num].parent_url_num = NULL;
  requests[*row_num].hdr_flags = 0;
  requests[*row_num].flags = 0;
  requests[*row_num].pre_url_fname[0] = 0;
  requests[*row_num].post_url_fname[0] = 0;
  return SUCCESS;
}


int proto_based_init(int row_num, int proto)
{
  requests[row_num].request_type = proto;

  if(proto == HTTP_REQUEST || proto == HTTPS_REQUEST) { // HTTP/HTTPS
    if(!(global_settings->protocol_enabled & HTTP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= HTTP_PROTOCOL_ENABLED;
    }
    requests[row_num].proto.http.url_without_path.seg_start = -1;
    requests[row_num].proto.http.url_without_path.num_entries = 0;
    requests[row_num].proto.http.url.seg_start = -1;
    requests[row_num].proto.http.url.num_entries = 0;
    requests[row_num].proto.http.hdrs.seg_start = -1;
    requests[row_num].proto.http.hdrs.num_entries= 0;
    requests[row_num].proto.http.auth_uname.seg_start = -1;
    requests[row_num].proto.http.auth_uname.num_entries = 0;
    requests[row_num].proto.http.auth_pwd.seg_start = -1;
    requests[row_num].proto.http.auth_pwd.num_entries= 0;
    requests[row_num].proto.http.repeat_inline.seg_start = -1;
    requests[row_num].proto.http.repeat_inline.num_entries= 0;
    requests[row_num].proto.http.post_idx = -1;
    requests[row_num].proto.http.cookies.cookie_start = -1;
    requests[row_num].proto.http.cookies.num_cookies = 0;
    requests[row_num].proto.http.dynvars.dynvar_start = -1;
    requests[row_num].proto.http.dynvars.num_dynvars = 0;

   // Following 3 fields are only used in xhttp/RMI based on exact
    requests[row_num].proto.http.url_got_bytes = 0; 
    requests[row_num].proto.http.exact = 0;
    requests[row_num].proto.http.bytes_to_recv = 0; 

    /* Body Encryption */
    requests[row_num].proto.http.body_encryption_args.encryption_algo = -1;
    requests[row_num].proto.http.body_encryption_args.base64_encode_option = -1;
    requests[row_num].proto.http.body_encryption_args.key_size = 0;
    requests[row_num].proto.http.body_encryption_args.ivec_size = 0;
    requests[row_num].proto.http.body_encryption_args.key.seg_start = -1;
    requests[row_num].proto.http.body_encryption_args.key.num_entries = 0;
    requests[row_num].proto.http.body_encryption_args.ivec.seg_start = -1;
    requests[row_num].proto.http.body_encryption_args.ivec.num_entries = 0;

    /* RBU */
    requests[row_num].proto.http.rbu_param.har_rename_flag = RBU_ENABLE_RUNTIME_HAR_RENAMING;
    requests[row_num].proto.http.rbu_param.browser_user_profile = -1;
    requests[row_num].proto.http.rbu_param.har_log_dir = -1;
    requests[row_num].proto.http.rbu_param.vnc_display_id = -1;
    requests[row_num].proto.http.rbu_param.page_load_wait_time = RBU_DEFAULT_PAGE_LOAD_WAIT_TIME;
    requests[row_num].proto.http.rbu_param.merge_har_file = RBU_DEFAULT_MERGE_HAR_FILE;
    requests[row_num].proto.http.rbu_param.primary_content_profile = -1;
    requests[row_num].proto.http.rbu_param.csv_file_name = -1;
    requests[row_num].proto.http.rbu_param.csv_fd = -1;
    requests[row_num].proto.http.rbu_param.performance_trace_mode = 0;  //By default disable performance trace dump
    requests[row_num].proto.http.rbu_param.performance_trace_timeout = RBU_DEFAULT_PERFORMANCE_TRACE_TIMEOUT;
    requests[row_num].proto.http.rbu_param.performance_trace_memory_flag = 1;
    requests[row_num].proto.http.rbu_param.performance_trace_screenshot_flag = 0;
    requests[row_num].proto.http.rbu_param.performance_trace_duration_level = 0;
    requests[row_num].proto.http.rbu_param.auth_username = -1;
    requests[row_num].proto.http.rbu_param.auth_password = -1;

    /* Protbuf */
    requests[row_num].proto.http.protobuf_urlattr.req_message = NULL;
    requests[row_num].proto.http.protobuf_urlattr.req_pb_file = -1;
    requests[row_num].proto.http.protobuf_urlattr.req_pb_msg_type = -1;
    requests[row_num].proto.http.protobuf_urlattr.grpc_comp_type = 0;  //set default comp type zero

    requests[row_num].proto.http.protobuf_urlattr.resp_message = NULL;
    requests[row_num].proto.http.protobuf_urlattr.resp_pb_file = -1;
    requests[row_num].proto.http.protobuf_urlattr.resp_pb_msg_type = -1;


#ifdef RMI_MODE
    requests[row_num].proto.http.bytevars.bytevar_start = -1;
    requests[row_num].proto.http.bytevars.num_bytevars = 0;
    requests[row_num].proto.http.keep_conn_flag = 0;
#endif
  } else if (proto == SMTP_REQUEST || proto == SMTPS_REQUEST) { // SMTP
    if(!(global_settings->protocol_enabled & SMTP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= SMTP_PROTOCOL_ENABLED;
    }
    total_smtp_request_entries++;

    requests[row_num].proto.smtp.user_id.seg_start = -1;
    requests[row_num].proto.smtp.user_id.num_entries= 0;
    
    requests[row_num].proto.smtp.passwd.seg_start = -1;
    requests[row_num].proto.smtp.passwd.num_entries= 0;

    requests[row_num].proto.smtp.to_emails_idx = -1;
    requests[row_num].proto.smtp.cc_emails_idx = -1;
    requests[row_num].proto.smtp.bcc_emails_idx = -1;

    requests[row_num].proto.smtp.from_email.seg_start = -1;
    requests[row_num].proto.smtp.from_email.num_entries= 0;

    requests[row_num].proto.smtp.body_idx = -1;

    requests[row_num].proto.smtp.enable_rand_bytes = 0;
    requests[row_num].proto.smtp.rand_bytes_min = -1;
    requests[row_num].proto.smtp.rand_bytes_max = -1;

/*
    requests[*row_num].proto.smtp.subject_idx.seg_start = -1;
    requests[*row_num].proto.smtp.subject_idx.num_entries= 0;
    requests[*row_num].proto.smtp.msg_count.seg_start = -1;
    requests[*row_num].proto.smtp.msg_count.num_entries= 0;
*/

    requests[row_num].proto.smtp.msg_count_min = 1;
    requests[row_num].proto.smtp.msg_count_max = 1;

    requests[row_num].proto.smtp.hdrs.seg_start = -1;
    requests[row_num].proto.smtp.hdrs.num_entries= 0;

    requests[row_num].proto.smtp.num_attachments = 0;
    requests[row_num].proto.smtp.attachment_idx = -1;
  } else if (proto == POP3_REQUEST || proto == SPOP3_REQUEST) { // POP3
    if( (proto == POP3_REQUEST || proto == SPOP3_REQUEST) && !(global_settings->protocol_enabled & POP3_PROTOCOL_ENABLED)) {
       global_settings->protocol_enabled |= POP3_PROTOCOL_ENABLED;
    }
    total_pop3_request_entries++;

    requests[row_num].proto.pop3.user_id.seg_start = -1;
    requests[row_num].proto.pop3.user_id.num_entries= 0;
    
    requests[row_num].proto.pop3.passwd.seg_start = -1;
    requests[row_num].proto.pop3.passwd.num_entries= 0;
  } else if (proto == FTP_REQUEST) { // ftp
    if(!(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= FTP_PROTOCOL_ENABLED;
    }
    total_ftp_request_entries++;
    requests[row_num].proto.ftp.user_id.seg_start = -1;
    requests[row_num].proto.ftp.user_id.num_entries = 0;

    requests[row_num].proto.ftp.ftp_cmd.seg_start = -1;
    requests[row_num].proto.ftp.user_id.num_entries= 0;
    
    requests[row_num].proto.ftp.passwd.seg_start = -1;
    requests[row_num].proto.ftp.passwd.num_entries= 0;

    requests[row_num].proto.ftp.get_files_idx = -1;
    requests[row_num].proto.ftp.num_get_files = 0;
  } else if (proto == JRMI_REQUEST) { // JRMI
    if(!(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= JRMI_PROTOCOL_ENABLED;
    }
    total_jrmi_request_entries++;
    NSDL2_MISC(NULL, NULL, "total_request_entries = %d",total_jrmi_request_entries);
    requests[row_num].proto.jrmi.jrmi_protocol = 1;

    requests[row_num].proto.jrmi.post_idx = -1;
    requests[row_num].proto.jrmi.no_param = 0;
   // requests[row_num].proto.jrmi.method.seg_start = -1;
   // requests[row_num].proto.jrmi.method.num_entries = 0;

   requests[row_num].proto.jrmi.object_id.seg_start = -1;
   requests[row_num].proto.jrmi.object_id.num_entries = 0;

   requests[row_num].proto.jrmi.number.seg_start = -1;
   requests[row_num].proto.jrmi.number.num_entries = 0;

   requests[row_num].proto.jrmi.time.seg_start = -1;
   requests[row_num].proto.jrmi.time.num_entries = 0;
    
   requests[row_num].proto.jrmi.count.seg_start = -1;
   requests[row_num].proto.jrmi.count.num_entries = 0;
 
   requests[row_num].proto.jrmi.method_hash.seg_start = -1;
   requests[row_num].proto.jrmi.method_hash.num_entries = 0;

   requests[row_num].proto.jrmi.operation.seg_start = -1;
   requests[row_num].proto.jrmi.operation.num_entries = 0;
    
 }else if (proto == DNS_REQUEST) { // dns
    if(!(global_settings->protocol_enabled & DNS_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= DNS_PROTOCOL_ENABLED;
    }
    total_dns_request_entries++;

    requests[row_num].proto.dns.name.seg_start = -1;
    requests[row_num].proto.dns.name.num_entries= 0;
    
    requests[row_num].proto.dns.qtype = DNS_DEFAULT_QUERY_TYPE;
    requests[row_num].proto.dns.recursive  = DNS_DEFAULT_RECURSION;
    requests[row_num].proto.dns.proto  = USE_DNS_ON_UDP;

    requests[row_num].proto.dns.assert_rr_type.seg_start = -1;
    requests[row_num].proto.dns.assert_rr_type.num_entries= 0;

    requests[row_num].proto.dns.assert_rr_data.seg_start = -1;
    requests[row_num].proto.dns.assert_rr_data.num_entries= 0;
  } else if (proto == IMAP_REQUEST || proto == IMAPS_REQUEST) { // IMAP
    if(((proto == IMAP_REQUEST) || (proto == IMAPS_REQUEST)) && !(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= IMAP_PROTOCOL_ENABLED;
    }
    total_imap_request_entries++;

    requests[row_num].proto.imap.authentication_type = 0;

    requests[row_num].proto.imap.user_id.seg_start = -1;
    requests[row_num].proto.imap.user_id.num_entries= 0;
    
    requests[row_num].proto.imap.passwd.seg_start = -1;
    requests[row_num].proto.imap.passwd.num_entries= 0;

    requests[row_num].proto.imap.mail_seq.seg_start = -1;
    requests[row_num].proto.imap.mail_seq.num_entries= 0;

    requests[row_num].proto.imap.fetch_part.seg_start = -1;
    requests[row_num].proto.imap.fetch_part.num_entries= 0;
  } else if (proto == WS_REQUEST || proto == WSS_REQUEST) {                      //websocket
    if(!(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= WS_PROTOCOL_ENABLED;
    }

    requests[row_num].proto.ws.uri_without_path.seg_start = -1;
    requests[row_num].proto.ws.uri_without_path.num_entries = 0;
    total_ws_request_entries++;
    NSDL2_MISC(NULL, NULL, "total_request_entries = %d", total_ws_request_entries);

    requests[row_num].proto.ws.origin       = -1;
    requests[row_num].proto.ws.conn_id      = -1;
    requests[row_num].proto.ws.opencb_idx   = -1;
    requests[row_num].proto.ws.sendcb_idx   = -1;
    requests[row_num].proto.ws.msgcb_idx    = -1;
    requests[row_num].proto.ws.errorcb_idx  = -1;

    requests[row_num].proto.ws.uri.seg_start = -1;
    requests[row_num].proto.ws.uri.num_entries = 0;
   
    requests[row_num].proto.ws.hdrs.seg_start = -1;
    requests[row_num].proto.ws.hdrs.num_entries = 0;

  } else if (proto == XMPP_REQUEST ) {                      //xmpp
    if(!(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= XMPP_PROTOCOL_ENABLED;
    }
    total_xmpp_request_entries++;
    NSDL2_MISC(NULL, NULL, "total_request_entries = %d", total_xmpp_request_entries);
    requests[row_num].proto.xmpp.action = -1;
    requests[row_num].proto.xmpp.starttls = -1;
    requests[row_num].proto.xmpp.user_type = -1;
    requests[row_num].proto.xmpp.accept_contact = 1; //Default value
  
    requests[row_num].proto.xmpp.user.seg_start = -1;
    requests[row_num].proto.xmpp.user.num_entries = 0;

    requests[row_num].proto.xmpp.domain.seg_start = -1;
    requests[row_num].proto.xmpp.domain.num_entries = 0;

    requests[row_num].proto.xmpp.sasl_auth_type.seg_start = -1;
    requests[row_num].proto.xmpp.sasl_auth_type.num_entries = 0;

    requests[row_num].proto.xmpp.password.seg_start = -1;
    requests[row_num].proto.xmpp.password.num_entries = 0;


    requests[row_num].proto.xmpp.message.seg_start = -1;
    requests[row_num].proto.xmpp.message.num_entries = 0;

    requests[row_num].proto.xmpp.file.seg_start = -1;
    requests[row_num].proto.xmpp.file.num_entries = 0;

    requests[row_num].proto.xmpp.group.seg_start = -1;
    requests[row_num].proto.xmpp.group.num_entries = 0;

  } else if (proto == FC2_REQUEST ) {                      //fc2
    if(!(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED)) 
        global_settings->protocol_enabled |= FC2_PROTOCOL_ENABLED; 

    total_fc2_request_entries++;
    NSDL2_MISC(NULL, NULL, "total_fc2_request_entries = %d", total_fc2_request_entries);
    
    requests[row_num].proto.fc2_req.uri.seg_start = -1;
    requests[row_num].proto.fc2_req.uri.num_entries = 0;

    requests[row_num].proto.fc2_req.message.seg_start = -1;
    requests[row_num].proto.fc2_req.message.num_entries = 0;

  }

  return (SUCCESS);
}

int create_post_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_post_entries == max_post_entries) {
    MY_REALLOC_EX (postTable, (max_post_entries + DELTA_POST_ENTRIES) * sizeof(StrEnt), max_post_entries * sizeof(StrEnt), "postTable", -1);
    if (!postTable) {
      fprintf(stderr, "create_post_table_entry(): Error allocating more memory for post entries\n");
      return(FAILURE);
    } else max_post_entries += DELTA_POST_ENTRIES;
  }
  *row_num = total_post_entries++;
  postTable[*row_num].seg_start = -1;
  postTable[*row_num].num_entries = 0;
  return (SUCCESS);
}

int create_host_table_entry(int *row_num) {
  NSDL2_HTTP(NULL, NULL, "Method called");
  if (total_host_entries == max_host_entries) {
    MY_REALLOC_EX (hostTable, (max_host_entries + DELTA_HOST_ENTRIES) * sizeof(HostElement), max_host_entries * sizeof(HostElement), "hostTable", -1);
    if (!hostTable) {
      fprintf(stderr,"create_host_table_entry(): Error allocating more memory for host entries\n");
      return(FAILURE);
    } else max_host_entries += DELTA_HOST_ENTRIES;
  }
  *row_num = total_host_entries++;
  hostTable[*row_num].next = -1;
  return (SUCCESS);
}

int create_report_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_report_entries == max_report_entries) {
    MY_REALLOC_EX (reportTable, (max_report_entries + DELTA_REPORT_ENTRIES) * sizeof(avgtime), max_report_entries * sizeof(avgtime), "reportTable", -1);
    if (!reportTable) {
      fprintf(stderr,"create_report_table_entry(): Error allocating more memory for report entries\n");
      return(FAILURE);
    } else max_report_entries += DELTA_REPORT_ENTRIES;
  }
  *row_num = total_report_entries++;
  return (SUCCESS);
}

int create_dynvar_table_entry(int *row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_dynvar_entries == max_dynvar_entries) {
    MY_REALLOC_EX (dynVarTable, (max_dynvar_entries + DELTA_DYNVAR_ENTRIES) * sizeof(DynVarTableEntry), max_dynvar_entries * sizeof(DynVarTableEntry), "dynVarTable", -1);
    if (!dynVarTable) {
      fprintf(stderr,"create_dynvar_table_entry(): Error allocating more memory for dynvar entries\n");
      return(FAILURE);
    } else max_dynvar_entries += DELTA_DYNVAR_ENTRIES;
  }
  *row_num = total_dynvar_entries++;
  return (SUCCESS);
}

int create_reqdynvar_table_entry(int *row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_reqdynvar_entries == max_reqdynvar_entries) {
    MY_REALLOC_EX (reqDynVarTable, (max_reqdynvar_entries + DELTA_REQDYNVAR_ENTRIES) * sizeof(ReqDynVarTableEntry), max_reqdynvar_entries * sizeof(ReqDynVarTableEntry), "reqDynVarTable", -1);
    if (!reqDynVarTable) {
      fprintf(stderr,"create_reqdynvar_table_entry(): Error allocating more memory for reqdynvar entries\n");
      return(FAILURE);
    } else max_reqdynvar_entries += DELTA_REQDYNVAR_ENTRIES;
  }
  *row_num = total_reqdynvar_entries++;
  return (SUCCESS);
}

int create_seg_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_seg_entries == max_seg_entries) {
    MY_REALLOC_EX (segTable, (max_seg_entries + DELTA_SEG_ENTRIES) * sizeof(SegTableEntry), max_seg_entries * sizeof(SegTableEntry), "segTable", -1);
    if (!segTable) { //Can be repeated
      fprintf(stderr,"create_seg_table_entry(): Error allocating more memory for seg entries\n");
      return(FAILURE);
    } else max_seg_entries += DELTA_SEG_ENTRIES;
  }
  *row_num = total_seg_entries++;
  segTable[*row_num].type = 0;
  NSDL2_MISC(NULL, NULL, "*****row_num = %d", *row_num);
  return (SUCCESS);
}


/*int create_clickawayprof_table_entry(int *row_num) {
  if (total_clickawayprof_entries == max_clickawayprof_entries) {
    clickawayProfTable = (ClickawayProfTableEntry *) realloc ((char*)clickawayProfTable,
						      (max_clickawayprof_entries + DELTA_CLICKAWAYPROF_ENTRIES) *
						      sizeof(ClickawayProfTableEntry));
    if (!clickawayProfTable) {
      fprintf(stderr, "create_clickawayprof_table_entry(): Error allcating more memory for clickawayprof entries\n");
      return FAILURE;
    } else max_clickawayprof_entries += DELTA_CLICKAWAYPROF_ENTRIES;
  }
  *row_num = total_clickawayprof_entries++;
  return (SUCCESS);
  }*/

#ifdef RMI_MODE
int create_bytevar_table_entry(int *row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_bytevar_entries == max_bytevar_entries) {
    MY_REALLOC_EX (byteVarTable, (max_bytevar_entries + DELTA_BYTEVAR_ENTRIES) * sizeof(ByteVarTableEntry), max_bytevar_entries * sizeof(ByteVarTableEntry), "byteVarTable", -1);
    if (!byteVarTable) {
      fprintf(stderr,"create_bytevar_table_entry(): Error allocating more memory for bytevar entries\n");
      return(FAILURE);
    } else max_bytevar_entries += DELTA_BYTEVAR_ENTRIES;
  }
  *row_num = total_bytevar_entries++;
  return (SUCCESS);
}

int create_reqbytevar_table_entry(int *row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_reqbytevar_entries == max_reqbytevar_entries) {
    MY_REALLOC_EX (reqByteVarTable, (max_reqbytevar_entries + DELTA_REQBYTEVAR_ENTRIES) * sizeof(ReqByteVarTableEntry), max_reqbytevar_entries * sizeof(ReqByteVarTableEntry), "reqByteVarTable", -1);
    if (!reqByteVarTable) {
      fprintf(stderr,"create_reqbytevar_table_entry(): Error allocating more memory for reqbytevar entries\n");
      return(FAILURE);
    } else max_reqbytevar_entries += DELTA_REQBYTEVAR_ENTRIES;
  }
  *row_num = total_reqbytevar_entries++;
  return (SUCCESS);
}
#endif

int create_clustervar_table_entry(int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_clustvar_entries == max_clustvar_entries) {
    MY_REALLOC_EX (clustVarTable, (max_clustvar_entries + DELTA_CLUSTVAR_ENTRIES) * sizeof(ClustVarTableEntry), max_clustvar_entries * sizeof(ClustVarTableEntry), "clustVarTable", -1);
    if (!clustVarTable) {
      fprintf(stderr, "create_clustervar_table_entry(): Error allocating more memory for clustvartable entries\n");
      return (FAILURE);
    } else max_clustvar_entries += DELTA_CLUSTVAR_ENTRIES;
  }
  *row_num = total_clustvar_entries++;
  return (SUCCESS);
}

int create_clusterval_table_entry(int* row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_clustval_entries == max_clustval_entries) {
    MY_REALLOC_EX (clustValTable, (max_clustval_entries + DELTA_CLUSTVAL_ENTRIES) * sizeof(ClustValTableEntry), max_clustval_entries * sizeof(ClustValTableEntry), "clustValTable", -1);
    if (!clustValTable) {
      fprintf(stderr, "create_clusterval_table_entry(): Error allocating more memory for clustvaltable entries\n");
      return (FAILURE);
    } else max_clustval_entries += DELTA_CLUSTVAL_ENTRIES;
  }
  *row_num = total_clustval_entries++;
  return (SUCCESS);
}

int create_groupvar_table_entry(int* row_num) {
  NSDL2_VARS(NULL, NULL, "Method called");
  if (total_groupvar_entries == max_groupvar_entries) {
    MY_REALLOC_EX (groupVarTable, (max_groupvar_entries + DELTA_GROUPVAR_ENTRIES) * sizeof(GroupVarTableEntry), max_groupvar_entries * sizeof(GroupVarTableEntry),"groupVarTable", -1);
    if (!groupVarTable) {
      fprintf(stderr, "create_groupvar_table_entry(): Error allocating more memory for groupvartable entries\n");
      return (FAILURE);
    } else max_groupvar_entries += DELTA_GROUPVAR_ENTRIES;
  }
  *row_num = total_groupvar_entries++;
  return (SUCCESS);
}

int create_groupval_table_entry(int* row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_groupval_entries == max_groupval_entries) {
    MY_REALLOC_EX (groupValTable, (max_groupval_entries + DELTA_GROUPVAL_ENTRIES) * sizeof(GroupValTableEntry), max_groupval_entries * sizeof(GroupValTableEntry), "groupValTable", -1);
    if (!groupValTable) {
      fprintf(stderr, "create_groupval_table_entry(): Error allocating more memory for groupvaltable entries\n");
      return (FAILURE);
    } else max_groupval_entries += DELTA_GROUPVAL_ENTRIES;
  }
  *row_num = total_groupval_entries++;
  return (SUCCESS);
}

int create_repeat_block_table_entry(int* row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_repeat_block_entries == max_repeat_block_entries) {
    MY_REALLOC_EX (repeatBlock, (max_repeat_block_entries + DELTA_REPEAT_BLOCK_ENTRIES) * sizeof(repeatBlock), max_repeat_block_entries * sizeof(RepeatBlock), "repeatBlock", -1);
    if (!repeatBlock) {
      fprintf(stderr, "create_repeat_block_entry(): Error allocating more memory for repeatBlock entries\n");
      return (FAILURE);
    } else max_repeat_block_entries += DELTA_REPEAT_BLOCK_ENTRIES;
  }
  *row_num = total_repeat_block_entries++;
  return (SUCCESS);
}

int create_add_header_table_entry(int *rnum)
{

  NSDL2_PARSING (NULL, NULL, "Method called");
  
  if (total_hdr_entries == max_hdr_entries)
  {
    MY_REALLOC_EX (addHeaderTable, (max_hdr_entries + DELTA_ADD_HEADER_ENTRIES) * sizeof(AddHeaderTableEntry), 
                  (max_hdr_entries * sizeof(AddHeaderTableEntry)), "addHeaderTable", -1); // Added old size of table

    if (!addHeaderTable) {
      fprintf(stderr, "create_add_header_table_entry(): Error allcating more memory for addHeader entries\n");
      return FAILURE;
    } else max_hdr_entries += DELTA_ADD_HEADER_ENTRIES;
  }
  *rnum = total_hdr_entries++;
  return (SUCCESS);

}
int create_big_buf_space(void) {
  char* old_big_buf_ptr = g_big_buf;

  NSDL3_MISC(NULL, NULL, "Method called. used_buffer_space = %d, max_buffer_space = %d, g_big_buf = %s, g_buf_ptr = %p", used_buffer_space, max_buffer_space, g_big_buf, g_big_buf);
  MY_REALLOC_EX (g_big_buf, max_buffer_space + DELTA_BIGBUFFER, max_buffer_space,"g_big_buf", -1);
  if (!g_big_buf){
    fprintf(stderr, "create_big_buf_space(): Error allocating more memory for g_big_buf\n");
    return(FAILURE);
  } else {
    if (old_big_buf_ptr != g_big_buf)
      g_buf_ptr = g_big_buf + used_buffer_space;
    max_buffer_space += DELTA_BIGBUFFER;
  }
  return (SUCCESS);
}

static int enough_memory(int space) {
  NSDL3_MISC(NULL, NULL, "Method called. space = %d, used_buffer_space = %d, max_buffer_space = %d, g_big_buf = %s, g_buf_ptr = %p", space, used_buffer_space, max_buffer_space, g_big_buf, g_big_buf);

  return (g_buf_ptr+space < g_big_buf+max_buffer_space);
}

ns_bigbuf_t
  copy_into_big_buf(char* data, ns_bigbuf_t size) {
  ns_bigbuf_t data_loc = used_buffer_space; // Index in the big buffer for this data to be copied

  NSDL3_MISC(NULL, NULL, "Method called. size = %d, used_buffer_space = %d, max_buffer_space = %d, g_big_buf = %s, g_buf_ptr = %p", 
                          size, used_buffer_space, max_buffer_space, g_big_buf, g_big_buf);

  if (size == 0)
  {
    size = strlen(data);
    NSDL4_MISC(NULL, NULL, "Size of data = %d, data = %s", size, data);
  }

  while (!enough_memory(size+1))
    if (create_big_buf_space() != SUCCESS)
      return -1;

  memcpy(g_buf_ptr, data, size);
  g_buf_ptr[size] = '\0';

  g_buf_ptr += size +1;
  used_buffer_space += size +1;
  return data_loc;
}



int find_sessprofindex_idx(char* name) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_sessprofindex_entries; i++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(sessProfIndexTable[i].name), name, strlen(name)))
      return i;

  return -1;
}

//Checking for script name along with project subproject. 
//If full name matched, return -1
//If full name not matched, session id will be provided 
int find_session_idx(char* name) {
  int i;
  char sess_name_with_proj_subproj[2048];

  NSDL2_SCHEDULE(NULL, NULL, "Method called, name = %s, total_sess_entries = %d", name, total_sess_entries);
  
  for (i = 0; i < total_sess_entries; i++)
  {
    snprintf(sess_name_with_proj_subproj, 2048, "%s/%s/%s", RETRIEVE_BUFFER_DATA(gSessionTable[i].proj_name), RETRIEVE_BUFFER_DATA(gSessionTable[i].sub_proj_name), RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name));

    //if (!strcmp(RETRIEVE_BUFFER_DATA(gSessionTable[i].sess_name), name))          //Before we were doing this

    if (!strcmp(sess_name_with_proj_subproj, name))
      return i;
  }
  return -1; 
}

int find_runindex_idx(char* name) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_runindex_entries; i++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(runIndexTable[i].name), name, strlen(name)))
      return i;

  return -1;
}

int find_dynvar_idx(char* name) {
  int i;

  NSDL2_VARS(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_dynvar_entries; i++)
    if (!strcmp(RETRIEVE_BUFFER_DATA(dynVarTable[i].name), name))
      return i;

  return -1;
}

/*int find_thinkprof_idx(char* name) {
  int i;

  for (i = 0; i < total_thinkprof_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(thinkProfTable[i].name), name))
      return i;
  }

  return -1;
  }*/

int find_errorcode_idx(int error_code) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, error_code = %d", error_code);
  for (i = 0; i < total_errorcode_entries; i++) {
    if (errorCodeTable[i].error_code == error_code)
      return i;
  }

  return -1;
}

#ifdef RMI_MODE
int find_bytevar_idx(char* name, int length) {
  int i;
  NSDL2_MISC(NULL, NULL, "Method called, name = %s, length = %d", name, length);
  for (i = 0; i < total_bytevar_entries; i++)
    if (!strncmp(RETRIEVE_BUFFER_DATA(byteVarTable[i].name), name, length))
      return i;

  return -1;
}
#endif

int find_page_idx_shr(char* name, SessTableEntry_Shr* sess_ptr) {
  int i;
  PageTableEntry_Shr *page_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, name = %s, sess_ptr = %p", name, sess_ptr);
  page_ptr = sess_ptr->first_page;
  
  for (i = 0; i < sess_ptr->num_pages; i++) {
    if (!strcmp(page_ptr->page_name, name))
      return i;
    page_ptr++;
  }

  return -1;
}

int find_page_idx(char* name, int sess_idx) {
  int i;
  int first_page;
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called, page_name = %s, sess_idx = %d, sess_name = %s", name, sess_idx, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

  first_page = gSessionTable[sess_idx].first_page;

  NSDL2_SCHEDULE(NULL, NULL, "first_page = %d, num_pages = %d", first_page, gSessionTable[sess_idx].num_pages);
  for (i = first_page; i < first_page + gSessionTable[sess_idx].num_pages; i++) {
    NSDL3_SCHEDULE(NULL, NULL, "Checking session table. page_name = %s, page_idx = %d", RETRIEVE_BUFFER_DATA(gPageTable[i].page_name), i);
    if (!strcmp(RETRIEVE_BUFFER_DATA(gPageTable[i].page_name), name))
      return i;
  }
  
  return -1;
}

int find_tagvar_idx(char* name, int len, int sess_idx) {
  int i;
  if(!len)
    len = strlen(name);
  char save_chr = name[len];
  name[len] = 0;

  NSDL2_VARS(NULL, NULL, "Method called, name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].tag_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].tag_start_idx; i < gSessionTable[sess_idx].tag_start_idx + gSessionTable[sess_idx].num_tag_entries; i++) {
    if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(tagTable[i].name), name)){
      name[len] = save_chr;      
      return i;
    }
  }
  name[len] = save_chr;  
  return -1;
}

int find_nslvar_idx(char* name, int len, int sess_idx) {
  int i;
  
  if(!len)
    len = strlen(name);
  char save_chr = name[len];
  name[len] = 0;
  
  NSDL2_VARS(NULL, NULL, "Method called, name = %s, sess_idx = %d", name, sess_idx);
  if (gSessionTable[sess_idx].nslvar_start_idx == -1)
    return -1;

  for (i = gSessionTable[sess_idx].nslvar_start_idx; i < gSessionTable[sess_idx].nslvar_start_idx + gSessionTable[sess_idx].num_nslvar_entries; i++) {
    if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[i].name), name)){
      name[len] = save_chr;
      return i;
    }
  }
  name[len] = save_chr;
  return -1;
}

int find_clustvar_idx(char* name) {
  int i;

  NSDL2_VARS(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_clustvar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(clustVarTable[i].name), name))
      return i;
  }

  return -1;
}

int find_groupvar_idx(char* name) {
  int i;

  NSDL2_VARS(NULL, NULL, "Method called, name = %s", name);
  for (i = 0; i < total_groupvar_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(groupVarTable[i].name), name))
      return i;
  }

  return -1;
}

int find_sg_idx_shr(char* scengrp_name) {
  int i;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, scengrp_name = %s", scengrp_name);
  for (i = 0; i < total_runprof_entries; i++) {
    if (!strcmp(runprof_table_shr_mem[i].scen_group_name, scengrp_name))
      return i;
  }

  return -1;
}

/* Modified function for schedule parsing for SGRP group having quantity or pct
 * zero*/
int find_sg_idx(char* scengrp_name)
{
  int i;
  NSDL2_SCHEDULE(NULL, NULL, "Method called, scengrp_name = %s", scengrp_name);

  /* Group name ALL */
  if (strcasecmp(scengrp_name, "ALL") == 0) {
    NSDL2_SCHEDULE(NULL, NULL, "SGRP keyword received with group name ALL");
    return NS_GRP_IS_ALL;
  }
  /* Search group name*/
  for (i = 0; i < total_runprof_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name), scengrp_name))
      return i; //SGRP name found
  }
  return NS_GRP_IS_INVALID; //Invalid group name
}

#if 0
void *
//do_shmget(key_t key, int size, int shmflg)
do_shmget(int size, char *msg)
{
int shmid;
void *addr;

  if (size == 0) return NULL;
  shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | 0666);
  if (shmid == -1) {
	printf ("ERROR: unable to allocate shm for '%s' of size=%d err=%s\n", msg, size, nslb_strerror(errno));
	exit (-1);
  }
  addr = shmat(shmid, NULL, 0);
  if (addr == (void *) -1) {
	printf ("ERROR: unable to attach shm for '%s' of size=%d err=%s\n", msg, size, nslb_strerror(errno));
	exit (-1);
  }
  //Mark shm for auto-deletion on exit
  if (shmctl (shmid, IPC_RMID, NULL)) {
	printf ("ERROR: unable to mark shm removal for '%s' of size=%d err=%s\n", msg, size, nslb_strerror(errno));
	exit (-1);
   }
  return addr;
}

#endif

// Abhishek (12/19/06) - change function for input argument for time in HH:MM:SS
char* get_time_in_hhmmss(int seconds) {
  NSDL2_MISC(NULL, NULL, "Method called, seconds = %d", seconds);
  int hours = (seconds)/3600;
  int minute = (seconds%3600)/60;
  int second = (seconds%3600)%60;
  static char buf[25];
  // Currently it is supporting max 2 digit days (upto 99)
  sprintf(buf, "%2.2d:%2.2d:%2.2d", hours, minute, second);
  return buf;
}

/* checks if USER data is good and also fills in CLIENT data--num_connections and num_fetches */
int
input_group_values(char* file_name) {
  FILE* group_val_fp;
  char line[MAX_LINE_LENGTH];
  char group_name[MAX_LINE_LENGTH];
  int line_number = 0;
  int num_values = 0;
  int i, j;
  int var_idx;
  int val_idx;
  int first_tok;
  char* tok;
  //int group_id;
  int groupval_idx;
  int first_line = 1;
  int get_fs = 0;
  char* line_ptr;
  char file_sep[2];

  NSDL2_MISC(NULL, NULL, "Method called, file_name = %s", file_name);
  strcpy(file_sep, ",");
  if ((group_val_fp = fopen(file_name, "r")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", file_name);
    perror("fopen");
    return -1;
  }

  /*first need to find out the number of total values per variable to preallocate the space on the tables */
  //num_values would have the total number of values
  while (fgets(line, MAX_LINE_LENGTH, group_val_fp)) {
    if (first_line) {
      if (!strncmp(line, "FS=", strlen("FS="))) {
      	get_fs = 1;
      	continue;
      }
      first_line = 0;
    }
    num_values++;
  }

  //make sure total values is equal to scenrio groups
  if (num_values < total_runprof_entries) {
    fprintf(stderr, "Number of Sgroup var values in file %s is %d. It must be same alteast number of Scenrio groups (%d)\n",
		file_name, num_values, total_runprof_entries);
    return -1;
  }

  for (i = 0; i < total_groupvar_entries; i++) {
    for (j = 0; j < num_values; j++) {
      if (create_groupval_table_entry(&groupval_idx) == -1) {
	fprintf(stderr, "Error in creating a group value entry\n");
	return -1;
      }
      if (j == 0) {
	groupVarTable[i].start = groupval_idx;
	groupVarTable[i].length = num_values;
      }
    }
  }

  rewind(group_val_fp);

  val_idx = 0;

  while (fgets(line, MAX_LINE_LENGTH, group_val_fp)) {
    if (strchr(line, '\n'))
      *(strchr(line, '\n')) = '\0';

    line_number++;

    if (get_fs) {
      line_ptr = line;
      line_ptr += strlen("FS=");
      if (strlen(line_ptr) != 1) {
	fprintf(stderr, "input_group_values(): In line %d inf file %s, file seperator must be one characater\n", line_number, file_name);
	return -1;
      }
      file_sep[0] = *line_ptr;
      get_fs = 0;
      continue;
    }

    first_tok = 1;
    var_idx = 0;

    for (tok = strtok(line, file_sep); tok; tok = strtok(NULL, file_sep)) {
      if (first_tok) {

	if (tok == NULL) {
	  fprintf(stderr, "input_group_values(): In line %d in file %s, invalid value format\n", line_number, file_name);
	  return -1;
	}

	strcpy(group_name, tok);
	first_tok = 0;
      } else {

	groupval_idx = groupVarTable[var_idx].start + val_idx;

	if ((groupValTable[groupval_idx].value = copy_into_big_buf(tok, 0)) == -1) {
	  NS_EXIT(-1, "input_group_values(): Failed to copy data into big buf");
	}

    	if ((groupValTable[groupval_idx].group_id = find_sg_idx(group_name)) == -1) {
      	    fprintf(stderr, "input_group_values():group_name '%s' used in group value file is not defined as a scenrio group\n", group_name);
      	    return -1;
    	}

	var_idx++;
      }
    }
    if (var_idx != total_groupvar_entries) {
      	    fprintf(stderr, "input_group_values():group_name %s in group value file does not have %d values", group_name, total_groupvar_entries);
      	    return -1;
    }
    val_idx++;
  }

  return 0;
}

int find_clust_idx(char * cluster_id) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, cluster_id = %s", cluster_id);
  for (i = 0; i < total_clust_entries; i++) {
    if (!strcmp(RETRIEVE_BUFFER_DATA(clustTable[i].cluster_id), cluster_id))
      return i;
  }

  return -1;
}


int
input_clust_values(char* file_name)
{
  FILE* clust_val_fp;
  char line[MAX_LINE_LENGTH];
  int line_number = 0;
  int num_values = 0;
  int i, j;
  int var_idx;
  int val_idx;
  int first_tok;
  char* tok;
  //int cluster_id;
  char * cluster_id;
  int cluster_idx;
  int clusterval_idx;
  int first_line = 1;
  int get_fs = 0;
  char* line_ptr;
  char file_sep[2];
  int discard_entry = 0;

  NSDL2_MISC(NULL, NULL, "Method called, file_name = %s", file_name);
  strcpy(file_sep, ",");
  if ((clust_val_fp = fopen(file_name, "r")) == NULL) {
    fprintf(stderr, "Error in opening file %s\n", file_name);
    perror("fopen");
    return -1;
  }

  /*first need to find out the number of total values per variable to preallocate the space on the tables */
  //num_values would have the total number of values
  while (fgets(line, MAX_LINE_LENGTH, clust_val_fp)) {
    if (first_line) {
      if (!strncmp(line, "FS=", strlen("FS="))) {
      	get_fs = 1;
      	continue;
      }
      first_line = 0;
    }
    num_values++;
  }


  for (i = 0; i < total_clustvar_entries; i++) {
    for (j = 0; j < num_values; j++) {
      if (create_clusterval_table_entry(&clusterval_idx) == -1) {
	fprintf(stderr, "Error in creating a cluster value entry\n");
	return -1;
      }
      if (j == 0) {
	clustVarTable[i].start = clusterval_idx;
	clustVarTable[i].length = num_values;
      }
    }
  }

  rewind(clust_val_fp);

  val_idx = 0;
  discard_entry=0;
 //There is one line for each set of values
 //And one option line for FS= for field seperator
  while (fgets(line, MAX_LINE_LENGTH, clust_val_fp)) {
    if (strchr(line, '\n'))
      *(strchr(line, '\n')) = '\0';

    line_number++;
    discard_entry = 0;

    //Field seperator
    if (get_fs) {
      line_ptr = line;
      line_ptr += strlen("FS=");
      if (strlen(line_ptr) != 1) {
	fprintf(stderr, "input_clust_values(): In line %d inf file %s, file seperator must be one characater\n", line_number, file_name);
	return -1;
      }
      file_sep[0] = *line_ptr;
      get_fs = 0;
      continue;
    }

    //First field is cluster_id
    first_tok = 1;
    var_idx = 0;

    for (tok = strtok(line, file_sep); tok; tok = strtok(NULL, file_sep)) {
      if (first_tok) {

	if (tok == NULL) {
	  fprintf(stderr, "input_clust_values(): In line %d in file %s, invalid value format\n", line_number, file_name);
	  return -1;
	}

#if 0
	for (j = 0; j < strlen(tok); j++) {
	  if (!isdigit(tok[j])) {
	    fprintf(stderr, "input_clust_values(): Error in parsing variable file at line %d in file %s, need a cluster id\n", line_number, file_name);
	    return -1;
	  }
	}

	cluster_id = atoi(tok);
#endif
	cluster_id = tok;
        if ((cluster_idx = find_clust_idx(cluster_id)) == -1) {
            // DL_ISSUE
#if 0
	    if (global_settings->debug > 3)
      	        fprintf(stderr, "input_cluster_values():Ignoring Values for  cluster_name '%s' used in cluster value file is not defined as a cluster_id for any scenrio group\n", cluster_id);
#endif
	    discard_entry = 1;
	    break;
    	}
	first_tok = 0;
      } else {

	clusterval_idx = clustVarTable[var_idx].start + val_idx;
        clustValTable[clusterval_idx].cluster_id = cluster_idx;

	if ((clustValTable[clusterval_idx].value = copy_into_big_buf(tok, 0)) == -1) {
	  NS_EXIT(-1, "input_clust_values(): Failed to copy data into big buf");
	}


	var_idx++;
      }
    }
    if (discard_entry)
	continue;
    //make sure required number of cluster variables are present
    if (var_idx != total_clustvar_entries) {
      	    fprintf(stderr, "input_group_values():cluster-id  %s in cluster value file does not have %d values", cluster_id, total_clustvar_entries);
      	    return -1;
    }

    val_idx++;
  }

  //make sure total values is equal to clusters
  if (val_idx != total_clust_entries) {
    fprintf(stderr, "Number of cluster var values in file %s is %d. It must be atleast  number of total clusters (%d)\n",
		file_name, val_idx, total_clust_entries);
    return -1;
  }

  //Reset the the number of values to actual values relating to valid clusters
  for (i = 0; i < total_clustvar_entries; i++) {
	clustVarTable[i].length = val_idx;
  }

  rewind(clust_val_fp);

  return 0;
}


//Check given script is url based script
int is_script_url_based(char *script_name)
{
		NSDL2_HTTP(NULL, NULL, "Method called, script_name = %s", script_name);
		if(!strncmp(URL_BASED_SCRIPT_PRIFIX, script_name, strlen(URL_BASED_SCRIPT_PRIFIX)))
		return 0;
	else
		return 1;
}

#if 0
/*This function is to save last data file
 * in Mode=USE_ONCe in static variables.*/
int save_last_data_file ()
{
  
  FILE *fp_last = NULL;
  char buf[1024 + 1] = "\0";
  char last_file[1024] = "\0";
  int i, j;
  int to_write = 0;
  char locl_data_fname[5 * 1024];
  char locl_data_fname2[5 * 1024];

  int total_nvm = global_settings->num_process;

  for (i = 0; i < total_group_entries; i++) {
    buf[0] = '\0';
    locl_data_fname[0] = '\0';
    locl_data_fname2[0] = '\0';

    strcpy(locl_data_fname, group_table_shr_mem[i].data_fname);
    strcpy(locl_data_fname2, group_table_shr_mem[i].data_fname);

    sprintf(last_file, "%s/.%s.last", dirname(locl_data_fname), basename(locl_data_fname2));
    
    fp_last = fopen(last_file, "w");

    if(fp_last == NULL) {
      fprintf(stderr, "Error: Unable to open last file '%s' for writing.\n", last_file);
      return 1;
    }
    chmod(last_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    NSDL2_VARS(NULL,NULL, "Last file  = %s.\n", last_file);

    for(j = 0; j < total_nvm; j++) {
      sprintf(buf, "%s%d,%d|", buf, per_proc_vgroup_table[(j * total_group_entries) + i].start_val, per_proc_vgroup_table[(j * total_group_entries) + i].num_val);
      NSDL2_VARS(NULL,NULL,"Writing in last file Buf = %s.\n", buf);
      //fprintf(stderr, "Writing in last file Buf = %s.\n", buf);
    }
  
    to_write = strlen(buf);
    buf[to_write - 1] = '\n';
    buf[to_write] = '\0';
    //printf("Function %s called at line: %d, Writing to last file = %s\n", (char *)__FUNCTION__, __LINE__, buf);

    if(fwrite(buf, to_write, 1, fp_last) < 1) {
      fprintf(stderr, "Error: Unable to write last file '%s'.\n", last_file);
      return 1;
    }
    fclose(fp_last);
  }
  return 0;
}
#endif
#if 0
void
writeback_static_values() {
  FILE* static_val_fp;
  char path_name[MAX_LINE_LENGTH];
  char cmd[MAX_LINE_LENGTH];
  char file_sep[2];
  char msg_buf[MAX_LINE_LENGTH];
  int i,j,k, start_var, num_var, is_rw, num_val;
  VarTableEntry_Shr *var_ptr;
  PointerTableEntry_Shr* val_ptr; /* pointer into the shared pointer table */

  NSDL2_MISC(NULL, NULL, "Method called");
  sprintf(msg_buf, "writeback_staticvar_values()");
  for (i = 0; i < total_group_entries; i ++) {
      start_var = group_table_shr_mem[i].start_var_idx;
      num_var = group_table_shr_mem[i].num_vars;
      var_ptr = variable_table_shr_mem + start_var;
      is_rw = 0;
      for (j = 0; j < num_var; j++, var_ptr++) {
	if (var_ptr->var_size > 0 ) {
	    is_rw = 1;
	    break;
	}
      }

      if (is_rw) {
	  strcpy(file_sep, group_table_shr_mem[i].column_delimiter);
	  strcpy(path_name, group_table_shr_mem[i].data_fname);
	  sprintf (cmd, "cp %s %s.last", path_name, path_name);
	  system (cmd);
  	  if ((static_val_fp = fopen(path_name, "w")) == NULL) {
    		fprintf(stderr, "%s: Error in opening static var writeback file \n", msg_buf);
    		perror("fopen");
    		return;
  	  }

          num_val = group_table_shr_mem[i].num_values;
          for (k = 0; k < num_val; k++) {
                var_ptr = variable_table_shr_mem + start_var;
      		for (j = 0; j < num_var; j++, var_ptr++) {
		    val_ptr = var_ptr->value_start_ptr + k;
		    if (j == num_var -1) { //Lastc oulmn
			fprintf (static_val_fp, "%s\n", val_ptr->big_buf_pointer);
		    } else {
			fprintf (static_val_fp, "%s%s", val_ptr->big_buf_pointer, file_sep);
		    }
		}
	  }
	  fclose (static_val_fp);
      }
  }

}

#endif
int
get_values(FILE* value_fptr, int var_type, int* rnum) {
  char line[MAX_LINE_LENGTH];
  int first = 1;
  int total_values = 0;
  int pointer_entry;
  int str_len;

  NSDL2_MISC(NULL, NULL, "Method called");
  while (fgets(line, MAX_LINE_LENGTH, value_fptr)) {
    if (create_pointer_table_entry(&pointer_entry) != SUCCESS)
      return -1;

    switch (var_type) {
#ifdef RMI_MODE
    case UTF_VAR: {
      short net_order_len;
      str_len = strlen(line);

      net_order_len = ntohs(str_len);

      memmove(&line[sizeof(short)], line, str_len);
      memcpy(line, &net_order_len, sizeof(short));
      str_len += sizeof(short);
      break;
    }
    case LONG_VAR: {
      long long int byte_value;
      int temp_int;
      if ((sizeof(long long int) != 8) && (sizeof(int) != 4)) {
	NS_EXIT(-1, "netstorm is not able to input LONG variables");
      }
      byte_value = strtoll(line, NULL, 16);
      memcpy(line, &byte_value, 8);

      /* these memory operations is done to put the bytes into network byte order */
      memcpy(&temp_int, line, 4);
      temp_int = ntohl(temp_int);
      memmove(line, line+4, 4);
      memcpy(line+4, &temp_int, 4);
      memcpy(&temp_int, line, 4);
      temp_int = ntohl(temp_int);
      memcpy(line, &temp_int, 4);

      str_len = 8;
      break;
    }
#endif
    default:
      str_len = strlen(line);
      break;
    }

    if ((pointerTable[pointer_entry].big_buf_pointer = copy_into_big_buf(line, str_len)) == -1) {
      NS_EXIT(-1, "get_values(): Failed to copy data into big buf");
    }

    pointerTable[pointer_entry].size = str_len;

    if (first)
      *rnum = pointer_entry;
    else
      first = 0;

    total_values++;
  }

  return total_values;
}

int
get_clust_values(FILE* value_fptr, char* file_name, int* rnum, int clustervar_idx) {
  char cluster_id[MAX_LINE_LENGTH+1];
  char cluster_value[MAX_LINE_LENGTH+1];
  char line[MAX_LINE_LENGTH];
  int clusterval_idx;
  int first = 1;
  int total_entries = 0;
  int line_number = 0;
  int cluster_id_num;
  int j;

  NSDL2_MISC(NULL, NULL, "Method called. file_name = %s, clustervar_idx = %d", file_name, clustervar_idx);
  while (fgets(line, MAX_LINE_LENGTH, value_fptr)) {
    line_number++;
    if (sscanf(line, "%s %s", cluster_id, cluster_value) != 2) {
      fprintf(stderr, "Error in parsing variable file at line %d in file %s, wrong clust format\n", line_number, file_name);
      return -1;
    }

    if (create_clusterval_table_entry(&clusterval_idx) == -1) {
      fprintf(stderr, "Error in creating a cluster value entry\n");
      return -1;
    }

    if (first)
      *rnum = clusterval_idx;
    else
      first = 0;

    if (!strcmp(cluster_id, "DEFAULT")) {
      cluster_id_num = -1;
    } else {
      for (j = 0; j < strlen(cluster_id); j++) {
	if (!isdigit(cluster_id[j])) {
	  fprintf(stderr, "Error in parsing variable file at line %d in file %s, need a cluster id\n", line_number, file_name);
	  return -1;
	}
      }
      cluster_id_num = atoi(cluster_id);
    }

    for (j = clustVarTable[clustervar_idx].start; j<(clustVarTable[clustervar_idx].start + clustVarTable[clustervar_idx].length); j++) {  /* checking to make sure no clust values that are using the same cluster_id */
      if (j == -1)
	break;
      if (clustValTable[j].cluster_id == cluster_id_num) {
	fprintf(stderr, "repeated cluster_id value at line %d\n", line_number);
	return -1;
      }
    }

    clustValTable[clusterval_idx].cluster_id = cluster_id_num;
    if ((clustValTable[clusterval_idx].value = copy_into_big_buf(cluster_value, 0)) == -1) {
      fprintf(stderr, "Error in copying cluster value into the big buf\n");
      return -1;
    }

    if (clustVarTable[clustervar_idx].start == -1)
      clustVarTable[clustervar_idx].start = clusterval_idx;
    total_entries++;
  }

  return total_entries;
}

#if 0
static void create_alias_script() {
  FILE *fp_ip, *fp_sh, *fp_down;
  int i = 0;
  unsigned int ip_addr;
  char buf[MAX_DATA_LINE_LENGTH+1];
  char ip_char[MAX_DATA_LINE_LENGTH];
  char ip_alias[] = "ip_alias.sh";
  char ip_down[] = "ip_down.sh";

  if (strcmp(global_settings->ip_fname, "NULL") == 0) {
    fprintf(stderr, "create_alias_script(): No IP File name provided, using default\n");
    return;
  }
  if ((fp_ip = fopen(global_settings->ip_fname, "r")) == NULL) {
    fprintf(stderr,"create_alias_script(): Error opening %s\n", global_settings->ip_fname);
    exit(-1);
  }
  remove(ip_down);
  if ((fp_down = fopen(ip_down, "w")) == NULL) {
    fprintf(stderr,"create_alias_script(): Error opening %s\n", ip_down);
    exit(-1);
  }
  remove(ip_alias);
  if ((fp_sh = fopen(ip_alias, "w")) == NULL) {
    fprintf(stderr,"create_alias_script(): Error opening %s\n", ip_alias);
    exit(-1);
  } else {
    fprintf(fp_sh, "#!/bin/sh\n\n");
    fprintf(fp_down, "#!/bin/sh\n\n");
    /*fprintf(fp_sh, "/sbin/ifconfig eth0 down\n");
      fprintf(fp_sh, "/sbin/ifconfig eth0 up\n");*/
    while (fgets(buf, MAX_DATA_LINE_LENGTH, fp_ip) != NULL) {
      buf[strlen(buf)-1] = '\0';
      sscanf(buf, "%s", ip_char);
      if ((ip_addr = inet_addr(ip_char)) < 0) {
	  fprintf(stderr,"read_ip_file(): Invalid address, ignoring <%s>\n", ip_char);
      } else {
	fprintf(fp_sh, "/sbin/ifconfig eth0:%d %s\n", i+1, ip_char);
	fprintf(fp_down, "/sbin/ifconfig eth0:%d down\n", i+1);
      }
      i++;
    }
    /*    fprintf(fp_sh, "/sbin/route add default gw 192.168.1.254 eth0\n");*/
    fclose(fp_ip);
    fclose(fp_sh);
    fclose(fp_down);
  }
}
#endif

//Created to free big buffer and related vars
void free_big_buf()
{
  FREE_AND_MAKE_NULL_EX (g_big_buf, max_buffer_space, "g_big_buf", -1);
  g_buf_ptr = NULL;
  used_buffer_space = 0;
  max_buffer_space = 0;
}

void free_structs(void) {
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  free_big_buf();
  FREE_AND_MAKE_NULL_EX (pointerTable, (max_pointer_entries * sizeof(PointerTableEntry)), "pointerTable", -1);
  FREE_AND_MAKE_NULL_EX (fparamValueTable, (max_fparam_entries * sizeof(PointerTableEntry)), "pointerTable", -1);
  FREE_AND_MAKE_NULL_EX (weightTable, (max_weight_entries * sizeof(WeightTableEntry)), "weightTable", -1);
  FREE_AND_MAKE_NULL_EX (groupTable, (max_group_entries * sizeof(GroupTableEntry)), "groupTable", -1);
  FREE_AND_MAKE_NULL_EX (varTable, (max_var_entries * sizeof(VarTableEntry)), "varTable", -1);
  nslb_bigbuf_free((bigbuf_t *)&file_param_value_big_buf);
  // Index var table
  FREE_AND_MAKE_NULL_EX (indexVarTable, (max_index_var_entries * sizeof(VarTableEntry)), "indexVarTable", -1);
  FREE_AND_MAKE_NULL_EX (gServerTable, (max_svr_entries * sizeof(SvrTableEntry)), "gServerTable", -1);
  FREE_AND_MAKE_NULL_EX (serverOrderTable, (max_serverorder_entries * sizeof(ServerOrderTableEntry)), "serverOrderTable", -1);
  FREE_AND_MAKE_NULL_EX (requests, (max_request_entries * sizeof(action_request)), "requests", -1);
  // TODO Need to free smtp to_emails, cc_emails, bcc_emails also
  FREE_AND_MAKE_NULL_EX (hostTable, (max_host_entries * sizeof(HostElement)), "hostTable", -1);
  FREE_AND_MAKE_NULL_EX (gPageTable, (max_page_entries * sizeof(PageTableEntry)), "gPageTable", -1);
  FREE_AND_MAKE_NULL_EX (gSessionTable, (max_sess_entries * sizeof(SessTableEntry)), "gSessionTable", -1);
  FREE_AND_MAKE_NULL_EX (userProfTable, (max_userprof_entries * sizeof(RunProfTableEntry)), "userProfTable", -1);
  FREE_AND_MAKE_NULL_EX (userIndexTable, (max_userindex_entries * sizeof(UserIndexTableEntry)), "userIndexTable", -1);
  FREE_AND_MAKE_NULL_EX (sessProfTable, (max_sessprof_entries * sizeof(SessProfTableEntry)),"sessProfTable", -1);
  FREE_AND_MAKE_NULL_EX (sessProfIndexTable, (max_sessprofindex_entries * sizeof(SessProfIndexTableEntry)), "sessProfIndexTable", -1);
  free_runprof_page_think_idx();
  free_runprof_inline_delay_idx();
  free_runprof_cont_on_page_err_idx();
  free_runprof_auto_fetch_idx(); // for auto fetch embedded table
  free_runprof_page_reload_idx();
  free_runprof_page_clickaway_idx();
  /* Free Structure PerGrpHostTable_Entry & PerHostSvrTable_Entry */
  //free_host_table_mem();
  if(addHeaderTable)
    FREE_AND_MAKE_NULL_EX (addHeaderTable, (total_hdr_entries * sizeof(AddHeaderTableEntry)), "AddHeaderTableEntry", -1);
  free_runprof_add_header_table();
  FREE_AND_MAKE_NULL_EX (runProfTable, (max_runprof_entries * sizeof(RunProfTableEntry)), "runProfTable", -1);
  FREE_AND_MAKE_NULL_EX (runIndexTable, (max_runindex_entries * sizeof(RunIndexTableEntry)), "runIndexTable", -1);
  FREE_AND_MAKE_NULL_EX (locAttrTable, (max_locattr_entries * sizeof(LocAttrTableEntry)), "locAttrTable", -1);
  FREE_AND_MAKE_NULL_EX (accAttrTable, (max_accattr_entries * sizeof(AccAttrTableEntry)), "accAttrTable", -1);
  FREE_AND_MAKE_NULL_EX (accLocTable, (max_accloc_entries * sizeof(AccLocTableEntry)), "accLocTable", -1);
  FREE_AND_MAKE_NULL_EX (browAttrTable, (max_browattr_entries * sizeof(BrowAttrTableEntry)), "browAttrTable", -1);
  FREE_AND_MAKE_NULL_EX (scSzeAttrTable, (max_screen_size_entries* sizeof(ScreenSizeAttrTableEntry)), "ScreenSizeAttrTableEntry", -1);
  FREE_AND_MAKE_NULL_EX (pfBwScSzTable, (max_pf_bw_screen_size_entries * sizeof(PfBwScSzTableEntry)), "PfBwScSzTableEntry", -1);
  FREE_AND_MAKE_NULL_EX (brScSzTable, (max_br_sc_sz_entries * sizeof(BRScSzMapTableEntry)), "BRScSzMapTableEntry", -1);
  //FREE_AND_MAKE_NULL_EX (freqAttrTable, (max_freqattr_entries * sizeof(FreqAttrTableEntry)), "freqAttrTable", -1);
  //FREE_AND_MAKE_NULL_EX (machAttrTable, (max_machattr_entries * sizeof(MachAttrTableEntry)), "machAttrTable", -1);
  FREE_AND_MAKE_NULL_EX (cookieTable, (max_cookie_entries * sizeof(CookieTableEntry)), "cookieTable", -1);
  FREE_AND_MAKE_NULL_EX (dynVarTable, (max_dynvar_entries * sizeof(DynVarTableEntry)), "dynVarTable", -1);
  FREE_AND_MAKE_NULL_EX (reqDynVarTable, (max_reqdynvar_entries * sizeof(ReqDynVarTableEntry)), "reqDynVarTable", -1);
  FREE_AND_MAKE_NULL_EX (proxySvrTable, (max_proxy_svr_entries * sizeof(ProxyServerTable)), "proxySvrTable", -1);
  FREE_AND_MAKE_NULL_EX (proxyExcpTable, (max_proxy_excp_entries * sizeof(ProxyExceptionTable)), "proxyExcpTable", -1);
  FREE_AND_MAKE_NULL_EX (proxyNetPrefixId, (max_proxy_ip_interfaces * sizeof(ProxyNetPrefix)), "proxyNetPrefixId", -1);
#ifdef RMI_MODE
  FREE_AND_MAKE_NULL_EX (byteVarTable, (max_bytevar_entries * sizeof(ByteVarTableEntry)), "byteVarTable", -1);
  FREE_AND_MAKE_NULL_EX (reqByteVarTable, (max_reqbytevar_entries * sizeof(ReqByteVarTableEntry)), "reqByteVarTable", -1);
#endif
  //FREE_AND_MAKE_NULL_EX (totSvrTable, (max_totsvr_entries * sizeof(TotSvrTableEntry)), "totSvrTable", -1);
  /* Free Structure PerGrpHostTable_Entry & PerHostSvrTable_Entry */
  //free_host_table_mem();
  FREE_AND_MAKE_NULL_EX (inuseSvrTable, (max_inusesvr_entries * sizeof(InuseSvrTableEntry)), "inuseSvrTable", -1);
  FREE_AND_MAKE_NULL_EX (inuseUserTable, (max_inuseuser_entries * sizeof(InuseUserTableEntry)), "inuseUserTable", -1);
  FREE_AND_MAKE_NULL_EX (large_location_array, (total_locattr_entries * total_locattr_entries * sizeof(short)), "large_location_array", -1);
  FREE_AND_MAKE_NULL_EX (lineCharTable, (max_linechar_entries * sizeof(LineCharTableEntry)), "lineCharTable", -1);
  FREE_AND_MAKE_NULL_EX (slaTable, (max_sla_entries * sizeof(SLATableEntry)), "slaTable", -1);
  FREE_AND_MAKE_NULL_EX (metricTable, (max_metric_entries * sizeof(MetricTableEntry)), "metricTable", -1);
  FREE_AND_MAKE_NULL_EX (thinkProfTable, (max_thinkprof_entries * sizeof(ThinkProfTableEntry)), "thinkProfTable", -1);
  FREE_AND_MAKE_NULL_EX (inlineDelayTable, (max_inline_delay_entries * sizeof(InlineDelayTableEntry)), "inlineDelayTable", -1);
  FREE_AND_MAKE_NULL_EX (autofetchTable, (max_autofetch_entries * sizeof(AutoFetchTableEntry)), "autofetchTable", -1); // for auto fetch embedded
  FREE_AND_MAKE_NULL_EX (continueOnPageErrorTable, (max_cont_on_err_entries * sizeof(ContinueOnPageErrorTableEntry)), "continue on page err table", -1); // for continue on err entry
  FREE_AND_MAKE_NULL_EX (overrideRecordedThinktimeTable, (max_recorded_think_time_entries * sizeof(OverrideRecordedThinkTime)), "override recorded think time", -1); // for continue on err entry
  FREE_AND_MAKE_NULL_EX (clickActionTable, (max_clickaction_entries * sizeof(ClickActionTableEntry)), "clickActionTable", -1); // for click and script user actions
  // At least one entry is there for Reload & Click Away
  FREE_AND_MAKE_NULL_EX (pageReloadProfTable, (max_pagereloadprof_entries * sizeof(PageReloadProfTableEntry)), "pageReloadProfTable", -1);
  FREE_AND_MAKE_NULL_EX (pageClickAwayProfTable, (max_pageclickawayprof_entries * sizeof(PageClickAwayProfTableEntry)), "pageClickAwayProfTable", -1);
  FREE_AND_MAKE_NULL_EX (pacingTable, (max_pacing_entries * sizeof(PacingTableEntry)), "pacingTable", -1);
  FREE_AND_MAKE_NULL_EX (searchVarTable, (max_searchvar_entries * sizeof(SearchVarTableEntry)), "searchVarTable", -1);
  FREE_AND_MAKE_NULL_EX (searchPageTable, (max_searchpage_entries * sizeof(SearchPageTableEntry)), "searchPageTable", -1);
  FREE_AND_MAKE_NULL_EX (perPageSerVarTable, (max_perpageservar_entries * sizeof(PerPageSerVarTableEntry)), "perPageSerVarTable", -1);
  //For JSON Var
  FREE_AND_MAKE_NULL_EX (jsonVarTable, (max_jsonvar_entries * sizeof(JSONVarTableEntry)), "jsonVarTable", -1);
  FREE_AND_MAKE_NULL_EX (jsonPageTable, (max_jsonpage_entries * sizeof(JSONPageTableEntry)), "jsonPageTable", -1);
  FREE_AND_MAKE_NULL_EX (perPageJSONVarTable, (max_perpagejsonvar_entries * sizeof(PerPageJSONVarTableEntry)), "perPageJSONVarTable", -1);

  FREE_AND_MAKE_NULL_EX (randomVarTable, (max_randomvar_entries * sizeof(RandomVarTableEntry)), "randomVarTable", -1);
  FREE_AND_MAKE_NULL_EX (dateVarTable, (max_datevar_entries * sizeof(DateVarTableEntry)), "dateVarTable", -1);
  FREE_AND_MAKE_NULL_EX (randomStringTable, (max_randomstring_entries * sizeof(RandomStringTableEntry)), "randomStringTable", -1);
  FREE_AND_MAKE_NULL_EX (uniqueVarTable, (max_uniquevar_entries * sizeof(UniqueVarTableEntry)), "uniqueVarTable", -1);
  FREE_AND_MAKE_NULL_EX (perPageChkPtTable, (max_perpagechkpt_entries * sizeof(PerPageChkPtTableEntry)), "perPageChkPtTable", -1);
  FREE_AND_MAKE_NULL_EX (perPageChkRepSizeTable, (max_perpagechkrepsize_entries * sizeof(PerPageCheckReplySizeTableEntry)), "perPageChkRepSizeTable", -1);
  FREE_AND_MAKE_NULL_EX (clustVarTable, (max_clustvar_entries * sizeof(ClustVarTableEntry)), "clustVarTable", -1);
  FREE_AND_MAKE_NULL_EX (clustValTable, (max_clustval_entries * sizeof(ClustValTableEntry)), "clustValTable", -1);
  FREE_AND_MAKE_NULL_EX (clustTable, (max_clust_entries * sizeof(ClustTableEntry)), "clustTable", -1);
  FREE_AND_MAKE_NULL_EX (repeatBlock, (max_repeat_block_entries * sizeof(RepeatBlock)), "RepeatBlock", -1);
  free_http_method_table();
  free_g_temp_buf();
  free_url_hash_table();
  #ifdef CAV_MAIN
  FREE_AND_MAKE_NULL_EX (ips, (max_ip_entries * sizeof(IP_data)), "ipData", -1);
  FREE_AND_MAKE_NULL_EX (clients, (max_client_entries * sizeof(Client_data)), "clients", -1);
  FREE_AND_MAKE_NULL_EX (checkPageTable, (max_checkpage_entries * sizeof(CheckPageTableEntry)), "checkpage", -1);
  FREE_AND_MAKE_NULL_EX (checkReplySizePageTable, (max_check_replysize_page_entries * sizeof(CheckReplySizePageTableEntry)), "checkreplysize", -1);
  FREE_AND_MAKE_NULL_EX (reqCookTable, (max_reqcook_entries * sizeof(ReqCookTableEntry)), "reqcook", -1);
  FREE_AND_MAKE_NULL_EX (segTable, (max_seg_entries * sizeof(SegTableEntry)), "seg", -1);
  FREE_AND_MAKE_NULL_EX (postTable, (max_post_entries * sizeof(StrEnt)), "post", -1);
  FREE_AND_MAKE_NULL_EX (reportTable, (max_report_entries * sizeof(avgtime)), "report", -1);
  FREE_AND_MAKE_NULL_EX (errorCodeTable, (max_errorcode_entries * sizeof(ErrorCodeTableEntry)), "errorcode", -1);
  FREE_AND_MAKE_NULL_EX (nsVarTable, (max_nsvar_entries * sizeof(NsVarTableEntry)), "nsvar", -1);
  FREE_AND_MAKE_NULL_EX (tagTable, (max_tag_entries * sizeof(TagTableEntry)), "tag", -1);
  FREE_AND_MAKE_NULL_EX (attrQualTable, (max_attrqual_entries * sizeof(AttrQualTableEntry)), "attrqual", -1);
  FREE_AND_MAKE_NULL_EX (tagPageTable, (max_tagpage_entries * sizeof(TagPageTableEntry)), "tagpage", -1);
  FREE_AND_MAKE_NULL_EX (randomStringTable, (max_randomstring_entries * sizeof(RandomStringTableEntry)), "randomstring", -1);
  FREE_AND_MAKE_NULL_EX (dateVarTable, (max_datevar_entries * sizeof(DateVarTableEntry)), "datevar", -1);
  FREE_AND_MAKE_NULL_EX (groupVarTable, (max_groupvar_entries * sizeof(GroupVarTableEntry)), "groupvar", -1);
  #endif 
  // Now free big_buf_shr_mem
/*  FREE_AND_MAKE_NULL_EX (big_buf_shr_mem, used_buffer_space, "bigbuf", -1);
  FREE_AND_MAKE_NULL_EX (weight_table_shr_mem, sizeof(WeightTableEntry) * total_weight_entries, "weight table", -1);
  // This will free weight_table_shr_mem, group_table_shr_mem, variable_table_shr_mem
  FREE_AND_MAKE_NULL_EX (pointer_table_shr_mem, sizeof(PointerTableEntry_Shr) * total_pointer_entries, "pointer table", -1);
  FREE_AND_MAKE_NULL_EX (group_table_shr_mem, sizeof(GroupTableEntry_Shr) * total_group_entries, "group table", -1);
  FREE_AND_MAKE_NULL_EX (variable_table_shr_mem, sizeof(VarTableEntry_Shr) * total_var_entries, "variable table", -1);
  FREE_AND_MAKE_NULL_EX (index_variable_table_shr_mem, sizeof(VarTableEntry_Shr) * total_index_var_entries, "index variable table", -1);
  FREE_AND_MAKE_NULL_EX (repeat_block_shr_mem, total_repeat_block_entries * sizeof(RepeatBlock_Shr), "repeat block table", -1);
  FREE_AND_MAKE_NULL_EX (randomvar_table_shr_mem, total_randomvar_entries * sizeof(RandomVarTableEntry_Shr), "random var table", -1);
  FREE_AND_MAKE_NULL_EX (randomstring_table_shr_mem, total_randomstring_entries * sizeof(RandomStringTableEntry_Shr), "random string table", -1);
  FREE_AND_MAKE_NULL_EX (uniquevar_table_shr_mem, total_uniquevar_entries * sizeof(UniqueVarTableEntry_Shr), "unique table", -1);
  FREE_AND_MAKE_NULL_EX (datevar_table_shr_mem, total_datevar_entries * sizeof(DateVarTableEntry_Shr), "datevar table", -1);
  FREE_AND_MAKE_NULL_EX (gserver_table_shr_mem, sizeof(SvrTableEntry_Shr) * total_svr_entries, "gserver table", -1);
  FREE_AND_MAKE_NULL_EX (seg_table_shr_mem, sizeof(SegTableEntry_Shr) * total_seg_entries, "seg table", -1);
  FREE_AND_MAKE_NULL_EX (serverorder_table_shr_mem, sizeof(ServerOrderTableEntry_Shr) * total_serverorder_entries, "serverorder table", -1);
  FREE_AND_MAKE_NULL_EX (post_table_shr_mem, sizeof(StrEnt_Shr) * total_post_entries, "post table", -1);
  FREE_AND_MAKE_NULL_EX (reqcook_table_shr_mem, sizeof(ReqCookTableEntry_Shr) * total_reqcook_entries, "reqcook table", -1);
  FREE_AND_MAKE_NULL_EX (reqdynvar_table_shr_mem, sizeof(ReqDynVarTableEntry_Shr) * total_reqdynvar_entries, "reqdyncvar table", -1);
  FREE_AND_MAKE_NULL_EX (clickaction_table_shr_mem, sizeof(ClickActionTableEntry_Shr) * total_clickaction_entries, "clickaction table", -1);
  FREE_AND_MAKE_NULL_EX (request_table_shr_mem, sizeof(action_request_Shr) * total_request_entries, "request table", -1);
  FREE_AND_MAKE_NULL_EX (host_table_shr_mem, sizeof(HostTableEntry_Shr) * total_host_entries, "host table", -1);
  FREE_AND_MAKE_NULL_EX (thinkprof_table_shr_mem, sizeof(ThinkProfTableEntry_Shr) * actual_num_pages, "thinkprof table", -1);
  FREE_AND_MAKE_NULL_EX (inline_delay_table_shr_mem, sizeof(InlineDelayTableEntry_Shr) * actual_num_pages, "inlinedelay table", -1);
  FREE_AND_MAKE_NULL_EX (autofetch_table_shr_mem, sizeof(AutoFetchTableEntry_Shr) * total_num_pages, "autofetch table", -1);
  FREE_AND_MAKE_NULL_EX (continueOnPageErrorTable_shr_mem, sizeof( ContinueOnPageErrorTableEntry_Shr) * total_num_pages, "continuepageerror table", -1);
  FREE_AND_MAKE_NULL_EX (perpageservar_table_shr_mem, total_perpageservar_entries * sizeof(PerPageSerVarTableEntry_Shr), "perpageserver table", -1);
  FREE_AND_MAKE_NULL_EX (perpagechkpt_table_shr_mem, total_perpagechkpt_entries * sizeof(PerPageChkPtTableEntry_Shr), "perpageservar table", -1);
  FREE_AND_MAKE_NULL_EX (session_table_shr_mem, sizeof(SessTableEntry) * total_sess_entries, "session table", -1);
  FREE_AND_MAKE_NULL_EX (locattr_table_shr_mem, total_locattr_entries * sizeof(LocAttrTableEntry_Shr), "locattr table", -1);
  FREE_AND_MAKE_NULL_EX (accattr_table_shr_mem, total_accattr_entries * sizeof(AccAttrTableEntry), "accattr table", -1);
  FREE_AND_MAKE_NULL_EX (browattr_table_shr_mem, total_browattr_entries * sizeof(BrowAttrTableEntry), "browattr table", -1);
//  FREE_AND_MAKE_NULL_EX (freqattr_table_shr_mem, sizeof(WeightTableEntry) * total_weight_entries, "freqattr table", -1);
//  FREE_AND_MAKE_NULL_EX (machattr_table_shr_mem, sizeof(WeightTableEntry) * total_weight_entries, "machattr table", -1);
  FREE_AND_MAKE_NULL_EX (scszattr_table_share_mem, total_screen_size_entries * sizeof(ScreenSizeAttrTableEntry), "scszattr table", -1);
  FREE_AND_MAKE_NULL_EX (sessprof_table_shr_mem, sizeof(SessProfTableEntry_Shr) * total_sessprof_entries, "sessprof table", -1);
  FREE_AND_MAKE_NULL_EX (sessprofindex_table_shr_mem, sizeof(SessProfIndexTableEntry_Shr) * total_sessprofindex_entries, "sessprofindex table", -1);
  FREE_AND_MAKE_NULL_EX (runprof_table_shr_mem, sizeof(RunProfTableEntry_Shr) * total_runprof_entries, "runprof table", -1);
  FREE_AND_MAKE_NULL_EX (proxySvr_table_shr_mem, sizeof(ProxyServerTable_Shr) * total_proxy_svr_entries, "proxyserver table", -1);
  FREE_AND_MAKE_NULL_EX (proxyExcp_table_shr_mem, sizeof(ProxyExceptionTable_Shr) * total_proxy_excp_entries, "proxyexcp table", -1);
  FREE_AND_MAKE_NULL_EX (proxyNetPrefix_table_shr_mem, sizeof(ProxyNetPrefix_Shr) * total_proxy_ip_interfaces, "proxynet table", -1);
  FREE_AND_MAKE_NULL_EX (metric_table_shr_mem, sizeof(MetricTableEntry_Shr) * total_metric_entries, "metric table", -1);
  FREE_AND_MAKE_NULL_EX (inusesvr_table_shr_mem, sizeof(InuseSvrTableEntry_Shr) * total_inusesvr_entries, "inusesvr table", -1);
  FREE_AND_MAKE_NULL_EX (errorcode_table_shr_mem, sizeof(ErrorCodeTableEntry_Shr) * total_errorcode_entries, "errorcode table", -1);
  FREE_AND_MAKE_NULL_EX (userprof_table_shr_mem, sizeof(UserProfTableEntry_Shr) * total_userprofshr_entries, "userprof table", -1);
  FREE_AND_MAKE_NULL_EX (userprofindex_table_shr_mem, sizeof(UserProfIndexTableEntry_Shr) * total_userindex_entries, "userprofindex table", -1);
  FREE_AND_MAKE_NULL_EX (runprofindex_table_shr_mem, sizeof(RunProfIndexTableEntry_Shr) * total_runindex_entries, "runprofindex table", -1);
  FREE_AND_MAKE_NULL_EX (tx_table_shr_mem, sizeof(PageTableEntry_Shr) * total_page_entries, "tx table", -1);
  FREE_AND_MAKE_NULL_EX (http_method_table_shr_mem, sizeof(http_method_t_shr) * total_http_method, "httpmethod table", -1);
  FREE_AND_MAKE_NULL_EX (testcase_shr_mem, sizeof(TestCaseType_Shr), "testcase table", -1);
  FREE_AND_MAKE_NULL_EX (pattern_table_shr, sizeof(PatternTable_Shr) * total_used_pattern_entries, "pattern table", -1);
  FREE_AND_MAKE_NULL_EX (gVUserSummaryTable, gVUserSummaryTableSize, "VUserSummaryTable", -1);
  #endif */
}


int create_client_table_entry(int *row_num) {
  NSDL2_PARENT(NULL, NULL, "Method called");
  if (total_client_entries == max_client_entries) {
    MY_REALLOC_EX (clients, (max_client_entries + DELTA_CLIENT_ENTRIES) * sizeof(Client_data),max_client_entries * sizeof(Client_data), "clients", -1);
    if (!clients) {
      fprintf(stderr,"create_client_table_entry(): Error allocating more memory for client entries\n");
      return(FAILURE);
    } else max_client_entries += DELTA_CLIENT_ENTRIES;
  }
  *row_num = total_client_entries++;
  return (SUCCESS);
}


#define SESSION_MEMORY_CONVERSION(index) (session_table_shr_mem + index)
#define SESSIONPROFILE_TABLE_MEMORY_CONVERSION(index) (sessprof_table_shr_mem + index)
#define USERPROFILEINDEX_TABLE_MEMORY_CONVERSION(index) (userprofindex_table_shr_mem + index)
#define SESSIONPROFILEINDEX_TABLE_MEMORY_CONVERSION(index) (sessprofindex_table_shr_mem + index)
#define PROXY_SVR_TABLE_MEMORY_CONVERSION(index) (proxySvr_table_shr_mem + index)
#define RUNPROFILE_TABLE_MEMORY_CONVERSION(index) (runprof_table_shr_mem + index)
#define RUNPROFILEINDEX_TABLE_MEMORY_CONVERSION(index) (runprofindex_table_shr_mem + index)
#define PACING_TABLE_MEMORY_CONVERSION(index) (pacing_table_shr_mem + index)

int insert_proftable_shr_mem(void) {
  int i, j;
  void* prof_table_shr_mem;
  int prof_tables_size;

  insert_user_proftable_shr_mem();

  prof_tables_size = WORD_ALIGNED(sizeof(SessProfTableEntry_Shr) * total_sessprof_entries) +
    WORD_ALIGNED(sizeof(SessProfIndexTableEntry_Shr) * total_sessprofindex_entries) +
    WORD_ALIGNED(sizeof(ProxyExceptionTable_Shr) * total_proxy_excp_entries) +
    WORD_ALIGNED(sizeof(ProxyNetPrefix_Shr) * total_proxy_ip_interfaces) +
    WORD_ALIGNED(sizeof(ProxyServerTable_Shr) * total_proxy_svr_entries) +
    WORD_ALIGNED(sizeof(RunProfTableEntry_Shr) * total_runprof_entries) +
    WORD_ALIGNED(sizeof(RunProfIndexTableEntry_Shr) * total_runindex_entries) +
    WORD_ALIGNED(sizeof(SLATableEntry_Shr) * total_sla_entries) +
    WORD_ALIGNED(sizeof(MetricTableEntry_Shr) * total_metric_entries) +
    WORD_ALIGNED(sizeof(TestCaseType_Shr)); 


   prof_table_shr_mem = do_shmget(prof_tables_size, "Session Profile Table");
  /* inserting the sessprof table */
  sessprof_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(SessProfTableEntry_Shr) * total_sessprof_entries);

  for (i = 0; i < total_sessprof_entries; i++) {
    sessprof_table_shr_mem[i].session_ptr = SESSION_MEMORY_CONVERSION(sessProfTable[i].session_idx);
    sessprof_table_shr_mem[i].pct = sessProfTable[i].pct;
  }

  /* inserting the sessprof index table */
  sessprofindex_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(SessProfIndexTableEntry_Shr) * total_sessprofindex_entries);

  for (i = 0; i < total_sessprofindex_entries; i++) {
    sessprofindex_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(sessProfIndexTable[i].name);
    sessprofindex_table_shr_mem[i].sessprof_start = SESSIONPROFILE_TABLE_MEMORY_CONVERSION(sessProfIndexTable[i].sessprof_start);
    sessprofindex_table_shr_mem[i].length = sessProfIndexTable[i].sessprof_length;
  }

  /* Copying proxy exception table into shared memory */
  proxyExcp_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(ProxyExceptionTable_Shr) * total_proxy_excp_entries);
  copy_proxyExcp_table_into_shr_mem((ProxyExceptionTable_Shr *)proxyExcp_table_shr_mem);

  /* Copying proxy ip table into shared memory */
  proxyNetPrefix_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(ProxyNetPrefix_Shr) * total_proxy_ip_interfaces);
  copy_proxyNetPrefix_table_into_shr_mem((ProxyNetPrefix_Shr *)proxyNetPrefix_table_shr_mem);  
 
 
  /* copying proxy table into shared memory */
  proxySvr_table_shr_mem = prof_table_shr_mem; 
  prof_table_shr_mem += WORD_ALIGNED(sizeof(ProxyServerTable_Shr) * total_proxy_svr_entries); 
  copy_proxySvr_table_into_shr_mem((ProxyServerTable_Shr *)proxySvr_table_shr_mem);

 /* inserting the runprof table */
  runprof_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(RunProfTableEntry_Shr) * total_runprof_entries);
 
  int k = 0;
  for (i = 0; i < total_runprof_entries; i++) {
    runprof_table_shr_mem[i].userindexprof_ptr = USERPROFILEINDEX_TABLE_MEMORY_CONVERSION(runProfTable[i].userprof_idx);
    if (global_settings->use_sess_prof)
        runprof_table_shr_mem[i].sessindexprof_ptr = SESSIONPROFILEINDEX_TABLE_MEMORY_CONVERSION(runProfTable[i].sessprof_idx);
    else
        runprof_table_shr_mem[i].sess_ptr = SESSION_MEMORY_CONVERSION(runProfTable[i].sessprof_idx);
    runprof_table_shr_mem[i].quantity = runProfTable[i].quantity;
    runprof_table_shr_mem[i].percentage = runProfTable[i].percentage;
    runprof_table_shr_mem[i].grp_type = runProfTable[i].grp_type;
    //runprof_table_shr_mem[i].users = runProfTable[i].users; /* TODO:CHECK:BHAV:XXXXX */
    runprof_table_shr_mem[i].cluster_id = runProfTable[i].cluster_id;
    runprof_table_shr_mem[i].group_num = runProfTable[i].group_num;
    runprof_table_shr_mem[i].start_page_idx = runProfTable[i].start_page_idx;
    runprof_table_shr_mem[i].scen_group_name = BIG_BUF_MEMORY_CONVERSION(runProfTable[i].scen_group_name);
    runprof_table_shr_mem[i].gset = runProfTable[i].gset;
    runprof_table_shr_mem[i].total_nsl_var_entries = gSessionTable[runProfTable[i].sessprof_idx].num_nslvar_entries;
    
    //Set proxy_ptr. If proxy not defined for a group, proxy_ptr will remain NULL, 
    //else will have pointer to its proxy in ProxyServerTable
    if(runProfTable[i].proxy_idx == -1)
      runprof_table_shr_mem[i].proxy_ptr = NULL;
    else
      runprof_table_shr_mem[i].proxy_ptr = PROXY_SVR_TABLE_MEMORY_CONVERSION(runProfTable[i].proxy_idx);

    /*NetOmni: Added code to save total generators if test running as generator and schedule_by group*/
    runprof_table_shr_mem[i].num_generator_per_grp = runProfTable[i].num_generator_per_grp;
    runprof_table_shr_mem[i].num_generator_kill_per_grp = runProfTable[i].num_generator_kill_per_grp;

    if (runProfTable[i].cluster_id < 0) //Default cluster group
        runprof_table_shr_mem[i].cluster_name = "None";
    else
        runprof_table_shr_mem[i].cluster_name = BIG_BUF_MEMORY_CONVERSION(clustTable[runProfTable[i].cluster_id].cluster_id);

    runprof_table_shr_mem[i].pacing_ptr = &pacing_table_shr_mem[i];

/*     if (runProfTable[i].pacing_idx == -1) { */
/*       runprof_table_shr_mem[i].pacing_ptr = PACING_TABLE_MEMORY_CONVERSION(0); */
/*     } else { */
/*       runprof_table_shr_mem[i].pacing_ptr = PACING_TABLE_MEMORY_CONVERSION(runProfTable[i].pacing_idx); */
/*     } */


    /* Keep in shr mem for runtime updation. */
    runprof_table_shr_mem[i].page_think_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].page_think_table");
    runprof_table_shr_mem[i].inline_delay_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].inline_delay_table");
    runprof_table_shr_mem[i].auto_fetch_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].auto_fetch_table"); // for auto fetch embedded
    runprof_table_shr_mem[i].continue_onpage_error_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].continue_onpage_error_table"); 
    runprof_table_shr_mem[i].addHeaders = do_shmget(sizeof(AddHTTPHeaderList_Shr) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                          "runprof_table_shr_mem[i].addHeaders");

    if(total_pagereloadprof_entries > 1 || (total_pagereloadprof_entries == 1 && pageReloadProfTable[0].min_reloads >= 0 ))
        runprof_table_shr_mem[i].page_reload_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].page_reload_table");

    if(total_pageclickawayprof_entries > 1 || (total_pageclickawayprof_entries == 1 && pageClickAwayProfTable[0].clickaway_pct >= 0 ))
        runprof_table_shr_mem[i].page_clickaway_table = do_shmget(sizeof(void *) * gSessionTable[runProfTable[i].sessprof_idx].num_pages,
                                                          "runprof_table_shr_mem[i].page_clickaway_table");

    runprof_table_shr_mem[i].num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    for (j = 0; j < gSessionTable[runProfTable[i].sessprof_idx].num_pages; j++) {

      runprof_table_shr_mem[i].page_think_table[j] = &thinkprof_table_shr_mem[k];
      runprof_table_shr_mem[i].inline_delay_table[j] = &inline_delay_table_shr_mem[k];
      runprof_table_shr_mem[i].auto_fetch_table[j] = &autofetch_table_shr_mem[k]; // for auto fetch embedded
      runprof_table_shr_mem[i].continue_onpage_error_table[j] = &continueOnPageErrorTable_shr_mem [k]; // for auto fetch embedded
      // Copying Additional Header saved in runProfTable
      if(runProfTable[i].addHeaders[j].MainUrlHeaderBuf.seg_start == -1)
        runprof_table_shr_mem[i].addHeaders[j].MainUrlHeaderBuf.seg_start = NULL;
      else
        runprof_table_shr_mem[i].addHeaders[j].MainUrlHeaderBuf.seg_start = 
                                                    SEG_TABLE_MEMORY_CONVERSION(runProfTable[i].addHeaders[j].MainUrlHeaderBuf.seg_start);

      if(runProfTable[i].addHeaders[j].InlineUrlHeaderBuf.seg_start == -1)
        runprof_table_shr_mem[i].addHeaders[j].InlineUrlHeaderBuf.seg_start = NULL;
      else
        runprof_table_shr_mem[i].addHeaders[j].InlineUrlHeaderBuf.seg_start = 
                                                    SEG_TABLE_MEMORY_CONVERSION(runProfTable[i].addHeaders[j].InlineUrlHeaderBuf.seg_start);

      if(runProfTable[i].addHeaders[j].AllUrlHeaderBuf.seg_start == -1)
        runprof_table_shr_mem[i].addHeaders[j].AllUrlHeaderBuf.seg_start = NULL;
      else
        runprof_table_shr_mem[i].addHeaders[j].AllUrlHeaderBuf.seg_start = 
                                                    SEG_TABLE_MEMORY_CONVERSION(runProfTable[i].addHeaders[j].AllUrlHeaderBuf.seg_start);

       runprof_table_shr_mem[i].addHeaders[j].AllUrlHeaderBuf.num_entries = runProfTable[i].addHeaders[j].AllUrlHeaderBuf.num_entries;
       runprof_table_shr_mem[i].addHeaders[j].MainUrlHeaderBuf.num_entries = runProfTable[i].addHeaders[j].MainUrlHeaderBuf.num_entries;
       runprof_table_shr_mem[i].addHeaders[j].InlineUrlHeaderBuf.num_entries = runProfTable[i].addHeaders[j].InlineUrlHeaderBuf.num_entries;

      //memcpy(&runprof_table_shr_mem[i].addHeaders[j], &runProfTable[i].addHeaders[j], sizeof(AddHTTPHeaderList));
      
      if(total_pagereloadprof_entries > 1 || (total_pagereloadprof_entries == 1 && pageReloadProfTable[0].min_reloads >= 0 ))
      {
        if(pagereloadprof_table_shr_mem[k].min_reloads >= 0)
         runprof_table_shr_mem[i].page_reload_table[j] = &pagereloadprof_table_shr_mem[k]; 
        else
         runprof_table_shr_mem[i].page_reload_table[j] = NULL;
      }

      if(total_pageclickawayprof_entries > 1 || (total_pageclickawayprof_entries == 1 && pageClickAwayProfTable[0].clickaway_pct >= 0 ))
      {
	if(pageclickawayprof_table_shr_mem[k].clickaway_pct >= 0)
          runprof_table_shr_mem[i].page_clickaway_table[j] = &pageclickawayprof_table_shr_mem[k]; 
        else
          runprof_table_shr_mem[i].page_clickaway_table[j] = NULL; 
      }

      NSDL4_MISC(NULL, NULL, "runprof_table_shr_mem[%d].page_think_table[%d] = %d", i, j, 
                 ((ThinkProfTableEntry_Shr *)runprof_table_shr_mem[i].page_think_table[j])->avg_time);
      NSDL4_MISC(NULL, NULL, "runprof_table_shr_mem[%d].inline_delay_table[%d] = %d", i, j, 
                 ((InlineDelayTableEntry_Shr *)runprof_table_shr_mem[i].inline_delay_table[j])->avg_time);
      k++;
    }
/*     for (j = 0; j < gSessionTable[runProfTable[i].sessprof_idx].num_pages; j++) { */
/*       if (runProfTable[i].page_think_table[j] == -1) { */
/*         runprof_table_shr_mem[i].page_think_table[j] = THINK_PROF_TABLE_MEMORY_CONVERSION(0); */
/*       } else { */
/*         runprof_table_shr_mem[i].page_think_table[j] = THINK_PROF_TABLE_MEMORY_CONVERSION(runProfTable[i].page_think_table[j]); */
/*       } */
/*     } */
  }
  //runprof index not needed. should be removed
  /* inserting the runprof index table */
  runprofindex_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(RunProfIndexTableEntry_Shr) * total_runindex_entries);

  for (i = 0; i < total_runindex_entries; i++) {
    runprofindex_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(runIndexTable[i].name);
    runprofindex_table_shr_mem[i].runprof_start = RUNPROFILE_TABLE_MEMORY_CONVERSION(runIndexTable[i].runprof_start);
    runprofindex_table_shr_mem[i].length = runIndexTable[i].runprof_length;
  }

  /* inserting the sla table */
  prof_table_shr_mem = copy_slaTable_to_shr(prof_table_shr_mem); // ns_goal_based_sla.c

  /*inserting the metric table */
  metric_table_shr_mem = prof_table_shr_mem;
  prof_table_shr_mem += WORD_ALIGNED(sizeof(MetricTableEntry_Shr) * total_metric_entries);

  for (i = 0; i < total_metric_entries; i++) {
    metric_table_shr_mem[i].name = metricTable[i].name;
    metric_table_shr_mem[i].port = metricTable[i].port;
    metric_table_shr_mem[i].qualifier = metricTable[i].qualifier;
    metric_table_shr_mem[i].relation = metricTable[i].relation;
    metric_table_shr_mem[i].target_value = metricTable[i].target_value;
    metric_table_shr_mem[i].min_samples = metricTable[i].min_samples;
    metric_table_shr_mem[i].udp_array_idx = metricTable[i].udp_array_idx;
  }

  /* inserting the testcase struct */
  testcase_shr_mem = prof_table_shr_mem;

  testcase_shr_mem->mode = testCase.mode;
  /*  if ((runindex = find_runindex_idx(testCase.run_name)) == -1) {
    fprintf(stderr, "Run Profile %s, in the test case, is not defined\n", testCase.run_name);
    return -1;
    }
  testcase_shr_mem->runindex_idx = runindex;
  testcase_shr_mem->runindex_ptr = RUNPROFILEINDEX_TABLE_MEMORY_CONVERSION(runindex);*/
  testcase_shr_mem->guess_type = testCase.guess_type;
  testcase_shr_mem->guess_num = testCase.guess_num;
  testcase_shr_mem->guess_prob = testCase.guess_prob;
  testcase_shr_mem->min_steps  = testCase.min_steps;
  testcase_shr_mem->stab_num_success = testCase.stab_num_success;
  testcase_shr_mem->stab_max_run = testCase.stab_max_run;
  testcase_shr_mem->stab_run_time = testCase.stab_run_time;
  testcase_shr_mem->stab_sessions = testCase.stab_sessions;
  testcase_shr_mem->stab_goal_pct = testCase.stab_goal_pct;
  testcase_shr_mem->target_rate = testCase.target_rate;

  return 0;
}

#define LOCATTR_TABLE_MEMORY_CONVERSION(index) (locattr_table_shr_mem + index)

int clustval_cmp(const void* ent1, const void* ent2) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (((ClustValTableEntry*)ent1)->cluster_id < ((ClustValTableEntry*)ent2)->cluster_id)
    return -1;
  else if (((ClustValTableEntry*)ent1)->cluster_id > ((ClustValTableEntry*)ent2)->cluster_id)
    return 1;
  else
    return 0;
}

int
create_clustvar_table(void)
{
  int tab_size = total_clustvar_entries * total_clust_entries;
  int i;
  //int clust_table_fd;
  int clustval_idx;
  //int clust_num;
  //int default_clust_idx;
  int j;

  NSDL2_MISC(NULL, NULL, "Method called");
  //ClustValTable has values for first vars for all clusters and than second var for all clusters ..
  //Sort each block of values  for each clust var on the order of clust-idx
  for (i = 0; i < total_clustvar_entries; i++) {
    qsort(&clustValTable[clustVarTable[i].start], clustVarTable[i].length, sizeof(ClustValTableEntry), clustval_cmp);
  }

  if (tab_size) {
    //Define two tables in this segment
#if 0
    if ((clust_table_fd = do_shmget(shm_base + total_num_shared_segs, sizeof(ClustValTableEntry_Shr) * tab_size + sizeof(char*) * total_clustvar_entries, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
      fprintf(stderr, "error in allocating sharmed mem for the cluster table\n");
      return -1;
    }

    total_num_shared_segs++;

    if ((clust_table_shr_mem = (ClustValTableEntry_Shr*)shmat(clust_table_fd, NULL, 0))) {
#endif

    clust_table_shr_mem = (ClustValTableEntry_Shr*) do_shmget(sizeof(ClustValTableEntry_Shr) * tab_size + sizeof(char*) * total_clustvar_entries, "cluster table");
      //Set the address for the second table
      clust_name_table_shr_mem = (char**)((char*) clust_table_shr_mem + (sizeof(ClustValTableEntry_Shr) * tab_size));

      //Note that the ClustValTable is sorted in for each clust var in cluster-idx order
      //For each grop var
      for (i = 0; i < total_clustvar_entries; i++) {
	//For each scenrio grouo
	for (j = 0; j < total_clust_entries; j++) {
		clustval_idx = i*total_clust_entries + j;
	  	if (clustValTable[clustval_idx].cluster_id != j)  {
		    //Cluster is missiong - quite possibly some other cluster got duplicated
                    NS_EXIT(-1, CAV_ERR_1031041, clustTable[j].cluster_id);
		} else {
	      	    clust_table_shr_mem[clustval_idx].value = BIG_BUF_MEMORY_CONVERSION(clustValTable[clustval_idx].value);
	            clust_table_shr_mem[clustval_idx].length = strlen(clust_table_shr_mem[clustval_idx].value);
		}
	 }
      }

      for (i = 0; i < total_clustvar_entries; i++) {
	clust_name_table_shr_mem[i] = BIG_BUF_MEMORY_CONVERSION(clustVarTable[i].name);
      }
    //} else
      //return -1;
  }
  return 0;
}


int groupval_cmp(const void* ent1, const void* ent2) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (((GroupValTableEntry*)ent1)->group_id < ((GroupValTableEntry*)ent2)->group_id)
    return -1;
  else if (((GroupValTableEntry*)ent1)->group_id > ((GroupValTableEntry*)ent2)->group_id)
    return 1;
  else
    return 0;
}


int
create_sgroupvar_table(void)
{
  int tab_size = total_groupvar_entries * total_runprof_entries;
  int i;
  //int group_table_fd;
  int groupval_idx;
  //int group_num;
  int j;

  NSDL2_MISC(NULL, NULL, "Method called");
  //GroupValTable has values for first vars for all groups and than second var for all groups ..
  //Sort each block of values  for each group var on the order of group -id
  for (i = 0; i < total_groupvar_entries; i++) {
    qsort(&groupValTable[groupVarTable[i].start], groupVarTable[i].length, sizeof(GroupValTableEntry), groupval_cmp);
  }

  if (tab_size) {
     //This SHM seg would have two tables
#if 0
    if ((group_table_fd = do_shmget(shm_base + total_num_shared_segs, sizeof(GroupValTableEntry_Shr) * tab_size + sizeof(char*) * total_groupvar_entries, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
      fprintf(stderr, "error in allocating sharmed mem for the group table\n");
      return -1;
    }

    total_num_shared_segs++;

    if ((rungroup_table_shr_mem = (GroupValTableEntry_Shr*)shmat(group_table_fd, NULL, 0))) {
#endif

    rungroup_table_shr_mem = (GroupValTableEntry_Shr*) do_shmget(sizeof(GroupValTableEntry_Shr) * tab_size + sizeof(char*) * total_groupvar_entries, "group table");
      //Set the start address of second table in this shm
      rungroup_name_table_shr_mem = (char**)((char*) rungroup_table_shr_mem + (sizeof(GroupValTableEntry_Shr) * tab_size));

      //Note that the groupValTable is sorted in for each group var in scenrio group order
      //For each grop var
      for (i = 0; i < total_groupvar_entries; i++) {
	//For each scenrio grouo
	for (j = 0; j < total_runprof_entries; j++) {
		groupval_idx = i*total_runprof_entries + j;
	  	if (groupValTable[groupval_idx].group_id != j)  {
		    //Group is missiong - quite possibly some other group got duplicated
                    NS_EXIT(-1, CAV_ERR_1031042, RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));
		} else {
	            rungroup_table_shr_mem[groupval_idx].value = BIG_BUF_MEMORY_CONVERSION(groupValTable[groupval_idx].value);
	            rungroup_table_shr_mem[groupval_idx].length = strlen(rungroup_table_shr_mem[groupval_idx].value);
		}
	 }
      }

      for (i = 0; i < total_groupvar_entries; i++) {
	rungroup_name_table_shr_mem[i] = BIG_BUF_MEMORY_CONVERSION(groupVarTable[i].name);
      }
    //} else
     // return -1;
  }
  return 0;
}

static void copy_http_or_https_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d, request_type = %d", i, requests[i].request_type);

  request_table_shr_mem[i].request_type = requests[i].request_type;
  request_table_shr_mem[i].hdr_flags = requests[i].hdr_flags;
  request_table_shr_mem[i].flags = requests[i].flags;

  request_table_shr_mem[i].proto.http.http_version = requests[i].proto.http.http_version;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  } else {
    //if (requests[i].index.svr_idx == -1) {
    if (requests[i].index.svr_idx < 0) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    } else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  NSDL2_SCHEDULE(NULL, NULL, "i = %d, url_without_path.seg_start = %d, url_without_path.num_entries = %d", 
                              i, requests[i].proto.http.url_without_path.seg_start, requests[i].proto.http.url_without_path.num_entries);
  if (requests[i].proto.http.url_without_path.seg_start == -1) {
    request_table_shr_mem[i].proto.http.url_without_path.seg_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.url_without_path.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.url_without_path.seg_start);
    request_table_shr_mem[i].proto.http.url_without_path.num_entries = requests[i].proto.http.url_without_path.num_entries;
  }

  if (requests[i].proto.http.url.seg_start == -1) {
    request_table_shr_mem[i].proto.http.url.seg_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.url.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.url.seg_start);
  }

  if (requests[i].proto.http.hdrs.seg_start == -1) {
    request_table_shr_mem[i].proto.http.hdrs.seg_start = NULL;
  } else {
    NSDL2_SCHEDULE(NULL, NULL, "i = %d, hdrs.num_entries = %d", i, requests[i].proto.http.hdrs.num_entries);
    request_table_shr_mem[i].proto.http.hdrs.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.hdrs.seg_start);
    request_table_shr_mem[i].proto.http.hdrs.num_entries = requests[i].proto.http.hdrs.num_entries;
  }

  if (requests[i].proto.http.auth_uname.seg_start == -1) {
    request_table_shr_mem[i].proto.http.auth_uname.seg_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.auth_uname.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.auth_uname.seg_start);
  }

  if (requests[i].proto.http.auth_pwd.seg_start == -1) {
    request_table_shr_mem[i].proto.http.auth_pwd.seg_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.auth_pwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.auth_pwd.seg_start);
  }

  if (requests[i].proto.http.repeat_inline.seg_start == -1) {
    request_table_shr_mem[i].proto.http.repeat_inline.seg_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.repeat_inline.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.repeat_inline.seg_start);
  }

  if (requests[i].proto.http.post_idx == -1) {
   request_table_shr_mem[i].proto.http.post_ptr = NULL;
  } else {
    request_table_shr_mem[i].proto.http.post_ptr = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.http.post_idx);
  }

  if (requests[i].proto.http.cookies.cookie_start == -1) {
    request_table_shr_mem[i].proto.http.cookies.cookie_start = NULL;
  } else {
   request_table_shr_mem[i].proto.http.cookies.cookie_start = REQCOOK_TABLE_MEMORY_CONVERSION(requests[i].proto.http.cookies.cookie_start);
  }

  if (requests[i].proto.http.dynvars.dynvar_start == -1) {
    request_table_shr_mem[i].proto.http.dynvars.dynvar_start = NULL;
  } else {
    request_table_shr_mem[i].proto.http.dynvars.dynvar_start = REQDYNVAR_TABLE_MEMORY_CONVERSION(requests[i].proto.http.dynvars.dynvar_start);
  }

  //RBU 
  if(requests[i].proto.http.rbu_param.browser_user_profile != -1)
    request_table_shr_mem[i].proto.http.rbu_param.browser_user_profile = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.browser_user_profile);
  else
    request_table_shr_mem[i].proto.http.rbu_param.browser_user_profile = NULL;

  if(requests[i].proto.http.rbu_param.har_log_dir != -1)
    request_table_shr_mem[i].proto.http.rbu_param.har_log_dir = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.har_log_dir);
  else
    request_table_shr_mem[i].proto.http.rbu_param.har_log_dir = NULL;

  if(requests[i].proto.http.rbu_param.vnc_display_id != -1)  
    request_table_shr_mem[i].proto.http.rbu_param.vnc_display_id = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.vnc_display_id);
  else
    request_table_shr_mem[i].proto.http.rbu_param.vnc_display_id = NULL;

  if(requests[i].proto.http.rbu_param.primary_content_profile != -1)
    request_table_shr_mem[i].proto.http.rbu_param.primary_content_profile = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.primary_content_profile);
  else
    request_table_shr_mem[i].proto.http.rbu_param.primary_content_profile = NULL;
  
  if(requests[i].proto.http.rbu_param.csv_file_name != -1)
    request_table_shr_mem[i].proto.http.rbu_param.csv_file_name = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.csv_file_name);
  else
    request_table_shr_mem[i].proto.http.rbu_param.csv_file_name = NULL;

  request_table_shr_mem[i].proto.http.rbu_param.csv_fd = requests[i].proto.http.rbu_param.csv_fd;

  request_table_shr_mem[i].proto.http.rbu_param.har_rename_flag = requests[i].proto.http.rbu_param.har_rename_flag;
  request_table_shr_mem[i].proto.http.rbu_param.page_load_wait_time = requests[i].proto.http.rbu_param.page_load_wait_time;
  request_table_shr_mem[i].proto.http.rbu_param.timeout_for_next_req = requests[i].proto.http.rbu_param.timeout_for_next_req;
  request_table_shr_mem[i].proto.http.rbu_param.phase_interval_for_page_load = requests[i].proto.http.rbu_param.phase_interval_for_page_load;
  request_table_shr_mem[i].proto.http.rbu_param.merge_har_file = requests[i].proto.http.rbu_param.merge_har_file;
  //Performance trace dump
  request_table_shr_mem[i].proto.http.rbu_param.performance_trace_mode = requests[i].proto.http.rbu_param.performance_trace_mode;
  request_table_shr_mem[i].proto.http.rbu_param.performance_trace_timeout = requests[i].proto.http.rbu_param.performance_trace_timeout;
  request_table_shr_mem[i].proto.http.rbu_param.performance_trace_memory_flag = requests[i].proto.http.rbu_param.performance_trace_memory_flag;
  request_table_shr_mem[i].proto.http.rbu_param.performance_trace_screenshot_flag = requests[i].proto.http.rbu_param.performance_trace_screenshot_flag;
  request_table_shr_mem[i].proto.http.rbu_param.performance_trace_duration_level = requests[i].proto.http.rbu_param.performance_trace_duration_level;

  //Retrieve authentication credential from BIG_BUF_MEMORY
  if(requests[i].proto.http.rbu_param.auth_username != -1)
    request_table_shr_mem[i].proto.http.rbu_param.auth_username = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.auth_username);
  else
    request_table_shr_mem[i].proto.http.rbu_param.auth_username = NULL;

  if(requests[i].proto.http.rbu_param.auth_password != -1)
    request_table_shr_mem[i].proto.http.rbu_param.auth_password = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.auth_password);
  else
    request_table_shr_mem[i].proto.http.rbu_param.auth_password = NULL;

  //Retrieve tti_profile_name from BIG_BUF_MEMORY
  if(requests[i].proto.http.rbu_param.tti_prof)
    request_table_shr_mem[i].proto.http.rbu_param.tti_prof = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.rbu_param.tti_prof);
  else
    request_table_shr_mem[i].proto.http.rbu_param.tti_prof = NULL;

  /* Protobuf */
  request_table_shr_mem[i].proto.http.protobuf_urlattr_shr.req_message = requests[i].proto.http.protobuf_urlattr.req_message;
  request_table_shr_mem[i].proto.http.protobuf_urlattr_shr.resp_message = requests[i].proto.http.protobuf_urlattr.resp_message;
  request_table_shr_mem[i].proto.http.protobuf_urlattr_shr.grpc_comp_type = requests[i].proto.http.protobuf_urlattr.grpc_comp_type;

#ifdef RMI_MODE
  if (requests[i].proto.http.bytevars.bytevar_start == -1) {
    request_table_shr_mem[i].poto.http.bytevars.bytevar_start = NULL;
  } else {
    request_table_shr_mem[i].poto.http.bytevars.bytevar_start = REQBYTEVAR_TABLE_MEMORY_CONVERSION(requests[i].proto.http.bytevars.bytevar_start);
  }
#endif

#ifdef WS_MODE_OLD
  if (requests[i].proto.http.wss_idx == -1) {
    request_table_shr_mem[i].poto.http.wss_ptr = NULL;
  } else {
    request_table_shr_mem[i].poto.http.wss_ptr = WEBSPEC_TABLE_MEMORY_CONVERSION(requests[i].proto.http.wss_idx);
  }

  request_table_shr_mem[i].next = NULL;
  request_table_shr_mem[i].prev = NULL;
#endif
  cache_copy_req_hdr_shr(i);  

  if(requests[i].proto.http.tx_idx != -1)
    request_table_shr_mem[i].proto.http.tx_hash_idx = txTable[requests[i].proto.http.tx_idx].tx_hash_idx;
  else
    request_table_shr_mem[i].proto.http.tx_hash_idx = -1;

  if(requests[i].proto.http.tx_prefix != -1) 
    request_table_shr_mem[i].proto.http.tx_prefix = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.http.tx_prefix);
  else
    request_table_shr_mem[i].proto.http.tx_prefix = NULL;
 
  if(requests[i].proto.http.body_encryption_args.key.seg_start != -1)
    request_table_shr_mem[i].proto.http.body_encryption_args.key.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.body_encryption_args.key.seg_start);
  else
    request_table_shr_mem[i].proto.http.body_encryption_args.key.seg_start = NULL;

  if(requests[i].proto.http.body_encryption_args.ivec.seg_start != -1)
    request_table_shr_mem[i].proto.http.body_encryption_args.ivec.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.http.body_encryption_args.ivec.seg_start);
  else
    request_table_shr_mem[i].proto.http.body_encryption_args.ivec.seg_start = NULL;

  request_table_shr_mem[i].proto.http.body_encryption_args.base64_encode_option = requests[i].proto.http.body_encryption_args.base64_encode_option;
  request_table_shr_mem[i].proto.http.body_encryption_args.encryption_algo = requests[i].proto.http.body_encryption_args.encryption_algo;

  if(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)
  {
    request_table_shr_mem[i].proto.http.sockjs.conn_id = requests[i].proto.http.sockjs.conn_id;
  } 
}

static void copy_smtp_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type    = requests[i].request_type;
  request_table_shr_mem[i].proto.smtp.authentication_type = requests[i].proto.smtp.authentication_type;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  request_table_shr_mem[i].proto.smtp.msg_count_min   = requests[i].proto.smtp.msg_count_min;
  request_table_shr_mem[i].proto.smtp.msg_count_max   = requests[i].proto.smtp.msg_count_max;
  request_table_shr_mem[i].proto.smtp.num_to_emails   = requests[i].proto.smtp.num_to_emails;
  request_table_shr_mem[i].proto.smtp.num_cc_emails   = requests[i].proto.smtp.num_cc_emails;
  request_table_shr_mem[i].proto.smtp.num_bcc_emails  = requests[i].proto.smtp.num_bcc_emails;
  request_table_shr_mem[i].proto.smtp.num_attachments = requests[i].proto.smtp.num_attachments;

  request_table_shr_mem[i].proto.smtp.enable_rand_bytes = 
    requests[i].proto.smtp.enable_rand_bytes;
  request_table_shr_mem[i].proto.smtp.rand_bytes_min = 
    requests[i].proto.smtp.rand_bytes_min;
  request_table_shr_mem[i].proto.smtp.rand_bytes_max  = 
    requests[i].proto.smtp.rand_bytes_max;
  
  if (requests[i].proto.smtp.user_id.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.user_id.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.user_id.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.user_id.seg_start);
  }

  if (requests[i].proto.smtp.passwd.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.passwd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.passwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.passwd.seg_start);
  }

  if (requests[i].proto.smtp.to_emails_idx == -1) {
   request_table_shr_mem[i].proto.smtp.to_emails = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.to_emails = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.to_emails_idx);
  }

  if (requests[i].proto.smtp.cc_emails_idx == -1) {
   request_table_shr_mem[i].proto.smtp.cc_emails = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.cc_emails = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.cc_emails_idx);
  }


  if (requests[i].proto.smtp.bcc_emails_idx == -1) {
   request_table_shr_mem[i].proto.smtp.bcc_emails = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.bcc_emails = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.bcc_emails_idx);
  }

  if (requests[i].proto.smtp.from_email.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.from_email.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.from_email.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.from_email.seg_start);
  }

  if (requests[i].proto.smtp.body_idx == -1) {
   request_table_shr_mem[i].proto.smtp.body_ptr = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.body_ptr = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.body_idx);
  }

/*
  if (requests[i].proto.smtp.subject_idx.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.subject_idx.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.subject_idx.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.subject_idx.seg_start);
  }
*/
/*
  if (requests[i].proto.smtp.msg_count.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.msg_count.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.msg_count.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.msg_count.seg_start);
  }
*/

  if (requests[i].proto.smtp.hdrs.seg_start == -1) {
    request_table_shr_mem[i].proto.smtp.hdrs.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.hdrs.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.hdrs.seg_start);
  }

  if (requests[i].proto.smtp.attachment_idx == -1) {
   request_table_shr_mem[i].proto.smtp.attachment_ptr = NULL;
  }
  else {
    request_table_shr_mem[i].proto.smtp.attachment_ptr = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.smtp.attachment_idx);
  }

/*
  if (requests[i].proto.http.dynvars.dynvar_start == -1) {
    request_table_shr_mem[i].proto.http.dynvars.dynvar_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.http.dynvars.dynvar_start = REQDYNVAR_TABLE_MEMORY_CONVERSION(requests[i].proto.http.dynvars.dynvar_start);
  }
*/
}

static void copy_pop3_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type    = requests[i].request_type;
  request_table_shr_mem[i].proto.pop3.pop3_action_type =
    requests[i].proto.pop3.pop3_action_type;

  request_table_shr_mem[i].proto.pop3.authentication_type = requests[i].proto.pop3.authentication_type;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  if (requests[i].proto.pop3.user_id.seg_start == -1) { /* Should not be; 
                                                         * make a check and exit.*/
    request_table_shr_mem[i].proto.pop3.user_id.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.pop3.user_id.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.pop3.user_id.seg_start);
  }

  if (requests[i].proto.pop3.passwd.seg_start == -1) {
    request_table_shr_mem[i].proto.pop3.passwd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.pop3.passwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.pop3.passwd.seg_start);
  }

}

static void copy_imap_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type    = requests[i].request_type;
  request_table_shr_mem[i].proto.imap.imap_action_type = requests[i].proto.imap.imap_action_type;
  request_table_shr_mem[i].proto.imap.authentication_type = requests[i].proto.imap.authentication_type;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  if (requests[i].proto.imap.user_id.seg_start == -1) {
    request_table_shr_mem[i].proto.imap.user_id.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.imap.user_id.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.imap.user_id.seg_start);
  }

  if (requests[i].proto.imap.passwd.seg_start == -1) {
    request_table_shr_mem[i].proto.imap.passwd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.imap.passwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.imap.passwd.seg_start);
  }

  if (requests[i].proto.imap.mail_seq.seg_start == -1) {
    request_table_shr_mem[i].proto.imap.mail_seq.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.imap.mail_seq.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.imap.mail_seq.seg_start);
  }

  if (requests[i].proto.imap.fetch_part.seg_start == -1) {
    request_table_shr_mem[i].proto.imap.fetch_part.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.imap.fetch_part.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.imap.fetch_part.seg_start);
  }
}

static void copy_ftp_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type    = requests[i].request_type;
  request_table_shr_mem[i].proto.ftp.ftp_action_type = requests[i].proto.ftp.ftp_action_type;
  request_table_shr_mem[i].proto.ftp.file_type = requests[i].proto.ftp.file_type;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  if (requests[i].proto.ftp.user_id.seg_start == -1) {
    request_table_shr_mem[i].proto.ftp.user_id.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ftp.user_id.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ftp.user_id.seg_start);
  }

  if (requests[i].proto.ftp.passwd.seg_start == -1) {
    request_table_shr_mem[i].proto.ftp.passwd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ftp.passwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ftp.passwd.seg_start);
  }

  if (requests[i].proto.ftp.ftp_cmd.seg_start == -1) {
    request_table_shr_mem[i].proto.ftp.ftp_cmd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ftp.ftp_cmd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ftp.ftp_cmd.seg_start);
  }


  request_table_shr_mem[i].proto.ftp.num_get_files = requests[i].proto.ftp.num_get_files;

  if (requests[i].proto.ftp.get_files_idx == -1) {
   request_table_shr_mem[i].proto.ftp.get_files_idx = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ftp.get_files_idx = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.ftp.get_files_idx);
  }
}

static void copy_jrmi_req_to_shr(int i)          //for jrmi 
{

  NSDL2_SCHEDULE(NULL,NULL, "Method called , i = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;
  request_table_shr_mem[i].proto.jrmi.jrmi_protocol = requests[i].proto.jrmi.jrmi_protocol;
  request_table_shr_mem[i].proto.jrmi.port = requests[i].proto.jrmi.port;
  request_table_shr_mem[i].proto.jrmi.no_param = requests[i].proto.jrmi.no_param;


  if (requests[i].server_base != -1) {
     request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
     request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
      request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  strcpy(request_table_shr_mem[i].proto.jrmi.method, requests[i].proto.jrmi.method);

  if (requests[i].proto.jrmi.post_idx == -1) {
   request_table_shr_mem[i].proto.jrmi.post_ptr = NULL;
  } else {
    request_table_shr_mem[i].proto.jrmi.post_ptr = POST_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.post_idx);
    NSDL2_SCHEDULE(NULL, NULL, "jrmi post ptr = %p" ,request_table_shr_mem[i].proto.jrmi.post_ptr);
  }
/*  if (requests[i].proto.jrmi.method.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.method.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.method.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.method.seg_start);
  }*/

  if (requests[i].proto.jrmi.object_id.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.object_id.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.object_id.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.object_id.seg_start);
  }

  if (requests[i].proto.jrmi.number.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.number.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.number.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.number.seg_start);
  }

  if (requests[i].proto.jrmi.time.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.time.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.time.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.time.seg_start);
  }

  if (requests[i].proto.jrmi.count.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.count.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.count.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.count.seg_start);
  }

  if (requests[i].proto.jrmi.method_hash.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.method_hash.seg_start = NULL;
  }
  else {
    NSDL2_SCHEDULE(NULL, NULL, "Method called, request_table_shr_mem[i].proto.jrmi.method_hash.num_entries = %d, i = %d", requests[i].proto.jrmi.method_hash.seg_start, i);
    request_table_shr_mem[i].proto.jrmi.method_hash.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.method_hash.seg_start);
  }

  if (requests[i].proto.jrmi.operation.seg_start == -1) {
    request_table_shr_mem[i].proto.jrmi.operation.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.jrmi.operation.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.jrmi.operation.seg_start);
  }
}

static void copy_ws_req_to_shr(int i)  //for websocket
{
  NSDL2_SCHEDULE(NULL,NULL, "Method called , i = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;

  if(requests[i].server_base != -1) 
  {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else 
  {          
    if(requests[i].index.svr_idx == -1) 
      request_table_shr_mem[i].index.svr_ptr = NULL;
    else 
      request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);

    request_table_shr_mem[i].server_base = NULL;
  }
  if(requests[i].is_url_parameterized)
  {
    if(requests[i].proto.ws.uri_without_path.seg_start == -1) 
      request_table_shr_mem[i].proto.ws.uri_without_path.seg_start = NULL;
    else 
    {
      request_table_shr_mem[i].proto.ws.uri_without_path.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ws.uri_without_path.seg_start);
      request_table_shr_mem[i].proto.ws.uri_without_path.num_entries = requests[i].proto.ws.uri_without_path.num_entries;
    } 
  }
  if(requests[i].proto.ws.uri.seg_start == -1) 
    request_table_shr_mem[i].proto.ws.uri.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.ws.uri.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ws.uri.seg_start);
    request_table_shr_mem[i].proto.ws.uri.num_entries = requests[i].proto.ws.uri.num_entries;
  } 


  //Atul: get segment table offset into pointer
  request_table_shr_mem[i].proto.ws.conn_id = requests[i].proto.ws.conn_id;


  if(requests[i].proto.ws.origin == -1)
    request_table_shr_mem[i].proto.ws.origin = NULL;
  else 
    request_table_shr_mem[i].proto.ws.origin = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.ws.origin);

  if(requests[i].proto.ws.hdrs.seg_start == -1) 
    request_table_shr_mem[i].proto.ws.hdrs.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.ws.hdrs.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ws.hdrs.seg_start);
    request_table_shr_mem[i].proto.ws.hdrs.num_entries = requests[i].proto.ws.hdrs.num_entries;
  } 

  NSDL3_SCHEDULE(NULL, NULL, "WS Shr Dump(request_table_shr_mem): req_id = %d, conn_id = %d, uri (seg_start = %p, num_entries = %d), "
                             "hdrs (seg_start = %p, num_entries = %d), origin = %s, opencb_idx = %d, sendcb_idx = %d, msgcb_idx = %d, "
                             "errorcb_idx = %d", 
                              i, request_table_shr_mem[i].proto.ws.conn_id, 
                              request_table_shr_mem[i].proto.ws.uri.seg_start, request_table_shr_mem[i].proto.ws.uri.num_entries,
                              request_table_shr_mem[i].proto.ws.hdrs.seg_start, request_table_shr_mem[i].proto.ws.hdrs.num_entries, 
                              request_table_shr_mem[i].proto.ws.origin, request_table_shr_mem[i].proto.ws.opencb_idx,
                              request_table_shr_mem[i].proto.ws.sendcb_idx, request_table_shr_mem[i].proto.ws.msgcb_idx,
                              request_table_shr_mem[i].proto.ws.errorcb_idx); 

  //calling websocket send and close table to shared memory
  copy_websocket_send_table_to_shr();
  copy_websocket_close_table_to_shr();
}

static void copy_dns_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type    = requests[i].request_type;
  request_table_shr_mem[i].proto.dns.proto    = requests[i].proto.dns.proto;  // UDP/TCP

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  if (requests[i].proto.dns.name.seg_start == -1) { /* Should not be; 
                                                         * make a check and exit.*/
    request_table_shr_mem[i].proto.dns.name.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.dns.name.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.dns.name.seg_start);
  }

  request_table_shr_mem[i].proto.dns.recursive   = requests[i].proto.dns.recursive;
  request_table_shr_mem[i].proto.dns.qtype   = requests[i].proto.dns.qtype;

  if (requests[i].proto.dns.assert_rr_type.seg_start == -1) {
    request_table_shr_mem[i].proto.dns.assert_rr_type.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.dns.assert_rr_type.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.dns.assert_rr_type.seg_start);
  }

  if (requests[i].proto.dns.assert_rr_data.seg_start == -1) {
    request_table_shr_mem[i].proto.dns.assert_rr_data.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.dns.assert_rr_data.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.dns.assert_rr_data.seg_start);
  }
}

static void copy_xmpp_req_to_shr(int i)  //for xmpp
{
  NSDL2_SCHEDULE(NULL,NULL, "Method called , i = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;

  if(requests[i].server_base != -1) 
  {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else 
  {          
    if(requests[i].index.svr_idx == -1) 
      request_table_shr_mem[i].index.svr_ptr = NULL;
    else 
      request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);

    request_table_shr_mem[i].server_base = NULL;
  }

  request_table_shr_mem[i].proto.xmpp.starttls = requests[i].proto.xmpp.starttls;
  request_table_shr_mem[i].proto.xmpp.user_type = requests[i].proto.xmpp.user_type;
  request_table_shr_mem[i].proto.xmpp.action = requests[i].proto.xmpp.action;
  request_table_shr_mem[i].proto.xmpp.accept_contact = requests[i].proto.xmpp.accept_contact;

  if(requests[i].proto.xmpp.user.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.user.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.user.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.user.seg_start);
    request_table_shr_mem[i].proto.xmpp.user.num_entries = requests[i].proto.xmpp.user.num_entries;
  } 
  
  if(requests[i].proto.xmpp.password.seg_start == -1)
    request_table_shr_mem[i].proto.xmpp.password.seg_start = NULL;
  else 
    request_table_shr_mem[i].proto.xmpp.password.seg_start =SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.password.seg_start);
    request_table_shr_mem[i].proto.xmpp.password.num_entries = requests[i].proto.xmpp.password.num_entries;
  
  if(requests[i].proto.xmpp.domain.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.domain.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.domain.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.domain.seg_start);
    request_table_shr_mem[i].proto.xmpp.domain.num_entries = requests[i].proto.xmpp.domain.num_entries;
  } 
  if(requests[i].proto.xmpp.sasl_auth_type.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.sasl_auth_type.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.sasl_auth_type.seg_start = 
                                          SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.sasl_auth_type.seg_start);
    request_table_shr_mem[i].proto.xmpp.sasl_auth_type.num_entries = requests[i].proto.xmpp.sasl_auth_type.num_entries;
  } 

  if(requests[i].proto.xmpp.message.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.message.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.message.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.message.seg_start);
    request_table_shr_mem[i].proto.xmpp.message.num_entries = requests[i].proto.xmpp.message.num_entries;
  } 

  if(requests[i].proto.xmpp.file.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.file.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.file.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.file.seg_start);
    request_table_shr_mem[i].proto.xmpp.file.num_entries = requests[i].proto.xmpp.file.num_entries;
  } 

  if(requests[i].proto.xmpp.group.seg_start == -1) 
    request_table_shr_mem[i].proto.xmpp.group.seg_start = NULL;
  else 
  {
    request_table_shr_mem[i].proto.xmpp.group.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.xmpp.group.seg_start);
    request_table_shr_mem[i].proto.xmpp.group.num_entries = requests[i].proto.xmpp.group.num_entries;
  } 

  NSDL3_SCHEDULE(NULL, NULL,"XMPP Shr Dump(request_table_shr_mem): idx = %d,"
                            "user (seg_start = %p, num_entries = %d),"
                            "password (seg_start = %p, num_entries = %d), "
                            "domain (seg_start = %p, num_entries = %d)," 
                            "sasl_auth_type (seg_start = %p, num_entries = %d),"
                            "starttls = %d, message (seg_start = %p, num_entries = %d), "
                            "file (seg_start = %p, num_entries = %d),"
                            "group (seg_start = %p, num_entries = %d),"
                            "user_type = %d", 
                            i, 
                            request_table_shr_mem[i].proto.xmpp.user.seg_start, 
                            request_table_shr_mem[i].proto.xmpp.user.num_entries,
                            request_table_shr_mem[i].proto.xmpp.password.seg_start, 
                            request_table_shr_mem[i].proto.xmpp.password.num_entries,
                            request_table_shr_mem[i].proto.xmpp.domain.seg_start, 
                            request_table_shr_mem[i].proto.xmpp.domain.num_entries,
                            request_table_shr_mem[i].proto.xmpp.sasl_auth_type.seg_start, 
                            request_table_shr_mem[i].proto.xmpp.sasl_auth_type.num_entries,
                            request_table_shr_mem[i].proto.xmpp.starttls, 
                            request_table_shr_mem[i].proto.xmpp.message.seg_start, request_table_shr_mem[i].proto.xmpp.message.num_entries,
                            request_table_shr_mem[i].proto.xmpp.file.seg_start, request_table_shr_mem[i].proto.xmpp.file.num_entries,
                            request_table_shr_mem[i].proto.xmpp.group.seg_start, request_table_shr_mem[i].proto.xmpp.group.num_entries,
                            request_table_shr_mem[i].proto.xmpp.user_type);
}

static void copy_fc2_req_to_shr(int i)  //for FC2
{
  NSDL2_SCHEDULE(NULL,NULL, "Method called , i = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;

  if(requests[i].server_base != -1)
  {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else
  {
    if(requests[i].index.svr_idx == -1)
      request_table_shr_mem[i].index.svr_ptr = NULL;
    else
      request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);

    request_table_shr_mem[i].server_base = NULL;
  }
    
  if(requests[i].proto.fc2_req.uri.seg_start == -1)
    request_table_shr_mem[i].proto.fc2_req.uri.seg_start = NULL;
  else
  {
    request_table_shr_mem[i].proto.fc2_req.uri.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.fc2_req.uri.seg_start);
    request_table_shr_mem[i].proto.fc2_req.uri.num_entries = requests[i].proto.fc2_req.uri.num_entries;
  }

  if(requests[i].proto.fc2_req.message.seg_start == -1)
    request_table_shr_mem[i].proto.fc2_req.uri.seg_start = NULL;
  else
  {
    request_table_shr_mem[i].proto.fc2_req.message.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.fc2_req.message.seg_start);
    request_table_shr_mem[i].proto.fc2_req.message.num_entries = requests[i].proto.fc2_req.message.num_entries;
  }


  NSDL3_SCHEDULE(NULL, NULL,"FC2 Shr Dump(request_table_shr_mem): idx = %d,"
                            "uri (seg_start = %p, num_entries = %d),"
                            "message (seg_start = %p, num_entries = %d), ",
                            i,
                            request_table_shr_mem[i].proto.fc2_req.uri.seg_start,
                            request_table_shr_mem[i].proto.fc2_req.uri.num_entries,
                            request_table_shr_mem[i].proto.fc2_req.message.seg_start,
                            request_table_shr_mem[i].proto.fc2_req.message.num_entries);
}

static void copy_ldap_req_to_shr(int i)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, i = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;
  request_table_shr_mem[i].proto.ldap.operation = requests[i].proto.ldap.operation;
  request_table_shr_mem[i].proto.ldap.type = requests[i].proto.ldap.type;

  if (requests[i].server_base != -1) {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else {
    if (requests[i].index.svr_idx == -1) {
      request_table_shr_mem[i].index.svr_ptr = NULL;
    }
    else {
     request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);
    }
    request_table_shr_mem[i].server_base = NULL;
  }

  if (requests[i].proto.ldap.dn.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.dn.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.dn.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.dn.seg_start);
  }

  if (requests[i].proto.ldap.username.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.username.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.username.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.username.seg_start);
  }

  if (requests[i].proto.ldap.passwd.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.passwd.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.passwd.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.passwd.seg_start);
  }

  if (requests[i].proto.ldap.scope.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.scope.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.scope.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.scope.seg_start);
  }

  if (requests[i].proto.ldap.filter.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.filter.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.filter.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.filter.seg_start);
  }

  if (requests[i].proto.ldap.base.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.base.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.base.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.base.seg_start);
  }

  if (requests[i].proto.ldap.deref_aliases.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.deref_aliases.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.deref_aliases.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.deref_aliases.seg_start);
  }

  if (requests[i].proto.ldap.time_limit.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.time_limit.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.time_limit.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.time_limit.seg_start);
  }

  if (requests[i].proto.ldap.size_limit.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.size_limit.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.size_limit.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.size_limit.seg_start);
  }

  if (requests[i].proto.ldap.types_only.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.types_only.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.types_only.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.types_only.seg_start);
  }

  if (requests[i].proto.ldap.mode.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.mode.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.mode.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.mode.seg_start);
  }

  if (requests[i].proto.ldap.attributes.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.attributes.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.attributes.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.attributes.seg_start);
  }

  if (requests[i].proto.ldap.attr_value.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.attr_value.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.attr_value.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.attr_value.seg_start);
  }

  if (requests[i].proto.ldap.attr_name.seg_start == -1) {
    request_table_shr_mem[i].proto.ldap.attr_name.seg_start = NULL;
  }
  else {
    request_table_shr_mem[i].proto.ldap.attr_name.seg_start = SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.ldap.attr_name.seg_start);
  }
}

static void copy_socket_req_to_shr(int i)  //for tcpsocket
{
  NSDL2_SCHEDULE(NULL,NULL, "Method called , Copy Non-Shared socket data into shared memory, socket req = %d", i);

  request_table_shr_mem[i].request_type = requests[i].request_type;

  if(requests[i].server_base != -1)
  {
    request_table_shr_mem[i].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[i].server_base);
    request_table_shr_mem[i].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[i].index.group_idx);
  }
  else
  {
    if(requests[i].index.svr_idx == -1)
      request_table_shr_mem[i].index.svr_ptr = NULL;
    else
      request_table_shr_mem[i].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[i].index.svr_idx);

    request_table_shr_mem[i].server_base = NULL;
  }

  // Copy common members 
  request_table_shr_mem[i].proto.socket.norm_id = requests[i].proto.socket.norm_id;
  request_table_shr_mem[i].proto.socket.operation = requests[i].proto.socket.operation;
  request_table_shr_mem[i].proto.socket.flag = requests[i].proto.socket.flag;
  request_table_shr_mem[i].proto.socket.timeout_msec = requests[i].proto.socket.timeout_msec;
  request_table_shr_mem[i].proto.socket.enc_dec_cb = requests[i].proto.socket.enc_dec_cb;

  // Open API
  if(requests[i].proto.socket.operation == SOPEN)
  {
    request_table_shr_mem[i].proto.socket.open.protocol = requests[i].proto.socket.open.protocol;
    request_table_shr_mem[i].proto.socket.open.ssl_flag = requests[i].proto.socket.open.ssl_flag;

    if(requests[i].proto.socket.open.remote_host.seg_start == -1)
      request_table_shr_mem[i].proto.socket.open.remote_host.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.open.remote_host.seg_start = 
         SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.open.remote_host.seg_start);
      request_table_shr_mem[i].proto.socket.open.remote_host.num_entries = requests[i].proto.socket.open.remote_host.num_entries;
    }

    if(requests[i].proto.socket.open.local_host.seg_start == -1)
      request_table_shr_mem[i].proto.socket.open.local_host.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.open.local_host.seg_start = 
         SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.open.local_host.seg_start);
      request_table_shr_mem[i].proto.socket.open.local_host.num_entries = requests[i].proto.socket.open.local_host.num_entries;
    }

    request_table_shr_mem[i].proto.socket.open.backlog = requests[i].proto.socket.open.backlog;
  }
  else if(requests[i].proto.socket.operation == SSEND)
  {
    request_table_shr_mem[i].proto.socket.send.msg_fmt.len_bytes = requests[i].proto.socket.send.msg_fmt.len_bytes;
    request_table_shr_mem[i].proto.socket.send.msg_fmt.len_type = requests[i].proto.socket.send.msg_fmt.len_type;
    request_table_shr_mem[i].proto.socket.send.msg_fmt.len_endian = requests[i].proto.socket.send.msg_fmt.len_endian;
    request_table_shr_mem[i].proto.socket.send.msg_fmt.msg_type = requests[i].proto.socket.send.msg_fmt.msg_type;
    request_table_shr_mem[i].proto.socket.send.msg_fmt.msg_enc_dec = requests[i].proto.socket.send.msg_fmt.msg_enc_dec;
 
    if(requests[i].proto.socket.send.msg_fmt.prefix.seg_start == -1)
      request_table_shr_mem[i].proto.socket.send.msg_fmt.prefix.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.send.msg_fmt.prefix.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.send.msg_fmt.prefix.seg_start);

      request_table_shr_mem[i].proto.socket.send.msg_fmt.prefix.num_entries = requests[i].proto.socket.send.msg_fmt.prefix.num_entries;
    }
 
    if(requests[i].proto.socket.send.msg_fmt.suffix.seg_start == -1)
      request_table_shr_mem[i].proto.socket.send.msg_fmt.suffix.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.send.msg_fmt.suffix.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.send.msg_fmt.suffix.seg_start);
      request_table_shr_mem[i].proto.socket.send.msg_fmt.suffix.num_entries = requests[i].proto.socket.send.msg_fmt.suffix.num_entries;
    }
 
    if(requests[i].proto.socket.send.buffer.seg_start == -1)
      request_table_shr_mem[i].proto.socket.send.buffer.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.send.buffer.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.send.buffer.seg_start);

      request_table_shr_mem[i].proto.socket.send.buffer.num_entries = requests[i].proto.socket.send.buffer.num_entries;
    }
 
    request_table_shr_mem[i].proto.socket.send.buffer_len = requests[i].proto.socket.send.buffer_len;
  }
  else if(requests[i].proto.socket.operation == SRECV)
  {
    request_table_shr_mem[i].proto.socket.recv.msg_fmt.len_bytes = requests[i].proto.socket.recv.msg_fmt.len_bytes;
    request_table_shr_mem[i].proto.socket.recv.msg_fmt.len_type = requests[i].proto.socket.recv.msg_fmt.len_type;
    request_table_shr_mem[i].proto.socket.recv.msg_fmt.len_endian = requests[i].proto.socket.recv.msg_fmt.len_endian;
    request_table_shr_mem[i].proto.socket.recv.msg_fmt.msg_type = requests[i].proto.socket.recv.msg_fmt.msg_type;
    request_table_shr_mem[i].proto.socket.recv.msg_fmt.msg_enc_dec = requests[i].proto.socket.recv.msg_fmt.msg_enc_dec;
 
    if(requests[i].proto.socket.recv.msg_fmt.prefix.seg_start == -1)
      request_table_shr_mem[i].proto.socket.recv.msg_fmt.prefix.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.recv.msg_fmt.prefix.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.recv.msg_fmt.prefix.seg_start);

      request_table_shr_mem[i].proto.socket.recv.msg_fmt.prefix.num_entries = requests[i].proto.socket.recv.msg_fmt.prefix.num_entries;
    }
 
    if(requests[i].proto.socket.recv.msg_fmt.suffix.seg_start == -1)
      request_table_shr_mem[i].proto.socket.recv.msg_fmt.suffix.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.recv.msg_fmt.suffix.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.recv.msg_fmt.suffix.seg_start);
      request_table_shr_mem[i].proto.socket.recv.msg_fmt.suffix.num_entries = requests[i].proto.socket.recv.msg_fmt.suffix.num_entries;
    }

    NSDL2_SCHEDULE(NULL,NULL, "Copy rmsg_suffix form non shared to shared, "
                   "i = %d, seg_start = %d, num_entries = %d", 
                    i, requests[i].proto.socket.recv.msg_fmt.suffix.seg_start, 
                    request_table_shr_mem[i].proto.socket.recv.msg_fmt.suffix.num_entries);
 
    if(requests[i].proto.socket.recv.buffer == -1)
      request_table_shr_mem[i].proto.socket.recv.buffer = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.recv.buffer = BIG_BUF_MEMORY_CONVERSION(requests[i].proto.socket.recv.buffer);
    }

    if(requests[i].proto.socket.recv.msg_contains.seg_start == -1)
      request_table_shr_mem[i].proto.socket.recv.msg_contains.seg_start = NULL;
    else
    {
      request_table_shr_mem[i].proto.socket.recv.msg_contains.seg_start = 
        SEG_TABLE_MEMORY_CONVERSION(requests[i].proto.socket.recv.msg_contains.seg_start);
      request_table_shr_mem[i].proto.socket.recv.msg_contains.num_entries = requests[i].proto.socket.recv.msg_contains.num_entries;
    }

    request_table_shr_mem[i].proto.socket.recv.buffer_len = requests[i].proto.socket.recv.buffer_len;
    request_table_shr_mem[i].proto.socket.recv.fb_timeout_msec = requests[i].proto.socket.recv.fb_timeout_msec;
    request_table_shr_mem[i].proto.socket.recv.end_policy = requests[i].proto.socket.recv.end_policy;
    request_table_shr_mem[i].proto.socket.recv.msg_contains_ord = requests[i].proto.socket.recv.msg_contains_ord;
    request_table_shr_mem[i].proto.socket.recv.msg_contains_action = requests[i].proto.socket.recv.msg_contains_action;
  }
  else if(requests[i].proto.socket.operation == SCLOSE)
  {
    request_table_shr_mem[i].proto.socket.norm_id = requests[i].proto.socket.norm_id;
  }
  else
  {
    NSDL2_SCHEDULE(NULL,NULL, "[SocketAPI] Error: unknown API Type, hence existing");
    exit(-1);
  }
}

/*******************  START: Bug 79149    *************************************************************************/
#if 0
static void copy_srv_base_to_shr(int idx)
{

  NSDL2_SCHEDULE(NULL,NULL, "Method called. req = %d", idx);
  if(requests[idx].server_base != -1)
  { 
    request_table_shr_mem[idx].server_base = SERVERORDER_TABLE_MEMORY_CONVERSION(requests[idx].server_base);
    request_table_shr_mem[idx].index.group_ptr = GROUP_TABLE_MEMORY_CONVERSION(requests[idx].index.group_idx);
  }
  else
  {
    if(requests[idx].index.svr_idx == -1)
      request_table_shr_mem[idx].index.svr_ptr = NULL;
    else
      request_table_shr_mem[idx].index.svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(requests[idx].index.svr_idx);
    
    request_table_shr_mem[idx].server_base = NULL;
  }
}
#endif

inline void copy_segment_data_to_shr(StrEnt *src, StrEnt_Shr *des_shr)
{
  NSDL2_RDP(NULL,NULL, "Method called");
  NSDL2_RDP(NULL,NULL, "src->seg_start = %d", src->seg_start);
  if(src->seg_start == -1)
    des_shr->seg_start = NULL;
  else
  {
    NSDL2_RDP(NULL,NULL, "assign segment data to shared memory");
    des_shr->seg_start = SEG_TABLE_MEMORY_CONVERSION(src->seg_start);
    des_shr->num_entries = src->num_entries;
  }
}
void copy_rdp_conn_req_to_shr(int idx)
{
 
  NSDL2_RDP(NULL,NULL, "Method called");

  NSDL2_RDP(NULL,NULL, "copy host to shr memory");
  copy_segment_data_to_shr(&(requests[idx].proto.rdp.connect.host), &(request_table_shr_mem[idx].proto.rdp.connect.host));  
  
  NSDL2_RDP(NULL,NULL, "copy user to shr memory");
  copy_segment_data_to_shr(&(requests[idx].proto.rdp.connect.user), &(request_table_shr_mem[idx].proto.rdp.connect.user));  
  
  NSDL2_RDP(NULL,NULL, "copy password to shr memory");
  copy_segment_data_to_shr(&(requests[idx].proto.rdp.connect.password), &(request_table_shr_mem[idx].proto.rdp.connect.password));  
  
  NSDL2_RDP(NULL,NULL, "copy domain to shr memory");
  copy_segment_data_to_shr(&(requests[idx].proto.rdp.connect.domain), &(request_table_shr_mem[idx].proto.rdp.connect.domain));  
  NSDL3_SCHEDULE(NULL, NULL,"RDP Shr Dump(request_table_shr_mem): idx = %d, user (seg_start = %p, num_entries = %d), password (seg_start = %p, num_entries = %d), domain (seg_start = %p, num_entries = %d), host (seg_start = %p, num_entries = %d)",
                            idx, 
                            request_table_shr_mem[idx].proto.rdp.connect.user.seg_start, 
                            request_table_shr_mem[idx].proto.rdp.connect.user.num_entries,
                            request_table_shr_mem[idx].proto.rdp.connect.password.seg_start, 
                            request_table_shr_mem[idx].proto.rdp.connect.password.num_entries,
                            request_table_shr_mem[idx].proto.rdp.connect.domain.seg_start, 
                            request_table_shr_mem[idx].proto.rdp.connect.domain.num_entries,
                            request_table_shr_mem[idx].proto.rdp.connect.host.seg_start, 
                            request_table_shr_mem[idx].proto.rdp.connect.host.num_entries );
}

void copy_mouse_data_to_shr(ns_mouse * req_ptr, ns_mouse_Shr *shr_ptr)
{
   NSDL2_RDP(NULL,NULL, "Method called. req_ptr = %p, shr_ptr = %p", req_ptr, shr_ptr);
   shr_ptr->x_pos	= req_ptr->x_pos;
   shr_ptr->y_pos	= req_ptr->y_pos; 
   shr_ptr->button_type	= req_ptr->button_type;
   shr_ptr->origin	= req_ptr->origin;
   NSDL2_RDP(NULL,NULL, "shr memory x = %d, y = %d, button = %d, origin = %d", shr_ptr->x_pos,  shr_ptr->y_pos,  shr_ptr->button_type, shr_ptr->origin );
}


static void copy_rdp_req_to_shr(int idx)  //for RDP bug 79149
{
  NSDL2_RDP(NULL,NULL, "Method called , idx = %d", idx);

  request_table_shr_mem[idx].request_type = requests[idx].request_type;

  //Copy common members 
  request_table_shr_mem[idx].proto.rdp.norm_id = requests[idx].proto.rdp.norm_id;
  request_table_shr_mem[idx].proto.rdp.operation = requests[idx].proto.rdp.operation;
  NSDL2_RDP(NULL,NULL, "Method called , operation = %d", requests[idx].proto.rdp.operation);
  switch(requests[idx].proto.rdp.operation)
  {
    case NS_RDP_CONNECT:
    copy_rdp_conn_req_to_shr(idx);
    break;

    case NS_KEY:
    {

      NSDL2_RDP(NULL,NULL, "copy key to shr memory");
      copy_segment_data_to_shr(&(requests[idx].proto.rdp.key.key_value), &(request_table_shr_mem[idx].proto.rdp.key.key_value));  
    }
    break;
 
    case NS_KEY_DOWN:
    {
      NSDL2_RDP(NULL,NULL, "copy key_down to shr memory");
      copy_segment_data_to_shr(&(requests[idx].proto.rdp.key_down.key_value), &(request_table_shr_mem[idx].proto.rdp.key_down.key_value));  
    }
    break;

    case NS_KEY_UP:
    {
      NSDL2_RDP(NULL,NULL, "copy key_up data to shr memory");
      copy_segment_data_to_shr(&(requests[idx].proto.rdp.key_up.key_value), &(request_table_shr_mem[idx].proto.rdp.key_up.key_value));  
    }
    break;

    case NS_TYPE:
    {

      NSDL2_RDP(NULL,NULL, "copy ns_type data to shr memory");
      copy_segment_data_to_shr(&(requests[idx].proto.rdp.type.key_value), &(request_table_shr_mem[idx].proto.rdp.type.key_value));  
    }
    break;

    case NS_MOUSE_DOWN:
    {
      NSDL2_RDP(NULL,NULL, "copy mouse_down data to shr memory");
      copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_down, &request_table_shr_mem[idx].proto.rdp.mouse_down);
    }
    break;

    case NS_MOUSE_UP:
    {
      NSDL2_RDP(NULL,NULL, "copy mouse_up data to shr memory");
      copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_up, &request_table_shr_mem[idx].proto.rdp.mouse_up);
    }
    break;

    case NS_MOUSE_CLICK:
    {
      NSDL2_RDP(NULL,NULL, "copy mouse_click data to shr memory");
      copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_click, &request_table_shr_mem[idx].proto.rdp.mouse_click);
    }
    break;

    case NS_MOUSE_DOUBLE_CLICK:
    {
      NSDL2_RDP(NULL,NULL, "copy mouse_double_click data to shr memory");
      copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_double_click, &request_table_shr_mem[idx].proto.rdp.mouse_double_click);
    }
    break;

    case NS_MOUSE_MOVE:
    {
      NSDL2_RDP(NULL,NULL, "copy mouse_move data to shr memory");
      copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_move, &request_table_shr_mem[idx].proto.rdp.mouse_move);
    }
    break;

    case NS_MOUSE_DRAG:
    {
       NSDL2_RDP(NULL,NULL, "copy mouse_move data to shr memory");
       copy_mouse_data_to_shr(&requests[idx].proto.rdp.mouse_move, &request_table_shr_mem[idx].proto.rdp.mouse_move);
       request_table_shr_mem[idx].proto.rdp.mouse_drag.x1_pos = requests[idx].proto.rdp.mouse_drag.x1_pos;
       request_table_shr_mem[idx].proto.rdp.mouse_drag.y1_pos = requests[idx].proto.rdp.mouse_drag.y1_pos;
    }
    break;

    case NS_SYNC:
    request_table_shr_mem[idx].proto.rdp.sync.timeout = requests[idx].proto.rdp.sync.timeout;
  }
}
/*******************  END: Bug 79149    *************************************************************************/

/*Set flag to 0 if any TIMEOUT is diffrent*/
void check_and_set_flag_for_idle_timer ()
{
  int last_value = -1;
  char flag = 1; //  ALL Equal
  int i;

  NSDL2_PARENT(NULL, NULL, "Method Called.");

  //TODO: IMAP: do we need to handle this and why, check it 
  for(i = 0; i < total_runprof_entries; i++) {
    if(last_value != -1) {
      if((last_value != runprof_table_shr_mem[i].gset.idle_secs) ||
         (total_smtp_request_entries && last_value != runprof_table_shr_mem[i].gset.smtp_timeout) ||
         (total_pop3_request_entries && last_value != runprof_table_shr_mem[i].gset.pop3_timeout) ||
         (total_ftp_request_entries && last_value != runprof_table_shr_mem[i].gset.ftp_timeout) ||
         (total_jrmi_request_entries && last_value != runprof_table_shr_mem[i].gset.jrmi_timeout) ||
         (total_imap_request_entries && last_value != runprof_table_shr_mem[i].gset.imap_timeout) ||
         (total_dns_request_entries && last_value != runprof_table_shr_mem[i].gset.dns_timeout)) {
        flag = 0;
        break;
      } else {
        last_value = runprof_table_shr_mem[i].gset.idle_secs; 
      }
    } else {
      last_value = runprof_table_shr_mem[i].gset.idle_secs; 
      if((total_smtp_request_entries && last_value != runprof_table_shr_mem[i].gset.smtp_timeout) ||
         (total_pop3_request_entries && last_value != runprof_table_shr_mem[i].gset.pop3_timeout) ||
         (total_ftp_request_entries && last_value != runprof_table_shr_mem[i].gset.ftp_timeout) ||
         (total_jrmi_request_entries && last_value != runprof_table_shr_mem[i].gset.jrmi_timeout) ||
         (total_imap_request_entries && last_value != runprof_table_shr_mem[i].gset.imap_timeout) ||
         (total_dns_request_entries && last_value != runprof_table_shr_mem[i].gset.dns_timeout)) {
        flag = 0;
        break;
      }
    }
  } //For loop

 for(i = 0; i< total_runprof_entries; i++)
 {
   if((runprof_table_shr_mem[i].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE &&
      runprof_table_shr_mem[i].gset.rampdown_method.option == RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES_USE_IDLE_TIME) ||
     (runprof_table_shr_mem[i].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE &&
      runprof_table_shr_mem[i].gset.rampdown_method.option == RDM_OPTION_HASTEN_COMPLETION_USING_IDLE_TIME)) {

      NSDL2_PARENT(NULL, NULL, "last_value = %d rampdown method time =%d", last_value, runprof_table_shr_mem[i].gset.rampdown_method.time);
      if(last_value != runprof_table_shr_mem[i].gset.rampdown_method.time) {
         flag = 0;
         break;
      }
  }
  }
  global_settings->idle_timeout_all_flag = flag;
}

inline void copy_repeat_block_into_shared_mem(){

  
  NSDL2_PARENT(NULL, NULL, "Method called , total repeat block entries = %d", total_repeat_block_entries);
  if(!total_repeat_block_entries) 
    return;
  int shr_mem_size = total_repeat_block_entries * sizeof(RepeatBlock_Shr);
  repeat_block_shr_mem = do_shmget(shr_mem_size, "repeat_block_shr_mem");
  memset(repeat_block_shr_mem, 0, shr_mem_size);
  int i;
  for(i=0; i < total_repeat_block_entries; i++) {
    repeat_block_shr_mem[i].repeat_count_type = repeatBlock[i].repeat_count_type;
    repeat_block_shr_mem[i].hash_code = repeatBlock[i].hash_code;
    if(repeatBlock[i].data != -1)
      repeat_block_shr_mem[i].data = BIG_BUF_MEMORY_CONVERSION(repeatBlock[i].data);
    if(repeatBlock[i].rep_sep != -1)
      repeat_block_shr_mem[i].rep_sep = BIG_BUF_MEMORY_CONVERSION(repeatBlock[i].rep_sep);
    repeat_block_shr_mem[i].rep_sep_len = repeatBlock[i].rep_sep_len;
    repeat_block_shr_mem[i].repeat_count = repeatBlock[i].repeat_count;
    repeat_block_shr_mem[i].num_repeat_segments = repeatBlock[i].num_repeat_segments;
    repeat_block_shr_mem[i].agg_repeat_segments = repeatBlock[i].agg_repeat_segments;
  }
}

void
copy_structs_into_shared_mem(void)
{
  int i;
  //int attr_tables_size;
  //int line_char_array_size;
  //LineCharEntry* line_char_array_ptr;
  //int user_location_idx;
  //int svr_location_idx;
  //int line_char_idx;
  //void *attr_table_shr_mem;
  void *serverorder_post_table_shr_mem;
  void *tx_page_table_shr_mem;
  int big_buf_shmid;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  if(malloced_sized)
  {
    //printf("freeing data memory--------------\n");
    FREE_AND_MAKE_NULL_EX(data_file_buf, malloced_sized, "malloced memmory", -1);
    malloced_sized = 0;
  }  

  /* Start with the big buf */
  if (used_buffer_space) {
    big_buf_shr_mem = (char*) do_shmget_with_id(used_buffer_space, "big buffer", &big_buf_shmid);
    memcpy(big_buf_shr_mem, g_big_buf, used_buffer_space);
    /* In case of cav_main we don't required this */
    #ifndef CAV_MAIN
    set_test_run_info_big_buff_shm_id(big_buf_shmid);
    #endif
  }

  /* Next up is the pointer table */
  NSDL2_SCHEDULE(NULL, NULL, "total_pointer_entries = %d", total_pointer_entries);
  if (total_pointer_entries) {
    pointer_table_shr_mem = (PointerTableEntry_Shr*) do_shmget(sizeof(PointerTableEntry_Shr) * total_pointer_entries, "Pointer Table");
    for (i = 0; i < total_pointer_entries; i++) 
    {
      NSDL2_SCHEDULE(NULL, NULL, "i = %d, size = %d", i, pointerTable[i].size);
      pointer_table_shr_mem[i].size = pointerTable[i].size;
      if(pointer_table_shr_mem[i].size == -1)
        pointer_table_shr_mem[i].big_buf_pointer = NULL;
      else
        pointer_table_shr_mem[i].big_buf_pointer = BIG_BUF_MEMORY_CONVERSION(pointerTable[i].big_buf_pointer);
      
      /*pointer_table_shr_mem[i].big_buf_pointer = BIG_BUF_MEMORY_CONVERSION(pointerTable[i].big_buf_pointer);
      if (pointerTable[i].size == -1)
        pointer_table_shr_mem[i].size = strlen(pointer_table_shr_mem[i].big_buf_pointer);
      else
        pointer_table_shr_mem[i].size = pointerTable[i].size;*/
    }

    #if 0 
    /*Shared memory pointer table data dump */
    for(i = 0; i < total_pointer_entries; i++){
      NSDL2_SCHEDULE(NULL, NULL, "Shared Memory Data dump: data = %s, size = %d", 
          RETRIEVE_SHARED_BUFFER_DATA(pointer_table_shr_mem[i].big_buf_pointer), pointer_table_shr_mem[i].size);
    }
    #endif
  }
  #if 0
  NSDL2_SCHEDULE(NULL, NULL, "Allocating shared memory for fparamValueTable_shr_mem, total_fparam_entries = %d", total_fparam_entries);
  if(total_fparam_entries)
  {
    char *bb_ptr = NULL;
    NSDL2_SCHEDULE(NULL, NULL, "MANISH: file_param_value_big_buf: buffer = %p, offset = %d, bufsize = %d", 
                                file_param_value_big_buf.buffer, 
                                file_param_value_big_buf.offset, file_param_value_big_buf.bufsize);

    file_param_value_big_buf_shr_mem = (char*) do_shmget(file_param_value_big_buf.offset, "file_param_value_big_buf_shr_mem");
    fparamValueTable_shr_mem = (PointerTableEntry_Shr*) do_shmget(sizeof(PointerTableEntry_Shr) * total_fparam_entries, "fparamValueTable_shr_mem");

    bb_ptr = file_param_value_big_buf_shr_mem;
    int g,v,pointer_idx;
    for(g = 0; g < total_group_entries; g++)
    {
      for(v = 0; v < groupTable[g].num_vars; v++)
      {
        if(varTable[v].is_file != 2)
        {
          for(i = 0; i < groupTable[g].num_values; i++)
          {
            pointer_idx = varTable[groupTable[g].start_var_idx + v].value_start_ptr  + i; 
            memcpy(bb_ptr, 
                 nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[pointer_idx].big_buf_pointer),
                 fparamValueTable[pointer_idx].size + 1);
            fparamValueTable_shr_mem[pointer_idx].big_buf_pointer = bb_ptr; 
            fparamValueTable_shr_mem[pointer_idx].size = fparamValueTable[pointer_idx].size;
            bb_ptr += fparamValueTable[pointer_idx].size + 1;
          }
        }
        else
        { 
          for(i = 0; i < groupTable[g].num_values; i++)
          {
            pointer_idx = varTable[groupTable[g].start_var_idx + v].value_start_ptr  + i; 
            fparamValueTable_shr_mem[pointer_idx].seg_start = SEG_TABLE_MEMORY_CONVERSION(fparamValueTable[pointer_idx].seg_start); 
            fparamValueTable_shr_mem[pointer_idx].num_entries = fparamValueTable[pointer_idx].num_entries;
          }
        }
      }
    }
    //Dump:
    #ifdef NS_DEBUG_ON
    for(i = 0; i < total_fparam_entries; i++)
    {
      NSDL4_SCHEDULE(NULL, NULL, "MM: pointer-> i = %d, size = %d, value = [%s]", 
                                  i, fparamValueTable_shr_mem[i].size, fparamValueTable_shr_mem[i].big_buf_pointer);
    } 
    #endif
  }
  #endif
  /*Event Definition we will make shared memory only if filter mode not DO_NOT_LOG_EVENT*/
  copy_event_def_to_shr_mem(big_buf_shmid);

  create_shm_for_static_index_var();

  //Static var
  //copy_staticvar_into_shared_mem();

  // index var
  copy_indexvar_into_shared_mem();

  //repeat block
  copy_repeat_block_into_shared_mem();

  randomvar_table_shr_mem = copy_randomvar_into_shared_mem();  // nsl_random_number_var()
  randomstring_table_shr_mem = copy_randomstring_into_shared_mem();  
  uniquevar_table_shr_mem = copy_uniquevar_into_shared_mem();
  datevar_table_shr_mem = copy_datevar_into_shared_mem(); 
/* Up next is gServerTable */
  if (total_svr_entries) {
    gserver_table_shr_mem = (SvrTableEntry_Shr*) do_shmget(sizeof(SvrTableEntry_Shr) * total_svr_entries, " gServer Table");
      for (i = 0; i < total_svr_entries; i++) {
	gserver_table_shr_mem[i].server_hostname = BIG_BUF_MEMORY_CONVERSION(gServerTable[i].server_hostname);
	gserver_table_shr_mem[i].server_hostname_len = strlen(BIG_BUF_MEMORY_CONVERSION(gServerTable[i].server_hostname)); // Added by Anuj 08/03/08 

	gserver_table_shr_mem[i].idx = gServerTable[i].idx;
	//gserver_table_shr_mem[i].server_ip = gServerTable[i].server_ip;
	//gserver_table_shr_mem[i].saddr = gServerTable[i].saddr;
	gserver_table_shr_mem[i].server_port = gServerTable[i].server_port;
	gserver_table_shr_mem[i].type = gServerTable[i].type;
	gserver_table_shr_mem[i].tls_version = gServerTable[i].tls_version;
      }
  }

  /* Group default settings shr mem. */
  GroupSettings *tmp_gset = (GroupSettings *) do_shmget(sizeof(GroupSettings), "Group Settings Shr"); 
  memcpy(tmp_gset, group_default_settings, sizeof(GroupSettings));
  FREE_AND_MAKE_NULL_EX(group_default_settings, sizeof(GroupSettings), "group default settings non shared", -1);
  group_default_settings = tmp_gset;

  /* Global_data global_settings */
  Global_data *tmp_glob_set = (Global_data *) do_shmget(sizeof(Global_data), "Global data Shr");
  memcpy(tmp_glob_set, global_settings, sizeof(Global_data));
  FREE_AND_MAKE_NULL_EX(global_settings, sizeof(Global_data), "global settings non shared", -1);
  global_settings = tmp_glob_set;
  
  char *var = NULL;
  /* now the seg table */
  if (total_seg_entries) {
    seg_table_shr_mem = (void *) do_shmget(sizeof(SegTableEntry_Shr) * total_seg_entries, "seg Tabel");
      for (i = 0; i < total_seg_entries; i++) {
	switch (segTable[i].type) {
	case STR:
	case PROTOBUF_MSG:
	  seg_table_shr_mem[i].seg_ptr.str_ptr = POINTER_TABLE_MEMORY_CONVERSION(segTable[i].offset);
	  break;

	case VAR:
          if(gSessionTable[segTable[i].sess_idx].var_hash_func)
          {
            var = BIG_BUF_MEMORY_CONVERSION(varTable[segTable[i].offset].name_pointer);
	    //seg_table_shr_mem[i].seg_ptr.var_ptr = VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
	    seg_table_shr_mem[i].seg_ptr.fparam_hash_code = gSessionTable[segTable[i].sess_idx].var_hash_func(var, strlen(var));
            NSDL2_SCHEDULE(NULL, NULL, "MANISH: var = %s, var_hash_code = %d", var, seg_table_shr_mem[i].seg_ptr.fparam_hash_code);
          }
          else
            seg_table_shr_mem[i].seg_ptr.fparam_hash_code = -1; //File param hash code not found
	  break;
	case RANDOM_VAR:
	  seg_table_shr_mem[i].seg_ptr.random_ptr = RANDOM_VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
	  break;
         case RANDOM_STRING:
          seg_table_shr_mem[i].seg_ptr.random_str = RANDOM_STRING_TABLE_MEMORY_CONVERSION(segTable[i].offset);
          break;
          case UNIQUE_VAR:
          seg_table_shr_mem[i].seg_ptr.unique_ptr = UNIQUE_VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
          break;
          case UNIQUE_RANGE_VAR: {
	  char* unique_range_var = RETRIEVE_BUFFER_DATA(uniquerangevarTable[segTable[i].offset].name);
	  int var_idx = gSessionTable[segTable[i].sess_idx].var_hash_func(unique_range_var, strlen(unique_range_var));
	  assert (var_idx != -1);
	  assert (gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].var_type == UNIQUE_RANGE_VAR);
	  seg_table_shr_mem[i].seg_ptr.var_idx = gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
          if(segTable[i].data != -1)
            seg_table_shr_mem[i].data = (void *)BIG_BUF_MEMORY_CONVERSION(segTable[i].data);
          else
            seg_table_shr_mem[i].data = NULL;
          NSDL2_SCHEDULE(NULL, NULL, "unique_range_var = %s seg_table_shr_mem[i].seg_ptr.var_idx = %d", unique_range_var, seg_table_shr_mem[i].seg_ptr.var_idx);
	  break;
         }
	case INDEX_VAR:
          //TODO: change to support RTC
	  seg_table_shr_mem[i].seg_ptr.var_ptr = INDEX_VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
	  break;
        case DATE_VAR:
          seg_table_shr_mem[i].seg_ptr.date_ptr = DATE_VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
          break;       
	case COOKIE_VAR: {
	  char* cookie = RETRIEVE_BUFFER_DATA(cookieTable[segTable[i].offset].name);
	  seg_table_shr_mem[i].seg_ptr.cookie_hash_code = cookie_hash(cookie, strlen(cookie));
	  break;
	}
	  /*	case DYN_VAR: {
	  char* dynvar = RETRIEVE_BUFFER_DATA(dynVarTable[segTable[i].offset].name);
	  seg_table_shr_mem[i].seg_ptr.dynvar_hash_code = dynvar_hash(dynvar, strlen(dynvar));
	  break;
	  }*/
	case SEARCH_VAR: {
	  char* searchvar = RETRIEVE_BUFFER_DATA(searchVarTable[segTable[i].offset].name);
	  int var_idx = gSessionTable[segTable[i].sess_idx].var_hash_func(searchvar, strlen(searchvar));
	  assert (var_idx != -1);
	  assert (gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].var_type == SEARCH_VAR);
	  seg_table_shr_mem[i].seg_ptr.var_idx = gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
    if(segTable[i].data != -1)
      seg_table_shr_mem[i].data = (void *)BIG_BUF_MEMORY_CONVERSION(segTable[i].data);
    else
      seg_table_shr_mem[i].data = NULL;
	  break;
	}
        
        //For JSON Var 
        case JSON_VAR: {
          char* jsonvar = RETRIEVE_BUFFER_DATA(jsonVarTable[segTable[i].offset].name);
          int var_idx = gSessionTable[segTable[i].sess_idx].var_hash_func(jsonvar, strlen(jsonvar));
          assert (var_idx != -1);
          assert (gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].var_type == JSON_VAR);
          seg_table_shr_mem[i].seg_ptr.var_idx = gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
    if(segTable[i].data != -1)
      seg_table_shr_mem[i].data = (void *)BIG_BUF_MEMORY_CONVERSION(segTable[i].data);
    else
      seg_table_shr_mem[i].data = NULL;
          break;
        }


	case TAG_VAR: {
	  char* tagvar = RETRIEVE_TEMP_BUFFER_DATA(tagTable[segTable[i].offset].name);
	  int var_idx = gSessionTable[segTable[i].sess_idx].var_hash_func(tagvar, strlen(tagvar));
	  assert (var_idx != -1);
	  assert (gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].var_type == TAG_VAR);
	  seg_table_shr_mem[i].seg_ptr.var_idx = gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
    if(segTable[i].data != -1)
      seg_table_shr_mem[i].data = (void *)BIG_BUF_MEMORY_CONVERSION(segTable[i].data);
    else
      seg_table_shr_mem[i].data = NULL;
	  break;
	}

	case NSL_VAR: {
	  char* nslvar = RETRIEVE_TEMP_BUFFER_DATA(nsVarTable[segTable[i].offset].name);
	  int var_idx = gSessionTable[segTable[i].sess_idx].var_hash_func(nslvar, strlen(nslvar));
	  assert (var_idx != -1);
	  assert (gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].var_type == NSL_VAR);
	  seg_table_shr_mem[i].seg_ptr.var_idx = gSessionTable[segTable[i].sess_idx].vars_trans_table_shr_mem[var_idx].user_var_table_idx;
    //set index if used.
    if(segTable[i].data != -1)
	    seg_table_shr_mem[i].data = (void *)BIG_BUF_MEMORY_CONVERSION(segTable[i].data);
    else 
      seg_table_shr_mem[i].data = NULL; 
	  break;
	}
#ifdef RMI_MODE
	case UTF_VAR:
 	  seg_table_shr_mem[i].seg_ptr.var_ptr = VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
 	  break;
	case BYTE_VAR: {
 	  char* bytevar = RETRIEVE_BUFFER_DATA(byteVarTable[segTable[i].offset].name);
 	  seg_table_shr_mem[i].seg_ptr.bytevar_hash_code = bytevar_hash(bytevar, strlen(bytevar));
 	  break;
 	}
 	case LONG_VAR:
 	  seg_table_shr_mem[i].seg_ptr.var_ptr = VAR_TABLE_MEMORY_CONVERSION(segTable[i].offset);
 	  break;
#endif
	case CLUST_VAR:
	case GROUP_VAR:
	case CLUST_NAME_VAR:
	case GROUP_NAME_VAR:
	case USERPROF_NAME_VAR:
  case SEGMENT:
	  seg_table_shr_mem[i].seg_ptr.var_idx = segTable[i].offset;
	  break;
        case HTTP_VERSION_VAR:
          seg_table_shr_mem[i].seg_ptr.var_idx = segTable[i].offset;
          break;

	default:
		NS_EXIT(1, "Undefined variable Type (%d)found", segTable[i].type);
	}
	seg_table_shr_mem[i].type = segTable[i].type;
        seg_table_shr_mem[i].pb_field_number = segTable[i].pb_field_number;
        seg_table_shr_mem[i].pb_field_type = segTable[i].pb_field_type;
      }
  }

  //Static var
  copy_staticvar_into_shared_mem();
  //copy_indexvar_into_shared_mem();

  /* Now the serverOrder and Post Table */
  if (total_serverorder_entries + total_post_entries) {
    serverorder_post_table_shr_mem = (void*) do_shmget(WORD_ALIGNED(sizeof(ServerOrderTableEntry_Shr) * total_serverorder_entries) + WORD_ALIGNED(sizeof(StrEnt_Shr) * total_post_entries), "ServerOrder Table");

      serverorder_table_shr_mem = (ServerOrderTableEntry_Shr*)serverorder_post_table_shr_mem;

      for (i = 0; i < total_serverorder_entries; i++) {
	serverorder_table_shr_mem[i].server_ptr = GSERVER_TABLE_MEMORY_CONVERSION(serverOrderTable[i].server_idx);
      }

      post_table_shr_mem = (StrEnt_Shr *) ((void*) serverorder_post_table_shr_mem + WORD_ALIGNED(sizeof(ServerOrderTableEntry_Shr) * total_serverorder_entries));

      for (i = 0; i < total_post_entries; i++) {

	post_table_shr_mem[i].num_entries = postTable[i].num_entries;

	if (postTable[i].seg_start == -1)
	  post_table_shr_mem[i].seg_start = NULL;
	else
	  post_table_shr_mem[i].seg_start = SEG_TABLE_MEMORY_CONVERSION(postTable[i].seg_start);
      }
  }

  /* next is the reqcook and dynvar table */
#ifdef RMI_MODE
  if (total_reqcook_entries + total_dynvar_entries + total_bytevar_entries) {
    reqcook_table_shr_mem = (ReqCookTableEntry_Shr *) do_shmget(WORD_ALIGNED(sizeof(ReqCookTableEntry_Shr) * total_reqcook_entries) + WORD_ALIGNED(sizeof(ReqDynVarTableEntry_Shr) * total_reqdynvar_entries) + WORD_ALIGNED(sizeof(ReqByteVarTableEntry_Shr) * total_reqbytevar_entries), "reqcook Table");
 }  
#else
  if (total_reqcook_entries + total_dynvar_entries) {
    reqcook_table_shr_mem = (ReqCookTableEntry_Shr *) do_shmget(WORD_ALIGNED(sizeof(ReqCookTableEntry_Shr) * total_reqcook_entries) + WORD_ALIGNED(sizeof(ReqDynVarTableEntry_Shr) * total_reqdynvar_entries), "reqcook Table");
#endif
      for (i = 0; i < total_reqcook_entries; i++) {
	reqcook_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(reqCookTable[i].name);
	reqcook_table_shr_mem[i].length = reqCookTable[i].length;
      }

      reqdynvar_table_shr_mem = (ReqDynVarTableEntry_Shr *) ((void*) reqcook_table_shr_mem + WORD_ALIGNED(sizeof(ReqCookTableEntry_Shr) * total_reqcook_entries));

      for (i = 0; i < total_reqdynvar_entries; i++) {
	reqdynvar_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(reqDynVarTable[i].name);
	reqdynvar_table_shr_mem[i].length = reqDynVarTable[i].length;
      }

#ifdef RMI_MODE
      reqbytevar_table_shr_mem = (ReqByteVarTableEntry_Shr *) ((void*) reqdynvar_table_shr_mem + WORD_ALIGNED(sizeof(ReqDynVarTableEntry_Shr) * total_reqdynvar_entries));

      for (i = 0; i < total_reqbytevar_entries; i++) {
 	reqbytevar_table_shr_mem[i].name = BIG_BUF_MEMORY_CONVERSION(reqByteVarTable[i].name);
 	reqbytevar_table_shr_mem[i].length = reqByteVarTable[i].length;
 	reqbytevar_table_shr_mem[i].offset = reqByteVarTable[i].offset;
 	reqbytevar_table_shr_mem[i].byte_length = reqByteVarTable[i].byte_length;
 	reqbytevar_table_shr_mem[i].type = reqByteVarTable[i].type;
 	reqbytevar_table_shr_mem[i].bytevar_hash_code = bytevar_hash(reqbytevar_table_shr_mem[i].name, strlen(reqbytevar_table_shr_mem[i].name));
      }
#endif
  }

#ifdef WS_MODE_OLD
  /* we do this here b/c the requests table has a web spec entry */
  insert_tag_shr_tables();
#endif

  /* click actions table */
  copy_click_actions_table_to_shr();

  /* Next, we do the requests(url) table */
  if (total_request_entries) {
    request_table_shr_mem = (action_request_Shr*) do_shmget(sizeof(action_request_Shr) * total_request_entries, "requests Table");
    if (sizeof(action_request) == sizeof(action_request_Shr)) {
      memcpy(request_table_shr_mem, requests, sizeof(action_request) * total_request_entries);
    } else {
      NS_EXIT(-1, CAV_ERR_1031038, "action_request", sizeof(action_request), "action_request_Shr", sizeof(action_request_Shr));
    }
    
    for (i = 0; i < total_request_entries; i++) {
      if(requests[i].request_type == HTTP_REQUEST ||
         requests[i].request_type == HTTPS_REQUEST)
      {
        copy_http_or_https_req_to_shr(i);
        if(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)
          copy_sockjs_close_table_to_shr();
      }
      else if (requests[i].request_type == SMTP_REQUEST || requests[i].request_type == SMTPS_REQUEST) 
        copy_smtp_req_to_shr(i);
      else if (requests[i].request_type == POP3_REQUEST || requests[i].request_type == SPOP3_REQUEST) 
        copy_pop3_req_to_shr(i);
      else if (requests[i].request_type == FTP_REQUEST) 
        copy_ftp_req_to_shr(i);
      else if (requests[i].request_type == JRMI_REQUEST)
        copy_jrmi_req_to_shr(i);
      else if (requests[i].request_type == DNS_REQUEST) 
        copy_dns_req_to_shr(i);
      else if (requests[i].request_type == LDAP_REQUEST || requests[i].request_type == LDAPS_REQUEST) 
        copy_ldap_req_to_shr(i);
      else if (requests[i].request_type == IMAP_REQUEST || requests[i].request_type == IMAPS_REQUEST) 
        copy_imap_req_to_shr(i);
      else if (requests[i].request_type == WS_REQUEST || requests[i].request_type == WSS_REQUEST)
        copy_ws_req_to_shr(i);
      else if (requests[i].request_type == XMPP_REQUEST)
        copy_xmpp_req_to_shr(i);
      else if (requests[i].request_type == FC2_REQUEST)
        copy_fc2_req_to_shr(i);
      else if (requests[i].request_type == SOCKET_REQUEST || requests[i].request_type == SSL_SOCKET_REQUEST)
        copy_socket_req_to_shr(i);
      else if (requests[i].request_type == RDP_REQUEST) /*bug 79149*/
        copy_rdp_req_to_shr(i);
    }
  }
  copy_http_method_shr();  

  /* In goes the Host Table */
  if (total_host_entries) {
    host_table_shr_mem = (HostTableEntry_Shr*) do_shmget(sizeof(HostTableEntry_Shr) * total_host_entries, "host Table");
      for (i = 0; i < total_host_entries; i++) {
	host_table_shr_mem[i].first_url = REQUEST_TABLE_MEMORY_CONVERSION(hostTable[i].first_url);
	host_table_shr_mem[i].svr_ptr = GSERVER_TABLE_MEMORY_CONVERSION(hostTable[i].svr_idx);
	host_table_shr_mem[i].num_url = hostTable[i].num_url;
	if (hostTable[i].next == -1)
	  host_table_shr_mem[i].next = NULL;
	else
	  host_table_shr_mem[i].next = HOST_TABLE_MEMORY_CONVERSION(hostTable[i].next);
      }
  }

  /* insert the think_timer table */
  copy_think_prof_to_shr();
  copy_inline_delay_to_shr();

   /*if (total_thinkprof_entries) {
    //thinkprof_table_shr_mem = (ThinkProfTableEntry_Shr*) do_shmget(sizeof(ThinkProfTableEntry_Shr) * total_thinkprof_entries, "think profile table");
    int actual_num_pages = 0;
    
    for (i = 0; i < total_runprof_entries; i++) {
      actual_num_pages += runProfTable[i].num_pages;
      NSDL4_MISC(NULL, NULL, "actual_num_pages = %d", actual_num_pages);
    }

    thinkprof_table_shr_mem = (ThinkProfTableEntry_Shr*) do_shmget(sizeof(ThinkProfTableEntry_Shr) * actual_num_pages, "think profile table");
    // Per session, per page assign think prof. 

    int j, k = 0;
    for (i = 0; i < total_runprof_entries; i++) {
      NSDL4_MISC(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
      for (j = 0; j < runProfTable[i].num_pages; j++) {
        int idx = runProfTable[i].page_think_table[j] == -1 ? 0 : runProfTable[i].page_think_table[j];
        NSDL4_MISC(NULL, NULL, "runProfTable[%d].page_think_table[%d] = %d, idx = %d, k = %d, avg_time = %d", 
                   i, j, runProfTable[i].page_think_table[j], idx, k, thinkProfTable[idx].avg_time);
        
	thinkprof_table_shr_mem[k].mode = thinkProfTable[idx].mode;
	thinkprof_table_shr_mem[k].avg_time = thinkProfTable[idx].avg_time;
	thinkprof_table_shr_mem[k].median_time = thinkProfTable[idx].median_time;
	thinkprof_table_shr_mem[k].var_time = thinkProfTable[idx].var_time;
	if (thinkProfTable[idx].mode == 1) {
	  if (ns_weibthink_calc(thinkProfTable[idx].avg_time, thinkProfTable[idx].median_time, thinkProfTable[idx].var_time, &thinkprof_table_shr_mem[k].a, &thinkprof_table_shr_mem[k].b) == -1) {
	    fprintf(stderr, "error in calculating a and b values for the weib ran num gen\n");
	    NS_EXIT(-1, "error in calculating a and b values for the weib ran num gen");
	  }
          NSDL3_TESTCASE(NULL, NULL, "The calulated a and b values for Page_Think_Time Mode = 1 are: a = %f, b = %f", thinkprof_table_shr_mem[k].a, thinkprof_table_shr_mem[k].b);
	}
	else if (thinkProfTable[idx].mode == 3) {
	  thinkprof_table_shr_mem[k].a = thinkProfTable[idx].avg_time;
	  thinkprof_table_shr_mem[k].b = thinkProfTable[idx].median_time;
	}

        k++;
      }
    }

//       for (i = 0; i < total_thinkprof_entries; i++) { */
/* 	thinkprof_table_shr_mem[i].mode = thinkProfTable[i].mode; */
/* 	thinkprof_table_shr_mem[i].avg_time = thinkProfTable[i].avg_time; */
/* 	thinkprof_table_shr_mem[i].median_time = thinkProfTable[i].median_time; */
/* 	thinkprof_table_shr_mem[i].var_time = thinkProfTable[i].var_time; */
/* 	if (thinkProfTable[i].mode == 1) { */
/* 	  if (ns_weibthink_calc(thinkProfTable[i].avg_time, thinkProfTable[i].median_time, thinkProfTable[i].var_time, &thinkprof_table_shr_mem[i].a, &thinkprof_table_shr_mem[i].b) == -1) { */
/* 	    fprintf(stderr, "error in calculating a and b values for the weib ran num gen\n"); */
/* 	    exit(-1); */
/* 	  } */
/*           NSDL3_TESTCASE(vptr, cptr, "The calulated a and b values for Page_Think_Time Mode = 1 are: a = %f, b = %f", thinkprof_table_shr_mem[i].a, thinkprof_table_shr_mem[i].b); */
/* 	} */
/* 	else if (thinkProfTable[i].mode == 3) { */
/* 	  thinkprof_table_shr_mem[i].a = thinkProfTable[i].avg_time; */
/* 	  thinkprof_table_shr_mem[i].b = thinkProfTable[i].median_time; */
/* 	} */
/*       } */
    //}*/

  /* auto fetch embedded table */
  copy_auto_fetch_to_shr();


 /* insert the pacing table */
  copy_session_pacing_to_shr();

 /*continue on page error table*/
  copy_continue_on_page_err_to_shr();


  if(total_pagereloadprof_entries > 1 || (total_pagereloadprof_entries == 1 && pageReloadProfTable[0].min_reloads >= 0 ))
    copy_pagereload_into_shared_mem();

  if(total_pageclickawayprof_entries > 1 || (total_pageclickawayprof_entries == 1 && pageClickAwayProfTable[0].clickaway_pct >= 0 ))
    copy_pageclickaway_into_shared_mem();

  perpageservar_table_shr_mem = copy_searchvar_into_shared_mem();  //nsl_search_var()
  
  perpagejsonvar_table_shr_mem = copy_jsonvar_into_shared_mem();   //nsl_json_var() 

  perpagechkpt_table_shr_mem = copy_checkpoint_into_shared_mem();  //nsl_web_find()

  perpagechk_replysize_table_shr_mem = copy_check_replysize_into_shared_mem();  //nsl_check_reply_size()
 

  /* Now, the Page Table */
  if (total_page_entries + total_tx_entries) {
    tx_page_table_shr_mem =  do_shmget(WORD_ALIGNED(sizeof(PageTableEntry_Shr) * total_page_entries) + sizeof(TxTableEntry_Shr) * total_tx_entries,
		"page Table");
    {
      FILE* page_name_fptr;

      sprintf (g_tmp_fname, "%s/%s/page_names.txt", g_ns_wdir, g_ns_tmpdir);
      page_name_fptr = fopen(g_tmp_fname, "w+");
      if (!page_name_fptr) {
	NS_EXIT(-1, CAV_ERR_1000006, g_tmp_fname, errno, nslb_strerror(errno));
      }

      page_table_shr_mem = (PageTableEntry_Shr *) tx_page_table_shr_mem;
      if (sizeof(PageTableEntry_Shr) == sizeof(PageTableEntry))
	memcpy(page_table_shr_mem, gPageTable, sizeof(PageTableEntry) * total_page_entries);
      else {
	NS_EXIT(-1, CAV_ERR_1031038, "PageTableEntry_Shr", sizeof(PageTableEntry_Shr), "PageTableEntry", sizeof(PageTableEntry));
      }
      
      for (i = 0; i < total_page_entries; i++) {
	page_table_shr_mem[i].page_name = BIG_BUF_MEMORY_CONVERSION(gPageTable[i].page_name);
	if (gPageTable[i].flow_name == -1)
          page_table_shr_mem[i].flow_name = NULL;
        else
	  page_table_shr_mem[i].flow_name = BIG_BUF_MEMORY_CONVERSION(gPageTable[i].flow_name);
	page_table_shr_mem[i].redirection_depth_bitmask = gPageTable[i].redirection_depth_bitmask;
	page_table_shr_mem[i].save_headers = gPageTable[i].save_headers;
	page_table_shr_mem[i].relative_page_idx = gPageTable[i].relative_page_idx;
        page_table_shr_mem[i].page_num_relative_to_flow = gPageTable[i].page_num_relative_to_flow; 
	page_table_shr_mem[i].first_eurl = REQUEST_TABLE_MEMORY_CONVERSION(gPageTable[i].first_eurl);
	if (gPageTable[i].head_hlist == -1)
	  page_table_shr_mem[i].head_hlist = NULL;
	else
	  page_table_shr_mem[i].head_hlist = HOST_TABLE_MEMORY_CONVERSION(gPageTable[i].head_hlist);
	if (gPageTable[i].tail_hlist == -1)
	  page_table_shr_mem[i].tail_hlist = NULL;
	else
	  page_table_shr_mem[i].tail_hlist = HOST_TABLE_MEMORY_CONVERSION(gPageTable[i].tail_hlist);

/* 	if (gPageTable[i].think_prof_idx == -1) { */
/* 	  page_table_shr_mem[i].think_prof_ptr = THINK_PROF_TABLE_MEMORY_CONVERSION(0); */
/* 	} else { */
/* 	  page_table_shr_mem[i].think_prof_ptr = THINK_PROF_TABLE_MEMORY_CONVERSION(gPageTable[i].think_prof_idx); */
/* 	} */

#ifndef RMI_MODE
	if (gPageTable[i].tag_root_idx == -1) {
	  page_table_shr_mem[i].thi_table_ptr = NULL;
	  //printf("Page %d is no-xml\n", i);
	} else {
	  page_table_shr_mem[i].thi_table_ptr = THI_TABLE_MEMORY_CONVERSION(gPageTable[i].tag_root_idx);
          page_table_shr_mem[i].thi_table_ptr->num_tag_entries = gPageTable[i].num_tag_entries;
	}
#endif

	if (gPageTable[i].first_searchvar_idx == -1)
	  page_table_shr_mem[i].first_searchvar_ptr = NULL;
	else
	  page_table_shr_mem[i].first_searchvar_ptr = perpageservar_table_shr_mem + gPageTable[i].first_searchvar_idx;

        NSDL2_SCHEDULE(NULL, NULL, "page_name = %s, page_table_shr_mem[%d].first_searchvar_ptr = %p, page_table_shr_mem[i].page_num_relative_to_flow = %d", page_table_shr_mem[i].page_name, i, page_table_shr_mem[i].first_searchvar_ptr, page_table_shr_mem[i].page_num_relative_to_flow);

        //For JSON Var
        if (gPageTable[i].first_jsonvar_idx == -1)
          page_table_shr_mem[i].first_jsonvar_ptr = NULL;
        else
          page_table_shr_mem[i].first_jsonvar_ptr = perpagejsonvar_table_shr_mem + gPageTable[i].first_jsonvar_idx;

        NSDL2_SCHEDULE(NULL, NULL, "page_name = %s, page_table_shr_mem[%d].first_jsonvar_ptr = %p", page_table_shr_mem[i].page_name, i, page_table_shr_mem[i].first_jsonvar_ptr);


	if (gPageTable[i].first_checkpoint_idx == -1)
	  page_table_shr_mem[i].first_checkpoint_ptr = NULL;
	else
	  page_table_shr_mem[i].first_checkpoint_ptr = perpagechkpt_table_shr_mem + gPageTable[i].first_checkpoint_idx;
       /* update the first_check_reply_size_ptr for the check reply size */
        if (gPageTable[i].first_check_replysize_idx== -1)
          page_table_shr_mem[i].first_check_reply_size_ptr= NULL;
        else
          page_table_shr_mem[i].first_check_reply_size_ptr = perpagechk_replysize_table_shr_mem + gPageTable[i].first_check_replysize_idx;

	fprintf(page_name_fptr, "%s\n", page_table_shr_mem[i].page_name);
      }

      fclose(page_name_fptr);

#if 0
      if (get_page_hash_fn() == -1)
	exit(-1);
#endif

      copy_tx_entry_to_shr(tx_page_table_shr_mem, total_page_entries);  // from ns_trans_parse.c :Anuj
    }
  }

  if(total_tx_entries){
    tx_hash_to_index_table_shr_mem = (int *) do_shmget(sizeof(int) * total_tx_entries, "tx_hash_to_index_table_shr_mem");
    for (i = 0; i < total_tx_entries; i++) {
      tx_hash_to_index_table_shr_mem[i] = tx_hash_to_index_table[i];  
    }
  }

  /* Up next is the session table */
  if (total_sess_entries) {
      session_table_shr_mem = (SessTableEntry_Shr*) do_shmget(sizeof(SessTableEntry) * total_sess_entries, "SessionTable");
      for (i = 0; i < total_sess_entries; i++) {
	session_table_shr_mem[i].sess_id = gSessionTable[i].sess_id;
	session_table_shr_mem[i].num_pages = gSessionTable[i].num_pages;
	session_table_shr_mem[i].completed = gSessionTable[i].completed;
	session_table_shr_mem[i].sess_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[i].sess_name);
	//session_table_shr_mem[i].jmeter_sess_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[i].jmeter_sess_name);
        if(gSessionTable[i].jmeter_sess_name != -1)
           session_table_shr_mem[i].jmeter_sess_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[i].jmeter_sess_name);
         else
           session_table_shr_mem[i].jmeter_sess_name = NULL;
        //Bug 70228 || If flow have no page then first_page should be NULL
        if(gSessionTable[i].first_page == -1)
          session_table_shr_mem[i].first_page = NULL;
        else
	  session_table_shr_mem[i].first_page = PAGE_TABLE_MEMORY_CONVERSION(gSessionTable[i].first_page);
	session_table_shr_mem[i].init_func_ptr = gSessionTable[i].init_func_ptr;
	session_table_shr_mem[i].exit_func_ptr = gSessionTable[i].exit_func_ptr;
        // Copying blindly as if if script mode is not 1 then null will be copied AN-CTX
	session_table_shr_mem[i].user_test_init = gSessionTable[i].user_test_init;
	session_table_shr_mem[i].user_test_exit = gSessionTable[i].user_test_exit;
        session_table_shr_mem[i].ctrlBlock = gSessionTable[i].ctrlBlock;
#if 0
	if (gSessionTable[i].pacing_idx == -1) {
	  session_table_shr_mem[i].pacing_ptr = PACING_TABLE_MEMORY_CONVERSION(0);
	} else {
	  session_table_shr_mem[i].pacing_ptr = PACING_TABLE_MEMORY_CONVERSION(gSessionTable[i].pacing_idx);
	}
#endif
	session_table_shr_mem[i].vars_trans_table_shr_mem = gSessionTable[i].vars_trans_table_shr_mem;
	session_table_shr_mem[i].var_type_table_shr_mem = gSessionTable[i].var_type_table_shr_mem;
	session_table_shr_mem[i].vars_rev_trans_table_shr_mem = gSessionTable[i].vars_rev_trans_table_shr_mem;
	session_table_shr_mem[i].var_hash_func = gSessionTable[i].var_hash_func;
	session_table_shr_mem[i].var_get_key = gSessionTable[i].var_get_key;
	session_table_shr_mem[i].numUniqVars = gSessionTable[i].numUniqVars;
	session_table_shr_mem[i].sp_grp_tbl_idx = gSessionTable[i].sp_grp_tbl_idx; //SYNC POINT
	session_table_shr_mem[i].sess_flag = gSessionTable[i].sess_flag; //To support multi - internet profile in same scenario but if groups use different sess
        session_table_shr_mem[i].script_type = gSessionTable[i].script_type; // added for java type script; 
        session_table_shr_mem[i].sess_norm_id = gSessionTable[i].sess_norm_id; 
        session_table_shr_mem[i].num_of_flow_path = gSessionTable[i].num_of_flow_path; 
        session_table_shr_mem[i].flow_path_start_idx = gSessionTable[i].flow_path_start_idx; 

        session_table_shr_mem[i].proj_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[i].proj_name);  // Project name
        session_table_shr_mem[i].sub_proj_name = BIG_BUF_MEMORY_CONVERSION(gSessionTable[i].sub_proj_name);  //Sub Project Name
        /* Saving url body and header response on session table */ 
        session_table_shr_mem[i].save_url_body_head_resp = gSessionTable[i].save_url_body_head_resp; 
	session_table_shr_mem[i].flags = gSessionTable[i].flags;
  NSDL2_SCHEDULE(NULL, NULL, "RBU ALERT, i = %d, ptr = %p", i, gSessionTable[i].rbu_alert_policy_ptr);
  if(gSessionTable[i].rbu_alert_policy_ptr == NULL)
    session_table_shr_mem[i].rbu_alert_policy = NULL; 
  else
    session_table_shr_mem[i].rbu_alert_policy = gSessionTable[i].rbu_alert_policy_ptr;
        PageTableEntry_Shr *tmp_page = session_table_shr_mem[i].first_page;
        int j;
        for (j = 0; j < session_table_shr_mem[i].num_pages; j++) {
          tmp_page[j].page_number = j;
        }
  // -- Add Achint- For global cookie - 10/04/2007
  if (gSessionTable[i].cookies.cookie_start == -1)
  {
    session_table_shr_mem[i].cookies.cookie_start = NULL;
    session_table_shr_mem[i].cookies.num_cookies = 0;
  }
  else
  {
    session_table_shr_mem[i].cookies.cookie_start = REQCOOK_TABLE_MEMORY_CONVERSION(gSessionTable[i].cookies.cookie_start);
    session_table_shr_mem[i].cookies.num_cookies = (gSessionTable[i].cookies.num_cookies);
    // copying the runlogic in to shr mem
  }
        //FREE rbu_tti_prof_tree 
        if(gSessionTable[i].rbu_tti_prof_tree){
          nslb_jsont_free(gSessionTable[i].rbu_tti_prof_tree);
          gSessionTable[i].rbu_tti_prof_tree = NULL;
        }
      }
  }

  copy_user_profile_attributes_into_shm();

  /* insert the profile tables into shared mem */
  if (total_sessprof_entries + total_userprof_entries + total_runprof_entries)
    if (insert_proftable_shr_mem() == -1)
      NS_EXIT(-1, "insert_proftable_shr_mem() failed");
  
  //nsl_var
  copy_nsl_var_into_shared_mem();

  insert_totstatic_host_shr_mem();
  /* insert the total(Actual) server table into shared mem */
  insert_totsvr_shr_mem();

  //static data
  create_static_file_table_shr_mem();

  /* insert the inuse_svr table into shared mem */
  if (total_inusesvr_entries) {
      inusesvr_table_shr_mem = (InuseSvrTableEntry_Shr*) do_shmget(sizeof(InuseSvrTableEntry_Shr) * total_inusesvr_entries, "inuse_svr Table");
      for (i = 0; i < total_inusesvr_entries; i++)
	inusesvr_table_shr_mem[i].location_idx = inuseSvrTable[i].location_idx;
  }


  /* insert the errorcode table into shared mem */
  copy_errorCodeTable_to_errorcode_table_shr_mem();

  if (create_clustvar_table() == -1)
    NS_EXIT(-1, "create_clustvar_table() failed");

  if (create_sgroupvar_table() == -1)
    NS_EXIT(-1, "create_sgroupvar_table() failed");

  //For SSL settings
  //if(create_ssl_cert_key_shr() == -1)
  //  NS_EXIT(-1, "create_ssl_cert_key_shr() failed");

  if(global_settings->sp_enable) {
    copy_sp_tbl_into_shr_memory();
    copy_sp_grp_tbl_into_shr_memory();
    copy_all_grp_tbl_into_shr_memory();
    copy_sp_api_tbl_into_shr_memory();
    init_sp_user_table ();
  }
  // copy query table into share memory
  if(global_settings->db_replay_mode == REPLAY_DB_QUERY_USING_FILE) {
    copy_query_table_into_shr_memory();
  }

  copy_alert_info_into_shr_mem();
}

#define MAX_CONF_LINE_LENGTH 16*1024
// Abhishek 9/10/2006- Function to get server IP addresses
// Start
char **server_stat_ip;
int no_of_host;



//Add - Achint 03/01/2007 - Perfmon

char temp_server_unix_rstat[(SERVER_NAME_SIZE + 1) * MAX_SERVER_STATS + 1];
char temp_server_win_perfmon[(SERVER_NAME_SIZE + 1)* MAX_SERVER_STATS + 1];

int num_server_unix_rstat = 0;
int num_server_win_perfmon = 0;

void add_to_server_list(char *type, char *list, char *server, int *count)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  if(strlen(server) > 64)
  {
	return;
  }
  if(*count > MAX_SERVER_STATS )  // Here we check if one type is not more than 10
  {
    NS_EXIT(1, "Number of servers %d is greater than  %d", *count, MAX_SERVER_STATS);
  }

  if (list[0] == '\0')
  {
	strcpy(list, server);
	strcat(list, " ");
  }
  else
  {
	strcat(list, server);
	strcat(list, " ");
  }
  (*count)++;
  printf("%s => %s  count = %d\n", type, list, *count);
}

void
get_server_perf_stats(char *buffer)
{
  char text[MAX_DATA_LINE_LENGTH];
  char text2[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char *ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  sscanf(buffer, "%s %s %s", keyword, text, text2);

  //printf("Function get_server_perf_stats is called\n");
  if(!strcmp(keyword, "SERVER_STATS")) // Old format of keyword. This is for unix_rstat
  {
	ptr = (char *)strtok(buffer, " ");
	ptr = (char *)strtok(NULL, " "); // To remove keyword
	while(ptr)
	{
	  add_to_server_list("unix_rstat", temp_server_unix_rstat, ptr, &num_server_unix_rstat);
	  ptr = (char *)strtok(NULL, " ");
	}
  }
}


//Added - 02/13/2007 Achint To convert the time into minutes.
float convert_to_per_minute (char *buf)
{
float time_min;
char buff[MAX_CONF_LINE_LENGTH + 1];
char keyword[MAX_DATA_LINE_LENGTH];
char option = 'M';
int num;

  NSDL2_MISC(NULL, NULL, "Method called, buf = %s", buf);
  strcpy(buff, buf);

  if ((num = sscanf(buff, "%s %f %c", keyword, &time_min, &option)) < 2)
  {
    NS_EXIT(-1, "Invalid  format of the keyword %s", keyword);
  }

  switch (option)
  {
    case 'S':
      time_min = time_min * 60.0;  //60 S = 60*60 M
      break;
    case 'H':
      time_min = time_min / 60.0;  //60 H = 60/60 M
      break;
    case 'M':
      break;
    default:
      NS_EXIT(-1, "Invalid  unit in the keyword %s", keyword);
      break;
  }
  return time_min;
}


#if 0
int
create_default_cluster(void) {
   int rnum;

   if (create_clust_table_entry(&rnum) == FAILURE)
     return -1;

   assert(rnum == DEFAULT_CLUST_IDX);

   clustTable[rnum].cluster_id = -1;
   return 0;
}
#endif


#if 0
int
resolve_scen_group_pacing(void)
{
  int i;
  int run_prof_idx;

  for (i = 1; i < total_pacing_entries; i++) {
    runprof_idx = find_sg_idx(RETRIEVE_TEMP_BUFFER_DATA(pacingTable[i].sg_name));
    if (runprof_idx == -1) {
      fprintf(stderr, "resolve_scen_group_pacing(): unknown scenario group %s used in a SG_PACING_PROFILE keyword\n", RETRIEVE_TEMP_BUFFER_DATA(pacingTable[i].sg_name));
      return -1;
    }
    runProfTable[runprof_idx].pacing_idx = i;
  }
  return 0;
}
#endif

//Added May/29/2007 - this function will create the file for Hits percentile reports
void
resp_create_hit_percentile_rpt()
{
  NSDL2_REPORTING(NULL, NULL, "Method called");
  if(resp_hits_fp == NULL)
  {
        char buf[600];
        sprintf(buf, "logs/TR%d/RespHitsPercentileRpt.dat", testidx);
        if ((resp_hits_fp = fopen(buf, "w")) == NULL)
        {
          NS_EXIT(1, "error in creating the Response hit percentile report file");
        }
        else
        {
          // This write header once when file is created first time
          fprintf(resp_hits_fp, "Object Mode|Response Time Window (RTW)|Number of Responses in  RTW|Pct of Responses in  RTW(%%)|Pct of Responses upto upper bound of RTW(%%)\n");
        }
  }
}


//#define TEST 1
#ifdef TEST
void dump_file() {
  int i;

  printf("#GLOBAL_SETTINGS\n");
  printf("KA_PCT %d\n", global_settings->ka_pct);
  printf("NUM_KA %d\n", global_settings->num_ka);
  printf("IP_FILE %s\n", g_ip_fname);
  /*  printf("#USERS\n");
  for (i = 0;i<total_user_entries;i++) {
    printf("USER %s %d %d %d %d %d %d %d\n", users[i].name,
		    users[i].fw_speed, users[i].rv_speed, users[i].fw_pct_loss, users[i].rv_pct_loss,
		    users[i].fw_delay, users[i].rv_delay, users[i].pct);
		    }*/
  printf("#URLS\n");
  printf("URL_FILE %s\n", g_url_file);
  printf("NUM_DIRS %d\n", global_settings->num_dirs);
  printf("\nContents of IP_FILE %s\n", g_ip_fname);
  for (i = 0;i<total_ip_entries;i++) {
    //printf("IP: %u PORT: %d\n", ips[i].ip_addr, ips[i].port);
    printf("IP: %s PORT: %d\n", nslb_sock_ntop((struct sockaddr *)&ips[i].ip_addr, ips[i].port));
  }
}
#endif /*test*/

void dump_urlvar_data () {
  int i;
  DBT key;
  DBT data;
  DBC* ht_cursor;

  NSDL2_MISC(NULL, NULL, "Method called");
  NSDL3_MISC(NULL, NULL, "Groups:\n");
  for (i = 0; i < total_group_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t type: %d\t sequence: %d\t weight index: %d\t num_values: %d\n", i, groupTable[i].type, groupTable[i].sequence, groupTable[i].weight_idx, groupTable[i].num_values);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Variables:\n");
  for (i = 0; i < total_var_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t group index: %d\t start_pointer: %d\t name: %s\t server base: %d\n", i, varTable[i].group_idx, varTable[i].value_start_ptr, RETRIEVE_BUFFER_DATA(varTable[i].name_pointer), varTable[i].server_base);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Pointers:\n");
  for (i = 0; i < total_pointer_entries; i++)
  {  
    if(pointerTable[i].size != -1)
      NSDL3_MISC(NULL, NULL, "index: %d\t value: %s\n", i, RETRIEVE_BUFFER_DATA(pointerTable[i].big_buf_pointer));
  }
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "FileParamValue:\n");
  for (i = 0; i < total_fparam_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t value: %s\n", i, 
           nslb_bigbuf_get_value(&file_param_value_big_buf, fparamValueTable[i].big_buf_pointer));
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Weights:\n");
  for (i = 0; i < total_weight_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t weight: %d\n", i, weightTable[i].value_weight);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Hash Table:\n");
  var_hash_table->cursor(var_hash_table, NULL, &ht_cursor, 0);
  memset (&key, 0, sizeof(DBT));
  memset (&data, 0, sizeof(DBT));
  if (ht_cursor->c_get(ht_cursor, &key, &data, DB_FIRST) == 0)
    NSDL3_MISC(NULL, NULL, "name: %s\t index: %s\n", (char*) key.data, (char *) data.data);
  memset (&key, 0, sizeof(DBT));
  memset (&data, 0, sizeof(DBT));
  while(ht_cursor->c_get(ht_cursor, &key, &data, DB_NEXT) == 0) {
    NSDL3_MISC(NULL, NULL, "name: %s\t index: %s\n", (char *) key.data, (char *) data.data);
    memset (&key, 0, sizeof(DBT));
    memset (&data, 0, sizeof(DBT));
  }
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Request Table:\n");
  for (i = 0; i < total_request_entries; i++) {
    NSDL3_MISC(NULL, NULL, "index: %d\t server base:%d\n", i, requests[i].server_base);
    NSDL3_MISC(NULL, NULL, "\turl_without_path segment start:%d\t url_without_path num_entries: %d\n",
	   requests[i].proto.http.url_without_path.seg_start, requests[i].proto.http.url_without_path.num_entries);
    NSDL3_MISC(NULL, NULL, "\turl segment start:%d\t url num_entries: %d\n",
	   requests[i].proto.http.url.seg_start, requests[i].proto.http.url.num_entries);
    NSDL3_MISC(NULL, NULL, "\thdr segment start:%d\t hdr num_entries: %d\n",
	   requests[i].proto.http.hdrs.seg_start, requests[i].proto.http.hdrs.num_entries);
    NSDL3_MISC(NULL, NULL, "\tauth_uname segment start:%d\t auth_uname num_entries: %d\n",
	   requests[i].proto.http.auth_uname.seg_start, requests[i].proto.http.auth_uname.num_entries);
    NSDL3_MISC(NULL, NULL, "\tauth_pwd segment start:%d\t auth_pwd num_entries: %d\n",
	   requests[i].proto.http.auth_pwd.seg_start, requests[i].proto.http.auth_pwd.num_entries);
  }
  NSDL3_MISC(NULL, NULL, "\n");

#if 0
  NSDL3_MISC(vptr, cptr, "Configuration File Server Table:\n");
  for (i = 0; i < total_server_entries; i++)
    NSDL3_MISC(vptr, cptr, "index: %d\t server name: %s\t server port: %d\n", i, servers[i].server_hostname, servers[i].server_port);
  NSDL3_MISC(vptr, cptr, "\n");
#endif

  NSDL3_MISC(NULL, NULL, "Connection Server Table:\n");
  for (i = 0; i < total_svr_entries; i++)
    NSDL3_MISC(NULL, NULL, "index:%d\t conn. server name: %s\t server port: %d\n", i, RETRIEVE_BUFFER_DATA(gServerTable[i].server_hostname), gServerTable[i].server_port);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Page Table\n");
  for (i=0; i<total_page_entries; i++){
    NSDL3_MISC(NULL, NULL, "index:%d\t Page Name:%s\t first eurl:%d\t num eurls%d num_check=%d\n", i, RETRIEVE_BUFFER_DATA(gPageTable[i].page_name), gPageTable[i].first_eurl, gPageTable[i].num_eurls, gPageTable[i].num_checkpoint);
#if 0
	for (j = 0; i < gPageTable[i].num_checkpoints; j++)
    	    NSDL3_MISC(vptr, cptr, "\tindex:%d\t Text:%s\t ID:%s\t fail%d report=%d\n", j, RETRIEVE_BUFFER_DATA(perPageChkPtTable[j + gPageTable[i].first_checkpoint_idx].text), gPageTable[i].first_eurl, gPageTable[i].num_eurls, gPageTable[i].num_checkpoints);
#endif
  }
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "Test Case\n");
  //  NSDL3_MISC(vptr, cptr, "Name: %s\t mode: %d\t runprof_idx: %s\t guess_type: %d\t guess_num: %d\t stab_num_success:%d\t stab_max_run: %d\t stab_run_time: %d\n", g_testrun, testCase.mode, testCase.run_name, testCase.guess_type, testCase.guess_num, testCase.stab_num_success, testCase.stab_max_run, testCase.stab_run_time);
  NSDL3_MISC(NULL, NULL, "\n");

  print_slaTable();

  NSDL3_MISC(NULL, NULL, "Metric Table\n");
  for (i = 0; i < total_metric_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t name: %d\t port: %d\t relation: %d\t time_ms: %d\t min_samples: %d\n", i, metricTable[i].name, metricTable[i].port, metricTable[i].relation, metricTable[i].target_value, metricTable[i].min_samples);
  NSDL3_MISC(NULL, NULL, "\n");
}

void dump_sharedmem(void) {
  int i;

  NSDL2_MISC(NULL, NULL, "Method called, All Shared Memory data dump");
  #if 0
  if ((u_ns_ptr_t) group_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Shared Groups:\n");
    for (i = 0; i < total_group_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t type: %d\t sequence: %d\t weight index(first weight): %d\t num_values: %d\n", i, group_table_shr_mem[i].type, group_table_shr_mem[i].sequence, group_table_shr_mem[i].group_wei_uni.weight_ptr?group_table_shr_mem[i].group_wei_uni.weight_ptr[0].value_weight:0, group_table_shr_mem[i].num_values);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t) variable_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Variables:\n");
    for (i = 0; i < total_var_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t group index(sequence): %d\t start_pointer(first value name): %s\t name: %s\n", i, variable_table_shr_mem[i].group_ptr[0].type, ((char* )((PointerTableEntry *)variable_table_shr_mem[i].value_start_ptr)->big_buf_pointer), variable_table_shr_mem[i].name_pointer);
    NSDL3_MISC(NULL, NULL, "\n");
  }
  #endif

  if ((u_ns_ptr_t) pointer_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Pointers:\n");
    for (i = 0; i < total_pointer_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t value: %s\n", i, pointer_table_shr_mem[i].big_buf_pointer);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  #if 0
  if ((u_ns_ptr_t) weight_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Weights:\n");
    for (i = 0; i < total_weight_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t weight: %d\n", i, weight_table_shr_mem[i].value_weight);
    NSDL3_MISC(NULL, NULL, "\n");
  }
  #endif

  if ((u_ns_ptr_t) seg_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Segment table:\n");
    for (i = 0; i < total_seg_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t type: %d\t segment: %s\n", i, seg_table_shr_mem[i].type, get_seg_name(&seg_table_shr_mem[i]));
  }

  if ((u_ns_ptr_t) post_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Post Table:\n");
    for (i = 0; i < total_post_entries; i++)
      NSDL3_MISC(NULL, NULL, "index: %d\t address: 0x%lx\t segment start:%s\t num_entries: %d\n", i,
	     (u_ns_ptr_t) &post_table_shr_mem[i],
	     post_table_shr_mem[i].seg_start?get_seg_name(post_table_shr_mem[i].seg_start):"NULL",
	     post_table_shr_mem[i].num_entries);
  }
  
  //Move to ns_cookie.c as dump_cookie_table() - Later
  if ((u_ns_ptr_t) reqcook_table_shr_mem != -1) {
    NSDL3_COOKIES(NULL, NULL, "Request Cookie Table:\n");
    for (i = 0; i < total_reqcook_entries; i++) {
      NSDL3_COOKIES(NULL, NULL, "index: %d\t cookie name: %s\n", i, reqcook_table_shr_mem[i].name);
    }
  }

  if ((u_ns_ptr_t) reqdynvar_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Request Dynamic Variable Table:\n");
    for (i = 0; i < total_reqdynvar_entries; i++) {
      NSDL3_MISC(NULL, NULL, "index: %d\t dynamic variable name: %s\n", i, reqdynvar_table_shr_mem[i].name);
    }
  }

#ifdef RMI_MODE
  if ((u_ns_ptr_t) reqbytevar_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Request Byte Variable Table:\n");
    for (i = 0; i < total_reqbytevar_entries; i++) {
      NSDL3_MISC(NULL, NULL, "index: %d\t byte variable name: %s\n", i, reqbytevar_table_shr_mem[i].name);
    }
  }
#endif

  if ((u_ns_ptr_t) request_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Request Table:\n");
    for (i = 0; i < total_request_entries; i++) {
      NSDL3_MISC(NULL, NULL, "index: %d\t server base:%s\n", i, request_table_shr_mem[i].server_base?request_table_shr_mem[i].server_base->server_ptr->server_hostname:"NULL");
      NSDL3_MISC(NULL, NULL, "\turl segment start:%s\t url_without_path num_entries: %d\n",
                 request_table_shr_mem[i].proto.http.url_without_path.seg_start?
                 get_seg_name(request_table_shr_mem[i].proto.http.url_without_path.seg_start):"NULL",
                 request_table_shr_mem[i].proto.http.url_without_path.num_entries);
      NSDL3_MISC(NULL, NULL, "\turl segment start:%s\t url num_entries: %d\n",
                 request_table_shr_mem[i].proto.http.url.seg_start?
                 get_seg_name(request_table_shr_mem[i].proto.http.url.seg_start):"NULL",
                 request_table_shr_mem[i].proto.http.url.num_entries);
      NSDL3_MISC(NULL, NULL, "\thdr segment start:%s\t hdr num_entries: %d\n",
                 request_table_shr_mem[i].proto.http.hdrs.seg_start?
                 get_seg_name(request_table_shr_mem[i].proto.http.hdrs.seg_start):"NULL",
                 request_table_shr_mem[i].proto.http.hdrs.num_entries);
      NSDL3_MISC(NULL, NULL, "\tauth_uname segment start:%s\t auth_uname num_entries: %d\n",
                 request_table_shr_mem[i].proto.http.auth_uname.seg_start?
                 get_seg_name(request_table_shr_mem[i].proto.http.auth_uname.seg_start):"NULL",
                 request_table_shr_mem[i].proto.http.auth_uname.num_entries);
      NSDL3_MISC(NULL, NULL, "\tauth_pwd segment start:%s\t auth_pwd num_entries: %d\n",
                 request_table_shr_mem[i].proto.http.auth_pwd.seg_start?
                 get_seg_name(request_table_shr_mem[i].proto.http.auth_pwd.seg_start):"NULL",
                 request_table_shr_mem[i].proto.http.auth_pwd.num_entries);
      NSDL3_MISC(NULL, NULL, "\tpost segment: 0x%lx\n", (u_ns_ptr_t) request_table_shr_mem[i].proto.http.post_ptr);
      NSDL3_COOKIES(NULL, NULL, "\tfirst cookies: %s\t cookie length: %d\n", request_table_shr_mem[i].proto.http.cookies.cookie_start?request_table_shr_mem[i].proto.http.cookies.cookie_start->name:"NULL", request_table_shr_mem[i].proto.http.cookies.num_cookies);
    }
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t) gserver_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Connection Server Table:\n");
    for (i = 0; i < total_svr_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t conn. server name: %s\n", i, gserver_table_shr_mem[i].server_hostname);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t) page_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Page Table\n");
    for (i=0; i<total_page_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t Page Name:%s\t first eurl:%s\t num eurls%d\n", i, page_table_shr_mem[i].page_name, get_seg_name((((page_table_shr_mem[i]).first_eurl)->proto.http.url).seg_start), page_table_shr_mem[i].num_eurls);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t) userprof_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "User Prof Table\n");
    for (i = 0; i<total_userprofshr_entries; i++)
      //NSDL3_MISC(NULL, NULL, "index:%d\t userprof_idx: %d\t Location:%s\t Access:%s\t Browser:%s\t Freq:%s\t Machine:%s Pct:%d\n", i, userprof_table_shr_mem[i].uprofindex_idx, userprof_table_shr_mem[i].location->name,
      NSDL3_MISC(NULL, NULL, "index:%d\t userprof_idx: %d\t Location:%s\t Access:%s\t Browser:%s\t Pct:%d\n", i, userprof_table_shr_mem[i].uprofindex_idx, userprof_table_shr_mem[i].location->name,
	     userprof_table_shr_mem[i].access->name, userprof_table_shr_mem[i].browser->name,
	     /*userprof_table_shr_mem[i].frequency->name, userprof_table_shr_mem[i].machine->name,*/
	     userprof_table_shr_mem[i].pct);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t)userprofindex_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "User Prof Index Table\n");
    for (i = 0; i<total_userindex_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t name: %s\t start pct:%d\t legnth:%d\n", i, userprofindex_table_shr_mem[i].name, userprofindex_table_shr_mem[i].userprof_start->pct,  userprofindex_table_shr_mem[i].length);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t)sessprof_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Sess Prof Table\n");
    for (i = 0; i < total_sessprof_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t session name: %s\t pct: %d\n", i, (sessprof_table_shr_mem[i].session_ptr)->sess_name, sessprof_table_shr_mem[i].pct);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t)sessprofindex_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Sess Prof Index Table\n");
    for (i = 0; i < total_sessprofindex_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t name: %s\t start_pct:%d\t length:%d\n", i, sessprofindex_table_shr_mem[i].name, (sessprofindex_table_shr_mem[i].sessprof_start)->pct, sessprofindex_table_shr_mem[i].length);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t)runprof_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Run Prof Table\n");
    for (i = 0; i < total_runprof_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t session profile name: %s\t pct: %d\n", i, (runprof_table_shr_mem[i].userindexprof_ptr)->name, runprof_table_shr_mem[i].quantity);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  if ((u_ns_ptr_t)runprofindex_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Run Prof Index Table\n");
    for (i = 0; i < total_runindex_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t name: %s\t start_pct:%d\t length:%d\n", i, runprofindex_table_shr_mem[i].name, (runprofindex_table_shr_mem[i].runprof_start)->quantity, runprofindex_table_shr_mem[i].length);
    NSDL3_MISC(NULL, NULL, "\n");
  }

  int g, h, s, total_rec_host_entries, total_act_svr_entries;
  NSDL3_MISC(NULL, NULL, "inusesvr_table_shr_mem Table, total_runprof_entries = %d\n", total_runprof_entries);
  for(g=0; g < total_runprof_entries; g++)
  {
    GrpSvrHostSettings *svr_host_settings = &runprof_table_shr_mem[g].gset.svr_host_settings;

    total_rec_host_entries = svr_host_settings->total_rec_host_entries;

    for(h=0; h < total_rec_host_entries; h++)
    {
      total_act_svr_entries = svr_host_settings->host_table[h].total_act_svr_entries;

      for(s=0; s < total_act_svr_entries; s++)
      {
        int user_idx = find_inusesvr_idx(s);
        if (user_idx == -1)
          user_idx = s;
        else
          user_idx = inusesvr_table_shr_mem[user_idx].location_idx;
        NSDL3_MISC(NULL, NULL, "index:%d\t name:%s\t location:%d\n", s, 
                     svr_host_settings->host_table[h].server_table[user_idx].server_name, 
                     svr_host_settings->host_table[h].server_table[user_idx].loc_idx);
        NSDL3_MISC(NULL, NULL, "\n");
      }
    }
  }

  if ((u_ns_ptr_t)inusesvr_table_shr_mem != -1) {
    NSDL3_MISC(NULL, NULL, "Inuse Svr Table\n");
    for (i = 0; i < total_inusesvr_entries; i++)
      NSDL3_MISC(NULL, NULL, "index:%d\t Location:%s\n", i, locattr_table_shr_mem[inusesvr_table_shr_mem[i].location_idx].name);
    NSDL3_MISC(NULL, NULL, "\n");
  }
  
  //ns_proxy_shr_data_dump();
}


void dump_profile_data(void) {
  int i;


  NSDL3_MISC(NULL, NULL, "SessProf Table\n");
  for (i = 0; i < total_sessprof_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t sessprofindx_idx: %d\t sessindex: %d\t pct: %d\n", i, sessProfTable[i].sessprofindex_idx, sessProfTable[i].session_idx, sessProfTable[i].pct);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "SessProfIndex Table\n");
  for (i = 0; i < total_sessprofindex_entries; i ++)
    NSDL3_MISC(NULL, NULL, "index: %d\t name: %s\t sessprof_start: %d\t sessprof_length: %d\n", i, RETRIEVE_BUFFER_DATA(sessProfIndexTable[i].name), sessProfIndexTable[i].sessprof_start, sessProfIndexTable[i].sessprof_length);
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "UserProf Table\n");
  for (i = 0; i < total_userprof_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t userindex_idx: %d\t type: %d\t attribute_idx: %d\t pct: %d\n", i, userProfTable[i].userindex_idx, userProfTable[i].type, userProfTable[i].attribute_idx, userProfTable[i].pct);
  NSDL3_MISC(NULL, NULL, "\n");


  NSDL3_MISC(NULL, NULL, "RunProf Table\n");
#if 0
  for (i = 0; i < total_runprof_entries; i++)
    NSDL3_MISC(vptr, cptr, "index: %d\t runindex_idx: %d\t userprof_idx: %d\t sessprof_idx: %d\t pct: %d\n", i, runProfTable[i].runindex_idx, runProfTable[i].userprof_idx, runProfTable[i].sessprof_idx, runProfTable[i].quantity);
#endif
  NSDL3_MISC(NULL, NULL, "\n");

  NSDL3_MISC(NULL, NULL, "RunProfileIndex Table\n");
  for (i = 0; i < total_runindex_entries; i++)
    NSDL3_MISC(NULL, NULL, "index: %d\t name: %s\t runprof_start: %d\t runprof_length: %d\n", i, RETRIEVE_BUFFER_DATA(runIndexTable[i].name), runIndexTable[i].runprof_start, runIndexTable[i].runprof_length);
  NSDL3_MISC(NULL, NULL, "\n");
}
 
#ifdef TEST
/* main */
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: conf_file <filename>\n");
    exit(1);
  }
  fprintf(stdout, "Processing input file: \"%s\".\n", argv[1]);
  fflush(stdout);

  parse_url_file(argv[1]);


  printf("Dumping data to stdout\n\n");
  dump_file();

  printf("\n\n");

  dump_urlvar_data();
  dump_profile_data();
  dump_user_profile_data();

  copy_structs_into_shared_mem();
  //check_and_set_flag_for_ka_timer();

  printf("SHARED MEMORY DUMP\n");
  dump_sharedmem();

  return 0;
}

#endif /* TEST */
#define MAX_SESSIONS_TO_STABALIZE 15


#ifdef RMI_MODE
#define MAX_RMI_RESPONSE 128*1024

static unsigned char rmi_buf[MAX_RMI_RESPONSE];

extern struct logging_table_entry* logging_table;   /* size of this table defined in global_settings->resp_logging_size */
extern int logging_table_idx;

int
gen_ascii_report(int testidx, int child_id)
{
  char bfile_name[32];
  char afile_name[32];
  int bfd;
  FILE *afp;
  int i, j, k, l, len, rbytes_left, bytes_read;
  int type; //0: Request & 1 = Response
  int num_blanks = 0;

  NSDL2_REPORTING(NULL, NULL, "Method called, testidx = %d, child_id = %d", testidx, child_id);
  sprintf(bfile_name, "logs/TR%d/bfile.%d", testidx, child_id);
  sprintf(afile_name, "logs/TR%d/afile.%d", testidx, child_id);

  bfd = open(bfile_name, O_RDONLY|O_CLOEXEC);
  if (bfd == -1) {
    fprintf(stderr, "gen_ascii_report: Error in opening log file %s\n", bfile_name);
    return -1;
  }

  if ((afp = fopen(afile_name, "w" )) == NULL) {
    fprintf(stderr, "gen_ascii_report: Error in opening ascii log file %s\n", afile_name);
    return -1;
  }

  for (i = 0; i <= logging_table_idx; i++) {
    fprintf(afp, "\nSession Instance = %d Page Id = %d Url ID = %d status = %d\n",
	    logging_table[i].sess_inst_id,
	    logging_table[i].pg_id,
	    logging_table[i].url_id,
	    (int)logging_table[i].status);

    type = 0;
    while (1) {
      if ( type == 0 ) {
	fprintf(afp, "Request follows, length: %d\n", logging_table[i].req_len);
	rbytes_left = logging_table[i].req_len;
      } else  {
	fprintf(afp, "Response follows, length: %d\n", logging_table[i].resp_len);
	rbytes_left = logging_table[i].resp_len;
      }

      len = 0;
      bytes_read = 0;
      if (rbytes_left > MAX_RMI_RESPONSE ) {
	fprintf(stderr, "gen_ascii_report: RMI message length is %d while Max RMI Buf set is %d\n", rbytes_left, MAX_RMI_RESPONSE);
	return -1;
      }
      while (rbytes_left) {
	len = read (bfd, rmi_buf+bytes_read, rbytes_left);
	if (len <= 0) {
	  fprintf(stderr, "gen_ascii_report: Unexpected end of file %s. expecting to read %d bytes\n", afile_name, rbytes_left);
	  return -1;
	}
	rbytes_left -= len;
	bytes_read += len;
      }

      if ( type == 0 ) {
	len = logging_table[i].req_len;
      } else  {
	len = logging_table[i].resp_len;
      }
      for (l = 0; l < len; l++) {
	fprintf(afp,"%02X ", rmi_buf[l]);
	if (!((l+1) % 16)) {
	  fprintf (afp, "    ");
	  for(j = 15; j >= 0; j--) {
	    fprintf(afp, "%c", (rmi_buf[l-j] >= 0x20 && rmi_buf[l-j] <= 0X7E)?rmi_buf[l-j]:'.');
	    if (j == 8)
	      fprintf(afp, " ");
	  }
	  fprintf(afp, "\n");
	} else if (!((l+1) % 8)) {
	  fprintf(afp, " ");
	}
      }
      /* print last line of chars */
      j = len % 16;
      if (j) {
	// blank pad
	num_blanks = 3 * (16 - j) +  4; // ecah char is 2 hex byte + 1 blank, 4 blanks after hex bytes
	if (j < 8 ) num_blanks++; // extra blank after 8th byte

	for (k = 0; k < num_blanks; k++)
	  fprintf( afp, " ");

	j = j -1;
	for(;j >= 0; j--) {
	  //fprintf(afp, "%c", rmi_buf[len -1 -j]);
	  fprintf(afp, "%c", (rmi_buf[len-1-j] >= 0x20 && rmi_buf[len-1-j] <= 0X7E)?rmi_buf[len-1-j]:'.');
	  if (j == 8)
	    fprintf(afp, " ");
	}
	fprintf(afp, "\n");
      }
      fprintf(afp, "\n");
      if (type == 0) type = 1;
      else break;
    }
  }
  fclose(afp);
  return 0;
}

#endif

#define get_sess_name_with_proj_subproj(x) get_sess_name_with_proj_subproj_int(x, NULL_SESS_ID, '/')

// Extension function of get_sess_name_with_proj_subproj, for passing project subproject along with script name
char *get_sess_name_with_proj_subproj_int(char *sess_name, int sess_id, char* delim)
{
  static __thread char proj_subproj_sess_name[256];
  /*bug id: 101320: add scripts dir before script_dir name, to support Multiple Workspace/WorkProfile */
  /* Support old function get_sess_name_with_proj_subproj */
  if(NULL_SESS_ID == sess_id)
  {
    sprintf(proj_subproj_sess_name, "%s%s%s%s%s%s%s", g_project_name, delim, g_subproject_name, delim, "scripts", delim, sess_name);
    NSDL2_USER_TRACE(NULL, NULL, "returning = %s", proj_subproj_sess_name);
    return proj_subproj_sess_name;
  }
  else {
    //Bug 80493: When session_table_shr_mem is available, fetch proj/sub-pproj from session_table_shr_mem (Shared Memory).
    if(session_table_shr_mem)
    { 
      sprintf(proj_subproj_sess_name, "%s%s%s%s%s%s%s", session_table_shr_mem[sess_id].proj_name, delim, 
                                       session_table_shr_mem[sess_id].sub_proj_name, delim, "scripts", delim, sess_name);
      NSDL2_USER_TRACE(NULL, NULL, "returning = (%s)", proj_subproj_sess_name);
      return proj_subproj_sess_name;
    }
    else {
      sprintf(proj_subproj_sess_name, "%s%s%s%s%s%s%s", RETRIEVE_BUFFER_DATA(gSessionTable[sess_id].proj_name), delim, 
                                       RETRIEVE_BUFFER_DATA(gSessionTable[sess_id].sub_proj_name), delim, "scripts", delim, sess_name);
      NSDL2_USER_TRACE(NULL, NULL, "returning = [%s]", proj_subproj_sess_name);
      return proj_subproj_sess_name;
    }
  }
} 

char *get_sess_name(char *sess_name) 
{
  return sess_name;
}

void get_test_completion_time_detail(char *buf)
{
  if(global_settings->num_fetches)
    sprintf(buf, "%d Sessions Completion",global_settings->num_fetches);
  else if (global_settings->test_stab_time)
    sprintf(buf, "%d Seconds (%s HH:MM:SS)", global_settings->test_stab_time, (char *)get_time_in_hhmmss(global_settings->test_stab_time));
  else
    sprintf(buf, "Till stopped by user");
}

/* Remove because get_sess_id_by_name function called from ns_trace_log.c in function get_parameters(), 
   There is no check.*/
//#ifdef NS_DEBUG_ON
/*unsigned int get_sess_id_by_name(char *sess_name)
{
  int sess_idx = 0;
  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) {
    if (strcmp(session_table_shr_mem[sess_idx].sess_name, sess_name) == 0)
      return sess_idx;
  }
  
  return -1;
}

unsigned int get_page_id_by_name(SessTableEntry_Shr *sess_ptr, char *page_name)
{
  int i;
  for (i = 0; i < sess_ptr->num_pages; i++) {
    if (strcmp(page_name, sess_ptr->first_page[i].page_name) == 0) 
      return i;
  }
  return -1;
}*/

//#endif /*  NS_DEBUG_ON */


//READER_RUN_MODE <mode>
int kw_set_reader_run_mode (char *buf, int runtime_flag, char *err_msg) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int time;
  char cmode[MAX_DATA_LINE_LENGTH];
  char ctime[MAX_DATA_LINE_LENGTH];
  cmode[0] = 0;
  ctime[0] = 0;
  int mode = 0;

  NSDL2_USER_TRACE(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, cmode, ctime, tmp);
  NSDL2_USER_TRACE(NULL, NULL, "Method called, num = %d", num);

  if(num > 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, READER_RUN_MODE_USAGE, CAV_ERR_1011111, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(cmode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, READER_RUN_MODE_USAGE, CAV_ERR_1011111, CAV_ERR_MSG_2);
  }
  mode = atoi(cmode);

  if(global_settings->continuous_monitoring_mode == 1 && strcmp(g_cavinfo.config, "NV"))
  {
    if((mode != 1) && (mode != 3))
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, READER_RUN_MODE_USAGE, CAV_ERR_1011113, "");
  }

  if(mode < 0 || mode > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, READER_RUN_MODE_USAGE, CAV_ERR_1011111, CAV_ERR_MSG_3);
  }
 
  time = 10;
  if(num == 3){
    time = atoi(ctime);
    if(time < 10)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, READER_RUN_MODE_USAGE, CAV_ERR_1011112, "");
    }
  }

  global_settings->reader_run_mode = mode;
  global_settings->reader_csv_write_time = time;

  NSDL2_USER_TRACE(NULL, NULL, "global_settings->reader_run_mode = %d, reader writing time = %d", global_settings->reader_run_mode, global_settings->reader_csv_write_time);
  return 0;
}

inline void init_ns_nvm_scratch_buf()
{
  // Calculate length of scratch buf by finding maximum of all gropus
  int i;
  int scratch_buf_size = 0;
  for(i = 0; i < total_runprof_entries; i++)
  {
    if(runprof_table_shr_mem[i].gset.max_trace_param_value_size > scratch_buf_size)
      scratch_buf_size = runprof_table_shr_mem[i].gset.max_trace_param_value_size; 
  }
  
  //malloc nvm scratch buf
  if(scratch_buf_size > global_settings->nvm_scratch_buf_size)
  {
    fprintf(stdout, "Maximum tracing buffer size (%d), is greater than scratch buffer size(%d).Making scratch buffer size to (%d).\n", scratch_buf_size, global_settings->nvm_scratch_buf_size, scratch_buf_size);
    global_settings->nvm_scratch_buf_size = scratch_buf_size;
  }

  MY_MALLOC(ns_nvm_scratch_buf, scratch_buf_size + 1, "ns_nvm_scratch_buf", -1);
  ns_nvm_scratch_buf_size = scratch_buf_size; 

  MY_MALLOC(ns_nvm_scratch_buf_trace, scratch_buf_size + 1, "ns_nvm_scratch_buf_trace", -1);
}

void ignore_hash_usages(char *err_msg){

  NSTL1_OUT(NULL, NULL, "Invalid value of G_IGNORE_HASH_IN_URL %s", err_msg);
  NSTL1_OUT(NULL, NULL, "Usages: G_IGNORE_HASH_IN_URL <grp_name> <mode>");
  NSTL1_OUT(NULL, NULL, "  This keyword is used to ignore hash in url.\n");
  NSTL1_OUT(NULL, NULL, "    in grp_name give any group name or ALL .\n");
  NSTL1_OUT(NULL, NULL, "    Mode: Mode for G_IGNORE_HASH_IN_URL 0 or 1\n");
  NSTL1_OUT(NULL, NULL, "      0 - dont igonre hash in url \n");
  NSTL1_OUT(NULL, NULL, "      1 - ignore hash in url\n");
  NS_EXIT(-1, "Invalid value of G_IGNORE_HASH_IN_URL %s\nUsages: G_IGNORE_HASH_IN_URL <grp_name> <mode>", err_msg);
}

void kw_set_g_ignore_hash(char *buf, GroupSettings *gset, char *msg){
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  char keyword[MAX_LINE_LENGTH];
  int mode;
  char sg_name[MAX_LINE_LENGTH];
  int num;

  num = sscanf(buf, "%s %s %d", keyword, sg_name, &mode);

  if(num != 3){
    ignore_hash_usages("No of arugument should be TWO"); 
  }

  val_sgrp_name(buf, sg_name, 0); 
  
  if(mode <0 || mode >1){
    ignore_hash_usages("mode can not ne other than 0 or 1"); 
  }

  gset->ignore_hash = mode; 
  NSDL2_PARSING(NULL, NULL, "gset->ignore_hash = %d", gset->ignore_hash);

}

// set_runproftable_start_page_idx(): Keep tracks on page index of group
inline void set_runproftable_start_page_idx()
{
  int i=0;
  int num_pages = 0;
  NSDL2_PARSING(NULL, NULL, "Method Called");

  //Set page_start in RunProfTable
  for(i = 0; i < total_runprof_entries; i++)
  {
    runProfTable[i].start_page_idx = num_pages;
    num_pages += runProfTable[i].num_pages;
  }
} 

//set_gpagetable_relative_page_idx(): Keep track on relative page index in Page table
inline void set_gpagetable_relative_page_idx()
{
  int i = 0, j = 0;
  for(i = 0; i < total_sess_entries; i++)
  {
    for( j = 0; j < gSessionTable[i].num_pages; j++)
    {
      NSDL2_PARSING(NULL, NULL, "relative_page_idx: total_sess_entries = %d, num_pages = %d", i, j);
      gPageTable[gSessionTable[i].first_page + j].relative_page_idx = j;
    }
  }
}

inline void init_status_code_table()
{
  memset (status_code, 0, STATUS_CODE_ARRAY_SIZE * sizeof(status_codes));
  status_code[100].status_settings |= STATUS_CODE_DONT_CHANGE_REFERER;
  status_code[101].status_settings |= STATUS_CODE_DONT_CHANGE_REFERER;
  status_code[301].status_settings |= STATUS_CODE_REDIRECT;
  status_code[302].status_settings |= STATUS_CODE_REDIRECT;
  status_code[307].status_settings |= STATUS_CODE_REDIRECT;
}

void kw_set_enable_memory_map(char *buf)
{
  char keyword[RBU_MAX_KEYWORD_LENGTH];
  char mode_str[RBU_MAX_USAGE_LENGTH];
  char tmp_str[RBU_MAX_USAGE_LENGTH]; 
  char usages[RBU_MAX_USAGE_LENGTH];
  int mode, ret; 
 
  NSDL2_MEMORY(NULL, NULL, "Method called. buf = [%s]", buf);

  sprintf(usages, "Usages:\n"
                  "ENABLE_MEMORY_MAP <mode>\n"
                  "Where:\n"
                  "mode         - Enable/Disable the feature\n"
                  "             0 --> Disable (Deafult)\n"
                  "             1 --> Enable.\n");


  int num_args = sscanf(buf, "%s %s %s", keyword, mode_str, tmp_str);

  NSDL2_MEMORY(NULL, NULL, "num_args = [%d]", num_args);
  if(num_args != 2) 
  {
    NS_EXIT(-1, "Error: provided number of argument (%d) is wrong.\n%s", num_args, usages); 
  }

  //setting mode
  ret = nslb_atoi(mode_str, &mode);

  if(!ret && (mode == 0 || mode == 1))
  {
    global_settings->enable_memory_map = mode;
    NSDL2_MEMORY(NULL, NULL, "enable_memory_map = %d", global_settings->enable_memory_map);

    if(global_settings->enable_memory_map)
    {
      //Initializes the memory map.
      nslb_mem_map_init();
    }
  }
  else 
  {
    NS_EXIT(-1, "Error: mode should be 0 or 1 only.\n%s", usages);
  }
 
  NSDL2_MEMORY(NULL, NULL, "Method end. global_settings->enable_memory_map = [%d]", global_settings->enable_memory_map);
  return;
}

/*--------------------------------------------------------------------------------------------------------------- 
 * Purpose   : 
 *    
 * Input     : buf          : G_RBU_HAR_TIMEOUT <group_name> <time (Sec)>
 *             err_msg      : buffer to fill error message
 *             runtime_changes : flag to runtime changes  
 *    
 * Output    : On error    -1
 *             On success   0
 *     
 * Build_ver : 4.1.8 B#5
-----------------------------------------------------------------------------------------------------------------*/
void process_cav_memory_map(Msg_com_con *mccptr)
{
  NSDL2_MEMORY(NULL, NULL, "Method called.");

  if(!global_settings->enable_memory_map)
  {
    NSTL1(NULL, NULL, "Keyword ENABLE_MEMORY_MAP is disabled");
    write_msg(mccptr, CAV_MEM_MAP_DONE_ERR_MSG, sizeof(CAV_MEM_MAP_DONE_ERR_MSG), 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE);
    return; 
  }
  //Send signal to all child/genrator
  send_rtc_msg_to_all_clients(CAV_MEMORY_MAP, NULL, 0);

  //Dump memory for parent
  ns_process_cav_memory_map();

  //Reply back to invoker if running as parent or controller.
  if(loader_opcode != CLIENT_LOADER)
  {
    //TODO DEBUG 
    send_rtc_msg_to_invoker(mccptr, CAV_MEMORY_MAP, NULL, 0);
  }
  NSDL2_MEMORY(NULL, NULL, "Method end.");
}

void ns_process_cav_memory_map()
{
  char fname[MAX_FILE_NAME + 1];

  NSDL2_MEMORY(NULL, NULL, "Method called.");
  
  if(!global_settings->enable_memory_map)
    return;
  
  //Create filename
  sprintf(fname, "%s/logs/TR%d/ns_logs/proc_mem_dump_%d.%lu", g_ns_wdir, testidx, my_child_index, pthread_self());

  NSDL2_MEMORY(NULL, NULL, "fname = %s", fname);

  nslb_mem_map_dump(fname);
}

/* This function gives the diff between time passed as an input
argument and current time. Returns difference in seconds */
long ns_get_delay_in_secs(char *date_str)
{
    char buf[256];
    long ret;
    struct tm mtm;
    time_t t;
    long ctime, utime;
    NSDL4_MEMORY(NULL, NULL, "Input Time %s", date_str);
    // Converting provided time 
    strptime(date_str, "%D-%T", &mtm);
    mtm.tm_isdst = 0;//Making daylight saving off explicitly
    strftime(buf, sizeof(buf), "%s", &mtm);
    utime = atol(buf);
    
    // Converting current time
    (void)time(&t);
    localtime_r(&t, &mtm);
    mtm.tm_isdst = 0;// Making daylight saving off explicitly
    strftime(buf, sizeof(buf), "%s", &mtm);
    ctime = atol(buf);
    
    ret = utime - ctime;
    NSDL4_MEMORY(NULL, NULL, " utime = %ld, ctime = %ld, Diff time(in seconds):: %ld", utime, ctime, ret);
    return ret;
}

// timer, cb_func and cb_args shouldn't be NU
void ns_sleep(timer_type* timer, u_ns_ts_t timeout, TimerProc* cb_func, void *cb_args)
{
  NSDL4_MEMORY(NULL, NULL, "Method Called Timeout:: %llu", timeout);
  ClientData client_data;
  client_data.p = cb_args;
  timer->actual_timeout = timeout;
  dis_timer_add_ex(0, timer, get_ms_stamp(), cb_func, client_data, 0, 0);
}

