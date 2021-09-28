#ifndef __NS_USER_MONITOR_H__
#define __NS_USER_MONITOR_H__

#include "ns_msg_def.h"

#define DATA_NOT_VALID  -1
typedef struct UM_info{
  char gdf_name[0xff];  /* stores gdf name */
  int group_idx; /* actual group idx in gdf */
  char *data_buf; /* ?? */
  int dindex; /* Used to store the index of starting of graph in um_data array*/
  unsigned long long *data; /* ?? */
  int rpt_group_id;     /* stores actual gdf group id */
  int number_of_graphs;
  char log_once; 
} UM_info;

typedef struct UM_data {
  char data_type;
  short rpt_id;
  int actual_idx;
  double count;
  double min;
  double max;
  double sum_of_squares;
  /*here we can make union because we can accumlate the cummulative data for cummulative graph.
    cum_value - used by parent to accumulate cummlative data
    In case of NC:
        Controller ---> accumulate generator data
        generator Parent  ---> accumulate from its NVM'S 
    In case of NS:
       NS ---> accumulate from its NVM'S            
   */

  union {
    double cum_value;
    double value;
  };
} UM_data;

#ifndef CAV_MAIN
extern int total_um_entries;
extern UM_info *um_info;  /* array */
extern int g_avg_um_data_idx; 
extern int g_cavg_um_data_idx;
#else
extern __thread int total_um_entries;
extern __thread UM_info *um_info;  /* array */
extern __thread int g_avg_um_data_idx; 
extern __thread int g_cavg_um_data_idx;
#endif
extern int total_um_data_entries;
extern UM_data *um_data;  /* array */

//extern avgtime *average_time;

// FUNCTIONS:
void user_monitor_config(char *keyword, char *buf);
void fill_um_graph_info(char *graph_name, int um_group_idx, int rpGraphID, char data_type);
int check_if_user_monitor(int rpGroupID);
void fill_um_group_info(char *gdf_name, char *groupName, int rpGroupID, int numGraph);
void process_user_monitor_gdf();
void insert_um_data_in_avg_time();
void print_um_data(FILE *fp1, FILE *fp2, UM_data *local_um_data, UM_data *local_cavg_um_data);
void fill_um_data(avgtime **g_avg, cavgtime **c_avg);
void init_um_data(UM_data *ud);
void fill_um_data_in_avg_and_cavg(UM_data *dest, UM_data *src, UM_data *cum_dest, UM_data *cum_dest_gen);
void calculate_um_data_in_cavg(UM_data *dest, UM_data *src);
extern void update_um_data_avgtime_size();
extern void update_um_data_cavgtime_size();

#endif /* __NS_USER_MONITOR_H__ */
