#include "nslb_multi_thread_trace_log.h"

#define LINE_LENGTH 8195
#define MAX_BUFFER_LEN 8195
#define MAX_USERPROF_TYPE_LENGTH 15
#define MAX_SELAGENDA_LENGTH 3
#define MAX_SVRTYPE_LENGTH 31

#define INIT_RUNPROFLOG_ENTRIES 8
#define DELTA_RUNPROFLOG_ENTRIES 8
#define INIT_USERPROFLOG_ENTRIES 8
#define DELTA_USERPROFLOG_ENTRIES 8
#define INIT_SESSPROFLOG_ENTRIES 8
#define DELTA_SESSPROFLOG_ENTRIES 8
#define INIT_RECSVRTABLELOG_ENTRIES 8
#define DELTA_RECSVRTABLELOG_ENTRIES 8
#define INIT_ACTSVRTABLELOG_ENTRIES 8
#define DELTA_ACTSVRTABLELOG_ENTRIES 8
#define INIT_PHASETABLELOG_ENTRIES 8
#define DELTA_PHASETABLELOG_ENTRIES 8

#define NO_TYPE    -1
#define URL_TYPE     0
#define SESSION_TYPE 1
#define PAGE_TYPE    2
#define TX_TYPE      3
#define GEN_TYPE     4
#define GROUP_TYPE   5
#define HOST_TYPE    6

typedef struct {
  char test_name[LINE_LENGTH+1];
  char test_type[LINE_LENGTH+1];
  short wan_env;
  int conn_rpm_target;
  short num_proc;
  int ramp_up_rate;
  int prog_msec;
  int run_length;
  int idle_sec;
  short ssl_pct;
  short ka_pct;
  short num_ka;
  int mean_think_ms;
  int median_think_ms;
  int var_think_ms;
  int think_mode;
  int reuse_mode;
  int user_rate_mode;
  int ramp_up_mode;
  int user_cleanup_ms;
  short max_conn_per_user;
  char sess_recording_file[LINE_LENGTH+1];
  short health_mon;
  int guess;
  char guess_conf;
  short stab_num_success;
  short stab_max_run;
  short stab_run_time;
  char sla_metric_entries[LINE_LENGTH+1];
  int start_time;
  int end_time;
} TestCaseLogEntry;

typedef struct {
  unsigned int group_num; // Neeraj - Changed from int to unsigned Long on Oct 2, 2010
  unsigned int userprof_id;
  unsigned int sessprof_id;
  short pct;
} RunProfileLogEntry;

typedef struct {
  unsigned int userprof_id;
  char userprof_name[LINE_LENGTH+1];
  char userprof_type[MAX_USERPROF_TYPE_LENGTH + 1];
  unsigned int value_id;
  char value[LINE_LENGTH+1];
  short pct;
} UserProfileLogEntry;

typedef struct {
  unsigned int sessprof_id;
  unsigned int sess_id;
  char sessprof_name[LINE_LENGTH+1];
  short pct;
} SessionProfileLogEntry;

typedef struct {
  unsigned int server_group;// Neeraj - Changed from int to unsigned Long on Oct 2, 2010
  unsigned int rec_svr_id;
  char rec_svr_name[LINE_LENGTH + 1];
  unsigned short rec_svr_port;
  char select_agenda[MAX_SELAGENDA_LENGTH + 1];
  char server_type[MAX_SVRTYPE_LENGTH + 1];
} RecSvrTableLogEntry;

typedef struct {
  unsigned int server_group; // Neeraj - Changed from int to unsigned Long on Oct 2, 2010
  char act_svr_name[LINE_LENGTH + 1];
  unsigned short act_svr_port;
  char location[LINE_LENGTH + 1];
  unsigned int group_idx;   //Added as SERVER_HOST is renamed to G_SERVER_HOST
} ActSvrTableLogEntry;

typedef struct {
  unsigned short phase_id; 
  char group_name[LINE_LENGTH + 1];
  unsigned short phase_type;
  char phase_name[LINE_LENGTH + 1];
} PhaseTableLogEntry;

extern int DynamicTableSizeiUrl; // TODO - Remove this
extern int DynamicTableSizeiTx; // TODO - Remove this

extern long long partition_idx;
extern MTTraceLogKey *lr_trace_log_key;

