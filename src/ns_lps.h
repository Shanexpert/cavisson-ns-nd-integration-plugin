#define LPS_CONFIG   "lps.conf"
//#define NS_LPS_TYPE   0
extern int kw_set_log_server(char *buf, int flag);
extern inline char *ndcollector_log_event();
extern int lps_recovery_connect();
extern inline void handle_lps(struct epoll_event *pfds, int i);
extern void start_ns_lps();

