#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "cdr_main.h"
#include "cdr_log.h"
#include "cdr_cache.h"
#include "cdr_config.h"
#include "cdr_utils.h"
#include "cdr_file_handler.h"
#include "cdr_cleanup.h"
#include "cdr_cmt_handler.h"
#include "cdr_nv_handler.h"
#include "cdr_manual_del.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"


/* Global variables */
char ns_wdir[CDR_FILE_PATH_SIZE] = "\0";
char *logsPath = NULL;
char hpd_root[CDR_FILE_PATH_SIZE] = "\0";

char nv_client_id[CDR_FILE_PATH_SIZE] = "NA"; 

int g_tr_num = 0;
long long int g_partition_num = 0;
char g_component_name[CDR_FILE_PATH_SIZE] = "\0";

char cache_file_path[CDR_FILE_PATH_SIZE];
char cmt_cache_file_path[CDR_FILE_PATH_SIZE];
char test_cycle_cache_file_path[CDR_FILE_PATH_SIZE];
char nv_cache_file_path[CDR_FILE_PATH_SIZE];
char config_file_path[CDR_FILE_PATH_SIZE];
int cmt_tr_num = 0; 
long long int cur_time_stamp;
long long int cur_time_stamp_with_no_hr;



int cmt_tr_cache_idx = -1;
/*********************************/
static inline void set_file_path()
{
  if (!ns_wdir[0])
    strcpy (ns_wdir, getenv ("NS_WDIR"));

  sprintf(config_file_path, "%s/sys/data_retention/config.json", ns_wdir);
  sprintf(cache_file_path, "%s/logs/data_retention/cache/test_runs/summary_all_trs.dat", ns_wdir);
  sprintf(cmt_cache_file_path, "%s/logs/data_retention/cache/test_runs/summary_cmt_patition.dat", ns_wdir);
  sprintf(test_cycle_cache_file_path, "%s/logs/data_retention/cache/test_cycles/summary_all_tcs.dat", ns_wdir);
  sprintf(nv_cache_file_path, "%s/logs/data_retention/cache/nv/summary_nv_partiton.dat", ns_wdir);
}

inline void cdr_init(int check_pid)
{
  CDRTL2("Method called");

  set_file_path();
  //set cache file path and config file path
  //sprintf(cache_file_path, "%s/logs/data_retention/cache/summary_all_trs.dat", ns_wdir);
  //sprintf(config_file_path, "%s/logs/data_retention/cache/test_runs/summary_all_trs.dat", ns_wdir); // Abhi : set config file path
  //sprintf(config_file_path, "%s/logs/data_retention/cache/test_runs/summary_cmt_patition.dat", ns_wdir); // Abhi : set config file path
  //sprintf(config_file_path, "%s/logs/data_retention/cache/nv/summary_nv_partiton.dat", ns_wdir); // Abhi : set config file path
  /*Check if cav data retenetion managder is already running*/
  if(check_pid)
    check_and_set_cdr_pid();
    
  check_and_set_lmd_config_file();
  
  
  cur_time_stamp = time(NULL); // setting current timestamp
  cur_time_stamp_with_no_hr = (cur_time_stamp / (ONE_DAY_IN_SEC)) * (ONE_DAY_IN_SEC);
  CDRTL3("cur_time_stamp = '%lld', cur_time_stamp_with_no_hr = '%lld'", cur_time_stamp, cur_time_stamp_with_no_hr);

  //Get CMT test run number from config.ini
  cmt_tr_num = get_cmt_tr_number();

  // code to get machine type
  char err_msg[1024] = "\0";
  if(nslb_init_cav_ex(err_msg) == -1)
  {
    CDRTL3("While getting config, error msg '%s'", err_msg);
  }
 // sprintf(g_cavinfo.config, "NV");  
  CDRTL1("Machine Type '%s'", g_cavinfo.config);
  if (!strcmp(g_cavinfo.config, "NV"))
    cdr_get_nv_client_id();

  // init norm method for test idx
  nslb_init_norm_id_table(&cache_entry_norm_table, CACHE_ENTRY_NORM_TABLE_SIZE);
  nslb_init_norm_id_table(&cmt_cache_entry_norm_table, CACHE_ENTRY_NORM_TABLE_SIZE);
  nslb_init_norm_id_table(&nv_cache_entry_norm_table, CACHE_ENTRY_NORM_TABLE_SIZE);

  CDRTL2("Method Exit, rebuild_cache=%d", rebuild_cache);
}

static inline void cdr_print_usage(char *progname, char *err)
{
  printf("Error : %s\n"
         "%s\n"
         "\t--n controller name, if not parse defile it will take '/home/cavisson/work/'\n"
         "\t--n hpd_root name, if not parse defile it will take '/var/www/hpd/'\n"
         "\t--t testid \n"
         "\t--P partition \n"
         "\t--c component name \n", err, progname);
}


static inline void cdr_parse_args(int argcount, char **argvector)
{
  char c;
  char msg[64] = "";

  while ((c = getopt(argcount, argvector, "n:t:P:c:h:")) != -1)
  {
    switch (c)
    {
      case 'n':
        snprintf(ns_wdir, CDR_FILE_PATH_SIZE, "%s", optarg);
        break;

      case 't':
        g_tr_num = atoi(optarg);
        break;

      case 'P':
        g_partition_num = atoll(optarg);
        break;

      case 'c':
        snprintf(g_component_name, CDR_FILE_PATH_SIZE, "%s", optarg);
        break;

      case 'h':
        snprintf(hpd_root, CDR_FILE_PATH_SIZE, "%s", optarg);
        break;


      case ':':
      case '?':
        snprintf(msg, 64, "Invalid argument %c", c);
        cdr_print_usage(argvector[0], msg);
        exit(1);
    }
  }
  // setting ns_wdir
  if((ns_wdir[0] == '\0')) // NS wdir is not parse from commandline 
  {
    snprintf(ns_wdir, CDR_FILE_PATH_SIZE, "%s", getenv ("NS_WDIR"));
  }

  if((hpd_root[0] == '\0')) // NS wdir is not parse from commandline 
  {
    snprintf(hpd_root, CDR_FILE_PATH_SIZE, "%s", getenv ("HPD_ROOT"));
  }
  
  CDRTL1("ns_wdir = '%s' and hpd_root = '%s'", ns_wdir, hpd_root);
  if(g_tr_num != 0 || g_partition_num != 0 || g_component_name[0] != '\0')
  {
    cdr_handle_manual_delete();
    exit(0);
  }

}
#ifdef CDR
int main(int argc, char **argv)
{
  //TODO: for local testing only
  //system("rm -f drm_trace.log"); // keep this for now log file is appending

  cdr_config.log_file_size = 10 * 1024 * 1024;
  cdr_config.audit_log_file_size = 10 * 1024 * 1024;

  set_file_path();
  cdr_parse_args(argc, argv);
  CDRAL(0, 0, 0, "NA", 0, "NA", "Data Retention start for today", 0, "Retention Manager");
  CDRTL2("Method called");
  cdr_init(1); // init cdr program

  if (cdr_process_config_file() == CDR_ERROR) // processing of config.json
  {
    CDRTL1("Error: error in reading config.json file, exiting ...");
    exit(CDR_ERROR);
  }
  if (cdr_config.enable == CDR_DISABLE)
  {
    printf ("Please ENABLE Data Retention flag in config.json\n");
    return 0;
  }
#ifdef DEBUG
  cdr_print_config_to_log();
#endif

  if(rebuild_cache != CDR_TRUE) // no need to read cache file if we need to rebuild the cache
  {
    if(cdr_read_cache_file(0) == CDR_ERROR) {
      CDRTL1("Error: cache file read failed");
    }
    if(cdr_read_cmt_cache_file(0) == CDR_ERROR) {
      CDRTL1("Error: cache file read failed");
    }
    if (!strcmp(g_cavinfo.config, "NV"))
      if(cdr_read_nv_cache_file(0) == CDR_ERROR) {
        CDRTL1("Error: cache file read failed");
      }
  }
  
  if(cdr_get_tr_list_from_disk(1) == CDR_ERROR) {
    CDRTL1("Error: getting tr list from disk failed");
    exit(CDR_ERROR);
  }

  if (cmt_tr_cache_idx  != 0)
  {
    cdr_get_cmt_partition_from_disk();
  }

  if (!strcmp(g_cavinfo.config, "NV"))
    cdr_get_nv_partition_from_disk();

  cleanup_process();

  cmt_cleanup_process();

  if (!strcmp(g_cavinfo.config, "NV"))
    nv_cleanup_process();


  cdr_handle_other_cleanup();
  cdr_handle_custom_cleanup();
  cdr_recyclebin_cleanup();
  cdr_remove_sm_data();

  cdr_dump_cache_to_file();
  cdr_dump_cmt_cache_to_file();
  cdr_dump_nv_cache_to_file();

  
  /*
  struct cdr_cache_entry *ent;
  ent = get_cache_entry(4203);

  if(ent) {
    printf("TR: %d\n", ent->tr_num);
    printf("report progres: %s\n", ent->report_progress);
  } else
    printf("Not found\n");
  */

  CDRAL(0, 0, 0, "NA", 0, "NA", "Data Retention end for today", 0, "Retention Manager");
  CDRTL2("Method exit");
  return 0;
}
#endif

