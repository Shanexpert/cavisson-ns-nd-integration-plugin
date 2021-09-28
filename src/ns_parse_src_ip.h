
#ifndef NS_PARSE_SRC_IP_H
#define NS_PARSE_SRC_IP_H
extern int kw_set_use_src_ip (char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_src_ip_list (char *buf, GroupSettings *gset, char *err_msg);
extern void kw_set_src_port_mode (char * text);
extern int kw_set_ip_version_mode(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_use_same_netid_src(char *buf, short *use_same_netid_src, char *err_msg, int runtime_flag);
extern void read_ip_file();

#define USE_PRIMARY_IP 0 
#define USE_SHARED_IP  1
#define USE_UNIQUE_IP  2
#define USE_SHARED_IP_FROM_FILE 3
#define USE_UNIQUE_IP_FROM_FILE 4
#define SRC_IP_PRIMARY -1
#define SRC_IP_SHARED 0 
#define SRC_IP_UNIQUE 1
#define IP_VERSION_MODE_AUTO 0
#define IP_VERSION_MODE_IPV4 1
#define IP_VERSION_MODE_IPV6 2



#endif
