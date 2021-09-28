#ifndef NS_PROGRESS_QUEUE_H
#define NS_PROGRESS_QUEUE_H

#include "ns_msg_def.h"
#include "ns_data_handler_thread.h"
#include "nslb_mem_pool.h"

#define NORMAL_COMPLETE	0
#define FORCE_COMPLETE	1
#define QUEUE_COMPLETE	2

//Macros to check completeness
#define HANDLE_IF_COMPLETE(X,Y) 	check_progress_report_complete(X, NORMAL_COMPLETE, Y)

#define HANDLE_FORCE_COMPLETE(X,Y) 	check_progress_report_complete(X, FORCE_COMPLETE, Y);\

#define HANDLE_QUEUE_COMPLETE()  {\
  int num_rcd;\
  do{\
    num_rcd = get_sample_info(cur_sample);\
    check_progress_report_complete(cur_sample, QUEUE_COMPLETE, num_rcd);\
  }while(num_rcd >= g_data_control_var.num_pge);\
} 

#define COPY_PROGRESS_REPORT()    handle_rcvd_report(amsg, gen_idx, 0)

#define COPY_FINISH_REPORT()      {\
  handle_rcvd_report(amsg, gen_idx, 1);\
  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_pge--)\
}

typedef struct progress_msg {
  NSLB_MP_COMMON
  int sample_id;           // sample number
  void *sample_data;       // sample ptr to avgtime
  void *sample_cum_data;   // sample ptr to cavgtime
  u_ns_ts_t sample_time;   // time sample is filled
} ProgressMsg;

typedef struct sample_info {
  int sample_count;        	// count of sample_id received
  int sample_id;           	// sample number
  unsigned long child_mask[4];	// 256 child supported 64 * 4 = 256
} SampleInfo;

extern void **progress_data_pool;  // array of memory pool pointers for every child
extern void check_progress_report_complete(int rcvd_sample, int completion_mode, int num_rcd);
extern void kw_set_progress_report_queue(char *buf);
extern void handle_rcvd_report(avgtime *amsg, int gen_idx, int finish_report);
extern int get_sample_info(int recv_sample);
extern int decrease_sample_count(int sample, int child);

#endif
