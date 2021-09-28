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

#include "netstorm.h"
#include "ns_log.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_sock_com.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_fetch_embd.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_http_cache.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_http_script_parse.h"
//#include "ns_http_cache_store.h"
//#include "ns_http_cache_reporting.h"
#include "ns_url_hash.h"
#include "nslb_encode.h"
#include "ns_dynamic_hosts.h"
//#include "ns_child_msg_com.h"
#include "ns_exit.h"

//extern void end_test_run( void );

#if 0
static inline void update_redirect_count_of_connections(connection* local_cptr, VUser *vptr, char *type)
{
  int i;
  NSDL2_HTTP(vptr, local_cptr, "Method called, local_cptr = %p", local_cptr);
  for ( i = 0; local_cptr != NULL; local_cptr = (connection *)local_cptr->next_inuse, i++) {
    if (local_cptr->redirect_count != 0) {
      /* Something is wrong */
      error_log("redirect_count (%d) should be zero in connection list of type %s for cptr at position = %d in the linked list\n", local_cptr->redirect_count, type, i);
      local_cptr->redirect_count = 0;
    }      
  }
}
/* Function used to reset redirect count of connections
 * in connection pool design connections can be in either
 * or both inuse or reuse link list
 * */
inline void reset_redirect_count(VUser *vptr) 
{
  NSDL2_HTTP(vptr, NULL, "Method called");
  if (vptr->head_cinuse)
  {
    update_redirect_count_of_connections(vptr->head_cinuse, vptr, "inuse");
  }
  if(vptr->head_creuse)
  {
    update_redirect_count_of_connections(vptr->head_creuse, vptr, "reuse");
  }
}
#endif

//For dynamic host
//Return: Success case returns 0, redirection of main url fails then returns -1 for embd url returns -2.
int
auto_redirect_connection(connection* cptr, VUser *vptr,
			 u_ns_ts_t now , int done, int redirect_flag)
{
  char url_extract[MAX_LINE_LENGTH];
  char hostname[MAX_LINE_LENGTH];
  char *redirect_url = cptr->location_url;
  action_request_Shr *redirect_url_num = NULL;
  action_request_Shr *old_url_num;
  int port, request_type;
  //int host_is_same_as_parent = 1;
  int gserver_shr_idx;
 /****** Redirect URL encoding: BEGIN *******/
  char replacement_chars[128];
  int out_len = 0;
/****** Redirect URL encoding: END *******/
  /***DYNAMIC HOST--BEGIN***/
  int url_type; //Added for debugging purpose
  /***DYNAMIC HOST--END***/
  char *absolute_url_request = NULL;//For redirect relative url

  NSDL2_HTTP(vptr, cptr, "Method called, redirect_url = %s, done = %d, redirect_flag = 0x%x",
			  redirect_url, done, redirect_flag);

  NS_DT3(vptr, cptr, DM_L1, MM_HTTP, "Redirecting to URL %s", redirect_url);
  //memset(url_extract, 0, MAX_LINE_LENGTH);
  url_extract[0] = '\0';

  old_url_num = cptr->url_num;
  
  if(extract_hostname_and_request(redirect_url, hostname, url_extract, &port, &request_type,
                               get_url_req_url(cptr), cptr->url_num->request_type) < 0) {
     NS_EXIT(-1, "extract_hostname_and_request() failed");
  }
  
  if (port == -1) {
    if (request_type == -1) {
      port = cptr->url_num->index.svr_ptr->server_port;
    } else { /* else we put the default port */
      if (cptr->cur_server.sin6_family == AF_INET6) { /* IPV6 */
        if (request_type == HTTPS_REQUEST) // https
          port = (6443);
        else // http
          port = (6880);
      } else { /* IPV4 */
        if (request_type == HTTPS_REQUEST) { // https
          port = (443);
        } else // http
          port = (80);
      }
      NSDL2_HTTP(vptr, cptr, "port = %d\n", port);
    }
  }
  NSDL2_HTTP(vptr, cptr, "Extracted URL (%s) from previous request line (%s), extracted host = %s, port = %d", 
              url_extract, old_url_num->proto.http.redirected_url, hostname, port);
  

  MY_MALLOC(redirect_url_num, sizeof(action_request_Shr), "redirect_url_num", -1);
  NSDL2_HTTP(vptr, cptr, "Alloc = %p\n", redirect_url_num);
  /* copy contents of cptr->url_num because now we are going pass redirect_url_num dircrly *
   * insted of cptr->url_num in try_url_* functions */
  memcpy(redirect_url_num, cptr->url_num, sizeof (action_request_Shr));
  
  //Redirect url can't be parameterized, hence resetting, but main url can be paramterized.
  redirect_url_num->is_url_parameterized = 0;
  if(!cptr->url_num->parent_url_num)
    redirect_url_num->parent_url_num = cptr->url_num;
  /* We initialize them every time and fill them so we dont have to distinguish while we are freeing them. */

  if (request_type != -1)
    redirect_url_num->request_type = request_type;
  
  url_type = cptr->url_num->proto.http.type;
  
  if (hostname[0] != '\0') {
    unsigned short rec_server_port; //Sending Dummy
    int hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);
    if ((gserver_shr_idx = find_gserver_shr_idx(hostname, port, hostname_len)) == -1) {
    /* Dynamic Host ---BEGIN */
      NSDL2_HTTP(vptr, cptr, "Redirection URL Type:cptr->url_num->proto.http.type = %d", cptr->url_num->proto.http.type);
      gserver_shr_idx = add_dynamic_hosts (vptr, hostname, port, url_type, 1, request_type, url_extract, vptr->sess_ptr->sess_name, vptr->cur_page->page_name, vptr->user_index, runprof_table_shr_mem[vptr->group_num].scen_group_name);
      /* Check whether main or embedded url:
       * main url: abort_bad_page()
       * embedded url: Ignore the url and go for next connection
       * Dynamic host cannot be added becoz
       * Case 1: user has exceeded host limit(mention in keyword)
       * Case 2: DNS look up failure
       * Case 3: Keyword disable
       */
      if (gserver_shr_idx < 0)
      {
        NS_DT3(vptr, cptr, DM_L1, MM_HTTP, "Redirection cannot be done due to host table full or host cannot be resolved");
        NSDL2_HTTP(vptr, cptr, "gserver_shr_idx = %d", gserver_shr_idx);   
        // Free allocated memory
        FREE_AND_MAKE_NULL_EX (redirect_url_num, sizeof (action_request_Shr), "Redirected Url", -1);
        FREE_LOCATION_URL(cptr, redirect_flag);
        FREE_CPTR_URL(cptr); // Url is needed in extract_hostname_and_request
        cptr->redirect_count = 0;

        if(url_type == MAIN_URL) {
          NSDL2_HTTP(vptr, cptr, "Main url failed hence aborting page...url_type = %d", url_type);
          return ERR_MAIN_URL_ABORT;
        } else if(url_type == EMBEDDED_URL) {
          NSDL2_HTTP(vptr, cptr, "Embedded url failed, ignore url and process next url...url_type = %d", url_type);
          return ERR_EMBD_URL_ABORT;
        }
      }
    }
    /*Dynamic Host ---END */
     
    redirect_url_num->index.svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];

    NSDL2_HTTP(vptr, cptr, "new_host = %s, old_host = %s\n",
                  redirect_url_num->index.svr_ptr->server_hostname,
                  cptr->url_num->index.svr_ptr->server_hostname);

/*       if ((strcmp(redirect_url_num->proto.http.index.svr_ptr->server_hostname,  */
/*                   cptr->url_num->proto.http.index.svr_ptr->server_hostname) != 0) || */
/*           (redirect_url_num->request_type != cptr->url_num->request_type) || */
/*           (port != cptr->url_num->proto.http.index.svr_ptr->server_port))  */

/*         host_is_same_as_parent = 0; */
    
  } else {
    redirect_url_num->index.svr_ptr = cptr->url_num->index.svr_ptr;
  }
  

  if (global_settings->g_auto_redirect_use_parent_method) 
  {
    redirect_url_num->proto.http.http_method = old_url_num->proto.http.http_method;
    redirect_url_num->proto.http.http_method_idx = old_url_num->proto.http.http_method_idx;
  }
  else 
  {
    redirect_url_num->proto.http.http_method = HTTP_METHOD_GET;
    redirect_url_num->proto.http.http_method_idx = HTTP_METHOD_GET;
  }
/****** Redirect URL encoding: BEGIN *******/
  memset(replacement_chars, 0, 128);
  replacement_chars[' '] = 1;

  /*Find relative url path "/.." in the embedded URL*/
  char *found_relative = NULL;
  found_relative = strstr(url_extract, "/../");
  if(found_relative != NULL)
  { 
    //Found relative path
    int url_len = strlen(url_extract);
    NSDL2_HTTP(NULL, cptr, "Relative url found. Going to make absolute url");
    //Malloc relative URL request
    MY_MALLOC (absolute_url_request, (url_len + 1), "Absolute url request", -1);
    //Update forged_request with absolute path
    make_part_of_relative_url_to_absolute(url_extract, url_len, absolute_url_request);
    NSDL2_HTTP(NULL, cptr, "Returned absolute url = %s", absolute_url_request);
    redirect_url_num->proto.http.redirected_url = ns_escape_ex(absolute_url_request, 0, &out_len, replacement_chars, "+", NULL, 1);
    FREE_AND_MAKE_NULL_EX(absolute_url_request, (url_len + 1), "Absolute url request", -1);
  }
  else
    redirect_url_num->proto.http.redirected_url = ns_escape_ex(url_extract, 0, &out_len, replacement_chars, "+", NULL, 1);
  //MY_MALLOC(redirect_url_num->proto.http.redirected_url, strlen(url_extract) + 1,
  //          "redirect_url_num->proto.http.redirected_url", -1);
  //strcpy(redirect_url_num->proto.http.redirected_url, url_extract); 
/****** Redirect URL encoding: END *******/
  redirect_url_num->proto.http.http_version = 1; // HTTP/1.1

  NSDL2_HTTP(vptr, cptr, "Redirected URL = %s", redirect_url_num->proto.http.redirected_url);
  
  if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT)
  { 
    NSDL2_HTTP(NULL, cptr, "Redirected URL = %s", redirect_url_num->proto.http.redirected_url);
    redirect_url_num->proto.http.url_index = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)redirect_url_num->proto.http.redirected_url, 
                                                             out_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    NSDL3_HTTP(vptr, NULL, "req_url->url_id = %d", redirect_url_num->proto.http.url_index);
  }
  NSDL1_HTTP(vptr, NULL, "redirect_url_num->proto.http.url_id = %d", redirect_url_num->proto.http.url_index); 

  FREE_LOCATION_URL(cptr, redirect_flag);
  FREE_CPTR_URL(cptr); // Url is needed in extract_hostname_and_request
 
  FREE_CPTR_PARAM_URL(cptr); 
  vptr->urls_left++;
 
  if (!done) {
    // Following code is for handling of http2 multiplexing, here we are checking if redirection is on same host, and streams are open on this     // connection, connection state is set to CNST_REUSE_CON as it is checked in try_url_on_cur_con   
    if((cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2->total_open_streams)){
      int cur_host, last_host;
      cur_host = get_svr_ptr(redirect_url_num, vptr)->idx;
      last_host = cptr->gServerTable_idx; /* taking from saved idx since we might have freed cptr->url_num in case of redirecion */
      if (cur_host == last_host) {
        cptr->conn_state = CNST_REUSE_CON;
      }
      else{
        if (!try_url_on_any_con (vptr, redirect_url_num, now, NS_HONOR_REQUEST)) { //Set flag in case of auto redirection, NS will get connection slot for requested redirected URL.
          printf("redirect_connection:%d:Unable to run Main URL\n", __LINE__);
         // NSTL1(NULL, NULL, "end_test_run called");
          end_test_run();
        }
        return 0; 
      }
    }

    if  (!try_url_on_cur_con (cptr, redirect_url_num, now)) {
      if (!try_url_on_any_con (vptr, redirect_url_num, now, NS_HONOR_REQUEST)) { //Set flag in case of auto redirection, NS will get connection slot for requested redirected URL.
	printf("redirect_connection:%d:Unable to run Main URL\n", __LINE__);
	//NSTL1(NULL, NULL, "end_test_run called");
        end_test_run();
      }
    }
  } else {
    if (!try_url_on_any_con (vptr, redirect_url_num, now, NS_HONOR_REQUEST)) {
      printf("%s:%d:Unable to run Main URL\n", __FUNCTION__, __LINE__);
      //NSTL1(NULL, NULL, "end_test_run called");
      end_test_run();
    }
  }
  return 0;
}

static inline void cache_arrange_location_url(connection *cptr, int *in_cache) {

  CacheTable_t *cache_ptr = NULL;
  *in_cache = 0;   // No 
  NSDL2_HTTP(NULL, cptr, "Method called");

  {
    cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    if(cache_ptr != NULL) {
      //In response we got no-store header, need to delte or clear the cache entry
      //if(cache_ptr->cache_resp_hdr->CacheControlFlags.no_store)
      if(!(cache_ptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE)) {
        NSDL2_CACHE(NULL, cptr, "Entry is not cachable");
        // URL was in cache and now it is not cacheable after we send the request
        if(cache_ptr->cache_flags & NS_CACHE_ENTRY_IN_CACHE) {
          //Case - When no-store received in validation
          NSDL2_CACHE(NULL, cptr, "Response(state) is in cache");
          /* cptr may have location_url as well as cache entry
             Two cases for cache entry has location_url:
             Case1 - cptr has no location_url -  Move from cache to cptr and do not free here
             Case2 - cptr has new location_url - Use this location_url
          */
          if(cptr->location_url == NULL) {
            cptr->location_url = cache_ptr->location_url;
            cache_ptr->location_url = NULL;
            *in_cache = 1; // Yes 
          } else {
            FREE_AND_MAKE_NULL_EX(cache_ptr->location_url, strlen(cache_ptr->location_url + 1), "Cache Location Url", -1);
          }
        } else {
          NSDL2_CACHE(NULL, cptr, "Entry is not cacheable & was not in CACHE");
          FREE_AND_MAKE_NULL_EX(cache_ptr->location_url, strlen(cache_ptr->location_url + 1), "Cache Location Url", -1);
        }
      } else { // Entry is Cachaable
        if(cache_ptr->cache_flags & NS_CACHE_ENTRY_NEW) {
          NSDL2_CACHE(NULL, cptr, "Response is not from cache, adding url to cache list");
          cache_ptr->location_url = cptr->location_url;
          *in_cache = 1; // Yes 
        } else {
           /* Location URL cases
            Case1 - Hit case (Cache is fresh)
              location_url from cache to cptr is set in cache_set_url_resp_from_cache() - So both are same (NULL or not NULL)

            Case2 - after revalidation after expiry
              304 - Both are same
              301/302 - we have in cptr only (copy from cptr to cache)
              200 - we have no where as it is not needed (We freed in cache after getting 200)
              Error - we may have in cache (copy cache to cptr)

            Case3 - after fresh req after expiry - cache does not have location url
              cptr may or may not have location_url (copy from cptr to cache)
          */

          if(cache_ptr->cache_flags & NS_CACHE_ENTRY_VALIDATE) {  // Case2
            switch(cptr->req_code) {
              case 301:
              case 302:
              case 307:
                cache_ptr->location_url = cptr->location_url;
                *in_cache = 1; // Yes
                break;
              case 200:
                /* We have already freed in validate_req_code*/
                break;
              default:
		cache_ptr->location_url = cptr->location_url;
                *in_cache = 1; // Yes
                break;
            }
          } else if(cache_ptr->cache_flags & NS_CACHE_ENTRY_IN_CACHE) { // Case1
            /* We have already copied in make request as entry is fresh*/ 
            *in_cache = 1; // Yes
          } else {   // Case 3
            cache_ptr->location_url = cptr->location_url;
            *in_cache = 1; // Yes 
          }
        }
      }
    } else {
      NSDL2_CACHE(NULL, cptr, "Cache Entry is NULL");
    }
  } 
  NSDL3_HTTP(NULL, cptr, "location_url = %s", cptr->location_url);
}
/* This function does the set up task for auto redirection in Cache
 * or Non-Cache mode both.
 * */
/* redirect_flag			  Reason
 * -------------------------------------------------------------------------
 *  (No Redirect, No Cache)               --> No 301/302
 * 					  --> Depth Exceed
 *		 			  --> Error
 * 					  --> Auto Redirect Disabled
 *
 *  (No Redirect, In Cache)               --> No 301/302
 * 					  --> Depth Exceed
 *		 			  --> Error
 * 					  --> Auto Redirect Disabled
 *				
 *  (Redirection but No Cache)		  --> Redirection is there but not in cache
 * 				     	      so free it. 
 *  (Redirection but in Cache)		  --> Redirection, So do not free it as its
 * 		                  	      is in cache.
 */
inline void http_setup_auto_redirection(connection *cptr, int *status,
					  	  int *redirect_flag) { 

  int in_cache = 0;
  *redirect_flag = 0;  // Default value
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(NULL, cptr, "Method Called, status = %d, location_url = %s, "
			 "redirect_count = %d, g_follow_redirects = %d, "
			 "redirect_flag = 0x%x",
			 *status, cptr->location_url, cptr->redirect_count,
			 global_settings->g_follow_redirects, *redirect_flag);

  // This is required for both auto and manual redirect cases
  if(NS_IF_CACHING_ENABLE_FOR_USER && cptr->cptr_data != NULL) {
    cache_arrange_location_url(cptr, &in_cache);
    if(in_cache) {
      *redirect_flag = NS_HTTP_REDIRECT_URL_IN_CACHE;
    } 
  }

  /* Here we handle the case of Permanent and Temporary redirect 301/302 */
  NSDL2_CONN(NULL, cptr,"location_url = %s, save_loc_hdr_on_all_rsp_code = %d", 
                  cptr->location_url, runprof_table_shr_mem[vptr->group_num].gset.save_loc_hdr_on_all_rsp_code);

  if((global_settings->g_follow_redirects) && (*status == NS_REQUEST_OK)) 
  {
     if(cptr->location_url)
     {
       cptr->redirect_count++; // This is to track depth of auto redirects
   
       if(cptr->redirect_count > global_settings->g_follow_redirects) {
         *status = NS_REDIRECT_EXCEED;
         NSDL1_HTTP(vptr, cptr, "Auto redirect depth (%d) exceeded the max depth value (%d), status = %d",
         cptr->redirect_count, global_settings->g_follow_redirects, *status);
         goto final;
       }

       *redirect_flag |= NS_HTTP_REDIRECT_URL;
       // We save it in vptr as cptr may get freed in auto redirect processing
       // When we allocate new cptr, then we copy redirect_count from vptr to cptr
       // Assumption is at any time, only one req will becoming here and then
       // starting auto redirection (till cptr->redirect_count is copied from vptr)
       vptr->redirect_count = cptr->redirect_count; 

       /* we have to preserve redirect_count value since vptr->redirect_count is set to zero once 
        we find that no more urls are left or redirection depth exceeds. redirect_count_save
        is used in search var and passed as redirection depth in do_data_processing. */
       //vptr->redirect_count_save = vptr->redirect_count;
       /* We mark it zero because we will retrieve the value from vptr. This ensures that when cptr is pushed to free list 
        * it does not retains redirect_count the value. */
       cptr->redirect_count = 0;
       return;
     }
  }
     
  final: 
    vptr->redirect_count = cptr->redirect_count = 0;
    vptr->urls_awaited--;
}
