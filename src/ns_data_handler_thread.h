#ifndef _NS_DATA_HANDLER_THREAD_H_
#define _NS_DATA_HANDLER_THREAD_H_

//Structure
typedef struct {
  int num_active;   /* number of Finish expected */
  int num_connected;
  int num_pge;         /* numprogress expected */ 
  int total_killed_gen;
  int total_killed_generator;
  int total_killed_nvms;
}Data_Control_con;

//variables
extern pthread_t data_handler_thid;
extern pthread_attr_t data_handler_attr;
extern unsigned int cur_sample;
extern unsigned int gen_delayed_samples;
extern avgtime **g_end_avg;
extern char g_test_end;
extern parent_msg *msg_dh;
//functions
extern void ns_data_handler_thread_create();
extern Data_Control_con g_data_control_var;
extern pthread_key_t glob_var_key;
extern u_ns_ts_t g_next_partition_switch_time_stamp; 

#define INIT_PERIODIC_EPOLL_STATS 			\
{ 							\
  int i; 						\
  for(i=0; i< global_settings->num_process + 1; i++) 	\
  { 							\
    g_epollerr_count[i] = 0; 				\
    g_epollin_count[i] = 0;				\
    g_epollout_count[i] = 0;				\
    g_epollhup_count[i] = 0;				\
  }							\
}

#define INC_EPOLL_EVENT_STATS(index, event)		\
{							\
  if(index >= 0)					\
  {							\
    event[0]++;						\
    if(loader_opcode == MASTER_LOADER)			\
    { /* 0 is for Overall i.e. sum of all NVMs/Generators */ 		\
      event[index+1]++;					\
      NSDL3_MESSAGES(NULL, NULL, "event[%d]=%lu", index, event[index+1]);\
    }							\
  } 							\
}

extern int g_progress_delay_read;
extern unsigned long total_rx_pr;
extern unsigned long total_tx_pr;
extern Msg_com_con *g_dh_msg_com_con_nvm0;
#endif
