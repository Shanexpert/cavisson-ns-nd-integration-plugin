#ifndef CDR_CINFIG_H
#define CDR_CONFIG_H

#define CONF_BUFF_SIZE 512

#define CDR_ENABLE 1
#define CDR_DISABLE 0

#define MODE_CLOUD 1
#define MODE_LOCAL 2


enum components
{
  RAW_DATA,
  CSV,
  LOGS,
  DB,
  GRAPH_DATA,
  PERF_TR,
  DBG_TR,
  ARCH_TR,
  GEN_TR,
  HAR_FILE,
  PAGEDUMP,
  TEST_DATA,
  DB_AGG,
  OCX,
  NA_TRACES,
  ACCESS_LOG,
  REPORTS,
  CONFIGS,

  TOTAL_COMPONENTS
};

struct custom_cleanup_struct
{
  char *path;
  int retention_time; // days
};

struct cleanup_struct
{
  int retention_time; // days
};

struct ngve_cleanup_days_struct
{
  char **start_dates;
  char **end_dates;
  int *start_ts;;
  int *end_ts;
  long long int *start_date_pf;
  long long int *end_date_pf;

  int total_entry;
  int max_entry; // use for negative days
};

struct ngve_tr_struct
{
  int *tr;
  int total_entry;
};

struct cdr_config_struct
{
  char enable; // 1: enable 0: not enable
  int log_file_size; 
  int audit_log_file_size; 
  char mode; // cloud or local
  int tr_num;
  char backup_path[CONF_BUFF_SIZE];
  char controller[CONF_BUFF_SIZE];
  struct cleanup_struct cleanup[TOTAL_COMPONENTS];
  struct cleanup_struct cleanup_all_tr[TOTAL_COMPONENTS];
  struct ngve_cleanup_days_struct ngve_cleanup_days[TOTAL_COMPONENTS];
  struct ngve_cleanup_days_struct ngve_days;
  struct ngve_tr_struct ngve_tr;
  char cleanup_flag;
  char cleanup_all_tr_flag;
  char data_remove_flag;

  struct custom_cleanup_struct *custom_cleanup;
  int total_custom_cleanup;
  int recyclebin_cleanup; 
};

extern struct cdr_config_struct cdr_config; // global var that contains all the configuration settings

extern int cdr_process_config_file();
extern void cdr_print_config_to_log();
extern int get_component(char *name);
#endif
