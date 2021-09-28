#ifndef _NS_TR069_LOAD_DATA_H_
#define _NS_TR069_LOAD_DATA_H_ 

#define TR069_MAX_PARAMETER_VALUE_LEN    64

#define NS_TR069_BUF_LEN                 2048
#define NS_TR069_PATH_LEN                2048
#define DELTA_TR069_BIGBUFFER            10240

#define NS_TR069_PARAM_FULL_PATH                     0x00000001
#define NS_TR069_PARAM_SCALAR                        0x00000002
#define NS_TR069_PARAM_RONLY                         0x00000004
#define NS_TR069_PARAM_OBJECT                        0x00000008
#define NS_TR069_PARAM_UINT                          0x00000010
#define NS_TR069_PARAM_STRING                        0x00000020
#define NS_TR069_PARAM_DATE_TIME                     0x00000040
#define NS_TR069_PARAM_BOOLEAN                       0x00000080
#define NS_TR069_INFORM_PARAMETER                    0x00000100 
#define NS_TR069_INFORM_PARAMETER_RUN_TIME           0x00000200 
 
#include "../../ns_data_types.h"

typedef struct {

  int path_table_index;    // Hash code of path name

} TR069FullParamPath_t;

typedef struct  {

  unsigned short start_index;
  unsigned short num_indexes;

} TR069PathTrans;

typedef struct {
  
  char notification;
  ns_ptr_t value_idx;            // Pointer to g_tr069_big_buf 
  unsigned short data_len;

} TR069DataValue_t;

typedef struct {

  void *ptr;

} TR069VectorInstances;

typedef struct {

  unsigned short path_type_flags; 
  unsigned int min_value;
  unsigned int max_value;

  union {
    TR069PathTrans       full_path;
    TR069DataValue_t     *value;
    TR069VectorInstances *vector_instances;
  } data;

} TR069ParamPath_t;

extern int total_param_path_entries;
extern int total_full_param_path_entries;

extern TR069ParamPath_t    *tr069_param_path_table;
extern TR069FullParamPath_t *tr069_full_param_path_table;

// Big buff logic copied from copy_into_big_buf
extern char* g_tr069_big_buf;
extern char* g_tr069_buf_ptr;

extern ns_bigbuf_t max_tr069_buffer_space;

extern int IGDMgSvrURLidx;
extern int IGDDeviceInfoManufactureridx;
extern int IGDDeviceInfoManufacturerOUIidx;
extern int IGDDeviceInfoProductClassidx;
extern int IGDDeviceInfoSerialNumberidx;
extern int IGDPeriodicInformEnableidx;
extern int IGDPeriodicInformIntervalidx;

extern void init_g_tr069_big_buf(int size);

extern ns_bigbuf_t copy_into_tr069_big_buf(char* data, ns_bigbuf_t size);

extern int tr069_get_param_table_idx(TR069ParamPath_t *ptr, char *to_search, int to_search_len);
extern char *tr069_get_value_from_big_buf(TR069DataValue_t *value_ptr, int *value_len);
extern unsigned long unused_cpe_data_size(FILE *cpe_data_file, unsigned int num_unused_record);

#endif
