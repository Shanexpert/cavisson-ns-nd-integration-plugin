#include "ns_data_types.h"

#ifndef NS_URL_HASH_H
#define NS_URL_HASH_H

/*
Mode0: Disable. It will not create any static url table.
Mode1: Enable. Create static url hash table using gperf.
Mode2: Enable. Create static url hash table using own dyn hash algo.
*/
#define NS_STATIC_URL_HASH_DISABLED               0
#define NS_STATIC_URL_HASH_USING_GPERF            1
#define NS_STATIC_URL_HASH_USING_DYNAMIC_HASH     2

/*
Mode0: Disable. It will not create any dynamic url table.
Mode1: Enable (Default). Create dynamic url hash table without using query-parameter in url.
    Example:
    i.      /abc/a.html will be same as /abc/a.html?name=Neeraj
    ii.     /abc/a.html will be same as /abc/a.html#name=Neeraj
Mode2: Enable. Create dynamic url hash table using complete url.
*/
#define NS_DYNAMIC_URL_HASH_DISABLED              0
#define NS_DYNAMIC_URL_HASH_WITHOUT_QUERY_PARAM   1
#define NS_DYNAMIC_URL_HASH_USING_COMPLETE_URL    2

#define URL_SUCCESS 0
#define URL_ERROR -1

//Mode to treat static parametrized url as dynamic url
#define NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_DISABLED     0
#define NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED      1

#ifdef TEST
char g_ns_tmpdir[50] ;
#define MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
        int sz =(int)size;      \
      fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", sz, index); \
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
        fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
      }  \
    } \
  }



#define MY_MALLOC(new, size, msg, index) {                              \
    if (size < 0)                                                       \
      {                                                                 \
        int sz = (int)size;                                             \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", sz, index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
        }                                                               \
      }                                                                 \
  }


#define MY_MALLOC_AND_MEMSET(new, size, msg, index) {                           \
    if (size < 0)                                                       \
      {                                                                 \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", (int)size, index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          fprintf(stderr, "Out of Memory: %s for index %d\n", msg, index); \
        }                                                               \
        memset(new, 0, size);                                           \
        if (NULL == new)                                                \
        {                                                               \
           fprintf(stderr, "Initialization Error: %s for index %d, size=%d", msg, index, (int)size);  \
        }                                                                                              \
      }                                                                 \
  }

#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    free((void*)to_free);  \
    to_free = NULL; \
  } \
}

typedef char *(*Hash_code_to_str)(unsigned int);
#define MAX_LINE_LENGTH 3500

#define CACHE_FNV_OFFSET_BASIS 2166136261u
#define CACHE_FNV_PRIME 16777619

extern int match_url(u_ns_char_t *url, unsigned int ihashValue, u_ns_char_t *url_to_search, int iUrlLen, unsigned int ihashValue_to_search);


#endif  //TEST

extern inline void alloc_mem_for_urltable ();
extern void url_hash_add_url(u_ns_char_t *complete_url, int url_id, int page_id, char *page_name); 
extern int url_hash_create_file(char *url_file);
//extern int url_hash_close_file(); 
extern int url_hash_create_hash_table(char *url_file);
extern void url_hash_dynamic_table_init();
extern int url_hash_get_url_idx_for_dynamic_urls(u_ns_char_t *url, int iUrlLen, int page_id, int static_dynamic, int static_url_id, char *page_name);

extern int dynamic_url_destroy();

extern int kw_set_static_url_hash_table_option(char *buf, int flag);
extern int kw_set_dynamic_url_hash_table_option(char *buf, int flag);

extern void free_url_hash_table();
extern int dynamic_url_dump();
#endif  //NS_URL_HASH_H
