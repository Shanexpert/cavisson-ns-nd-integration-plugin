/******************************************************************
 * Name    :    ns_embd_objects.h
 * Purpose :    This file contains methods to extract all Embedded
                URL from buffer.
 * Author  :    Archana
 * Intial version date:    20/08/08
 * Last modification date: 25/08/08
 * Note    :
*******************************************************************/

#ifndef _NS_EMBD_OBJECTS_H_
#define _NS_EMBD_OBJECTS_H_

#define XML_TEXT_JAVASCRIPT             0x00000001
#include <regex.h>

#define INCLUDE_DOMAIN 0
#define INCLUDE_URL 1 
#define EXCLUDE_DOMAIN 2
#define EXCLUDE_URL 3

#define KEEP_URL 0 
#define REMOVE_URL 1 


typedef struct {
  unsigned int embd_type;
  unsigned int duration; 
  char *embd_url;
} EmbdUrlsProp;

extern void free_embd_array(EmbdUrlsProp *extracted_embd_url, int num_urls);
extern EmbdUrlsProp* get_embd_objects(char *buffer, unsigned int len, int *num_embd_urls, char *errMsg);
extern EmbdUrlsProp* get_embd_m3u8_url(char *buffer, unsigned int len, int *num_embd_urls, char *errMsg, int bandwidth);

#define INCLUDE_PATTERN 0
#define EXCLUDE_PATTERN 1 

typedef struct {
  regex_t comp_regex;
} RegArray;
RegArray *reg_array_ptr;

typedef struct {
  int reg_start_idx;
  int num_entries;
} PatternTable;

typedef struct {
  int reg_start_idx;
  int num_entries;
} PatternTable_Shr;

#ifndef CAV_MAIN
extern PatternTable_Shr *pattern_table_shr;
extern PatternTable *pattern_table;
#else
extern __thread PatternTable_Shr *pattern_table_shr;
extern __thread PatternTable *pattern_table;
#endif


#endif
