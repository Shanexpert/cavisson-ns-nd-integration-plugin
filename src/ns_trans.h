#include "nslb_get_norm_obj_id.h"

/*****************************

ns_trans.h

*****************************/
#ifndef _ns_trans_h
#define _ns_trans_h

// This structure is for Transaction Node for running transaction,
// This node is kept in the linked list of all running transations.

//#define NS_TRANS_REQUEST_STOPPED 2

#define TX_MAX_PAGES_PER_TX 24 // TODO: This is serious limitation

#define NS_TX_IS_API_BASED  0
#define NS_TX_IS_PAGE_AS    1
#define NS_TX_IS_INLINE    2

#ifndef CAV_MAIN
extern NormObjKey normRuntimeTXTable;
#else
extern __thread NormObjKey normRuntimeTXTable;
#endif
extern int dynamic_tx_used;
/************************************************************************************/

typedef struct TxInfo
{
  int           hash_code;                // holds hash code
  u_ns_ts_t     begin_at;                 // holds starting time stamps
  // unsigned char is_done;               // flag, is transaction is done or not (Returns 0 - if running , 1 - if done)


  u_ns_ts_t think_duration;            // think duration for transaction in millisec(We can make it unsigned int but then we ned to make changes in logging.c, logging.h and logging_reader.c files)
  struct TxInfo *next;                    // hold the address of next node.
  char           api_or_pg;               // It specifies that node is for pg_as_tx or for api, for api it is 0, else 1
  unsigned char status;                   // holds status
  // Unique transaction instance for each session
  // used for logging data into data base, worked as foreign key to map how many pages (urls) are there in a  transaction.
  unsigned short instance;                

  // This is added for tracking on nested transactions.
  unsigned short  num_pages;  // Number of pages part of this trasaction
  // Array of page instances part of this transaction.
  int rbu_tx_time;       // Only for RBU
  unsigned short page_instance[1]; // This array size is based on G_MAX_PAGES_PER_TX keyword and taken care in Malloc
  // Bit mask of page Ids which are part of this transaction. This is added for tracking on nested transactions.
  // Array size is dynamic based on the total number of pages. One int holds 32 pages (32 bit)
  // Bits are set on completion of page.
  // int    page_id_bit_mask[1];  

  int wasted_time;
    /* Description: wasted_time -> this is extra time which we are using in our intrenal calculation, 
       This must be redused from total tranaction time to get actual transaction time */
  u_ns_8B_t tx_tx_bytes;               //Total bytes send in the context of this transaction
  u_ns_8B_t tx_rx_bytes;               //Total bytes recv in the context of this transaction
} TxInfo;

#define UPD_TX_NW_CACHE_USED_STATS(vptr, hash_code, download_time, sq_download_time) \
{ \
if(vptr->flags & NS_RESP_NETCACHE) \
  { \
    txData[hash_code].tx_netcache_fetches++; \
    NSDL2_TRANS(vptr, NULL, "tx_netcache_fetches = %u", txData[hash_code].tx_netcache_fetches); \
    SET_MIN (txData[hash_code].tx_netcache_hit_min_time, download_time); \
    SET_MAX (txData[hash_code].tx_netcache_hit_max_time, download_time);  \
\
    txData[hash_code].tx_netcache_hit_tot_time += download_time;  \
    txData[hash_code].tx_netcache_hit_tot_sqr_time += sq_download_time;  \
    NSDL2_TRANS(vptr, NULL, "txData[hash_code].tx_netcache_hit_tot_time = %lld, txData[hash_code].tx_netcache_hit_tot_sqr_time = %lld", txData[hash_code].tx_netcache_hit_tot_time, txData[hash_code].tx_netcache_hit_tot_sqr_time); \
  } else { \
    SET_MIN (txData[hash_code].tx_netcache_miss_min_time, download_time); \
    SET_MAX (txData[hash_code].tx_netcache_miss_max_time, download_time); \
  \
    txData[hash_code].tx_netcache_miss_tot_time += download_time;  \
    txData[hash_code].tx_netcache_miss_tot_sqr_time += sq_download_time;  \
    NSDL2_TRANS(vptr, NULL, "txData[hash_code].tx_netcache_miss_tot_time = %lld, txData[hash_code].tx_netcache_miss_tot_sqr_time = %lld", txData[hash_code].tx_netcache_miss_tot_time, txData[hash_code].tx_netcache_miss_tot_sqr_time);  \
  }  \
}  \

extern void tx_begin_on_page_start(VUser *vptr, u_ns_ts_t now);
extern void tx_update_begin (VUser *vptr, unsigned int addition_amt, int flag);
extern void tx_logging_on_session_completion (VUser *vptr, u_ns_ts_t now, int status);
extern void set_tx_status_on_close_connection (VUser *vptr, int flag);
extern void tx_logging_on_page_completion(VUser *vptr, u_ns_ts_t now);
extern void add_tx_tot_think_time_node(VUser *vptr);

extern int log_tx_record(VUser* vptr, u_ns_ts_t now, TxInfo *node_ptr);
extern int log_tx_record_v2(VUser* vptr, u_ns_ts_t now, TxInfo *node_ptr);

extern inline int tx_end_as (char *name, int status, char *end_name, VUser *vptr);
extern int tx_get_time (char *name, VUser *vptr);

extern int tx_get_cur_tx_hash_code(VUser *vptr);
extern int tx_start_with_name (char *name, VUser *vptr);
extern int tx_end (char *name, int status, VUser *vptr);
extern int tx_set_status_by_name (char* name, int status, VUser* vptr);
extern int tx_get_status (char* name, VUser* vptr);

// Prototype of Transaction APIs are defined in ns_string.h


extern inline int tx_start_with_hash_code (char *name, int hash_code, VUser *vptr, int flag);
extern inline void tx_add_node (int hash_code, VUser *vptr, int flag, u_ns_ts_t now);
extern void inline tx_end_inline_tx (char *name, int status, int tx_inst, VUser *vptr, u_ns_ts_t now, int hash_code);

//Dynamic Tx Functions
extern int kw_set_show_runtime_tx_data(char *buf);
extern int dynamic_tx_start_with_name(VUser *vptr, char *name, int tx_name_len, int hash_code);
extern int allocate_tx_id_and_send_discovery_msg_to_parent(VUser *vptr, char *data, int data_len);
extern unsigned int ns_tx_hash_func(const char *name, unsigned int len);
extern inline void tx_logging_on_url_completion (VUser *vptr, connection *cptr, u_ns_ts_t now);
#ifdef CHK_AVG_FOR_JUNK_DATA
void validate_tx_entries(char *from, avgtime *loc_avgtime, int slot_id);
#endif
#endif

