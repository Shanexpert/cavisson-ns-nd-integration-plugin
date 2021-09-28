#define NV_MAP_TBL_ALLOC_SIZE 10000
#define MAX_2_POW_N 1073741824

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_nv_tbl.h"
#include "ns_alloc.h"
#include "ns_compress_idx.h"
#include "ns_log.h"
#include "util.h"
#include "ns_custom_monitor.h"
#include "ns_monitor_profiles.h"
#include "ns_tsdb.h"

static int nslb_change_to_2_power_n(int value)
{
  int ret = MAX_2_POW_N  >> 1; /* Right shift 1 bit means divide by 2 */

  while(ret > 0)
  {
    if (value > ret)
      return (ret << 1); /* Left shift one bit means multiply by 2 */

    ret = ret >> 1; /* Right shift 1 bit means divide by 2 */
  }

  return value;
}


int nv_get_id_value(nv_id_map_table_t *map_ptr, int id, int *is_new_vector)
{
  
  if(id == 0) //for overall
  {
    return id;
  }

  // in case of not malloc
  if(!map_ptr)
    return -1;

  //allocated size is small
  //check if = required
  if(id >= map_ptr->mapSize)
  {
    //Need to realloc the map
    int new_size = nslb_change_to_2_power_n(id+1);  //malloc for id 0 also

    MY_REALLOC_AND_MEMSET(map_ptr->idxMap, (sizeof(int) * new_size), (sizeof(int) * map_ptr->mapSize), "map table", -1);
    if(!(map_ptr->idxMap))
      return -1;
    map_ptr->mapSize = new_size;
  }
  //new vector
  if (map_ptr->idxMap[id] == 0)
  {
    if(is_new_vector != NULL)
    {
      *is_new_vector = 1;      //new_vector
    }
    map_ptr->idxMap[id] = map_ptr->maxAssValue++;
  }
  //existing vector, return mapped id
  return map_ptr->idxMap[id];
}

int parse_and_fill_nv_data(char *buffer, CM_info *cus_mon_ptr)
{  
  char vector[VECTOR_NAME_MAX_LEN] = {0};
  int vectorid = -1;
  char *buffer_check = NULL;
  int is_new_vector = 0, ret = -1, mapped_id = -1;

  if(strchr(buffer, ':') == NULL)
    return -2;
    
  //buffer : (1) id:vector name (2) id:vector name|10 20 30
  buffer_check = strchr(buffer, '|');
 
  if(buffer_check) 
  {
    *buffer_check = '\0';
    buffer_check++;
  }
  
  //extract vector and  vector id
  ret = parse_nv_vector(buffer, vector, &vectorid);

  if ( ret == -2) //old format
  {
    return ret;
  }

  if(ret == 0) 
  {  
    //Check if vectorIdxmap table was not malloced then do that. 
    if(!cus_mon_ptr->vectorIdxmap)
    {
      cus_mon_ptr->vectorIdxmap = ns_init_map_table(NV_MAP_TBL);
      
      MY_MALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl, sizeof(NVMapDataTbl), "malloc nv data table", -1);

      //cus_mon_ptr->vector_list[0].vectorIdx = -1; 
    }

    //mapped id
    mapped_id = nv_get_id_value(cus_mon_ptr->vectorIdxmap, vectorid, &is_new_vector);

    /* Setting vectoridx in vector list is not required here. As the same step is done in add_cm_vector_info_node(). So commenting it here.
       cahnge is done from 4.1.13.19. BUG 57871.
    //If this is first vector then update vectorIdx. 
    if(cus_mon_ptr->vector_list[0].vectorIdx == -1) 
      cus_mon_ptr->vector_list[0].vectorIdx = mapped_id;
    */

    //fill data
    //if(buffer_check)
    check_realloc_filldata_nv_map_data_tbl(cus_mon_ptr, mapped_id, buffer_check, vector);
  }

  return is_new_vector;
}

/* This function parses vector of NV monitor (id:vectorname)
 * Return values:
 *      -1     Error- address of vector is NULL or passed vector name is NULL
 *      -2     ':' is not present in vector
 *       0      success case , extracted vector id and vector name
 */
int parse_nv_vector(char *vector_name, char *vector, int *vectorid)
{
  char *ptr = NULL;

  //KJ:why to check as this is an array & address of integer
  //if(vector_name == NULL || vector == NULL)
  //  return -1;

  ptr = strchr(vector_name, ':');

  if(!ptr)
    return -2; //incorrect format
  
  *ptr = '\0';

  *vectorid = atoi(vector_name);
 
  strncpy(vector, ptr+1, VECTOR_NAME_MAX_LEN - 1);  
  return 0;
}

//return values: old vector -> 0 , new vector -> 1
int check_realloc_filldata_nv_map_data_tbl(CM_info *cus_mon_ptr, int id, char *data, char *vector)
{
  int no_of_element = -1;

  //get no of elements
  if(cus_mon_ptr->no_of_element == 0)
  {
    get_no_of_elements(cus_mon_ptr, &no_of_element);
    cus_mon_ptr->no_of_element = no_of_element;
  } 
  else
    no_of_element = cus_mon_ptr->no_of_element;     

  if(cus_mon_ptr->dummy_data == NULL)
  {
    MY_MALLOC_AND_MEMSET(cus_mon_ptr->dummy_data, (no_of_element * sizeof(double)), "cus_mon_ptr->dummy_data", -1); 
  }

  if ( id >= cus_mon_ptr->nv_map_data_tbl->cur_entries )
  {
    MY_REALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->data, ((NV_MAP_TBL_ALLOC_SIZE + cus_mon_ptr->nv_map_data_tbl->cur_entries) * sizeof(double)), (cus_mon_ptr->nv_map_data_tbl->cur_entries * sizeof(double)), "cus_mon_ptr->nv_map_data_tbl->data", -1);
   
/*    if(!(g_tsdb_configuration_flag & RTG_MODE))
    {
       MY_REALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->metrics_idx, ((NV_MAP_TBL_ALLOC_SIZE + cus_mon_ptr->nv_map_data_tbl->cur_entries) * sizeof(long)), (cus_mon_ptr->nv_map_data_tbl->cur_entries * sizeof(long)), "cus_mon_ptr->nv_map_data_tbl->data", -1);
    }*/

    cus_mon_ptr->nv_map_data_tbl->cur_entries += NV_MAP_TBL_ALLOC_SIZE;
  }
  else
  {
    NSDL2_MON(NULL, NULL, "Have space in NVMapDataTbl no need to do any allocation");
  }

  //although temp_enteries and cur_enteries are equal but allocation of cus_mon_ptr->nv_map_data_tbl->temp_data is not in above block where we are allocationg cus_mon_ptr->nv_map_data_tbl->data because we will allocate cus_mon_ptr->nv_map_data_tbl->temp_data only if we get header in   data and we will allocate cus_mon_ptr->nv_map_data_tbl->data when we receive vector list.

  if(cus_mon_ptr->nv_data_header & NV_START_HEADER)
  {
    if(id >= cus_mon_ptr->nv_map_data_tbl->temp_entries)
    {
      MY_REALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->temp_data, ((cus_mon_ptr->nv_map_data_tbl->cur_entries) * sizeof(double)), (cus_mon_ptr->nv_map_data_tbl->temp_entries * sizeof(double)), "cus_mon_ptr->nv_map_data_tbl->temp_data", -1);

      /*if(!(g_tsdb_configuration_flag & RTG_MODE))
      {  
        MY_REALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->metrics_idx_tmp, ((cus_mon_ptr->nv_map_data_tbl->cur_entries) * sizeof(long)), (cus_mon_ptr->nv_map_data_tbl->temp_entries * sizeof(long)), "cus_mon_ptr->nv_map_data_tbl->temp_data", -1);
      }*/


      cus_mon_ptr->nv_map_data_tbl->temp_entries = cus_mon_ptr->nv_map_data_tbl->cur_entries;
    }
  } 

  if(cus_mon_ptr->nv_map_data_tbl->data[id] == NULL)
  {
    MY_MALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->data[id], (no_of_element * sizeof(double)), "cus_mon_ptr->nv_map_data_tbl->data[id]", -1);
  }

   if(!(g_tsdb_configuration_flag & RTG_MODE))
   {
      ns_tsdb_malloc_metric_id_buffer(&cus_mon_ptr->nv_map_data_tbl->metrics_idx_tmp, no_of_element);
   //  MY_MALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->metrics_idx_tmp, (no_of_element * sizeof(long)), "cus_mon_ptr->nv_map_data_tbl->temp_data[id]", -1); 
   }


  if((cus_mon_ptr->nv_data_header & NV_START_HEADER) && (cus_mon_ptr->nv_map_data_tbl->temp_data[id] == NULL))
  {
    MY_MALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->temp_data[id], (no_of_element * sizeof(double)), "cus_mon_ptr->nv_map_data_tbl->temp_data[id]", -1); 
  }

   if(!(g_tsdb_configuration_flag & RTG_MODE))
   {
      //MY_MALLOC_AND_MEMSET(cus_mon_ptr->nv_map_data_tbl->metrics_idx, (no_of_element * sizeof(long)), "cus_mon_ptr->nv_map_data_tbl->temp_data[id]", -1); 
       ns_tsdb_malloc_metric_id_buffer(&cus_mon_ptr->nv_map_data_tbl->metrics_idx, no_of_element);
   }
  
  if( data != NULL )
  {
    if(cus_mon_ptr->nv_data_header & NV_START_HEADER)
    {
      validate_and_fill_data(cus_mon_ptr, no_of_element, data, cus_mon_ptr->nv_map_data_tbl->temp_data[id], cus_mon_ptr->nv_map_data_tbl->metrics_idx_tmp, &cus_mon_ptr->nv_map_data_tbl->metrics_idx_tmp_found, vector);
    }
    else
    {
      validate_and_fill_data(cus_mon_ptr, no_of_element, data, cus_mon_ptr->nv_map_data_tbl->data[id], cus_mon_ptr->nv_map_data_tbl->metrics_idx,  &cus_mon_ptr->nv_map_data_tbl->metrics_idx_found, vector);

    }
  }
  //if ( data != NULL )
  //{
   // validate_and_fill_data(cus_mon_ptr, no_of_element, data, cus_mon_ptr->nv_map_data_tbl->data[id]);
  //}
  return 0;    
}
