/**
 * FILE: ns_auto_fetch_embed.c
 *
 * Purpose: The file contains all the code related to auto fetching of
 *          embedded URLs from Main URL.
 *
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "ns_parse_src_ip.h"
#include "nslb_sock.h"
#include "comp_decomp/nslb_comp_decomp.h"
#include "ns_trans_parse.h"
#include "ns_custom_monitor.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_goal_based_sla.h"
#include "ns_tag_vars.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "amf.h"
#include "ns_string.h"
#include "ns_auto_fetch_embd.h"
#include "ns_url_resp.h"
#include "decomp.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_event_log.h"
#include "ns_embd_objects.h"
#include "ns_http_script_parse.h"
#include "ns_url_hash.h"
#include "ns_event_id.h"
#include "ns_dynamic_hosts.h"
#include "ns_trace_level.h"
#include "ns_websocket.h"

#define MAX_HOST_NAME 256

/**
 * This function makes absolute url from relative url using parent url. 
 * in_url must be relative url
 * parent should be aboslute (starting with /)
 * eg - in_url = abc.html, parent_url_line = /logs/xyz.html returns out_url = /logs/abc.html
 *
 * Note: we can have in_url = out_url in calling function.
 */

#ifndef CAV_MAIN
PatternTable_Shr *pattern_table_shr;
#else
__thread PatternTable_Shr *pattern_table_shr;
#endif
void make_absolute_from_relative_url(char *in_url, char *parent_url_line, char *out_url) 
{
char *last_ptr = NULL;
char *first_ptr = in_url;
int i;
  
  NSDL1_HTTP(NULL, NULL, "Method Called. in_url = %s, parent_url_line = %s", 
                in_url, parent_url_line);
 
  if (parent_url_line[0] != '/')
  {
    NSDL1_HTTP(NULL, NULL, "parent_url_line does not start with /, parent_url_line = %s, in_url = %s", 
                parent_url_line, in_url);
    return;
  }
 
  /* Search for ? or # because we can have '/' character even after actual last '/'
  eg - /logs/test.html#fda@d/dfs */
  last_ptr = strpbrk(parent_url_line, "?#");
    
  if (!last_ptr)
    last_ptr = parent_url_line + strlen(parent_url_line); // AN-TODO need OPTIMIZATION
    //last_ptr = strrchr(cur_ptr, '/');

  for (i = last_ptr - parent_url_line - 1 ; i >= 0; i--) {
    if (parent_url_line[i] == '/')  {
      last_ptr = parent_url_line + i;
      break;
    }
  }

  if (!strncmp(first_ptr, "./", 2))
    first_ptr += 2; // Move relative path pointer forward when it starts with self directory

  while (!strncmp(first_ptr, "../", 3))
  {
    /* Move relative path pointer forward */
    first_ptr += 3;

    /* Move parent last pointer backwards */
    for (i = last_ptr - parent_url_line - 1 ; i >= 0; i--)
    {
      if (parent_url_line[i] == '/')
      {
        last_ptr = parent_url_line + i;
        break;
      }
    }

    /* moved beyond root level of parent URL */
    if (i < 0)
    {
      NSDL1_HTTP(NULL, NULL, "Method Returned NULL out_url. parent_url_line = %s, in_url = %s", 
                parent_url_line, in_url);
      out_url[0] = '\0';
      return;
    }
  }
  
  {
  int parent_path_len = last_ptr - parent_url_line + 1;
  int in_file_url_len = strlen(first_ptr);
  
    // First copy in url after leaving speace for parent path
    // This is done in case in_url and out_url are same buffer
    memmove(out_url + parent_path_len, first_ptr, in_file_url_len);
    strncpy(out_url, parent_url_line, parent_path_len);
    out_url[parent_path_len + in_file_url_len] = '\0';
  }
  NSDL1_HTTP(NULL, NULL, "Method Returned. out_url = %s", out_url);
}

/**
 * This function extracts hostname, request, port and request_type from the eurl.
 * If not found hostname is set to NULL, port to -1 and request_type to -1
 * It is assumed request will always be there.
 */
int extract_hostname_and_request(char *passed_eurl, char *hostname, char *request,
                                         int *port, int *request_type, char *parent_url_line, int parent_url_request_type)
{
  /*char *url_extract;*/
  char *x;
  *request_type = -1;
  *port = -1; // set with -1
  char *eurl = passed_eurl; //need to preserve original emb_url pointer

  NSDL1_HTTP(NULL, NULL, "Method called. eurl = %s, parent_url_line = %p", eurl, parent_url_line);
  /* In Kohls enviornment, embedded url was received with white space
   * e.g <img src=" http://media.kohls.com.edgesuite.net/is/image/kohls/ME-MarketingEquity1-20130605-C6?wid=159&hei=143&qlt=100 " />
   * Here src tag URL was received with an extra space, which failed in parse url
   * Hence clearing white space from start and end of URLs
   * */
  CLEAR_WHITE_SPACE(eurl);
  CLEAR_WHITE_SPACE_FROM_END(eurl);
  NSDL1_HTTP(NULL, NULL, "After clearing white space eurl = %s", eurl);

  if(parent_url_request_type == HTTP_REQUEST || parent_url_request_type == HTTPS_REQUEST)
  {
    if (RET_PARSE_NOK == parse_url(eurl, "/?#", request_type, hostname, request))
    {
      NSTL1(NULL, NULL, "Error: Malformed embedded url [%s].", eurl);
      return -1; 
    }
  }
  else if(parent_url_request_type == WS_REQUEST || parent_url_request_type == WSS_REQUEST)
  {
    if (RET_PARSE_NOK == parse_uri(eurl, "/?#", request_type, hostname, request))
    {
      NSTL1(NULL, NULL, "Error: Malformed embedded url [%s].", eurl);
      return -1; 
    }
  }
  else if(parent_url_request_type == SOCKET_REQUEST || parent_url_request_type == SSL_SOCKET_REQUEST)
  {
    parse_socket_host_and_port(eurl, request_type, hostname, port);
  }

  NSDL1_HTTP(NULL, NULL, "parse_url returned Request Type = %d, Hostname = %s, Request = %s", *request_type, hostname, request);

  if ((REQUEST_TYPE_NOT_FOUND == *request_type) && strncmp(eurl, "//", 2) != 0) // Not fully qualified URL
  {
    if (eurl[0] == '/') /* Absolute URL */
    {
      // *port = -1; /* rest of the parameters are already filled */
    }
    else /* Relative URL */
    {
      if(parent_url_line) {
        make_absolute_from_relative_url(request, parent_url_line, request);
      }
      // *port = -1; /* rest of the parameters are already filled */
    }
  }
  else
  {
    /* Fully qualified URL */
    if ((x = index(hostname, ':'))) { /* extract port if any */
      *port = atoi(x+1);
      /* Fix done for bug 5457:
       * If Request type missing then update type with main URL's request type*/
      if (*request_type == REQUEST_TYPE_NOT_FOUND)
        *request_type = parent_url_request_type;
      /* In case of hostname is given with default port then we need to remove port from host string*/
      if ((*request_type == HTTP_REQUEST && *port == 80) || (*request_type == HTTPS_REQUEST && *port == 443)) {
        *x = '\0';      
        NSDL2_HTTP(NULL, NULL, "If hostname is given with default port with respect to url request type then we need to remove port from host string.  hostname = %s, request-type = %d, port = %d", hostname, *request_type, *port);
      } 
    }
    if (strncmp(eurl, "//", 2) == 0)
    {

    }
  }
  /* If Request type missing then update type with main URL's request type*/
  if (*request_type == REQUEST_TYPE_NOT_FOUND)
    *request_type = parent_url_request_type;
  return 0;
}

/**
 * The function generates a spoof list which can be directly inserted in vptr for
 * normal operation.
 */
HostTableEntry_Shr *get_hel_from_eurl_list(EmbdUrlsProp *eurls, int *num_eurls, connection *cptr, VUser *vptr)
{
  HostTableEntry_Shr *hel = NULL, *hel_prev = NULL, *hel_head = NULL;
  int i;
  char hostname[MAX_HOST_NAME];
  char request[MAX_LINE_LENGTH];
  char *forged_request = NULL;
  action_request_Shr* req_url;
  int request_type;
  int port;
  int hel_found;
  int gserver_shr_idx;
  int malformed_eurl = 0;
  action_request_Shr *main_url_num = cptr->url_num;
  /****** Dynamic Host: BEGIN *******/
  int dyn_host_err_count = 0; // Count for unresolved url.
  //int url_type; //added for debugging purpose 
  //url_type = cptr->url_num->proto.http.type;
  /****** Dynamic Host: END *******/
  int iUrlLen = 0;//added for relative path in embedded url

  NSDL1_HTTP(vptr, NULL, "Method called. num_eurls = %d", *num_eurls);
  for (i = 0; i < *num_eurls; i++) {
    hel_found = 0;
    if(eurls[i].embd_url == NULL){
      malformed_eurl++;
      continue;
    }
    if(extract_hostname_and_request(eurls[i].embd_url, hostname,
                                    request, &port, 
                                    &request_type, get_url_req_url(cptr), main_url_num->request_type) < 0) {
      //Hash 
      malformed_eurl++;
      NSDL2_HTTP(vptr, NULL, "Got malformed eurl, hence continueing,"
                             " total malformed_eurl = %d, num_eurls = %d",
                              malformed_eurl, *num_eurls);
      continue;
    }
                                                 
    NSDL3_HTTP(vptr, NULL, "eulrs[%d] = %s. Extracted: hostname = %s,"
			   " port = %d, request = %s, request_type = %d",
			   i, eurls[i].embd_url, hostname, port, request, request_type);

    /* Case 1) Only URI part was fetched, hence updating host and port of main URL*/
    if (hostname[0] == '\0') { /* Set same as previously hit url */
      // hostname = strdup(main_url_num->proto.http.index.svr_ptr->server_hostname);
      strncpy(hostname, main_url_num->index.svr_ptr->server_hostname, MAX_HOST_NAME - 1);
      port = main_url_num->index.svr_ptr->server_port;
      NSDL3_HTTP(vptr, NULL, "Filling in hostname = %s, port = %d", hostname, port);
    }
    /* Case 2) Host name was given but port was missing, hence updating port with respect to request type of main URL*/
    if (port == -1) {
      if (request_type == HTTPS_REQUEST)
        port = 443;
      else
        port = 80;
    }
    unsigned short rec_server_port; //Sending Dummy
    int hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);
    gserver_shr_idx = find_gserver_shr_idx(hostname, port, hostname_len);

    if (gserver_shr_idx != -1) {

      /* Find existing hel for host */
      for (hel = hel_head; hel != NULL; hel = hel->next) {
        if (hel->svr_ptr->idx == gserver_shr_idx) {
          hel_found = 1;
          break;
        }
      }
    }
    /****** Dynamic Host: BEGIN *******/
    else 
    {
      gserver_shr_idx = add_dynamic_hosts (vptr, hostname, port, 2, 0, request_type, eurls[i].embd_url, vptr->sess_ptr->sess_name, vptr->cur_page->page_name, vptr->user_index, runprof_table_shr_mem[vptr->group_num].scen_group_name);
      /* Dynamic host cannot be added becoz of following reasons:
       * Case 1: User has exceeded host limit(mention in keyword)
       *         Before exiting the funct, num_eurls must be updated wrt 
       *         to malformed_eurl and by total number of embedded URLs found at each host.
       * Case 2: Keyword disable
       * Case 3: DNS look up failure we will ignore the url
       * */
      if (gserver_shr_idx < 0)
      {
          dyn_host_err_count ++;
          continue; //ignore url.
      }
    }
    /****** Dynamic Host: END *******/
 
    if (!hel_found) {
      MY_MALLOC (hel, sizeof(HostTableEntry_Shr), "Host Table", i);
      hel->num_url = 0;
      memset(hel, 0, sizeof(HostTableEntry_Shr));
              
      if (hel_prev) {
        hel_prev->next = hel;
        hel_prev = hel;
      }

      hel->next = NULL;
              
      if (!hel_head) {
        hel_head = hel;
        hel_prev = hel;
      }
    }
           
    NSDL3_HTTP(vptr, NULL, "gserver_table_shr_mem  = %p, gserver_shr_idx = %d", gserver_table_shr_mem, gserver_shr_idx);
    //hel->svr_ptr = malloc(sizeof(SvrTableEntry_Shr));
    hel->svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];
    hel->num_url++;
      
    MY_REALLOC_EX(hel->first_url, sizeof(action_request_Shr) * hel->num_url, (hel->num_url - 1) * sizeof(action_request_Shr), "hel->first_url", i);
    memset(&hel->first_url[hel->num_url - 1], 0, sizeof(action_request_Shr));
    req_url = &hel->first_url[hel->num_url - 1];

    iUrlLen = strlen(request);

    /*Find relative url path "/.." in the embedded URL*/
    char *found_relative = NULL; 
    found_relative = strstr(request, "/../");

    MY_MALLOC (forged_request, (iUrlLen + 1), "Forge request", i);

    if(found_relative != NULL)
    { //Found relative path
      NSDL2_CACHE(NULL, cptr, "Relative url found. Going to make absolute url");
      //Malloc forged URL request 
      //Update forged_request with absolute path
      make_part_of_relative_url_to_absolute(request, iUrlLen, forged_request);
      iUrlLen = strlen(forged_request); //calulate request's string length 
    }
    else
    {
      /* Forge request */
      strcpy(forged_request, request);
      NSDL3_HTTP(vptr, NULL, "new request = %s", forged_request);
    }

    if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
    {
      NSDL3_HTTP(NULL, cptr, "Redirected URL = %s", request);
      req_url->proto.http.url_index = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)request, iUrlLen, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
      NSDL3_HTTP(vptr, NULL, "req_url->url_id = %d", req_url->proto.http.url_index);
    }
 
    //memcpy(read_url, main_url_num, sizeof(http_request_Shr));
    /* Fill req_url */
    MY_MALLOC (req_url->proto.http.url.seg_start, sizeof(SegTableEntry_Shr), "Fill req_url", i);
    MY_MALLOC (req_url->proto.http.url.seg_start->seg_ptr.str_ptr, sizeof(PointerTableEntry_Shr), "Fill req_url", i);
      
    req_url->proto.http.url.seg_start->seg_ptr.str_ptr->big_buf_pointer = forged_request;
    req_url->proto.http.url.seg_start->seg_ptr.str_ptr->size = iUrlLen;
    req_url->proto.http.url.seg_start->type = STR;
    req_url->proto.http.url.num_entries = 1;
    memcpy(&req_url->proto.http.hdrs, &main_url_num->proto.http.hdrs, sizeof(StrEnt_Shr)); /* Copy same as MAIN url ?? */
    req_url->proto.http.type = EMBEDDED_URL;
        
    req_url->index.svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];//malloc(sizeof(SvrTableEntry_Shr));
            
    req_url->proto.http.http_method = HTTP_METHOD_GET; /* GET */
    req_url->proto.http.http_method_idx = HTTP_METHOD_GET; /* Index to get method name (GET ) */
    req_url->proto.http.http_version = 1; /* HTTP/1.1 */
    req_url->proto.http.tx_hash_idx = -1; 
    req_url->request_type = request_type; /* HTTP/HTTPs. If nothing mentioned use same os main url or
                                             * use the one mentioned. */
    req_url->server_base = NULL;

    /*Type of embedded url script/else */
    if(eurls[i].embd_type == XML_TEXT_JAVASCRIPT)
      req_url->proto.http.header_flags |=  NS_URL_KEEP_IN_CACHE;
    else
      req_url->proto.http.header_flags = 0; 

    req_url->proto.http.url_got_bytes = eurls[i].duration; //we are adding url duration here to set it in cptr when making request
    if (main_url_num->parent_url_num)
      req_url->parent_url_num = main_url_num->parent_url_num;
    else
      req_url->parent_url_num = main_url_num;
  }
  /* Dynamic Host--BEGIN */
  *num_eurls-= dyn_host_err_count; // ignoring error case of dynamic hosts
  /* Dynamic Host--END */  
  *num_eurls -= malformed_eurl;
  return hel_head;
}

void set_embd_objects(connection *cptr, int num_eurls, EmbdUrlsProp *eurls)
{
  VUser *vptr = cptr->vptr;
  int num_hel = 0;
  HostTableEntry_Shr *hel, *tmp_hel, *old = NULL;
  //char err[0xfff];
  //char *inp_buffer_body = url_resp_buff + vptr->body_offset;
  //int body_size = vptr->bytes - vptr->body_offset;
  http_request_Shr_free *hfree = NULL;    
  int i = 0;

  NSDL2_HTTP(vptr, cptr, "Method called num_eurls = %d", num_eurls);

  /* Free all embedded one's*/

  if ((vptr->is_embd_autofetch) && (cptr->url_num->proto.http.type == MAIN_URL)) {
        NSDL1_HTTP(NULL, cptr, "is_embd_autofetch is ON, freeing all_embedded_urls");
        free_all_embedded_urls(vptr);
  }

  /* Create http_request structure and qsort it. */
  hel = get_hel_from_eurl_list(eurls, &num_eurls, cptr, vptr);
  /* Once we are here, we refill all hptr (of vptr) i.e. we are overwriting 
   * all other embedded URLS given in the script. */

  NSDL2_HTTP(vptr, cptr, "Setting num_eurls = %d to urls_left & urls_awaited.", num_eurls);

  vptr->urls_awaited = vptr->urls_left = num_eurls;
  vptr->head_hlist = vptr->tail_hlist = NULL;
  vptr->is_embd_autofetch = 1;
  /* compression_type should not be set to zero here since we will need to 
   * decompress it again for processing of vars in do_data_processing. 
   * we need to find an efficient way of doing since we will be uncompressing 
   * the buffers twice.  */
  //vptr->compression_type = 0;

  tmp_hel = hel;
  while (tmp_hel) { /* After this, the code must execute normally. */
    add_to_hlist(vptr, tmp_hel);
    num_hel++;
    tmp_hel = tmp_hel->next;
  }

  num_hel += vptr->num_http_req_free;
  hfree = vptr->http_req_free;
  NSDL2_HTTP(NULL, NULL,"vptr->num_http_req_free = %d, num_hel = %d", vptr->num_http_req_free, num_hel);
  MY_REALLOC(hfree, sizeof(http_request_Shr_free) * num_hel, "hfree", -1 )
  for (i = vptr->num_http_req_free, tmp_hel = hel; tmp_hel != NULL; i++) {
    old = tmp_hel;
    hfree[i].http_req_ptr = old->first_url;
    hfree[i].num_url = old->num_url;
    tmp_hel = tmp_hel->next;
    FREE_AND_MAKE_NULL_EX(old, sizeof(HostTableEntry_Shr),"old", i); //added struct size.
    NSDL2_HTTP(NULL, NULL,"vptr->http_req_free[%d].num_url = %d, vptr->http_req_free = %p", i, hfree[i].num_url, hfree);
  }
  vptr->http_req_free = hfree;
  vptr->num_http_req_free = num_hel;
}

static int check_inlcude_url(int pattern_table_index, char *url){
  PatternTable_Shr* pattern_table_loc;
  int start_idx, num_entries, i, status;
  regmatch_t pmatch;

  pattern_table_loc = pattern_table_shr + pattern_table_index + INCLUDE_URL; 
  start_idx = pattern_table_loc->reg_start_idx;
  num_entries = pattern_table_loc->num_entries;
  NSDL2_HTTP(NULL, NULL, "Include Url start_idx = %d, num_entries = %d", start_idx, num_entries);

  // Excute include url 
  if(num_entries){ 
    for(i = start_idx; i < (start_idx + num_entries); i++){
      NSDL2_HTTP(NULL, NULL, "Executing Include at idx = %d", i);
      status = regexec(&reg_array_ptr[i].comp_regex, url, 1, &pmatch, 0);
      if(!status){
        if(pmatch.rm_so == 0 && pmatch.rm_eo == strlen(url)){
          NSDL2_HTTP(NULL, NULL, "Include url, match found. url = %s", url);
          return KEEP_URL;
        }
      }
    } 
  } else {
    NSDL2_HTTP(NULL, NULL, "Returning from else of url include, keeping by dfault. url = %s", url);
    return KEEP_URL;
  }

  return REMOVE_URL;
}

int get_domain(connection *cptr, char *url, char *host_name){
  char *ptr;
  int len;
  VUser *vptr = cptr->vptr;

  if(!strncmp(url, "http://", 7)) {
    url += 7; 
  } else if(!strncmp(url, "https://", 8)){
    url += 8; 
  } else if(!strncmp(url, "//", 2)){ // Url may be relative to scheme, in this case host is given
    url += 2;
  } else { 
    // Url may not be fully qualified, take current page host as its domain
    /* This mode is added to do/dont consider relative url host as its main url host in autofetch filters
     * mode 0(do not consider) is not recommended mode, it is supported for khols special case only. Case is following:
     * In khols 4.1.5(2016) release we were not considering relative urls in autofetch filters, due to this many relative 
     * inline url(s) were not hitting.
     * In khols 4.1.6(2017) release this bug was fixed, but the fix created a huge response time increase as number of 
     * inline url requests increased.
     * So this mode is added to reproduce same baseline results. for any other scenario default mode 1(consider) is recomended
    */
    if (runprof_table_shr_mem[vptr->group_num].gset.include_exlclude_relative_url) {
      PerHostSvrTableEntry_Shr* svr_entry = get_svr_entry(cptr->vptr, cptr->url_num->index.svr_ptr);
      strncpy(host_name, svr_entry->server_name, svr_entry->server_name_len);
      host_name[svr_entry->server_name_len] = '\0';
      NSDL2_HTTP(NULL, cptr, "Url is not fully qualified, hence consider its main host domain as its domain. host_name = %s", host_name);
      return 0;
    } else {
      return -1; 
    }
  }

  ptr = strpbrk(url, "{/?#");

  if(!ptr){
    strcpy(host_name, url);
  } else {
    len = ptr - url;
    strncpy(host_name, url, len);
    host_name[len] = '\0';
  }

  return 0;
}


int process_pattern(connection *cptr, char *url, int pattern_table_index){

  int i;
  int status;
  PatternTable_Shr* pattern_table_loc;
  char host_name[1024];
  int num_entries;
  regmatch_t pmatch;
  int start_idx;

  if(pattern_table_index == -1){ // Should not come here its just a seftry check
    NSDL2_HTTP(NULL, cptr, "No pattern is defined for this group. returning form process_pattern");
    return KEEP_URL;
  }


  NSDL2_HTTP(NULL, cptr, "Method called, url = %s, pattern_table_index = %d", url, pattern_table_index);

  // Excute exclude domain first  
  pattern_table_loc = pattern_table_shr + pattern_table_index + EXCLUDE_DOMAIN; 
  start_idx = pattern_table_loc->reg_start_idx;
  num_entries = pattern_table_loc->num_entries;

   NSDL2_HTTP(NULL, cptr, "Exclude domain start_idx = %d, num_entries = %d", start_idx, num_entries);

  if (get_domain(cptr, url, host_name) != -1){
    for(i = start_idx; i < (start_idx + num_entries); i++){
      status = regexec(&reg_array_ptr[i].comp_regex, host_name, 1, &pmatch, 0);
      if(!status){
        if(pmatch.rm_so == 0 && pmatch.rm_eo == strlen(host_name)){
          NSDL2_HTTP(NULL, cptr, "Exclude Patten, Domain match found. url = %s", url);
          return REMOVE_URL;
        } else {
           NSDL2_HTTP(NULL, cptr, "Exclude Pattern, Domain match found. bt not complete match url = %s", url);
        }    
      }
    }
  }

  // Excute exclude url  
  pattern_table_loc = pattern_table_shr + pattern_table_index + EXCLUDE_URL; 
  start_idx = pattern_table_loc->reg_start_idx;
  num_entries = pattern_table_loc->num_entries;
  NSDL2_HTTP(NULL, cptr, "Exclude url start_idx = %d, num_entries = %d", start_idx, num_entries);

  for(i = start_idx; i < (start_idx + num_entries); i++){
    status = regexec(&reg_array_ptr[i].comp_regex, url, 1, &pmatch, 0);
    if(!status){
      if(pmatch.rm_so == 0 && pmatch.rm_eo == strlen(url)){
        NSDL2_HTTP(NULL, cptr, "Exclude Pattern, Url match found. url = %s", url);
        return REMOVE_URL;
      }
    }
  }

  // Execute include domain   
  pattern_table_loc = pattern_table_shr + pattern_table_index + INCLUDE_DOMAIN; 
  start_idx = pattern_table_loc->reg_start_idx;
  num_entries = pattern_table_loc->num_entries;

  if(num_entries){
    if (get_domain(cptr, url, host_name) != -1){
      for(i = start_idx; i < (start_idx + num_entries); i++){
        status = regexec(&reg_array_ptr[i].comp_regex, host_name, 1, &pmatch, 0);
        if(!status){
          if(pmatch.rm_so == 0 && pmatch.rm_eo == strlen(host_name)){
            NSDL2_HTTP(NULL, cptr, "Include Patten, Domain match found. Going to check in include url list. url = %s", url);
            return (check_inlcude_url(pattern_table_index, url));
          }
        }
      }
    }
  } else{
    return (check_inlcude_url(pattern_table_index, url));
  }

  NSDL2_HTTP(NULL, NULL, "Returning from end , removing by dfault. url = %s", url);
  return REMOVE_URL;

}
  // Excute exclude url  
static inline void init_host_server_entries(VUser *vptr){
  int index;
  for(index = 0; index <= g_cur_server; index++) // TODO - Make static inline method
  {
    (((HostSvrEntry *)vptr->hptr) + index)->num_parallel = 0;
    (((HostSvrEntry *)vptr->hptr) + index)->hurl_left = 0;
    (((HostSvrEntry *)vptr->hptr) + index)->prev_hlist = NULL;
    (((HostSvrEntry *)vptr->hptr) + index)->next_hlist = NULL;
    (((HostSvrEntry *)vptr->hptr) + index)->cur_url = NULL;
    (((HostSvrEntry *)vptr->hptr) + index)->cur_url_head = NULL;
  }
  
}

int filter_embd_urls(connection *cptr, int num_eurls, EmbdUrlsProp *eurls, int pattern_table_idx){

  int i, num_filter_eurls = 0;
  NSDL1_HTTP(NULL, NULL, "Method called, num_eurls = %d, pattern_table_idx = %d", num_eurls, pattern_table_idx);

  for(i = 0; i < num_eurls; i++) {

    if(process_pattern(cptr, eurls[i].embd_url, pattern_table_idx) == REMOVE_URL){
      free(eurls[i].embd_url);
      eurls[i].embd_url = NULL;
      num_filter_eurls++;
    }
  }
  return num_filter_eurls;
}

void populate_auto_fetch_embedded_urls(VUser *vptr, connection *cptr, int pattern_table_index, int hls_flag)
{
  //char **eurls = NULL;
  EmbdUrlsProp *eurls = NULL;
  int num_eurls = 0;
  int num_filter_eurls = 0;
  //int num_hel = 0;
  //HostTableEntry_Shr *hel, *tmp_hel, *old = NULL;
  char err[1024 + 1];
  char *inp_buffer_body = url_resp_buff + vptr->body_offset;
  int body_size = 0;

  uncomp_cur_len = 0;

  //TODO: need to rethink why body size is taken from content_length, Is HLS will work for chunked encoding????
  if(hls_flag) 
    body_size = cptr->content_length;
  else 
    body_size = vptr->bytes - vptr->body_offset; 
  //http_request_Shr_free *hfree;    
  
  NSDL1_HTTP(vptr, cptr, "Method called, pattern_table_index = %d, hls_flag= %x, url_resp_buff= %s, size of buffer = %d", 
                          pattern_table_index, hls_flag,url_resp_buff, body_size );
  /* Manpreet: In regard to bug#3690 and bug#3389
   * 
   * While script parsing we store page information of each page on vptr->hptr(create link list of hosts on vptr)
   * next while fetching embd URLs we populate vptr->hptr.
   * In case of page with or without embedded URLs it is required to memset host 
   * link list with 0
   * 
   * g_cur_server this is the index of count of total servers(including dynamic) + 1
   * */
  NSDL1_HTTP(vptr, cptr, "Memset HostSvrEntry structure, g_cur_server = %d vptr->hptr = %p vptr->hptr->hurl_left = %d", g_cur_server, vptr->hptr, vptr->hptr->hurl_left);
  //memset(vptr->hptr, 0, sizeof(HostSvrEntry) * (g_cur_server + 1));

  /* 
   * Manpreet: In release 3.8.2 above changes were done to fix bug 3690 and 3389.
   * But in release 3.8.5, following issues occured due to changes done:
   * a) REUSE CONNECTION, while memsetting HostSvrEntry structure following pointers became
   *    NULL (connection* svr_con_head, connection* svr_con_tail) hence NS was unable to 
   *    resuse connection, and every time new connection was made.
   * b) SSL_SESSION, sess pointer got null hence ssl sessions became unavailable at hptr.

   * NOTE: Therefore following HostSvrEntry fields should not be memset: 
   * connection* svr_con_head
   * connection* svr_con_tail
   * SSL_SESSION *sess

   * HostSvrEntry fields which are memset: 
   * num_parallel            :  Gets intialize at ns_page.c while executing first page 
   * hurl_left               :  In bug 3690 and 3389 defect was hurl_left count which 
   *                            was updated by inline urls 
   * HostSvrEntry* prev_hlist:  Gets updated while auto fetching embd URLs
   * *next_hlist                 
   * action_request_Shr* cur_url : These fields get forged.
   * *cur_url_head 
   * */
  init_host_server_entries(vptr);
  
  if (url_resp_size) { /* Dont pass buffer with 0 size */
    if (vptr->compression_type) {
      //if (ns_decomp_do_new (inp_buffer_body, body_size, vptr->compression_type, err)) {
      if (nslb_decompress(inp_buffer_body, body_size, &uncomp_buf, (size_t *)&uncomp_max_len, (size_t *)&uncomp_cur_len,
                          vptr->compression_type, err, 1024)) {
        /* Here in this case we will have Zero eurls */
        NSDL3_HTTP(vptr, cptr, "Error Decompressing: %s", err);
        error_log("Error Decompressing: %s", err);
        //fprintf (stderr, "Error decompressing non-chunked body: %s\n", err); /*bug 78764 : commented unwanted trace*/
      } else {
        if(hls_flag)
          eurls = get_embd_m3u8_url(uncomp_buf, uncomp_cur_len, &num_eurls, err, runprof_table_shr_mem[vptr->group_num].gset.m3u8_gsettings.bandwidth); 
        else
          eurls = get_embd_objects(uncomp_buf, uncomp_cur_len, &num_eurls, err);
      }
    } 
    else 
        if(hls_flag)
          eurls = get_embd_m3u8_url(url_resp_buff, body_size, &num_eurls, err, runprof_table_shr_mem[vptr->group_num].gset.m3u8_gsettings.bandwidth); 
        else
          eurls = get_embd_objects(url_resp_buff, body_size, &num_eurls, err);
  }

  //Need to merge embd urls which are in API also
  if(vptr->flags & NS_BOTH_AUTO_AND_API_EMBD_OBJS_NEED_TO_GO && g_num_eurls_set_by_api) 
  {
    NSDL1_HTTP(vptr, cptr, "Both API and Autofetched urls will be going to feteched, adding APIs url wwith auto featched urls");
    MY_REALLOC_EX (eurls, sizeof(EmbdUrlsProp) * (num_eurls + g_num_eurls_set_by_api), (num_eurls) * sizeof(EmbdUrlsProp), "eurls", -1);
    int j = 0;
    int i;
     
    NSDL1_HTTP(vptr, cptr, "Total Auto fetched urls = %d, API urls = %d", num_eurls, g_num_eurls_set_by_api);
    for(i = num_eurls; i < (num_eurls + g_num_eurls_set_by_api); i++)
    {
      eurls[i].embd_type = eurls_prop[j].embd_type;
      eurls[i].embd_url = eurls_prop[j].embd_url;
      j++;
    }
    num_eurls += g_num_eurls_set_by_api;
    FREE_AND_MAKE_NOT_NULL_EX(eurls_prop, sizeof(eurls_prop), "EmbdUrlsProp", -1);
    vptr->flags &= ~NS_EMBD_OBJS_SET_BY_API;
    vptr->flags &= ~NS_BOTH_AUTO_AND_API_EMBD_OBJS_NEED_TO_GO;
    g_num_eurls_set_by_api = 0;
  }

  NSDL3_HTTP(vptr, cptr, "num_eurls = %d", num_eurls);

  if(pattern_table_index != -1)
    num_filter_eurls = filter_embd_urls(cptr, num_eurls, eurls, pattern_table_index);

  // ISSUE: Memory leak in case of malformed URL or dyn host issue
  if (num_eurls) { /* We dont need to do anything if there are no embedded URLs */

#ifdef NS_DEBUG_ON 
    {
      int i = 0;
      for (i = 0; i < num_eurls; i++)
        NSDL3_HTTP(vptr, cptr, "Extracted embedded url[%d] = %s",
                                i, eurls[i].embd_url);
    }
#endif
    if(num_eurls != num_filter_eurls)
      set_embd_objects(cptr, num_eurls, eurls);
    else
      vptr->urls_awaited = vptr->urls_left = (num_eurls - num_filter_eurls);

    /* Free EURLs */
    free_embd_array(eurls, num_eurls);
    eurls = NULL;

  } else { /* Print the error encountered */
    vptr->urls_awaited = vptr->urls_left = 0;
    //   error_log("Error while parsing HTML buffer: %s\n", err);
  }
  NSDL4_HTTP(vptr, cptr, "vptr->urls_awaited = %d vptr->urls_left = %d num_filter_eurls = %d", vptr->urls_awaited, 
                          vptr->urls_left, num_filter_eurls);
}

void free_all_embedded_urls(VUser *vptr)
{
      int ii, i;
      action_request_Shr *curl;
      //      HostTableEntry_Shr *hel;
   
     NSDL3_HTTP(vptr, NULL, "Method free_all_embedded_urls called vptr = %p, "
                             "vptr->num_http_req_free = %d", vptr,vptr->num_http_req_free);
      
      for (i = 0; i <  vptr->num_http_req_free; i++) {
        curl = vptr->http_req_free[i].http_req_ptr;
        NSDL3_HTTP(vptr, NULL, "vptr->http_req_free[%d].num_url = %d", i, vptr->http_req_free[i].num_url);
        for (ii = 0; ii < vptr->http_req_free[i].num_url; ii++) {
          FREE_AND_MAKE_NULL_EX(curl[ii].proto.http.url.seg_start->seg_ptr.str_ptr->big_buf_pointer, curl[ii].proto.http.url.seg_start->seg_ptr.str_ptr->size + 1, "free all embedded urls", ii);// added big_buf_pointer size including NULL
          FREE_AND_MAKE_NULL_EX(curl[ii].proto.http.url.seg_start->seg_ptr.str_ptr, sizeof(PointerTableEntry_Shr), "free all embedded urls", ii);// added size of str_ptr
          FREE_AND_MAKE_NULL_EX(curl[ii].proto.http.url.seg_start, sizeof(SegTableEntry_Shr), "free all embedded urls", ii);// added size of struct
        }
        FREE_AND_MAKE_NULL_EX(curl, sizeof(action_request_Shr), "curl", i); // added size of struct
      }
      FREE_AND_MAKE_NULL_EX(vptr->http_req_free, sizeof(http_request_Shr_free), "vptr->http_req_free", -1); // added size of struct
      vptr->is_embd_autofetch = 0;
      vptr->num_http_req_free = 0;
      /* Unset the flag; we can stop using is_embd_autofetch ?? */
      RESET_BIT_FLAG_NS_EMBD_OBJS_SET_BY_API(vptr);
      /* vptr->flags &= ~NS_EMBD_OBJS_SET_BY_API; */
}

// This method will fill inline for replay acccess log
// In case we are playing inline as per log this method will fill inline urls
extern EmbdUrlsProp *get_replay_embd_objects(int replay_user_idx, int *num_nvm, char *error_msg);
void populate_replay_embedded_urls(VUser *vptr, connection *cptr)
{
  EmbdUrlsProp *eurls = NULL;
  int num_eurls = 0;
  char err[0xfff];
  
  NSDL2_HTTP(vptr, cptr, "Method called. g_cur_server = %d vptr->hptr = %p vptr->hptr->hurl_left = %d", g_cur_server, vptr->hptr, vptr->hptr->hurl_left);

  init_host_server_entries(vptr);

  eurls = get_replay_embd_objects(vptr->replay_user_idx, &num_eurls, err);

  NSDL3_HTTP(vptr, cptr, "num_eurls = %d", num_eurls);

  if (num_eurls) { /* We dont need to do anything if there are no embedded URLs */
#ifdef NS_DEBUG_ON 
    {
      int i = 0;
      for (i = 0; i < num_eurls; i++)
        NSDL3_HTTP(vptr, cptr, "Replay Embedded url[%d] = %s", i, eurls[i].embd_url);
    }
#endif

    set_embd_objects(cptr, num_eurls, eurls);

    /* Free EURLs */
    free_embd_array(eurls, num_eurls);
    eurls = NULL;
  } else { 
    vptr->urls_awaited = vptr->urls_left = 0;
  }
}

