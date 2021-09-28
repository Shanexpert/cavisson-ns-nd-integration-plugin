/************************************************************************************************
 * File Name            : ns_http_cache_store.c
 * Author(s)            : Shilpa Sethi
 * Date                 : 23 November 2010
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains functions for creating, storing, searching, deleting
 *                        http request & responses in cache hash table
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/

#include "ns_data_types.h"
#include "ns_cache_include.h"
#include "ns_http_cache_reporting.h"
#include "ns_http_cache_table.h"
#include "ns_group_data.h"

//Global Declarations
//vptr->httpData.cache_table_size is maximum number of urls that can be cahced per user
//Provide vptr->httpData.cache_table_size a value any exponential of 2 e.g 128 or 512 for optimized results
// static int vptr->httpData.cache_table_size = 512;

//<<<<<<<<<<<<<<<<<<<<<<<<<<<< F U N C T I O N   D E F I N I T I O N S i>>>>>>>>>>>>>>>>>>>>>>>>>

/***********************************************************************************************
Description       : Free the linked list created for the response buffer

Input Parameters  :
          buf_head: pointer to the first node of response buffer

Output Parameters : None

Return Value      : None
************************************************************************************************/

void free_link_list (void *buf_head)
{
  struct copy_buffer* buffer = (struct copy_buffer *)buf_head;
  struct copy_buffer* nxt_buffer;

  NSDL2_CACHE(NULL, NULL, "Method called, buffer = %p, buf_head = %p",
					  buffer, buf_head);
  while(buffer) {
      nxt_buffer = buffer->next;
      FREE_AND_MAKE_NULL_EX(buffer, COPY_BUFFER_LENGTH, "Freeing old_buffer", -1);
      buffer = nxt_buffer;
  }
}

/*************************************************************************************************
Description       : cache_init() allocates the memory for cache for a user 
                      
Input Parameters  : 
             vptr : pointer to user table

Output Parameters : None

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/
int cache_init(VUser *vptr)
{
int grp_idx;

   NSDL3_CACHE(NULL, NULL, "Method Called");
   //Checking the allocation of memory to vptr
   if (NULL == vptr)
   {
      NSDL1_CACHE(NULL, NULL, "Cannot Proceed 'vptr' found NULL");
      return NS_CACHE_ERROR;
   }
   if (NULL == vptr->httpData)
   {
      NSDL1_CACHE(vptr, NULL, "Cannot Proceed 'vptr->httpData' found NULL");
      return NS_CACHE_ERROR;
   }
  
   grp_idx = vptr->group_num;
   //Set cache_table_size for the user
   vptr->httpData->cache_table_size = cas_ptr[grp_idx].cache_table_size_value;
  
   int size = sizeof(CacheTable_t *) * vptr->httpData->cache_table_size; 
   MY_MALLOC_AND_MEMSET(vptr->httpData->cacheTable, size, "User cache table", -1);

   if(runprof_table_shr_mem[grp_idx].gset.master_cache_mode == 0)
   {
     vptr->httpData->master_cacheTable = vptr->httpData->cacheTable;
     vptr->httpData->master_cache_table_size = vptr->httpData->cache_table_size;
   }
   else
   {
     vptr->httpData->master_cacheTable = cas_ptr[grp_idx].master_cache_table_ptr; 
     vptr->httpData->master_cache_table_size = cas_ptr[grp_idx].master_cache_table_size; 
   }
  
   NSDL2_CACHE(vptr, NULL, "Cache Table Created with array size = %d", vptr->httpData->cache_table_size);
   
   NSDL3_CACHE(vptr, NULL, "Exiting Method");
   return NS_CACHE_SUCCESS;
}


 /*This function is to create master node*/
static CacheTable_t *create_master_node(connection *cptr, VUser *vptr, CacheTable_t *cache_data, int masterhashidx,  unsigned int ihashValue)
{
  CacheTable_t *cacheptr = NULL;
  NSDL2_CACHE(vptr, cptr, "Method called");

  MY_MALLOC_AND_MEMSET(cacheptr, sizeof(CacheTable_t), "Malloc cacheptr for adding into master table", 1);

  // We keep the malloced copy of url in master table as user cache table url will be freed
  MALLOC_AND_COPY(cache_data->url, cacheptr->url, cache_data->url_len + 1, "Master table url", -1);
  cacheptr->url_len = cache_data->url_len;

  cacheptr->resp_buf_head = cache_data->resp_buf_head;
  cacheptr->resp_len = cache_data->resp_len;
  cacheptr->http_method = cache_data->http_method;
  cacheptr->compression_type = cache_data->compression_type;
  cacheptr->body_offset = cache_data->body_offset;
  cacheptr->ihashValue = ihashValue;
  cacheptr->linked_count++;
  cacheptr->next = NULL;
  /*Make response buffer pointer NULL*/
  cache_data->resp_buf_head = NULL;
  cache_data->resp_len = 0;

  NSDL2_CACHE(vptr, cptr, "Linked count = %d, resp length = %d, cacheptr->http_method = %hd, cacheptr->ihashValue = %u, cacheptr->compression_type = %hd", cacheptr->linked_count, cacheptr->resp_len, cacheptr->http_method, cacheptr->ihashValue, cacheptr->compression_type);

  NSDL2_CACHE(vptr, cptr, "Method exiting");
  return cacheptr;
}

/*************************************************************************************************
Description       : Creates chain on collided hashindex using linked list (if not already existing)
                    and stores the cptr->cptr_data->cache_data at the end of chained link.
                      
Input Parameters  : 
      pCacheEntry : Points to the start of the list (For Hash Table chaining)
 CacheTable_entry : Pointer to newly allocated cache entry

Output Parameters : 
             link : Used for logging purposes to store the chain index in case of collision, 
                    starting with 0 for entry which is stored directly in hashtable

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/

static int cache_list_add(CacheTable_t *pCacheEntry, CacheTable_t *cacheTable_entry, int *link)
{
   NSDL3_CACHE(NULL, NULL, "Method Called");

   while(NULL != pCacheEntry->next)
   {
      pCacheEntry = pCacheEntry->next;
      ++*link;
   } 
   //Store the new cache_entry at the end
   pCacheEntry->next = cacheTable_entry;
   ++*link;

   NSDL3_CACHE(NULL, NULL, "Exiting Method");

   return NS_CACHE_SUCCESS;
}

/*************************************************************************************************
Description       : 1. cache_add_url() accepts url as input
                    2. gets the hashindex from the hashtable based on url passed
                    3. checks whether the hash index returned by nslb_hash_get_hash_index() is free for use
                    4. If hashindex free, stores the address of cptr->cptr_data->cache_data at the hashindex in hashtable
                    5. If hashindex not free, create chain on that index using linked list
                       and stores the cptr->cptr_data->cache_data on that chained link.
                      
Input Parameters  : 
             vptr : Pointer to User Structure
             url  : url formulated in request

Output Parameters : None

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/
int cache_add_url(connection *cptr, VUser *vptr, unsigned char *url)
{
   //Pointer to newly allocated cache entry
   CacheTable_t  *cacheTable_entry;
   //Pointer to traverse the chain in case of collision
   CacheTable_t  *pCacheEntry;
   //Used for logging purposes to store the chain index in case of collision, 
   //starting with 0 for entry which is stored directly in hashtable
   int link=0; 
   //int iRetVal;
   unsigned int ihashValue;

   int iUrlLen = ((CacheTable_t *)cptr->cptr_data->cache_data)->url_len;
   NSDL3_CACHE(vptr, cptr, "Method Called for Url=[%*.*s], table = %p", iUrlLen, iUrlLen, url, vptr->httpData->cacheTable);

   //2. gets the hashindex from the hashtable based on url passed
   //unsigned int ihashIndex = nslb_hash_get_hash_index(vptr, url, iUrlLen, &ihashValue);
   unsigned int ihashIndex = nslb_hash_get_hash_index(url, iUrlLen, &ihashValue, vptr->httpData->cache_table_size);

   if(NS_CACHE_ERROR == ihashIndex)
   {
      NSDL2_CACHE(vptr, cptr, "Hash Function Error - Could Not Find Hashindex");
      INC_CACHE_NUM_ENTERIES_ERR_CREATION_COUNTERS(vptr);
      return NS_CACHE_ERROR;
   }

   if (NULL == cptr->cptr_data->cache_data)
   {
      NSDL1_CACHE(vptr, cptr, "Cannot Proceed 'cptr->cptr_data->cache_data' found NULL");
      INC_CACHE_NUM_ENTERIES_ERR_CREATION_COUNTERS(vptr);
      return NS_CACHE_ERROR;
   }

   //Point to cptr->cptr_data->cache_data which stores the cache entry for the current request
   cacheTable_entry = (CacheTable_t *)cptr->cptr_data->cache_data;
   cacheTable_entry->ihashValue = ihashValue;
   cacheTable_entry->next = NULL;

   //3. checks whether the hash index returned by nslb_hash_get_hash_index() is free for use
   NSDL2_CACHE(vptr, cptr, "vptr->httpData->cacheTable[ihashIndex]=%p", vptr->httpData->cacheTable[ihashIndex]);
   if(NULL == vptr->httpData->cacheTable[ihashIndex])
   {
      //4. If hashindex free, stores the address of cptr->cptr_data->cache_data 
      //   at the hashindex in hashtable
      vptr->httpData->cacheTable[ihashIndex] = cacheTable_entry;
      NSDL2_CACHE(vptr, cptr, "Stored at Hashindex=[%d] url=%s in CacheTable, method = %d, table = %p", ihashIndex, cacheTable_entry->url, cacheTable_entry->http_method, vptr->httpData->cacheTable);
   }

   //5. If hashindex is not free, create chain on that index using linked list
   //   and stores the cptr->cptr_data->cache_data on that chained link.
   else
   {
      NSDL2_CACHE(vptr, cptr, "Collission occurred at Hashindex=[%d] in CacheTable ", ihashIndex, cacheTable_entry->url);
      pCacheEntry = vptr->httpData->cacheTable[ihashIndex];
      cache_list_add(pCacheEntry, cacheTable_entry, &link);
      NSDL2_CACHE(vptr, cptr, "Stored at chained link->%d at Hashindex=[%d] url=%s ", 
                                              link, ihashIndex, cacheTable_entry->url);
      //Increment Collission Counter
      INC_CACHE_NUM_ENTERIES_COLLISION_COUNTERS(vptr);
   }
    
   //Increment Number of Cache Entries
   INC_CACHE_NUM_ENTERIES_COUNTERS(vptr);
   // we are incrementing this counter so that we know how many max entries a user have.
   vptr->httpData->max_cache_entries++;

   //Add resp_len
   // DELTE if(runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 0 || cptr->url_num->proto.http.type == MAIN_URL)
   if(cacheTable_entry->ihashMasterIdx == -1)
   {
     NSDL2_CACHE(vptr, cptr, "Before adding resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);
     cache_avgtime->cache_bytes_used += cacheTable_entry->resp_len;
     INC_CACHE_BYTES_USED_COUNTERS(vptr, cacheTable_entry->resp_len);
     NSDL2_CACHE(vptr, cptr, "After adding resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);
   }

   NSDL2_CACHE(vptr, cptr, "cache_num_entries=%lld, Bytes in CacheTable = %u, Total Bytes = %u, max_cache_entries counter = %d", cache_avgtime->cache_num_entries, cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used, vptr->httpData->max_cache_entries);

   //Doing NULL as in connection reuse case we will not 
   //close the connection so making NULL for second request
   // Move to calling function
   // cptr->cptr_data->cache_data = NULL;

   NSDL3_CACHE(vptr, cptr, "Exiting Method");
   return NS_CACHE_SUCCESS;
}
/*************************************************************************************************
Description       : 1. cache_add_url_in_master_table() accepts url as input
                    2. gets the hashindex from the hashtable based on url passed
                    3. checks whether the hash index returned by nslb_hash_get_hash_index() is free for use
                    4. If hashindex free, stores the address of cptr->cptr_data->cache_data at the hashindex in hashtable
                    5. If hashindex not free, create chain on that index using linked list
                       and stores the cptr->cptr_data->cache_data on that chained link.
                      
Input Parameters  : 
             vptr : Pointer to User Structure
             url  : url formulated in request

Output Parameters : None

Return Value      : Master table Node in case function executed successfully
                    NULL in case any failure occurs in the function
*************************************************************************************************/
CacheTable_t *cache_add_url_in_master_tbl(connection *cptr, VUser *vptr, unsigned char *url, int iUrlLen, int *masterhashidx)
{
   //Pointer to newly allocated cache entry
   CacheTable_t  *cacheptr;
   //Pointer to traverse the chain in case of collision
   CacheTable_t  *pCacheEntry;
   //Used for logging purposes to store the chain index in case of collision, 
   //starting with 0 for entry which is stored directly in hashtable
   int link = 0; 
   //int iRetVal;
   unsigned int ihashValue;
   int increment_linked_count; 

   NSDL3_CACHE(vptr, cptr, "Method Called for Url=[%*.*s], master table = %p", iUrlLen, iUrlLen, url, vptr->httpData->master_cacheTable);

   /*Adding cache node into the CacheTable
   * Node could be added in User table and may or may not be added in the master table 
   * depending it exist in the master table or not */
   //First check for Master table, if URL is already there then
   //Increment the linked_count 
   increment_linked_count = *masterhashidx;
   cacheptr = cache_url_found(vptr, url, iUrlLen, NULL, masterhashidx, &ihashValue, cptr->url_num->proto.http.http_method, MASTER_NODE);
   if(cacheptr != NULL)
   {
     //URL is already in Master table
     //Increment the linke_count.
      if(increment_linked_count == -1){
        cacheptr->linked_count++;
        NSDL2_CACHE(vptr, cptr, "After incrementing linked count = %d", cacheptr->linked_count);
      }

     //Free the response buffer for this User node.
     //As this is already pointing Master table 
     NSDL2_CACHE(vptr, cptr, "URL is found in master table, freeing the old response from master table");

     NSDL2_CACHE(vptr, NULL, "Before deleting resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);
     cache_avgtime->cache_bytes_used -= cacheptr->resp_len;
     DEC_CACHE_BYTES_USED_COUNTERS(vptr, cacheptr->resp_len);
     NSDL2_CACHE(vptr, NULL, "After deleting resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);

     free_link_list(cacheptr->resp_buf_head);
     cacheptr->resp_len = 0;
     //Now point resp buff from user node to  master node
     cacheptr->resp_buf_head = ((CacheTable_t *)cptr->cptr_data->cache_data)->resp_buf_head;
     cacheptr->resp_len = ((CacheTable_t *)cptr->cptr_data->cache_data)->resp_len;

     NSDL2_CACHE(vptr, NULL, "Before adding resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);
     cache_avgtime->cache_bytes_used += cacheptr->resp_len;
     INC_CACHE_BYTES_USED_COUNTERS(vptr, cacheptr->resp_len);
     NSDL2_CACHE(vptr, NULL, "After adding resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);

     NSDL3_CACHE(vptr, cptr, "Exiting method after adding url [%s], linked_count = %d, master node = %p", url, cacheptr->linked_count, cacheptr);
     return cacheptr;
   }
   else
   {
     //URL not found in Master table
     //Add node into Master table.
     //1.Create cache node,
     //2.Fill node with response buffer
     //3.Increment the linked count
     cacheptr = create_master_node(cptr, vptr, cptr->cptr_data->cache_data, *masterhashidx, ihashValue);
   }

   //3. checks whether the hash index returned by nslb_hash_get_hash_index() is free for use
   if(NULL == vptr->httpData->master_cacheTable[*masterhashidx])
   {
     //4. If hashindex free, stores the address of cptr->cptr_data->cache_data 
     //   at the hashindex in hashtable
     vptr->httpData->master_cacheTable[*masterhashidx] = cacheptr;
     NSDL2_CACHE(vptr, cptr, "vptr->httpData->master_cacheTable[%d]=%p", *masterhashidx, vptr->httpData->master_cacheTable[*masterhashidx]);
     NSDL2_CACHE(vptr, cptr, "Stored at Hashindex=[%d] url=%s in CacheTable ", *masterhashidx, cacheptr->url);
   }

   //5. If hashindex is not free, create chain on that index using linked list
   //   and stores the cptr->cptr_data->cache_data on that chained link.
   else
   {
      NSDL2_CACHE(vptr, cptr, "Collission occurred at Hashindex=[%d] in CacheTable ", *masterhashidx, cacheptr->url);
      pCacheEntry = vptr->httpData->master_cacheTable[*masterhashidx];
      cache_list_add(pCacheEntry, cacheptr, &link);
      NSDL2_CACHE(vptr, cptr, "Stored at chained link->%d at Hashindex=[%d] url=%s ", 
                                              link, *masterhashidx,  cacheptr->url);
   }
    
   //Add resp_len
   NSDL2_CACHE(vptr, cptr, "Before adding resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);
   cache_avgtime->cache_bytes_used += cacheptr->resp_len;
   INC_CACHE_BYTES_USED_COUNTERS(vptr, cacheptr->resp_len);
   NSDL2_CACHE(vptr, cptr, "After adding resp_len = %d, cache_bytes_used = %u", cacheptr->resp_len, cache_avgtime->cache_bytes_used);

   NSDL3_CACHE(vptr, cptr, "Exiting method after adding url [%s], linked_count = %d, master node = %p", url, cacheptr->linked_count, cacheptr);
   return cacheptr;
}

CacheTable_t *get_cache_node(unsigned int hash, short http_method, CacheTable_t **prev_cache_link, 
                       CacheTable_t **cacheTable, unsigned char *url, int iUrlLen, unsigned int ihashValue)
{
  int url_found=1;
  //Pointer to traverse the chain in case of collision
  CacheTable_t  *pCacheEntry;
  //Pointer to cached entry found in cache table
  CacheTable_t  *cacheTable_entry=NULL;
  //Used for logging purposes to store the chain index in case of collision, 
  //starting with 0 for entry which is stored directly in hashtable
  int link=0;

  NSDL2_CACHE(NULL, NULL, "hash = %d, http_method = %d, ihashValue = %u, table = %p", hash, http_method, ihashValue, cacheTable);

   //2. If hashindex doesn't contain any cacheTable_entry, Url not found in cache,
   //   cache_url_found() function will return NULL
   NSDL2_CACHE(NULL, NULL, "Going to check at cacheTable[%d]=%p", hash, cacheTable[hash]);

   if(NULL == cacheTable)
   {
      NSDL2_CACHE(NULL, NULL, "Table is NULL");
      return NULL;
   }

   if(NULL == cacheTable[hash])
   {
      NSDL2_CACHE(NULL, NULL, "Url=%s NOT FOUND in Hash Table", url);
      return NULL;
   }
      
   pCacheEntry = cacheTable[hash];

   //3. If hashindex contains cacheTable_entry call cache_list_search() 
   //   to search the cache entry in the list by matching the url to be searched 
   //   with the url in the chained linked list. 
   //   Traverse the linked list to reach the searched url
   do {
        if(NULL != pCacheEntry->url)
        {
           //Matching the method first
           NSDL2_CACHE(NULL, NULL, "pCacheEntry->http_method = %d, http_method = %d", pCacheEntry->http_method, http_method);
           if(pCacheEntry->http_method == http_method)
           {
             //Then the url
             url_found = nslb_hash_match_value(pCacheEntry->url, pCacheEntry->ihashValue, url, iUrlLen, ihashValue);
           }
           else
           {
               NSDL2_CACHE(NULL, NULL, "Url=%s Method Not Matching, Mothod in Cachetable=%d, Method to search=%d, link = %d", url, pCacheEntry->http_method, http_method, link);
               url_found=0;
           }

        }
        //If url is found break the while loop
        if (url_found != 0)
        {
           break;
        }
        //If combination of (url & method) not matching go to the next link in the chain
        else
        {
           if(NULL != prev_cache_link)
               *prev_cache_link = pCacheEntry;   
           pCacheEntry = pCacheEntry->next;
        }
        ++link;
     } while(NULL != pCacheEntry);
     //iRetVal = cache_list_search(pCacheEntry, url, iUrlLen, &url_found, &link, prev_cache_link);

   //4. If url found in chained list, return the pointer to the found CacheTable entry
   if(url_found)
   {
      cacheTable_entry = pCacheEntry;         
      NSDL2_CACHE(NULL, NULL, "FOUND at Hashindex %d chained link->%d, url->[%*.*s]",
                                                           hash, link, iUrlLen, iUrlLen, url);
      return cacheTable_entry;
   }
   //5. If url not found in the chained list, return NULL
   else
   {
      NSDL2_CACHE(NULL, NULL, "NOT FOUND in Cache Table url->[%*.*s]", iUrlLen, iUrlLen, url);
      return NULL;      
   }
}

/*************************************************************************************************
Description       : 1. Calls the nslb_hash_get_hash_index() index to get the hashindex based on url to be searched
                    2. If hashindex doesn't contain any cacheTable_entry, Url not found in cache,
                       cache_url_found() function will return NULL
                    3. If hashindex contains cacheTable_entry call cache_list_search() 
                       to search the cache entry in the list by matching the url to be searched 
                       with the url in the chained linked list (formed in case of collision). 
                    4. If url found in chained list, return the pointer to the found CacheTable entry
                    5. If url not found in the chained list, return NULL
                      
Input Parameters  : 
             vptr : Pointer to User Structure
             url  : url to be searched
         iUrlLen  : Length of the url to be searched

Output Parameters : 
  prev_cache_link : Need not pass for searching url
                    Need only to pass in case cache_url_found() is called for searching url for deleting it
                    cache_url_found() will store the address of previous cache entry's address

Return Value      : Pointer to cache entry where url is found in cache table
                    Null if not found in cache
*************************************************************************************************/

CacheTable_t *cache_url_found(VUser *vptr, unsigned char *url, int iUrlLen, 
				CacheTable_t **prev_cache_link, int *ihashIndex, unsigned int *ihashValue, short http_method, int node_type)
{
  //Pointer to cached entry found in cache table
  CacheTable_t  *cacheTable_entry=NULL;
  //Used to store the hashValue
  unsigned int hash = -1;
   
   NSDL2_CACHE(vptr, NULL, "Method Called for Url=[%*.*s], Method = %hd", iUrlLen, iUrlLen, url, http_method);

   if(NULL == vptr || NULL == url)
   {
      NSDL1_CACHE(vptr, NULL, "Cannot Proceed 'cptr || vptr' found NULL");
      return NULL;
   }

   //1. Calls the nslb_hash_get_hash_index() index to get the hashindex based on url to be searched
   if(node_type == USER_NODE)
     hash = nslb_hash_get_hash_index(url, iUrlLen, ihashValue, vptr->httpData->cache_table_size);
   else
     hash = nslb_hash_get_hash_index(url, iUrlLen, ihashValue, vptr->httpData->master_cache_table_size);

   if(NS_CACHE_ERROR == hash)
   {
      NSDL2_CACHE(vptr, NULL, "Error in Hash Function");
      return NULL;
   }

   if(node_type == USER_NODE)
   {
     NSDL2_CACHE(vptr, NULL, "About to call get_cache_node() for user node");
     cacheTable_entry = get_cache_node(hash, http_method, prev_cache_link, 
                             vptr->httpData->cacheTable, url, iUrlLen, *ihashValue);
   }
   else
   {
     NSDL2_CACHE(vptr, NULL, "About to call get_cache_node() for master node");
     cacheTable_entry = get_cache_node(hash, http_method, prev_cache_link, 
                             vptr->httpData->master_cacheTable, url, iUrlLen, *ihashValue);
   }

   //Fill hash idx in every case.This is optimization.
   //If we are searching a URL and dont find in hash table
   //then we may need to add URL in hash table.So, Adding time we 
   //would not need to get hash value again because every time URL is same and we get same value.
   //if(cacheTable_entry != NULL)
   *ihashIndex = hash;

   NSDL2_CACHE(vptr, NULL, "Returning with %p", cacheTable_entry);
   return cacheTable_entry;
}


/***********************************************************************************************
Description       : Free the memory allocated in Cache Table including url, 
                    response buffer & cache related headers
                    This is used in two cases:
                      1. Entry is no longer required e.g. delteing from cache
                      2. Entry is required e.g. 304 case
                      TODO: Make separate method for optimization

Input Parameters  : 
             cre  : Pointer to Cache Table Entry to be freed
         free_url : flag stating url to be freed or not. 1 to free
        free_resp : flag stating resp buffer to be freed or not. 1 to free


Output Parameters : None

Return Value      : None
************************************************************************************************/
void cache_free_data(CacheTable_t *cte, short free_url, short free_resp, short free_header_node)
{
   
   NSDL2_CACHE(NULL, NULL, "Method called, free_url = %d, free_resp = %d, free_header_node = %d", free_url, free_resp, free_header_node);

   //Free Last Modified
   FREE_AND_MAKE_NULL_EX(cte->cache_resp_hdr->last_modified, cte->cache_resp_hdr->last_modified_len, "cte->cache_resp_hdr->last_modified", -1);
   cte->cache_resp_hdr->last_modified_len = 0;
   cte->cache_resp_hdr->last_modified_in_secs = -1;

   //Free ETag
   FREE_AND_MAKE_NULL_EX(cte->cache_resp_hdr->etag, cte->cache_resp_hdr->etag_len, "cte->cache_resp_hdr->etag", -1);
   cte->cache_resp_hdr->etag_len = 0;

   //Reset all flag bits
   memset(&cte->cache_resp_hdr->CacheControlFlags, 0, 1);
   
   cte->cache_resp_hdr->max_age = -1;
   cte->cache_resp_hdr->expires = -1;
   cte->cache_resp_hdr->date_value = -1;
   cte->cache_resp_hdr->age = -1;
   cte->cache_ts = -1;
   cte->request_ts = -1;
   cte->doc_age_when_received = -1;
   cte->cache_hits_cnt = 0;
   cte->cache_flags &= ~NS_VALIDATOR_PRESENT;
   cte->cache_flags &= ~NS_CACHEABLE_HEADER_PRESENT;
   cte->cache_flags &= ~NS_304_FOR_ETAG;
   //cte->ihashMasterIdx = -1; // Do not change it as we need to keep this value intact

   //Free header
   if(free_header_node)
      FREE_AND_MAKE_NULL_EX(cte->cache_resp_hdr, sizeof(CacheResponseHeader), "cte->cache_resp_hdr", -1);

   if(free_resp)
   {
      //Free Response Stored in Linked List
      free_link_list(cte->resp_buf_head);
      cte->resp_buf_head = NULL;
      cte->resp_len = 0;;
   }

   //Free Location_url
   FREE_AND_MAKE_NULL_EX(cte->location_url, strlen(cte->location_url), "cte->location_url", -1);

   if(free_url)
   {
     FREE_AND_MAKE_NULL_EX(cte->url, cte->url_len, "cte->url", -1);
     cte->url_len = 0;
     cte->resp_buf_head = NULL;
   }
   NSDL2_CACHE(NULL, NULL, "Method exiting");
}

/*This method is for delet node from master table
 * In master table we cache only resp buff other fields
 * are NULL so made new function to free master node*/
void cache_free_data_for_master_table_node(CacheTable_t *cte)
{
   NSDL2_CACHE(NULL, NULL, "Method called, node to delete = %p", cte);

   //Free Response Stored in Linked List
   free_link_list(cte->resp_buf_head);
   cte->resp_buf_head = NULL;
   cte->resp_len = 0;;

   //FREE_AND_MAKE_NULL(cte->url, "cte->url", -1);
   //cte->url_len = 0;
   NSDL2_CACHE(NULL, NULL, "Method exiting");
}
/*************************************************************************************************
Description       : 1. cache_delete_url() search for the url in cache (hash table) 
                    2. If url not found in hash table, return with error code
                    3. If url found from chained list(in case of collision)
                       store the next of cache entry to be deleted in the prev->next
                    4. Remove the cache entry from the cache table
                      
Input Parameters  : 
             vptr : Pointer to User Structure
             url  : url to be searched
         iUrlLen  : Length of the url to be searched

Output Parameters : 

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/

int cache_delete_entry_from_user_tbl(VUser *vptr, unsigned char *url, int url_type, CacheTable_t *cacheTable_entry, CacheTable_t *prev_cache_link, int ihashIndex)
{
  NSDL2_CACHE(vptr, NULL, "Method called for url=[%s]", url);

  // If url found from chained list(in case of collision)
  // store the next of cache entry to be deleted in the prev->next
  if(NULL != prev_cache_link)
  {
    prev_cache_link->next = cacheTable_entry->next;
  }
  //If Linked List head is to be deleted,
  //Check if chain exist in the hashIndex, store the next in the chain at the hashIndex
  else 
  {
    vptr->httpData->cacheTable[ihashIndex] = cacheTable_entry->next;
  }

  // Reduce cache_num_entries
  DEC_CACHE_NUM_ENTERIES_COUNTERS(vptr);

  NSDL2_CACHE(vptr, NULL, "url->[%s] deleted from Cache table", url);

  // Freeing memory 
  NSDL2_CACHE(vptr, NULL, "Master Mode = %d, url type = %d", runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode, url_type);

  //if(runprof_table_shr_mem[vptr->group_num].gset.master_cache_mode == 0 || url_type == MAIN_URL) {
  //TODO : Need to check if we can reallu use ihashMasterIdx for testing MT mode.
  //Bcoz in some case we make it -1 when freeing data from MT(Eg. When cache entry is expired and dont have validators)
  //There was problm in case:
  //       If we got validators and in response of validation we got 200 with No-Store, so in cache_update_entries
  //       we have deleted MT entry for this user node and update ihashMasterIdx with -1, now when we are deleting user node
  //       then its treating mode as non MT mode.
  if(cacheTable_entry->ihashMasterIdx == -1){// This entry is not in master table
    NSDL2_CACHE(vptr, NULL, "Before deleting resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);
    cache_avgtime->cache_bytes_used -= cacheTable_entry->resp_len;
    DEC_CACHE_BYTES_USED_COUNTERS(vptr, cacheTable_entry->resp_len);
    // cacheTable_entry->resp_len = 0;
    NSDL2_CACHE(vptr, NULL, "After deleting resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);
    //4. Free all pointers stored in cacheTable_entry
    cache_free_data(cacheTable_entry, 1, 1, 1);
  }
  else
  {
    /*We will not free url and response from the user node
     * as both are pointing into Master node*/
    cache_free_data(cacheTable_entry, 1, 0, 1); // Must free URL as this is not copied to master table
  }

  NSDL2_CACHE(vptr, NULL, "cache_num_entries=%d, cache_bytes_used=%u",
                  cache_avgtime->cache_num_entries, cache_avgtime->cache_bytes_used);

  FREE_AND_MAKE_NULL_EX(cacheTable_entry, sizeof(CacheTable_t),"Freeing cache entry", -1);
  return NS_CACHE_SUCCESS;
}

int cache_delete_url(VUser *vptr, connection *cptr, unsigned char *url, int iUrlLen, int http_method)
{
   CacheTable_t *cacheTable_entry;
   CacheTable_t *prev_cache_link = NULL;
   int ihashIndex;
   unsigned int ihashValue;
   
   NSDL2_CACHE(vptr, NULL, "Method called for url=[%*.*s]", iUrlLen, iUrlLen, url);
   if(NULL == vptr || NULL == url)
   {
      NSDL1_CACHE(vptr, NULL, "Cannot Proceed 'cptr || vptr' found NULL");
      return NS_CACHE_ERROR;
   }

   //1. search for the url in cache hash table 
   cacheTable_entry = cache_url_found(vptr, url, iUrlLen, &prev_cache_link, &ihashIndex, &ihashValue, http_method, USER_NODE);
 
   //2. If url not found in hash table, return with error code
   if(NULL == cacheTable_entry)
   {
      NSDL2_CACHE(vptr, NULL, "url->[%*.*s] to delete not found in Cache table", iUrlLen, iUrlLen, url);
      return NS_CACHE_SUCCESS;
   }

  return(cache_delete_entry_from_user_tbl(vptr, url, cptr->url_num->proto.http.type, cacheTable_entry, prev_cache_link, ihashIndex));
}


/*************************************************************************************************
Description       : 1. cache_delete_url_frm_master_tbl() search for the url in cache (hash table) 
                    2. If url not found in hash table, return with error code
                    3. If url found from chained list(in case of collision)
                       store the next of cache entry to be deleted in the prev->next
                    4. Remove the cache entry from the cache table
                      
Input Parameters  : 
             vptr : Pointer to User Structure
             url  : url to be searched
         iUrlLen  : Length of the url to be searched

Output Parameters : 

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
*************************************************************************************************/

// This is passed cache table entry
int cache_delete_entry_frm_master_tbl(VUser *vptr, CacheTable_t *cacheTable_entry, CacheTable_t *prev_cache_link, int ihashIndex, int *free_emd, int free_resp)
{
   
  NSDL2_CACHE(vptr, NULL, "Method called. linked count = %d", cacheTable_entry->linked_count);

  cacheTable_entry->linked_count--;
  //Check for linked count.
  //If linked count is not 0 then do not free the cache entry
  //otherwise free cache entry
  if(cacheTable_entry->linked_count != 0)
  {
    *free_emd = 0;
    NSDL2_CACHE(vptr, NULL, "Linked count [%d] is not 0 so returning with free_emd = %d", 
                      cacheTable_entry->linked_count, *free_emd);
    return NS_CACHE_SUCCESS;
  }

  NSDL2_CACHE(vptr, NULL, "linked count = %d, going to delete in master table", cacheTable_entry->linked_count);

  //3. If url found from chained list(in case of collision)
  //   store the next of cache entry to be deleted in the prev->next
  if(NULL != prev_cache_link)
  {
    prev_cache_link->next = cacheTable_entry->next;
  }
  //If Linked List head is to be deleted,
  // store the next in the chain at the hashIndex (this can also be NULL)
  else 
  {
    vptr->httpData->master_cacheTable[ihashIndex] = cacheTable_entry->next;
  }

  // Freeing memory 
  NSDL2_CACHE(vptr, NULL, "Before deleting resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);
  cache_avgtime->cache_bytes_used -= cacheTable_entry->resp_len;
  DEC_CACHE_BYTES_USED_COUNTERS(vptr, cacheTable_entry->resp_len);
  NSDL2_CACHE(vptr, NULL, "After deleting resp_len = %d, cache_bytes_used = %u", cacheTable_entry->resp_len, cache_avgtime->cache_bytes_used);

  NSDL2_CACHE(vptr, NULL, "url->[%s] deleted from master cache table", cacheTable_entry->url);
  //Doing this as we need resp buf in do_data_processing
  //Setting resp_buf_head = NULl to retain the resp_head in cptr & 
  if(free_resp){
    cache_free_data_for_master_table_node(cacheTable_entry);
  }
  else{
    //freeing entry from cache
    cacheTable_entry->resp_buf_head = NULL;
  }

  //Free URL 
  FREE_AND_MAKE_NULL_EX(cacheTable_entry->url, (cacheTable_entry->url_len + 1), "Master table url", -1);
  
  //4. Free all pointers stored in cacheTable_entry
  //cache_free_data_for_master_table_node(cacheTable_entry);
  FREE_AND_MAKE_NULL_EX(cacheTable_entry, sizeof(CacheTable_t), "Freeing master cache entry", -1);

  return NS_CACHE_SUCCESS;
}

// This is passed url to search the cache table entry
int cache_delete_url_frm_master_tbl(VUser *vptr, int http_method, unsigned char *url, 
                             int iUrlLen, int *free_emd, int free_resp)
{
   CacheTable_t *cacheTable_entry = NULL;
   CacheTable_t *prev_cache_link = NULL;
   int ihashIndex;
   unsigned int ihashValue;
   
   NSDL2_CACHE(NULL, NULL, "Method called for url=[%*.*s]", iUrlLen, iUrlLen, url);
   if(NULL == vptr || NULL == url)
   {
      NSDL1_CACHE(NULL, NULL, "Cannot Proceed 'url || vptr' found NULL");
      return NS_CACHE_ERROR;
   }

   //Now search URL in MASter table
   cacheTable_entry = cache_url_found(vptr, url, iUrlLen, &prev_cache_link, &ihashIndex, &ihashValue, http_method, MASTER_NODE);

   //2. If url not found in hash table, return with error code
   if(NULL == cacheTable_entry)
   {
      NSDL2_CACHE(vptr, NULL, "url->[%*.*s] to delete not found in master cache table", iUrlLen, iUrlLen, url);
      return NS_CACHE_ERROR;
   }

   return(cache_delete_entry_frm_master_tbl(vptr, cacheTable_entry, prev_cache_link, ihashIndex, free_emd, free_resp));
}

/******************************************************************************************
Description       : cache_destroy() deletes all enteries from cache (hash table) 
                    and destroy the cache of the user.
                      
Input Parameters  : 
             vptr : Pointer to User Structure

Output Parameters : None

Return Value      : NS_CACHE_SUCCESS in case function executed successfully
                    NS_CACHE_ERROR in case any failure occurs in the function
************************************************************************************************/
int cache_destroy(VUser *vptr)
{
   int ihashIndex;
   int free_emd;
   CacheTable_t *cache_entry;
   CacheTable_t *next_cache_entry;
#ifdef NS_DEBUG_ON
   int link=0;
#endif

   NSDL2_CACHE(NULL, NULL, "Method Called");
   if(NULL == vptr)
   {
      NSDL1_CACHE(NULL, NULL, "Cannot Proceed 'cptr || vptr' found NULL");
      return NS_CACHE_ERROR;
   }
   if(NULL == vptr->httpData->cacheTable)
   {
      NSDL1_CACHE(vptr, NULL, "Nothing to delete..Cache Table not existing");
      return NS_CACHE_ERROR;
   }
   //This is done to make the max cache entry counter 0 at the time when we free cache.
   vptr->httpData->max_cache_entries = 0;

   NSDL2_CACHE(vptr, NULL, "value in vptr->httpData->cache_table_size = %d", vptr->httpData->cache_table_size);
   for(ihashIndex = 0; ihashIndex < vptr->httpData->cache_table_size; ++ihashIndex)
   {
      cache_entry = NULL;
      next_cache_entry = NULL;
      if (NULL != vptr->httpData->cacheTable[ihashIndex])   
      {      
         NSDL3_CACHE(vptr, NULL, "FREE ENTRY START Hashindex=[%d]", ihashIndex);
         NSDL3_CACHE(vptr, NULL, "Deleting urls at Hashindex=[%d]", ihashIndex);
         cache_entry = vptr->httpData->cacheTable[ihashIndex];
         NSDL3_CACHE(vptr, NULL, "cache_entry=[%p]", cache_entry);
         
         //freeing chained linked list
         while(NULL != cache_entry->next)
         {
            // Reduce cache_num_entries
            DEC_CACHE_NUM_ENTERIES_COUNTERS(vptr);

            next_cache_entry = cache_entry->next;
            NSDL3_CACHE(vptr, NULL, "Deleting url=[%*.*s] at Hashindex=[%d] in Chained Link=[%d]", 
                               cache_entry->url_len, cache_entry->url_len, cache_entry->url,ihashIndex, ++link);
            NSDL3_CACHE(vptr, NULL, "Going to delete url in master table");
            //if(NS_CACHE_ERROR != cache_delete_url_frm_master_tbl(vptr, cache_entry->http_method, cache_entry->url, cache_entry->url_len, &free_emd, 1))
            if(cache_entry->ihashMasterIdx != -1){
              cache_delete_url_frm_master_tbl(vptr, cache_entry->http_method, cache_entry->url, 
                                        cache_entry->url_len, &free_emd, 1);
              //Free all headers.
              //Dont free resp as it is done by cache_destroy_url_frm_master_tbl()
              //This is for user table, free user tabel node
              cache_free_data(cache_entry, 1, 0, 1);
            }
            else
            {
              //URL not found in master table - Means mode 0 or MAin url
              // Freeing memory 
              cache_avgtime->cache_bytes_used -= cache_entry->resp_len;
              DEC_CACHE_BYTES_USED_COUNTERS(vptr, cache_entry->resp_len);
	      cache_free_data(cache_entry, 1, 1, 1);
            }
            NSDL2_CACHE(vptr, NULL, "cache_num_entries = %d, Total = %u", cache_avgtime->cache_num_entries, cache_avgtime->cache_bytes_used);
            cache_entry->next = NULL;
            FREE_AND_MAKE_NULL_EX(cache_entry, sizeof(CacheTable_t), "cache_entry", -1);
            cache_entry = next_cache_entry;            
            NSDL3_CACHE(vptr, NULL, "cache_entry=[%p]", cache_entry);
         }//While loop
      
         // Reduce cache_num_entries
         DEC_CACHE_NUM_ENTERIES_COUNTERS(vptr);
         //if(NS_CACHE_ERROR != cache_delete_url_frm_master_tbl(vptr, cache_entry->http_method, cache_entry->url, cache_entry->url_len, &free_emd, 1)) 
         //{
         if(cache_entry->ihashMasterIdx != -1)
         {
           NSDL3_CACHE(vptr, NULL, "Mode 1 is found, going to delete url in master table");
           cache_delete_url_frm_master_tbl(vptr, cache_entry->http_method, cache_entry->url, 
                                        cache_entry->url_len, &free_emd, 1);
           //Free all headers.
           cache_free_data(cache_entry, 1, 0, 1);
         }
         else
         {
           //URL not found in master table - Means mode 0 or MAin url
           // Freeing memory 
           cache_avgtime->cache_bytes_used -= cache_entry->resp_len;
           DEC_CACHE_BYTES_USED_COUNTERS(vptr, cache_entry->resp_len);
	   cache_free_data(cache_entry, 1, 1, 1);
         }
         cache_entry->next = NULL;
         
         FREE_AND_MAKE_NULL_EX(cache_entry, sizeof(CacheTable_t), "cache_entry", -1);
         //free(cache_entry);
         //cache_entry=NULL;
         NSDL3_CACHE(vptr, NULL, "FREE ENTRY END Hashindex=[%d]", ihashIndex);
         
         //Delete the array of pointer
         vptr->httpData->cacheTable[ihashIndex] = NULL;
      }
   }

   FREE_AND_MAKE_NULL_EX(vptr->httpData->cacheTable, vptr->httpData->cache_table_size,"vptr->httpData->cacheTable", -1);
   NSDL2_CACHE(vptr, NULL, "Exiting Method");
   return NS_CACHE_SUCCESS;
}
