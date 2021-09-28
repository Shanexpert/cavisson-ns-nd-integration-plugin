#ifndef CDR_UTILS_H
#define CDR_UTILS_H

enum tr_type
{
    BAD_TR = 0,
    CMT_TR,
    RUNNING_TR,
    OLD_CMT_TR,
    DEBUG_TR,
    ARCHIVED_TR,
    LOCKED_TR,
    PERFORMANCE_TR,
    GENERATOR_TR
};

extern char rebuild_cache;

extern int read_file(char *file_path,char flag, char present_flag);
extern int send_buffer(char *buffer, int bytes_read, int *partial_buf_len,char flag);

extern long long summary_top_time_convert_to_ts(char * buffer);
extern int convert_to_secs(char *buffer);
extern long long int get_lmd_ts(char *path);
extern int tr_is_bad(int tr_num);
extern int get_cmt_tr_number();
extern char get_test_run_type(int tr_num, char *test_mod_buf, int remove_tr);
extern int check_and_set_cdr_pid();
extern int check_and_set_lmd_config_file();
extern long long format_date_convert_to_ts(char * buffer);
extern void change_ts_partition_name_format(long long int time_stamp, char *outbuff,int outbuff_size);
extern long long partition_format_date_convert_to_ts(char * buffer);
extern long long int get_ts_in_ms();
extern int update_running_test_list();
extern char tr_is_running(int tr_num);
#endif
