#include "ni_schedule_phases_parse.h"
#include <openssl/md5.h>
#include "../../../ns_test_init_stat.h"
#include "nslb_alloc.h"
#ifndef KEYWORD_PARSING_H
#define KEYWORD_PARSING_H

//Macro
//Generator name
#define GENERATOR_NAME_LEN 512
#define INIT_GENERATOR_ENTRIES 256
#define DELTA_GENERATOR_ENTRIES 128

//Generator used list entries
#define INIT_USED_GENERATOR_ENTRIES 256
#define DELTA_USED_GENERATOR_ENTRIES 128

//Scenario file path
#define FILE_PATH_SIZE   4096
#define MAX_COMMAND_SIZE  1024
//Keyword parsing
#ifndef MAX_DATA_LINE_LENGTH 
  #define MAX_DATA_LINE_LENGTH 2048
#endif
#define MAX_CONF_LINE_LENGTH 16*1024 

//SGRP parsing
#define INIT_SGRP_ENTRIES 256
#define DELTA_SGRP_ENTRIES 128
#define MAX_GENERATORS 100

//Global Vars
#define PCT_MODE_NUM            0
#define PCT_MODE_PCT            1
#define PCT_MODE_NUM_AUTO       PCT_MODE_NUM
#define DEFAULT_CLUST_IDX      -1
//STYPE
#define TC_FIX_CONCURRENT_USERS 0 //FCU
#define TC_FIX_USER_RATE 1     //FSR
#define TC_MIXED_MODE 99 //MixMode
//Exit status
#define SUCCESS_EXIT 0
#define FAILURE_EXIT -1

//Schedule Types
#define SCHEDULE_TYPE_SIMPLE    0
#define SCHEDULE_TYPE_ADVANCED  1
//Schedule By
#define SCHEDULE_BY_SCENARIO    0
#define SCHEDULE_BY_GROUP  1

#define PROGRESS_INTERVAL_NC 30000
#define PROGRESS_INTERVAL_NDE_NVSM 60000
#define PROGRESS_INTERVAL_NDE 60000

//debug log
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

#define NIDL(log_level, ...)  ni_debug_logs(log_level, _FLN_, __VA_ARGS__)

#define MAKE_ABS_TO_REL(a) (a+15)

#define MAX_DEBUG_LOG_BUF_SIZE 64000

//shipping gen data
#define SHIP_TEST_ASSETS 0
#define DO_NOT_SHIP_TEST_ASSETS 1

typedef struct GeneratorUsedList 
{
  char generator_name[GENERATOR_NAME_LEN]; 
}GeneratorUsedList;


#define IS_GEN_ACTIVE        0x00000001
#define IS_GEN_REMOVE        0x00000002
#define IS_GEN_INACTIVE      0x00000004
#define SCEN_DETAIL_MSG_SENT 0x00000008
#define RCVD_GEN_FINISH      0x00000010
#define IS_GEN_FAILED        0x00000020
#define IS_GEN_KILLED        0x00000040

typedef struct GeneratorEntry 
{
  char mark_gen;
  unsigned char gen_name[GENERATOR_NAME_LEN];// Name of the generator, size is same as in RunProfTableEntry struct.
  char IP[128]; //xxx.yyy.zzz.aaa\0, 3+1+3+1+3+1+3+1
  char resolved_IP[128]; //Used to store dns resolved ip of given generators. Format: IPV4:xxx.yyy.zzz.aaa\0.port, 3+1+3+1+3+1+3+1
  char agentport[6];
  char location[512];
  char work[512]; //default value is work
  char gen_type[9]; //Generator type whether genrator reside internally or on cloud
  char comments[512];
  char gen_path[GENERATOR_NAME_LEN];
  char gen_keyword[24000];
  int fd; //To save the fd
  int used_gen_flag; //Enable flag for used generator
  int ramp_up_vuser_or_sess_per_gen; /*Used to store virtual users or sessions among generators*/
  int ramp_down_vuser_or_sess_per_gen; /*Used to store virtual users or sessions among generators*/
  int testidx; //Used to store test run number
  int pct_fd;
  //Schedule pointers per generator
  schedule *scenario_schedule_ptr;
  schedule *group_schedule_ptr;
  FILE *rtg_fp;
  int flags;
  int gen_flag;
  int num_groups; 
  int group_id_list[1000];/*Hardcorded value, max number of group ids on a generator*/
  int resolve_flag;
  unsigned long long last_prgrss_rpt_time;
  unsigned int last_prgrss_rpt_elapsed;
  char mark_gen_delayed_rep_msg;// used to show for which generator the delayed report has been logged.
  char mark_killed_gen;//used to mark killed genrator.
  char test_start_time_on_gen[128];
  char test_end_time_on_gen[128];
  int total_killed_nvm;
  int total_nvms;
  char send_buff[4096];
  int msg_len;
  int num_cvms;
  int num_cpus;
  int con_gen_com_diff;
  unsigned char token[2*MD5_DIGEST_LENGTH +1];
  int loc_idx;
  int pct_value;
}GeneratorEntry;

typedef struct LocationTable 
{
  char name[512];
  int genCapacity;
  int pct;
  int numUserOrSession;
  int genRequired;
  int startIndexGenTbl;
} LocationTable;

typedef struct FCSPerGenerator
{
  int quantity;
  int session_limit;
  int pool_size;
} perGenFCSTable;

typedef struct 
{
  //Checking generator health
  char check_generator_health;  // Mode of CHECK_GENERATOR_HEALTH
  int minDiskAvailability;  // Min disk availability.
  int minCpuAvailability;  // Min cpu utilization.
  int minMemAvailability;  // Min memory availability.
  int minBandwidthAvailability;  // Min BW/Ethernet speed on generator
} CheckGenHealth;

#define STOP_TEST 0
#define CONT_TEST 1

extern int num_users; //NUM_USERS
extern int prof_pct_mode;    //PROF_PCT_MODE
extern int mode; //Scenario Type
extern int schedule_type; //Schedule Type
extern int schedule_by; //Schedule By
extern int total_sessions; // TARGET_RATE
extern perGenFCSTable* per_gen_fcs_table;

extern int total_sgrp_entries;
extern int sgrp_used_genrator_entries;
extern char netomni_scenario_file[FILE_PATH_SIZE];
extern char netomni_proj_subproj_file[FILE_PATH_SIZE];
extern int total_generator_entries;
extern int max_generator_entries;
extern char controller_dir[FILE_PATH_SIZE];//Added for script distribution of file paramter
extern char work_dir[FILE_PATH_SIZE];
extern int test_run_num;
extern int total_used_generator;
extern int check_health_kill_gen;
extern int max_used_generator_entries;
extern int g_per_cvm_distribution;

extern GeneratorEntry* generator_entry;
extern GeneratorUsedList* gen_used_list;
extern CheckGenHealth checkgenhealth;
/*Add variable for storing num-connection and Vusers request per minute*/
extern double vuser_rpm;		     /* TARGET_RATE; Vusers request per minute */  
extern int num_connections;          /* NUM_USERS*/
extern int netcloud_mode; /*NETCLOUD_MODE*/
extern int continue_on_file_param_dis_err;
extern int total_gen_location_entries;

/*PROGRESS interval variable*/
extern int progress_msecs;

/*FCS variables*/
extern int enable_fcs_settings_mode;
extern int enable_fcs_settings_limit;
extern int enable_fcs_settings_queue_size;

extern char **g_data_dir_table;

extern void ni_debug_logs(int log_level, char *filename, int line, char *fname, char *format, ...);
extern int init_scenario_distribution_tool(char *scen_name, int debug_levl, int test_num, int tool_call_at_init_rtc, int default_gen_file, int for_quantity_rtc, char *err_msg);
extern void kw_set_controller_server_ip (char *buf, int flag, char *out_buff, int *port);
extern int kw_set_ns_gen_fail(char *buf, char *err_msg, int runtime_flag);
extern int get_tokens_(char *read_buf, char *fields[], char *token, int max_flds);
extern int create_generator_table_entry(int* row_num);
extern void init_generator_entry();
extern int sort_generator_list (const void *G1, const void *G2);
extern void ns_copy_jmeter_scripts_to_generator();
#endif //KEYWORD_PARSING_H
