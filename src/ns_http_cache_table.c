#include "ns_cache_include.h"
#include "ns_common.h"
#include "ns_exit.h"

#ifndef CAV_MAIN
CacheAllocSize *cas_ptr = NULL;
#else
__thread CacheAllocSize *cas_ptr = NULL;
#endif
// This variable be set based on the mode of cache table size keyword
// As grp based setting is in shared memory, each NVM need to keep track of this. 
// So we are keeping at as local variable


// this method is to caclulate cache table size based on previous session cache entries, used in mode 1.
// here cache table size is calculated on the bases of initial size given by user and previus session entries.For example
// if initial size value is 128 and max_cache_entries for first session is 64,then cache table size for the session will be 
// (128*1+64)/1+1 = 96. 

void cache_set_cache_table_size_value(int grp_idx, int max_cache_entries)
{
  NSDL2_CACHE(NULL, NULL, "method called. cache_table_size_value = %d max_cache_entries = %d", cas_ptr[grp_idx].cache_table_size_value, max_cache_entries);
  
  cas_ptr[grp_idx].cache_table_size_value = (cas_ptr[grp_idx].cache_table_size_value * cas_ptr[grp_idx].cum_count + 
                                     max_cache_entries)/(cas_ptr[grp_idx].cum_count + 1);
  
  // convert to 2^n
  cas_ptr[grp_idx].cache_table_size_value = change_to_2_power_n(cas_ptr[grp_idx].cache_table_size_value);
  cas_ptr[grp_idx].cum_count++;
  
  NSDL2_CACHE(NULL, NULL, "after taking avg with previus sessions cache_table_size_value = %d, cumlative count = %d", cas_ptr[grp_idx].cache_table_size_value, cas_ptr[grp_idx].cum_count);
}

/*This function is for allocating master table pointers.
 * This function should call after allocation of shared memory.*/
void init_cache_table (){

  int flag = 0;
  int j;
  int grp_idx;
  int flag_cache_enable = 0;

  NSDL3_CACHE(NULL, NULL, "Method called");
  
  //Check if cache is enabled or not for group
  for( grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    if(runprof_table_shr_mem[grp_idx].gset.cache_user_pct > 0)
    {
      flag_cache_enable = 1;
      break;
    }
  }
 
  if(flag_cache_enable == 0)
  {
    NSDL3_CACHE(NULL, NULL, "Cacheing is not enable for any group.");
    return;
  } 
 
  if(cas_ptr == NULL) 
  {
    NSDL3_CACHE(NULL, NULL, "allocating and initilizing CacheAllocSize");
    MY_MALLOC_AND_MEMSET(cas_ptr, sizeof(CacheAllocSize) * total_runprof_entries, "cache CacheAllocSize allocated", 0);
 
    for( grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
    {  
      flag = 0;
      cas_ptr[grp_idx].cache_table_size_value = runprof_table_shr_mem[grp_idx].gset.cache_table_size_value; 
      NSDL3_CACHE(NULL, NULL, "Group = %d, master table mode = %d", grp_idx, runprof_table_shr_mem[grp_idx].gset.master_cache_mode);
      if(runprof_table_shr_mem[grp_idx].gset.master_cache_mode == 1)
      {
        for(j = 0; j < grp_idx; j++)
        {
           NSDL3_CACHE(NULL, NULL, "runprof_table_shr_mem[%d].gset.master_cache_tbl_name = %s, runprof_table_shr_mem[%d].gset.master_cache_tbl_name = %s", grp_idx, runprof_table_shr_mem[grp_idx].gset.master_cache_tbl_name, j, runprof_table_shr_mem[j].gset.master_cache_tbl_name);
          if(runprof_table_shr_mem[j].gset.master_cache_mode == 1)
          {
            if(!strcmp(runprof_table_shr_mem[grp_idx].gset.master_cache_tbl_name, runprof_table_shr_mem[j].gset.master_cache_tbl_name))
            {
              NSDL3_CACHE(NULL, NULL, "Allocating old master table pointer");
              if(runprof_table_shr_mem[grp_idx].gset.master_cache_table_size != runprof_table_shr_mem[j].gset.master_cache_table_size)
              {
                NS_EXIT(-1, "For master table %s two different sizes %d and %d are given. Please correct.", runprof_table_shr_mem[grp_idx].gset.master_cache_tbl_name, runprof_table_shr_mem[grp_idx].gset.master_cache_table_size, runprof_table_shr_mem[j].gset.master_cache_table_size);
              }
              cas_ptr[grp_idx].master_cache_table_ptr = cas_ptr[j].master_cache_table_ptr;
              cas_ptr[grp_idx].master_cache_table_size = cas_ptr[j].master_cache_table_size; 
              NSDL3_CACHE(NULL, NULL, "master table pointer %p, size = %d, group = %d", cas_ptr[grp_idx].master_cache_table_ptr, cas_ptr[grp_idx].master_cache_table_size, grp_idx);
              flag = 1;
              break;
            }
          }  
        }
        //Filled in inner loop
        if(flag == 1)
          continue;
        cas_ptr[grp_idx].master_cache_table_size = change_to_2_power_n(runprof_table_shr_mem[grp_idx].gset.master_cache_table_size);
        MY_MALLOC_AND_MEMSET(cas_ptr[grp_idx].master_cache_table_ptr, sizeof(CacheTable_t *) * cas_ptr[grp_idx].master_cache_table_size, "Master table allocation", 0);
        NSDL3_CACHE(NULL, NULL, "Allocated master table pointer %p, size = %d, group = %d", cas_ptr[grp_idx].master_cache_table_ptr, cas_ptr[grp_idx].master_cache_table_size, grp_idx);
      }
    }
  }
  NSDL3_CACHE(NULL, NULL, "Method exiting");
}
