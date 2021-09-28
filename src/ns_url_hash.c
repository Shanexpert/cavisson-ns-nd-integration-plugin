/************************************************************************************************
 * File Name            : ns_url_hash.c
 * Author(s)            : Abhishek Mittal
 * Date                 : 
 * Copyright            : (c) Cavisson Systems
 * Purpose              : 
 Start - Code to add all URLs parsed from all scripts in the hash table 
 *                        
 * Modification History :
 *              <Shilpa>, <12/04/12>, 
 *              <Prachi>, <28/06/2012>,
                
 * File Sections
 *  - INCLUDE
 *  - DECLARATION
 *  - UTILITY
 *  - STATIC_URL_HASH_TABLE
 *  - DYNAMIC_URL_HASH_TABLE
 *  - KEYWORD_PARSING
 ***********************************************************************************************/

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
#include <regex.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include "nslb_hash_code.h"
#include "ns_url_hash.h"
#include "ns_data_types.h"
#include "ns_exit.h"
#ifndef TEST
  #include "ns_cache_include.h"
  #include "ns_http_cache_store.h"
  #include "util.h"
  #include "logging.h"
#endif
#include "ns_auto_fetch_parse.h"
#include "ns_global_settings.h"
#include "nslb_dyn_hash.h"
#include "ns_error_msg.h"
//---------------- INCLUDE SECTION ENDS ----------------------------------------

//---------------- DECLARATION SECTION BEGINS ----------------------------------------
#define MAX_BUF_SIZE_FOR_URL 1024
#define MAX_BUF_SIZE_FOR_URL_PATH 4096 
//#define DYNAMIC_URL_TABLE_SIZE 256 
#define DELTA_URL_ENTRIES 64 
//#define INIT_URL_ENTRIES 64
#define MAX_URL_LEN_FOR_HASH 255
#define DEFAULT_URL_HASH_SIZE 256
#define DEFAULT_DYNAMIC_HASH_THRESHOLD 5000
#define DEFAULT_SEARCH_TIME_THRESHOLD 2

#ifndef CAV_MAIN
static int total_url_entries;
static int max_url_entries;
static FILE *url_fp;
Str_to_hash_code_ex url_hash_func;
#else
static __thread int total_url_entries;
static __thread int max_url_entries;
static __thread FILE *url_fp;
__thread Str_to_hash_code_ex url_hash_func;
#endif

/* Usage: For storing url information of urls in the script, 
   Creation: Populated at the time of parsing the script
   Destroy: freed after creating hash table by parent process
*/
typedef struct StaticUrlHashTableEntry_s{
  ns_tmpbuf_t url; /* offset of temp buf */
  int url_len;
} StaticUrlHashTableEntry;

/* Usage: This structure is a parallel to StaticUrlHashTableEntry.
          Used to store the url id of request_table static Url table will have single entry 
          for duplicate urls across script. However there will be multiple entries for the same 
          in request table and hence will have different url id.
   Creation: Populated at the time of parsing the script
   Destroy: 
*/
typedef struct UrlIndex_s{
  int s_url_index;  //Index in case of static url
} UrlIndex_t;

#ifndef CAV_MAIN
static StaticUrlHashTableEntry *staticUrlTable;
static UrlIndex_t *Url_Index;
static DyamicUrlHashTableEntry **dynamicUrlTable = NULL;
#else
static __thread StaticUrlHashTableEntry *staticUrlTable;
static __thread UrlIndex_t *Url_Index;
static __thread DyamicUrlHashTableEntry **dynamicUrlTable = NULL;
#endif

//---------------- DECLARATION SECTION ENDS ----------------------------------------

//---------------- UTILITY FUNCTIONS SECTION BEGINS ----------------------------------------

//Will consider dynamic urls without query parameters
//i.	/abc/a.html will be same as /abc/a.html?name=Neeraj
//ii.	/abc/a.html will be same as /abc/a.html#name=Neeraj
inline static int handle_query_param(char *url, int iUrlLen)
{
  char *query_str_chr;

  NSDL4_PARSING(NULL, NULL, "url before truncating query parameter len=%d, url=[%*.*s]", iUrlLen, iUrlLen, iUrlLen, url);
  //Many urls have jspjsessionid=1234; which is dynamic. 
  query_str_chr = strpbrk (url, ";?#");

  // Some times, URL is /?address=noida but can never to ?address=noida. So iUrlLen will never be 0
  if(query_str_chr != NULL) 
    iUrlLen = (query_str_chr - url); //Len till query param 

  NSDL4_PARSING(NULL, NULL, "url till query parameter len=%d, url=[%*.*s]", iUrlLen, iUrlLen, iUrlLen, url);
  return iUrlLen;
}

/* gperf is escaping some sp. characters - backslash(\) and quotes(") by preceding them with backslash(\)
   So to match the url strings, we also need to do the same.
   Hence function handle_special_char() is replacing \ by \\ and " by \" in URL passed
   url is the input string of this function and buf is the output string of this function*/
//Return Value: Length of string after sp. chars replaced
static int handle_special_char(u_ns_char_t *url, u_ns_char_t *str_sp_char_replaced, int len)
{
  u_ns_char_t *in_ptr, *out_ptr;

  NSDL4_PARSING(NULL, NULL, "Method called. len=%d, url=%*.*s", len, len, len, url);
  in_ptr = (u_ns_char_t *)url;
  out_ptr = str_sp_char_replaced;

  while(*in_ptr != '\0')
  {
    if(*in_ptr == '"' || *in_ptr == '\\')
    {
      *out_ptr = '\\';
      len++;
    }
    *out_ptr = *in_ptr;
    in_ptr++;
    out_ptr++;
  }
  *out_ptr = '\0';
  NSDL4_PARSING(NULL, NULL, "Url after replacing sp. chars len=%d, url=%*.*s", len, len, len, (char *)str_sp_char_replaced);
  return len;
}


/* We are useing gpref to make hash function
 * And gpref is not support the pipe (|),
 * So pipe(|) is replace by underscore(_) */
  /* Find pipe in the url and replace by the underscore
   * eg: 
   * Input : /frameimagehandler/universal/frameimage.jpg?frame=[FAP:0+PRT:[PAP=3559533|PRW=16|PRH=12|PIP=%5c26%5c2685%5cTSXUD00Z.jpg|LFN=NGSPOD   
   * Output : /frameimagehandler/universal/frameimage.jpg?frame=[FAP:0+PRT:[PAP=3559533_PRW=16_PRH=12_PIP=%5c26%5c2685%5cTSXUD00Z.jpg_LFN=NGSPOD*/
void check_and_replace_pipe(u_ns_char_t *url, u_ns_char_t *str_pipe_replaced, int len)
{
  u_ns_char_t *in_ptr, *out_ptr;

  NSDL4_PARSING(NULL, NULL, "Url  len=%d, url=%*.*s", len, len, len, url);
  in_ptr = url;
  out_ptr = str_pipe_replaced;

  while(*in_ptr != '\0')
  {
    if(*in_ptr == '|')
      *out_ptr = '_';
    else
      *out_ptr = *in_ptr;      
    in_ptr++;
    out_ptr++;
  }
  *out_ptr = '\0';
  NSDL4_PARSING(NULL, NULL, "Url after replacing pipe. len=%d, url=%*.*s", len, len, len, str_pipe_replaced);
}

//Retrieves url from shared memory in case its a static url, else retrieve url from big_buf
inline static char *RETRIEVE_URL(DyamicUrlHashTableEntry *pUrlEntry) 
{                             
  NSDL3_PARSING(NULL, NULL, "Method Called");
  if (((pUrlEntry->d_url_index) & DYNAMIC_URL_MASK))                          
  {
    NSDL3_PARSING(NULL, NULL, "Dynamic Url %s", RETRIEVE_BUFFER_DATA(pUrlEntry->url));
    return RETRIEVE_BUFFER_DATA(pUrlEntry->url);                                 
  }
  else                                                                    
  {
    NSDL3_PARSING(NULL, NULL, "Static Url %s", RETRIEVE_SHARED_BUFFER_DATA(pUrlEntry->url));
    return RETRIEVE_SHARED_BUFFER_DATA(pUrlEntry->url);                         
  }
}


//---------------- UTILITY FUNCTIONS SECTION ENDS ----------------------------------------


//---------------- STATIC_URL_HASH_TABLE SECTION BEGINS ----------------------------------------

/* Function url_hash_create_file is use for open the url_hash file 
   and file is open in append mode*/
int url_hash_create_file(char *url_file)
{
  char url_file_path[MAX_BUF_SIZE_FOR_URL_PATH];
  NSDL3_PARSING(NULL, NULL, "Method called. url_file = %s, static_url_hash_mode =%d", url_file, global_settings->static_url_hash_mode);

  if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
  {
    sprintf(url_file_path,"%s/%s", g_ns_tmpdir, url_file);
    if((url_fp = fopen(url_file_path, "a")) == NULL)
    {
      NSDL1_PARSING(NULL, NULL, "Error in open %s file", url_file_path);
      write_log_file(NS_SCENARIO_PARSING, "Failed to open %s file, error = %s", url_file_path, nslb_strerror(errno));
      NS_EXIT(-1, CAV_ERR_1000006, url_file_path, errno, nslb_strerror(errno));
    }
  }
  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return URL_SUCCESS;
}


/* Function url_hash_close_file is use for close the url_hash file */
static int url_hash_close_file()
{
  NSDL3_PARSING(NULL, NULL, "Method called");
  if(url_fp != NULL)
  {
    if(fclose(url_fp) < 0)
    { 
      NS_EXIT(-1, CAV_ERR_1000021, "url.txt", errno, nslb_strerror(errno));
    }
  }
 
  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return URL_SUCCESS;
}


// used for allocating the memory in the bigbuf for static url table
static inline void alloc_mem_for_static_urltable ()
{
  total_url_entries = 0;
  NSDL3_PARSING(NULL, NULL, "Method Called. static_url_hash_mode =%d", global_settings->static_url_hash_mode);
    
  MY_MALLOC(staticUrlTable, global_settings->static_url_table_size * sizeof(StaticUrlHashTableEntry), "staticUrlTable", -1);
  MY_MALLOC(Url_Index, global_settings->static_url_table_size * sizeof(UrlIndex_t), "staticUrlTable", -1);
  max_url_entries = global_settings->static_url_table_size;
  NSDL3_PARSING(NULL, NULL, "Method exiting max_url_entries = %d ", max_url_entries);
}

//For Allocating extra memroy for Static Url Table than INIT_URL_ENTRIES
static inline int url_hash_create_table_entry(int *row_num)
{
  NSDL4_PARSING(NULL, NULL, "Method called");

  if (total_url_entries == max_url_entries)
  {
    MY_REALLOC_EX(staticUrlTable ,(max_url_entries + DELTA_URL_ENTRIES) * sizeof(StaticUrlHashTableEntry), (max_url_entries * sizeof(StaticUrlHashTableEntry)),"staticUrlTable", -1);
    MY_REALLOC_EX(Url_Index,(max_url_entries + DELTA_URL_ENTRIES) * sizeof(UrlIndex_t), (max_url_entries * sizeof(StaticUrlHashTableEntry)),"staticUrlTable", -1);
    max_url_entries += DELTA_URL_ENTRIES;
  }
  *row_num = total_url_entries++;
  NSDL4_PARSING(NULL, NULL, "Method exiting, row_num=%d", *row_num);
  return (URL_SUCCESS);
}


// This method is called from two places - 
//   1. To check the if url already exist in staticUrlTable
//   2. To get the index for creating static hash based on the occurance of the url
// This function returns the url index from static url array on success, URL_ERROR otherwise
int url_hash_find_idx(char* url)
{
  int i;

  NSDL4_PARSING(NULL, NULL, "Method called. URL = %s, total_url_entries=%d ", url, total_url_entries);
  for (i = 0; i < total_url_entries; i++)
  {
    //NSDL4_PARSING(NULL, NULL, "staticUrlTable[%d].url=[%s]", i, RETRIEVE_BUFFER_DATA(staticUrlTable[i].url));
    if(strlen(url) == staticUrlTable[i].url_len)
    {
      //NSDL4_PARSING(NULL, NULL, "url_len=%d, i=%d, staticUrlTable_len=%d", strlen(url), i, staticUrlTable[i].url);
      if (!strcmp(RETRIEVE_TEMP_BUFFER_DATA(staticUrlTable[i].url), (char *)url))
      {
        NSDL4_PARSING(NULL, NULL, "Static Table Index - staticUrlTable[%d].url = %s", i,RETRIEVE_TEMP_BUFFER_DATA(staticUrlTable[i].url));
        return i;
      }
    }
  }
  NSDL4_PARSING(NULL, NULL, "URL = %s does not found in the urlTable", url);
  return URL_ERROR;
}

void alloc_mem_for_urltable()
{
  NSDL3_PARSING(NULL, NULL, "Method Called. static_url_hash_mode =%d", global_settings->static_url_hash_mode);
    
  if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
    alloc_mem_for_static_urltable();
  else if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_DYNAMIC_HASH)
    url_hash_dynamic_table_init();

  NSDL3_PARSING(NULL, NULL, "Exitting Method");

}
/*
  Called from script parsing for all main & embedded url legacy/ctype.
  Creates entry in static url table
  & stores its request table url_id(row_num) in index table at appropraite index
  
   url_id will be the (row_num) of request table
   Duplicate urls will have different url_id in request table, 
   however duplicate urls will have same index in static url table
   E.g.
   		request url table id	static url table id
   URL1		0			0
   URL2		1			1
   URL3		2			2
   URL4		3			3
   URL1		4			0
   URL5		5			4
 
   when the static url table id is mapped with requst url table id, the mapping is wrong
   because static url table id 4 is mapped with request url table id 5 URL1
*/

void url_hash_add_in_static_hash_table(u_ns_char_t *complete_url, int url_id)
{
  int row_num;
  u_ns_char_t url_sp_char_replaced[MAX_LINE_LENGTH + 1];
  u_ns_char_t *url;
  int iUrlLen = 0;

  NSDL3_PARSING(NULL, NULL, "Method called. complete_url = %s url_id = %d, static_url_hash_mode=%d",       
                          complete_url, url_id, global_settings->static_url_hash_mode);

   // We need to escape few char. See comments before this method
   iUrlLen = handle_special_char(complete_url, url_sp_char_replaced, strlen((char *)complete_url));
   url = url_sp_char_replaced;

   //gpref supports only 255 character, so if url len is more then 255 truncate the url by 255 character
   if(iUrlLen > MAX_URL_LEN_FOR_HASH)
   {
     url[MAX_URL_LEN_FOR_HASH] = '\0';
     iUrlLen = MAX_URL_LEN_FOR_HASH;
   }

   /* Put url into url_hash file and buf into bigbuf array */

   /* We are useing gpref to make hash function and using pipe as field separator
   * So pipe(|) is to replaced by underscore(_) */
   // Since this method replaces pipe by one character only, we are using url as both in and out buf
   check_and_replace_pipe(url, url, iUrlLen);
     
   if ((row_num = url_hash_find_idx((char *)url)) == -1)
   {
     if (url_hash_create_table_entry(&row_num) == -1)
     {
        NS_EXIT(-1, "Error: - Error allocating memory for URL, URL is %s", url);
      }
      fprintf(url_fp, "%s\n", (char *)url);
   
      staticUrlTable[row_num].url_len = iUrlLen;
      Url_Index[row_num].s_url_index = url_id; // url_id can be different then index in hash table due to duplicate URL

      if ((staticUrlTable[row_num].url = copy_into_temp_buf((char *)url, staticUrlTable[row_num].url_len)) == -1)
      {
        NS_EXIT(-1, "Error: - Error allocating shared memory for URL, URL is %*.*s", iUrlLen, iUrlLen, url_sp_char_replaced);
      }
      NSDL3_PARSING(NULL, NULL, "Adding URL in static hash table: staticUrlTable[%d].url_len = %d", row_num, staticUrlTable[row_num].url_len );
      NSDL3_PARSING(NULL, NULL, "Url_Index[%d].s_url_index = %d", row_num, Url_Index[row_num].s_url_index);
      NSDL3_PARSING(NULL, NULL, "staticUrlTable[%d].url= %s", row_num, url);
    }
    else
    {
      NSDL3_PARSING(NULL, NULL, "URL %s found at index %d", (char *) url, row_num);
    }
    NSDL3_PARSING(NULL, NULL, "Method exiting");
}

/* To create static hash table taking input as url.txt (static urls from script)
   Also free static table after creating hash out of it */
int url_hash_create_hash_table(char *url_file)
{
  NSDL3_PARSING(NULL, NULL, "Method called");
  char path[MAX_BUF_SIZE_FOR_URL_PATH];
  Hash_code_to_str_ex url_get_key;
  u_ns_ts_t start_ts;
  double time_taken;

  NSDL3_PARSING(NULL, NULL, "Method called. global_settings->static_url_hash_mode=%d", global_settings->static_url_hash_mode);
  if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
  {
    if(url_hash_close_file()) return URL_ERROR; 
    sprintf(path, "%s", g_ns_tmpdir);
    
    printf("Creating URL hash table. It may take time ...\n");
    start_ts = ns_get_ms_stamp();
    generate_hash_table_as_index(url_file, "url_hash_func", &url_hash_func, &url_get_key, 0, path, url_hash_find_idx);
    time_taken = ((ns_get_ms_stamp() - start_ts)/1000);
    printf("Creation of URL hash table is complete. Time taken to create static url hash table = %3f seconds\n", time_taken);
  }

  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return URL_SUCCESS;
}

void free_url_hash_table()
{
  NSDL3_PARSING(NULL, NULL, "Method called. global_settings->static_url_hash_mode=%d", global_settings->static_url_hash_mode);
  if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
  {
    if(my_port_index == 255)
    {
      NSDL3_PARSING(NULL, NULL, "free static url table from parent");
      FREE_AND_MAKE_NULL_EX(staticUrlTable, (global_settings->static_url_table_size * sizeof(StaticUrlHashTableEntry)), "StaticUrlTable", -1);
    }
    if((get_max_report_level() < 2) && run_mode == NORMAL_RUN && my_port_index == 255)
    {
      NSDL3_PARSING(NULL, NULL, "Rrporting is not set so free Url_Index table from parent");
      FREE_AND_MAKE_NULL_EX(Url_Index, (max_url_entries * sizeof(UrlIndex_t)), "Url_Index", -1);
    }
    if((get_max_report_level() >= 2) && run_mode == NORMAL_RUN && my_port_index != 255)
    {
      NSDL3_PARSING(NULL, NULL, "Rrporting is set so free Url_Index table from child");
      FREE_AND_MAKE_NULL_EX(Url_Index, (max_url_entries * sizeof(UrlIndex_t)), "Url_Index", -1);
    }
  }
  NSDL3_PARSING(NULL, NULL, "Method exiting");
}
//---------------- STATIC_URL_HASH_TABLE SECTION ENDS ----------------------------------------


//---------------- DYNAMIC_URL_HASH_TABLE SECTION BEGINS ----------------------------------------

// For creating the dynamic url table if Auto fetch or auto redirect is on
//Called from child_init in case only dynamic urls are stored in dynamic url hash table
//Called from alloc_mem_for_urltable in case static urls are also to be stored in dynamic url hash table
void url_hash_dynamic_table_init()
{
  //In Bug 3204: Earlier we were creating dynamic url hash table if Auto-Fetch or Auto-Redirect is ON.
  //However when someone call API ns_set_embd_objects() from script for fetching dynamic url, 
  //then we were not able to create id for that dynamic url.
  //Hence will be creating dynamic hash url table all the time irrespective of whether Auto-Fetch & Auto-Redirect is Off. 
  /*int ret;
  ret = check_auto_fetch_enable();
  NSDL4_PARSING(NULL, NULL, "method called. global_settings->g_auto_redirect_use_parent_method = %d, auto_fetch_embedded = %d", global_settings->g_auto_redirect_use_parent_method, ret);
  if(global_settings->g_follow_redirects != 0 || ret != 0)*/
  
  //int table_size = 0;
  NSDL4_PARSING(NULL, NULL, "Method Called"); 
  
    
  
  if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_DYNAMIC_HASH)
    global_settings->url_hash.dynamic_url_table_size += global_settings->static_url_table_size;
  
  global_settings->url_hash.dynamic_url_table_size = change_to_2_power_n(global_settings->url_hash.dynamic_url_table_size);

  NSDL4_PARSING(NULL, NULL, "Dynamic Url Table - Array size = %d", global_settings->url_hash.dynamic_url_table_size); 

  if(!dynamicUrlTable)
  {
    MY_MALLOC_AND_MEMSET(dynamicUrlTable, sizeof(ns_ptr_t) * global_settings->url_hash.dynamic_url_table_size, "Dynamic URL Table Creation", -1);
  }
  else
    NSDL4_PARSING(NULL, NULL, "Dynamic url hash table already existing"); 

  NSDL4_PARSING(NULL, NULL, "Method exiting");
}

/* Check if the url exists in dynamic url table or not
   if exist the return the d_url_index
   otherwise return the index where the url is added in dynamic url table 
   Returns d_url_index if found, else return URL_ERROR  
   Return values hash & ihashValue for adding url in hash table, in case url is not found in url table*/
static int url_hash_dynamic_url_found(u_ns_char_t *url, int urlLen, int *hash, unsigned int *ihashValue, int static_url)
{
  NSDL3_PARSING(NULL, NULL, "Method called");
  int url_found = 1;
  //Pointer to url entry found in url table
  DyamicUrlHashTableEntry  *urlTable_entry = NULL;
  //Pointer to traverse the chain in case of collision
  DyamicUrlHashTableEntry *pUrlEntry;
  //Used for logging purposes to store the chain index in case of collision,
  //starting with 0 for entry which is stored directly in hashtable
  int link = 0;

  NSDL3_PARSING(NULL, NULL, "Method Called. static_url=%d, url=%s", static_url, url);

  //1. Calls the url_hash() index to get the hashindex based on url to be searched
  *hash = nslb_hash_get_hash_index(url, urlLen, &(*ihashValue), global_settings->url_hash.dynamic_url_table_size);
  NSDL3_PARSING(NULL, NULL, "hash=%d", *hash);
  if(URL_ERROR == *hash)
     return URL_ERROR;

  //2. If hashindex doesn't contain any urlTable_entry, Url not found in urlTable_entry,
  NSDL3_PARSING(NULL, NULL, "dynamicUrlTable=%p dynamicUrlTable[%d]=%p", dynamicUrlTable, *hash, dynamicUrlTable[*hash]);
  if(NULL == dynamicUrlTable[*hash])
     return URL_ERROR;   
 

  pUrlEntry = dynamicUrlTable[*hash];
                       
  //3. If hashindex contains node urlTable_entry 
  //   search the url entry in the list by matching the url to be searched
  //   with the url in the chained linked list.
  //   Traverse the linked list to reach the searched url
  do {
       //NSDL4_PARSING(NULL, NULL, "url in hashtable = [%s]", RETRIEVE_BUFFER_DATA(pUrlEntry->url));
       if(static_url)
       {
         //Search for static url in dynamic url hash table in case STATIC_URL_HASH_TABLE_OPTION - Mode2
         // nslb_hash_match_value() : this function is moved in library
         url_found = nslb_hash_match_value((u_ns_char_t *)RETRIEVE_BUFFER_DATA(pUrlEntry->url), pUrlEntry->iHashValue, url, urlLen, *ihashValue);
         NSDL3_PARSING(NULL, NULL, "Static url %s, %s in static url list(dynamic url hash table)",
         RETRIEVE_BUFFER_DATA(pUrlEntry->url), ((url_found)?"found":"not found"));
       }
       else
       {
         url_found = nslb_hash_match_value((u_ns_char_t *)(RETRIEVE_URL(pUrlEntry)), pUrlEntry->iHashValue, url, urlLen, *ihashValue);
         NSDL3_PARSING(NULL, NULL, "url %s, %s in dynamic url hash table", 
                              RETRIEVE_URL(pUrlEntry), ((url_found)?"found":"not found"));
       }

       //Loop through till url is found
       if (url_found != 0)
          break;
       else
          pUrlEntry = pUrlEntry->next;
       ++link;
    } while(NULL != pUrlEntry);

  //4. If url found in chained list, return the url index
  if(url_found)
  {
     urlTable_entry = pUrlEntry;
     NSDL3_PARSING(NULL, NULL, "Method exiting url_index=%d [0x%x]", urlTable_entry->d_url_index, urlTable_entry->d_url_index);
     return urlTable_entry->d_url_index; 
  }
  //5. If url not found in the chained list, return -1
  else
  {
     NSDL3_PARSING(NULL, NULL, "Method exiting url_index=-1");
     return URL_ERROR;
  }
}


// This function is use too create & init new node for dynamic url table
// Returns pointer to the dynamic table node created
// Output parameter: url_idx - Url index comprising for NVM ID + Running URL Id
static DyamicUrlHashTableEntry *url_hash_dynamic_table_init_entry(u_ns_char_t *url,int UrlLen, unsigned int iHashValue, int *url_idx)
{
  NSDL3_PARSING(NULL, NULL, "Method called");
  static int dynamic_url_id = 0;
  DyamicUrlHashTableEntry *tmpUrlEntry;
  MY_MALLOC_AND_MEMSET(tmpUrlEntry, sizeof(DyamicUrlHashTableEntry), "tmpUrlEntry", 1);

  tmpUrlEntry->url_len = UrlLen;
  if ((tmpUrlEntry->url = copy_into_big_buf((char *)url, UrlLen)) == -1)
  {
    NS_EXIT(-1, "Error: - Error allocating shared memory for URL, URL is %s", url);
  }

  if(my_port_index == 255)
  {
    //For Static urls in dynamic table URLS
    tmpUrlEntry->d_url_index = *url_idx;
  }
  else
  {
    //URL Index is combination of NVMID in 1st byte and dynamic url sequence in next 3 bytes to make it unique across all NVMs
    // NVM0 is clashing the d_url_index with static url index which may cause confusion
    //Hence starting the NVM from 1 instead of 0 
    //e.g 
    //if(global_settings->static_url_hash_mode != NS_URL_HASH_DISABLED)
    //For Dynamic URLS
    {
      tmpUrlEntry->d_url_index = my_port_index + 1;
      tmpUrlEntry->d_url_index <<= 24;
      *url_idx = tmpUrlEntry->d_url_index |= dynamic_url_id;
      NSDL1_PARSING(NULL, NULL, "Dynamic Hash Code = %d, my_port_index=%u, url_id=%d", tmpUrlEntry->d_url_index, my_port_index, *url_idx);
      dynamic_url_id++;
    }
  }

  tmpUrlEntry->iHashValue = iHashValue;
  tmpUrlEntry->next = NULL;
  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return tmpUrlEntry;
}


//This function will return the d_url_index of dyanmic url table
static int url_hash_add_dynamic_url(u_ns_char_t *url, int urlLen, int ihashIndex, int ihashValue, int static_url_id)
{
  NSDL3_PARSING(NULL, NULL, "Method called");

  //Pointer to newly allocated url entry
  DyamicUrlHashTableEntry *urlTable_entry;
  //Pointer to traverse the chain in case of collision
  DyamicUrlHashTableEntry *pUrlEntry;
  //Used for logging purposes to store the chain index in case of collision,
  //starting with 0 for entry which is stored directly in hashtable
  int link=0;
  int url_index = static_url_id;

  urlTable_entry = url_hash_dynamic_table_init_entry(url, urlLen, ihashValue, &url_index);  

  //checks whether the hash index returned by nslb_hash_get_hash_index() is free for use
  if(NULL == dynamicUrlTable[ihashIndex])
  {
     //If hashindex free, stores the address of url_data
     //at the hashindex in hashtable
     dynamicUrlTable[ihashIndex] = urlTable_entry;
     NSDL2_PARSING(NULL, NULL, "Stored at Hashindex=[%d] url=%s ",
                                              ihashIndex, (char *)url);
  }

  //If hashindex is not free, create chain on that index using linked list
  //and stores the url_data on that chained link.
  else
  {
     pUrlEntry = dynamicUrlTable[ihashIndex];
     nslb_url_hash_dynamic_list_add(pUrlEntry, urlTable_entry, &link);
     NSDL1_PARSING(NULL, NULL, "Stored at chained link=%d Hashindex=[%d] url=%s ",
                                              link, ihashIndex, (char *)url);
  }
 
  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return url_index;
}

// To destroy the dyanamic url table
// Called from netstorm_parent for deleting hash table from parent used for static urls
// and called from NVM_cleanup for deleting from NVMs
int dynamic_url_destroy()
{
  // this function is moved in library.
  return nslb_dynamic_url_hash_destroy(dynamicUrlTable, global_settings->url_hash.dynamic_url_table_size); 
}

// To destroy the dyanamic url table
//Called from netstorm_parent for deleting hash table from parent used for static urls
// and called from NVM_cleanup for deleting from NVMs
int dynamic_url_dump()
{ 
  // this function is moved in library.
  return nslb_dynamic_url_hash_dump(dynamicUrlTable, global_settings->url_hash.dynamic_url_table_size);
}


// Add the url in UrlHashTableEntry, return 0 on success or -1 on failure
// This will be called from parsing of script code.
//     C Type -> ns_http_script_parse.c -> set_url
//     Leagcy -> Main URL and Embedded URL (it is already in absolute url format)
// URL must be passed as in the script (absoule URL) including parameters if any e.g.
//   /macys/index.ognc?ID={CatId}

// Issue: 
// 1 ->Is URL Case sensitive? If so, how to handle it?
// 2 -> Hash code generated  using gperf does not allow comma. So if url has comma,how to handle it?

/* Function url_hash_get_url_idx_for_dynamic_urls() is called from all dynamic urls 
   Step1 : Check in static url table if exist then return the request_url_id,
   Step2 : If doesnt exist in static url then check in dynamic url table if exist then return url_index
   Step3 : If deosnt exist in dynamic url table then add and return the url_index */ 

//static_url_id is request table index of the url, in case of dynamic urls it will be 0
//static_url will be 1 in case url is a static url, 0 otherwise
int url_hash_get_url_idx_for_dynamic_urls(u_ns_char_t *url, int iUrlLen, int page_id, int static_url, int static_url_id, char *page_name)
{
  int url_index; 
//  int iUrlLen;
  unsigned int ihashValue;
  int ihashIndex;
  u_ns_char_t url_pipe_replaced[MAX_LINE_LENGTH + 1];
  u_ns_char_t url_sp_char_replaced[MAX_LINE_LENGTH + 1];
  static int dynamic_url_count = 0;

  u_ns_ts_t start_ts;
  int search_time;

  u_ns_char_t *url_for_hash = url;

  NSDL3_PARSING(NULL, NULL, "Method called URL=%s, iUrlLen=%d, page_id=%d", url, iUrlLen, page_id);
  if(NULL == url)
  {
     NSDL3_PARSING(NULL, NULL, "URL is NULL");
     return URL_ERROR;
  }

  //Dynamic URL Table creation disabled
  if(global_settings->dynamic_url_hash_mode == NS_DYNAMIC_URL_HASH_DISABLED)
  {
    NSDL3_PARSING(NULL, NULL, "Returning Dummy dynamic url index = %d", global_settings->url_hash.dummy_dynamic_url_index);
    return global_settings->url_hash.dummy_dynamic_url_index;
  }

  //Using dynamic urls only - without using query parameter for the purpose of hashing 
  if ((global_settings->dynamic_url_hash_mode == NS_DYNAMIC_URL_HASH_WITHOUT_QUERY_PARAM) && !static_url)
  {
    iUrlLen = handle_query_param((char *)url, iUrlLen);
    NSDL3_PARSING(NULL, NULL, "Dynamic url without query parameter iUrlLen=%d, url=%s", iUrlLen, url); 
  }

  //static URL Table creation is using GPERF
  if (global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
  {
    //Replace pipe with _ to search in static url table
    //Use original dynamic url table for entrying in dynamic url table
    // Here we cannot use url as output as url should not be changed
    check_and_replace_pipe(url, url_pipe_replaced, iUrlLen);

    iUrlLen = handle_special_char(url_pipe_replaced, url_sp_char_replaced, iUrlLen);
    NSDL4_PARSING(NULL, NULL, "Url after handling special characters=%s", url_sp_char_replaced);

    // Step1 - Search in static url hash table. If found, return the hash code which is same as index
    start_ts = ns_get_ms_stamp();

    //Assumption: gperf will match only 255 characters of a string, and will ignore the rest of the string
    //int iUrlLenForStaticHash = ((iUrlLen < 255)? iUrlLen: 255);

    url_for_hash = url_sp_char_replaced;
    //url_index will be index from static hash table
    if ((url_index = url_hash_func((char *)url_for_hash, iUrlLen)) == -1)
    {
      search_time = (ns_get_ms_stamp() - start_ts);
      if(search_time > global_settings->static_url_table_search_time_threshold)
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         __FILE__, (char*)__FUNCTION__,
                         "Time taken %d ms for searching url but not found in Static Url Hash Table has exceeded the threshold %d ms for url=%s", 
                         search_time, global_settings->static_url_table_search_time_threshold, (char *)url_for_hash);

      NSDL1_PARSING(NULL, NULL, "url %s does not exist in static url hash table \n", (char *)url_for_hash);
    }
    else
    {
      /* If url is found in the static url table then we return the index of request table, 
         which is stored in the index url table */
      NSDL1_PARSING(NULL, NULL, "url %s exists in static url hash table. s_url_index = %d, Request Table Index = %d", 
                           (char *)url_for_hash, url_index, Url_Index[url_index].s_url_index);
      search_time = (ns_get_ms_stamp() - start_ts);
      if(search_time > global_settings->static_url_table_search_time_threshold)
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         __FILE__, (char*)__FUNCTION__,
                         "Time taken %d ms for searching url found in Static Url Hash Table has exceeded the threshold %d ms for url=%s", 
                         search_time, global_settings->static_url_table_search_time_threshold, (char *)url_for_hash);

      return (Url_Index[url_index].s_url_index);
    }
  }

  // Step2 - Search in dynamic hash table. If found, return the index stored
  if(dynamicUrlTable != NULL)
  {
    start_ts = ns_get_ms_stamp();

    NSDL3_PARSING(NULL, NULL, "Checking for existing entry for Url [%s] in dynamic hash table", url);

    //In case dynamic url not found in dynamic url hash table
    if((url_index = url_hash_dynamic_url_found(url_for_hash, iUrlLen, &ihashIndex, &ihashValue, static_url)) == URL_ERROR)
    {
      NSDL1_PARSING(NULL, NULL, "URL %s not found in dynamic url hash table, so add at hashindex = %d", (char *)url, ihashIndex);
    
      // Step3 - If not found in dynamic table, add in dynamic table, assign index and log in url table
      url_index = url_hash_add_dynamic_url(url_for_hash, iUrlLen, ihashIndex, ihashValue, static_url_id);
      search_time = (ns_get_ms_stamp() - start_ts);
      if(search_time > global_settings->static_url_table_search_time_threshold)
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         __FILE__, (char*)__FUNCTION__,
                         "Time taken %d ms for searching url but not found in Dynamic Url Hash Table has exceeded the threshold %d ms for url=%s", 
                         search_time, global_settings->dynamic_url_table_search_time_threshold, url_for_hash);

      NSDL1_PARSING(NULL, NULL, "URL added in dynamic url hash table at the hashindex = %d, dynamic URL index = %d", ihashIndex, url_index);

      if(!static_url)
      {
        if(log_url((char*)url_for_hash, iUrlLen, url_index, page_id, ihashIndex, ihashValue, page_name) == -1)
          NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                       __FILE__, (char *)__FUNCTION__, "Error in logging the URL %s  in slog, page_id = %d",(char*)url_for_hash, page_id);
        dynamic_url_count++;

        // Log event every threshhold
        // If threshold is 5000, then this event will be logged at 5000, 10000, 15000, ...
        if((dynamic_url_count%global_settings->dynamic_table_threshold) == 0)
          NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                           __FILE__, (char*)__FUNCTION__,
          "Dynamic url count [%d] has reached/exceeded the threshold [%d]",
              dynamic_url_count, global_settings->dynamic_table_threshold);
      }
    }
    //In case dynamic url found in dynamic url hash table
    else
    {
      NSDL1_PARSING(NULL, NULL, "URL %s found in dynamic url hash table at hashindex=%d, url_index=%d", (char *)url, ihashIndex, url_index);
      search_time = (ns_get_ms_stamp() - start_ts);
      if(search_time > global_settings->dynamic_url_table_search_time_threshold)
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         __FILE__, (char*)__FUNCTION__,
            "Time taken %d ms for searching url found in Dynamic Url Hash Table has exceeded the threshold %d ms for url=%s", 
                         search_time, global_settings->dynamic_url_table_search_time_threshold, url_for_hash);
    }
  }
  else
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                       __FILE__, (char *)__FUNCTION__, "dynamicUrlTable table size is zero but hitting the Dynamic URLs %s, for the page id %d",(char*)url, page_id);
    fprintf(stderr, "Warring : dynamicUrlTable table size is zero, but hitting the Dynamic URLs\n");
    return URL_ERROR;
  }
  
  NSDL3_PARSING(NULL, NULL, "Method exiting");
  return url_index;
}
//---------------- DYNAMIC_URL_HASH_TABLE SECTION ENDS ----------------------------------------


//Used for adding static url in either static/dynamic url hash table depending on the mode
//Called from url.c & ns_http_script_parse.c
void url_hash_add_url(u_ns_char_t *url, int url_id, int page_id, char *page_name)
{

  NSDL3_PARSING(NULL, NULL, "Method Called");

  if (global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_GPERF)
    url_hash_add_in_static_hash_table(url, url_id);
  else if(global_settings->static_url_hash_mode == NS_STATIC_URL_HASH_USING_DYNAMIC_HASH)
    url_hash_get_url_idx_for_dynamic_urls(url, strlen((char *)url), page_id, 1, url_id, page_name);

  NSDL3_PARSING(NULL, NULL, "Exitting Method ");
}



//---------------- KEYWORD_PARSING SECTION BEGINS ------------------------------------

//For print the use of STATIC_URL_HASH_TABLE_OPTION keyword
static void static_url_hash_table_usage(char *err)
{
  NSTL1_OUT(NULL,NULL,"This keyword is used to create static URL hash table.\n");
  NSTL1_OUT(NULL,NULL, "  Error: Invalid value of URL_STATIC_HASH_TABLE_OPTION keyword: %s\n", err);
  NSTL1_OUT(NULL,NULL, " Mode:\n");
  NSTL1_OUT(NULL,NULL, " 0: Disable. Static url hash table will not be created \n");
  NSTL1_OUT(NULL,NULL, " 1: Enable. Create static url hash table using gperf \n");
  NSTL1_OUT(NULL,NULL, "  	   (Should be used in case of scripts contains less number of urls) \n");
  NSTL1_OUT(NULL,NULL, "  	2: Enable(default). Create static url hash table using static url hash table");
  NSTL1_OUT(NULL,NULL, "  	   (Should be used in case of scripts contains quiet large number of urls) \n");
  NSTL1_OUT(NULL,NULL, "  static_url_table_size \n");
  NSTL1_OUT(NULL,NULL, "     Size of static url table used for storing static urls from auto-fetch & auto-redirect.\n");
  NSTL1_OUT(NULL,NULL, "     Default is 256\n");
  NSTL1_OUT(NULL,NULL, "  static_table_search_time_threshold\n");
  NSTL1_OUT(NULL,NULL, "     Threshold to log a warning event when time taken (in ms) to search for a static url ");
  NSTL1_OUT(NULL,NULL, "     table exceeds the value specified.\n");
  NSTL1_OUT(NULL,NULL, "  Mode for parametrized URL:\n");  
  NSTL1_OUT(NULL,NULL, "  	0: Disable(default). Do not treat parametrized URL as dyn URL\n");
  NSTL1_OUT(NULL,NULL, "  	1: Treat parametrized URL as dyn URL and can be Used ONLY if dynamic URL is not disabled\n");
  NS_EXIT(-1,"%s\nUsage: URL_STATIC_HASH_TABLE_OPTION <mode> [static url table size] [static table search time threshold] [mode for parametrized URL]");
}

//For print the use of URL_HASH_TABLE_OPTION keyword
static void dynamic_url_hash_table_usage(char *err)
{
  NSTL1_OUT(NULL,NULL, "This keyword is used to create dynamic URL hash table.\n");
  NSTL1_OUT(NULL,NULL, "  Error: Invalid value of URL_DYNAMIC_HASH_TABLE_OPTION keyword: %s\n", err);
  NSTL1_OUT(NULL,NULL, "   [dynamic table threshold] [dynamic table search time threshold]\n");
  NSTL1_OUT(NULL,NULL, "  Mode:\n");
  NSTL1_OUT(NULL,NULL, "  	0: Disable. Dynamic url hash table will not be created \n");
  NSTL1_OUT(NULL,NULL, "  	1: Enable (Default). Dynamic url hash table will be created without using query-parameter \n");
  NSTL1_OUT(NULL,NULL, "  	2: Enable. Create dynamic url hash table using complete url\n");
  NSTL1_OUT(NULL,NULL, "  dynamic_url_table_size \n");
  NSTL1_OUT(NULL,NULL, "     Size of dynamic url table used for storing dynamic urls from auto-fetch & auto-redirect.\n");
  NSTL1_OUT(NULL,NULL, "     Default is 256\n");
  NSTL1_OUT(NULL,NULL, "  dynamic_table_search_time_threshold\n");
  NSTL1_OUT(NULL,NULL, "     Threshold to log a warning event when time taken (in ms) to search for a dynamic url ");
  NSTL1_OUT(NULL,NULL, "     table exceeds the value specified.\n");
  NSTL1_OUT(NULL,NULL, "  dynamic_url_threshold\n");
  NSTL1_OUT(NULL,NULL, "     Threshold to log a warning event when total number of URL in the dynamic hash table cross ");
  NSTL1_OUT(NULL,NULL, "     some threshold or is multiple of that threshold \n");
  NSTL1_OUT(NULL,NULL, "     at 1000 dynamic urls then on 2000, 3000, 4000 etc.\n");
  NSTL1_OUT(NULL,NULL, "     Default is 5000\n");
  NS_EXIT(-1, "%s\nUsage: URL_DYNAMIC_HASH_TABLE_OPTION <mode> [dynamic url table size] ",err);
}

//DYNAMIC_URL_HASH_TABLE_OPTION <enable_dynamic_url_table>[dynamic_url_table_size] [dynamic_url_threshold] [dynamic table search.time threshold]
int kw_set_dynamic_url_hash_table_option(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int  dynamic_url_hash_mode = 0;
  char dynamic_table_size[MAX_DATA_LINE_LENGTH] = "\0";
  char dynamic_table_threshold[MAX_DATA_LINE_LENGTH] = "\0";
  char dynamic_time_threshold[MAX_DATA_LINE_LENGTH] = "\0";

  int num;
  NSDL4_PARSING(NULL, NULL, "Method called");

  num = sscanf(buf, "%s %d %s %s %s", keyword, &dynamic_url_hash_mode, dynamic_table_size, dynamic_time_threshold, dynamic_table_threshold);

  if(num < 2)
    dynamic_url_hash_table_usage("Insufficient Parameters");

  if((dynamic_url_hash_mode != NS_STATIC_URL_HASH_DISABLED) && (dynamic_url_hash_mode != NS_STATIC_URL_HASH_USING_GPERF) && (dynamic_url_hash_mode != NS_STATIC_URL_HASH_USING_DYNAMIC_HASH))
    dynamic_url_hash_table_usage("Invalid URL_HASH_TABLE_OPTION mode");

  if(dynamic_table_size != NULL)
    if(ns_is_numeric(dynamic_table_size) == 0)
      dynamic_url_hash_table_usage("dynamic url table size can be numeric only");

  if(dynamic_table_threshold != NULL)
    if(ns_is_numeric(dynamic_table_threshold) == 0)
      dynamic_url_hash_table_usage("dynamic table threshold can be numeric only");

  if(ns_is_numeric(dynamic_time_threshold) == 0)
    dynamic_url_hash_table_usage("dynamic table search time threashold can be numeric only");
 
  NSDL3_PARSING(NULL, NULL, "URL_DYNAMIC_HASH_TABLE_OPTION = %d %s %s %s", dynamic_url_hash_mode, dynamic_table_size, dynamic_table_threshold, dynamic_time_threshold);

  global_settings->dynamic_url_hash_mode = dynamic_url_hash_mode;
  global_settings->url_hash.dynamic_url_table_size = atoi(dynamic_table_size);
  global_settings->dynamic_table_threshold = atoi(dynamic_table_threshold);
  global_settings->dynamic_url_table_search_time_threshold = atoi(dynamic_time_threshold);
  
  if(global_settings->url_hash.dynamic_url_table_size == 0)  global_settings->url_hash.dynamic_url_table_size = DEFAULT_URL_HASH_SIZE;
  if(global_settings->dynamic_table_threshold == 0)  global_settings->dynamic_table_threshold = DEFAULT_DYNAMIC_HASH_THRESHOLD;
  if(global_settings->dynamic_url_table_search_time_threshold == 0)  global_settings->dynamic_url_table_search_time_threshold = DEFAULT_SEARCH_TIME_THRESHOLD;

  NSDL4_PARSING(NULL, NULL, "Exitting method. dynamic_url_hash_mode = %hd, dynamic_url_table_size = %d, dynamic_table_threshold = %d, dynamic_url_table_search_time_threshold=%d", 
                              global_settings->dynamic_url_hash_mode, 
                              global_settings->url_hash.dynamic_url_table_size, global_settings->dynamic_table_threshold,
                              global_settings->static_url_table_search_time_threshold, 
                              global_settings->dynamic_url_table_search_time_threshold);
  return 0;
}

//STATIC_URL_HASH_TABLE_OPTION < static_url_table_mode> [static_url_table_size] [static table search.time threshold] [parm_url_as_dyn_url_mode]
int kw_set_static_url_hash_table_option(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int static_url_hash_mode = 0;
  int parm_url_as_dyn_url_mode = 0;  //mode is to treat paramterized URL as dyn URL
  char static_table_size[MAX_DATA_LINE_LENGTH] = "\0";
  char static_time_threshold[MAX_DATA_LINE_LENGTH] = "\0";

  int num;
  NSDL4_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %d %s %s %d", keyword, &static_url_hash_mode, static_table_size, static_time_threshold, &parm_url_as_dyn_url_mode);

  if(num < 2)
    static_url_hash_table_usage("Insufficient Parameters");

  if(global_settings->dynamic_url_hash_mode == NS_DYNAMIC_URL_HASH_DISABLED && static_url_hash_mode != NS_STATIC_URL_HASH_DISABLED)
  {
    global_settings->static_url_hash_mode = NS_STATIC_URL_HASH_DISABLED;
    NSDL4_PARSING(NULL, NULL, "static_url_hash_mode = %d", static_url_hash_mode);

    /* Replacing print_core_events() with fprintf because at this point event log keywords are not parsed thats why on calling 
     * print_core_events() will get this warning message 2 times on screen: (1) from print_core_events() (2) from ns_el because 
     * event log keywords are still not parsed hence this funct will print message on screen only.*/
    NS_DUMP_WARNING("Disabling Static Url Hash Table as Dynamic Url Hash Table is Disabled");
    return 0;
  }
    
  if((static_url_hash_mode != NS_STATIC_URL_HASH_DISABLED) && (static_url_hash_mode != NS_STATIC_URL_HASH_USING_GPERF) && (static_url_hash_mode != NS_STATIC_URL_HASH_USING_DYNAMIC_HASH))
    static_url_hash_table_usage("Invalid STATIC_URL_HASH_TABLE_OPTION mode");

  if(static_table_size[0] != '\0')
    if(ns_is_numeric(static_table_size) == 0)
      static_url_hash_table_usage("static url table size can be numeric only");

  if(static_table_size != NULL)
    if(ns_is_numeric(static_table_size) == 0)
      static_url_hash_table_usage("static url table size can be numeric only");

  if(ns_is_numeric(static_time_threshold) == 0)
    static_url_hash_table_usage("static table search time threashold can be numeric only");
 
  if(ns_is_numeric(static_time_threshold) == 0)
    static_url_hash_table_usage("static table search time threashold can be numeric only");

  if((parm_url_as_dyn_url_mode != NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_DISABLED) && (parm_url_as_dyn_url_mode != NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED)) 
    static_url_hash_table_usage("Invalid STATIC_URL_HASH_TABLE_OPTION mode for parametrized URL as dynamic URL.");

  if((parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED) && (global_settings->dynamic_url_hash_mode == NS_DYNAMIC_URL_HASH_DISABLED))
    static_url_hash_table_usage("This MODE can be Used ONLY if dynamic URL is not disabled.");

  NSDL3_PARSING(NULL, NULL, "STATIC_URL_HASH_TABLE_OPTION = %d %s %s", static_url_hash_mode, static_table_size, static_time_threshold);

  global_settings->static_url_hash_mode = static_url_hash_mode;
  global_settings->static_parm_url_as_dyn_url_mode = parm_url_as_dyn_url_mode;
  global_settings->static_url_table_size = atoi(static_table_size);
  global_settings->static_url_table_search_time_threshold = atoi(static_time_threshold);
  
  if(global_settings->static_url_table_size == 0)  global_settings->static_url_table_size = DEFAULT_URL_HASH_SIZE;
  if(global_settings->static_url_table_search_time_threshold == 0)  global_settings->static_url_table_search_time_threshold = DEFAULT_SEARCH_TIME_THRESHOLD;

  NSDL4_PARSING(NULL, NULL, "Exitting method. static_url_hash_mode = %hd, static_url_table_size = %d,"
                             "static_url_table_search_time_threshold=%d, static_parm_url_as_dyn_url_mode=%d", 
                              global_settings->static_url_hash_mode, global_settings->static_url_table_size, 
                              global_settings->static_url_table_search_time_threshold, global_settings->static_parm_url_as_dyn_url_mode);
  return 0;
}

//----------------  KEYWORD_PARSING SECTION ENDS ------------------------------------

