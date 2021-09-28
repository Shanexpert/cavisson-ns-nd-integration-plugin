#ifndef NS_PAGE_DUMP_H
#define NS_PAGE_DUMP_H

#include "ns_trans.h"

typedef struct PerProcSessionTable {
  int num_sess;
  int freq; /*Add freq for each nvm*/
  int multiplier; /*Used to multiply with calculated freq as well as running session counter*/
} PerProcSessionTable;
extern PerProcSessionTable *per_proc_sess_table;

#define NS_IF_PAGE_DUMP_ENABLE (vptr->flags & NS_PAGE_DUMP_ENABLE)
#define NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL ((vptr->flags & NS_PAGE_DUMP_ENABLE) && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail > TRACE_ALL_SESS && runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail != TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL))


/*This is for user trace. If user trace is enable on this user
 * then dont free it from page dump. It will get free 
 * in user trace*/
#define UT_FREE_ALL_NODES(vptr)\
  if(!(vptr->flags & NS_VUSER_TRACE_ENABLE))\
    ut_free_all_nodes (vptr->pd_head);

/*Macros for tracing session limit value*/
#define PAGE_DUMP_MODE_ALL 0
#define PAGE_DUMP_MODE_PERCENTAGE 1
#define PAGE_DUMP_MODE_NUMBER 2

/*Macros for tracing Level*/
#define TRACE_DISABLE  0
#define TRACE_URL_DETAIL  1
#define TRACE_ONLY_REQ_RESP  2
#define TRACE_PARAMETERIZATION  3
#define TRACE_CREATE_PG_DUMP  4

/*Macros for trace-session*/
#define TRACE_ALL_SESS  0
#define TRACE_FAILED_PG  1
#define TRACE_WHOLE_IF_PG_TX_FAIL  2
#define TRACE_PAGE_ON_HEADER_BASED 3

/*Macros for resetting all page dump flags*/
#define RESET_ALL_PAGE_DUMP_FLAGS \
  vptr->flags &= ~NS_PAGE_DUMP_ENABLE; \
  vptr->flags &= ~NS_PAGE_DUMP_CAN_DUMP; \
  vptr->flags &= ~NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL; \

extern void divide_session_per_proc();
extern void start_dumping_page_dump (VUser *vptr, u_ns_ts_t now);
extern int need_to_dump_session(VUser *vptr, int mode, int group_idx);
extern void init_page_dump_data_and_add_in_list(VUser *vptr);
extern int need_to_enable_page_dump(VUser *vptr, int mode, int group_idx);
extern void create_per_proc_sess_table();
extern void cal_freq_to_dump_session(int group_num, double limit_mode_val);
extern void dump_url_details(connection *cptr, u_ns_ts_t now);
extern int trace_log_current_sess(VUser* vptr);
extern inline void free_nodes(VUser *vptr);
extern int write_into_rep_body_file(char *path, char *filename, char *buff_data, int buff_len);
extern int log_page_dump_record(VUser* vptr, int child_index, u_ns_ts_t page_start_time, u_ns_ts_t page_end_time, int sess_instance, int sess_index, int page_index, int page_instance, char* parameter, short parameter_size, int page_status, char *flow_name, int log_file_sfx_size, char *log_file, int res_body_orig_name_size, char *res_body_orig_name, char *page_name, int page_response_time, int future1, int txName_len, char *txName, int fetched_param_len, char *fetched_param);
extern void mark_pg_instace_on_tx_failure(TxInfo *node_ptr, VUser* vptr);
#endif
