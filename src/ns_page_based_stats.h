#ifndef PAGE_BASED_STAT_DATA_H
#define PAGE_BASED_STAT_DATA_H

#define PAGE_BASED_STAT_ENABLED  1
#define PAGE_BASED_STAT_DISABLED 0

// total_page_entries is total number of pages of all scripts
// ISSUE - If same script is used in more than on Grp, total page entries is not counted twice. So we need to use g_actual_num_pages
#define PAGE_STAT_AVGTIME_SIZE (sizeof(PageStatAvgTime) * g_actual_num_pages)

//Set periodic elements of struct a with b into a
#define SET_MIN_MAX_PAGE_BASED_STAT_PERIODICS(a, b)\
  for (i = 0; i < g_actual_num_pages ; i++) {\
    SET_MIN (a[i].page_think_min_time, b[i].page_think_min_time);\
    SET_MAX (a[i].page_think_max_time, b[i].page_think_max_time);\
    SET_MIN (a[i].inline_delay_min_time, b[i].inline_delay_min_time);\
    SET_MAX (a[i].inline_delay_max_time, b[i].inline_delay_max_time);\
    SET_MIN (a[i].block_min_time, b[i].block_min_time);\
    SET_MAX (a[i].block_max_time, b[i].block_max_time);\
    SET_MIN (a[i].conn_reuse_delay_min_time, b[i].conn_reuse_delay_min_time);\
    SET_MAX (a[i].conn_reuse_delay_max_time, b[i].conn_reuse_delay_max_time);\
  }

#define ACC_PAGE_BASED_STAT_PERIODICS(a, b)\
  for (i = 0; i < g_actual_num_pages ; i++) {\
    a[i].page_think_time += b[i].page_think_time;\
    a[i].page_think_counts += b[i].page_think_counts;\
    a[i].inline_delay_time += b[i].inline_delay_time;\
    a[i].inline_delay_counts += b[i].inline_delay_counts;\
    a[i].block_time += b[i].block_time;\
    a[i].block_counts += b[i].block_counts;\
    a[i].conn_reuse_delay_time += b[i].conn_reuse_delay_time;\
    a[i].conn_reuse_delay_counts += b[i].conn_reuse_delay_counts;\
  }
    
#define CHILD_RESET_PAGE_BASED_STAT_AVGTIME(a) \
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) { \
    for (i = 0; i < g_actual_num_pages; i++) {\
      a[i].page_think_min_time = MAX_VALUE_4B_U;\
      a[i].page_think_max_time = 0;\
      a[i].page_think_counts = 0;\
      a[i].page_think_time = 0;\
      a[i].inline_delay_min_time = MAX_VALUE_4B_U;\
      a[i].inline_delay_max_time = 0;\
      a[i].inline_delay_counts = 0;\
      a[i].inline_delay_time = 0;\
      a[i].block_min_time = MAX_VALUE_4B_U;\
      a[i].block_max_time = 0;\
      a[i].block_counts = 0;\
      a[i].block_time = 0;\
      a[i].conn_reuse_delay_min_time = MAX_VALUE_4B_U;\
      a[i].conn_reuse_delay_max_time = 0;\
      a[i].conn_reuse_delay_counts = 0;\
      a[i].conn_reuse_delay_time = 0;\
    }\
  }

// Group Data Structure
typedef struct Page_based_stat_gp
{
  Times_data page_think_time_gp;  
  Times_data inline_delay_time_gp; 
  Times_data block_time_gp;  
  Times_data conn_reuse_delay_time_gp;  
} Page_based_stat_gp;

typedef struct PageStatAvgTime
{
  //Page think time related counters
  int page_think_counts;
  int page_think_time; // Sun for all think times (ms)
  unsigned int page_think_min_time;
  unsigned int page_think_max_time;

  //Inline delay time related counters
  int inline_delay_counts;
  int inline_delay_time; // Sun for all inline delay times (ms)
  unsigned int inline_delay_min_time;
  unsigned int inline_delay_max_time;
  
  //Block time related counters
  int block_counts;
  int block_time; // Sun for all block times (ms)
  unsigned int block_min_time;
  unsigned int block_max_time;

  //Connection Reuse Delay related attributes
  int conn_reuse_delay_counts;
  int conn_reuse_delay_time;
  unsigned int conn_reuse_delay_min_time;
  unsigned int conn_reuse_delay_max_time;
} PageStatAvgTime;

extern Page_based_stat_gp *page_based_stat_gp_ptr;
#ifndef CAV_MAIN
extern unsigned int page_based_stat_gp_idx;
extern PageStatAvgTime *page_stat_avgtime;
#else
extern __thread unsigned int page_based_stat_gp_idx;
extern __thread PageStatAvgTime *page_stat_avgtime;
#endif
extern unsigned int page_based_stat_idx;
extern char **printPageBasedStat();
extern char **init_2d(int no_of_host);
extern void fill_2d(char **TwoD, int i, char *fill_data);
extern int kw_set_page_based_stat(char *buf, char *err_msg, int runtime_flag);
extern inline void update_page_based_stat_avgtime_size();
extern inline void set_page_based_stat_avgtime_ptr();
 
extern inline void init_page_based_stat();
extern void initialise_page_based_stat_min();

extern void set_page_based_counter_for_page_think_time(void *vptr, int pg_think_time);
extern void set_page_based_counter_for_inline_delay_time(void *vptr, int inline_req_delay);
extern void set_page_based_counter_for_block_time(void *vptr, int inline_exec_start_ts);
extern void set_page_based_counter_for_conn_reuse_delay(void *vptr, int conn_reuse_delay);

extern void fill_page_based_stat_gp(avgtime **page_stat_avgtime);

#endif
