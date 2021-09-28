#ifndef CDR_DIR_OPERATION_H
#define CDR_DIR_OPERATION_h
extern long long int get_dir_size_ex(char *path);
extern struct dirent **get_tr_partiton_list(int tr_num, int *count, char *path);
extern long long int get_similar_file_size(char *path, int (*filter_fun_ptr)(const struct dirent *));
extern int testrun_filter(const struct dirent *dir_e);
extern int testrunouput_log_filter(const struct dirent *dir_e);
extern int pct_filter(const struct dirent *dir_e);
extern int rtg_filter(const struct dirent *dir_e);
extern int log_or_trace_filter(const struct dirent *dir_e);
extern int json_filter(const struct dirent *dir_e);

extern void remove_dir_file_ex(char *path);
extern long long get_file_size(const char *file_path);
extern void remove_dir_file_with_retention_time_ex(char *path, int retention_time, int check_core_file_flag);
extern void remove_similar_file(char *path, int (*filter_fun_ptr)(const struct dirent *), int retention_time);
extern void remove_file(char *path);
extern void remove_dir(char *path);
#endif
