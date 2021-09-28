#ifndef NS_KW_SET_NON_RTC_H
#define NS_KW_SET_NON_RTC_H

/* Http Body Check sum header structure and macros*/
#define CHKSUM_HDR_MAX_LEN             128
#define CHKSUM_PFX_MAX_LEN             CHKSUM_HDR_MAX_LEN
#define CHKSUM_SFX_MAX_LEN             CHKSUM_HDR_MAX_LEN
typedef struct
{
  short mode;
  short if_body_empty;
  short if_pfx_sfx;
  char h2_hdr_name[CHKSUM_HDR_MAX_LEN + 1];
  char hdr_name[CHKSUM_HDR_MAX_LEN + 1];
  char pfx[CHKSUM_PFX_MAX_LEN + 1];
  char sfx[CHKSUM_SFX_MAX_LEN + 1];
  int hdr_name_len;
  int h2_hdr_name_len;
  int pfx_len;
  int sfx_len;
}HttpBodyCheckSumHdr;

extern int create_accattr_table_entry(int *row_num);
extern int create_accloc_table_etnry(int *row_num);
extern int create_browattr_table_entry(int *row_num);
extern int create_freqattr_table_entry(int *row_num);
extern int create_locattr_table_entry(int *row_num);
extern int create_machattr_table_entry(int *row_num);
extern int create_metric_table_entry(int *row_num);
//extern int create_thinkprof_table_entry(int *row_num);
extern int create_userindex_table_entry(int *row_num); 
extern int create_userprof_table_entry(int *row_num);
extern int insert_linechar_table_entry (char *src, char *dst, int fw_lat, int rv_lat, int fw_loss, int rv_loss);
extern int find_accattr_idx(char* name);
extern int find_browattr_idx(char* name);
extern int find_machattr_idx(char* name);
extern int find_freqattr_idx(char* name);

extern void kw_set_run_time(char *buf, Global_data *glob_set, int flag);
extern void kw_set_testname(char *buf, Global_data *glob_set, int flag);
extern int kw_set_master(char *buf, Global_data *glob_set, int flag);
extern void kw_set_warmup_time(char *buf, Global_data *glob_set, int flag);
extern void kw_set_default_page_think_time(char *buf, int flag);
extern int kw_set_g_page_think_time(char *buf, char *err_msg, int runtime_flag);
//extern void kw_set_default_session_pacing(char *buf, int flag);
extern void kw_set_clickaway_global_profile(char *buf, Global_data *glob_set, int flag);
extern void kw_set_clickaway_profile(char *buf, Global_data *glob_set, int flag);
extern void kw_set_page_clickaway(char *buf, Global_data *glob_set, int flag);
extern void kw_set_sp(char *buf, Global_data *glob_set, int flag);
//extern void kw_set_use_dns(char *text, GroupSettings *gset, char *err_msg, int flag);
extern int kw_set_client(char *buf, int flag);
extern void kw_set_threshold(char *buf, Global_data *glob_set, int flag);
extern void kw_set_logdata_process(char *buf, Global_data *glob_set, int flag);
extern int kw_set_stype(char *buf, int flag, char *err_msg);
extern void kw_set_error_log(char *buf);
extern void kw_set_target_rate(char *buf, Global_data *glob_set, int flag);
extern int kw_set_metric(char *buf, int flag);
extern int kw_set_nvm_distribution(char *text, Global_data *glob_set, int flag, char *err_msg, char *buf);
extern void kw_set_location(char *buf, int flag);
extern void kw_set_ulocation(char *buf, int flag);
extern void kw_set_uplocation(char *buf, int flag);
extern int kw_set_uaccess(char *buf, int flag, char *err_msg);
extern void kw_set_upaccess(char *buf, int flag);
extern void kw_set_upal(char *buf, int flag);
extern void kw_set_ubrowser(char *buf, int flag);
extern int kw_set_upbrowser(char *buf, int flag, char *err_msg);
extern void kw_set_umachine(char *buf, int flag);
extern void kw_set_upmachine(char *buf, int flag);
extern void kw_set_ufreq(char *buf, int flag);
extern void kw_set_upfreq(char *buf, int flag);
extern int kw_set_g_max_users(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag);
extern int kw_set_use_http_10(char *buf);
extern int sort_userprof_tables(void);
extern void kw_set_upscreen_size(char *buf, int flag);
extern void kw_set_ser_prof_selc_mode(char *buf);
extern int kw_set_ns_server_secondary(char *buf, int flag);
extern int kw_set_health_mon(char *buf, char *err_msg, int runtime_changes);
#endif
