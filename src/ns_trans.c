/********************************************************************
* Name: ns_trans.c
* Purpose: API,s and functions for the transaction feature of Netstorm.
* Author: Anuj
* Intial version date: 22/10/07
* Last modification date
********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_trans.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "logging.h"
#include "ns_log.h"
#include "ns_trans_parse.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_debug_trace.h"
#include "ns_percentile.h"
#include "ns_string.h"

#include "ns_vuser_ctx.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_page.h"
#include "ns_page_dump.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"
#include "ns_vuser_tasks.h"
#include "ns_server_ip_data.h"
#include "ns_trace_level.h"
#include "ns_child_thread_util.h"
#include "ns_dynamic_avg_time.h"
#include "ns_script_parse.h"

#ifndef CAV_MAIN
NormObjKey normRuntimeTXTable;
#else
__thread NormObjKey normRuntimeTXTable;
#endif
int dynamic_tx_used = 0;
//As we cant replace the special characters in original constant string so we are using static buffer to store the replaced string
//Donot use this buffer for any other purpose as it stores the transaction name
static char tx_name_buffer[1024 + 1];

#define TX_STATUS_NOT_SET    255

#define TX_CLICK_AWAY_STATUS_SUCCESS 0  
#define TX_CLICK_AWAY_STATUS_CLICKAWAY 1

#define CHECK_SET_TX_NAME_LEN(vptr, name, len) {\
  len = strlen(name); \
  if(len > global_settings->max_dyn_tx_name_len ){ \
    len = global_settings->max_dyn_tx_name_len; \
  }\
}

// TODO - reivew and check why print_core_events Vs NS_EL_2_ATTR
#define CHECK_IF_ANY_TX_USED(api_name) \
{ \
  if(tx_hash_func == NULL) \
  {  \
    print_core_events((char *)__FUNCTION__, __FILE__, "Transaction API \"%s\" called with no transaction used in this test", api_name); \
    return NS_NO_TX_RUNNING; \
  } \
  else \
  { \
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index, \
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL, \
                                  vptr->sess_ptr->sess_name, \
                                  name, \
                                  "Invalid transaction name passed in start transaction API. Transaction name = %s", name); \
    return NS_TX_BAD_TX_NAME; \
  } \
} 
 
#define SET_TX_STATUS_BASED_ON_PAGE_STATUS(lol_vptr, lol_node_ptr, lol_status)  \
{  \
  if(lol_status != NS_REQUEST_CLICKAWAY) \
  { \
    lol_node_ptr->status = lol_status; \
  } \
  else \
  { \
    PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL; \
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[lol_vptr->group_num].page_clickaway_table[lol_vptr->cur_page->page_number]; \
    if (pageclickaway_ptr->transaction_status == TX_CLICK_AWAY_STATUS_SUCCESS) \
      lol_node_ptr->status = NS_REQUEST_OK; \
    else \
      lol_node_ptr->status = lol_status; \
  } \
}

/* For C Type script, following design changes are done:
   1. Start time of tx is set on transaction API is called as opposed to when page is started
   2. Transaciton is closed on calling on end API. So removed is_done flag
   3. ns_end_transaction and ns_end_transaction_as will return status of tx on success of API
   4. ns_set_tx_status() will return status on success of API
   5. Added different return codes of API
*/

// static int tx_info_size;
// static int tx_page_id_bit_mask_size;

// Parent will call this
// void tx_init()
// {
  // tx_page_id_bit_mask_size = g_cur_page/32 + 0 or 1;
  // tx_info_size = sizeof (TxInfo) + (tx_page_id_bit_mask_size - 1) * sizeof(int);
// }
//
// START - Utililty Functions used withing this file

// Fuction for adding a node at the front end of the link list TxInfo (head is tx_info_str_ptr),
// It will update the head of the link list, now "cur_vptr->tx_info_ptr" vl point to updated adress of LL (link list).
// flag specifies that node is of API or of pg_as_tx
inline void tx_add_node (int hash_code, VUser *vptr, int flag, u_ns_ts_t now)
{
  TxInfo *node_ptr;
  int tx_node_size;

  NSDL4_TRANS(vptr, NULL, "Method called, hash_code = %d, flag = %d, now = %u", hash_code, flag, now); 

  tx_node_size = sizeof (TxInfo) + (runprof_table_shr_mem[vptr->group_num].gset.max_pages_per_tx - 1) * sizeof (short);
  MY_MALLOC(node_ptr, tx_node_size, "tx_add_node() - Transaction node (TxInfo)", -1);  // third argument in "Malloc"for printing the error, not an inbuilt function

  node_ptr->next = (TxInfo *) vptr->tx_info_ptr;       // setting the pointer of the new node as the head in the VUser
  vptr->tx_info_ptr = (char *) node_ptr;
 
  // Intializing the contents of node
  node_ptr->hash_code = hash_code;
  node_ptr->begin_at = now;
  node_ptr->status = TX_STATUS_NOT_SET;
  node_ptr->think_duration = 0;
  node_ptr->api_or_pg = flag;
  node_ptr->num_pages = 0;
  node_ptr->tx_tx_bytes = 0;
  node_ptr->tx_rx_bytes = 0;

  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
  {
    node_ptr->rbu_tx_time = -1;
    node_ptr->wasted_time = 0;
  }
  else
    node_ptr->rbu_tx_time = 0;

  vptr->tx_instance++;   // incrementing the tx entry in vptr to assign unique instance for this transaction within this session
  node_ptr->instance = vptr->tx_instance;        // making the unique id for each tx

  NSDL3_TRANS(vptr, NULL, "Before: Starting Tx: "
                          "tx_info_ptr = %p, hash_code = %d, begin_at = %u, "
                          "tx_instance = %hd, Total tx_fetches_started = %llu, "
                          "Total tx_fetches_started for this Tx = %d",
                          vptr->tx_info_ptr, node_ptr->hash_code, node_ptr->begin_at,
                          node_ptr->instance, average_time->tx_fetches_started,
                          txData[node_ptr->hash_code].tx_fetches_started);

  average_time->tx_fetches_started++;
  txData[hash_code].tx_fetches_started++;
 
  NSDL3_TRANS(vptr, NULL, "hash_code = %d", hash_code);

  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->tx_fetches_started++;
  }
  NSDL3_TRANS(vptr, NULL, "Starting Tx: "
                          "hash_code = %d, begin_at = %u, "
                          "tx_instance = %hd, Total tx_fetches_started = %llu, "
                          "Total tx_fetches_started for this Tx = %d",
                          node_ptr->hash_code, node_ptr->begin_at,
                          node_ptr->instance, average_time->tx_fetches_started,
                          txData[node_ptr->hash_code].tx_fetches_started);
}

// Function for seaching the node in the TxInfo link list through hash code, called by the tx_start_with_hash ()
// Return value = NULL, if no node is found with the given hash code
// Other wise will return the pointer to the particular node
static inline TxInfo *tx_get_node_by_hash_code_and_tx_instnace (char *name, int hash_code, VUser *vptr, TxInfo **prev_node, int tx_inst)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;

  NSDL3_TRANS(vptr, NULL, "Method called. name = %s, hash_code = %d", name, hash_code); 

  *prev_node = NULL; // Means it is head
 
  while (node_ptr != NULL)
  {
    if (hash_code == node_ptr->hash_code && (tx_inst == node_ptr->instance)) return node_ptr;
    *prev_node = node_ptr; // Save previous node. Used for deleting
    node_ptr = node_ptr->next;
  }
  // can not log this as error this will always for come end_as API
  NSDL4_TRANS(vptr, NULL, "tx_get_node_by_hash_code() - TxInfo node not found in the list of running transactions linked list. Transaction name = %s", name);
  return NULL;
}

// Function for seaching the node in the TxInfo link list through hash code, called by the tx_start_with_hash ()
// Return value = NULL, if no node is found with the given hash code
// Other wise will return the pointer to the particular node
static inline TxInfo *tx_get_node_by_hash_code (char *name, int hash_code, VUser *vptr, TxInfo **prev_node)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;

  NSDL3_TRANS(vptr, NULL, "Method called. name = %s, hash_code = %d", name, hash_code); 

  *prev_node = NULL; // Means it is head
 
  while (node_ptr != NULL)
  {
    if (hash_code == node_ptr->hash_code) return node_ptr;
    *prev_node = node_ptr; // Save previous node. Used for deleting
    node_ptr = node_ptr->next;
  }
  // can not log this as error this will always for come end_as API
  NSDL4_TRANS(vptr, NULL, "tx_get_node_by_hash_code() - TxInfo node not found in the list of running transactions linked list. Transaction name = %s", name);
  return NULL;
}

#if 0
// It will return the node pointer if hash code found, else returns NULL, This fn is also called from tx_end_pg_as_trans (), ns_trans_parse.c
static inline TxInfo *tx_get_node_by_name (char *name, VUser *vptr, TxInfo **prev_node)
{
  int hash_code = -1;

  NSDL3_TRANS(vptr, NULL, "Method called, name = %s", name); 

  if((hash_code = tx_hash_func (name, strlen(name))) < 0)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "tx_get_node_by_name() - Hash code not found for the transaction. Transaction name = %s", name);
    return NULL;
  }
  return (tx_get_node_by_hash_code(name, hash_code, vptr, prev_node));
}
#endif

// Function for starting the tx with the hash code,
// It with return void since it alwaz start the transaction with the given hash_code
// flag specifies that node is of API(0) or of pg_as_tx(1)
inline int tx_start_with_hash_code (char *name, int hash_code, VUser *vptr, int flag)
{
  TxInfo *node_ptr;
  TxInfo *prev_node;
  
  NSDL3_TRANS(vptr, NULL, "Method called, name = %s, hash_code = %d, flag = %d", name, hash_code, flag);

  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Starting execution of transaction '%s'", name);

  node_ptr = tx_get_node_by_hash_code(name, hash_code, vptr, &prev_node);

  if (node_ptr != NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Running transaction is started again. Transaction name = %s", name);
    return NS_TX_RUNNING;
  }

  u_ns_ts_t now = get_ms_stamp();

  tx_add_node(hash_code, vptr, flag, now);
  
  return NS_TX_SUCCESS;
}


// This fn for setting the status to the tx, pointed by node_ptr
// Return status (>=0) - if sucessfull to set the status
// Else return -1
static inline int tx_set_status(TxInfo *node_ptr, int status, VUser *vptr, char *tx_name)
{
  NSDL3_TRANS(NULL, NULL, "Method called, hash_code = %d, status = %d, node_ptr->status = %d, vptr->page_status = %d", 
                           node_ptr->hash_code, status, node_ptr->status, vptr->page_status);

   if(status == NS_AUTO_STATUS) {
   // Set status based on page status  
   // If tran status is already set, then we should not change
    // if (node_ptr->status == NS_REQUEST_OK || node_ptr->status == 255)
    if (node_ptr->status == TX_STATUS_NOT_SET )
    {
      if(vptr->first_page_url && (vptr->first_page_url->request_type == WS_REQUEST || vptr->first_page_url->request_type == WSS_REQUEST))
      {
        NSDL3_TRANS(NULL, NULL, "request_type = %d, ws_status = %d", vptr->first_page_url->request_type, vptr->ws_status);
        node_ptr->status = vptr->ws_status;
      }
      else
        node_ptr->status = vptr->page_status;
    }
    else
      NSDL3_TRANS(NULL, NULL, "Tx status already set with some error for auto status, so not changing it. status = %d, node_ptr->status = %d",
                               status, node_ptr->status);

    return node_ptr->status;
  }

   // Following code will be removed in 3.5.1 once we migrate MAcys scripts
  if(status == NS_REQUEST_OK) {
    // if trans status is already filled will error we will not set status
    if (node_ptr->status == TX_STATUS_NOT_SET)
    {
      node_ptr->status = status;
    }
    NSDL3_TRANS(NULL, NULL, "status = %d, node_ptr->status = %d", status, node_ptr->status);
    return node_ptr->status;
  }

  if((status < TOTAL_TX_ERR) && (status >= TOTAL_TX_ERR - USER_DEF_TX_ERR)) {
    if (node_ptr->status == TX_STATUS_NOT_SET)
      node_ptr->status = status;
    NSDL3_TRANS(NULL, NULL, "status = %d, node_ptr->status = %d", status, node_ptr->status);
    return node_ptr->status;
  }

  if((status == NS_TX_CV_FAIL) && 
       ((global_settings->protocol_enabled & RBU_API_USED ) && (vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)))
  {
    NSDL3_TRANS(NULL, NULL, "We are here to set Transaction Fail for RBU with JTS");
    if (node_ptr->status == TX_STATUS_NOT_SET)
      node_ptr->status = status;

    NSDL3_TRANS(NULL, NULL, "status = %d, node_ptr->status = %d", status, node_ptr->status);
    return node_ptr->status;
  }
 
  // Adarsh(bug id-70763) : Adding transaction error for JMS, (NS_REQUEST_ERRMISC = 1) 
  if((status == NS_REQUEST_ERRMISC) && (global_settings->protocol_enabled & JMS_PROTOCOL_ENABLED)) 
  {
    NSDL3_TRANS(NULL, NULL, "Setting Transaction fail for JMS");
    if(node_ptr->status == TX_STATUS_NOT_SET)
      node_ptr->status = status;   
    NSDL3_TRANS(NULL, NULL, "status = %d, node_ptr->status = %d", status, node_ptr->status);
    return node_ptr->status; 
  } 

  //Shikha : Code was duplicated, So, removing the second lag

  NS_EL_2_ATTR(EID_TRANS, vptr->user_index,
                          vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                          vptr->sess_ptr->sess_name, 
                          tx_name,
                          "Invalid status pass in the transaction API. Transaction name = %s, status = %d", tx_name, status);
  NSDL3_TRANS(NULL, NULL, " invalid status = %d, current status = %d", status, node_ptr->status);
  return NS_TX_ERROR;
}

// Delete node from the linked list
static inline void tx_delete_node (VUser *vptr, TxInfo *node_ptr, TxInfo *prev_node)
{
  NSDL3_TRANS(vptr, NULL, "Method called. vptr->tx_info_ptr = %p, node_ptr = %p, prev_node = %p, hash_code = %d", vptr->tx_info_ptr, node_ptr, prev_node, node_ptr->hash_code);  

  // if(prev_node == NULL) // Deleting head node
  if((void *)node_ptr == vptr->tx_info_ptr) // Deleting head node
  {
    NSDL3_TRANS(vptr, NULL, "Deleting head node");
    //(TxInfo *) vptr->tx_info_ptr = node_ptr->next;
    vptr->tx_info_ptr = (char *)node_ptr->next;
  }
  else
  {
    NSDL3_TRANS(vptr, NULL, "Deleting non head node");
    prev_node->next = node_ptr->next;
  }
  FREE_AND_MAKE_NOT_NULL(node_ptr, "node_ptr", -1);
}

// This function will be called from tx logging on session completion () and from tx logging on page completion ()
static inline void do_tx_data_logging(VUser *vptr, TxInfo *node_ptr, u_ns_ts_t now, TxInfo *prev_node)
{
  unsigned int download_time = 0;
  unsigned int sq_download_time = 0;//TODO : move calculation of sq_download_time above
  int hash_code = node_ptr->hash_code;

  NSDL3_TRANS(vptr, NULL, "Method called, hash code = %d", node_ptr->hash_code);

  // excluding the failed page & tx statistics. Refer the debug log below for more detail
  if(NS_EXCLUDE_STOPPED_STATS_FROM_PAGE_TX || NS_EXCLUDE_STOPPED_STATS_FROM_PAGE_TX_USEONCE)
  {
    NSDL2_SCHEDULE(NULL, vptr, "exclude_stopped_stats is on and page status is stopped, hence excluding the stopped stats from page dump, hits, drilldown database, response time & tracing");
    tx_delete_node(vptr, node_ptr, prev_node);
    return;
  }
  average_time->tx_fetches_completed++;
  txData[hash_code].tx_fetches_completed++;

  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->tx_fetches_completed++;
  }

  if (node_ptr->status)
  {
    if ((node_ptr->status > 0) && (node_ptr->status < TOTAL_TX_ERR)) {
      average_time->tx_error_codes[node_ptr->status]++;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_error_codes[node_ptr->status]++;
      }
    }
    else
    {
      NS_EL_1_ATTR(EID_TRANS, vptr->user_index,
                          vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                          vptr->sess_ptr->sess_name,
                          "Invalid transaction status. Status = %d", node_ptr->status);
      average_time->tx_error_codes[NS_REQUEST_ERRMISC]++;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_error_codes[NS_REQUEST_ERRMISC]++;
      }
    }
  }

  //if (!node_ptr->status || !global_settings->exclude_failed_agg)
  if((!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) || (vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA))
  {
    //It may be possible that now is less then the begin_at.
    //It can be happen when we have write fail.
    if(now > (node_ptr->begin_at))
    {
      download_time = now - node_ptr->begin_at;
      /* To get the actual time taken by websocket_Read this code has been commented */
     /* if(download_time > node_ptr->wasted_time)
        download_time -= node_ptr->wasted_time;
      NSDL2_TRANS(vptr, NULL, "Tx download_time = %u, wasted_time = %d, now = %lld, node_ptr->begin_at = %lld", 
                               download_time, node_ptr->wasted_time, now, node_ptr->begin_at); */
      NSDL2_TRANS(vptr, NULL, "Tx download_time = %u, now = %lld, node_ptr->begin_at = %lld",
                               download_time, now, node_ptr->begin_at);
    }
    else
      download_time = 0;
  }
  else
  {
    NSDL2_TRANS(vptr, NULL, "RBU - Tx download_time = %ld", node_ptr->rbu_tx_time);
    download_time = node_ptr->rbu_tx_time + node_ptr->think_duration;
  }
  // Computing sq_download_time
  sq_download_time = download_time * download_time;
  if (!node_ptr->status)  //Success
  {
    average_time->tx_succ_fetches++;
    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->tx_succ_fetches++;

      SET_MIN (lol_average_time->tx_succ_min_resp_time, download_time);
      SET_MAX (lol_average_time->tx_succ_max_resp_time, download_time);
      lol_average_time->tx_succ_tot_resp_time += download_time;
      
    }
    txData[hash_code].tx_succ_fetches++;
    SET_MIN (txData[hash_code].tx_succ_min_time, download_time);
    SET_MAX (txData[hash_code].tx_succ_max_time, download_time);
  
    txData[hash_code].tx_succ_tot_time += download_time;
    txData[hash_code].tx_succ_tot_sqr_time += sq_download_time;
  
    SET_MIN (average_time->tx_succ_min_resp_time, download_time);
    SET_MAX (average_time->tx_succ_max_resp_time, download_time);
    average_time->tx_succ_tot_resp_time += download_time;

    NSDL4_TRANS(NULL, NULL, "Tx Success : average_time->tx_succ_min_resp_time - %d, average_time->tx_succ_max_resp_time - %d, "
                            " average_time->tx_succ_tot_resp_time - %d, download_time - %d",
                            average_time->tx_succ_min_resp_time, average_time->tx_succ_max_resp_time, 
                            average_time->tx_succ_tot_resp_time, download_time);
  
  }
  else  //Failure
  {
    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));

      SET_MIN (lol_average_time->tx_fail_min_resp_time, download_time);
      SET_MAX (lol_average_time->tx_fail_max_resp_time, download_time);
      lol_average_time->tx_fail_tot_resp_time += download_time;
    }

    SET_MIN (txData[hash_code].tx_failure_min_time, download_time);
    SET_MAX (txData[hash_code].tx_failure_max_time, download_time);
  
    txData[hash_code].tx_failure_tot_time += download_time;
    txData[hash_code].tx_failure_tot_sqr_time += sq_download_time;
  
    SET_MIN (average_time->tx_fail_min_resp_time, download_time);
    SET_MAX (average_time->tx_fail_max_resp_time, download_time);
    average_time->tx_fail_tot_resp_time += download_time;

    NSDL4_TRANS(NULL, NULL, "Tx Failure : average_time->tx_fail_min_resp_time - %d, average_time->tx_fail_max_resp_time - %d, "
                            " average_time->tx_fail_tot_resp_time - %d",
                            average_time->tx_fail_min_resp_time, average_time->tx_fail_max_resp_time, average_time->tx_fail_tot_resp_time);
  }
  
  NSDL2_TRANS(NULL, NULL, "Succ/Fail download_time = %d, tx_succ_min_time = %d, tx_succ_max_time = %d, tx_succ_tot_time = %llu, tx_succ_tot_sqr_time = %llu", download_time, txData[hash_code].tx_succ_min_time, txData[hash_code].tx_succ_max_time, txData[hash_code].tx_succ_tot_time, txData[hash_code].tx_succ_tot_sqr_time);


  //Overall  
  SET_MIN (average_time->tx_min_time, download_time);
  SET_MAX (average_time->tx_max_time, download_time);
  average_time->tx_tot_time += download_time;
  
  //sq_download_time = download_time*download_time;
  average_time->tx_tot_sqr_time += sq_download_time;

  SET_MIN (txData[hash_code].tx_min_time, download_time);
  SET_MAX (txData[hash_code].tx_max_time, download_time);
  
  txData[hash_code].tx_tot_time += download_time;
  txData[hash_code].tx_tot_sqr_time += sq_download_time;
  
  NSDL2_TRANS(NULL, NULL, "Overall_before: download_time = %d, tx_min_time = %d, tx_max_time = %d, tx_tot_time = %llu, tx_tot_sqr_time = %llu , txData[%d].tx_min_time = %d, txData[hash_code] = %p", download_time, average_time->tx_min_time, average_time->tx_max_time, average_time->tx_tot_time, average_time->tx_tot_sqr_time, hash_code, txData[hash_code].tx_min_time, &txData[hash_code].tx_min_time);

  //Think Time
  SET_MIN (average_time->tx_min_think_time, node_ptr->think_duration);
  SET_MAX (average_time->tx_max_think_time, node_ptr->think_duration);
  average_time->tx_tot_think_time += node_ptr->think_duration;
  average_time->tx_tx_bytes += node_ptr->tx_tx_bytes;
  average_time->tx_rx_bytes += node_ptr->tx_rx_bytes;
 
  SET_MIN (txData[hash_code].tx_min_think_time, node_ptr->think_duration);
  SET_MAX (txData[hash_code].tx_max_think_time, node_ptr->think_duration);
  txData[hash_code].tx_tot_think_time += node_ptr->think_duration;
  txData[hash_code].tx_tx_bytes += node_ptr->tx_tx_bytes;
  txData[hash_code].tx_rx_bytes += node_ptr->tx_rx_bytes;
  
  NSDL2_TRANS(NULL, NULL, "Overall: download_time = %d, tx_min_time = %d, tx_max_time = %d, tx_tot_time = %llu, tx_tot_sqr_time = %llu , txData[%d].tx_min_time = %d ", download_time, average_time->tx_min_time, average_time->tx_max_time, average_time->tx_tot_time, average_time->tx_tot_sqr_time, hash_code, txData[hash_code].tx_min_time);
  
  NSDL2_TRANS(NULL, NULL, "Overall: Page think_time = %d, tx_min_think_time = %d, tx_max_think_time = %d, tx_tot_think_time = %llu, "
                          "txData for hashcode - [%d] : tx_min_think_time = %d, tx_max_think_time = %d, tx_tot_think_time = %llu", 
                           node_ptr->think_duration, average_time->tx_min_think_time, average_time->tx_max_think_time, 
                           average_time->tx_tot_think_time, hash_code, txData[hash_code].tx_min_think_time, 
                           txData[hash_code].tx_max_think_time, txData[hash_code].tx_tot_think_time);
  
  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    SET_MIN (lol_average_time->tx_min_time, download_time);
    SET_MAX (lol_average_time->tx_max_time, download_time);
    lol_average_time->tx_tot_time += download_time;
    lol_average_time->tx_tot_sqr_time += sq_download_time;
    NSDL2_TRANS(NULL, NULL, "Groupbased: group_num = %d, download_time = %d, tx_min_time = %d, tx_max_time = %d, tx_tot_time = %llu, tx_tot_sqr_time = %llu", (vptr->group_num + 1), download_time, lol_average_time->tx_min_time, lol_average_time->tx_max_time, lol_average_time->tx_tot_time, lol_average_time->tx_tot_sqr_time);
  
    //THINK TIME
    SET_MIN (lol_average_time->tx_min_think_time, node_ptr->think_duration);
    SET_MAX (lol_average_time->tx_max_think_time, node_ptr->think_duration);
    lol_average_time->tx_tot_think_time += node_ptr->think_duration;
    lol_average_time->tx_tx_bytes += node_ptr->tx_tx_bytes;
    lol_average_time->tx_rx_bytes += node_ptr->tx_rx_bytes;

    NSDL2_TRANS(NULL, NULL, "Groupbased Think Time: group_num = %d, think time = %d, tx_min_think_time = %d,"
                            " tx_max_think_time = %d, tx_tot_think_time = %llu", 
                            (vptr->group_num + 1), node_ptr->think_duration, lol_average_time->tx_min_think_time, 
                            lol_average_time->tx_max_think_time, lol_average_time->tx_tot_think_time);
  }
  
  
  if (g_percentile_report == 1) {
    update_pdf_data(download_time, pdf_transaction_time, 0, 0, hash_code);
    update_pdf_data(download_time, pdf_average_transaction_response_time, 0, 0, 0);
  }
  
  NSDL3_TRANS(vptr, NULL, "Ending Sucessful Tx hash_code = %d, download_time = %u, Overall(tx_fetches_completed = %lld, tx_succ_fetches = %lld, min = %lld, max = %lld, total = %lld, tx_tot_sqr_time = %llu), This Tx(tx_fetches_completed = %u, tx_succ_fetches = %u, min = %u, max = %u, total = %lld, tx_tot_sqr_time = %llu, Avg : tx_tot_think_time - %d, TxData tx_tot_think_time = %d)", \
      node_ptr->hash_code, download_time, \
      average_time->tx_fetches_completed, average_time->tx_succ_fetches, average_time->tx_min_time, average_time->tx_max_time, average_time->tx_tot_time, average_time->tx_tot_sqr_time, \
      txData[hash_code].tx_fetches_completed, txData[hash_code].tx_succ_fetches, txData[hash_code].tx_min_time, txData[hash_code].tx_max_time, txData[hash_code].tx_tot_time, txData[hash_code].tx_tot_sqr_time);

  //If NS_RESP_NETCACHE flag enable then increment netcache_fetches
  UPD_TX_NW_CACHE_USED_STATS(vptr, hash_code, download_time, sq_download_time);

  //Currently not doing logging of tx as it needs page status but for url level tx 
  //we dont have page status so skipping tx logging for embd urls
  if(LOG_LEVEL_FOR_DRILL_DOWN_REPORT && (node_ptr->api_or_pg != NS_TX_IS_INLINE))
  {
    NSDL3_TRANS(vptr, NULL, "Call log_tx_record_v2 function");
    #ifndef CAV_MAIN
    if(node_ptr->num_pages) // Do only if any pages…
      log_tx_record_v2(vptr, now, node_ptr);
    #endif
  }
  tx_delete_node(vptr, node_ptr, prev_node);
}


// This fn is for changing the hash_code of the node_ptr for end_as and for setting the related variable of txData
// This the couters used in this fn are used for intiated trasactions
static inline void tx_change_hash_code (TxInfo *node_ptr, int end_hash_code)
{
#ifdef NS_DEBUG_ON
int start_hash_code = node_ptr->hash_code;
#endif

  NSDL3_TRANS(NULL, NULL, "current_hash_code = %d, end_hash_code = %d, tx_fetches_started (old) = %d, tx_fetches_started (new) = %d", node_ptr->hash_code, end_hash_code, txData[node_ptr->hash_code].tx_fetches_started, txData[end_hash_code].tx_fetches_started);

  txData[node_ptr->hash_code].tx_fetches_started--;
  node_ptr->hash_code = end_hash_code;
  txData[node_ptr->hash_code].tx_fetches_started++;

  NSDL4_TRANS(NULL, NULL, "After change: current_hash_code = %d, end_hash_code = %d, tx_fetches_started (old) = %d, tx_fetches_started (new) = %d", start_hash_code, end_hash_code, txData[start_hash_code].tx_fetches_started, txData[end_hash_code].tx_fetches_started);
}

// END - Utililty Functions used withing this file
/******************************************************************/

/******************************************************************/
// Start - Function for starting and ending of PAGE_AS_TRANSACTION

// For ending the pg_as_tx
static inline void tx_end_pg_as_trans (VUser* vptr, char *tx_name, TxInfo *node_ptr, int status)
{
  NSDL3_TRANS(vptr, NULL, "Method called, hash_code = %d, status = %d", node_ptr->hash_code, status);

  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Completed execution of transaction '%s' with status '%s' (status code = %d", tx_name, ns_get_status_name(status), status);
  node_ptr->status = status;
}

// For ending the pg_as_tx with the different name
static inline void tx_end_as_pg_as_trans(VUser* vptr, char *tx_name, TxInfo *node_ptr, char *end_name, int end_hash_code, int status)
{
  TxInfo *end_name_node_ptr;
  TxInfo *prev_node;

  NSDL3_TRANS(vptr, NULL, "Method called, current_hash_code = %d, end_name = %s, end_hash_code = %d, status = %d", node_ptr->hash_code, end_name, end_hash_code, status); 

  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Completed execution of transaction '%s' as '%s' with status '%s' (status code = %d)", tx_name, end_name, ns_get_status_name(status), status);

  if((end_name_node_ptr = tx_get_node_by_hash_code (end_name, end_hash_code, vptr, &prev_node)) != NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  end_name,
                                  "Page as transaction to be ended as different transaction is already running. Transaction name = %s, Running end as transaction name = %s", tx_name, end_name);
    return;
  }

  tx_change_hash_code (node_ptr, end_hash_code);

  SET_TX_STATUS_BASED_ON_PAGE_STATUS(vptr, node_ptr, status)
  //node_ptr->status = status;
}

// This fn will called from the tx_begin_on_page_start () ns_trans.c
static inline void tx_start_pg_as_trans(VUser *vptr, u_ns_ts_t now)
{
  PageTableEntry_Shr *pg_table_ptr;
  char *tx_name;

  pg_table_ptr = vptr->cur_page;
  tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, pg_table_ptr->tx_table_idx);

  NSDL1_TRANS(vptr, NULL, "Method called"); 

  // For starting the main tx of the page. For all modes, we use main tx
  if(tx_start_with_hash_code (tx_name, tx_table_shr_mem[pg_table_ptr->tx_table_idx].tx_hash_idx, vptr, NS_TX_IS_PAGE_AS) == -1)
  {
    // Note event is alreay logged in the above method
    NSDL1_TRANS(vptr, NULL, "Error: tx_start_pg_as_trans() - Error in starting the main transaction of page, transaction ignored, Transaction name %s", tx_name);
  }
}

// This fn is for ending the pg_as_tx. Called from ns_trans.c
// Flag 0 - fn called from tx_logging_on_page_completion()
// Flag 1 - fn called from tx logging on session completion()
// Flag is not used
static inline void tx_end_for_pg_as_trans (TxInfo *node_ptr, VUser *vptr, int flag, int tx_status)
{
  PageTableEntry_Shr *pg_table_ptr = vptr->cur_page;
  char *end_name = NULL;
  char *tx_name;
  int  end_hash_code; // hash code of end as tx

  NSDL3_TRANS(vptr, NULL, "Method called, hash_code = %d, flag = %d", node_ptr->hash_code, flag); 

  tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, pg_table_ptr->tx_table_idx);

  if (global_settings->pg_as_tx == 1) // End with same name
  {
    tx_end_pg_as_trans (vptr, tx_name, node_ptr, tx_status);
    return;
  }

  if (global_settings->pg_as_tx == 2) // End with Success or Fail
  {
    if(tx_status == 0)  // 0 means success
    {
      // Get TX Name and hash code of sucess transaction from txTable.
      // Sucess Name was added one after main tx
      end_name = RETRIEVE_SHARED_BUFFER_DATA(txTable[(pg_table_ptr->tx_table_idx) + 1].tx_name);
      end_hash_code = txTable[(pg_table_ptr->tx_table_idx) + 1].tx_hash_idx;
      tx_end_as_pg_as_trans (vptr, tx_name, node_ptr, end_name, end_hash_code, tx_status);
      return;
    }
    else
    {
      // Get TX Name and hash code of Fail transaction from txTable.
      // Fail Name was added two after main tx
      end_name = RETRIEVE_SHARED_BUFFER_DATA(txTable[(pg_table_ptr->tx_table_idx) + 2].tx_name);
      end_hash_code = txTable[(pg_table_ptr->tx_table_idx) + 2].tx_hash_idx;
      tx_end_as_pg_as_trans (vptr, tx_name, node_ptr, end_name, end_hash_code, tx_status);
      return;
    }
  } //end with the succ or fail

  if (global_settings->pg_as_tx == 3) // End based on pg status
  {
    // Get TX Name and hash code of transaction based on pg status from txTable.
    // Status based Name was added using pg status in txTable
    end_name = RETRIEVE_SHARED_BUFFER_DATA(txTable[(pg_table_ptr->tx_table_idx) + vptr->page_status + 1].tx_name);
    tx_end_as_pg_as_trans (vptr, tx_name, node_ptr, end_name, txTable[(pg_table_ptr->tx_table_idx) + vptr->page_status + 1].tx_hash_idx, tx_status);
    return;
  }
}

// End - Function for starting and ending of PAGE_AS_TRANSACTION
/*********************************************************************/

// End - Utililty Functions used withing this file
/******************************************************************************************************/

/*********************************************************************/
// START: Functions which are called from other source files of netstorm.(add prototype in ns_trans.h)

// This fn will be called from "on_page_start()" ns_page.c (line 1369)
inline void tx_begin_on_page_start(VUser *vptr, u_ns_ts_t now)
{
  // Intialization of the node_ptr will be done after adding the node, since in the begining node_ptr may be NULL
  // TxInfo *node_ptr;
  char *tx_name;

  NSDL3_TRANS(vptr, NULL, "Method called"); 

  if (global_settings->pg_as_tx != 0)
    tx_start_pg_as_trans (vptr, now);  // It will start the page as tx


  // Add this page instance in page in all transactions in the linked list
  // For all nodes in linked list, add this page instance
  
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
  while(node_ptr)
  {
    NSDL4_TRANS(vptr, NULL, "node_ptr = %p", node_ptr);
    tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code);
 
    if(node_ptr->num_pages >= runprof_table_shr_mem[vptr->group_num].gset.max_pages_per_tx)
    { 
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,__FILE__, (char *)__FUNCTION__,
                                    "Number of page instances in transaction %s are more than maximum page instances (%d) allowed per transaction. Additional pages will not be reported in transaction drill down reports. This limit can be increased by changing maximum page instances allowed per transaction setting of scenario group", tx_name, runprof_table_shr_mem[vptr->group_num].gset.max_pages_per_tx); 

      node_ptr = node_ptr->next;
      continue;
    }
    else
    { 
      node_ptr->page_instance[node_ptr->num_pages] = vptr->page_instance;  

      NSDL2_TRANS(vptr, NULL, "Added page instance %hd (page_id = %d) at index %d for transaction %s", vptr->page_instance, vptr->cur_page->page_id, node_ptr->num_pages, tx_name);
 
      node_ptr->num_pages++;
    }
    node_ptr = node_ptr->next;
  }
}

//Logging tx sent and recieve throughput based on page complete.
inline void tx_logging_on_url_completion (VUser *vptr, connection *cptr, u_ns_ts_t now)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
  
  NSDL3_TRANS(vptr, NULL, "Method called. %p", node_ptr);
  
  while (node_ptr != NULL)
  {
    NSDL3_TRANS(vptr, NULL, "Updating tcp_byte_sent and tcp_bytes_recv of a transaction, tx_name = %s, hash code = %d, bytes_sent = %ld,"
                             "byte_recv = %d", nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code), node_ptr->hash_code,
                              cptr->tcp_bytes_sent, cptr->tcp_bytes_recv);
    node_ptr->tx_tx_bytes += cptr->tcp_bytes_sent;
    node_ptr->tx_rx_bytes += cptr->tcp_bytes_recv;
    node_ptr = node_ptr->next;
  }
}


// For setting the manipulating the fields of transaction at the time of end, called from the close_connection (), netstorm.c
// It is called when page is complete (urls_awaited = 0)
// pg_status is already filled
inline void tx_logging_on_page_completion (VUser *vptr, u_ns_ts_t now)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
  TxInfo *prev_node = NULL;
  TxInfo *next_node = NULL;

  unsigned short har_timeout;

  NSDL3_TRANS(vptr, NULL, "Method called");

  if(vptr->page_status == NS_REQUEST_RELOAD)
  { 
    NSDL3_TRANS(vptr, NULL, "Page is going to reload so do not log.");
    return;
  }

  // Only page as Tx need to be closed here
  // API based transaction will be closed by API
  while (node_ptr != NULL)
  {
    next_node = node_ptr->next;
    
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
    {
      HAR_TIMEOUT
      NSDL3_TRANS(vptr, NULL, "Adding on load time %d to tarnaction node node_ptr %p, rbu_tx_time = %d, next_node = %p"
                              ", page_load_time = %d , wait_time = %d", 
                               vptr->httpData->rbu_resp_attr->on_load_time, node_ptr, node_ptr->rbu_tx_time, next_node,
                               vptr->httpData->rbu_resp_attr->page_load_time, har_timeout);
      //Commented: Bug Id-18022: In Click and Script(link API), we dont capture onload time, so Transaction time is zero for that.
      //But Transaction is successfull, so keeping transaction time as page load(Over All) time.
      //node_ptr->rbu_tx_time += (vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0; 
      node_ptr->rbu_tx_time += (vptr->httpData->rbu_resp_attr->page_load_time > 1) ? 
                                vptr->httpData->rbu_resp_attr->page_load_time :
                                har_timeout * 1000;

      NSDL3_TRANS(vptr, NULL, "rbu_tx_time = %d", node_ptr->rbu_tx_time);
    }

    if (node_ptr->api_or_pg == NS_TX_IS_PAGE_AS) // If node was for API, return
    {
      NSDL3_TRANS(vptr, NULL, "Processing page as tx. hash code = %d", node_ptr->hash_code);
      tx_end_for_pg_as_trans (node_ptr, vptr, 0, vptr->page_status);
      do_tx_data_logging(vptr, node_ptr, now, prev_node); // It will free node_ptr
    }
    else
    {
      NSDL3_TRANS(vptr, NULL, "Skipping non page as tx. hash code = %d", node_ptr->hash_code);
      prev_node = node_ptr; // Update prev only if not freed
    }
    node_ptr = next_node;
  }
}


// This fn will be called from the on_session_completion (), netstorm.c
// status - status of last page
inline void tx_logging_on_session_completion (VUser *vptr, u_ns_ts_t now, int status)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
  NSDL2_TRANS(vptr, NULL, "node_ptr = %p, vptr->tx_info_ptr = %p", node_ptr, vptr->tx_info_ptr);
  TxInfo *node_next = NULL;
  char *tx_name;

  NSDL2_TRANS(vptr, NULL, "Method called. status = %d", status);


  while (node_ptr != NULL)
  {
    NSDL3_TRANS(vptr, NULL, "Ending transaction (hash_code = %d)", node_ptr->hash_code);
    tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code);
    node_next = node_ptr->next;
      // At this place page there should not be any page as trans as it is ended in tx_logging_on_session_completion which is called from handle page complete.
      // we are doing it for safty purpose only.   
    if(node_ptr->api_or_pg == NS_TX_IS_PAGE_AS){
      print_core_events((char*)__FUNCTION__, __FILE__, "Ending page as transaction %s here, it should be ended in on page completion",
                                                                                                     tx_name);
      tx_end_for_pg_as_trans (node_ptr, vptr, 1, status);
    }
    if(node_ptr->status == TX_STATUS_NOT_SET) {
      SET_TX_STATUS_BASED_ON_PAGE_STATUS(vptr, node_ptr, status)
    }
      //node_ptr->status = status; // Set status to the status of last page
    
    // Bug 999 - Transaction which is not ended in script.c are passing and not warning given
    // It is not possible to find out if end tx is there or not in case of page failure and continue on page erorr is 0
    // Following cases are possible and if event should be logged or not
    // 			    Status 0	Status != 0 and CPE 0	 Status != 0 and CPE 1
    // 	End API is there     No		  No                         No
    // 	End API is not there YES	  YES                         YES
    //
    // 	Since most common case is that end tx will be there, we commenting this event log
    // 	Bug is moved to Deferred status

#if 0
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,__FILE__, (char *)__FUNCTION__, 
                                    "ns_end_transaction API may not be in the script for the transaction \"%s\". Ending it with last page status (%d)", 
                                    tx_table_shr_mem[node_ptr->hash_code].name, status);

#endif

    NSDL4_TRANS(vptr, NULL, "ns_end_transaction may not be in the script for the transaction \"%s\". Ending it with last page status (%d)", tx_name, status);
    //Reset NS_RESP_NETCACHE flag for last page, 
    //here we do not want to increment Netcache fetches as ns_end_transaction 
    //or ns_end_transaction_as was not called 

    // commenting the below code as we are ending transactions by last page status, so tx netcache stats will also get last page stats, BUG 19101
    /* if (vptr->flags & NS_RESP_NETCACHE)
        vptr->flags &= ~NS_RESP_NETCACHE;     */
    do_tx_data_logging(vptr, node_ptr, now, NULL); // it will free node 
   
    node_ptr = node_next;
  }
}

// This fn will be called from the start_page_think_timer(), ns_page_think_time.c.
inline void add_tx_tot_think_time_node(VUser *vptr)
{
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;

  NSDL3_TRANS(vptr, NULL, "Method called");

  while (node_ptr != NULL)
  {
    node_ptr->think_duration += vptr->timer_ptr->actual_timeout; 
    NSDL3_TRANS(vptr, NULL, "think_duration - %d, node_ptr - %p", node_ptr->think_duration);
    node_ptr = node_ptr->next;
  }
}

// This function will be called from logging.c, two times in the log_page_record() and from the log_url_record()
// This will return hash code of latest transaction if any. Otherwise -1 is returned
inline int tx_get_cur_tx_hash_code(VUser *vptr)
{
TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;

  NSDL4_TRANS(vptr, NULL, "Method called");

  // Check if there is at least one transaction in the list
  if(node_ptr != NULL)
  {
    NSDL4_TRANS(vptr, NULL, "Tx hash code = %d", node_ptr->hash_code);
    return (node_ptr->hash_code);
  }
  NSDL4_TRANS(vptr, NULL, "Return -1 as Tx hash code as no tx running");
  return (-1);
}

int allocate_tx_id_and_send_discovery_msg_to_parent(VUser *vptr, char *data, int data_len)
{
  int flag_new;
  int norm_id;

  NSDL3_TRANS(vptr, NULL, "Method called, tx name = %s tx_len = %d", data, data_len);

  norm_id = nslb_get_or_set_norm_id(&normRuntimeTXTable, data, data_len, &flag_new);

  NSDL3_TRANS(vptr, NULL, "Here dynamic transaction came: flag_new = %d, norm_id = %d", flag_new, norm_id);
  if(flag_new)
  {
    int row_num;
    int old_avg_size = g_avgtime_size;
    create_dynamic_data_avg(&g_dh_child_msg_com_con, &row_num, my_port_index, NEW_OBJECT_DISCOVERY_TX);
    //send local_norm_id to parent
    send_new_object_discovery_record_to_parent(vptr, data_len, data, NEW_OBJECT_DISCOVERY_TX, norm_id); 
    //Check if g_avgtime_size is greater than the size of mccptr_buf_size then realloc connection's read_buf buffer.
    check_if_need_to_realloc_connection_read_buf(&g_dh_child_msg_com_con, my_port_index, old_avg_size, NEW_OBJECT_DISCOVERY_TX);
  }
  else
  {
    NSTL1(vptr, NULL, "Error - It should be always new at this point .. tx_name = %s, tx_nem_len = %d ", data, data_len);
  }
  return norm_id;
}

// END: Functions which are called from other source files of netstorm.(add prototype in ns_trans.h)
/*************************************************************************************************************/

/******************************************************************************************************/
// START: Methods called from Transaction APIs

static void set_dynamic_tx_used(void *vptr, char dtu, char *purpose)
{

  NSTL1(vptr, NULL,  "Setting dynamic transactions used flag to %d from API = %s", dtu, purpose);
  dynamic_tx_used = dtu; // Set so that we use normal hash table next time for all transactions

}
static int VALID_TRANSACTION_VALUE_CHARS[] = {
    0 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
    0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 0 /* HT   */,
    0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
    0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
    0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
    0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
    0 /* RS   */, 0 /* US   */, 0 /* SPC  */, 1 /* !    */, 0 /* "    */,
    0 /* #    */, 1 /* $    */, 0 /* %    */, 1 /* &    */, 0 /* '    */,
    1 /* (    */, 1 /* )    */, 0 /* *    */, 1 /* +    */, 0 /* ,    */,
    1 /* -    */, 1 /* .    */, 1 /* /    */, 1 /* 0    */, 1 /* 1    */,
    1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
    1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 1 /* :    */, 1 /* ;    */,
    0 /* <    */, 1 /* =    */, 0 /* >    */, 1 /* ?    */, 1 /* @    */,
    1 /* A    */, 1 /* B    */, 1 /* C    */, 1 /* D    */, 1 /* E    */,
    1 /* F    */, 1 /* G    */, 1 /* H    */, 1 /* I    */, 1 /* J    */,
    1 /* K    */, 1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
    1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */, 1 /* T    */,
    1 /* U    */, 1 /* V    */, 1 /* W    */, 1 /* X    */, 1 /* Y    */,
    1 /* Z    */, 1 /* [    */, 0 /* \    */, 1 /* ]    */, 0 /* ^    */,
    1 /* _    */, 0 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
    1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
    1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
    1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
    1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
    1 /* x    */, 1 /* y    */, 1 /* z    */, 1 /* {    */, 0 /* |    */,
    1 /* }    */, 0 /* ~    */, 0 /* DEL  */, 0 /* 0x80 */, 0 /* 0x81 */,
    0 /* 0x82 */, 0 /* 0x83 */, 0 /* 0x84 */, 0 /* 0x85 */, 0 /* 0x86 */,
    0 /* 0x87 */, 0 /* 0x88 */, 0 /* 0x89 */, 0 /* 0x8a */, 0 /* 0x8b */,
    0 /* 0x8c */, 0 /* 0x8d */, 0 /* 0x8e */, 0 /* 0x8f */, 0 /* 0x90 */,
    0 /* 0x91 */, 0 /* 0x92 */, 0 /* 0x93 */, 0 /* 0x94 */, 0 /* 0x95 */,
    0 /* 0x96 */, 0 /* 0x97 */, 0 /* 0x98 */, 0 /* 0x99 */, 0 /* 0x9a */,
    0 /* 0x9b */, 0 /* 0x9c */, 0 /* 0x9d */, 0 /* 0x9e */, 0 /* 0x9f */,
    0 /* 0xa0 */, 0 /* 0xa1 */, 0 /* 0xa2 */, 0 /* 0xa3 */, 0 /* 0xa4 */,
    0 /* 0xa5 */, 0 /* 0xa6 */, 0 /* 0xa7 */, 0 /* 0xa8 */, 0 /* 0xa9 */,
    0 /* 0xaa */, 0 /* 0xab */, 0 /* 0xac */, 0 /* 0xad */, 0 /* 0xae */,
    0 /* 0xaf */, 0 /* 0xb0 */, 0 /* 0xb1 */, 0 /* 0xb2 */, 0 /* 0xb3 */,
    0 /* 0xb4 */, 0 /* 0xb5 */, 0 /* 0xb6 */, 0 /* 0xb7 */, 0 /* 0xb8 */,
    0 /* 0xb9 */, 0 /* 0xba */, 0 /* 0xbb */, 0 /* 0xbc */, 0 /* 0xbd */,
    0 /* 0xbe */, 0 /* 0xbf */, 0 /* 0xc0 */, 0 /* 0xc1 */, 0 /* 0xc2 */,
    0 /* 0xc3 */, 0 /* 0xc4 */, 0 /* 0xc5 */, 0 /* 0xc6 */, 0 /* 0xc7 */,
    0 /* 0xc8 */, 0 /* 0xc9 */, 0 /* 0xca */, 0 /* 0xcb */, 0 /* 0xcc */,
    0 /* 0xcd */, 0 /* 0xce */, 0 /* 0xcf */, 0 /* 0xd0 */, 0 /* 0xd1 */,
    0 /* 0xd2 */, 0 /* 0xd3 */, 0 /* 0xd4 */, 0 /* 0xd5 */, 0 /* 0xd6 */,
    0 /* 0xd7 */, 0 /* 0xd8 */, 0 /* 0xd9 */, 0 /* 0xda */, 0 /* 0xdb */,
    0 /* 0xdc */, 0 /* 0xdd */, 0 /* 0xde */, 0 /* 0xdf */, 0 /* 0xe0 */,
    0 /* 0xe1 */, 0 /* 0xe2 */, 0 /* 0xe3 */, 0 /* 0xe4 */, 0 /* 0xe5 */,
    0 /* 0xe6 */, 0 /* 0xe7 */, 0 /* 0xe8 */, 0 /* 0xe9 */, 0 /* 0xea */,
    0 /* 0xeb */, 0 /* 0xec */, 0 /* 0xed */, 0 /* 0xee */, 0 /* 0xef */,
    0 /* 0xf0 */, 0 /* 0xf1 */, 0 /* 0xf2 */, 0 /* 0xf3 */, 0 /* 0xf4 */,
    0 /* 0xf5 */, 0 /* 0xf6 */, 0 /* 0xf7 */, 0 /* 0xf8 */, 0 /* 0xf9 */,
    0 /* 0xfa */, 0 /* 0xfb */, 0 /* 0xfc */, 0 /* 0xfd */, 0 /* 0xfe */,
    0 /* 0xff */
};

#define CHECK_AND_REMOVE_SPECIAL_CHARACTERS(tx_name, tx_name_len) \
{\
int loc_idx, idx;\
\
  if(!tx_name_len)\
  {\
    strcpy(tx_name_buffer,"TxNameWithZeroLength");\
    tx_name_len = 20;\
    replace_done = 1;\
  }\
  else {\
    for(loc_idx = 0; loc_idx < tx_name_len; loc_idx++)\
    {\
      idx = tx_name[loc_idx];\
      if(!VALID_TRANSACTION_VALUE_CHARS[idx])\
      {\
        tx_name_buffer[loc_idx] = '_';\
        replace_done = 1;\
      }\
      else\
        tx_name_buffer[loc_idx] = tx_name[loc_idx];\
    }\
    tx_name_buffer[tx_name_len] = '\0';\
  }\
\
  if(replace_done)\
    tx_name = tx_name_buffer;\
}

// This macro is used of start tx and end as tx
#define GET_SET_TX_HASH_CODE(vptr, name, tx_name_len, purpose, hash_code) \
{ \
  CHECK_SET_TX_NAME_LEN(vptr, name, tx_name_len); \
  int replace_done = 0; \
  \
  if(!dynamic_tx_used)  \
  {  \
    if(tx_hash_func) \
      hash_code = tx_hash_func (name, tx_name_len); \
     \
    if(hash_code < 0) \
    { \
      CHECK_AND_REMOVE_SPECIAL_CHARACTERS(name, tx_name_len); \
      if(replace_done) \
      {\
        if(tx_hash_func) \
          hash_code = tx_hash_func (name, tx_name_len); \
      }\
      if(hash_code < 0)\
      {\
        NSDL3_TRANS(vptr, NULL, "First Dynamic transaction discovery, name = %s, hash_code = %d", name, hash_code); \
        set_dynamic_tx_used(vptr, 1, purpose); \
        hash_code = allocate_tx_id_and_send_discovery_msg_to_parent(vptr, name, tx_name_len); \
      }\
    } \
  } else {   \
    hash_code = nslb_get_norm_id(&normRuntimeTXTable, name, tx_name_len); \
    NSDL3_TRANS(vptr, NULL, "Dynamic transaction already discovery, hash_code = %d", hash_code); \
    if(hash_code < 0 ) \
    { \
      CHECK_AND_REMOVE_SPECIAL_CHARACTERS(name, tx_name_len) \
      if(replace_done) \
         hash_code = nslb_get_norm_id(&normRuntimeTXTable, name, tx_name_len); \
      if(hash_code == -2) \
        hash_code = allocate_tx_id_and_send_discovery_msg_to_parent(vptr, name, tx_name_len); \
    }  \
  } \
}

// This is used where we need to get hash code without adding new tx like end and get tx time etc
#define GET_TX_HASH_CODE(vptr, name, tx_name_len, purpose, hash_code) \
{ \
  CHECK_SET_TX_NAME_LEN(vptr, name, tx_name_len); \
 \
  int replace_done = 0; \
  if(!dynamic_tx_used) \
  {  \
    if(tx_hash_func) \
      hash_code = tx_hash_func (name, tx_name_len); \
\
    if(hash_code < 0) \
    { \
      CHECK_AND_REMOVE_SPECIAL_CHARACTERS(name, tx_name_len); \
      if(replace_done) \
      {\
        if(tx_hash_func) \
          hash_code = tx_hash_func (name, tx_name_len); \
      }\
    }\
     \
  } else {   \
    hash_code = nslb_get_norm_id(&normRuntimeTXTable, name, tx_name_len); \
    NSDL4_TRANS(vptr, NULL, "Dynamic transaction hash_code = %d", hash_code); \
    if(hash_code < 0) \
    { \
      CHECK_AND_REMOVE_SPECIAL_CHARACTERS(name, tx_name_len); \
      if(replace_done) \
         hash_code = nslb_get_norm_id(&normRuntimeTXTable, name, tx_name_len); \
    } \
   } \
}

// Fuction for allocating the memory for the tx in link list, called fron ns_start_transaction ()
// name is the tx_name,to be started
// static inline int tx_start_with_name (char *name, VUser *vptr)
// Returns success (0) or error (< 0)
int inline tx_start_with_name (char *name, VUser *vptr)
{
  int hash_code = -1;  //must be set -1 as it is checked in the code.
  int tx_name_len;
  int ret;

  NSDL3_TRANS(vptr, NULL, "Method called, tx name = %s tx_len = %d", name, strlen(name));

  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    NSDL3_TRANS(vptr, NULL, "Returning without starting transaction (%s) as page is reloading.", name);
    return NS_TX_PG_RELOADING;
  }

  //Handling NS parametrisation  
  if ((name[0] == '{') && (name[strlen(name)-1] ==  '}')) 
  {
    // evaluating the value of variable "name" using ns_eval_string
    char *tx_name_using_param = ns_eval_string(name); 
    if(tx_name_using_param == NULL) 
    {
      NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "transaction name \"%s\" is not valid. Transaction name given by the eval_string API is: = %s"
				  ,name, tx_name_using_param);
      return NS_TX_BAD_TX_NAME;
    }  
    else
    {
      name = tx_name_using_param; 
      NSDL3_TRANS(vptr, NULL, "Transaction Name after ns_eval_string= %s", name);
    }
  }

  GET_SET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_start", hash_code);

  ret = tx_start_with_hash_code (name, hash_code, vptr, NS_TX_IS_API_BASED);

  return ret ; 
}

void inline tx_end_inline_tx (char *name, int status, int tx_inst, VUser *vptr, u_ns_ts_t now, int hash_code)
{
  TxInfo *node_ptr;
  TxInfo *prev_node;

  NSDL3_TRANS(vptr, NULL, "Method called, name = %s, status = %d", name, status);

  if((node_ptr = tx_get_node_by_hash_code_and_tx_instnace(name, hash_code, vptr, &prev_node, tx_inst)) == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in end transaction API is not running. Transaction name = %s", name);
    return;
  }

  node_ptr->status = status;

  do_tx_data_logging(vptr, node_ptr, now, prev_node);
  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Completed execution of transaction '%s' with status '%s' (status code = %d)", name, ns_get_status_name(status), status);

  return;// Return status of the transaction
}

// For ending the transaction, it sets the field required for ending of the transaction
// static inline int tx_end (char *name, int status, VUser *vptr)
// Returns status (>= 0) or error (< 0)
int inline tx_end (char *name, int status, VUser *vptr)
{
  int hash_code;
  TxInfo *node_ptr;
  TxInfo *prev_node;
  u_ns_ts_t  now = get_ms_stamp();
  int tx_name_len;

  NSDL3_TRANS(vptr, NULL, "Method called, name = %s, status = %d", name, status);

  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    NSDL3_TRANS(vptr, NULL, "Returning without starting transaction (%s) as page is reloading.", name);
    return NS_TX_PG_RELOADING;
  }
  if (name[0] == '{')
  {
    // evaluating the value of variable "name" using ns_eval_string
    char *tx_name_using_param = ns_eval_string(name);
    if(tx_name_using_param == NULL)
    {
      NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "transaction name \"%s\" is not valid. Transaction name given by the eval_string API is: = %s"
                                  ,name, tx_name_using_param);
      return NS_TX_BAD_TX_NAME;
    }
    else
    {
      name = tx_name_using_param;
      NSDL3_TRANS(vptr, NULL, "Name after ns_eval_string= %s", name);
    }
  }


  GET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_end", hash_code);

  if(hash_code < 0)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Invalid transaction name passed in end transaction API. Transaction name = %s", name);
    return NS_TX_BAD_TX_NAME;
  }

  if((node_ptr = tx_get_node_by_hash_code(name, hash_code, vptr, &prev_node)) == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in end transaction API is not running. Transaction name = %s", name);
    return NS_TX_NOT_RUNNING;
  }

  if(vptr->flags & NS_RESP_NETCACHE) // Main URL response is from netcache
  {
    // Add prefix NetCache, it will be used in netcache tx report 
    if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.end_tx_mode == END_TX_BASED_ON_NETCACHE_HIT)
    {
      NSDL3_TRANS(vptr, NULL, "Going to end %s in tx_end() for netcache", name);
      // End as will take care of adding _NetCache suff
      return(tx_end_as (name, status, name, vptr));
    }
  }

  int ret = tx_set_status(node_ptr, status, vptr, name); // It will return status set for tx
  if(ret == NS_TX_ERROR) return (ret);

  //If transaction status is not OK then set bit
  if(ret != NS_REQUEST_OK) {
    vptr->flags |= NS_PAGE_DUMP_CAN_DUMP;
    //Mark all page instance on transaction failure, run a loop in UserTrace node and mark flag for matching page instance
    mark_pg_instace_on_tx_failure(node_ptr, vptr);
  }
  status = node_ptr->status;
  do_tx_data_logging(vptr, node_ptr, now, prev_node);

  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Completed execution of transaction '%s' with status '%s' (status code = %d)", name, ns_get_status_name(status), status);

  return(status); // Return status of the transaction
}

// This function for ending a transaction with the different name, this fn will be called from an API named "ns_end_transaction_as ()"
// Returns status (>= 0) or error (< 0)
int inline tx_end_as (char *name, int status, char *end_name, VUser *vptr)
{
  TxInfo *node_ptr;
  TxInfo *end_name_node_ptr;
  int end_hash_code;
  TxInfo *prev_node;
  u_ns_ts_t now = get_ms_stamp();
  int hash_code;
  int tx_name_len;


  NSDL3_TRANS(vptr, NULL, "Method called, current_name = %s, end_name = %s, status = %d", name, end_name, status); 

  
  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    NSDL3_TRANS(vptr, NULL, "Returning without starting transaction (%s) as page is reloading.", name);
    return NS_TX_PG_RELOADING;
  }

  // This code is for ns variable support in end_transaction_as
  if ((name[0] == '{') && (name[strlen(name)-1] ==  '}'))
  {
    // evaluating the value of variable "name" using ns_eval_string
    char *tx_name_using_param = ns_eval_string(name);
    if(tx_name_using_param == NULL)
    {
      NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "transaction name \"%s\" is not valid. Transaction name given by the eval_string API is: = %s"
                                  ,name, tx_name_using_param);
      return NS_TX_BAD_TX_NAME;
    }
    else
    {
      name = tx_name_using_param;
      NSDL3_TRANS(vptr, NULL, "Name after ns_eval_string= %s", name);
    }
  }

  GET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_end_as", hash_code); 
  
  if(hash_code < 0)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Invalid transaction name passed in end as transaction API. Transaction name = %s", name);
    return NS_TX_BAD_TX_NAME;
  }

  if((node_ptr = tx_get_node_by_hash_code(name, hash_code, vptr, &prev_node)) == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in end as transaction API is not running. Transaction name = %s", name);
    return NS_TX_NOT_RUNNING;
  }

  // This code is for ns variable support in end_transaction_as
  if ((end_name[0] == '{') && (end_name[strlen(end_name)-1] ==  '}'))
  {
     NSDL3_TRANS(vptr, NULL, "end_name[0]= %c", end_name[0]);
     char *tx_end_name_using_param = ns_eval_string(end_name);
     if(tx_end_name_using_param == NULL)
     {
       NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  end_name,
                                  "transaction name \"%s\" is not valid. Transaction name given by the eval_string API is: = %s"
                                  ,end_name, tx_end_name_using_param);
      return NS_TX_BAD_TX_NAME;
    }
    else
    {
      end_name = tx_end_name_using_param;
      NSDL3_TRANS(vptr, NULL, "Name after ns_eval_string= %s", end_name);
    }
  }

  if(vptr->flags & NS_RESP_NETCACHE) // Main URL response is from netcache
  {
    // Add suffix NetCache, it will be used in netcache tx report 
    if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.end_tx_mode == END_TX_BASED_ON_NETCACHE_HIT)
    {
      char name_buf[256 + 1];
      sprintf(name_buf, "%s%s", end_name, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.end_tx_suffix);
      NSDL3_TRANS(vptr, NULL, "Going to end %s in tx_end_as() as %s for netcache", name, name_buf);
      end_name = name_buf;
      NSDL3_TRANS(vptr, NULL, "Tx is served from netcache and it to be ended with _NetCache. end_name = %s", end_name);
    }
  }
 

  GET_SET_TX_HASH_CODE(vptr, end_name, tx_name_len, "tx_end_as", end_hash_code);
 
  // Make sure end as tx is not running
  // Shalu :Fixed Bug 3102 
  // Here we were passing prev_node in fourth argument, this was creating problem in on_session_completion as we call 
  // do_tx_data_logging because here  previous node of node_ptr is overwritten. We are not intrested in previous node of 
  // end_name_node_ptr, so here we are passing a temporary variable prev_tmp 
  TxInfo *prev_tmp;
  if((end_name_node_ptr = tx_get_node_by_hash_code (end_name, end_hash_code, vptr, &prev_tmp)) != NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "End as transaction used in end as transaction API is running. Transaction name = %s, End transaction name = %s", name, end_name);
    return NS_TX_END_AS_RUNNING;
  }

  tx_change_hash_code (node_ptr, end_hash_code);

  int ret = tx_set_status(node_ptr, status, vptr, name);
  if(ret == NS_TX_ERROR) return (ret);

  //If transaction status is not OK then set bit
  if(ret != NS_REQUEST_OK)
    vptr->flags |= NS_PAGE_DUMP_CAN_DUMP;

  status = node_ptr->status;

  do_tx_data_logging(vptr, node_ptr, now, prev_node);
  
  NS_DT2(vptr, NULL, DM_L1, MM_TRANS, "Completed execution of transaction '%s' as '%s' with status '%s' (status code = %d)", name, end_name, ns_get_status_name(status), status);

  return(status);
}

// This fn return the total time of execution of the transaction (in millisec)
// This fn will be called through ns_get_tx_time () API,
// Returns time (>= 0) or error (< 0)
int inline tx_get_time (char *name, VUser *vptr)
{
  TxInfo *node_ptr;
  TxInfo *prev_node;
  int hash_code;
  int tx_name_len;

  NSDL3_TRANS(vptr, NULL, "Method called, name = %s", name);

  GET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_get_time", hash_code);

  if(hash_code < 0)
  {
      NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Invalid transaction name passed in get transaction time API. Transaction name = %s", name);
    
    return NS_TX_BAD_TX_NAME;
  }

  if((node_ptr = tx_get_node_by_hash_code(name, hash_code, vptr, &prev_node)) == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in get transaction time API is not running. Transaction name = %s", name);
    
    return NS_TX_NOT_RUNNING;
  }
  
  return (get_ms_stamp() - node_ptr->begin_at);
}

// This fn will be called from a new API, named as ns_get_tx_status ()
// Returns status (>= 0) or error (< 0)
int inline tx_get_status (char* name, VUser* vptr)
{
  TxInfo *node_ptr;
  int hash_code = -1;
  TxInfo *prev_node;
  int tx_name_len;
 
  NSDL3_TRANS(vptr, NULL, "Method called, name = %s", name);
  
  GET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_get_status", hash_code);

  if(hash_code < 0){ 
  NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                      vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                      vptr->sess_ptr->sess_name,
                                      name,
                                     "Invalid transaction name passed in get transaction status API. Transaction name = %s", name);
    return NS_TX_BAD_TX_NAME;
  }
  

  node_ptr = tx_get_node_by_hash_code (name, hash_code, vptr, &prev_node);
  if (node_ptr == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in get transaction status API is not running. Transaction name = %s", name);
    return NS_TX_NOT_RUNNING;
  }
  
  //If tx status is not set then set it to the page status 
  if (node_ptr->status == TX_STATUS_NOT_SET ){
    return vptr->page_status;
  }
  return (node_ptr->status);
}

// This fn wil be called by the API ns_set_tx_status
// Returns set status (>= 0) or error (< 0)
int inline tx_set_status_by_name (char* name, int status, VUser* vptr)
{
  TxInfo *node_ptr;
  int hash_code = -1;
  TxInfo *prev_node;
  int ret;
  int tx_name_len;

  NSDL3_TRANS(vptr, NULL, "Method called, name = %s status = %d", name, status); 
 
  GET_TX_HASH_CODE(vptr, name, tx_name_len, "tx_get_status", hash_code); 
 
  if(hash_code < 0){
     NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                      vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                      vptr->sess_ptr->sess_name,
                                      name,
                                      "Invalid transaction name passed in set transaction status API. Transaction name = %s", name);
    return NS_TX_BAD_TX_NAME;
  }
  

  node_ptr = tx_get_node_by_hash_code(name, hash_code, vptr, &prev_node);
  if (node_ptr == NULL)
  {
    NS_EL_2_ATTR(EID_TRANS_BY_NAME, vptr->user_index,
                                  vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
                                  vptr->sess_ptr->sess_name,
                                  name,
                                  "Transaction used in set transaction status API is not running. Transaction name = %s", name);
    return NS_TX_NOT_RUNNING;
  }

  ret = tx_set_status(node_ptr, status, vptr, name);

  //If transaction status is not OK then set bit
  if(ret != NS_REQUEST_OK)
    vptr->flags |= NS_PAGE_DUMP_CAN_DUMP;

  return (ret);
}

// All API must be in ns_string_api.c so that it can be resolved when linking with script.c

// END: API which are used from script.c (add prototype in ns_api_strings.h)
/*******************************************************************************************************/
unsigned int ns_tx_hash_func(const char *name, unsigned int len)
{
  return (unsigned int)nslb_get_norm_id(&normRuntimeTXTable, (char*)name, len);
}

#ifdef CHK_AVG_FOR_JUNK_DATA
//Function to tranverse all transaction
void validate_tx_entries(char *from, avgtime *loc_avgtime, int pool_id)
{
  int i;
  int bad_tx = 0;
  int good_tx = 0;
  TxDataSample *loc_txData = NULL;

  loc_txData = (TxDataSample*)((char *)loc_avgtime + g_trans_avgtime_idx);

  for(i = 0; i < max_tx_entries; i++)
  {
    if((loc_txData[i].tx_fetches_completed > 2000) || (loc_txData[i].tx_succ_fetches > 2000))
    {
      bad_tx++;
      NSTL1(NULL, NULL, "%s, Got BadTX:, fetches_completed = %d, succ_fetches = %d, txData = %p, avgtime = %p, max_tx_entries = %d, g_cur_avg[0] = %p, g_next_avg[0] = %p, pool_id = %d", from, loc_txData[i].tx_fetches_completed, loc_txData[i].tx_succ_fetches, loc_txData, loc_avgtime, max_tx_entries, g_cur_avg[0], g_next_avg[0], pool_id);
      abort();
    }
    else
      good_tx++;
  }
  NSTL2(NULL, NULL, "Called from %s, Traverse %d no of transactions, good_tx = %d, bad_tx = %d, txData = %p, avgtime = %p", from, total_tx_entries, good_tx, bad_tx, loc_txData, loc_avgtime);
}
#endif
