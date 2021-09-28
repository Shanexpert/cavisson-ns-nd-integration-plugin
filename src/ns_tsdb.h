#include "cav_tsdb_interface.h"

#define METRIC_PRIORITY_HIGH                   0
#define METRIC_PRIORITY_MEDIUM                 1
#define METRIC_PRIORITY_LOW                    2
#define STORE_SUBJECT (enable_store_config)?"/Store!":""

#define TSDB_LOG_TIMEOUT        900000
#define TSDB_ERROR_BUF_8192     8192
#define TSDB_INGESTION_RATE     900
enum tsdb_api
{
  TSDB_REGISTER_GROUP=0,
  TSDB_REGISTER_SUBJECT,
  TSDB_ADD_BULK_METRICS,
  TSDB_DELETE_SUBJECT,
  TOTAL_API_CALL
};

typedef struct tsdb_api_timing {
   char *api_name;
   long avg;
   long min;
   long max;   
   long count;
   int error_count;
}tsdb_api_timing;
tsdb_api_timing api_timing[TOTAL_API_CALL];

extern char g_send_backlog_data_to_TSDB;
extern inline void ns_tsdb_insert(CM_info *cm_info_mon_conn_ptr, double *data, void *metrics_idx, char *metrics_idx_found, long ts, char *vector_name);
extern inline void ns_tsdb_delete_metrics_by_id(CM_info *cus_mon_ptr, void *metric_id);
extern inline void ns_tsdb_log_api_timing(struct timespec *start, struct timespec *stop, int api_type, int error_count);
extern inline void ns_tsdb_send_pending_data();
extern inline void ns_tsdb_dumb_logs();
extern inline void ns_tsdb_init();
extern  inline char get_subject_name(CM_info *cus_mon_ptr, char *vector_name,  tag_val_ptr_t tv_arr_buf[], int num_levels);
extern inline void ns_tsdb_malloc_metric_id_buffer(void **ptr , int num);
extern inline void ns_tsdb_free_metric_id_buffer(void **ptr);
