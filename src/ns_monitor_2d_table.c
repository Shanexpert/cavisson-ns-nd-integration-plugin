#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ns_custom_monitor.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_alloc.h"
#include "ns_log.h"

#define VECTOR_NAME_MAX_LEN 512

#define NODE_DELTA_SIZE  //maloc delta size for TIER & INSTANCE
#define NODE_VECTOR_SIZE //malloc delta size for VECTOR and NORMALIZED VECTOR

NormObjKey nd_bci_dyn_mon_tier_agg_backend_norm_key;

#ifdef TEST2
typedef struct global_settings_t
{
  char hierarchical_view_vector_separator;
} global_settings_t;

global_settings_t *global_settings, gset;
#endif

int instance_vector_table_size = 64;
int tier_normalized_vector_table_size = 64;



/*************************************************************
ns_normalize_monitor_vector()
returns -1 on failure
0 if method exists in norm table
1 if method is new
***************************************************************/

char ns_normalize_monitor_vector (CM_info *cm_ptr,  char *method_name, int *norm_idx)
{
  if(!norm_idx)
    return -1;
  int flag_new = 0;

  if(!cm_ptr->is_norm_init_done)
  {
    if(0 > nslb_init_norm_id_table(&(cm_ptr->key), ND_BCI_DYN_MON_TIER_AGG_BACKEND_NORM_TABLE_SIZE))
      return -1;
    cm_ptr->is_norm_init_done = 1;
  }

  *norm_idx  = nslb_get_or_gen_norm_id(&(cm_ptr->key), method_name, strlen(method_name), &flag_new);

  return flag_new;
}


int ns_allocate_2d_matrix(void ***ptr, short delta_row_size, short delta_col_size, int *cur_row_size, int *cur_col_size)
{
  int i;
  void **matrix = *ptr;

  if(!delta_row_size)
    delta_row_size = 64;

  if(!delta_col_size)
    delta_col_size = 64;

  MY_REALLOC_AND_MEMSET(matrix, ((delta_col_size + *cur_col_size) * sizeof(void *)), 
                      ((*cur_col_size) * sizeof(void *)), "2d matrix", -1);
 
  for(i = *cur_col_size; i < *cur_col_size + delta_col_size; i++) 
  {
    if(!(*cur_row_size))
      *cur_row_size = delta_row_size;

    MY_MALLOC_AND_MEMSET(matrix[i], ((*cur_row_size) * sizeof(void *)), "2d matrix", -1); 
  }

  *cur_col_size += delta_col_size;
  *ptr = matrix;
  return 0;
}

int ns_allocate_matrix_col(void ***ptr, short delta_size, int *cur_row_size, int *cur_col_size)
{
  int i;
  void **matrix = *ptr;

  if(!delta_size)
    delta_size = 64;

  for(i = 0; i < *cur_col_size; i++) 
  {
    MY_REALLOC_AND_MEMSET(matrix[i], ((delta_size + *cur_row_size) * sizeof(void *)), 
                      ((*cur_row_size) * sizeof(void *)), "2d matrix", -1);
  }
  *cur_row_size += delta_size;
  *ptr = matrix;
  return 0;
}

