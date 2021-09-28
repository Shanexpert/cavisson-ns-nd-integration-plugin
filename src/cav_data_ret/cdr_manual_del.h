#ifndef CDR_MANUAL_DEL_H
#define CDR_MANUAL_DEL_H

extern int g_tr_num;
extern long long int g_partition_num;
extern char g_component_name[CDR_FILE_PATH_SIZE];

extern void cdr_handle_manual_delete();
extern  int get_component(char *name);
extern void remove_partition_har_file(int norm_id);
extern void remove_partition_reports(int norm_id);
extern int partition_is_running(int tr_num, long long int partition_num);

#endif
