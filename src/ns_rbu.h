#ifndef ns_rbu_h__
#define ns_rbu_h__

#include "tmr.h"

#define RBU_PARAM_BROWSER_USER_PROFILE     0
#define RBU_PARAM_HAR_LOG_DIR              1
#define RBU_PARAM_VNC_DISPLAY_ID           2
#define RBU_PARAM_HAR_RENAME_FLAG          3
#define RBU_PARAM_PAGE_LOAD_WAIT_TIME      4
#define RBU_PARAM_MERGE_HAR_FILES          5
#define RBU_PARAM_SCROLL_PAGE_X            6
#define RBU_PARAM_SCROLL_PAGE_Y            7
#define RBU_PARAM_PRIMARY_CONTENT_PROFILE  8
#define RBU_PARAM_WAIT_FOR_NEXT_REQ        9  
#define RBU_PARAM_PERFORMANCE_TRACE        10  
#define RBU_PARAM_AUTH_CREDENTIAL          11
#define RBU_PARAM_PHASE_INTERVAL           12

#define RBU_ENABLE_RUNTIME_HAR_RENAMING  1
#define RBU_DEFAULT_PAGE_LOAD_WAIT_TIME  65 // Set default value is 65 sec, so that it can take time to made har file 
                                            // when firebug invoked after 60 sec
#define RBU_DEFAULT_PERFORMANCE_TRACE_TIMEOUT  10000 // Set default value is 10 sec, so that it can take time to made performance trace file 
#define RBU_DEFAULT_MERGE_HAR_FILE       0

#define RBU_BM_FIREFOX                   0
#define RBU_BM_CHROME                    1
#define RBU_BM_VENDOR                    99

#define FIREFOX                          0
#define CHROME                           1
#define FIREFOX_AND_CHROME               2

#define NONE_DIR_FLAG                    0
#define SCREEN_SHOT_DIR_FLAG             1
#define SNAP_SHOT_DIR_FLAG               2
#define ALL_DIR_FLAG                     3
#define LIGHTHOUSE_DIR_FLAG              4
#define PERFORMANCE_TRACE_DIR_FLAG       5 

#define MAX_SAMPLE_PROF_LEN              512
#define RBU_REGISTRATION_FILENAME ".registrations.spec"

#define RBU_ACC_LOG_DUMP_HEADER    0      //To dump header in Access Log file
#define RBU_ACC_LOG_DUMP_LOG       1      //Not to dump header, just to dump access log

//har_timeout stores timeout time for HAR 
// if PageLoadWaitTime, then har_timeout = PageLoadWaitTime,
// else if G_RBU_HAR_TIMEOUT, then har_timeout = G_RBU_HAR_TIMEOUT
// else RBU_DEFAULT_PAGE_LOAD_WAIT_TIME
#define HAR_TIMEOUT \
   har_timeout = ((vptr->first_page_url->proto.http.rbu_param.page_load_wait_time > RBU_DEFAULT_PAGE_LOAD_WAIT_TIME) ? \
      vptr->first_page_url->proto.http.rbu_param.page_load_wait_time : \
      runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.har_timeout); 

typedef struct RBU_Param 
{
  ns_bigbuf_t browser_user_profile;
  ns_bigbuf_t har_log_dir;
  ns_bigbuf_t vnc_display_id;
  ns_bigbuf_t primary_content_profile;   // To save tti final string 
  char har_rename_flag;                  // To enable Runtime renaming on
  unsigned short page_load_wait_time;    // To save load time    
  char merge_har_file;                   // To enable merging of har file 
  int scroll_page_x;                     // scroll_page in x axis
  int scroll_page_y;                     // scroll_page in y axis
  ns_bigbuf_t csv_file_name; 
  int csv_fd;
  ns_bigbuf_t tti_prof;                  //Stores tti_ primary content profile name i.e.; 'home2' from PrimaryContentProfile=home2
  int timeout_for_next_req;              //G_RBU_PAGE_LOADED_TIMEOUT, handling from script in millisecond
  int phase_interval_for_page_load;      //G_RBU_PAGE_LOADED_TIMEOUT (phase_interval), handling from script in millisecond

  char performance_trace_mode;                //Enable or Disable performance trace dump
  int performance_trace_timeout;              //performance trace dump timeout value in millisecond
  char performance_trace_memory_flag;         //capture memory stats with performance trace dump
  char performance_trace_screenshot_flag;     //capture screenshot with performance trace dump 
  char performance_trace_duration_level;      //duration level(0/1)

  ns_bigbuf_t auth_username;                  //AuthCredential - username
  ns_bigbuf_t auth_password;                  //AuthCredential - password
}RBU_Param;

typedef struct RBU_Param_Shr 
{
  char *browser_user_profile;
  char *har_log_dir;
  char *vnc_display_id;
  char *primary_content_profile;         // To save tti final string   
  char har_rename_flag;                  // To enable Runtime renaming on
  unsigned short page_load_wait_time;    // To save load time    
  char merge_har_file;                   // To enable merging of har file 
  int scroll_page_x;                     // scroll_page in x axis
  int scroll_page_y;                     // scroll_page in y axis
  char *csv_file_name;                   // To store csv file name 
  int csv_fd;                            // To save fd of open file
  char *tti_prof;                        //Stores tti_ primary content profile name i.e.; 'home2' from PrimaryContentProfile=home2
  int timeout_for_next_req;              //G_RBU_PAGE_LOADED_TIMEOUT, handling from script in millisecond
  int phase_interval_for_page_load;      //G_RBU_PAGE_LOADED_TIMEOUT (phase_interval), handling from script in millisecond

  char performance_trace_mode;           //Enable or Disable performance trace dump
  int performance_trace_timeout;         //performance trace dump timeout value in millisecond
  char performance_trace_memory_flag;         //capture memory stats with performance trace dump
  char performance_trace_screenshot_flag;     //capture screenshot with performance trace dump
  char performance_trace_duration_level;      //duration level(0/1)
  
  char *auth_username;                        //AuthCredential(username:password)
  char *auth_password;                        //AuthCredential(username:password)
}RBU_Param_Shr;

extern char g_ns_firefox_binpath[];
extern char g_rbu_user_agent_str[];
extern char g_rbu_dummy_url[];
extern char g_ns_chrome_binpath[];
extern char g_home_env[];
int is_rbu_web_url_end_done;
//Group based keyword declaration has been moved to util.h
extern int ns_rbu_parse_registrations_file(char *registration_file, int sess_idx, int *reg_ln_no);
extern int kw_set_ns_firefox(char *buf, char *err_mag, int runtime_changes);
extern int kw_set_ns_chrome(char *buf, char *err_mag, int runtime_changes);
//extern int kw_set_rbu_user_agent(char *buf, char *err_mag, int runtime_changes);
//extern int kw_set_rbu_screen_size_sim(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_rbu_post_proc_parameter(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_rbu_enable_dummy_page(char *buf, char *err_msg, int runtime_flag);
extern inline void ns_rbu_kill_browsers_before_start_test();
extern int ns_rbu_set_csv_file_name(int url_idx, char *hostname);
//extern int kw_set_rbu_settings_parameter(char *buf); 
extern int ns_rbu_generate_csv_file(); 
extern inline int ns_rbu_post_proc(); 
extern inline int kw_set_rbu_enable_csv(char *buf, char *err_msg, int runtime_flag);
extern inline int kw_set_rbu_browser_com_settings(char *buf, char *err_msg, int runtime_flag);
extern int make_dir(char *create_dir);
extern int kw_set_rbu_enable_auto_param(char *buf);
extern int ns_rbu_start_vnc_and_create_profiles(char *controller_name);
extern int ns_rbu_on_test_start();
//extern inline int kw_set_tti(char *buf);
extern inline int ns_rbu_check_for_browser_existence();
extern int kw_set_rbu_alert_policy(char *buf, char *err_msg, int runtime_changes);
//extern inline void ns_rbu_stop_browser_on_sess_end(vuser *vptr);
//extern int ns_rbu_execute_page(VUser *vptr, int page_id);
extern int kw_set_rbu_ext_tracing(char *buf, char *err_msg);
extern int remove_directory(const char *dir_path);
extern int kw_set_rbu_domain_stats();
extern int kw_set_g_rbu_waittime_after_onload();
extern void nsi_rbu_handle_page_failure();
extern int kw_set_rbu_mark_measure_matrix();
extern int kw_set_g_rbu_enable_auto_selector();
extern int kw_set_g_rbu_capture_performance_trace();
extern int kw_set_g_rbu_selector_mapping_profile();
extern int kw_set_g_rbu_throttling_setting();
extern int kw_set_g_rbu_reload_har();
extern int kw_set_g_rbu_wait_until();
extern int kw_set_g_rbu_lighthouse_setting();

extern int get_clip_id(char *clip);
extern void ns_rbu_action_after_browser_start_callback(ClientData client_data);
extern void make_rbu_connection_callback(ClientData client_data);
extern void make_click_con_to_browser_v2_callback(ClientData client_data);
#endif //ns_rbu_h__

