#ifndef __NS_HTTP_PIPELINING_H__
#define __NS_HTTP_PIPELINING_H__


extern int pipeline_connection(VUser *vptr, connection* cptr, u_ns_ts_t now);

extern int kw_set_g_enable_pipelining(char *text, GroupSettings *gset, char *err_msg);
extern int get_any_pipeline_enabled();
extern void kw_set_max_pipeline(char *text, Global_data *glob_set, int flag);
extern action_request_Shr *get_top_url_num(connection *cptr);
extern void inline setup_cptr_for_pipelining(connection *cptr);
extern void validate_pipeline_keyword();
#endif  /*  __NS_HTTP_PIPELINING_H__ */
