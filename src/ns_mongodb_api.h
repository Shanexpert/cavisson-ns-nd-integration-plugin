#ifndef NS_MONGODB_API_H
#define NS_MONGODB_API_H   

#include <bson.h>
#include <mongoc.h>

#define NS_MONGODB_MAX_LEN_128B        128
#define NS_MONGODB_MAX_LEN_256B        256
#define NS_MONGODB_MAX_LEN_1K          1024
#define NS_MONGODB_MAX_LEN_2K          2048
#define NS_MONGODB_MAX_LEN_4K          4096
#define NS_MONGODB_MAX_LEN_10M         10240 

#define mongodbt(p)    ((struct mongodb_s *)(p)) 

typedef struct mongodb_s
{
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  mongoc_cursor_t *cursor;  
  char *dbname;
  int dbname_len;
  char *query_result;
  int query_result_len;
} mongodb_t;

extern void ns_mongodb_init();

#endif
