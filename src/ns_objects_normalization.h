#ifndef NS_URL_ID_NORM_H
#define NS_URL_ID_NORM_H

#define MAX_BUF_LENGTH 1024

// Moved from logging_reader.h to avoid include of this file in ns_url_id_normalization.c
/*
typedef struct {
  unsigned int url_id;
  unsigned int pg_id;
  unsigned int url_hash_id;
  unsigned int url_hash_code;
  int len; // URL Length
  char url_name[MAX_URL_LEN + 1]; 
} UrlTableLogEntry;
*/

typedef unsigned int normIdType; 

//netcloud
extern unsigned int get_url_norm_id_for_generator(char *gen_name, int gen_len, int gen_id, int gen_url_index, int *is_new_url);
extern void url_norm_init(int total_nvms, int total_generator_entries);

// url/session/page/tx
//extern void object_norm_init();
extern void object_norm_init(MTTraceLogKey *lr_trace_log_key, int total_nvm, int total_generators);

//url
extern unsigned int get_norm_id_from_nvm_table(unsigned int nvm_url_index, unsigned int nvm_index, int gen_id);
extern unsigned int gen_url_norm_id(unsigned int url_id, char *url_name, int url_len, int *is_new_url, int gen_id);
extern unsigned int get_url_norm_id(unsigned int id, int gen_id);

//session
extern unsigned int gen_session_norm_id(int session_index, char *session_name, int session_len, int *is_new_session);
extern unsigned int get_session_norm_id(unsigned int sess_id);

//page
extern unsigned int gen_page_norm_id(int page_index, char *page_name, int page_len, int *is_new_page);
extern unsigned int get_page_norm_id(unsigned int page_id);

//tx
extern unsigned int gen_tx_norm_id(int tx_index, char *tx_name, int tx_len, int *is_new_tx);
extern unsigned int get_tx_norm_id(unsigned int tx_id);
extern unsigned int gen_tx_norm_id_v2(unsigned int tx_index, char *tx_name, int tx_len, int *is_new_tx, int nvm_id, int gen_id);
extern unsigned int get_tx_norm_id_v2(int nvm_id, unsigned int nvm_tx_id, int gen_id);

extern inline void build_norm_tables_from_metadata_csv(int test_run_num, char *common_files_dir);

extern void reset_norm_id_mapping_tbl();

//generator
extern unsigned int gen_generator_norm_id(char *generator_name, int generator_id, int generator_len, int *is_new_generator);
extern unsigned int get_generator_norm_id(unsigned int gen_id);

//group
extern unsigned int gen_group_norm_id(int group_num, char *group_name, int userprof_id, int sess_prof, int pct, int *is_new_group, int group_len);
extern unsigned int get_group_norm_id(unsigned int grp_id);

//Host
extern unsigned int gen_host_norm_id(int *is_new_host, int host_id, int host_len, char *host_name, int gen_id, int nvm_id);


extern unsigned int get_host_norm_id(unsigned int id, int gen_id, int nvm_id1);
#endif
