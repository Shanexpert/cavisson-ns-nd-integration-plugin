#ifndef NS_MASTER_AGENT_H
#define NS_MASTER_AGENT_H

#define FTP_SCENARIO 1
#define EXTRACT_SCENARIO 2
#define FTP_SCRIPTS 3
#define EXTRACT_SCRIPTS 4
#define RUNNING_SCENARIO_ON_GENERATORS 5
#define FTP_DIV_DATA_FILE 6
#define EXTRACT_DIV_DATA_FILE 7
#define FTP_ABS_DATA_FILE 8
#define EXTRACT_ABS_DATA_FILE 9
#define FTP_DATA_FILE_RTC 10
#define EXTRACT_DATA_FILE_RTC 11
#define CHECK_GENERATOR_HEALTH 12
#define CHECK_JMETER_ENABLED 13
#define UPLOAD_ALL_GEN_DATA 14
#define MAX_GENERATOR_ENTRIES 255

typedef struct master_agent {
  int opcode;
  int port;
  int collect_no_eth_data; // If 1, do not collect ETH data.
  int event_logger_port;
  int debug_on;
  char scen_name[256];
} Master_agent;

//NC: Added struct to hold generator's user/sessions value. 
//Used for redistribution of quantity/sessions
typedef struct
{
  int id;
  int cur_vuser_sess;
  int rem_not_ramped_usr;
  double quantity;
  double rate_val;
} RunningGenValue;

extern RunningGenValue *running_gen_value;
extern int gen_updated_timeout[MAX_GENERATOR_ENTRIES];

extern int master_init(char *user_conf);
extern void wait_for_start_msg(int udp_fp);
extern void extract_external_controller_data();
extern int find_controller_type(char *type);
//RTC functionality
extern int nc_create_epoll_wait_for_controller();
extern void send_rtc_msg_to_controller(int fd, int opcode, char *msg, int grp_idx);
extern int send_rtc_settings_to_generator(char *keyword, int flag, int runtime_id);
extern void remove_epoll_from_controller();
extern void on_failure_resume_gen_update_rtc_log(char *err);
extern int distribute_quantity_among_generators(char *buf, char *err_msg, int runtime_id, int first_time, int *runtime_idx);
extern void get_schedule_detail_frm_generator(int grp_idx);
extern int find_group_idx_using_gen_idx(int gen_id, int grp_id);
extern void log_generator_table();
extern void create_generator_dir_by_name();
extern void ni_create_data_files_frm_last_files();
extern void close_gen_pct_file(int idx);
extern void close_gen_pct_file(int idx);
extern void create_gen_pct_file(int idx);
//extern inline void process_resume_from_rtc();
extern int run_command_in_thread (int mode, int runtime);
extern int wait_for_all_generator();
extern void process_rtc_qty_schedule_detail(int controller_fd, char *msg);
extern void dump_pdf_data_into_file(int idx);
#endif
