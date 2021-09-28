#ifndef NS_SERVER_IP_NORMALIZATION_H
#define NS_SERVER_IP_NORMALIZATION_H

#include "nslb_get_norm_obj_id.h"


typedef struct
{
  int loc2norm_alloc_size;
  int *nvm_svr_ip_loc2norm_table;
  int loc_srv_ip_avg_idx;
  int num_entries;
  int dyn_total_entries;
  int last_gen_local_norm_id;
} SvrIpLoc2NormTable;

extern SvrIpLoc2NormTable *g_svr_ip_loc2norm_table; //Global variable for starting address of g_tx_loc2norm table

extern int ns_add_svr_ip(short nvmindex, int local_svr_ip_id, char *server_name, short server_name_len, int *flag_new);
extern inline void ns_svr_ip_init_loc2norm_table();

#endif

