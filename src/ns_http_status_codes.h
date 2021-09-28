#include "ns_alloc.h"
#include "ns_string.h"
#include "util.h"
#include "ns_test_gdf.h"
#include "divide_users.h"
#include "divide_values.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "netstorm.h"
#include "ns_child_msg_com.h"


#ifndef __NS_HTTP_STATUS_CODES_H__
#define __NS_HTTP_STATUS_CODES_H__

#define INIT_HTTP_STATUS_CODE_ARR_SIZE  			50
#define MAX_HTTP_CODE_DISPLAY_NAME      			128
#define MAX_HTTP_RESP_CODES_ALLOWED				254   // total code allowed
#define MAX_HTTP_RESP_CODE                   		    	999   // Maximum http status code allowed
#define TOTAL_HTTP_RESP_CODES_WITH_OTHER			total_http_resp_codes

#define DELTA_STATUS_CODE_ENTRIES				5    //Keeping status code delta discovery as 5

#define HTTP_STATUS_CODE_OTHERS                                 "Others"
#define HTTP_STATUS_CODE_OTHERS_LEN                             6

typedef struct http_status_code_loc2norm_table_t
{
  int loc2norm_http_status_code_alloc_size;   //store number of status code occurence
  int *nvm_http_status_code_loc2norm_table;
  int loc_http_status_code_avg_idx;           //Point to particular generator's avgtime
  int num_entries;
  int dyn_total_entries;
  int last_gen_local_norm_id;
} HTTP_Status_Code_loc2norm_table;

typedef struct {
  short status_code_norm_idx;
  char status_text[MAX_HTTP_CODE_DISPLAY_NAME + 1];
  char display;
} HTTPRespCodeInfo;

typedef struct {
  int http_resp_code_count;
}HTTPRespCodeAvgTime;

typedef struct {
  Long_data http_resp_code_count;
} Http_Status_Codes_gp;

extern HTTPRespCodeInfo *http_resp_code_info;
#ifndef CAV_MAIN
extern HTTP_Status_Code_loc2norm_table *g_http_status_code_loc2norm_table;
extern int http_resp_code_avgtime_idx;                   //Index of HTTPRespCodeAvgTime structure
extern HTTPRespCodeAvgTime *http_resp_code_avgtime; 
#else
extern __thread HTTP_Status_Code_loc2norm_table *g_http_status_code_loc2norm_table;
extern __thread int http_resp_code_avgtime_idx;                   //Index of HTTPRespCodeAvgTime structure
extern __thread HTTPRespCodeAvgTime *http_resp_code_avgtime; 
#endif
#define CHILD_RESET_HTTP_STATUS_CODE_AVGTIME() { \
        for (i = 0; i < total_http_resp_code_entries; i++) { \
          http_resp_code_avgtime[i].http_resp_code_count = 0;\
      }\
}

//Need to accumulate data only for first sample else.. just assign
#define ACC_HTTP_RESP_CODE_PERIODICS(count, a, b, child_id) { \
  NSDL2_MESSAGES(NULL, NULL, "At ACC_HTTP_RESP_CODE_PERIODICS : count = %d, child_id = %d", count, child_id); \
  for(i = 0; i < count; i++) {             \
    int parent_norm_id = g_http_status_code_loc2norm_table[child_id].nvm_http_status_code_loc2norm_table[i];\
    NSDL2_MESSAGES(NULL, NULL, "At ACC_HTTP_RESP_CODE_PERIODICS : parent_norm_id = %d, cur_count = %d", parent_norm_id, b[i].http_resp_code_count); \
    if(parent_norm_id >= 0){\
      a[parent_norm_id].http_resp_code_count += b[i].http_resp_code_count ;\
      NSDL2_MESSAGES(NULL, NULL, "At ACC_HTTP_RESP_CODE_PERIODICS : http_resp_code_count = %d", a[parent_norm_id].http_resp_code_count); \
    }\
   }\
 }

#define COPY_PERIODICS_STATUS_CODE(start_idx, total_idx, a, b) {\
  for (i = start_idx ; i < total_idx; i++) { \
    NSDL2_MESSAGES(NULL, NULL, "At COPY_PERIODICS_STATUS_CODE : cur : http_resp_code_count = %d", a[i].http_resp_code_count); \
    a[i].http_resp_code_count += b[i].http_resp_code_count; \
    NSDL2_MESSAGES(NULL, NULL, "At COPY_PERIODICS_STATUS_CODE : next : http_resp_code_count = %d", a[i].http_resp_code_count); \
  } \
}

extern NormObjKey status_code_normtbl;

extern void init_http_response_codes();
extern void update_http_status_codes(connection *cptr, avgtime *average_time);
extern void update_http_resp_code_avgtime_size();
extern void set_http_resp_code_avgtime_ptr();
extern void fill_http_status_codes_gp();
extern void update_status_code_avg(connection *cptr, avgtime *average_time);
extern void fill_http_status_codes(avgtime **average_time);
extern void set_http_status_codes_count(VUser *vptr, int status_code, int count, avgtime *average_time);
extern char **print_resp_status_code_gdf_grp_vectors(int gid);
extern int ns_add_dyn_status_code(short nvmindex, int local_dyn_status_code_id, char *status_name, short status_name_len, int *flag_new);

/* HTTP Status Codes data structures */
extern Http_Status_Codes_gp *http_status_codes_gp_ptr;
extern int http_status_codes_gp_idx;
int total_http_resp_codes;
extern int total_http_status_codes;

extern char* get_http_status_text(int code);
#endif  /* __NS_HTTP_STATUS_CODES_H__ */
