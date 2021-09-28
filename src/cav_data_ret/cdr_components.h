#ifndef CDR_COMPONENT_H
#define CDR_COMPONENT_H


/* funtions to get component file size */
extern long long get_key_files_size(int tr_num);
extern long long get_greph_data_size(int tr_num);
extern long long get_har_file_size(int tr_num);
extern long long get_csv_size(int tr_num);
extern long long get_page_dump_size(int tr_num);
extern long long get_db_size(int tr_num);
extern long long get_logs_size(int tr_num);
extern long long get_raw_files_size(int tr_num);
extern long long get_scripts_size(int tr_num);
extern long long get_test_data_size(int tr_num);
extern long long get_db_file_size(int tr_num);
extern long long get_reports_size(int tr_num);


#endif
