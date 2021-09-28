/**********************************************************************
 * File Name            : ns_http_cache_hdr.c
 * Author(s)            : Shilpa Sethi
 * Date                 : 25 November 2010
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Callback functions for processing cache headers
 *                        and storing them in cache resposne header structure
 * Modification History :
 *		<Author(s)>, <Date>, <Change Description/Location>
 **********************************************************************/
#define _GNU_SOURCE
#include <string.h>
#include "ns_cache_include.h"
// shchange
#include "ns_http_cache_table.h"

#define EXTRA_BUFFER_REQ(comma_cnt) ((comma_cnt + 1) * 2)


  
/*************************************************************************************************
Description       :Checks if caching is enabled for the user or not and
                   url not marked for no-store 
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : NS_CACHE_SUCCESS if user is enabled for cache and user not marked for no-store
                    NS_CACHE_ERROR if
			1. user is not enabled for cache
                        2. user is enabled for cache but marked for no-store
*************************************************************************************************/
void cache_ability_on_resp_code(connection *cptr)
{
  //VUser *vptr = cptr->vptr;
  CacheTable_t *cacheptr;
  NSDL2_CACHE(NULL, cptr, "Method called ");

  //Check if user is enabled for cache
//  if(vptr->flags & NS_CACHING_ON)
  { //Make the bit 1
    cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    cacheptr->cache_flags |= NS_CACHE_ENTRY_IS_CACHABLE;
  }
#if 0
  else
  { //Make the bit 0
    NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Caching Disabled for User", cptr->url);
    return;
  }
#endif

  //Check the cachable response codes
 //cptr->req_code != 206 && cptr->req_code != 203 && cptr->req_code != 300 && cptr->req_code != 307 && cptr->req_code != 410 
  if(cptr->req_code != 200 &&   cptr->req_code != 301 && 
              cptr->req_code != 302 && cptr->req_code != 304)
  { //Make the bit 0
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
    NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Not a Cacheable Response Code=%d", cacheptr->url, cptr->req_code);
    return;
  }

  //Checking cachability on HTTP method
  // TODO: Check how we keep invalid method name. may be kept as POST
  if(!(cptr->url_num->proto.http.http_method == HTTP_METHOD_GET || cptr->url_num->proto.http.http_method == HTTP_METHOD_HEAD))
  {
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
    // TODO: log method name, not method numeric value
    NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Not a Cacheable Method=%d", 
                           cacheptr->url, cptr->url_num->proto.http.http_method);
    return;
  }

  // If client freshess enabled and no-store, we clear NS_CACHE_ENTRY_IS_CACHABLE bit
  if(cptr->url_num->proto.http.cache_req_hdr.CacheControlFlags.no_store)
  {
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
    NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Due to client freshness constraints - HTTP request header has no-store"                            , cacheptr->url);
    return;
  }
    
  if(cptr->req_code == 304)
  {
     cacheptr->cache_flags |= NS_304_FOR_ETAG;
  }else{
    //Dont reset in case of 304
    //bcoz in case of 304 we must have validators 
    //So, dont reset the bits
    //Set the header bits to 0
    cacheptr->cache_flags &= ~NS_CACHEABLE_HEADER_PRESENT;
    cacheptr->cache_flags &= ~NS_VALIDATOR_PRESENT;
  }


  //#Shilpa 16Feb2011 Bug#2037
  //Implementing Client Freshness Constraint
  //memcpy(cacheptr->,cptr->url_num->proto.http.CacheReqHdr,sizeof(CacheRequestHeader));
}


/*************************************************************************************************
Description       : 1. Fetch vptr from cptr 
                    2. Checks if caching is enabled for the user or not and
                       url not marked for no-store 
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : NS_CACHE_SUCCESS if user is enabled for cache and user not marked for no-store
                    ns_caCHE_ERROR if
			1. user is not enabled for cache
                        2. user is enabled for cache but marked for no-store
*************************************************************************************************/
// This MUST be used only for processing of cache related headers and
// must be called once at the start of the callback method

void cache_ability_on_caching_headers(connection *cptr)
{
  NSDL2_CACHE(NULL, cptr, "Method called ");
  CacheTable_t *cacheptr;
  CacheResponseHeader *crh;
  VUser *vptr = cptr->vptr;
  CacheRequestHeader *cache_req_hdr;
  int expires_value, current_time;

#ifdef NS_DEBUG_ON
  char cacheable[5120];
  char expires[50];
  cacheable[0] = 0;
  expires[0] = 0;
#endif

  //Check if user is enabled for cache
  if(vptr->flags & NS_CACHING_ON)
  {
    cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    crh = cacheptr->cache_resp_hdr;
    // cache_req_hdr = cacheptr->cache_req_hdr;
    cache_req_hdr = &(cptr->url_num->proto.http.cache_req_hdr);
  }
  else
  { 
    //NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=%s - Caching Disabled for User", cptr->url);
    return;
  }

  //NSDL2_CACHE(NULL, cptr, "Checking Cacheability for url=%s", cacheptr->url);
  //Check for the cacheability bit first
  if(!(cacheptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE))
  {
      //NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=%s - Url already not cacheable", cacheptr->url);
      return;
  }
 
  // Note - We are not checking if cleint fressness is enabled or not
  //        as cache_req_hdr will have default values and will not meet these conditions 
  //If no-cache without any validator exists, not caching it
  if(((cache_req_hdr->CacheControlFlags.no_cache == 1) || (crh->CacheControlFlags.no_cache == 1)) && !(cacheptr->cache_flags & NS_VALIDATOR_PRESENT))
  {
     cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
#ifdef NS_DEBUG_ON
    if(cache_req_hdr->CacheControlFlags.no_cache == 1)
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] Due to client freshness constraints - HTTP request header has no-chche and No Validators Present", cacheptr->url); 
    else
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] No-Cache Header Present & No Validators Present", cacheptr->url); 
#endif
     return;
  }

  //Max_age with value=0 needs to be revalidated every time. 
  //If no revalidator present with max-age=0, we are not caching it
  if(((cache_req_hdr->max_age == 0) || (crh->max_age == 0)) && !(cacheptr->cache_flags & NS_VALIDATOR_PRESENT)) 
  {
     cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
#ifdef NS_DEBUG_ON
    if(cache_req_hdr->CacheControlFlags.no_cache == 1)
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] Due to client freshness constraints - HTTP request header has Max-Age=0 and No Validators Present", cacheptr->url); 
    else
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Max-Age=0 and No Validators Present", cacheptr->url); 
#endif
     return;
  }

  //If Max-Age is not present & Expires is past date , not caching it
  if(crh->expires != -1 && ((crh->max_age < 0) || cache_req_hdr->max_age < 0))
  {
     if(crh->date_value != -1)
     {
        expires_value = crh->expires - crh->date_value;
       }
     else
     {
        current_time = (get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt());
        expires_value = cacheptr->cache_resp_hdr->expires - current_time;
        NSDL2_CACHE(NULL, cptr, "current_time=%u, expires_value=%u, expires=%u",
                                current_time, cacheptr->cache_resp_hdr->expires, expires_value);
     }
     if(expires_value <= 0)
     {
        cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
        NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Expires is a past date and Max-Age is not present or -ve. Date diff=%u", cacheptr->url, expires_value); 
        return;
     }
#ifdef NS_DEBUG_ON
	 sprintf(expires, "Expires is a future date=%d\n", crh->expires);
#endif
  }

  if(cptr->req_code != 304)
  {
    //If No Cacheable Header Present & No Validator present, dont cache it
    if(!((cacheptr->cache_flags & NS_CACHEABLE_HEADER_PRESENT) || (cacheptr->cache_flags & NS_VALIDATOR_PRESENT)))
    {
      cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=[%s] - Not Sufficient Cacheable Headers Present", cacheptr->url); 
      return;
    }
  }

#ifdef NS_DEBUG_ON
  int i=3;
  sprintf(cacheable, "1. Caching enabled for User 2. Response Code=%d 3. Cacheable Method=", cptr->req_code);

  if (cptr->url_num->proto.http.http_method == HTTP_METHOD_GET)
	  sprintf(cacheable, "%sGET", cacheable);
  else if(cptr->url_num->proto.http.http_method == HTTP_METHOD_POST)
	  sprintf(cacheable, "%sPOST", cacheable);
  else if(cptr->url_num->proto.http.http_method == HTTP_METHOD_PUT)
	  sprintf(cacheable, "%sPUT", cacheable);
  else if(cptr->url_num->proto.http.http_method == HTTP_METHOD_HEAD)
      sprintf(cacheable, "%sHEAD", cacheable);
  else 
      sprintf(cacheable, "%sUnknownMethod", cacheable);

  if(crh->max_age >= 0) sprintf(cacheable, "%s  %d. Max-Age=%d", cacheable, ++i, crh->max_age);
  if (expires[0] != 0) sprintf(cacheable, "%s  %d. %s", cacheable, ++i, expires);
  if (crh->last_modified_len > 0) sprintf(cacheable, "%s  %d. Last Modified Date=%s", cacheable, ++i, crh->last_modified);
  if(crh->etag_len > 0) sprintf(cacheable, "%s  %d. ETag=%s", cacheable, ++i, crh->etag);

  NSDL2_CACHE(NULL, cptr, "CACHEABLE url=[%s] as %s", cacheptr->url, cacheable);
#endif
  //NSDL2_CACHE(NULL, cptr, "CACHEABLE url=%s", cacheptr->url);
}

/*************************************************************************************************
Description       : Set Cache Entry State
                    Reset other bits
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
       cache_State : NS_CACHE_ENTRY_NEW, NS_CACHE_ENTRY_VALIDATE, NS_CACHE_ENTRY_EXPIRED

Output Parameters : None

Return Value      : None
*************************************************************************************************/

void cache_set_cacheEntryState(connection *cptr, CacheState_et cache_State)
{
  NSDL2_CACHE(NULL, cptr, "Method called ");
  CacheTable_t *cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;

  if(cache_State == NS_CACHE_ENTRY_NEW)
  {
    cacheptr->cache_flags |= NS_CACHE_ENTRY_NEW;
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_VALIDATE;
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_EXPIRED;
    NSDL2_CACHE(NULL, cptr, "NS_CACHE_ENTRY_NEW set");
  }
  else if(cache_State == NS_CACHE_ENTRY_VALIDATE)
  {
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_NEW;
    cacheptr->cache_flags |= NS_CACHE_ENTRY_VALIDATE;
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_EXPIRED;
    NSDL2_CACHE(NULL, cptr, "NS_CACHE_ENTRY_VALIDATE set");
  }
  else if(cache_State == NS_CACHE_ENTRY_EXPIRED)
  {
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_NEW;
    cacheptr->cache_flags &= ~NS_CACHE_ENTRY_VALIDATE;
    cacheptr->cache_flags |= NS_CACHE_ENTRY_EXPIRED;
    NSDL2_CACHE(NULL, cptr, "NS_CACHE_ENTRY_EXPIRED set");
  }

  NSDL2_CACHE(NULL, cptr, "Exiting Method");
}


/*************************************************************************************************
Description       : This method is called to free the buffer used in partial read of cache header,
                    if Cache header was not received in one read 

Input Parameters  :
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/

void cache_free_partial_hdr(connection *cptr)
{
  CacheResponseHeader *crh;

  NSDL2_CACHE(NULL, cptr, "Method Called ");
  crh = ((CacheTable_t *)(cptr->cptr_data->cache_data))->cache_resp_hdr;

  FREE_AND_MAKE_NULL_EX(crh->partial_hdr, crh->hdr_len, "cptr->cptr_data->cache_data->cache_resp_hdr->partial_hdr", -1);
  crh->hdr_len = 0; // is used to keep length of cache header
  NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : This method is used in case cache header is partially read
                    
Input Parameters  :
      cache_buffer: Pointer in the received buffer for the cache without NULL termination
            length: Number of bytes read in latest read (without NULL)
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/

inline void cache_save_partial_headers(char* cache_buffer, connection *cptr, int length)
{
  CacheResponseHeader *crh;

  NSDL2_CACHE(NULL, cptr, "Method Called");
  crh = ((CacheTable_t *)(cptr->cptr_data->cache_data))->cache_resp_hdr;

  int prev_length = crh->hdr_len;
  crh->hdr_len += length;
  
  //Adding 1 for NULL first time only
  if(0 == prev_length)
       ++crh->hdr_len;
   
  MY_REALLOC_EX(crh->partial_hdr, crh->hdr_len, prev_length, "Set Cache Header Line", -1);
  memset(crh->partial_hdr + prev_length, 0, length); 

  strncpy((char *)crh->partial_hdr + prev_length, cache_buffer, length);
  NSDL2_CACHE(NULL, cptr, "Exiting Method = %s", crh->partial_hdr);
}

/*************************************************************************************** 
Description       : This method is used to fetch max-age/max-stale/min-fresh value from
                    cache-control directive
Assumption        : header passed is NULL terminated in place of \r
Input Parameters  :
  start_ptr: Points to the position where directive starts (e.g. max-age=30, it will point to m)
  name_len: Lenght of the directive (e.g. for max-age=, it is 8)
  left_len: Lenght of the header value from start_ptr till end (NULL)
Output Parameters : None
Return Value      : Value of max-age/max-stale/min-fresh
***************************************************************************************/

static cache_age_t cache_get_age_value(char *start_ptr, int name_len, int left_len)
{
char *commapos;
char comma=44;
char age_value[10];
int age_num;
int len;

  NSDL2_CACHE(NULL, NULL, "Method called. start_ptr = %s, name_len = %d, left_len = %d", start_ptr, name_len, left_len);

  commapos = strchr(start_ptr, comma);
  //If comma not found after max-age/max-stale/min-fresh
  if(NULL == commapos)
    len =  left_len - name_len;
  else //if comma found after max-age/max-stale/min-fresh
    len = (commapos - start_ptr) - name_len;

  if (len > 9) len=9;
  strncpy(age_value, start_ptr + name_len, len);
  age_value[len] = '\0';


  commapos = strchr(start_ptr, comma);
  //If comma not found after max-age/max-stale/min-fresh
  if(NULL == commapos)
    len =  left_len - name_len;
  else //if comma found after max-age/max-stale/min-fresh
    len = (commapos - start_ptr) - name_len;

  if (len > 9) len=9;
  strncpy(age_value, start_ptr + name_len, len);

  age_num = atoi(age_value);
  NSDL2_CACHE(NULL, NULL, "Age value = %d", age_num); 
  return age_num;
}

/*************************************************************************************************
Description       : This method is called when cache-control header is received
                    USed to parse & save the cache-control header values received
                    
Input Parameters  :
 cCacheHeaderValue: The cache header value read with NULL terminated
  cache_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response
     req_resp_flag: Flag to indicate wether function called for request header parsing or response header parsing
                    0 - request, 1 - response

Output Parameters : None

Return Value      : None
*************************************************************************************************/

void cache_parse_cache_control(connection *cptr, char *cCacheHeaderValue, int iCacheHeaderLength) 
{
   char *pos, *commapos;
   char max_age[10];
   char comma=44;
   int max_age_len = 8;   // 8 characters for "max-age="
   CacheResponseHeader *crh;
   short cachecontrol_flag=0;
   int len;
   CacheTable_t *cacheptr;

     cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
     NSDL2_CACHE(NULL, NULL, "Method Called.  Parsing Response Cache-Control Header. cCacheHeaderValue=[%s],    \
                         iCacheHeaderLength=[%d]", cCacheHeaderValue, iCacheHeaderLength); 
     crh = cacheptr->cache_resp_hdr;

   //Do case insensitive check for "no-cache"
   // TODO: How not to match super set like no-cache123
   // can check for next character of no-cache to be [(,)(/r)(/n)(32)]
   if(strcasestr(cCacheHeaderValue,"no-cache") != NULL)
   {
      crh->CacheControlFlags.no_cache = 1;
      cachecontrol_flag = 1;
      //cacheptr->cache_resp_hdr->cacheable_hdr_present = 1;
      NSDL2_CACHE(NULL, NULL, "Cache Control 'NO-CACHE' received");
   }
  
   //Do case insensitive check for "no-store"
   if(strcasestr(cCacheHeaderValue,"no-store") != NULL)
   {
      crh->CacheControlFlags.no_store = 1;
      cachecontrol_flag = 1;
      cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;//Make the bit 0
      NSDL2_CACHE(NULL, NULL, "NOT CACHEABLE url=%s - Cache Control 'NO-STORE' received", cacheptr->url);
   }

   //Do case insensitive check for "must-revalidate"
   if(strcasestr(cCacheHeaderValue,"must-revalidate") != NULL)
   {
      crh->CacheControlFlags.must_revalidate = 1;
      cachecontrol_flag = 1;
      //cacheptr->cache_resp_hdr->cacheable_hdr_present = 1;
      NSDL2_CACHE(NULL, NULL, "Cache Control 'MUST-REVALIDATE' received");
   }

   //Do case insensitive check for "max-age"
   if(strcasestr(cCacheHeaderValue,"max-age") != NULL)
   {
      NSDL2_CACHE(NULL, NULL, "Got max-age");
      //Fetching max-age value
      pos = strcasestr(cCacheHeaderValue,"max-age");
      NSDL2_CACHE(NULL, NULL, "Max age received at pos = %s in cache-control", pos);
      commapos = strchr(pos, comma);

      //If comma not found after max-age  
      if(NULL == commapos)
         len =  iCacheHeaderLength - (pos - cCacheHeaderValue) - max_age_len;
      else //if comma found after max-age
         len = commapos - pos - max_age_len;
      //TODO SS: Take the right-most bits
      if(len > 9)
        len = 9;

      strncpy(max_age, pos + max_age_len, len);
      crh->max_age = atoi(max_age);

      //TODO: CacheRequestHeader
      if(crh->max_age >= 0) 
      {
         cacheptr->cache_flags |= NS_CACHEABLE_HEADER_PRESENT;
      }
      else
      {
         cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;//Make the bit 0
         NSDL2_CACHE(NULL, NULL, "NOT CACHEABLE url=%s- Max-Age < 0, Max-age=%u", cacheptr->url, crh->max_age); 
      }
   
      NSDL2_CACHE(NULL, NULL, "Cache Control 'MAX_AGE=%d'", crh->max_age);
      cachecontrol_flag = 1;
   }

   // TODO: Do we need to parse s-maxage or not
   
  if (!cachecontrol_flag)
  {
    NSDL2_CACHE(NULL, NULL, "Unhandled Cache-Control Header Received ---> '%s'", cCacheHeaderValue);
  }
  NSDL2_CACHE(NULL, NULL, "Exiting Method");
}


/*

Return:
  0 - Header not parsed
  1 - Header is parsed
*/
//#Shilpa 16Feb2011 Bug#2037 - Implementing Client Freshness Constraint

int cache_parse_req_cache_control(char *cCacheHeaderValue, CacheRequestHeader *cache_req_hdr, int sess_idx, int line_num, char *file_name)
{
char *pos; 
short cachecontrol_flag=0;
char *r_index;

  NSDL2_CACHE(NULL, NULL, "Method Called, cCacheHeaderValue = %s", cCacheHeaderValue);
  if ((strcasestr(cCacheHeaderValue, "Cache-Control") != NULL) && (strcasestr(cCacheHeaderValue, "Pragma") != NULL))
    return 0;

  NSDL2_CACHE(NULL, NULL, "Parsing Response Cache-Control Header. cCacheHeaderValue=[%s]", cCacheHeaderValue);
  #ifndef CAV_MAIN
  if(!runProfTable[sess_idx].gset.client_freshness_constraint) // TODO - Fix it
  {
    NSDL2_HTTP(NULL, NULL, "HTTP Caching client freshness is disabled. So not processing Cachce-Control header");
    return 0;
  }
  #else
  if(!group_default_settings->client_freshness_constraint)
  {
    NSDL2_HTTP(NULL, NULL, "HTTP Caching client freshness is disabled. So not processing Cachce-Control header");
    return 0;
  }
  #endif

  NSDL2_CACHE(NULL, NULL, "Processing Cachce-Control header, cheHeaderValue = %s", cCacheHeaderValue);

  // we are replacing '\r' by ''\0',because we need null terminated header to parse them, at the end we are reversing it back 
  r_index = index(cCacheHeaderValue, '\r');
  if(r_index != NULL)
    *r_index = '\0';

  //Do case insensitive check for "no-cache"
  // TODO: How not to match super set like no-cache123
  // can check for next character of no-cache to be [(,)(/r)(/n)(32)]
  if(strcasestr(cCacheHeaderValue, "no-cache") != NULL)
  {
    cache_req_hdr->CacheControlFlags.no_cache = 1;
    cachecontrol_flag = 1;
    NSDL2_CACHE(NULL, NULL, "Cache Control 'NO-CACHE' received");
  }

  // Do case insensitive check for all checks
  if(strcasestr(cCacheHeaderValue, "no-store") != NULL)
  {
    cache_req_hdr->CacheControlFlags.no_store = 1;
    cachecontrol_flag = 1;
    NSDL2_CACHE(NULL, NULL, "Cache Control 'NO-STORE' received");
  }
  if(strcasestr(cCacheHeaderValue,"must-revalidate") != NULL)
  {
    cache_req_hdr->CacheControlFlags.must_revalidate = 1;
    cachecontrol_flag = 1;
    NSDL2_CACHE(NULL, NULL, "Cache Control 'MUST-REVALIDATE' received");
  }
  if(strcasestr(cCacheHeaderValue, "max-age=") != NULL)
  {
    cachecontrol_flag = 1;
    pos = strcasestr(cCacheHeaderValue, "max-age=");
    cache_req_hdr->max_age = cache_get_age_value(pos, 8, strlen(pos)); // 8 includes = in max-age=
    NSDL2_CACHE(NULL, NULL, "Cache Control 'MAX_AGE = %d'", cache_req_hdr->max_age);
  }

  if(strcasestr(cCacheHeaderValue, "max-stale") != NULL)
  {
    if(strcasestr(cCacheHeaderValue, "max-stale=") != NULL)
    {
       cachecontrol_flag = 1;
       pos = strcasestr(cCacheHeaderValue, "max-stale=");
       cache_req_hdr->max_stale = cache_get_age_value(pos, 10, strlen(pos)); // 8 includes = in max-stale=
       NSDL2_CACHE(NULL, NULL, "Cache Control 'MAX_STALE = %d'", cache_req_hdr->max_stale);
    }
    else
    { 
      cachecontrol_flag = 1;

      cache_req_hdr->max_stale = CACHE_ALWAYS_SERVE_FROM_CACHE;
      NSDL2_CACHE(NULL, NULL, "Cache Control 'MAX_STALE = Always serve from cache");
    }
  }
  if(strcasestr(cCacheHeaderValue, "min-fresh=") != NULL)
  {
    cachecontrol_flag = 1;
    pos = strcasestr(cCacheHeaderValue, "min-fresh=");
    cache_req_hdr->min_fresh = cache_get_age_value(pos, 10, strlen(pos)); // 8 includes = in min-fresh=
    NSDL2_CACHE(NULL, NULL, "Cache Control 'MIN_FRESH = %d'", cache_req_hdr->min_fresh);
  }
  // If we are getting max-stale and min-fresh togather, as per RFC to be on safer side we give priority to 
  //min-fresh and reset the value of max-stale to -1.
  if((cache_req_hdr->max_stale != -1) && (cache_req_hdr->min_fresh != -1))
  {
    NSDL2_CACHE(NULL, NULL, "got max_stale and min_fresh both in the request. max_stale = %d, min_fresh = %d", 
                                              cache_req_hdr->max_stale, cache_req_hdr->min_fresh);
    cache_req_hdr->max_stale = -1;
    
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
        "max-stale and min-fresh both are coming in script %s/%s at line no %d.", get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), file_name, line_num);
        //Previously taking with only script name
        //Previously taking with only script name
        //"max-stale and min-fresh both are coming in script %s/%s at line no %d.", get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), file_name, line_num);

  }
  if (!cachecontrol_flag)
  {
    NSDL2_CACHE(NULL, NULL, "Unhandled Cache-Control Header Received ---> '%s'", cCacheHeaderValue);
  }
  if(r_index != NULL)
    *r_index = '\r';

  return 1;
}

/*************************************************************************************************
Description       : This method is called when etag header is received
                    Used to parse & save the etags received
                    
Input Parameters  :
 cCacheHeaderValue: The cache header value read
                    cache_value buffer contains – cache name, domain and path for cache(with \r\n) and last \r\n.
  cache_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/
static void cache_parse_etag(connection *cptr, char *cCacheHeaderValue, int iCacheHeaderLength )
{

   CacheResponseHeader *crh;
   char comma_delim[] = ",";
   char *entitytags= NULL;
   int taglen, comma_cnt; 
   char *p;
   
   
   NSDL2_CACHE(NULL, cptr, "Method Called. cCacheHeaderValue=[%s], iCacheHeaderLength=[%d]", 
						cCacheHeaderValue, iCacheHeaderLength); 
   
   CacheTable_t *cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
   crh = cacheptr->cache_resp_hdr;

   p = cCacheHeaderValue; 
   comma_cnt = 0;

   while(*p != '\0')
   {
      if(*p == ',')
          comma_cnt++;
      p++;
   }
   
   if(cacheptr->cache_flags & NS_304_FOR_ETAG)
   {
      //Free previously stored ETag
      // TODO: review if crh->etag_len is correct or not
      FREE_AND_MAKE_NULL_EX(crh->etag, crh->etag_len, "cte->cache_resp_hdr->etag", -1);
      crh->etag_len = 0;
      //Reset the flag in case 2nd Etag is received in response 304, 
      //as we dont have to free previous etag in that case
      cacheptr->cache_flags &= ~NS_304_FOR_ETAG;
   }
        
   //Allocating extra buffer in case etags received without quotes
   //extra_buffer_req = (comma_cnt + 1) * 2; 
   if(crh->etag == NULL)
   {
       MY_MALLOC(crh->etag, iCacheHeaderLength + EXTRA_BUFFER_REQ(comma_cnt), "HTTP CACHE HEADER - ETAG", -1);
       memset(crh->etag, 0, iCacheHeaderLength);
   }
   else
   {
       // TODO: Review if crh->etag_len is correct as old size of buffer
       MY_REALLOC_EX(crh->etag, iCacheHeaderLength + crh->etag_len + EXTRA_BUFFER_REQ(comma_cnt), crh->etag_len,
                                   "HTTP CACHE HEADER - ETAG", -1);
       memset(crh->etag + crh->etag_len, 0, iCacheHeaderLength);
   }

   //Tokenize the string on comma
   entitytags = strtok( cCacheHeaderValue, comma_delim);
   while( entitytags != NULL) 
   {
       cacheptr->cache_flags |= NS_VALIDATOR_PRESENT;

       //If not the first token
       //TODO SS: crh->etag_len > 0
       if(strlen(crh->etag) != 0)
       {
           strcat(crh->etag, ",");
           ++crh->etag_len;
       }

       //Removing white spaces & tabs from both left & right of the etag
       CLEAR_WHITE_SPACE(entitytags);
       CLEAR_WHITE_SPACE_FROM_END(entitytags);
       
       taglen = strlen(entitytags);

       //Check for "W/"
       if((entitytags[0] == 'W' || entitytags[0] == 'w')&& entitytags[1] == 47) 
       {
          strcat(crh->etag, "W/");
          crh->etag_len += 2;
          taglen -= 2;
          entitytags += 2;
       }

       //Check for " at the start of the etag, if not present put it
       if(entitytags[0] != 34)
       {
          strcat(crh->etag, "\"");
          ++crh->etag_len;
       }

       //Append etag 
       strcat(crh->etag , entitytags);
       crh->etag_len += taglen;

       //Check for " at the end of the etag, if not present put it
       if(entitytags[taglen-1] != 34)
       {
           strcat(crh->etag, "\"");
           ++crh->etag_len;
       }

       NSDL2_CACHE(NULL, cptr, "crh->etag=[%s], crh->etag_len=[%d]",  crh->etag, crh->etag_len); 
       entitytags = strtok( NULL, comma_delim );
   } //while( entitytags != NULL ) 

}

/*************************************************************************************************
 * Description       : cache_convert_http_hdr_date_in_secs() allocates the memory for cache
 *                     for a user
 * Input Parameters  :
 *      http_hdr_date: Date received from HTTP Server in some Date header
 *          hdr_name : Name of the header in which http_hdr_date is received. 
                       Used for logging purposes only
 *          hdr_type : Header Type includes - 1 for Date, 2 for Expires, 3 for Last Modified
                       Used to handle values -ve values received in Expires
 *              cptr : Connection pointer which is used for received response
 *
 * Output Parameters : None
 *
 * Return Value      : a.The number of seconds since the Epoch, that is, since  1970-01-01 00:00:00
 *                       in case date is a valid date
 *                     b.In case Expires header contains -1, returns -1
 *                     c.NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/

u_ns_ts_t cache_convert_http_hdr_date_in_secs(connection *cptr, char *http_hdr_date, char *hdr_name, 
                                                                         cacheHeaders_et hdr_type)
{
  struct tm time_s;
  char *gmt_start, *ptr;
  static char buf[255]="-1";

  NSDL2_CACHE(NULL, cptr, "Method Called http_hdr_date=%s, hdr_name=%s for url=%s",
                                                    http_hdr_date, hdr_name, cptr->url);

  //Check for Expires date to be -1 & 0, for marking it as past date and hence not cacheable
  if(hdr_type == CACHE_EXPIRES)
  {
     if((strcmp(http_hdr_date,"-1") == 0) || (strcmp(http_hdr_date,"0") == 0))
        return 0;
  }

  // Check if GMT is present or not
  if((gmt_start = (char *)strcasestr(http_hdr_date,"gmt")) == NULL)
  {
     //event log - Date is not in GMT format  is not correct in HTTP header xxxx value =
     NSDL2_CACHE(NULL, cptr, "Date not in GMT format, Date Format='%s'                                \
                                    received for header=%d\n", http_hdr_date, hdr_name);
     return NS_CACHE_ERROR;
  }
 //This method supports date format 1 Jan 2011 19:00:00 GMT used for cache expires header and Tue, 1-Jan-2011 19:00:00 GMT(rfc format) format used by cookie expires attribute.
 //For cookie expires attribute both the formats are supported currently.
  if((ptr = (char *)strptime(http_hdr_date, "%a, %d %b %Y %T %Z", &time_s)) == NULL)//Format added for caching e.g.Tue, 1 Jan 2011 19:00:00 GMT.
  {
    if((ptr = (char *)strptime(http_hdr_date, "%a, %d-%b-%Y %H:%M:%S %Z", &time_s)) == NULL)//Format added for Cookie expires attribute date format e.g.Tue, 1-Jan-2011 19:00:00 GMT.
    {
      NSDL2_CACHE(NULL,cptr, "Not Supported Date Format='%s' received for header=%s", 
                                                             http_hdr_date, hdr_name); // Return -1 for invalid date format.
      //event log ->   ...
      //return -1;
      return NS_CACHE_ERROR;
    }
  }
  //Minimum accepted date length "Sat,8Jan2011 1:3:5 GMT"
   if ((gmt_start - http_hdr_date) < 18)
   {
     NSDL2_CACHE(NULL, cptr, "Minimum Length required for Date is 21. Date Format='%s'              \
                                  received for header=%s",http_hdr_date, hdr_name);

     //event log
     return NS_CACHE_ERROR;
   }

   strftime(buf, sizeof(buf), "%s", &time_s);
   return(atol(buf));
}


/*************************************************************************************************
Description       : This method is called when cache header is completely read 
                    To parse & save the header value received
                    
Input Parameters  :
 cCacheHeaderValue: The cache header value read
  cache_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response
     cache_header : Indicates which header is received

Output Parameters : None

Return Value      : None
*************************************************************************************************/

void cache_headers_parse_set(char *cCacheHeaderValue, int iCacheHeaderLength, 
                               connection *cptr, cacheHeaders_et cache_header)
{
  CacheResponseHeader *crh;

  NSDL2_CACHE(NULL, cptr, "Method Called Header Received=%s", cCacheHeaderValue);
  CacheTable_t *cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
  crh = cacheptr->cache_resp_hdr;

  switch (cache_header)
  {
    //Parse & Save Cache Control Headers - no-store, no-cache, must-revalidate & max-age
    case CACHE_CONTROL:
  	NSDL2_CACHE(NULL, cptr, "Cache Control Header Received");
        cache_parse_cache_control(cptr, cCacheHeaderValue, iCacheHeaderLength);
        break;
    
    //Saving Date Value & Date length in cptr->cptr_data->cache_data->CacheResponseHeader
    case CACHE_DATE:
  	NSDL2_CACHE(NULL, cptr, "Cache Header 'DATE = %s' received", cCacheHeaderValue);
        crh->date_value = cache_convert_http_hdr_date_in_secs(cptr, cCacheHeaderValue, "SERVER DATE", CACHE_DATE);
        if(crh->date_value == -1)
           break;

  	NSDL2_CACHE(NULL, cptr, "Cache Header 'DATE = %d'", crh->date_value);
        break;

    //Saving Expires Value & Expires length cptr->cptr_data->cache_data->CacheResponseHeader
    case CACHE_EXPIRES:
  	NSDL2_CACHE(NULL, cptr, "Cache Header 'EXPIRES = %s' received", cCacheHeaderValue );
        crh->expires = cache_convert_http_hdr_date_in_secs(cptr, cCacheHeaderValue, "EXPIRES", CACHE_EXPIRES);
        if(crh->expires == -1)
              break;
           
        if(crh->expires == 0)
        {
           cacheptr->cache_flags &= ~NS_CACHE_ENTRY_IS_CACHABLE;//Make the bit 0
  	   NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=%s - Invalid 'EXPIRES = %s' Header received",
                                                                    cacheptr->url ,cCacheHeaderValue);
           break;
        }
        if(crh->expires > 0)
             cacheptr->cache_flags |= NS_CACHEABLE_HEADER_PRESENT;

  	NSDL2_CACHE(NULL, cptr, "Cache Header 'EXPIRES = %d' as stored in Cache", crh->expires);
        break;

    //Saving Last Modified Date & Last Modified Date length in cptr->cptr_data->cache_data->CacheResponseHeader
    case CACHE_LMD:
        crh->last_modified_in_secs = cache_convert_http_hdr_date_in_secs(cptr, cCacheHeaderValue, 
                                                                      "LAST MODIFIED", CACHE_LMD);
        if(crh->last_modified_in_secs == -1)
           break;

        if(NULL == crh->last_modified)
        {
	   MY_MALLOC(crh->last_modified, iCacheHeaderLength, "HTTP CACHE HEADER - LAST MODIFIED DATE", -1);
        }
	else //Reallocating for cases header already exisiting and new value received in 304 response
        {
	   MY_REALLOC_EX(crh->last_modified, iCacheHeaderLength, (crh->last_modified_len + 1), "HTTP CACHE HEADER - LAST MODIFIED DATE", -1);
        }
           
        memset(crh->last_modified, 0, iCacheHeaderLength);
        strncpy(crh->last_modified, cCacheHeaderValue, iCacheHeaderLength);
        crh->last_modified_len = iCacheHeaderLength-1;

        cacheptr->cache_flags |= NS_VALIDATOR_PRESENT;

  	NSDL2_CACHE(NULL, cptr, "Cache Header 'LAST MODIFIED DATE = %s'", crh->last_modified);
        break;

    //Saving Cache Age
    case CACHE_AGE:
        // TODO: What if age has some more value like Age=10, xxx
        crh->age= atoi(cCacheHeaderValue);
  	NSDL2_CACHE(NULL, cptr, "Cache Header 'AGE = %d'", crh->age);
        break;

    //Saving Expires Value & Expires length in cptr->cptr_data->cache_data->CacheResponseHeader
    case CACHE_ETAG:
        cache_parse_etag(cptr, cCacheHeaderValue, iCacheHeaderLength);
        break;

    //Saving Pragma Directive in cptr->cptr_data->cache_data->CacheResponseHeader
    case CACHE_PRAGMA:
        if(strcasestr(cCacheHeaderValue,"no-cache") != 0)
            crh->CacheControlFlags.no_cache = 1;
  	NSDL2_CACHE(NULL, cptr, "Cache Pragma Directive no-cache received'");
        break;

    //If no matching header received
    default:
        NSDL3_CACHE(NULL, cptr, "Unhandled Cache Header Received = %s");
        break;
   }
   // Free partial hdr buf if it was allocated
   NSDL2_CACHE(NULL, cptr, "Exiting Method");
          
}

/*************************************************************************************************
Description       : This method will extract cache from the response and store in the Cache Response Header
                    It uses strstr() for \r\n to see if end of cache is received or not.     
                    It will handle all cases:
                    1.If end of header (\r\n) received
                      Case 1 [No earlier and complete now (Complete in one go)]
                        - Parse cache header value and add it in Cache Response Header depending on header received.
                      Case 2 [Partial earlier, complete now]
                        – Realloc buffer
                        - Append with existing cache header value.
                        - Parse cache header value and add it in Cache Response Header depending on header received.
                    Set state HANDLE_READ_NL_CR
                    2.If end of header (\r\n) not received
                      Case 3 [No earlier and partial now (Not complete in one read. It will go to next read)]
                       – Realloc buffer
                       - Save cache.
                      Case 4 [Partial earlier, partial now]
                       - Realloc buffer
                       - Append with previous
                       - Save cache.

Input Parameters  :
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response
     cache_header : Indicates which header is received

Output Parameters : Set cptr->header_state
                    HANDLE_READ_NL_CR (set if get whole cache)

Return Value      : Number of consumed bytes.
*************************************************************************************************/

int cache_save_header(connection *cptr, char* cache_buffer, int cache_buffer_len, int *consumed_bytes, 
								u_ns_ts_t now, cacheHeaders_et cache_header)
{
  char  *cache_end;
  int   length;
  CacheResponseHeader *crh;
  CacheTable_t *cacheptr;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_CACHE(NULL, cptr, "Method called, cache_buffer=%s, cache_buffer_len=%d", 
             cache_buffer, cache_buffer_len);

  //Check if user is enabled for cache
  if(vptr->flags & NS_CACHING_ON)
  { //Make the bit 1
    cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    //Checking the cache flag for cacheability -- check for bit 0
    if(!(cacheptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE))
    {
      cptr->header_state = HDST_TEXT; // Complete set-cache header line processed. Set state to CR to parse next header line
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=%s, Not Processing Further Cacheable      \
                                      Headers as response is not cacheable", cacheptr->url);
      return 0;
    }
  }
  else
  { //Make the bit 0
    cptr->header_state = HDST_TEXT; // Complete set-cache header line processed. Set state to CR to parse next header line
    NSDL2_CACHE(NULL, cptr, "Caching Disabled for User");
    return 0;
  }

   crh = ((CacheTable_t *)(cptr->cptr_data->cache_data))->cache_resp_hdr;

   // Check if end of cache header line is received
   // if((cache_end = memchr(cache_buffer, "\r\n", cache_buffer_len)))
   // Issue - memchr only take one char. So we are only checking \r.
   // Also if header is terminated by \n only, then this will not work
   if((cache_end = memchr(cache_buffer, '\r', cache_buffer_len)))
   {
     length = cache_end - cache_buffer; // Length is without \r
     //A. Replacing \r with \0 to identify the end-of-the-header-line
     //for facilitating the string search and string copy functions,
     //as they operate on \0
     cache_buffer[length] = 0;
     
     // Case 1 - Complete cache header line recieved in one read
     if(NULL == crh->partial_hdr)
     {
        NSDL3_CACHE(NULL, cptr, "Complete cache header line received in one read");
        // +1 in lenght is so that we send NULL termination also
        cache_headers_parse_set(cache_buffer, length+1, cptr, cache_header);
     }
     // Case 2 - Complete cache header line recieved now and was partially recieved earlier
     else
     {
        NSDL3_CACHE(NULL, cptr, "Complete cache header line received now and it was partial earlier");
        cache_save_partial_headers(cache_buffer, cptr, length);
        cache_headers_parse_set(crh->partial_hdr, crh->hdr_len, cptr, cache_header);
   	cache_free_partial_hdr(cptr);
     }
      //*consumed_bytes = length + 1;   // add 1 as we have checked till /r
      *consumed_bytes = length;   // add 1 as we have checked till /r
      cptr->header_state = HDST_CR; // Complete set-cache header line processed. Set state to CR to parse next header line
      //Reverting \0 back with \r for reasons mentioned in (A) above
      cache_buffer[length] = '\r';
    }
    else
    {
      // Case 3 - Parital cache header line recieved first time
      if(NULL == crh->partial_hdr)
      {
        NSDL3_CACHE(NULL, cptr, "Parital cache header line recieved first time");
      }
      // Case 4 - Parital cache header line recieved and was partially recieved earlier
      else
      {
        NSDL3_CACHE(NULL, cptr, "Partial cache header line recieved and was partially recieved earlier");
      }
      
      cache_save_partial_headers(cache_buffer, cptr, cache_buffer_len);
      *consumed_bytes = cache_buffer_len; // All bytes are consumed
    }

  NSDL2_CACHE(NULL, cptr, "Exiting Method, consumed_bytes = %d", *consumed_bytes);
  return(0);
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Cache-control headers 
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_cache_control(connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Cache-Control Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_CONTROL));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Last Modified Date
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_last_modified(connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Last Modified Date Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_LMD));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Expires header
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_expires(connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Expires Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_EXPIRES));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading ETag Headers
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_etag (connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Etag Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_ETAG));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Date Headers
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_date (connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Date Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_DATE));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Date Headers
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_age (connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Age Header Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_AGE));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}

/*************************************************************************************************
Description       : Call cache_save_header() for reading Pragma Directives
                    
Input Parameters  :
              cptr: Connection pointer which is used for received response
      cache_buffer: Pointer in the received buffer for the cache.
  cache_buffer_len: Number of bytes available
    consumed_bytes: 
               now: 

Output Parameters : None

Return Value      : None
*************************************************************************************************/
int proc_http_hdr_pragma (connection* cptr, char* cache_buffer, int cache_buffer_len, 
								int *consumed_bytes, u_ns_ts_t now)
{
    NSDL2_CACHE(NULL, cptr, "Method called, Pragma Directive Received", cache_buffer_len);
    return(cache_save_header(cptr, cache_buffer, cache_buffer_len, consumed_bytes, now, CACHE_PRAGMA));
    NSDL2_CACHE(NULL, cptr, "Exiting Method");
}



