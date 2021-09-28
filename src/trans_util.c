#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "smon.h"
#include "ns_summary_rpt.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "wait_forever.h"
#include "output.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_data_types.h"
#include "ns_monitoring.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "nslb_get_norm_obj_id.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_msg_def.h"
#include "ns_trans_parse.h"
#include "ns_exit.h"
#ifndef CAV_MAIN
extern NormObjKey normRuntimeTXTable;
#else
extern __thread NormObjKey normRuntimeTXTable;
#endif

//this macro calculate the cummulative data for transaction
#define CALC_CUM_TX_RPT\
    tx_c_fetches_completed += num_completed;\
    tx_c_num_initiated += num_initiated;\
    tx_c_num_succ += num_succ;\
    tx_c_num_samples += num_samples;\
    tx_c_num_netcache_hits += num_netcache_fetches;\
    SET_MIN(c_min_time, min_time);\
    SET_MAX(c_max_time, max_time);\
    c_avg_time = (double)(((double)(c_tot_time))/((double)(1000.0*(double)tx_c_num_samples)));\
    c_std_dev = get_std_dev (tx_c_tot_sqr_time, c_tot_time, c_avg_time, tx_c_num_samples); 

//NC: In release 3.9.3, TxData has been changed to double pointer now it holds TxData pointers 
//for NetCloud: its number of generators + controller.
//for standalone: its one pointer
TxDataCum **gsavedTxData = NULL;
//TxDataSample *txData;
TxDataCum *txCData;
static char * no_succ_time_string_tx = "min - sec, avg - sec, max - sec, stddev -";

#define MAX_TRANS_BUF_LEN 1024*64

//this method is to create trans_not_run.dat in logs dir
static void create_trans_not_run_data_file(char *buf, char *mode, int gen_idx)
{
  FILE *txfp=NULL;
  char file_name[1024]="\0";

  NSDL2_TRANS(NULL, NULL, "Method called. gen_idx = %d", gen_idx);
  //109015 -In NC test this 2  trans_detail.dat and trans_not_run.dat files shouldn't be created so returning.
  if(loader_opcode == CLIENT_LOADER)
     return;
  if (gen_idx == -1 )
  {
    sprintf(file_name, "logs/TR%d/trans_not_run.dat", testidx);
  }
  else
  {
    sprintf(file_name, "logs/TR%d/NetCloud/%s/TR%d/trans_not_run.dat", testidx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].testidx);
  }
  if ((txfp = fopen(file_name, mode)) == NULL)
  {
    NSDL2_TRANS(NULL, NULL, "Unable to open file = %s", file_name);
    NS_EXIT(-1, "Error: Unable to open file = %s, error is %s ", file_name, nslb_strerror(errno));
  }  
  fprintf(txfp, "%s", buf);
  fclose(txfp);
}

//this method is to create trans_detail.dat in logs dir
static void create_trans_detail_data_file(char *buf, char *mode, int gen_idx)
{
  FILE *txfp=NULL;
  char file_name[1024]="\0";

  NSDL2_TRANS(NULL, NULL, "Method called. gen_idx = %d", gen_idx);
  //BUG 109015 -- In NC test this 2  trans_detail.dat and trans_not_run.dat files shouldn't be created so returning.
  if(loader_opcode == CLIENT_LOADER)
     return;
  /*if((loader_opcode == MASTER_LOADER) && !(generator_entry[gen_idx].flags & IS_GEN_ACTIVE))
   Bug 61192: gen_idx = -1 causing core dump */
  if((loader_opcode == MASTER_LOADER) && (gen_idx != -1) && !(generator_entry[gen_idx].flags & IS_GEN_ACTIVE))
    return;

  if (gen_idx == -1)
  {
    sprintf(file_name, "logs/TR%d/trans_detail.dat", testidx);
  }
  else
  {
    sprintf(file_name, "logs/TR%d/NetCloud/%s/TR%d/trans_detail.dat", testidx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].testidx);
  }
  if ((txfp = fopen(file_name, mode)) == NULL)
  {
    NSDL2_TRANS(NULL, NULL, "Unable to open file = %s", file_name);
    NS_EXIT(-1, "Error: Unable to open file = %s, error is %s ", file_name, nslb_strerror(errno));
  }  
  fprintf(txfp, "%s", buf);
  fclose(txfp); 
}

/*******************************************************************************
Purpose		: Create data buf for transaction detail summary
Arguments	: 
		 - data buffer
		 - size of buffer
Return Value	: 
		 - size of buffer
Bug#4512: The design of this method is being changed such that this function 
          will be called multile times for retrieving data if it exceeds the
          size of the buffer.

          the Second argument is both input and output type.
          The second argument (size) will have the size of the buffer being 
          passed. This function will put only as many entries as this buffer
          can hold. The method will fill the number of bytes written

          A new argument is added (int *num_tx_done). This has two purposes:

          - The caller will use this to pass the index of the last tx 
            whose data has already been retrieved during previous call to 
            this function. The caller will set this to 0 during the first time.
            Next time it will pass the index of the first tx record to be
            retrieved.

          - This function will fill the number of total transactions done so far,
            i.e. the index of the next transaction to be retrieved.

********************************************************************************/

#define ADD_NETCACHE_HIT_PCT \
  ((global_settings->protocol_enabled & NETWORK_CACHE_STATS_ENABLED) && !(TX_END_NETCACHE_ENABLED & global_settings->protocol_enabled))

//static int create_tx_detail_data_buf(char *buf, int *size)
//NC: In release 3.9.3, TxData struct pointer will be passed to function for controller/standalone and generators
static int create_tx_detail_data_buf(char *buf, int *size, int *num_tx_done, TxDataCum *savedTxData)
{
  NSDL2_TRANS(NULL, NULL, "Method called.");
  int copied, left, tot_copied;
  char *sbuf_ptr = buf;
  char *sbuf;
  double min_time, max_time, avg_time, std_dev;
  static double c_min_time, c_max_time, c_avg_time, c_tot_time , c_std_dev, tx_c_tot_sqr_time, failure_pct,  netcache_pct, tx_tps;
  // Taking 8 bytes as it also used in cumulative case
  static u_ns_8B_t num_completed, num_initiated, num_succ, num_samples, num_netcache_fetches;
  static unsigned int loc_test_duration;  //For total test duration time   int i, is_periodic = 0;
  char *tbuf, tbuffer[1024];
  static u_ns_8B_t tx_c_fetches_completed , tx_c_num_initiated , tx_c_num_succ , tx_c_num_samples, tx_c_num_netcache_hits;
  int i;
  char *tx_name;
  if(*num_tx_done == 0) //Bug#4512: Initialize static variables only for first time
  {
    //following variables are used in cumm data for transaction
    tx_c_num_netcache_hits = tx_c_fetches_completed = tx_c_num_initiated = tx_c_num_succ = tx_c_num_samples = tx_c_tot_sqr_time = 0;
    c_min_time = 0xFFFFFFFF; 
    c_max_time = c_avg_time = c_tot_time = 0;
    loc_test_duration = get_ms_stamp() - global_settings->test_start_time;
  }

  left = *size;
  tot_copied = 0;

  if(*num_tx_done == 0)
  {
    if(global_settings->show_initiated){
      if(ADD_NETCACHE_HIT_PCT)
        copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Min (sec)|Avg (sec)|Max (sec)|Std Dev|Initiated|Completed|Success|Failure (%%) | Completed (Per sec) |NetCache Hits (%%)\n");
      else  
        copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Min (sec)|Avg (sec)|Max (sec)|Std Dev|Initiated|Completed|Success|Failure (%%) | Completed (Per sec)\n");
    } else {
      if(ADD_NETCACHE_HIT_PCT)  
        copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Min (sec)|Avg (sec)|Max (sec)|Std Dev|Attempted|Success|Failure (%%)|Completed (Per sec) |NetCache Hits (%%)\n");
      else
        copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Min (sec)|Avg (sec)|Max (sec)|Std Dev|Attempted|Success|Failure (%%)|Completed (Per sec)\n");
    }
    left -= copied;
    sbuf = sbuf_ptr + copied;
  }
  else 
    sbuf = sbuf_ptr;

  for (i = *num_tx_done; i < total_tx_entries; i++ )
  {
    tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, i);

    if (left < 1024)
    {
      /* Margin of 1024 bytes is kept so that the footer record (ALL|...) can
       * be accomodtaed
       */
      //printf ("Breaking tx info\n");
      break;
    }
    
    NSDL2_TRANS(NULL, NULL, "i = %d, savedTxData = [%p], tx_name = %s",
                             i, savedTxData, tx_name);
    /*if (is_periodic && savedTxData)
    {
      num_completed = savedTxData[i].tx_fetches_completed;
      num_succ = savedTxData[i].tx_succ_fetches;
      num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = savedTxData[i].tx_fetches_started;
      num_netcache_fetches = savedTxData[i].tx_netcache_fetches;

      if (num_samples)
      {
        min_time = (double)(((double)(savedTxData[i].tx_min_time))/1000.0);
        max_time = (double)(((double)(savedTxData[i].tx_max_time))/1000.0);
        avg_time = (double)(((double)(savedTxData[i].tx_tot_time))/((double)(1000.0*(double)num_samples)));
        std_dev = get_std_dev (savedTxData[i].tx_tot_sqr_time, savedTxData[i].tx_tot_time, avg_time, num_samples);
        c_tot_time += savedTxData[i].tx_tot_time;
        tx_c_tot_sqr_time += savedTxData[i].tx_tot_sqr_time;
        NSDL4_TRANS(NULL, NULL, "TxIdx = %d, name = %s, num_samples = %llu, "
                              "tx_min_time = %u ms (min_time = %.3f sec), "
                              "tx_max_time = %u ms (max_time = %.3f sec), "
                              "tx_tot_time = %llu ms (avg_time = %.3f sec), "
                              "tx_tot_sqr_time = %llu ms sqr (std_dev = %.3f), "
                              "c_tot_time = %.3f, tx_c_tot_sqr_time = %.3f, "
                              "tx_fetches_completed = %llu, "
                              "tx_fetches_started = %d",
                              "tx_netcache_fetches = %llu",  
                              i, tx_table_shr_mem[i].name, num_samples,
                              savedTxData[i].tx_min_time, min_time,
                              savedTxData[i].tx_max_time, max_time,
                              savedTxData[i].tx_tot_time, avg_time,
                              savedTxData[i].tx_tot_sqr_time, std_dev,
                              c_tot_time, tx_c_tot_sqr_time,
                              savedTxData[i].tx_fetches_completed,
                              savedTxData[i].tx_fetches_started,
                              savedTxData[i].tx_netcache_fetches);
      } else {
        NSDL4_TRANS(NULL, NULL, "TxIdx = %d, Name = %s, num_samples = %llu", i, tx_table_shr_mem[i].name, num_samples);
      }
    }*/
    if (savedTxData)
    {
      num_completed = savedTxData[i].tx_c_fetches_completed;
      num_succ = savedTxData[i].tx_c_succ_fetches;
      num_samples = num_completed;
      //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = savedTxData[i].tx_c_fetches_started;
      num_netcache_fetches = savedTxData[i].tx_c_netcache_fetches;

      if (num_samples)
      {
        min_time = (double)(((double)(savedTxData[i].tx_c_min_time))/1000.0);
        max_time = (double)(((double)(savedTxData[i].tx_c_max_time))/1000.0);
        avg_time = (double)(((double)(savedTxData[i].tx_c_tot_time))/((double)(1000.0*(double)num_samples)));
        std_dev = get_std_dev (savedTxData[i].tx_c_tot_sqr_time, savedTxData[i].tx_c_tot_time, avg_time, num_samples);
        c_tot_time += savedTxData[i].tx_c_tot_time;
        tx_c_tot_sqr_time += savedTxData[i].tx_c_tot_sqr_time;

        NSDL4_TRANS(NULL, NULL, "TxIdx = %d, Name = %s, num_samples = %llu, tx_c_min_time = %u ms (min_time = %.3f sec), "
                              "tx_c_max_time = %u ms (max_time = %.3f sec), "
                              "tx_c_tot_time = %llu ms (avg_time = %.3f sec), "
                              "tx_c_tot_sqr_time = %llu ms sqr (std_dev = %.3f), "
                              "c_tot_time = %.3f, tx_c_tot_sqr_time = %.3f, "
                              "tx_c_fetches_completed = %llu, "
                              "tx_c_fetches_started = %llu,"
                              "num_netcache_fetches = %llu",
                              i, tx_name, num_samples, savedTxData[i].tx_c_min_time, min_time,
                              savedTxData[i].tx_c_max_time, max_time,
                              savedTxData[i].tx_c_tot_time, avg_time,
                              savedTxData[i].tx_c_tot_sqr_time, std_dev,
                              c_tot_time, tx_c_tot_sqr_time,
                              savedTxData[i].tx_c_fetches_completed,
                              savedTxData[i].tx_c_fetches_started,
                              savedTxData[i].tx_c_netcache_fetches);
      } else {
        NSDL4_TRANS(NULL, NULL, "TxIdx = %d, Name = %s, num_samples = %llu", i, tx_name, num_samples);
      }

    } else {
      num_completed = num_initiated = num_succ = num_samples = num_netcache_fetches = 0;
    }
    //calculating cumm data as we dont have cumm data at this insatnt of time
    if (num_samples)
    {
      //set only if sample is there
      CALC_CUM_TX_RPT
      sprintf(tbuffer, "%6.3f|%6.3f|%6.3f|%6.3f", min_time, avg_time, max_time, std_dev);
    }
    else
    {
      sprintf(tbuffer, "-|-|-|-");
      NSDL4_TRANS(NULL, NULL, "num_samples are 0, num_completed = %llu, "
                              "show_initiated = %hd, num_initiated = %llu",
                              num_completed, global_settings->show_initiated,
                              num_initiated);
    }
    tbuf = tbuffer;

    if ((num_completed || ((global_settings->show_initiated) && num_initiated)))
    {
      tx_tps = (double)num_samples/((double)loc_test_duration/1000);
      if(num_completed) { 
        failure_pct = (double)((num_completed - num_succ)*100)/(double)num_completed;
      } else  {
       failure_pct = 0;
      }

      if(ADD_NETCACHE_HIT_PCT) 
        netcache_pct = (double) ((num_netcache_fetches)*100)/(double)num_completed;

      NSDL4_TRANS(NULL, NULL, "TxIdx = %d, name = %s, num_initiated = %llu, num_completed = %llu, "
                               "loc_test_duration = %u ms, num_succ = %llu ,"
                               " tx_tps = %.3f, failure_pct = %.3f, netcache_pct = %.3f",
                               i, tx_name, num_initiated, num_completed,
                               loc_test_duration, num_succ,
                               tx_tps, failure_pct, netcache_pct);

      if (global_settings->show_initiated) {

        if(ADD_NETCACHE_HIT_PCT)
          copied = sprintf(sbuf, "%s|%s|%llu|%llu|%llu|%6.3f|%6.3f|%6.3f\n", 
                               tx_name, tbuf,
                               num_initiated, num_completed,
                               num_succ, failure_pct, tx_tps, netcache_pct); 
        else
          copied = sprintf(sbuf, "%s|%s|%llu|%llu|%llu|%6.3f|%6.3f\n", 
                               tx_name, tbuf,
                               num_initiated, num_completed,
                               num_succ, failure_pct, tx_tps); 
      } else {

        if(ADD_NETCACHE_HIT_PCT)
          copied = sprintf(sbuf, "%s|%s|%llu|%llu|%6.3f|%6.3f|%6.3f\n",
                                tx_name, tbuf,
                                num_completed, num_succ,
                                failure_pct, tx_tps, netcache_pct);
        else
          copied = sprintf(sbuf, "%s|%s|%llu|%llu|%6.3f|%6.3f\n",
                                tx_name, tbuf,
                                num_completed, num_succ,
                                failure_pct, tx_tps);
      }
    } else {
      copied = 0;
    }
    NSDL4_TRANS(NULL, NULL, "XXX sbuf = [%s]", sbuf);
    left -= copied;
    sbuf += copied;
    tot_copied += copied;
    NSDL4_TRANS(NULL, NULL, "XXX buf = [%s]", buf);
  }

  *num_tx_done = i; /* Caller should call with this value in third argument next time */

  tbuffer[0]='\0';
  
/*  if(global_settings->exclude_failed_agg)
    tx_tps = (double)tx_c_num_succ/((double)loc_test_duration/1000);
  else */
    tx_tps = (double)tx_c_fetches_completed/((double)loc_test_duration/1000);

  failure_pct = (double)((tx_c_fetches_completed - tx_c_num_succ)*100)/(double)tx_c_fetches_completed;

  if(ADD_NETCACHE_HIT_PCT)
    netcache_pct = (double) ((tx_c_num_netcache_hits)*100)/(double)tx_c_fetches_completed;
 
  //following is the buffer for cumm data at the end of each sample
  if(total_tx_entries && (*num_tx_done >= total_tx_entries) && (tx_c_fetches_completed || ((global_settings->show_initiated) && tx_c_num_initiated)))
  {
    if (global_settings->show_initiated){
  
     if(ADD_NETCACHE_HIT_PCT)
       copied = sprintf(tbuffer, "ALL|%6.3f|%6.3f|%6.3f|%6.3f|%llu|%llu|%llu|%6.3f|%6.3f|%6.3f\n", c_min_time, c_avg_time, c_max_time, c_std_dev, tx_c_num_initiated, tx_c_fetches_completed, tx_c_num_succ, failure_pct, tx_tps, netcache_pct);
      else 
copied = sprintf(tbuffer, "ALL|%6.3f|%6.3f|%6.3f|%6.3f|%llu|%llu|%llu|%6.3f|%6.3f\n", c_min_time, c_avg_time, c_max_time, c_std_dev, tx_c_num_initiated, tx_c_fetches_completed, tx_c_num_succ, failure_pct, tx_tps);
    } else {
      if(ADD_NETCACHE_HIT_PCT) 
        copied = sprintf(tbuffer, "ALL|%6.3f|%6.3f|%6.3f|%6.3f|%llu|%llu|%6.3f|%6.3f|%6.3f\n", c_min_time, c_avg_time, c_max_time, c_std_dev, tx_c_fetches_completed, tx_c_num_succ, failure_pct, tx_tps, netcache_pct);
      else
        copied = sprintf(tbuffer, "ALL|%6.3f|%6.3f|%6.3f|%6.3f|%llu|%llu|%6.3f|%6.3f\n", c_min_time, c_avg_time, c_max_time, c_std_dev, tx_c_fetches_completed, tx_c_num_succ, failure_pct, tx_tps);
    }

    
    strcat(sbuf, tbuffer);
    tot_copied += copied;
  }
  sbuf_ptr[tot_copied++] = '\0';
  *size = tot_copied;


  return tot_copied;
}


/*
Purpose		: This method is to create data buf for not running transactions detail summary
Arguments	:
                 - data buffer
                 - size of buffer
Return Value	: 
		 - size of buffer 
Bug#4512: Similar changes as in create_tx_detail_data_buf(). See header of this method.
*/
static int create_tx_not_running_data_buf(char *buf, int *size, int *num_tx_done, TxDataCum *savedTxData) 
{
  NSDL2_TRANS(NULL, NULL, "Method called");
  int copied, left, tot_copied;
  char *sbuf_ptr = buf;
  char *sbuf;
  char *tx_name;
  static u_ns_8B_t num_completed, num_initiated, num_succ, num_samples;
  int i;

  if(*num_tx_done == 0)
    num_completed = num_initiated = num_succ = num_samples = 0;

  left = *size;
  tot_copied = 0;

  /* put header */
  // In case of not running transaction currently we will not check show initiated count 

  if(*num_tx_done == 0)
  {
    copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Initiated Count\n");

#if 0
  if (global_settings->show_initiated)
    copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name|Initiated Count\n");
  else
    copied = tot_copied = sprintf(sbuf_ptr, "Transaction Name\n");
#endif

    left -= copied;
    sbuf = sbuf_ptr + copied;
  }
  else
    sbuf = sbuf_ptr;

 for (i = *num_tx_done; i < total_tx_entries; i++ )
  {
    tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, i);
    if (left < 1024)
    {
      //printf ("Breaking tx info\n");
      break;
    }
    /*if (is_periodic && savedTxData)
    {
      num_completed = savedTxData[i].tx_fetches_completed;
      num_succ = savedTxData[i].tx_succ_fetches;
      num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = savedTxData[i].tx_fetches_started;
      NSDL2_TRANS(NULL, NULL, "savedTxData[%d].tx_fetches_completed = %llu,  savedTxData[%d].tx_fetches_started = %llu",i ,savedTxData[i].tx_fetches_completed, i, savedTxData[i].tx_fetches_started);
    }
    else if (savedTxData)*/
    {
      num_completed = savedTxData[i].tx_c_fetches_completed;
      num_succ = savedTxData[i].tx_c_succ_fetches;
      num_samples = num_completed;
      //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = savedTxData[i].tx_c_fetches_started;
      NSDL2_TRANS(NULL, NULL, "savedTxData[%d].tx_c_fetches_completed = %llu,  savedTxData[%d].tx_c_fetches_started = %llu",i ,savedTxData[i].tx_c_fetches_completed, i, savedTxData[i].tx_c_fetches_started);
    }

    //if (!num_completed || ((global_settings->show_initiated) && (num_succ < num_completed)) ) 
    if (!num_completed || ((global_settings->show_initiated) && (num_completed < num_initiated) ) ) 
    {
      //if (global_settings->show_initiated)
	copied = sprintf(sbuf,"%s|%llu\n", tx_name, num_initiated);
      //else
	//copied = sprintf(sbuf,"%s\n", tx_table_shr_mem[i].name);
    }
    else
      copied = 0;
    left -= copied;
    sbuf += copied;
    tot_copied += copied;
  }
  *num_tx_done = i;

  sbuf_ptr[tot_copied++] = '\0';
  *size = tot_copied;

  return tot_copied;
}

/*
Purpose     : This method is to create trans_detail.dat and trans_not_run.dat
              data file from saved data at the end of every progress report sample
Args        : None
Return Value: None 
In release 3.9.3:
For NetCloud: Added two arguments
gen_idx	: Generator index, in case of controller/standalone value would be -1
          for generators value woulb be greater than -1
          The gen_idx is used to distinguish path for creating transaction files  
TxData *savedTxData: TxData pointer will be passed with respect to process         	
*/
void create_trans_data_file(int gen_idx, TxDataCum *savedTxData)
{
  NSDL2_TRANS(NULL, NULL, "Method called.");
  char sbuffer[MAX_TRANS_BUF_LEN];             // Buffer for running transactions
  //char sbuffer_not_run[MAX_TRANS_BUF_LEN];     // Buffer for not running transactions
  int tot_copied = MAX_TRANS_BUF_LEN;	// Total size of buffer
  int num_done = 0;
  char *filemode = "w";

  if(total_tx_entries > 0)
  {
    filemode = "w";
    while(1)
    {
      sbuffer[0] = '\0';
      create_tx_detail_data_buf(sbuffer, &tot_copied, &num_done, savedTxData);
      // call method for trans_detail.dat
      create_trans_detail_data_file(sbuffer, filemode, gen_idx);
      filemode = "a";
      if(num_done >= total_tx_entries)
        break;

      tot_copied = MAX_TRANS_BUF_LEN;
    }

    filemode = "w";
    num_done = 0;
    tot_copied = MAX_TRANS_BUF_LEN;
    while(1)
    {
      sbuffer[0] = '\0';
      create_tx_not_running_data_buf(sbuffer, &tot_copied, &num_done, savedTxData);
      //call method for trans_not_run.dat
      create_trans_not_run_data_file(sbuffer, filemode, gen_idx);
      filemode = "a";
      if(num_done >= total_tx_entries)
        break;
      tot_copied = MAX_TRANS_BUF_LEN;
    }
  }
}

/* Function is used to copy transaction data in avgtime structure into gsavedTxData for either
 * controller/standalone or for particular generator
 * This function is called from print_all_tx_report() for controller or standalone
 * And from copy_progress_data() or copy_end_data() for generators
 * */
inline void copy_data_into_tx_buf(cavgtime *cavg, TxDataCum *gsavedTxData)
{
  NSDL2_TRANS(NULL, NULL, "Method called");
  txCData = (TxDataCum*)((char *)cavg + g_trans_cavgtime_idx);
  bcopy ((char *)txCData, (char *)gsavedTxData, total_tx_entries * sizeof(TxDataCum));
}

void
print_all_tx_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg, char *heading)
{
  double min_time, max_time, avg_time, std_dev;
  //double succ_per_sec, failure_pct;
  u_ns_8B_t num_completed, num_initiated, num_succ, num_samples;
  int i;
  char *tbuf, tbuffer[1024]; //, *my_buf;
  char *tx_name;
 
  NSDL2_TRANS(NULL, NULL, "Method called, is_periodic = %d, heading = %s, total_tx_entries = %d", is_periodic, heading, total_tx_entries);
  //NC: In release 3.9.3 , in case of controller or standalone we will call this function with gsavedTxData for index 0
  //KJ DO we need to handle this
  copy_data_into_tx_buf(cavg, gsavedTxData[0]);

  if (total_tx_entries)
  {
    if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) 
      fprint2f(fp1, fp2, "%s: \n", heading);
  }  
      
  for (i =0; i < total_tx_entries; i++ )
  {
    tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, i);
    /*if (is_periodic)
    {
      num_completed = avg->txData[i].tx_fetches_completed;
      num_succ = avg->txData[i].tx_succ_fetches;
      num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = avg->txData[i].tx_fetches_started;
      if (num_samplestx_name)
      {
        min_time = (double)(((double)(avg->txData[i].tx_min_time))/1000.0);
        max_time = (double)(((double)(avg->txData[i].tx_max_time))/1000.0);
        avg_time = (double)(((double)(avg->txData[i].tx_tot_time))/((double)(1000.0*(double)num_samples)));
        std_dev = get_std_dev (avg->txData[i].tx_tot_sqr_time, avg->txData[i].tx_tot_time, avg_time, num_samples);
      }
    }
    else*/
    {
      num_completed = txCData[i].tx_c_fetches_completed;
      num_succ = txCData[i].tx_c_succ_fetches;
      num_samples = num_completed;
      //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
      num_initiated = txCData[i].tx_c_fetches_started;
      if (num_samples)
      {
        min_time = (double)(((double)(txCData[i].tx_c_min_time))/1000.0);
        max_time = (double)(((double)(txCData[i].tx_c_max_time))/1000.0);
        avg_time = (double)(((double)(txCData[i].tx_c_tot_time))/((double)(1000.0*(double)num_samples)));
        std_dev = get_std_dev (txCData[i].tx_c_tot_sqr_time, txCData[i].tx_c_tot_time, avg_time, num_samples);
      }
    }
    NSDL2_TRANS(NULL, NULL, "tx_name = %s, num_completed = %llu, num_succ = %llu, num_samples = %llu, num_initiated = %llu, "
                            "global_settings->show_initiated = %d, min %6.3f sec, avg %6.3f sec, max %6.3f sec, stddev %6.3f sec",
                             tx_name, num_completed, num_succ, num_samples, num_initiated, global_settings->show_initiated,
                             min_time, avg_time, max_time, std_dev);
    if (num_samples)
    {
      sprintf(tbuffer, "min %6.3f sec, avg %6.3f sec, max %6.3f sec, stddev %6.3f sec", min_time, avg_time, max_time, std_dev);
      tbuf = tbuffer;
    }
    else
    {
      tbuf = no_succ_time_string_tx;
    }
    if ((num_completed || ((global_settings->show_initiated) && num_initiated)))
    {
      //succ_per_sec = (double)num_succ/((double)loc_test_duration/1000);
      //failure_pct = (double)((num_completed - num_succ)*100)/(double)num_completed;
      if (global_settings->show_initiated)
      {
        if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) 
          fprint2f(fp1, fp2," %24s:  %s  TOT: initiated %llu/completed %llu/succ %llu", tx_name, tbuf, num_initiated, num_completed, num_succ);
      }
      else
      {
        if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) 
          fprint2f(fp1, fp2, " %24s:  %s  TOT: completed %llu/succ %llu", tx_name, tbuf, num_completed, num_succ);
      }
      if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) 
        fprint2f(fp1, fp2, "\n");

    }
  }
 
  //here we calculate the cum trans data
  /*if (is_periodic)
  {
    num_completed = avg->tx_fetches_completed;
    num_succ = avg->tx_succ_fetches;
    num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
    num_initiated = avg->tx_fetches_started;
    if (num_samples)
    {
      min_time = (double)(((double)(avg->tx_min_time))/1000.0);
      max_time = (double)(((double)(avg->tx_max_time))/1000.0);
      avg_time = (double)(((double)(avg->tx_tot_time))/((double)(1000.0*(double)num_samples)));
      std_dev = get_std_dev (avg->tx_tot_sqr_time, avg->tx_tot_time, avg_time, num_samples);
    }
  } 
  else*/
  {
    num_completed = cavg->tx_c_fetches_completed;
    num_succ = cavg->tx_c_succ_fetches;
    num_samples = num_completed;
    //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
    num_initiated = cavg->tx_c_fetches_started;

    
    if (num_samples)
    {
      min_time = (double)(((double)(cavg->tx_c_min_time))/1000.0);
      max_time = (double)(((double)(cavg->tx_c_max_time))/1000.0);
      avg_time = (double)(((double)(cavg->tx_c_tot_time))/((double)(1000.0*(double)num_samples)));
      std_dev = get_std_dev (cavg->tx_c_tot_sqr_time, cavg->tx_c_tot_time, avg_time, num_samples);
    }
  }  
  //succ_per_sec = (double)num_succ/((double)loc_test_duration/1000);
  //failure_pct = (double)((num_completed - num_succ)*100)/(double)num_completed;

}

inline void send_tx_data(Msg_com_con *mccptr, int opcode)
{
  NSDL2_TRANS(NULL, NULL, "Method called.");

  char sbuffer[MAX_TRANS_BUF_LEN + 4]; /* 4 for size of int */
  int tot_copied = MAX_TRANS_BUF_LEN;
  int num_done = 0; /* Number of transactions whose data is sent to parent */

  /* Calling the method (that creates the buffer) repetitively until all the
   * transaction data is retrieved. Multiple messages are sent to parent
   * for sending the details in case the transaction details data can not
   * be contained in buffer of size MAX_TRANS_BUF_LEN.
   */
  while(num_done < total_tx_entries)
  {
    switch (opcode)
    {
      case GET_ALL_TX_DATA:
        create_tx_detail_data_buf(sbuffer + sizeof(int), &tot_copied, &num_done, gsavedTxData[0]); //For controller or standalone gsavedTxData is send with index 0
        break;
      case GET_MISSING_TX_DATA:
        create_tx_not_running_data_buf(sbuffer + sizeof(int), &tot_copied, &num_done, gsavedTxData[0]);//For controller or standalone gsavedTxData is send with index 0
        break;
      default:
        /* should never come here */
        return;
    }
        
    memcpy(sbuffer, &tot_copied, sizeof(int)); /* First four bytes contain the size */
    write_msg(mccptr, sbuffer, tot_copied + sizeof(int), 0, DATA_MODE);
    tot_copied = MAX_TRANS_BUF_LEN;
  }
  /* Once all the messages with tx data are sent, send a message with size 0
   * so that the parent understands that the data is over. The code for parent
   * reading these messages in ns_tx_summary.c.
   */
  *((int *)sbuffer) = 0;
  write_msg(mccptr, sbuffer, sizeof(int), 0, DATA_MODE);
}
