#ifndef NS_DYNAMIC_HOSTS_H
#define NS_DYNAMIC_HOSTS_H

#define ERR_DYNAMIC_HOST_TBL_FUL -1
#define ERR_IP_NOT_RESOLVED -2
#define ERR_DYNAMIC_HOST_DISABLE -3
#ifndef CAV_MAIN
extern int num_dyn_host_left; //Number of dynamic host left
#else
extern __thread int num_dyn_host_left; //Number of dynamic host left
#endif
extern int kw_set_max_dyn_host(char *buf, char *err_msg, int runtime_flag);
extern void setup_rec_tbl_dyn_host(int max_dyn_host);
extern int add_dynamic_hosts (VUser *vptr, char *hostname, short port, int main_or_inline, int redirect_flag, int request_type, char *url, char *session_name, char *page_name, int user_id, char *scenario_group);
//Macros used in auto redirection 
#define ERR_MAIN_URL_ABORT -1
#define ERR_EMBD_URL_ABORT -2

#endif//NS_DYNAMIC_HOSTS_H
