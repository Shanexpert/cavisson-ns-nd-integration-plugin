/************************************************************************************************
 * File Name            : ns_http_cache_store.h
 * Author(s)            : Shilpa Sethi
 * Date                 : 29 November 2010
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Declares Cache Hash Table Directives and Functions
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/
#ifndef NS_HTTP_CACHE_STORE_H
#define NS_HTTP_CACHE_STORE_H

//<<<<<<<<<<<<<<<<<<<<<<<<< P R E P R O C E S S O R   D I R E C T I V E S >>>>>>>>>>>>>>>>>>>>>>>

//As per 32-bit cache_FNV Algorithm
#define CACHE_FNV_PRIME 16777619
#define CACHE_FNV_OFFSET_BASIS 2166136261u

/***********************************************************************************************/

/************************************************************************************************/
//<<<<<<<<<<<<<<<<<<<<<<<<< F O R W A R D   D E C L A R A T I O N S >>>>>>>>>>>>>>>>>>>>>>>>>>>>>

extern int cache_init(VUser *vptr);

extern int cache_add_url(connection *cptr, VUser *vptr, unsigned char *url);

extern CacheTable_t *cache_url_found(VUser *vptr, unsigned char *url, int iUrlLen,
		CacheTable_t **prev_cacheTable_entry, int *ihashIndex, unsigned int *ihashValue, short http_method, int node_type);

extern int cache_delete_url(VUser *vptr, connection *cptr, unsigned char *url, int iUrlLen, int http_method);

extern int cache_destroy(VUser *vptr);
extern void cache_free_data(CacheTable_t *cte, short free_all, short free_resp, short free_header_node);
extern void cache_set_cache_table_size_value(int grp_idx, int max_cache_entries);
extern CacheTable_t *get_cache_node(unsigned int hash, short http_method, CacheTable_t **prev_cache_link, 
                       CacheTable_t **cacheTable, unsigned char *url, int iUrlLen, unsigned int ihashValue);

extern int cache_delete_url_frm_master_tbl(VUser *vptr, int http_method, unsigned char *url, int iUrlLen, int *free_emd, int free_resp);
extern CacheTable_t *cache_add_url_in_master_tbl(connection *cptr, VUser *vptr, unsigned char *url, 
                                     int iUrlLen, int *masterhashidx);
extern void free_link_list (void *buf_head);

int cache_delete_entry_from_user_tbl(VUser *vptr, unsigned char *url, int url_type, CacheTable_t *cacheTable_entry, CacheTable_t *prev_cache_link, int ihashIndex);
int cache_delete_entry_frm_master_tbl(VUser *vptr, CacheTable_t *cacheTable_entry, CacheTable_t *prev_cache_link, int ihashIndex, int *free_emd, int free_resp);

/***********************************************************************************************/


#endif //NS_HTTP_CACHE_STORE_H
