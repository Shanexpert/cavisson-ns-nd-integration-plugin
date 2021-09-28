#ifndef NS_PARENT_H
#define NS_PARENT_H
/******************************************************************
 * Name    :    ns_parent.h
 * Purpose :    This file contains methods related to parent 
                communication
 * Note    :
 * Author  :    Archana
 * Intial version date:    07/04/08
 * Last modification date: 07/04/08
*****************************************************************/
#include "ns_msg_com_util.h"
#define RUN_MODE_OPTION_COMPILE    0x00000001
//NetCloud 
#define CREATE_TAR_AND_EXIT 0x00000001 //Create tar and exit 
#define CREATE_TAR_AND_CONTINUE 0x00000002 //Create tar and continue
#define DO_NOT_CREATE_TAR 0x00000004 //Create tar and continue

#define WRITE_TR_STATUS\
  fprintf(g_instance_fp, "1");\
  fclose(g_instance_fp);\

//Taking mutex lock while update data and control connection glob vars 
#define UPDATE_GLOB_DATA_CONTROL_VAR(X) \
  pthread_mutex_lock(&glob_var_mutex_key); \
  X; \
  pthread_mutex_unlock(&glob_var_mutex_key);

#define SAVE_IP_AND_CONTROLLER_TR\
  if(testidx != -1)\
    sprintf(ip_and_controller_tr, "%s_%d", g_cavinfo.NSAdminIP, testidx); \
  else \
    sprintf(ip_and_controller_tr, "%s", g_cavinfo.NSAdminIP);  

#define KW_NONE 0
#define KW_ARGS 1
#define KW_FILE 2
#define ADD_HDR_MAIN_URL 0
#define ADD_HDR_INLINE_URL 1
#define ADD_HDR_ALL_URL 2

// G_HTTP_HDR
extern int  g_set_args_type;
extern char *g_set_args_value;
 
extern int io_vector_size;
extern int io_vector_size;
extern int init_io_vector_size;
extern int io_vector_delta_size;
extern int io_vector_max_size;
extern int io_vector_max_used;

extern struct iovec *vector;
extern int *free_array;

extern int run_mode_option; //For script compile
extern int ni_make_tar_option; //For NI to make tar of scripts and scenario
//extern int rtc_failed_msg_rev;
extern int rec_all_gen_msg;

extern int g_debug_script;
extern char g_enable_test_init_stat;
//#ifndef CAV_MAIN
extern char g_test_inst_file[64+1];
//#else
//extern __thread char g_test_inst_file[64+1];
//#endif
extern FILE *g_instance_fp;
extern char ip_and_controller_tr[128+1];
extern int netstorm_parent( int argc, char** argv );
extern void handle_parent_sigusr1( int sig );
extern void handle_parent_sigrtmin1( int sig );
extern void parent_save_data_before_end ();
extern int find_generator_idx_using_ip (char *gen_ip, int testidx);
extern Msg_com_con ndc_mccptr;
extern Msg_com_con ndc_data_mccptr;
extern Msg_com_con lps_mccptr;
extern int log_records;
extern void write_generator_table_in_csv();
extern void send_testrun_gdf_to_controller();
extern void process_nc_apply_rtc_message (int controller_fd, User_trace *vuser_trace_msg);
extern void process_nc_get_schedule_detail (int opcode, int grp_idx, char *msg, int *len, char *buff_ptr);
extern void update_generator_list();
extern int process_pause_for_rtc(int opcode, int seq_num, User_trace *msg);
extern inline void process_resume_from_rtc();
extern int process_pause_done_message(int fd, User_trace *msg);
extern int process_resume_done_message(int fd, int opcode, int child_id, int seq_num);
extern void process_nc_rtc_failed_message(int gen_fd, User_trace *msg, int runtime_flag);
extern void find_max_num_nvm_per_generator(Msg_com_con *mccptr, parent_msg *msg, int num_started);
extern int get_ns_port_defined(int ns_port_range_max, int ns_port_range_min, int flag, int *listen_fd, int listenPurpose);
extern void kw_ns_parent_logger_listen_ports(char *buffer);
extern int kw_set_continous_monitoring_check_demon(char *buf);
void handle_parent_rtc_msg(char *tool_msg);
extern void create_db_tables_and_check_compatibility ();
extern int g_gui_bg_test;
extern int g_do_not_ship_test_assets;
extern char memory_based_fs_mode;
extern short memory_based_fs_size;
extern char memory_based_fs_mnt_path[512];
extern pthread_mutex_t glob_var_mutex_key;
extern void end_test_init_running_stage(char *controller_ip);
extern void init_runtime_data_struct();
extern void reset_avgtime();
extern void clean_parent_memory_for_goal_based();
extern void get_testid_and_set_debug_option();
extern int parse_keyword_before_init(char *file_name, char *err_msg);
extern void  init_all_avgtime();
#endif
