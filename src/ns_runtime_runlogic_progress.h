#ifndef NS_RUNTIME_RUNLOGIC_PROGRESS_H
#define NS_RUNTIME_RUNLOGIC_PROGRESS_H

#include "netstorm.h" 
 
#define SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED 1

#define SHOW_RUNTIME_RUNLOGIC_PROGRESS ((global_settings->show_vuser_flow_mode == SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED)?SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED:0)

#define SHOW_RUNTIME_RUNLOGIC_PROGRESS_GRP(grp_num) ((runprof_table_shr_mem[grp_num].gset.show_vuser_flow_mode == SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED)?SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED:0)

#define RUNTIME_RUNLOGIC_PROGRESS_AVGTIME_SIZE (sizeof(VUserFlowAvgTime) * total_flow_path_entries)

typedef struct
{       
  Long_data vuser_running_flow;                      //Vuser running flow per sec
} VUserFlowBasedGP;
          
typedef struct
{         
  int cur_vuser_running_flow;                        //Vuser running flow per sec
} VUserFlowAvgTime;

typedef struct
{
  char flow_path[1024];
  int id;
  int num_of_flow_path;
}VUserFlowData;

extern int total_flow_path_entries;
extern VUserFlowData *vuser_flow_data;
extern VUserFlowBasedGP *vuser_flow_gp_ptr;
extern unsigned int show_vuser_flow_gp_idx;
#ifndef CAV_MAIN
extern unsigned int show_vuser_flow_idx;
extern VUserFlowAvgTime *vuser_flow_avgtime;
#else
extern __thread unsigned int show_vuser_flow_idx;
extern __thread VUserFlowAvgTime *vuser_flow_avgtime;
#endif
extern int kw_set_g_show_vuser_flow(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern char **printVuserFlowDataStat();
extern inline void update_show_vuser_flow_avgtime_size();
extern inline void set_vuser_flow_stat_avgtime_ptr();
extern inline void fill_vuser_flow_gp(avgtime **g_avg);
extern void get_path_pointer(int grp_num, int id);
extern void update_user_flow_count_ex(VUser* vptr, int id);
extern void fill_vuser_flow_data_struct(int sess_id, char *script_tree_path);
#endif
