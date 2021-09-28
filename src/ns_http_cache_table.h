#ifndef NS_HTTP_CACHE_TABLE_H
#define NS_HTTP_CACHE_TABLE_H
#include "ns_data_types.h"
typedef int cache_age_t;   // Age related 

//Contains the response header values & their lengths for caching
typedef struct {
  struct 
  {
    //No-store carries the highest proirity
    //Must not store the response that contains no-store 
    unsigned char no_store:1,                   
    //No-cache has priority is lesser than no-store 
    //Do not server from cache without revalidation 
    no_cache:1,                    
    //must-revalidate has priority is lesser than no-cache
    //Cannot server a stale copy without first revalidating with the origin server
    must_revalidate:1;   
  } CacheControlFlags;

  //Max-age has priority lesser than must-revalidate
  //Maximum amount of time(in seconds) since it came from the server
  //for which a document can be considered fresh
  cache_age_t max_age; 
 
  //Expires has priority lesser than max-age
  //Entity header field giving the date/time(in GMT) after which the response is considered stale 
  //char *expires;   
  //short expires_len;
  int expires;

  //Date has priority lesser than Expires
  //Time at which the response was originated - format GMT
  //char *date_value;   
  //short date_value_len;
  int date_value;

  //Entity header field stating the date/time at which the origin server believes the variant was last modified
  //Used for sending in the request for revalidation, if entry is in cache and requires revalidation
  //Storing last_modified in string format as we need to send it the request for revalidation
  char *last_modified;
  short last_modified_len;
  int last_modified_in_secs;

  //Entity header field which provides the current value of the entity tag for the requested variant
  //Used for sending in the request for revalidation, if entry is in cache and requires revalidation
  char *etag; 
  short etag_len;

  //Sender's estimate of the amount of time since the response was generated at the origin server
  //A cached response is considered "fresh" if its age does not exceed its freshness lifetime
  int age; 

  //Set it to 1 if MaxAge or Expires present and none is 0
  //char cacheable_hdr_present;
  //Set it to 1 if any of the validator - ETag or LMD Present
  //char validator_present;
  //Set it to 1 in case 304 is received in response
  //Used to replace existing ETag if received in response to 304
  //Set it to 0 immediately after use
  //char first_etag_received_in_304;

  // Used for storing partial headers temporarily while parsing in callback methods
  char *partial_hdr;
  int hdr_len;

} CacheResponseHeader;
 
typedef struct
{
  struct
  {
    //No-store carries the highest proirity
    //Must not store the response that contains no-store
    unsigned char no_store:1,
    //No-cache has priority is lesser than no-store
    //Do not server from cache without revalidation
    no_cache:1,
    //must-revalidate has priority is lesser than no-cache
    //Cannot server a stale copy without first revalidating with the origin server
    must_revalidate:1;
    //Indicates that the client only wishes to return a stored response
    //only_if_cached:1;
  } CacheControlFlags;

  //Max-age has priority lesser than must-revalidate
  //Maximum amount of time(in seconds) since it came from the server
  //for which a document can be considered fresh
  cache_age_t max_age;
  //max_stale denotes the willingness of the client to accept a response
  //that has exceeded its expiration time.
  cache_age_t max_stale;
  //min_fresh indicates that the client is willin gthe accept a response
  //that will still be fresh for at least specified number of seconds
  cache_age_t min_fresh;

} CacheRequestHeader;



/*
Issues:
  1. In 64 bit machine, what is the size of Long -> 8 bytes. Then what will be timestamp?
  2. In what foramt to keep date - string or ts  - Use string
  3. Mulitple etags - Yes
*/

//================= Cache Table Structure =============================================
typedef struct CacheTable_s 
{

 //parameterized url string – the cache table is hashed on url
  unsigned char *url; 	   
  unsigned int  url_len; 
  /* We have complete Url in unsigned char *url
   * In Following format:
   * http://www.google.com/index.html
   * url_offset stores the offset to /index.html
   */
  unsigned short url_offset;

  //Response headers storing the head of the response linked list
  struct copy_buffer* resp_buf_head;
  unsigned int resp_len;

  //If the pointer of CacheTable kept in cptr is from cache 
  unsigned short cache_flags;

  //Required in case of Redirection (Response Code - 301) 
  char *location_url;

  //Bug 2119  #Shilpa 10Feb2011
  short http_method;
  //Contains the pointer to the response header structure containing 
  //cache header values & their lengths for caching
  CacheResponseHeader *cache_resp_hdr;

  //Time in seconds at which the response is received
  u_ns_ts_t cache_ts; 
  //Time in seconds at which the request is send in seconds
  u_ns_ts_t request_ts; 
  //Age of document (in seconds) when arrived at cache
  //This includes apparent age based on Age and/or Date header and
  //the response delay time
  unsigned int doc_age_when_received; 

  //Hash Index Value used for finding the cache entry in case of collisson
  unsigned int ihashValue;
  
  /*Body offset points body start in response buffer*/
  int body_offset;
  
  short compression_type;
  
  short cache_hits_cnt;

//  CacheRequestHeader *cache_req_hdr;
 
  //Used to create the chain (linked list) in case of collision, 
  //storing the pointer to the next cache entry in the list
  struct CacheTable_s *next; 
  //Used to track total number of reference count
  //of this node.
  int linked_count;
  //This is for node type. Weather node is for Master
  //or for user
  char node_type;
  //This is for optimization purpose.
  //We can also use ihashMasterIdx
  unsigned int ihashIndex;

  //stores the index of master table used to store the response buffer
  int ihashMasterIdx;

  /*Pointing to node in master table.
   *This will have address of the master table node*/
  struct CacheTable_s *master_tbl_node;
} CacheTable_t;

#define MASTER_NODE     1
#define USER_NODE       2

#define NS_CACHE_ENTRY_IS_CACHABLE       0x0001
#define NS_CACHE_ENTRY_IN_CACHE          0x0010
//Set it if MaxAge or Expires present and none is 0
#define NS_CACHEABLE_HEADER_PRESENT      0x0020
//Set it if any of the validator - ETag or LMD Present
#define NS_VALIDATOR_PRESENT             0x0040 
//Used to replace existing ETag if received in response to 304
//Free it immediately after use
#define NS_304_FOR_ETAG                  0x0080

#define NS_CACHE_ENTRY_NOT_FOR_CACHE     0x0100

//Enum is taken as we require to compare the current cache entry state, 
//and based on that need to take the decision 
//as we need to set one bit and reset others
typedef enum 
{
   NS_CACHE_ENTRY_NEW = 0x0002,
   NS_CACHE_ENTRY_VALIDATE = 0x0004,
   NS_CACHE_ENTRY_EXPIRED = 0x0008
} CacheState_et;


typedef struct
{
  unsigned int cache_table_size_value; // In case of mode 1, it is avg value
  unsigned int cum_count; // Number of times avg is done so far

  /*For master tabel*/
  CacheTable_t **master_cache_table_ptr;
  unsigned int master_cache_table_size; 
} CacheAllocSize;

#ifndef CAV_MAIN
extern CacheAllocSize *cas_ptr;
#else
extern __thread CacheAllocSize *cas_ptr;
#endif

extern void cache_set_cache_table_size_value(int grp_idx, int max_cache_entries);
extern void init_cache_table ();
#endif
