#ifndef _NS_VARS_H_
#define _NS_VARS_H_

#include "nslb_search_vars.h" /*search api and XML Var api macros are moved to libnscore/nslb_search_vars.h */
#include "ns_exit.h"

#define VALUE 0
#define COUNT 1
#define NUM   2
#define ITEMDATA_HASH_ARRAY_SIZE 500

#define END_TEST_RUN_MSG\
   if (my_port_index == 255){ \
     kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__); \
     NS_EXIT(-1, "All children got killed.");             \
   }                       \
   else \
   end_test_run_ex (error_msg, USE_ONCE_ERROR);

int get_var_element(FILE* fp, char* buf, int* line_number, char* fname);
int read_urlvar_file(FILE* fp, int session_idx, int* line_number, char* fname);
int create_variable_functions(int sess_idx);
int get_value_index(GroupTableEntry_Shr* groupTableEntry, VUser *vptr, char* var_name);
extern inline PointerTableEntry_Shr* get_var_val(VUser* vptr, int var_val_flag, int var_hashcode);
void process_tag_vars_from_url_resp(VUser *vptr, char *full_buffer, int blen, int present_depth);
void process_search_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int blen, int depth);
void process_checkpoint_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int full_buffer_len, int *outlen);
void process_check_replysize_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int *outlen);
void segment_line(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname);
extern int amf_segment_line(StrEnt* segtable, char* line, int no_param_flag, int line_number, int sess_idx, char *fname);
void segment_line_noparam (StrEnt* segtable, char* line,int sess_idx);
void set_page_status_for_registration_api(VUser *vptr, int pg_status, int to_stop, char *api_name);
int set_depth_bitmask(unsigned int redirection_depth_bitmask, int redirection_depth);
int check_redirection_limit(int check_buf); 
void segment_line_noparam_multipart (StrEnt* segtable, char* line, int sess_idx);
void segment_line_int(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname, int first_segment);
void segment_line_int_int(StrEnt* segtable, char* line, int url, int line_number, int sess_idx, char *fname, int first_segment, int *hash_arr);
extern void process_json_vars_from_url_resp(connection *cptr, VUser *vptr, char *full_buffer, int blen, int present_depth);
extern inline void apply_checkpoint(CheckPointTableEntry_Shr* checkpoint_ptr, int *count, int *page_fail, int *to_stop, char *full_buffer, int full_buffer_len, VUser *vptr, connection *cptr);

extern int ns_protobuf_segment_line(int url_idx, StrEnt* segtable, char* line, int noparam_flag,
                                    int line_number, int sess_idx, char *fname, char *err_msg, int err_msg_len);
#endif /* _NS_VARS_H_ */
