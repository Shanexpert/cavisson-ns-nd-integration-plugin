#ifndef CDR_MAIN_H
#define CDR_MAIN_H

#define ONE_DAY_IN_SEC 24 * 60 * 60
#define CDR_FILE_PATH_SIZE 256
#define CDR_BUFFER_SIZE 1024 * 2
#define CDR_CACHE_LINE_SIZE 1024 * 2
#define DELTA_REALLOC_SIZE 5
#define CACHE_ENTRY_NORM_TABLE_SIZE 1024 * 2

#define CDR_SUCCESS 0
#define CDR_ERROR  -1
#define CDR_TRUE    1
#define CDR_FALSE   0
#define TRUE    1
#define FALSE   0
#define CONFIG_FALSE   -1
#define TRUE_FOR_NEXT_DAYS   3

/*  Global variables    */
extern char ns_wdir[CDR_FILE_PATH_SIZE];
extern char *logsPath;
extern char hpd_root[CDR_FILE_PATH_SIZE];
extern char nv_client_id[CDR_FILE_PATH_SIZE];
extern char cache_file_path[CDR_FILE_PATH_SIZE];
extern char cmt_cache_file_path[CDR_FILE_PATH_SIZE];
extern char nv_cache_file_path[CDR_FILE_PATH_SIZE];
extern char config_file_path[CDR_FILE_PATH_SIZE];
extern char rebuild_cache;
extern long long int cur_time_stamp;
long long int cur_time_stamp_with_no_hr;
extern int cmt_tr_num;
extern int cmt_tr_cache_idx;
extern int g_tr_num;
extern long long int g_partition_num;
extern char g_component_name[CDR_FILE_PATH_SIZE];

#endif
