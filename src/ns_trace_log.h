/**
 * Name             : ns_trace_log.h
 * Author           : Shri Chandra
 * Purpose          : This file contains declaration of methods
 * Initial Version  : 03/11/09
 * Modification Date:

**/

#ifndef _NS_TRACE_LOG_H_
#define _NS_TRACE_LOG_H_

//Narendra(26/10/2013): Change handling of second (snwritten >= snleft) case
//The functions snprintf() and vsnprintf() do not write more than size bytes  (including the trailing '\0')
//but in some old cases it was not writing \0 in case of overflow so to handle both the implementation we are
//checking last byte of buffer if that is \0 then reduce snwritten by 1
#define HANDLE_SNPRINT_ERRCASE(snbuf, snwritten, total_written, snleft) { \
        if(snwritten < 0) {                                \
            snwritten = strlen(snbuf);                     \
         } else if (snwritten >= snleft) {                  \
           snwritten = snleft;                              \
           if(!snbuf[total_written + snleft - 1])           \
             snwritten --;                                  \
         }                                                 \
      }
#define MAX_LOG_SPACE_FOR_SNAPSHOT 1024
#define PCT_MULTIPLIER 100
extern int log_message_record2(unsigned int msg_num, u_ns_ts_t now, char* buf, int buf_length, char *buf2, int len2);

extern int msg_num;
void do_trace_log(connection *cptr, VUser *vptr, int blen, char *log_space, int total_bytes_copied, int page_status_point, u_ns_ts_t now);
extern int get_parameter_name_value(VUser* vptr, int used_param_id, char **name, int *name_len, int *vector_var_idx, char **value, int *value_len);
extern int get_parameters( connection *cptr, char* log_space, VUser* vptr, int max_bytes, int log_size, int *page_status_point);
extern void init_trace_up_t(VUser *vptr);
extern void free_trace_up_t(VUser *vptr);
extern void ns_save_used_param(VUser *vptr, SegTableEntry_Shr *seg_ptr, char *value, int value_len, char type, int malloc_flag, unsigned short cur_seq);
extern void make_page_dump_buff(connection *cptr, VUser* vptr, u_ns_ts_t now, int blen, int *page_status_point, int *total_bytes_copied);
extern int inline encode_parameter_value(char *name, char *value, char* log_space, int max_bytes , int *complete_flag, int first_parameter);
extern int copy_and_escape(char *in, int in_len, char *out, int out_max);
#endif /* _NS_TRACE_LOG_H_*/


