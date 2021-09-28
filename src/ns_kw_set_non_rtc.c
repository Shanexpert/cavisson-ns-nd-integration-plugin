#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_phases_parse.h"
#include "ns_child_msg_com.h"
#include "ns_kw_set_non_rtc.h"
#include "ns_log.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "wait_forever.h"
#include "nslb_util.h"
#include "nslb_string_util.h"
#include "nslb_encode_decode.h"
#include "ns_parse_scen_conf.h"
#include "ns_trace_level.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_exit.h"

// This Global variable will take care of total entries for Group ALL Page ALL
// Required for G_HTTP_HDR
int all_group_all_page_header_entries = 0;

int create_metric_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_metric_entries == max_metric_entries) {
    MY_REALLOC_EX (metricTable, (max_metric_entries + DELTA_METRIC_ENTRIES) * sizeof(MetricTableEntry), max_metric_entries * sizeof(MetricTableEntry), "metricTable", -1);
    if (!metricTable) {
      fprintf(stderr,"create_metrics_table_entry(): Error allocating more memory for metricTable entries\n");
      return(FAILURE);
    } else max_metric_entries += DELTA_METRIC_ENTRIES;
  }
  *row_num = total_metric_entries++;
  metricTable[*row_num].name = -1;
  metricTable[*row_num].relation = -1;
  return (SUCCESS);
}


void kw_set_run_time(char *buf, Global_data *glob_set, int flag)
{
  int time, num;
  char option;
  char keyword[MAX_DATA_LINE_LENGTH];

  if ((num = sscanf(buf, "%s %d %c", keyword, &time, &option)) != 3) {
    fprintf(stderr, "read_keywords(): Need TWO fields after key RUN_TIME\n");
    exit(-1);
  }

  //RUN_TIME may be given more than one times, so set with only the last value of RUN_TIME, so all prev value must be set to 0
  glob_set->test_stab_time = glob_set->num_fetches = 0;
  switch (option) {
  case 'S':
    glob_set->test_stab_time = time;
    break;
  case 'M':
    glob_set->test_stab_time = time * 60;
    break;
  case 'H':
    glob_set->test_stab_time = time * 3600;
    break;
  case 'C':
    glob_set->num_fetches = time;
    break;
  case 'I':
    break;
  default:
    fprintf(stderr, "read_keywords(): RUN_TIME: Option %c invalid\n", option);
    exit(1);
  }
  if ((option != 'I') && (time <= 0)) {
    fprintf(stderr, "read_keywords(): RUN_TIME: invalid value %d %c\n", time, option);
    exit(1);
  }
}

void kw_set_testname(char *buf, Global_data *glob_set, int flag)
{
  if((glob_set->testname[0] == '\0') || (!strcmp(glob_set->testname, "NA")))
  {
    strncpy(glob_set->testname, buf+6, MAX_TNAME_LENGTH); //keyword is 5 chars+space. Value will start after 6 chars
    glob_set->testname[MAX_TNAME_LENGTH] = '\0';
  }
}

int kw_set_master(char *buf, Global_data *glob_set, int flag)
{
  unsigned int ip_addr;
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int port, num;

  if ((num = sscanf(buf, "%s %s %d", keyword, text, &port)) != 3) 
  {
    fprintf(stderr,"read_keywords(): Need TWO fields after key MASTER\n");
    return 1;
  }
  else if ((ip_addr = inet_addr(text)) < 0)
  {
     fprintf(stderr,"read_keywords(): Invalid MASTER address, ignoring <%s>\n", text);
  }
  return 0;
}

int kw_set_ns_server_secondary(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char IP[MAX_DATA_LINE_LENGTH];
  char port[MAX_DATA_LINE_LENGTH];
  int num;

  NSDL1_MISC(NULL, NULL, "Method called. buf = %s", buf);
  if ((num = sscanf(buf, "%s %s %s", keyword, IP, port)) != 3) {
    fprintf(stderr, "Need two fields after %s keyword.\n", keyword);
    if(!flag)
      exit(-1);
    else
      return 1;
  }

  strcpy(global_settings->secondary_gui_server_addr, IP);
  global_settings->secondary_gui_server_port = atoi(port);
  if(flag == 1) //run time changes need to apply
  {
    if(gui_fd2 != -1)
    {
      close(gui_fd2);
      gui_fd2 = -1;
      open_connect_to_gui_server(&gui_fd2, global_settings->secondary_gui_server_addr, global_settings->secondary_gui_server_port);
    }
  }
  NSDL1_MISC(NULL, NULL, "global_settings->secondary_gui_server_addr = %s, Port = %hd", global_settings->secondary_gui_server_addr, global_settings->secondary_gui_server_port);
  return 0;
}

void kw_set_warmup_time(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;

  if ((num = sscanf(buf, "%s %d", keyword, &glob_set->warmup_seconds)) < 2) 
  {
    fprintf(stderr, "read_keywords(): Need one fields after key WARMUP_TIME\n");
    exit(-1);
  }
  //Earlier, we used to ask for # of warmup sessions too. Disabled for now.
  //child_global_data.warmup_sessions = 0;

  //if ((child_global_data.warmup_sessions < 0) || (global_settings->warmup_seconds < 0)) {
  if ((glob_set->warmup_seconds < 0)) 
  {
    fprintf(stderr, "numbers after keyword WARMUP_TIME can't be negative\n");
    exit(-1);
  }
}

void kw_set_default_page_think_time(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num, think_mode, avg_time, median_time, var_time;

  num = sscanf(buf, "%s %d %d %d", keyword, &think_mode, &avg_time, &median_time);
  if (num < 2) {
    fprintf(stderr, "read_keywords(): Need at least ONE field after key DEFAULT_PAGE_THINK_TIME\n");
    exit(-1);
  }
  if (think_mode == 3) {
    if (num < 4) {
      fprintf(stderr, "read_keywords(): Need at least THREE fields after key DEFAULT_PAGE_THINK_TIME for think_mode of 3\n");
      exit(-1);
    }
    if (median_time <= avg_time) {
      fprintf(stderr, "read_keywords(): For key DEFAULT_PAGE_THINK_TIME the max value must be greater than the min value\n");
      exit(-1);
    }
  } else if (think_mode == 1) {
    if (num != 3) {
      fprintf(stderr, "read_keywords(): Need TWO fields after key DEFAULT_PAGE_THINK_TIME for think_mode of 1\n");
      exit(-1);
    }
    median_time = avg_time;  /* The user is alway inputting the first time arguement as the median time for mode 1, others are not provided by user */
    avg_time = -1;
    var_time = -1;
  } else if ((think_mode == 2) || (think_mode == 4)) {
    if (num < 3) {
      fprintf(stderr, "read_keywords(): Need at least TWO fields after key DEFAULT_PAGE_THINK_TIME for think_mode of 2\n");
      exit(-1);
    }
  } else if (think_mode != 0) {
    fprintf(stderr, "read_keywords(): Unknown think mode %d for keyword DEFAULT_PAGE_THINK_TIME\n", think_mode);
    exit(-1);
  }
  thinkProfTable[0].mode = think_mode;
  thinkProfTable[0].avg_time = avg_time;
  thinkProfTable[0].median_time = median_time;
  thinkProfTable[0].var_time = var_time;
}

#if 0
void kw_set_clickaway_global_profile(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];

	  if ((num = sscanf(buf, "%s %d %d %d %d", keyword, &immed_pct, &4_sec_pct, &8_sec_pct, &16_sec_pct, &32_sec_pct)) != 6) {
	    fprintf(stderr, "read_keywords(): Need FIVE fields after key CLICKAWAY_GLOBAL_PROFILE\n");
	    exit(-1);
	  }
	  clickawayProfTable[0].immed_pct = immed_pct;
	  clickawayProfTable[0].4_sec_pct = 4_sec_pct;
	  clickawayProfTable[0].8_sec_pct = 8_sec_pct;
	  clickawayProfTable[0].16_sec_pct = 16_sec_pct;
	  clickawayProfTable[0].32_sec_pct = 32_sec_pct;
}

void kw_set_clickaway_profile(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
	  if ((num = sscanf(buf, "%s %s %d %d %d %d", keyword, text, &immed_pct, &4_sec_pct, &8_sec_pct, &16_sec_pct, &32_sec_pct)) != 7) {
	    fprintf(stderr, "read_keywords(): Need SIX fields after key CLICKAWAY_PROFILE\n");
	    exit(-1);
	  }
	  if (create_clickawayprof_table_entry(&rnum) == FAILURE) {
	    fprintf(stderr, "read_keywords(): Error in creating new clickaway profile entry\n");
	    exit(-1);
	  }
	  clickawayProfTable[rnum].immed_pct = immed_pct;
	  clickawayProfTable[rnum].4_sec_pct = 4_sec_pct;
	  clickawayProfTable[rnum].8_sec_pct = 8_sec_pct;
	  clickawayProfTable[rnum].16_sec_pct = 16_sec_pct;
	  clickawayProfTable[rnum].32_sec_pct = 32_sec_pct;
	  if ((clickawayProfTable[rnum].name = copy_into_big_buf(text, 0)) == -1) {
	    fprintf(stderr, "read_keywords(): Error in copying new clickaway profile name into big_buf\n");
	    exit(-1);
	    }
  

}

void kw_set_page_clickaway(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
	  if ((num = sscanf(buf, "%s %d %s", keyword, &page_id, text)) != 3) {
	    fprintf(stderr, "read_keywords(): Need TWO fields after key PAGE_CLICKAWAY\n");
	    exit(-1);
	  }
	  if ((page_id < 0) || (page_id > total_page_entries)) {
	    fprintf(stderr, "read_keywords(): page_id after keyword PAGE_THINK invalid\n");
	    exit(-1);
	  }
	  if ((gPageTable[page_id].clickaway_prof_idx = find_clickawayprof_idx(text)) == -1) {
	    fprintf(stderr, "read_keywords(): unknown clickaway profile %s\n", text);
	    exit(-1);
	    }
}

void kw_set_sp(char *buf, Global_data *glob_set, int flag)
{
	if ((num = sscanf(buf, "%s %s %s %d", keyword, text, name, &pct)) != 4) {
	  fprintf(stderr,"read_keywords(): Need THREE fields after key SP\n");
	  exit(-1);
	}
	if ((idx = find_sessprofindex_idx(text)) == -1) {
	  if (create_sessprofindex_table_entry(&idx) != SUCCESS) {
	    fprintf(stderr, "read_keywords(): Error in getting sessprofindex_table entry\n");
	    exit(-1);
	  }
	  if ((sessProfIndexTable[idx].name = copy_into_big_buf(text, 0)) == -1) {
	    fprintf(stderr, "read_keywords(): Error in copying new SP name into big_buf\n");
	    exit(-1);
	  }
	}
	if (create_sessprof_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "read_keywords(): Error in getting sessprof_table entry\n");
	  exit(-1);
	}
	sessProfTable[rnum].sessprofindex_idx = idx;
	if ((idx = find_session_idx(name)) == -1) {
	  fprintf(stderr, "read_keywords(): Unknown session name\n");
	  exit(-1);
	}
	sessProfTable[rnum].session_idx = idx;
	sessProfTable[rnum].pct = pct;

	if (create_runprof_table_entry(&rnum) != SUCCESS) {
	  fprintf(stderr, "read_keywords(): Error in getting sessprof_table entry\n");
	  exit(-1);
	}

	for (i = 0; i < total_runprof_entries; i++) {
	  if (!strcmp(scen_group, RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name))) {
	    fprintf(stderr, "read_keywords(): Scen group %s already defined\n", scen_group);
	    exit(-1);
	  }
	}

	runProfTable[rnum].group_num = rnum;

	if ((runProfTable[rnum].scen_group_name = copy_into_big_buf(scen_group, 0)) == -1) {
	  fprintf(stderr, "read_keywords(): Error in copying into the big buf\n");
	  exit(-1);
	}

	if ((idx = find_userindex_idx(user_name)) == -1) {
	  fprintf(stderr, "read_keywords(): Unknown user profile %s \n", user_name);
	  exit(-1);
	}
	runProfTable[rnum].userprof_idx = idx;

	if ((idx = find_sessprofindex_idx(session_name)) == -1) {
	  fprintf(stderr, "read_keywords(): Unknown session profile\n");
	  exit(-1);
	}
	runProfTable[rnum].sessprof_idx = idx;
	runProfTable[rnum].quantity = pct;

	if (num == 6) {
	  int clusttab_idx;
	  if ((clusttab_idx = find_clust_idx(cluster_id)) == -1) {
	    if (create_clust_table_entry(&clusttab_idx) == -1)
	      exit(-1);
	    clustTable[clusttab_idx].cluster_id = cluster_id;
	  }
	  runProfTable[rnum].cluster_id = clusttab_idx;
	} else {
	  runProfTable[rnum].cluster_id = DEFAULT_CLUST_IDX;
	}

#endif

static void kw_set_use_dns_usage(char *err, char *buf, char *err_msg)
{
  NSDL2_MISC(NULL, NULL, "Method called. buf = %s, err = %s", buf, err);
  sprintf(err_msg, "Error: Invalid value of G_USE_DNS keyword: %s\n Given keyword value: %s", err, buf);
  strcat(err_msg, " Usage: G_USE_DNS <groupname or ALL> <Enable 0/1/2> <DNS Caching Mode 0/1/2> <DNS Log Mode 0/1> <DNS Connection Type<0/1> <DNS Cache ttl (in milliseconds)>\n");
  NSTL1(NULL, NULL, "%s", err_msg);
} 

int kw_set_use_dns(char *buf, GroupSettings *gset, char *err_msg, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char use_dns_str[MAX_DATA_LINE_LENGTH]; 
  char tmp_str[MAX_DATA_LINE_LENGTH];
  char dns_caching_mode_str[MAX_DATA_LINE_LENGTH];
  char dns_enable_debug_str[MAX_DATA_LINE_LENGTH];
  char dns_conn_type[MAX_DATA_LINE_LENGTH] = "0";
  char dns_ttl_time[128] = {0};
  /*Default setting to 0*/ 
  use_dns_str[0] = '0';
  use_dns_str[1] = '\0';
  dns_caching_mode_str[0] = '0';
  dns_caching_mode_str[1] = '\0'; 
  dns_enable_debug_str[0] = '0';
  dns_enable_debug_str[1] = '\0';

  NSDL2_MISC(NULL, NULL, "Method called. buf=%s, flag = %d", buf, flag);

  if(!gset)
  {
    if(err_msg)
    {
      kw_set_use_dns_usage("Wrong arguments passed in the function kw_set_use_dns(), second arg should not be null", buf, err_msg);
    }
    return -1;
  }
  //G_USE_DNS ALL 1 0 1
  int num = sscanf(buf, "%s %s %s %s %s %s %s %s", keyword, sgrp_name, use_dns_str, dns_caching_mode_str, dns_enable_debug_str, dns_conn_type, dns_ttl_time, tmp_str);
  NSDL2_MISC(NULL, NULL, "num = %d", num);
  //Validate number of arguments
  if (num < 3 || num > 7) 
  {
    if(err_msg)
    {
      kw_set_use_dns_usage("Invalid number of arguments. Must be greater than 2 and less than 6", buf, err_msg);
    }
  }

  //gset->use_dns = atoi(use_dns_str);
  //DNS ENABLE MODE
  gset->use_dns = atoi(use_dns_str); 
  if (!((gset->use_dns >= 0) && (gset->use_dns < 3))) 
  {
    if(err_msg)
    {
      kw_set_use_dns_usage("Invalid enable option, setting DNS lookup at the time of making connection disabled.", buf, err_msg);
    }
    gset->use_dns = 0;
  }

  //DNS CACHE MODE
  gset->dns_caching_mode = atoi(dns_caching_mode_str);
  if(gset->dns_caching_mode < 0 || gset->dns_caching_mode > 2)
  {
    if(err_msg)
    {
      kw_set_use_dns_usage("DNS Cache Mode must be positive. Hence setting DNS Caching Mode to 0", buf, err_msg);
    }
    gset->dns_caching_mode = 0;
  }
  
  //DNS LOG MODE
  if(ns_is_numeric(dns_enable_debug_str) == 0)
  {
    kw_set_use_dns_usage("DNS log enable option must be numeric. Hence setting DNS Log mode to 0",  buf, err_msg);
    gset->dns_debug_mode = 0;
  } 
  else 
  {
    gset->dns_debug_mode = atoi(dns_enable_debug_str);
    if(gset->dns_debug_mode < 0 || gset->dns_debug_mode > 1)
    {
      if(err_msg)
      {
        kw_set_use_dns_usage("DNS log enable option must be 0 or 1. Hence setting DNS Log mode to 0", buf, err_msg);
        gset->dns_debug_mode = 0;
      }
    }
  }

  //dns connection type
  if(ns_is_numeric(dns_conn_type) == 0)
  {
    kw_set_use_dns_usage("DNS connection type option must be numeric. Hence setting DNS connection type to 1(TCP)",  buf, err_msg);
    gset->dns_conn_type = 1;
  }else{
    gset->dns_conn_type = atoi(dns_conn_type);
    if(gset->dns_conn_type < 0 || gset->dns_conn_type > 1){
      kw_set_use_dns_usage("DNS connection type must be 0 or 1. Setting default DNS connection type to 1(TCP)",  buf, err_msg);
    } 
  }

  if((gset->dns_caching_mode == 2)  && (dns_ttl_time[0] == '\0'))
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, G_USE_DNS_USAGE, CAV_ERR_1011086, "");
  }

  if(ns_is_numeric(dns_ttl_time) == 0)
  {
    kw_set_use_dns_usage("DNS cache minimum ttl should be numeric in milliseconds", buf, err_msg);
    gset->dns_cache_ttl = 0;
  }
  else
  {
    gset->dns_cache_ttl = atoll(dns_ttl_time);
    if(gset->dns_cache_ttl < 0 || gset->dns_cache_ttl > 3600000)
      kw_set_use_dns_usage("DNS cache ttl should not be negative or more than 3600000", buf, err_msg);
  }

  NSDL2_MISC(NULL, NULL, "Use DNS mode = %d, Cache mode = %d, Debug mode = %d Connection Type = [%d], cache ttl = [%lld]", gset->use_dns, gset->dns_caching_mode, gset->dns_debug_mode, gset->dns_conn_type, gset->dns_cache_ttl);

  return 0;
}

int kw_set_client(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  unsigned int ip_addr;
  int num, port, rnum;

  if ((num = sscanf(buf, "%s %s %d", keyword, text, &port)) != 3) 
  {
    fprintf(stderr,"read_keywords(): Need TWO fields after key CLIENT\n");
    return 1; 
  } 
  else if (create_client_table_entry(&rnum) == FAILURE)
  {
    return 0;
  }
  else if ((ip_addr = inet_addr(text)) < 0)
  {
    fprintf(stderr,"read_keywords(): Invalid CLIENT address, ignoring <%s>\n", text);
  }
  else clients[rnum].ip_addr = ip_addr;

  clients[rnum].port = (unsigned short)port;
  return 0;
}

void kw_set_threshold(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num, intval1, intval2;

  num = sscanf(buf, "%s %d %d", keyword, &intval1, &intval2);
  if (num != 3)
  {
    printf("wrong format, using default: THRESHOLD Keyword format is: THRESHOLD warning_threshold alert_threshold\n");
  } 
  else
  {
    if ((intval1 < 0) || (intval2 <0)) 
    {
        printf ("read_keywords(): THRESHOLD : warn threshold (%d) and alert threshold(%d), both should be positive, using default\n", intval1, intval2);
    } 
    else
    {
       glob_set->warning_threshold = intval1;
       glob_set->alert_threshold = intval2;
    }
  }
}

void kw_set_logdata_process(char *buf, Global_data *glob_set, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num, log_level = 1;
  int remove_raw_file_mode = 1;

  num = sscanf(buf, "%s %d %d", keyword, &log_level, &remove_raw_file_mode);
  if (num < 3) 
  {
    printf("wrong format, using default: LOGDATA_PROCESS Keyword format is: LOGDATA_PROCESS log-data-process-mode raw-file-remove-mode\n");
  }
  else
  {
    if((log_level < 0) || (log_level >1)) 
    {
      printf ("read_keywords(): LOGDATA_PROCESS : %d is not valid log-data-process-mode, valid values are 0/1, using default 1\n", log_level);
      log_level = 1;
    }
   
    if((remove_raw_file_mode < 0) || (remove_raw_file_mode >1))
    {
      printf ("read_keywords(): LOGDATA_PROCESS : %d is not valid raw-file-remove-mode, valid values are 0/1, using default 1\n", remove_raw_file_mode);
      remove_raw_file_mode = 1;
    }
  }
  glob_set->log_postprocessing = log_level;
  glob_set->remove_log_file_mode = remove_raw_file_mode;
}

void kw_set_error_log(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int val;
  int num;

  if ((num = sscanf(buf, "%s %d", keyword, &val)) != 2) {
    fprintf(stderr, "read_keywords(): Need ONE fields after keyword %s\n", keyword);
    exit(-1);
  }

  if(val == 1) global_settings->error_log = 0x000000FF;
  if(val == 2) global_settings->error_log = 0x0000FFFF;
  if(val == 3) global_settings->error_log = 0x00FFFFFF;
  if(val == 4) global_settings->error_log = 0xFFFFFFFF;
  if(val == 0) global_settings->error_log = 0;
}

int kw_set_stype(char *buf, int flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];
  int num;

  if ((num = sscanf(buf, "%s %s", keyword, mode)) != 2) {
   NS_KW_PARSING_ERR(buf, 0, err_msg, STYPE_USAGE, CAV_ERR_1011172, CAV_ERR_MSG_1);
   // fprintf(stderr, "read_keywords(): Need ONE fields after key STYPE\n");
    //exit(-1);
  }

  if (!strcmp(mode, "FIX_CONCURRENT_USERS")) {
    testCase.mode = TC_FIX_CONCURRENT_USERS;
    global_settings->use_prof_pct = PCT_MODE_NUM; /* This gets overridden when keyword is defined later */
  } else if (!strcmp(mode, "FIX_SESSION_RATE")) {
    testCase.mode = TC_FIX_USER_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT; /* This gets overridden when keyword is defined later */
  } else if (!strcmp(mode, "MIXED_MODE"))
    testCase.mode = TC_MIXED_MODE;
  else if (!strcmp(mode, "REPLAY_ACCESS_LOGS"))
  {
    testCase.mode = TC_FIX_USER_RATE;
    global_settings->replay_mode = 1;
    global_settings->use_prof_pct = PCT_MODE_PCT; /* This gets overridden when keyword is defined later */
    global_settings->vuser_rpm = 1 * 1000; // Since in replay mode target rate is not needed
  }
  else if (!strcmp(mode, "FIX_HIT_RATE")) {
    testCase.mode = TC_FIX_HIT_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_PAGE_RATE")) {
    testCase.mode = TC_FIX_PAGE_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_TX_RATE")) {
    testCase.mode = TC_FIX_TX_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "MEET_SLA")) {
    testCase.mode = TC_MEET_SLA;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "MEET_SERVER_LOAD")) {
    testCase.mode = TC_MEET_SERVER_LOAD;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_MEAN_USERS")) {
    testCase.mode = TC_FIX_MEAN_USERS;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else {
    NS_KW_PARSING_ERR(buf, 0, err_msg, STYPE_USAGE, CAV_ERR_1011241, mode);
   // fprintf(stderr, "read_keywords(): Unknown Scenario Type mode\n");
    //exit(-1);
  }
  return 0;
}


void kw_set_target_rate(char *buf, Global_data *glob_set, int flag)
{
  float hit_rate;
  
  /*
  if ((num = sscanf(buf, "%s %d", keyword, &hit_rate)) != 2) {
    fprintf(stderr, "read_keywords(): Need One field after key TARGET_RATE\n");
    exit(-1);
  }
  */
  // Add 02/13/07 - Achint for supporting units in TARGET_RATE keyword
  hit_rate = convert_to_per_minute(buf);

  if (testCase.mode == TC_FIX_USER_RATE) {
    if (global_settings->use_prof_pct == PCT_MODE_NUM) {
      NSTL1_OUT(NULL, NULL, "TARGET_RATE ignored. in NUM mode");
    } else if (global_settings->use_prof_pct == PCT_MODE_PCT && 
               global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
      NSTL1_OUT(NULL, NULL, "Ignoring TARGET_RATE in SCHEDULE_TYPE_ADVANCED and PCT mode schedule.");
    } else {
      glob_set->vuser_rpm = hit_rate * 1000;
    }
  } else
    testCase.target_rate = hit_rate;
}

int kw_set_metric(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char name[MAX_DATA_LINE_LENGTH];
  char relation[MAX_DATA_LINE_LENGTH];
  int port, target, min_samples, num, rnum;
  
  if ((num = sscanf(buf, "%s %s %d %s %d %d", keyword, name, &port, relation, &target, &min_samples)) != 6) 
  {
     fprintf(stderr, "read_keywords(): Need FIVE fields after key SLA\n");
     exit(-1);
  }

  if (testCase.mode != TC_MEET_SERVER_LOAD)
  {
    NSDL2_MISC(NULL, NULL, "read_keywords(): Warning, METRIC entry is ignored for this test case type\n");
    NS_DUMP_WARNING("METRIC entry is ignored because test type is not MEET_SERVER_LOAD type.");
    return 1; 
  }

  if (create_metric_table_entry(&rnum) != SUCCESS) 
  {
    fprintf(stderr, "read_keywords(): Failed to create no SLA Table entry\n");
    exit(-1);
  }

  if (!strcmp(name, "CPU"))
    metricTable[rnum].name = METRIC_CPU;
  else if (!strcmp(name, "PORT"))
    metricTable[rnum].name = METRIC_PORT;
  else if (!strcmp(name, "RUN_QUEUES"))
    metricTable[rnum].name = METRIC_RUN_QUEUES;
  else
  {
    fprintf(stderr, "read_keywords(): Unknown name <%s> in METRIC entry\n", name);
    exit(-1);
  }

  if (port < 0)
  {
    fprintf(stderr, "METRIC entry: Port must be greater than 0\n");
    exit(1);
  }

  metricTable[rnum].port = port;

  if (!strcmp(relation, "<"))
    metricTable[rnum].relation = METRIC_LESS_THAN;
  else if (!strcmp(relation, ">"))
    metricTable[rnum].relation = METRIC_GREATER_THAN;
  else
  {
    fprintf(stderr, "read_keywords(): Unknown relation <%s> in METRIC entry\n", relation);
    exit(-1);
  }

  if (target <= 0)
  {
    fprintf(stderr, "METRIC entry: target must be greater than 0\n");
    exit(1);
  }

  metricTable[rnum].target_value = target;
  if (min_samples < 0) 
  {
    fprintf(stderr, "METRIC entry: min_samples must be greater than or equal to 0\n");
    exit(1);
  }

  metricTable[rnum].min_samples = min_samples;

  return 0;
}

int kw_set_nvm_distribution(char *text, Global_data *glob_set, int flag, char *err_msg, char *buf)
{
  glob_set->nvm_distribution = atoi(text);
  if ((glob_set->nvm_distribution < 0) || (glob_set->nvm_distribution > 2)) 
  {
    NS_KW_PARSING_ERR(buf, flag, err_msg, NVM_DISTRIBUTION_USAGE, CAV_ERR_1011071, CAV_ERR_MSG_3);
  }

  return 0;
}


int
kw_set_g_max_users(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH]; 
  char text[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  unsigned int users;
  int num;
  num = sscanf(buf, "%s %s %s %s ", keyword, grp, text, tmp);
  if(num != 3) { //Check for extra arguments.
    sprintf(err_msg, "Invalid number of arguments for Keyword G_MAX_USERS\n");
    if(!runtime_flag)
    {
      fprintf(stderr, "%s", err_msg);
      exit (-1);
    }
    else
      return -1;
  }
  if(ns_is_numeric(text)) {
    users = atoi(text);
  }
  else
  {
    sprintf(err_msg, "Error: Value of G_MAX_USERS is not numeric");
    if(!runtime_flag)
    {
      fprintf(stderr, "%s\n", err_msg);
      exit(-1);
    }
    else
      return(-1);
  }
  if(users<0)
  {
    sprintf(err_msg, "Error: Value of G_MAX_USERS cannot be less than 1");
    if(!runtime_flag)
    {
      fprintf(stderr, "%s\n", err_msg);
      exit (-1);
    }
    else 
     return(-1);
  }    
  if(users == 0)
    users = INT_MAX;

  *to_change = users;
  return 0; 
} 

int kw_set_log_shr_buf_size(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num;
  num = sscanf(buf, "%s %s %s", keyword, text, tmp);
  if(num!=2)
  {
    sprintf(err_msg, "invalid number of arguments for keyword LOG_SHR_BUFFER_SIZE\n");
    return -1;
  }
  if(ns_is_numeric(text))
  {
    global_settings->log_shr_buffer_size = atoi(text);
  }
  else
  {
    sprintf(err_msg, "Error: Value of LOG_SHR_BUFFER_SIZE is not numeric");
    return(-1);
  }
  if(global_settings->log_shr_buffer_size < 1024)
  {
    fprintf(stdout, "Warning: LOG_SHR_BUFFER_SIZE , buffer size can not be less than 1024(Bytes), setting default(8192 Bytes) value\n");
    global_settings->log_shr_buffer_size = 8192;    
  }
  return 0;
}
int kw_set_max_users(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int num;
  num = sscanf(buf, "%s %s %s", keyword, text, tmp);
  if(num!=2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_USERS_USAGE, CAV_ERR_1011221, CAV_ERR_MSG_1);
  }
  
  if(ns_is_numeric(text))
  {
    global_settings->max_user_limit = atoi(text);
  } 
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_USERS_USAGE, CAV_ERR_1011221, CAV_ERR_MSG_2);
  }
  if(global_settings->max_user_limit<0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, MAX_USERS_USAGE, CAV_ERR_1011221, CAV_ERR_MSG_8);
  }
  
  if(global_settings->max_user_limit == 0)
    global_settings->max_user_limit = INT_MAX;

  return 0;
}

// This function sets http version
int kw_set_use_http_10(char *buf) {

  char keyword[MAX_DATA_LINE_LENGTH];
  char value[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int num_fields;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. buf=%s", buf);
  num_fields = sscanf(buf, "%s %s %s", keyword, value, tmp);

  if(num_fields > 2) {
    fprintf(stderr,"<USE_HTTP_10> keyword needs only 2 values");
    exit(-1);
  }

  if(strlen(value) > 1) {
    fprintf(stderr, "<http(s) version> can have only single digit integer values for %s.\n", keyword);
    exit(-1);
  }

  if(ns_is_numeric(value) == 0) {
    fprintf(stderr, "<http(s) version> can have only integer values for %s.\n", keyword);
    exit(-1);
  }
 
  global_settings->use_http_10 = atoi(value); 
  if(global_settings->use_http_10 != 0 && global_settings->use_http_10 != 1) {
    fprintf(stderr, "<http(s) version> can have only two values 0 or 1 for %s.\n", keyword);
    exit(-1);
  }

  return 0;
}

#if 0
#define ns_strncpy(dest, dest_len, src, src_len, emsg) \
{ \
  int ret; \
  ret = nslb_strncpy(dest, dest_len, src, 0); \
  if(ret == -1) \
    exit(-1); \
  if(ret == 1) \
  { \
    sprintf(err_msg, "%s", emsg); \
    return -1; \
  } \
}
#endif

/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : kw_g_http_body_chksum_hdr 
 *
 * Purpose   : This function will parse keyword G_HTTP_BODY_CHECKSUM_HEADER and fill user provided inputs
 *             into DS GroupSettings  
 *
 * Input     : buf = G_HTTP_BODY_CHECKSUM_HEADER <Group>  <mode> <add header in case of empty body> <add prefix and suffix> <header name> <prefix for the http_body> <suffix for the http_body>  
 *
 * Output    : On error     -1
 *             On success    0
 *        
 * Build_v   : 4.1.11 & 4.1.12  
 *------------------------------------------------------------------------------------------------------------------*/
int kw_g_http_body_chksum_hdr(char *buf, GroupSettings *gset, char *err_msg)
{
  char keyword[CHKSUM_HDR_MAX_LEN + 1];
  char group_name[CHKSUM_HDR_MAX_LEN + 1];
  char header_name[CHKSUM_HDR_MAX_LEN + 1];
  //char header_name_ex[CHKSUM_HDR_MAX_LEN + 1] = "";
  char body_pfx[CHKSUM_HDR_MAX_LEN + 1];
  char body_sfx[CHKSUM_HDR_MAX_LEN + 1];
  char usages[2048 + 1];
  short mode;
  short if_body_empty;
  short if_pfx_sfx = 0;
  int ret;

  HttpBodyCheckSumHdr *chksum_hdr;

  NSDL1_PARSING(NULL, NULL, "Method Called, buf = [%s], gset = [%p]", buf, gset);


  sprintf(usages, "Usages:\n"
                   "G_HTTP_BODY_CHECKSUM_HEADER <Group>  <mode> <add header in case of empty body> <add prefix and suffix> <header name> <prefix for the http_body> <suffix for the http_body>\n"
                   "Where:\n"
                   "  group_name                    - group name can be ALL or any valid group\n"
                   "  mode                            0 - disable keyword(Default)\n"
                   "                                  1 - enable keyword\n"
                   "  if_body_empty                 - disabled.\n"
                   "  if_pfx_sfx                    - provide prefix_suffix to checksum string.\n"
                   "                                  0 - disable prefix_suffix\n"
                   "                                  1 - enabled, for sending headers \n"
                   "  header_name                   - X-Payload-Confirmation\n"
                   "  body_pfx                      - '@'\n"
                   "  body_sfx                      - '@'\n"
                   );


  int count_arg = sscanf(buf, "%s %s %hd %hd %hd %s %s %s", keyword, group_name, &mode, &if_body_empty, &if_pfx_sfx, header_name, body_pfx, body_sfx);


  NSDL1_PARSING(NULL, NULL, "keyword = %s, group_name = %s, mode = %hd, if_body_empty = %hd, header_name = %s,"
                            "if_pfx_sfx = %hd, body_pfx = %s, body_sfx = %s",
                             keyword, group_name, mode, if_body_empty, header_name, if_pfx_sfx, body_pfx, body_sfx);
  if(count_arg != 8)
  {
    sprintf(err_msg, "Provide number of arguments is not equal to 7.\n [%s]\n", usages);
    return -1;
  }

if(((mode > 1) || (mode < 0)) || ((if_body_empty > 1) || (if_body_empty < 0)) || ((if_pfx_sfx > 1) || (if_pfx_sfx < 0)))
  {
    sprintf(err_msg, "mode, if_body_empty and is_curly_bracket should be 0 or 1 respectively\n [%s]\n", usages);
    return -1;
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  chksum_hdr = &gset->http_body_chksum_hdr;

  /* Insert values into group setting data structure */
  if(if_pfx_sfx)
  {
    // Fill prefix
    if (!strcmp(body_pfx,"NA"))
    {
      //strcpy(chksum_hdr->pfx, "{");
      ret = nslb_strncpy(chksum_hdr->pfx, CHKSUM_PFX_MAX_LEN, "{", 0);
      if(ret == -1)
        exit(-1);
      if(ret == 1)
      {
        sprintf(err_msg, "prefix '{' is truncated.");
        return -1;
      }
    }
    else
    {
      //strcpy(chksum_hdr->pfx, body_pfx);
      ret = nslb_strncpy(chksum_hdr->pfx, CHKSUM_PFX_MAX_LEN, body_pfx, 0);
      if(ret == -1)
        exit(-1);
      if(ret == 1)
      {
        sprintf(err_msg, "prefix '%s' is truncated.", body_pfx);
        return -1;
      }
    }
    // Fill suffix
    if (!strcmp(body_sfx,"NA"))
    {
      //strcpy(chksum_hdr->sfx, "}");
      ret = nslb_strncpy(chksum_hdr->sfx, CHKSUM_PFX_MAX_LEN, "}", 0);
      if(ret == -1)
        exit(-1);
      if(ret == 1)
      {
        sprintf(err_msg, "suffix '}' is truncated.");
        return -1;
      }
    }
    else
    {
      //strcpy(chksum_hdr->sfx, body_sfx);
      ret = nslb_strncpy(chksum_hdr->sfx, CHKSUM_PFX_MAX_LEN, body_sfx, 0);
      if(ret == -1)
        exit(-1);
      if(ret == 1)
      {
        sprintf(err_msg, "suffix '}' is truncated.");
        return -1;
      }
    }

    chksum_hdr->pfx_len = strlen(chksum_hdr->pfx);
    chksum_hdr->sfx_len = strlen(chksum_hdr->sfx);

    NSDL1_PARSING(NULL, NULL, "http_body_chksum_hdr.pfx  = [%s], http_body_chksum_hdr.sfx = [%s],"
                              " http_body_chksum_hdr.pfx_len = [%d], http_body_chksum_hdr.sfx_len = [%d]",
                              chksum_hdr->pfx, chksum_hdr->sfx, chksum_hdr->pfx_len,
                              chksum_hdr->sfx_len);
  }

  chksum_hdr->if_pfx_sfx = if_pfx_sfx;
  if (!strcmp(header_name,"NA"))
  {
    strcpy(chksum_hdr->hdr_name, "X-Payload-Confirmation: ");
    strcpy(chksum_hdr->h2_hdr_name, "x-payload-confirmation");
  }
  else
  {
    snprintf(chksum_hdr->hdr_name, CHKSUM_HDR_MAX_LEN, "%s: ", header_name);
    strcpy(chksum_hdr->h2_hdr_name, nslb_strlower(header_name));
    //strcpy(chksum_hdr->hdr_name, header_name_ex);
  }

  chksum_hdr->hdr_name_len = strlen(chksum_hdr->hdr_name);
  chksum_hdr->h2_hdr_name_len = strlen(chksum_hdr->h2_hdr_name);
  chksum_hdr->mode = mode;
  chksum_hdr->if_body_empty = if_body_empty;

  NSDL1_PARSING(NULL, NULL, "Method end, http_body_chksum_hdr_mode  = [%d], http_body_chksum_hdr_if_body_empty = [%d], "
                            ", http_body_chksum_hdr_if_pfx_sfx = [%d], http_body_chksum_hdr_name = [%s]"
                              ", h2_hdr_name_len = [%d], h2_hdr_name = [%s]",
                               chksum_hdr->mode, chksum_hdr->if_body_empty, chksum_hdr->if_pfx_sfx, chksum_hdr->hdr_name,
                              chksum_hdr->h2_hdr_name_len, chksum_hdr->h2_hdr_name);

  return 0;
}

int kw_set_health_mon(char *buf, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];
  char value[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp[MAX_DATA_LINE_LENGTH];
  int num_fields;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. buf=%s", buf);
  
  num_fields = sscanf(buf, "%s %s %s %s", keyword, mode, value, tmp);
  if(num_fields < 2 || num_fields > 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, HEALTH_MONITOR_USAGE, CAV_ERR_1011139, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, HEALTH_MONITOR_USAGE, CAV_ERR_1011139, CAV_ERR_MSG_2);
  }

  global_settings->smon = atoi(mode);
  
  if(global_settings->smon && value[0] != '\0')
  {
    if(ns_is_numeric(value) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, HEALTH_MONITOR_USAGE, CAV_ERR_1011139, CAV_ERR_MSG_2);
    }
 
    global_settings->tw_sockets_limit = atoi(value);
  }
  
  if(global_settings->smon != 0 && global_settings->smon != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, HEALTH_MONITOR_USAGE, CAV_ERR_1011139, CAV_ERR_MSG_3);
  }

  
  if(!global_settings->tw_sockets_limit)
    global_settings->tw_sockets_limit = 1000;  //default value

  if(global_settings->tw_sockets_limit < 1 || global_settings->tw_sockets_limit > 65000)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, HEALTH_MONITOR_USAGE, CAV_ERR_1011140, "");
  }

  NSDL2_SCHEDULE(NULL, NULL, "Method exiting, Mode = %d, time wait sockets limit = %d", global_settings->smon,
                              global_settings->tw_sockets_limit);
  return 0;
}
#if 0
static int create_add_header_table_entry(int *rnum)
{

  NSDL2_PARSING (NULL, NULL, "Method called");

  if (total_hdr_entries == max_hdr_entries)
  {
    MY_REALLOC_EX (addHeaderTable, (max_hdr_entries + DELTA_ADD_HEADER_ENTRIES) * sizeof(AddHeaderTableEntry), (max_hdr_entries * sizeof(AddHeaderTableEntry)), "addHeaderTable", -1); // Added old size of table
    if (!addHeaderTable) {
      fprintf(stderr, "create_add_header_table_entry(): Error allcating more memory for addHeader entries\n");
      return FAILURE;
    } else max_hdr_entries += DELTA_ADD_HEADER_ENTRIES;
  }
  *rnum = total_hdr_entries++;
  return (SUCCESS);

}
#endif

void fill_add_header_table(int rnum, int mode, int grp_num, int page_id, char *hname, char *hvalue)
{
  addHeaderTable[rnum].mode = mode;
  addHeaderTable[rnum].groupid = grp_num;
  addHeaderTable[rnum].pageid = page_id;
  snprintf(addHeaderTable[rnum].headername, MAX_HEADER_NAME_LEN, "%s", hname);
  snprintf(addHeaderTable[rnum].headervalue, MAX_HEADER_VALUE_LEN, "%s", hvalue);
  NSDL2_PARSING(NULL, NULL, "G_HTTP_HDR:: New Header Entry(%d), mode(%d) group(%d), page(%d), name(%s), value(%s)", 
                rnum, addHeaderTable[rnum].mode, addHeaderTable[rnum].groupid, addHeaderTable[rnum].pageid, 
                addHeaderTable[rnum].headername, addHeaderTable[rnum].headervalue);
}


int kw_set_g_http_hdr(char *buf, char *change_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char pg_name[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];
  char hname[MAX_DATA_LINE_LENGTH]; // Header name
  char hvalue[MAX_DATA_LINE_LENGTH]; // Header value
  char tmp[MAX_DATA_LINE_LENGTH];
  int hmode; // header mode
  int page_id;
  int num_fields;

  NSDL2_PARSING(NULL, NULL, "Method called. buf=%s", buf);

  num_fields = sscanf(buf, "%s %s %s %s %s %s %s", keyword, sg_name, pg_name, mode, hname, hvalue, tmp);
  if(num_fields != 6)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_HTTP_HDR_USAGE, CAV_ERR_1011365, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_HTTP_HDR_USAGE, CAV_ERR_1011365, CAV_ERR_MSG_2);
  }

  hmode = atoi(mode);
  // Check for Valid Mode
  if((hmode < 0) || (hmode > 2))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_HTTP_HDR_USAGE, CAV_ERR_1011366, "");
  }

  // We couldn't have same page name in ALL scripts
  if ((strcasecmp(sg_name, "ALL") == 0) && (strcasecmp(pg_name, "ALL") != 0)) 
  { 
    NSTL1(NULL, NULL, "G_HTTP_HDR:: Page %s should be ALL with Group %s", pg_name, sg_name); 
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_HTTP_HDR_USAGE, CAV_ERR_1011367, pg_name, "");
  } 
 
  // for particular group. For this keyword -1 is a valid group
  int grp_idx;
  if ((grp_idx = find_sg_idx(sg_name)) == NS_GRP_IS_INVALID) //invalid group
  {
    NSTL1(NULL,NULL, "Warning: For Keyword G_HTTP_HDR, header can't be applied for unknown group '%s'. Group (%s) ignored.", sg_name, sg_name);
    return 0;
  }
  
  //We will allow only following headers
  if((!hname[0]) || (!strcmp(hname, "Content-length")) || (!strcmp(hname, "Host")) || (!strcmp(hname, "Connection"))
    || (!strcmp(hname, "User-Agent")) || (!strcmp(hname, "Keep-Alive")) || (!strcmp(hname, "Accept")) || (!strcmp(hname, "Accept-Encoding")))
  {

    NSDL2_PARSING(NULL, NULL, "For Keyword G_HTTP_HDR, header %s is not supported", hname); 
    NSTL1(NULL,NULL, "Warning: For Keyword G_HTTP_HDR, header %s is not supported", hname);
    return 0;
  
  }
  
  // Can have following cases
  // 1. GRP ALL => PAGE ALL
  // 2. GRP ALL => PAGE SPECIFIC - this case should not happened
  // 3. GRP SPECIFIC => PAGE ALL
  // 4. GRP SPECIFIC => PAGE SPECIFIC
 
  int rnum;
  // 1. GRP ALL => PAGE ALL
  // We are keeping page_id -1 for case when Group is ALL and Pagename is ALL
  NSDL2_PARSING(NULL, NULL, "G_HTTP_HDR Case of Group %s, pg_name %s", sg_name, pg_name);
  if ((strcasecmp(pg_name, "ALL") == 0) && (strcasecmp(sg_name, "ALL") == 0))
  {   
    if (create_add_header_table_entry(&rnum) == FAILURE)
    { 
          NS_EXIT(-1, CAV_ERR_1000002);
    }

    fill_add_header_table(rnum, hmode, -1, -1, hname, hvalue);
    all_group_all_page_header_entries++;

    return 0;
  }

  int session_idx = runProfTable[grp_idx].sessprof_idx;
  // 3. GRP SPECIFIC => PAGE ALL
  if (strcasecmp(pg_name, "ALL") == 0)
  {
    int i;
    int first_page = gSessionTable[session_idx].first_page;
    for (i = first_page; i < first_page + gSessionTable[session_idx].num_pages; i++)
    {
      if (create_add_header_table_entry(&rnum) == FAILURE)
      {
        NS_EXIT(-1, CAV_ERR_1000002);
      }
      fill_add_header_table(rnum, hmode, grp_idx, i, hname, hvalue);
    }
    return 0;
  }
    
 
/*  if(!check_and_update_header(grp_id, page_id, hname, hvalue))
    return 0; */
  
  // 4. GRP SPECIFIC => PAGE SPECIFIC
  if ((page_id = find_page_idx(pg_name, session_idx)) == -1) // invalid page print error message and exit
  { 
    char *session_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
    NS_KW_PARSING_ERR(buf, runtime_flag, change_msg, G_HTTP_HDR_USAGE, CAV_ERR_1011368, pg_name, sg_name, session_name, "");
  }
  if (create_add_header_table_entry(&rnum) == FAILURE)
  { 
    NS_EXIT(-1, CAV_ERR_1000002);
  }
  // This has been done in order to get relative page_id corressponding to a script
  fill_add_header_table(rnum, hmode, grp_idx, page_id, hname, hvalue);
  return 0;
}
