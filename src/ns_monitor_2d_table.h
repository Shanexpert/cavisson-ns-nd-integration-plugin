#ifndef _ns_monitor_2d_table_h
#define _ns_monitor_2d_table_h


#if 0
typedef struct instanceVectorIdx
{
  double *data;
  short norm_vector_id;
  unsigned short tier_id;  //tier ID returned from vector/data. This can be different from what NS is storing and should be taken care.
} instanceVectorIdx;


typedef struct tierNormVectorIdx
{
 double *aggrgate_data;  //do we need to have a double ptr storing all the data received from different BCI
} tierVectorIdx;

#endif

void init_instance_vector_table(instanceVectorIdx **);
void inti_tier_normalized_vector_table(tierNormVectorIdx **);

extern void ns_normalize_monitor_vector(CM_info *, char *, int *);
extern int ns_allocate_matrix_col(void ***, short , int *, int *);

#endif
