#ifndef CDR_NV_HANDLER_H
#define CDR_NV_HANDLER_H

extern struct dirent **nv_partition_list;
extern int nv_partition_count;

extern long long int get_nv_partition_disk_size(int norm_id);
extern long long int get_nv_partition_csv_size(int norm_id);
extern long long int get_nv_partition_db_table_size(int norm_id);
extern long long int get_nv_partition_db_index_size(int norm_id);
extern long long int get_nv_partition_logs_size(int norm_id);
extern long long int get_nv_logs_size(int norm_id);
extern long long int get_nv_partition_ocx_size(int norm_id);
extern long long int get_nv_partition_na_traces_size(int norm_id);
extern long long int get_nv_partition_access_log_size(int norm_id);
extern long long int get_nv_access_log_size(int norm_id);
extern int get_nv_partition_type(long long int partition_num);
extern void nv_check_ngve_range(int norm_id, int cur_idx);
extern void nv_cleanup_process();

extern void cdr_get_nv_client_id();
extern int get_nv_partition_num_proc(long long int partition_num);
extern void cdr_dump_nv_cache_to_file();
extern void remove_nv_partition_logs(int norm_id);
extern void remove_nv_partition_db(int i);
extern void remove_nv_partition_csv(int norm_id);
extern void remove_nv_partition_ocx(int norm_id);
extern void remove_nv_partition_na_traces(int norm_id);
extern void remove_nv_partition_access_log(int norm_id);
extern void remove_nv_partition_logs(int norm_id);
#endif
