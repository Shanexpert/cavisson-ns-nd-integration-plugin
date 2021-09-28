#include "nslb_get_norm_obj_id.h"

#ifndef NS_SVR_IP_DATA_H
#define NS_SVR_IP_DATA_H

#define SHOW_SERVER_IP_DATA_ENABLED 1
#define SHOW_SERVER_IP ((global_settings->show_server_ip_data == SHOW_SERVER_IP_DATA_ENABLED)?SHOW_SERVER_IP_DATA_ENABLED:0)
#define DELTA_SRV_IP_ENTRIES 10
#define INIT_SRV_IP_ENTRIES 20

typedef struct
{       
  Long_data url_req;                      // URL requests sent/sec
} SrvIPStatGP;

typedef struct
{        
  int cur_url_req;                            // URL requests sent/sec
  //delay_time;
  //int ip_norm_id;
  //int tot_normalized_svr_ips;
  char ip[1024];
} SrvIPAvgTime;

extern NormObjKey normServerIPTable;
extern char dynamic_feature_name[MAX_DYN_OBJS][32];

extern int max_srv_ip_entries;
extern int total_normalized_svr_ips;
extern SrvIPAvgTime *srv_ip_avgtime;
extern SrvIPStatGP *srv_ip_stat_gp_ptr;
extern unsigned int srv_ip_data_idx;
extern unsigned int srv_ip_data_gp_idx;
extern int kw_set_show_server_ip_data(char *buf, char *err_msg, int runtime_flag);
extern inline void set_srv_ip_based_stat_avgtime_ptr();
extern void fill_srv_ip_data_gp(avgtime **g_avg);
extern char **printSrvIpStat(int group_id);
extern void printSrvIpStatGraph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId);
extern inline void update_srv_ip_data_avgtime_size();
extern void increment_srv_ip_data_counter(char *server_name, int server_name_len, int norm_id);
extern void update_counters_for_this_server(VUser *vptr, struct sockaddr_in6 *addr_in, char* host_name, int host_name_len);
extern void dump_svr_ip_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *c_avg, char *heading);
extern int  realloc_avgtime_and_set_ptrs(int new_size, int old_size, int type);
extern void check_if_need_to_realloc_connection_read_buf (Msg_com_con *mccptr, int nvm_id, int old_size, int type);
extern void send_new_object_discovery_record_to_parent(VUser *vptr, int data_len, char *data, int type, int norm_id);
#endif
