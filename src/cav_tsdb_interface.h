#ifndef CAV_TSDB_INTERFACE_H
#define CAV_TSDB_INTERFACE_H
/**
 * Interface to interact with TSDB
 */


#include <stdbool.h>
#include <stdint.h>

#define TSDB_BUFF_SIZE 256
#define MAX_SUBJECT_HIERARCHY_LEVELS 256

/**
 * This structure represents a metric group
 */
typedef struct tsdb_metric_group
{
  char group_name[TSDB_BUFF_SIZE];
  char group_desc[TSDB_BUFF_SIZE];
  char group_metric[TSDB_BUFF_SIZE];
  char heirarchy[TSDB_BUFF_SIZE];
  unsigned short num_graphs;
  char group_type;   // 0 - Scalar Group Type 1 - Vector Group Type
  int rpt_grp_id;
} tsdb_metric_group;

/**
 * This structure represents a single metric
 */
#if 0
typedef struct tsdb_metric_info
{
  char graph_name[TSDB_BUFF_SIZE];
  char graph_desc[TSDB_BUFF_SIZE];
  char graph_type;
  char data_type;
  char num_fileds_per_graph;
  char formula;
  char graph_state;
  char metric_priority;
} tsdb_metric_info;
#endif

enum formula {
 TSDB_FORMULA_SECS=1,
 TSDB_FORMULA_PM=2,
 TSDB_FORMULA_PS=3,
 TSDB_FORMULA_KBPS=4,
 TSDB_FORMULA_DBH=5
};

typedef struct tsdb_metric_info
{
  char graph_type;
  char data_type;
  char formula;
  char graph_state;
  char metric_priority;
  unsigned char next_hml;
  unsigned short rpt_id;
  unsigned short num_vectors;
  unsigned int graph_msg_index;
  unsigned int pdf_data_index;
  unsigned int cm_index;
  int pdf_info_idx;
  int pdf_id;
  char derived_formula[TSDB_BUFF_SIZE];

  char graph_name[TSDB_BUFF_SIZE];
  char gline[TSDB_BUFF_SIZE];

  char graph_desc[TSDB_BUFF_SIZE];
  char ***graph_data;
  char **vector_names;
  char num_fileds_per_graph;
  int size;
} tsdb_metric_info;


typedef struct
{
  char *tag;
  char *val;
}tag_val_ptr_t;

/**
 * This function will register a subject to tsdb
 * 
 * @param subject: A subject string
 * @return A subject ID
 */
extern int32_t tsdb_register_subject(char *subject);

/**
 * This function will register multiple metrices to tsdb.
 * 
 * @param subject_id: the ID of the subject to which metric_group belongs to
 * @param metric_group: metric group of the metric
 * @param metric_info: array of metric to be added
 * @param out_metric_id: IDs of the metric will be store here
 * @param metric_count: Count of metrics to be registered
 * @return true if all metrices are added, false if any failed
 */
extern bool tsdb_register_bulk_metric(int32_t subject_id, tsdb_metric_group *metric_group , tsdb_metric_info metric_info[], int metric_count, long out_metric_id[], int frequency);

/**
 * This function will register and add a single metric to tsdb by using the vector name 
 * 
 * @param subject: a subject string (vector heirarchy) 
 * @param metric_group: metric group of the metric
 * @param metric_info : metric to be added
 * @param frequency: frequency of the metric
 * @param metric_data: actual metric datas to be inserted
 * @return metric ID if success, -1 if failed
 */
extern long tsdb_insert_metric_by_name(char *subject, tsdb_metric_group *metric_group, tsdb_metric_info *metric_info, int frequency, double metric_data[], long timestamp);

extern bool tsdb_insert_bulk_metric_by_subid(int leaf_subid, tsdb_metric_group *metric_group , tsdb_metric_info metric_info[], int frequency, double metric_data[], int metric_count, long out_metric_id[], long timestamp);

extern bool tsdb_insert_bulk_metric_by_gdf_name(char *subject, tsdb_metric_group *metric_group , tsdb_metric_info metric_info[], int frequency, double metric_data[], int metric_count, long out_metric_id[], long timestamp);

/**
 * This function will register and add multiple metrices to tsdb by using the vector name
 *
 * @param subject: a subject string (vector heirarchy) 
 * @param metric_group: metric group of the metric
 * @param metric_info : array of metrices to be added
 * @param frequency: Frequency of the metric
 * @param metric_data: array of the actual metric datas to be inserted
 * @param metric_count: total numbers of metrices in metric_info
 * @param out_metric_id: IDs of the metrices will be store here
 * @return true if success, false if failed
 */
extern bool tsdb_insert_bulk_metric_by_name(char *subject, tsdb_metric_group *metric_group , tsdb_metric_info metric_info[], int frequency, double metric_data[], int metric_count, long out_metric_id[], long timestamp);

/**
 * This function will add a single metric to tsdb by using its metric_id
 *
 * @param metric_id:  A metric ID previosly returned from tsdb
 * @param frequency:   Frequency of the metric
 * @param metric_data: Actual metric data to be inserted
 * @return true if success, false if failed
 */
extern int  tsdb_insert_metric_by_id(void* metric_id, int frequency, double metric_data[], long timestamp);
//extern int tsdb_insert_metric_by_id(Partition_info_t *pcptr, metric_id_t metric_id, int frequency, double metric_data[], long timestamp);
/**
extern int  tsdb_insert_metric_by_id(void* metric_id, int frequency, double metric_data[], long timestamp);
 * This function will add multiple metrices to tsdb by using its metric_id
 *
 * @param metric_id:  Array of metric ID previosly returned from tsdb
 * @param frequency:   Frequency of the metric
 * @param metric_data: Array of actual metric data to be inserted
 * @param metric_count: total numbers of metrices in metric_info
 * @return true if success, false if failed
 */
extern int tsdb_insert_bulk_metric_by_id(char *, void *metric_id_buf, int frequency, double metric_data[], int metric_count, long timestamp);

extern long tsdb_register_gdf(tsdb_metric_group *metric_group, int metric_count, tsdb_metric_info metric_info[], char *err_msg);

extern int tsdb_insert_bulk_metric_by_mg_gid(char *subject, int mg_gid, int frequency, double metric_data[], int metric_count, void *out_metric_id_buffer, long timestamp, char *err_msg);

/**
 * This will delete a single metric from tsdb
 *
 * @param metric_id: metric ID to be deleted
 * @param err_msg:   error message
 * @return true if success, false if failed
 */
extern bool tsdb_delete_metric_by_id(void *metric_id, char *err_msg);

/**
 * This will delete multiple metrices from tsdb
 *
 * @param metric_id: An array of metric IDs to be deleted
 * @param err_msg: Error message
 * @param metric_count: Count of metrices to be deleted 
 * @return true if success, false if failed
 */
extern bool tsdb_delete_bulk_metric_by_id(long metric_id[], int metric_count, char *err_msg);

extern int tsdb_init(void);

extern int tsdb_val_data(char *monitor, char *subject, int num_metrics, int freq, unsigned int timestamp, long metric_id[], double expected_data[], char *err_msg);

/************ Time Series Data Type *************/
#define DATA_TYPE_MAX_FIELDS 16 // Example TIMES types have 4 fields, STD type has 5

typedef struct{
  uint32_t multiplier;
  uint8_t size_bytes;
  uint8_t numeric_type; //0 - integer, 1 float
} data_field_t;

 typedef struct{
  uint16_t numfields;
  data_field_t fields[DATA_TYPE_MAX_FIELDS];
} timeseries_data_t;

extern int tsdb_register_timeseries_data_type(timeseries_data_t ts_data);

typedef struct {
  char *tag;
  char *value;
} node_t;

//Extended version - pass tag value array
extern int tsdb_insert_bulk_metric_by_mg_gid_ex(tag_val_ptr_t  tv_arr[], int num_levels, int mg_gid, int frequency, double metric_data[], int metric_count, void *out_metric_id_buffer, long timestamp, char *err_msg);

extern int tsdb_convert_gdf_vector_name_to_subject_node(char *subject, char *tag_heirarchy, node_t **nodes_path);
extern void *tsdb_malloc_metric_id_buffer(int num_metrics);
extern void tsdb_free_metric_id_buffer(void *buffer);
#endif
