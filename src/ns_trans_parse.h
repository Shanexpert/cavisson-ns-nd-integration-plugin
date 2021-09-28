/********************************************************************
* Name: ns_trans_parse.h
* Purpose: Function prototype related to parsing of transaction and PAGE_AS_TRANSACTION
* Author: Anuj
* Intial version date: 01/11/07
* Last modification date
********************************************************************/


#ifndef NS_TRANS_PARSE_H
#define NS_TRANS_PARSE_H

#define MAX_ALLOWED_TX  300
#define INIT_TX_ENTRIES 16  //moved from util.h
#define TX_NAME_MAX_LEN 1024  // The maximum length of tx_name

#ifndef CAV_MAIN
extern int max_tx_entries;
extern unsigned int (*tx_hash_func)(const char*, unsigned int);
extern int g_trans_avgtime_idx;
#else
extern __thread int max_tx_entries;
extern __thread unsigned int (*tx_hash_func)(const char*, unsigned int);
extern __thread int g_trans_avgtime_idx;
#endif
extern int g_trans_cavgtime_idx;
extern inline int set_page_as_trans (char *buf, char *err_msg, int runtime_flag);
extern void tx_add_pages_as_trans ();
extern int get_tx_names(FILE* c_file);
extern int get_tx_hash();
extern void alloc_mem_for_txtable();
extern void copy_tx_entry_to_shr (void *tx_page_table_shr_mem, int total_page_entries);
extern int parse_transaction(char *buffer, char *fname, int line_num, int sess_idx);
extern int match_pattern(const char *name, char *pattern);
extern int get_args(char *read_buf, char *args[]);

extern int kw_set_g_max_pages_per_tx(char *buf, unsigned short *max_pages_per_tx, char *err_msg, int runtime_flag);
//extern char *(*get_tx)(unsigned int);;

extern inline char* find_tx_name(int idx);
//extern int get_tokens(char *read_buf, char *fields[], char *token ); // Currently defined in the ns_gdf.c, need to move in some common library

//To add Tx name in Tx table with suffix
extern inline void add_trans_name_with_netcache();
extern inline int add_trans_name (char *tx_name, int sess_id);
extern int kw_set_g_send_ns_tx_http_header(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_g_end_tx_netcache(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern inline void update_trans_avgtime_size();
extern inline void update_trans_cavgtime_size();
extern inline void set_trans_avgtime_ptr();
extern int create_tx_table_entry(int *row_num);
extern int kw_set_dynamic_tx_settings(char *buf);
// End of file
#endif

