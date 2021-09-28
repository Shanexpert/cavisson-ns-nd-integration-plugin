#ifndef _NS_HTTP_SCRIPT_PARSE_H_
#define _NS_HTTP_SCRIPT_PARSE_H_

extern int set_header_flags(char *http_hdr, int url_idx, int sess_idx, char *flow_file);
extern void get_post_content_dyn_vars(FILE* fp, char* line, int req_idx, int line_num);
extern void save_and_segment_body(int *rnum, int req_index, 
                                  char *body_buf, int noparam_flag, int *line_number,
                                  int sess_idx, char *file_name );
extern int post_process_post_buf(int req_index, int sess_idx, int * line_number, char *cap_fname);
extern int init_pre_post_url_callback(int index, void *handle);
extern void free_post_buf();
extern int parse_http_hdr(char *http_hdr, char *all_http_hdrs, int line_num, char *fname, int url_idx, int sess_idx);
extern int parse_web_url(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx, char inline_enabled);
extern int init_url(int *url_idx, int embedded_urls, char *flow_file, char inline_enabled);
extern int set_url(char *url, char *flow_file, int sess_idx, int url_idx, char embedded_url, char inline_enabled, char *apiname);
extern int set_url_internal(char *url, char *flow_file, int sess_idx, int url_idx, char embedded_url, char inline_enabled, char *apiname);
extern int set_http_method(char *method, char *flow_file, int url_idx, int line_number);
extern int parse_and_set_pagename(char *api_name, char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                  FILE *outfp,  char *flow_outfile, int sess_idx, char **page_end_ptr, char *pagename);
extern int ns_parse_websocket_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_websocket_connect(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);

extern int ns_parse_xmpp_login(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_create_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_delete_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_add_contact(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_delete_contact(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_add_member(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_delete_member(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_join_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int ns_parse_xmpp_leave_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx);
extern int add_static_file_entry(char *file, int sess_idx);
extern int add_all_static_files(int sess_idx);

extern void init_web_url_page_id();
extern int find_http_method_idx(char *method);
extern void copy_http_method_shr(void);
extern void load_http_methods(void);
extern void free_http_method_table();
extern void init_http_method_table(void);
extern void validate_body_and_ignore_last_spaces(void);
extern int validate_body_and_ignore_last_spaces_c_type(int sess_idx);
extern int set_fully_qualified_url(char *fully_qualified_url_value, char *flow_file, int url_idx, int script_ln_no);

extern long get_tti_profile(char *prof, int url_idx, int sess_idx, int script_ln_no, char *flow_file); 
extern int ns_parse_jrmi_ex(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_file, char *flowout_file, int sess_idx);
extern int check_duplicate_pagenames(char *pagename, int sess_idx);
extern int extract_body(FILE *flow_fp, char *start_ptr, char **end_ptr, char embedded_url, char *flow_file, int body_begin_flag, FILE *outfp);
extern  int set_body(int url_idx, int sess_idx, char *body_start, char *body_end, char *flow_file);
extern int copy_to_post_buf(char *buf, int blen);
extern void save_and_segment_ws_body(int send_tbl_idx, char *body_buf, int noparam_flag, int *script_ln_no, int sess_idx, char *file_name);
extern int set_rbu_param(char *val, char *flow_file, int url_idx, int sess_idx, int script_ln_no, int rbu_param_flag);
extern int ns_rbu_parse_tti_json_file(int sess_idx);
extern int create_and_fill_sess_host_table_entry(SessTableEntry *sess_table, int sess_idx, unsigned long host_idx);
extern char *nslb_decode_base64(char *input, int oplen, char *output);
extern int set_body_encryption_param(char *attribute_value, char *flow_file, int url_idx, int sess_idx, int script_ln_no);
// structure for storing http method 
typedef struct http_method_t {
  ns_bigbuf_t method_name; // Index in bigbuf
  int method_name_len; // Length of method name
} http_method_t;

typedef struct http_method_t_shr {
  char *method_name;  // Pointer to shared memory bigbuf
  int method_name_len; // Length of method name
} http_method_t_shr;
 
#ifndef CAV_MAIN
extern http_method_t_shr *http_method_table_shr_mem;
extern int web_url_page_id;
#else
extern __thread http_method_t_shr *http_method_table_shr_mem;
extern __thread int web_url_page_id;
#endif
#endif
