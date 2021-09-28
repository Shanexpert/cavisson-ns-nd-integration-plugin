#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_compress_idx.h"
#include "ns_alloc.h"
#include "ns_log.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_nv_tbl.h"

//#ifdef TEST
#define MAX_2_POW_N 1073741824

static int nslb_change_to_2_power_n(int value)
{
  int ret = MAX_2_POW_N  >> 1; /* Right shift 1 bit means divide by 2 */

  if (value <= 0)
    return 1;

  while(ret > 0)
  {
    if (value > ret)
      return (ret << 1); /* Left shift one bit means multiply by 2 */

    ret = ret >> 1; /* Right shift 1 bit means divide by 2 */
  }

  return value;
}
//#endif

//This will allocate id_map_tbl

void *ns_init_map_table(int tbl_type)
{
  id_map_table_t *map_ptr;
  if(tbl_type == NS_MAP_TBL)
  {
    MY_MALLOC_AND_MEMSET(map_ptr, sizeof(id_map_table_t), "NS id Map Table", -1);
    //we need to start with index 1
    map_ptr->maxAssValue = 1;
  }
  else if(tbl_type == NV_MAP_TBL)
  {
    MY_MALLOC_AND_MEMSET(map_ptr, sizeof(nv_id_map_table_t), "NV id Map Table", -1);
    //we need to start with index 1
    map_ptr->maxAssValue = 1;
  }
  return map_ptr;
}


//get the compressed id for the id passed

int ns_get_id_value(id_map_table_t *map_ptr, int id)
{

  //either put '0' mapped value while creating overall or return map id '0' if id is '0'
  if(id == 0) //for overall
  {
    return id;
  }

  // in case of not malloc
  if(!map_ptr)
    return -1;

  //No need to malloc
  if(id < map_ptr->mapSize)
  {
    //id found
    if(map_ptr->idxMap[id] != 0)
      return map_ptr->idxMap[id];
  }
  else
  {
    //Need to realloc the map
    int new_size = nslb_change_to_2_power_n(id+1);  //malloc for id 0 also

    MY_REALLOC_AND_MEMSET(map_ptr->idxMap, (sizeof(short) * new_size), (sizeof(short) * map_ptr->mapSize), "map table", -1);
    if(!(map_ptr->idxMap))
      return -1;
    map_ptr->mapSize = new_size;
  }

  map_ptr->idxMap[id] = map_ptr->maxAssValue++;
  return map_ptr->idxMap[id];
}

#ifdef TEST
int main()
{
  id_map_table_t *map_ptr = ns_init_map_table(NS_MAP_TBL);
  int id;
  while(1)
  {
    printf("Enter the Id:");
    scanf("%d", &id);
    printf("Value of id %d is %d\n", id, ns_get_id_value(map_ptr, id));
  }
  return 0;
}
#endif
