/************************************************************************************
 * Name            : ns_page.c 
 * Purpose         : This file contains all the page related function of netstorm
 * Initial Version : Monday, July 13 2009
 * Modification    : -
 ***********************************************************************************/

#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_auto_fetch_embd.h"
#include "ns_debug_trace.h"
#include "ns_string.h"
#include "poi.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_session.h"
#include "ns_trans.h"
#include "ns_page.h"
#include "ns_page_think_time.h"
#include "ns_vuser.h"
#include "ns_parallel_fetch.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_resp.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_schedule_phases_parse.h"
#include "ns_sock_com.h"
#include "ns_vars.h"
#include "ns_pop3.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "nslb_sock.h"
#include "ns_vuser_tasks.h"
#include "runlogic.h"
#include "ns_vuser_ctx.h"
#include "ns_replay_access_logs.h"
#include "ns_vuser_trace.h"
#include "ns_child_thread_util.h"
#include "ns_page_dump.h"
#include "ns_connection_pool.h"
#include "ns_trace_log.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_script_parse.h"
#include "ns_ldap.h"
#include "ns_user_define_headers.h"
#include "ns_jrmi.h"
#include "ns_click_script.h"
#include "ns_inline_delay.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_websocket.h"
#include "ns_exit.h"
#include "ns_websocket_reporting.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_socket.h"
#include "ns_test_monitor.h"

#ifndef CAV_MAIN
PageReloadProfTableEntry *pageReloadProfTable = NULL;
PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem = NULL;

PageClickAwayProfTableEntry *pageClickAwayProfTable = NULL;
PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem = NULL;
int max_pagereloadprof_entries = 0;
int total_pagereloadprof_entries = 0 ;

int max_pageclickawayprof_entries = 0;
int total_pageclickawayprof_entries = 0;

#else
__thread PageReloadProfTableEntry *pageReloadProfTable = NULL;
__thread PageReloadProfTableEntry_Shr *pagereloadprof_table_shr_mem = NULL;

__thread PageClickAwayProfTableEntry *pageClickAwayProfTable = NULL;
__thread PageClickAwayProfTableEntry_Shr *pageclickawayprof_table_shr_mem = NULL;
__thread int max_pagereloadprof_entries = 0;
__thread int total_pagereloadprof_entries = 0 ;

__thread int max_pageclickawayprof_entries = 0;
__thread int total_pageclickawayprof_entries = 0;

#endif


extern void dump_URL_SHR(PageTableEntry_Shr*);
extern int log_page_record_v2(VUser* vptr, u_ns_ts_t now);

int warmup_sess=0;
char interactive_buf[4096];

static void reload_connection_callback(VUser *vptr, connection *cptr, u_ns_ts_t now)
{
  NSDL2_SOCKETS(vptr, cptr, "Method called");

  if(cptr->url_num->proto.http.type == MAIN_URL && 
     cptr->req_ok != NS_REQUEST_OK && cptr->completion_code != NS_COMPLETION_EXACT && vptr->reload_attempts > 0)
      Close_connection( cptr , 0, now, NS_REQUEST_RELOAD, NS_COMPLETION_RELOAD);
}

static void click_away_connection_callback(VUser *vptr, connection *cptr, u_ns_ts_t now)
{
  NSDL2_SOCKETS(vptr, cptr, "Method called");

  if(cptr->url_num->proto.http.type == MAIN_URL && (cptr->req_ok != NS_REQUEST_OK && cptr->completion_code != NS_COMPLETION_EXACT) &&
     is_clickaway_page(vptr, cptr)) {
      NSDL2_SCHEDULE(vptr, cptr, "Page is clicked awayed");
      Close_connection( cptr , 0, now, NS_REQUEST_CLICKAWAY, NS_COMPLETION_CLICKAWAY);
  }
}

/* This function forces to reload/click away a page if reload/click awy is configured with 0 timeout
 * Case:1 Not Reload No Click Away -- Do not Do anything
 * Case:2 Reload timeout is zero; Close Connect 
 * Case:3 Reload timeout is non zero & Click away timeout is 0 is non zero; then do click away immediately without waiting for response; In this case reload may not be done if timely done.
 *
 * Note: Should be called only for MAIN page.
 */
void chk_and_force_reload_click_away(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;
  PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  if (runprof_table_shr_mem[vptr->group_num].page_reload_table)
    pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number];
  
  if (runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];

  if(pagereload_ptr)
  { 
    if(pagereload_ptr->reload_timeout == 0)
    {
      reload_connection_callback(vptr, cptr, now);
      return;
    }
  }

  if(pageclickaway_ptr)
  { 
      if(pageclickaway_ptr->clickaway_timeout == 0)
      {
        click_away_connection_callback(vptr, cptr, now);
        return;
      }
  }
}

int validate_idle_secs_wrt_reload_clickaway(int pass, int group_idx, char *buf)
{
  int session_idx, i;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  if (pass == 1) {
    if(total_pagereloadprof_entries > 1 || (total_pagereloadprof_entries == 1 && pageReloadProfTable[0].min_reloads >= 0)) { /* remove warning */
      //fprintf(stderr, "Warning: '%s' has no mean with Page Reload.\n", temp_buf);
      return 1;
    }

    if((total_pageclickawayprof_entries > 1 || (total_pageclickawayprof_entries == 1 && pageClickAwayProfTable[0].clickaway_pct >= 0))){ /* remove warning */
      //fprintf(stderr, "Warning: '%s' has no mean with Page Click Away.\n", temp_buf);
      return 1;
    }
  } else if (pass == 2) {

  
    session_idx = runProfTable[group_idx].sessprof_idx;

    for (i = 0; i < gSessionTable[session_idx].num_pages; i++) {
      if(runProfTable[group_idx].page_reload_table[i] != -1 || runProfTable[group_idx].page_reload_table[i] != -1) {
        //fprintf(stderr, "Warning: '%s' has no mean with Page Reload or Click Away for grp %d\n", temp_buf, group_idx);
        return 1;
      }
    }
  }
  return 0;
}

void delete_reload_or_clickaway_timer(connection *cptr)
{
  NSDL2_SCHEDULE(NULL, cptr, "Method Called");

  VUser *vptr = cptr->vptr;
  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;
  PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

  if(runprof_table_shr_mem[vptr->group_num].page_reload_table)
    pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number]; 

  if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];

  if((pagereload_ptr || pageclickaway_ptr) && cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE) 
  {
    NSDL2_SCHEDULE(NULL, cptr, "Deleting Page Reload or Click Away timer");
    dis_timer_del(cptr->timer_ptr); 
  }
}

// Set reload attempts if not  page reload keyword is given & not already set
static void set_reload_attempts(VUser *vptr)
{
  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;

  NSDL2_SCHEDULE(vptr, NULL, "Reload Attempt = %d, Page Number = %d", vptr->reload_attempts, vptr->cur_page->page_number);

  if(runprof_table_shr_mem[vptr->group_num].page_reload_table)
    pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number]; 

 //  a + (int) ((b - (a - 1)) * (rand() / (RAND_MAX + b)))
 //  if not set && no page reload prof entries 
 if(pagereload_ptr && vptr->reload_attempts == -1) {

   vptr->reload_attempts = pagereload_ptr->min_reloads + 
                           (int) ((pagereload_ptr->max_reloads - (pagereload_ptr->min_reloads - 1)) * (rand() /
                                (RAND_MAX + pagereload_ptr->max_reloads)));
   NSDL4_SCHEDULE(vptr, NULL, "Reload Attempt = %d, min reloads = %f", vptr->reload_attempts, pagereload_ptr->min_reloads);
 } 
}

void reload_connection( ClientData client_data, u_ns_ts_t now )
{
  connection* cptr;
  cptr = client_data.p;
  VUser *vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "Method called");

  reload_connection_callback(vptr, cptr, now);
}

void click_away_connection( ClientData client_data, u_ns_ts_t now )
{
  connection* cptr;
  cptr = client_data.p;
  VUser *vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "Method called");

  click_away_connection_callback(vptr, cptr, now);
}
/* Return 1 if page has to be reload.
 * Else   0
 */
inline void is_reload_page(VUser *vptr, connection *cptr, int *reload_page)
{
  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;

  //if(runprof_table_shr_mem[vptr->group_num].page_reload_table)
  pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number];

  if(pagereload_ptr) {
    NSDL3_SCHEDULE(vptr, cptr, "Method called, reload_timeout = %d, reload_attempt = %d", 
                              pagereload_ptr->reload_timeout, vptr->reload_attempts);

    if(cptr->url_num->proto.http.type == MAIN_URL && vptr->reload_attempts > 0 && cptr->req_ok == NS_REQUEST_RELOAD) {
      *reload_page = 1;
    } else {
      *reload_page = 0;
    }
  }
  else {
    *reload_page = 0;
  }
}

/* Return 1 if page has to be click away.
 * Else   0
 */
int is_clickaway_page(VUser *vptr, connection *cptr)
{
  PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;
  double pct;
  NSDL2_SCHEDULE(vptr, cptr, "Method called"); 

  if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];

  if(pageclickaway_ptr) {
    pct =  1.0 + (int) ((100.0 - (1.0 - 1)) * (rand() / (RAND_MAX + 100.0)));

    NSDL3_SCHEDULE(vptr, cptr, "type = %d, pct = %lf, clickaway_pct = %lf", cptr->url_num->proto.http.type, pct, pageclickaway_ptr->clickaway_pct); 

    if(cptr->url_num->proto.http.type == MAIN_URL && pageclickaway_ptr->clickaway_pct >= pct)
      return 1;
    else
      return 0;
  }
  else
    return 0;
}

// allocate page_reload_table
void initialize_runprof_page_reload_idx()
{
  
  NSDL2_MISC(NULL, NULL, "Method called");

  int i, num_pages;
  for (i = 0; i < total_runprof_entries; i++) {
    //sessProfIndexTable[runProfTable[i].sessprof_idx].
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages; 

    MY_MALLOC(runProfTable[i].page_reload_table, sizeof(int) * num_pages, "runProfTable[i].page_reload_table", i);
    memset(runProfTable[i].page_reload_table, -1, sizeof(int) * num_pages);
  }
}

// free page_reload_table
void free_runprof_page_reload_idx()
{
  NSDL2_MISC(NULL, NULL, "Method called");

  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = session_table_shr_mem[runProfTable[i].sessprof_idx].num_pages; 
    FREE_AND_MAKE_NULL_EX(runProfTable[i].page_reload_table, sizeof(int) * session_table_shr_mem[runProfTable[i].sessprof_idx].num_pages,"runProfTable[i].page_reload_table", i);
  }
}

// allocate page_clickaway_table
void initialize_runprof_page_clickaway_idx()
{
/* We dont know here we have Click Away Key or not
*/

  NSDL2_MISC(NULL, NULL, "Method called");

  int i, num_pages;
  for (i = 0; i < total_runprof_entries; i++) {
    //sessProfIndexTable[runProfTable[i].sessprof_idx].
    num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;

    MY_MALLOC(runProfTable[i].page_clickaway_table, sizeof(int) * num_pages, "runProfTable[i].page_clickaway_table", i);
    memset(runProfTable[i].page_clickaway_table, -1, sizeof(int) * num_pages);
  }
}

// free page_clickaway_table
void free_runprof_page_clickaway_idx()
{
  NSDL2_MISC(NULL, NULL, "Method called");

  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    //num_pages = session_table_shr_mem[runProfTable[i].sessprof_idx].num_pages;;
    FREE_AND_MAKE_NULL_EX(runProfTable[i].page_clickaway_table, sizeof(int) * session_table_shr_mem[runProfTable[i].sessprof_idx].num_pages,"runProfTable[i].page_clickaway_table", i);
  }
}

static int create_pagereloadprof_table_entry(int *row_num) {

  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_pagereloadprof_entries == max_pagereloadprof_entries) {
    MY_REALLOC_EX (pageReloadProfTable, (max_pagereloadprof_entries + DELTA_PAGERELOADPROF_ENTRIES) * sizeof(PageReloadProfTableEntry), max_pagereloadprof_entries * sizeof(PageReloadProfTableEntry),"pageReloadProfTable", -1);
    if (!pageReloadProfTable) {
      fprintf(stderr, "create_pagereloadprof_table_entry(): Error allcating more memory for ReloadProf entries\n");
      return FAILURE;
    } else max_pagereloadprof_entries += DELTA_PAGERELOADPROF_ENTRIES;
  }
  *row_num = total_pagereloadprof_entries++;
  return (SUCCESS);
}

static int create_pageclickawayprof_table_entry(int *row_num) {

  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_pageclickawayprof_entries == max_pageclickawayprof_entries) {
    MY_REALLOC_EX (pageClickAwayProfTable, (max_pageclickawayprof_entries + DELTA_PAGECLICKAWAYPROF_ENTRIES) * sizeof(PageClickAwayProfTableEntry), max_pageclickawayprof_entries * sizeof(PageClickAwayProfTableEntry), "pageReloadProfTable", -1);
    if (!pageClickAwayProfTable) {
      fprintf(stderr, "create_pageclickawayprof_table_entry(): Error allcating more memory for ClickAwayProf entries\n");
      return FAILURE;
    } else max_pageclickawayprof_entries += DELTA_PAGECLICKAWAYPROF_ENTRIES;
  }
  *row_num = total_pageclickawayprof_entries++;
  return (SUCCESS);
}

void create_default_reload_table()
{
  int rnum;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called");
  
  if (create_pagereloadprof_table_entry(&rnum) == FAILURE) {
       NS_EXIT(-1, "read_keywords(): Error in creating click away time entry");
  }

  pageReloadProfTable[rnum].min_reloads    = -1; 
  pageReloadProfTable[rnum].max_reloads    = -1;
  pageReloadProfTable[rnum].reload_timeout =  0;  //unisgned int
}
 
void create_default_click_away_table()
{
  int rnum;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called");
  if (create_pageclickawayprof_table_entry(&rnum) == FAILURE) {
       NS_EXIT(-1, "read_keywords(): Error in creating new click away time entry");
  }

  pageClickAwayProfTable[rnum].clicked_away_on   = -1; 
  pageClickAwayProfTable[rnum].clickaway_timeout = 0;
  pageClickAwayProfTable[rnum].clickaway_pct     = -1;
  pageClickAwayProfTable[rnum].call_check_page   = 0;
  pageClickAwayProfTable[rnum].transaction_status   = 0;
}

// G_PAGE_RELOAD <Group Name> <Page> <Min Reloads> <Max Reloads> <Timeout in ms>
int  kw_set_g_page_reload(char *buf, int pass, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char *session_name;
  char page_name[MAX_DATA_LINE_LENGTH];
  float min_reloads, max_reloads;
  int num;
  int reload_timeout = 0;
  int session_idx, page_id, rnum;

  num = sscanf(buf, "%s %s %s %f %f %d", keyword, sgrp_name, page_name, &min_reloads, &max_reloads, &reload_timeout);
  if (num < 5 || num > 6) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PAGE_RELOAD_USAGE, CAV_ERR_1011109, CAV_ERR_MSG_1);
  }
 
  if(min_reloads < 0 || max_reloads < 0 || reload_timeout < 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PAGE_RELOAD_USAGE, CAV_ERR_1011109, CAV_ERR_MSG_8);
  }

  if(min_reloads > max_reloads)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PAGE_RELOAD_USAGE, CAV_ERR_1011109, CAV_ERR_MSG_5);
  }

  if(!runtime_flag) {
    if (strcasecmp(sgrp_name, "ALL") == 0 && strcasecmp(page_name, "ALL") == 0){
        pageReloadProfTable[0].min_reloads    = min_reloads;
        pageReloadProfTable[0].max_reloads    = max_reloads;
        pageReloadProfTable[0].reload_timeout = reload_timeout;
    } else {
      int grp_idx;
      if ((grp_idx = find_sg_idx(sgrp_name)) == -1) {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PAGE_RELOAD_USAGE, CAV_ERR_1011286, sgrp_name, "");
      } else {
        session_idx = runProfTable[grp_idx].sessprof_idx;
        if (strcasecmp(page_name, "ALL") == 0) {
          int i;
          for (i = 0; i < gSessionTable[session_idx].num_pages; i++) {
             if (runProfTable[grp_idx].page_reload_table[i] == -1) {
                 if (create_pagereloadprof_table_entry(&rnum) == FAILURE) {
                   fprintf(stderr, "read_keywords(): Error in creating new think time entry\n");
                   exit(-1);
                 }
                 pageReloadProfTable[rnum].min_reloads    = min_reloads;
                 pageReloadProfTable[rnum].max_reloads    = max_reloads;
                 pageReloadProfTable[rnum].reload_timeout = reload_timeout;
                
                 runProfTable[grp_idx].page_reload_table[i] = rnum;
                 NSDL3_MISC(NULL, NULL, "ALL runProfTable[%d].page_reload_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_reload_table[i]);
             } else {
               NSDL3_MISC(NULL, NULL, "ELSE runProfTable[%d].page_reload_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_reload_table[i]);
             }
          } 
        } else {
            if ((page_id = find_page_idx(page_name, session_idx)) == -1) {
              session_name = RETRIEVE_BUFFER_DATA(gSessionTable[session_idx].sess_name);
              NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PAGE_RELOAD_USAGE, CAV_ERR_1011110, page_name, sgrp_name, session_name, "");
            }
              
            if (create_pagereloadprof_table_entry(&rnum) == FAILURE) {
              fprintf(stderr, "read_keywords(): Error in creating new think time entry\n");
              exit(-1);
            }
  
            pageReloadProfTable[rnum].min_reloads    = min_reloads;
            pageReloadProfTable[rnum].max_reloads    = max_reloads;
            pageReloadProfTable[rnum].reload_timeout = reload_timeout;
  
            runProfTable[grp_idx].page_reload_table[page_id - gSessionTable[session_idx].first_page] = rnum;
            NSDL3_MISC(NULL, NULL, "runProfTable[%d].page_reload_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                       grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                       runProfTable[grp_idx].page_reload_table[page_id - gSessionTable[session_idx].first_page]);
        }
      }
    }
   
  } else {
    NSTL1(NULL, NULL, "Run time changes is not enabled for G_PAGE_RELOAD currently.");
  }

  return 0;
}

// G_PAGE_CLICK_AWAY <Group Name> <Page> <Next page> <pct> <Time in ms> <Call check page or not> <transaction status>
int  kw_set_g_page_click_away(char *buf, int pass, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char page_name[MAX_DATA_LINE_LENGTH];
  char clicked_away_on[MAX_DATA_LINE_LENGTH] = "\0";
  int clicked_away_page_id;
  float clickaway_pct = 0.0; 
  int num;
  int clickaway_timeout;
  int session_idx, page_id, rnum;
  int call_check_page = 0;
  int transaction_status = 0;

  // G_CLICK_AWAY ALL 2 5 0 0 1
  // G_PAGE_CLICK_AWAY <Group Name> <Page> <Next page> <pct> <Time in ms> <Call check page or not> <transaction status>
  num = sscanf(buf, "%s %s %s %s %f %d %d %d", keyword, sgrp_name, page_name, clicked_away_on, &clickaway_pct, &clickaway_timeout, &call_check_page, &transaction_status);
  if (num < 5 || num > 8) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011289, CAV_ERR_MSG_1);
  }
  if(strcmp(clicked_away_on, "ALL") == 0)
  {
    fprintf(stderr, "Clicked Away page can not have ALL.\n");
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011274, "");
  }

  if(clickaway_pct < 0.0  || clickaway_pct > 100)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011289, CAV_ERR_MSG_6);
  }

  if(clickaway_timeout < 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011289, CAV_ERR_MSG_8);
  }

  if(call_check_page != 0 && call_check_page != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011289, CAV_ERR_MSG_3);
  }
  if(transaction_status != 0 && transaction_status != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011289, CAV_ERR_MSG_3);
  }

  if (!runtime_flag) {
    if ((strcasecmp(sgrp_name, "ALL") == 0) && (strcasecmp(page_name, "ALL") == 0)) {
        pageClickAwayProfTable[0].call_check_page   = call_check_page;

      if(strcmp(clicked_away_on, "-1") == 0) {
         clicked_away_page_id = -1;
      }
      else {
         NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011275, "");
      }
        
      pageClickAwayProfTable[0].clicked_away_on   = clicked_away_page_id; 
      pageClickAwayProfTable[0].clickaway_timeout = clickaway_timeout;
      pageClickAwayProfTable[0].clickaway_pct     = clickaway_pct;

    } else {
      int grp_idx;
      if ((grp_idx = find_sg_idx(sgrp_name)) == -1) {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011276, sgrp_name, "");
      } else {
        session_idx = runProfTable[grp_idx].sessprof_idx;
        if(strcmp(clicked_away_on, "-1") == 0) {
           clicked_away_page_id = -1;
        } else { 
            if( (clicked_away_page_id = find_page_idx(clicked_away_on, session_idx)) < 0) {
              NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011277, clicked_away_on, sgrp_name, "");
            }
        }
        /* We need to get relative page index.
        *  Suppose we have 2 two grps have both have different scripts, script 1 has 7 & 2 has 5 pages.
        *  first page of script 2 will start from index 7 If we use find_page_idx to get 3 page of script 2 it returns index 10,
        *  but we need 10 - 7, the 3rd page. */
        if(clicked_away_page_id != -1)
          clicked_away_page_id = clicked_away_page_id - gSessionTable[session_idx].first_page;
        if (strcasecmp(page_name, "ALL") == 0) {
          int i;
          for (i = 0; i < gSessionTable[session_idx].num_pages; i++) {
             if (runProfTable[grp_idx].page_clickaway_table[i] == -1) {
                 create_pageclickawayprof_table_entry(&rnum);
                 pageClickAwayProfTable[rnum].call_check_page   = call_check_page;
                 pageClickAwayProfTable[rnum].clicked_away_on   = clicked_away_page_id;
                 pageClickAwayProfTable[rnum].clickaway_timeout = clickaway_timeout;
                 pageClickAwayProfTable[rnum].clickaway_pct     = clickaway_pct;
                 pageClickAwayProfTable[rnum].transaction_status     = transaction_status;
                  
                 runProfTable[grp_idx].page_clickaway_table[i] = rnum;
                 NSDL3_MISC(NULL, NULL, "ALL runProfTable[%d].page_reload_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_clickaway_table[i]);
             } else {
               NSDL3_MISC(NULL, NULL, "ELSE runProfTable[%d].page_reload_table[%d] = %d\n", grp_idx, i, runProfTable[grp_idx].page_clickaway_table[i]);
             }
          } 
        } else {
            if ((page_id = find_page_idx(page_name, session_idx)) == -1) {
              NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CLICK_AWAY_USAGE, CAV_ERR_1011278, page_name, sgrp_name, "");
            }
            
            create_pageclickawayprof_table_entry(&rnum);

            pageClickAwayProfTable[rnum].call_check_page   = call_check_page;
            pageClickAwayProfTable[rnum].clicked_away_on   = clicked_away_page_id;
            pageClickAwayProfTable[rnum].clickaway_timeout = clickaway_timeout;
            pageClickAwayProfTable[rnum].clickaway_pct     = clickaway_pct;
            pageClickAwayProfTable[rnum].transaction_status     = transaction_status;
  
            runProfTable[grp_idx].page_clickaway_table[page_id - gSessionTable[session_idx].first_page] = rnum;
            NSDL3_MISC(NULL, NULL, "runProfTable[%d].page_reload_table[page_id(%d) - gSessionTable[%d].first_page(%d)] = %d\n",
                       grp_idx, page_id, session_idx, gSessionTable[session_idx].first_page,
                       runProfTable[grp_idx].page_clickaway_table[page_id - gSessionTable[session_idx].first_page]);
        }
      }
    }
  }
  else {
    NSTL1(NULL, NULL, "Run time changes is not enabled for G_CLICK_AWAY currently.");
  }

  return 0;
}

void copy_pagereload_into_shared_mem()
{
  int actual_num_pages = 0;
  int i, j, k = 0;
  
  NSDL2_MISC(NULL, NULL, "Method called");

  for (i = 0; i < total_runprof_entries; i++) {
    actual_num_pages += runProfTable[i].num_pages;
    NSDL4_MISC(NULL, NULL, "actual_num_pages = %d", actual_num_pages);
  }

  pagereloadprof_table_shr_mem = (PageReloadProfTableEntry_Shr*) do_shmget(sizeof(PageReloadProfTableEntry_Shr) * actual_num_pages, "page reload profile table");

  for (i = 0; i < total_runprof_entries; i++) {
    NSDL4_MISC(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
    for (j = 0; j < runProfTable[i].num_pages; j++) {
      int idx = runProfTable[i].page_reload_table[j] == -1 ? 0 : runProfTable[i].page_reload_table[j];
      NSDL4_MISC(NULL, NULL, "runProfTable[%d].page_reload_table[%d] = %d, idx = %d, k = %d", 
                 i, j, runProfTable[i].page_reload_table[j], idx, k);
      
      pagereloadprof_table_shr_mem[k].min_reloads = pageReloadProfTable[idx].min_reloads;
      pagereloadprof_table_shr_mem[k].max_reloads = pageReloadProfTable[idx].max_reloads;
      pagereloadprof_table_shr_mem[k].reload_timeout = pageReloadProfTable[idx].reload_timeout;
      k++;
    }
  }
}

void copy_pageclickaway_into_shared_mem()
{
  int actual_num_pages = 0;
  int i, j, k = 0;
  
  NSDL2_MISC(NULL, NULL, "Method called");

  for (i = 0; i < total_runprof_entries; i++) {
    actual_num_pages += runProfTable[i].num_pages;
    NSDL4_MISC(NULL, NULL, "actual_num_pages = %d", actual_num_pages);
  }

  pageclickawayprof_table_shr_mem = (PageClickAwayProfTableEntry_Shr*) do_shmget(sizeof(PageClickAwayProfTableEntry_Shr) * actual_num_pages, "page reload profile table");

  for (i = 0; i < total_runprof_entries; i++) {
    NSDL4_MISC(NULL, NULL, "runProfTable[%d].num_pages = %d", i, runProfTable[i].num_pages);
    for (j = 0; j < runProfTable[i].num_pages; j++) {
      int idx = runProfTable[i].page_clickaway_table[j] == -1 ? 0 : runProfTable[i].page_clickaway_table[j];
      NSDL4_MISC(NULL, NULL, "runProfTable[%d].page_reload_table[%d] = %d, idx = %d, k = %d", 
                 i, j, runProfTable[i].page_reload_table[j], idx, k);
      
      pageclickawayprof_table_shr_mem[k].call_check_page = pageClickAwayProfTable[idx].call_check_page;
      pageclickawayprof_table_shr_mem[k].clicked_away_on = pageClickAwayProfTable[idx].clicked_away_on;
      pageclickawayprof_table_shr_mem[k].clickaway_timeout = pageClickAwayProfTable[idx].clickaway_timeout;
      pageclickawayprof_table_shr_mem[k].clickaway_pct = pageClickAwayProfTable[idx].clickaway_pct;
      pageclickawayprof_table_shr_mem[k].transaction_status = pageClickAwayProfTable[idx].transaction_status;
      k++;
    }
  }
}

static int
get_repeat_count(VUser *vptr, action_request_Shr *cur_url)
{
  int repeat_count = 0;
  char buf[1024 + 1];
  int size = 0;

  //get REPEAT count
  if(cur_url->proto.http.repeat_inline.num_entries == 1 && cur_url->proto.http.repeat_inline.seg_start->type == STR)
  {
    NSDL2_HTTP(vptr, NULL, "repeat_inline: CASE 1 entry=1 and type STR (%d) ",
                            cur_url->proto.http.repeat_inline.seg_start->type);

    size = cur_url->proto.http.repeat_inline.seg_start->seg_ptr.str_ptr->size;
    strncpy(buf, cur_url->proto.http.repeat_inline.seg_start->seg_ptr.str_ptr->big_buf_pointer, size);
    buf[size] = '\0';
    repeat_count = atoi(buf);
  }
  else if(cur_url->proto.http.repeat_inline.num_entries >= 1)
  {
    /* In case it is parameterized using {<paramname>} in script file, num_entries is always 2 (VAR and STR) */
    NSDL2_HTTP(vptr, NULL, "repeat_inline: CASE 2 entries=%d and type=%d",
                            cur_url->proto.http.repeat_inline.num_entries,
                            cur_url->proto.http.repeat_inline.seg_start->type);

    if(get_full_element(vptr, &cur_url->proto.http.repeat_inline, buf, &size) < 0)
      return 0;
    buf[size]='\0';
    repeat_count = atoi(buf);
  }
  else
    error_log("Error: Unable to fetch repeat count.");

  NSDL2_SCHEDULE(vptr, NULL, "Repeat = [%d]", repeat_count);

  return repeat_count;
}

/*

  This function is used to make copy of URL (action_request_Shr) for handling of inline repeat or delay.
  For delay, we do not need to make more URLs but since we need to keep schedule time in the URL, we are making copy
*/

static inline void
setup_urls_for_repeat(VUser *vptr)
{
HostSvrEntry *hel;
int num_heurls; // Total number of URLs for a host
int repeat_count;
short i, j;
char tx_name[512];
// int url_index; // Url index of a URL in the host
action_request_Shr *cur_url;
action_request_Shr *cur_url_allocated;
action_request_Shr *allocated_urls; // Array of all URLs including repeat of a Host (This will be freed after page is done)

  NSDL2_SCHEDULE(vptr, NULL, "Method called");

  hel = vptr->head_hlist;

  vptr->urls_awaited = vptr->urls_left = 0;
  while (hel) // For all hosts of a page
  {
    num_heurls = 0;
    //url_index = 0;
    cur_url = hel->cur_url; // Point the first URL (shared memory) of this host

    allocated_urls = NULL;
    NSDL3_SCHEDULE(vptr, NULL, "Setting up URLs for host, num_urls = [%d]", hel->hurl_left);
    for(i = 0; i < hel->hurl_left; i++) // For all urls of a host
    {
      
      repeat_count = get_repeat_count(vptr, cur_url) + 1;
      NSDL3_SCHEDULE(vptr, NULL, "Make copy of URL %d. Repeat count = [%d]", i, repeat_count);
      // Malloc action_request_Shr
      MY_REALLOC(allocated_urls, (num_heurls + repeat_count) * sizeof(action_request_Shr), "allocated_urls for repeat", num_heurls);
      cur_url_allocated = allocated_urls + num_heurls;
      //In a loop, do memcopy from Shr to malloc
      for(j = 0; j < repeat_count; j++)
      {
        NSDL3_SCHEDULE(vptr, NULL, "copying URL %d for repeat num = %d", i, j);
        memcpy(cur_url_allocated, cur_url, sizeof(action_request_Shr));
        if (!cur_url->parent_url_num)
          cur_url_allocated->parent_url_num = cur_url;
        //Generate txName for all repeated urls
        if(cur_url->proto.http.tx_prefix) {
          sprintf(tx_name, "%s_%d", cur_url->proto.http.tx_prefix, j + 1);
          NSDL3_SCHEDULE(vptr, NULL, "Adding Tx hash idx for url [%d], tx_name = [%s] ", j, tx_name);
          cur_url_allocated->proto.http.tx_hash_idx = tx_hash_func(tx_name, strlen(tx_name));
        }
        cur_url_allocated++; 
        num_heurls++;
      } 

      cur_url++;
    }
    hel->cur_url = hel->cur_url_head = allocated_urls; // Point to allocated URL array for this host

    hel->hurl_left = num_heurls; // Update with new num urls
    vptr->urls_awaited = vptr->urls_left += num_heurls;
    NSDL3_SCHEDULE(vptr, NULL, "hel->hurl_left = [%d],  vptr->urls_awaited = [%d], vptr->urls_left = [%d], num_heurls = [%d]",
                                hel->hurl_left, vptr->urls_awaited, vptr->urls_left, num_heurls);
    NSDL3_SCHEDULE(vptr, NULL, " hel->cur_url_head = %p", hel->cur_url_head);
    hel = (HostSvrEntry *)(hel->next_hlist);
  }

  vptr->urls_awaited = vptr->urls_left += 1; // TODO -For main urls - NEed to check again
  NSDL3_SCHEDULE(vptr, NULL, "vptr->urls_awaited = [%d], vptr->urls_left = [%d]", vptr->urls_awaited, vptr->urls_left);

}


inline void
static get_page_hosts (VUser *vptr, PageTableEntry_Shr *page)
{
  HostTableEntry_Shr *hel;
  NSDL3_SCHEDULE(vptr, NULL, "Method called");
  vptr->head_hlist = vptr->tail_hlist = NULL;
  hel = page->head_hlist;

  NSDL3_SCHEDULE(vptr, NULL, "hel = %p", hel);
  while (hel) {
    add_to_hlist(vptr, hel);
    hel = hel->next;
  }
  return;
}


inline void
on_page_start(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method Called. vptr[%p]->head_hlist=%p", vptr, vptr->head_hlist);
  if(!global_settings->replay_mode)
    vptr->inline_req_delay = -1; // Set it here, it is used to check first inline, while applying inline delay

  // Calculate reload attempts using max & min reload
  set_reload_attempts(vptr);

  NS_DT2(vptr, NULL, DM_L1, MM_SCHEDULE, "Starting execution of page '%s'", vptr->cur_page->page_name);
  average_time->pg_fetches_started++;

  int id;

  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS) 
  {
    id = (vptr->runtime_runlogic_flow_id + (vptr->cur_page->page_num_relative_to_flow + 1));
    NSDL2_SCHEDULE(vptr, NULL, "Updating the runtime runlogic progress data id = %d, vptr->runtime_runlogic_flow_id = %d, vptr->cur_page->page_num_relative_to_flow = %d", id, vptr->runtime_runlogic_flow_id, vptr->cur_page->page_num_relative_to_flow);
    update_user_flow_count_ex(vptr, id);
  }
 
  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->pg_fetches_started++;
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_LEGACY)
  {
    NSDL2_SCHEDULE(vptr, NULL, "Script Mode 0");
    calc_page_think_time(vptr);
    if (vptr->cur_page->prepage_func_ptr)
    {
      TLS_SET_VPTR(vptr);
      cur_time = now; // TODO: What is the purpose of this
      NS_DT3(vptr, NULL, DM_L1, MM_SCHEDULE, "Starting execution of pre_page() for page '%s'", vptr->cur_page->page_name);
      vptr->cur_page->prepage_func_ptr();
      NS_DT3(vptr, NULL, DM_L1, MM_SCHEDULE, "Completed execution of pre_page() for page '%s'",
                             vptr->cur_page->page_name);
    }
  }

  vptr->url_num = 0;
  vptr->bytes = 0;
  vptr->next_page = NULL; // TODO: Do we need this?
  vptr->page_instance++;

  //HTTP protocol is enabled then the bit flag
  if(global_settings->protocol_enabled & HTTP_PROTOCOL_ENABLED) {
    vptr->flags |= NS_VPTR_FLAGS_HTTP_USED;
  }

  // If last page has been reloaded then no need start page as transation
  if(vptr->page_status != NS_REQUEST_RELOAD)
  {
    // reset here it may possible page status is used in tx_begin_on_page_start some where
    vptr->page_status = NS_REQUEST_OK;
    tx_begin_on_page_start( vptr, now);
  }
  vptr->page_status = NS_REQUEST_OK;
 
  vptr->first_page_url = vptr->cur_page->first_eurl;
  vptr->urls_awaited = vptr->urls_left = vptr->cur_page->num_eurls;
  /*bug 54315:  code updated to reset main urls->hurl_left = 0*/
  HostSvrEntry * temp_hptr = vptr->hptr + vptr->first_page_url->index.svr_ptr->idx;
  temp_hptr->hurl_left = 0 ;
  NSDL2_SCHEDULE(vptr, NULL, "vptr->hptr=%p vptr->first_page_url->index.svr_ptr->idx=%d temp_hptr[%p]->hurl_left=%d", vptr->hptr, vptr->first_page_url->index.svr_ptr->idx, temp_hptr, temp_hptr->hurl_left);
  //vptr->urls_left = -1;

  NSDL2_SCHEDULE(vptr, NULL, "vptr->first_page_url=%p, vptr->urls_awaited = [%d], num_eurls = %d vptr->first_page_url[%p]->index.svr_ptr->idx=%d", vptr->first_page_url, vptr->urls_awaited, vptr->cur_page->num_eurls, vptr->first_page_url,  vptr->first_page_url->index.svr_ptr->idx);

  get_page_hosts(vptr, vptr->cur_page);
                          
                          
  #ifdef NS_DEBUG_ON
    dump_URL_SHR(vptr->cur_page);
  #endif
  
  vptr->pg_begin_at = now;
  //Abhishek
  if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){
    NSDL2_USER_TRACE(vptr, NULL, "User tracing enable for %p vptr", vptr);
    ut_add_page_node(vptr);
  }
  //Reset first 16 bits of flags variable
  vptr->flags = vptr->flags & RESET_GRP_FIRST_32_BITS;
  NSDL2_SCHEDULE(vptr, NULL, "Resetting vptr->flags with RESET_GRP_FIRST_32_BITS. vptr->flags = [%x]", vptr->flags);

  // Set vptr flag for inline repeat and/or delay
  // We are doing it so that we do not have to make complex check at other places
  // This code MUST be after reset of flags
  InlineDelayTableEntry_Shr *delay_ptr;
  delay_ptr = (InlineDelayTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].inline_delay_table[vptr->cur_page->page_number];

  if(delay_ptr->mode > 0) // Delay is to be applied
  {
    vptr->flags |= NS_VPTR_FLAGS_INLINE_DELAY; 
    NSDL2_SCHEDULE(vptr, NULL, "InLine Delay is present in this page. delay_ptr->mode = [%d], vptr->flags = [%x]", 
                   delay_ptr->mode, vptr->flags);
  }
  
  NSDL2_SCHEDULE(vptr, NULL, "vptr->cur_page->flags = [%x]", vptr->cur_page->flags);
  if(vptr->cur_page->flags & PAGE_WITH_INLINE_REPEAT)
  {
    vptr->flags |= NS_VPTR_FLAGS_INLINE_REPEAT;
    NSDL2_SCHEDULE(vptr, NULL, "InLine repeat is present in this page. vptr->flags = [%x]", vptr->flags);
  }

  // If either inline repeat or delay is to be done, then we need to make copy of URL for fetching
  if(vptr->flags & NS_VPTR_FLAGS_INLINE_REPEAT_DELAY_MASK)
    setup_urls_for_repeat(vptr);
    
  //In release 3.9.7, in case of NDE save partition index where we need to dump request/response files
  //whereas in non partition request response files will logged in test run directory
  vptr->partition_idx = g_partition_idx; 
  NSDL1_HTTP(vptr, NULL, "Save partition index  %lld", vptr->partition_idx);
}

// New script design
// This replaces execute first page and execute next page
void execute_page( VUser *vptr, int page_id, u_ns_ts_t now)
{
  connection* cptr;

  cptr = vptr->last_cptr;
  action_request_Shr *url_num = vptr->first_page_url;

  NSDL1_HTTP(vptr, cptr, "Method called: vptr=%p, cptr=%p, page_id = %d", vptr, cptr, page_id);
  //dump_parallel(vptr);

  // Neeraj - Jun 7, 2011 - This is done in nsi_web_url. So no need to do it here
  // vptr->cur_page = vptr->sess_ptr->first_page + page_id;

  assert(vptr->cur_page);

  /* Bug 70903 - on calling ns_url_get_resp_msg and ns_url_get_resp_size after 3 pages it is returning
                 header message of all 3 pages and header size of  all 3 pages */
  if(vptr->response_hdr)
     vptr->response_hdr->used_hdr_buf_len = 0;
 
  // This is for first page of new user only
  // TODO: Assuming last cptr is NOT null for reuse user. need to check this logic
  //Bug#2426
  if((vptr->page_instance == 0) && !(vptr->flags & NS_REUSE_USER)) // First page of session && New User
  {
    // TODO: Can we eliminate this block for first page

    vptr->urls_left--;
    NSDL1_HTTP(vptr, NULL, "urls_left at new_user %d, idx = %d", vptr->urls_left, (get_svr_ptr(vptr->first_page_url, vptr)->idx));
    //vptr->start_url_ridx = 1; //index 0 is the main URL. Set to 1 for further processing
                                //Main is taken care right here
    (vptr->hptr + get_svr_ptr(vptr->first_page_url, vptr)->idx)->num_parallel = 1;

    vptr->cnum_parallel = 1;
    //dump_con(vptr, "First Page", NULL);

    if(url_num->request_type != JRMI_REQUEST){
      NSDL1_HTTP(vptr, NULL, "Before get_free_connection_slot cptr=%p", cptr);
      cptr = get_free_connection_slot(vptr);

      NSDL1_HTTP(vptr, NULL, "Before cptr->url_num = %p", cptr->url_num); 
      SET_URL_NUM_IN_CPTR(cptr, url_num);
      NSDL1_HTTP(vptr, NULL, "After cptr->url_num = %p", cptr->url_num); 
      cptr->num_retries = 0;
      
      //Registering function for handling receive data
      if((cptr->request_type == SOCKET_REQUEST) || (cptr->request_type == SSL_SOCKET_REQUEST))
      {
        cptr->proc_recv_data = process_socket_recv_data;
      }

      NSDL2_WS(vptr, cptr, "request_type = %d", cptr->url_num->request_type);
      if((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
      {
        vptr->ws_cptr[ws_idx_list[vptr->first_page_url->proto.ws.conn_id]] = cptr;
        NSDL2_WS(vptr, cptr, "vptr->first_page_url->proto.ws.conn_id = %d, ws_conn id = %d, "
                             "vptr->ws_cptr[vptr->first_page_url->proto.ws.conn_id] = %p", 
                             vptr->first_page_url->proto.ws.conn_id, ws_idx_list[vptr->first_page_url->proto.ws.conn_id],
                             vptr->ws_cptr[ws_idx_list[vptr->first_page_url->proto.ws.conn_id]]); 
      }
      NSDL1_HTTP(vptr, NULL, "After get_free_connection_slot cptr=%p, request_type = %d, header_state = %d", 
                              cptr, cptr->url_num->request_type, cptr->header_state);
    } else {
      jrmi_con_setup(vptr, url_num, now);
      init_trace_up_t(vptr);
      return; 
    }

    // To init used_param table for page dump and user trace 
    init_trace_up_t(vptr);
    if(cptr->url_num->is_url_parameterized)
    {
      char *loc_url;
      int loc_url_len;
      action_request_Shr *loc_url_num; 
      SvrTableEntry_Shr *svrptr;
     
      NSDL2_CONN(vptr, NULL, "Host is parameterized");
      if(( loc_url_num = process_segmented_url(vptr, cptr->url_num, &loc_url, &loc_url_len)) == NULL)
      {
        NSTL2(NULL, NULL, "Start Socket: Unknown host.");
        NSDL2_CONN(vptr, NULL, "Start Socket: Unknown host.");
        int status = NS_REQUEST_ERRMISC;
        if(cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST)
        {
          INC_WS_WSS_FETCHES_STARTED_COUNTER(vptr);
          INC_WS_WSS_NUM_TRIES_COUNTER(vptr); 
        }
        else if(cptr->url_num->request_type == HTTP_REQUEST || cptr->url_num->request_type == HTTPS_REQUEST)
        {
          INC_HTTP_FETCHES_STARTED_COUNTER(vptr);
          INC_HTTP_HTTPS_NUM_TRIES_COUNTER(vptr); 
        }
        vptr->page_status = NS_REQUEST_URL_FAILURE;
        vptr->sess_status = NS_REQUEST_ERRMISC;
        calc_pg_time(vptr, now);
        tx_logging_on_page_completion(vptr, now);
        vptr->cnum_parallel = 0;
        HANDLE_URL_PARAM_FAILURE(vptr);
        return;
      }
      (vptr->hptr + cptr->url_num->index.svr_ptr->idx)->num_parallel--;
      svrptr = loc_url_num->index.svr_ptr;
      (vptr->hptr + svrptr->idx)->num_parallel++;
      NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d, svrptr->idx = %d", loc_url, loc_url_len, svrptr->idx);
      cptr->url = loc_url;
      cptr->url_len = loc_url_len;
      cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
      SET_URL_NUM_IN_CPTR(cptr, loc_url_num);                                 
    }
    // To init used_param table for page dump and user trace 
    //init_trace_up_t(vptr); 
    start_new_socket(cptr, now );
  }
  else
  {
    // To init used_param table for page dump and user trace 
    NSDL2_HTTP(vptr, NULL, "in else part of new user, cptr = %p", cptr);
    init_trace_up_t(vptr);

    //we must execute first URL anyhow
    if (cptr) {
      NSDL1_HTTP(vptr, NULL, "In else get_free_connection_slot cptr=%p, request_type = %d, header_state = %d", 
                              cptr, cptr->request_type, cptr->header_state);
      if(cptr->request_type == JRMI_REQUEST){
        jrmi_con_setup(vptr, vptr->first_page_url, now);
      }else{
        if  (!try_url_on_cur_con (cptr, vptr->first_page_url, now)) {
          if (!try_url_on_any_con (vptr, vptr->first_page_url, now, NS_DO_NOT_HONOR_REQUEST)) {
            printf("execute_page:Unable to run Main URL\n");
            end_test_run();
          }
        }
      }
    } else {
      if(url_num->request_type == JRMI_REQUEST){
        jrmi_con_setup(vptr, vptr->first_page_url, now);
       }else if (!try_url_on_any_con (vptr, vptr->first_page_url, now, NS_DO_NOT_HONOR_REQUEST)) {
        printf("execute_page:Unable to run Main URL, vptr = %p\n",vptr);
        end_test_run();
      }
    }
    // Do this for next page only
    // TODO: may be we can do for first page as well as bit may not be set
    //       and it does not harm to reset
    RESET_BIT_FLAG_NS_EMBD_OBJS_SET_BY_API(vptr);
    /*Reset this bit as it is checked in http_close_connection*/
  }
}

inline void script_exit_log(VUser *vptr, u_ns_ts_t now)
{
  //For NetTest

  if((vptr->httpData->rbu_resp_attr) && 
       ((global_settings->protocol_enabled & RBU_API_USED) && runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu))
  {
    RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
    vptr->sess_ptr->netTest_start_time = now - vptr->sess_ptr->netTest_start_time;

    if(vptr->sess_status == 0)
    {  
      NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Success; flowName=%s; Page=All; Session=%s; "
                   "Cookie_CavNVC=%s; Cookie_CavNV=%s; Duration=%llu", 
                   get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"), vptr->cur_page->flow_name,
                   vptr->sess_ptr->sess_name, rbu_resp_attr->netTest_CavNVC_cookie_val, rbu_resp_attr->netTest_CavNV_cookie_val,
                   vptr->sess_ptr->netTest_start_time);
    }
    else
    {
      NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Failure; flowName=%s; Page=%s; Session=%s; "
                   "Cookie_CavNVC=%s; Cookie_CavNV=%s; Duration=%llu; ErrorType=%s; MSG=%s; "
                   "PageDumpImg=page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.jpeg",
                   get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
                   vptr->cur_page->flow_name, vptr->cur_page->page_name, vptr->sess_ptr->sess_name, rbu_resp_attr->netTest_CavNVC_cookie_val,
                   rbu_resp_attr->netTest_CavNV_cookie_val, vptr->sess_ptr->netTest_start_time, get_error_code_name(vptr->page_status),
                   script_execution_fail_msg, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                   vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    }
  }
  NS_DT1(vptr, NULL, DM_L1, MM_SCHEDULE, "Completed execution of script '%s'", vptr->sess_ptr->sess_name);
}

//execute function pointer from scripts
void run_script_exit_func(VUser *vptr/*, u_ns_ts_t now*/)
{

  vptr->flags |= NS_VPTR_FLAGS_SESSION_EXIT;

  if (vptr->sess_ptr->exit_func_ptr)
  {
    NSDL2_SCHEDULE(vptr, NULL, "Method Called. exit_script function is defined. Calling exit_script function");
    TLS_SET_VPTR(vptr);
    cur_time = vptr->now;
    NS_DT2(vptr, NULL, DM_L1, MM_SCHEDULE, "Starting execution of exit_script()");
    vptr->sess_ptr->exit_func_ptr();
    NS_DT2(vptr, NULL, DM_L1, MM_SCHEDULE, "Completed execution of exit_script()");
  }
  else
  {
    NSDL2_SCHEDULE(vptr, NULL, "Method Called. exit_script function is not defined. So not calling exit_script function");
  }

  ns_exit_session_ext(vptr);
}

#if 1
/*This log event to event log file in case of any page error*/
static void log_page_error_to_event(VUser *vptr, connection*cptr, int request_type) {

  NSDL2_SCHEDULE(vptr, NULL, "Method called, request_type = %d", request_type);

  switch(request_type) {
   case HTTP_REQUEST:
   case HTTPS_REQUEST:
     NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;
   case SMTP_REQUEST:
   case SMTPS_REQUEST:
     NS_EL_4_ATTR(EID_SMTP_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;
   case POP3_REQUEST:
   case SPOP3_REQUEST:
     NS_EL_4_ATTR(EID_POP3_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;
   case DNS_REQUEST:
     NS_EL_4_ATTR(EID_DNS_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;
   case FTP_REQUEST:
     NS_EL_4_ATTR(EID_FTP_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;
  /* case IMAP_REQUEST:   //TODO:
   case IMAPS_REQUEST:
     NS_EL_4_ATTR(EID_IMAP_PAGE_ERR_START + vptr->page_status, vptr->user_index,
                                 vptr->sess_inst,
                                 EVENT_CORE, EVENT_MAJOR, vptr->sess_ptr->sess_name,
                                 vptr->cur_page->page_name,
                                 nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                 get_request_string(cptr),
                                 "Page failed with status %s", get_error_code_name(vptr->page_status));
     break;*/
  }

}
#endif

/*
  Case 0 - page id or -1
  Other Cases - 0 - continue, -1 do not continue

  Three cases:
    Hiting next
    Hiting same page
    End of session
*/

// #define NS_NEXT_PG_USE_CUR_PAGE -2
//#define NS_NEXT_PG_STOP_SESSION -1

/*
  Page reload and clickaway have issue of transactions starting/ending
  Options:
   - Open another transaction

*/

static int get_next_page(VUser* vptr, u_ns_ts_t now){
  int next_page;
  int call_check_page_or_not = -1;

  NSDL2_HTTP(vptr, NULL, "Method called, page_status = %d", vptr->page_status);
  TLS_SET_VPTR(vptr);
  cur_time = now;

  // In Reload check page will not be called as we are reloading a page
  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    // On reload do not a call a check page funct
    NSDL4_HTTP(vptr, NULL, "Cur page %s is reloaded. attempt = %d", vptr->cur_page->page_name, vptr->reload_attempts);
    vptr->reload_attempts--;

    // return (vptr->cur_page);
    // return (NS_NEXT_PG_USE_CUR_PAGE); // TODO: return cur page id
    return (vptr->next_pg_id); 
  }

  // Reset when page is not to be reloaded
  NSDL4_SCHEDULE(vptr, NULL, "Setting reload attempt to -1 in page %s", vptr->cur_page->page_name);
  vptr->reload_attempts = -1;

  if(vptr->page_status == NS_REQUEST_CLICKAWAY)
  {
    PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

    if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
      pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];
    next_page = pageclickaway_ptr->clicked_away_on;
    call_check_page_or_not = pageclickaway_ptr->call_check_page;
    NSDL2_HTTP(vptr, NULL, "Page has to be click away, next page = %d", next_page);
  }
  else
  {
     if(vptr->sess_status == NS_USEONCE_ABORT || vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION)
       return NS_NEXT_PG_STOP_SESSION;

     /* if Page error and [ continue_on_page_error is 0 and action is stop] then return.*/
     ContinueOnPageErrorTableEntry_Shr *ptr;
     ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];
     if ((vptr->page_status != NS_REQUEST_OK) &&
       ((ptr->continue_error_value == 0) &&
       (!(vptr->flags & NS_ACTION_ON_FAIL_CONTINUE))))
    {
      NSDL2_HTTP(NULL, NULL, "returning NULL. ptr->continue_error_value = %d, vptr->flags = 0x%x\n",
          ptr->continue_error_value, vptr->flags);
      //vptr->flags &= ~NS_ACTION_ON_FAIL_CONTINUE; // Must clear this flag
      return (NS_NEXT_PG_STOP_SESSION);
    }

    if(vptr->flags & NS_ACTION_ON_FAIL_CONTINUE)
      vptr->flags &= ~NS_ACTION_ON_FAIL_CONTINUE; // Must clear this flag
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_LEGACY)
  {
    // If scenario is replay access logs, then call method to set next page to be executed in a NS variable which is used in the Runlogic of the script
    if(global_settings->replay_mode)    
      set_next_replay_page(vptr);
    if(vptr->page_status == NS_REQUEST_CLICKAWAY)
    {
       NSDL2_HTTP(vptr, NULL, "Page has to be click away, and script type is C Type, so return end of the session");
       return (NS_NEXT_PG_STOP_SESSION); // Session is over
    }
    return 0; // Continue normal execution to next line in the script C code
  }

  if(call_check_page_or_not != 0)
  {
     int page_from_check_page_funct;

     NS_DT3(vptr, NULL, DM_L1, MM_HTTP, "Starting execution of check_page() for page '%s'", vptr->cur_page->page_name);
     page_from_check_page_funct = vptr->cur_page->nextpage_func_ptr();
      if(vptr->page_status != NS_REQUEST_CLICKAWAY)
       next_page = page_from_check_page_funct;

     NS_DT3(vptr, NULL, DM_L1, MM_HTTP, "Completed execution of check_page() with return next page id '%d' for page '%s'", next_page, vptr->cur_page->page_name);
  }

  if (vptr->sess_ptr->ctrlBlock != NULL)
  {
    if(vptr->page_status == NS_REQUEST_CLICKAWAY)
      next_page = NS_NEXT_PG_STOP_SESSION;
    else
      next_page = get_next_page_using_cflow ((char *)vptr);   }

  if(next_page == NS_NEXT_PG_STOP_SESSION)
  {
    NSDL4_SCHEDULE(vptr, NULL, "Session is over");
    return (NS_NEXT_PG_STOP_SESSION); // Session is over
  }

  NSDL4_SCHEDULE(vptr, NULL, "Cur page is %s & next page is %s", vptr->cur_page->page_name, (vptr->sess_ptr->first_page + next_page)->page_name);
  return (next_page);

  // return (vptr->sess_ptr->first_page + next_page);
}

/**bug 86575: reset promise_count*/
#define NS_H2_RESET_PROMISE_COUNT()\
{\
  NSDL1_HTTP(vptr, cptr, "enable_push=%d http_protocol=%d", runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push, cptr->http_protocol);\
  if((cptr->http2) && (HTTP_MODE_HTTP2 == cptr->http_protocol) && (runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push))\
  {\
      NSDL1_HTTP(vptr, cptr,"cptr->http2->promise_count=%d", cptr->http2->promise_count);\
      cptr->http2->promise_count = 0;\
      NSDL1_HTTP(vptr, cptr,"Now cptr->http2->promise_count=%d", cptr->http2->promise_count);\
  }\
}

/* */
inline void handle_page_complete(connection *cptr, VUser *vptr, int done, u_ns_ts_t now, int request_type)
{

  NSDL1_HTTP(vptr, cptr, "Method called");

  NS_DT2(vptr, cptr, DM_L1, MM_CONN, "Completed execution of page '%s'", vptr->cur_page->page_name);
  /*bug 86575: reset promise_count*/
  NS_H2_RESET_PROMISE_COUNT()
  /* Page is complete */
  //reset_redirect_count(vptr);   /* Safety net */


  /* Page is complete thereofore reset this for next page*/
  vptr->redirect_count = 0;

  /* Free scan list stuff if any */
  if (cptr->pop3_scan_list_head)
    pop3_free_scan_listing(cptr);
 
//set server mapping flag to 0 so that we dont try to save the host/URL in any
//other page than where we intended to
  if (vptr->svr_map_change)
    vptr->svr_map_change->flag = 0;

  //if no validation is off
  if (!runprof_table_shr_mem[vptr->group_num].gset.no_validation) {
    /* here do_data_processing() is being called for last depth
       Last argument of do_data_processing() is 1 since this is the last call of do_data_processing.
       and  we have to free the bufferi at this moment.
    */
    if ((request_type == HTTP_REQUEST) || (request_type == HTTPS_REQUEST) || (request_type == JNVM_JRMI_REQUEST) || 
        (request_type == JRMI_REQUEST) || ((request_type == LDAP_REQUEST || request_type == LDAPS_REQUEST ) && 
                                            cptr->url_num->proto.ldap.operation != LDAP_LOGOUT) || 
        ((request_type == WS_REQUEST || request_type == WSS_REQUEST) && !done))
      do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1); //for LAST depth
  }

  // Free action request allocated for inline repeat and inline delay 
  AutoFetchTableEntry_Shr *ptr = NULL;
  ptr = runprof_table_shr_mem[vptr->group_num].auto_fetch_table[vptr->cur_page->page_number]; 
  NSDL2_HTTP(vptr, cptr, "vptr->flags before freeing repeat urls = [%x]", vptr->flags);
  if(!(ptr->auto_fetch_embedded) && (vptr->flags & NS_VPTR_FLAGS_INLINE_REPEAT_DELAY_MASK))
  {
    if(cptr->url_num->parent_url_num)
      cptr->url_num = cptr->url_num->parent_url_num;
    free_repeat_urls(vptr); // This method will free action request allocated to inline and delay 
  }
  /*Moved calc_pg_time() up in 3.8.2 release- Tuesday, March 06 2012 - Achint
 *  Previously (For legacy mode) it was getting called after freeing the tx node
 *  because of this, when we were going to log page data in data base we were getting
 *  NUUL value for transaction. It was happening only for Legacy type scripts.*/
  calc_pg_time(vptr, now);

  //Reset cookie flag for page
  if (global_settings->g_auto_cookie_mode) 
    reset_cookie_flag(vptr); 
  
  //Reset used_len variable for page which used in na_web_add_hdr() API
  if(vptr->httpData != NULL && vptr->httpData->usr_hdr != NULL)
    reset_header_flag(vptr);

  if(!global_settings->replay_mode)
    vptr->inline_req_delay = 0; // Reset it here as it is checked in renew_connection for 0
  // In new design, next page will be controlled by script
  // We need to handle if page fails and then continue or not
  // Get next page
  vptr->next_pg_id = get_next_page(vptr, now);

  
  // Free header buffer
  /* If ns_web_url contain MAIN as well as inline URL, cptr->url_num->proto.http.type is set as EMBEDDED url because of this 
     vptr->response_hdr->hdr_buffer was not getting freed. To avoid above issue removing check for MAIN_URL */
  //if((vptr->cur_page->save_headers) && (cptr->url_num->proto.http.type == MAIN_URL)) 

  //Bug 71023: Getting vptr->response_hdr = NULL in ns_url_get_body_msg() API, hence freeing this in user_cleanup
  /*if(vptr->cur_page->save_headers) {
    if(vptr->response_hdr) {
      FREE_AND_MAKE_NULL(vptr->response_hdr->hdr_buffer, "vptr->response_hdr->hdr_buffer", -1);
      FREE_AND_MAKE_NULL(vptr->response_hdr, "vptr->response_hdr", -1);
    }
   }*/


 
  // This is to free redirect URL as we did not free in one case
  // where redirect is from Main URL and vtp has the the final URL
  if (vptr->url_num) {
    char *redirected_url = vptr->url_num->proto.http.redirected_url;
    /*In case of redirection vptr->url_num is freed here but in case it is same as cptr->url_num , 
    cptr->url_num becomes dangling pointer so need to set to original url_num that was before 
    redirection as it is used in case of XMPP FILE UPLOAD*/
    FREE_REDIRECT_URL(cptr, redirected_url, vptr->url_num);/*bug 54315: similar check placed inside FREE_REDIRECT_URL() so removed from here*/
  }

  if (global_settings->interactive) {
    NSDL1_CONN(vptr, cptr, "running cmd '%s'", interactive_buf);
    system(interactive_buf);
  }

  if(vptr->page_status == NS_REQUEST_URL_FAILURE)
    log_page_error_to_event(vptr, cptr, request_type);



  

  // This must be called after calc_pg_time as it log page record which keep running tx hash code
  // tx_logging_on_page_completion will close the tx so it will be gone
  tx_logging_on_page_completion(vptr, now);

  /* In case of RAMP_DOWN_METHOD 1 we stop executing next page. */
  // Check page already called

  // If user is ramping down and mode is allow current page to complete
  // then do not go to next page. We need to stop session
  if (vptr->flags & NS_VUSER_RAMPING_DOWN)
  {
    NSDL3_SCHEDULE(vptr, cptr, "User is marked as ramp down.");
    if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE) {
      NSDL3_SCHEDULE(vptr, cptr, "Ramp down method mode is allow current page to complete. So stop sesssion now and also make page think time 0");
      vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION;
      vptr->pg_think_time = 0; // Force think time to 0 so that it does not start think time

    }
  }

  // It will close transactions which are not yet closed
  // Done before starting think timer so that think time is not included
  // There are few cases:
  //   Case 1 - Script issue. tx_end_transaton not called
  //   Case 2 - Check page not called due to error and continue on pg error is false

  // In all script types, if page fails and continue on page error is false
  // In Legacy, if last page. In C type, last page is not known here
  // This is also done in session completion method to handle other cases

  if (vptr->next_pg_id == NS_NEXT_PG_STOP_SESSION)
    tx_logging_on_session_completion(vptr, now, vptr->page_status);

  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    NSDL3_HTTP(vptr, cptr, "Fetching page id %d again due to reload", vptr->next_pg_id);
    //We are not doing ns_web_url_ext() becouse in NS_SCRIPT_MODE_USER_CONTEXT 
    //we switch the context but in the case of reload we are already in nvm context 
    // ns_web_url_ext(vptr, vptr->next_pg_id);
    vut_add_task(vptr, VUT_WEB_URL);
    return;
  }
  send_test_monitor_gdf_data(vptr, global_settings->monitor_type, &average_time, NULL);
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    //XMPP File Upload Done
    if(cptr->url_num->proto.http.header_flags & NS_XMPP_UPLOAD_FILE)
    {
      cptr->url_num->proto.http.header_flags &= ~NS_XMPP_UPLOAD_FILE ;
      xmpp_file_upload_complete(vptr, now, cptr->req_ok);
      return;
    }
    // Page failed and continue on page error is false
    if(vptr->next_pg_id == NS_NEXT_PG_STOP_SESSION)
    {
      NSDL3_HTTP(vptr, cptr, "No more pages to execute due to error or user ramped down with allow current page to complete mode. So ending session");
      on_session_completion(now, vptr, done?NULL:cptr, 1);
    }
    else
    {
      NSDL3_HTTP(vptr, cptr, "Currnet page is complete. Switching to vuser context");
      switch_to_vuser_ctx(vptr, "WebUrlOver");
    }
    return;
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    // Page failed and continue on page error is false
    if(vptr->next_pg_id == NS_NEXT_PG_STOP_SESSION)
    {
      NSDL3_HTTP(vptr, cptr, "No more pages to execute due to error or user ramped down with allow current page to complete mode. So ending session");
      on_session_completion(now, vptr, done?NULL:cptr, 1);
      /* On session completion NVM is already sending the response to thread, so no need to 
         send WEB_URL_RESP in this case */
      /*  Bug 77663 */
      return;
    }
    if((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
      send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_CLOSE_REP, 0);
    else
      send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);
    return;
  }
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    int opcode = NS_NJVM_API_WEB_URL_REP;
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      opcode = NS_NJVM_API_CLICK_API_REP;
    //page failed and continue on page error is false 
    if(vptr->next_pg_id == NS_NEXT_PG_STOP_SESSION)
    {
      NSDL3_HTTP(vptr, cptr, "No more pages to execute due to error or user ramped down with allow current page to complete mode. So ending session");
      //bug33149 -2 is passed as third argument to send_msg_to_njvm in case of Failure or stop session from java side
      send_msg_to_njvm(vptr->mcctptr, opcode, -2);
    }
    else if (request_type == JNVM_JRMI_REQUEST)
     return; //TODO: handle this case properly
    else
    {
      if(vptr->page_status == NS_REQUEST_OK)
        send_msg_to_njvm(vptr->mcctptr, opcode, 0);
      else // In case of failure and we are continue to next page we are passing -1.
        send_msg_to_njvm(vptr->mcctptr, opcode, -1);
    }
    return;
  }

  // Legacy mode come here

  if (vptr->pg_think_time) {
    // Add page think time node in the user tasks
    ns_page_think_time_ext(vptr, vptr->pg_think_time);
  } else {
    if(vptr->next_pg_id == NS_NEXT_PG_STOP_SESSION)
    {
      on_session_completion(now, vptr, done?NULL:cptr, 1);
    }
    else
    {
      ns_web_url_ext(vptr, vptr->next_pg_id);
    }
  }
}

      /*Reset this bit as it is checked in http_close_connection*/
      /*Bug 2120 Fixed*/
      //RESET_BIT_FLAG_NS_EMBD_OBJS_SET_BY_API(vptr);
      /* Free if using auto fetch embedded */
      /* This has to be done here after execute_next_page because execute_next calls try_url_on_cur_con
       * which needs last_host (extracted from cptr->url_num...)
       * Wed Dec 22 15:14:59 IST 2010 : 
       * 	o In normal case free_all_embedded_urls is before
       * 	  fetching embedded from main url's response
       * 	o But in cache free_all_embedded_urls is called just after 
       * 	  fetching embedded from main url's response, reson because,	
       * 	  response is taken from cache & it did not come from ns_child loop,
       * 	  which is came from server (mean to say came on read event) 
       *
       * So we will call free_all_embedded_urls in 
       * 1) Before session completion 
       * 2) On page complete if Page think time (It will go to that child loop to fire timer)
       * 3) in set_embd_objects just before allocation.
      if (vptr->is_embd_autofetch) {
        NSDL1_HTTP(NULL, cptr, "is_embd_autofetch is ON, freeing all_embedded_urls");
        free_all_embedded_urls(vptr);
      } */


void calc_pg_time( VUser* vptr, u_ns_ts_t now)
{
  u_ns_ts_t download_time;
 // avgtime *lol_average_time;

  NSDL3_SCHEDULE(vptr, NULL, "Method called for pg %s", vptr->cur_page->page_name);

  // excluding the failed page & tx statistics. Refer the debug log below for more detail
  if(NS_EXCLUDE_STOPPED_STATS_FROM_PAGE_TX)
  {
     NSDL2_SCHEDULE(NULL, vptr, "exclude_stopped_stats is on and page status is stopped, hence excluding the stopped stats from page dump, hits, drilldown database, response time & tracing");
    return;
  }

#ifdef NS_TIME

  average_time->pg_tries++;
 
  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->pg_tries++;
  }
 
  if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT){
    NSDL3_SCHEDULE(NULL, NULL, "Call log_page_record_v2 Function"); 
    #ifndef CAV_MAIN
    if (log_page_record_v2(vptr, now) == -1)
      fprintf(stderr, "Error in logging the page record\n");
    #endif
  }

  if (!vptr->page_status) {
    average_time->pg_hits++;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->pg_hits++;
    }
  } else if ((vptr->page_status >= 0) && (vptr->page_status < TOTAL_PAGE_ERR)) {
    average_time->pg_error_codes[vptr->page_status]++;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->pg_error_codes[vptr->page_status]++;
    }
  } else {
    printf("Unknown error\n");
    average_time->pg_error_codes[NS_REQUEST_ERRMISC]++;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->pg_error_codes[NS_REQUEST_ERRMISC]++;
    }
  }

  NSDL3_SCHEDULE(NULL, NULL, "Setting page download time enable_rbu = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu);

  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
    download_time = now - vptr->pg_begin_at;
  else
    download_time = (vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0;

  NSDL3_SCHEDULE(NULL, NULL, "download_time = %u", download_time);
  
  if(!vptr->page_status)  //Success Page Response time
  {
    NSDL4_SCHEDULE(NULL, NULL, "Page is success");
    SET_MIN (average_time->pg_succ_min_resp_time, download_time);
    SET_MAX (average_time->pg_succ_max_resp_time, download_time);
    average_time->pg_succ_tot_resp_time += download_time;

    if(global_settings->monitor_type == HTTP_API)
    {
      cavtest_http_avg->avail_status = 1;
      cavtest_http_avg->unavail_reason = 0;
    }
    
    if(SHOW_GRP_DATA)
    { 
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
   
      SET_MIN (lol_average_time->pg_succ_min_resp_time, download_time);
      SET_MAX (lol_average_time->pg_succ_max_resp_time, download_time);
      lol_average_time->pg_succ_tot_resp_time += download_time;
    }
    NSDL4_SCHEDULE(NULL, NULL, "Page is success : average_time->pg_succ_min_resp_time - %d, average_time->pg_succ_max_resp_time - %d, "
                               "average_time->pg_succ_tot_resp_time - %d ",
                                average_time->pg_succ_min_resp_time, average_time->pg_succ_max_resp_time,
                                average_time->pg_succ_tot_resp_time);
  }
  else   //Failure Page Response time
  {
    NSDL4_SCHEDULE(NULL, NULL, "Page is failed");
    SET_MIN (average_time->pg_fail_min_resp_time, download_time);
    SET_MAX (average_time->pg_fail_max_resp_time, download_time);
    average_time->pg_fail_tot_resp_time += download_time;
    
    if(global_settings->monitor_type == HTTP_API)
    {
      cavtest_http_avg->avail_status = 0;
      //TODO: Fill unavail_reason
    }

    if(SHOW_GRP_DATA)
    { 
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
   
      SET_MIN (lol_average_time->pg_fail_min_resp_time, download_time);
      SET_MAX (lol_average_time->pg_fail_max_resp_time, download_time);
      lol_average_time->pg_fail_tot_resp_time += download_time;
    }
    NSDL4_SCHEDULE(NULL, NULL, "Page is failed: average_time->pg_fail_min_resp_time - %d, average_time->pg_fail_max_resp_time - %d, "
                               "average_time->pg_fail_tot_resp_time - %d",
                                average_time->pg_fail_min_resp_time, average_time->pg_fail_max_resp_time,
                                average_time->pg_fail_tot_resp_time); 
 
  }

  //Overall Page Response time
  SET_MIN (average_time->pg_min_time, download_time);
  SET_MAX (average_time->pg_max_time, download_time);
  average_time->pg_tot_time += download_time;
  
  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));

    SET_MIN (lol_average_time->pg_min_time , download_time);
    SET_MAX (lol_average_time->pg_max_time , download_time);
    lol_average_time->pg_tot_time += download_time; 
  }

  if (g_percentile_report == 1)
    update_pdf_data(download_time, pdf_average_page_response_time, 0, 0, 0);

  if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){
    NSDL2_USER_TRACE(vptr, NULL, "Method called, User tracing enabled");
    ut_update_page_values(vptr, download_time, now);
  }

  if(vptr->page_status != NS_REQUEST_OK)
    vptr->flags |= NS_PAGE_DUMP_CAN_DUMP;
#endif
}

static void close_reuse_inuse_connections(connection* conn_ptr, int num_closed) 
{
  /* In below code if connection state is REUSE, we need to close connection
   * in connection pool design connections entry need to be remove from either or both reuse
   * or inuse list, hence next we save connection back in connection pool
   * For doing above, local variable(cptr_next) is used to save next cptr in list bec in close fd
   * we remove cptr which breaks the link list.
   * To fetch next connection in list we are using next_inuse, next_inuse and next_reuse
   * belongs to union, which assure that only one union member is used at a time
   * hence to initiate traversing of link list we require head node
   */
  connection *cptr_next = NULL;
  NSDL2_HTTP(NULL, conn_ptr, "Method called, conn_ptr = %p", conn_ptr);

  for (; conn_ptr != NULL; ) {
    cptr_next = (connection *)conn_ptr->next_inuse;//local var to save next cptr in link list. Must be done after for
    if ((conn_ptr->conn_state == CNST_FREE) 
        || (conn_ptr->conn_state == CNST_REUSE_CON)){
      conn_ptr = cptr_next; //Update cptr with next cptr in list
      continue;
    }
    //if (conn_ptr->conn_state == CNST_REUSE_CON) continue;
    NSDL2_CONN(NULL, conn_ptr, "[cptr=0x%lx]: Connection closed from abort_bad_page and in state %d", (u_ns_ptr_t)conn_ptr, conn_ptr->conn_state);
    close_fd_and_release_cptr(conn_ptr, NS_FD_CLOSE_REMOVE_RESP, get_ms_stamp());
    num_closed++;
    conn_ptr = cptr_next; //Update cptr with next cptr in list
  }
}

void
abort_bad_page(connection* cptr, int status, int redirect_flag)
{
  VUser *vptr;
  int num_closed=0;
  
  vptr = cptr->vptr;
  vptr->page_status = status;

  NSDL2_HTTP(NULL, cptr, "Method called, [cptr=0x%lx], redirect_flag = 0x%x sess = [%d], status = %d", 
			 (u_ns_ptr_t)cptr, redirect_flag, vptr->sess_status, status);



  // Reloading a page can not be a cause of session failure
  if((status != NS_REQUEST_RELOAD) && (status != NS_USEONCE_ABORT))
     vptr->sess_status = NS_REQUEST_ERRMISC;
  
  /* Since we can come here in between fetching redirection chain due to CVFail (
  * when searchvar with certain depth fails in between) we will have to explicitly
  * free the redirect_location since it will not be freed in auto_redirect_connection()
  * and in the current flow. This will result is cptr retaining old location value
  * which gets appended at the begining of the new location which is fetched. */
  FREE_LOCATION_URL(cptr, redirect_flag);
  /* We will also need to make these zero since we have encountered CV fail in between
  * fetching the redirect chain. */
  cptr->redirect_count = vptr->redirect_count = 0;

  if (vptr->urls_awaited == vptr->urls_left) {
    vptr->urls_awaited = vptr->urls_left = 0;
    return;
  }

  /* abort the outstanding url requests
   * belonging to this page
   * the one that were initiated earlier
   */

  /* In connection pool design, connections can be available in either or both
   * lists(reuse list, inuse list).
   * Here we are sending head node of each list, and performing connection cleanup
   * for cptr*/
  if (vptr->head_cinuse)
  {
    close_reuse_inuse_connections(vptr->head_cinuse, num_closed);
  }
  if(vptr->head_creuse)
  {
    close_reuse_inuse_connections(vptr->head_creuse, num_closed);
  }

  if (num_closed != (vptr->urls_awaited - vptr->urls_left))
    NSDL2_CONN(vptr, cptr, "num_closed: %d, vptr->urls_awaited: %d, vptr->urls_left: %d, cur_page: %d", 
                num_closed, vptr->urls_awaited, vptr->urls_left, cptr->url_num - request_table_shr_mem);

  //assert(num_closed == (vptr->urls_awaited - vptr->urls_left));

  vptr->urls_left = vptr->urls_awaited = 0;
}
