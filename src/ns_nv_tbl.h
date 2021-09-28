#ifndef _ns_nv_tbl_h
#define _ns_nv_tbl_h

#include "ns_custom_monitor.h"

typedef struct nv_id_map_table_t
{
  int *idxMap;
  int mapSize;
  int maxAssValue;
} nv_id_map_table_t;


typedef struct NVMapDataTbl
{
  double **data;
  void *metrics_idx;
  void *metrics_idx_tmp;
  char metrics_idx_found;
  char metrics_idx_tmp_found;
  double **temp_data; //this is used to temporarily store the data
  int cur_entries;  // MAN : rename this to 'cur_entries' 
  int temp_entries;
} NVMapDataTbl;

//extern NVMapDataTbl *nv_map_data_tbl;
//extern nv_id_map_table_t *nv_init_map_table();
extern int nv_get_id_value(nv_id_map_table_t *map_ptr, int id, int *is_new_vector);
extern int parse_and_fill_nv_data(char *buffer, CM_info *cus_mon_ptr);
extern int parse_nv_vector(char *vector_name, char *vector, int *vectorid);
extern int check_realloc_filldata_nv_map_data_tbl(CM_info *cus_mon_ptr, int id, char *data ,char *vector);
#endif
