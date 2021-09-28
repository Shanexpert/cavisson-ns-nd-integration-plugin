#ifndef __NS_PERCENTILE_H__
#define __NS_PERCENTILE_H__

#define NS_PCT_MAX_FILE_NAME_LEN           1024
#define NS_PCT_MAX_LINE_BUF_LEN            1024

#define PERCENTILE_MODE_TOTAL_RUN          0
#define PERCENTILE_MODE_ALL_PHASES         1
#define PERCENTILE_MODE_INTERVAL           2

#define PCT_STABILIZATION_SHM_IDX          0
#define PCT_DURATION_SHM_IDX               1
#define PCT_FINISH_SHM_IDX                 2
                                         
#define PCT_REPORT_CONTINUE                0
#define PCT_REPORT_FINISH                  1

typedef unsigned long long Pdf_Data_8B; //Parent will have 8 byte 
typedef unsigned int Pdf_Data;

typedef struct pdf_data_hdr {
  Pdf_Data_8B sequence; // Can we get rid of this as we have now time stamp
  Pdf_Data_8B active;
  Pdf_Data_8B abs_timestamp;   // Absolute time stamp of the packet (Same as in Msg_data_hdr) (added be Neeraj)
  // Do we need to add parition idx as in Msg_data_hdr?
  int total_tx_entries;
  Pdf_Data future3;
} pdf_data_hdr;

typedef struct Pdf_Shm_Info {
  void *addr[3];
  int active_addr_idx;
  int total_pdf_data_size_for_this_child;
  int total_tx; //To hold the number of transactions. TODO: Need to check if this need to be per address???
  int shm_id;
} pdf_shm_info_t;

typedef struct Pdf_Lookup_Data {
  char pdf_name[255 + 1]; // PDF Filename without path
  int min_granules;
  int max_granules;
  int num_granules;
  int pdf_data_offset;
  int pdf_data_offset_parent;
  int group_id;
  int graph_id;
} pdf_lookup_data_t ;

#define DELTA_PDF_ENTRIES 10

#define PDF_DATA_HEADER_SIZE sizeof(pdf_data_hdr)
#define PDF_MAX_FIELDS 25

void kw_set_report_granule_size(char *text);
void kw_set_num_buckets(char *text);
void kw_set_report_pg_granule_size(char *text);
void kw_set_num_pg_buckets(char *text);
void kw_set_pg_bucket(char *text);
void kw_set_report_tx_granule_size(char *text);
void kw_set_num_tx_buckets(char *text);
void kw_set_tx_bucket(char *text);
void kw_set_report_sess_granule_size(char *text);
void kw_set_num_sess_buckets(char *text);
void kw_set_sess_bucket(char *text);
void kw_set_bucket(char *text);
int kw_set_percentile_report(char *text, int runtime_flag, char *err_msg);

extern int kw_set_url_pdf_file(char *text, int runtime_flag, char *err_msg);
extern int kw_set_page_pdf_file(char *text, int runtime_flag, char *err_msg);
extern int kw_set_session_pdf_file(char *text, int runtime_flag, char *err_msg);
extern int kw_set_trans_resp_pdf_file(char *text, int runtime_flag, char *err_msg);
extern int kw_set_trans_time_pdf_file(char *text, int runtime_flag, char *err_msg);
extern void pct_switch_partition(char *cur_pdf_file, char *prev_pdf_file);
extern void pdf_append_end_line_ex(char *prev_file);

void granule_data_output(int mode, FILE *rfp, FILE* srfp, avgtime *avg, double *data);
extern void create_testrun_pdf();
void init_pdf_shared_mem();
extern int create_pdf_lookup_table(int *row_num, char *pdf_name, int min_granules, int max_granules, 
                            int num_granules, int pdf_data_offset, int pdf_data_offset_8B, int rpGroupID, int rpGraphID);
extern void copy_and_flush_child_pdf_data(int child_id, int shm_idx, int finish_report_flag_set, char *buff, int total_tx);
void  dump_pdf_shrmem_to_file(char *addr, int finish_report_flag_set);
//void  clear_pdf_shrmem(char *addr);
extern int switch_pdf_shm(int shm_idx, char *caller);
void update_pdf_data(Pdf_Data value, int index, int group_vector_num, 
                     int group_pdf_data_size, int graph_vector_num);
void open_pct_message_file();
void close_pct_msg_dat_fd();
void update_pdf_data_parent(Pdf_Data value, int index, 
                            int group_vector_num, int group_pdf_data_size, int graph_vector_num);
void set_percentile_report_for_sla(int run_mode, int org_percentile_report);
void pdf_append_end_line();
void add_pdf_data(Pdf_Data_8B *parent_pdf_data, Pdf_Data *child_pdf_data, int num_tx, int child_id);

//extern int process_pdf(int pdf_id, int *min_granules, int *max_granules);
extern int process_pdf(int *pdf_id, int *min_granules, int *max_granules, char *pdf_name);
extern int total_pdf_data_size;
extern long long int total_pdf_data_size_8B;
extern int total_pdf_data_size_child;
extern int g_percentile_report;
extern int g_percentile_mode;
extern int g_percentile_interval;

extern int pdf_average_url_response_time;
extern int pdf_average_smtp_response_time;
extern int pdf_average_pop3_response_time;
extern int pdf_average_ftp_response_time;
extern int pdf_average_dns_response_time;
extern int pdf_average_page_response_time;
extern int pdf_average_session_response_time;
extern int pdf_average_transaction_response_time;
extern int pdf_transaction_time;
extern int testrun_pdf_and_pctMessgae_version;
extern int is_new_tx_add;
extern u_ns_ts_t testrun_pdf_ts;

extern pdf_lookup_data_t *pdf_lookup_data;
extern pdf_shm_info_t *pdf_shm_info;
extern void *parent_pdf_addr;
extern void *parent_pdf_next_addr;
//extern int num_unique_pdfs;
//extern int **unique_pdfs_list;
extern char *process_last_sample_array;
extern int parent_cur_count;
extern int parent_next_count;
extern int g_percentile_sample;
extern int g_percentile_sample_missed;
extern void validate_percentile_settings();
extern int num_process_for_nxt_sample;
extern int total_pdf_msg_size;
extern void fill_percentiles(char *addr, double *url_data, double *smtp_data, double *pop3_data, double *ftp_data, double *dns_data, double *pg_data, double *tx_data, double *ss_data);
extern void create_memory_for_parent ();
extern inline void process_attach_pdf_shm_msg(parent_msg *msg);
extern inline void reset_pct_vars_for_dynameic_tx();
extern int chk_and_add_in_pdf_lookup_table(char *graph_description, int *pdf_id, int *row_num, int rpGroupID, int rpGraphID, int runtime_flag);
extern void check_if_need_to_resize_parent_pdf_memory();
extern void check_duplicate_pdf_files();
extern void process_percentile_report_msg();
extern void send_pct_report_msg_nvm2p(int finish_report, int shm_idx, char *caller); 
extern void flush_pctdata();
extern void pct_run_phase_mode_chk_send_ready_msg(int phase_idx, int finish);
extern void pct_time_mode_chk_send_ready_msg(int finish);

#endif /* __NS_PERCENTILE_H__ */
