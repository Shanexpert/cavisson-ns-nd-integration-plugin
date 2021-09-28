/************************************************************************
*
* HISTORY
* 8/28/01	Typo in output corrected
* 03/15/03	recorded URL prasing added
* 05/02/03	hostname & port picked up from Host header
* 05/14/03	keep hostname info in diffent table & keep just the index
* 		in requests table
* 06/05/03	Arrange URL's on host basis
* 08/30/06 	make body size run time variable and allow the BODY=$CAVINCLUDE$=
*		Only fixes BODY read from capture file. Still need fix
*		BODY from detail file and header size
*
*		set max parallel capped to num embed max
************************************************************************/
/***********************************************************************
 * Design: Parallel connections:
 * An User accesses page by hits its first URL. After this fetch completes
 * the user would issue parallel fetches to get other embedded URL's of
 * this page. There is limit on max number of parallel connections
 * per user (cmax_parallel) and also a limit on per_server max parallel.
 *
 * User's cmax_parallel and per_serve_max_parallel are init at the user
 * creation. Connections's slots of a VUser arranged in a linked list
 * of free slots. At the session start, all connections slots of Vuser
 * are arranged in a linked list. connection slots are allocated to an
 * user right at program start. vuser->freeConnHead keeps the head
 * of this list. and connction's  next_free navigates throgh this list.
 *
 * All URL's are kept in request table while all Hosts (drived from
 * Host headers are kept in HostSvrTable. Each URL entry has an index of
 * gServerTable. It is assumed that the number of hosts accross all
 * sessions would be a relatively small number (typically < 10) because
 * netstorm would typically hit one application at a time. Each Vuser
 * keeps a table of hosts to keep track of number of connections per
 * host and the doubly linked list of currently unused RESUE connected
 * connections. So, if there are a total of 10 hosts found accross all
 * sesssions. They are stored in gServereTable. Each VUser slot is assigned
 * 10 slots of HostServers array. Vuser->hptr is the starting index
 * for this vuser. HostServer slots has svr_con_head and svr_con_tail
 * fields that acts as the head and taild of the double linked list
 * of the connections slots for this Vuser connected to this server
 * host (currently inactive RESUE enetries). connections entries next_svr
 * & prev_svr help nevigate this linked list through connection entries.
 *
 * Also, On the Vuser level a global list (accross all hosts) is maintained.
 * Vuser->head_creuse and VUser->tail_creuse acts as the head and tail
 * of this doubly linked list.A connection entries next_reuse and prev_reuse
 * helpd neviage through the connection entries.
 *
 * For each page, the embedded URLs are kept as an fdset (bit masks)
 * for each URL with an starting offset of 0. So, let us say there are
 * 34 URLs on a page. urlset would have first 34 bits set to 1 and rest
 * of it to 0 (typically there are 1024 bits in fdset). Vptr->first_page_url
 * is the real strating index of the first URL of the current page.
 * Vptr->start_url_rindex and end_url_rindex keep the starting and the
 * ending relative index of the valid range of the fdset bits.
 * In the begining it would be 0 to num_urls-1.
 *
 * next_connection() implemnets the request generation. It would pass
 * through the urlset from start_url_rindex to end_url_rindex.
 * end_url_rindex is never changed but start would move forward
 * after an URL is send as request.
 * URL is taken from the urlset, its host server is found from requsts
 * table. host_servers table is accsed for this VUser and host, if an
 * unused connected RESUE connection is available. If it is not avilable,
 * it is cheked from the same table if the max_paralle for this server
 * has been reached. If the max_connection is reached, this URL is kept
 * in the urlset mask & next URL is gotten from the urlset (start_url_rindex
 * is not moved in this case).
 * In the situations, if max_parallel is not reached for the of the
 * URL, a free connection slot is gotten from free list. If free
 * connection entry is found, URL is issed using this free slot.
 * But if free slot is not found, oldest currently inactive
 * connection is closed, connection slot reclaaimed and url is
 * issued using a new connection in this slot
 ***********************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h> /* not needed on IRIX */
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
// 02/01/07 - Achint Start - For Pre and Post URL Callback
#include <regex.h>

#include <v1/topolib_structures.h>
#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

// 02/01/07 - Achint End - For Pre and Post URL Callback
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_smtp_parse.h"
#include "ns_page.h"
#include "ns_pop3_parse.h"
#include "ns_ftp_parse.h"
#include "ns_dns.h"
#include "ns_embd_objects.h"
#include "ns_http_cache.h"
#include "ns_http_cache_hdr.h"
#include "ns_http_cache_table.h"
#include "ns_vuser_ctx.h"
#include "ns_script_parse.h"
#include "ns_http_script_parse.h"

#include "ns_url_hash.h"
#include "ns_data_types.h"
#include "ns_sync_point.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_inline_delay.h"
#include "nslb_hash_code.h"
#include "ns_trace_level.h"
#include "ns_monitoring.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_monitor_profiles.h"
#include "nslb_cav_conf.h"
#include "ns_global_settings.h"
#include "ns_socket.h"
int max_server_id_used = 0;
int self_tier_id = 0;
int total_tier_table_entries =0;
int topo_idx = -1;
extern void open_event_log_file(char*, int, int);
extern int elog_fd;
extern void create_partition_dir();
#ifdef CAV_MAIN
extern int num_total_event_id;
#ifdef NS_DEBUG_ON
  #define NS_STRING_API "cg_string_api_debug.so"
#else
  #define NS_STRING_API "cg_string_api.so"
#endif
#else
#ifdef NS_DEBUG_ON
  #define NS_STRING_API "ns_string_api_debug.so"
#else
  #define NS_STRING_API "ns_string_api.so"
#endif
#endif

#define NS_MAX_SESSIONS 128
#define NS_MAX_PAGES 1024
#define NS_MAX_SERVERS 1024
#define MAX_PHASE_NAME_LENGTH 512
#include "ns_auto_fetch_parse.h"
#include "ns_page_think_time_parse.h"
#include "ns_parent.h"
#include "ns_proxy_server.h"
#include "ns_njvm.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "wait_forever.h"
#include  "ns_continue_on_page.h"
#include "ns_exit.h"
#include "ns_gdf.h"
#include "ns_appliance_health_monitor.h"
#include "ns_ndc.h"
#include "ns_test_init_stat.h"
#include "ns_monitor_init.h"


/*SessTableEntry gSessionTable[NS_MAX_SESSIONS];
PageTableEntry gPageTable[NS_MAX_PAGES];
SvrTableEntry gServerTable[NS_MAX_SERVERS];*/

/* following are current indices */
int g_cur_session = -1;
#ifndef CAV_MAIN
int g_cur_page = -1;
int g_cur_server = -1;
int g_max_num_embed = 0;
#else
__thread int g_cur_page = -1;
__thread int g_cur_server = -1;
__thread int g_max_num_embed = 0;
#endif

pthread_t script_val_thid;
pthread_attr_t script_val_attr;

#define MAX_NAME_BUF 128*1024

#define NS_JAVA_SCRIPT_URL 1

#define NS_NONE 0
#define NS_NEXT_PAGE_FUNC 1
#define NS_PAGE 2
#define NS_URL 3
#define NS_WEB_URL 4
#define NS_METHOD 5
#define NS_SWITCH_BREAK 6
#define NS_NO_PAGES 7
#define NS_CLOSE_BRACE 8
#define NS_COMP_RAT 9
#define NS_COOKIE 10
#define NS_DET_PAGE 11
#define NS_CMDBEGIN 12
#define NS_CMD 13
#define NS_LAST_NEXT_PAGE 14
#define NS_THINK_TIME 15
#define NS_DO_THINK 16
#define NS_SMTP_SEND 17
#define NS_POP3_STAT 18
#define NS_POP3_LIST 19
#define NS_POP3_GET  20
#define NS_FTP_GET  21
#define NS_DNS_QUERY  22

#define NS_ST_PAGE 1
#define NS_ST_COMP 2
#define NS_ST_COOKIE 3
#define NS_ST_PAGE_CHARAC_END 4
#define NS_ST_CMDBEGIN 5
#define NS_ST_NEXT_PAGE_FUNC 6
#define NS_ST_SWITCH_BREAK 7
#define NS_ST_LAST_NEXT_PAGE 8
#define NS_ST_CLOSE_SWITCH 9
#define NS_ST_CLOSE_WHILE 10
#define NS_ST_CLOSE_MAIN 11
#define NS_ST_SESSION 12
#define NS_ST_TXBEGIN 13
#define NS_ST_ANY 14

/* any addition of new protos should be reflected here too */
char g_proto_str[][0xff] = {  /* The length of each string can not be >= 0xff */
  "UNKNOWN",
  "HTTP_REQUEST",
  "HTTPS_REQUEST",
  "RMI_REQUEST",
  "JBOSS_CONNECT_REQUEST",
  "RMI_CONNECT_REQUEST",
  "PING_ACK_REQUEST",
  "SMTP_REQUEST",
  "POP3_REQUEST",
  "FTP_REQUEST",
  "FTP_DATA_REQUEST",
  "DNS_REQUEST",
  "UNKNOWN",
  "UNKNOWN"
};
int g_proto_str_max_idx = 13;
#ifndef CAV_MAIN
redirect_location *red_loc = NULL;
redirect_location *red_loc_tail = NULL;
#else
__thread redirect_location *red_loc = NULL;
__thread redirect_location *red_loc_tail = NULL;
#endif

//extern void end_test_run( void );

//following variable is for SCRIPT COMMENT used for block comment found or not, used in different fn so made static 
int g_cmt_found;

static redirect_location *new_red_loc()
{
  redirect_location *new;
  MY_MALLOC(new, sizeof(redirect_location), "new", -1);
  new->next = new->prev = NULL;
  new->loc[0] = '\0';
  return new;
}
// this method is used in c type script redirection
int add_redirect_location(int redirect, char *location)
{
  NSDL2_PARSING(NULL, NULL, "Method called. g_follow_redirects = %d, redirect = %d, location = %s", global_settings->g_follow_redirects, redirect, location);

  if(!(global_settings->g_follow_redirects && redirect))
    return 1; // Not redirect or auto redirect is off

  if (!red_loc) {
    red_loc = new_red_loc();
    red_loc_tail = red_loc;
  } else {
    red_loc_tail->next = (void *)new_red_loc();
    ((redirect_location *)red_loc_tail->next)->prev = red_loc_tail;
    red_loc_tail = red_loc_tail->next;
  }
  strcpy(red_loc_tail->loc, location);
  return 0;
}

static int add_red_location(char *loc_str)
{
  char *loc = loc_str + strlen("LOCATION=");
/*   char *end = NULL; */
  int len = 0;

/*   if (!(end = strchr(loc, ';'))) */
/*     end = strchr(loc, ' '); */

/*   if (end) { */
/*     len = end - loc; */
/*   } else { /\* last one *\/ */
/*     len = strlen(loc); */
/*   } */
  
  /* Assuming LOCATION field will always be the last one and no other fields are allowed after that. */
  len = strlen(loc);

  NSDL1_HTTP(NULL, NULL, "Method called. Adding %*.*s", len, len, loc);

  if (strstr(loc, "POST_CB="))
  {
    NSTL1_OUT(NULL, NULL, "Warning: POST_CB should not be defined after LOCATION field.\n");
  }
  if (strstr(loc, "PRE_CB=")) 
  {
    NSTL1_OUT(NULL, NULL, "Warning: PRE_CB should not be defined after LOCATION field.\n");
  }

  if (!red_loc) {
    red_loc = new_red_loc();
    red_loc_tail = red_loc;
  } else {
    red_loc_tail->next = (void *)new_red_loc();
    ((redirect_location *)red_loc_tail->next)->prev = red_loc_tail;
    red_loc_tail = red_loc_tail->next;
  }
  strncpy(red_loc_tail->loc, loc, len);
  red_loc_tail->loc[len] = '\0';
  
  return strlen("LOCATION=") + strlen(loc);
}

void delete_red_location(redirect_location *r)
{
  NSDL1_HTTP(NULL, NULL, "Method called. Deleting %s", r->loc);
  if (r->prev)
    ((redirect_location *)r->prev)->next = r->next;
  else
    red_loc = r->next;

  if (r->next)
    ((redirect_location *)r->next)->prev = r->prev;
  else
    red_loc_tail = r->prev;

  FREE_AND_MAKE_NOT_NULL_EX(r, sizeof(redirect_location), "redirect_location", -1);// added struct size
}


// this is used in c type script redirection
redirect_location *search_redirect_location(char *url)
{
  redirect_location *r;

  NSDL1_HTTP(NULL, NULL, "Method called. Searching %s..", url);
  for (r = red_loc; r != NULL; r = r->next) {
    if (strcmp(r->loc, url) == 0) {
      NSDL2_HTTP(NULL, NULL, "Found %s..", url);
      return r;
    }
  }
  NSDL2_HTTP(NULL, NULL, "Not found %s..", url);
  return NULL;
}

// Removing search_red_location as this was only used by legacy script. Since legacy script is now no longer supported 

//Changes made in this method after valgrind reported bug
void free_leftover_red_location(char *file_name)
{
  redirect_location *r;
  redirect_location *r_tmp;

  if(red_loc) { // This means, some of the LOCATION fields were not found in the script.detail file 
    for (r = red_loc; r != NULL;) {
      error_log("Redirect LOCATION=%s was not found in %s, ignored.\n", r->loc, file_name);
      r_tmp = r;
      r = r->next;
      delete_red_location(r_tmp);
    }
  }
}

/* This function assumes that index will be in 2nd order for all protocols. Hence generic code. */
int
url_host_comp (const void *r1, const void *r2)
{
	NSDL2_HTTP(NULL, NULL, "Method called");
	if (((action_request *)r1)->index.svr_idx > ((action_request *)r2)->index.svr_idx)
		return 1;
	else if (((action_request *)r1)->index.svr_idx < ((action_request *)r2)->index.svr_idx)
		return -1;
	else
		return 0;
}

void
add_pg_hostlist (PageTableEntry *page, short svr_idx, short num, int start)
{
  HostElement *hel;
  int rnum;

  NSDL1_HTTP(NULL, NULL, "Method called, svr %d num=%d start=%d", svr_idx, num, start);

  if (create_host_table_entry(&rnum) != SUCCESS) {
    NS_EXIT(-1, "Error in creating in host table element");
  }

  hel = &hostTable[rnum];
  hel->first_url = start; //Represents indexing of request
  hel->svr_idx = svr_idx;
  hel->num_url = num;   //Inserting how many urls of this host
  hel->next = -1;

  //Inserting index of host here
  if (page->tail_hlist == -1) { //List is empty
    page->head_hlist = page->tail_hlist = rnum;
  } else {
    hostTable[page->tail_hlist].next = rnum;
    page->tail_hlist = rnum;
  }
}

int get_no_inlined()
{
  int grp_idx, get_no_inlined_obj_set_for_all = 1;
  /* we've set group based values for this keyword during parsing - as an
   * optimization, we check if it is set for all groups. If
   * the value is uniformly on for all groups,
   * we can avoid arranging the pages for the embedded URLs.
   */
  for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
    if (!runProfTable[grp_idx].gset.get_no_inlined_obj) {
      get_no_inlined_obj_set_for_all  = 0;
      break;
    }
  }
  NSDL4_HTTP(NULL, NULL,"get_no_inlined_obj %s for all groups",get_no_inlined_obj_set_for_all?"SET":"NOT SET");
  return get_no_inlined_obj_set_for_all;
}

void
arrange_page_urls (PageTableEntry *page)
{
  int start_url, num_url;
  int ii;
  action_request *r;
  short last_host = -1;
  short cur_host;
  short num;
  int start;
  int get_no_inlined_obj_set_for_all = 1;


  /* we've set group based values for this keyword during parsing - as an
   * optimization, we check if it is set for all groups. If
   * the value is uniformly on for all groups,
   * we can avoid arranging the pages for the embedded URLs.
   */
/*
  for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
    if (!runProfTable[grp_idx].gset.get_no_inlined_obj) {
      get_no_inlined_obj_set_for_all  = 0;
      break;
    }
  }
*/
  if((loader_opcode == MASTER_LOADER))
  {
    //In NetCloud we need to support DDR, We need url ids for embedded urls also but if some body has 
    //enabled GET_NO_INLINE_OBJ then we will not get ids. In NC we need url ids at controller. 
    //So making get_no_inlined_obj_set_for_all to 0 on controller machine
    get_no_inlined_obj_set_for_all = 0;
  }
  else
    get_no_inlined_obj_set_for_all = get_no_inlined();
  NSDL4_HTTP(NULL, NULL,"get_no_inlined_obj %s for all groups",get_no_inlined_obj_set_for_all?"SET":"NOT SET"); 
 
  NSDL2_HTTP(NULL, NULL, "Method called. Page = %s, requests[page->first_eurl] = %p, request_type = %d", 
                          RETRIEVE_BUFFER_DATA(page->page_name), &requests[page->first_eurl], requests[page->first_eurl].request_type);

  if ((requests[page->first_eurl].request_type == HTTP_REQUEST || 
      requests[page->first_eurl].request_type == HTTPS_REQUEST) &&
      requests[page->first_eurl].proto.http.type == REDIRECT_URL)
  {
      //If the main page URL is redirect, mark the first non-redirect URL as the main url
      ii = 0;
      while (requests[page->first_eurl + ii].proto.http.type == REDIRECT_URL) {
	ii++;
        //Redirect url >= num of urls (embd + main)
        if(ii >= page->num_eurls)
        {
          NSTL1(NULL, NULL, "Warning: arrange_page_urls() - Main URL of page '%s' is redirect URL but there are no embedded URLs in this page which are not redirect URL. Script may not be correct. \n", RETRIEVE_BUFFER_DATA(page->page_name));
          NS_DUMP_WARNING("Main URL of page '%s' is redirect URL but there are no embedded URLs in this page which are not redirect URL. Script may not be correct.",
 RETRIEVE_BUFFER_DATA(page->page_name));
          //exit(-1);
        }
      }
      requests[page->first_eurl + ii].proto.http.type = MAIN_URL;
      if (get_no_inlined_obj_set_for_all) page->num_eurls = ii+1;
      start_url = page->first_eurl + ii + 1;
      num_url = page->num_eurls - (ii+1);
  } else {
      if (get_no_inlined_obj_set_for_all) page->num_eurls = 1;
      start_url = page->first_eurl +1;
      num_url = page->num_eurls -1;  // -1 for removing main url
  }

  //All REDIRECT URLS for main URL are not ignored.
  //Ignore all REDIRECTS after Main URL
  //Mark all in-between URLs  as embedded
  for (ii=0; ii < num_url; ii++) {
      if (requests[start_url + ii].proto.http.type == REDIRECT_URL)
          requests[start_url + ii].proto.http.type = EMBEDDED_URL;
    NSDL2_HTTP(NULL, NULL, "requests[start_url + ii].proto.http.type = %d", requests[start_url + ii].proto.http.type);
  }

  r = &requests[start_url];
  NSDL2_HTTP(NULL, NULL, "requests[%d] = %p, num_url = %d", start_url, &requests[start_url], num_url);

  if(num_url > 1) // If inline url is not present in case of manual redirection then num_url will be -1, this cause core so check added here
    qsort (r, num_url, sizeof(action_request), url_host_comp);

  //printf("Arranging URLs for page 0x%x with start=%d and num=%d\n", page, start_url, num_url);
  for (ii=0; ii < num_url; ii++, r++) {
    cur_host = r->index.svr_idx;
    NSDL2_HTTP(NULL, NULL, "cur_host = %d, last_host = %d", cur_host, last_host);
    if (last_host != cur_host) {
      if (last_host != -1)   //-2 for parameterized request // Why it is not creating entry for first host
	add_pg_hostlist(page, last_host, num, start);
      num = 1;		     //Represents same host has how many urls
      start = start_url +ii;
      last_host = cur_host;
    } else {
      num++;
    }
    NSDL2_HTTP(NULL, NULL, "cur_host = %d, last_host = %d, start = %d, num = %d", cur_host, last_host, start, num);
  }
  NSDL2_HTTP(NULL, NULL, "last_host = %d", last_host);
  //if ((last_host != -1) && (last_host != -2))  //bcz entry is already done above
  if (last_host != -1)  //bcz entry is already done above
    add_pg_hostlist(page, last_host, num, start);
}

void
dump_URL_SHR (PageTableEntry_Shr *pg)
{
HostTableEntry_Shr *hel;

       NSDL2_HTTP(NULL, NULL, "Method called, Page # 0x%lx Name = %s num_urls = %d first_url = 0x%lx", (ns_ptr_t) pg, pg->page_name,
				pg->num_eurls,
	                        (ns_ptr_t) pg->first_eurl);
	hel = pg->head_hlist;
	while (hel) {
		NSDL2_HTTP(NULL, NULL, "Svr=0x%lx first=0x%lx num=%d", (ns_ptr_t) hel->svr_ptr, (ns_ptr_t)hel->first_url, hel->num_url);
		hel = hel->next;
	}
}

void
dump_URL (int pnum)
{
PageTableEntry *pg = &gPageTable[pnum];
HostElement *hel;

       NSDL2_HTTP(NULL, NULL, "Method called, Page # %d Name = %s num_urls = %d first_url = %d", pnum, RETRIEVE_BUFFER_DATA(pg->page_name),
				pg->num_eurls,
				pg->first_eurl);
       if (pg->head_hlist == -1)
	 return;
	hel = &hostTable[pg->head_hlist];
	while (hel) {
		NSDL2_HTTP(NULL, NULL, "Svr=%d first=%d num=%d", hel->svr_idx, hel->first_url, hel->num_url);
		if (hel->next == -1)
		  hel = NULL;
		else
		  hel = &hostTable[hel->next];
	}
}

// 02/01/07 - Achint Start - For Pre and Post URL Callback

int match(const char *string, char *pattern)
{
  int status;
  regex_t re;

  NSDL2_HTTP(NULL, NULL, "Method called");
  if(regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0)
    return 0;

  status = regexec(&re, string, (size_t)0, NULL, 0);
  regfree(&re);
  if(status != 0)
    return 0;
  return 1;
}


int val_fname(char *fname, int max_len)
{
  int len = strlen(fname);
  ptt_cll_back_msg[0] = '\0';

  NSDL2_MISC(NULL, NULL, "Method called");
  // Check function is starting with Alpha or Underscore and followed by Alpha/Numeric or Underscore
  if(!match(fname, "^[A-Za-z_][A-Za-z0-9_]*$"))
  {
    sprintf(ptt_cll_back_msg, "Function name (%s) is not as per C language standard. It should start with Alpha or Underscore and followed by Alpha/Numeric or Underscore", fname);
    NSTL1(NULL, NULL, "Function name (%s) is not as per C language standard. It should start with Alpha or Underscore and followed by Alpha/Numeric or Underscore", fname);
    return -1;
  }

  if((len > max_len) || (len < 1))
  {
    sprintf(ptt_cll_back_msg, "Length of function name (%s) should not be greater than 64", fname);
    NSTL1(NULL, NULL, "Length of function name (%s) should not be greater than 64", fname);
    return -1;
  }
  return 0;
}


char *proc_main_url_line (char *line_ptr, struct http_charac *charac)
{
char buf[MAX_LINE_LENGTH];
char *ptr, *div_ptr;
  
  NSDL2_HTTP(NULL, NULL, "Method called, Line = %s", line_ptr);

  ptr = strtok(line_ptr, ";");

  free_leftover_red_location("script.detail");
  while(ptr != NULL)
  { 
 	// printf("proc_main_url_line(): After strtok, Line = %s\n", line_ptr);
	if(!strncmp(ptr, "CMP_RAT", strlen("CMP_RAT")))
	{
	   ptr += 8;
	   div_ptr = strchr(ptr, '/');
	   *div_ptr = '\0';
	   charac->tx_ratio = atoi(ptr);
	   ptr = div_ptr;
	   ptr++;
	   charac->rx_ratio = atoi(ptr);
	}

	if(!strncmp(ptr, "REDIRECT=YES", strlen("REDIRECT=YES"))) 
	  charac->redirect = 1;
        
	if(!strncmp(ptr, "LOCATION=", strlen("LOCATION=")) && 
           global_settings->g_follow_redirects && charac->redirect) { /* Assuming REDIRECT= comes before LOCATION */
          char loc[MAX_LINE_LENGTH];
          strcpy(loc, ptr);
          while (ptr) {
            ptr = strtok(NULL, ";");
            if (ptr) {
              strcat(loc, ";");
              strcat(loc, ptr);
            }
          }
          add_red_location(loc);
          break; /* we assume LOCATION will be the last field hence break; */
        }

	if(!strncmp(ptr, "PRE_CB=", strlen("PRE_CB=")))
	{
	  ptr += 7;
	  strcpy(buf, ptr);
	  if(val_fname(buf, 31))
	    NS_EXIT(-1, "val_fname() failed");
	  strcpy(charac->pre_url_fname, buf);
	}

	if(!strncmp(ptr, "POST_CB=", strlen("POST_CB=")))
	{
	  ptr += 8;
	  strcpy(buf, ptr);
	  if(val_fname(buf, 31))
	    NS_EXIT(-1, "val_fname() failed");
	  strcpy(charac->post_url_fname, buf);
	}
  	ptr = strtok(NULL, ";");
  }
  // printf("proc_main_url_line: REDIRECT = %d, CMP_RAT=%d/%d, PRE_CB = %s, POST_CB = %s\n", charac->redirect, charac->tx_ratio, charac->rx_ratio, charac->pre_url_fname, charac->post_url_fname);

  return(ptr);
}


char *proc_eurl_pre_post_fname(char *line_ptr, char *token, struct http_charac* charac)
{
  char buf[MAX_LINE_LENGTH];
  char *ptr;
  NSDL2_MISC(NULL, NULL, "proc_eurl_pre_post_fname() called, Line = %s", line_ptr);

  ptr = strtok(line_ptr, " ");

  if (!strncmp(token, "PRE_CB=", strlen("PRE_CB=")))
  {
	ptr += 7;
    //printf("proc_eurl_pre_post_fname: PRE_CB= %s\n", ptr);
	strcpy(buf, ptr);
	if(val_fname(buf, 31))
	  NS_EXIT(-1, "val_fname() failed");
	strcpy(charac->pre_url_fname, buf);
	ptr += (strlen(buf) + 1 );
  }
  else if(!strncmp(token, "POST_CB=", strlen("POST_CB=")))
  {
	ptr += 8;
    //printf("proc_eurl_pre_post_fname: POST_CB= %s\n", ptr);
	strcpy(buf, ptr);
	//printf("BUF= %s\n", buf);
	if(val_fname(buf, 31))
	  NS_EXIT(-1, "val_fname() failed");
	strcpy(charac->post_url_fname, buf);
    ptr += (strlen(buf) + 1);
  }
  return(ptr);
}

// 02/01/07 - Achint End - For Pre and Post URL Callback


//get an elemt from detail or capture file
int
get_element(FILE *fp, char *buf, struct http_charac* charac, int* line_num, char *fname)
{
  char line[MAX_LINE_LENGTH];
  char* line_ptr;
  char token[MAX_LINE_LENGTH];
  char value[MAX_LINE_LENGTH];
  int i=0;
  int type;
  char num_buf[16];
#ifdef WS_MODE_OLD
  char name_buf[MAX_LINE_LENGTH];
  int wss_idx;
#endif
#ifdef RMI_MODE
  int exact;
  int bytes;
  char first_msg_len_exp = 0;
#endif
  int blank_line = 1;
  int colon_found;
  int comma_found;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, fname = %s", fname);  
  charac->redirect = 0; //will be set to 1, if redirect
  charac->pre_url_fname[0] = charac->post_url_fname[0] = 0;

  /*To know it is JS or not*/
  charac->content_type = 0;

  while (blank_line) {
    if (!fgets(line, MAX_LINE_LENGTH, fp)) {
      NSDL3_SCHEDULE(NULL, NULL, "get_element returns 0 for file %s", fname);
      return NS_NONE;
    }
    (*line_num)++;

    line_ptr = line;

    CLEAR_WHITE_SPACE(line_ptr);
    IGNORE_COMMENTS(line_ptr);
   
    if (*line_ptr == '\n')
      blank_line = 1;
    else
      blank_line = 0;
    //(*line_num)++;
  }

  /* remove the newline character from end of line. */
  if (strchr(line_ptr, '\n'))
    if (strlen(line_ptr) > 0)
      line_ptr[strlen(line_ptr) - 1] = '\0';

  if (!strncmp(line_ptr, "next_page", strlen("next_page"))) {
    line_ptr += strlen("next_page");
    CLEAR_WHITE_SPACE(line_ptr);
    if (*line_ptr != '=') {
      NSTL1_OUT(NULL, NULL, "get_element: expecting an '=' at line %d in file %s\n", *line_num, fname);
      return -1;
    }
    line_ptr++;
    CLEAR_WHITE_SPACE(line_ptr);
    if (!strncmp(line_ptr, "check_page_", strlen("check_page_"))) {
      type = NS_NEXT_PAGE_FUNC;
    } else if (!strncmp(line_ptr, "-1;", strlen("-1;"))) {
      type = NS_LAST_NEXT_PAGE;
    } else {
      NSTL1_OUT(NULL, NULL, "get_element(): bad syntax at line %d in file %s\n", *line_num, fname);
      return -1;
    }
  } else if (!strncmp(line_ptr, "think_time", strlen("think_time"))) {
    line_ptr += strlen("think_time");
    CLEAR_WHITE_SPACE(line_ptr);
    if (*line_ptr != '=') {
      NSTL1_OUT(NULL, NULL, "get_element: expecting an '=' at line %d in file %s\n", *line_num, fname);
      return -1;
    }
    line_ptr++;
    CLEAR_WHITE_SPACE(line_ptr);
    if (!strncmp(line_ptr, "pre_page_", strlen("pre_page_"))) {
      type = NS_THINK_TIME;
    } else {
      NSTL1_OUT(NULL, NULL, "get_element(): bad syntax at line %d in file %s\n", *line_num, fname);
      return -1;
    }
  } else if (!strncmp(line_ptr, "case", strlen("case"))) {
    type = NS_PAGE;
  } else if (!strncmp(line_ptr, "web_url", strlen("web_url"))) {
    type = NS_WEB_URL;
  } else if (!strncmp(line_ptr, "smtp_send", strlen("smtp_send"))) {
    type = NS_SMTP_SEND;
  } else if (!strncmp(line_ptr, "pop_stat", strlen("pop_stat"))) {
    type = NS_POP3_STAT;
  } else if (!strncmp(line_ptr, "pop_list", strlen("pop_list"))) {
    type = NS_POP3_LIST;
  } else if (!strncmp(line_ptr, "pop_get", strlen("pop_get"))) {
    type = NS_POP3_GET;
  } else if (!strncmp(line_ptr, "ftp_get", strlen("ftp_get"))) {
    type = NS_FTP_GET;
  } else if (!strncmp(line_ptr, "dns_query", strlen("dns_query"))) {
    type = NS_DNS_QUERY;
  } else if (!strncmp(line_ptr, "METHOD", strlen("METHOD"))) {
    type = NS_METHOD;
  } else if (!strncmp(line_ptr, "break;", strlen("break;"))) {
    type = NS_SWITCH_BREAK;
  } else if (!strncmp(line_ptr, "default:", strlen("default:"))) {
    type = NS_NO_PAGES;
  } else if (!strncmp(line_ptr, "}", 1)) {
    type = NS_CLOSE_BRACE;
  } else if (!strcmp(line_ptr, "do_think(think_time);")) {
    type = NS_DO_THINK;
  } else if (!strncmp(line_ptr, "CMP_RAT", strlen("CMP_RAT"))) {
    type = NS_COMP_RAT;
    // 02/01/07 - Achint Start - For Pre and Post URL Callback
    line_ptr = proc_main_url_line(line_ptr, charac);
	// 02/01/07 - Achint End - For Pre and Post URL Callback
  } else if (!strncmp(line_ptr, "COOKIE=", strlen("COOKIE="))) {
    type = NS_COOKIE;
  } else if (!strncmp(line_ptr, "URL", strlen("URL"))) {
    type = NS_URL;
  } else if (!strncmp(line, "--Page", 6)) {
#ifdef RMI_MODE
    int got_all_options = 0;
    line_ptr += 6;

    charac->keep_conn_open = 0;

    while (*line_ptr != '\0') {

      switch(*line_ptr) {
      case ' ':
	line_ptr++;
	break;

      case 'C':
	if (!strncmp(line_ptr, "CONN_CONTINUE", strlen("CONN_CONTINUE"))) {
	  charac->keep_conn_open = 1;
	  line_ptr += strlen("CONN_CONTINUE");
	} else {
	  got_all_options = 1;
	}
	break;

      default:
	got_all_options = 1;
	break;
      }

      if (got_all_options)
	break;
    }
#endif
    type = NS_DET_PAGE;
  } else if (!strncmp(line, "----", 4)) {
    line_ptr += 4;
    memset(charac, 0, sizeof(struct http_charac));
#ifdef WS_MODE_OLD
    charac->wss_idx = 0;
#endif

    if (*line_ptr == '\n') {
      type = NS_CMD;
    }

    charac->type = HTTP_REQUEST;   /* This is the default type of request */

    while (*line_ptr != 0) {

      switch (*line_ptr) {

      case ' ':
	line_ptr++;
	break;

      case 'C':
   	if (!strncmp(line_ptr, "Content-Type=", strlen("Content-Type="))) {
           line_ptr = line_ptr + strlen("Content-Type=");
           if(line_ptr) {
              CLEAR_WHITE_SPACE(line_ptr);
              if(line_ptr) {
                 if(!strcmp(line_ptr, "text/javascript")) {
                   line_ptr = line_ptr + strlen("text/javascript");
                   charac->content_type = NS_JAVA_SCRIPT_URL;
                   printf("Setting Content-Type=text/javascript\n");
                 }
              }
           }
        }
	break;

#ifdef RMI_MODE
      case 'F':
	if (!strncmp(line_ptr, "FIRST_MSG_LEN:", strlen("FIRST_MSG_LEN:"))) {
	  if (charac->type != JBOSS_CONNECT_REQUEST) {
	    NS_EXIT(-1, "invalid option (FIRST_MSG_LEN) for request type of JBOSS_CONNECT");
	  }
	  line_ptr += strlen("FIRST_MSG_LEN:");
	  sscanf(line_ptr, "%d", &bytes);
	  charac->first_mesg_len = bytes;
	  first_msg_len_exp = 0;
	}
	break;
#endif

      case 'H':
	if (!strncmp(line_ptr, "HTTPS", 5)) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type (HTTP/HTTPS) at line %d file %s", *line_num, fname);
	  }
	  charac->type = HTTPS_REQUEST;
	  line_ptr+= 5;
	}
	else if (!strncmp(line_ptr, "HTTP", 4)) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type (HTTP/HTTPS) at line %d file %s", *line_num, fname);
	  }
	  charac->type = HTTP_REQUEST;
	  line_ptr+= 4;
	}
	break;

#ifdef RMI_MODE
      case 'J':
	if (!strncmp(line_ptr, "JBOSS_CONNECT", strlen("JBOSS_CONNECT"))) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type at line %d", *line_num);
	  }
	  charac->type = JBOSS_CONNECT_REQUEST;
	  line_ptr += strlen("JBOSS_CONNECT");
	  first_msg_len_exp = 1;
	}
	break;
#endif //-- Added here



      case 'P':
#ifdef RMI_MODE // -- Added here
	if (!strncmp(line_ptr, "PING_ACK_REQUEST", strlen("PING_ACK_REQUEST"))) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type at line %d", *line_num);
	  }
	  charac->type = PING_ACK_REQUEST;
	  line_ptr += strlen("PING_ACK_REQUEST");
	}
#endif
    // 02/01/07 - Achint Start - For Pre and Post URL Callback
   	if (!strncmp(line_ptr, "PRE_CB", strlen("PRE_CB")))
	  line_ptr = proc_eurl_pre_post_fname(line_ptr, "PRE_CB=", charac);

    if(!strncmp(line_ptr, "POST_CB", strlen("POST_CB")))
      line_ptr = proc_eurl_pre_post_fname(line_ptr, "POST_CB=", charac);


    // 02/01/07 - Achint End - For Pre and Post URL Callback

      case 'R':
	if (!strncmp(line_ptr, "RX_RAT:", 7)) {
	  line_ptr += 7;
	  for (i = 0; ((*line_ptr != ' ') && (*line_ptr != '\0')); i++, line_ptr++)
	    num_buf[i] = *line_ptr;

	  num_buf[i] = 0;
	  charac->rx_ratio = atoi(num_buf);
	} else if (!strncmp(line_ptr, "REDIRECT=Y", 10)) {
	  line_ptr += 10;
          charac->redirect = 1;
	} else if (!strncmp(line_ptr, "REDIRECT=N", 10)) { /* This will just skip it. */
	  line_ptr += 10;
#ifdef RMI_MODE
	} else if (!strncmp(line_ptr, "RMI_REQUEST", strlen("RMI_REQUEST"))) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type at line %d", *line_num);
	  }
	  charac->type = RMI_REQUEST;
	  line_ptr += strlen("RMI_REQUEST");

	} else if (!strncmp(line_ptr, "RCV_BYTES:", strlen("RCV_BYTES:"))) {
	  if (charac->type != RMI_REQUEST) {
	    NS_EXIT(-1, "invalid option (RCV_BYTES) for request type of RMI");
	  }
	  line_ptr += strlen("RCV_BYTES:");
	  sscanf(line_ptr, "%d,%d", &exact, &bytes);
	  charac->exact = exact;
	  charac->bytes_to_recv = bytes;

	  for (; ((*line_ptr != ' ') && (*line_ptr != '\0')); line_ptr++);

	} else if (!strncmp(line_ptr, "RMI_CONNECT", strlen("RMI_CONNECT"))) {
	  if (charac->type != HTTP_REQUEST) {
	    NS_EXIT(-1, "request can only be of one type at line %d", *line_num);
	  }
	  charac->type = RMI_CONNECT_REQUEST;
	  line_ptr += strlen("RMI_CONNECT");
#endif
	}
	break;

      case 'L':
        if (global_settings->g_follow_redirects && charac->redirect) { /* Assuming REDIRECT= comes before LOCATION */
          if (!strncmp(line_ptr, "LOCATION=", 9)) {
            line_ptr += add_red_location(line_ptr);
            /* remove white space. */
            for (; *line_ptr == ' '; line_ptr++);
          }
        } else {
          for (; *line_ptr != ' ' && *line_ptr != '\0'; line_ptr++);
          if (*line_ptr == ' ') for (; *line_ptr == ' '; line_ptr++);
        }
        break;

      case 'T':
	if (!strncmp(line_ptr, "TX_RAT:", 7)) {
	  line_ptr += 7;
	  for (i = 0; ((*line_ptr != ' ') && (*line_ptr != '\0')); i++, line_ptr++)
	    num_buf[i] = *line_ptr;

	  num_buf[i] = 0;
	  charac->tx_ratio = atoi(num_buf);
	}
	break;

#ifdef WS_MODE_OLD
      case 'W':
	if (!strncmp(line_ptr, "WSS=", 4)) {
	  line_ptr += 4;

	  for (i = 0; ((*line_ptr != ' ') && (*line_ptr != '\0')); i++, line_ptr++)
	    name_buf[i] = *line_ptr;

	  name_buf[i] = 0;

	  wss_idx = find_webspec_table_entry(name_buf);
	  charac->wss_idx = wss_idx;
	}
	break;
#endif

      default:
	line_ptr++;
	break;
      }
    }
    type = NS_CMDBEGIN;
  } else {
    //printf("Warning:Ignoring Unrecognized line <%s> on line %d file %s", line, *line_num, fname);
    type = NS_CMD;
  }

#ifdef RMI_MODE
  if (first_msg_len_exp) {
    NSTL1_OUT(NULL, NULL, "At line %d, expecting a FIRST_MSG_LEN keyword in file %s\n", *line_num, fname);
    return -1;
  }
#endif

  switch (type) {
#ifndef RMI_MODE
  case NS_PAGE:
    i = sscanf(line, "%s %s", token, value);
    if (i != 2) {
      NSTL1_OUT(NULL, NULL, "Invalid format at line=%d in file %s\n", *line_num, fname);
      return -1;
    }

    for (colon_found = 0, i = 0; i < strlen(value); i++) {
      if (value[i] == ' ')
	value[i] = '\0';

      if (value[i] == ':') {
	value [i] = '\0';
	colon_found = 1;
	break;
      }
    }

    if (!colon_found) {
      NSTL1_OUT(NULL, NULL, "Invalid format in capture file at line=%d in file %s\n", *line_num, fname);
      return -1;
    }

    strncpy(buf, value, MAX_LINE_LENGTH);
    break;
#endif
  case NS_DET_PAGE:
    i = sscanf(line, "%s %s", token, value);
    if (i != 2) {
      NSTL1_OUT(NULL, NULL, "Invalid format in file %s at line=%d. Expecting page name after --Page \n",fname, *line_num);
      return -1;
    }
    strncpy(buf, value, MAX_LINE_LENGTH);
    break;
#ifdef RMI_MODE
  case NS_PAGE:
    i = sscanf(line_ptr, "%s", value);
    if (i != 1) {
      NSTL1_OUT(NULL, NULL, "Name must be specified after for Page entry at line=%d in file %s\n", *line_num, fname);
      return -1;
    }
    strncpy(buf, value, MAX_LINE_LENGTH);
    break;
#endif
  case NS_WEB_URL:
  case NS_SMTP_SEND:
  case NS_POP3_STAT:
  case NS_POP3_LIST:
  case NS_POP3_GET:
  case NS_FTP_GET:
  case NS_DNS_QUERY:
    i = sscanf(line_ptr, "%s (%s", token, value);
    if (i != 2) {
      NSTL1_OUT(NULL, NULL, "get_element(): Invalid format at line=%d in file %s . Expecting web_url/smtp_send (<page_name>, \n", *line_num, fname);
      return -1;
    }

    for (comma_found = 0, i = 0; i < strlen(value); i++) {
      if (value[i] == ' ')
	value[i] = '\0';

      if (value[i] == ',') {
	value [i] = '\0';
	comma_found = 1;
	break;
      }
    }

    if (!comma_found) {
      NSTL1_OUT(NULL, NULL, "Invalid format in file %s at line=%d. Expecting web_url/smtp_send (<page_name>, \n", fname, *line_num);
      return -1;
    }

    strncpy(buf, value, MAX_LINE_LENGTH);
    break;
  case NS_CMD:
    strncpy(buf, line, MAX_LINE_LENGTH);
    strcat(buf, "\n");
    break;
  case NS_METHOD:
    line_ptr = strchr(line_ptr, '=');
    if (!line_ptr) {
      NSTL1_OUT(NULL, NULL, "get_element(): Invalid format at line=%d in file %s. Expecting METDOD=<method-name>,\n", *line_num, fname);
      return -1;
    }
    line_ptr++;

    CLEAR_WHITE_SPACE(line_ptr);

    for (comma_found = 0, i = 0; i < strlen(line_ptr); i++) {
      if (line_ptr[i] == ' ')
	line_ptr[i] = '\0';

      if (line_ptr[i] == ',') {
	line_ptr[i] = '\0';
	comma_found = 1;
	break;
      }
    }

    if (!comma_found) {
      NSTL1_OUT(NULL, NULL, "Invalid format in file %s at line=%d. Expecting METHOD=<method-name>,\n", fname, *line_num);
      return -1;
    }

    strncpy(buf, line_ptr, MAX_LINE_LENGTH);
    break;
  case NS_COOKIE:
    if (strchr(line, '=')) {
      line_ptr = strchr(line, '=');
      line_ptr++;
    } else {
      NSTL1_OUT(NULL, NULL, "Invalid format at line=%d\n", *line_num);
      return -1;
    }
    strncpy(buf, line_ptr, MAX_LINE_LENGTH);
    break;
  case NS_URL:
    line_ptr += strlen("URL");
    CLEAR_WHITE_SPACE(line_ptr);
    if (*line_ptr != '=') {
      NSTL1_OUT(NULL, NULL, "Invalid format at line=%d in file %s. Expecting URL=<url>,\n", *line_num, fname);
      return -1;
    }

    line_ptr++;

    CLEAR_WHITE_SPACE(line_ptr);

    char *last_comma = rindex(line_ptr, ',');
    int url_len;

    if (last_comma == NULL) {
      NSTL1_OUT(NULL, NULL, "Invalid format at line=%d in File %s. Expecting URL=<url>,\n", *line_num, fname);
      return -1;
    }

    url_len = ((last_comma - line_ptr));

    //for (comma_found = 0, i = 0; i < strlen(line_ptr); i++) {
    for (comma_found = 0, i = 0; i <= url_len; i++) {
      if (line_ptr[i] == ' ') {
	line_ptr[i] = '\0';
        NSTL1_OUT(NULL, NULL, "Invalid format at Line=%d in File %s. No spaces allowed in the URL\n", *line_num, fname);
        return -1;
      }

/*       if (line_ptr[i] == ',') { */
/* 	line_ptr[i] = '\0'; */
/* 	comma_found = 1; */
/* 	break; */
/*       } */
    }

    line_ptr[url_len] = '\0';


    NSDL4_MISC(NULL, NULL, "last_comma = %s, line_ptr = %s, url_len = %d", 
               last_comma, line_ptr, url_len);

/*     if (!comma_found) { */
/*       NSTL1_OUT(NULL, NULL, "Invalid format at line=%d in File %s. Expecting URL=<url>,\n", *line_num, fname); */
/*       return -1; */
/*     } */

    strncpy(buf, line_ptr, MAX_LINE_LENGTH);
    break;
  case NS_CMDBEGIN:
  case NS_NEXT_PAGE_FUNC:
  case NS_THINK_TIME:
  case NS_DO_THINK:
  case NS_SWITCH_BREAK:
  case NS_NO_PAGES:
  case NS_LAST_NEXT_PAGE:
  case NS_CLOSE_BRACE:
  case NS_COMP_RAT:
    buf[0] = '\0';
    break;
  default:
    NSTL1_OUT(NULL, NULL, "Unexpected type\n");
    NSDL3_SCHEDULE(NULL, NULL, "get_element returns -1 in file %s", fname);
    return -1;
  }

  NSDL3_SCHEDULE(NULL, NULL, "get_element returns %d buf=%s and buflen=%d file=%s", type, buf, strlen(buf), fname);
  return type;
}

void
check_end_hdr_line(char* buf, int max_size) {
  int line_len = strlen(buf);

  NSDL2_HTTP(NULL, NULL, "Method called, max_size = %d", max_size);
  if (buf[line_len-1] == '\n') {
    if (buf[line_len-2] == '\r')
      return;
    if (line_len + 2 <= max_size) {
      buf[line_len-1] = '\r';
      buf[line_len] = '\n';
      buf[line_len+1] = '\0';
    }
  }
}


/* This method parses URL in following formats
Case1:
   Absolute URLs
     /abc (Host is empty, url_path is "/abc" and request_type is not known

   Relartive URLs
     abc (Host is empty, url_path is "abc" and request_type is not known
     ?abc (Host is empty, url_path is "?abc" and request_type is not known
     #abc (Host is empty, url_path is "#abc" and request_type is not known
     {abc} (Not practical case) (Host is empty, url_path is "{abc}" and request_type is not known

   Fully qualified URLs
Case2:
     http://host:port (Host is host:port, url_path is "/" and request_type is http
Case3:
     http://host:port/abc (Host is host:port, url_path is "/abc" and request_type is http
     http://host:port?name=abc (Host is host:port, url_path is "/?name=abc" and request_type is http
     http://host:port#name=abc (Host is host:port, url_path is "/#name=abc" and request_type is http
     http://host:port{abc} (Host is host:port, url_path is "{abc}" and request_type is http

   Error cases:
     Empty url
     http://
*/

/*This function will parse ns_decrypt API in the query parameter and return decrypted string
  E.g. http://host:port?name=ns_decrypt("KQnkl=") 
  Result :http://host:port?name=abc*/

void ns_parse_decrypt_api(char *input)
{
  char *output = NULL;
  char *ptr1 =  NULL;
  char *ptr2 = NULL;
  char *buf = input;
  char string[256];
  int length = 0;

  NSDL1_HTTP(NULL, NULL, "Method called, input = %s", input);
  if(!input || !input[0])
  {
    NSTL1_OUT(NULL, NULL, "Error: Input String is empty");
    return;
  }
  while(1)
  { 
    if(((ptr1 = strstr(buf,"ns_decrypt(")) != NULL && ((ptr2 = strstr(ptr1,"\")")) != NULL)))
    {
      if(!output)
        MY_MALLOC(output, strlen(input), "allocate buffer for ns_decrypt", -1);

      *ptr1 = '\0';
      length += sprintf(&output[length], "%s", buf);
      buf = ptr2 + 2; 

      ptr1+=11; // 11 = length of ns_decrypt(". Now ptr1 is pointing at " or "\".
      if(*ptr1 == '\\' && *(ptr2 - 1) == '\\') //  "Checking if ptr1 is pointing at \ and ptr2 is pointing at \ "
      {
        ptr1++;
        ptr2--;
      }
      else if(*ptr1 == '\\' && *(ptr2 - 1) != '\\') // if someone will use ns_decrypt(\"L14lXiRxcwR3") 
      {
        SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in compiling the script files");
      }
   
      else if(*ptr1 == '\"' && *(ptr2 - 1) == '\\') // if someone will use ns_decrypt("L14lXiRxcwR3\")
      {
        SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in compiling the script files");
      }

      ptr1++; // For skipping " and now ptr1 is at first character of the string
      
      snprintf(string, (ptr2-ptr1+1), "%s", ptr1);
      length += sprintf(&output[length], "%s", ns_decrypt(string));
    }
    else
    { 
      if(output)
        length += sprintf(&output[length], "%s", buf);
      break;
    }
  }
  if(output)
  {
    strcpy(input, output);
    NSDL1_HTTP(NULL, NULL, "Method end: Output = %s", input);
    FREE_AND_MAKE_NOT_NULL(output, "freeing buffer for ns_decrypt", NULL);
  }
}

int parse_url(char *in_url, char *host_end_markers, int *request_type, char *hostname, char *path)
{
  char *host_end = NULL;
  char *http_str = "http://";
  char *xhttp_str = "xhttp://";
  char *https_str = "https://";
  char *url = in_url;

  NSDL1_HTTP(NULL, NULL, "Method called. Parse URL = %s, host_end_markers = %s", url, host_end_markers);

  if (in_url[0] == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: Url is empty. url = %s\n", in_url);
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Url is empty. url = %s",
                                                in_url);
    return RET_PARSE_NOK;
  }

  // Shalu: change strncmp to strncasecmp to ignorecase of http, https and xhttp. In visa we getting HTTP in place of 
  // http in Location url
  if (!strncasecmp(in_url, http_str, strlen(http_str))) {
    *request_type = HTTP_REQUEST;
    url += strlen(http_str);
    NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
  } else if (!strncasecmp(in_url, xhttp_str, strlen(xhttp_str))) {
    *request_type = XHTTP_REQUEST;
    url += strlen(xhttp_str);
    NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
  } else if (!strncasecmp(in_url, https_str, strlen(https_str))) {
    *request_type = HTTPS_REQUEST;
    url += strlen(https_str);
    NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
  } else if (!strncmp(in_url, "//", 2)) {
    *request_type = REQUEST_TYPE_NOT_FOUND;
    url += 2;
  } else {
    *request_type = REQUEST_TYPE_NOT_FOUND;
        
    /* 
      Case 1: when Request type is not found
      hostname will be empty 
      path will be url
      eg - 
      img/test.gif     (relative)
      /img/test.gif    (absolute)
    */
    hostname[0] = '\0';
    strcpy(path, url); /* if (path[0] == '/') then absolute otherwise relative */
    NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);

    return RET_PARSE_OK;
  }

  // Check if it is empty after schema (e.g. http://)
  if (url[0] == '\0')
  {
    NSDL2_HTTP(NULL, NULL, "Error: Url host is empty. url = %s\n", in_url);
    NSTL1_OUT(NULL, NULL, "Error: Url host is empty. url = %s\n", in_url);
    return RET_PARSE_NOK;
  }

  if(host_end_markers)
     host_end = strpbrk(url, host_end_markers);

  if (!host_end) {
    /* 
      Case 2: when Request type is found and path is not there
      hostname will be url 
      path will be /
      eg -
      http://www.test.com
    */
    strcpy(hostname, url);
    strcpy(path, "/");
    return RET_PARSE_OK;
  }

  if (host_end == url) // E.g. http://?
  {
    NSDL2_HTTP(NULL, NULL, "Error: Url host is empty with query paramters. url = %s\n", in_url);
    NSTL1_OUT(NULL, NULL, "Error: Url host is empty with query paramters. url = %s\n", in_url);
    return RET_PARSE_NOK;
  }

  /* 
    Case 3: when Request type is found and path is there
    hostname will be extracted 
    path will be extracted
    eg -
    http://www.test.com/abc.html    (path - /abc.html )
    http://www.test.com?x=2         (path - /?x=2     )
    http://www.test.com#hello       (path - /#hello   )
  */
  strncpy(hostname, url, host_end - url);
  hostname[host_end - url] = '\0';

  if(*host_end == '?' || *host_end == '#')
  {
    path[0] = '/';
    strcpy(path+1, host_end);
  }

  else
  {
    strcpy(path, host_end);
  }
  NSDL2_MISC(NULL, NULL, "path of url = %s",path);

  //Here all ns_decrypt API in query parameter will be parsed.
  ns_parse_decrypt_api(path);

  return RET_PARSE_OK;
}

int parse_url_param(char *in_url, char *host_end_markers, int *request_type, char *hostname, char *path)
{
  char *host_end = NULL;
  char *http_str = "http://";
  char *xhttp_str = "xhttp://";
  char *https_str = "https://";
  char *url = in_url;

  char param_stflag = 0;
  char param_edflag = 0;
  char path_flag = 0;
  char *url_path_ptr;
  char is_param_scheme = 0;
  char is_param_host = 0;
  char is_param_port = 0;

  NSDL1_HTTP(NULL, NULL, "Method called. Parse URL = %s, host_end_markers = %s", url, host_end_markers);

  if (in_url[0] == '\0')
  {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Url is empty. url = %s", in_url);
    return RET_PARSE_NOK;
  }
  
  //Check whether URL is parameterized or not. i.e URL="{scheme}://{host:port}/path?queryString"
  if(tolower(in_url[0]) < 97 || tolower(in_url[0]) > 123)
  {
    NSTL1_OUT(NULL, NULL, "Error: URL must be starts with either '{' or any character, invalid url = %s", in_url);
    return RET_PARSE_NOK;
  }
  //Scheme is parameterized
  if(in_url[0] == '{')
  {
    is_param_scheme = 1;
    url++;
    param_stflag++;
    NSDL2_HTTP(NULL, NULL, "Scheme is parameterized");
  }
  while(*url)
  {

    NSDL2_HTTP(NULL, NULL, "path_flag = %d, *url = %c, url = %s", path_flag, *url, url);
    if((*url == ':') && (*(url + 1) == '/') && (*(url + 2) == '/'))
    {
      if(is_param_scheme)
      {
        if((*(url - 1)) != '}')
        {
          NSTL1_OUT(NULL, NULL, "Error: Scheme cannot be half parameterized, invalid url = %s", in_url);
          return RET_PARSE_NOK;
        }
        if(param_stflag != param_edflag)
        {
          NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, url = %s", in_url);
          return RET_PARSE_NOK;
        }
        param_stflag = param_edflag = 0;
      }
      url += 3;  //Skipping ://
      if (*url == '{')
      {
        param_stflag++;
        is_param_host = 1;
        NSDL2_HTTP(NULL, NULL, "Host is parameterized"); 
        url++; 
      }
      continue;
    }
    else if (*url == ':')
    {
      if(is_param_host)
      {
        if((*(url - 1)) != '}')
        { 
          NSTL1_OUT(NULL, NULL, "Error: Host cannot be half parameterized, invalid url = %s", in_url);
          return RET_PARSE_NOK;
        }
        if(param_stflag != param_edflag)
        { 
          NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, url = %s", in_url);
          return RET_PARSE_NOK;
        }
        param_stflag = param_edflag = 0;
      }
      url++; //Skiping :
      if (*url == '{')
      {
        param_stflag++; 
        is_param_port = 1;
        NSDL2_HTTP(NULL, NULL, "Port is parameterized"); 
        url++;
      }
      continue; 
    }
    else if((*url == '/') || (*url == '?') || (*url == '#'))
    {
      path_flag = 1;
      NSDL2_HTTP(NULL, NULL, "path_flag = %d, *url = %c, url = %s", path_flag, *url, url);
      url_path_ptr = url;
      if(param_stflag  != param_edflag)
      { 
        NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, url = %s", in_url);
        return RET_PARSE_NOK;
      }
      if(param_stflag)
        is_param_host = 1;
      break;
    }
    else if(*url == '{')
    {
      param_stflag++;
    }
    else if(*url == '}')
    {
      param_edflag++;
      if(param_edflag > param_stflag)
      {
        NSTL1_OUT(NULL, NULL, "Error: end brackets should be less than start brackets, url = %s", in_url);
        return RET_PARSE_NOK;
      }
    }
    url++;
  }
  if(param_stflag  != param_edflag)
  { 
    NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, url = %s", in_url);
    return RET_PARSE_NOK;
  }

  if(is_param_scheme || is_param_host || is_param_port)
  {
    *request_type = PARAMETERIZED_URL;
    url = in_url;
    NSDL2_HTTP(NULL, NULL, "path_flag = %d, path = %s", path_flag, path);
    if(path_flag)
    {
      if(url_path_ptr[0] == '/')
        strcpy(path, url_path_ptr);
      else
      {
        path[0] = '/';
        strcpy(path + 1, url_path_ptr);
      }
      url_path_ptr[0] = '\0';
    }
  }
  else 
  {
    url = in_url;
    // Shalu: change strncmp to strncasecmp to ignorecase of http, https and xhttp. In visa we getting HTTP in place of 
    // http in Location url
    if (!strncasecmp(in_url, http_str, strlen(http_str))) {
      *request_type = HTTP_REQUEST;
      url += strlen(http_str);
      NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
    } else if (!strncasecmp(in_url, xhttp_str, strlen(xhttp_str))) {
      *request_type = XHTTP_REQUEST;
      url += strlen(xhttp_str);
      NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
    } else if (!strncasecmp(in_url, https_str, strlen(https_str))) {
      *request_type = HTTPS_REQUEST;
      url += strlen(https_str);
      NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
    } else if (!strncmp(in_url, "//", 2)) {
      *request_type = REQUEST_TYPE_NOT_FOUND;
      url += 2;
    } else {
      *request_type = REQUEST_TYPE_NOT_FOUND;
      //http:{host}:port   /cgi?sdjsj        
      /* 
        Case 1: when Request type is not found
        hostname will be empty 
        path will be url
        eg - 
        img/test.gif     (relative)
        /img/test.gif    (absolute)
      */
      hostname[0] = '\0';
      strcpy(path, url); /* if (path[0] == '/') then absolute otherwise relative */
      NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
  
      return RET_PARSE_OK;
    }
  
    // Check if it is empty after schema (e.g. http://)
    if (url[0] == '\0')
    {
      NSDL2_HTTP(NULL, NULL, "Error: Url host is empty. url = %s\n", in_url);
      NSTL1_OUT(NULL, NULL, "Error: Url host is empty. url = %s\n", in_url);
      return RET_PARSE_NOK;
    }
      
    if(host_end_markers)
       host_end = strpbrk(url, host_end_markers);
 
    if (!host_end) {
      /* 
        Case 2: when Request type is found and path is not there
        hostname will be url 
        path will be /
        eg -
        http://www.test.com
      */
      strcpy(hostname, url);
      strcpy(path, "/");
      return RET_PARSE_OK;
    }
    
    if (host_end == url) // E.g. http://?
    {
      NSDL2_HTTP(NULL, NULL, "Error: Url host is empty with query paramters. url = %s\n", in_url);
      NSTL1_OUT(NULL, NULL, "Error: Url host is empty with query paramters. url = %s\n", in_url);
      return RET_PARSE_NOK;
    }
      
    /* 
      Case 3: when Request type is found and path is there
      hostname will be extracted 
      path will be extracted
      eg -
      http://www.test.com/abc.html    (path - /abc.html )
      http://www.test.com?x=2         (path - /?x=2     )
      http://www.test.com#hello       (path - /#hello   )
      http://www.test.com{path}       (path - path is parametrise   )
    */
    strncpy(hostname, url, host_end - url);
    hostname[host_end - url] = '\0';
      
    if(*host_end == '?' || *host_end == '#')
    {
      path[0] = '/';
      strcpy(path+1, host_end);
    }
    else
    {
      strcpy(path, host_end);
    }
  }
  NSDL2_MISC(NULL, NULL, "path of url = %s", path);
 
  //Here all ns_decrypt API in query parameter will be parsed.
  if(path[0])
    ns_parse_decrypt_api(path);
   
  return RET_PARSE_OK;
}

#ifdef RMI_MODE
void get_rmi_headers(FILE* fp, char* line, int req_idx, int* line_number)
{
  char* ptr;
  int got_host = 0;
  char hbuf[MAX_LINE_LENGTH];

  NSDL2_MISC(NULL, NULL, "Method called, line = '%s', req_idx = %d", line, req_idx);
  do {
    *line_number+=1;
    if (!(strncmp(line, "----", 4)))
      break;

    if (!(strncmp(line, "PREVIOUS_HOST", strlen("PREVIOUS_HOST")))) {
      requests[req_idx].proto.http.index.svr_idx = -1;
      got_host = 1;
    }

    if (!(strncmp(line, "HOST:", 5))) {
      ptr = line + 5;
      while (isspace(*ptr)) ptr++;

      if (snprintf(hbuf, MAX_LINE_LENGTH, "%s", ptr) > MAX_LINE_LENGTH) {
 	NS_EXIT(-1, "get_jboss_headers(): Host name is too big at line=%d", *line_number);
      }

      hbuf[strlen(hbuf) - 1] = '\0'; /*remove \n */

      requests[req_idx].proto.http.index.svr_idx = get_server_idx(hbuf, requests[req_idx].request_type, 
                                                                  *line_number);
      got_host = 1;
    }
  } while (fgets(line, MAX_LINE_LENGTH, fp));

  if (!got_host) {
    NS_EXIT(-1, "get_rmi_headers(): Request ending at line %d needs a host", *line_number);
  }
}

void
get_jboss_headers(FILE* fp, char* line, int req_idx, int* line_number)
{
  char* ptr;
  int got_host = 0;
  char hbuf[MAX_LINE_LENGTH];

  NSDL2_HTTP(NULL, NULL, "Method called");
  do {
    *line_number+=1;
    if (!(strncmp(line, "----", 4)))
      break;

    if (!(strncmp(line, "HOST:", 5))) {
      ptr = line+5;
      while (isspace(*ptr)) ptr++;

      if (snprintf(hbuf, MAX_LINE_LENGTH, "%s", ptr) > MAX_LINE_LENGTH) {
 	NS_EXIT(-1, "get_jboss_headers(): Host name is too big at line=%d", *line_number);
      }

      hbuf[strlen(hbuf) - 1] = '\0'; /*remove \n */

       requests[req_idx].proto.http.index.svr_idx = get_server_idx(hbuf, requests[req_idx].request_type, 
                                                                   *line_number);
       got_host = 1;
    }
  }
  while (fgets(line, MAX_LINE_LENGTH, fp));

  if (!got_host) {
    NS_EXIT(-1, "get_jboss_headers(): Request ending at line %d needs a host", *line_number);
  }
}
#endif


int 
get_headers_of_inline_url(FILE *fp, char *buf, char* hbuf, int req_idx, int line_num, int sess_idx)
{
  char* line;
  char line_arr[MAX_LINE_LENGTH];
  char temp_line[MAX_LINE_LENGTH];
  char temp_buf[MAX_REQUEST_BUF_LENGTH];
  char temp_hbuf[MAX_LINE_LENGTH];
  int done=0;
  char *ptr;
  int var_idx;
  int num_values;
  char* value;
  int server_base_set;
  int i;
  int rnum;
  char var[MAX_VAR_SIZE];
  DBT key, data;
  int ret_val;
  int got_host = 0;
  int num_read;
  //http_request* request_ptr = &requests[req_idx];
  //int num_cookies;
  //int first_cookie;
  //int name_offset;
  int num_headers = 0;

  /* header may contains body too.
   * for example for POST request.
   * Headers need to be terminated by new line
   * also whole header section is termonated by
   * ____ keyword line
   */

  NSDL2_HTTP(NULL, NULL, "Method called, req_idx = %d, line_num = %d, sess_idx = %d", req_idx, line_num, sess_idx);
  buf[0] = '\0';
  hbuf[0] = '\0';

  // Here cacheRequestHeader is initialized. this is dont to set the vale of max-age, min-fresh, max-stale to -1.
  cache_init_cache_req_hdr(&(requests[req_idx].proto.http.cache_req_hdr));

  while (fgets(line_arr, MAX_LINE_LENGTH, fp)) {

    num_read = strlen(line_arr);

    if (num_read == MAX_LINE_LENGTH-1)
      if (line_arr[MAX_LINE_LENGTH-2] != '\n') {
	NS_EXIT(-1, "Get_header(): ERROR, header line is more that %d bytes big at line=%d", MAX_LINE_LENGTH, line_num);
      }

    line = line_arr;
    IGNORE_COMMENTS (line);

    line_num++;
    num_headers++;

    if (strncasecmp(line, "User-Agent:", strlen("User-Agent:")) == 0)
      continue; // Ignore this header in the script as NetStorm makes this header from User profile

    if (strncasecmp(line, "Content-Length:", strlen("Content-Length:")) == 0)
      continue; // Ignore this header in the script as NetStorm makes this header using actual body length

    if(add_embedded_url_cookie(line, line_num, req_idx, sess_idx) == 0) // cookie found and parsed 
      continue; // Ignore this header in the script as NetStorm makes this header using cookies coming in respose

    // parse cache control header and set cacheRequestHeader flags
    cache_parse_req_cache_control(line, &(requests[req_idx].proto.http.cache_req_hdr), sess_idx, line_num, "script.detail");
    // We need to send this header in the HTTP request

    if (strncasecmp(line, "Host:", 5) == 0) {

      ptr = line+5;
      while (isspace(*ptr)) ptr++;

      strcpy(temp_hbuf, hbuf);

      if (snprintf(hbuf, MAX_LINE_LENGTH, "%s%s", temp_hbuf, ptr) > MAX_LINE_LENGTH) {
	NS_EXIT(-1, "Get_Headers(): Host name is too big at line=%d", line_num);
      }
      //printf("Anuj: Get_Headers(): The Host name is '%s' and the whole header line is '%s', line=%d\n", ptr, hbuf, line_num);

      if (hbuf[strlen(hbuf) - 2] == '\r')
          hbuf[strlen(hbuf) - 2] = '\0'; /*remove \r\n */
      else
          hbuf[strlen(hbuf) - 1] = '\0'; /*remove \n */

      if ((sscanf(hbuf, "$CAVS{%s}", var)) == 1) { /* the host name is a varible that is defined in the url variable file */
	strchr(var,'}')[0] = '\0';

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = var;
	key.size = strlen(var);

	ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

	if (ret_val == DB_NOTFOUND) {
	  NS_EXIT(-1, "Get_headers(): Variable %s not defined on line=%d", var, line_num);
	}

	if (ret_val != 0) {
	  NS_EXIT(-1, "Get_headers(): Hash Table Get failed on line=%d", line_num);
	}

	var_idx = atoi(data.data);
	num_values = groupTable[varTable[var_idx].group_idx].num_values;

	if (groupTable[varTable[var_idx].group_idx].type != SESSION) {
	  NSTL1(NULL, NULL, "Get_headers(): Warning, variable %s is being converted to a session type at line=%d\n", RETRIEVE_BUFFER_DATA(varTable[var_idx].name_pointer), line_num);
	  groupTable[varTable[var_idx].group_idx].type = SESSION;
	}

	if (num_values == 1) {
	  value = RETRIEVE_BUFFER_DATA(pointerTable[varTable[var_idx].value_start_ptr].big_buf_pointer);

	  requests[req_idx].index.svr_idx = get_server_idx(value, requests[req_idx].request_type, line_num);
	  strcpy(ptr, value);

	  strcpy(temp_line, line);

	  if (snprintf(line, MAX_LINE_LENGTH, "%s%s", temp_line, "\r\n") > MAX_LINE_LENGTH) {
	    NS_EXIT(-1, "Get_Headers(): Host name is too big at line=%d", line_num);
	  }

	  strcpy(temp_buf, buf);

	  if (snprintf(buf, MAX_REQUEST_BUF_LENGTH, "%s%s", temp_buf, line) > MAX_REQUEST_BUF_LENGTH) {
	    NS_EXIT(-1, "Get_Headers(): Host name is too big at line=%d", line_num);
	  }

	  got_host = 1;
	  continue;
	} else {
	  if (num_values > 1) {
	    requests[req_idx].index.group_idx = 0;
	    if (varTable[var_idx].server_base != -1)
	      requests[req_idx].server_base = varTable[var_idx].server_base;
	    else {
	      server_base_set = 0;
	      for (i = 0; i < num_values; i++) {

		if (create_serverorder_table_entry(&rnum) == FAILURE)
		  NS_EXIT (-1, "create_serverorders_table_entry(): Error allocating more memory for serverOrderTable entries"); 

		serverOrderTable[rnum].server_idx = get_server_idx(RETRIEVE_BUFFER_DATA(pointerTable[varTable[var_idx].value_start_ptr+i].big_buf_pointer), requests[req_idx].request_type, line_num);

		if (!server_base_set) {
		  varTable[var_idx].server_base = rnum;
		  requests[req_idx].server_base = rnum;
		  server_base_set = 1;
		}
	      }
	    }
      //printf("Anuj: Get_Headers(): The Host name Variable is '%s', line=%d\n", RETRIEVE_BUFFER_DATA(varTable[var_idx].name_pointer), line_num);
	  } else {
	    NS_EXIT(-1, "Get_headers(): Host Name Variable %s has no values on line=%d",
		   RETRIEVE_BUFFER_DATA(varTable[var_idx].name_pointer), line_num);
	  }

	  check_end_hdr_line(line, MAX_LINE_LENGTH);

	  strcpy(temp_buf, buf);

	  if (snprintf(buf, MAX_REQUEST_BUF_LENGTH, "%s%s", temp_buf, line) > MAX_REQUEST_BUF_LENGTH) {
	    NS_EXIT(-1, "Get_Headers(): Request is too big at line=%d", line_num);
	  }

	  got_host = 1;
	  continue;
	}
      }

      /* The host name is already in the url request */
      requests[req_idx].index.svr_idx = get_server_idx(hbuf, requests[req_idx].request_type, line_num);
      if(requests[req_idx].index.svr_idx != -1) {
        if(gServerTable[requests[req_idx].index.svr_idx].main_url_host == -1) 
          gServerTable[requests[req_idx].index.svr_idx].main_url_host = 0; // marking as embedded host
      }

#if 0
//Do not put in the pre-build request.
//Will be added by make request at run time

      strcpy(temp_buf, buf);

      if (snprintf(buf, MAX_REQUEST_BUF_LENGTH, "%s%s", temp_buf, line) > MAX_REQUEST_BUF_LENGTH) {
	printf("Get_Headers(): Request is too big at line=%d\n", line_num);
	exit(-1);
      }
#endif

      got_host = 1;
      continue;
    }

    if(set_header_flags(line, req_idx, sess_idx, "script.detail") == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

    if (!(strncmp(line, "----", 4))) {
      done =1;
      break;
    }

    check_end_hdr_line(line, MAX_LINE_LENGTH);

    strcpy(temp_buf, buf);

    if (snprintf(buf, MAX_REQUEST_BUF_LENGTH, "%s%s", temp_buf, line) > MAX_REQUEST_BUF_LENGTH) {
      NS_EXIT(-1, "Get_Headers(): Request is too big at line=%d", line_num);
    }

    if (!strcmp(line, "\r\n")) {
      done = 1;
      break;
    }

  } /* while (num_read = fgets(line, MAX_LINE_LENGTH, fp)) */

  if (!got_host) {
    NS_EXIT(-1, NULL, NULL, "Get_headers(): Error: Request ending at line=%d has no host name", line_num);
  }

  if (done)
    return num_headers;
  else
    return -1;
}

// Removing get_url_options as no longer required . this Method was only used for legacy script 

#ifdef RMI_MODE
int inline hex_to_int(char digit) {
  if (isdigit(digit))
    return digit - 48;
  else {
    switch (digit) {
    case 'A':
    case 'a':
      return 10;
    case 'B':
    case 'b':
      return 11;
    case 'C':
    case 'c':
      return 12;
    case 'D':
    case 'd':
      return 13;
    case 'E':
    case 'e':
       return 14;
    case 'F':
    case 'f':
      return 15;
    }
  }
  return -1;
}

int
bytevar_comp(const void* bytevar1, const void* bytevar2) {
  if (((ReqByteVarTableEntry*) bytevar1)->offset > ((ReqByteVarTableEntry*) bytevar2)->offset)
    return 1;
  else
    return 0;
}

void
create_rmi_segments(StrEnt* segtable, char* line, FILE* fp, int req_idx, int* line_number, int sess_idx) {
   char* line_ptr;
   int rnum;
   int first_segment = 1;
   char var[MAX_LINE_LENGTH];
   int var_length;
   DBT key, data;
   int ret_val;
   char line_char;
   int state;
   int number = 0;
   char element_buffer[MAX_NUM_ELEMENTS];
   int num_elements;
   int point_rnum;
   char* bytevar_start;
   char* bytevar_end;
   int bytevar_idx;
   int bytevar_length;
   int first_byte_var;
   char bytevar_params[MAX_LINE_LENGTH];
   char bytevar_name[MAX_LINE_LENGTH];
   int bytevar_offset;
   int bytevar_type;
   int bytes_to_yank;

   do {
     if (strncmp(line, "----", strlen("----")) == 0) {
       bytevar_start = line + strlen("----");
       first_byte_var = 1;
       while(*bytevar_start != '\n') {
	 while (*bytevar_start == ' ') bytevar_start++;

	 if (strncasecmp(bytevar_start, "CAVB", strlen("CAVB")) == 0) {
	   bytevar_start += strlen("CAVB");
	   if (*bytevar_start != '=') {
	     NS_EXIT(-1, "create_rmi_segments(): cavb variable format incorrect (CAVB=<variable_name>,<byte offset>,<num bytes>,<inc option>) at line=%d", *line_number);
	   }
	   bytevar_start++;

	   if (!(bytevar_end = strchr(bytevar_start, ','))) {
	     NS_EXIT(-1, "create_rmi_segments(): cavb variable format incorrect (CAVB=<variable_name>,<byte offset>,<num bytes>,<inc option>) at line=%d", *line_number);
	   }

	   bytevar_length = bytevar_end - bytevar_start;
	   memcpy(bytevar_name, bytevar_start, bytevar_length);
	   bytevar_start = bytevar_end + 1;

	   if (!(bytevar_end = strchr(bytevar_start, ' ')))
	     bytevar_end = strchr(bytevar_start, '\n');
	   bytevar_length = bytevar_end - bytevar_start;
	   memcpy(bytevar_params, bytevar_start, bytevar_length);
	   bytevar_params[bytevar_length] = 0;

	   if (sscanf(bytevar_params, "%d,%d,%d", &bytevar_offset, &bytes_to_yank, &bytevar_type) != 3) {
	     NS_EXIT(-1, "create_rmi_segments(): cavb varaible format incorrect (CAVB=<variable_name>,<byte offset>,<num bytes>,<inc option>) at line=%d", *line_number);
	   }

	   sprintf(bytevar_name, "%s!%s", bytevar_name, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

	   if ((bytevar_idx = find_bytevar_idx(bytevar_name, strlen(bytevar_name))) == -1) {
	     if ((create_bytevar_table_entry(&bytevar_idx)) != SUCCESS) {
	       NS_EXIT(-1, "create_rmi_segments(): Error, could not allocate memory for bytevar table");
	     }

	     if ((byteVarTable[bytevar_idx].name = copy_into_big_buf(bytevar_name, 0)) == -1) {
	       NS_EXIT(-1, "create_rmi_segments(): Error in copying data into big buf");
	     }
	   }

	   if ((create_reqbytevar_table_entry(&rnum) != SUCCESS)) {
	     NS_EXIT(-1, "create_rmi_segments(): Error, could not allocate memory for bytevar table");
	   }

	   reqByteVarTable[rnum].name = byteVarTable[bytevar_idx].name;
	   reqByteVarTable[rnum].length = strlen(bytevar_name);
	   reqByteVarTable[rnum].offset = bytevar_offset;
	   reqByteVarTable[rnum].byte_length = bytes_to_yank;
	   reqByteVarTable[rnum].type = bytevar_type;

	   if (first_byte_var) {
 	     requests[req_idx].proto.http.bytevars.bytevar_start = rnum;
	     first_byte_var = 0;
 	  }

	   requests[req_idx].bytevars.num_bytevars++;

	   bytevar_start = bytevar_end;
	 }
       }

       qsort(&reqByteVarTable[requests[req_idx].bytevars.bytevar_start], requests[req_idx].bytevars.num_bytevars,
	     sizeof(ReqByteVarTableEntry), bytevar_comp);

       break;
     }

     if (create_seg_table_entry(&rnum) != SUCCESS) {
       NS_EXIT(-1, "create_rmi_segments(): could not get new seg table entry");
     }

     if (first_segment) {
       segtable->seg_start = rnum;
       first_segment = 0;
     }

     segtable->num_entries++;

     if (strncmp(line, "$CAVU", strlen("$CAVU")) == 0) {
       sscanf(line, "$CAVU{%s}", var);
       strchr(var, '}')[0] = '\0';

       sprintf(var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

       var_length = strlen(var);

       memset(&key, 0, sizeof(DBT));
       memset(&data, 0, sizeof(DBT));

       key.data = var;
       key.size = strlen(var) + 1;

       ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

       if (ret_val == DB_NOTFOUND) {
	 NS_EXIT(-1, "create_rmi_segments(): variable %s not defined on line=%d", var, *line_number);
       }

       if (ret_val != 0) {
	 NS_EXIT(-1, "create_rmi_segments(): Hash Table Get failed on line=%d", *line_number);
       }

       segTable[rnum].type = UTF_VAR;
       segTable[rnum].offset = (int) atoi((char*)data.data);

       *line_number+=1;

       continue;
     }

     if (!strncasecmp(line, "$CAVL{", strlen("$CAVL{"))) {
       sscanf(line, "$CAVL{%s}", var);
       strchr(var, '}')[0] = '\0';

       sprintf(var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

       var_length = strlen(var);

       memset(&key, 0, sizeof(DBT));

       key.data = var;
       key.size = strlen(var) + 1;

       ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

       if (ret_val == DB_NOTFOUND) {
	 NS_EXIT(-1, "Segment_line(): Variable %s not defined on line=%d", var, *line_number);
       }

       if (ret_val != 0) {
	 NS_EXIT(-1, "Segment_line(): Hash Table Get failed on line=%d", *line_number);
       }

       segTable[rnum].type = LONG_VAR;
       segTable[rnum].offset = (int) atoi((char*)data.data);

       *line_number+=1;

       continue;
     }

     if (!strncasecmp(line, "$CAVB{", strlen("$CAVB"))) { /* case for byte variables */
       sscanf(line, "$CAVB{%s}", var);
       strchr(var,'}')[0] = '\0';

       sprintf(var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

       var_length = strlen(var);

       if ((bytevar_idx = find_bytevar_idx(var, var_length)) == -1) {
	 NS_EXIT(-1, NULL, NULL, "Segment_line(): Could not find an entry for the CAVB variable %s on line = %d", var, *line_number);
       }

       segTable[rnum].type = BYTE_VAR;
       segTable[rnum].offset = bytevar_idx;

       *line_number += 1;

       continue;
     }

     if (!strncasecmp(line, "$CAVS{", strlen("$CAVS"))) { /* case for regular variables */
       sscanf(line, "$CAVS{%s}", var);
       strchr(var,'}')[0] = '\0';

       sprintf(var, "%s!%s", var, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));

       memset(&key, 0, sizeof(DBT));
       memset(&data, 0, sizeof(DBT));

       key.data = var;
       key.size = strlen(var) + 1;

       ret_val = var_hash_table->get(var_hash_table, NULL, &key, &data, 0);

       if (ret_val == DB_NOTFOUND) {
	 NS_EXIT(-1, "Segment_line(): Variable %s not defined on line=%d", var, *line_number);
       }

       if (ret_val != 0) {
	 NS_EXIT(-1, "Segment_line(): Hash Table Get failed on line=%d", *line_number);
       }

       segTable[rnum].type = VAR;
       segTable[rnum].offset = (int) atoi((char*)data.data);

       *line_number+=1;

       continue;
     }

     state = FIRST_NUM_STATE;
     num_elements = 0;
     for (line_ptr = line; *line_ptr != '\0'; line_ptr++) {

       line_char = *line_ptr;

       switch(state) {
       case FIRST_NUM_STATE:
	 if (isxdigit(line_char)) {
	   number = 16 * hex_to_int(line_char);
	   state = SECOND_NUM_STATE;
	   break;
	 } else {
	   NS_EXIT(-1, "create_rmi_segments(): Format of url file incorrect at line %d", *line_number);
	 }
       case SECOND_NUM_STATE:
	 if (isxdigit(line_char)) {
	   number += hex_to_int(line_char);

	   element_buffer[num_elements++] = number;
	   number = 0;

	   if (num_elements > MAX_NUM_ELEMENTS) {
	     NS_EXIT(-1, "create_rmi_segments(): Too man elements in url file at line %d", *line_number);
	   }

	   state = SPACE_STATE;
	   break;
	 } else {
	   NS_EXIT(-1, "create_rmi_segments(): Format of url file incorrect at line %d", *line_number);
	 }
       case SPACE_STATE:
	 if (isspace(line_char)) {
	   state = FIRST_NUM_STATE;
	   break;
	 } else {
	   NS_EXIT(-1, "create_rmi_segments(): Format of url file incorrect at line %d", *line_number);
	 }
       }
     }

     AddPointerTableEntry (&point_runm, element_buffer, num_elements);
     segTable[rnum].offset = point_rnum;
     segTable[rnum].type = STR;
     *line_number+=1;
   } while (fgets(line, MAX_LINE_LENGTH, fp));
}
#endif


#ifndef RMI_MODE

// Removing this as this was required only in legacy script . We no longer support legacy script . 

void arrange_pages(int sess_idx)
{
  int num_pages, first_page, page_index;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called sess_idx=%d",sess_idx);

  num_pages = gSessionTable[sess_idx].num_pages;
  first_page = gSessionTable[sess_idx].first_page;

  NSDL2_SCHEDULE(NULL, NULL, "Session # %d Name = %s num_pages = %d first_page = %d\n", 
               sess_idx, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), 
               gSessionTable[sess_idx].num_pages, first_page);

  for (page_index = 0; page_index < num_pages; page_index++) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "\tPage # %d Name = %s num_urls = %d first_url = %d\n", 
             page_index + first_page, RETRIEVE_BUFFER_DATA(gPageTable[page_index + first_page ].page_name),
             gPageTable[page_index + first_page].num_eurls,
             gPageTable[page_index + first_page].first_eurl);

    arrange_page_urls(&gPageTable[page_index + first_page]);
    //calculate urlset
    //FD_ZERO(&gPageTable[page_index + first_page].urlset);

    //for (index = 0; index < gPageTable[page_index + first_page].num_eurls; index++) 
   // {
    //  FD_SET (index, &gPageTable[page_index + first_page].urlset);
    //}
#ifdef NS_DEBUG_ON
    dump_URL(page_index + first_page);
#endif
  }
  NSDL2_SCHEDULE(NULL, NULL, "Exiting Method");
}


// Removing this as legacy script is no longer supported NS . This function was used only in case of legacy script  
#endif

#ifdef RMI_MODE
int
get_rmi_requests(FILE *fp)
{
  char line[MAX_REQUEST_BUF_LENGTH];
  int ii=0, jj; /* requests idx */
  int done = 0;
  int ret;
  int sess=0, pg=0, murls =0, eurls = 0, tx=0;
  int cur_state = NS_ST_SESSION;
  int s, p, np, f;
  struct http_charac charac, dummy;
  int new_tx = 0;
  short* keep_conn_flag;
  int line_number = 0;
  char err_msg[MAX_ERR_MSG_LINE_LENGTH + 1];

  while(!done) {
    switch (cur_state) {
    case NS_ST_SESSION:
      ret = get_element(fp, line, &dummy, &line_number, cap_fname);
      if (ret != NS_SESSION) {
	printf("Expecting Session on line %d\n", line_number);
	return -1;
      } else {
	if ((g_cur_session = find_session_idx(line)) == -1) {
	  NSTL1_OUT(NULL, NULL, "get_url_requests: Unknown session %s\n", line);
	  return -1;
	}
	gSessionTable[g_cur_session].num_pages = 0;
	gSessionTable[g_cur_session].completed = 0;
	sess++;
	cur_state = NS_ST_PAGE;
      }
      break;
    case NS_ST_PAGE:
      ret = get_element(fp, line, &charac, &line_number, cap_fname);
      if (ret == NS_PAGE) {
	if (create_page_table_entry(&g_cur_page) != SUCCESS)
	  return -1;
	if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(line, 0)) == -1) {
	  NS_EXIT(-1, "get_rmi_requests: failed to copy into big buffer");
	}
	if (gSessionTable[g_cur_session].num_pages == 0)
	  gSessionTable[g_cur_session].first_page = g_cur_page;
	gSessionTable[g_cur_session].num_pages++;
	gPageTable[g_cur_page].num_eurls = 0;
	gPageTable[g_cur_page].head_hlist = -1;
	gPageTable[g_cur_page].tail_hlist = -1;
	pg++;
	if (!charac.keep_conn_open)
	  keep_conn_flag = NULL;
	cur_state = NS_ST_CMDBEGIN;
      } else {
	printf("Expecting Page or TxBegin on line %d\n", line_number);
	return -1;
      }
      break;
    case NS_ST_TXBEGIN:
      ret = get_element(fp, line, &charac, &line_number, cap_fname);
      if (ret == NS_PAGE) {
	if (create_page_table_entry(&g_cur_page) != SUCCESS)
	  return -1;
	if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(line, 0)) == -1) {
	  NS_EXIT(-1, "get_rmi_requests: failed to copy into big buffer");
	}
	if (gSessionTable[g_cur_session].num_pages == 0)
	  gSessionTable[g_cur_session].first_page = g_cur_page;
	gSessionTable[g_cur_session].num_pages++;
	gPageTable[g_cur_page].num_eurls = 0;
	gPageTable[g_cur_page].head_hlist = -1;
	gPageTable[g_cur_page].tail_hlist = -1;
	pg++;
	if (!charac.keep_conn_open)
	  keep_conn_flag = NULL;
	cur_state = NS_ST_CMDBEGIN;
      } else {
	printf("Expecting Page on line %d\n", line_number);
	return -1;
      }
      break;
    case NS_ST_CMDBEGIN:
      ret = get_element(fp, line, &charac, &line_number, cap_fname);
      if (ret == NS_CMDBEGIN) {
	/* This is main url */
	ret = get_element(fp, line, &dummy, &line_number, cap_fname);
	if ((ret != NS_CMD) && (charac.type != PING_ACK_REQUEST)) {
	  printf("Expecting no reserved keyword, HTTP (Main URL)command expected on line %d\n",
		 line_number);
	  return -1;
	}

	if (create_requests_table_entry(&ii, charac.type) != SUCCESS)
	  return -1;
       
        proto_based_init(ii, charac.type);
	//requests[ii].request_type = charac.type;
	requests[ii].proto.http.rx_ratio = charac.rx_ratio;
	requests[ii].proto.http.tx_ratio = charac.tx_ratio;

	switch (requests[ii].request_type) {
	case PING_ACK_REQUEST:
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  if (keep_conn_flag)
	    *keep_conn_flag = 1;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	case RMI_CONNECT_REQUEST:
	  get_rmi_headers(fp, line, ii, &line_number);
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	case RMI_REQUEST:
	  requests[ii].proto.http.exact = charac.exact;
	  requests[ii].proto.http.bytes_to_recv = charac.bytes_to_recv;
	  create_rmi_segments(&(requests[ii].proto.http.url), line, fp, ii, &line_number, g_cur_session);
	  requests[ii].proto.http.index.svr_idx = -1;
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  if (keep_conn_flag)
	    *keep_conn_flag = 1;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	case JBOSS_CONNECT_REQUEST:
	  requests[ii].proto.http.first_mesg_len = charac.first_mesg_len;
	  get_jboss_headers(fp, line, ii, &line_number);
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  murls++;
	  break;

	default:
	  NSTL1(NULL, NULL, "Main request should not be of type %d", requests[ii].proto.http.type);
	  NS_EXIT(-1, NULL, NULL, "Main request should not be of type %d", requests[ii].proto.http.type);
	}
	cur_state = NS_ST_ANY;
      } else {
	printf("Expecting ---- (URL begin marker)on line %d\n",
	       line_number);
	return -1;
      }
      break;
    case NS_ST_ANY:
      ret = get_element(fp, line, &charac, &line_number, cap_fname);
      if (ret == NS_CMDBEGIN) {
	/* this is embedded URL */

	ret = get_element(fp, line, &dummy, &line_number, cap_fname);

	if ((ret != NS_CMD) && (charac.type != PING_ACK_REQUEST)) {

	  printf("Expecting no reserved keyword, HTTP command ( embedded URL) expected on line %d\n",
		 line_number);
	  return -1;
	}

	if (create_requests_table_entry(&ii, charac.type) != SUCCESS)
	  return -1;

        proto_based_init(ii, charac.type);
	//requests[ii].request_type = charac.type;
	requests[ii].proto.http.rx_ratio = charac.rx_ratio;
	requests[ii].proto.http.tx_ratio = charac.tx_ratio;

	switch (requests[ii].request_type) {
	case PING_ACK_REQUEST:
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  if (keep_conn_flag)
	    *keep_conn_flag = 1;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	case RMI_CONNECT_REQUEST:
	  get_rmi_headers(fp, line, ii, &line_number);
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	case RMI_REQUEST:
	  requests[ii].proto.http.exact = charac.exact;
	  requests[ii].proto.http.bytes_to_recv = charac.bytes_to_recv;
	  create_rmi_segments(&(requests[ii].proto.http.url), line, fp, ii, &line_number, g_cur_session);
	  requests[ii].proto.http.index.svr_idx = -1;
	  if (gPageTable[g_cur_page].num_eurls == 0)
	    gPageTable[g_cur_page].first_eurl = ii;
	  gPageTable[g_cur_page].num_eurls++;
	  if (keep_conn_flag)
	    *keep_conn_flag = 1;
	  keep_conn_flag = &requests[ii].proto.http.keep_conn_flag;
	  eurls++;
	  break;

	default:
	  NSTL1(NULL, NULL, "request type %d should not be an embedded request", requests[ii].proto.http.type);
	  NS_EXIT(-1, "request type %d should not be an embedded request", requests[ii].proto.http.type);
	}
	requests[ii].proto.http.type = EMBEDDED_URL;
	cur_state = NS_ST_ANY;
      } else if (ret == NS_PAGE) {
	if (create_page_table_entry(&g_cur_page) != SUCCESS)
	  return -1;
	if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(line, 0)) == -1) {
	  NS_EXIT(-1, "get_rmi_requests: failed to copy into big buffer");
	}
	if (gSessionTable[g_cur_session].num_pages == 0)
	  gSessionTable[g_cur_session].first_page = g_cur_page;
	gSessionTable[g_cur_session].num_pages++;
	gPageTable[g_cur_page].num_eurls = 0;
	gPageTable[g_cur_page].head_hlist = -1;
	gPageTable[g_cur_page].tail_hlist = -1;
	pg++;
	if (!charac.keep_conn_open)
	  keep_conn_flag = NULL;
	cur_state = NS_ST_CMDBEGIN;
      } else if (ret == NS_SESSION) {
	if ((g_cur_session = find_session_idx(line)) == -1) {
	  NSTL1_OUT(NULL, NULL, "get_url_requests: Unknown session %s\n", line);
	  return -1;
	}
	gSessionTable[g_cur_session].num_pages = 0;
	gSessionTable[g_cur_session].completed = 0;

	sess++;
	cur_state = NS_ST_PAGE;
      } else if (ret == NS_NONE) {
	/* Mark earler Tx end, if any */
	done = 1;
      } else {
	printf("Expecting Page or TxBegin on line %d\n", line_number);
	return -1;
      }
      break;
    default:
      return -1;
    }
  }

  //printf("Processed: %d Sessions, %d Tx, %d Pages, %d Main Urls, %d Embedded Urls\n", sess, tx, pg, murls, eurls);

  for  (s=0; s < sess; s++)  {
    np = gSessionTable[s].num_pages;
    f = gSessionTable[s].first_page;
    NSDL2_SCHEDULE(NULL, NULL, "Session # %d Name = %s num_pages = %d first_page = %d\n", s, RETRIEVE_BUFFER_DATA(gSessionTable[s].sess_name), gSessionTable[s].num_pages, f);
    for (p=0; p<np; p++) {
      //calculate urlset
      FD_ZERO(&gPageTable[p+f].urlset);
      for (jj =0; jj < gPageTable[p+f].num_eurls; jj++) {
	FD_SET (jj, &gPageTable[p+f].urlset);
      }
      NSDL3_SCHEDULE(NULL, NULL, "\tPage # %d Name = %s num_urls = %d first_url = %d\n", p+f, RETRIEVE_BUFFER_DATA(gPageTable[p+f].page_name),
	       gPageTable[p+f].num_eurls,
	       gPageTable[p+f].first_eurl);
    }
  }
  return (ii);
}
#endif

#ifdef RMI_MODE
int
create_bytevar_hash() {
  FILE* bytevar_file, *hbytevar_file, *bytevar_hash_file;
  ByteVarTableEntry* bytevar_ptr;
  int i;
  char* bytevar_name;
  int bytevar_size;
  char file_buffer[MAX_FILE_NAME];
  char buffer[MAX_LINE_LENGTH];
  char fname[MAX_LINE_LENGTH];
  char absol_bytevar[MAX_LINE_LENGTH];
  char* buf_ptr;
  void* handle;
  char* error;

  /* first create bytevar.txt */
  sprintf(fname, "%s/bytevar.txt", g_ns_tmpdir);
  if ((bytevar_file = fopen(fname, "w+")) == NULL) {
    NSTL1_OUT(NULL, NULL, "Error in creating the bytevar.txt file\n");
    return -1;
  }

  for (sess_idx = 0; sess_idx < total_sess_entries; sess_idx) {
    for (i = 0, bytevar_ptr = byteVarTable; i < total_bytevar_entries; i++, bytevar_ptr++) {
      bytevar_name = RETRIEVE_BUFFER_DATA(bytevar_ptr->name);
      sprintf(absol_bytevar, "%s!%s\n", bytevar_name, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
      bytevar_size = strlen(absol_bytevar);
      if (fwrite(absol_bytevar, sizeof(char), bytevar_size, bytevar_file) != bytevar_size) {
	NSTL1_OUT(NULL, NULL, "create_bytevar_hash(): Error in writing bytevar into bytevar.txt file\n");
	return -1;
      }
    }
  }

  fclose(bytevar_file);

  /* call the library method to create hash for the bytevar.txt file*/
  max_bytevar_hash_code = generate_hash_table_ex_ex("bytevar.txt", "in_bytevar_set", &in_bytevar_hash, &bytevar_get_key, "hash_bytevar",
                                                                                  &bytevar_hash, NULL, 0, g_ns_tmpdir, err_msg);
  if(max_bytevar_hash_code < 0 )
  {
   write_log_file(NS_SCENARIO_PARSING, "Failed to generate hash, error:%s", err_msg)
   return max_bytevar_hash_code;
  }

  return 0;
}
#endif

void create_default_sessprof() {
  int sessprofidx_idx;
  int avg_pct;
  int start_rnum = -1;
  int pct_left;
  int i;
  int rnum;

  if (total_sess_entries) {
    if (create_sessprofindex_table_entry(&sessprofidx_idx) != SUCCESS) {
      NS_EXIT(-1, "create_default_sessprof(): Error in getting sessprofindex_table entry");
    }

    if ((sessProfIndexTable[sessprofidx_idx].name = copy_into_big_buf("_DefaultSessProf", 0)) == -1) {
      NS_EXIT(-1, CAV_ERR_1000018, "_DefaultSessProf");
    }

    avg_pct = 100/total_sess_entries;

    for ( i=0 ; i<total_sess_entries; i++) {
      if (create_sessprof_table_entry(&rnum) != SUCCESS) {
	NS_EXIT(-1, "read_conf_file(): Error in getting sessprof_table entry");
      }
      if (start_rnum == -1)
	start_rnum = rnum;
      sessProfTable[rnum].sessprofindex_idx = sessprofidx_idx;
      sessProfTable[rnum].session_idx = i;
      sessProfTable[rnum].pct = avg_pct;
    }

    if ((pct_left = (100 - (avg_pct * total_sess_entries)))) {
      while (pct_left) {
	for ( i=0 ; i<total_sess_entries; i++, start_rnum++) {
	  sessProfTable[start_rnum].pct++;
	  pct_left--;
	  if (!pct_left)
	    break;
	}
      }
    }
  }
}

int insert_default_errorcodes(void) {

#ifdef WS_MODE_OLD
  if (insert_default_ws_errorcodes() != 0) {
    NSTL1_OUT(NULL, NULL, "insert_default_errocodes: error in inserting default ws errorcodes\n");
    return -1;
  }
#endif

  return 0;
}

/* 
Purpose:
  This method used to read user defined library or flags from script.libs file that will be in script directory
Arguments:
   - session_name is a script name (input argument)
   - script_libs_flag will keep all flags and libraries that defined by user in script.libs file. (output argument)
     - If file is empty or only commented lines are there then ignore that lines, and script_libs_flag will keep "" value
     - script_libs_flag will have concatenation of libraries and/or flags
Note:
 - User can use methods in script.c which are in system libraries/user libraries which is NOT used by netstorm
 - It means to add libraries/flags in the script.libs file which will be linked to netstorm at the run time.
 - script.libs file will be in script directory. It will have one or more lines with all libraries and/or flag
Example:
   -g -lcurl -lmylib
   -l /home/cavisson/test/mylib/abc.so
   -l /home/cavisson/test/mylib/xyz.a
*/
void read_script_libs(char *sess_name, char *script_libs_flag, int sess_idx)
{
  FILE *fp;
  char text[MAX_LINE_LENGTH];
  char buff[MAX_LINE_LENGTH];
  char file_name[MAX_LINE_LENGTH];
  char buffr[MAX_LINE_LENGTH];
  char *exptr = NULL; 
  script_libs_flag[0] = '\0'; 

  /* Fist we have chaeck script.libs is exist ignore hiden file,
   * If script.libs is not exist then check hiden file .script.libs.
   * If hiden file is also not exist do nathing */
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(file_name, "%s/%s/script.libs", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"));
  //Previously taking with only script name
  //sprintf(file_name, "./scripts/%s/script.libs", get_sess_name_with_proj_subproj(sess_name));
  NSDL3_HTTP(NULL, NULL, "file_name = %s", file_name);
  fp = fopen(file_name, "r");
  if (fp == NULL) 
  {
    NSDL3_HTTP(NULL, NULL, "User defined library file %s not found", file_name);
    sprintf(file_name, "%s/%s/.script.libs", GET_NS_RTA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"));
    //Previously taking with only script name
    //sprintf(file_name, "./scripts/%s/.script.libs", get_sess_name_with_proj_subproj(sess_name));
    fp = fopen(file_name, "r");
    if(fp == NULL)
    {
      NSDL3_HTTP(NULL, NULL, "User defined library file %s not found", file_name);
      return;
    }
  }

  while(fgets(buff, MAX_LINE_LENGTH, fp) != NULL)
  {
    buff[strlen(buff) - 1] = '\0';  // Replace new line by Null
     if((buff[0] == '#') || buff[0] == '\0')
      continue;
    sscanf(buff, "%s", text);

    exptr = strrchr(buff, '.');
    if(exptr) 
    {
      exptr++;
      if(!strcmp(exptr, "c")) 
      {
        if(buff[0] != '/') 
        {
          sprintf(buffr, "%s/%s/%s", GET_NS_TA_DIR(), 
                   get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), buff);
                   //Previously taking with only script name
                   //get_sess_name_with_proj_subproj(sess_name), buff);
          NSDL2_HTTP(NULL, NULL, "buffr with NS_TA_DIR(%s) = %s", GET_NS_TA_DIR(), buffr);
          strcat(script_libs_flag, buffr); 
          strcat(script_libs_flag, " "); 
          continue;
        }
      }
    }

    strcat(script_libs_flag, buff);
    strcat(script_libs_flag, " ");  

  }
  fclose(fp);
}


// Removing create_page_script_ptr as this function is used in case of legacy script only . Er no longer support Legacy script in NS.                                                                       
 

//Anuj: get_tx_names () moved to ns_trans_parse.c

//Anuj: get_tx_hash () moved to ns_trans_parse.c


// Removing read_script_start() as used only in legacy . 

// Removing read_script_middle function as no longer required. We now noo longer support legacy script

// Removing parse_script_mode0 as this function is used for parsing oflegacy script only . We are now not supporting legacy script .  

static void generators_entry_in_topology()
{
  int i;
  char err_buf[MAX_LINE_SIZE];
  err_buf[0]='\0';
  for (i = 0; i < sgrp_used_genrator_entries; i++)
  {
    max_server_id_used += 1;
    //By default let generator be autoscaled from cmon to apply monitors
    topolib_add_generator_entry(generator_entry[i].IP, (char *)generator_entry[i].gen_name,self_tier_id,topo_idx,err_buf);
    
  }
}

// keyword parsing usages 
static void ns_stop_test_if_dnsmasq_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of STOP_TEST_IF_DNSMASQ_NOT_RUNNING keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: STOP_TEST_IF_DNSMASQ_NOT_RUNNING <mode>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is used to stop the test if dnsmasq not running.\n");
  NSTL1_OUT(NULL, NULL, "    Mode: Mode for enable/disable. It can only be 0, 1\n");
  NSTL1_OUT(NULL, NULL, "      0 - Disable(default).\n");
  NSTL1_OUT(NULL, NULL, "      1 - Enable.\n");
  NS_EXIT(-1, "Error: Invalid value of STOP_TEST_IF_DNSMASQ_NOT_RUNNING keyword: %s\nUsage: STOP_TEST_IF_DNSMASQ_NOT_RUNNING <mode>", err);
}

int kw_set_stop_test_if_dnsmasq_not_run(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
        
  num = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);
    
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);
    
  if(num != 2)
  { 
    ns_stop_test_if_dnsmasq_usages("Invalid number of arguments");
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    ns_stop_test_if_dnsmasq_usages("STOP_TEST_IF_DNSMASQ_NOT_RUNNING mode is not numeric");
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    ns_stop_test_if_dnsmasq_usages("STOP_TEST_IF_DNSMASQ_NOT_RUNNING mode is not valid");
  }

  global_settings->stop_test_if_dnsmasq_not_run = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->stop_test_if_dnsmasq_not_run = %d", global_settings->stop_test_if_dnsmasq_not_run);

  return 0;
}

void *ns_do_script_validation(void *args)
{
  char script_path[512], cmd[2048], script_libs_flag[2048];
  char script_version[64];
  int script_type, sess_idx, ret;
  static int script_error = 0;
  char err_msg[1024]= "\0";
  char *runlogic_list = NULL;
  // Initializing thread local storage
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);
  
  NSDL2_PARSING(NULL, NULL, "Method called , total_sess_entries = %d", total_sess_entries);
  for(sess_idx = 0; sess_idx < total_sess_entries; sess_idx++)
  {
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    sprintf(script_path, "%s/%s", GET_NS_RTA_DIR(),
          get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));

    NSDL2_PARSING(NULL, NULL, "script_path = %s", script_path);
    ret = get_script_type(script_path, &script_type, script_version, sess_idx);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    { 
      continue; 
    }   
    
    script_libs_flag[0] = '\0';
    read_script_libs(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), script_libs_flag, sess_idx);

    get_used_runlogic_unique_list(sess_idx, &runlogic_list);
    //Create command to gcc file and produce error if any on console/TestRunOutput.log
    //sprintf(cmd, "gcc -c -fgnu89-inline -w -I%s/include/ %s", g_ns_wdir, filename);
    if ((run_mode_option & RUN_MODE_OPTION_COMPILE) || (gSessionTable[sess_idx].flags & ST_FLAGS_SCRIPT_OLD_FORMAT))
      sprintf(cmd, "$NS_WDIR/bin/nsu_validate_script %d %d %s '%s' %s %s", testidx, script_type, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), script_libs_flag, GET_NS_WORKSPACE(), GET_NS_PROFILE());
    else
      sprintf(cmd, "$NS_WDIR/bin/nsu_validate_script %d %d %s '%s' %s %s %s", testidx, script_type, get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), script_libs_flag, GET_NS_WORKSPACE(), GET_NS_PROFILE(), runlogic_list);

    NSDL2_PARSING(NULL, NULL, "cmd = %s", cmd);
    ret = nslb_system(cmd,1,err_msg);
    if(ret)
    {
      script_error = 1;
    }
  }

  NSDL2_PARSING(NULL, NULL, "Method exit");
  TLS_FREE_AND_RETURN(&script_error);
}

void default_ramp_down_phase(Schedule *schedule, char* grp_phase_name, int difference_of_ramp_up_down)
{
  Phases *ph;
  int phase_idx = 0;
  NSDL2_PARSING(NULL, NULL, "Method called");
 
  schedule->num_phases = (schedule->num_phases + 1);
  phase_idx = (schedule->num_phases - 1);
  MY_REALLOC(schedule->phase_array, sizeof(Phases) * schedule->num_phases, "phase_array", schedule->num_phases);
  ph = &(schedule->phase_array[phase_idx]);
  memset(ph, 0, sizeof(Phases));
  
  strcpy(ph->phase_name, grp_phase_name); 

  ph->phase_type = SCHEDULE_PHASE_RAMP_DOWN;                                       //Fill phase type ramp down
  ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;         //Fill ramp down phase mode immediate
  ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;      //Fill ramp down pattern Linearly
  ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = difference_of_ramp_up_down;   //Fill difference of ramp_up and ramp_down users
  NSTL1(NULL,NULL, "phase_name = %s, ramp_down_mode = %d, ramp_down_pattern = %d, num_vusers_or_sess = %d, phase_type = %d,"                                       "phase_idx = %d, num_phases = %d" , ph->phase_name, ph->phase_cmd.ramp_down_phase.ramp_down_mode, 
                    ph->phase_cmd.ramp_down_phase.ramp_down_pattern, ph->phase_cmd.ramp_down_phase.num_vusers_or_sess, 
                    ph->phase_type, phase_idx, schedule->num_phases);
} 

int find_total_ramp_up_down_users(Schedule *schedule)
{
  Phases *ph;
  int phase_idx;
  int remaining_ramp_up_users = 0;
  NSDL2_PARSING(NULL, NULL, "Method called");

  //Compute Ramp up user and find how many users are remaining to get rampdown 
  for(phase_idx = 0; phase_idx < schedule->num_phases; phase_idx++) {
    ph = &(schedule->phase_array[phase_idx]);
    if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      NSDL2_PARSING(NULL, NULL, "num_vusers_or_sess = %d", ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);
      remaining_ramp_up_users += ph->phase_cmd.ramp_up_phase.num_vusers_or_sess;
    } 
    else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      NSDL2_PARSING(NULL, NULL, "num_vusers_or_sess = %d", ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
      if(ph->phase_cmd.ramp_up_phase.num_vusers_or_sess != -1) {
        remaining_ramp_up_users -= ph->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      } else {
        remaining_ramp_up_users = 0;
      }
    }
  }

  NSTL1(NULL, NULL, "remaining_ramp_up_users = %d", remaining_ramp_up_users);

  return remaining_ramp_up_users;
}

void add_default_ramp_down_phase()
{
  int difference_of_ramp_up_down = 0, i;
  char grp_phase_name[PHASE_NAME_SIZE + 1];
  NSDL2_PARSING(NULL, NULL, "Method called");
  
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    difference_of_ramp_up_down = find_total_ramp_up_down_users(scenario_schedule);

    if (difference_of_ramp_up_down > 0) {
      //Add default ramp_down phase on phase_array structure according to difference
      strcpy(grp_phase_name, "Default_RampDown");
      default_ramp_down_phase(scenario_schedule, grp_phase_name, difference_of_ramp_up_down);
    }
  } 
  else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_runprof_entries; i++) {
      //Find the difference of ramp_down and ramp_up users per group
      difference_of_ramp_up_down = find_total_ramp_up_down_users(&group_schedule[i]);

      if(difference_of_ramp_up_down > 0) {
        snprintf(grp_phase_name, PHASE_NAME_SIZE + 1, "%sDefault_RampDown", RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
        //Add default ramp_down phase on phase_array structure according to difference on per group 
        default_ramp_down_phase(&group_schedule[i], grp_phase_name, difference_of_ramp_up_down);
      } 
    } 
  } 
}

// Note that unless DEBUG and Module mask are set, debug log will not generate any logs. TODO - We need to fix this issue
int parse_files(void)
{ 
  char site_file_name[MAX_FILE_NAME];
  char script_path[4096], script_version[64];
  //char error_string[1024] = "\0";
  int ret;
  int sess_idx;
  int script_type = -1;
  char *url_hash_file = "url.txt";
//  char event_log_file[1024 + 1] = {0};
  char cmd[128] = {0};
  char buf[1024 + 1] = {0};
  //char owner_name[128];
  //char group_name[128];
  int status = -1;
  int i=0;
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  shm_base = 0xca000000 + (100 * (testidx%100));

  //sys/site_keyword.default
  sprintf(site_file_name, "%s/%s", g_ns_wdir, DEFAULT_KEYWORD_FILE); 

  //Create  all dynamic sized tables
  write_log_file(NS_SCENARIO_PARSING, "Initializing all dynamic tables");
  if (init_userinfo() == FAILURE) {
    NSTL1_OUT(NULL, NULL, "parse_file(): Error in initializing userinfo");
    write_log_file(NS_SCENARIO_PARSING, "Failed to initialize user entries for storing information of the test");
    return -1;
  }

  /*  if (insert_default_errorcodes() == -1) {
    NSTL1_OUT(NULL, NULL, "parse_file(): Error in inserting default error codes\n");
    return -1;
    }*/

  //insert error code descriptions
  write_log_file(NS_SCENARIO_PARSING, "Filling error code description");
  if (input_error_codes() == -1) {
    NSTL1_OUT(NULL, NULL, "parse_file(): Error in inputing the error codes");
    write_log_file(NS_SCENARIO_PARSING, "Failed to create error code entries");
    return -1;
  }

  /* Use sort script  -- This code is already present while parsing and creation of partition
  if (sort_conf_file(g_conf_file, g_sorted_conf_file) == -1)
    NS_EXIT(-1, "Failed to merge and sort keywords in scenario %s with total keywords", g_conf_file);
  */

  
  init_default_values();

  //moving read_server_file() from here to after read_scripts_glob_vars(), becoz we are parsing HIERARCHICAL_VIEW in read_scripts_glob_vars()
  //and we need in read_server_file() - 15th Feb 2014

  //Set any global_settings NS variables (group & cluster variables)
  //Also initialize scenrio groups & test scripts (sessions)
  //reads the keywords from site_keywords.default : PASS - I
  //NO EXIT if file not found as it is an optional file
  //if (read_scripts_glob_vars(site_file_name) == -1)
  //NSDL2_SCHEDULE(vptr, cptr, "%s file not found or error in opening.", site_file_name);

  //read scen file 
  //if (read_scripts_glob_vars(g_conf_file) == -1) 


  create_default_global_think_profile();
  create_default_global_inline_delay_profile();
  create_default_reload_table();
  create_default_click_away_table();
  create_default_pacing();
  create_default_auto_fetch_table(); // to create default auto fetch table 
  create_default_cont_onpage_err(); // to create default auto fetch table 
  create_default_recorded_think_time();// to create default recorded think time table

#ifdef NS_DEBUG_ON
  // Reset to 0 as will be set by keyword
  group_default_settings->debug = 0;
  group_default_settings->module_mask = 0;
#endif

  write_log_file(NS_SCENARIO_PARSING, "Parsing global scenario");
  if (read_scripts_glob_vars(g_sorted_conf_file) == -1) {
    NSTL1_OUT(NULL, NULL, "parse_file(): Error in getting the script file names");
    write_log_file(NS_SCENARIO_PARSING, "Failure in getting the script file names");
    return -1;
  }

  set_scenario_type();
  write_test_init_header_file(NULL);

  //delete unused stage files
  rem_invalid_stage_files();
  //set nd log directories if nd is enable.
  if(global_settings->net_diagnostics_server[0] != '\0')
  {
    make_ndlogs_dir_and_link();
  }

  if(loader_opcode != CLIENT_LOADER)
    trace_log_init(); //to print header of debug_trace.log file

  /* StartupOptimization:
     Creation of DB tables and checking of controller-generator compatibility on MASTER/PARENT
     concurrently both can save time
  */
  if(loader_opcode != CLIENT_LOADER)
    create_db_tables_and_check_compatibility();

  create_pattern_shr_mem();
  // TO DO : CAVMAIN  
  if(!(run_mode_option & RUN_MODE_OPTION_COMPILE))
  {  /* Either read server.dat/Tier configuration files depending upon heirarchical keyword mode for monitors specification */
   //read_topology(global_settings->hierarchical_view_topology_name, global_settings->hierarchical_view_vector_separator, error_string, loader_opcode);
  
//   topolib_read_topology_and_init_method(getenv("ns_wdir"), global_settings->hierarchical_view_topology_name, global_settings->hierarchical_view_vector_separator, error_string, 0, testidx, nslb_get_owner(owner_name), nslb_get_group(group_name), g_partition_idx, "ns_logs/topo_debug.log", global_settings->ns_trace_level, max_trace_level_file_size, topo_idx);
  
  if(!strncmp(g_cavinfo.config, "NS", 2))
    sprintf(g_machine, "NSAppliance");
  else if(!strncmp(g_cavinfo.config, "NDE", 3))
  {
    if(!strncmp(g_cavinfo.SUB_CONFIG,"NVSM",4))
      sprintf(g_machine,"Controller");
    else
      sprintf(g_machine, "NDAppliance");
  }
  else if(!strncmp(g_cavinfo.config, "NV", 2))
    sprintf(g_machine, "NVAppliance");
  else if(!strncmp(g_cavinfo.config, "NO", 2))
    sprintf(g_machine, "NOAppliance");
  else if(!strncmp(g_cavinfo.config, "NCH", 3))
    sprintf(g_machine, "NCHAppliance");
  else if(!strncmp(g_cavinfo.config, "NC", 2))
  {
     if(loader_opcode == 1)
       sprintf(g_machine,"Controller");
     else
       sprintf(g_machine, "NSAppliance");
  }    
  else if(!strncmp(g_cavinfo.config, "SM", 2))
    sprintf(g_machine, "SMAppliance");

    init_hm_data_structures();
    
    hm_data->num_tiers = total_tier_table_entries;
    hm_data->num_instances = total_instance_table_entries;
    hm_data->num_servers = total_server_table_entries;
  }
 /*
  if(error_string[0] != '\0')
  {
    NS_EXIT(-1, "Topology is incorrect, please correct topology and re-run the test, error: %s", error_string);
  }
  */
  if((loader_opcode == MASTER_LOADER))
  {
    //In case of NetCloud, make generators entry in topology.
    generators_entry_in_topology(); 
  }

  //for RBU checking test case mode before creating vnc user profile
  if(global_settings->concurrent_session_mode) {
    if (testCase.mode != TC_FIX_CONCURRENT_USERS) {
      NS_EXIT(-1, "Fix Concurrent Sessions (FCS) is only supported in Fix Concurrent Users (FCU),"
                   " please set STYPE keyword as FIX_CONCURRENT_USERS and re-run the test");
    }
  }  
  /* Updating proxy_idx in RunProfTable:
   * Here since first two entry (i.e. System and ALL) in ProxyServerTable have been filled if exit
   * So check entry in ProxyServerTable for all group and if exit then update proxy index in RunProfTable 
   * for all groups
   * So pass group_idx = -1 and proxy_idx = 1*/
  //printf("\n**http=[%lu], https=[%lu]", proxySvrTable[1].http_proxy_server, proxySvrTable[1].https_proxy_server);
  //if(proxySvrTable[1].http_proxy_server != 0 || proxySvrTable[1].https_proxy_server != 0)
  NSDL2_PROXY(NULL, NULL, "ALL_GROUP_IDX=%d, ALL_PROXY_IDX=%d", ALL_GROUP_IDX, ALL_PROXY_IDX);
  if(update_proxy_index(ALL_GROUP_IDX, ALL_PROXY_IDX) == -1)
    return -1; 

  /*  Creating partition here as partition id must be created before opening event_log file.
   *  Also filling tr_or_partition and tr_or_common_files vars according to partition_creation_mode  */
  if(!(run_mode_option & RUN_MODE_OPTION_COMPILE))
  { 
    // TODO CAV_MAIN: Protected this part as we are not allocating test run shared memory 
     #ifndef CAV_MAIN
     update_test_run_info_shm();
    //create_partition_dir();
    /*In some keyword parsing we are doing event log and in partition mode tr_or_partition buffer will get set 2 times 
     * (1) initially TR<testidx>/ & (2) TR<testidx>/<partition> that's why there are changes event log opened in TR/ hence 
     * after setting tr_or_partition buffer to TR/partition close TR event_log fd and open in TR/partition event_log */
    if(elog_fd > 0)
    {
      close(elog_fd);
      elog_fd = -1;
    }
    #endif
  }
  g_start_partition_idx = g_partition_idx; //storing partition idx to global first_partition_idx for monitoring purpose

  if(global_settings->multidisk_nslogs_path)
  {
    create_links_for_logs(0);  //Calling for creating links in partition
    //Set default trace level here, because logs are dumped before parsing trace level keywrod also
    global_settings->ns_trace_level = 1;
    NSTL1(NULL, NULL, "Multidisk nslogs path = [%s]", global_settings->multidisk_nslogs_path);
  }
  
  make_partition_info_buf_to_append();

  // Moved here in 3.8.5
  // Initialize events at the earliest possible so that event can go to event log
  // So we do it here after following keywords are parsed in read_scripts_glob_vars method
  //    EVENT_LOG and EVENT_DEFINITION_FILE
  //    ENABLE_LOG_MGR is needed for init events but it is parsed along other keywords
  /* In case of "Do not log event", no need to make hash table of events ids and do not copy any event id file to logs dir*/
 //If compilation option is given then TR dir will not get created
 //In event logs we need TR dir, so if compilation is on then dont 
 //initialize event. 
 
  if(!(run_mode_option & RUN_MODE_OPTION_COMPILE)){
  // TODO CAV_MAIN: we are not creating event.log so protecting this part
   #ifndef CAV_MAIN
  /*  Opening event.log file here as partition id must be created before opening event_log file;
   *  and PARTITION_SETTINGS keyword must be parsed before creating partition.  */
    char event_log_file[1024 + 1] = {0};
    sprintf(event_log_file, "%s/logs/%s/event.log", g_ns_wdir, global_settings->tr_or_partition); 
    open_event_log_file(event_log_file, O_CREAT | O_WRONLY | O_LARGEFILE | O_CLOEXEC, 1);
    if(elog_fd > 0)
    {
      close(elog_fd);
      elog_fd = -1;
    } 
    if(global_settings->filter_mode != DO_NOT_LOG_EVENT) {
      init_events();
    }
   #else
   num_total_event_id = 0;
   #endif
  }
  if (total_runprof_entries == 0) {
    NS_EXIT(-1, "Scenario can not run without group, please add atleast one group to proceed");
  } else if(global_settings->replay_mode != 0 && total_runprof_entries != 1){
    NS_EXIT(-1, CAV_ERR_1031033);
  }

  //In case of controller num_generator_per_group is not filled, hence need to update static 
  //table as we need to set bitmask for schedule 
  if (loader_opcode == MASTER_LOADER) {
    int grp_id = 0, i;
    for (i = 0; i < total_runprof_entries; i++) {
      runProfTable[i].num_generator_per_grp = scen_grp_entry[grp_id].num_generator;
    }
    grp_id = grp_id + scen_grp_entry[grp_id].num_generator; 
  }

  load_http_methods(); // Load standard HTTP methods before parsing of scripts

  //Allocating memory for static Hash URL Table
  alloc_mem_for_urltable();

  //Open url file here
  if(url_hash_create_file(url_hash_file) == URL_ERROR)
    return URL_ERROR;

  NSDL2_RBU(NULL, NULL, "Create Profiles and start vnc instances - total_runprof_entries = %d, protocol_enabled = %d"
                        ", loader_opcode = %d, rbu_enable_auto_param = %d",
                         total_runprof_entries, (global_settings->protocol_enabled & RBU_API_USED)
                         , loader_opcode, global_settings->rbu_enable_auto_param);

  //Do not create RBU profiles and start VNC on Controller
  if((global_settings->protocol_enabled & RBU_API_USED) && 
     (loader_opcode != MASTER_LOADER) && (global_settings->rbu_enable_auto_param)) 
  {
    //if(ns_rbu_start_vnc_and_create_profiles() == -1)
    if(ns_rbu_on_test_start() == -1)
    {
      NSDL2_RBU(NULL, NULL, "Unable to create profile and start vnc");
      write_log_file(NS_SCENARIO_PARSING, "[RBU] Failed to start vnc and create profiles");
      end_test_run();
    }       
  }

  if((global_settings->protocol_enabled & RTE_PROTOCOL_ENABLED) && ( loader_opcode != MASTER_LOADER))
  {
    ns_rte_on_test_start(); //no need to check return status
  }

  NSDL2_RBU(NULL, NULL, "disable_script_validation = %d", global_settings->disable_script_validation);
  if ((loader_opcode != CLIENT_LOADER) && !global_settings->disable_script_validation)
  {
    pthread_attr_init(&script_val_attr);
    pthread_attr_setdetachstate(&script_val_attr, PTHREAD_CREATE_JOINABLE);
    if (pthread_create(&script_val_thid, &script_val_attr, ns_do_script_validation, NULL) != 0) {
      //Error
      NSTL1(NULL, NULL, "Error in creating thread for script validation");
    }
  }
   
  //SocketNormTable

  nslb_init_norm_id_table(&(g_socket_vars.proto_norm_id_tbl), 1000);
  init_global_timeout_values();  //Setting all global variables to -1

  for(sess_idx = 0; sess_idx < total_sess_entries; sess_idx++)
  {
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    sprintf(script_path, "%s/%s", GET_NS_RTA_DIR(),
             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
          //Previously taking with only script name
          //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)));

    NSDL2_PARSING(NULL, NULL, "script_path = %s", script_path);
    write_log_file(NS_SCENARIO_PARSING, "Parsing script %s (%d out of %d)",
             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"),
             sess_idx + 1, total_sess_entries);
    NSDL2_PARSING(NULL, NULL, "Parsing Script <%s>",
                               get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
    //Previously taking with only script name
    //NSDL2_PARSING(NULL, NULL, "Parsing Script <%s>\n", get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)));
    
    ret = get_script_type(script_path, &script_type, script_version, sess_idx);

    NSDL2_PROXY(NULL, NULL,"Get script type for session index '%d' function return value '%d', script_type = %d", sess_idx, ret, script_type);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    { 
      NSTL1_OUT(NULL, NULL, "Error in getting script type for %s. Exiting ...\n",
                             RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
      write_log_file(NS_SCENARIO_PARSING, "Failed to get script type for script %s", RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
      return ret;
    } 
    //handle JMeter script
    /*if(RETRIEVE_BUFFER_DATA(strstr(gSessionTable[sess_idx].sess_name), ".jmx") != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "Script '%s' is of type JMeter", gSessionTable[sess_idx].sess_name);
      gSessionTable[sess_idx].script_type = NS_SCRIPT_TYPE_JMETER; 
    }
    else*/
    gSessionTable[sess_idx].script_type = script_type;

    // We will call parse_script for both c type as well as java type script 
    ret = parse_script(sess_idx, script_path);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      return NS_PARSE_SCRIPT_ERROR;
    }
    // Make jar in case of java type script 
    if(script_type == NS_SCRIPT_TYPE_JAVA){
      make_scripts_jar();
    }
    fill_vuser_flow_data_struct(sess_idx, script_path);
    #ifndef CAV_MAIN
    int length = 0;
    if(!length)
       length = strlen(testInitStageTable[NS_SCENARIO_PARSING].stageDesc); //length of string at first
    snprintf(testInitStageTable[NS_SCENARIO_PARSING].stageDesc + length, 1024 - length, " (%d out of %d script validated)",
                                               sess_idx + 1, total_sess_entries);
    update_summary_desc(NS_SCENARIO_PARSING, testInitStageTable[NS_SCENARIO_PARSING].stageDesc);
    #endif
  }
  
  nslb_obj_hash_destroy(&(g_socket_vars.proto_norm_id_tbl));

  //Terminate to thread in case and print error from file if has 
  if ((loader_opcode != CLIENT_LOADER) && !global_settings->disable_script_validation)
  {
    int *script_val_ret;
    pthread_attr_destroy(&script_val_attr);
    if(pthread_join(script_val_thid, (void *)&script_val_ret) != 0)
    {
      //Error
      NSTL1(NULL, NULL, "Error in joining script validation thread");
    }
    if(*script_val_ret != 0)
    { 
      char script_val_log_file[1024];
      sprintf(script_val_log_file, "%s/logs/TR%d/ns_logs/script_validation.log", g_ns_wdir, testidx);
      FILE *efp = fopen(script_val_log_file, "r");
      FILE *scen_sum_fp = NULL;
      end_stage(NS_SCENARIO_PARSING, TIS_ERROR, "Failed to validate scripts, error: Invalid Syntax");
      if(efp)
      {
        while(!feof(efp))
        {
          fread(buf, 1, 1024, efp);
          append_summary_file(NS_SCENARIO_PARSING, buf, &scen_sum_fp);
          fprintf(stderr, "%s", buf);
        }
        fclose(efp);
        fclose(scen_sum_fp); //closing summary file of NS_SCENARIO_PARSING stage
      }
      NS_EXIT(-1, CAV_ERR_1031032);
    }
    write_log_file(NS_SCENARIO_PARSING, "Scripts are successfully compiled");
  }

  for(i = 0; i < total_runprof_entries; i++)
  {
    NSDL2_WS(NULL,NULL, "i = %d, mode = %d, num_of_flow_path = %d", i, runProfTable[i].gset.show_vuser_flow_mode, gSessionTable[runProfTable[i].sessprof_idx].num_of_flow_path);
    if(!runProfTable[i].gset.show_vuser_flow_mode) 
      continue;

    if(gSessionTable[runProfTable[i].sessprof_idx].num_of_flow_path <= 0) {
      NS_EXIT(-1, CAV_ERR_1031031, RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[i].sessprof_idx].sess_name), RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
    }
    total_flow_path_entries += gSessionTable[runProfTable[i].sessprof_idx].num_of_flow_path;
  }
  NSDL2_WS(NULL,NULL, "total_flow_path_entries = %d", total_flow_path_entries);

  //Atul for testing only
  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)
  {
    NSDL2_WS(NULL,NULL,"Calling Method ns_ws_data_structure_dump");
    //if (ns_ws_data_structure_dump() != 0)
    //  NSDL2_WS(NULL,NULL,"Returned from Method ns_ws_data_structure_dump with error");

    NSDL2_WS(NULL,NULL,"Returned from Method ns_ws_data_structure_dump sucessfully");
  }
  if(global_settings->sp_enable) 
    init_sp_group_table(0);
 
  free_post_buf(); //Free buf, if alloacted for post buf

  //url_hash_dynamic_table_init();
  if(url_hash_create_hash_table(url_hash_file)) return -1;

  //create perfect hash code for tx names
  tx_add_pages_as_trans ();                 // For PAGE_AS_TRANASCTION: Anuj

  //Add Tx name with suffix "_NetCache" in Tx tabel
  add_trans_name_with_netcache();

  if(total_tx_entries <= global_settings->threshold_for_using_gperf){
    if (get_tx_hash() == -1)              // here we are genrating the hash code
    {
      NSTL1_OUT(NULL, NULL, "Error: %s () Error in getting the tx names hash\n", (char*)__FUNCTION__);
      write_log_file(NS_SCENARIO_PARSING, "Failed to generate hash code for static transactions, function = %s ()", (char*)__FUNCTION__);
      return -1;
    }
  }
  else{
    for (i = 0; i < total_tx_entries; i++)
    {
      //char* tx_name = RETRIEVE_BUFFER_DATA(txTable[i].tx_name);
      txTable[i].tx_hash_idx = i;
      assert(txTable[i].tx_hash_idx != -1);
    }
    dynamic_tx_used = 1;
    g_avgtime_size = sizeof(avgtime);
    g_cavgtime_size = sizeof(cavgtime);
  }

  MY_MALLOC_AND_MEMSET(tx_hash_to_index_table, sizeof(int)*total_tx_entries, "tx_hash_index_table", -1);
  for (i = 0; i < total_tx_entries; i++)
  {
    tx_hash_to_index_table[txTable[i].tx_hash_idx] = i;   
  }
  //serachVarTable[] is a table of all serach variables
  //Session table keeps FirstEntry & num-elemsnt of searchVarTable
  //serachPageTable[] is a table of all serach pages associated with all serach variables
  //Session table keeps FirstEntry & num-elemsnt of searchPageTable
  //SearchPageTable keeps index of serachVarTable with each entry
  //Following function creates PerPageSerVarTable (list of indices into searchVarTable)
  //and links to each page.
  write_log_file(NS_SCENARIO_PARSING, "Creating search variable table");
  if (process_searchvar_table() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in processing the search var table\n");
    return -1;
  }

  write_log_file(NS_SCENARIO_PARSING, "Creating JSON variable table");
  //For JSON Var
  if (process_jsonvar_table() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in processing the json var table\n");
    return -1;
  } 
 
  write_log_file(NS_SCENARIO_PARSING, "Creating checkpoint variable table");
  if (process_checkpoint_table() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in processing the checkpoint table\n");
    return -1;
  }

  write_log_file(NS_SCENARIO_PARSING, "Creating checkreplysize table");
  if (process_check_replysize_table() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in processing the checkreplysize table\n");
    return -1;
  }

  write_log_file(NS_SCENARIO_PARSING, "Creating tag variable table");
  if (create_tagtables() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in creating the tag tables\n");
    return -1;
  }

  write_log_file(NS_SCENARIO_PARSING, "Creating cookie variable table");
  if (create_cookie_hash() == -1) 
  {
    NSTL1_OUT(NULL, NULL, "Error in creating cookie hash\n");
    return -1;
  }

  create_default_sessprof();

  //reads keyword from vender.default 
  read_default_file();
  //reads keyword from $NS_WDIR/sys/site_keyword.default : PASS - II
  //read_default_keyword_file(site_file_name);

  initialize_runprof_page_think_idx();
  initialize_runprof_inline_delay_idx();
  initialize_runprof_auto_fetch_idx(); // creates auto fetch embedded table.
  initialize_runprof_override_rec_think_time();
  initialize_runprof_cont_on_page_err();
  // creates page reload table  
  initialize_runprof_page_reload_idx();
  // creates page clickaway table
  initialize_runprof_page_clickaway_idx();
  //read_conf_file(g_conf_file);
  write_log_file(NS_SCENARIO_PARSING, "Validating all keywords");
  read_conf_file(g_sorted_conf_file);
  
  /*Check rampdown phase in scenario if not exist add default rampdown phase of mode immadiatly*/
  if(global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if(loader_opcode != CLIENT_LOADER) {
      add_default_ramp_down_phase();
    }
  }

  /* check_duplicate_pdf_files(): Bug: 57798- Since all PDF file related keywords(eg: URL_PDF, etc) 
     have been parsed till now so checking for any duplicate entry of PDF files */
  check_duplicate_pdf_files();

  //This function will add standard monitor on auto scale servers.
  //add_json_monitors_ex(0);
  
  if(global_settings->json_mode == 1)
    read_json_and_apply_monitor(PROCESS_PRIMARY_JSON, 0);
  else if(global_settings->json_mode == 2)
  {
    mj_read_json_dir_create_config_struct(global_settings->json_files_directory_path);
    start_monitors_on_all_servers();
  }
  //All montiors have been added in Main table from JSON and MPROF. so we are not adding MBEAN based monitor while parsing JSON, we are storing it in a structure. We will add these entries in mon table in this function.
  add_in_cm_table_and_create_msg(0);
  mbean_mon_rtc_applied = 0;

  //This code check dnsmasq is running or not, if not running or G_USE_DNS in enabled and STOP_TEST_IF_DNSMASQ_NOT_RUNNING is enabled
  //then we cannot start the test.
  if(global_settings->stop_test_if_dnsmasq_not_run && (loader_opcode != MASTER_LOADER))
  {
    write_log_file(NS_SCENARIO_PARSING, "Checking DNSMASQ is enabled or not");
    for (i = 0; i < total_runprof_entries; i++) 
    {
      NSDL1_SCHEDULE(NULL, NULL, "runProfTable[%d].gset->use_dns = %d", runProfTable[i].gset.use_dns);
      if (runProfTable[i].gset.use_dns) 
      {
        sprintf(cmd, "%s/bin/nsu_check_dns_masq_setup", g_ns_wdir);
        if(nslb_run_cmd_and_get_last_line(cmd, 1024, buf) != 0) {
          NS_EXIT(-1, CAV_ERR_1031027, cmd);
        }
        status = atoi(buf);
        NSDL1_SCHEDULE(NULL, NULL, "status = %d, buf = %s", status, buf);
        if(status != 0) {
          NS_EXIT(-1, CAV_ERR_1031026); 
        } 
        break; //DNSMSQ is running so break the loop no need to check further
      }
    }
  }
  /*Manish: ProxyServerData Dump for testing*/
  ns_proxy_table_dump();  

  if (global_settings->max_con_per_vuser > (g_max_num_embed +1)) {
    if (total_ftp_request_entries) {
      /* its 3 since in case of FTP atleset one prev conn might be reusable and
       * hence might not have returned into the free pool. So for FTP data conn
       * we need to have an extra so 1 for reuse, 1 for ftp command, and 1 for data */
      global_settings->max_con_per_vuser = g_max_num_embed + 2;
    } else if(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED) {
      // We need 1 cptr for listen and 1 for accepting connection
      global_settings->max_con_per_vuser = g_max_num_embed + 2;
    } else
      global_settings->max_con_per_vuser = g_max_num_embed + 1;
  }
  //Bug 10027: This function should be called after parsing of NUM_NVM keyword
  init_schedule_bitmask(); 


#if 0
  if (resolve_scen_group_pacing() == -1) {
    NSTL1_OUT(NULL, NULL, "parse_file(): error in resolving the scenario groups\n");
    return -1;
  }
#endif
  // load replay host for inline urls sothat we do not need to add host at runtime 
  if(global_settings->replay_mode == REPLAY_USING_ACCESS_LOGS)
    load_replay_host();

  insert_default_svr_location();

  ret = validate_and_process_user_profile();
  if(ret == -1)
    return ret;

  // Function for G_HTTP_HDR
  process_additional_header();
  
  return 0;

  //dump_sharedmem();
  //  exit(0);
 
}

void free_runprof_add_header_table()
{
  NSDL2_PARSING(NULL, NULL, "Method Called");
  int i;
  for(i=0; i<total_runprof_entries; i++)
  {
     FREE_AND_MAKE_NULL_EX(runProfTable[i].addHeaders, -1, "add_headers", -1);
  }
  NSDL2_PARSING(NULL, NULL, "Method Exit");  
}

static int addHeaderTable_cmp(const void* ent1, const void* ent2)
{
  NSDL2_PARSING(NULL, NULL, "Method called");
  if (((AddHeaderTableEntry *)ent1)->groupid < ((AddHeaderTableEntry *)ent2)->groupid)
    return -1;
  else if (((AddHeaderTableEntry *)ent1)->groupid > ((AddHeaderTableEntry *)ent2)->groupid)
    return 1;
  else if (((AddHeaderTableEntry *)ent1)->pageid < ((AddHeaderTableEntry *)ent2)->pageid)
    return -1;
  else if (((AddHeaderTableEntry *)ent1)->pageid > ((AddHeaderTableEntry *)ent2)->pageid)
    return 1;
}
#define LOCAL_BUF_LENGTH 2048
void fill_runprof_additional_header(int gp_idx, int rel_pg_idx, int org_pg_idx)
{
   // We are considering maximum header name and value as 64K for each main, inline and ALL 
   static char main_hdr_buf[MAX_HEADER_LENGTH]; 
   static char inline_hdr_buf[MAX_HEADER_LENGTH];
   static char all_hdr_buf[MAX_HEADER_LENGTH];
   char lbuf[LOCAL_BUF_LENGTH+1]; // Local buf to append header with above buffers
   int i, j, headermatch;
 
   // Initializing buffers
   main_hdr_buf[0]=0;
   inline_hdr_buf[0]=0;
   all_hdr_buf[0]=0;
   int pg_start = -1;
   int pg_end = 0;

   NSDL2_SCHEDULE(NULL, NULL, "Method called. gp_idx=%d, relative pg_idx=%d, original pg_idx=%d", gp_idx, rel_pg_idx, org_pg_idx);

   for(i=all_group_all_page_header_entries; i<total_hdr_entries; i++)
   {
     if((gp_idx == addHeaderTable[i].groupid) && (org_pg_idx == addHeaderTable[i].pageid))
     {
       snprintf(lbuf, LOCAL_BUF_LENGTH, "%s: %s\r\n",addHeaderTable[i].headername,addHeaderTable[i].headervalue );
           
       if(addHeaderTable[i].mode == ADD_HDR_MAIN_URL)
         strcat(main_hdr_buf, lbuf);
       else if(addHeaderTable[i].mode == ADD_HDR_INLINE_URL)
         strcat(inline_hdr_buf, lbuf);
       else if(addHeaderTable[i].mode == ADD_HDR_ALL_URL)
         strcat(all_hdr_buf, lbuf);
   
       if(pg_start == -1)
          pg_start = i;
      }
      else
      {  
        if(pg_start != -1)
        {
          pg_end = i;
          break;
        }
      }
    }

   for(j=0; j<all_group_all_page_header_entries; j++)
   {
     headermatch = 0;
     for(i=pg_start; i<pg_end; i++)
     {
       if(!strcmp(addHeaderTable[i].headername, addHeaderTable[j].headername))
       {
         headermatch = 1;
         break;
       }
     }
     if(!headermatch)
     {
       snprintf(lbuf, LOCAL_BUF_LENGTH, "%s: %s\r\n",addHeaderTable[j].headername,addHeaderTable[j].headervalue );
       
       if(addHeaderTable[j].mode == ADD_HDR_MAIN_URL)
         strcat(main_hdr_buf, lbuf);
       else if(addHeaderTable[j].mode == ADD_HDR_INLINE_URL)
         strcat(inline_hdr_buf, lbuf);
       else if(addHeaderTable[j].mode == ADD_HDR_ALL_URL)
         strcat(all_hdr_buf, lbuf);
     }       
   }
   // To Do call segment line
   int sess_idx = runProfTable[gp_idx].sessprof_idx;

   NSDL1_SCHEDULE(NULL, NULL, "MainHdrBuf %s, InlineHdrBuf %s, AllHdrBuf %s", main_hdr_buf, inline_hdr_buf, all_hdr_buf);
   
   segment_line(&runProfTable[gp_idx].addHeaders[rel_pg_idx].MainUrlHeaderBuf,   main_hdr_buf,   0, 0, sess_idx, NULL);
   segment_line(&runProfTable[gp_idx].addHeaders[rel_pg_idx].InlineUrlHeaderBuf, inline_hdr_buf, 0, 0, sess_idx, NULL);
   segment_line(&runProfTable[gp_idx].addHeaders[rel_pg_idx].AllUrlHeaderBuf, all_hdr_buf,    0, 0, sess_idx, NULL);

}

/* We have filled the addHeaders table with below logic:
   FOR GROUP ALL && PAGE ALL 
      We have maintained a value in the addHeader Table as -1 -1
   FOR GROUP ALL && PAGE NOT ALL(This case is already taken care in parsing - NOT ALLOWED)
   FOR GROUP SPECIFIC PAGE SPECIFIC - Group Id and Page ID respective Page Id is mentioned in table
   FOR GROUP SPECIFIC PAGE ALL - Group ID and All Page IDs are stored in the table. 

   So We have table in the below format.
  
   0   1 hname hvalue
   1   0 hname hvalue
  -1  -1 hname hvalue
   0   2 hname hvalue
  -1  -1 hname hvalue

   Later on in this function we short this table based on group id and page id which makes this table as
 
   -1 -1 hname hvalue
   -1 -1 hname hvalue
    0  1 hname hvalue
    0  2 hname hvalue
    1  0 hname hvalue

    In Above function fill_runprof_additional_header we are first filling hname and hvalue for specific group and
    specific page and then concatenating it with GROUP ALL and PAGE ALL.

    Total entries in table with -1(ALL) -1(ALL) is taken with global variable all_group_all_page_header_entries 
*/

void process_additional_header()
{
   int i,j, num_pages;

   NSDL2_SCHEDULE(NULL, NULL, "Method Called, total_runprof_entries %d, total_hdr_entries %d, all_group_all_page_header_entries %d", 
                               total_runprof_entries, total_hdr_entries, all_group_all_page_header_entries);

   for(i=0; i<total_runprof_entries; i++) 
   {
     num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;
     MY_MALLOC_AND_MEMSET(runProfTable[i].addHeaders, num_pages * sizeof(AddHTTPHeaderList), "add_headers", -1);
   }
   if(!total_hdr_entries)
     return;
  
  // We are sorting table here to make it easier in getting header name and value for a partcular group and page 
  qsort(addHeaderTable, total_hdr_entries, sizeof(AddHeaderTableEntry), addHeaderTable_cmp);

  // CASE When GROUP is ALL And PAge is ALL along with specific group and specific page    
   for(i=0; i< total_runprof_entries; i++) 
   {  
     int first_page = gSessionTable[runProfTable[i].sessprof_idx].first_page;
     num_pages = gSessionTable[runProfTable[i].sessprof_idx].num_pages;
     for(j = 0; j < num_pages; j++) 
     {
       fill_runprof_additional_header(i, j, j+first_page);
     }
   }
   NSDL2_SCHEDULE(NULL, NULL, "Method Exit");
}

void call_user_test_init_for_each_sess(void)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  int sess_id;
  //printf("call_user_test_init_for_each_sess(): total_sess_entries = %d\n", total_sess_entries);
  for (sess_id = 0; sess_id < total_sess_entries; sess_id++)
  {
    if (session_table_shr_mem[sess_id].user_test_init)
    {
      //printf("call_user_test_init_for_each_sess(): sess_id = %d\n", sess_id);
      session_table_shr_mem[sess_id].user_test_init();
    }
  }
}

void call_user_test_exit_for_each_sess(void)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  int sess_id;
  //printf("call_user_test_exit_for_each_sess(): total_sess_entries = %d\n", total_sess_entries);
  for (sess_id = 0; sess_id < total_sess_entries; sess_id++)
  {
    if (session_table_shr_mem[sess_id].user_test_exit)
    {
      //printf("call_user_test_exit_for_each_sess(): sess_id = %d\n", sess_id);
      session_table_shr_mem[sess_id].user_test_exit();
    }
  }
}
