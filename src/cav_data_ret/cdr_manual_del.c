#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdr_cache.h"
#include "cdr_config.h"
#include "cdr_log.h"
#include "cdr_mem.h"
#include "cdr_main.h"
#include "cdr_utils.h"
#include "cdr_components.h"
#include "cdr_dir_operation.h"

#include "cdr_cleanup.h"
#include "cdr_cmt_handler.h"
#include "cdr_nv_handler.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"
#include "cdr_manual_del.h"


void cdr_handle_manual_delete()
{
  int normid = -2;
  char testrun [8];
  int c;
  
  sprintf (testrun, "%d",  g_tr_num);

  if(g_tr_num != 0)
  {
    if (partition_is_running(g_tr_num, g_partition_num) == CDR_TRUE)
    {
      printf ("Partition Provided is running partition... Please try after some time or provide another partition number\n");
      return;
    }

    if(cdr_read_cache_file(1) == CDR_ERROR) {
      CDRTL1("Error: cache file read failed");
    }
    normid = nslb_get_norm_id (&cache_entry_norm_table, testrun, strlen (testrun));
    if(normid < 0) 
    {
      if(normid == -2) 
      { 
        if((normid = create_cache_entry_from_tr(g_tr_num, normid, 1)) == CDR_ERROR) // add to cache_entry_list
          return;
      }
    }
    if (g_partition_num != 0 )
    {
      if (cdr_read_cmt_cache_file(1) == CDR_ERROR) 
        CDRTL1("Error: cache file read failed");
     
      normid = nslb_get_norm_id (&cmt_cache_entry_norm_table, testrun, strlen(testrun));
      if (normid < 0) 
      {
        if (normid == -2) 
        {
          char partition_num_str[16];
          sprintf (partition_num_str, "%lld", g_partition_num);

          if ((normid = cmt_cache_entry_add_from_partition(g_partition_num, partition_num_str, normid, -1)) == CDR_ERROR)
           return;
        }
      }
      if (g_component_name [0] == '\0') {
        remove_partition (g_tr_num, g_partition_num);
        cdr_cmt_cache_entry_list[normid].is_partition_present = FALSE;
        printf ("Partition Removed Successfully\n");
        return;
      }

      c = get_component (g_component_name);
      switch(c)
      {
               case GRAPH_DATA:
                         remove_partition (g_tr_num, g_partition_num);
                         cdr_cmt_cache_entry_list[normid].is_partition_present = FALSE;
                         cdr_cmt_cache_entry_list[normid].partition_graph_data_remove_f = TRUE;
                         break;
               case CSV:
                         remove_partition_csv (normid);
                         cdr_cmt_cache_entry_list[normid].partition_csv_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_csv_remove_f = TRUE;
                         break;
               case RAW_DATA:
                         remove_partition_raw_data(normid);
                         cdr_cmt_cache_entry_list[normid].partition_raw_file_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_raw_file_remove_f = TRUE;
                         break;
               case DB:
                         remove_partition_db(normid);
                         cdr_cmt_cache_entry_list[normid].partition_db_table_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_db_index_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_db_remove_f = 0;
                         break;
               case HAR_FILE:
                         remove_partition_har_file(normid);
                         cdr_cmt_cache_entry_list[normid].partition_har_file_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_har_file_remove_f = TRUE;
                         break;
               case PAGEDUMP:
                         remove_partition_page_dump (normid);
                         cdr_cmt_cache_entry_list[normid].partition_page_dump_size = 0;
                         cdr_cmt_cache_entry_list[normid].partition_page_dump_remove_f = TRUE;
                         break;
               case LOGS:
                         remove_partition_logs (normid);
                         cdr_cmt_cache_entry_list[normid].partition_logs_size = TRUE;
                         cdr_cmt_cache_entry_list[normid].partition_logs_remove_f = TRUE;
                         break;
               case REPORTS:
                         remove_partition_reports (normid);
                         cdr_cmt_cache_entry_list[normid].partition_reports_size = TRUE;
                         cdr_cmt_cache_entry_list[normid].partition_reports_remove_f = TRUE;
                         break;
      }
      cdr_dump_cmt_cache_to_file();
      printf ("component %s removed successfully removed from TR%d and partition %lld\n", g_component_name, g_tr_num, g_partition_num); 
      return;
    }
    else if (g_component_name[0] != '\0') 
    {

       c = get_component (g_component_name);
       switch (c)
       {
	       case GRAPH_DATA:
		       //remove_tr (cdr_cache_entry_list[normid].tr_num);
		       remove_tr (&cdr_cache_entry_list[normid]);
		       cdr_cache_entry_list[normid].is_tr_present = FALSE;
		       break;
	       case CSV:
		       remove_csv(&cdr_cache_entry_list[normid]);
		       cdr_cache_entry_list[normid].csv_size = 0;
		       break;
	       case RAW_DATA:
		        remove_raw_data(&cdr_cache_entry_list[normid]);
			cdr_cache_entry_list[normid].raw_file_size = 0;
			break;
	       case DB:
			 //remove_db(&cdr_cache_entry_list[normid]);
			 remove_db(&cdr_cache_entry_list[normid]);
			 cdr_cache_entry_list[normid].tr_db_table_size = 0;
			 cdr_cache_entry_list[normid].tr_db_index_size = 0;
			 break;
	       case HAR_FILE:
			 remove_har_file(&cdr_cache_entry_list[normid]);
			 cdr_cache_entry_list[normid].har_file_size = 0;
			 break;
	       case PAGEDUMP:
			 remove_pagedump (&cdr_cache_entry_list[normid]);
			 cdr_cache_entry_list[normid].page_dump_size = 0;
			 break;
	       case LOGS:
			 remove_logs (&cdr_cache_entry_list[normid]);
			 cdr_cache_entry_list[normid].logs_size = 0;
			 break;
               case REPORTS:
                         remove_reports (&cdr_cache_entry_list[normid]);
                         cdr_cache_entry_list[normid].reports_size = 0;
                         break;
       }

       /* dump the cache  */
       cdr_dump_cache_to_file();
       printf ("component %s removed successfully removed from TR%d\n", g_component_name, g_tr_num); 
       return;
    }
    
    /* remove_tr (g_tr_num) */
    remove_tr_ex (g_tr_num);

    /* dump the cache  */
    cdr_dump_cache_to_file();
    printf ("TR%d removed successfully\n", g_tr_num); 

  }
  else 
  {
    //upload NV cache
    if(cdr_read_nv_cache_file(1) == CDR_ERROR) {
      CDRTL1("Error: cache file read failed");
    }

    //norm_id = nslb_get_norm_id(&nv_cache_entry_norm_table, partition_ptr, strlen(partition_ptr));
    normid = nslb_get_norm_id(&nv_cache_entry_norm_table, NULL, 8);

    if(normid < 0) {
      if(normid == -2) {
        // -2 means tr is not present
        //if((norm_id = nv_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
        if((normid = nv_cache_entry_add_from_partition(g_partition_num, NULL, normid, 0)) == CDR_ERROR) // add to cache_entry_list
		return;
      }
    }

    if (g_partition_num != 0 && g_component_name[0] != '\0')
    {
      // remove nv  compomnet
       //update the size to for tat compmnent
       c = get_component (g_component_name);
       switch(c)
       {
	    case DB_AGG:
		    remove_nv_partition_db (normid);
		    break;
	    case CSV:
		    remove_nv_partition_csv (normid);
		    break;
	    case OCX:
		    remove_nv_partition_ocx (normid);
		    break;
	    case NA_TRACES:
		    remove_nv_partition_na_traces (normid);
		    break;
	    case ACCESS_LOG:
		    remove_nv_partition_access_log (normid);
		    break;
	    case LOGS:
		    remove_nv_partition_logs (normid);
		    break;
       }

       // dump the cache;
       cdr_dump_nv_cache_to_file();

       return;
    }
    if (g_partition_num != 0)
    {
      //call Nv remove partion

      // update cache is partition_present flag 0

      //dump cache
      cdr_dump_nv_cache_to_file();

      return;
    }
  }
}

