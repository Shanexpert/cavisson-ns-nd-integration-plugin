#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <regex.h>
#include <complex.h>
#undef I

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "smon.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_msg_com_util.h"
#include "output.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"
#include "netstorm.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_gdf.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "eth.h"
#include "ns_log.h"
#include "ns_summary_rpt.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_child_msg_com.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "nslb_util.h"
#include "ns_proxy_server_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_trans.h"
#include "ns_websocket_reporting.h"
#include "ns_jmeter.h"
#include "ns_h2_reporting.h"
#include "ns_socket.h"

static char * no_succ_time_string = "min - sec, avg - sec, max - sec";
static char * no_succ_time_string_tx = "min - sec, avg - sec, max - sec, stddev -";
extern void gen_http_status_code_in_progress_report(char *tbuffer, avgtime *avg_time);

char heading[1024];
//static TxData *savedTxData; // moved to trans_util.c
char g_test_start_time[32];

#define SUMMARY_TOP_BUF_SIZE 16384

unsigned long get_test_start_time_from_summary_top()
{
  FILE *fp = NULL;
  char file_name[1024] = {0};
  struct stat s;
  char buffer[SUMMARY_TOP_BUF_SIZE] = {0}; 
  char buff[1024] = {0};
  int num_field = 0;
  char *fields[20];
  struct tm t;
  long start_time = 0;
 
  sprintf(file_name, "%s/logs/TR%d/summary.top", g_ns_wdir, testidx);
  if((stat(file_name, &s) == 0)) 
  {
    if ((fp = fopen(file_name, "r" )) == NULL) {
      fprintf(stderr, "Error in opening %s", file_name);
      return 0;
    }

    if(nslb_fgets(buffer, SUMMARY_TOP_BUF_SIZE, fp, 0) == NULL) {
      fprintf(stderr, "Error in reading summary.top file\n");
      return 0;
    }
    fclose(fp);

    num_field = get_tokens(buffer, fields, "|", 20);
    if(num_field < 3)
      return 0;

   strcpy(buff, fields[2]);
   num_field = get_tokens_with_multi_delimiter(buff, fields, "/ :", 10);
   if(num_field < 6)
      return 0;
   
    // 5/21/14  15:30:47 
    t.tm_isdst = -1;  
    t.tm_mon = atoi(fields[0]) - 1; //tm_mon    The number of months since January, in the range 0 to 11.
    t.tm_mday = atoi(fields[1]);
    t.tm_hour = atoi(fields[3]);
    t.tm_min = atoi(fields[4]);
    t.tm_sec = atoi(fields[5]);
   
    // Taking 100 for (2000 - 1990 = 100) 
    t.tm_year = atoi(fields[2]) + 100;
 
    start_time = (long)mktime(&t); 
    return(start_time);
  }
  else
    return 0;
}

/*************************************************************** 
 * Description      : Function used to update summary.top file, 
 *                    currently calling func from ns_parent.c for 
 *                    tracing getting enable at rtc. 
 * Input Parameters : 
 * field_idx        : Field that need to be updated
 * buf              : Value of the field
 * Output Parameter : None
 * Return           : None      
 *****************************************************************/
void update_summary_top_field(int field_idx, char *value_to_replace, int update_partition_file)
{
  FILE *fp_up_s = NULL;
  char file_name[1024]; // Should be big enough to hold big line in summary.top file
  char buffer[SUMMARY_TOP_BUF_SIZE]; // Should be big enough to hold big line in summary.top file
  char summary_buffer[SUMMARY_TOP_BUF_SIZE]; // Used to rewrite in summary.top file
  char *tmp;
  int buffer_len;
  int count = 0;
  char prev_duration[64];
  unsigned long long total_duration; 
  char *summary_buffer_ptr = summary_buffer; /*Used for writing field*/
  int value_to_replace_len = strlen(value_to_replace);

  NSDL2_REPORTING(NULL, NULL, "Method Called. field_idx = [%d], value_to_replace = [%s], value_to_replace_len = [%d], update_partition_file = [%d]", field_idx, value_to_replace, value_to_replace_len, update_partition_file);

  buffer[0] = '\0'; 
  memset(summary_buffer, 0, SUMMARY_TOP_BUF_SIZE);

  /*Open file in write mode*/
  if(update_partition_file)
    sprintf(file_name, "%s/logs/%s/summary.top", g_ns_wdir, global_settings->tr_or_partition);
  else
    sprintf(file_name, "%s/logs/TR%d/summary.top", g_ns_wdir, testidx);

  /*Error handling*/
  if ((fp_up_s = fopen(file_name, "r" )) == NULL) {
    fprintf(stderr, "Error in opening %s", file_name);
    return;
  }

  if(nslb_fgets(buffer, (SUMMARY_TOP_BUF_SIZE), fp_up_s, 0) == NULL) {
    fprintf(stderr, "Error in reading summary.top file\n");
    return;
  }

  fclose(fp_up_s);

  buffer_len = strlen(buffer);
  if (buffer_len > 0)
    buffer[buffer_len - 1] = '\0'; //Remove new line

  NSDL2_REPORTING(NULL, NULL, "Summary top line = %s", buffer);
  /* Here we need to move buffer till find field to replace.
   * Next replace field with given value*/
  char *org_data_ptr = buffer;

  while(*org_data_ptr != '\0')
  {
    if(*org_data_ptr == '|')
    {
      count++;
    }

    if(count == field_idx && *org_data_ptr == '|') //When count equal to given field index
    {
      *summary_buffer_ptr = *org_data_ptr; //Adding | in dest buff
      summary_buffer_ptr++;
      org_data_ptr++;

      // We are using this for parition mode to add duration in TR level summary.top file
      if(!update_partition_file && field_idx == 14) // TR file (Not partition)
      {
        //strncpy(prev_duration, summary_buffer_ptr, 8);
        strncpy(prev_duration, org_data_ptr, 8);

        total_duration = global_settings->partition_duration + get_time_from_format(prev_duration);

        sprintf(value_to_replace, "%s", (char *)get_time_in_hhmmss((int)(total_duration/1000)));
        value_to_replace_len = strlen(value_to_replace);
        NSDL2_REPORTING(NULL, NULL, "For TR summary.top field_idx = [%d], value_to_replace = [%s], value_to_replace_len = [%d]", field_idx, value_to_replace, value_to_replace_len);
       }
       else
       {
         NSDL2_REPORTING(NULL, NULL, "For partition summary.top field_idx = [%d], value_to_replace = [%s], value_to_replace_len = [%d]", field_idx, value_to_replace, value_to_replace_len);
       }

      NSDL2_REPORTING(NULL, NULL, "COMMON ::: field_idx = [%d], value_to_replace = [%s], value_to_replace_len = [%d]", field_idx, value_to_replace, value_to_replace_len);

      strncpy(summary_buffer_ptr, value_to_replace, value_to_replace_len); //copy given filed value into dest buffer
      summary_buffer_ptr += value_to_replace_len;
      NSDL2_REPORTING(NULL, NULL, "summary_buffer_ptr = %s", summary_buffer_ptr);
      tmp = strchr(org_data_ptr, '|'); 
      if(tmp != NULL)
      {
        strcpy(summary_buffer_ptr, tmp);
        NSDL2_REPORTING(NULL, NULL, "summary_buffer = %s", summary_buffer);
      } 
      //Done with work break from here
      break;
    }
    else
    {
      *summary_buffer_ptr = *org_data_ptr;
      summary_buffer_ptr++;
      org_data_ptr++;
    }
  }

  /*Calculate its length*/
  NSDL2_REPORTING(NULL, NULL, "New buffer created is summary_buffer = %s", summary_buffer);
  /*Replace summary.top line with new data*/
  if ((fp_up_s = fopen(file_name, "w" )) == NULL) {
    fprintf(stderr, "2 Error in opening summary.top\n");
    return;
  }
  fwrite (summary_buffer, strlen(summary_buffer), 1, fp_up_s);
  fwrite ("\n", strlen("\n"), 1, fp_up_s);
  /*Close file pointer*/
  fclose(fp_up_s);
}

static char *get_summary_top_field(int field_idx, char *buf)
{
FILE *fp_in_s;
char buffer[4096*4]; // Should be bug enough to hold big line in summary.top file
int buffer_len;
int total_flds;
char *field[20];

  NSDL2_REPORTING(NULL, NULL, "Method called");

  sprintf(buffer, "logs/%s/summary.top", global_settings->tr_or_partition);
  if ((fp_in_s = fopen(buffer, "r" )) == NULL) {
    strcpy(buf, "SummaryTopFileNotFound");
    fprintf(stderr, "3 Error in opening summary.top (%s)\n", buffer);
    return ("error"); 
  }

  // Read first line only as this file has only one line
  if(nslb_fgets(buffer, (4096*4), fp_in_s, 0) == NULL)
  {
    strcpy(buf, "ErrorReadingSummaryTopFile");
    fprintf(stderr, "Error in reading summary.top file\n");
    return ("error");
  }
 
  fclose(fp_in_s);

  buffer_len = strlen(buffer);
  if (buffer_len > 0)
    buffer[buffer_len - 1] = '\0';

  NSDL2_REPORTING(NULL, NULL, "Summary top line = %s", buffer);
    
  total_flds = get_tokens(buffer, field, "|", 20);

  // field_idx starts with 0
  if(field_idx >= total_flds)
    strcpy(buf, "InvalidFieldIdx");
  else
    strcpy(buf, field[field_idx]); 
  NSDL2_REPORTING(NULL, NULL, "total_flds = %d, field_idx = %d, field_value = %s", total_flds, field_idx, buf);

  return (buf);
} 

/************************************************************************
     Name      :   get_std_dev 
     Purpose   :   Calculate sample standard deviation                    
     Input     :   1. sum_sqr - Sum of square of sample data
                   2. sum_sumple - Sum of the samples
                   3. num_sample - Total number of sample
     Output    :   Standard deviation of sample data
*************************************************************************/
inline double get_std_dev (u_ns_8B_t sum_sqr, u_ns_8B_t sum_sample, double avg_time, u_ns_8B_t num_samples)
{
    double complex nsamples, std_dev, total;
    double std_dev_value;

    NSDL2_REPORTING(NULL, NULL, "Method called");
    NSDL3_TRANS(NULL, NULL, "sum_sqr = %lld, sum_sample = %lld, avg_time = %f, num_samples = %lld", sum_sqr, sum_sample, avg_time, num_samples);

    //variance = (sum_sqr - sum*mean)/(n - 1)  */
    nsamples = (double complex)num_samples;

    /******************************************************************************************************************
      Bug 72574:
      1. We are repleacing sqrt() funtion to csqrt() funtion because sqrt() funtion does not work with negative value. 
      2. csqrt() funtion returns complex number. 
      3. There two types of complex number 
          1. real number 
          2. imagnary number
      4. If value of total is negative csqrt returns imagnary number.
      5. If value of total is positive or zero csqrt returns real number. 
    *******************************************************************************************************************/
    if (num_samples > 1) {
      total = ((((double complex)(sum_sqr)/1000000.0) - (double complex)((double complex)(sum_sample) * avg_time / 1000.0)) / (nsamples-1));
      std_dev = csqrt (total);
    }
    else
      std_dev = 0;
    
    //This is the case of less than zero value
    if((double)total < 0)
    {
      std_dev_value = cimag(std_dev);
      NSDL3_TRANS(NULL, NULL, "std_dev = %lf", std_dev_value);
      return std_dev_value;
    }

  //This is the case of positive or zero value
  std_dev_value = creal(std_dev);
  NSDL3_TRANS(NULL, NULL, "std_dev = %lf", std_dev_value);
  return std_dev_value;
}

void
print_report(FILE *fp1, FILE *fp2, int obj_type, int is_periodic, avgtime *avg, cavgtime *c_avg, char *heading)
{
  double min_time = 0, max_time = 0, avg_time = 0, std_dev;
  u_ns_8B_t num_completed = 0, num_initiated = 0, num_succ = 0, num_samples = 0;
  unsigned int max_err = 0, *err_codes;
  int i, base_code = 0;
  FTPAvgTime *ftp_avg = NULL;
  FTPCAvgTime *ftp_cavg = NULL;
  LDAPAvgTime *ldap_avg = NULL;
  LDAPCAvgTime *ldap_cavg = NULL;
  IMAPAvgTime *imap_avg = NULL;
  IMAPCAvgTime *imap_cavg = NULL;
  JRMIAvgTime *jrmi_avg = NULL;
  JRMICAvgTime *jrmi_cavg = NULL;
  WSAvgTime *ws_avg = NULL;
  //jmeter_avgtime *jmeter_avg = NULL;
  WSCAvgTime *ws_cavg = NULL;

  char *tbuf, tbuffer[4096], tbuffer2[4096];

  double js_proc_min_time, js_proc_max_time, js_proc_avg_time;
  double pg_proc_min_time, pg_proc_max_time, pg_proc_avg_time;
    
    NSDL2_REPORTING(NULL, NULL, "Method called, obj_type = %d, is_periodic = %d, avg = %p, c_avg = %p", obj_type, is_periodic, avg, c_avg);
    switch (obj_type) {
	case URL_REPORT:
	    if (is_periodic) {
	   	num_completed = num_samples = avg->num_tries;
	   	num_succ = avg->num_hits;
	   	num_initiated = avg->fetches_started;
	   	if (num_completed) {
	       	    min_time = (double)(((double)(avg->url_overall_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->url_overall_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->url_overall_tot_time))/((double)(1000.0*(double)num_completed)));
	   	}
	   	err_codes = &(avg->url_error_codes[0]);
	    } else {
	   	num_completed = num_samples = c_avg->url_fetches_completed;
	   	num_succ = c_avg->url_succ_fetches;
	   	num_initiated = c_avg->cum_fetches_started;
	   	if (num_completed) {
	       	    min_time = (double)(((double)(c_avg->c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->c_tot_time))/((double)(1000.0*(double)num_completed)));
	   	}
	   	err_codes = &(c_avg->cum_url_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
	    base_code = 0;
	    break;
	case SMTP_REPORT:
	    if (is_periodic) {
	   	num_completed = avg->smtp_num_tries;
	   	num_samples = num_succ = avg->smtp_num_hits;
	   	num_initiated = avg->smtp_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->smtp_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->smtp_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->smtp_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(avg->smtp_error_codes[0]);
	    } else {
	   	num_completed = c_avg->smtp_fetches_completed;
	   	num_samples = num_succ = c_avg->smtp_succ_fetches;
	   	num_initiated = c_avg->cum_smtp_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->smtp_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->smtp_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->smtp_c_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(c_avg->cum_smtp_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
	    base_code = 0;
	    break;
	case POP3_REPORT:
	    if (is_periodic) {
	   	num_completed = num_samples = avg->pop3_num_tries;
	   	num_succ = avg->pop3_num_hits;
	   	num_initiated = avg->pop3_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->pop3_overall_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->pop3_overall_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->pop3_overall_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(avg->pop3_error_codes[0]);
	    } else {
	   	num_completed = num_samples = c_avg->pop3_fetches_completed;
	   	num_succ = c_avg->pop3_succ_fetches;
	   	num_initiated = c_avg->cum_pop3_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->pop3_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->pop3_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->pop3_c_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(c_avg->cum_pop3_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
	    base_code = 0;
	    break;

	case FTP_REPORT:
          if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
             ftp_avg = (FTPAvgTime*)((char *)avg + g_ftp_avgtime_idx);
             ftp_cavg = (FTPCAvgTime*)((char *)c_avg + g_ftp_cavgtime_idx);
	    if (is_periodic) {
	   	num_completed = num_samples = ftp_avg->ftp_num_tries;
	   	num_succ = ftp_avg->ftp_num_hits;
	   	num_initiated = ftp_avg->ftp_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(ftp_avg->ftp_overall_min_time))/1000.0);
	       	    max_time = (double)(((double)(ftp_avg->ftp_overall_max_time))/1000.0);
	       	    avg_time = (double)(((double)(ftp_avg->ftp_overall_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
	   	}
	   	err_codes = &(ftp_avg->ftp_error_codes[0]);
	    } else {
	   	num_completed = ftp_cavg->ftp_fetches_completed;
	   	num_samples = num_succ = ftp_cavg->ftp_succ_fetches;
	   	num_initiated = ftp_cavg->cum_ftp_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(ftp_cavg->ftp_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(ftp_cavg->ftp_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(ftp_cavg->ftp_c_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of cum min_time is = %d, value of cum max_time is = %d", min_time, max_time); 
	   	}
	   	err_codes = &(ftp_cavg->cum_ftp_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
          }
	  base_code = 0;
	  break;

	case LDAP_REPORT:
          if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
             ldap_avg = (LDAPAvgTime*)((char *)avg + g_ldap_avgtime_idx);
             ldap_cavg = (LDAPCAvgTime*)((char *)c_avg + g_ldap_cavgtime_idx);
	    if (is_periodic) {
	   	num_completed = ldap_avg->ldap_num_tries;
	   	num_samples = num_succ = ldap_avg->ldap_num_hits;
	   	num_initiated = ldap_avg->ldap_fetches_started;
	   	if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, "  ldap_min_time in parent = %llu, ldap_max_time in parent = %llu", ldap_avg->ldap_min_time, ldap_avg->ldap_max_time); 
	       	    min_time = (double)(((double)(ldap_avg->ldap_min_time))/1000.0);
	       	    max_time = (double)(((double)(ldap_avg->ldap_max_time))/1000.0);
	       	    avg_time = (double)(((double)(ldap_avg->ldap_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
	   	}
              
	   	err_codes = &(ldap_avg->ldap_error_codes[0]);
              
	    } else {
	   	num_completed = ldap_cavg->ldap_fetches_completed;
	   	num_samples = num_succ = ldap_cavg->ldap_succ_fetches;
                NSDL2_MESSAGES(NULL, NULL, " the value of num_succ is = %llu, the value of ldap success fetches is = %llu num_completed = %llu",num_succ, ldap_cavg->ldap_succ_fetches, ldap_cavg->ldap_fetches_completed);
	   	num_initiated = ldap_cavg->cum_ldap_fetches_started;
	   	if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, " ldap_c_min_time in parent = %llu, ldap_c_max_time in parent = %llu", ldap_cavg->ldap_c_min_time, ldap_cavg->ldap_c_max_time); 
	       	    min_time = (double)(((double)(ldap_cavg->ldap_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(ldap_cavg->ldap_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(ldap_cavg->ldap_c_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of cum min_time is = %d, value of cum max_time is = %d", min_time, max_time); 
	   	}
	   	err_codes = &(ldap_cavg->cum_ldap_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
          }
	  base_code = 0;
	  break;

	case IMAP_REPORT:
          if((global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
             imap_avg = (IMAPAvgTime*)((char *)avg + g_imap_avgtime_idx);
             imap_cavg = (IMAPCAvgTime*)((char *)c_avg + g_imap_cavgtime_idx);
	    if (is_periodic) {
	   	num_completed = num_samples = imap_avg->imap_num_tries;
	   	num_succ = imap_avg->imap_num_hits;
	   	num_initiated = imap_avg->imap_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(imap_avg->imap_overall_min_time))/1000.0);
	       	    max_time = (double)(((double)(imap_avg->imap_overall_max_time))/1000.0);
	       	    avg_time = (double)(((double)(imap_avg->imap_overall_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
	   	}
	   	err_codes = &(imap_avg->imap_error_codes[0]);
	    } else {
	   	num_completed = num_samples = imap_cavg->imap_fetches_completed;
	   	num_succ = imap_cavg->imap_succ_fetches;
	   	num_initiated = imap_cavg->cum_imap_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(imap_cavg->imap_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(imap_cavg->imap_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(imap_cavg->imap_c_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of cum min_time is = %d, value of cum max_time is = %d", min_time, max_time); 
	   	}
	   	err_codes = &(imap_cavg->cum_imap_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
          }
	  base_code = 0;
	  break;

	case JRMI_REPORT:
          if((global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED)) {
             jrmi_avg = (JRMIAvgTime*)((char *)avg + g_jrmi_avgtime_idx);
             jrmi_cavg = (JRMICAvgTime*)((char *)c_avg + g_jrmi_cavgtime_idx);
	    if (is_periodic) {
	   	num_completed = jrmi_avg->jrmi_num_tries;
	   	num_samples = num_succ = jrmi_avg->jrmi_num_hits;
	   	num_initiated = jrmi_avg->jrmi_fetches_started;
	   	if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, "  jrmi_min_time in parent = %llu, jrmi_max_time in parent = %llu", jrmi_avg->jrmi_min_time, jrmi_avg->jrmi_max_time); 
	       	    min_time = (double)(((double)(jrmi_avg->jrmi_min_time))/1000.0);
	       	    max_time = (double)(((double)(jrmi_avg->jrmi_max_time))/1000.0);
	       	    avg_time = (double)(((double)(jrmi_avg->jrmi_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
	   	}
              
	   	err_codes = &(jrmi_avg->jrmi_error_codes[0]);
              
	    } else {
	   	num_completed = jrmi_cavg->jrmi_fetches_completed;
	   	num_samples = num_succ = jrmi_cavg->jrmi_succ_fetches;
                NSDL2_MESSAGES(NULL, NULL, " the value of num_succ is = %llu, the value of jrmi success fetches is = %llu num_completed = %llu",num_succ, jrmi_cavg->jrmi_succ_fetches, jrmi_cavg->jrmi_fetches_completed);
	   	num_initiated = jrmi_cavg->cum_jrmi_fetches_started;
	   	if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, " jrmi_c_min_time in parent = %llu, jrmi_c_max_time in parent = %llu", jrmi_cavg->jrmi_c_min_time, jrmi_cavg->jrmi_c_max_time); 
	       	    min_time = (double)(((double)(jrmi_cavg->jrmi_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(jrmi_cavg->jrmi_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(jrmi_cavg->jrmi_c_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of cum min_time is = %d, value of cum max_time is = %d", min_time, max_time); 
	   	}
	   	err_codes = &(jrmi_cavg->cum_jrmi_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
          }
	  base_code = 0;
	  break;

	case DNS_REPORT:
	    if (is_periodic) {
	   	num_completed = num_samples = avg->dns_num_tries;
	   	num_succ = avg->dns_num_hits;
	   	num_initiated = avg->dns_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->dns_overall_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->dns_overall_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->dns_overall_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(avg->dns_error_codes[0]);
	    } else {
	   	num_completed = num_samples = c_avg->dns_fetches_completed;
	   	num_succ = c_avg->dns_succ_fetches;
	   	num_initiated = c_avg->cum_dns_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->dns_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->dns_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->dns_c_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(c_avg->cum_dns_error_codes[0]);
	    }
	    max_err = TOTAL_URL_ERR;
	    base_code = 0;
	    break;
       case WS_REPORT:
          if((global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)) {
             ws_avg = (WSAvgTime*)((char *)avg + g_ws_avgtime_idx);
             ws_cavg = (WSCAvgTime*)((char *)c_avg + g_ws_cavgtime_idx);
             NSDL2_MESSAGES(NULL, NULL, "is_periodic = %d, num_samples = %llu", 
                                         is_periodic, ws_avg->ws_num_hits);
           if (is_periodic) {
               //num_completed = ws_avg->ws_num_tries;
               num_samples = num_succ = ws_avg->ws_num_hits;
               //num_initiated = ws_avg->ws_fetches_started;
               if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, "ws_min_time in parent = %llu, ws_max_time in parent = %llu", ws_avg->ws_min_time, ws_avg->ws_max_time); 
                   min_time = (double)(((double)(ws_avg->ws_min_time))/1000.0);
                   max_time = (double)(((double)(ws_avg->ws_max_time))/1000.0);
                   avg_time = (double)(((double)(ws_avg->ws_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
               }
              
               err_codes = &(ws_avg->ws_error_codes[0]);
              
           } else {
               //num_completed = ws_cavg->ws_fetches_completed;
               num_samples = num_succ = ws_cavg->ws_succ_fetches;
                NSDL2_MESSAGES(NULL, NULL, "the value of num_succ is = %llu, the value of ws success fetches is = %llu ",num_succ, ws_cavg->ws_succ_fetches);
               //num_initiated = ws_cavg->cum_ws_fetches_started;
               if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, "ws_c_min_time in parent = %llu, ws_c_max_time in parent = %llu", ws_cavg->ws_c_min_time, ws_cavg->ws_c_max_time); 
                   min_time = (double)(((double)(ws_cavg->ws_c_min_time))/1000.0);
                   max_time = (double)(((double)(ws_cavg->ws_c_max_time))/1000.0);
                   avg_time = (double)(((double)(ws_cavg->ws_c_tot_time))/((double)(1000.0*(double)num_samples)));
                    NSDL2_MESSAGES(NULL, NULL, "value of cum min_time is = %d, value of cum max_time is = %d", min_time, max_time); 
               }
               err_codes = &(ws_cavg->cum_ws_error_codes[0]);
           }
           max_err = TOTAL_URL_ERR;
          }
         base_code = 0;
         break;
       #if 0
       case JMETER_REPORT:
          NSDL2_MESSAGES(NULL, NULL, "JMETER ENABLED = %d", 
                                      global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED);
          if((global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)) {
             jmeter_avg = (jmeter_avgtime*)((char *)avg + g_jmeter_avgtime_idx);
           if (is_periodic) {
               //num_completed = ws_avg->ws_num_tries;
               num_samples = num_succ = jmeter_avg->jm_num_hits;
               //num_initiated = ws_avg->ws_fetches_started;
               if (num_samples) {
                    NSDL2_MESSAGES(NULL, NULL, "jm_min_time in parent = %llu, jm_max_time in parent = %llu", 
                                                jmeter_avg->jm_min_time, jmeter_avg->jm_max_time); 

                   min_time = (double)(((double)(jmeter_avg->jm_min_time))/1000.0);
                   max_time = (double)(((double)(jmeter_avg->jm_max_time))/1000.0);
                   avg_time = (double)(((double)(jmeter_avg->jm_tot_time))/((double)(1000.0*(double)num_samples)));
                   NSDL2_MESSAGES(NULL, NULL, "value of min_time is = %f, value of max_time is = %f", min_time, max_time); 
               }
               //err_codes = &(ws_avg->ws_error_codes[0]);
              
           } 
         }
         base_code = 0;
         break;
       #endif 
       case TCP_CLIENT_REPORT:
         if((global_settings->protocol_enabled & IS_TCP_CLIENT_API_EXIST)) 
         {
           SocketClientAvgTime *avg_ptr = (SocketClientAvgTime *)((char *)avg + g_tcp_client_avg_idx);
           //SocketClientCAvgTime *cavg_ptr = (SocketClientCAvgTime *)((char *)c_avg + g_tcp_client_cavg_idx);

           NSDL2_MESSAGES(NULL, NULL, "is_periodic = %d, num_samples = %llu", is_periodic, avg_ptr->num_send);

           if (is_periodic) 
           {
             num_samples = num_succ = avg_ptr->recv_time.count;
             if (num_samples) 
             {
               min_time = (double)(((double)(avg_ptr->recv_time.min))/1000.0);
               max_time = (double)(((double)(avg_ptr->recv_time.max))/1000.0);
               avg_time = (double)(((double)(avg_ptr->recv_time.tot))/((double)(1000.0*(double)num_samples)));

               NSDL2_MESSAGES(NULL, NULL, "[SocketStats-Report] avg_time = %f, min_time = %f, max_time = %f", 
                   avg_time, min_time, max_time); 
             }
           }
         }
         base_code = 0;
         break;
	case PAGE_REPORT:
	    if (is_periodic) {
	   	num_completed = avg->pg_tries;
	   	num_succ = avg->pg_hits;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = avg->pg_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->pg_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->pg_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->pg_tot_time))/((double)(1000.0*(double)num_samples)));
                    /*JS process time*/
	       	    js_proc_min_time = (double)(((double)(avg->page_js_proc_time_min))/1000.0);
	       	    js_proc_max_time = (double)(((double)(avg->page_js_proc_time_max))/1000.0);
                    //fprintf(stderr, "page_js_proc_time_tot = %lu\n", (avg->page_js_proc_time_tot));
	       	    js_proc_avg_time = (double)(((double)(avg->page_js_proc_time_tot))/((double)(1000.0*(double)num_samples)));
                    /*Page process time*/
	       	    pg_proc_min_time = (double)(((double)(avg->page_proc_time_min))/1000.0);
	       	    pg_proc_max_time = (double)(((double)(avg->page_proc_time_max))/1000.0);
                    //fprintf(stderr, "page_proc_time_tot = %lu\n", (avg->page_proc_time_tot));
	       	    pg_proc_avg_time = (double)(((double)(avg->page_proc_time_tot))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(avg->pg_error_codes[0]);
	    } else {
	   	num_completed = c_avg->pg_fetches_completed;
	   	num_succ = c_avg->pg_succ_fetches;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = c_avg->cum_pg_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->pg_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->pg_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->pg_c_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(c_avg->cum_pg_error_codes[0]);
	    }
	    max_err = TOTAL_PAGE_ERR;
	    base_code = pg_error_code_start_idx;
	    break;
	case TX_REPORT:
	    if (is_periodic) {
	   	num_completed = avg->tx_fetches_completed;
	   	num_succ = avg->tx_succ_fetches;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = avg->tx_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->tx_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->tx_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->tx_tot_time))/((double)(1000.0*(double)num_samples)));
		    std_dev = get_std_dev (avg->tx_tot_sqr_time, avg->tx_tot_time, avg_time, num_samples);
	   	}
	   	err_codes = &(avg->tx_error_codes[0]);
	    } else {
	   	num_completed = c_avg->tx_c_fetches_completed;
	   	num_succ = c_avg->tx_c_succ_fetches;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = c_avg->tx_c_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->tx_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->tx_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->tx_c_tot_time))/((double)(1000.0*(double)num_samples)));
		    std_dev = get_std_dev (c_avg->tx_c_tot_sqr_time, c_avg->tx_c_tot_time, avg_time, num_samples);
	   	}
	   	err_codes = &(c_avg->cum_tx_error_codes[0]);
	    }
	    max_err = TOTAL_TX_ERR;
	    base_code = tx_error_code_start_idx;
	    break;

	case SESS_REPORT:
	    if (is_periodic) {
	   	num_completed = avg->sess_tries;
	   	num_succ = avg->sess_hits;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = avg->ss_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(avg->sess_min_time))/1000.0);
	       	    max_time = (double)(((double)(avg->sess_max_time))/1000.0);
	       	    avg_time = (double)(((double)(avg->sess_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(avg->sess_error_codes[0]);
	    } else {
	   	num_completed = c_avg->sess_fetches_completed;
	   	num_succ = c_avg->sess_succ_fetches;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	   	num_initiated = c_avg->cum_ss_fetches_started;
	   	if (num_samples) {
	       	    min_time = (double)(((double)(c_avg->sess_c_min_time))/1000.0);
	       	    max_time = (double)(((double)(c_avg->sess_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(c_avg->sess_c_tot_time))/((double)(1000.0*(double)num_samples)));
	   	}
	   	err_codes = &(c_avg->cum_sess_error_codes[0]);
	    }
	    max_err = TOTAL_SESS_ERR;
	    base_code = sess_error_code_start_idx;
	    break;
	default:
	   printf("Output requested for Undefined Object type: %d \n", obj_type);
    }

    if (num_samples) {
	if (obj_type == TX_REPORT) {
	    sprintf(tbuffer, "min %'6.3f sec, avg %'6.3f sec, max %'6.3f sec, stddev %'6.3f sec", min_time, avg_time, max_time, std_dev);
	} else{
 	  sprintf(tbuffer, "min %'6.3f sec, avg %'6.3f sec, max %'6.3f sec", min_time, avg_time, max_time);
        }
	tbuf = tbuffer;
    } else {
	if (obj_type == TX_REPORT)
          tbuf = no_succ_time_string_tx;
        else
          tbuf = no_succ_time_string;
    }

    if ((num_completed || ((global_settings->show_initiated) && num_initiated && !WS_REPORT))) {
      if (global_settings->show_initiated) {
        fprint2f(fp1, fp2,"%s:  %s    TOT: initiated %'llu/completed %'llu/succ %'llu", heading, tbuf, num_initiated, num_completed, num_succ);
      } else
      {
        fprint2f(fp1, fp2, "%s:  %s    TOT: completed %'llu/succ %'llu", heading, tbuf, num_completed, num_succ);
      }

      for (i = 1; i < max_err; i++) {
        if (err_codes[i]) fprint2f(fp1, fp2, "/%s: %'lu", errorcode_table_shr_mem[base_code + i].error_msg, err_codes[i]);
      }

      if(obj_type == PAGE_REPORT && is_periodic)/*Came here for writting Page releated data*/
      {
        sprintf(tbuffer2, "\n    Page Proc Time(Sec) Total:   min %'6.3f sec, avg %'6.3f sec, max %'6.3f sec JS: min %'6.3f sec, avg %'6.3f sec, max %'6.3f sec", pg_proc_min_time, pg_proc_avg_time, pg_proc_max_time, js_proc_min_time, js_proc_avg_time, js_proc_max_time);
        fprint2f(fp1, fp2, "%s", tbuffer2);
      }

      fprint2f(fp1, fp2, "\n");
    }
}

void
print_vuserinfo(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{
char sessrate[1024]="";
    NSDL2_REPORTING(NULL, NULL, "Method called");
    if (!is_periodic) {
	//Session rate for end report
	sprintf(sessrate, " Avg. Session Rate (Per Minute) %6.2f,",
		    (float) ((double)(cavg->sess_fetches_completed * 60000)/ (double)(global_settings->test_duration)));
    }
  
    fprint2f(fp1, fp2, "    Vusers: Avg. %'d,%s Current: Active %'d, Thinking %'d, Waiting(Between Sessions) %'d, Idling %'d, SyncPoint %'d"                       ", Blocked %'d, Paused %'d \n",
		    avg->running_users, sessrate,
		    avg->cur_vusers_active,
		    avg->cur_vusers_thinking,
		    avg->cur_vusers_waiting,
		    avg->cur_vusers_cleanup,
                    avg->cur_vusers_in_sp,
                    avg->cur_vusers_blocked,
                    avg->cur_vusers_paused);

}

void
print_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{
  double duration;
  double tcp_rx, tcp_tx;
  double smtp_tcp_rx, smtp_tcp_tx;
  double pop3_tcp_rx, pop3_tcp_tx;
  double dns_tcp_rx, dns_tcp_tx;
  //double ws_tcp_rx, ws_tcp_tx;
  u_ns_8B_t  tot_tcp_rx = 0, tot_tcp_tx = 0;
  u_ns_8B_t  smtp_tot_tcp_rx = 0, smtp_tot_tcp_tx = 0;
  u_ns_8B_t  pop3_tot_tcp_rx = 0, pop3_tot_tcp_tx = 0;
  u_ns_8B_t  dns_tot_tcp_rx = 0, dns_tot_tcp_tx = 0;
  //u_ns_8B_t  ws_tot_tcp_rx = 0, ws_tot_tcp_tx = 0;
  double ssl_new, ssl_reused, ssl_reuse_attempted;
  u_ns_8B_t con_made_rate, con_break_rate, hit_tot_rate, hit_succ_rate, hit_initited_rate;
  u_ns_8B_t smtp_con_made_rate, smtp_con_break_rate, smtp_hit_tot_rate, smtp_hit_succ_rate, smtp_hit_initited_rate;
  u_ns_8B_t pop3_con_made_rate, pop3_con_break_rate, pop3_hit_tot_rate, pop3_hit_succ_rate, pop3_hit_initited_rate;
  u_ns_8B_t dns_con_made_rate, dns_con_break_rate, dns_hit_tot_rate, dns_hit_succ_rate, dns_hit_initited_rate;
  //u_ns_8B_t ws_con_made_rate, ws_con_break_rate, ws_hit_tot_rate, ws_hit_succ_rate, ws_hit_initited_rate;
  u_ns_8B_t num_completed, num_initiated, num_succ; //, num_samples;
  u_ns_8B_t smtp_num_completed, smtp_num_initiated, smtp_num_succ; //, smtp_num_samples;
  u_ns_8B_t pop3_num_completed, pop3_num_initiated, pop3_num_succ; //, pop3_num_samples;
  u_ns_8B_t dns_num_completed, dns_num_initiated, dns_num_succ; //, dns_num_samples;
  //u_ns_8B_t ws_num_completed, ws_num_initiated, ws_num_succ; //, ws_num_samples;

  char tbuffer[1024]="";
  char smtp_tbuffer[1024]="";
  char pop3_tbuffer[1024]="";
  char dns_tbuffer[1024]="";
  char ubuffer[1024]="";
  //char ws_tbuffer[1024]="";

    NSDL2_REPORTING(NULL, NULL, "Method called, is_periodic = %d", is_periodic);
    if (is_periodic) {
	duration = (double)((double)(global_settings->progress_secs)/1000.0);
	//Following should realy for #ifdef RMI_MODE
	sprintf(tbuffer, " HTTP Body(Rx=%'.3f)", (double)(((double)(avg->total_bytes))/(duration*128.0)));
#if 0
	eth_rx = (double) ((double)get_eth_rx_kbps(0, global_settings->progress_secs)/1000.0);
	eth_tx = (double) ((double)get_eth_tx_kbps(0, global_settings->progress_secs)/1000.0);
	//eth_tx = get_eth_tx_kbps(0, global_settings->progress_secs);
	pkt_rx = (double) get_eth_rx_pps(0, global_settings->progress_secs);
	pkt_tx = (double) get_eth_tx_pps(0, global_settings->progress_secs);
#endif
	tcp_rx = ((double)(avg->rx_bytes))/(duration*128.0);
	tcp_tx = ((double)(avg->tx_bytes))/(duration*128.0);

	con_made_rate = (avg->num_con_succ * 1000)/global_settings->progress_secs;
	con_break_rate = (avg->num_con_break * 1000)/global_settings->progress_secs;
	ssl_new = ((double)(avg->ssl_new))/(duration);
	ssl_reused = ((double)(avg->ssl_reused))/(duration);
	ssl_reuse_attempted = ((double)(avg->ssl_reuse_attempted))/(duration);
	num_completed = avg->num_tries;
	/*num_samples = */num_succ = avg->num_hits;
	num_initiated = avg->fetches_started;
	hit_tot_rate = (num_completed * 1000)/global_settings->progress_secs;
	hit_succ_rate = (num_succ * 1000)/global_settings->progress_secs;
	hit_initited_rate = (num_initiated * 1000)/global_settings->progress_secs;

        /* for SMTP */
	sprintf(smtp_tbuffer, " SMTP Mail(Rx=%'.3f)", (double)(((double)(avg->smtp_total_bytes))/(duration*128.0)));
	smtp_tcp_rx = ((double)(avg->smtp_rx_bytes))/(duration*128.0);
        smtp_tcp_tx = ((double)(avg->smtp_tx_bytes))/(duration*128.0);

	smtp_con_made_rate = (avg->smtp_num_con_succ * 1000)/global_settings->progress_secs;
	smtp_con_break_rate = (avg->smtp_num_con_break * 1000)/global_settings->progress_secs;
	smtp_num_completed = avg->smtp_num_tries;
	/*smtp_num_samples = */smtp_num_succ = avg->smtp_num_hits;
	smtp_num_initiated = avg->smtp_fetches_started;
	smtp_hit_tot_rate = (smtp_num_completed * 1000)/global_settings->progress_secs;
	smtp_hit_succ_rate = (smtp_num_succ * 1000)/global_settings->progress_secs;
	smtp_hit_initited_rate = (smtp_num_initiated * 1000)/global_settings->progress_secs;

        /* for pop3 */
	sprintf(pop3_tbuffer, " POP3 Mail(Rx=%'.3f)", (double)(((double)(avg->pop3_total_bytes))/(duration*128.0)));
	pop3_tcp_rx = ((double)(avg->pop3_rx_bytes))/(duration*128.0);
        pop3_tcp_tx = ((double)(avg->pop3_tx_bytes))/(duration*128.0);

	pop3_con_made_rate = (avg->pop3_num_con_succ * 1000)/global_settings->progress_secs;
	pop3_con_break_rate = (avg->pop3_num_con_break * 1000)/global_settings->progress_secs;
	pop3_num_completed = avg->pop3_num_tries;
	/*pop3_num_samples = */pop3_num_succ = avg->pop3_num_hits;
	pop3_num_initiated = avg->pop3_fetches_started;
	pop3_hit_tot_rate = (pop3_num_completed * 1000)/global_settings->progress_secs;
	pop3_hit_succ_rate = (pop3_num_succ * 1000)/global_settings->progress_secs;
	pop3_hit_initited_rate = (pop3_num_initiated * 1000)/global_settings->progress_secs;


        /* DNS */
	sprintf(dns_tbuffer, " DNS (Rx=%'.3f)", (double)(((double)(avg->dns_total_bytes))/(duration*128.0)));
	dns_tcp_rx = ((double)(avg->dns_rx_bytes))/(duration*128.0);
        dns_tcp_tx = ((double)(avg->dns_tx_bytes))/(duration*128.0);

	dns_con_made_rate = (avg->dns_num_con_succ * 1000)/global_settings->progress_secs;
	dns_con_break_rate = (avg->dns_num_con_break * 1000)/global_settings->progress_secs;
	dns_num_completed = avg->dns_num_tries;
	/*dns_num_samples = */dns_num_succ = avg->dns_num_hits;
	dns_num_initiated = avg->dns_fetches_started;
	dns_hit_tot_rate = (dns_num_completed * 1000)/global_settings->progress_secs;
	dns_hit_succ_rate = (dns_num_succ * 1000)/global_settings->progress_secs;
	dns_hit_initited_rate = (dns_num_initiated * 1000)/global_settings->progress_secs;
 
    } else {
	duration = (double)((double)(global_settings->test_duration)/1000.0);
	//Following should realy for #ifdef RMI_MODE
	sprintf(tbuffer, " HTTP Body(Rx=%'.3f)", (double)(((double)(cavg->c_tot_total_bytes))/(duration*128.0)));
#if 0
	eth_rx = (double) get_eth_rx_kbps(1, global_settings->test_duration);
	//eth_rx = get_eth_rx_kbps(1, global_settings->test_duration);
	pkt_rx = (double) get_eth_rx_pps(1, global_settings->test_duration);
	pkt_tx = (double) get_eth_tx_pps(1, global_settings->test_duration);
#endif
	tcp_rx = ((double)(cavg->c_tot_rx_bytes))/(duration*128.0);
	tcp_tx = ((double)(cavg->c_tot_tx_bytes))/(duration*128.0);

	con_made_rate = (cavg->c_num_con_succ * 1000)/global_settings->test_duration;
	con_break_rate = (cavg->c_num_con_break * 1000)/global_settings->test_duration;
	ssl_new = ((double)(cavg->c_ssl_new))/(duration);
	ssl_reused = ((double)(cavg->c_ssl_reused))/(duration);
	ssl_reuse_attempted = ((double)(cavg->c_ssl_reuse_attempted))/(duration);
	num_completed = cavg->url_fetches_completed;
	/*num_samples = */num_succ = cavg->url_succ_fetches;
	num_initiated = cavg->cum_fetches_started;
	hit_tot_rate = (num_completed * 1000)/global_settings->test_duration;
	hit_succ_rate = (num_succ * 1000)/global_settings->test_duration;
	hit_initited_rate = (num_initiated * 1000)/global_settings->test_duration;

        /* SMTP */
	sprintf(smtp_tbuffer, " SMTP Mail(Rx=%'.3f)", (double)(((double)(cavg->smtp_c_tot_total_bytes))/(duration*128.0)));
	smtp_tcp_rx = ((double)(cavg->smtp_c_tot_rx_bytes))/(duration*128.0);
	smtp_tcp_tx = ((double)(cavg->smtp_c_tot_tx_bytes))/(duration*128.0);

	smtp_con_made_rate = (cavg->smtp_c_num_con_succ * 1000)/global_settings->test_duration;
	smtp_con_break_rate = (cavg->smtp_c_num_con_break * 1000)/global_settings->test_duration;
	smtp_num_completed = cavg->smtp_fetches_completed;
	/*smtp_num_samples = */smtp_num_succ = cavg->smtp_succ_fetches;
	smtp_num_initiated = cavg->cum_smtp_fetches_started;
	smtp_hit_tot_rate = (smtp_num_completed * 1000)/global_settings->test_duration;
	smtp_hit_succ_rate = (smtp_num_succ * 1000)/global_settings->test_duration;
	smtp_hit_initited_rate = (smtp_num_initiated * 1000)/global_settings->test_duration;

        /* POP3 */
	sprintf(pop3_tbuffer, " POP3 Mail(Rx=%'.3f)", (double)(((double)(cavg->pop3_c_tot_total_bytes))/(duration*128.0)));
	pop3_tcp_rx = ((double)(cavg->pop3_c_tot_rx_bytes))/(duration*128.0);
	pop3_tcp_tx = ((double)(cavg->pop3_c_tot_tx_bytes))/(duration*128.0);

	pop3_con_made_rate = (cavg->pop3_c_num_con_succ * 1000)/global_settings->test_duration;
	pop3_con_break_rate = (cavg->pop3_c_num_con_break * 1000)/global_settings->test_duration;
	pop3_num_completed = cavg->pop3_fetches_completed;
	/*pop3_num_samples = */pop3_num_succ = cavg->pop3_succ_fetches;
	pop3_num_initiated = cavg->cum_pop3_fetches_started;
	pop3_hit_tot_rate = (pop3_num_completed * 1000)/global_settings->test_duration;
	pop3_hit_succ_rate = (pop3_num_succ * 1000)/global_settings->test_duration;
	pop3_hit_initited_rate = (pop3_num_initiated * 1000)/global_settings->test_duration;


        /* DNS */
	sprintf(dns_tbuffer, " DNS (Rx=%'.3f)", (double)(((double)(cavg->dns_c_tot_total_bytes))/(duration*128.0)));
	dns_tcp_rx = ((double)(cavg->dns_c_tot_rx_bytes))/(duration*128.0);
	dns_tcp_tx = ((double)(cavg->dns_c_tot_tx_bytes))/(duration*128.0);

	dns_con_made_rate = (cavg->dns_c_num_con_succ * 1000)/global_settings->test_duration;
	dns_con_break_rate = (cavg->dns_c_num_con_break * 1000)/global_settings->test_duration;
	dns_num_completed = cavg->dns_fetches_completed;
	/*dns_num_samples = */dns_num_succ = cavg->dns_succ_fetches;
	dns_num_initiated = cavg->cum_dns_fetches_started;
	dns_hit_tot_rate = (dns_num_completed * 1000)/global_settings->test_duration;
	dns_hit_succ_rate = (dns_num_succ * 1000)/global_settings->test_duration;
	dns_hit_initited_rate = (dns_num_initiated * 1000)/global_settings->test_duration;
    }

    tot_tcp_rx = (u_ns_8B_t)cavg->c_tot_rx_bytes;
    tot_tcp_tx = (u_ns_8B_t)cavg->c_tot_tx_bytes;

    /* SMTP */
    smtp_tot_tcp_rx = (u_ns_8B_t)cavg->smtp_c_tot_rx_bytes;
    smtp_tot_tcp_tx = (u_ns_8B_t)cavg->smtp_c_tot_tx_bytes;

    /* POP3 */
    pop3_tot_tcp_rx = (u_ns_8B_t)cavg->pop3_c_tot_rx_bytes;
    pop3_tot_tcp_tx = (u_ns_8B_t)cavg->pop3_c_tot_tx_bytes;


    /* dns */
    dns_tot_tcp_rx = (u_ns_8B_t)cavg->dns_c_tot_rx_bytes;
    dns_tot_tcp_tx = (u_ns_8B_t)cavg->dns_c_tot_tx_bytes;

    //    printf("XXXX %llu/%llu | %llu/%llu\n", avg->c_tot_rx_bytes, avg->c_tot_tx_bytes, avg->rx_bytes, avg->tx_bytes);

    /*
Throughput (Kbits/s): Eth(Rx=119.235 Tx=103.024) TCP(Rx=97.344 Tx=64.943) HTTP Body(Rx=85.000)
Throughput (Total Bytes): Eth(Rx=1234567890 Tx=1234567890) TCP(Rx=1234567890 Tx=1234567890) HTTP Body(Rx=1234567890)
TCP Conns: Rate/s(Open=123456 Close=123456) Total(Open=1234567890 Close=1234567890)
SSL Sess: Rate/s(New=0.000 Reuse Req=0.000 Reused=0.000) Total(New=1234567890 Reuse Req=1234567890 Reused=1234567890)
Eth Pkts: Rate/s(Rx=123456 Tx=123456) Total(Rx=1234567890 Tx=1234567890)
    */

    sprintf(ubuffer, " HTTP Body(Rx=%'llu)", (u_ns_8B_t)(cavg->c_tot_total_bytes));

    if (avg->num_hits) {
      if (global_settings->show_initiated)
        fprint2f(fp1, fp2, "    HTTP hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
                 hit_initited_rate, hit_tot_rate, hit_succ_rate);
      else
        fprint2f(fp1, fp2, "    HTTP hit rate (per sec): Total=%'llu Success=%'llu\n", hit_tot_rate, hit_succ_rate);

      fprint2f(fp1, fp2, "    HTTP (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)%s\n", tcp_rx, tcp_tx, tbuffer);
      fprint2f(fp1, fp2, "    HTTP (Total Bytes) TCP(Rx=%'llu Tx=%'llu)%s\n", 
               tot_tcp_rx, tot_tcp_tx, ubuffer);
 
      fprint2f(fp1, fp2, "    HTTP TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
               avg->num_connections,
               con_made_rate,
               con_break_rate,
               cavg->c_num_con_succ,
               cavg->c_num_con_break);
      
      fprint2f(fp1, fp2, "    SSL Sessions: Rate/s(New=%'.3f Reuse Req=%'.3f Reused=%'.3f) Total(New=%'llu Reuse Req=%'llu Reused=%'d)\n",
               ssl_new,
               ssl_reuse_attempted,
               ssl_reused, 
               cavg->c_ssl_new,
               cavg->c_ssl_reuse_attempted,
               cavg->c_ssl_reused);

    }
 
    /*Cache data in progress report*/
    cache_print_progress_report(fp1, fp2, is_periodic, avg);

    NSDL2_REPORTING(NULL, NULL, " cavg->cum_srv_pushed_resources= %llu avg->num_srv_push=%llu ", cavg->cum_srv_pushed_resources,avg->num_srv_push);
    /*bug 70480 :print  HTTP2 Server Push data in progress report*/
    if (IS_SERVER_PUSH_ENABLED)
      h2_server_push_print_progress_report(fp1, fp2, cavg);

    /*Proxy data in progress report*/
    if(IS_PROXY_ENABLED)
    {
      proxy_print_progress_report(fp1, fp2, is_periodic, avg);
    }

    if(IS_NETWORK_CACHE_STATS_ENABLED)
    {
      print_nw_cache_stats_progress_report (fp1, fp2, is_periodic, avg); 
    }

    if(IS_DNS_LOOKUP_STATS_ENABLED)
    {
      print_dns_lookup_stats_progress_report (fp1, fp2, is_periodic, avg);
    }

   //Dos data in progress report
    dos_attack_print_progress_report(fp1, fp2, is_periodic, avg);

    if (avg->smtp_num_hits) {
      if (global_settings->show_initiated)
        fprint2f(fp1, fp2, "    SMTP hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
                 smtp_hit_initited_rate, smtp_hit_tot_rate, smtp_hit_succ_rate);
      else
        fprint2f(fp1, fp2, "    SMTP hit rate (per sec): Total=%'llu Success=%'llu\n", smtp_hit_tot_rate, smtp_hit_succ_rate);

      sprintf(ubuffer, " SMTP Body(Rx=%'llu)", 
              (u_ns_8B_t)(cavg->smtp_c_tot_total_bytes));

      fprint2f(fp1, fp2, "    SMTP (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n", 
               smtp_tcp_rx, smtp_tcp_tx);
      fprint2f(fp1, fp2, "    SMTP (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n", 
               smtp_tot_tcp_rx, smtp_tot_tcp_tx);

      fprint2f(fp1, fp2, "    SMTP TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
               avg->smtp_num_connections,
               smtp_con_made_rate,
               smtp_con_break_rate,
               cavg->smtp_c_num_con_succ,
               cavg->smtp_c_num_con_break);


    }

    if (avg->pop3_num_hits) {
      if (global_settings->show_initiated)
        fprint2f(fp1, fp2, "    pop3 hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
                 pop3_hit_initited_rate, pop3_hit_tot_rate, pop3_hit_succ_rate);
      else
        fprint2f(fp1, fp2, "    pop3 hit rate (per sec): Total=%'llu Success=%'llu\n", pop3_hit_tot_rate, pop3_hit_succ_rate);

      sprintf(ubuffer, " pop3 Body(Rx=%'llu)", 
              (u_ns_8B_t)(cavg->pop3_c_tot_total_bytes));

      fprint2f(fp1, fp2, "    POP3 (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n", 
               pop3_tcp_rx, pop3_tcp_tx);
      fprint2f(fp1, fp2, "    POP3 (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n", 
               pop3_tot_tcp_rx, pop3_tot_tcp_tx);

      fprint2f(fp1, fp2, "    POP3 TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
               avg->pop3_num_connections,
               pop3_con_made_rate,
               pop3_con_break_rate,
               cavg->pop3_c_num_con_succ,
               cavg->pop3_c_num_con_break);


    }

   /*for ftp through put */
    print_ftp_throughput (fp1, fp2, is_periodic, avg, cavg);   
 
    print_tcp_client_throughput(fp1, fp2, is_periodic, avg, cavg);
    print_udp_client_throughput(fp1, fp2, is_periodic, avg, cavg);

   /*for ldap through put */
    print_ldap_throughput (fp1, fp2, is_periodic, avg, cavg);  
 
   /*for imap through put */
    print_imap_throughput (fp1, fp2, is_periodic, avg, cavg); 
 
   /*for jrmi through put */
    print_jrmi_throughput (fp1, fp2, is_periodic, avg, cavg); 

    /* WS */
    NSDL2_WS(NULL, NULL, "Calling for print_ws_throughput, avg = [%p], cavg = [%p], is_periodic = [%d]", avg, cavg, is_periodic);
    print_ws_throughput(fp1, fp2, is_periodic, avg, cavg);

    //print_jm_throughput(fp1, fp2, is_periodic, avg);

    /* dns */
    if (avg->dns_num_hits) {
      if (global_settings->show_initiated)
        fprint2f(fp1, fp2, "    dns hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
                 dns_hit_initited_rate, dns_hit_tot_rate, dns_hit_succ_rate);
      else
        fprint2f(fp1, fp2, "    dns hit rate (per sec): Total=%'llu Success=%'llu\n", dns_hit_tot_rate, dns_hit_succ_rate);

/*       sprintf(ubuffer, " dns Body(Rx=%'llu)",  */
/*               (u_ns_8B_t)(avg->dns_c_tot_total_bytes)); */

      fprint2f(fp1, fp2, "    dns (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n", 
               dns_tcp_rx, dns_tcp_tx);
      fprint2f(fp1, fp2, "    dns (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n", 
               dns_tot_tcp_rx, dns_tot_tcp_tx);

      fprint2f(fp1, fp2, "    dns TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
               avg->dns_num_connections,
               dns_con_made_rate,
               dns_con_break_rate,
               cavg->dns_c_num_con_succ,
               cavg->dns_c_num_con_break);
    }

}

inline int
get_byte_offset (char *p1, char *p2)
{
int offset;
    	NSDL2_REPORTING(NULL, NULL, "Method called");
	offset = p2-p1;
	//printf ("offset is %d\n", offset);
	return offset;
}

//Following log data is used for comparing test results
//NC: In release 3.9.3, TxData will be passed from deliver_reports for controller and standalone test
void
log_summary_data (avgtime *avg, cavgtime *cavg, double *url_data, double *smtp_data, double *pop3_data, double *ftp_data, double *ldap_data, double *imap_data, double *jrmi_data, double *dns_data, double *pg_data, double *tx_data, double *ss_data, TxDataCum *savedTxData, double *ws_data)
{
FILE *fp;
char buf[600];
char *tx_name;
double /* min_time, max_time, */ avg_time, std_dev;
u_ns_8B_t num_completed, num_succ, num_samples;
int i, j;
SummaryMinMax *dptr;
double duration, tcp_rx, tcp_tx; //, pkt_rx, pkt_tx;
double ssl_new, ssl_reused, ssl_reuse_attempted;
u_ns_8B_t con_made_rate, con_break_rate;
//u_ns_8B_t num_initiated;
    	NSDL2_REPORTING(NULL, NULL, "Method called");
    // this is used by test suite for checking results. Hence creating in TR directory. 
  	sprintf(buf, "logs/TR%d/summary.data", testidx);
	//printf ("Creating %s file\n", buf);
   	if ((fp = fopen(buf, "w" )) == NULL) {
    	    fprintf(stderr, "log_summary_data: Error in opening summary.data %s\n", buf);
    	    return;
  	}

	//fprintf(fp, "TestNum|Urlmin|Urlavg|Urlmax|UrlMed|Url80|Url90|Url95|Url99|UrlTot|UrlSucc|UrlFail|UrlFailPct|pgmin|pgavg|pgmax|pgMed|pg80|pg90|pg95|pg99|pgTot|pgSucc|pgFail|pgFailPct|txmin|txavg|txmax|txMed|tx80|tx90|tx95|tx99|txTot|txSucc|txFail|txFailPct|ssmin|ssavg|ssmax|ssMed|ss80|ss90|ss95|ss99|ssTot|ssSucc|ssFail|ssFailPct|Avg.Vusers|Avg.SessionRate|EthRx|EthTx|AppRx|AppTx|TName\n");

	//Data Display Oder is within a group Display orde
	fprintf (fp, "Group Display Order|Group Name|Data Display Order|Data Name|Data Value\n");
/*
        if (global_settings->testname[0] == '\0')
		sprintf(global_settings->testname, "%d", testidx);
	fprintf (fp, "10|Identity|10|Test Description|%s\n", global_settings->testname);
*/
        char local_buf[MAX_TNAME_LENGTH + 1];
  
	fprintf (fp, "10|Identity|10|Test Description|%s\n", get_summary_top_field(12, local_buf));

	fprintf (fp, "20|Configuration|10|Test Name|%s\n", g_testrun);
	fprintf (fp, "20|Configuration|20|Start Time|%s\n", g_test_start_time);
	fprintf (fp, "20|Configuration|40|Virtual Users|%d\n", global_settings->num_connections);
	fprintf (fp, "20|Configuration|40|Test Duration (Minutes)|%d\n", (int)(global_settings->test_duration/60000));

	num_completed = cavg->url_fetches_completed;
	num_samples = cavg->url_fetches_completed;
	num_succ = cavg->url_succ_fetches;
	//num_initiated = avg->cum_fetches_started;

 	if (num_completed) {
 	  if (num_samples) {
	    fprintf(fp, "30|Url|10|Url Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->c_min_time))/1000.0));
	    fprintf(fp, "30|Url|20|Url Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->c_tot_time))/((double)(1000.0*(double)num_samples))));
	    fprintf(fp, "30|Url|30|Url Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->c_max_time))/1000.0));
	    fprintf(fp, "30|Url|40|Url Median Time(Sec)|%1.3f\n", url_data[0]);
	    fprintf(fp, "30|Url|50|Url 80th percentile Time(Sec)|%1.3f\n", url_data[1]);
	    fprintf(fp, "30|Url|60|Url 90th percentile Time(Sec)|%1.3f\n", url_data[2]);
	    fprintf(fp, "30|Url|70|Url 95th percentile Time(Sec)|%1.3f\n", url_data[3]);
	    fprintf(fp, "30|Url|80|Url 99th percentile Time(Sec)|%1.3f\n", url_data[4]);
	    fprintf(fp, "30|Url|90|Url Total|%llu\n", num_completed);
	    fprintf(fp, "30|Url|100|Url Success|%llu\n", num_succ);
	    fprintf(fp, "30|Url|110|Url Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "30|Url|120|Url Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  } else {
	    fprintf(fp, "30|Url|10|Url Min Time(Sec)|-\n");
	    fprintf(fp, "30|Url|20|Url Avg Time(Sec)|-\n");
	    fprintf(fp, "30|Url|30|Url Max Time(Sec)|-\n");
	    fprintf(fp, "30|Url|40|Url Median Time(Sec)|-\n");
	    fprintf(fp, "30|Url|50|Url 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|60|Url 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|70|Url 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|80|Url 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|90|Url Total|%llu\n", num_completed);
	    fprintf(fp, "30|Url|100|Url Success|%llu\n", num_succ);
	    fprintf(fp, "30|Url|110|Url Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "30|Url|120|Url Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  }
	} else {
	    fprintf(fp, "30|Url|10|Url Min Time(Sec)|-\n");
	    fprintf(fp, "30|Url|20|Url Avg Time(Sec)|-\n");
	    fprintf(fp, "30|Url|30|Url Max Time(Sec)|-\n");
	    fprintf(fp, "30|Url|40|Url Median Time(Sec)|-\n");
	    fprintf(fp, "30|Url|50|Url 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|60|Url 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|70|Url 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|80|Url 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "30|Url|90|Url Total|0\n");
	    fprintf(fp, "30|Url|100|Url Success|0\n");
	    fprintf(fp, "30|Url|110|Url Failures|0\n");
	    fprintf(fp, "30|Url|120|Url Failure PCT|0.00\n");
	}
	fprintf(fp, "30|Url|140|URL Hits/Sec|%1.2f\n", (float) ((double)(cavg->url_fetches_completed * 1000)/ (double)(global_settings->test_duration)));

	num_completed = cavg->pg_fetches_completed;
	num_succ = cavg->pg_succ_fetches;
	num_samples = num_completed;
	//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	//num_initiated = avg->cum_pg_fetches_started;
 	if (num_completed) {
 	  if (num_samples) {
	    fprintf(fp, "40|Page|10|Page Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->pg_c_min_time))/1000.0));
	    fprintf(fp, "40|Page|20|Page Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->pg_c_tot_time))/((double)(1000.0*(double)num_samples))));
	    fprintf(fp, "40|Page|30|Page Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->pg_c_max_time))/1000.0));
	    fprintf(fp, "40|Page|40|Page Median Time(Sec)|%1.3f\n", pg_data[0]);
	    fprintf(fp, "40|Page|50|Page 80th percentile Time(Sec)|%1.3f\n", pg_data[1]);
	    fprintf(fp, "40|Page|60|Page 90th percentile Time(Sec)|%1.3f\n", pg_data[2]);
	    fprintf(fp, "40|Page|70|Page 95th percentile Time(Sec)|%1.3f\n", pg_data[3]);
	    fprintf(fp, "40|Page|80|Page 99th percentile Time(Sec)|%1.3f\n", pg_data[4]);
	    fprintf(fp, "40|Page|90|Page Total|%llu\n", num_completed);
	    fprintf(fp, "40|Page|100|Page Success|%llu\n", num_succ);
	    fprintf(fp, "40|Page|110|Page Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "40|Page|120|Page Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  } else {
	    fprintf(fp, "40|Page|10|Page Min Time(Sec)|-\n");
	    fprintf(fp, "40|Page|20|Page Avg Time(Sec)|-\n");
	    fprintf(fp, "40|Page|30|Page Max Time(Sec)|-\n");
	    fprintf(fp, "40|Page|40|Page Median Time(Sec)|-\n");
	    fprintf(fp, "40|Page|50|Page 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|60|Page 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|70|Page 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|80|Page 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|90|Page Total|%llu\n", num_completed);
	    fprintf(fp, "40|Page|100|Page Success|%llu\n", num_succ);
	    fprintf(fp, "40|Page|110|Page Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "40|Page|120|Page Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  }
	} else {
	    fprintf(fp, "40|Page|10|Page Min Time(Sec)|-\n");
	    fprintf(fp, "40|Page|20|Page Avg Time(Sec)|-\n");
	    fprintf(fp, "40|Page|30|Page Max Time(Sec)|-\n");
	    fprintf(fp, "40|Page|40|Page Median Time(Sec)|-\n");
	    fprintf(fp, "40|Page|50|Page 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|60|Page 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|70|Page 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|80|Page 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "40|Page|90|Page Total|0\n");
	    fprintf(fp, "40|Page|100|Page Success|0\n");
	    fprintf(fp, "40|Page|110|Page Failures|0\n");
	    fprintf(fp, "40|Page|120|Page Failure PCT|0.00\n");
	}
	fprintf(fp, "40|Page|140|Page Views/Sec|%1.2f\n", (float) ((double)(cavg->pg_fetches_completed * 1000)/ (double)(global_settings->test_duration)));

        if(total_tx_entries) {
	  num_completed = cavg->tx_c_fetches_completed;
	  num_succ = cavg->tx_c_succ_fetches;
	  num_samples = num_completed;
	  //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	  //num_initiated = avg->tx_c_fetches_started;
 	  if (num_completed) {
 	    if (num_samples) {
	      fprintf(fp, "50|Trans.|10|Trans. Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->tx_c_min_time))/1000.0));
	      avg_time =  (double)(((double)(cavg->tx_c_tot_time))/((double)(1000.0*(double)num_samples)));
	      fprintf(fp, "50|Trans.|20|Trans. Avg Time(Sec)|%1.3f\n", avg_time);
	      fprintf(fp, "50|Trans.|30|Trans. Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->tx_c_max_time))/1000.0));
	      std_dev = get_std_dev (cavg->tx_c_tot_sqr_time, cavg->tx_c_tot_time, avg_time, num_samples);
	      fprintf(fp, "50|Trans.|35|Trans. Std Dev(Sec)|%1.3f\n", std_dev);
	      fprintf(fp, "50|Trans.|40|Trans. Median Time(Sec)|%1.3f\n", tx_data[0]);
	      fprintf(fp, "50|Trans.|50|Trans. 80th percentile Time(Sec)|%1.3f\n", tx_data[1]);
	      fprintf(fp, "50|Trans.|60|Trans. 90th percentile Time(Sec)|%1.3f\n", tx_data[2]);
	      fprintf(fp, "50|Trans.|70|Trans. 95th percentile Time(Sec)|%1.3f\n", tx_data[3]);
	      fprintf(fp, "50|Trans.|80|Trans. 99th percentile Time(Sec)|%1.3f\n", tx_data[4]);
	      fprintf(fp, "50|Trans.|90|Trans. Total|%llu\n", num_completed);
	      fprintf(fp, "50|Trans.|100|Trans. Success|%llu\n", num_succ);
	      fprintf(fp, "50|Trans.|110|Trans. Failures|%llu\n", num_completed -  num_succ);
	      fprintf(fp, "50|Trans.|120|Trans. Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	    } else {
	      fprintf(fp, "50|Trans.|10|Trans. Min Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|20|Trans. Avg Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|30|Trans. Max Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|35|Trans. Srd Dev(Sec)|-\n");
	      fprintf(fp, "50|Trans.|40|Trans. Median Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|50|Trans. 80th percentile Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|60|Trans. 90th percentile Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|70|Trans. 95th percentile Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|80|Trans. 99th percentile Time(Sec)|-\n");
	      fprintf(fp, "50|Trans.|90|Trans. Total|%llu\n", num_completed);
	      fprintf(fp, "50|Trans.|100|Trans. Success|%llu\n", num_succ);
	      fprintf(fp, "50|Trans.|110|Trans. Failures|%llu\n", num_completed -  num_succ);
	      fprintf(fp, "50|Trans.|120|Trans. Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	    }
	  } else {
	    fprintf(fp, "50|Trans.|10|Trans. Min Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|20|Trans. Avg Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|30|Trans. Max Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|35|Trans. Std Dev (Sec)|-\n");
	    fprintf(fp, "50|Trans.|40|Trans. Median Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|50|Trans. 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|60|Trans. 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|70|Trans. 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|80|Trans. 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "50|Trans.|90|Trans. Total|0\n");
	    fprintf(fp, "50|Trans.|100|Trans. Success|0\n");
	    fprintf(fp, "50|Trans.|110|Trans. Failures|0\n");
	    fprintf(fp, "50|Trans.|120|Trans. Failure PCT|0.00\n");
	  }
	  fprintf(fp, "50|Trans.|140|Transacions/Sec|%1.2f\n", (float) ((double)(cavg->tx_c_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
        }

	num_completed = cavg->sess_fetches_completed;
	num_succ = cavg->sess_succ_fetches;
	num_samples = num_completed;
	//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
	//num_initiated = avg->cum_ss_fetches_started;
 	if (num_completed) {
 	  if (num_samples) {
	    fprintf(fp, "60|Session|10|Session Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->sess_c_min_time))/1000.0));
	    fprintf(fp, "60|Session|20|Session Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->sess_c_tot_time))/((double)(1000.0*(double)num_samples))));
	    fprintf(fp, "60|Session|30|Session Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->sess_c_max_time))/1000.0));
	    fprintf(fp, "60|Session|40|Session Median Time(Sec)|%1.3f\n", ss_data[0]);
	    fprintf(fp, "60|Session|50|Session 80th percentile Time(Sec)|%1.3f\n", ss_data[1]);
	    fprintf(fp, "60|Session|60|Session 90th percentile Time(Sec)|%1.3f\n", ss_data[2]);
	    fprintf(fp, "60|Session|70|Session 95th percentile Time(Sec)|%1.3f\n", ss_data[3]);
	    fprintf(fp, "60|Session|80|Session 99th percentile Time(Sec)|%1.3f\n", ss_data[4]);
	    fprintf(fp, "60|Session|90|Session Total|%llu\n", num_completed);
	    fprintf(fp, "60|Session|100|Session Success|%llu\n", num_succ);
	    fprintf(fp, "60|Session|110|Session Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "60|Session|120|Session Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  } else {
	    fprintf(fp, "60|Session|10|Session Min Time(Sec)|-\n");
	    fprintf(fp, "60|Session|20|Session Avg Time(Sec)|-\n");
	    fprintf(fp, "60|Session|30|Session Max Time(Sec)|-\n");
	    fprintf(fp, "60|Session|40|Session Median Time(Sec)|-\n");
	    fprintf(fp, "60|Session|50|Session 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|60|Session 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|70|Session 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|80|Session 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|90|Session Total|%llu\n", num_completed);
	    fprintf(fp, "60|Session|100|Session Success|%llu\n", num_succ);
	    fprintf(fp, "60|Session|110|Session Failures|%llu\n", num_completed -  num_succ);
	    fprintf(fp, "60|Session|120|Session Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	  }
	} else {
	    fprintf(fp, "60|Session|10|Session Min Time(Sec)|-\n");
	    fprintf(fp, "60|Session|20|Session Avg Time(Sec)|-\n");
	    fprintf(fp, "60|Session|30|Session Max Time(Sec)|-\n");
	    fprintf(fp, "60|Session|40|Session Median Time(Sec)|-\n");
	    fprintf(fp, "60|Session|50|Session 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|60|Session 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|70|Session 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|80|Session 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "60|Session|90|Session Total|0\n");
	    fprintf(fp, "60|Session|100|Session Success|0\n");
	    fprintf(fp, "60|Session|110|Session Failures|0\n");
	    fprintf(fp, "60|Session|120|Session Failure PCT|0.00\n");
	}


	fprintf(fp, "60|Session|130|Avergae VUsers|%d\n", avg->running_users);
	fprintf(fp, "60|Session|140|Session/minute|%1.2f\n", (float) ((double)(cavg->sess_fetches_completed * 60000)/ (double)(global_settings->test_duration)));

	duration = (double)((double)(global_settings->test_duration)/1000.0);
#if 0
	eth_rx = (double) get_eth_rx_kbps(1, global_settings->test_duration);
	eth_tx = (double) get_eth_tx_kbps(1, global_settings->test_duration);

	pkt_rx = (double) get_eth_rx_pps(1, global_settings->test_duration);
	pkt_tx = (double) get_eth_tx_pps(1, global_settings->test_duration);
#endif
        //pkt_rx = (double) (avg->eth_rx_pps);
        //pkt_tx = (double) (avg->eth_tx_pps);

	tcp_rx = ((double)(cavg->c_tot_rx_bytes))/(duration*128.0);
	tcp_tx = ((double)(cavg->c_tot_tx_bytes))/(duration*128.0);
	con_made_rate = (cavg->c_num_con_succ * 1000)/global_settings->test_duration;
        con_break_rate = (cavg->c_num_con_break * 1000)/global_settings->test_duration;
	ssl_new = ((double)(cavg->c_ssl_new))/(duration);
	ssl_reused = ((double)(cavg->c_ssl_reused))/(duration);
	ssl_reuse_attempted = ((double)(cavg->c_ssl_reuse_attempted))/(duration);

	fprintf(fp, "70|Network|30|TCP Rx Throughput (Kbps)|%1.3f\n", tcp_rx);
	fprintf(fp, "70|Network|30|TCP Rx Total (Bytes)|%llu\n", cavg->c_tot_rx_bytes);
	fprintf(fp, "70|Network|40|TCP Tx Throughput (Kbps)|%1.3f\n", tcp_tx);
	fprintf(fp, "70|Network|40|TCP Tx Total (Bytes)|%llu\n", cavg->c_tot_tx_bytes);
	fprintf(fp, "70|Network|41|TCP Connections Open/Sec|%llu\n", con_made_rate);
	fprintf(fp, "70|Network|58|Total TCP Connections Open|%llu\n", cavg->c_num_con_succ);
	fprintf(fp, "70|Network|42|TCP Connections Close/Sec|%llu\n", con_break_rate);
	fprintf(fp, "70|Network|59|Total TCP Connections Close|%llu\n", cavg->c_num_con_break);
	fprintf(fp, "70|Network|43|TCP Connections Total|%llu\n", cavg->c_num_con_initiated);
	fprintf(fp, "70|Network|44|TCP Connections Success|%llu\n", cavg->c_num_con_succ);
	fprintf(fp, "70|Network|45|TCP Connections Failures|%llu\n", cavg->c_num_con_fail);
	fprintf(fp, "70|Network|46|SSL New Sessions/Sec|%1.3f\n", ssl_new);
        fprintf(fp, "70|Network|55|SSL Total New Sessions|%llu\n", cavg->c_ssl_new);
	fprintf(fp, "70|Network|47|SSL Reuse Requested Sessions/Sec|%1.3f\n", ssl_reuse_attempted);
	fprintf(fp, "70|Network|56|SSL Total Reuse Requested|%llu\n", cavg->c_ssl_reuse_attempted);
	fprintf(fp, "70|Network|48|SSL Reused Sessions/Sec|%1.3f\n", ssl_reused);
	fprintf(fp, "70|Network|57|SSL Total Reused Sessions|%llu\n", cavg->c_ssl_reused);
  // Start of code for getting min/max/avg from rtgMessage.dat file
  //printf ("All%d servers strt = %d\n", num, sizeof(SummaryMinMax));
  MY_MALLOC(dptr, (no_of_host * 17 +1) * sizeof(SummaryMinMax), "dptr", -1); //17 elements is server stats and 1 connections
  j = 0;

  dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(vuser_gp_ptr->num_connection));

  Server_stats_gp *server_stats_gp_local_ptr = server_stats_gp_ptr;
  for (i = 0; i < no_of_host; i++)
  {
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->v_swtch));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->interrupts));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->loadAvg1m));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->loadAvg5m));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->loadAvg15m));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->cpuUser));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->cpuSys));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->cpuTotalBusy));
    //dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->cpuNice));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->pageIn));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->pageOut));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->swapIn));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->swapOut));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->InSegs));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->OutSegs));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->InErrs));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->OutRsts));
    dptr[j++].offset = get_byte_offset (msg_data_ptr, (char *)&(server_stats_gp_local_ptr->collisions));
    server_stats_gp_local_ptr++;
  }

  // End of code for getting min/max/avg from rtgMessage.dat file

    	for (i =0; i < total_tx_entries; i++ ) {
	   	num_completed = savedTxData[i].tx_c_fetches_completed;
	   	num_succ = savedTxData[i].tx_c_succ_fetches;
		num_samples = num_completed;
		//num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
    //printf("Transaction name = %s\n", tx_table_shr_mem[i].name); //Anuj
                tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, i);
    	   	if (num_samples) {
	       	    //min_time = (double)(((double)(savedTxData[i].tx_c_min_time))/1000.0);
	       	    //max_time = (double)(((double)(savedTxData[i].tx_c_max_time))/1000.0);
	       	    avg_time = (double)(((double)(savedTxData[i].tx_c_tot_time))/((double)(1000.0*(double)num_samples)));

	    	    fprintf(fp, "80|Trans. Details|10|Trans.: (%s) Min Time(Sec)|%1.3f\n", tx_name,
				 (double)(((double)(savedTxData[i].tx_c_min_time))/1000.0));
	    	    fprintf(fp, "80|Trans. Details|20|Trans.: (%s) Avg Time(Sec)|%1.3f\n", tx_name, avg_time);
	    	    fprintf(fp, "80|Trans. Details|30|Trans.: (%s) Max Time(Sec)|%1.3f\n", tx_name,
				 (double)(((double)(savedTxData[i].tx_c_max_time))/1000.0));
		    std_dev = get_std_dev (savedTxData[i].tx_c_tot_sqr_time, savedTxData[i].tx_c_tot_time, avg_time, num_samples);
	    	    fprintf(fp, "80|Trans. Details|35|Trans.: (%s) Std Dev (Sec)|%1.3f\n", tx_name, std_dev);
	   	}  else {
	    	    fprintf(fp, "80|Trans. Details|10|Trans.: (%s) Min Time(Sec)|-\n", tx_name);
	    	    fprintf(fp, "80|Trans. Details|20|Trans.: (%s) Avg Time(Sec)|-\n", tx_name);
	    	    fprintf(fp, "80|Trans. Details|30|Trans.: (%s) Max Time(Sec)|-\n", tx_name);
	    	    fprintf(fp, "80|Trans. Details|35|Trans.: (%s) Std Dev (Sec)|-\n", tx_name);
		}

	    fprintf(fp, "80|Trans. Details|90|Trans.: (%s) Total|%llu\n", tx_name, num_completed);
	    fprintf(fp, "80|Trans. Details|100|Trans.: (%s) Success|%llu\n", tx_name, num_succ);
	    fprintf(fp, "80|Trans. Details|110|Trans.: (%s) Failures|%llu\n", tx_name, num_completed -  num_succ);

            if (num_completed )
	        fprintf(fp, "80|Trans. Details|120|Trans.: (%s) Failure PCT|%1.2f\n", tx_name, (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
	    else
	        fprintf(fp, "80|Trans. Details|120|Trans.: (%s) Failure PCT|-\n", tx_name);

        }

        /* SMTP */
        if (total_smtp_request_entries) {
          num_completed = cavg->smtp_fetches_completed;
          num_samples = num_succ = cavg->smtp_succ_fetches;
          //num_initiated = avg->cum_fetches_started;

          if (num_completed) {
            if (num_samples) {
              fprintf(fp, "90|smtp|10|smtp Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->smtp_c_min_time))/1000.0));
              fprintf(fp, "90|smtp|20|smtp Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->smtp_c_tot_time))/((double)(1000.0*(double)num_samples))));
              fprintf(fp, "90|smtp|30|smtp Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->smtp_c_max_time))/1000.0));
              fprintf(fp, "90|smtp|40|smtp Median Time(Sec)|%1.3f\n", smtp_data[0]);
              fprintf(fp, "90|smtp|50|smtp 80th percentile Time(Sec)|%1.3f\n", smtp_data[1]);
              fprintf(fp, "90|smtp|60|smtp 90th percentile Time(Sec)|%1.3f\n", smtp_data[2]);
              fprintf(fp, "90|smtp|70|smtp 95th percentile Time(Sec)|%1.3f\n", smtp_data[3]);
              fprintf(fp, "90|smtp|80|smtp 99th percentile Time(Sec)|%1.3f\n", smtp_data[4]);
              fprintf(fp, "90|smtp|90|smtp Total|%llu\n", num_completed);
              fprintf(fp, "90|smtp|100|smtp Success|%llu\n", num_succ);
              fprintf(fp, "90|smtp|110|smtp Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "90|smtp|120|smtp Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
              fprintf(fp, "90|smtp|10|smtp Min Time(Sec)|-\n");
              fprintf(fp, "90|smtp|20|smtp Avg Time(Sec)|-\n");
              fprintf(fp, "90|smtp|30|smtp Max Time(Sec)|-\n");
              fprintf(fp, "90|smtp|40|smtp Median Time(Sec)|-\n");
              fprintf(fp, "90|smtp|50|smtp 80th percentile Time(Sec)|-\n");
              fprintf(fp, "90|smtp|60|smtp 90th percentile Time(Sec)|-\n");
              fprintf(fp, "90|smtp|70|smtp 95th percentile Time(Sec)|-\n");
              fprintf(fp, "90|smtp|80|smtp 99th percentile Time(Sec)|-\n");
              fprintf(fp, "90|smtp|90|smtp Total|%llu\n", num_completed);
              fprintf(fp, "90|smtp|100|smtp Success|%llu\n", num_succ);
              fprintf(fp, "90|smtp|110|smtp Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "90|smtp|120|smtp Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
	    fprintf(fp, "90|smtp|10|smtp Min Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|20|smtp Avg Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|30|smtp Max Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|40|smtp Median Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|50|smtp 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|60|smtp 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|70|smtp 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|80|smtp 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "90|smtp|90|smtp Total|0\n");
	    fprintf(fp, "90|smtp|100|smtp Success|0\n");
	    fprintf(fp, "90|smtp|110|smtp Failures|0\n");
	    fprintf(fp, "90|smtp|120|smtp Failure PCT|0.00\n");
          }
          fprintf(fp, "90|smtp|140|smtp Hits/Sec|%1.2f\n", (float) ((double)(cavg->smtp_fetches_completed * 1000)/ (double)(global_settings->test_duration)));

        }

        if (total_pop3_request_entries) {
          /* POP3 */
          num_completed = cavg->pop3_fetches_completed;
          num_samples = num_succ = cavg->pop3_succ_fetches;
          //num_initiated = avg->cum_fetches_started;

          if (num_completed) {
            if (num_samples) {
              fprintf(fp, "100|pop3|10|pop3 Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->pop3_c_min_time))/1000.0));
              fprintf(fp, "100|pop3|20|pop3 Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->pop3_c_tot_time))/((double)(1000.0*(double)num_samples))));
              fprintf(fp, "100|pop3|30|pop3 Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->pop3_c_max_time))/1000.0));
              fprintf(fp, "100|pop3|40|pop3 Median Time(Sec)|%1.3f\n", pop3_data[0]);
              fprintf(fp, "100|pop3|50|pop3 80th percentile Time(Sec)|%1.3f\n", pop3_data[1]);
              fprintf(fp, "100|pop3|60|pop3 90th percentile Time(Sec)|%1.3f\n", pop3_data[2]);
              fprintf(fp, "100|pop3|70|pop3 95th percentile Time(Sec)|%1.3f\n", pop3_data[3]);
              fprintf(fp, "100|pop3|80|pop3 99th percentile Time(Sec)|%1.3f\n", pop3_data[4]);
              fprintf(fp, "100|pop3|90|pop3 Total|%llu\n", num_completed);
              fprintf(fp, "100|pop3|100|pop3 Success|%llu\n", num_succ);
              fprintf(fp, "100|pop3|110|pop3 Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "100|pop3|120|pop3 Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
              fprintf(fp, "100|pop3|10|pop3 Min Time(Sec)|-\n");
              fprintf(fp, "100|pop3|20|pop3 Avg Time(Sec)|-\n");
              fprintf(fp, "100|pop3|30|pop3 Max Time(Sec)|-\n");
              fprintf(fp, "100|pop3|40|pop3 Median Time(Sec)|-\n");
              fprintf(fp, "100|pop3|50|pop3 80th percentile Time(Sec)|-\n");
              fprintf(fp, "100|pop3|60|pop3 90th percentile Time(Sec)|-\n");
              fprintf(fp, "100|pop3|70|pop3 95th percentile Time(Sec)|-\n");
              fprintf(fp, "100|pop3|80|pop3 99th percentile Time(Sec)|-\n");
              fprintf(fp, "100|pop3|90|pop3 Total|%llu\n", num_completed);
              fprintf(fp, "100|pop3|100|pop3 Success|%llu\n", num_succ);
              fprintf(fp, "100|pop3|110|pop3 Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "100|pop3|120|pop3 Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
	    fprintf(fp, "100|pop3|10|pop3 Min Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|20|pop3 Avg Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|30|pop3 Max Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|40|pop3 Median Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|50|pop3 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|60|pop3 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|70|pop3 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|80|pop3 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "100|pop3|90|pop3 Total|0\n");
	    fprintf(fp, "100|pop3|100|pop3 Success|0\n");
	    fprintf(fp, "100|pop3|110|pop3 Failures|0\n");
	    fprintf(fp, "100|pop3|120|pop3 Failure PCT|0.00\n");
          }
          fprintf(fp, "100|pop3|140|pop3 Hits/Sec|%1.2f\n", (float) ((double)(cavg->pop3_fetches_completed * 1000)/ (double)(global_settings->test_duration)));

        }

       /*for ftp log summary data */
       ftp_log_summary_data(avg, ftp_data, fp, cavg);//TODO: pass only cavg

       /*for ldap log summary data */
       ldap_log_summary_data(avg, ldap_data, fp, cavg);

       /*for imap log summary data */
       imap_log_summary_data(avg, imap_data, fp, cavg);

       /*for jrmi log summary data */
       jrmi_log_summary_data(avg, jrmi_data, fp, cavg);

       /*for ws log summary data */
       //ws_log_summary_data(avg, ws_data, fp, cavg);

        if (total_dns_request_entries) {
          /* dns */
          num_completed = cavg->dns_fetches_completed;
          num_samples = num_succ = cavg->dns_succ_fetches;
          //num_initiated = avg->cum_fetches_started;

          if (num_completed) {
            if (num_samples) {
              fprintf(fp, "102|dns|10|dns Min Time(Sec)|%1.3f\n", (double)(((double)(cavg->dns_c_min_time))/1000.0));
              fprintf(fp, "102|dns|20|dns Avg Time(Sec)|%1.3f\n", (double)(((double)(cavg->dns_c_tot_time))/((double)(1000.0*(double)num_samples))));
              fprintf(fp, "102|dns|30|dns Max Time(Sec)|%1.3f\n", (double)(((double)(cavg->dns_c_max_time))/1000.0));
              fprintf(fp, "102|dns|40|dns Median Time(Sec)|%1.3f\n", dns_data[0]);
              fprintf(fp, "102|dns|50|dns 80th percentile Time(Sec)|%1.3f\n", dns_data[1]);
              fprintf(fp, "102|dns|60|dns 90th percentile Time(Sec)|%1.3f\n", dns_data[2]);
              fprintf(fp, "102|dns|70|dns 95th percentile Time(Sec)|%1.3f\n", dns_data[3]);
              fprintf(fp, "102|dns|80|dns 99th percentile Time(Sec)|%1.3f\n", dns_data[4]);
              fprintf(fp, "102|dns|90|dns Total|%llu\n", num_completed);
              fprintf(fp, "102|dns|100|dns Success|%llu\n", num_succ);
              fprintf(fp, "102|dns|110|dns Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "102|dns|120|dns Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
              fprintf(fp, "102|dns|10|dns Min Time(Sec)|-\n");
              fprintf(fp, "102|dns|20|dns Avg Time(Sec)|-\n");
              fprintf(fp, "102|dns|30|dns Max Time(Sec)|-\n");
              fprintf(fp, "102|dns|40|dns Median Time(Sec)|-\n");
              fprintf(fp, "102|dns|50|dns 80th percentile Time(Sec)|-\n");
              fprintf(fp, "102|dns|60|dns 90th percentile Time(Sec)|-\n");
              fprintf(fp, "102|dns|70|dns 95th percentile Time(Sec)|-\n");
              fprintf(fp, "102|dns|80|dns 99th percentile Time(Sec)|-\n");
              fprintf(fp, "102|dns|90|dns Total|%llu\n", num_completed);
              fprintf(fp, "102|dns|100|dns Success|%llu\n", num_succ);
              fprintf(fp, "102|dns|110|dns Failures|%llu\n", num_completed -  num_succ);
              fprintf(fp, "102|dns|120|dns Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
	    fprintf(fp, "102|dns|10|dns Min Time(Sec)|-\n");
	    fprintf(fp, "102|dns|20|dns Avg Time(Sec)|-\n");
	    fprintf(fp, "102|dns|30|dns Max Time(Sec)|-\n");
	    fprintf(fp, "102|dns|40|dns Median Time(Sec)|-\n");
	    fprintf(fp, "102|dns|50|dns 80th percentile Time(Sec)|-\n");
	    fprintf(fp, "102|dns|60|dns 90th percentile Time(Sec)|-\n");
	    fprintf(fp, "102|dns|70|dns 95th percentile Time(Sec)|-\n");
	    fprintf(fp, "102|dns|80|dns 99th percentile Time(Sec)|-\n");
	    fprintf(fp, "102|dns|90|dns Total|0\n");
	    fprintf(fp, "102|dns|100|dns Success|0\n");
	    fprintf(fp, "102|dns|110|dns Failures|0\n");
	    fprintf(fp, "102|dns|120|dns Failure PCT|0.00\n");
          }
          fprintf(fp, "102|dns|140|dns Hits/Sec|%1.2f\n", (float) ((double)(cavg->dns_fetches_completed * 1000)/ (double)(global_settings->test_duration)));

        }

	fclose(fp);
        FREE_AND_MAKE_NOT_NULL(dptr, "dptr", -1);
}
