/******************************************************************
 * Name    : ns_monitor_profiles.h
 * Author  : Archana
 * Purpose : This file contains methods to read keywords from
             monitor profile file for MONITOR_PROFILE
 * Note:
 * MONITOR_PROFILE <monitor profile name>
 * Modification History:
 * 05/09/08 - Initial Version
*****************************************************************/

#ifndef _ns_monitor_profiles_h
#define _ns_monitor_profiles_h
#define MAX_NAME_LEN 1024
#define MAX_AUTO_MON_BUF_SIZE 4*1024

#define ACTIVATE 1
#define INACTIVATE 0

#define MON_SUCCESS 0
#define MON_FAILURE -1

#define SVR_TYPE_NS 1
#define SVR_TYPE_CONTROLLER 2 
#define SVR_TYPE_GEN 3 
#define SVR_TYPE_NO 4
#define SVR_TYPE_NS_NO 5 //If both NO and NS auto mon are configured on 127.0.0.1, in this case use SVR_TYPE_NS_NO

#define LINUX 5
#define AIX 6
#define SOLARIS 7
#define HPUX_MACHINE 8
#define WIN_MACHINE 9
#define OTHERS 10

#define PS_LINUX "ps -ef"
#define PS_SOLARIS "/usr/ucb/ps -auxwww"
#define DF_LINUX "df -h"
#define DF_AIX "df -k"
#define DF_SOLARIS "/usr/bin/df -h"
#define MEMINFO "vmstat 2 2"

#ifndef MAX_FIELDS
#define MAX_FIELDS 11  //pipe separated max fields in server.dat 
#endif
#define PROCESS_DIFF_JSON 1
#define PROCESS_PRIMARY_JSON 0


//Bit set for TSDB keyword value
#define RTG_MODE 0x01
#define TSDB_MODE 0x02
#define RTG_TSDB_MODE 0X04


extern int g_enable_iperf_monitoring;
extern int g_iperf_monitoring_port;
extern int g_auto_ns_mon_flag;
extern int g_auto_no_mon_flag;
extern int g_cmon_agent_flag; 
extern int g_auto_server_sig_flag;
extern char g_generator_process_monitoring_flag;
extern char g_tsdb_configuration_flag;
extern int g_cmon_port;
typedef struct 
{
  char mon_name[MAX_NAME_LEN];
  int ns_state;
  int no_state;
}AutoMonTable;

typedef struct
{
  char tier_server[MAX_NAME_LEN];
  char server_name[MAX_NAME_LEN];
  char server_disp_name[MAX_NAME_LEN];
  char vector_name[MAX_NAME_LEN];
  char cavmon_home[MAX_NAME_LEN];
  char controller_name[MAX_NAME_LEN];
  int server_type;   //This flag will tell server is - NS(or Controller), NO, Generator
}AutoMonSvrTable;

extern int get_controller_name(char *con_name);
extern int kw_set_monitor_profile(char *profile_name, int num, char *err_msg, int runtime_flag, int *mon_status);
extern int kw_set_enable_ns_monitors(char *keyword, char *buf);
extern void kw_set_enable_generator_process_monitoring(char *buf);
extern void kw_set_enable_process_monitoring(char *buf);
extern int kw_set_enable_no_monitors(char *keyword, char *buf);
extern int kw_set_disable_ns_no_monitors(char *keyword, char *buf);
extern void kw_set_controller_mode_monitors(char *keyword, char *buf);
extern int kw_set_enable_cmon_agent(char *keyword, char *buf);
extern int kw_set_enable_auto_json_monitor(char *keyword, char *buf, char runtime_flag, char *err);

extern int make_ns_auto_mprof();
extern int kw_set_cmon_settings(char *buf, char *err_msg, int run_time_flag);

extern int kw_set_enable_cavmon_inbound_port(char *keyword ,char *buff);
extern int start_iperf_server_for_netcloud_test(char *path, int *port);

extern int apply_java_process_monitor_for_generators();
//Monitors Hierarchical view
extern int kw_enable_hierarchical_view(char *keyword, char *buffer);
extern char *get_vector_prefix(int is_NO, int gen_tbl_idx, int is_controller);
extern void kw_set_enable_recreate_epoll_fd(char *keyword, char *buf);
extern int kw_set_enable_store_config(char *keyword,char *buff);
extern int apply_jmeter_monitors();
#endif
