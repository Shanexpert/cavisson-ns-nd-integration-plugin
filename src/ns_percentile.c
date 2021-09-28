/***************************************************************************************
 Name        : ns_percentile.c
 Purpose     : This file contain all functions related to Percentile feature.
               Design Doc CVS Path -
                1 - docs/Products/NetStorm/TechDocs/NetStormCore/Design/PercentileCoreDesign.doc
                2 - docs/Products/NetStorm/TechDocs/NetStormCore/Design/NSDynamicTxPercentileDesign.docx
               Requirement Doc CVS Path -
                1 - docs/Products/NetStorm/TechDocs/Reporting/Req/PercentileReportsReq.doc
 Mod. Hist.  : 4 July 2020
***************************************************************************************/

/*
HLD:
  NVM maintains 4 bytes counters in two shared memory - active and other
  Parent maintains 8 bytes counters in two allocated memory - current and next
  In case of NetCloud, client sends message with MSG_HDR, PDF Hdr and 4 bytes counters
*/

#define _GNU_SOURCE 
#define _FILE_OFFSET_BITS 64  /* enable large file writing support  */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <errno.h>

#include "nslb_big_buf.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "timing.h"
#include "util.h"
#include "tmr.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "divide_users.h"
#include "divide_values.h"

#include "netstorm.h"
#include "ns_ftp.h"
#include "ns_summary_rpt.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_pdf_parse.h"
#include "ns_schedule_phases_parse.h"
#include "ns_string.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_schedule_divide_over_nvm.h"
#include "ns_msg_com_util.h"
#include "ns_trace_level.h"
#include "ns_child_msg_com.h"
#include "ns_static_vars_rtc.h"
#include "ns_trans_normalization.h"
#include "ns_trans.h"
#include "ns_exit.h"
#include "wait_forever.h"
#include "ns_data_handler_thread.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_schedule_ramp_down_fcu.h"

extern int sgrp_used_genrator_entries;
extern int g_generator_idx ;
extern Msg_com_con *g_master_msg_com_con;
extern int master_fd;
extern FILE *resp_hits_fp;
extern void resp_create_hit_percentile_rpt();
int num_process_for_nxt_sample;
static int pct_msg_file_fd = -1;
int total_pdf_data_size = PDF_DATA_HEADER_SIZE; //This is for child shared memory size
long long int total_pdf_data_size_8B = PDF_DATA_HEADER_SIZE; //This is for parent memory size
int old_total_pdf_data_size_8B = 0;
int old_total_pdf_data_size = 0;
#define DELTA_TX_PDF_ENTRIES 16
int child_total_pdf_data_size_except_hdr_and_tx;
char *pdf_mem;
pdf_shm_info_t *pdf_shm_info;
pdf_shm_info_t *pdf_shm_info_temp;
int g_percentile_report = 1;    /* 0 - Disabled; 1 - Enabled */
int g_percentile_mode = PERCENTILE_MODE_INTERVAL;
int g_percentile_interval = 300000;
int g_percentile_sample = 1;
static int g_percentile_sample_dump_to_file = 0;  // this will go to the testrun.pdf
int g_percentile_sample_missed = 0;
static int g_last_sample_count = 0;
char *process_last_sample_array = NULL;
int save_pct_file_on_generator = 0;  //Changed from 1 to 0 in 4.3.0 as we do not need pct file in generator

void *parent_pdf_addr = NULL;
void *parent_pdf_next_addr = NULL;
static PercentileMsgHdr *pdf_msg_ptr = NULL;
static void *pdf_msg_addr;
int parent_cur_count = 0;
int parent_next_count = 0;

static int total_pdf_entries = 0;
static int max_pdf_entries = 0;
pdf_lookup_data_t *pdf_lookup_data = NULL;
int total_pdf_msg_size = 0; 
int old_total_pdf_msg_size = 0;

/* Offsets for std graphs */
int pdf_average_url_response_time = -1; /* Average URL Response Time  */
int pdf_average_smtp_response_time = -1; /* Average URL Response Time  */
int pdf_average_pop3_response_time = -1; /* Average URL Response Time  */
int pdf_average_ftp_response_time = -1; /* Average URL Response Time  */
int pdf_average_dns_response_time = -1; /* Average dns Response Time  */
int pdf_average_page_response_time = -1; /* Average Page Response Time */
int pdf_average_session_response_time = -1; /* Average Session Response Time */
int pdf_average_transaction_response_time = -1; /* Average Transaction Response Time */
int pdf_transaction_time = -1;                  /* Transaction Time */

int is_new_tx_add = 0;
int testrun_pdf_and_pctMessgae_version = 0;
u_ns_ts_t testrun_pdf_ts;

extern int loader_opcode;
char **parent_controller_pdf_mem;

inline void update_pdf_shm_info_addr();
inline void send_attach_pdf_shm_msg_to_parent();

/*This function will calculate remainder of percentile interval and progress report interval
  Here remainder is non zero then form interval as multiple of progress report interval */
static inline void validate_interval_time()
{
  int pct_interval;//Need for debug
  NSDL1_PERCENTILE(NULL, NULL, "Method called");

  int remainder = (g_percentile_interval / 1000) % (global_settings->progress_secs / 1000);

  NSDL2_PERCENTILE(NULL, NULL, "Calculate remainder for percentile interval(in sec) = %d and progress sample (in sec) = %d, remainder = %d", 
                g_percentile_interval / 1000, global_settings->progress_secs / 1000, remainder);

  if (remainder != 0)
  {
    pct_interval = g_percentile_interval / 1000;
    g_percentile_interval = (pct_interval + (global_settings->progress_secs / 1000) - remainder) * 1000; 
    fprintf(stderr, "Warning: Interval (%d) in percentile report keyword (PERCENTILE_REPORT) should be multiple of progress report interval (%d). Hence making percentile interval (%d) multiple of progress report interval.\n", (pct_interval * 1000), global_settings->progress_secs, g_percentile_interval);  
    NS_DUMP_WARNING("Interval (%d) in percentile report keyword (PERCENTILE_REPORT) should be multiple of progress report interval (%d). Hence making percentile interval (%d) multiple of progress report interval.", (pct_interval * 1000), global_settings->progress_secs, g_percentile_interval);  
  
  }
}

int kw_set_percentile_report(char *kw_line, int runtime_flag, char *err_msg) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char text1[MAX_DATA_LINE_LENGTH];
  char text2[MAX_DATA_LINE_LENGTH];
  char text3[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  
  num = sscanf(kw_line, "%s %s %s %s %s", keyword, text1, text2, text3, tmp);

  if (num < 1 || num > 4 ) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_1);
  }
  /*Check enable and disable option*/
  if(ns_is_numeric(text1) == 0){
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_2);
  }
  g_percentile_report = atoi(text1);

  if(g_percentile_report < 0 || g_percentile_report > 1){
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_3);
  }

  if (g_percentile_report == 1) { /* enabled */
    /* Mode*/
    if (num >= 3) 
    {
      if (ns_is_numeric(text2) == 0) {
        NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_2);
      }
      g_percentile_mode = atoi(text2);
 
      if (g_percentile_mode < 0 || g_percentile_mode > 2) {
        NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_3);
      }
    }
    /*In case of interval mode then validate timestamp*/
    if (g_percentile_mode == PERCENTILE_MODE_INTERVAL) {
      if (num == 4) 
      {
        if (ns_is_numeric(text3) == 0) {
          NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011096, CAV_ERR_MSG_2);
        }
        g_percentile_interval = atoi(text3);    
        /* percentile data interval should be a multiple of progress secs */
        if (g_percentile_interval < global_settings->progress_secs) {
          NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011097, "");
        }
        /*Function used to validate percentile interval time stamp*/
        validate_interval_time(); 
      }
    }
    else if(global_settings->continuous_monitoring_mode)//CM mode,Total time and run phase are not allowed so existing.
    {
      NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PERCENTILE_REPORT_USAGE, CAV_ERR_1011098, "");
    }
  }

  NSDL3_PERCENTILE(NULL, NULL, "g_percentile_report = %d, g_percentile_mode = %d, g_percentile_interval = %d", 
             g_percentile_report, g_percentile_mode, g_percentile_interval);

  return 0;
}

int
kw_set_url_pdf_file(char *kw_line, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(kw_line, "%s %s", keyword, text);
  if (num < 2) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, URL_PDF_USAGE, CAV_ERR_1011055, CAV_ERR_MSG_1);
  }
  strcpy(global_settings->url_pdf_file, text); 
  NSDL3_PERCENTILE(NULL, NULL, "url_pdf_file = %s\n",global_settings->url_pdf_file);
  return 0;
}

int
kw_set_page_pdf_file(char *kw_line, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(kw_line, "%s %s", keyword, text);
  if (num < 2) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, PAGE_PDF_USAGE, CAV_ERR_1011056, CAV_ERR_MSG_1);
  }
  strcpy(global_settings->page_pdf_file, text);
  NSDL3_PERCENTILE(NULL, NULL, "page_pdf_file = %s\n", global_settings->page_pdf_file);
  return 0;
}

int
kw_set_session_pdf_file(char *kw_line, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(kw_line, "%s %s", keyword, text);
  if (num < 2) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, SESSION_PDF_USAGE, CAV_ERR_1011057, CAV_ERR_MSG_1);
  }
  strcpy(global_settings->session_pdf_file, text);
  NSDL3_PERCENTILE(NULL, NULL, "session_pdf_file = %s\n", global_settings->session_pdf_file);
  return 0;
}

int
kw_set_trans_resp_pdf_file(char *kw_line, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(kw_line, "%s %s", keyword, text);
  if (num < 2) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, TRANSACTION_RESPONSE_PDF_USAGE, CAV_ERR_1011059, CAV_ERR_MSG_1);
  }
  strcpy(global_settings->trans_resp_pdf_file, text);
  NSDL3_PERCENTILE(NULL, NULL, "trans_resp_pdf_file = %s\n", global_settings->trans_resp_pdf_file);
  return 0;
}

int
kw_set_trans_time_pdf_file(char *kw_line, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(kw_line, "%s %s", keyword, text);
  if (num < 2) {
    NS_KW_PARSING_ERR(kw_line, runtime_flag, err_msg, TRANSACTION_TIME_PDF_USAGE, CAV_ERR_1011058, CAV_ERR_MSG_1);
  }
  strcpy(global_settings->trans_time_pdf_file, text);
  NSDL3_PERCENTILE(NULL, NULL, "trans_time_pdf_file = %s\n", global_settings->trans_time_pdf_file);
  return 0;
}

#if 0
void
granule_data_output(int mode, FILE *rfp, FILE* srfp, avgtime *avg, double *data)
{
  int b, c;
  int granule;
  int median;
  int median_granule = -1;
  int eighty_perc;
  int eighty_perc_gran = -1;
  int ninety_perc;
  int ninety_perc_gran = -1;
  int ninety_five_perc;
  int ninety_five_perc_gran = -1;
  int ninety_nine_perc;
  int ninety_nine_perc_gran = -1;
  unsigned int bucket_total[MAX_BUCKETS+1];
  unsigned int  running_total = 0;
  unsigned int running_buckets = 0;
  unsigned short g_num_buckets;
  unsigned short g_max_granules;
  unsigned short g_granule_size;
  unsigned short *g_buckets;
  char mode_buf[32];
  unsigned int *avg_response;
  u_ns_8B_t avg_succ_fetches;

  NSDL2_MISC(NULL, NULL, "Method called, mode = %d", mode);
  if (mode == NS_HIT_REPORT) {
    g_num_buckets = global_settings->num_buckets;
    g_max_granules = global_settings->max_granules;
    g_granule_size = global_settings->granule_size;
    g_buckets = &global_settings->buckets[0];
    avg_response = &(avg->response[0]);
    avg_succ_fetches = avg->url_succ_fetches;
    sprintf(mode_buf, "URL Hits");
  } else if (mode == NS_SMTP_REPORT) {
    g_num_buckets = global_settings->smtp_num_buckets;
    g_max_granules = global_settings->smtp_max_granules;
    g_granule_size = global_settings->smtp_granule_size;
    g_buckets = &global_settings->smtp_buckets[0];
    avg_response = &(avg->smtp_response[0]);
    avg_succ_fetches = avg->smtp_succ_fetches;
    sprintf(mode_buf, "SMTP Mails");
  } else if (mode == NS_POP3_REPORT) {
    g_num_buckets = global_settings->pop3_num_buckets;
    g_max_granules = global_settings->pop3_max_granules;
    g_granule_size = global_settings->pop3_granule_size;
    g_buckets = &global_settings->pop3_buckets[0];
    avg_response = &(avg->pop3_response[0]);
    avg_succ_fetches = avg->pop3_succ_fetches;
    sprintf(mode_buf, "POP3 Mails");
  } else if (mode == NS_FTP_REPORT) {
    // Nikita
    FTPAvgTime *ftp_avg = (FTPAvgTime *) ((char*) avg + g_ftp_avgtime_idx);
    g_num_buckets = global_settings->ftp_num_buckets;
    g_max_granules = global_settings->ftp_max_granules;
    g_granule_size = global_settings->ftp_granule_size;
    g_buckets = &global_settings->ftp_buckets[0];
    avg_response = &(ftp_avg->ftp_response[0]);
    avg_succ_fetches = ftp_avg->ftp_succ_fetches;
    sprintf(mode_buf, "FTP Fetches");
  } else if (mode == NS_DNS_REPORT) {
    g_num_buckets = global_settings->dns_num_buckets;
    g_max_granules = global_settings->dns_max_granules;
    g_granule_size = global_settings->dns_granule_size;
    g_buckets = &global_settings->dns_buckets[0];
    avg_response = &(avg->dns_response[0]);
    avg_succ_fetches = avg->dns_succ_fetches;
    sprintf(mode_buf, "DNS Fetches");
  } else if (mode == NS_PG_REPORT) {
    g_num_buckets = global_settings->pg_num_buckets;
    g_max_granules = global_settings->pg_max_granules;
    g_granule_size = global_settings->pg_granule_size;
    g_buckets = &global_settings->pg_buckets[0];
    avg_response = &avg->pg_response[0];
    avg_succ_fetches = global_settings->exclude_failed_agg?avg->pg_succ_fetches:avg->pg_fetches_completed;
    sprintf(mode_buf, "Page Hits");
  } else if (mode == NS_TX_REPORT) {
    g_num_buckets = global_settings->tx_num_buckets;
    g_max_granules = global_settings->tx_max_granules;
    g_granule_size = global_settings->tx_granule_size;
    g_buckets = &global_settings->tx_buckets[0];
    avg_response = &avg->tx_response[0];
    avg_succ_fetches = global_settings->exclude_failed_agg?avg->tx_c_succ_fetches:avg->tx_c_fetches_completed;
    sprintf(mode_buf, "Tx Hits");
  } else if (mode == NS_SESS_REPORT) {
    g_num_buckets = global_settings->sess_num_buckets;
    g_max_granules = global_settings->sess_max_granules;
    g_granule_size = global_settings->sess_granule_size;
    g_buckets = &global_settings->sess_buckets[0];
    avg_response = &avg->sess_response[0];
    avg_succ_fetches = global_settings->exclude_failed_agg?avg->sess_succ_fetches:avg->sess_fetches_completed;
    sprintf(mode_buf, "Sess Hits");
  } else {
    printf("Bad mode specified for granule_data_output\n");
    return;
  }
  /* print out report of granules */
#if 0
  if (g_num_buckets) {
    memset(&bucket_total, 0, sizeof(bucket_total));

    fprint3f(srfp, rfp, stdout, "\n%s Percentile Report\n\n", mode_buf);
    fprint3f(srfp, rfp, stdout, "  Response Time       Number of Responses       Pct of Responses   Pct of Responses upto\n");
    fprint3f(srfp, rfp, stdout, "  Window (RTW)        in  RTW                   in RTW             upper bound of RTW\n");
    fprint3f(srfp, rfp, stdout, "  ----------------    -------------------       ---------------    ---------------------\n");

    if ( srfp != NULL )
      add_obj_percentile_report_table_start_srfp_html(mode_buf);

    // This is executed once for creating the report file
    resp_create_hit_percentile_rpt();

    /* calculate granule that holds median value */
    median = avg_succ_fetches / 2;
    eighty_perc = (avg_succ_fetches * 80)/100;
    ninety_perc = (avg_succ_fetches * 90)/100;
    ninety_five_perc = (avg_succ_fetches * 95)/100;
    ninety_nine_perc = (avg_succ_fetches * 99)/100;
    for (b = 0; b < (g_max_granules+1) ; b++) {
      running_total += avg_response[b];
      if ((running_total > median) && (median_granule == -1))
	median_granule = b;
      if ((running_total > eighty_perc) && (eighty_perc_gran == -1))
	eighty_perc_gran = b;
      if ((running_total > ninety_perc) && (ninety_perc_gran == -1))
	ninety_perc_gran = b;
      if ((running_total > ninety_five_perc) && (ninety_five_perc_gran == -1))
	ninety_five_perc_gran = b;
      if ((running_total > ninety_nine_perc) && (ninety_nine_perc_gran == -1))
	ninety_nine_perc_gran = b;
    }

    granule = 0;
    running_total = 0;
    for (b = 0; b < g_num_buckets+1; b++) {

      if (b == g_num_buckets) {
	//printf("TST: granule=%d g_max_granules=%d b=%d bucket_total[b]=%d avg_response[granule]=%d runnung_buckets=%d g_granule_size=%d\n", granule, g_max_granules, b, bucket_total[b], avg_response[granule], running_buckets, g_granule_size);
	for (; granule < g_max_granules+1; granule++) {
	  bucket_total[b] += avg_response[granule];
	}
	//printf("TST1: granule=%d g_max_granules=%d b=%d bucket_total[b]=%d avg_response[granule]=%d runnung_buckets=%d g_granule_size=%d\n", granule, g_max_granules, b, bucket_total[b], avg_response[granule], running_buckets, g_granule_size);

	running_total += bucket_total[b];
	fprint3f(srfp, rfp, stdout, "%6.2f-higher:\t\t%'17lu\t\t%6.2f%%\t\t%6.2f%%\n",
		  (float)(running_buckets*g_granule_size)/(float)1000,
		  bucket_total[b], (float)(((float)bucket_total[b]*100.0)/avg_succ_fetches),
		  (float)(((float)running_total*100.0)/avg_succ_fetches));
        // This will write hit percentile higher response time data to report file with object mode
    	fprintf(resp_hits_fp, "%s|%0.2f - Higher|%u|%0.2f|%0.2f\n", mode_buf,
            (float)(running_buckets*g_granule_size)/(float)1000,
            bucket_total[b], (float)(((float)bucket_total[b]*100.0)/avg_succ_fetches),
      	    (float)(((float)running_total*100.0)/avg_succ_fetches));
        if ( srfp != NULL)

      } else {
	for (c = 0; c < g_buckets[b]; c++) {
	  bucket_total[b] += avg_response[granule++];
	}
	running_total += bucket_total[b];
	fprint3f(srfp, rfp, stdout, "%6.2f-%6.2f secs:\t\t%'9lu\t\t%6.2f%%\t\t%6.2f%%\n",
		  (float)(running_buckets*g_granule_size)/(float)1000,
		  (float)(((running_buckets+g_buckets[b])*g_granule_size) - 1)/(float)1000,
		  bucket_total[b], (float)(((float)bucket_total[b]*100.0)/avg_succ_fetches),
		  (float)(((float)running_total*100.0)/avg_succ_fetches));
 	// This will write hit percentile response time data to report file with object mode
    	fprintf(resp_hits_fp, "%s|%0.2f - %0.2f|%u|%0.2f|%0.2f\n",mode_buf,
          (float)(running_buckets*g_granule_size)/(float)1000,
          (float)(((running_buckets+g_buckets[b])*g_granule_size) - 1)/(float)1000,
          bucket_total[b], (float)(((float)bucket_total[b]*100.0)/avg_succ_fetches),
      	  (float)(((float)running_total*100.0)/avg_succ_fetches));
        if ( srfp != NULL)

	running_buckets += g_buckets[b];
      	}

    }

    data[0] = (double)((double)((median_granule+1)*g_granule_size)/1000.0);
    data[1] = (double)((double)((eighty_perc_gran+1)*g_granule_size)/1000.0);
    data[2] = (double)((double)((ninety_perc_gran+1)*g_granule_size)/1000.0);
    data[3] = (double)((double)((ninety_five_perc_gran+1)*g_granule_size)/1000.0);
    data[4] = (double)((double)((ninety_nine_perc_gran+1)*g_granule_size)/1000.0);

    fprint3f(srfp, rfp, stdout, "\n\nmedian-time: %6.3f sec, 80%%: %6.3f sec, 90%%: %6.3f sec, 95%%: %6.3f sec, 99%%: %6.3f sec\n\n",
		data[0], data[1], data[2], data[3], data[4]);
    // DL_ISSUE
#ifdef NS_DEBUG_ON
    {
    // if (global_settings->debug) {
      unsigned int granule_total = 0;
      for (b = 0; b< g_max_granules+1; b++) {
	NSDL3_REPORTING(NULL, NULL, "granule[%d]: %lu ", b, avg_response[b]);
	granule_total += avg_response[b];
      }
      NSDL3_REPORTING(NULL, NULL, "granule total: %lu", granule_total);
    //}
    }
#endif


  } else {
      fprint3f(srfp, rfp, stdout, "No granule reports specified in the configuration file for %s\n", mode_buf);
  }
#endif
}
#endif


void update_transaction_count(int total_tx)
{
  pdf_shm_info[my_port_index].total_tx = total_tx; 
}

int create_pdf_lookup_table(int *row_num, char *pdf_name, int min_granules, int max_granules, int num_granules, int pdf_data_offset, int pdf_data_offset_8B, int rpGroupID, int rpGraphID)
{
  NSDL2_PERCENTILE(NULL, NULL, "Method called.  pdf_name = %s, min_granules = %d, max_granules = %d, "
                               "num_granules = %d, pdf_data_offset = %d", 
                                pdf_name, min_granules, max_granules, num_granules, pdf_data_offset,
                                rpGroupID, rpGraphID);

  if (total_pdf_entries == max_pdf_entries) {
    MY_REALLOC_EX (pdf_lookup_data, 
                  (max_pdf_entries + DELTA_PDF_ENTRIES) * sizeof(pdf_lookup_data_t), 
                  (max_pdf_entries * sizeof(pdf_lookup_data_t)), "pdf_lookup_data_t", -1);

    memset(&pdf_lookup_data[total_pdf_entries], -1, (DELTA_PDF_ENTRIES * sizeof(pdf_lookup_data_t)));

   //removed code for error handling of MY_REALLOC_EX, as it handled internally 
    max_pdf_entries += DELTA_PDF_ENTRIES;//change page entries to pdf entries for old bug
  }

  *row_num = total_pdf_entries++;

  strcpy(pdf_lookup_data[*row_num].pdf_name, pdf_name);
  pdf_lookup_data[*row_num].min_granules = min_granules;
  pdf_lookup_data[*row_num].max_granules = max_granules;
  pdf_lookup_data[*row_num].num_granules = num_granules;
  pdf_lookup_data[*row_num].pdf_data_offset = pdf_data_offset;
  pdf_lookup_data[*row_num].pdf_data_offset_parent = pdf_data_offset_8B;
  pdf_lookup_data[*row_num].group_id = rpGroupID;
  pdf_lookup_data[*row_num].graph_id = rpGraphID;

  NSDL3_PERCENTILE(NULL, NULL, "PdfLookUpTable Dump: index = %d, min = %d max = %d, num = %d, pdf_lookup_data[%d].pdf_name = %s, "
                               "pdf_lookup_data[*row_num].pdf_data_offset = %d, "
                               "pdf_lookup_data[*row_num].pdf_data_offset_parent = %d",
                                *row_num, min_granules, max_granules, num_granules, *row_num,
                                pdf_lookup_data[*row_num].pdf_name, pdf_lookup_data[*row_num].pdf_data_offset,
                                pdf_lookup_data[*row_num].pdf_data_offset_parent);

  return (SUCCESS);
}

void set_pdf(int rpGroupID, int rpGraphID, int row_num)
{
  NSDL2_PERCENTILE(NULL, NULL, "Method called, rpGroupID = %d, rpGraphID = %d, row_num = %d", 
                                rpGroupID, rpGraphID, row_num);
  /* Setting pdf_* */
  if (rpGroupID == URL_HITS_RPT_GRP_ID && rpGraphID == 3) {
    pdf_average_url_response_time = row_num;
  } else if (rpGroupID == SMTP_HITS_RPT_GRP_ID && rpGraphID == 4) {
    pdf_average_smtp_response_time = row_num;
  } else if (rpGroupID == POP3_HITS_RPT_GRP_ID && rpGraphID == 4) {
    pdf_average_pop3_response_time = row_num;
  } else if (rpGroupID == FTP_HITS_RPT_GRP_ID && rpGraphID == 4) {
    pdf_average_ftp_response_time = row_num;
  } else if (rpGroupID == DNS_HITS_RPT_GRP_ID && rpGraphID == 4) {
    pdf_average_dns_response_time = row_num;
  } else if (rpGroupID == PG_DOWNLOAD_RPT_GRP_ID && rpGraphID == 3) {
    pdf_average_page_response_time = row_num;
  } else if (rpGroupID == SESSION_RPT_GRP_ID && rpGraphID == 3) {
    pdf_average_session_response_time = row_num;
  } else if (rpGroupID == TRANSDATA_RPT_GRP_ID && rpGraphID == 3) {
    pdf_average_transaction_response_time = row_num;
  } else if (rpGroupID == TRANS_STATS_RPT_GRP_ID && rpGraphID == 1) {
    pdf_transaction_time = row_num;
  }
}

int chk_and_add_in_pdf_lookup_table(char *graph_description, int *pdf_id, int *row_num, int rpGroupID, int rpGraphID, int runtime_flag)
{
  char pdf_name[MAX_FILE_NAME];
  int min_granules = 0;
  int max_granules = 0;
  int num_granules = 0;
  int i;

  NSDL2_PERCENTILE(NULL, NULL, "Method called, graph_description = %s, pdf_id = %d, "
                               "row_num = %d, rpGroupID = %d, rpGraphID = %d, g_runtime_flag = %d",
                                graph_description, *pdf_id, *row_num, rpGroupID, rpGraphID, runtime_flag);

  if((g_percentile_report != 1) || (*pdf_id == -1))
  {
    NSDL2_PERCENTILE(NULL, NULL, "Either percentile report is desibled or pdf_id is not set, hence returning");
    return num_granules;
  }

  NSDL2_PERCENTILE(NULL, NULL, "Check PDF is already is in pdf look up table or not, total_pdf_entries = %d");

  if((num_granules = process_pdf(pdf_id, &min_granules, &max_granules, pdf_name)) == -1)
  {
    if(runtime_flag == 0)
    {
      NS_EXIT(-1, "Unable to parse PDF id %d", *pdf_id);
    }
    else
    {
      NSTL1_OUT(NULL, NULL, "Unable to parse PDF id %d. Exiting.\n", *pdf_id);
      return 0;
    }
  }

  for(i = 0; i < total_pdf_entries; i++)
  {
    if((pdf_lookup_data[i].group_id == rpGroupID) && (pdf_lookup_data[i].graph_id == rpGraphID))
    {
      NSDL2_PERCENTILE(NULL, NULL, "PDF for group id %d and graph id %d is already registered into pdf_lookup_table hence returning..",
                                    rpGroupID, rpGraphID);
      return num_granules;
    }
  }

  NSDL3_GDF(NULL, NULL, "pdf_id = %d, Graph_Description = %s, num_granules = %d, "
                        "min_granules = %d, max_granules = %d",
                         *pdf_id, graph_description, num_granules, min_granules, max_granules);

  create_pdf_lookup_table(row_num, pdf_name,  min_granules, max_granules, num_granules,
                            total_pdf_data_size, total_pdf_data_size_8B/*  +  */
                           /*                               ((numVector ? numVector : 1) * (num_granules * sizeof(Pdf_Data))) */,
                           rpGroupID, rpGraphID);

  /* Setting pdf_* */
  set_pdf(rpGroupID, rpGraphID, *row_num);

  return num_granules;
}


void copy_to_testrun_pdf(FILE *tspdf_fd, int pdf_idx)
{
  FILE *copy_from;
  char buff[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  
  char file_name[MAX_FILE_NAME];
  char fname[MAX_FILE_NAME];
  
  strcpy(file_name, pdf_lookup_data[pdf_idx].pdf_name);
  NSDL3_PERCENTILE(NULL, NULL, "File_name is = %s pdf_name = %s pdf_id = %d", file_name, pdf_lookup_data[pdf_idx].pdf_name, pdf_idx);
  sprintf(fname, "%s/pdf/%s", g_ns_wdir, file_name);
  copy_from = fopen(fname, "r");

  if (copy_from == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, buff, errno, nslb_strerror(errno));
  }

  while(nslb_fgets(buff, MAX_LINE_LENGTH, copy_from, 0)) {
    if (buff[0] == '#' || buff[0] == '\0') /* Blank line */
      continue;
    if(sscanf(buff, "%s", tmpbuff) == -1)  // for blank line with some spaces.
      continue;
    if(!(strncmp(buff, "info|", strlen("info|"))) || !(strncmp(buff, "Info|", strlen("Info|"))))
      continue;
    /* Anything else we just copy */
    fwrite(buff, strlen(buff), 1, tspdf_fd);
  }  
  
  fclose(copy_from);
}

void create_testrun_pdf()
{
  NSTL1(NULL, NULL, "[Percentile]: Method called, total_pdf_entries = %d", total_pdf_entries);

  if (!total_pdf_entries)
    return;

  FILE *testrun_pdf_fd;
  char line[MAX_LINE_LENGTH];
  char file_name[MAX_LINE_LENGTH];
  int i;

  // TODO - Once GUI make percentile work for patition, then we will change this code to open file in partitin dir
  sprintf(file_name, "%s/logs/%s/testrun.pdf", g_ns_wdir, global_settings->tr_or_partition);
  // sprintf(file_name, "%s/logs/%s/testrun.pdf", g_ns_wdir, global_settings->tr_or_partition);
  testrun_pdf_fd = fopen(file_name, "w");

  if (testrun_pdf_fd == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }

  /**
   * Insert Info Line :
   * Info|pctMsgVersion|numPDF|sizeOfPctMsgData|Mode|Interval
   */
  // We are calculating the size of pdf with 4 bytes but when we dump into file then we dump using 8 bytes. Therefore the size will be double of calculated size. hdr comes only once in each okt so hdr size should not be doubled
  sprintf(line, "Info|1.0|%d|%lu|%d|%d\n\n", 
                         total_pdf_entries, 
                         (((total_pdf_data_size - sizeof(pdf_data_hdr))*2) + sizeof(pdf_data_hdr)), 
                         g_percentile_mode, 
                         ((g_percentile_mode == PERCENTILE_MODE_INTERVAL) ? g_percentile_interval : -1));
  
  fwrite(line, strlen(line), 1, testrun_pdf_fd);

  for (i = 0; i < total_pdf_entries; i++) {
    copy_to_testrun_pdf(testrun_pdf_fd, i);
    fwrite("\n", strlen("\n"), 1, testrun_pdf_fd);
  }

  fclose(testrun_pdf_fd);

  /* Release the minions by freeing stuff. */
}

//Clear child memory
static void  clear_pdf_shrmem(char *addr)
{
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, addr = %p", addr);

  memset(addr + sizeof(pdf_data_hdr), 0, total_pdf_data_size - sizeof(pdf_data_hdr));
}

//Clear parent memory
static void  clear_parent_pdf_shrmem(char *addr)
{
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, addr = %p", addr);

  memset(addr + sizeof(pdf_data_hdr), 0, total_pdf_data_size_8B  - sizeof(pdf_data_hdr));
}

//Clear parent memory
//TODO: Need to move the memory if other featuer's memory is assigined
static void  clear_parent_realloced_pdf_mem(char *addr)
{
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, addr = %p", addr);

  memset(addr + old_total_pdf_data_size_8B, 0, (total_pdf_data_size_8B - old_total_pdf_data_size_8B));
}

void init_pdf_child_shared_memory(pdf_shm_info_t *local_pdf_shm_info, int child_id, int *shmid, int runtime)
{
  void *pdf_shm_info_mem;
  pdf_data_hdr *pdf_hdr;

  NSDL1_PERCENTILE(NULL, NULL, "Method called local_pdf_shm_info = %p, child = %d, total_pdf_data_size = %d, runtime = %d", 
                                local_pdf_shm_info, child_id, total_pdf_data_size, runtime);
  /*******************************************************************
  Bug: 
  If Parent make shared memory, it should set delete flag
  If Child make shared memory, it should not set delete flag. 
  Child will tell parent to set delete flag
  *******************************************************************/
  if (g_percentile_mode == PERCENTILE_MODE_ALL_PHASES)
  {
    pdf_shm_info_mem = do_shmget_with_id_ex((total_pdf_data_size * 3), "Pdf shm memory for child", shmid, !runtime); 
    NSDL4_PERCENTILE(NULL, NULL, "pdf_shm_info_mem = %p, size = %d", pdf_shm_info_mem, (total_pdf_data_size * 3));
  }
  else 
  {
    pdf_shm_info_mem = do_shmget_with_id_ex((total_pdf_data_size * 2), "Pdf shm memory for child", shmid, !runtime); 
    NSDL4_PERCENTILE(NULL, NULL, "pdf_shm_info_mem = %p, size = %d", pdf_shm_info_mem, (total_pdf_data_size * 2));
  }

  NSDL1_PERCENTILE(NULL, NULL, "pdf_shm_info_mem = %p, shmid = %d", pdf_shm_info_mem, *shmid);
  get_shm_info(*shmid);

  local_pdf_shm_info[child_id].addr[0] = pdf_shm_info_mem; 
  pdf_shm_info_mem += total_pdf_data_size;
  local_pdf_shm_info[child_id].addr[1] = pdf_shm_info_mem; 
  //Save the size
  local_pdf_shm_info[child_id].total_pdf_data_size_for_this_child = total_pdf_data_size;

  if (g_percentile_mode == PERCENTILE_MODE_ALL_PHASES)
  {
    pdf_shm_info_mem += total_pdf_data_size;
    local_pdf_shm_info[child_id].addr[2] = pdf_shm_info_mem; 
  }

  if(!runtime)
  {
    local_pdf_shm_info[child_id].active_addr_idx = 0; /* First switch */

    pdf_hdr = (pdf_data_hdr *)local_pdf_shm_info[child_id].addr[local_pdf_shm_info[child_id].active_addr_idx];
    pdf_hdr->sequence = 1; /* starting with 1 */
    pdf_hdr->active = 1;
  }
  else
  {
    local_pdf_shm_info[child_id].active_addr_idx = pdf_shm_info[child_id].active_addr_idx; 
    pdf_hdr = (pdf_data_hdr *)local_pdf_shm_info[child_id].addr[local_pdf_shm_info[child_id].active_addr_idx];
    pdf_hdr->sequence = ((pdf_data_hdr *)pdf_shm_info[child_id].addr[pdf_shm_info[child_id].active_addr_idx])->sequence;
    pdf_hdr->active = ((pdf_data_hdr *)pdf_shm_info[child_id].addr[pdf_shm_info[child_id].active_addr_idx])->active;
  }

  NSDL2_PERCENTILE(NULL, NULL, "Filling total_tx_entries into pct packet header "
                               "child_id = %d, active_addr_idx = %d, ",
                                child_id, pdf_shm_info[child_id].active_addr_idx);

  ((pdf_data_hdr *)local_pdf_shm_info[child_id].addr[0])->total_tx_entries = total_tx_entries;
  ((pdf_data_hdr *)local_pdf_shm_info[child_id].addr[1])->total_tx_entries = total_tx_entries;

  clear_pdf_shrmem(local_pdf_shm_info[child_id].addr[0]);
  clear_pdf_shrmem(local_pdf_shm_info[child_id].addr[1]);

  if (g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    clear_pdf_shrmem(local_pdf_shm_info[child_id].addr[2]);
    
    pdf_hdr = (pdf_data_hdr *)local_pdf_shm_info[child_id].addr[1];
    pdf_hdr->sequence = !runtime?2:((pdf_data_hdr *)pdf_shm_info[child_id].addr[pdf_shm_info[child_id].active_addr_idx])->sequence;

    pdf_hdr = (pdf_data_hdr *)local_pdf_shm_info[child_id].addr[2];
    pdf_hdr->sequence = !runtime?3:((pdf_data_hdr *)pdf_shm_info[child_id].addr[pdf_shm_info[child_id].active_addr_idx])->sequence;
  }

  NSDL3_PERCENTILE(NULL, NULL, " shm_get for child %d, addresses = %p, %p", child_id, 
                                 local_pdf_shm_info[child_id].addr[0], local_pdf_shm_info[child_id].addr[1]);
}

//For both Parent and child these vairiables will work
int total_tx_pdf_entries = 0;
int max_tx_pdf_entries = 0;

void check_if_need_to_resize_child_pdf_memory()
{
  int shm_id;
  old_total_pdf_data_size = total_pdf_data_size;

  NSDL4_PERCENTILE(NULL, NULL, "Method called, old_total_pdf_data_size = %d, max_tx_pdf_entries = %d, total_tx_pdf_entries = %d", 
                                old_total_pdf_data_size, max_tx_pdf_entries, total_tx_pdf_entries);

  if(max_tx_pdf_entries == total_tx_pdf_entries)
  {
    NSTL1(NULL, NULL, "[Percentile]: Reallocating child PDF data memory, total_pdf_data_size = %d, num_granules = %d", 
                       total_pdf_data_size, pdf_lookup_data[pdf_transaction_time].num_granules);
    total_pdf_data_size = old_total_pdf_data_size + 
                          (DELTA_TX_PDF_ENTRIES * pdf_lookup_data[pdf_transaction_time].num_granules  * sizeof(Pdf_Data));
    init_pdf_child_shared_memory(pdf_shm_info_temp, my_port_index, &shm_id, 1);
    max_tx_pdf_entries += DELTA_TX_PDF_ENTRIES;
   
    /********************************************
      Update addr of pdf_shm_info with 
      new shm of pdf data
     *******************************************/
    update_pdf_shm_info_addr();
    send_attach_pdf_shm_msg_to_parent(shm_id);
  }
  else
  {
    NSDL2_PERCENTILE(NULL, NULL, "Updating total_tx_entries in pct packet header my_port_index = %d, pdf_shm_info = %p, total_tx_entries = %d",
                                  my_port_index, pdf_shm_info, total_tx_entries);

    ((pdf_data_hdr *)pdf_shm_info[my_port_index].addr[0])->total_tx_entries = total_tx_entries;
    ((pdf_data_hdr *)pdf_shm_info[my_port_index].addr[1])->total_tx_entries = total_tx_entries;
  }

  total_tx_pdf_entries++;

  is_new_tx_add = 1; 
}

/*Function to create memory for parent*/
void realloc_memory_for_parent ()
{
  int i;
  NSDL1_PERCENTILE(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
  //Controller/Parent will two memory 
  //If running as controller then it will have memory for generators also

   //No need to realloc memory for generators
  //MY_REALLOC(parent_controller_pdf_mem, sizeof(char *) * (sgrp_used_genrator_entries + 1), "parent_controller_pdf_mem", -1);
  MY_REALLOC(parent_controller_pdf_mem[0], (total_pdf_data_size_8B * 2), "parent_pdf_addr", -1);

  //TODO: How to know which generator has added the transaction. We are increasing all generators size
  for(i = 1; i <= sgrp_used_genrator_entries; i++)
  {
    MY_REALLOC(parent_controller_pdf_mem[i], (total_pdf_data_size_8B), "parent_pdf_addr", -1);
    clear_parent_realloced_pdf_mem(parent_controller_pdf_mem[i]);
  }

  parent_pdf_addr = parent_controller_pdf_mem[0];

  /* Master mem for parent */
  parent_pdf_next_addr = (char*)parent_pdf_addr + total_pdf_data_size_8B; 
  clear_parent_realloced_pdf_mem(parent_pdf_addr);
  clear_parent_realloced_pdf_mem(parent_pdf_next_addr);
  NSDL3_PERCENTILE(NULL, NULL, "shm_get for Parent, addresses = %p, next = %p", parent_pdf_addr, parent_pdf_next_addr);
}

void check_if_need_to_resize_parent_pdf_memory()
{
  old_total_pdf_data_size_8B = total_pdf_data_size_8B;
  old_total_pdf_data_size = total_pdf_data_size;
  old_total_pdf_msg_size = total_pdf_msg_size;

  if(max_tx_pdf_entries == total_tx_pdf_entries)
  {
    NSTL1(NULL, NULL, "[Percentile]: Reallocating parent PDF data memory, old_total_pdf_data_size = %d, old_total_pdf_data_size_8B = %d, "
                      "num_granules = %d, loader_opcode = %d",
                       old_total_pdf_data_size, old_total_pdf_data_size_8B,
                       pdf_lookup_data[pdf_transaction_time].num_granules, loader_opcode);

    total_pdf_data_size_8B =  old_total_pdf_data_size_8B +
                              ((DELTA_TX_PDF_ENTRIES ) *  pdf_lookup_data[pdf_transaction_time].num_granules  * sizeof(Pdf_Data_8B));
    realloc_memory_for_parent();

    if(loader_opcode == CLIENT_LOADER)
    {
      total_pdf_data_size = old_total_pdf_data_size +
                          (DELTA_TX_PDF_ENTRIES * pdf_lookup_data[pdf_transaction_time].num_granules  * sizeof(Pdf_Data));

      total_pdf_msg_size = total_pdf_data_size + sizeof(PercentileMsgHdr);

      MY_REALLOC_AND_MEMSET(pdf_msg_ptr, total_pdf_msg_size, old_total_pdf_msg_size, "Re-allocat pdf_msg_ptr", -1);
      pdf_msg_ptr->msg_len = total_pdf_msg_size - sizeof(int);
      pdf_msg_addr = (char*)pdf_msg_ptr + sizeof(PercentileMsgHdr);

      NSTL1(NULL, NULL, "[Percentile]: Reallocated pdf_msg_ptr succefully, total_pdf_data_size = %d, total_pdf_msg_size = %d, pdf_msg_ptr->msg_len = %d, "
                        "pdf_msg_addr = %p, pdf_msg_ptr = %p",
                         total_pdf_data_size, total_pdf_msg_size, pdf_msg_ptr->msg_len, pdf_msg_addr, pdf_msg_ptr);
    }
    max_tx_pdf_entries += DELTA_TX_PDF_ENTRIES;
  }

  total_tx_pdf_entries++;

  is_new_tx_add = 1;
}

/*Function to create memory for parent*/
void create_memory_for_parent ()
{
  int i;
  NSDL1_PERCENTILE(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
  //Controller/Parent will two memory 
  //If running as controller then it will have memory for generators also

  MY_MALLOC(parent_controller_pdf_mem, sizeof(char *) * (sgrp_used_genrator_entries + 1), "parent_controller_pdf_mem", -1);
  MY_MALLOC(parent_controller_pdf_mem[0], (total_pdf_data_size_8B * 2), "parent_pdf_addr", -1);

  for(i = 1; i <= sgrp_used_genrator_entries; i++)
  {
    MY_MALLOC(parent_controller_pdf_mem[i], (total_pdf_data_size_8B), "parent_pdf_addr", -1);
    clear_parent_pdf_shrmem(parent_controller_pdf_mem[i]);
  }

  parent_pdf_addr = parent_controller_pdf_mem[0];
  //Moved from init_pdf_shared_mem() as this will be called in both NS and NC
  /*Initially this should hold value of total number of NVMs*/
  num_process_for_nxt_sample = global_settings->num_process; 

  /* Master mem for parent */
  parent_pdf_next_addr = (char*)parent_pdf_addr + total_pdf_data_size_8B; 
  clear_parent_pdf_shrmem(parent_pdf_addr);
  clear_parent_pdf_shrmem(parent_pdf_next_addr);

  child_total_pdf_data_size_except_hdr_and_tx = pdf_lookup_data[pdf_transaction_time].pdf_data_offset - sizeof(pdf_data_hdr);

  NSDL3_PERCENTILE(NULL, NULL, "shm_get for Parent, addresses = %p, next = %p, child_total_pdf_data_size_except_hdr_and_tx = %d", 
                                parent_pdf_addr, parent_pdf_next_addr, child_total_pdf_data_size_except_hdr_and_tx);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name 		: send_pct_report_msg_nvm2p()
  Purpose	: Send message PERCENTILE_REPORT to Parent. This message just inform
                  Parent to switch and flush percentile data from shared memory. 
                  This message does not send any percentile data on socket. 
  Inputs	: finish_report - provide information that this is last message or not
                                  i.e. continuity of message
                                  0 for continuos
                                  1 for last
                  shm_idx       - provide shared memory index from which parent has to read
                                   
  Date		: 6 July 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void send_pct_report_msg_nvm2p(int finish_report, int shm_idx, char *caller)
{
  parent_child msg;
  int size = sizeof(parent_child);

  NSDL2_MESSAGES(NULL, NULL, "Method called, Sending Percentile Done message of size (%d) from NVM:%d -> Parent", size, my_port_index);

  memset(&msg, 0, size);

  msg.msg_len = size - sizeof(int);   // Actual message length, Starting 4Bytes contains message length 
  msg.opcode = PERCENTILE_REPORT;     // Message Opcode
  msg.child_id = my_port_index;       // NVM index
  msg.testidx = total_tx_entries;     // Use testidx to store total number of transactions
  msg.gen_rtc_idx = finish_report;    // Last report
  msg.abs_ts = (time(NULL)) * 1000;   // BirthTime
  msg.shm_id = shm_idx;               // Percentile shmidx 

  NSTL1(NULL, NULL, "[Percentile]: Sending message PERCENTILE_REPORT of size (%d Bytes), "
    "from NVM:%d -> Parent (OnDataConnection), MsgLen = %d, MsgBirthTime = %f, "
    "MsgContinutity = %d, TotTrans = %d, PCTShmInx = %d, %s", 
    size, my_port_index, msg.msg_len, msg.abs_ts, msg.gen_rtc_idx, msg.testidx, msg.shm_id, caller); 

  write_msg(&g_dh_child_msg_com_con, (char *) &msg, size, 0, DATA_MODE);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name 		: pct_time_mode_chk_send_ready_msg()
  Purpose	: Switch and send percetile report from NVM to Parent 
                  Used for mode PERCENTILE_MODE_INTERVAL and PERCENTILE_MODE_TOTAL_RUN. 
                  It is called whenever any phase message sent from NVM2Parent,
                  so need to check for required phase  
  Inputs	: finish   
                    1 - Last Progress Report else 0
  Date		: 6 July 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void pct_time_mode_chk_send_ready_msg(int finish)
{
  NSDL1_PERCENTILE(NULL, NULL, "Method called, finish = %d, g_percentile_mode = %d,"
    " global_settings->progress_secs = %d, v_cur_progress_num = %d, g_percentile_interval = %d,"
    " g_percentile_report = %d, total_pdf_data_size = %d", 
    finish, g_percentile_mode, global_settings->progress_secs, v_cur_progress_num, 
    g_percentile_interval, g_percentile_report, total_pdf_data_size);

  /*============================================================================
    Switch and Send shared memory iff -
    [A] For mode PERCENTILE_MODE_TOTAL_RUN
         1. Percentile Time Interval elapsed OR last sample
            We are checking this condition first as this will be true less time so that
            we can optimize the checking
         2. OR Last Progress Report 
    [B] For mode PERCENTILE_MODE_TOTAL_RUN 
         1. Last Progress Report 
    [C] Check for both
         2. Percentile feature is enabled
         4. There is percentile data to send   
  ============================================================================*/
  if((((g_percentile_mode == PERCENTILE_MODE_INTERVAL) && 
     (!((global_settings->progress_secs * (v_cur_progress_num)) % g_percentile_interval) || finish)) || 
     ((g_percentile_mode == PERCENTILE_MODE_TOTAL_RUN) && finish)) &&
    g_percentile_report && (total_pdf_data_size > PDF_DATA_HEADER_SIZE)) 
  {
    //switch and send percentile
    int shm_idx = switch_pdf_shm(-1, finish? "TIME_OR_TOTAL_MODE_FINISH": "TIME_OR_TOTAL_MODE");
    send_pct_report_msg_nvm2p(finish, shm_idx, finish? "TIME_OR_TOTAL_MODE_FINISH": "TIME_OR_TOTAL_MODE");
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name 		: pct_run_phase_mode_chk_send_ready_msg()
  Purpose	: Switch and send percetile report from NVM to Parent 
                  Used for mode PERCENTILE_MODE_ALL_PHASES only. 
                  It is called whenever any phase message sent from NVM2Parent,
                  so need to check for required phase  
  Inputs	: phase_idx   - provide phase index
  Date		: 6 July 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void pct_run_phase_mode_chk_send_ready_msg(int phase_idx, int finish)
{
  Schedule *schedule = scenario_schedule;

  /*============================================================================
    Switch and Send shared memory only iff -
      1. Percentile feature is enabled
      2. Percentile mode is run phase mode (PERCENTILE_MODE_ALL_PHASES)
      3. There is percentile data to send   
      4. All Phases over OR Last sample
    Note: Here is_all_phase_over() is using to take of PERCENTILE_MODE_TOTAL_RUN
          and finish is using to take of FORCE_PHASE_COMPLETE
  ============================================================================*/
  if((g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) && g_percentile_report &&
     (total_pdf_data_size > PDF_DATA_HEADER_SIZE))
  {
    //TODO:is_all_phase_over - can this be done by parent
    //if((apo = is_all_phase_over()) || finish)
    //finish can happen from index 0 or 1 or 2..How to handle??
    if(finish)
    {
      /*=======================================================================
        For percentile we need to switch buffers here since ctrl-C was pressed
        hence, pahse completion msg will be by-passed
        mode 1: switch to shm 2
      =======================================================================*/
      //switch_pdf_shm(2, (apo)? "ALL_PHASE_OVER": "FORCE_PHASE_COMPLETE");
      //send_pct_report_msg_nvm2p(finish, 2, (apo)? "ALL_PHASE_OVER": "FORCE_PHASE_COMPLETE");

      switch_pdf_shm(2, "FINISH");  //TODO: 
      send_pct_report_msg_nvm2p(finish, 2, "FINISH");
    }
    else if(schedule->phase_array[phase_idx].phase_type == SCHEDULE_PHASE_STABILIZE)
    {
      //switch and send percentile
      switch_pdf_shm(1, "SCHEDULE_PHASE_STABILIZE_COMPLETE");
      send_pct_report_msg_nvm2p(finish, 0, "SCHEDULE_PHASE_STABILIZE_COMPLETE");
    }
    else if(schedule->phase_array[phase_idx].phase_type == SCHEDULE_PHASE_DURATION)
    {
      //switch and send percentile
      switch_pdf_shm(2, "SCHEDULE_PHASE_DURATION_COMPLETE");
      send_pct_report_msg_nvm2p(finish, 1, "SCHEDULE_PHASE_DURATION_COMPLETE");
    }
  }
}

/*Function to create memory for children and parent*/
void init_pdf_shared_mem()
{
  int i;
  int shm_id;
  //pdf_data_hdr *pdf_hdr;
  //void *pdf_shm_info_mem;

  NSDL2_PERCENTILE(NULL, NULL, "Method Called, loader_opcode = %d, num_process = %d, total_pdf_msg_size = %d, g_generator_idx = %d", 
                                loader_opcode, global_settings->num_process, total_pdf_msg_size, g_generator_idx);

  /*Initially this should hold value of total number of NVMs*/
  //moved to create_memory_for_parent ()
  //num_process_for_nxt_sample = global_settings->num_process; 

  //In case of generator we will be having one more buffer
  //This is use to send data to controller
  total_pdf_msg_size = total_pdf_data_size + sizeof(PercentileMsgHdr);
  if(loader_opcode == CLIENT_LOADER)
  {
    MY_MALLOC(pdf_msg_ptr, total_pdf_msg_size, "pdf_msg_ptr", -1);
    pdf_msg_ptr->opcode = PERCENTILE_REPORT;
    pdf_msg_ptr->child_id = g_generator_idx;
    pdf_msg_ptr->msg_len = total_pdf_msg_size - sizeof(int);
    pdf_msg_addr = (char*)pdf_msg_ptr + sizeof(PercentileMsgHdr);
    clear_pdf_shrmem(pdf_msg_addr);
  }
   
  MY_MALLOC(pdf_shm_info, (sizeof(pdf_shm_info_t) * global_settings->num_process), "pdf_shm_info", -1);
  MY_MALLOC(pdf_shm_info_temp, (sizeof(pdf_shm_info_t) * global_settings->num_process), "pdf_shm_info_temp", -1);
  
  for (i = 0; i < global_settings->num_process; i++) {
    NSDL3_PERCENTILE(NULL, NULL, "shm_get for child %d", i);
    init_pdf_child_shared_memory(pdf_shm_info, i, &shm_id, 0);
  }
  
  create_memory_for_parent();
  child_total_pdf_data_size_except_hdr_and_tx = pdf_lookup_data[pdf_transaction_time].pdf_data_offset - sizeof(pdf_data_hdr);
  //int parent_total_pdf_data_size_except_hdr_and_tx = pdf_lookup_data[pdf_transaction_time].pdf_data_offset_parent - sizeof(pdf_data_hdr);
}


#ifdef NS_DEBUG_ON

static void dump_pdf_for_one_object(int pct_messages_sample_fd, pdf_data_hdr *pdf_hdr, struct tm *cur_tm, char *pkt_start_addr, int shm_idx, char *object, int i)
{
  char buff[1024*1024];
  int j;
  Pdf_Data_8B *bucketCountPtr;
  Pdf_Data *bucketCountPtr4B;
  int amt_written = sprintf(buff, "%llu,%d/%d/%d %d:%d:%d,%llu,%d,%s,%d,%d,", 
        pdf_hdr->abs_timestamp, cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec, pdf_hdr->sequence, shm_idx, object, pdf_lookup_data[i].num_granules, pdf_lookup_data[i].min_granules);
  
  if(my_port_index == 255) 
    bucketCountPtr = (Pdf_Data_8B *)(pkt_start_addr + pdf_lookup_data[i].pdf_data_offset_parent);
  else
    bucketCountPtr4B = (Pdf_Data *)(pkt_start_addr + pdf_lookup_data[i].pdf_data_offset);

  for(j = 0; j < pdf_lookup_data[i].num_granules; j++)
  {
    if(my_port_index == 255) 
    {
      amt_written += sprintf(buff + amt_written, "%lld,", *bucketCountPtr);
      bucketCountPtr++;
    }
    else
    {
      amt_written += sprintf(buff + amt_written, "%d,", *bucketCountPtr4B);
      bucketCountPtr4B++;
    }
  }
  buff[amt_written - 1] = '\n'; // Replace last comma by new line
  buff[amt_written] = 0;  // Null terminate

  write(pct_messages_sample_fd, buff, amt_written);
}

static void dump_pdf_shm(char *pkt_start_addr, int shm_idx)
{
  char file[MAX_LINE_LENGTH];
  int pct_messages_sample_fd;
  int i;
  struct tm *cur_tm, tm_struct;

  pdf_data_hdr *pdf_hdr = (pdf_data_hdr *)pkt_start_addr;
  time_t timestamp = pdf_hdr->abs_timestamp / 1000;
  cur_tm = nslb_localtime(&(timestamp), &tm_struct, 1);
  
  NSDL2_PERCENTILE(NULL, NULL, "Method called, pkt_start_addr = %p", pkt_start_addr);
  sprintf(file, "%s/logs/%s/pct_messages_samples_%d.csv", g_ns_wdir, global_settings->tr_or_partition, my_port_index);
  pct_messages_sample_fd = open(file, O_CREAT|O_WRONLY|O_CLOEXEC | O_APPEND, 0666);
  if (pct_messages_sample_fd < 0) {
    fprintf(stderr, "Unable to open file %s. Ignored. Error %s\n", file, nslb_strerror(errno));
    return;
  }

  NSDL2_PERCENTILE(NULL, NULL, "total_pdf_entries = %d", total_pdf_entries);
  for (i = 0; i < total_pdf_entries; i++) {
    if(pdf_lookup_data[i].pdf_data_offset <= 0)
      continue;

    // Format of record:
    // TS(ms), TS(date/time), Seq, shmIdx, Object, numBuckets, bucketSize(ms), count1, count2, …..
    // Where Object is  Url, Page, Tx, Session, TxName1, TxName2, ...
    // Tx may not be present is scripts do not have any tx
    char object[128] = "\0";

    if(i == pdf_average_url_response_time) strcpy(object, "url");
    else if(i == pdf_average_smtp_response_time) strcpy(object, "smtp");
    else if(i == pdf_average_pop3_response_time) strcpy(object, "pop3");
    else if(i == pdf_average_ftp_response_time) strcpy(object, "ftp");
    else if(i == pdf_average_dns_response_time) strcpy(object, "dns");
    else if(i == pdf_average_page_response_time) strcpy(object, "page");
    else if(i == pdf_average_session_response_time) strcpy(object, "session");
    else if(i == pdf_average_transaction_response_time) strcpy(object, "transaction");
    else if(i == pdf_transaction_time)
    {
       int tx_id;
       NSDL2_PERCENTILE(NULL, NULL, "total_tx_entries = %d", total_tx_entries);
       for(tx_id = 0; tx_id < total_tx_entries; tx_id++ ){ 
         strcpy(object, nslb_get_norm_table_data(&normRuntimeTXTable, tx_id));
         dump_pdf_for_one_object(pct_messages_sample_fd, pdf_hdr, cur_tm, pkt_start_addr, shm_idx, object, i);
       }
       continue;
    }
    if(object[0] != '\0') // Skip if i is not matching with any pdf row id
      dump_pdf_for_one_object(pct_messages_sample_fd, pdf_hdr, cur_tm, pkt_start_addr, shm_idx, object, i);
  }
  close(pct_messages_sample_fd);
  
}
#endif

void close_pct_msg_dat_fd()
{
  if (pct_msg_file_fd > 0) {
    NSDL3_PERCENTILE(NULL, NULL, "closing pct_msg_file_fd");
    close(pct_msg_file_fd);

    if (g_percentile_mode == PERCENTILE_MODE_INTERVAL) {
      pdf_append_end_line();
    }
  }
}

void open_pct_message_file()
{
  char file[MAX_LINE_LENGTH];

  // TODO - Once GUI make percentile work for patition, then we will change this code to open file in partitin dir
  //sprintf(file, "%s/logs/TR%d/pctMessage.dat", g_ns_wdir, tr_or_partition);
   sprintf(file, "%s/logs/%s/pctMessage.dat", g_ns_wdir, global_settings->tr_or_partition);
  pct_msg_file_fd = open(file, O_CREAT|O_WRONLY |O_LARGEFILE| O_APPEND | O_CLOEXEC, 0666);
  if (pct_msg_file_fd < 0) {
    NS_EXIT(-1, CAV_ERR_1000006, file, errno, nslb_strerror(errno));
  }
  
}
//earlier pctMessage.dat was stored in partition dir but now moving to TR
//Hence no need to call this function
void pct_switch_partition(char *cur_pdf_file, char *prev_pdf_file){
  
  char err_msg[MAX_LINE_LENGTH] = "";

  NSDL2_PERCENTILE(NULL, NULL, "Method called , prev_pdf_file = %s, cur_pdf_file = %s", prev_pdf_file, cur_pdf_file);

  // Close previous parition percentile message file
  if(pct_msg_file_fd > 0)
    close(pct_msg_file_fd);  
  else
   NSDL2_PERCENTILE(NULL, NULL, "File %s is not open", prev_pdf_file);

  //copy data from previous to current file. Must be done before appending of END line
  if(nslb_copy_file(cur_pdf_file, prev_pdf_file, err_msg) < 0)
    NSDL2_PERCENTILE(NULL, NULL, "Error in copying testrun.pdf from '%s' to '%s'. Error = %s",  prev_pdf_file, cur_pdf_file, err_msg);

  if (g_percentile_mode == PERCENTILE_MODE_INTERVAL) {
     pdf_append_end_line_ex(prev_pdf_file);
  }
  
  NSDL2_PERCENTILE(NULL, NULL, "Error from nslb_copy_file - [%s] ", err_msg);

  // Open pct message file for new partition
  open_pct_message_file();
}  

/* Function computes percentile for given samples and fills double array provided by caller, 
   1. Double array should be of size 101 (to hold percentile 1 to 100) 
   2. Calculate total number of samples 
   3. Find compare count for each percentile and for linear interpolation find integer and fractional part 
   4. Calculate percentile using cumilative count and fill percentile array
   NOTE: 
   num_buckets is one less than total buckets as we have one extra bucket for all samples whose value is greater than 
   maximum granule
   For example: it will be 10000 and shm will have 10001 buckets
   bucket_size is minimum granule size
*/
static void get_percentiles(double *out_pct, unsigned long long *arr_sample_counts, int num_buckets, int bucket_size)
{
  unsigned long long total_samples = 0;
  int i;

  NSDL1_REPORTING(NULL, NULL, "Method called, num_buckets = %d, bucket_size = %d", 
                 num_buckets, bucket_size);

  /*Find total number of samples and find first non zero index*/
  for(i = 0; i < num_buckets; i++)
  {
    total_samples += arr_sample_counts[i]; //Calculate total number of samples
  }

  NSDL2_REPORTING(NULL, NULL, "Total number of samples = %llu", total_samples);

  int n = 1; /* This is the percentile number, 1 for 1st, 2 for 2nd and so on */

  /* cmp_count is the compare count. The count of arr_sample_counts starting from index 0 will be added to a 
   * cumulative counter (cum_count) and as soon as the cum_count exceeds the compare count for
   * particular n'th percentile, the index of the arr_sample_counts represents the scaled value of n'th percentile
   */
  double cmp_count = ((((double) total_samples - 1) * (double) n) / (double) 100) + 1;
  int cmp_count_int = (int) cmp_count; 
  double cmp_count_fraction = cmp_count - (double) cmp_count_int; 

  NSDL3_REPORTING(NULL, NULL, " Compare count cmp_count = %f, integer part cmp_count_int = %d" 
                  " and fraction cmp_count_fraction = %f", cmp_count, cmp_count_int, cmp_count_fraction);
  /* Compute all the percentiles */
  int cum_count = 0;

  for(i = 0; i <= num_buckets; i++)
  {
    cum_count += arr_sample_counts[i];
    while(cum_count >= cmp_count_int && n <= 100)
    {
      NSDL4_REPORTING(NULL, NULL, "Calculate cumulative counter: cum_count = %d", cum_count);
      //out_pct[n] = (((double) i * bucket_size) + bucket_size / 2);
      out_pct[n] = ((double) (i + 1) * bucket_size); 
      NSDL4_REPORTING(NULL, NULL, "For percentile %d and sample index = %d" 
                  " computed percentile value is out_pct[%d] = %f", n, i, n, out_pct[n]);

      int cur_idx = i + 1;
      NSDL4_REPORTING(NULL, NULL, "Next sample index cur_idx = %d", cur_idx);
      if(cum_count == cmp_count_int && cmp_count_fraction > 0)
      {
        while(!arr_sample_counts[cur_idx] && cur_idx <= num_buckets) cur_idx++;

        out_pct[n] += ((double)(cur_idx - i) * cmp_count_fraction * bucket_size);
        NSDL4_REPORTING(NULL, NULL, "Add value in percentile %d where sample index = %d (cur_idx = %d, i = %d)" 
                  " computed percentile value is out_pct[%d] = %f", n, (cur_idx - i), cur_idx, i, n, out_pct[n]);
      }
      
      n++;
      cmp_count = (((((double) total_samples) - 1) * (double) n) / (double) 100) + 1;
      cmp_count_int = (int) cmp_count; 
      cmp_count_fraction = cmp_count - (double) cmp_count_int;  
      NSDL4_REPORTING(NULL, NULL, " Next percentile %d, Compare count cmp_count = %f, integer part cmp_count_int = %d" 
                  "and fraction cmp_count_fraction = %f", n, cmp_count, cmp_count_int, cmp_count_fraction);
    }
  }
}

/*Function calculate percentile array and write percentile data into csv file*/
static void dump_pct_for_one_object(int percentile_fd, char *pkt_start_addr, pdf_data_hdr *pdf_hdr, struct tm *cur_tm, int shm_idx, char *object, int i)
{
  double percentiles[101]; //Store percentile values (1st, 2nd .... 100th percentile)
  char buff[1024*1024];
  Pdf_Data_8B *bucketCountPtr;
  //Pdf_Data *bucketCountPtr4B;
  int idx, ret;

  NSDL1_REPORTING(NULL, NULL, "Method called, object = %s, index = %d, pct start addr = %p",
               object, i, pkt_start_addr); 

  if(my_port_index == 255) 
    bucketCountPtr = (Pdf_Data_8B *)(pkt_start_addr + pdf_lookup_data[i].pdf_data_offset_parent);
  //else
    //bucketCountPtr4B = (Pdf_Data *)(pkt_start_addr + pdf_lookup_data[i].pdf_data_offset);

  get_percentiles(percentiles, bucketCountPtr, pdf_lookup_data[i].num_granules, pdf_lookup_data[i].min_granules);
   
  int amt_written = sprintf(buff, "%llu,%d/%d/%d %d:%d:%d,%llu,%s,",
        pdf_hdr->abs_timestamp, cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec,
        pdf_hdr->sequence, object);

  for(idx = 1; idx <= 100; idx++)
    amt_written += sprintf(buff + amt_written, "%f,", percentiles[idx]/1000);

  buff[amt_written - 1] = '\n'; // Replace last comma by new line
  buff[amt_written] = 0;  // Null terminate

  if((ret = write(percentile_fd, buff, amt_written)) != amt_written)
  {
    //fprintf(stderr, "Error in writing percentile file. Return value = %d. Pkt Size = %d. Error = %s\n", ret, amt_written, nslb_strerror(errno));
    NSTL1(NULL, NULL, "[Percentile]: Error in writing percentile file. Return value = %d. Pkt Size = %d. Error = %s", 
      ret, amt_written, nslb_strerror(errno));
  }

}

static void dump_percentile_shm(char *pkt_start_addr, int shm_idx)
{
  char file[MAX_LINE_LENGTH];
  int percentile_fd;
  int i;
  struct tm *cur_tm, tm_struct;

  pdf_data_hdr *pdf_hdr = (pdf_data_hdr *)pkt_start_addr;

  time_t timestamp = pdf_hdr->abs_timestamp / 1000;
  cur_tm = nslb_localtime(&(timestamp), &tm_struct, 1);

  NSDL2_REPORTING(NULL, NULL, "Method called, pdf_hdr = %p", pdf_hdr);
  /*Open percentile.csv file*/
  sprintf(file, "%s/logs/%s/percentile.csv", g_ns_wdir, global_settings->tr_or_partition);
  percentile_fd = open(file, O_CREAT|O_WRONLY | O_APPEND|O_CLOEXEC, 0666);
  if (percentile_fd < 0) {
    fprintf(stderr, "Unable to open file %s. Ignored. Error %s\n", file, nslb_strerror(errno));
    return;
  }

  for (i = 0; i < total_pdf_entries; i++) 
  {
    if(pdf_lookup_data[i].pdf_data_offset <= 0)
      continue;
    // Format of record:
    // TS(ms), TS(date/time), Object, Percentile(1st, 2nd, 3rd …..100th)
    // Where Object is  Url, Page, Tx, Session, TxName1, TxName2, ...
    // Tx may not be present is scripts do not have any tx
    char object[128] = "\0";

    if(i == pdf_average_url_response_time) strcpy(object, "url");
    else if(i == pdf_average_smtp_response_time) strcpy(object, "smtp");
    else if(i == pdf_average_pop3_response_time) strcpy(object, "pop3");
    else if(i == pdf_average_ftp_response_time) strcpy(object, "ftp");
    else if(i == pdf_average_dns_response_time) strcpy(object, "dns");
    else if(i == pdf_average_page_response_time) strcpy(object, "page");
    else if(i == pdf_average_session_response_time) strcpy(object, "session");
    else if(i == pdf_average_transaction_response_time) strcpy(object, "transaction");
    else if(i == pdf_transaction_time)
    {
       int tx_id;
       for(tx_id = 0; tx_id < total_tx_entries; tx_id++ ){
         strcpy(object, nslb_get_norm_table_data(&normRuntimeTXTable, tx_id));
         dump_pct_for_one_object(percentile_fd, pkt_start_addr, pdf_hdr, cur_tm, shm_idx, object, i);
       }
       continue;
    }
    if(object[0] != '\0') {// Skip if i is not matching with any pdf row id
      dump_pct_for_one_object(percentile_fd, pkt_start_addr, pdf_hdr, cur_tm, shm_idx, object, i);
    }
  }
  close(percentile_fd);

}

inline void reset_pct_vars_for_dynameic_tx()
{
  NSTL1(NULL, NULL, "[Percentile]: Method called, reset pct varibales for dynamic tranaction");
  is_new_tx_add = 0;
  testrun_pdf_and_pctMessgae_version = 0;
  testrun_pdf_ts = 0;
}

void create_new_ver_of_testrun_pdf_and_pctMessage_dat()
{
  char fname[NS_PCT_MAX_FILE_NAME_LEN + 1];
  char line_buf[NS_PCT_MAX_LINE_BUF_LEN + 1];
  FILE *fp1 = NULL;
  FILE *fp2 = NULL;
  int size = 0;
  int update_info_line_done;

  NSDL2_PERCENTILE(NULL, NULL, "Method called, ts = %llu, is_new_tx_add = %d, testrun_pdf_and_pctMessgae_version = %d", 
                         testrun_pdf_ts, is_new_tx_add, testrun_pdf_and_pctMessgae_version);

  /****************************************************
    Increase version number first as default is 0
    Reset flag is_new_tx_add 
   ***************************************************/
  testrun_pdf_and_pctMessgae_version++;
  is_new_tx_add = 0;

  /****************************************************
    Create new testrun.pdf with version and timestamp
    Update pdf_data_siez in testrun.pdf
   ***************************************************/
  snprintf(fname, NS_PCT_MAX_FILE_NAME_LEN, "%s/logs/%s/testrun.pdf", 
                  g_ns_wdir, global_settings->tr_or_partition);

  NSDL2_PERCENTILE(NULL, NULL, "Opening file = %s", fname);
  if((fp1 = fopen(fname, "r")) == NULL)
  {
    NSTL2(NULL, NULL, "Error: failed to open file %s due to errrno = %d and err = %s", 
                       fname, errno, nslb_strerror(errno));
    goto err; 
  }
  
  snprintf(fname, NS_PCT_MAX_FILE_NAME_LEN, "%s/logs/%s/testrun.pdf.%d.%llu", 
                  g_ns_wdir, global_settings->tr_or_partition, testrun_pdf_and_pctMessgae_version, testrun_pdf_ts);

  NSDL2_PERCENTILE(NULL, NULL, "Opening file = %s", fname);
  if((fp2 = fopen(fname, "w")) == NULL)
  {
    NSTL2(NULL, NULL, "Error: failed to open file %s due to errrno = %d and err = %s", 
                       fname, errno, nslb_strerror(errno));
    goto err; 
  }

  update_info_line_done = 0;
  while(nslb_fgets(line_buf, NS_PCT_MAX_LINE_BUF_LEN, fp1, 0) != NULL)
  {
    NSDL3_PERCENTILE(NULL, NULL, "update_info_line_done = %d, line_buf = %s", update_info_line_done, line_buf);
    if(!update_info_line_done)
    {
      NSDL3_PERCENTILE(NULL, NULL, "total_pdf_entries = %d, total_pdf_data_size_8B = %d, g_percentile_mode = %d",
                                    total_pdf_entries, total_pdf_data_size_8B, g_percentile_mode);
      //CLEAR_WHITE_SPACE(line_buf);
      if(!strncasecmp(line_buf, "Info", 4))
      {
        //Info|1.0|5|48080|2|10000
        sprintf(line_buf, "Info|1.0|%d|%llu|%d|%d\n\n",
                         total_pdf_entries,
                         total_pdf_data_size_8B, g_percentile_mode,
                         ((g_percentile_mode == PERCENTILE_MODE_INTERVAL) ? g_percentile_interval : -1));
       
        update_info_line_done = 1;
      }  
    }

    size = strlen(line_buf);
    NSDL3_PERCENTILE(NULL, NULL, "Write line = %s, of size = %d into file = %s", line_buf, size, fname);
    if((fwrite(line_buf, 1, size, fp2)) != size)
    {
      NSTL2(NULL, NULL, "Error: failed to write data '%s' into file '%s'", line_buf, fname);
      break;
      //goto err; 
    }
  }

  /****************************************************
    Create pctMessage file with version
   ***************************************************/
  NSDL2_PERCENTILE(NULL, NULL, "Closing old pct_msg_file_fd = %d", pct_msg_file_fd);
  if(pct_msg_file_fd > 0)
    close(pct_msg_file_fd);

  snprintf(fname, NS_PCT_MAX_FILE_NAME_LEN, "%s/logs/%s/pctMessage.dat.%d", 
                  g_ns_wdir, global_settings->tr_or_partition, testrun_pdf_and_pctMessgae_version);

  NSDL2_PERCENTILE(NULL, NULL, "Opening file fname = %s", fname);
  pct_msg_file_fd = open(fname, O_CREAT|O_WRONLY |O_LARGEFILE| O_APPEND|O_CLOEXEC, 0666);
  if (pct_msg_file_fd < 0) 
  {
    NSTL2(NULL, NULL, "Error: unable to open file %s due to errno = %d, err = %s\n", fname, errno, nslb_strerror(errno));
    goto err;
  }

  NSDL2_PERCENTILE(NULL, NULL, "pct_msg_file_fd = %d", pct_msg_file_fd);
  /**********************
    Error handling
   *********************/
  err:
 
  if(fp1)
    fclose(fp1);

  if(fp2)
    fclose(fp2);
}

void dump_pdf_shrmem_to_file(char *addr, int finish_report_flag_set)
{
  int active_idx = -1; /* pdf_shm_info[my_port_index].active_addr_idx, in case of parent this value will not applicable*/
  struct stat st;
  char fname[NS_PCT_MAX_FILE_NAME_LEN + 1];
  
  pdf_data_hdr *pdf_hdr = (pdf_data_hdr *)addr;
  pdf_hdr->abs_timestamp  = (time(NULL)) * 1000; // Time stamp in ms but ms part is not used so takings sec and multiplyin by 1000
  
  NSDL2_PERCENTILE(NULL, NULL, "Method called. dumping for sequence = %llu, "
       	                "g_percentile_sample_dump_to_file = %d, time_stamp_val = %lld, is_new_tx_add = %d, testrun_pdf_and_pctMessgae_version = %d", 
       			 pdf_hdr->sequence, g_percentile_sample_dump_to_file, pdf_hdr->abs_timestamp, is_new_tx_add, 
                         testrun_pdf_and_pctMessgae_version);

  g_percentile_sample_dump_to_file++;

  /* Get previous file pctMessage.dat size and if its size is more than 2GB make new version */
  if(!testrun_pdf_and_pctMessgae_version)
    snprintf(fname, NS_PCT_MAX_FILE_NAME_LEN, "%s/logs/%s/pctMessage.dat",
                    g_ns_wdir, global_settings->tr_or_partition);
  else
    snprintf(fname, NS_PCT_MAX_FILE_NAME_LEN, "%s/logs/%s/pctMessage.dat.%d",
                    g_ns_wdir, global_settings->tr_or_partition, testrun_pdf_and_pctMessgae_version);

  if((stat(fname, &st) == 0) && ((st.st_size + total_pdf_data_size_8B) > 2147483648)) //2GB
  {
    NSTL1(NULL, NULL, "[Percentile]: PCT File %s size exceed maximum size limit 2GB. Creating new file." );
    is_new_tx_add = 1;
  }

  /***************************************************************
    If new transaction added, then 
    1. Create new testrun.pdf in following format
       testrun.pdf.<version>.<timestamp>
    2. Create new pctMessage.dat in following format
       pctMessage.dat.<version>  
   **************************************************************/
  if(is_new_tx_add)
  {
    testrun_pdf_ts = pdf_hdr->abs_timestamp;
    create_new_ver_of_testrun_pdf_and_pctMessage_dat(); 
  }

#ifdef NS_DEBUG_ON
   if((group_default_settings->debug & 0x0000FF00) && (group_default_settings->module_mask & MM_REPORTING))
   {
     dump_pdf_shm(addr, active_idx);
     dump_percentile_shm(addr, active_idx);
   } 
#else
   if (finish_report_flag_set)
     dump_percentile_shm(addr, active_idx);
#endif
   int ret;
   pdf_data_hdr *hdr;
   if(loader_opcode == CLIENT_LOADER)
   {
     hdr = (pdf_data_hdr *)pdf_msg_addr;
     memcpy(hdr, pdf_hdr, sizeof(pdf_data_hdr));
     hdr->total_tx_entries = total_tx_entries;
     NSTL1(NULL, NULL, "[Percentile]: Sending percentile messge to controller."
       " Message size is %d, hdr->sequence = %llu, total transactions = %d\n", 
       total_pdf_msg_size, hdr->sequence, hdr->total_tx_entries);

     forward_dh_msg_to_master_ex(g_dh_master_msg_com_con->fd, (parent_msg *)pdf_msg_ptr, total_pdf_msg_size, sizeof(MsgHdr), global_settings->data_comp_type);
     clear_pdf_shrmem(pdf_msg_addr);
     if(save_pct_file_on_generator) {
       if((ret = write(pct_msg_file_fd, pdf_hdr, total_pdf_data_size_8B)) != total_pdf_data_size_8B)
       {
         /*fprintf(stderr, "Error in writing pct message pkt. Return value = %d. Pkt Size = %d. Error = %s\n",
                          ret, total_pdf_data_size_8B, nslb_strerror(errno));*/
         NSTL1(NULL, NULL, "[Percentile]: Error in writing pct message pkt. Return value = %d. Pkt Size = %d. Error = %s",
                          ret, total_pdf_data_size_8B, nslb_strerror(errno));
       }
     }
   }
   else
   {
     if((ret = write(pct_msg_file_fd, pdf_hdr, total_pdf_data_size_8B)) != total_pdf_data_size_8B)
     {
       /*fprintf(stderr, "Error in writing pct message pkt. Return value = %d. Pkt Size = %d. Error = %s\n", 
                        ret, total_pdf_data_size_8B, nslb_strerror(errno));*/
       NSTL1(NULL, NULL, "[Percentile]: Error in writing pct message pkt. Return value = %d. Pkt Size = %d. Error = %s",
                        ret, total_pdf_data_size_8B, nslb_strerror(errno));
     }
   }    
  // File is Closed at the end of the testrun or on switch of partition
}


/**
 * Here we also fill in the progress sample in the pdf data header also.
   Return - Shared memory index of child shared memory which is ready for parent
 */
int switch_pdf_shm(int shm_idx, char *caller)
{
  int active_idx;
  pdf_data_hdr *pdf_hdr;

  if(shm_idx != -1) // Switch to specific shmid 
  {
    NSDL2_PERCENTILE(NULL, NULL, "child id = %d, Switching %d, progress report num = %d", 
               my_port_index, pdf_shm_info[my_port_index].active_addr_idx,
               v_cur_progress_num);
    active_idx = pdf_shm_info[my_port_index].active_addr_idx; 
    pdf_hdr = (pdf_data_hdr *) pdf_shm_info[my_port_index].addr[active_idx];
    pdf_hdr->abs_timestamp  = (time(NULL)) * 1000;
 
    #ifdef NS_DEBUG_ON
    if((group_default_settings->debug & 0x0000FF00) && (group_default_settings->module_mask & MM_REPORTING))
      dump_pdf_shm(pdf_shm_info[my_port_index].addr[active_idx], active_idx);
    #endif
 
    pdf_shm_info[my_port_index].active_addr_idx = shm_idx;

    NSTL1(NULL, NULL, "[Percentile]: Switching NVM:%d percentile shared memory from index %d to %d"
      " at interval %llu for sequence %d, shm_idx = %d, calling from %s", 
      my_port_index, active_idx, pdf_shm_info[my_port_index].active_addr_idx, 
      pdf_hdr->abs_timestamp, pdf_hdr->sequence, shm_idx, caller);
  }
  else
  {
    active_idx = pdf_shm_info[my_port_index].active_addr_idx;
    pdf_hdr = (pdf_data_hdr *) pdf_shm_info[my_port_index].addr[active_idx];
    static int child_percentile_sequence = 1;
 
    NSDL2_PERCENTILE(NULL, NULL, "child id = %d, Switching %d, progress report num = %d, child_percentile_sequence = %d", 
               my_port_index, pdf_shm_info[my_port_index].active_addr_idx,
               v_cur_progress_num, child_percentile_sequence);
 
    pdf_hdr->sequence = child_percentile_sequence;
    
    child_percentile_sequence++;
 
    pdf_hdr->abs_timestamp  = (time(NULL)) * 1000; // Time stamp in ms but ms part is not used so takings sec and multiplyin by 1000
    NSDL2_PERCENTILE(NULL, NULL, "Inactive will be  = %d", pdf_shm_info[my_port_index].active_addr_idx);
 
    #ifdef NS_DEBUG_ON
    if((group_default_settings->debug & 0x0000FF00) && (group_default_settings->module_mask & MM_REPORTING))
      dump_pdf_shm(pdf_shm_info[my_port_index].addr[active_idx], active_idx);
    #endif
 
    /* Make old active - Inactive in shm hdr */
    pdf_hdr = pdf_shm_info[my_port_index].addr[active_idx];
    pdf_hdr->active = 0;
 
    if (active_idx/*  pdf_shm_info[my_port_index].active_addr_idx */) {
      /* Copy shm from active to inactive. */
      //memcpy(pdf_shm_info[my_port_index].addr[0], pdf_shm_info[my_port_index].addr[1], total_pdf_data_size);
 
      pdf_shm_info[my_port_index].active_addr_idx = 0; /* actual switch */
    } else {
      /* Copy from inactive to active. */
      //memcpy(pdf_shm_info[my_port_index].addr[1], pdf_shm_info[my_port_index].addr[0], total_pdf_data_size);
 
      pdf_shm_info[my_port_index].active_addr_idx = 1; /* actual switch */
    }
    pdf_hdr = pdf_shm_info[my_port_index].addr[pdf_shm_info[my_port_index].active_addr_idx];
    pdf_hdr->active = 1;
 
    NSDL2_PERCENTILE(NULL, NULL, "Active now  = %d", pdf_shm_info[my_port_index].active_addr_idx);

    //Doing trace level 2 bcos it is a more frequent based on interval set
    NSTL2(NULL, NULL, "[Percentile]: Switching NVM:%d percentile shared memory from index %d to %d"
      " at interval %llu for sequence %d, calling from %s", 
      my_port_index, active_idx, pdf_shm_info[my_port_index].active_addr_idx, 
      pdf_hdr->abs_timestamp, pdf_hdr->sequence, caller);
  }
  return active_idx;
}

int get_tx_offset_in_4b_mem(int graph_vector_num)
{
  int offset;
  offset = pdf_lookup_data[pdf_transaction_time].pdf_data_offset +
    (graph_vector_num * (pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data)));
  return offset - sizeof(pdf_data_hdr);
}
  
int get_tx_offset_in_8b_mem(int graph_vector_num)
{
  int offset;
  offset = pdf_lookup_data[pdf_transaction_time].pdf_data_offset_parent +
      (graph_vector_num * (pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data_8B)));
  return offset - sizeof(pdf_data_hdr);
}

//This function converts 4 byte data into 8 byte
//This function doesnot reset the src data
void add_4Bpdf_data_to_8B(Pdf_Data_8B *parent_pdf_data, Pdf_Data *child_pdf_data, int num_tx, int child_id)
{
  int j, i;
  Pdf_Data_8B *parent_pdf_data_ptr;
  int total_blocks = child_total_pdf_data_size_except_hdr_and_tx / sizeof(Pdf_Data);
  
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, 4 bytes total_blocks = %d, num_tx = %d, child_id = %d, "
                               "parent_pdf_data = %llu, child_pdf_data = %u, HRD size = %d",
                                total_blocks, num_tx, child_id, *parent_pdf_data, *child_pdf_data, sizeof(pdf_data_hdr));


  //Here parent is accumulating data in 8 byte var but child has data in 4 byte var
  //So parent pointer will increased by 8 byte and child pointer will be increased by 4 byte.
  //Also total number of samples will be taken by the size of child's data type
  for (j = 0; j < total_blocks; j++) {
    *parent_pdf_data += *child_pdf_data;
    //NSDL2_PERCENTILE(NULL, NULL, "J = %d, *parent_pdf_data = %llu, *child_pdf_data = %u", j, *parent_pdf_data, *child_pdf_data);
    parent_pdf_data++;
    child_pdf_data++;
  }

  //Copy Tx stats data
  for(i = 0; (i < num_tx) && (i < g_tx_loc2norm_table[child_id].loc2norm_alloc_size); i++)
  {
    //Get Parents memory address
    int graph_idx =  g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i];
    parent_pdf_data_ptr = (Pdf_Data_8B*)((char *)parent_pdf_data + 
                                         (graph_idx * pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data_8B)));

    NSDL4_PERCENTILE(NULL, NULL, "graph_idx = %d, parent_pdf_data_ptr = %p, num_granules = %d",
                                  graph_idx, parent_pdf_data_ptr, pdf_lookup_data[pdf_transaction_time].num_granules);
    for(j = 0; j < pdf_lookup_data[pdf_transaction_time].num_granules; j++)
    {
      NSDL2_PERCENTILE(NULL, NULL, "j = %d, *child_pdf_data = %u, *parent_pdf_data_ptr = %llu", j, *child_pdf_data, *parent_pdf_data_ptr);
      *parent_pdf_data_ptr += *child_pdf_data;
      child_pdf_data++;
      parent_pdf_data_ptr++;
    }
  }
}

void add_4Bpdf_data_to_4B(Pdf_Data *dest_pdf_data, Pdf_Data *src_pdf_data, int num_tx, int child_id)
{
  int j;
  int total_blocks = child_total_pdf_data_size_except_hdr_and_tx / sizeof(Pdf_Data);
  Pdf_Data *dest_pdf_data_ptr;

  
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, 4 bytes total_blocks = %d", total_blocks);

  for (j = 0; j < total_blocks; j++) {
    *dest_pdf_data += *src_pdf_data;
    dest_pdf_data++;
    src_pdf_data++;
  }
  
  int i;
  //Copy Tx stats data
  for(i = 0; (i < num_tx) && (i < g_tx_loc2norm_table[child_id].loc2norm_alloc_size); i++)
  {
    int graph_idx =  g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i];
    dest_pdf_data_ptr = (Pdf_Data*)((char *)dest_pdf_data + 
                                         (graph_idx * pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data)));

    NSDL2_PERCENTILE(NULL, NULL, "graph_idx = %d, dest_pdf_data_ptr = %p, num_granules = %d", 
                                  graph_idx, dest_pdf_data_ptr, pdf_lookup_data[pdf_transaction_time].num_granules);

    for(j = 0; j < pdf_lookup_data[pdf_transaction_time].num_granules; j++)
    {
      NSDL4_PERCENTILE(NULL, NULL, "Before: j = %d, *src_pdf_data = %u, *dest_pdf_data_ptr = %llu", j, *src_pdf_data, *dest_pdf_data_ptr);
      *dest_pdf_data_ptr += *src_pdf_data;
      NSDL4_PERCENTILE(NULL, NULL, "After: j = %d, *src_pdf_data = %u, *dest_pdf_data_ptr = %llu", j, *src_pdf_data, *dest_pdf_data_ptr);
      src_pdf_data++;
      dest_pdf_data_ptr++;
    }
  }
}
// Copy src to cur
void add_pdf_data_to_cur(Pdf_Data *src_pdf_data, int num_tx, int child_id) {
   NSDL2_PERCENTILE(NULL, NULL, "Method Called");

   int pdf_hdr_size = sizeof(pdf_data_hdr);
   Pdf_Data_8B *parent_pdf_data;
   Pdf_Data *child_pdf_to_send_data = (Pdf_Data *)(pdf_msg_addr + pdf_hdr_size);
   parent_pdf_data = (Pdf_Data_8B *)(parent_pdf_addr + pdf_hdr_size);

   //Must call befaore the add_pdf_data because add_pdf_data() reset the child data
   if(loader_opcode == CLIENT_LOADER)
     add_4Bpdf_data_to_4B(child_pdf_to_send_data, src_pdf_data, num_tx, child_id);

   add_pdf_data(parent_pdf_data, src_pdf_data, num_tx, child_id);
}

// Copy src to next
void add_pdf_data_to_next(Pdf_Data *src_pdf_data, int num_tx, int child_id) {
   NSDL2_PERCENTILE(NULL, NULL, "Method Called");

   int pdf_hdr_size = sizeof(pdf_data_hdr);
   Pdf_Data_8B *parent_nxt_pdf_data;
   parent_nxt_pdf_data = (Pdf_Data_8B *)(parent_pdf_next_addr + pdf_hdr_size);
   add_pdf_data(parent_nxt_pdf_data, src_pdf_data, num_tx, child_id);
}

void add_pdf_data(Pdf_Data_8B *parent_pdf_data, Pdf_Data *child_pdf_data, int num_tx, int child_id)
{
  int j, i;
  Pdf_Data_8B *parent_pdf_data_ptr;
  int total_blocks = child_total_pdf_data_size_except_hdr_and_tx / sizeof(Pdf_Data);
  
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, 4 bytes total_blocks = %d, num_tx = %d, child_id = %d, "
                               "parent_pdf_data = %llu, child_pdf_data = %u, HRD size = %d", 
                                total_blocks, num_tx, child_id, *parent_pdf_data, *child_pdf_data, sizeof(pdf_data_hdr));

  //Here parent is accumulating data in 8 byte var but child has data in 4 byte var
  //So parent pointer will increased by 8 byte and child pointer will be increased by 4 byte.
  //Also total number of samples will be taken by the size of child's data type
  for (j = 0; j < total_blocks; j++) {
    *parent_pdf_data += *child_pdf_data;
    *child_pdf_data = 0; /* Mark it Zero here so we dont have to call clear_pdf_shrmem() */
    parent_pdf_data++;
    child_pdf_data++;
  }

  //Copy Tx stats data
  for(i = 0; (i < num_tx) && (i < g_tx_loc2norm_table[child_id].loc2norm_alloc_size); i++)
  {
    int graph_idx =  g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i];
    parent_pdf_data_ptr = (Pdf_Data_8B*)((char *)parent_pdf_data + 
                                         (graph_idx * pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data_8B)));

    NSDL2_PERCENTILE(NULL, NULL, "graph_idx = %d, parent_pdf_data_ptr = %p, num_granules = %d", 
                                  graph_idx, parent_pdf_data_ptr, pdf_lookup_data[pdf_transaction_time].num_granules);

    for(j = 0; j < pdf_lookup_data[pdf_transaction_time].num_granules; j++)
    {
      NSDL4_PERCENTILE(NULL, NULL, "Before: j = %d, *child_pdf_data = %u, *parent_pdf_data_ptr = %llu", j, *child_pdf_data, *parent_pdf_data_ptr);
      *parent_pdf_data_ptr += *child_pdf_data;
      *child_pdf_data = 0;
      NSDL4_PERCENTILE(NULL, NULL, "After: j = %d, *child_pdf_data = %u, *parent_pdf_data_ptr = %llu", j, *child_pdf_data, *parent_pdf_data_ptr);
      child_pdf_data++;
      parent_pdf_data_ptr++;
    }
  }
}

//Module Mask - User Reporing or new PERCENTILE
void add_8Bpdf_to_4B(Pdf_Data *dest_pdf_data, Pdf_Data_8B *src_pdf_data, int num_tx, int child_id)
{
  int i, j;
  //int pdf_hdr_size = sizeof(pdf_data_hdr);
  //int total_blocks = ((total_pdf_data_size - pdf_hdr_size) / sizeof(Pdf_Data));
  int total_blocks = child_total_pdf_data_size_except_hdr_and_tx / sizeof(Pdf_Data);
  Pdf_Data *dest_pdf_data_ptr;

  NSDL2_PERCENTILE(NULL, NULL, "Method Called, 4 bytes total_blocks = %d, num_tx = %d, child_id = %d, "
                               "parent_pdf_data = %llu, child_pdf_data = %llu, HRD size = %d",
                                total_blocks, num_tx, child_id, *dest_pdf_data, *src_pdf_data, sizeof(pdf_data_hdr));

  for (j = 0; j < total_blocks; j++) {
    *dest_pdf_data += *src_pdf_data;
    dest_pdf_data++;
    src_pdf_data++;
  }

  //TODO:
  //Copy Tx stats data
  for(i = 0; (i < num_tx) && (i < g_tx_loc2norm_table[child_id].loc2norm_alloc_size); i++)
  {
    int graph_idx =  g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i];
    dest_pdf_data_ptr = (Pdf_Data*)((char *)dest_pdf_data +
                                         (graph_idx * pdf_lookup_data[pdf_transaction_time].num_granules * sizeof(Pdf_Data)));

    NSDL2_PERCENTILE(NULL, NULL, "graph_idx = %d, dest_pdf_data_ptr = %p, num_granules = %d",
                                  graph_idx, dest_pdf_data_ptr, pdf_lookup_data[pdf_transaction_time].num_granules);

    for(j = 0; j < pdf_lookup_data[pdf_transaction_time].num_granules; j++)
    {
      NSDL4_PERCENTILE(NULL, NULL, "Before: j = %d, *src_pdf_data = %llu, *dest_pdf_data_ptr = %u", j, *src_pdf_data, *dest_pdf_data_ptr);
      *dest_pdf_data_ptr += *src_pdf_data;
      NSDL4_PERCENTILE(NULL, NULL, "After: j = %d, *src_pdf_data = %llu, *dest_pdf_data_ptr = %u", j, *src_pdf_data, *dest_pdf_data_ptr);
      src_pdf_data++;
      dest_pdf_data_ptr++;
    }
  }
}

void add_8Bpdf_to_8B(Pdf_Data_8B *parent_pdf_data, Pdf_Data_8B *child_pdf_data, int num_tx, int child_id)
{
  int j;
  //int pdf_hdr_size = sizeof(pdf_data_hdr);
  //int total_blocks = ((total_pdf_data_size_8B - pdf_hdr_size) / sizeof(Pdf_Data_8B));
  int total_blocks = child_total_pdf_data_size_except_hdr_and_tx / sizeof(Pdf_Data);
  
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, 8 bytes total_blocks = %d", total_blocks);

  for (j = 0; j < total_blocks; j++) {
    *parent_pdf_data += *child_pdf_data;
    parent_pdf_data++;
    *child_pdf_data = 0; /* Mark it Zero here so we dont have to call clear_pdf_shrmem() */
    child_pdf_data++;
  }
}

void add_8Bpdf_data_to_cur_8B(Pdf_Data_8B *src_pdf_data, int num_tx, int child_id) {
   NSDL2_PERCENTILE(NULL, NULL, "Method Called");

   int pdf_hdr_size = sizeof(pdf_data_hdr);
   Pdf_Data_8B *parent_pdf_data;
   Pdf_Data *child_pdf_added_data = (Pdf_Data *)(pdf_msg_addr + pdf_hdr_size);
   parent_pdf_data = (Pdf_Data_8B *)(parent_pdf_addr + pdf_hdr_size);
   if(loader_opcode == CLIENT_LOADER)
     add_8Bpdf_to_4B(child_pdf_added_data, src_pdf_data, num_tx, child_id);
   add_8Bpdf_to_8B(parent_pdf_data, src_pdf_data, num_tx, child_id);
}

static void set_parent_cur_sequence(int sequence) {
   NSDL2_PERCENTILE(NULL, NULL, "Method Called");
   pdf_data_hdr *p_pdf_hdr;

   p_pdf_hdr = (pdf_data_hdr *)parent_pdf_addr;
   p_pdf_hdr->sequence = sequence; // Set sequence 
}

static void set_parent_next_sequence(int sequence) {
   NSDL2_PERCENTILE(NULL, NULL, "Method Called");
   pdf_data_hdr *p_pdf_hdr;

   p_pdf_hdr = (pdf_data_hdr *)parent_pdf_next_addr;
   p_pdf_hdr->sequence = sequence; // Set sequence 
}


int dump_pdf_to_file_if_cur_complete(int finish_report_flag_set) {
  int num_left = 0;
  NSDL2_PERCENTILE(NULL, NULL, "Method Called, parent_cur_count = %d," 
    " g_last_sample_count = %d, num_process_for_nxt_sample = %d, "
    "total_killed_nvms = %d, total_killed_generator = %d, num_left = %d",
     parent_cur_count, g_last_sample_count, num_process_for_nxt_sample, 
     g_data_control_var.total_killed_nvms, g_data_control_var.total_killed_generator, num_left);

   //In case of NC: On Controller total_killed_nvms always will be 0
  //In case of Generator/Standalone: total_killed_gen always will be 0
  if (g_percentile_mode == PERCENTILE_MODE_TOTAL_RUN || g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    num_left = ((global_settings->num_process - g_data_control_var.total_killed_nvms) - g_data_control_var.total_killed_generator);
  } else {
    num_left = ((num_process_for_nxt_sample - g_data_control_var.total_killed_nvms) - g_data_control_var.total_killed_generator);//Number of running processes for next sample
  }

  NSDL2_PERCENTILE(NULL, NULL, "parent_cur_count = %d, num_left = %d, g_last_sample_count = %d", 
    parent_cur_count, num_left, g_last_sample_count);

  if (parent_cur_count == num_left) {
    NSDL3_PERCENTILE(NULL, NULL, "cur complete  parent_cur_count = %d, num_left = %d", parent_cur_count, num_left);
    num_process_for_nxt_sample -= g_last_sample_count;//Number of running processes for next sample 
    g_last_sample_count = 0;    /* Making it Zero ??????????? */

    dump_pdf_shrmem_to_file(parent_pdf_addr, finish_report_flag_set);
    parent_cur_count = 0;
    g_percentile_sample++;

    return 1;
  }
  NSDL3_PERCENTILE(NULL, NULL, "NOT COMPLETE parent_cur_count = %d, num_left = %d", parent_cur_count, num_left);
  return 0;
}

int dump_pdf_to_file_if_next_complete(int finish_report_flag_set, int num_tx, int child_id) {
  int num_left;
  int pdf_hdr_size = sizeof(pdf_data_hdr);

  NSDL2_PERCENTILE(NULL, NULL, "Method Called, parent_next_count = %d," 
                  " g_last_sample_count = %d, num_process_for_nxt_sample = %d",
                      parent_next_count, g_last_sample_count, num_process_for_nxt_sample);
  if (g_percentile_mode == PERCENTILE_MODE_TOTAL_RUN || g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    num_left = ((global_settings->num_process - g_data_control_var.total_killed_nvms) - g_data_control_var.total_killed_generator);
  } else {
    num_left = ((num_process_for_nxt_sample - g_data_control_var.total_killed_nvms) - g_data_control_var.total_killed_generator);//Number of running processes for next sample
  }

  if (parent_next_count == num_left) {
    NSDL3_PERCENTILE(NULL, NULL, "cur complete  parent_cur_count = %d, num_left = %d", parent_cur_count, num_left);
    num_process_for_nxt_sample -= g_last_sample_count; //Number of running processes for next sample
    g_last_sample_count = 0;    /* Making it Zero ??????????? */
    /* Add next to cur */
    set_parent_cur_sequence(((pdf_data_hdr *)parent_pdf_next_addr)->sequence); /* Save next seq to cur seq */
    add_8Bpdf_data_to_cur_8B(parent_pdf_next_addr + pdf_hdr_size, num_tx, child_id);
    dump_pdf_shrmem_to_file(parent_pdf_addr, finish_report_flag_set);
    return 1;
  }
  return 0;
}

/**
 * This function will copy the pdf data to file.
 */
void copy_and_flush_child_pdf_data(int child_id, int shm_idx, int finish_report_flag_set, char *data_buf, int total_tx) 
{
  //int inactive_idx;
  Pdf_Data *child_pdf_data;
  pdf_data_hdr *c_pdf_hdr;

  Pdf_Data_8B *ctrl_gen_pdf_data;
  pdf_data_hdr *ctrl_gen_pdf_hdr;

  int pdf_hdr_size = sizeof(pdf_data_hdr);
  int i, ret;

  NSDL2_PERCENTILE(NULL, NULL, "Method Called, child = %d, shm_idx = %d, pdf_hdr_size = %d, "
    "parent_cur_count = %d, parent_next_count = %d, g_percentile_sample = %d",
     child_id, shm_idx, pdf_hdr_size, parent_cur_count, parent_next_count, g_percentile_sample);

  i = child_id;

  if(loader_opcode == MASTER_LOADER)
  {
    c_pdf_hdr = (pdf_data_hdr *)(data_buf);
    child_pdf_data = (Pdf_Data *)(data_buf + pdf_hdr_size);
    
    ctrl_gen_pdf_hdr = (pdf_data_hdr *)(parent_controller_pdf_mem[child_id + 1]);
    ctrl_gen_pdf_data = (Pdf_Data_8B *)(parent_controller_pdf_mem[child_id + 1] + pdf_hdr_size);

    NSDL1_PERCENTILE(NULL, NULL, "percentile sample. NVM = %d, " 
      		    	  "sequence = %llu, cur_count = %d, next_count = %d",
                           child_id, c_pdf_hdr->sequence,
      		           parent_cur_count, parent_next_count);

    memcpy(ctrl_gen_pdf_hdr, c_pdf_hdr, sizeof(pdf_data_hdr));
    add_4Bpdf_data_to_8B(ctrl_gen_pdf_data, child_pdf_data, total_tx, child_id);

    /* Creating new version of pctMessage.dat for generator */
    dump_pdf_data_into_file(child_id);

    if((ret = write(generator_entry[child_id].pct_fd, ctrl_gen_pdf_hdr, total_pdf_data_size_8B)) != total_pdf_data_size_8B)
    {
      /*fprintf(stderr, "Error in writing pct message pkt for generator %d. Return value = %d. Pkt Size = %d. Error = %s\n", 
                       child_id, ret, total_pdf_data_size_8B, nslb_strerror(errno));*/
      NSTL1(NULL, NULL, "[Percentile]: Error in writing pct message pkt for generator %d. Return value = %d. Pkt Size = %d. Error = %s",
                       child_id, ret, total_pdf_data_size_8B, nslb_strerror(errno));
      return;
    }
  }
  else
  {
    #if 0
    /* Use Inactive one because active one is now used for filling new data. */
    c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[0]);

    if (g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
        c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[shm_idx]);
        child_pdf_data = (Pdf_Data *)(pdf_shm_info[i].addr[shm_idx] + pdf_hdr_size);
    } else {
      if (c_pdf_hdr->active) {
        c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[1]);
        child_pdf_data = (Pdf_Data *)(pdf_shm_info[i].addr[1] + pdf_hdr_size);
       // inactive_idx = 1;
      } else {
        c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[0]);
        child_pdf_data = (Pdf_Data *)(pdf_shm_info[i].addr[0] + pdf_hdr_size);
        //inactive_idx = 0;
      }
    }
    #endif 

    c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[shm_idx]);
    child_pdf_data = (Pdf_Data *)(pdf_shm_info[i].addr[shm_idx] + pdf_hdr_size);
  }

  /* Case1: Sequence is older than expected Sample. Ignore */
  if (c_pdf_hdr->sequence < g_percentile_sample) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                               __FILE__, (char*)__FUNCTION__,
                               "Percentile data sample number (%llu) from NVM %d is " 
      			       "less than expected sample number (%d). Ignored.",
      			       c_pdf_hdr->sequence, child_id, g_percentile_sample);
    return;
  } 

  /* Case2: Sequence is same as expected Sample */
  if (c_pdf_hdr->sequence == g_percentile_sample) {

    NSDL3_PERCENTILE(NULL, NULL, "Correct percentile sample. NVM = %d, " 
      		    	  "sequence = %llu, cur_count = %d, next_count = %d",
                           child_id, c_pdf_hdr->sequence,
      		           parent_cur_count, parent_next_count);

    /* Add child to parent cur and make child 0*/
    add_pdf_data_to_cur(child_pdf_data, total_tx, child_id);
    parent_cur_count++; /* Increment cur count */
    set_parent_cur_sequence(c_pdf_hdr->sequence); // Set sequence 

    /* if current pdf is complete than dump cur into file*/ 
    if (dump_pdf_to_file_if_cur_complete(finish_report_flag_set)) {
      if(parent_next_count != 0) { // If next has some samples, then we need to add these in cur
        /* Add next to parent cur and make next 0*/
        add_8Bpdf_data_to_cur_8B((parent_pdf_next_addr + pdf_hdr_size), total_tx, child_id);
        // Note - Next cannot be complete as it is already handled in Case3
      }
      parent_cur_count = parent_next_count; // 0 or not 0
      parent_next_count = 0; // Must set to 0 as we have used next data
    }
  /* Case3: next sequence came from some child */ 
  } else if (c_pdf_hdr->sequence == (g_percentile_sample + 1)) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                              __FILE__, (char*)__FUNCTION__,
      			      "Next percentile sample (%llu) recieved from NVM (%d). "
     			      "Saving in next data. cur_count = %d, next_count = %d",
                              c_pdf_hdr->sequence, child_id,
      		              parent_cur_count, parent_next_count);

    /* Add child to parent next and make child 0*/
    add_pdf_data_to_next(child_pdf_data, total_tx, child_id);
    parent_next_count++; /* Increment next count */
    set_parent_next_sequence(c_pdf_hdr->sequence); /* Save child seq to parent next sequnce */

    if (dump_pdf_to_file_if_next_complete(finish_report_flag_set, total_tx, child_id)) {
      /* If next is complete than add next to cur & dump.
       * So current sample is lost (not dumped to file). Only next got dumped */
      /* Here also we have one miss */ 
      parent_cur_count = parent_next_count = 0;
      g_percentile_sample += 2; // Increment by 2 as we dumped next and cur was lost
    }
    /* Case4: (next + 1) sequence came for same child */ 
  } else if (c_pdf_hdr->sequence == (g_percentile_sample + 2)) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                              __FILE__, (char*)__FUNCTION__,
      			      "Next plus 1 percentile sample (%llu) "
     			      "recieved from NVM (%d). "
      			      "Moving next data to current data. "
      			      "cur_count = %d, next_count = %d",
                              c_pdf_hdr->sequence, child_id,
      			      parent_cur_count, parent_next_count);

    /* Add next to cur & make next to 0*/ 
    if(parent_next_count) {
      add_8Bpdf_data_to_cur_8B(parent_pdf_next_addr + pdf_hdr_size, total_tx, child_id);
      // Need to set seq also 
      set_parent_cur_sequence(((pdf_data_hdr *)parent_pdf_next_addr)->sequence); /* Save next seq to cur seq */
    }
    parent_cur_count = parent_next_count;

    /* Add child to parent next and make child 0*/
    add_pdf_data_to_next(child_pdf_data, total_tx, child_id);
    parent_next_count = 1;
    set_parent_next_sequence(c_pdf_hdr->sequence); /* Save child seq to parent next sequnce */

    // Cur cannot be complete at this point as it is copied from next
    // Next can be complete only if one NVM is there or only one NVM left to send reports
    //
    if (dump_pdf_to_file_if_next_complete(finish_report_flag_set, total_tx, child_id)) { 
      parent_cur_count = parent_next_count = 0;
      g_percentile_sample = c_pdf_hdr->sequence + 1;
    }

    /* Case4: > (next + 1) sequence came for same child */ 
  } else if (c_pdf_hdr->sequence > (g_percentile_sample + 2)) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                              	    __FILE__, (char*)__FUNCTION__,
     				    "Next plus 2 or more percentile sample (%llu) "
      				    "recieved from NVM (%d). Expected sample = %d. "
      				    "Moving next data to current data. "
      				    "cur_count = %d, next_count = %d",
                              	    c_pdf_hdr->sequence, child_id, g_percentile_sample,
				    parent_cur_count, parent_next_count);

    // First add next data to cur so we do not lose it
    if(parent_next_count) {
      add_8Bpdf_data_to_cur_8B(parent_pdf_next_addr + pdf_hdr_size, total_tx, child_id);
      set_parent_cur_sequence(((pdf_data_hdr *)parent_pdf_next_addr)->sequence); /* Save next seq to cur seq */
      parent_cur_count += parent_next_count;
    }

    if(parent_cur_count) {
      dump_pdf_shrmem_to_file(parent_pdf_addr, finish_report_flag_set); // Dump what ever we have
      /*Here is magic for getting missed samples*/
      g_percentile_sample_missed += (c_pdf_hdr->sequence - (g_percentile_sample + 1));
    }
    else {
      /*Here is magic for getting missed samples*/
      g_percentile_sample_missed += (c_pdf_hdr->sequence - g_percentile_sample);
    }

    // Now we start from clean state again
    parent_cur_count = parent_next_count = 0;

    // Now add data from child to current
    add_pdf_data_to_cur(child_pdf_data, total_tx, child_id);
    parent_cur_count = 1;

    // Note: GUI handles missing sequences in the file
    set_parent_cur_sequence(c_pdf_hdr->sequence); // Set sequence in cur

    /* Here is magic for skipping samples. Must be done after calculating missed samples */
    g_percentile_sample = c_pdf_hdr->sequence; // Reset to the seq to recover
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                              	    __FILE__, (char*)__FUNCTION__,
       				   "Cumulative missed percentile samples = %d for NVM (%d)",
      				    g_percentile_sample_missed, child_id);
    dump_pdf_to_file_if_cur_complete(finish_report_flag_set); // In case of one NVM or one NVM left
  }
}

//Called from child to fill percentile data
void update_pdf_data(Pdf_Data value, int index, 
                     int group_vector_num, int group_pdf_data_size, int graph_vector_num)
{
  Pdf_Data *pdf_data_ptr;
  int active_idx;
  int granule;
  int offset;
  
  if(index == -1)
    return;
  
  active_idx = pdf_shm_info[my_port_index].active_addr_idx;

  offset = pdf_lookup_data[index].pdf_data_offset + 
    (graph_vector_num * (pdf_lookup_data[index].num_granules * sizeof(Pdf_Data)));

  /* Adjust group vector */
  if (group_pdf_data_size) {
    offset = offset + (group_pdf_data_size * group_vector_num);
  }
  
  if (value > (unsigned long)pdf_lookup_data[index].max_granules) {
    granule = pdf_lookup_data[index].num_granules - 1;
  } else {
    granule = value / pdf_lookup_data[index].min_granules;
  }
  
  pdf_data_ptr = (Pdf_Data *)(pdf_shm_info[my_port_index].addr[active_idx] + offset);
  pdf_data_ptr[granule]++;
}

/**
 * Function disables percentile report generation when run mode is not NORMAL_RUN
 */
void set_percentile_report_for_sla(int run_mode, int org_percentile_report)
{
  if (org_percentile_report == 1) {
    if (run_mode != NORMAL_RUN) {
      g_percentile_report = 0;
    } else {
      g_percentile_report = org_percentile_report;
    }
  }
}

/**
 * Function appends END|last_sequence line at the end of testrun.pdf
 * Parent calls it.
 */
void pdf_append_end_line_ex(char *prev_file)
{
  //int interval = (global_settings->progress_secs * (cur_sample - 1)) % g_percentile_interval;
  FILE *testrun_pdf_fd;
  char line[MAX_LINE_LENGTH];
  char file_name[MAX_LINE_LENGTH];
  struct stat st;

  NSDL2_PERCENTILE(NULL, NULL, "Method Called, g_percentile_sample = %d, "
					"g_percentile_sample_missed = %d, "
					"g_percentile_sample_dump_to_file = %d",
					g_percentile_sample,
					g_percentile_sample_missed,
					g_percentile_sample_dump_to_file);

  if(stat(prev_file, &st) == -1)
  {
    NSDL2_PERCENTILE(NULL, NULL, "File '%s' does not exist. errno = %d, err = %s\n", prev_file, errno, nslb_strerror(errno));
    return;
  }

  testrun_pdf_fd = fopen(prev_file, "a");

  if (testrun_pdf_fd == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }

  //sprintf(line, "\nEND|%d\n", interval);
  sprintf(line, "\nEND|%d\n", g_percentile_sample_dump_to_file);
  fwrite(line, strlen(line), 1, testrun_pdf_fd);
  fclose(testrun_pdf_fd);
}

void pdf_append_end_line()
{
  char file_name[MAX_LINE_LENGTH] = "";

  sprintf(file_name, "%s/logs/%s/testrun.pdf", g_ns_wdir, global_settings->tr_or_partition);

  pdf_append_end_line_ex(file_name);
}

void validate_percentile_settings() 
{
  if (global_settings->num_fetches && g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    NS_EXIT(-1, CAV_ERR_1011293);
  }

  if (g_percentile_report == 1 && g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    if (global_settings->schedule_by != SCHEDULE_BY_SCENARIO ||
        global_settings->schedule_type != SCHEDULE_TYPE_SIMPLE) {
      NS_EXIT(-1, CAV_ERR_1011294);
    }
  }
}

/*To fill percentile data in summary.data file*/
static void set_data (double *out_arr, double *in_arr)
{
  NSDL1_REPORTING(NULL, NULL, "Method called");
  out_arr[0] = in_arr[50];
  out_arr[1] = in_arr[80];
  out_arr[2] = in_arr[90];
  out_arr[3] = in_arr[95];
  out_arr[4] = in_arr[99];
}

void fill_percentiles(char *addr, double *url_data, double *smtp_data, double *pop3_data, double *ftp_data, double *dns_data, double *pg_data, double *tx_data, double *ss_data)
{
  int i;
  double percentiles[101]; //Store percentile values (1st, 2nd .... 100th percentile)  
  Pdf_Data *bucketCountPtr;

  NSDL2_PERCENTILE(NULL, NULL, "Method called");
  
  //101973 - Case when percentile data not exist and in deliver_report while handling FINISH_REPORT we will call this function 
  if(!addr)
  {
    NSTL1(NULL, NULL,"Percentile data doesn't exist, hence returing..");
    return;
  }
  for (i = 0; i < total_pdf_entries; i++)
  {
    if(pdf_lookup_data[i].pdf_data_offset <= 0)
      continue;
    char object[128] = "\0";

    if(i == pdf_average_url_response_time) strcpy(object, "url");
    else if(i == pdf_average_smtp_response_time) strcpy(object, "smtp");
    else if(i == pdf_average_pop3_response_time) strcpy(object, "pop3");
    else if(i == pdf_average_ftp_response_time) strcpy(object, "ftp");
    else if(i == pdf_average_dns_response_time) strcpy(object, "dns");
    else if(i == pdf_average_page_response_time) strcpy(object, "page");
    else if(i == pdf_average_session_response_time) strcpy(object, "session");
    else if(i == pdf_average_transaction_response_time) strcpy(object, "transaction");

    /*Achint: Here addr is parent address but pdf_data_offset is arranged with 4 byte data. So error*/
    bucketCountPtr = (Pdf_Data *)(addr + pdf_lookup_data[i].pdf_data_offset_parent);
    get_percentiles(percentiles, (unsigned long long *)bucketCountPtr, pdf_lookup_data[i].num_granules, pdf_lookup_data[i].min_granules);

    if (!strcmp(object, "url")) {
      set_data(url_data, percentiles);
    } else if (!strcmp(object, "pop3")) {
      set_data(pop3_data, percentiles);
    } else if (!strcmp(object, "ftp")) {
      set_data(ftp_data, percentiles);
    } else if (!strcmp(object, "dns")) {
      set_data(dns_data, percentiles);
    } else if (!strcmp(object, "page")) {
      set_data(pg_data, percentiles);
    } else if (!strcmp(object, "session")) {
      set_data(ss_data, percentiles);
    } else if (!strcmp(object, "transaction")) {
      set_data(tx_data, percentiles);
    }
  }
}

inline void update_pdf_shm_info_addr()
{
  int old_pdf_data_size = 0;
  int pdf_hdr_size = sizeof(pdf_data_hdr);

  NSDL1_PERCENTILE(NULL, NULL, "Method called, my_port_index = %d, g_percentile_mode = %d", 
                                my_port_index, g_percentile_mode);

  /****************************************************************
    Copying pdf data of pdf_shm_info into pdf_shm_info_temp
   ****************************************************************/

  old_pdf_data_size = pdf_shm_info[my_port_index].total_pdf_data_size_for_this_child - pdf_hdr_size;

  NSDL1_PERCENTILE(NULL, NULL, "old_pdf_data_size = %d", old_pdf_data_size);
  memcpy(pdf_shm_info_temp[my_port_index].addr[0] + pdf_hdr_size, pdf_shm_info[my_port_index].addr[0] + pdf_hdr_size, old_pdf_data_size);
  memcpy(pdf_shm_info_temp[my_port_index].addr[1] + pdf_hdr_size, pdf_shm_info[my_port_index].addr[1] + pdf_hdr_size, old_pdf_data_size);

  if(g_percentile_mode == PERCENTILE_MODE_ALL_PHASES)  
    memcpy(pdf_shm_info_temp[my_port_index].addr[2] + pdf_hdr_size, pdf_shm_info[my_port_index].addr[2] + pdf_hdr_size, old_pdf_data_size);

  /****************************************************************
    Detatch old shared memory i.e pdf_shm_info[child_id].addr[0]
   ****************************************************************/
  detach_shared_memory(pdf_shm_info[my_port_index].addr[0]);

  /****************************************************************
    Update pdf_shm_info for this child  
   ****************************************************************/
  pdf_shm_info[my_port_index].addr[0] = pdf_shm_info_temp[my_port_index].addr[0]; 
  pdf_shm_info[my_port_index].addr[1] = pdf_shm_info_temp[my_port_index].addr[1]; 

  if(g_percentile_mode == PERCENTILE_MODE_ALL_PHASES)  
    pdf_shm_info[my_port_index].addr[2] = pdf_shm_info_temp[my_port_index].addr[2]; 

  pdf_shm_info[my_port_index].total_pdf_data_size_for_this_child = pdf_shm_info_temp[my_port_index].total_pdf_data_size_for_this_child;
}

inline void send_attach_pdf_shm_msg_to_parent(int shm_id)
{
  parent_child send_msg;
  
  NSDL1_PERCENTILE(NULL, NULL, "Method called, my_port_index = %d, total_pdf_data_size = %d, shm_id = %d", 
                                my_port_index, pdf_shm_info[my_port_index].total_pdf_data_size_for_this_child, 
                                pdf_shm_info[my_port_index].shm_id);

  NSTL1(NULL, NULL, "[Percentile]: Method called, my_port_index = %d, total_pdf_data_size = %d, shm_id = %d", 
                                my_port_index, pdf_shm_info[my_port_index].total_pdf_data_size_for_this_child, 
                                shm_id);

  send_msg.opcode = ATTACH_PDF_SHM_MSG; 
  send_msg.child_id = my_port_index;
  send_msg.total_pdf_data_size = pdf_shm_info[my_port_index].total_pdf_data_size_for_this_child;
  send_msg.shm_id = shm_id;
  send_msg.msg_len = sizeof(send_msg) - sizeof(int);

  send_child_to_parent_msg("PDF_ATTACH_SHM_MSG", (char *)&send_msg, sizeof(send_msg), DATA_MODE);
}

inline void process_attach_pdf_shm_msg(parent_msg *msg)
{
  void *shm_addr = NULL;
  parent_child *msg_ptr = (parent_child *)&msg->top.internal;

  NSDL1_PERCENTILE(NULL, NULL, "Method called, control connection child_id = %d, shm_id = %d", msg_ptr->child_id, msg_ptr->shm_id);
  NSTL1(NULL, NULL, "[Percentile]: Going to attach pdf data shared memory:"
    " child_id = %d, shm_id = %d, total_pdf_data_size = %d, g_percentile_mode = %d", 
    msg_ptr->child_id, msg_ptr->shm_id, msg_ptr->total_pdf_data_size, g_percentile_mode);

  /****************************************************************
    Attaching newly created shared memory of pdf 
    and updating addr 
  ****************************************************************/
  shm_addr = attach_shm_by_shmid_ex(msg_ptr->shm_id, AUTO_DEL_SHM);
  if(get_shm_info(msg_ptr->shm_id) == 0)
  {
    NSTL1(NULL, NULL, "[Percentile]: WARN: control connection Shm-at for id %d looks failed due to memory already deleted by child, "
                      "may child created new sharem memory and delete last one", msg_ptr->shm_id);
    return;
  }

  /****************************************************************
    Detatch old shared memory i.e pdf_shm_info[child_id].addr[0]
   ****************************************************************/
  detach_shared_memory(pdf_shm_info[msg_ptr->child_id].addr[0]);

  /****************************************************************
    Update addr into pdf_shm_info 
   ****************************************************************/
  pdf_shm_info[msg_ptr->child_id].addr[0] = shm_addr;
  pdf_shm_info[msg_ptr->child_id].addr[1] = pdf_shm_info[msg_ptr->child_id].addr[0] + msg_ptr->total_pdf_data_size;

  if(g_percentile_mode == PERCENTILE_MODE_ALL_PHASES)
    pdf_shm_info[msg_ptr->child_id].addr[2] = pdf_shm_info[msg_ptr->child_id].addr[0] + (2 * msg_ptr->total_pdf_data_size);

  NSDL1_PERCENTILE(NULL, NULL, "End: control connection addr[0] = %p, addr[1] = %p, addr[2] = %p", 
                                pdf_shm_info[msg_ptr->child_id].addr[0], pdf_shm_info[msg_ptr->child_id].addr[1], 
                                pdf_shm_info[msg_ptr->child_id].addr[2]);

  NSTL1(NULL, NULL, "[Percentile]: Control connection Pdf data shared memory attached successfully, addr[0] = %p, addr[1] = %p, addr[2] = %p",
                     pdf_shm_info[msg_ptr->child_id].addr[0], pdf_shm_info[msg_ptr->child_id].addr[1],
                     pdf_shm_info[msg_ptr->child_id].addr[2]);
}

/********************************************************************************************
 * Name    :    check_duplicate_pdf_files
 * Purpose :    Checking for duplicate entries of PDF file used in scenario file 
  
                Eg: TRANSACTION_RESPONSE_PDF _trans_avg_resp_time_medium_granularity.pdf
                    TRANSACTION_TIME_PDF     _trans_avg_resp_time_medium_granularity.pdf

                Error wil come if same pdf file will be used twice for PDF files in scenario.
**********************************************************************************************/
void check_duplicate_pdf_files()
{
  int i, j;
  int total_pdf_entries = 5;
  
  struct PDF_LIST 
  {
    char pdf_type[32];
    char pdf_fname[1024]; 
  };

  struct PDF_LIST pdf_list[total_pdf_entries];

  NSDL1_PERCENTILE(NULL, NULL, "Method called");

  strcpy(pdf_list[0].pdf_type, "URL_PDF");
  strcpy(pdf_list[0].pdf_fname, global_settings->url_pdf_file);           //0 - URL_PDF

  strcpy(pdf_list[1].pdf_type, "SESSION_PDF");
  strcpy(pdf_list[1].pdf_fname, global_settings->session_pdf_file);       //1 - SESSION_PDF
  
  strcpy(pdf_list[2].pdf_type, "PAGE_PDF");
  strcpy(pdf_list[2].pdf_fname, global_settings->page_pdf_file);          //2 - PAGE_PDF
  
  strcpy(pdf_list[3].pdf_type, "TRANSACTION_TIME_PDF");
  strcpy(pdf_list[3].pdf_fname, global_settings->trans_time_pdf_file);    //3 - TRANSACTION_TIME_PDF
  
  strcpy(pdf_list[4].pdf_type, "TRANSACTION_RESPONSE_PDF");
  strcpy(pdf_list[4].pdf_fname, global_settings->trans_resp_pdf_file);    //4 - TRANSACTION_RESPONSE_PDF
 
  for(i = 0; i < total_pdf_entries; i++)
  {
    for(j = i+1; j < total_pdf_entries; j++)
    {
      if(!strcasecmp(pdf_list[i].pdf_fname, pdf_list[j].pdf_fname))
      {
        NSDL1_PERCENTILE(NULL, NULL, "pdf_name[i] = %s, pdf_name[j] = %s", pdf_list[i].pdf_fname, pdf_list[j].pdf_fname);
        NS_EXIT(-1, CAV_ERR_1031028, pdf_list[i].pdf_fname, pdf_list[i].pdf_type, pdf_list[j].pdf_type); 
      }
    }
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 Name		: process_percentile_report_msg()
 Purpose	: This function is called by Parent's Data thread -
                  (i.e. Data Connection) 
                  Process percentile message sent by NVMs/Generators.
                  NVM will send only small message of type parent_child to 
                  inform Parent/Generator that Dump percentaile report now.
                  But Generator will send complete pct message to Controller.
 Date		: 4 July 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void process_percentile_report_msg(Msg_com_con *mccptr)
{
  parent_child *msg;             // Point to message sent by NVM/Generator 
  pdf_data_hdr *pct_hdr;         // Percentile Data Header
  char *pct_pkt = NULL;          // Percentile Data Packet
  int child_id = -1;             // NVM index 
  int shmidx = -1;               // Shared memory index
  int finish = 0;                // Flag to for FINISH Report
  int tot_tx = 0;                // Total numbar of tranactions
  unsigned long now = time(NULL) * 1000;

  msg = (parent_child *)mccptr->read_buf;
  child_id = msg->child_id;
  shmidx = msg->shm_id;
  finish = msg->gen_rtc_idx;

  NSDL2_PERCENTILE(NULL, NULL, "Method called");

  //TODO: Add trace in if case, add generator name also
  NSTL1(NULL, NULL, "[Percentile]: Received message PERCENTILE_REPORT from NVM:%d -> %s,"
    "MsgLen = %d, MsgBirthTime = %f, MsgReceiveTime = %lu, MsgContinutity = %d, TotTrans = %d, "
    "PCTShmInx = %d, finish = %d",
     child_id, (loader_opcode == MASTER_LOADER)?"Controller":"Parent", msg->msg_len, 
     msg->abs_ts, now, finish, msg->testidx, shmidx, finish); 

  if(loader_opcode == MASTER_LOADER) // Handle Generator's message
  {
    pct_pkt = (char*)msg + sizeof(PercentileMsgHdr);  //Skip Parent-Child message header
    pct_hdr = (pdf_data_hdr *)pct_pkt;
    tot_tx = pct_hdr->total_tx_entries;
  }
  else  // Handle NVM message
  {
    tot_tx = msg->testidx;       // Here testidx is used to store total transactions 

    if(g_percentile_mode == PERCENTILE_MODE_INTERVAL)
    {
      if(finish)
        g_last_sample_count++;

      //TODO: Remove sample_array code and use bitmask
      if (!process_last_sample_array)
      {
        process_last_sample_array = malloc(global_settings->num_process + 1);
        if(process_last_sample_array)
          memset(process_last_sample_array, 0, global_settings->num_process);
      }

      process_last_sample_array[child_id] = 1;
    }
  }
 
  NSDL2_PERCENTILE(NULL, NULL, "[Percentile]: Copy and flush data, child_id = %d, "
    "shmidx = %d, finish = %d, pct_pkt = %p, tot_tx = %d, g_last_sample_count = %d", 
     child_id, shmidx, finish, pct_pkt, tot_tx, g_last_sample_count);

  // Copy data from NVM/gen memory/message into Parent/Master memory
  copy_and_flush_child_pdf_data(child_id, shmidx, finish, pct_pkt, tot_tx);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name 		: flush_pctdata()
  Purpose	: Flush percentile data if any
  Date		: 6 July 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void flush_pctdata()
{
  int i;
  Pdf_Data *child_pdf_data;
  Pdf_Data_8B *parent_pdf_data;
  pdf_data_hdr *p_pdf_hdr, *c_pdf_hdr;
  int copied_flag = 0;
  
  NSDL1_PERCENTILE(NULL, NULL, "[PCT-Flush], Method called, g_percentile_report = %d, "
    "g_percentile_mode = %d, total_pdf_data_size = %d, parent_cur_count = %d, "
    "parent_next_count = %d", 
    g_percentile_report, g_percentile_mode, total_pdf_data_size, parent_cur_count, 
    parent_next_count);

  /*================================================================ 
   Flush Percentile data only if -
     1. Percentile is ON 
     2. Percentile mode is INTERVAL
     3. Percentile data is available either for current sample OR
        for next sample
     Note:=> Why need to check total_pdf_data_size as if percentile 
      is ON then its total_pdf_data_size must be greater than 
      PDF_DATA_HEADER_SIZE. 
      TODO:  need to check and remove in future.
   ================================================================*/
  if((g_percentile_mode == PERCENTILE_MODE_INTERVAL) && g_percentile_report && 
     (total_pdf_data_size > PDF_DATA_HEADER_SIZE) && parent_cur_count && parent_next_count)
  {
    if (parent_cur_count) {   /*  what is both are non zero?? */
      parent_pdf_data = parent_pdf_addr;
    } else /* (parent_next_count) */ {
      parent_pdf_data = parent_pdf_next_addr;
    }
    
    //TODO: Remove process_last_sample_array and use bitmask 
    //check if bitmask is 1 to indicate report did not come 
    for (i = 0; i < global_settings->num_process; i++)
    {
      if(process_last_sample_array && (process_last_sample_array[i] == 0))
      {
        c_pdf_hdr = (pdf_data_hdr *)(pdf_shm_info[i].addr[0]);
        child_pdf_data= c_pdf_hdr->active ? pdf_shm_info[i].addr[0] : pdf_shm_info[i].addr[1];
        // If any data left for child then copy into parent memory
        add_pdf_data(parent_pdf_data, child_pdf_data, pdf_shm_info[i].total_tx, i);
        copied_flag = 1;
      }
    }
 
    // Dump and send send message to Controller
    if(copied_flag)
    {
      NSDL3_GDF(NULL, NULL, "dumping end and out of sync for data connection.");
      p_pdf_hdr = (pdf_data_hdr *)parent_pdf_addr;
      if(parent_next_count)
      {
        g_percentile_sample++;
        p_pdf_hdr->sequence = g_percentile_sample;
        g_percentile_sample++;
      }
      else
      {
        p_pdf_hdr->sequence = g_percentile_sample;
        g_percentile_sample++;
      }
      dump_pdf_shrmem_to_file(parent_pdf_addr, 1);
    }
  }
}
