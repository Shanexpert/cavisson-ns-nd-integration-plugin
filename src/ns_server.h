#ifndef NS_SERVER_H
#define NS_SERVER_H

#include <netinet/in.h>
#include "ns_data_types.h"

#ifndef MAX_DATA_LINE_LENGTH
  #define MAX_DATA_LINE_LENGTH 2048 
#endif

#ifndef MAX_DATA_LENGTH
  #define MAX_DATA_LENGTH 1024
#endif

#ifndef MAX_FIELDS
  #define MAX_FIELDS 100
#endif

#define SERVER_MIN 1
#define SERVER_ANY 2

//Macro to set server_flags
//      --------------------------------------------------------------------
//      |    |    |    |     |     |WITHOUT_PORT|IS_DOMAIN|ALREADY_RESOLVED|
//      --------------------------------------------------------------------
//       256  128    32   16    8         4          2          1
#define NS_SVR_FLAG_SVR_ALREADY_RESOLVED 0x00000001
#define NS_SVR_FLAG_SVR_IS_DOMAIN 0x00000002
#define NS_SVR_FLAG_SVR_WITHOUT_PORT 0x00000004  //This flag will indicate whether given host is with port or without port

#ifndef CAV_MAIN
extern int gNAServerHost;
#else
extern __thread int gNAServerHost;
#endif

typedef struct
{
  int sess_host_idx;
} sessHostTableEntry;
  
//Table will be filled in parsing of script (Data of Recorded Host)
typedef struct SvrTableEntry {
  ns_bigbuf_t server_hostname;		// Name of Recorded host, big buf offset
  int idx;				// Indexing of host (for mapping of script's and scenario's recorded server i.e idx and hostidx)
  //struct sockaddr_in saddr;
  unsigned short server_port;		// Recorded host's port
  short type;   			// Either MIN or ANY
  short request_type;			// Request type 0 for HTTP 1 for HTPPS etc
  short tls_version; 			// TLS version 
  char main_url_host;     		// Is it a MAIN URL Host  1/0
} SvrTableEntry;

//Table will be filled in parsing of keyword G_SERVER_HOST (Data of Actual Svr)
typedef struct PerHostSvrTableEntry {
  char server_name[MAX_DATA_LENGTH];	// Name of actual-svr, offset of big_buf //TODO
  int server_name_len;
  struct sockaddr_in6 saddr;		// Actual svr’s information
  int host_id;				// same as in gServerTableEntry
  char loc_name[MAX_DATA_LINE_LENGTH];// Location name	//TODO
  int loc_idx;				// Location idx
  //char server_already_resolved;   	// look up done (1) or not (0)
  char server_flags; 			// 1 bit for NS_SVR_FLAG_SVR_ALREADY_RESOLVED
                     			// NS_SVR_FLAG_SVR_IS_DOMAIN
  u_ns_ts_t last_resolved_time;
  short net_idx;
} PerHostSvrTableEntry;

//This will be stored data of actual svr 
typedef struct PerGrpHostTableEntry {
  int host_idx;				// Indexing of host (for mapping of script's and scenario's recorded server i.e idx and hostidx)
  int total_act_svr_entries;		// Total no of server entries the in the recorded-host.
  int max_act_svr_entries;		// Max no of server entries the in the recorded-host.
  char grp_dynamic_host;                // Host is dynamic for that grp
  PerHostSvrTableEntry *server_table;	// Ptr to PerHostServerEntry data structure
} PerGrpHostTableEntry;

//This will be stored data of recorded host
typedef struct GrpSvrHostSettings {
  int total_rec_host_entries;		// Total no of host entries in the same group
  int max_rec_host_entries;		// Max no of host entries in the same group
  PerGrpHostTableEntry *host_table;	// Ptr to PerGrpHostEntry data structure
} GrpSvrHostSettings;

/*
typedef struct PerHostSvrTableEntry_Shr {
  char*  server_name; 			// Server name with port if any. This is point to buf buffer shared memory
  int server_name_len;
  int host_id;				// Host id of host with respect to group
  struct sockaddr_in6 saddr;
  int loc_idx;
  short net_idx;
  //char server_already_resolved;   	// look up done (1) or not (0)
  char server_flags; 			// 1 bit for NS_SVR_FLAG_SVR_ALREADY_RESOLVED
                    			// NS_SVR_FLAG_SVR_IS_DOMAIN
  u_ns_ts_t last_resolved_time;
  //struct sockaddr_in6 resolve_addr;
} PerHostSvrTableEntry_Shr;
*/
typedef PerGrpHostTableEntry PerGrpHostTableEntry_Shr;  //temp
typedef PerHostSvrTableEntry PerHostSvrTableEntry_Shr;  //temp
typedef GrpSvrHostSettings GrpSvrHostSettings_Shr;  //temp
/*
typedef struct PerGrpHostTableEntry_Shr {
  int host_idx;
  int total_act_svr_entries;
  PerHostSvrTableEntry_Shr *server_table;
} PerGrpHostTableEntry_Shr;


//This will be stored data of recorded host
typedef struct GrpSvrHostTableEntry_Shr {
  int total_rec_host_entries;
  PerGrpHostTableEntry_Shr *host_table;
} GrpSvrHostTableEntry_Shr;
*/

typedef struct {
  PerHostSvrTableEntry_Shr* svr_ptr;
} UserSvrEntry;

typedef struct SvrTableEntry_Shr {
  char* server_hostname;  		// Pointer into the shared big buf
  int server_hostname_len; 		// for keyword USE_RECORDED_HOST_IN_HOST_HDR:Anuj 08/03/08
  unsigned short server_port;		// Recorded host's port
  //int num_svrs;
  int idx;				// Id of host in script	
  short type;   			// could be either SERVER_ANY or SERVER_MIN
  short tls_version; 			// TLS version
  PerHostSvrTableEntry_Shr *totsvr_table_ptr;  //temp remove
} SvrTableEntry_Shr;

typedef struct StaticHostTable {
  char host_name[MAX_DATA_LENGTH];
  char ip[64];
  int family;
} StaticHostTable;

typedef struct PerGrpStaticHostTable {
  int total_static_host_entries;
  int max_static_host_entries;
  StaticHostTable *static_host_table;
} PerGrpStaticHostTable;

#define ANY_ACTUAL_SERVER 0
#define SAME_ACTUAL_SERVER 1

#define CAL_SHR_SIZE(svr_host_settings) \
{ \
  int h; \
  NSDL3_HTTP(NULL, NULL, "Method called, svr_host_settings = %p", svr_host_settings); \
  int total_rec_host_entries = svr_host_settings->total_rec_host_entries; \
  for(h = 0; h < total_rec_host_entries; h++) \
  { \
    int total_act_svr_entries =  svr_host_settings->host_table[h].total_act_svr_entries; \
    svr_table_shm_size += sizeof(PerHostSvrTableEntry) * total_act_svr_entries; \
    total_totsvr_entries +=  total_act_svr_entries; \
    NSDL4_HTTP(NULL, NULL, "total_totsvr_entries = %d, total_act_svr_entries = %d", total_totsvr_entries, total_act_svr_entries); \
  } \
  host_table_shm_size += sizeof(PerGrpHostTableEntry) * total_rec_host_entries; \
  total_shm_size = host_table_shm_size + svr_table_shm_size ; \
 \
}

#define FILL_SHR_MEM(svr_host_settings) \
{\
  int h, s;  \
  \
  for(h = 0; h < svr_host_settings->total_rec_host_entries; h++, host_table_ptr++) \
  { \
    host_table_ptr->host_idx = svr_host_settings->host_table[h].host_idx; \
    host_table_ptr->grp_dynamic_host = svr_host_settings->host_table[h].grp_dynamic_host; \
    host_table_ptr->server_table = svr_table_ptr; \
    host_table_ptr->total_act_svr_entries = svr_host_settings->host_table[h].total_act_svr_entries; \
    NSDL3_HTTP(NULL, NULL, "h = %d, total_act_svr_entries = %d", h, host_table_ptr->total_act_svr_entries); \
    for(s=0; s < host_table_ptr->total_act_svr_entries; s++, svr_table_ptr++) \
    { \
      NSDL3_HTTP(NULL, NULL, "server_name = %s, for host = %d, for svr = %d, loc_name = %s", svr_host_settings->host_table[h].server_table[s].server_name, h, s, svr_host_settings->host_table[h].server_table[s].loc_name); \
      strcpy(svr_table_ptr->server_name, svr_host_settings->host_table[h].server_table[s].server_name); \
      svr_table_ptr->server_name_len = strlen(svr_host_settings->host_table[h].server_table[s].server_name); \
      memcpy(&svr_table_ptr->saddr, &(svr_host_settings->host_table[h].server_table[s].saddr), sizeof(struct sockaddr_in6)); \
      /* host_id is -1, we are filling this in log_host_table() */ \
      svr_table_ptr->host_id = -1; \
      strcpy(svr_table_ptr->loc_name, svr_host_settings->host_table[h].server_table[s].loc_name); \
      svr_table_ptr->loc_idx = svr_host_settings->host_table[h].server_table[s].loc_idx; \
      svr_table_ptr->server_flags = svr_host_settings->host_table[h].server_table[s].server_flags; \
      svr_table_ptr->last_resolved_time = now; \
      svr_table_ptr->net_idx = -1; \
      if(!index(svr_table_ptr->server_name, ':'))\
      { \
        NSDL2_HTTP(NULL, NULL, "Setting server_flag for hostname '%s' to NS_SVR_FLAG_SVR_WITHOUT_PORT.", svr_table_ptr->server_name); \
        svr_table_ptr->server_flags |= NS_SVR_FLAG_SVR_WITHOUT_PORT; \
      } \
    } \
    if(svr_host_settings->host_table[h].server_table) \
      FREE_AND_MAKE_NULL_EX (svr_host_settings->host_table[h].server_table, (svr_host_settings->host_table[h].max_act_svr_entries * sizeof(PerHostSvrTableEntry)), "PerHostSvrTableEntry", -1); \
  } \
  if(svr_host_settings->host_table) \
      FREE_AND_MAKE_NULL_EX (svr_host_settings->host_table, (svr_host_settings->max_rec_host_entries * sizeof(PerGrpHostTableEntry)), "PerGrpHostTableEntry", -1); \
}

#define FIND_GRP_HOST_ENTRY(svr_host_settings_ptr, svr_idx, host_ptr)\
{\
  int h;\
  NSDL2_HTTP(NULL, NULL, "svr_host_settings_ptr = %p, svr_idx = %d, host_ptr = %p", svr_host_settings_ptr, svr_idx, host_ptr); \
  PerGrpHostTableEntry *host_table_ptr = svr_host_settings_ptr->host_table; \
  int total_rec_host_entries = svr_host_settings_ptr->total_rec_host_entries; \
  NSDL2_HTTP(NULL, NULL, "Finding entry host_table_ptr = %p, total_rec_host_entries = %d", host_table_ptr, total_rec_host_entries); \
  host_ptr = NULL;\
  for(h = 0; h < total_rec_host_entries; h++) { \
    if (host_table_ptr[h].host_idx == svr_idx) \
    {\
      host_ptr = &host_table_ptr[h];\
      NSDL2_HTTP(NULL, NULL, "host_ptr = %p, total_act_svr_entries = %d", host_ptr, host_ptr->total_act_svr_entries); \
      break;\
    }\
  }\
}

#define FIND_GRP_HOST_ENTRY_SHR(grp_idx, svr_idx, host_ptr)\
{\
  int h;\
  GrpSvrHostSettings_Shr *svr_host_settings_shr = &runprof_table_shr_mem[grp_idx].gset.svr_host_settings;\
  PerGrpHostTableEntry_Shr *host_table_ptr = svr_host_settings_shr->host_table; \
  int total_rec_host_entries = svr_host_settings_shr->total_rec_host_entries; \
  host_ptr = NULL;\
  NSDL3_HTTP(NULL, NULL, "For Group[%d], total_rec_host_entries %d, svr_idx = %d", grp_idx, total_rec_host_entries, svr_idx); \
  for(h = 0; h < total_rec_host_entries; h++) { \
    NSDL3_HTTP(NULL, NULL, "host_table_ptr[h].host_idx = %d, svr_idx = %d, server_name = %s", host_table_ptr[h].host_idx, svr_idx, host_table_ptr[h].server_table->server_name); \
    if (host_table_ptr[h].host_idx == svr_idx) \
    {\
      host_ptr = &host_table_ptr[h];\
      break;\
    }\
  }\
}

#define FREE_NON_SHR_MEM(svr_host_settings) \
{ \
  NSDL3_HTTP(NULL, NULL, "Method called, svr_host_settings = %p", svr_host_settings); \
  total_rec_host_entries = svr_host_settings->total_rec_host_entries; \
  \
  NSDL3_HTTP(NULL, NULL, "total_rec_host_entries = %d, max_rec_host_entries = %d", total_rec_host_entries, svr_host_settings->max_rec_host_entries); \
  for(host_idx = 0; host_idx < total_rec_host_entries; host_idx++) \
  { \
    host_table = &svr_host_settings->host_table[host_idx]; \
    NSDL3_HTTP(NULL, NULL, "host_table = %p, host_table->server_table = %p", host_table, host_table->server_table); \
    \
    if(host_table->server_table != NULL) \
      FREE_AND_MAKE_NULL_EX (host_table->server_table, (host_table->max_act_svr_entries * sizeof(PerHostSvrTableEntry)), "PerHostSvrTableEntry", -1); \
  } \
  if(svr_host_settings->max_rec_host_entries && svr_host_settings != NULL) \
    FREE_AND_MAKE_NULL_EX (svr_host_settings->host_table, (svr_host_settings->max_rec_host_entries * sizeof(PerGrpHostTableEntry)), "PerGrpHostTableEntry", -1); \
}

#define CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_index, msg) \
{ \
  int matched=0, i; \
  NSDL3_PARSING(NULL, NULL, "%s, %d", msg, url_index); \
  for(i=0; i< gSessionTable[sess_idx].total_sess_host_table_entries; i++) \
  { \
    if(gSessionTable[sess_idx].host_table_entries[i].sess_host_idx == requests[url_index].index.svr_idx) \
    { \
      matched=1; \
      break; \
    } \
  } \
  if(!matched) \
  { \
    if(create_and_fill_sess_host_table_entry(&gSessionTable[sess_idx], sess_idx, requests[url_index].index.svr_idx) != SUCCESS) \
      return -1; \
  } \
  NSDL3_PARSING(NULL, NULL, "host_id = %lu, hostname = %s", requests[url_index].index.svr_idx, RETRIEVE_BUFFER_DATA(gServerTable[requests[url_index].index.svr_idx].server_hostname)); \
} 

#define CAL_STATIC_HOST_SHR_SIZE(static_host_settings) \
{ \
  int total_static_host_entries; \
  \
  NSDL3_HTTP(NULL, NULL, "Method called, svr_host_settings = %p", static_host_settings); \
  total_static_host_entries = static_host_settings->total_static_host_entries; \
  static_host_table_shm_size += sizeof(PerGrpStaticHostTable)  + (sizeof(StaticHostTable) * total_static_host_entries); \
 \
}

#define FILL_STATIC_HOST_SHR_MEM(per_group_static_host_settings) \
{\
  int i; \
  for(i=0; i < total_static_host_entries; i++, tot_static_host_shr_mem++) { \
  NSDL2_PARSING(NULL, NULL, " i = %d host_name = %s ip = %s", i, per_group_static_host_settings->static_host_table[i].host_name, per_group_static_host_settings->static_host_table[i].ip); \
    strcpy(tot_static_host_shr_mem->host_name, per_group_static_host_settings->static_host_table[i].host_name) ; \
    strcpy(tot_static_host_shr_mem->ip, per_group_static_host_settings->static_host_table[i].ip); \
    tot_static_host_shr_mem->family = per_group_static_host_settings->static_host_table[i].family; \
  } \
  if(per_group_static_host_settings->static_host_table)\
      FREE_AND_MAKE_NULL_EX (per_group_static_host_settings->static_host_table, (per_group_static_host_settings->max_static_host_entries * sizeof(StaticHostTable)), "Static Host Table", -1); \
}

#ifndef CAV_MAIN 
extern SvrTableEntry *gServerTable;
extern SvrTableEntry_Shr *gserver_table_shr_mem;
extern int total_totsvr_entries;
extern PerHostSvrTableEntry_Shr *totsvr_table_shr_mem;
#else
extern __thread SvrTableEntry *gServerTable;
extern __thread SvrTableEntry_Shr *gserver_table_shr_mem;
extern __thread int total_totsvr_entries;
extern __thread PerHostSvrTableEntry_Shr *totsvr_table_shr_mem;
#endif

extern int max_static_host_id;
extern int create_totsvr_table_entry(int *row_num);
extern void find_gserver_idx(char* name, int port, int *search_index, int host_name_len);
extern int find_gserver_shr_idx(char* name, int port, int hostname_len);
extern int create_svr_table_entry(int *row_num);
extern short get_server_idx(char *hostname, int request_type, int line_num);
extern short get_parameterized_server_idx(char *hostname, int request_type, int line_num);
extern int insert_totsvr_shr_mem(void);
extern int find_host_name_length_without_port(char *host_name, unsigned short *port);
extern void free_host_table_mem();
extern PerHostSvrTableEntry_Shr* find_actual_server_shr(char* serv, int grp_idx, int host_idx);
extern int ns_get_host(int grp_idx, struct sockaddr_in6 *addr, char *server_name, int server_port, char *err_msg);
extern int kw_set_g_static_host();
extern void insert_totstatic_host_shr_mem();
extern int get_is_static_host_shm_created();
#endif
