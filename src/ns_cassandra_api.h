#ifndef NS_CASSDB_API_H
#define NS_CASSDB_API_H   

#include "cassandra.h"
#define NS_CASSDB_MAX_LEN_10M         10240 
#define NS_CASSDB_MAX_LEN_4K          4096
#define NS_CASSDB_MAX_LEN_256B        256

typedef struct {
  char password[256];
  char username[256];
} Credentials;

typedef struct 
{
  CassCluster* cluster;
  CassSession* cass_session;
  //CassFuture* future;
  const CassResult* result;
  //char *dbname;
  //int dbname_len;
  char *query_result;
  int query_result_len;
} cassdb_t;

#define cassdbt(p)    ((cassdb_t *)(p))

#endif
 

