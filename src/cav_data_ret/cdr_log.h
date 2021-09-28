/*****************************
cdr_log.h
*****************************/


#define DEBUG_HEADER "\nAbsolute Time Stamp|File|Line|Function|Log Message"
#define AUDIT_HEADER "\nAbsolute Time Stamp|TR # / Client ID|Partition|Component|Size|Opretion|Description|Time taken by operation|UserName"
#define CDT_BUFF	100
#define BUFF_SIZE_1024 1024 
#define MAX_DEBUG_ERR_LOG_BUF_SIZE 1024*1024


#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

extern void cdr_trace_log(int log_level, char *file, int line, char *fname,  char *format, ...);

//Trace log 
#define CDRTL1(...)  cdr_trace_log(0x000000FF, _FLN_, __VA_ARGS__)
#define CDRTL2(...)  cdr_trace_log(0x0000FF00, _FLN_, __VA_ARGS__)
#define CDRTL3(...)  cdr_trace_log(0x00FF0000, _FLN_, __VA_ARGS__)
#define CDRTL4(...)  cdr_trace_log(0xFF000000, _FLN_, __VA_ARGS__)

#define CDRTTL(...)  cdr_tmp_trace_log(0x000000FF, _FLN_, __VA_ARGS__)

//audit log


extern void cdr_audit_log(int entry_type, int tr_num, long long int partition_num, char *cmp, long long int size, char *operation, char *discription, long long int tot_time, char *user, char *nv_client_id); 

#define CDRAL(entry_type, tr_num, partition_num, cmp, size, operation, discription, tot_time, user)  cdr_audit_log(entry_type, tr_num, partition_num, cmp, size, operation, discription, tot_time, user, NULL)

#define CDRNVAL(entry_type, nv_client_id, partition_num, cmp, size, operation, discription, tot_time, user)  cdr_audit_log(entry_type, 0, partition_num, cmp, size, operation, discription, tot_time, user, nv_client_id)

extern int g_debug_level;
	//End of file
