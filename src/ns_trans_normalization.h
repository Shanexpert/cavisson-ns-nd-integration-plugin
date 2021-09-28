#ifndef NS_TRANS_NORMALIZATION_H
#define NS_TRANS_NORMALIZATION_H

#include "nslb_get_norm_obj_id.h"
typedef struct 
{
  int loc2norm_alloc_size;
  int num_entries;
  int *nvm_tx_loc2norm_table;
  int dyn_total_entries[2]; //per progress interval dynamic transaction ,0 idx for NEW_OBJECT_DISCOVERY_TX 1 for NEW_OBJECT_DISCOVERY_TX_CUM
  int loc_tx_avg_idx;       //avg index of tx,
  int last_gen_local_norm_id[2];
} TxLoc2NormTable;

#ifndef CAV_MAIN
extern TxLoc2NormTable *g_tx_loc2norm_table; //Global variable for starting address of g_tx_loc2norm table
#else
extern __thread TxLoc2NormTable *g_tx_loc2norm_table; //Global variable for starting address of g_tx_loc2norm table
#endif
extern NormObjKey g_key_tx_normid;

extern int ns_trans_add_dynamic_tx (short nvmindex, int local_tx_id, char *tx_name, short tx_name_len, int *flag_new);
extern inline void ns_trans_init_loc2norm_table(int static_tx_count);

#endif
