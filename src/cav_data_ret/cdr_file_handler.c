#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "cdr_file_handler.h"
#include "cdr_log.h"
#include "cdr_main.h"
#include "cdr_utils.h"
#include "cdr_cache.h"
#include "cdr_dir_operation.h"
#include "cdr_cmt_handler.h"
#include "cdr_nv_handler.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"


static int is_tr(const struct dirent *a) 
{
  
  if (nslb_get_file_type(logsPath, a) == DT_DIR && (a->d_name[0] == 'T') && (a->d_name[1] == 'R') ) {
    if(!ns_is_numeric((char *)(a->d_name + 2)))
      return CDR_FALSE;
    return CDR_TRUE;
  }
  else
    return CDR_FALSE;
}

/************************************************************************************
 * Name: cdr_get_tr_list_from_disk
 *    This function will read the current TRs present in <ns_wdir>/logs
 *    And compare it with the cache_entry_list
 *    If present in cache entry, flag is marked present
 *    Otherwise new entry is add to the cache entry
 ***********************************************************************************/
int cdr_get_tr_list_from_disk(int cal_size)
{
  int n;
  int norm_id = -2;
  int tr_num;
  char *tr_ptr;
  struct dirent **tr_list;
  char tr_path[CDR_FILE_PATH_SIZE];
  char summary_top[CDR_FILE_PATH_SIZE];

  CDRTL2("Method called");

  sprintf(tr_path, "%s/logs", ns_wdir);
  logsPath = tr_path;

  // getting tr list in <nswdir>/logs
  n = scandir(tr_path, &tr_list, is_tr, NULL);
  if(n == -1) {
    CDRTL1("Error: scandir failed. error: %s", strerror(errno));
    return CDR_ERROR;
  }
  CDRTL3("Number of TRs in disk: %d", n);
  
  update_running_test_list ();
 
  while(n--) { // start accessing from the last
    tr_ptr = tr_list[n]->d_name + 2; // +2 for TR
    norm_id = -2;
    if(rebuild_cache != CDR_TRUE) // no need to get norm id  
    {
      norm_id = nslb_get_norm_id(&cache_entry_norm_table, tr_ptr, strlen(tr_ptr));
      //DRTL1("cache_entry_norm_table = %p, tr_ptr '%s' strlen(tr_ptr) '%d'", cache_entry_norm_table, tr_ptr, strlen(tr_ptr));
    }
    tr_num = atoi(tr_ptr);
    sprintf(summary_top, "%s/logs/TR%d/summary.top", ns_wdir, tr_num);

    if(norm_id < 0) {
      if(norm_id == -2) {
        // -2 means tr is not present

        if((norm_id = create_cache_entry_from_tr(tr_num, norm_id, cal_size)) == CDR_ERROR) // add to cache_entry_list
          continue;
        
        if (cdr_cache_entry_list[norm_id].tr_type == CMT_TR)
          cmt_tr_cache_idx = norm_id; // to read the cache index

        CDRTL3("tr: [%d] not present in cache, added to cdr_cache_entry_list", tr_num);
      }
      else {
        CDRTL1("Error: nslb_get_norm_id , norm_id '%d'", norm_id);
        return CDR_ERROR;
      }
    }
    else if((cdr_cache_entry_list[norm_id].tr_type == RUNNING_TR) || cdr_cache_entry_list[norm_id].lmd_summary_top != get_lmd_ts(summary_top))
    {
      if((norm_id = create_cache_entry_from_tr(cdr_cache_entry_list[norm_id].tr_num, norm_id, 1)) == CDR_ERROR) 
        continue;
    }

    cdr_cache_entry_list[norm_id].is_tr_present = CDR_TRUE;
  }

  CDRTL2("Method exit");
  return CDR_SUCCESS;
}

int partition_is_bad(int tr_num, long long int partition_num)
{
  return 0;
}

int cdr_get_cmt_partition_from_disk()
{
  
  int n;
  int norm_id = -2;
  long long int partition_num;
  char *partition_ptr;
  struct dirent **partition_list;
  CDRTL2("Method called");

  if(cmt_tr_cache_idx == -1)
    return 0;
  if(cdr_cache_entry_list[cmt_tr_cache_idx].partition_list == NULL)
    cdr_cache_entry_list[cmt_tr_cache_idx].partition_list = get_tr_partiton_list(cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, &(cdr_cache_entry_list[cmt_tr_cache_idx].count), NULL);

  CDRTL3("Partition_list is set for CMT tr '%d', total number of partition '%d'.", 
          cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cache_entry_list[cmt_tr_cache_idx].count);

  if(cdr_cache_entry_list[cmt_tr_cache_idx].count == 0) {
    CDRTL1("Error: CMT tr '%d' didn't have an partition.", cdr_cache_entry_list[cmt_tr_cache_idx].tr_num);
    return CDR_ERROR;
  }

  partition_list = cdr_cache_entry_list[cmt_tr_cache_idx].partition_list;

  for(n = 0; n < cdr_cache_entry_list[cmt_tr_cache_idx].count; n++)
  {
    partition_ptr = partition_list[n]->d_name; 
    norm_id = -2;
    
    if(rebuild_cache != CDR_TRUE) // no need to get norm id  
      norm_id = nslb_get_norm_id(&cmt_cache_entry_norm_table, partition_ptr, strlen(partition_ptr));

    if(norm_id < 0) {
      if(norm_id == -2) {
        // -2 means tr is not present
        partition_num = atoll(partition_ptr);


        if((norm_id = cmt_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
          continue;
        
        CDRTL3("partition: [%lld] not present in cache, added to cdr_cmt__cache_entry_list", partition_num);
      }
      else {
        CDRTL1("Error: nslb_get_norm_id");
        return CDR_ERROR;
      }
    }
    else if (cdr_cmt_cache_entry_list[norm_id].partition_type == RUNNING_PARTITION)
    {
      if((norm_id = cmt_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
        continue;
    }

    cdr_cmt_cache_entry_list[norm_id].is_partition_present = CDR_TRUE;
  }

  CDRTL2("Method exit");
  
  return CDR_SUCCESS;
}


int cdr_get_nv_partition_from_disk()
{
  
  CDRTL2("Method called");
  int n;
  int norm_id = -2;
  long long int partition_num;
  char *partition_ptr;

  char path[CDR_FILE_PATH_SIZE] = "\0";

  snprintf(path, CDR_FILE_PATH_SIZE, "%s/rum/", hpd_root);

  nv_partition_list = get_tr_partiton_list(0, &(nv_partition_count), path);

  CDRTL3("Partition_list is set for NV Client ID '%d', total number of partition '%d'.", 
          nv_client_id, nv_partition_count);

  if(nv_partition_count == 0) {
    CDRTL1("Error: NV  Client ID'%d' didn't have an partition.", nv_client_id); 
    return CDR_ERROR;
  }

  if (nv_client_id[0] != '\0')
  {
    norm_id = nv_cache_entry_add_overall(nv_client_id); 
  }

  for(n = 0; n < nv_partition_count; n++)
  {
    partition_ptr = nv_partition_list[n]->d_name; 
    norm_id = -2;
    
    if(rebuild_cache != CDR_TRUE) // no need to get norm id  
      norm_id = nslb_get_norm_id(&nv_cache_entry_norm_table, partition_ptr, strlen(partition_ptr));

    if(norm_id < 0) {
      if(norm_id == -2) {
        // -2 means tr is not present
        partition_num = atoll(partition_ptr);


        if((norm_id = nv_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
          continue;
        
        CDRTL3("partition: [%lld] not present in cache, added to cdr_nv_cache_entry_list", partition_num);
      }
      else {
        CDRTL1("Error: nslb_get_norm_id");
        return CDR_ERROR;
      }
    }
    else if (cdr_nv_cache_entry_list[norm_id].partition_type == RUNNING_PARTITION)
    {
      if((norm_id = nv_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
        continue;
    }
   
    cdr_nv_cache_entry_list[norm_id].is_partition_present = CDR_TRUE;
  }

  CDRTL2("Method exit");
  
  return CDR_SUCCESS;
}
