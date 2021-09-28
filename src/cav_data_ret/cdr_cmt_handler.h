#ifndef CDR_CMT_HANDLER_H
#define CDR_CMT_HANDLER_H

#define NORMAL_PARTITION 0
#define RUNNING_PARTITION 1
#define BAD_PARTITION 2

extern long long int get_partition_disk_size(int norm_id);
extern long long int get_partition_graph_data_size(int norm_id);
extern long long int get_partition_csv_size(int norm_id);
extern long long int get_partition_raw_files_size(int norm_id);
//extern long long int get_partition_db_table_size(int norm_id);
//extern long long int get_partition_db_index_size(int norm_id);
extern long long int get_partition_har_file_size(int norm_id);
extern long long int get_partition_page_dump_size(int norm_id);
extern long long int get_partition_page_dump_size(int norm_id);
extern long long int get_partition_logs_size(int norm_id);
extern long long int get_partition_db_file_size(int norm_id);
extern long long int get_partition_reports_size(int norm_id);

extern int get_partition_type(int tr_num, long long int partition_num);
extern void remove_partition(int tr_num, long long int partition_num);

extern void cmt_check_ngve_range(int norm_id, int cur_idx);

extern void cmt_cleanup_process();

extern void cdr_dump_cmt_cache_to_file();
extern void remove_partition_har_file(int norm_id);
extern void remove_partition_csv(int norm_id);
extern void remove_partition_raw_data(int norm_id);
extern void remove_partition_db(int norm_id);
extern void remove_partition_page_dump(int norm_id);
extern void remove_partition_logs(int norm_id);
extern void remove_partition_reports(int norm_id);
#endif
