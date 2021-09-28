#ifndef NS_CONNECTION_POOL_H
#define NS_CONNECTION_POOL_H

#define INIT_CONN_BUFFER  1024 //Connection buffer size
#ifndef MAX_DATA_LINE_LENGTH
  #define MAX_DATA_LINE_LENGTH 2048 
#endif

#define MAX_TEXT_LINE_LENGTH 10*1048
 
/* Macro to check whether protocol is http2 . We also set http_protocol in cptr */
#define SET_HTTP_VERSION() {\
  free->http_protocol = runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode;\
}


//Global variables
extern connection* gFreeConnHead;
extern connection* gFreeConnTail;
extern connection* total_conn_list_head;
extern connection* total_conn_list_tail;

extern inline void free_connection_slot(connection* cptr, u_ns_ts_t now);
extern inline connection *remove_from_all_reuse_list(connection *cptr);
extern int kw_set_g_max_con_per_vuser(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int validate_browser_connection_values(int max_con_per_svr_http1_0, int max_con_per_svr_http1_1, int max_proxy_per_svr_http1_0, int max_proxy_per_svr_http1_1, int max_con_per_vuser, char *browser_name);
#endif//NS_CONNECTION_POOL_H

