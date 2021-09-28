/*********************************************************************************************
* Name                   : ns_parse_scen_conf.h 
* Author                 : Arun Nishad
* Intial version date    : Tuesday, January 27 2009 
* Last modification date : -
*********************************************************************************************/


#ifndef _ns_parse_scen_conf_h
#define _ns_parse_scen_conf_h

#define RUNTIME_CHANGES_CONF "runtime_changes.conf"
#define SORTED_RUNTIME_CHANGES_CONF "sorted_runtime_changes.conf"
#define RUNTIME_CHANGES_CONF_ALL "runtime_changes_all.conf"

#define PROGRESS_INTERVAL_NS 10000
#define PROGRESS_INTERVAL_NC 30000
#define PROGRESS_INTERVAL_NO 60000
#define PROGRESS_INTERVAL_NDE 60000
#define PROGRESS_INTERVAL_NDE_NVSM 60000
#define PROGRESS_INTERVAL_NV 60000
#define PROGRESS_INTERVAL_ED 60000

extern int enable_test_metrics_mode;
extern int g_mssql_data_buf_size;
extern int g_script_or_url;
extern int jm_script;
extern int process_monitoring_flag;
extern int sort_conf_file();
extern void init_default_values();
extern int read_scripts_glob_vars();
extern void read_default_file();
extern void read_default_keyword_file();
/* Parses conf file  */
extern void read_conf_file(char *filename);
extern int kw_set_tcp_keepalive(char *buff);
extern int kw_set_max_url_retries(char *buf, int *to_change, short *retry_on_timeout, char *err_msg, int runtime_flag);
extern int kw_set_get_no_inlined_obj(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_no_validation(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_reuseaddr(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_no_http_compression(char *buf, unsigned int *to_change, char *err_msg);
extern int kw_set_disable_host_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_ua_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_accept_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_accept_enc_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_ka_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_connection_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_disable_all_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_use_recorded_host_in_host_hdr(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_auto_redirect(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_on_eurl_err(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_err_code_ok(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_logging (char *buf, short *log_level_new, short *log_dest_new, char *err_msg);
/*extern int kw_set_tracing (char *buf, short *to_change_trace_level, short *to_change_trace_dest, short *to_change_trace_on_fail, 
                           int *to_change_max_log_space, char *err_msg);*/
int kw_set_tracing (char *buf, short *to_change_trace_level, short *to_change_max_trace_level, short *to_change_trace_dest, short *to_change_max_trace_dest, short *to_change_trace_on_fail, int *to_change_max_log_space, short *to_change_trace_inline_url, short *to_change_trace_limit_mode, double *to_change_trace_limit_mode_val, char *err_msg, int runtime_flag);
extern int kw_set_reporting (char *buf, short *to_change, char *err_msg, int runtime_flag);
extern void kw_set_exclude_failed_aggregates(char *buf, short *to_change);
extern int kw_set_url_idle_secs(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_idle_secs(char *buf, int *to_change, char *err_msg);
extern int kw_set_idle_msecs(char *buf,  GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_user_cleanup_msecs(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_g_session_pacing_runtime(char *buf, PacingTableEntry_Shr *pageTable, char *err_msg);
extern int kw_set_g_first_session_pacing_runtime(char *buf, PacingTableEntry_Shr *pacingTable_ptr, char *err_msg);
extern int kw_set_g_new_user_on_session_runtime(char *buf, PacingTableEntry_Shr *pacingTable_ptr, char *err_msg);
extern int kw_set_page_think_time_runtime(char *buf, char *err_msg);
extern int kw_set_ka_pct(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_num_ka(char *buf, int *to_change_min, int *to_change_range, char *err_msg, int runtime_flag);
extern void parse_runtime_changes();
extern int parse_group_keywords(char *line, int pass, int line_num);
extern int kw_set_enable_referer(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_max_users(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_log_shr_buf_size(char *buf, char *err_msg, int rumtime_flag);
extern void create_default_global_think_profile(void);
extern void create_default_pacing(void);
extern void val_sgrp_name (char *line, char *sgrp_name, int line_num);

extern int find_group_idx(char *grp_name);
extern int kw_set_traceing_limit(char *buf, int *to_chagnge_max_trace_param_entries, int *to_change_max_trace_param_value_size, char *err_msg);
extern void kw_set_inline_url_filter(char * buff);
extern void kw_set_multidisk_path(char *buf);
extern void kw_set_tablespace_info(char *buf);
extern int kw_check_use_of_gen_specific_kwd_file(char *buf);
extern int kw_save_nvm_file_param_val(char *buf, int runtime_flag, char *err_msg);
extern int kw_set_nd_monitor_log(char *buff, char *err);
extern int kw_set_g_http_mode(char *buf, short *http_mode, char *err_msg, int runtime_flag);
/*bug 70480 : signature updated*/
int kw_set_g_http2_settings(char *buf,GroupSettings *, char *err_msg, int runtime_flag);
extern int kw_set_test_metrics_mode(char *buff);
extern int kw_set_mssql_stats_monitor_buf_size(char *buff); 
extern void get_jmeter_script_name(char *jmx_sess_name, char *grp_proj_name, char *grp_subproj_name, char *sess_name);
extern int check_json_format(char *filename);
extern void kw_set_enable_nc_tcpdump(char *buf);
extern int kw_set_enable_alert(char *buf, char *err_msg, int runtime);
extern void kw_set_enable_memory_map(char *buf);
extern int kw_set_tsdb_configuration(char *buf);
extern int run_and_get_cmd_output(char *cmd, int out_len , char *output);
extern void kw_set_write_rtg_data_in_db_or_csv(char *buf);
extern void kw_set_write_rtg_data_in_influx_db(char *buf);
extern int kw_set_num_nvm(char *buf, Global_data *glob_set, int flag, char *err_msg);

#endif
