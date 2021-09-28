#ifndef NS_IP_DATA_H
#define NS_IP_DATA_H

#define IP_BASED_DATA_ENABLED 1
#define IP_BASED_DATA_DISABLED 0

typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
} IP_based_stat_gp;

typedef struct
{
  int cur_url_req;                            // URL requests sent/sec
} IPBasedAvgTime;

extern IPBasedAvgTime *ip_avgtime;
extern IP_based_stat_gp *ip_data_gp_ptr;
extern unsigned int ip_data_gp_idx;
extern unsigned int ip_data_idx;
extern char **printIpDataStat();
extern char **init_2d(int no_of_host);
extern void fill_2d(char **TwoD, int i, char *fill_data);
extern int kw_set_ip_based_data(char *buf, char *err_msg);
extern inline void update_ip_data_avgtime_size();
extern inline void set_ip_based_stat_avgtime_ptr();
extern void fill_ip_gp(avgtime **grp_avg);
extern int ip_avgtime_size;
extern int *g_ipdata_loc_ipdata_avg_idx;
#endif
