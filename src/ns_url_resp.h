#ifndef _NS_URL_RESP_BUF_H_
#define _NS_URL_RESP_BUF_H_

inline void on_url_start(connection *cptr, u_ns_ts_t now);

// 02/01/07 - Achint Start - For Pre and Post URL Callback
//void on_url_done(connection *cptr, int is_redirect, int);
void on_url_done(connection *cptr, int is_redirect, int, u_ns_ts_t now);
// Achint End

void do_data_processing( connection *cptr, u_ns_ts_t now , int depth, int free_buffer_flag);
char *cptr_to_string(connection *cptr, char *str, int size);
extern char *vptr_to_string(VUser *vptr, char *str, int size);
extern void copy_url_resp(connection *cptr);
extern char *get_reply_buffer(connection *cptr, int *blen, int present_depth, int free_buffer_flag);

extern char *full_buffer;
extern char *hessian_buffer;
#endif /* _NS_URL_RESP_BUF_H_ */
