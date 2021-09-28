#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "tmr.h"
#include "ns_msg_def.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "nslb_comp_recovery.h"
#include "wait_forever.h"
#include "nslb_util.h"
#include "ns_license.h"
#include "ns_trace_level.h"
#include "db_aggregator.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#define MAX_RECOVERY_COUNT                5
#define RESTART_TIME_THRESHOLD_IN_SEC   900


int db_aggregator_pid = -1;
ComponentRecoveryData dbaggregatorRecoveryData;

//Method to recovery db_aggregator only called in controller mode
int db_aggregator_recovery()
{
  NSDL2_DB_AGG(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "db_aggregator_recovery Method called");
  if (db_aggregator_pid != -1)
    return -1;
  
  if(nslb_recover_component_or_not(&dbaggregatorRecoveryData) == 0)
  {
    NSDL2_DB_AGG(NULL, NULL, "DB aggregator not running. Recovering db aggregator.");
    NSTL2(NULL, NULL, "DB aggregator not running. Recovering db aggregator.");

    if(create_db_aggregator_process(1) == 0)
    { 
      NSDL2_DB_AGG(NULL, NULL, "db_aggregator recovered, Pid = %d.", db_aggregator_pid);
      NSTL1(NULL, NULL, "db_aggregator recovered, Pid = %d.", db_aggregator_pid);
    } 
    else    
    {
      NSDL2_DB_AGG(NULL, NULL, "db_aggregator recovery failed");
      NSTL2(NULL, NULL, "db_aggregator recovery failed");
      return -1;
    }
  }
  else
  {
    NSDL2_DB_AGG(NULL, NULL, "db_aggregator max restart count is over. Cannot recover db_aggregator"
            " Retry count = %d, Max Retry count = %d", dbaggregatorRecoveryData.retry_count, dbaggregatorRecoveryData.max_retry_count);
    NSTL1(NULL, NULL, "db_aggregator max restart count is over. Cannot recover db_aggregator"
            " Retry count = %d, Max Retry count = %d", dbaggregatorRecoveryData.retry_count, dbaggregatorRecoveryData.max_retry_count);
    return -1;
  }
  return 0;
}


int create_db_aggregator_process(int recovery_flag)
{
  NSTL1(NULL, NULL, "create_db_aggregator_process Method called");
  char aggregator_path[512];
  NSDL2_DB_AGG(NULL, NULL, "Method called");

  if((db_aggregator_pid = fork()) == 0){
     char tr_num[50];
     char wdir[512] = "";
     char conf_file[1024] = "";

     if(getenv("NS_WDIR") == NULL)
       strcpy(wdir, "/home/cavisson/work");
     else
       strcpy(wdir, getenv("NS_WDIR"));

     sprintf(tr_num, "%d", testidx);
     snprintf(conf_file, 1024, "%s", global_settings->db_aggregator_conf_file);
     sprintf(aggregator_path, "%s/bin/nd_aggregate_reports", wdir);

     NSDL2_DB_AGG(NULL, NULL, "Running nd_aggregate_reports, %s -t %s -c %s", aggregator_path, tr_num, conf_file);

     if((execlp(aggregator_path, aggregator_path, "-t", tr_num, "-c", conf_file, NULL)) == -1)
     {
       NSDL2_DB_AGG(NULL, NULL, "Error in initializing db_aggregator, Error: %s\n", nslb_strerror(errno));
       NSTL2(NULL, NULL, "Error in initializing db_aggregator, Error: %s", nslb_strerror(errno));
       NS_EXIT(1, CAV_ERR_1000043, errno, nslb_strerror(errno));
     }

  }else{
    if(db_aggregator_pid < 0){
      NSDL2_DB_AGG(NULL, NULL, "Error: Unable to fork db_aggregator");
      NSTL2(NULL, NULL, "Error: Unable to fork db_aggregator");
      if(recovery_flag == 0){
        NS_EXIT(-1, CAV_ERR_1000044, errno, nslb_strerror(errno));
      }
      else 
         return -1;
    }
  }
  NSDL2_DB_AGG(NULL, NULL, "Db_aggregator starts\n");
  NSTL1(NULL, NULL, "DB aggregator starts\n");
  return 0;
}

void init_component_rec_and_start_db_aggregator()
{
   NSTL1(NULL, NULL, "init_component_rec_and_start_db_aggregator Method called");
   if(nslb_init_component_recovery_data(&dbaggregatorRecoveryData, MAX_RECOVERY_COUNT, (global_settings->progress_secs/1000 + 5), RESTART_TIME_THRESHOLD_IN_SEC) == 0)
     {
       NSDL2_DB_AGG(NULL, NULL, "Recovery data initialized with"
                     "MAX_RECOVERY_COUNT = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                      MAX_RECOVERY_COUNT, RESTART_TIME_THRESHOLD_IN_SEC);
     }
     else
     {
       NSDL2_DB_AGG(NULL, NULL, "Method Called. Component recovery could not be initialized\n");
     }

     create_db_aggregator_process(0);
}
