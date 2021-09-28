#include "ns_data_types.h"
//#include "ns_msg_def.h"
#include "ns_cache_include.h"
#include "ns_http_cache_reporting.h"
#include "ns_js.h"
#include "ns_common.h"
#include "ns_http_auth.h"
#include "ns_h2_req.h"
#include <nghttp2/nghttp2.h>
#include "ns_group_data.h"

const char hdr_end[] = "\r\n";
const int hdr_end_len = 2;

const char ims_hdr[] = "If-Modified-Since: ";
const int ims_hdr_len = sizeof(ims_hdr) - 1;

const char etag_hdr[] = "If-None-Match: ";
const int etag_hdr_len = sizeof(etag_hdr) - 1;

char *HTTP2_IMS_HEADER = "if-modified-since";
#define HTTP2_IMS_HEADER_LEN 17
char *HTTP2_ETAG_HEADER = "if-none-match";
#define HTTP2_ETAG_HEADER_LEN 13

/*Function to check if Caching is to be enabled for the user 
 * Input: Vuser
 * Return: None. Sets flag
 */
void cache_vuser_check_and_enable_caching(VUser *vptr)
{
  NSDL2_CACHE(vptr, NULL, "Method called");

  // If 100%, not need to do random number
  if(runprof_table_shr_mem[vptr->group_num].gset.cache_user_pct == 10000.00) {
    vptr->flags |= NS_CACHING_ON;
    return;
  }

  int rand_num = (1 + (int) (100.0 * (rand() / (RAND_MAX + 1.0))))*100;

  NSDL2_CACHE(vptr, NULL, "rand_num = %d, err_pct = %d", 
                rand_num, runprof_table_shr_mem[vptr->group_num].gset.cache_user_pct);

  if(rand_num <= runprof_table_shr_mem[vptr->group_num].gset.cache_user_pct) {
    vptr->flags |= NS_CACHING_ON;
  }
}

// This method is used to calculate the max age when client freshness is eabled 
static cache_age_t calc_max_age(int req_max_age, int resp_max_age)
{
  NSDL2_CACHE(NULL, NULL, "Method called req_max_age = %d, resp_max_age = %d", req_max_age, resp_max_age);
  if(req_max_age == -1 && resp_max_age == -1)
    return -1;
  else if(req_max_age == -1)
    return resp_max_age;
  else if(resp_max_age == -1)
    return req_max_age;
  else
    return ((req_max_age < resp_max_age)? req_max_age: resp_max_age);
}


/*Function to get server freshness of the cached response */
static int get_freshness_lifetime(connection *cptr, CacheTable_t *cacheptr, int grp_idx)
{
  cache_age_t freshness_lifetime=0;
  float lm_factor = 0.1; //10%
  CacheResponseHeader *crh = cacheptr->cache_resp_hdr;
  cache_age_t max_age = 0;
  CacheRequestHeader *cache_req_hdr;
  
  cache_req_hdr = &(cptr->url_num->proto.http.cache_req_hdr);
  NSDL2_CACHE(NULL, NULL, "Method called. grp_idx = %d", grp_idx);

  /* Check what headers we have in response, depending on headers 
   * we will check the freshness of the response*/
  //If we have multiple headers or validators then we will use following priority
   
  //Assuming all values will be initialize with -1 
  if(runprof_table_shr_mem[grp_idx].gset.client_freshness_constraint)
  {
    NSDL2_CACHE(NULL, NULL, "request max_age, response max_age = %d, %d",
                                   cache_req_hdr->max_age, crh->max_age );
    max_age = calc_max_age(cache_req_hdr->max_age, crh->max_age );
    NSDL2_CACHE(NULL, NULL, "max_age calculated on both request & response_header = %d", max_age);
  }
  else
  {
    NSDL2_CACHE(NULL, NULL, "max_age calculated on response_header = %d", max_age);
    max_age = crh->max_age;
  }

  if(max_age > -1)
  {
    freshness_lifetime = max_age;
    NSDL2_CACHE(NULL, NULL, "freshness lifetime calculated on max-age= %d", freshness_lifetime);
  }
  else if(crh->expires > -1)
  {
    freshness_lifetime = crh->expires - crh->date_value;
    NSDL2_CACHE(NULL, NULL, "freshness lifetime calculated on expires= %u, crh->expires=%d, crh->date_value= %d ", 
                 freshness_lifetime, crh->expires, crh->date_value);
  } 
  else if (crh->last_modified_in_secs > -1)
  {
    //Get max value of 0 and (server_date - last_modified_date)
    freshness_lifetime = ((((crh->date_value - crh->last_modified_in_secs) > 0)?  
			(crh->date_value - crh->last_modified_in_secs):0) *lm_factor);
    NSDL2_CACHE(NULL, NULL, "freshness lifetime calculated on LMD= %u", freshness_lifetime);
  }
  //If neither max_age, expires & last_modified is present, freshness_lifetime will be returned with value 0
  else
  {
    freshness_lifetime = 0;
  }

  NSDL2_CACHE(NULL, NULL, "freshness_lifetime  = %u", freshness_lifetime);
  return freshness_lifetime;
}

/*Function to check if given given request's response is fresh
 * to use as response
 * Input: url 
 * Return: 1 for response is fresh or 0 (need to revalidate)*/
static int cache_check_freshness(connection *cptr, CacheTable_t *cacheptr, u_ns_ts_t now)
{
  int freshness_lifetime;
  int current_age;
  int grp_idx;
  double freshness_factor=1;
  CacheRequestHeader *cache_req_hdr;
  NSDL2_CACHE(NULL, NULL, "Method called");
  /*First get server freshness and then get the current age of the 
 * cached response */
  grp_idx = ((VUser *)(cptr->vptr))->group_num;
  freshness_lifetime = get_freshness_lifetime(cptr, cacheptr, grp_idx);
  cache_req_hdr = &(cptr->url_num->proto.http.cache_req_hdr);
  //This is the case when request have max-stale without value
  if(runprof_table_shr_mem[grp_idx].gset.client_freshness_constraint) {
    if(cache_req_hdr->max_stale == CACHE_ALWAYS_SERVE_FROM_CACHE) {
       NSDL2_CACHE(cptr->vptr, cptr, "Servig from cashe CACHE_ALWAYS_SERVE_FROM_CACHE max_stale value=%d", cache_req_hdr->max_stale);
       return 1;
    }
  }
  //For Testing Purpose only
  //grp_idx = ((VUser *)(cptr->vptr))->group_num;
  freshness_factor = runprof_table_shr_mem[grp_idx].gset.cache_freshness_factor;
  freshness_lifetime *= freshness_factor; 
  NSDL2_CACHE(cptr->vptr, cptr, "freshness lifetime=%u, grp_idx=%d, freshness_factor=%f",
                               freshness_lifetime, grp_idx, freshness_factor);

  current_age = cache_get_current_age (cptr, cacheptr, now);
  
  if(runprof_table_shr_mem[grp_idx].gset.client_freshness_constraint)
  {
    if(freshness_lifetime > 0 && cache_req_hdr->max_stale != -1)
    {
      freshness_lifetime += cache_req_hdr->max_stale;
      NSDL2_CACHE(cptr->vptr, cptr, "max_stale = %u, max-stale will be added to freshness lifetime, freshness lifetime = %u", cache_req_hdr->max_stale, freshness_lifetime);
    }

    if(cache_req_hdr->min_fresh != -1)//min_fresh shuold be checked?
    {
      current_age += cache_req_hdr->min_fresh;
      NSDL2_CACHE(cptr->vptr, cptr, "min_fresh = %u, min fresh will be added to current age, current_age = %u", cache_req_hdr->min_fresh, current_age);
    }

    NSDL2_CACHE(cptr->vptr, cptr, "freshness lifetime=%u, max_stale=%d, current_age=%u, min_fresh=%d",
              freshness_lifetime, cache_req_hdr->max_stale, current_age, cache_req_hdr->min_fresh);
  }

  if(current_age < 0)
      current_age = 0;

  NSDL2_CACHE(NULL, NULL, "freshness lifetime = %u, current_age=%u", freshness_lifetime, current_age);

  if(freshness_lifetime > current_age)
  {
    NSDL2_CACHE(NULL, NULL, "(freshness lifetime - current_age) = %u....CACHED ENTRY IS FRESH", 
                                                                          freshness_lifetime - current_age);
    return 1;//Reponse is fresh
  }
  else
  {
    NSDL2_CACHE(NULL, NULL, "(freshness lifetime - current_age) = %u....CACHED ENTRY IS STALE", 
                                                                          freshness_lifetime - current_age);
    return 0;
  }
}

/*******************************************************************************************
  Purpose: This method is used for filling the "if-modified-since" header and if-none-match
            header in http2 table.
  Input  : It takes cacheptr as input. 
********************************************************************************************/
void cache_fill_validators_in_req_http2(CacheTable_t *cacheptr, int push_flag)
{
  CacheResponseHeader *cache_resp_hdr_lol = cacheptr->cache_resp_hdr;

  NSDL2_CACHE(NULL, NULL, "Method called, last_modified_len = %d, cache_resp_hdr_lol->etag = %s, etag_len = %d", 
                          cache_resp_hdr_lol->last_modified_len, cache_resp_hdr_lol->etag, cache_resp_hdr_lol->etag_len);

  cache_avgtime->cache_num_entries_revalidation++;

  if(cache_resp_hdr_lol->last_modified_len > 0)//Last modified time is given
  {
    NSDL2_CACHE(NULL, NULL, "Last modified time is given. Time = %s", cache_resp_hdr_lol->last_modified);
    if(!push_flag){
      FILL_HEADERS_IN_NGHTTP2(HTTP2_IMS_HEADER, HTTP2_IMS_HEADER_LEN, cache_resp_hdr_lol->last_modified, cache_resp_hdr_lol->last_modified_len + 1, 0, 0);
    }
    else{
      LOG_PUSHED_REQUESTS(HTTP2_IMS_HEADER, HTTP2_IMS_HEADER_LEN, cache_resp_hdr_lol->last_modified, cache_resp_hdr_lol->last_modified_len + 1);  }
    cache_avgtime->cache_num_entries_revalidation_ims++;
  } 
 
  if(cache_resp_hdr_lol->etag_len > 0)//Last modified time is given
  {
    NSDL2_CACHE(NULL, NULL, "Etag is given. Etag = %s", cache_resp_hdr_lol->etag);
    
    if(!push_flag){
      FILL_HEADERS_IN_NGHTTP2(HTTP2_ETAG_HEADER, HTTP2_ETAG_HEADER_LEN, cache_resp_hdr_lol->etag, cache_resp_hdr_lol->etag_len, 0, 0);
    }
    else{
      LOG_PUSHED_REQUESTS(HTTP2_ETAG_HEADER, HTTP2_ETAG_HEADER_LEN, cache_resp_hdr_lol->etag, cache_resp_hdr_lol->etag_len);
    } 
    cache_avgtime->cache_num_entries_revalidation_etag++;
  }  
}

int cache_fill_validators_in_req(CacheTable_t *cacheptr, VUser *vptr)
{
  CacheResponseHeader *cache_resp_hdr_lol = cacheptr->cache_resp_hdr;

  NSDL2_CACHE(NULL, NULL, 
           "Method called, last_modified_len = %d, cache_resp_hdr_lol->etag = %s, etag_len = %d", 
            cache_resp_hdr_lol->last_modified_len, cache_resp_hdr_lol->etag, cache_resp_hdr_lol->etag_len);

  INC_CACHE_NUM_ENTERIES_REVALIDATION_COUNTERS(vptr);

  if(cache_resp_hdr_lol->last_modified_len > 0)//Last modified time is given
  {
    NSDL2_CACHE(NULL, NULL, "Last modified time is given. Time = %s", cache_resp_hdr_lol->last_modified);
    
    NS_FILL_IOVEC(g_req_rep_io_vector, (char *)ims_hdr, ims_hdr_len);
    NS_FILL_IOVEC(g_req_rep_io_vector, cache_resp_hdr_lol->last_modified, cache_resp_hdr_lol->last_modified_len);
    NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);
    INC_CACHE_NUM_ENTERIES_REVALIDATION_IMS_COUNTERS(vptr);
  } 
 
  if(cache_resp_hdr_lol->etag_len > 0)//Last modified time is given
  {
    NSDL2_CACHE(NULL, NULL, "Etag is given. Etag = %s", cache_resp_hdr_lol->etag);

    NS_FILL_IOVEC(g_req_rep_io_vector, (char *)etag_hdr, etag_hdr_len);
    NS_FILL_IOVEC(g_req_rep_io_vector, cache_resp_hdr_lol->etag, cache_resp_hdr_lol->etag_len);
    NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);
    INC_CACHE_NUM_ENTERIES_REVALIDATION_ETAG_COUNTERS(vptr);
  }  
  NSDL2_CACHE(NULL, NULL, "Returning with vector idx = %d", g_req_rep_io_vector.cur_idx);  
  return 0;
}

/*This function initialize the cache entry */
static void cache_init_cache_entry (connection *cptr, char *url, int url_len,
				    unsigned short url_offset, int ihashIndex)
{
  NSDL2_CACHE(NULL, cptr, "Method called, cptr_data = %p", cptr->cptr_data);
  CacheTable_t *cache_ptr;
  
  MY_MALLOC(cptr->cptr_data->cache_data, sizeof(CacheTable_t), "cptr->cptr_data->cache_data", 1); 
  memset(cptr->cptr_data->cache_data, 0, sizeof(CacheTable_t));
  
  cache_ptr = (cptr->cptr_data->cache_data); 

  MY_MALLOC(cache_ptr->url, url_len + 1, "cptr->cptr_data->cache_data->url", 1); 
  memset(cache_ptr->url, 0, url_len + 1);

  strcpy((char *)cache_ptr->url, url);
  cache_ptr->url_len = url_len;
  cache_ptr->url_offset = url_offset; // Offset of page in url
  cache_ptr->http_method = cptr->url_num->proto.http.http_method; //Bug 2119  #Shilpa 10Feb2011
  cache_ptr->ihashIndex = ihashIndex;

  NSDL2_CACHE(NULL, cptr, "URL = %*.*s", url_len, url_len, (char *)cache_ptr->url);

  MY_MALLOC(cache_ptr->cache_resp_hdr, sizeof(CacheResponseHeader), "CacheResponseHeader", 1);
  memset(cache_ptr->cache_resp_hdr, 0, sizeof(CacheResponseHeader)); //TODO: Is it needed ?? AC

  cache_ptr->cache_resp_hdr->max_age = -1;
  cache_ptr->cache_resp_hdr->expires = -1;
  cache_ptr->cache_resp_hdr->date_value = -1;
  cache_ptr->cache_resp_hdr->last_modified_in_secs = -1;
  cache_ptr->cache_resp_hdr->age = -1;
  cache_ptr->cache_ts = -1;
  cache_ptr->request_ts = -1;
  cache_ptr->doc_age_when_received = -1;
  cache_ptr->cache_hits_cnt = 0;
  cache_ptr->ihashMasterIdx = -1;
}

// this is to initilize CacheRequestHeader 
void cache_init_cache_req_hdr(CacheRequestHeader *cache_req_hdr)
{
  NSDL2_CACHE(NULL, NULL, "Method called");
  memset(cache_req_hdr, 0, sizeof(CacheRequestHeader));
  cache_req_hdr->max_age = cache_req_hdr->max_stale = cache_req_hdr->min_fresh = -1;
}


/*This function is for checking validators.
 * If no validators given then we will send 
 * full request with out any validaators.*/
static int check_for_validators(CacheTable_t *cacheptr, connection *cptr, VUser *vptr)
{
  CacheResponseHeader *cache_resp_hdr_lol = cacheptr->cache_resp_hdr;
  int free_emd;

  NSDL2_CACHE(vptr, cptr, "Method called");

  if(cache_resp_hdr_lol->last_modified_len <= 0 && 
                            cache_resp_hdr_lol->etag_len <= 0)
  {
    // We should remove all cache hdr and resp list & keep entry in table
    // Also keep CacheTable and url for use in cptr_data
    if(cacheptr->ihashMasterIdx != -1)
    //if(runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 1 && cptr->url_num->proto.http.type != MAIN_URL)
    {
      //Doing here because we need to free response 
      //Here case can be one user's cache is expired and other one's is not expired.
      //and mode is master mode then we need to free master table resp
      //Case: If one user's resp expired but other user's resp is not expired
      //      then at the time of saving URL in master table we check if there 
      //      is already resp buf then first we free it then we fill new resp buff.
      NSDL3_CACHE(vptr, cptr, "Mode 1 is found, going to delete url in master table");
      cache_delete_url_frm_master_tbl(vptr, cacheptr->http_method, cacheptr->url, cacheptr->url_len, &free_emd, 1);
      cacheptr->ihashMasterIdx = -1; //Doing -1 as we have deleted MT entry for this node TODO: Need to check bcoz from here this node will traet like we are using Non MT mode, so in any case if we get no store then we check for ihashMasterIdx and we will get -1 and we will treat it as non MT mode.
      cacheptr->master_tbl_node = NULL;
      //Free all headers.
      cache_free_data(cacheptr, 0, 0, 0);
    }
    else
    {
      NSDL2_CACHE(vptr, cptr, "Before deleting resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used); 
      cache_avgtime->cache_bytes_used -= cacheptr->resp_len;
      DEC_CACHE_BYTES_USED_COUNTERS(vptr, cacheptr->resp_len);
      cache_free_data(cacheptr, 0, 1, 0);
      NSDL2_CACHE(vptr, cptr, "After deleting resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used); 
    }
    INC_CACHE_NUM_MISSED_COUNTERS(vptr);
    NSDL2_CACHE(vptr, cptr, "CACHE ACTION: cached entry is EXPIRED.."
                     "no validators present, sending new request,"
                     " cache miss count = %d, url=%s", cache_avgtime->cache_num_missed, cacheptr->url);
    return  CACHE_NOT_ENABLE;
  }
  else 
  {
    cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_VALIDATE);
    NSDL2_CACHE(vptr, cptr, "CACHE ACTION: cache is not fresh, sending revalidation request. Url=%s",cacheptr->url );
    return CACHE_NEED_TO_VALIDATE;
  }
}

inline void copy_main_url_in_vptr_http_data(connection *cptr, char *url_local, int url_local_len) {

  VUser *vptr = cptr->vptr;

  NSDL2_CACHE(NULL, cptr, "Method Called");

  if(runprof_table_shr_mem[vptr->group_num].gset.js_mode != NS_JS_DISABLE &&
     cptr->url_num->proto.http.type == MAIN_URL)
  {
    NSDL2_CACHE(NULL, cptr, "page_main_url = %p, page_main_url_len = %d"
                            " url_local_len = %d, url_local = [%s]",
                            vptr->httpData->page_main_url, 
                            vptr->httpData->page_main_url_len,
                            url_local_len, url_local);

    // Case 1: Coming here first time
    if(vptr->httpData->page_main_url == NULL) {
      MY_MALLOC(vptr->httpData->page_main_url, url_local_len + 1, "saving main url", -1);
      strcpy(vptr->httpData->page_main_url, url_local);
      vptr->httpData->page_main_url_len = url_local_len + 1;
    } else {  // Case 2: Already allocated
      // Case 2a: Url is too big to save in already allocated buffer
      if(vptr->httpData->page_main_url_len <= url_local_len) {
         MY_REALLOC_EX(vptr->httpData->page_main_url, url_local_len + 1, vptr->httpData->page_main_url_len, 
                    "Reallocating space for main url", -1);
         vptr->httpData->page_main_url_len = url_local_len + 1;
      } // else Case 2b: Enough buffer to save url
      strcpy(vptr->httpData->page_main_url, url_local);
    }
  }
}

int cache_check_if_url_cached(VUser *vptr, connection *cptr,
                              u_ns_ts_t now, char *url, int *len)
{
  char full_url[MAX_LINE_LENGTH + 1] = "\0";
  int full_url_len = 0;
  int url_offset = 0;
  int ihashIndex = -1;
  unsigned int ihashValue;
  CacheTable_t *cacheptr = NULL;
  CacheTable_t *cachemptr = NULL;

  u_ns_ts_t lol_search_url_start;
  u_ns_4B_t lol_search_url_time;

  NSDL2_CACHE(vptr, cptr, "Method called");

  INC_CACHE_COUNTERS(vptr);
  //Check for existance of user cache table
  //if not create table 
  if(NULL == vptr->httpData->cacheTable)
  {
    cache_init(vptr);      
  }

  full_url_len = 0;
  get_abs_url_req_line(cptr->url_num, vptr, url, full_url,
                       &full_url_len, &url_offset, MAX_LINE_LENGTH);

  /* JS-TODO copying complete url to vptr http_data we will use this to get
   * host/path in JS execution of included Java Script */
  copy_main_url_in_vptr_http_data(cptr, full_url, full_url_len);

  //Check if URL is already cached
  lol_search_url_start = get_ms_stamp();
  cacheptr = cache_url_found(vptr, (unsigned char *)full_url, full_url_len, NULL, &ihashIndex, &ihashValue, 
                                                     cptr->url_num->proto.http.http_method, USER_NODE);
  lol_search_url_time = (u_ns_4B_t)(get_ms_stamp() - lol_search_url_start);

  INC_CACHE_SEARCH_URL_TIME_TOT_COUNTERS(vptr, lol_search_url_time);

  if(lol_search_url_time < cache_avgtime->cache_search_url_time_min)
    cache_avgtime->cache_search_url_time_min = lol_search_url_time;
  if(lol_search_url_time > cache_avgtime->cache_search_url_time_max)
    cache_avgtime->cache_search_url_time_max = lol_search_url_time;

  NSDL1_CACHE(vptr, NULL, "Time taken to search URL in Cache = %'.3f seconds, min = %'.3f, max = %'.3f, Total time =  %'.3f", (double )lol_search_url_time/1000.0, (double )cache_avgtime->cache_search_url_time_min/1000.0, (double )cache_avgtime->cache_search_url_time_max/1000.0, (double)cache_avgtime->cache_search_url_time_total/1000.0);

  if(cptr->cptr_data == NULL){
    MY_MALLOC_AND_MEMSET(cptr->cptr_data, sizeof(cptr_data_t), "cptr->cptr_data", 1); 
    //cptr->cptr_data->buffer = NULL;
  }    

  if(cacheptr == NULL) // Entry is not in cache store
  {
    NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ: URL %s is not cached", full_url);
    cache_init_cache_entry(cptr, full_url, full_url_len, url_offset, ihashIndex);
    cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_NEW);
    //First time, This is same as cache is not enable for this user
    INC_CACHE_NUM_MISSED_COUNTERS(vptr);
    return CACHE_NOT_ENABLE; 
  }
  
  //Bug 2119  #Shilpa 10Feb2011
/*
  if(cacheptr->http_method != cptr->url_num->proto.http.http_method)
  {
    NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ: URL %s is cached for method=%d", full_url, cacheptr->http_method);
    cache_init_cache_entry(cptr, full_url, full_url_len, url_offset);
    cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_NEW);
    //First time, This is same as cache is not enable for this user
    cache_avgtime->cache_num_missed++;
    return CACHE_NOT_ENABLE; 
  }
*/

  // else //URL is cached now check for freshness of response for given URL
  {
    //Here we have cached data so assign cache entry pointer to data
    cptr->cptr_data->cache_data = cacheptr;

    //We got node into user node.Now fill resp buff from master table.
    //Get Node from the master table
    if(cacheptr->ihashMasterIdx != -1) { // Node is in master table
      NSDL2_CACHE(vptr, cptr, "About to call get_cache_node() for master node");
      cachemptr = get_cache_node(cacheptr->ihashMasterIdx, cptr->url_num->proto.http.http_method, NULL, vptr->httpData->master_cacheTable, cacheptr->url, cacheptr->url_len, cacheptr->ihashValue);
      if(cachemptr){
        //We are not pointing resp buffer this time.
        //We will take resp when we will need it.
        //cacheptr->resp_buf_head = cachemptr->resp_buf_head;
        //cacheptr->resp_len = cachemptr->resp_len;
        NSDL2_CACHE(vptr, cptr, "URL [%s] found in master table, cachemptr->resp_buf_head = %p, cachemptr->resp_len = %d", cachemptr->url, cachemptr->resp_buf_head, cachemptr->resp_len);
      } 
      else
      {
        // This should not happen. Log event.
        NS_EL_2_ATTR(EID_HTTP_CACHE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  get_request_string(cptr),
                                  (char*)__FUNCTION__,
                                 "Mode is master mode and url is embedded url. URL not found in master table.");

      }
    }
    
    cacheptr->cache_flags |= NS_CACHE_ENTRY_IN_CACHE;

    /*If in resp we got no-cache header
     *then revalidation is must*/
    if(cacheptr->cache_resp_hdr->CacheControlFlags.no_cache)
    {
      NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ: no-cache header                            \
                       found need to revalidate. url=%s", cacheptr->url);
      return(check_for_validators(cacheptr, cptr, vptr));
      //cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_VALIDATE);
      //return CACHE_NEED_TO_VALIDATE;
    }

    //First check for MUST REVALIDATION header
    // When the must-revalidate directive is present in a response received
    // by a cache, that cache MUST NOT use the entry after it becomes 
    // stale to respond to a subsequent request without first 
    // revalidating it with the origin server
    if(cacheptr->cache_resp_hdr->CacheControlFlags.must_revalidate)
    {
      NSDL2_CACHE(vptr, cptr, "must-revalidate header found check for freshness");
      if(cache_check_freshness(cptr, cacheptr, now) == 1)//response is fresh
      {
        //update the counters and return
        INC_CACHE_NUM_HITS_COUNTERS(vptr);
        cacheptr->cache_hits_cnt++;
        NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ: must-revalidate header found check for freshness and response is fresh");
        return CACHE_RESP_IS_FRESH;
      }
      else
      {
        NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ: must-revalidate header found and sending for revalidation");
        //cacheptr->cache_flags |= NS_CACHE_ENTRY_VALIDATE;
        return(check_for_validators(cacheptr, cptr, vptr));
        //cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_VALIDATE);
        //return CACHE_NEED_TO_VALIDATE;
      }
    }

    if(cache_check_freshness(cptr, cacheptr, now) == 1)//response is fresh
    {
      //update the counters and return
      INC_CACHE_NUM_HITS_COUNTERS(vptr);
      cacheptr->cache_hits_cnt++;
      NSDL2_CACHE(vptr, cptr, "CACHE ACTION REQ:: Url=%s Cache is fresh", cacheptr->url);
      return CACHE_RESP_IS_FRESH;
    }
    else //Response is not fresh. send request for validation
    {
      return(check_for_validators(cacheptr, cptr, vptr));
    }
  } 
}

/*This function is to set the flags and buffer
 * if we need to get resp from cache*/
void cache_set_resp_frm_cache ( connection *cptr, VUser *vptr)
{
  CacheTable_t* cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
   
  NSDL2_CACHE(vptr, cptr, "Method called.");
  
  if((runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 1) && (cptr->url_num->proto.http.type != MAIN_URL) && (cache_ptr->master_tbl_node != NULL))
  {
    //MT mode
    NSDL2_CACHE(vptr, cptr, "Mode is 1, its emd url and master node is not NULL");
    cptr->buf_head = cache_ptr->master_tbl_node->resp_buf_head;
    cptr->bytes = cache_ptr->master_tbl_node->resp_len;
    cptr->compression_type = cache_ptr->master_tbl_node->compression_type;
    cptr->body_offset = cache_ptr->master_tbl_node->body_offset;
  }
  else
  {
    cptr->buf_head = cache_ptr->resp_buf_head;
    cptr->bytes = cache_ptr->resp_len;
    cptr->compression_type = cache_ptr->compression_type;
    cptr->body_offset = cache_ptr->body_offset;
  }
  cache_ptr->cache_flags |= NS_CACHE_ENTRY_IN_CACHE; 
  cache_avgtime->cache_bytes_hit += cache_ptr->resp_len;
  SET_CACHE_BYTES_HIT_COUNTERS(vptr, cache_ptr->resp_len);
  NSDL2_CACHE(vptr, cptr, "cache_ptr = %p, resp_buf_head = %p, resp_len = %d, location_url = %s",
			   cache_ptr, cache_ptr->resp_buf_head, cache_ptr->resp_len, cache_ptr->location_url);
  NSDL2_CACHE(vptr, cptr, "Cache bytes hits = %lu", cache_avgtime->cache_bytes_hit);
 
  /* Save location url */
  cptr->location_url = (char*)cache_ptr->location_url;
  //cache_set_cacheEntryState(cptr, NS_CACHE_ENTRY_IN_CACHE);
}

void cache_update_entry_time(CacheTable_t *cache_ptr){

  NSDL3_CACHE(NULL, NULL, "Method called");

  //storing current time in cache_ts as response time
  cache_ptr->cache_ts = get_ns_start_time_in_secs() + 
                        get_ms_stamp()/1000 +
                        get_timezone_diff_with_gmt();

  //If Server Date_value = -1, put cache_ts in server date 
  if(cache_ptr->cache_resp_hdr->date_value < 0)
     cache_ptr->cache_resp_hdr->date_value = cache_ptr->cache_ts;
  
  //Calculate document age when received by cache
  cache_ptr->doc_age_when_received = calc_doc_age_when_received(cache_ptr);

  NSDL3_CACHE(NULL, NULL, "Cache_ts=%u, Date_value=%u, request_ts=%u, doc_age=%u",
                           cache_ptr->cache_ts, cache_ptr->cache_resp_hdr->date_value, 
                           cache_ptr->request_ts, cache_ptr->doc_age_when_received);
}

inline void validate_req_code (connection *cptr)
{
  NSDL3_HTTP(NULL, cptr, "Method Called, req_code = %d", cptr->req_code);
  CacheTable_t *cache_ptr;
  VUser *vptr = cptr->vptr;
  // There are three mechanism to know when the whole body is complete in the reply
  // in this priority
  // 1. Chunked encoding
  // 2. Using content-length header (If both 1 and 2 come, then use 1)
  // 3. Connection closed by server after the whole response is send by server (see handle_server_close())
  
  // For few status code, we do not get body (But server can send it)
  // We only get headers.
  // So set it to 0 so that if body does not come, we come after headers are over
  // If body comes, then it will read it as per content-length header
  //
  /* Manish: Here we need to check if response code is 200 and connect method is used 
   *  then we need to set content_length to zero (0) because on the time of connect handshake 
   *  body does not come with response*/
  // Adding status code 204 no content, it also may not get content length, if it will get then content length will be overwritten
  if(cptr->req_code == 304 || cptr->req_code == 100 || (cptr->req_code == 204) || (cptr->req_code == 101) || (cptr->req_code == 200 && (cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)))
  {
    NSDL3_HTTP(NULL, cptr, "Setting content length to 0 for req code = %d", cptr->req_code);
    cptr->content_length = 0;
  }

  // Code Added to Control the Referer Behaviour
  //save_referer will only build & save referer if all the necessary checks are passed
  save_referer(cptr); 

  if(NS_IF_CACHING_ENABLE_FOR_USER)
  {
    cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    // Case1 - Validation req was send
    if(cache_ptr->cache_flags & NS_CACHE_ENTRY_VALIDATE)
    {        
      //This is case when sent request for validation 
      //and got 200 OK response
      //so first we need to remove old values(headers, resp buf)from cache entry
      //then fill new values 

      if(cptr->req_code == 304)
      {
        NSDL3_CACHE(cptr->vptr, cptr, "Got 304 in Validation response, setting resp to take from cache");

        //cache_avgtime->cache_revalidation_success++; BUG
        INC_CACHE_REVALIDATION_NOT_MODIFIED_COUNTERS(vptr);
        INC_CACHE_NUM_HITS_COUNTERS(vptr); 
        cache_ptr->cache_hits_cnt++;

        cache_update_entry_time(cache_ptr);
      } 
      //    Case 1b - 200 or other good codes
      else //if(cptr->req_code == 200) // TODO: add other good status codes
      { 
        INC_CACHE_REVALIDATION_SUCC_COUNTERS(vptr);
        INC_CACHE_NUM_ENTERIES_REPLACED_COUNTERS(vptr);
        /*Free all data including Location URL*/
        //if(runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 0 || cptr->url_num->proto.http.type == MAIN_URL)
        if(cache_ptr->ihashMasterIdx == -1)
        { 
          NSDL2_CACHE(vptr, cptr, "Before deleting resp_len = %d, cache_bytes_used = %u", cache_ptr->resp_len, cache_avgtime->cache_bytes_used);
          cache_avgtime->cache_bytes_used -= cache_ptr->resp_len;
          DEC_CACHE_BYTES_USED_COUNTERS(vptr, cache_ptr->resp_len);
          //cache_free_data(cptr->cptr_data->cache_data, 0, 1, 0); // Free all except URL
          cache_free_data(cache_ptr, 0, 1, 0); // Free all except URL 
          NSDL2_CACHE(vptr, cptr, "After deleting resp_len = %d, cache_bytes_used = %u", cache_ptr->resp_len, cache_avgtime->cache_bytes_used); 
        }
        else   //For Mode = 1 && Embedded URL
        {
         //In Mode 1 we are freeing response when we are adding node in master table, 
         //hence freeing only header nodes
          cache_free_data(cache_ptr, 0, 0, 0); 
          NSDL2_CACHE(vptr, cptr, "Freeing headers in case of Mode=%d, url_type = %d", runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode, cptr->url_num->proto.http.type);
        }

        NSDL3_CACHE(cptr->vptr, cptr, "Got %d in Validation response, "
                                      "cache_revalidation_success count = %d cacheptr[%p]->request_ts=%d",
                                      cptr->req_code, cache_avgtime->cache_revalidation_success, cache_ptr, cache_ptr->request_ts);

        //Adding request time in cache entry, used for calculating age of the document
        cache_ptr->request_ts = get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt();

        cache_update_entry_time(cache_ptr);

        NSDL3_CACHE(NULL, cptr, "Cache_ts=%u, Date_value=%u, request_ts=%u, doc_age=%u cacheptr[%p]->request_ts=%d",
        cache_ptr->cache_ts, cache_ptr->cache_resp_hdr->date_value, cache_ptr->request_ts, cache_ptr->doc_age_when_received, cache_ptr, cache_ptr->request_ts);
         
        //Resetting the flags
        cache_ptr->cache_flags = cache_ptr->cache_flags & ~NS_CACHE_ENTRY_NEW;
      }
    }
    // Case2 - Cache entry expired and non validation req was send 
    // Case3 - Req is send for the first time
    // Dont do any thing we have already freed data in cache_check_if_url_cached () for expired cached
  }

  //For Auth - NTLM
  if ((cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC && cptr->proto_state == ST_AUTH_BASIC_RCVD) ||
     (cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST && cptr->proto_state == ST_AUTH_DIGEST_RCVD) ||
     (cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS && cptr->proto_state == ST_AUTH_KERBEROS_RCVD) ||
     (cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS && cptr->proto_state == ST_AUTH_KERBEROS_CONTINUE) ||
     (cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM && cptr->proto_state == ST_AUTH_NTLM_TYPE2_RCVD))
    auth_validate_handshake_complete(cptr);
}

static inline void copy_cptr_to_cacheptr(connection *cptr, CacheTable_t *cache_ptr)
{
      NSDL3_CACHE(NULL, cptr, "Method called, resp_len = %d, "
                              "buf_head=%s, body_offset = %d, "
                              "compression_type= %d",
                              cptr->bytes,
                              cptr->buf_head,
                              cptr->body_offset,
                              cptr->compression_type);
      cache_ptr->resp_buf_head = cptr->buf_head;
      cache_ptr->resp_len = cptr->bytes;
      cache_ptr->body_offset = cptr->body_offset;
      cache_ptr->compression_type = cptr->compression_type;
}

static void copy_url_frm_cahe_to_cptr (CacheTable_t *cache_ptr, connection *cptr)
{
  if(cache_ptr->url) {
    NSDL2_CACHE(NULL, cptr, "Allocating & copying url in cptr from"
                            " cache as cache Url is going to free.");

    NSDL2_CACHE(NULL, cptr, " cache_ptr->url_len = %d, strln length = %d", cache_ptr->url_len, strlen((char*)cache_ptr->url));
                            
    //if(cptr->url != NULL)
    if((cptr->flags & NS_CPTR_FLAGS_FREE_URL))
    {
      fprintf(stderr, "Error: cptr->url (%s) is not NULL. Length = %d\n", cptr->url, cptr->url_len);
      FREE_AND_MAKE_NOT_NULL_EX(cptr->url, cptr->url_len, "cptr->url", -1);
    }

    // If cache_ptr->url_len is 24 (23 + 1) and offset is 10, cptr->url_len will be 24 - 10 - 1 = 13
    // But we allocate one extra for NULL
    MY_MALLOC(cptr->url, cache_ptr->url_len - cache_ptr->url_offset + 1, "cptr->url", 1); 
     
    strcpy(cptr->url, (char *)cache_ptr->url + cache_ptr->url_offset);
    // cache_ptr->url_len has 1 extra for NULL
    // But cptr->url_len does not have 1 extra for NULL
    //Changes done for bug 760, if url len and offset is same then set cptr->url_len was getting -1 and resulting into coredump
    // Hence making its length 0, example in Kohls script URL was parameterize with empty string, http://127.0.0.1{search-var}
    if (cache_ptr->url_len == cache_ptr->url_offset)
      cptr->url_len = 0;
    else
      cptr->url_len = cache_ptr->url_len - cache_ptr->url_offset - 1; // Exact URL len without NULL termination
    NSDL2_CACHE(NULL, cptr, " cache_ptr->url_len = %d, cache_ptr->url_offset = %d, cptr->url_len = %hu\n", cache_ptr->url_len, cache_ptr->url_offset, cptr->url_len);
    cptr->flags |= NS_CPTR_FLAGS_FREE_URL; 
  }
}

/* Return 0 if response buff need not to free else 1*/
// TODO: Why we are calling this method is response was taken from cache
int cache_update_table_entries (connection *cptr)
{
  VUser *vptr = cptr->vptr;
  CacheTable_t *cache_ptr;
  CacheTable_t *cache_master_node = NULL;
  int free_emd = 1; //Default is 1 - Free emd url resp

  u_ns_ts_t lol_add_url_start;
  u_ns_4B_t lol_add_url_time;

  NSDL2_CACHE(vptr, cptr, "Method called");
  /*Check for caching.If enable then check 
 * 1. Need to clear the cache(Got no-store in resp) as we did not added this cache entry
 * 2. Need to delete the cache (Got no-store in resp) as this cache entry already added in cache 
 * 3. Update the cache if got 304 or 200 OK resp 
 * 4. */

  cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
  NSDL2_CACHE(vptr, cptr, "cache_ptr[%p]->cache_flags=%d", cache_ptr, cache_ptr->cache_flags);
  //In response we got no-store header, need to delte or clear the cache entry
  if(!(cache_ptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE)) { // Not cacheable
    if(!(cptr->url_num->proto.http.header_flags & NS_URL_KEEP_IN_CACHE)) { // Need to keep in cache for JS

      if(cache_ptr->cache_flags & NS_CACHE_ENTRY_IN_CACHE) {
        //Case - When no-store received in validation
        //This cache entry is from cache table, delete this entry
        if(cache_ptr->ihashMasterIdx != -1) // Entry is in master table
        {
          NSDL2_CACHE(vptr, cptr, "CACHE ACTION RESP: Url was EARLIER CACHABLE and was in master table. NOW NOT CACHEABLE. Deleting cache entry url=%s", cache_ptr->url); 
          // This method does not free resp buffer if there linked count > 1
          // otherwise free the response buffer (sending 0 as we will not free resp here, we will set free_emd = 1)
          if(cache_delete_url_frm_master_tbl(vptr, cache_ptr->http_method, cache_ptr->url, cache_ptr->url_len, &free_emd, 0) == NS_CACHE_ERROR) 
          {
            NSDL2_CACHE(vptr, cptr, "Mode is master mode and url is embedded url. URL not found in master table.");
            NS_EL_2_ATTR(EID_HTTP_CACHE, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  get_request_string(cptr),
                                  (char*)__FUNCTION__,
                                 "Mode is master mode and url is embedded url. URL not found in master table.");
          }
          else 
          {
            //case1:
            //Doing forece fully free_emd to 1 as we dont want this resp
            //In master table linked count is not 0 but we got No-Store so free this resp.
            //This is case when we sent req for validation and we got 200 with No-Store hdr
            //This is not 
            //free_emd = 1; 
            // Dont do -1 as we have as we will use this in deleting resp frm user table. 
            // In user table deltion use check this for mode.If its is -1 then it means we are in non MT mode
            // but in this case we are in Mt mode, so not making it -1
           // cache_ptr->ihashMasterIdx = -1; 
            cache_ptr->master_tbl_node = NULL;
          }
        }
        else
        {
          NSDL2_CACHE(vptr, cptr, "CACHE ACTION RESP: Url was EARLIER CACHABLE. NOW NOT CACHEABLE. Deleting cache entry url=%s", cache_ptr->url); 
        }

        // In cache table, we keep URL in fully qualified format (e.g. http://www.abc.com/index.html
        // So need to make copy from cache to cptr so thaat cptr  url is pointing to the url path only
        copy_url_frm_cahe_to_cptr (cache_ptr, cptr);

        //Doing this as we need resp buf in do_data_processing
        //Setting resp_buf_head = NULl to retain the resp_head in cptr & 
        //freeing entry from cache
        cache_ptr->resp_buf_head = NULL;
        /*We can use cache_free_data with (1,0,1) in cache_delete_url(), because cache_delete_url ()
         *get called from many places(ex from cache_destroy()).so need to make resp_buf_hed NULL 
         * before calling cache_delete_url*/
        cache_delete_url(vptr, cptr, cache_ptr->url, cache_ptr->url_len, cache_ptr->http_method);
        /*Doing after cache_delete url because in function we decrementing the cahe used memory.*/
        //cache_ptr->resp_len = 0;
      } 
      else
      {
        //This cache entry is not added in cache table
        //this happend bcoz this req was not cached and 
        //we created the cache entry.
        //Bcoz this is new entry and did not added in cache table, so no need to delete in MT.
        NSDL2_CACHE(vptr, cptr, "CACHE ACTION RESP: URL was not in cache and it not Cacheable Url=%s", cache_ptr->url);
        copy_url_frm_cahe_to_cptr(cache_ptr, cptr);
        //NOTE: For embd URLs, we have already setted free_emd to 1 (default)
        // DO NOT Free linked list of body
        cache_free_data(cptr->cptr_data->cache_data, 1, 0, 1);
        FREE_AND_MAKE_NULL_EX(cptr->cptr_data->cache_data, sizeof(CacheTable_t), "cptr->cptr_data->cache_data", -1);
      } 
      INC_CACHE_NUM_ENTERIES_NON_CACHEABLE_COUNTERS(vptr);
      //If ret is 1 the free otherwise dont free the emd buffer
      //because in master may be possible that we will not free the url
      //we just decrement the linked counter 
      if(free_emd) // This means master table entry is deleted (note: resp buffer is not freed)
        return 1;  // Free ==> This return value is used for freeing emb response 
      else
        return 0;
    } else { // Set bit to indicate that entry was not cacheable but keeping in cache for JS engine
      cache_ptr->cache_flags |= NS_CACHE_ENTRY_NOT_FOR_CACHE;
    }
  }

  // //no-store else
  // At this point entry is to be kept in the cache
  // Either it is cacheable or forced due to NS_URL_KEEP_IN_CACHE bit for JS engine
  //Got response, now need add the cache entry as it was first request
  //and this entry was not cached.
  //This is case when caheche entry is expired.
  //We have already freed data pointers now new data
  //should point to cache resp data
  /* In POST Callback of Embedded Urls we save response in
   * cptr->buf_head & for Main we do it in vptr->buf_head*/
  //Checking for 304 bcoz if we get 304 we will just update the
  //timing nothing else

  if(cptr->req_code != 304) {
    if(cptr->url_num->proto.http.type == EMBEDDED_URL) {
      if(cptr->url_num->post_url_func_ptr != NULL || 
         cptr->url_num->proto.http.header_flags & NS_URL_KEEP_IN_CACHE){

        NSDL2_CACHE(vptr, NULL, "Copying resp_buf_head from cptr to "
                                "cache as embedded URL. post_url_func_ptr = %p, "
                                "cptr->flags = 0x%x", 
                                cptr->url_num->post_url_func_ptr,
                                cptr->flags);
        copy_cptr_to_cacheptr(cptr, cache_ptr);
        //Making cptr->buf_head to NULL as we have copied it to cachetable & dont require it in cptr for further processing
        cptr->buf_head = NULL;
      }else{
         //printf("");   TODO
         //NSDL2_CACHE(vptr, NULL, "Copying resp_buf_head from cptr to cache.");
         //printf("Copying resp_buf_head from cptr to cache.");
      }
    } else { // Main URL
      NSDL2_CACHE(vptr, NULL, "Copying resp_buf_head from cptr to cache."
                              "cptr: resp_buf_head = %p, resp_len = %d,"
                              " body_offset = %d", 
                              cptr->buf_head, cptr->bytes,
                              cptr->body_offset);

      copy_cptr_to_cacheptr(cptr, cache_ptr);
    }
  }

  if(cptr->req_code != 304) {
    //If mode 1 is given in cache, then first add node in master table
    if((runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 1) && (cptr->url_num->proto.http.type != MAIN_URL))
    {
      NSDL3_CACHE(NULL, cptr, "Mode 1 is found, going to add url in master table");
      cache_master_node = cache_add_url_in_master_tbl(cptr, vptr, cache_ptr->url, 
                         cache_ptr->url_len, &cache_ptr->ihashMasterIdx);
      if(NULL == cache_master_node)
      {
        NSDL3_CACHE(NULL, cptr, "Error occured in adding URL = %s in master table");
        //TODO: wat we will do???
      }
      else
      {
        cache_ptr->master_tbl_node = cache_master_node;
        NSDL3_CACHE(NULL, cptr, "Master node = %p added in user table node = %p", cache_master_node, cache_ptr);
      }
    }
  }

  if(cache_ptr->cache_flags & NS_CACHE_ENTRY_NEW) {

    NSDL2_CACHE(vptr, cptr, "CACHE ACTION RESP: Url is not from cache,"
                            " adding url to cache list. Url=%s", 
                            cache_ptr->url);

    //Doing from vptr, as before this function call 
    //we have called copy_cptr_to_vptr() function which makes cptr to NULL
   
    cache_update_entry_time(cache_ptr);

    lol_add_url_start = get_ms_stamp();

    NSDL3_CACHE(NULL, cptr, "Method = %d", cache_ptr->http_method);
    cache_add_url(cptr, vptr, cache_ptr->url);
     
    lol_add_url_time = (u_ns_4B_t)(get_ms_stamp() - lol_add_url_start);

    INC_CACHE_ADD_URL_TIME_TOT_COUNTERS(vptr, lol_add_url_time);

    if(lol_add_url_time < cache_avgtime->cache_add_url_time_min)
      cache_avgtime->cache_add_url_time_min = lol_add_url_time;
    if(lol_add_url_time > cache_avgtime->cache_add_url_time_max)
      cache_avgtime->cache_add_url_time_max = lol_add_url_time;

     NSDL1_CACHE(vptr, NULL, "Time taken to add URL in Cache = %'.3f seconds, min = %'.3f, max = %'.3f, Total time =  %'.3f", (double )lol_add_url_time/1000.0, (double )cache_avgtime->cache_add_url_time_min/1000.0, (double )cache_avgtime->cache_add_url_time_max/1000.0, (double)cache_avgtime->cache_add_url_time_total/1000.0);

    //Resetting the flags
    cache_ptr->cache_flags = cache_ptr->cache_flags & ~NS_CACHE_ENTRY_NEW;
    INC_CACHE_NUM_ENTERIES_CACHEABLE_COUNTERS(vptr);
  } else {
    //Case - When the earlier request gets expired and there is no validator present
    //We are sending the new request
    //Putting cache time & calculating age of the document
    NSDL2_CACHE(vptr, cptr, "CACHE ACTION RESP: Cacheable Response received"
                            " after revalidation. Url=%s",
                            cache_ptr->url);
    NSDL2_CACHE(vptr, cptr, "NS_CACHE_ENTRY_NEW not New");

    cache_update_entry_time(cache_ptr);

    //Add resp_len
    //Adding response len in case of mode 0 coz in mode 0
    //we save resp in user table and we dont add cache node in table in case of expired.
    //So, need to increment used resp len here.
    //For mode 1, we have already done above.

    //Bug#2921 cache_bytes_used was wrongly incremented every time when resp code 304 received in response to
    //revalidation request of MAIN url
    //Added check to increment cache_bytes_used only resp code is not 304
    if((runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 0 || cptr->url_num->proto.http.type == MAIN_URL) 
           && cptr->req_code != 304) {
      NSDL2_CACHE(vptr, cptr, "Before adding resp_len = %d, cache_bytes_used = %u", cache_ptr->resp_len, cache_avgtime->cache_bytes_used);
      cache_avgtime->cache_bytes_used += cache_ptr->resp_len;
      INC_CACHE_BYTES_USED_COUNTERS(vptr, cache_ptr->resp_len);
      NSDL2_CACHE(vptr, cptr, "After adding resp_len = %d, cache_bytes_used = %u", cache_ptr->resp_len, cache_avgtime->cache_bytes_used);
    }

   
    NSDL3_CACHE(NULL, cptr, "Cache_ts=%u, Date_value=%u, request_ts=%u, doc_age=%u",
                             cache_ptr->cache_ts, cache_ptr->cache_resp_hdr->date_value, 
                             cache_ptr->request_ts, cache_ptr->doc_age_when_received);
  }
  //Doing NULL as in connection reuse case we will not 
  //close the connection so making NULL for second request
  cptr->cptr_data->cache_data = NULL;

  // Note: We are not making cptr or vptr buf head null as there is not need
  //
  return 0; // Return 0 to indicate caller not to free response buffer
}


void free_cache_for_user(VUser *vptr)
{
  NSDL2_CACHE(vptr, NULL, "Method called");
  cache_destroy(vptr);
  vptr->flags = vptr->flags & ~NS_RESP_FRM_CACHING;
  /* Free main page url  as we dont need main page url as its a END of page*/
  FREE_AND_MAKE_NULL_EX(vptr->httpData->page_main_url, vptr->httpData->page_main_url_len, "main page url", -1);
}
