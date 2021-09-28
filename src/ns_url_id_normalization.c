/********************************************************************************
 * File Name            : ns_url_id_normalization.c
 * Author(s)            : Prachi Kumari
 * Date                 : 23 June 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              :  TO normalized index of dynamic urls.
 *                         Example:
 *                         Earlier: format of dynamic urls in urt.csv 
 *                           18982,16777216,0,/caching/url_max_age2.html
 *                           18982,16777220,0,/caching/url_max_age2.html
 *
 *                         After Normalization: format of dynamic urls in urt.csv
 *                           18983,6,0,/caching/url_max_age2.html 
 *                           18983,6,0,/caching/url_max_age2.html
 *
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 * ********************************************************************************/

//---------------- INCLUDE SECTION BEGINS ----------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include "nslb_alloc.h"
#include "nslb_hash_code.h"

#include "ns_data_types.h"
#include "logging_reader.h"

#include "nslb_log.h"

#include "nslb_dyn_hash.h"

#include "ns_url_id_normalization.h"
#include "nslb_util.h"
#include "ns_exit.h"
/*
 Both NVM URL Id and Normalized URL is assumed to be unsigned int so that we CANNOT return -1 in case of errors.

 Non normalized ID which is stored in URT and URC should BE unigned int as 4th felds is NVM id which is from 0 to 255
 but ns_url_hash.c is assuming int.

 So issue may come ONLY for NVM 255 and if URL ID of that NVM reaches FFFFFF (16777215) (Approx 16.5 Million)  which is not likely
 Also if NVM is >= 128 then URL will print -ve number
   nvm_id = 128 (80)
   url_id = -2147483632 (80000010)

 int url_id = -1 is all bits ON (0xffffffff)

 So we in this file we are assuming both normalized id and nvm url id as unsinged int
*/

//---------------- INCLUDE SECTION ENDS ----------------------------------------

//---------------- DECLARATION SECTION BEGINS ----------------------------------------

#define MAX_NVM_NUM 255

#define INIT_NVM_TABLE_SIZE 1000
#define G_URL_INDEX_ENTRIES 1000

#define DELTA_NVM_TABLE_SIZE 1000 

#define IS_URL_STATIC(url_id)  (!(url_id & 0xFF000000))

#define GET_URL_PART_NVM_ID(url_index)  (url_index >> (8*3) & 0xff)
#define GET_URL_PART_URL_ID(url_index)  (url_index >> (8*0) & 0x00ffffff)

// NVM table for nvm url id to norm id mapping
static int total_nvm_table_entries[MAX_NVM_NUM]; 

// Normalization Table
static DyamicUrlHashTableEntry **normUrlTable=NULL;

// URL normalization table
static UrlIndexTable *g_url_index_table[MAX_NVM_NUM];  // Array of pointer

static unsigned int max_static_url_id = 0; // Dyn URL id start after this ID
static unsigned int num_dyn_urls = 0; // Number of dynamic URLs. Used to create dyn url id
static int NormalizedTableSize = 0;
  


//---------------- DECLARATION SECTION ENDS ----------------------------------------

//This method is used to return the next 2^n of the number passed to it. if the no is already a power of to then it will 
//the same no other wise it will return the next power of two.
static int change_to_2_power_n(int size_value)
{
int i;
int count = 0;
int pos = 0;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. size_value = %d", size_value);

  if(size_value >= 65536) //TODO: Took limit as MAX_CACHE_TABLE_SIZE need to check later
  {
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "limit exceded. size_value = %d", 65536); 
    return 65536;
  }

  //Here we are counting no. of ones in size value to check whether it a power of 2 or not 
  // Optmize
  for(i = 0; i < 32; i++)
  {
    int ii = pow(2,i);
    if(size_value & ii) 
    {
      count++;
      pos = i;
    }
  }
  //count > 1 means it is not a power of 2. so increasing it to next power of 2
  if(count > 1)
  {
    pos = pos +1;
    size_value = pow(2,pos);
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa,"Method called. after changing to power of 2 size_value = %d", size_value);
  }
  return size_value;
}

static void url_norm_init_nvm_table() {
  unsigned char nvm_id = 0;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called");

  // TODO: Loop this for number of NVMs used in the test
  while(nvm_id < MAX_NVM_NUM) {
    NSLB_MALLOC_AND_MEMSET(g_url_index_table[nvm_id], (sizeof(UrlIndexTable) * INIT_NVM_TABLE_SIZE), "NVM table creation", -1,log_fp);
    total_nvm_table_entries[nvm_id] = INIT_NVM_TABLE_SIZE;
    nvm_id++;	
  }
}


/* Allocating Normalization table*/
static void url_norm_init_norm_table() {

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called");
  NormalizedTableSize = change_to_2_power_n(DynamicTableSizeiUrl);

  // Allocate normalization table which is array of pointers
  NSLB_MALLOC_AND_MEMSET(normUrlTable, (sizeof(void *) * NormalizedTableSize), "Normalization table creation", -1, log_fp);
}


/* Init called  from logging_reader.c */
//This function is called from two places 
//1. from logging reader.In this case max_static_url_ids will 0
//2. from url_id_norm for NetCloud, In NC we know the static ids so just set it
void url_norm_init(int max_static_url_ids) {

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. Dynamic URL Table SIze = %d", NormalizedTableSize);
  max_static_url_id = max_static_url_ids;
  url_norm_init_nvm_table();
  url_norm_init_norm_table();
}

/* Called from logging_reader.c */
int set_max_static_url_index(unsigned int url_id)
{
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. Url_id = %u (0x%FF)", url_id, url_id);

  if(url_id > max_static_url_id)
  {
    max_static_url_id = url_id;
    LIB_DEBUG_LOG(4, debug_level_reader, debug_file_nsa, "max_static_url_id = %u", max_static_url_id);
  }

  return 0;
}

// This function is use too create & init new node for dynamic url table
// Returns pointer to the dynamic table node created
static DyamicUrlHashTableEntry *url_norm_dynamic_table_init_entry(unsigned char *url, int UrlLen, unsigned int iHashValue) {

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. url = %p, UrlLen = %d, iHashValue = %u", url, UrlLen, iHashValue);

  static int first_time = 1;
  DyamicUrlHashTableEntry *tmpUrlEntry;

  NSLB_MALLOC_AND_MEMSET(tmpUrlEntry, sizeof(DyamicUrlHashTableEntry), "Dynamic Url Hash Table entry", -1, log_fp);

  if ((tmpUrlEntry->url = nslb_copy_into_big_buf((char *)url, UrlLen)) == -1) { 
    NS_EXIT(-1, "Error: - Error allocating memory for URL, URL is %s", url);
  }
 
  if(first_time)
  {
    fprintf(stderr, "Info: Max static url id is %d\n", max_static_url_id);
    LIB_DEBUG_LOG(1, debug_level_reader, debug_file_nsa, "Max static url id is %d", max_static_url_id);     
    first_time = 0;
  }
  
  tmpUrlEntry->url_len = UrlLen;
  num_dyn_urls++;
  tmpUrlEntry->d_url_index = max_static_url_id + num_dyn_urls;  // Normalized ID will start after max static url ID.
  tmpUrlEntry->iHashValue = iHashValue;
  tmpUrlEntry->next = NULL;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Dynamic normalized URL index = %d, num_dyn_urls = %d", tmpUrlEntry->d_url_index, num_dyn_urls);
  
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method exiting");

  return tmpUrlEntry;
}


/*******************************************************************************************
 * Description  : This function will return the d_url_index of dyanmic url table
 ******************************************************************************************/

static unsigned int add_dynamic_url(unsigned char *url, int urlLen, int ihashIndex, int ihashValue) 
{
  DyamicUrlHashTableEntry *urlTable_entry;
  DyamicUrlHashTableEntry *pUrlEntry;
  
  //Used for logging purposes to store the chain index in case of collision,
  //starting with 0 for entry which is stored directly in hashtable
  int link=0;
  int iRetVal;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. url = %p, urlLen = %d, ihashIndex = %d, ihashValue = %d", url, urlLen, ihashIndex, ihashValue);

  urlTable_entry = url_norm_dynamic_table_init_entry(url, urlLen, ihashValue);

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, " AFTER url_norm_dynamic_table_init_entry ");

  // checks whether the any other entry is there at this index
  if(NULL == normUrlTable[ihashIndex]) {
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, " IF NULL ");
    normUrlTable[ihashIndex] = urlTable_entry;
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Stored at Hashindex=[%d] url=%s ",ihashIndex, (char *)url);
  }

  //If hashindex is not free, create chain on that index using linked list
  //and stores the data on that chained link.

  else {
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "  INSIDE ELSE ");   
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "ihashIndex = %d ", ihashIndex);
    pUrlEntry = normUrlTable[ihashIndex];
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called %p", pUrlEntry);
 
    iRetVal = nslb_url_hash_dynamic_list_add(pUrlEntry, urlTable_entry, &link);
    // TODO - Add check for iRetVal ?? 
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Stored at chained link=%d Hashindex=[%d] url=%s ", link, ihashIndex, (char *)url);
  }

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method exiting");
   LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "return val : urlTable_entry->d_url_index = %u", urlTable_entry->d_url_index);
  return urlTable_entry->d_url_index;
}


/* Check if the url exists in dynamic url table or not
   if exist the return the d_url_index
   otherwise return the index where the url is added in dynamic url table
   Returns d_url_index if found, else return URL_ERROR
   Return values hash & ihashValue for adding url in hash table, in case url is not found in url table*/
unsigned int get_or_gen_url_norm_id(UrlTableLogEntry* urlptr, int *dyn_flag) {

  int num_url_found = 1;
  unsigned int url_index;
  //Pointer to url entry found in url table
  DyamicUrlHashTableEntry  *urlTable_entry = NULL;
  //Pointer to traverse the chain in case of collision
  DyamicUrlHashTableEntry *pUrlEntry = NULL;
  //Used for logging purposes to store the chain index in case of collision,
  //starting with 0 for entry which is stored directly in hashtable
  int link = 0;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method Called. url_hash_id=%u", urlptr->url_hash_id);  

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "normUrlTable=%p", normUrlTable);
  pUrlEntry = normUrlTable[urlptr->url_hash_id];
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "pUrlEntry = %p", pUrlEntry);

  // If hashindex contains node urlTable_entry
  // search the url entry in the list by matching the url to be searched
  // with the url in the chained linked list.
  // Traverse the linked list to reach the searched url

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "before while");

  while(NULL != pUrlEntry)
  {
    num_url_found = nslb_hash_match_value((u_ns_char_t *)RETRIEVE_BUFFER_DATA_NORM(pUrlEntry->url), pUrlEntry->iHashValue, (unsigned char*)urlptr->url_name, urlptr->len, urlptr->url_hash_code);
  
    //Loop through till url is found
    //If url found in chained list, return the url index
    if (num_url_found != 0)
    {
      LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "num_url_found = %d, pUrlEntry = %p", num_url_found, pUrlEntry);
      urlTable_entry = pUrlEntry;
      LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "urlTable_entry = %p", urlTable_entry);
      return urlTable_entry->d_url_index; // Found this entry
    }

    pUrlEntry = pUrlEntry->next;
    ++link;
  } 

 url_index = add_dynamic_url((unsigned char*)urlptr->url_name, urlptr->len, urlptr->url_hash_id, urlptr->url_hash_code);
  *dyn_flag = 1;
  
 
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method exiting with url_index = %d", url_index);    
  return url_index;
}


// Index NVM table using NVM URl Index, if found will return the normalized id.
// If 0 is returned, then we assumed that norm id is not available as dyn url norm id > 0 
unsigned int get_norm_id_from_nvm_table(unsigned int nvm_url_index, unsigned int nvm_index)
{
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. nvm_url_index = %u, nvm_index = %u", nvm_url_index, nvm_index);

  UrlIndexTable *nvmIndexPtr = g_url_index_table[nvm_index];
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method Ends");
  return *(nvmIndexPtr + nvm_url_index);
}


// Called from logging_reader.c , while filling URL_RECORD.
unsigned int get_url_norm_id_for_urc(unsigned int id, int *found) {

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. id = %u (0x%X)", id, id);

  *found = 1;
  unsigned int nvm_id = GET_URL_PART_NVM_ID(id);  // NVM ID


  if(nvm_id == 0) // static, no normalization is needed.
    return id; // This can be 0 for first static URL

  unsigned int nvm_url_id = GET_URL_PART_URL_ID(id); // first three bytes are URL Index

  // It can be 0, so set found to 0 if it is 0
  unsigned int ret_val_for_urc = get_norm_id_from_nvm_table(nvm_url_id, nvm_id); // finding normalized url index.
  if(ret_val_for_urc == 0)
    *found = 0;

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method End. with ret_val_for_urc = %u, found = %d", ret_val_for_urc, *found);
  return ret_val_for_urc;
}


/*******************************************************************************************
 * Description  : Called from logging_reader.c
 *                Based on fourth byte of url id, further calculation will be done.
 *                
 *                Byte 4 is NVM Id and Bytes 1-3 are Url Index
 ******************************************************************************************/



static int get_url_norm_id_for_url(int nvm_id, int nvm_url_index, UrlTableLogEntry* utptr, int *is_new_url)
{

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "nvm_url_index = %d, total_nvm_table_entries[%d] = %d", nvm_url_index, nvm_id, total_nvm_table_entries[nvm_id]);

  //If Index is more than current size of nvm table, then ralloc table
  if (nvm_url_index >= total_nvm_table_entries[nvm_id])
  {
    
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Realloc of nvm table ");
    NSLB_REALLOC(g_url_index_table[nvm_id], ((DELTA_NVM_TABLE_SIZE +total_nvm_table_entries[nvm_id])* sizeof(UrlIndexTable)), "realloc nvm table", 1, log_fp);
    memset(g_url_index_table[nvm_id] + total_nvm_table_entries[nvm_id], 0, (sizeof(UrlIndexTable) * DELTA_NVM_TABLE_SIZE));
    total_nvm_table_entries[nvm_id] += DELTA_NVM_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_url_id = get_norm_id_from_nvm_table(nvm_url_index, nvm_id);

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "After get_norm_id_from_nvm_table :norm_url_id = %u", norm_url_id);
  if (norm_url_id != 0) // Found
  {
    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Found: norm_url_id = %u", norm_url_id);
    return norm_url_id; 
  }

  // Generation of Normalized Index 
  UrlIndexTable *nvmIndexPtr = g_url_index_table[nvm_id];
  unsigned  int nor_idx = get_or_gen_url_norm_id(utptr, is_new_url);
  
 *(nvmIndexPtr + nvm_url_index) = nor_idx;
  
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Generated Normalized ID is: (%u)",g_url_index_table[nvm_id][nvm_url_index]);

  return nor_idx;   // Return normalized ID
}

unsigned int get_url_norm_id(UrlTableLogEntry* utptr, int *is_new_url)  {

  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Method called. utptr = %p, is_new_url = %p", utptr, is_new_url);

  if(IS_URL_STATIC(utptr->url_id))
  {
    *is_new_url = 1; // Static are always treated like new URL as that this record gets added in URL Table

    LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Returning utptr->url_id = %u", utptr->url_id);

    set_max_static_url_index(utptr->url_id);
    return(utptr->url_id);
  }
  LIB_DEBUG_LOG(2, debug_level_reader, debug_file_nsa, "Returning utptr->url_id = %u", utptr->url_id);

  unsigned int nvm_id = GET_URL_PART_NVM_ID(utptr->url_id); // NVM ID
  unsigned int nvm_url_index = GET_URL_PART_URL_ID(utptr->url_id);   // first three bytes are URL Index

  return(get_url_norm_id_for_url(nvm_id, nvm_url_index, utptr, is_new_url));
}

unsigned int get_url_norm_id_for_generator(UrlTableLogEntry* utptr, int gen_id, int gen_url_index, int *is_new_url)  
{
  // Treating all URLs from generators as dynamic URL to make code easy
  return(get_url_norm_id_for_url(gen_id, gen_url_index, utptr, is_new_url));
}

// To destroy the dyanamic url table
int dynamic_norm_url_destroy() {
  // Free all NVM tables
  // Free URL normalization table 
  return nslb_dynamic_url_hash_destroy(normUrlTable, NormalizedTableSize);
}
 
