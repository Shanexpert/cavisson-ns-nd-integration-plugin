/*****************************

* Name: ns_summary_rpt.h
* Purpose: Contaning the method prototypes for genrating the summary.html
* Author: Anuj Dhiman
* Intial version date: 04/09/08
* Last modification date

*****************************/

#ifndef NS_SUMMARY_RPT_H 
#define NS_SUMMARY_RPT_H

extern void open_and_add_title_srfp_html(void);
extern void add_version_header_srfp_html(char *version_buf, char *g_testrun, int testidx, char *g_test_start_time);
extern void add_test_configuration_srfp_html(TestCaseType_Shr *testcase_shr_mem);

extern void add_object_summary_table_start_srfp_html(void);
extern void add_object_summary_table_row_srfp_html(int obj_type, double min_time, double avg_time, double max_time, u_ns_8B_t num_initiated, u_ns_8B_t num_completed, u_ns_8B_t num_succ);
extern void add_object_summary_table_end_srfp_html(void);

extern void add_vuser_info_srfp_html(int avg_users, char *sessrate, int cur_vusers_active, int cur_vusers_thinking, int cur_vusers_waiting, int cur_vusers_cleanup, int cur_vusers_in_sp, int cur_vusers_blocked, int cur_vusers_paused);
extern void add_other_info_srfp_html(u_ns_8B_t hit_initited_rate, u_ns_8B_t hit_tot_rate, u_ns_8B_t hit_succ_rate, double tcp_rx, double tcp_tx, char *tbuffer, int num_connections, u_ns_8B_t con_made_rate, u_ns_8B_t con_break_rate, double ssl_new, double ssl_reuse_attempted, double ssl_reused);

extern void add_tx_info_table_start_srfp_html(void);
extern void add_tx_info_table_row_srfp_html(int is_initiated, char *tx_name, float min_time, float avg_time, float max_time, float std_dev, u_ns_8B_t num_initiated, u_ns_8B_t num_completed, u_ns_8B_t num_succ);
extern void add_tx_info_table_end_srfp_html(void);

extern void add_obj_percentile_report_table_start_srfp_html(char *mode_buf);
extern void add_obj_percentile_report_table_row_srfp_html(float start_time, float end_time, unsigned int num_responses, float pct, float upper_pct);
void add_obj_percentile_report_table_last_row_srfp_html(float start_time, unsigned int num_responses, float pct, float upper_pct);
extern void add_obj_percentile_report_table_end_srfp_html(void);

extern void add_obj_median_time_info_srfp_html(float data0, float data1, float data2, float data3, float data4);

extern void add_close_button_with_footer(void);
extern void close_srfp_html(void);
extern void end_and_close_srfp_html(void);
extern void log_http_status_codes_to_summary_report(cavgtime *cavg);

#endif

