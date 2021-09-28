#include"ns_log.h"
  
#define CHILD_RESET_NETSTORM_DIAGNOSTICS_AVGTIME(a)\
  if(global_settings->g_enable_ns_diag){\
    (a)->c_mem_allocated = 0;\
    (a)->c_mem_freed = 0;\
    (a)->c_mem_shared_allocated = 0;\
    (a)->c_mem_num_malloced = 0;\
    (a)->c_mem_num_freed = 0;\
    (a)->c_mem_num_shared_allocated = 0;\
    (a)->t_total_threads = 0;\
    (a)->t_busy_threads = 0;\
    (a)->t_dead_threads = 0;\
  }

//Acumulate only childs data only
#define ACC_NETSTORM_DIAGNOSTICS(total_avg, msg, save)\
  if(global_settings->g_enable_ns_diag){\
    NSDiagAvgTime *a = (NSDiagAvgTime*)((char*)total_avg + g_ns_diag_avgtime_idx); \
    NSDiagAvgTime *b = (NSDiagAvgTime*)((char*)msg + g_ns_diag_avgtime_idx); \
    NSDiagCAvgTime *c = (NSDiagCAvgTime*) ((char*)save + g_ns_diag_cavgtime_idx); \
    (a)->c_mem_allocated += (b)->c_mem_allocated;\
    (a)->c_mem_freed += (b)->c_mem_freed;\
    (a)->c_mem_shared_allocated += (b)->c_mem_shared_allocated;\
    (a)->c_mem_num_malloced += (b)->c_mem_num_malloced;\
    (a)->c_mem_num_freed += (b)->c_mem_num_freed;\
    (a)->c_mem_num_shared_allocated += (b)->c_mem_num_shared_allocated;\
    (a)->t_total_threads += (b)->t_total_threads; \
    (a)->t_busy_threads += (b)->t_busy_threads;\
    (a)->t_dead_threads += (b)->t_dead_threads;\
    (c)->c_mem_cum_allocated += (b)->c_mem_allocated;\
    (c)->c_mem_cum_freed += (b)->c_mem_freed;\
    (c)->c_mem_cum_num_malloced += (b)->c_mem_num_malloced;\
    (c)->c_mem_cum_num_freed += (b)->c_mem_num_freed;\
    (c)->c_mem_cum_num_shared_allocated += (b)->c_mem_num_shared_allocated;\
    (c)->c_mem_cum_shared_allocated += (b)->c_mem_shared_allocated;\
    NSDL2_LOGGING(NULL, NULL, "(b)->t_total_threads = %d, (a)->t_total_threads = %d", (b)->t_total_threads, (a)->t_total_threads); \
  }

//Acumulate only childs data only
#define ACC_NETCLOUD_DIAGNOSTICS(total_avg, msg, save)\
  if(global_settings->g_enable_ns_diag){\
    NCDiagAvgTime *a = (NCDiagAvgTime*)((char*)total_avg + g_ns_diag_avgtime_idx); \
    NCDiagAvgTime *b = (NCDiagAvgTime*)((char*)msg + g_ns_diag_avgtime_idx); \
    NCDiagCAvgTime *c = (NCDiagCAvgTime*) ((char*)save + g_ns_diag_cavgtime_idx); \
    (a)->c_mem_allocated += (b)->c_mem_allocated + (b)->p_mem_allocated;\
    (a)->c_mem_freed += (b)->c_mem_freed + (b)->p_mem_freed;\
    (a)->c_mem_shared_allocated += (b)->c_mem_shared_allocated + (b)->p_mem_cum_shared_allocated;\
    (a)->c_mem_num_malloced += (b)->c_mem_num_malloced + (b)->p_mem_num_malloced;\
    (a)->c_mem_num_freed += (b)->c_mem_num_freed + (b)->p_mem_num_freed;\
    (a)->c_mem_num_shared_allocated += (b)->c_mem_num_shared_allocated + (b)->p_mem_num_shared_allocated;\
    (a)->t_total_threads += (b)->t_total_threads; \
    (a)->t_busy_threads += (b)->t_busy_threads;\
    (a)->t_dead_threads += (b)->t_dead_threads;\
    (c)->c_mem_cum_allocated += (b)->c_mem_allocated + (b)->p_mem_allocated;\
    (c)->c_mem_cum_freed += (b)->c_mem_freed + (b)->p_mem_freed;\
    (c)->c_mem_cum_num_malloced += (b)->c_mem_num_malloced + (b)->p_mem_num_malloced;\
    (c)->c_mem_cum_num_freed += (b)->c_mem_num_freed + (b)->p_mem_num_freed;\
    (c)->c_mem_cum_num_shared_allocated += (b)->c_mem_num_shared_allocated + (b)->p_mem_num_shared_allocated ;\
    (c)->c_mem_cum_shared_allocated += (b)->c_mem_shared_allocated + (b)->p_mem_cum_shared_allocated;\
    NSDL2_LOGGING(NULL, NULL, "(b)->t_total_threads = %d, (a)->t_total_threads = %d, (a)->c_mem_allocated = %d, (a)->c_mem_freed = %d, (a)->c_mem_shared_allocated = %d", (b)->t_total_threads, (a)->t_total_threads, (a)->c_mem_allocated, (a)->c_mem_freed, (a)->c_mem_shared_allocated); \
  }


//This is only for child
#define UPDATE_NS_DIAG_DATA(a)\
  if(global_settings->g_enable_ns_diag) {\
    (a)->c_mem_shared_allocated = g_shared_mem_allocated;\
    (a)->c_mem_allocated = g_mem_allocated;\
    (a)->c_mem_freed = g_mem_freed;\
    (a)->c_mem_num_malloced = g_alloc_count;\
    (a)->c_mem_num_freed = g_free_count;\
    (a)->c_mem_num_shared_allocated = g_shared_mem_alloc_count;\
    (a)->t_total_threads = total_free_thread + total_busy_thread;\
    (a)->t_busy_threads = total_busy_thread;\
    (a)->t_dead_threads = num_ceased_thread;\
  }

#define ACC_NETSTORM_DIAGNOSTICS_CUMULATIVES_FROM_NEXT_SAMPLE(total_avg, msg)\
  if(global_settings->g_enable_ns_diag){\
  NSDiagCAvgTime *a  = ((NSDiagCAvgTime*)((char*)total_avg + g_ns_diag_cavgtime_idx));\
  NSDiagCAvgTime *b = ((NSDiagCAvgTime*)((char*)msg + g_ns_diag_cavgtime_idx));\
    (a)->c_mem_cum_allocated += (b)->c_mem_cum_allocated;\
    (a)->c_mem_cum_freed += (b)->c_mem_cum_freed;\
    (a)->c_mem_cum_num_malloced += (b)->c_mem_cum_num_malloced;\
    (a)->c_mem_cum_num_freed += (b)->c_mem_cum_num_freed;\
    (a)->c_mem_cum_num_shared_allocated += (b)->c_mem_cum_num_shared_allocated;\
    (a)->c_mem_cum_shared_allocated += (b)->c_mem_cum_shared_allocated;\
  }

#define ACC_NETCLOUD_DIAGNOSTICS_CUMULATIVES_FROM_NEXT_SAMPLE(total_avg, msg)\
  if(global_settings->g_enable_ns_diag){\
  NCDiagCAvgTime *a = ((NCDiagCAvgTime*)((char*)total_avg + g_ns_diag_cavgtime_idx));\
  NCDiagCAvgTime *b = ((NCDiagCAvgTime*)((char*)msg + g_ns_diag_cavgtime_idx));\
    (a)->c_mem_cum_allocated += (b)->c_mem_cum_allocated;\
    (a)->c_mem_cum_freed += (b)->c_mem_cum_freed;\
    (a)->c_mem_cum_num_malloced += (b)->c_mem_cum_num_malloced;\
    (a)->c_mem_cum_num_freed += (b)->c_mem_cum_num_freed;\
    (a)->c_mem_cum_num_shared_allocated += (b)->c_mem_cum_num_shared_allocated;\
    (a)->c_mem_cum_shared_allocated += (b)->c_mem_cum_shared_allocated;\
    (a)->p_mem_cum_allocated += (b)->p_mem_cum_allocated;\
    (a)->p_mem_cum_freed += (b)->p_mem_cum_freed;\
    (a)->p_mem_cum_num_malloced += (b)->p_mem_cum_num_malloced;\
    (a)->p_mem_cum_num_freed += (b)->p_mem_cum_num_freed;\
    (a)->p_mem_cum_num_shared_allocated += (b)->p_mem_cum_num_shared_allocated;\
    (a)->p_mem_cum_shared_allocated += (b)->p_mem_cum_shared_allocated;\
    NSDL2_LOGGING(NULL, NULL, "(a)->c_mem_cum_allocated = %d, (b)->c_mem_cum_allocated = %d", (a)->c_mem_cum_allocated, (b)->c_mem_cum_allocated); \
  }

#define ACC_NETSTORM_PARENT_DIAGNOSTICS(total_avg, msg, save)\
  if(global_settings->g_enable_ns_diag){\
    NCDiagAvgTime *a = (NCDiagAvgTime*)((char*)total_avg + g_ns_diag_avgtime_idx); \
    NCDiagAvgTime *b = (NCDiagAvgTime*)((char*)msg + g_ns_diag_avgtime_idx); \
    NCDiagCAvgTime *c = (NCDiagCAvgTime*) ((char*)save + g_ns_diag_cavgtime_idx); \
    (a)->c_mem_allocated += (b)->p_mem_allocated;\
    (a)->c_mem_freed += (b)->p_mem_freed;\
    (a)->c_mem_shared_allocated += (b)->p_mem_cum_shared_allocated;\
    (a)->c_mem_num_malloced += (b)->p_mem_num_malloced;\
    (a)->c_mem_num_freed += (b)->p_mem_num_freed;\
    (a)->c_mem_num_shared_allocated += (b)->p_mem_num_shared_allocated;\
    (c)->c_mem_cum_allocated += (b)->p_mem_allocated;\
    (c)->c_mem_cum_freed += (b)->p_mem_freed;\
    (c)->c_mem_cum_num_malloced += (b)->p_mem_num_malloced;\
    (c)->c_mem_cum_num_freed += (b)->p_mem_num_freed;\
    (c)->c_mem_cum_num_shared_allocated += (b)->p_mem_num_shared_allocated;\
    (c)->c_mem_cum_shared_allocated += (b)->p_mem_cum_shared_allocated;\
    NSDL2_LOGGING(NULL, NULL, "(a)->c_mem_allocated = %d, (b)->p_mem_allocated = %d, (c)->c_mem_cum_allocated = %d", (a)->c_mem_allocated, (b)->p_mem_allocated, (c)->c_mem_cum_allocated); \
  }

#define ACC_NETSTORM_DIAGNOSTICS_CUMULATIVES(cavg, msg)\
  if(global_settings->g_enable_ns_diag){\
    NCDiagCAvgTime *a  = ((NCDiagCAvgTime*)((char*)cavg + g_ns_diag_cavgtime_idx));\
    NCDiagAvgTime *b = (NCDiagAvgTime*)((char*)msg + g_ns_diag_avgtime_idx); \
    (a)->c_mem_cum_allocated += (b)->c_mem_allocated;\
    (a)->c_mem_cum_freed += (b)->c_mem_freed;\
    (a)->c_mem_cum_num_malloced += (b)->c_mem_num_malloced;\
    (a)->c_mem_cum_num_freed += (b)->c_mem_num_freed;\
    (a)->c_mem_cum_num_shared_allocated += (b)->c_mem_num_shared_allocated;\
    (a)->c_mem_cum_shared_allocated += (b)->c_mem_shared_allocated;\
    (a)->p_mem_cum_allocated += (b)->p_mem_allocated;\
    (a)->p_mem_cum_freed += (b)->p_mem_freed;\
    (a)->p_mem_cum_num_malloced += (b)->p_mem_num_malloced;\
    (a)->p_mem_cum_num_freed += (b)->p_mem_num_freed;\
    (a)->p_mem_cum_num_shared_allocated += (b)->p_mem_num_shared_allocated;\
    (a)->p_mem_cum_shared_allocated += (b)->p_mem_cum_shared_allocated;\
    NSDL2_LOGGING(NULL, NULL, "(a)->c_mem_cum_allocated = %d, (b)->c_mem_allocated = %d, (a)->p_mem_cum_num_malloced = %d", (a)->c_mem_cum_allocated, (b)->c_mem_allocated, (a)->p_mem_cum_num_malloced); \
  }


#define FILL_PARENT_NESTORM_DIAGNOSTICS_STATS(avg) \
{ \
  if(global_settings->g_enable_ns_diag && loader_opcode != STAND_ALONE) {\
    NCDiagAvgTime *a = (NCDiagAvgTime*)((char*)avg + g_ns_diag_avgtime_idx); \
    (a)->p_mem_cum_shared_allocated = g_shared_mem_allocated;\
    (a)->p_mem_allocated = g_mem_allocated;\
    (a)->p_mem_freed = g_mem_freed;\
    (a)->p_mem_num_malloced = g_alloc_count;\
    (a)->p_mem_num_freed = g_free_count;\
    (a)->p_mem_num_shared_allocated = g_shared_mem_alloc_count;\
    NSDL2_LOGGING(NULL, NULL, "(a)->p_mem_allocated = %d, g_mem_allocated = %d", (a)->p_mem_allocated, g_mem_allocated); \
  }\
}

// Structure used by Netstorm NVM to fill data and send to parent in progress report message
typedef struct {
  //Child periodics
  u_ns_4B_t c_mem_allocated; //Total memory alloacted
  u_ns_4B_t c_mem_freed; //Total memory freed
  u_ns_4B_t c_mem_shared_allocated; //Total shared memory alloacted
  u_ns_4B_t c_mem_num_malloced; //Total cum malloc called
  u_ns_4B_t c_mem_num_freed;  //Total cum free called
  u_ns_4B_t c_mem_num_shared_allocated; //Total cum shared memory allocated
 
  //Thread related data
  u_ns_4B_t t_total_threads; // Cumulative number of total allocated threads
  u_ns_4B_t t_busy_threads; // Current number of busy threads
  u_ns_4B_t t_dead_threads; // Current number of dead threads
  
} NSDiagAvgTime;


typedef struct {
 // child cummulative
  u_ns_8B_t c_mem_cum_allocated; //Total cum memory alloacted
  u_ns_8B_t c_mem_cum_freed; //Total cum memory freed
  u_ns_8B_t c_mem_cum_num_malloced; //Total cum malloc called
  u_ns_8B_t c_mem_cum_num_freed; //Total cum free called
  u_ns_8B_t c_mem_cum_num_shared_allocated; //Total cum shared memory allocated
  u_ns_8B_t c_mem_cum_shared_allocated; ////Total shared memory allocated

} NSDiagCAvgTime;

// Structure used by NetCloud NVM/Generators to fill data and send to parent in progress report message
typedef struct {
  //Child periodics
  u_ns_4B_t c_mem_allocated; //Total memory alloacted
  u_ns_4B_t c_mem_freed; //Total memory freed
  u_ns_4B_t c_mem_shared_allocated; //Total shared memory alloacted
  u_ns_4B_t c_mem_num_malloced; //Total cum malloc called
  u_ns_4B_t c_mem_num_freed;  //Total cum free called
  u_ns_4B_t c_mem_num_shared_allocated; //Total cum shared memory allocated
 
  //Parent periodics
  u_ns_4B_t p_mem_allocated; //Total memory alloacted
  u_ns_4B_t p_mem_freed; //Total memory freed
  u_ns_4B_t p_mem_cum_shared_allocated; //Total shared memory alloacted
  u_ns_4B_t p_mem_num_malloced; //Total cum malloc called
  u_ns_4B_t p_mem_num_freed;  //Total cum free called
  u_ns_4B_t p_mem_num_shared_allocated; //Total cum shared memory allocated
  
  //Thread related data
  u_ns_4B_t t_total_threads; // Cumulative number of total allocated threads
  u_ns_4B_t t_busy_threads; // Current number of busy threads
  u_ns_4B_t t_dead_threads; // Current number of dead threads
  
} NCDiagAvgTime;

typedef struct {
 // child cummulative
  u_ns_8B_t c_mem_cum_allocated; //Total cum memory alloacted
  u_ns_8B_t c_mem_cum_freed; //Total cum memory freed
  u_ns_8B_t c_mem_cum_num_malloced; //Total cum malloc called
  u_ns_8B_t c_mem_cum_num_freed; //Total cum free called
  u_ns_8B_t c_mem_cum_num_shared_allocated; //Total cum shared memory allocated
  u_ns_8B_t c_mem_cum_shared_allocated; ////Total shared memory allocated

  // parent cummulative in case of generators
  u_ns_8B_t p_mem_cum_allocated; //Total cum memory alloacted
  u_ns_8B_t p_mem_cum_freed; //Total cum memory freed
  u_ns_8B_t p_mem_cum_num_malloced; //Total cum malloc called
  u_ns_8B_t p_mem_cum_num_freed; //Total cum free called
  u_ns_8B_t p_mem_cum_num_shared_allocated; //Total cum shared memory allocated
  u_ns_8B_t p_mem_cum_shared_allocated; ////Total shared memory allocated

} NCDiagCAvgTime;

//Struct to fill GDF data
typedef struct {
  //Parent
  Long_data p_mem_cum_allocated; // Total cum memory alloacted in MB since start of the test
  Long_data p_mem_allocated;     // Memory alloacted in MB in the sampling period
  Long_data p_mem_cum_freed; //Total cum memory freed in MB 
  Long_data p_mem_freed; //Total memory freed in the sampling period
  Long_data p_mem_cum_shared_allocated; //Total cum shared memory allocated in MB

  Long_data p_mem_num_malloced; //Total numbers malloc called in MB
  Long_data p_mem_num_freed; //Total numbers free called
  Long_data p_mem_num_shared_allocated; //Total numbers shared memory allocated
  
  //Child
  Long_data c_mem_cum_allocated; //Total cum memory alloacted
  Long_data c_mem_allocated; //Total memory alloacted
  Long_data c_mem_cum_freed; //Total cum memory freed
  Long_data c_mem_freed; //Total memory freed
  Long_data c_mem_shared_allocated; //Total shared memory allocated

  Long_data c_mem_num_malloced; //Total numbers malloc called
  Long_data c_mem_num_freed; //Total numbers free called
  Long_data c_mem_num_shared_allocated; //Total numbers shared memory allocated

  //Thread related data
  Long_data t_total_threads;
  Long_data t_total_busy_threads;
  Long_data t_total_dead_threads;
  Long_data t_stack_mem_used; // Total stack memory used by all threads in MB

  //Epoll related data
  //Long_data t_epoll_in;
  //Long_data t_epoll_out;
  //Long_data t_epoll_err;
  //Long_data t_epoll_hup;
}NSDiag_gp;


extern NSDiag_gp *ns_diag_gp_ptr;
extern unsigned int ns_diag_gp_idx;

#ifndef CAV_MAIN
extern int g_ns_diag_avgtime_idx;
extern NSDiagAvgTime *ns_diag_avgtime;
#else
extern __thread int g_ns_diag_avgtime_idx;
extern __thread NSDiagAvgTime *ns_diag_avgtime;
#endif
extern int g_ns_diag_cavgtime_idx;
extern NSDiagCAvgTime *ns_diag_cavgtime;
extern NCDiagAvgTime *nc_diag_avgtime;
extern NCDiagCAvgTime *nc_diag_cavgtime;

extern inline void set_ns_diag_avgtime_ptr();
extern inline void update_ns_diag_avgtime_size();
extern inline void fill_ns_diag_cum_gp (cavgtime **cavg);
extern inline void fill_ns_diag_gp (avgtime **avg);
extern void kw_set_netstorm_diagnostics (char *buf);
extern inline void update_ns_diag_cavgtime_size();

extern unsigned long* g_epollin_count;
extern unsigned long* g_epollout_count;
extern unsigned long* g_epollerr_count;
extern unsigned long* g_epollhup_count;
