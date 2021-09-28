#ifndef NS_SCRIPT_PARSE_H
#define NS_SCRIPT_PARSE_H

#include <dlfcn.h>
#include "ns_cache_include.h"
#include "ns_trans_parse.h"
#include "ns_http_script_parse.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_error_msg.h"

/* Defferent modes of script*/
#define NS_SCRIPT_TYPE_LEGACY 0
#define NS_SCRIPT_TYPE_C      1 
#define NS_SCRIPT_TYPE_JAVA   2 
#define NS_SCRIPT_TYPE_JMETER 3 

#define NS_DO_NOT_TRIM           0
#define NS_TRIM                  1
#define MAX_FILE_NAME_LEN        512

#define NS_PARSE_SCRIPT_ERROR           -1
#define NS_PARSE_SCRIPT_SUCCESS         0

#define MAX_SCRIPT_FILE_NAME_LEN 1152  // 1024 + 128

#define NS_REGISTRATION_FILENAME "registrations.spec"

#define HANDLE_URL_PARAM_FAILURE(vptr) \
{ \
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD) \
  { \
    if((vptr->first_page_url->request_type == WS_REQUEST) || (vptr->first_page_url->request_type == WSS_REQUEST))\
      send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_CLOSE_REP, 0); \
    else\
      send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0); \
  } \
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA) \
  { \
    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_URL_REP, -1); \
  } \
  else \
  { \
    switch_to_vuser_ctx(vptr, "WebUrlFailed"); \
  } \
}

extern char indent[];
#ifndef CAV_MAIN
extern int  script_ln_no;
extern char *script_line;
extern char *script_name;
extern char *flow_filename;
extern char parse_tti_file_done;
#else
extern __thread int  script_ln_no;
extern __thread char *script_line;
extern __thread char *script_name;
extern __thread char *flow_filename;
extern __thread char parse_tti_file_done;
#endif
extern int first_flow_page;
extern char  g_dont_skip_test_metrics;// This flag tells whether to skip test metric or not

typedef struct 
{
  char orig_filename[255 + 1];
  char file_withoutcomments[255 + 1];
  char flow_execution_file[255 + 1];
} FlowFileList_st;

extern int get_flow_filelist(char *path, FlowFileList_st **filelist_arr, int sess_idx);
extern int parse_tx_from_file(char *file_name , int sess_idx);
extern int get_protocols_from_flow_files(char *script_filepath, FlowFileList_st *script_filelist, int num_files);
extern int parse_flow_file(char *flow_filename, char *flowout_filename, int sess_idx, char inline_enabled);

extern char * read_line (FILE *flow_fp, short trim_line);
extern char * read_line_and_comment (FILE *flow_fp, FILE *outfp);

extern inline int write_line(FILE *fp, char *line, char new_line);

extern int get_next_argument(FILE *flow_fp, char *starting_quotes, char *attribute_name,
                               char *attribute_value, char **closing_quotes, char read_body);
extern int get_script_type(char *path, int *script_type, char *version, int sess_idx);

extern int parse_script(int sess_idx, char *script_filepath);

extern void log_script_parse_error(int do_exit, char *err_msg_code, char *file, int line, char *fname, char *line_buf, char *format, ...);

extern int read_till_start_of_next_quotes(FILE *flow_fp, char *flow_file, char *closing_quotes,
                                        char **starting_quotes, char embedded_urls, FILE *outfp);

extern int extract_pagename(FILE *flow_fp, char *flow_file, char *line_ptr, char *pagename, char **page_end_ptr);

extern int ns_parse_registration_file(char *registration_file, int sess_idx, int *reg_ln_no);

extern int parse_ns_timer(char *buffer, char *fname, int session_idx);

extern int set_headers(FILE *flow_fp, char *flow_file, char *header_val, char *header_buf, int sess_idx, int url_idx);
extern void skip_test_metrices(); 
extern void get_used_runlogic_unique_list(int sess_idx, char **list);
extern void runprof_save_runlogic(int sess_idx, void *handle);
#endif //NS_SCRIPT_PARSE_H
