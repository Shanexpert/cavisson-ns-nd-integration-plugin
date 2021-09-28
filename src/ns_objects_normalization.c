/********************************************************************************
 * File Name            : ns_objects_normalization.c
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
#include "ns_objects_normalization.h"
#include "nslb_util.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_multi_thread_trace_log.h"

MTTraceLogKey *lr_norm_trace_log_key;
long long partition_idx = -1;

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

/* ------------------------------  URL NORMALIZATION SECTION BEGINS ---------------------------------------- */

#define MAX_NVM_NUM 255
#define MAX_GEN_NUM 255

#define INIT_NORM_URL_PER_NVM_TABLE_SIZE 1000
#define DELTA_NORM_URL_PER_NVM_TABLE_SIZE 1000 

#define IS_URL_STATIC(url_id)  (!(url_id & 0xFF000000))

#define GET_URL_PART_NVM_ID(url_index)  (url_index >> (8*3) & 0xff)
#define GET_URL_PART_URL_ID(url_index)  (url_index >> (8*0) & 0x00ffffff)

#define NVM_TABLE_SIZE \
  int size; \
  if (total_gen_entries == 0) \
    size = MAX_NVM_NUM + 1;\
  else\
    size = MAX_GEN_NUM * (MAX_NVM_NUM + 1);

#define GET_NVM_IDX(gen_id, nvm_id, index) \
    index = ((MAX_NVM_NUM + 1) * gen_id) + nvm_id; \
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "gen_id = %d, nvm_id = %d, tot_nvms = %d, index = %d", gen_id, nvm_id, tot_nvms, index);

int tot_nvms = 0, total_gen_entries = 0;

//Total no. of entries per NVM table 
static int *total_nvm_table_entries; 

// Normalization Table
static NormObjKey normUrlTable;

// NVM table for nvm url id to norm id mapping
static normIdType **g_url_norm_index_table;  // Array of pointer

static int NormalizedTableSize = 0; // Use for all tables
static int NormalizedTxTableSize = 0; // Use for Tx tables

//This method is used to return the next 2^n of the number passed to it. if the no is already a power of to then it will 
//the same no other wise it will return the next power of two.
static int change_to_2_power_n(int size_value)
{
int i;
int count = 0;
int pos = 0;

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. size_value = %d", size_value);

  if(size_value >= 65536) //TODO: Took limit as MAX_CACHE_TABLE_SIZE need to check later
  {
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "limit exceded. size_value = %d", 65536); 
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
    NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO,"Method called. after changing to power of 2 size_value = %d", size_value);
  }
  return size_value;
}

static void url_norm_init_nvm_table() {
  //int nvm_id;

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, total number of nvms = %d, total number generators = %d", tot_nvms, total_gen_entries);
  NVM_TABLE_SIZE

  NSLB_MALLOC_AND_MEMSET(g_url_norm_index_table, (sizeof(normIdType *) * size), "NVM table per generator", -1, log_fp);
  NSLB_MALLOC_AND_MEMSET(total_nvm_table_entries, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);
  //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(total_nvm_table_entries, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);

#if 0    
  for (nvm_id = 0; nvm_id < size; nvm_id++)
  {
    //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_url_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_URL_PER_NVM_TABLE_SIZE), 
               //  "NVM table creation", -1,log_fp);
    NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_url_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_URL_PER_NVM_TABLE_SIZE), 
                 "NVM table creation", -1,log_fp);
      
    total_nvm_table_entries[nvm_id] = INIT_NORM_URL_PER_NVM_TABLE_SIZE;
  }
#endif
}

/* Init called  from logging_reader.c */
//This function is called from two places 
//1. from logging reader.In this case max_static_url_ids will 0
//2. from url_id_norm for NetCloud, In NC we know the static ids so just set it
void url_norm_init(int total_nvm, int total_generators) {
  tot_nvms = total_nvm;
  total_gen_entries = total_generators;
  url_norm_init_nvm_table();
  nslb_init_norm_id_table(&normUrlTable, NormalizedTableSize);
}


// Index NVM table using NVM URl Index, if found will return the normalized id.
// If 0 is returned, then we assumed that norm id is not available as dyn url norm id > 0 
//used by for netcloud
unsigned int get_norm_id_from_nvm_table(unsigned int nvm_url_index, unsigned int nvm_index, int gen_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. nvm_url_index = %u, nvm_index = %u", nvm_url_index, nvm_index);
  int index;
  GET_NVM_IDX(gen_id, nvm_index, index);

  if (nvm_url_index >= total_nvm_table_entries[index])
  {
    //fprintf(stderr, "Error: Url index %u is greater than total entries %d in nvm table.\n", nvm_url_index, total_nvm_table_entries[index]);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, " Url index %u is greater than total entries %d in nvm table.", nvm_url_index, total_nvm_table_entries[index]);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, " get_norm_id_from_nvm_table(): nvm_url_index = %u, nvm_index = %u, total_nvm_table_entries[index] = %u \n" , nvm_url_index, index, total_nvm_table_entries[index]);

  normIdType *nvmIndexPtr = g_url_norm_index_table[index];

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, " nvmIndexPtr = %d, g_url_norm_index_table[index] = %p\n", *nvmIndexPtr, g_url_norm_index_table[index]);

  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method Ends");
  return *(nvmIndexPtr + nvm_url_index);
}

// Called from logging_reader.c , while filling URL_RECORD.
unsigned int get_url_norm_id(unsigned int id, int gen_id) {

  unsigned int nvm_url_id;
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %u (0x%X)", id, id);

  unsigned int nvm_id = GET_URL_PART_NVM_ID(id);  // NVM ID

  if(nvm_id == 0) // static
    nvm_url_id = id;
  else
    nvm_url_id = GET_URL_PART_URL_ID(id); // first three bytes are URL Index

  unsigned int ret_val_for_urc = get_norm_id_from_nvm_table(nvm_url_id, nvm_id, gen_id); // finding normalized url index.

  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_urc = %u", ret_val_for_urc);
  return ret_val_for_urc;
}

/*******************************************************************************************
 * Description  : Called from logging_reader.c
 *                Based on fourth byte of url id, further calculation will be done.
 *                
 *                Byte 4 is NVM Id and Bytes 1-3 are Url Index
 ******************************************************************************************/

static int get_url_norm_id_for_url(int nvm_id, int nvm_url_index, char *url_name, int url_len, int *is_new_url, int gen_id)
{
   int index;
   GET_NVM_IDX(gen_id, nvm_id, index);
   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "nvm_url_index = %d, total_nvm_table_entries[%d] = %d, gen_id = %d", nvm_url_index, index, total_nvm_table_entries[index], gen_id);
  //If Index is more than current size of nvm table, then ralloc table
  if (nvm_url_index >= total_nvm_table_entries[index])
  {
    /* Suppose nvm_url_index is much greater than total_url_entries then it should
       malloc range of nvm_url_index + delta entries to occupy nvm_url_index in the slot
       for e.g current size = 2000, nvm_url_index = 4150,  malloc size = 4150/1000 * 1000 + 1000 = 5000 */
    int total_url_entries = (total_nvm_table_entries[index]==-1?0:total_nvm_table_entries[index]);
    int max_url_entries = ((nvm_url_index/DELTA_NORM_URL_PER_NVM_TABLE_SIZE) * DELTA_NORM_URL_PER_NVM_TABLE_SIZE) + DELTA_NORM_URL_PER_NVM_TABLE_SIZE;
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, 
                   "Realloc of g_url_norm_index_table, total_url_entries = %d, max_url_entries = %d, nvm_url_index = %u",
                   total_url_entries, max_url_entries, nvm_url_index);
    NSLB_REALLOC(g_url_norm_index_table[index], (max_url_entries * sizeof(normIdType)), "realloc nvm table", 1, log_fp);

    memset(g_url_norm_index_table[index] + total_url_entries, -1, (sizeof(normIdType) * (max_url_entries - total_url_entries)));
    total_nvm_table_entries[index] = max_url_entries;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_url_id = get_norm_id_from_nvm_table(nvm_url_index, nvm_id, gen_id);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "After get_norm_id_from_nvm_table :norm_url_id = %u", norm_url_id);
  if (norm_url_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_url_id = %u", norm_url_id);
    *is_new_url = 0;
    return norm_url_id; 
  }

  // Generation of Normalized Index 
  int is_new_url_flag = 0;
  normIdType *nvmIndexPtr = g_url_norm_index_table[index];
  unsigned  int nor_idx =  nslb_get_or_gen_norm_id(&normUrlTable, url_name, url_len, &is_new_url_flag);
  *is_new_url = is_new_url_flag;
  
 *(nvmIndexPtr + nvm_url_index) = nor_idx;
  

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Normalized ID is: (%u)",g_url_norm_index_table[index][nvm_url_index]);

  return nor_idx;   // Return normalized ID
}

unsigned int gen_url_norm_id(unsigned int url_id, char *url_name, int url_len, int *is_new_url, int gen_id)  {

  unsigned int nvm_id;
  unsigned int nvm_url_index;

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. is_new_url = %p, gen_id = %d", is_new_url, gen_id);

  if(IS_URL_STATIC(url_id))
  {
    /*if(not continous mode) //normal mode .. issue in skipping normalization of static urls.. 
    {
      NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Returning utptr->url_id = %u", utptr->url_id);

      set_max_static_url_index(utptr->url_id);
      *is_new_url = 1; // Static are always treated like new URL as that this record gets added in URL Table
      return(utptr->url_id);
    }*/
    //else //continous monitoring mode
    //{
      nvm_id = 0;
      nvm_url_index = url_id;
    //}
  }
  else
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Returning url_id = %u", url_id);
  
    nvm_id = GET_URL_PART_NVM_ID(url_id); // NVM ID
    nvm_url_index = GET_URL_PART_URL_ID(url_id);   // first three bytes are URL Index
  }

  return(get_url_norm_id_for_url(nvm_id, nvm_url_index, url_name, url_len, is_new_url, gen_id));
}

unsigned int get_url_norm_id_for_generator(char *gen_name, int gen_len, int gen_id, int gen_url_index, int *is_new_url)  
{
  // Treating all URLs from generators as dynamic URL to make code easy
  return(get_url_norm_id_for_url(gen_id, gen_url_index, gen_name, gen_len, is_new_url, gen_id));
}

// To destroy the dyanamic url table
/*int dynamic_norm_url_destroy() {
  // Free all NVM tables
  // Free URL normalization table 
  return nslb_dynamic_url_hash_destroy(normUrlTable, NormalizedTableSize);
}*/
 
/* ------------------------------  URL NORMALIZATION SECTION ENDS ---------------------------------------- */


/* ------------------------------ SESSION NORMALIZATION SECTION STARTS ---------------------------------------- */

#define INIT_NORM_SESSION_TABLE_SIZE 100  
#define DELTA_NORM_SESSION_TABLE_SIZE 100

//Normalized table
static NormObjKey normSessionTable;

//Local mapping table
static normIdType *g_session_norm_index_table;

//total entries in local mapping table 
static int total_session_table_entries;

static void session_norm_init() {

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. SESSION Table SIze = %d", NormalizedTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_session_norm_index_table, (sizeof(normIdType) * INIT_NORM_SESSION_TABLE_SIZE), "SESSION table creation", -1, log_fp);
  total_session_table_entries = INIT_NORM_SESSION_TABLE_SIZE;
  
   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "total_session_table_enteries = %d", total_session_table_entries);

  //Right now using 'DynamicTableSizeiUrl' for all 'URL,SESSION,TX,PAGE'
  //init normalized table
  nslb_init_norm_id_table(&normSessionTable, NormalizedTableSize);
}

unsigned int get_session_norm_id(unsigned int sess_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. sess_id = %u", sess_id);

  if(sess_id >= total_session_table_entries)
  {
    //fprintf(stderr, "Error: Session index %u is greater than or equal to total entries %d in session mapping table.\n", sess_id, total_session_table_entries);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Session index %u is greater than or equal to total entries %d in session mapping table.", sess_id, total_session_table_entries);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_src = %u", g_session_norm_index_table[sess_id]);
  return(g_session_norm_index_table[sess_id]);
}

unsigned int gen_session_norm_id(int session_index, char *session_name, int session_len, int *is_new_session)
{
   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. session_index = %d, session_name = %s, session_len = %d, *is_new_session = %d", session_index, session_name, session_len, *is_new_session);

   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_session_table_enteries = %d",total_session_table_entries);
  //If Index is more than current size of table, then ralloc table
  if (session_index >= total_session_table_entries)
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_session_norm_index_table, ((DELTA_NORM_SESSION_TABLE_SIZE + total_session_table_entries)* sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_session_norm_index_table + total_session_table_entries, -1, (sizeof(normIdType) * DELTA_NORM_SESSION_TABLE_SIZE));
    total_session_table_entries += DELTA_NORM_SESSION_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_session_id = get_session_norm_id(session_index);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_session_id = %u", norm_session_id);
  if (norm_session_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_session_id = %u", norm_session_id);
    *is_new_session = 0;
    return norm_session_id; 
  }

  // Generation of Normalized Index 
  int is_new_session_flag = 0;
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normSessionTable, session_name, session_len, &is_new_session_flag);
  *is_new_session = is_new_session_flag;
 
  g_session_norm_index_table[session_index] = nor_idx; 
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Session Normalized ID is: (%u)", g_session_norm_index_table[session_index]);

  return nor_idx;   // Return normalized ID
}

/* ------------------------------ SESSION NORMALIZATION SECTION ENDS ---------------------------------------- */

/* ------------------------------ Generator NORMALIZATION SECTION STARTS ------------------------------------ */

#define DELTA_NORM_GENERATOR_TABLE_SIZE 10
#define INIT_NORM_GENERATOR_TABLE_SIZE 10

//total entries in local mapping table
static int total_generator_table_entries;

//Local mapping table
static normIdType *g_generator_norm_index_table;

//Normalized table
static NormObjKey normGeneratorTable;

static void generator_norm_init() {

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. GENERATOR Table SIze = %d", NormalizedTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_generator_norm_index_table, (sizeof(normIdType) * INIT_NORM_GENERATOR_TABLE_SIZE), "GENERATOR table creation", -1, log_fp);
  total_generator_table_entries = INIT_NORM_GENERATOR_TABLE_SIZE;

  //Right now using 'DynamicTableSizeiUrl' for all 'URL,SESSION,TX,PAGE'
  //init normalized table
  nslb_init_norm_id_table(&normGeneratorTable, NormalizedTableSize);
}

unsigned int get_generator_norm_id(unsigned int gen_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. gen_id = %u", gen_id);

  if(gen_id >= total_generator_table_entries)
  {
    //fprintf(stderr, "Error: Generator id %u is greater than or equal to total entries %d in generator mapping table.\n", gen_id, total_generator_table_entries);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Generator id %u is greater than or equal to total entries %d in generator mapping table.", gen_id, total_generator_table_entries);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_src = %u", g_generator_norm_index_table[gen_id]);
  return(g_generator_norm_index_table[gen_id]);
}

unsigned int gen_generator_norm_id(char *generator_name, int generator_id, int generator_len, int *is_new_generator)
{
   NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called.generator_name = %s, generator_id = %d,generator_len = %d, *is_new_generator = %d", generator_name, generator_id, generator_len, *is_new_generator);

  //If Id is more than current size of table, then ralloc table
  if (generator_id >= total_generator_table_entries)
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_generator_norm_index_table, ((DELTA_NORM_GENERATOR_TABLE_SIZE + total_generator_table_entries)* sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_generator_norm_index_table + total_generator_table_entries, -1, (sizeof(normIdType) * DELTA_NORM_GENERATOR_TABLE_SIZE));
    total_generator_table_entries += DELTA_NORM_GENERATOR_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized id 
  unsigned int norm_generator_id = get_generator_norm_id(generator_id);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_generator_id = %u", norm_generator_id);
  if (norm_generator_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_generator_id = %u", norm_generator_id);
    *is_new_generator = 0;
    return norm_generator_id; 
  }

  // Generation of Normalized Index 
  int is_new_generator_flag = 0;
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normGeneratorTable, generator_name, generator_len, &is_new_generator_flag);
  *is_new_generator = is_new_generator_flag;
 
  g_generator_norm_index_table[generator_id] = nor_idx; 
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Session Normalized ID is: (%u)", g_generator_norm_index_table[generator_id]);

  return nor_idx;   // Return normalized ID
}

/* ------------------------------ GENERATOR NORMALIZATION SECTION ENDS --------------------------------------- */


/* ------------------------------ GROUP NORMALIZATION SECTION STARTS ---------------------------------------- */

#define DELTA_NORM_GROUP_TABLE_SIZE 100
#define INIT_NORM_GROUP_TABLE_SIZE 100

//total entries in local mapping table
static int total_group_table_entries;

//Local mapping table
static normIdType *g_group_norm_index_table;

//Normalized table
static NormObjKey normGroupTable;

static void group_norm_init() {

  NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. GROUP Table SIze = %d", NormalizedTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_group_norm_index_table, (sizeof(normIdType) * INIT_NORM_GROUP_TABLE_SIZE), "GROUP table creation", -1, log_fp);
  total_group_table_entries = INIT_NORM_GROUP_TABLE_SIZE;

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "total_group_table_enteries = %d", total_group_table_entries);
  //Right now using 'DynamicTableSizeiUrl' for all 'URL,SESSION,TX,PAGE'
  //init normalized table
  nslb_init_norm_id_table(&normGroupTable, NormalizedTableSize);
}

unsigned int get_group_norm_id(unsigned int grp_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. grp_id = %u", grp_id);

  if(grp_id >= total_group_table_entries)
  {
    //fprintf(stderr, "Error: Group id %u is greater than or equal to total entries %d in group mapping table.\n", grp_id, total_group_table_entries);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Group id %u is greater than or equal to total entries %d in group mapping table.", grp_id, total_group_table_entries);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_src = %u", g_group_norm_index_table[grp_id]);
  return(g_group_norm_index_table[grp_id]);
}

unsigned int gen_group_norm_id(int group_num, char *group_name, int userprof_id, int sess_prof, int pct, int *is_new_group, int group_len)
{
   NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called.group_num = %d, group_name = %s, userprof_id = %d, sess_prof = %d, pct = %d, is_new_group = %d, total_group_table_enteries = %d", group_num, group_name, userprof_id, sess_prof, pct, *is_new_group, total_group_table_entries);

  //If Id is more than current size of table, then ralloc table
  if (group_num >= total_group_table_entries)
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_group_norm_index_table, ((DELTA_NORM_GROUP_TABLE_SIZE + total_group_table_entries)* sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_group_norm_index_table + total_group_table_entries, -1, (sizeof(normIdType) * DELTA_NORM_GROUP_TABLE_SIZE));
    total_group_table_entries += DELTA_NORM_GROUP_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized id 
  unsigned int norm_group_id = get_group_norm_id(group_num);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_group_id = %u", norm_group_id);
  if (norm_group_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_group_id = %u", norm_group_id);
    *is_new_group = 0;
    return norm_group_id; 
  }

  // Generation of Normalized Index 
  int is_new_group_flag = 0;
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normGroupTable, group_name, group_len, &is_new_group_flag);
  *is_new_group = is_new_group_flag;
 
  g_group_norm_index_table[group_num] = nor_idx; 
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Group Normalized ID is: (%u)", g_group_norm_index_table[group_num]);

  return nor_idx;   // Return normalized ID
}


/* ------------------------------ GROUP NORMALIZATION SECTION ENDS --------------------------------------- */

/* -------------------------------HOST NORMALIZATION SECTION START --------------------------------------- */

#define DELTA_NORM_HOST_TABLE_SIZE 100
#define INIT_NORM_HOST_TABLE_SIZE 100

//total entries in local mapping table
static int *total_host_table_entries;

//Local mapping table
static normIdType **g_host_norm_index_table;

//Normalized table
static NormObjKey normHostTable;

static void host_norm_init_nvm_table()
{
  //int nvm_id;
 
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, total number of nvms = %d, total number generators = %d", tot_nvms, total_gen_entries);
  NVM_TABLE_SIZE

  NSLB_MALLOC_AND_MEMSET(g_host_norm_index_table, (sizeof(normIdType *) * size), "NVM table per generator", -1, log_fp);
  NSLB_MALLOC_AND_MEMSET(total_host_table_entries, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);
  //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(total_host_table_entries, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);
   
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "size = %d", size); 

#if 0
  for (nvm_id = 0; nvm_id < size; nvm_id++)
  {
    //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_url_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_URL_PER_NVM_TABLE_SIZE), 
               //  "NVM table creation", -1,log_fp);
    NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_host_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_HOST_TABLE_SIZE),
                 "NVM table creation", -1,log_fp);

    total_host_table_entries[nvm_id] = INIT_NORM_HOST_TABLE_SIZE;
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "total_host_table_entries[%d] = %d", nvm_id, total_host_table_entries[nvm_id]);    
  }
#endif
}

static void host_norm_init(int total_nvm, int total_generators) {
  tot_nvms = total_nvm;
  total_gen_entries = total_generators;
  host_norm_init_nvm_table(); 
  nslb_init_norm_id_table(&normHostTable, NormalizedTableSize); 
  /*NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. HOST Table SIze = %d", NormalizedTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_host_norm_index_table, (sizeof(normIdType) * INIT_NORM_HOST_TABLE_SIZE), "HOST table creation", -1, log_fp);
  total_host_table_entries = INIT_NORM_HOST_TABLE_SIZE;

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "total_host_table_entries = %d", total_host_table_entries);
  //init normalized table
  nslb_init_norm_id_table(&normHostTable, NormalizedTableSize);*/
}

unsigned int get_host_norm_id_from_nvm_table(unsigned int nvm_host_index, unsigned int nvm_index, int gen_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. nvm_host_index = %u, nvm_index = %u, gen_id = %d", nvm_host_index, nvm_index, gen_id);
  int index;
  GET_NVM_IDX(gen_id, nvm_index, index);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "total_host_table_entries[%d] = %d", index, total_host_table_entries[index]);
  if(nvm_host_index >= total_host_table_entries[index])
  {
    //fprintf(stderr, "Error: Host idx %d is greater than or equal to total entries %d in host mapping table.\n", nvm_host_index, total_host_table_entries[index]);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Host idx %d is greater than to total entries %d in host mapping table.", nvm_host_index, total_host_table_entries[index]);
    return -1;
  }
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, " get_host_norm_id_from_nvm_table(): nvm_host_index = %u, nvm_index = %u, total_host_table_entries[index] = %u \n" , nvm_host_index, index, total_host_table_entries[index]);

  normIdType *nvmIndexPtr = g_host_norm_index_table[index];
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, " nvmIndexPtr = %d, g_host_norm_index_table[index] = %d\n", *nvmIndexPtr, g_host_norm_index_table[index]);
  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method Ends");
  return *(nvmIndexPtr + nvm_host_index);
}

extern unsigned int get_host_norm_id(unsigned int id, int gen_id, int nvm_id1)
{
  unsigned int nvm_host_id;
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %u (0x%X), gen_id = %d", id, id, gen_id);

  unsigned int nvm_id = GET_URL_PART_NVM_ID(id);  // NVM ID

  if(nvm_id == 0) // static
    nvm_host_id = id;
  else
    nvm_host_id = GET_URL_PART_URL_ID(id); // first three bytes are URL Index
 
  unsigned int ret_val_for_urc = get_host_norm_id_from_nvm_table(nvm_host_id, nvm_id, gen_id); // finding normalized url index.

  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_urc = %u", ret_val_for_urc);
  return ret_val_for_urc;
}

unsigned int get_host_norm_id_for_url(int nvm_id, int nvm_host_index, char *host_name, int host_len, int *is_new_host, int gen_id) 
{
  int index;
  GET_NVM_IDX(gen_id, nvm_id, index);
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "nvm_host_index = %d, total_host_table_entries[%d] = %d, gen_id = %d", nvm_host_index, index, total_host_table_entries[index], gen_id);

  //If Id is more than current size of table, then ralloc table
  //if host_index is much greater than expected then
  if (nvm_host_index >= total_host_table_entries[index])
  {
    int total_host_entries = (total_host_table_entries[index]==-1?0:total_host_table_entries[index]); 
    int max_host_entries = ((nvm_host_index/DELTA_NORM_HOST_TABLE_SIZE) * DELTA_NORM_HOST_TABLE_SIZE) + DELTA_NORM_HOST_TABLE_SIZE;
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_host_norm_index_table[index], (max_host_entries * sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_host_norm_index_table[index] + total_host_entries, -1, (sizeof(normIdType) * (max_host_entries - total_host_entries)));
    total_host_table_entries[index] = max_host_entries;
  }
 
  //If value is not 0, then we already have normalized id 
  unsigned int norm_host_id = get_host_norm_id_from_nvm_table(nvm_host_index, nvm_id, gen_id);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_host_id = %d", norm_host_id);
  if (norm_host_id != -1) // Found
  { 
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_host_id = %d", norm_host_id);
    *is_new_host = 0;
    return norm_host_id;
  }

  // Generation of Normalized Index 
  int is_new_host_flag = 0;
  normIdType *hostIndexPtr = g_host_norm_index_table[index];
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normHostTable, host_name, host_len, &is_new_host_flag);
  *is_new_host = is_new_host_flag;

  *(hostIndexPtr + nvm_host_index) = nor_idx;
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Host Normalized ID is: (%u), nor_idx = %d", g_host_norm_index_table[index][nvm_host_index], nor_idx);
    
  return nor_idx;   // Return normalized ID
}

unsigned int gen_host_norm_id(int *is_new_host, int host_id, int host_len, char *host_name, int gen_id, int nvm_id)
{
  int nvm_host_id;
  NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, is_new_host = %d, host_id = %d, host_len = %d, host_name = %s", *is_new_host, host_id, host_len, host_name);

  //if(IS_URL_STATIC(host_id)) {
  //  nvm_id = 0;
   // nvm_host_index = host_id;
  //} 
  //else {
  //  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Returning host_id = %u", host_id); 
   // nvm_id = GET_URL_PART_NVM_ID(host_id); // NVM ID
    //nvm_host_index = GET_URL_PART_URL_ID(host_id);   // first three bytes are URL Index
 // }
  
  unsigned int nvm_id_loc = GET_URL_PART_NVM_ID(host_id);  // NVM ID

  if(nvm_id_loc == 0) // static
    nvm_host_id = host_id;
  else
    nvm_host_id = GET_URL_PART_URL_ID(host_id); // first three bytes are URL Index

  return(get_host_norm_id_for_url(nvm_id_loc, nvm_host_id, host_name, host_len, is_new_host, gen_id)); 
}

/* -------------------------------HOST NORMALIZATION SECTION END --------------------------------------- */

/* ------------------------------ PAGE NORMALIZATION SECTION STARTS ---------------------------------------- */

#define INIT_NORM_PAGE_TABLE_SIZE 1000 
#define DELTA_NORM_PAGE_TABLE_SIZE 1000

//Normalized table
static NormObjKey normPageTable;

//Local mapping table
static normIdType *g_page_norm_index_table;

//total entries in local mapping table 
static int total_page_table_entries;

static void page_norm_init() {

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. PAGE Table SIze = %d", NormalizedTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_page_norm_index_table, (sizeof(normIdType) * INIT_NORM_PAGE_TABLE_SIZE), "PAGE table creation", -1, log_fp);
  total_page_table_entries = INIT_NORM_PAGE_TABLE_SIZE;

  //Right now using 'DynamicTableSizeiUrl' for all 'URL,SESSION,TX,PAGE'
  //init normalized table
  nslb_init_norm_id_table(&normPageTable, NormalizedTableSize);
}

unsigned int get_page_norm_id(unsigned int page_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. page_id = %u", page_id);

  if(page_id > total_page_table_entries)
  {
    //fprintf(stderr, "Error: Page index %u is greater than total entries %d in page table.\n", page_id, total_page_table_entries);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Page index %u is greater than total entries %d in page table", page_id, total_page_table_entries);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_prc = %u", g_page_norm_index_table[page_id]);
  return(g_page_norm_index_table[page_id]);
}

unsigned int gen_page_norm_id(int page_index, char *page_name, int page_len, int *is_new_page)
{
   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. page_index = %d, page_name = %s, page_len = %d, *is_new_page = %d", page_index, page_name, page_len, *is_new_page);

  //If Index is more than current size of table, then ralloc table
  if (page_index >= total_page_table_entries)
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_page_norm_index_table, ((DELTA_NORM_PAGE_TABLE_SIZE + total_page_table_entries)* sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_page_norm_index_table + total_page_table_entries, -1, (sizeof(normIdType) * DELTA_NORM_PAGE_TABLE_SIZE));
    total_page_table_entries += DELTA_NORM_PAGE_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_page_id = get_page_norm_id(page_index);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_page_id = %u", norm_page_id);
  if (norm_page_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_page_id = %u", norm_page_id);
    *is_new_page = 0;
    return norm_page_id; 
  }

  // Generation of Normalized Index 
  int is_new_page_flag = 0;
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normPageTable, page_name, page_len, &is_new_page_flag);
  *is_new_page = is_new_page_flag;
 
  g_page_norm_index_table[page_index] = nor_idx; 
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Page Normalized ID is: (%u)", g_page_norm_index_table[page_index]);

  return nor_idx;   // Return normalized ID
}

/* ------------------------------ PAGE NORMALIZATION SECTION ENDS ---------------------------------------- */




/* ------------------------------ TX NORMALIZATION SECTION STARTS ---------------------------------------- */

#define INIT_NORM_TX_TABLE_SIZE 1000 
#define DELTA_NORM_TX_TABLE_SIZE 1000

//Normalized table
static NormObjKey normTxTable;

//Local mapping table
static normIdType *g_tx_norm_index_table;
static normIdType **g_tx_norm_index_table_v2;

//total entries in local mapping table 
static int total_tx_table_entries;
static int *total_tx_table_entries_v2;

static void tx_norm_init_nvm_table_v2() {
  //TODO: tot_nvms or tot_txs?
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, total number of nvms = %d, total number generators = %d", tot_nvms, total_gen_entries);
  NVM_TABLE_SIZE

  NSLB_MALLOC_AND_MEMSET(g_tx_norm_index_table_v2, (sizeof(normIdType *) * size), "NVM table per generator", -1, log_fp);
  NSLB_MALLOC_AND_MEMSET(total_tx_table_entries_v2, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);
  //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(total_nvm_table_entries, (sizeof(int) * size), "Total entries per NVM", -1, log_fp);

#if 0    
  for (nvm_id = 0; nvm_id < size; nvm_id++)
  {
    //NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_url_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_URL_PER_NVM_TABLE_SIZE), 
               //  "NVM table creation", -1,log_fp);
    NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_url_norm_index_table[nvm_id], (sizeof(normIdType) * INIT_NORM_URL_PER_NVM_TABLE_SIZE), 
                 "NVM table creation", -1,log_fp);
      
    total_nvm_table_entries[nvm_id] = INIT_NORM_URL_PER_NVM_TABLE_SIZE;
  }
#endif
}

/* Init called  from logging_reader.c */
//This function is called from two places 
//1. from logging reader.In this case max_static_url_ids will 0
//2. from url_id_norm for NetCloud, In NC we know the static ids so just set it
void tx_norm_init_v2(int total_nvm, int total_generators) {
  tot_nvms = total_nvm;
  total_gen_entries = total_generators;
  tx_norm_init_nvm_table_v2();
  nslb_init_norm_id_table(&normTxTable, NormalizedTxTableSize);
}


// Index NVM table using NVM URl Index, if found will return the normalized id.
// If 0 is returned, then we assumed that norm id is not available as dyn url norm id > 0 
//used by for netcloud
unsigned int get_norm_id_from_nvm_table_v2(unsigned int nvm_tx_index, unsigned int nvm_index, int gen_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. nvm_tx_index = %u, nvm_index = %u", nvm_tx_index, nvm_index);
  int index;
  GET_NVM_IDX(gen_id, nvm_index, index);

  if (nvm_tx_index >= total_tx_table_entries_v2[index])
  {
    //fprintf(stderr, "Error: Tx index %u is greater than total entries %d in tx table.\n", nvm_tx_index, total_tx_table_entries_v2[index]);
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Tx index %u is greater than total entries %d in tx table.", nvm_tx_index, total_tx_table_entries_v2[index]);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, " get_norm_id_from_nvm_table(): nvm_tx_index = %u, nvm_index = %u, total_tx_table_entries[index] = %u \n", nvm_tx_index, index, total_tx_table_entries_v2[index]);

  normIdType *nvmIndexPtr = g_tx_norm_index_table_v2[index];

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, " nvmIndexPtr = %d, g_tx_norm_index_table_v2[index] = %d\n", *nvmIndexPtr, g_tx_norm_index_table_v2[index]);

  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method Ends");

  return *(nvmIndexPtr + nvm_tx_index);
}

// Called from logging_reader.c , while filling TX_RECORD. TODO
unsigned int get_tx_norm_id_v2(int nvm_id, unsigned int nvm_tx_id, int gen_id) {
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %u (0x%X)", nvm_tx_id, nvm_tx_id);

  /*unsigned int nvm_id = GET_URL_PART_NVM_ID(id);  // NVM ID

  if(nvm_id == 0) // static
    nvm_url_id = id;
  else
    nvm_url_id = GET_URL_PART_URL_ID(id); // first three bytes are URL Index
*/
  unsigned int ret_val_for_trc = get_norm_id_from_nvm_table_v2(nvm_tx_id, nvm_id, gen_id); // finding normalized url index.

  NSLB_TRACE_LOG3(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_trc = %u", ret_val_for_trc);
  return ret_val_for_trc;
}

static int get_tx_norm_id_for_tx_v2(int nvm_id, int nvm_tx_index, char *tx_name, int tx_len, int *is_new_tx, int gen_id)
{
   int index;
   GET_NVM_IDX(gen_id, nvm_id, index);
   NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "nvm_tx_index = %d, total_tx_table_entries[%d] = %d, gen_id = %d", nvm_tx_index, index, total_tx_table_entries_v2[index], gen_id);
  //If Index is more than current size of nvm table, then ralloc table
  if (nvm_tx_index >= total_tx_table_entries_v2[index])
  {
    int var = (total_tx_table_entries_v2[index]==-1?0:total_tx_table_entries_v2[index]);
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of tx table");
    NSLB_REALLOC(g_tx_norm_index_table_v2[index], ((DELTA_NORM_TX_TABLE_SIZE + var)* sizeof(normIdType)), "realloc tx table", 1, log_fp);
    memset(g_tx_norm_index_table_v2[index] + var, -1, (sizeof(normIdType) * DELTA_NORM_TX_TABLE_SIZE));
    var += DELTA_NORM_TX_TABLE_SIZE;
    total_tx_table_entries_v2[index] = var;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_tx_id = get_norm_id_from_nvm_table_v2(nvm_tx_index, nvm_id, gen_id);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "After get_norm_id_from_nvm_table_v2:norm_tx_id = %u", norm_tx_id);
  if (norm_tx_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_tx_id = %u", norm_tx_id);
    *is_new_tx = 0;
    return norm_tx_id; 
  }

  // Generation of Normalized Index 
  int is_new_tx_flag = 0;
  normIdType *nvmIndexPtr = g_tx_norm_index_table_v2[index];
  unsigned  int nor_idx =  nslb_get_or_gen_norm_id(&normTxTable, tx_name, tx_len, &is_new_tx_flag);
  *is_new_tx = is_new_tx_flag;
  
 *(nvmIndexPtr + nvm_tx_index) = nor_idx;
  

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Normalized ID is: (%u)", g_tx_norm_index_table_v2[index][nvm_tx_index]);

  return nor_idx;   // Return normalized ID
}

unsigned int gen_tx_norm_id_v2(unsigned int tx_index, char *tx_name, int tx_len, int *is_new_tx, int nvm_id, int gen_id)  {

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. is_new_tx = %p, gen_id = %d", is_new_tx, gen_id);

  return(get_tx_norm_id_for_tx_v2(nvm_id, tx_index, tx_name, tx_len, is_new_tx, gen_id));
}

/*
unsigned int get_tx_norm_id_for_generator_v2(char *gen_name, int gen_len, int gen_id, int gen_url_index, int *is_new_url)  
{
  // Treating all URLs from generators as dynamic URL to make code easy
  return(get_tx_norm_id_for_tx_v2(gen_id, gen_url_index, gen_name, gen_len, is_new_url, gen_id));
}
*/
void tx_norm_init() {

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. TX Table SIze = %d", NormalizedTxTableSize);

  //init local mapping table
  NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(g_tx_norm_index_table, (sizeof(normIdType) * INIT_NORM_TX_TABLE_SIZE), "TX table creation", -1, log_fp);
  total_tx_table_entries = INIT_NORM_TX_TABLE_SIZE;

  //Right now using 'DynamicTableSizeiUrl' for all 'URL,SESSION,TX,PAGE'
  //init normalized table
  nslb_init_norm_id_table(&normTxTable, NormalizedTableSize);
}

unsigned int get_tx_norm_id(unsigned int tx_id)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. tx_id = %u", tx_id);

  if(tx_id > total_tx_table_entries)
  {
    NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Tx index %u is greater than total entries %d in tx table", tx_id, total_tx_table_entries);
    //fprintf(stderr, "Error: Tx index %u is greater than total entries %d in tx table.\n", tx_id, total_tx_table_entries);
    return -1;
  }

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End. with ret_val_for_trc = %u", g_tx_norm_index_table[tx_id]);
  return(g_tx_norm_index_table[tx_id]);
}

unsigned int gen_tx_norm_id(int tx_index, char *tx_name, int tx_len, int *is_new_tx)
{
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. tx_index = %d, tx_name = %s, tx_len = %d, *is_new_tx = %d", tx_index, tx_name, tx_len, *is_new_tx);

  //If Index is more than current size of table, then ralloc table
  if (tx_index >= total_tx_table_entries)
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Realloc of nvm table ");
    NSLB_REALLOC(g_tx_norm_index_table, ((DELTA_NORM_TX_TABLE_SIZE + total_tx_table_entries)* sizeof(normIdType)), "realloc table", 1, log_fp);
    memset(g_tx_norm_index_table + total_tx_table_entries, -1, (sizeof(normIdType) * DELTA_NORM_TX_TABLE_SIZE));
    total_tx_table_entries += DELTA_NORM_TX_TABLE_SIZE;
  }

  //If value is not 0, then we already have normalized index 
  unsigned int norm_tx_id = get_tx_norm_id(tx_index);

  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Obtained norm_tx_id = %u", norm_tx_id);
  if (norm_tx_id != -1) // Found
  {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found: norm_tx_id = %u", norm_tx_id);
    *is_new_tx = 0;
    return norm_tx_id; 
  }

  // Generation of Normalized Index 
  int is_new_tx_flag = 0;
  unsigned int nor_idx = nslb_get_or_gen_norm_id(&normTxTable, tx_name, tx_len, &is_new_tx_flag);
  *is_new_tx = is_new_tx_flag;
 
  g_tx_norm_index_table[tx_index] = nor_idx; 
  
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Generated Page Normalized ID is: (%u)", g_tx_norm_index_table[tx_index]);

  return nor_idx;   // Return normalized ID
}
/* ------------------------------ TX NORMALIZATION SECTION ENDS ---------------------------------------- */

//All norm tables init
void object_norm_init(MTTraceLogKey *lr_trace_log_key, int total_nvm, int total_generators)
{
  lr_norm_trace_log_key = lr_trace_log_key;

  NormalizedTableSize = change_to_2_power_n(DynamicTableSizeiUrl);
  NormalizedTxTableSize = change_to_2_power_n(DynamicTableSizeiTx);
  //NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. Dynamic URL Table SIze = %d", NormalizedTableSize);

  url_norm_init(total_nvm, total_generators); //url normalization
  session_norm_init(); //session normalization
  page_norm_init(); //page normalization
  tx_norm_init(); //tx normalization
  tx_norm_init_v2(total_nvm, total_generators);

  //TODO: Calling generator normalization table even if it is not NC test
  generator_norm_init(); //generator normalization
  group_norm_init(); //group normalization
  host_norm_init(total_nvm, total_generators);  //host normalization
}

void reset_norm_id_mapping_tbl()
{
  int nvm_id = 0;
  NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. Reset Url/Session/Page/Tx tables");

  //reset url per nvm table
  NVM_TABLE_SIZE
  while(nvm_id < size) {
    NSLB_TRACE_LOG2(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "nvm_id = %d, total_nvm_table_entries = %d", 
                    nvm_id, total_nvm_table_entries[nvm_id]);
    memset(g_url_norm_index_table[nvm_id], -1, (sizeof(normIdType) * total_nvm_table_entries[nvm_id]));
    total_nvm_table_entries[nvm_id] = 0;
    memset(g_host_norm_index_table[nvm_id], -1, (sizeof(normIdType) * total_host_table_entries[nvm_id]));
    total_host_table_entries[nvm_id] = 0;
    memset(g_tx_norm_index_table_v2[nvm_id], -1, (sizeof(normIdType) * total_tx_table_entries_v2[nvm_id])); 
    total_tx_table_entries_v2[nvm_id] = 0;
    nvm_id++; 
  }

  //reset session table
  memset(g_session_norm_index_table, -1, (sizeof(normIdType) * total_session_table_entries)); 
  total_session_table_entries = 0;

  //reset page table
  memset(g_page_norm_index_table, -1, (sizeof(normIdType) * total_page_table_entries)); 
  total_page_table_entries = 0;

  //reset group table
  memset(g_group_norm_index_table, -1, (sizeof(normIdType) * total_group_table_entries)); 
  //total_group_table_entries = 0;

  //reset generator table
  memset(g_generator_norm_index_table, -1, (sizeof(normIdType) * total_generator_table_entries));
  //total_generator_table_entries = 0;

  //reset host table 
  /*while(nvm_id < size) {
    memset(g_host_norm_index_table[nvm_id], -1, (sizeof(normIdType) * total_host_table_entries[nvm_id]));
    total_host_table_entries[nvm_id] = 0;
    nvm_id++;
  }*/

}


/********* Loading of URL, Page, Tx, Session normalization table from csv file on start  ************************/

 /**************************************************************
 * build_norm_tables_from_metadata_csv()
 * Read Meta Data csv file and build norm tables.
 *************************************************************/
//Add another argument to pass trace logging key
//inline void build_norm_tables_from_metadata_csv(char *test_run_num, char *common_files_dir)
inline void build_norm_tables_from_metadata_csv(int test_run_num, char *common_files_dir)
{
  /* Normalized Table Recovery Map (ntrmap)*/
  struct 
  {
   NormObjKey *key; 
   char csvname[64];
   char name_fieldnum;
   char len_fieldnum;
   char normid_fieldnum;
  } ntrmap[] =

  { 
    //7010,9,1,0,0,17,/tours/index.html,23,page1:/tours/index.html
    {&normUrlTable, "urt.csv", 8, 7, 1}, 
    //7010,1,0,login,13,script1:login
    {&normPageTable, "pgt.csv", 5, 4, 1},
    //7010,0,hpd_tours_c_1
    {&normSessionTable, "sst.csv", 2, -1, 1},
    //1001,3,Tx_AddToBag
    {&normTxTable, "trt.csv", 2, -1, 1},
    //MAC_66_WORK4,0,/home/cavisson/work4,192.168.1.66,192.168.1.66,7891
    {&normGeneratorTable, "generator_table.csv", 0, -1, 1},
    //7102,0,0,-1,50,g1
    {&normGroupTable, "rpf.csv", 5, -1, 1},
    //10.10.70.6:9008, 0
    {&normHostTable, "HostTable.csv", 1, -1, 0}
  };
  
  NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. test_run_num = %d, common_files_dir = %s", test_run_num, common_files_dir);
  int num_obj_types = sizeof(ntrmap) / sizeof(ntrmap[0]); // Num of object types
  int obj_iter;
  for(obj_iter = 0; obj_iter < num_obj_types; obj_iter++)
  {
       /* Open csv file */
       char filename[512];
       snprintf(filename, 512, "logs/TR%d/%s/reports/csv/%s", test_run_num, common_files_dir, ntrmap[obj_iter].csvname);
 
       FILE *fp = fopen(filename, "r");
       if(!fp)
       {
         NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Could not open file %s,"
                      " for reading while creating in memory normalised table\n", filename);
         continue;
       }

       /* Find the max number of fields to be read */ 
       int max = 0;
       if(ntrmap[obj_iter].name_fieldnum > max) max = ntrmap[obj_iter].name_fieldnum;
       if(ntrmap[obj_iter].len_fieldnum > max) max = ntrmap[obj_iter].len_fieldnum;
       if(ntrmap[obj_iter].normid_fieldnum > max) max = ntrmap[obj_iter].normid_fieldnum;
       max++; //add 1 to max as max is a number and field num is index
          
       NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "max = %d", max);

       /* Read line by line */
       char line[64*1024];
       while(nslb_fgets(line, 64*1024, fp, 0))
       {
         /* Split the fields of line */
         char *fields[max];
         int num_fields;
         num_fields = get_tokens_with_multi_delimiter(line, fields, ",", max); 
         if(num_fields < max)
         {
           NSLB_TRACE_LOG1(lr_norm_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Error in reading file %s," 
                           "while creating in memory normalised table. "
                           "Number of fields are less than expected in the read line '%s'\n", filename, line);
           continue; // Skip this line
         }

         /* Now set the normalized ID */
         /*unsigned int nslb_set_norm_id(NormObjKey *key, char *in_str, int in_strlen, unsigned int normid)*/

         char *newline_ptr = NULL;
         newline_ptr = strstr(fields[(int)(ntrmap[obj_iter].name_fieldnum)], "\n");
         if(newline_ptr != NULL)
           *newline_ptr = '\0'; 

         if(ntrmap[obj_iter].len_fieldnum == -1) //Since len field is not there, use strlen 
           nslb_set_norm_id(ntrmap[obj_iter].key, 
                            fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                            strlen(fields[(int)(ntrmap[obj_iter].name_fieldnum)]), 
                            atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)])); 
         else
           nslb_set_norm_id(ntrmap[obj_iter].key, 
                            fields[(int)(ntrmap[obj_iter].name_fieldnum)], 
                            atoi(fields[(int)(ntrmap[obj_iter].len_fieldnum)]), 
                            atoi(fields[(int)(ntrmap[obj_iter].normid_fieldnum)])); 
       }
       if(fp) 
         fclose(fp); 
       fp = NULL;
  }
}
