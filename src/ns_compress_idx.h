#ifndef _ns_compress_idx_h
#define _ns_compress_idx_h
#define NS_MAP_TBL 1
#define NV_MAP_TBL 2

typedef struct id_map_table_t
{
  short *idxMap;
  int mapSize;
  int maxAssValue;
} id_map_table_t;

extern void *ns_init_map_table(int tbl_type);
extern int ns_get_id_value(id_map_table_t *map_ptr, int id);
#endif
