#ifndef NS_LOG_REQ_REP_H
#define NS_LOG_REQ_REP_H

#define SAVE_REQ_REP_FILES \
 char req_rep_file_path[256]; \
 if(vptr->partition_idx <= 0) \
  sprintf(req_rep_file_path, "TR%d/ns_logs/req_rep", testidx); \
 else \
  sprintf(req_rep_file_path, "TR%d/%lld/ns_logs/req_rep", testidx, vptr->partition_idx);

#define LOG_HTTP_REQ(my_cptr, buf, bytes_to_log, complete_data, first_trace_write_flag)\
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL) \
                            && ((my_cptr->url_num->proto.http.type == EMBEDDED_URL \
                                && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) \
                                || my_cptr->url_num->proto.http.type != EMBEDDED_URL)) \
  { \
    log_http_req(my_cptr, vptr, buf, bytes_to_log, complete_data, first_trace_write_flag); \
  } 

#define LOG_HTTP_RES(my_cptr, buf, bytes_to_log)\
  VUser *vptr; \
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL) \
                            && ((my_cptr->url_num->proto.http.type == EMBEDDED_URL \
                                 && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) \
                                 || my_cptr->url_num->proto.http.type != EMBEDDED_URL)) \
  { \
    log_http_res(my_cptr, vptr, buf, bytes_to_log);\
  }

#define LOG_HTTP_RES_BODY(my_cptr, vptr, buf, bytes_to_log)\
  if (NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                 || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                             && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)) \
  { \
    log_http_res_body(vptr, buf, bytes_to_log);\
  }

#define CACHE_LOG_CACHE_REQ(my_cptr, buf, bytes_to_log)\
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL) \
                            && ((my_cptr->url_num->proto.http.type == EMBEDDED_URL \
                                && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) \
                                || my_cptr->url_num->proto.http.type != EMBEDDED_URL)) \
  { \
    cache_debug_log_cache_req(my_cptr, buf, bytes_to_log);\
  } 

#define CACHE_LOG_CACHE_RES(my_cptr)\
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL) \
                            && ((my_cptr->url_num->proto.http.type == EMBEDDED_URL \
                                && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) \
                                || my_cptr->url_num->proto.http.type != EMBEDDED_URL)) \
  { \
    cache_debug_log_cache_res(my_cptr);\
  } 

#define MAX_RESP_BODY_FILE_LEN 1024
#define OPEN_AND_DUMP(file_name, buffer, size) \
{ \
  int file_fd = 0; \
  if((file_fd = open(file_name, O_CREAT|O_WRONLY|O_CLOEXEC|O_APPEND|O_LARGEFILE, 00666)) < 0) \
  { \
    NSDL3_HTTP(NULL, NULL, "Error: error in opening file %s", file_name); \
    fprintf(stderr, "Error: save_http_resp_body() - error in opening file %s\n", file_name); \
    return -1; \
  } \
 \
  NSDL2_HTTP(NULL, NULL, "Writing data in file %s for fd %d, size = %d", file_name, file_fd, size); \
  write(file_fd, buffer, size); \
  close(file_fd); \
}

extern int g_parent_idx;
extern void debug_log_http_res(connection *cptr, char *buf, int size);
extern void log_http_res(connection *cptr, VUser *vptr, char *buf, int size);

extern void debug_log_http_res_line_break(connection *cptr);

extern void debug_log_http_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern void log_http_req(connection *cptr, VUser *vptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);

extern void debug_log_http_res_body(VUser* vptr, char *buf, int size);
extern void log_http_res_body(VUser* vptr, char *buf, int size);
extern void amf_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors);
extern void hessian_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors);
extern void cache_debug_log_cache_res(connection *cptr);
extern void cache_debug_log_cache_req(connection *cptr, char *parametrized_url, int parametrized_url_len);
extern int save_http_resp_body(connection *cptr, char *buf, int size, int total_size);
extern char* create_url_name (connection *cptr, char *url_name);
extern void java_obj_debug_log_http_req(connection *cptr, struct iovec *vector, int num_vectors);

extern void debug_log_http_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
#endif
