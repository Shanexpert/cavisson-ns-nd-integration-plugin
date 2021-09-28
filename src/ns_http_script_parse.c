#define _GNU_SOURCE 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>  //not needed on IRIX 
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>

#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_check_replysize_vars.h"
#include "ns_server.h"


#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"


#include "netstorm.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_alloc.h"

#include "ns_http_script_parse.h"
#include "ns_script_parse.h"
#include "ns_http_cache_hdr.h"

#include "ns_url_hash.h"
#include "ns_websocket.h"
#include "ns_rbu.h"
#include "ns_exit.h"

#include "ns_protobuf.h"
#include "comp_decomp/nslb_comp_decomp.h"

//if request type is set to GRPC 
#define GRPC (requests[url_idx].flags & NS_REQ_FLAGS_GRPC_CLIENT)  
#define SET_HDR_FLAG(X) requests[url_idx].hdr_flags |= X
#define CHECK_HDR_FLAG(X) requests[url_idx].hdr_flags & X

// TODO: Move end_test_run extern to some other header file as ns_child_msg_com.h has many dependencies
//extern void end_test_run( void );


// #include "ns_child_msg_com.h" 

//Post buf related vars
//Size of the post buf increments
#ifndef CAV_MAIN
static int max_post_buf_len;    
char *g_post_buf; // Needed in other protocol files also
int cur_post_buf_len; // Needed in other protocol files also
int rbu_web_url_host_id = -1;
int end_inline_url_count ; 
extern int page_num_relative_to_flow;
#else
static __thread int max_post_buf_len;    
__thread char *g_post_buf; // Needed in other protocol files also
extern __thread int cur_post_buf_len; // Needed in other protocol files also
__thread int rbu_web_url_host_id = -1;
__thread int end_inline_url_count ; 
extern __thread int page_num_relative_to_flow;
#endif


//static char pagename[MAX_LINE_LENGTH + 1];


/********************Multipart Body Start***********************/
/* Boundary delimiters must not appear within the encapsulated material, and must be no longer than 70 characters, not counting the two
 *  leading hyphens.*/
#define MAX_BOUNDARY_LEN  128    //len of the <x> in boundary=<x> for multipart header
#define MAX_MULTIPART_BOUNDARIES  32    //max no of boundaries. The main header contains above, and each part of the 
                                        // multipart body contains --<x> as separator
#define MAX_MULTIPART_BODIES_PER_BOUNDARY 32
//Total no of body parts - anything within BEGIN and END is a part
#define MAX_MULTIPART_BODIES      (MAX_MULTIPART_BOUNDARIES*MAX_MULTIPART_BODIES_PER_BOUNDARY)  
char *g_multipart_body_post_buf[MAX_MULTIPART_BODIES];
int noparam_array[MAX_MULTIPART_BODIES];
char (*multipart_boundaries)[MAX_BOUNDARY_LEN] = NULL;
int multipart_body, multipart_body_begin=0;
int multipart_bdry_index =0, multipart_body_parts_count =0;
typedef struct {
  //int bdry_index;      //index into multipart_boundaries that contains boundary
  int len;              //len of this part 
  int cur_buf_len;     //track the current buffer size used for g_multipart_body_post_buf[index of this multipart_body_parts]
  int max_buf_len;     //max buffer size allocated g_multipart_body_post_buf[index of this multipart_body_parts]
  int content_type;    //Content-Type in the header for this body part
} multipartBodyParts; 

multipartBodyParts multipart_body_parts[MAX_MULTIPART_BODIES] = {[0 ... MAX_MULTIPART_BODIES-1] = {0,0,0,0} };
enum {CONTENT_TYPE_TEXT_PLAIN=1, CONTENT_TYPE_MULTIPART_MIXED, CONTENT_TYPE_MULTIPART_FORM_DATA, CONTENT_TYPE_OTHER};

/********************Multipart Body End*************************/

/************************for http method************************/
#define DELTA_METHOD_ENTRIES 7
#ifndef CAV_MAIN
int web_url_page_id = 0;
static http_method_t     *http_method_table = NULL; // for http method
http_method_t_shr *http_method_table_shr_mem = NULL; //for http method share mem
static int max_http_method = 0;
static int total_http_method = 0;
#else
__thread int web_url_page_id = 0;
static __thread http_method_t     *http_method_table = NULL; // for http method
__thread http_method_t_shr *http_method_table_shr_mem = NULL; //for http method share mem
static __thread int max_http_method = 0;
__thread int total_http_method = 0;
#endif


int create_http_method_table_entry(int *table_idx)
{
  NSDL2_PARSING(NULL, NULL, "Method called. max_http_method = %d, total_http_method = %d, http_method_table = %p", max_http_method, total_http_method, http_method_table);

  if(total_http_method == max_http_method)
  {
    MY_REALLOC_EX(http_method_table, (max_http_method + DELTA_METHOD_ENTRIES) * sizeof(http_method_t), max_http_method * sizeof(http_method_t), "http_method_table", -1);
    if(!http_method_table) {
      NS_EXIT(-1, "Error - create_http_method_table_entry(): Error allocating more memory for http_method_table");
    } else max_http_method += DELTA_METHOD_ENTRIES;
  }
  *table_idx = total_http_method++;
  return SUCCESS;
}

// Must be called from util.c like other init function
void init_http_method_table(void)
{
  NSDL2_MISC(NULL, NULL, "Method called.");

  total_http_method = max_http_method = 0;

  MY_MALLOC (http_method_table, DELTA_METHOD_ENTRIES * sizeof(http_method_t), "http_method_table", -1);
  max_http_method = DELTA_METHOD_ENTRIES;
}


// This method is used to search the method in http_method_table
// if method is found then it returns the index of the method.
//if method is not found then a new entry is created and method is stored in table and index of that entry is returned
int find_http_method_idx(char *method)
{
char method_to_search[MAX_LINE_LENGTH];
int table_idx;
int len;
char *ptr;

  NSDL2_PARSING(NULL, NULL, "Method called. Method = %s, total_http_method = %d", method, total_http_method);

  // We keep method names with space so that we can put in request line
  // GET /abc/index.html HTTP/1.1
  sprintf(method_to_search, "%s ", method); 
  len = strlen(method_to_search);

  for(table_idx =0; table_idx < total_http_method; table_idx++)
  {
    ptr = RETRIEVE_BUFFER_DATA(http_method_table[table_idx].method_name);
    NSDL3_PARSING(NULL, NULL, "Matching ptr = [%s], method = [%s]",
		           ptr, method);
    if(!strcmp(ptr, method_to_search)){
      NSDL3_PARSING(NULL, NULL, "Found http method im method table [%s], table_idx = %d", ptr, table_idx);
      return table_idx;
    }
  }

  NSDL3_PARSING(NULL, NULL, "Not Found http method im method table [%s]", method);

  create_http_method_table_entry(&table_idx);
  if((http_method_table[table_idx].method_name = copy_into_big_buf(method_to_search, len)) == -1){
    NS_EXIT(-1, CAV_ERR_1000018, method_to_search);
  }
  http_method_table[table_idx].method_name_len = len;
  return table_idx;
}

void copy_http_method_shr(void)
{  
int table_idx;

  NSDL2_PARSING(NULL, NULL, "Method called, total_http_method = %d", total_http_method);

  if(total_http_method == 0){
    NS_EXIT(-1, "Do not have anything to copy, total_http_method is 0");
  }

  http_method_table_shr_mem = (http_method_t_shr*) do_shmget(sizeof(http_method_t_shr) * total_http_method, "http method table");
  for(table_idx = 0; table_idx < total_http_method; table_idx++)
  {
    http_method_table_shr_mem[table_idx].method_name = BIG_BUF_MEMORY_CONVERSION(http_method_table[table_idx].method_name);
    http_method_table_shr_mem[table_idx].method_name_len = http_method_table[table_idx].method_name_len; 
    NSDL2_PARSING(NULL, NULL, "Method name[%d] = [%s], len = [%d]", 
		             table_idx, http_method_table_shr_mem[table_idx].method_name,
		              http_method_table_shr_mem[table_idx].method_name_len);
  }
  NSDL2_PARSING(NULL, NULL, "http method is copied to share memory");  
}

void free_http_method_table()
{
  NSDL2_PARSING(NULL, NULL, "Method called. http_method_table = %p", http_method_table);
  FREE_AND_MAKE_NULL_EX (http_method_table, max_http_method * sizeof(http_method_t), "http_method_table", -1);
}

// This method will load all standard method names
// All standard methods MUST be in the correct index as per HTTP method #defines
// We can use invalid methods in scripts for negative testing.
// So if script is using standard methods, then it is aleady loaded. Invalid methods will be 
// loaded as we parse these methods in the script
void load_http_methods(void)
{
  NSDL2_PARSING(NULL, NULL,"method called");
  find_http_method_idx("NA");  // Index 0 is reserved and not used. So fill NA
  find_http_method_idx("GET"); // Get must be at index 1
  find_http_method_idx("POST"); // Post must be at index 2
  find_http_method_idx("HEAD"); // Head must be at index 3
  find_http_method_idx("PUT");  // Put must be at index 4
  find_http_method_idx("PATCH");  // Patch must be at index 5
  find_http_method_idx("DELETE"); // Delete must be at index 6 (Method used is POST)
  find_http_method_idx("OPTIONS"); // Options must be at index 7 (Method used is POST)
  find_http_method_idx("TRACE");   // Trace must be at index 8 (Method used is POST)
  find_http_method_idx("CONNECT"); // CONNECT must be at index 9
                                   // This is used only for CONNECT method when connection will make vai proxy 
}
// End of http method data strcuture

#define DELTA_POST_BUF_LEN 100*1024

// return 1 if skip & continue else 0
int set_header_flags(char *http_hdr, int url_idx, int sess_idx, char *flow_file) 
{
  char *content_type_val;

  NSDL2_PARSING(NULL, NULL, "Method called. http_hdr = %s, url_idx = %d, sess_idx = %d, flow_file = %s  java_obj_mgr = [%d]", http_hdr, url_idx, sess_idx, flow_file, global_settings->use_java_obj_mgr);

  if (strncasecmp(http_hdr, "Content-Type:", 13) == 0) 
  {
    SET_HDR_FLAG(NS_REQ_HDR_FLAGS_CONTENT_TYPE);
    content_type_val = http_hdr + 13;
    CLEAR_WHITE_SPACE(content_type_val);
    //Setting Encoding Type
    if (strstr(content_type_val, "x-www-form-urlencoded"))
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to URL_ENCODED for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_URL_ENCODED;
    }
    else if (strstr(content_type_val, "application/x-amf"))
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to AMF for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_AMF;
    }
    else if (strstr(content_type_val, "application/x-hessian") || strstr(content_type_val, "x-application/hessian"))
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to hessian for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_HESSIAN;
    }
    else if (strstr(content_type_val, "application/octet-stream") && (global_settings->use_java_obj_mgr))
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to java object for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_JAVA_OBJ;
    }
    else if (strstr(content_type_val, "application/x-protobuf") || strstr(content_type_val, "x-application/protobuf"))
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to protobuf for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_PROTOBUF;
    }
    else if (strstr(content_type_val, "application/grpc+json"))     
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to grpc for header value received at script_line %d in file %s",
                              script_ln_no, flow_file); 
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_GRPC_JSON;
    }
    /*
    else if (strstr(content_type_val, "application/grpc+proto"))     
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to grpc for header value received at script_line %d in file %s",
                              script_ln_no, flow_file); 
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_GRPC_PROTO;
    }*/
    else if (strstr(content_type_val, "application/grpc"))     
    {
      NSDL2_PARSING(NULL, NULL, "Body Encoding set to grpc for header value received at script_line %d in file %s",
                              script_ln_no, flow_file); 
      requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_GRPC_PROTO;
    }
 
    //Setting text/Javascript
    if (strstr(content_type_val, "text/javascript") || (runProfTable[sess_idx].gset.js_all == 1))
    {
      NSDL2_PARSING(NULL, NULL, "Http Header Flags set to NS_URL_KEEP_IN_CACHE as	\
		text/javascript received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.header_flags |= NS_URL_KEEP_IN_CACHE;
    }

    //multipart message
    if (strstr(content_type_val, "multipart")) {
      NSDL2_PARSING(NULL, NULL, "Http Header Flags set to NS_MULTIPART as multipart/<something> received at script_line %d in file %s",
          script_ln_no, flow_file);
      requests[url_idx].proto.http.header_flags |= NS_MULTIPART; 
    }
   
  } 
  //Bug 43675 - NetStorm is sending two "Authorization" header with second request if server is giving "401 Unauthorised" 
  else if(strncasecmp(http_hdr, "Authorization:", 14) == 0)
  {
    NSDL2_PARSING(NULL, NULL, "'Authorization' Header Found. Setting NS_HTTP_AUTH_HDR flags.");
    requests[url_idx].proto.http.header_flags |= NS_HTTP_AUTH_HDR;
  } 

  if ((strncasecmp(http_hdr, "Expect: 100-continue", strlen("Expect: 100-continue")) == 0)) 
  {
    NSDL2_PARSING(NULL, NULL, "Http Header Flags set to NS_HTTP_100_CONTINUE_HDR at script_line %d in file %s",
                              script_ln_no, flow_file);
    requests[url_idx].proto.http.header_flags |= NS_HTTP_100_CONTINUE_HDR;

    /* Expect continue is only for POST body*/
    if(requests[url_idx].proto.http.http_method != HTTP_METHOD_POST)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012076_ID, CAV_ERR_1012076_MSG);

   //pipelining issue TODO
    if(get_any_pipeline_enabled() == 1)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012077_ID, CAV_ERR_1012077_MSG);
  }

  //Parse "grpc-encoding:" header of grpc request
  if ((strncasecmp(http_hdr, "grpc-encoding:", strlen("grpc-encoding:")) == 0))
  {
    NSDL2_PARSING(NULL, NULL, "'grpc-encoding:' Header Found. Setting NS_HTTP_AUTH_HDR flags.");
    content_type_val = http_hdr + 14;
    CLEAR_WHITE_SPACE(content_type_val);
    if (strstr(content_type_val, "gzip"))
    {
      NSDL2_PARSING(NULL, NULL, "Message Encoding type set to 'gzip' for header value received at script_line %d in file %s",
                                 script_ln_no, flow_file);
      requests[url_idx].proto.http.protobuf_urlattr.grpc_comp_type = NSLB_COMP_GZIP;
    } 
    else if (strstr(content_type_val, "deflate"))
    {
      NSDL2_PARSING(NULL, NULL, "Message encoding type set to 'deflate' for header value received at script_line %d in file %s",
                              script_ln_no, flow_file);
      requests[url_idx].proto.http.protobuf_urlattr.grpc_comp_type = NSLB_COMP_DEFLATE;
    } 
  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

/*******************Mutltipart Body Start************************/


int validate_multipart_body_and_ignore_last_spaces_c_type(int body_part_index)
{
  char *last_quotes_index, *ptr;
  char comma_found = '0';
  int cur_post_buf_len = multipart_body_parts[body_part_index].cur_buf_len;

  NSDL2_PARSING(NULL, NULL, "Method called, cur_post_buf_len = %d", cur_post_buf_len);

  //Is this check valid in our case ? commenting for now
  // Body must have at least two bytes as even empty body must be like
  //  "Body=",  (Quotes, comma and newline)
  //  "Body=" (Quotes and newline)
  if ((cur_post_buf_len < 2) || ((g_multipart_body_post_buf[body_part_index][cur_post_buf_len - 1]) != '\n') )
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);

  // Point to the last quote in the post buf
  last_quotes_index = rindex(g_multipart_body_post_buf[body_part_index], '"');

  if(last_quotes_index == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);

  // Post buffer length from start to last character before quotes
  cur_post_buf_len = last_quotes_index - g_multipart_body_post_buf[body_part_index];
  g_multipart_body_post_buf[body_part_index][cur_post_buf_len] = 0; // Null terminate
  //reset actual length now
  multipart_body_parts[body_part_index].cur_buf_len = cur_post_buf_len ;

  ptr = last_quotes_index + 1;  //Will start with next character after "
  // Check that after last quote, there is only max one comma and white space
  while(*ptr != '\n')
  {
    if(*ptr == ',')
    {
      if(comma_found == '1')
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012058_ID, CAV_ERR_1012058_MSG);
      comma_found = '1';
    }
    else if(!isspace(*ptr))
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);
    }
    ptr++;
  }

  NSDL4_PARSING(NULL, NULL, "Exiting Method, cur_post_buf_len = %d,\nBody=[%s]", cur_post_buf_len, g_multipart_body_post_buf);
  return NS_PARSE_SCRIPT_SUCCESS;
}


//Called every time, data is added to post buf
int copy_to_multipart_body_post_buf(char *buf, int blen, int body_part_index, int validate_body)
{
  int cur_buf_len, max_buf_len; 
  max_buf_len = multipart_body_parts[body_part_index].max_buf_len;
  cur_buf_len = multipart_body_parts[body_part_index].cur_buf_len;

  NSDL4_PARSING(NULL, NULL, "Method called, buf = %s, blen = %d body_part_index %d cur_buf_len = %d max_buf_len =%d", buf, blen, body_part_index , cur_buf_len, max_buf_len);
 
  while (blen + cur_buf_len + 1 > max_buf_len) {
    MY_REALLOC_EX(g_multipart_body_post_buf[body_part_index], max_buf_len + DELTA_POST_BUF_LEN, max_buf_len, "g_post_multipart_body_buf", -1);
    max_buf_len += DELTA_POST_BUF_LEN;
    multipart_body_parts[body_part_index].max_buf_len = max_buf_len; 
  }
  
  memcpy(g_multipart_body_post_buf[body_part_index]+cur_buf_len, buf, blen);
  cur_buf_len += blen;
  multipart_body_parts[body_part_index].cur_buf_len = cur_buf_len; 
  multipart_body_parts[body_part_index].len = blen;    //do we need this? if so, len may change on validation below
  /* validate body and remove "\n at the end in the post buffer. actually we need to do it only for this body (buf), but only the end
   * is modified, so for now, use the whole body accumulated in the global buffer
   */
  g_multipart_body_post_buf[body_part_index][cur_buf_len] = 0;
  if (validate_body) { 
    validate_multipart_body_and_ignore_last_spaces_c_type(body_part_index);
  }
  return 0;
}

#define ADD_CUSTOM_BOUNDARY(page_name, index) \
  {\
   NSDL2_PARSING(NULL, NULL, "first header for multipart - allocating mem for boundaries"); \
   snprintf(multipart_boundaries[index], MAX_BOUNDARY_LEN, "--%s_%d_%d----------", page_name, testidx, index); \
  }  


int
parse_main_multipart_hdr(char *http_hdr)
{
  char *boundary_ptr = NULL;
  char *end_ptr = NULL;
  char save = '\0';

  MY_MALLOC(multipart_boundaries, MAX_BOUNDARY_LEN*MAX_MULTIPART_BOUNDARIES, "malloc for all boundaries of multipart body", -1);
  bzero(multipart_boundaries, MAX_BOUNDARY_LEN*multipart_bdry_index); 
  if ( (boundary_ptr = strstr(http_hdr, "boundary")) != NULL) {
    NSDL2_PARSING(NULL, NULL, "first header for multipart - allocating mem for boundaries");
  } else { // If boundary is not found, create custom boundary in format <pagename>_<TRno>_<index>---------- and return from method
    ADD_CUSTOM_BOUNDARY(RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name), multipart_bdry_index);
    return NS_PARSE_SCRIPT_SUCCESS; 
  }
  boundary_ptr = strchr(boundary_ptr, '='); 
  if(boundary_ptr == NULL)
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012059_ID, CAV_ERR_1012059_MSG, "main");
  boundary_ptr++; //step over '=' 
  CLEAR_WHITE_SPACE(boundary_ptr);
  // see is \" is present 
  if(*boundary_ptr == '\"'){
    boundary_ptr++;
    end_ptr = strchr(boundary_ptr, '\"');    
    if(end_ptr == NULL) 
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012060_ID, CAV_ERR_1012060_MSG);

    save = *end_ptr;
    *end_ptr = '\0';
  }
  NSDL2_PARSING(NULL, NULL, "boundary value %s", boundary_ptr);
  /* boundary must begin on a new line -separator boundaries are like this: --<boundary>
  */
  snprintf(multipart_boundaries[multipart_bdry_index],MAX_BOUNDARY_LEN, "%s%s", "--",boundary_ptr);
  if(save) 
    *end_ptr = save;
  return 0;
}

int
parse_multipart_hdr(char *http_hdr, char *all_http_hdrs, int sess_idx, char *flow_file)
{
  char *boundary_ptr = NULL;
  char *content_type_val;
 
  NSDL2_PARSING(NULL, NULL, "http_hdr = %s", http_hdr); 
   
  //check Content-Type and set a flag
  if (strncasecmp(http_hdr, "Content-Type:", 13) == 0) 
  {
    content_type_val = http_hdr + 13;
    CLEAR_WHITE_SPACE(content_type_val);
    //Setting Encoding Type
    if (strstr(content_type_val, "text/plain")) {
      multipart_body_parts[multipart_body_parts_count-1].content_type = CONTENT_TYPE_TEXT_PLAIN;
    } else if (strstr(content_type_val, "multipart/mixed")) {
      multipart_body_parts[multipart_body_parts_count-1].content_type = CONTENT_TYPE_MULTIPART_MIXED;
    } else if (strstr(content_type_val, "multipart/form-data")) {
      multipart_body_parts[multipart_body_parts_count-1].content_type = CONTENT_TYPE_MULTIPART_FORM_DATA;
    } else {
      multipart_body_parts[multipart_body_parts_count-1].content_type = CONTENT_TYPE_OTHER;
    }

    if(strncasecmp(content_type_val,"multipart", 9) == 0){
      if ( (boundary_ptr = strstr(http_hdr, "boundary")) != NULL) {
        multipart_bdry_index++;
        if (!multipart_body_begin) { //first header after BEGIN - boundary can appear only for BEGIN/END blocks inside the main BEGIN
              SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012061_ID, CAV_ERR_1012061_MSG, "outside MULTIPART_BODY");
        }
        //check - we cant have HEADER with boundary int outermost BEGIN block - because these go in the main header part
        if (multipart_bdry_index == 1) {
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012061_ID, CAV_ERR_1012061_MSG, "in the outer most BEGIN/END block");
        }

        NSDL2_PARSING(NULL, NULL, "header contains boundary - this is a part of a multipart body message. multipart_bdry_index %d", multipart_bdry_index);
        NSDL2_PARSING(NULL, NULL, "copying header with boundary %s to multipart post body",http_hdr);
        copy_to_multipart_body_post_buf(http_hdr, strlen(http_hdr), multipart_body_parts_count-1, 0);
        copy_to_multipart_body_post_buf("\xd\xa",2, multipart_body_parts_count-1, 0);     //end with CRLF
        if (multipart_bdry_index == MAX_MULTIPART_BOUNDARIES) { //limit exceede
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012062_ID, CAV_ERR_1012062_MSG);
        }
        boundary_ptr = strchr(boundary_ptr, '='); 
        if(boundary_ptr == NULL)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012059_ID, CAV_ERR_1012059_MSG, "after");

        boundary_ptr++; //step over '=' 
        char *end_ptr = NULL;
        CLEAR_WHITE_SPACE(boundary_ptr);
        // see is \" is present 
        if(*boundary_ptr == '\"'){
          boundary_ptr++;
          end_ptr = strchr(boundary_ptr, '\"');    
          if(end_ptr == NULL)
             SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012060_ID, CAV_ERR_1012060_MSG);
 
          *end_ptr = '\0';
        }
        NSDL2_PARSING(NULL, NULL, "boundary value %s", boundary_ptr);
        /* boundary must begin on a new line -separator boundaries are like this: --<boundary>
         */
        snprintf(multipart_boundaries[multipart_bdry_index-1],MAX_BOUNDARY_LEN, "%s%s", "--",boundary_ptr);

        NSDL2_PARSING(NULL, NULL, "content-type set to %d for at index %d header value received at script_line %d in file %s",
        multipart_body_parts[multipart_body_parts_count-1].content_type, multipart_body_parts_count-1, script_ln_no, flow_file);
        return NS_PARSE_SCRIPT_SUCCESS;
      } else{   // If boundary is not found, create custom boundary in format <pagename>_<TRno>_<index>--------- 
        multipart_bdry_index++;
        ADD_CUSTOM_BOUNDARY(RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name), multipart_bdry_index-1);
      }
    }
  }

  /* this is a param=value that appears in the main header or the in the multipart message
   * eg., Content-type: multipart/form-data, boundary=AaB03x
   */
  //these headers come after MULTIPART_BOUNDARY. copy into the body
  NSDL2_PARSING(NULL, NULL, "copying header %s to multipart post body",http_hdr);
  //copy_to_multipart_body_post_buf("\xd\xa",2, multipart_body_parts_count-1, 0);     //start on a new line
  copy_to_multipart_body_post_buf(http_hdr, strlen(http_hdr), multipart_body_parts_count-1, 0);
  copy_to_multipart_body_post_buf("\xd\xa",2, multipart_body_parts_count-1, 0);     //header inside the body also needs CRLF at the end

  return NS_PARSE_SCRIPT_SUCCESS;
}
/*******************Mutltipart Body End***************************/

void save_and_segment_body(int *rnum, int req_index, 
                                  char *body_buf, int noparam_flag, int *script_ln_no,
                                  int sess_idx, char *file_name) 
{
  NSDL2_PARSING(NULL, NULL, "Method called.");

  int version, ret;
  char err_msg[NS_PB_MAX_ERROR_MSG_LEN];

  create_post_table_entry(rnum);

  requests[req_index].proto.http.post_idx = *rnum;

  if ((requests[req_index].proto.http.body_encoding_flag == BODY_ENCODING_AMF) && (!global_settings->amf_seg_mode)) { //AMF
    NSDL2_PARSING(NULL, NULL, "Body is AMF");
    version = amf_segment_line(&postTable[*rnum], body_buf, noparam_flag, *script_ln_no, sess_idx, file_name);
    if(version == 0) {
      requests[req_index].proto.http.header_flags |= NS_URL_BODY_AMF0;
    } else if(version == 3) {
      requests[req_index].proto.http.header_flags |= NS_URL_BODY_AMF3;
    } else {
      //fprintf(stderr, "%s(): Request BODY AMF is not correct or version (%d) is not supported. Treating as non AMF request\n", (char*)__FUNCTION__, version);
      segment_line_noparam(&postTable[*rnum], body_buf, sess_idx);
    }
  } else if(requests[req_index].proto.http.body_encoding_flag == BODY_ENCODING_PROTOBUF || requests[req_index].proto.http.body_encoding_flag ==  BODY_ENCODING_GRPC_PROTO) {
    NSDL2_PARSING(NULL, NULL, "Body is Protobuf/GRPC");
    ret = ns_protobuf_segment_line(req_index, &postTable[*rnum], body_buf, noparam_flag,
                                   *script_ln_no, sess_idx, file_name, err_msg, NS_PB_MAX_ERROR_MSG_LEN);

    if(ret == -1)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012063_ID, "%s", err_msg);
    }
    else if(ret == -2)
      NSTL1(NULL, NULL, "%s", err_msg); 
  } else {
    if (noparam_flag) {
      segment_line_noparam(&postTable[*rnum], body_buf, sess_idx);
    } else {
      segment_line(&postTable[*rnum], body_buf, 0, *script_ln_no, sess_idx, file_name);
    }
  }
}

void save_and_segment_ws_body(int send_tbl_idx,
                                  char *body_buf, int noparam_flag, int *script_ln_no,
                                  int sess_idx, char *file_name)
{
  NSDL2_PARSING(NULL, NULL, "Method called, send_tbl_idx = %d, noparam_flag = %d", send_tbl_idx, noparam_flag);
 
  if (noparam_flag) {
    segment_line_noparam(&g_ws_send[send_tbl_idx].send_buf, body_buf, sess_idx);
  } else {
    segment_line(&g_ws_send[send_tbl_idx].send_buf, body_buf, 0, *script_ln_no, sess_idx, file_name);
  }
}

void save_and_segment_jrmi_body(int *rnum, int req_index, 
                                  char *body_buf, int noparam_flag, int *script_ln_no,
                                  int sess_idx, char *file_name) 
{
  NSDL2_PARSING(NULL, NULL, "Method called.");

  //int version;

  create_post_table_entry(rnum);

  requests[req_index].proto.jrmi.post_idx = *rnum;

  if (noparam_flag) {
      requests[req_index].proto.jrmi.no_param = 1;
      segment_line_noparam(&postTable[*rnum], body_buf, sess_idx);
  } else {
      segment_line(&postTable[*rnum], body_buf, 0, *script_ln_no, sess_idx, file_name);
  }
}

// This method is used in case of multipart only, and it can be called multiple times for one request
void save_and_segment_body_multipart(int rnum, int req_index, 
                                  char *body_buf, int noparam_flag, int *script_ln_no,
                                  int sess_idx, char *file_name) 
{
  int version, ret;
  char err_msg[NS_PB_MAX_ERROR_MSG_LEN];

  NSDL2_PARSING(NULL, NULL, "Method called.");

  if (requests[req_index].proto.http.body_encoding_flag == BODY_ENCODING_AMF) { //AMF
    NSDL2_PARSING(NULL, NULL, "Body is AMF");
    version = amf_segment_line(&postTable[rnum], body_buf, noparam_flag, *script_ln_no, sess_idx, file_name);
    if(version == 0) {
      requests[req_index].proto.http.header_flags |= NS_URL_BODY_AMF0;
    } else if(version == 3) {
      requests[req_index].proto.http.header_flags |= NS_URL_BODY_AMF3;
    } else {
      segment_line_noparam_multipart(&postTable[rnum], body_buf, sess_idx);
    }
  } else if(requests[req_index].proto.http.body_encoding_flag == BODY_ENCODING_PROTOBUF) {
    NSDL2_PARSING(NULL, NULL, "Body is Protobuf");
    ret = ns_protobuf_segment_line(req_index, &postTable[rnum], body_buf, noparam_flag,
                 *script_ln_no, sess_idx, file_name, err_msg, NS_PB_MAX_ERROR_MSG_LEN);
    if(ret == -1)
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012063_ID, "%s", err_msg);

  }else {
    if (noparam_flag) {
      segment_line_noparam_multipart(&postTable[rnum], body_buf, sess_idx);
    } else {
      segment_line_int(&postTable[rnum], body_buf, 0, *script_ln_no, sess_idx, file_name, 0);
    }
  }
}

//Called every time a URL from capture file is read
void init_post_buf ()
{
    NSDL2_PARSING(NULL, NULL, "Method called");
    if (max_post_buf_len == 0) {
	//Alloc g_post_buf
        MY_MALLOC(g_post_buf, DELTA_POST_BUF_LEN, "g_post_buf", -1);
	max_post_buf_len = DELTA_POST_BUF_LEN;
    }
    g_post_buf[0] = '\0';
    cur_post_buf_len = 0;
}

//Called every time, data is added to post buf
int copy_to_post_buf(char *buf, int blen)
{
    NSDL4_PARSING(NULL, NULL, "Method called, buf = %s, blen = %d", buf, blen);
    /*Complete body is passed to parse all ns_decrypt API */
    ns_parse_decrypt_api(buf);
    NSDL4_PARSING(NULL, NULL, "buf = %s", buf);
    while (blen + cur_post_buf_len + 1 > max_post_buf_len) {
	//Realloc g_post_buf
        MY_REALLOC_EX(g_post_buf, max_post_buf_len + DELTA_POST_BUF_LEN, max_post_buf_len, "g_post_buf", -1);
	max_post_buf_len += DELTA_POST_BUF_LEN;
    }
    memcpy(g_post_buf+cur_post_buf_len, buf, blen);
    cur_post_buf_len += blen;
    g_post_buf[cur_post_buf_len] = 0;
    //printf("post_buf_len is %d\n", cur_post_buf_len);
    return 0;
}

//called after all capture and detailed are processed
void
free_post_buf()
{
    NSDL2_PARSING(NULL, NULL, "Method called");
    if (max_post_buf_len) {
        FREE_AND_MAKE_NOT_NULL_EX(g_post_buf, max_post_buf_len, "g_post_buf", -1);
        max_post_buf_len = cur_post_buf_len = 0;
    }
}

void get_post_content_dyn_vars(FILE* fp, char* script_line, int req_idx, int line_num) {
  int content_length;
  char buf[MAX_LINE_LENGTH];
  //char temp_line[MAX_LINE_LENGTH];
  char temp_line[MAX_REQUEST_BUF_LENGTH];
  char* dyn_start;
  char* dyn_end;
  int dyn_length;
  int dyn_idx;
  int first_dyn_var;
  int rnum;

  script_line[0] = '\0';

  while (strncmp(nslb_fgets(buf, MAX_LINE_LENGTH, fp, 1), "----\n", strlen("----\n"))) {

    strcpy(temp_line, script_line);

    //if (snprintf(script_line, MAX_LINE_LENGTH, "%s%s", temp_line, buf) > MAX_LINE_LENGTH)
    if (snprintf(script_line, MAX_REQUEST_BUF_LENGTH, "%s%s", temp_line, buf) > MAX_REQUEST_BUF_LENGTH)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012065_ID, CAV_ERR_1012065_MSG, MAX_REQUEST_BUF_LENGTH);

    line_num++;
  }

  content_length = strlen(script_line);
  if (content_length) {
    if (script_line[content_length - 1] == '\n')   /* NEWLINE SHOULD BE THERE, WE PUT IT THERE IN THE CAPTURE PROGRAM */
      script_line[content_length - 1] = '\0';
  }

  dyn_start = buf + strlen("----");
  first_dyn_var = 1;
  while(*dyn_start != '\n') {
    while (*dyn_start == ' ')
      dyn_start++;

    if (strncasecmp(dyn_start, "CAVD", strlen("CAVD")) == 0) {
      dyn_start += strlen("CAVD");
      if (*dyn_start != '=') 
	SCRIPT_PARSE_ERROR_EXIT(script_line, "cavd variable format incorrect (CAVV=<variable_name>)");
      dyn_start++;
      while (*dyn_start == ' ')
	dyn_start++;
      if (!(dyn_end = strchr(dyn_start, ' '))) {
	dyn_end = strchr(dyn_start, '\n');
      }

      if ((dyn_length = dyn_end - dyn_start)) {
	bcopy(dyn_start, temp_line, dyn_length);
	temp_line[dyn_length] = '\0';
	if ((dyn_idx = find_dynvar_idx(temp_line)) == -1) 
        {
	  create_dynvar_table_entry(&dyn_idx);

	  if ((dynVarTable[dyn_idx].name = copy_into_big_buf(dyn_start, dyn_length)) == -1) 
            SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, dyn_start);
	}

	create_reqdynvar_table_entry(&rnum); 

	reqDynVarTable[rnum].name = dynVarTable[dyn_idx].name;
	reqDynVarTable[rnum].length = dyn_length;
      } else 
           SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012066_ID, CAV_ERR_1012066_MSG);

      if (first_dyn_var) {
 	requests[req_idx].proto.http.dynvars.dynvar_start = rnum;
	first_dyn_var = 0;
      }

      requests[req_idx].proto.http.dynvars.num_dynvars++;

      dyn_start = dyn_end;
    }
  }
}

int init_pre_post_url_callback(int index, void *handle)
{
  int url_idx;
  char func_name[MAX_LINE_LENGTH];
  char* error;

  NSDL3_PARSING(NULL, NULL, "Method called. Page index = %d", index);

  for( url_idx = gPageTable[index].first_eurl; url_idx < gPageTable[index].first_eurl + gPageTable[index].num_eurls; url_idx++)
  {
    requests[url_idx].pre_url_func_ptr = NULL;
    if(requests[url_idx].pre_url_fname[0] != 0)
    {
      strcpy(func_name, requests[url_idx].pre_url_fname);
      // printf("Converting URL PRE_CB function to function pointer. Page Name = %s, Func Name = %s, URL IDX = %d\n", RETRIEVE_BUFFER_DATA(gPageTable[i].page_name), func_name, url_idx);
      requests[url_idx].pre_url_func_ptr = dlsym(handle, func_name);

      if ((error = dlerror()))
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012017_ID, CAV_ERR_1012017_MSG, func_name);
    }
    requests[url_idx].post_url_func_ptr = NULL;
    if(requests[url_idx].post_url_fname[0] != 0)
    {
      strcpy(func_name,requests[url_idx].post_url_fname);
      // printf("Converting URL post_CB function to function pointer. Page Name = %s, Func Name = %s, URL IDX = %d\n", RETRIEVE_BUFFER_DATA(gPageTable[i].page_name), func_name, url_idx);
      requests[url_idx].post_url_func_ptr = dlsym(handle, func_name);

      if ((error = dlerror()))
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012017_ID, CAV_ERR_1012017_MSG, func_name);
    }
  }//end of for loop
  return 0;
}//end of function

// For C Type script
// Issue - How do we know comma is coming or not
// Resolution - We handle both with and without comma in this method

/* This function validate HTTP/HTTPS BODY. Body msut be terminated by ,\n
 * Here we are trying to skip spaces after last ',' found & also chking if after the last occurance 
 * of ',' if we found any character execpt TABS or SPACE we give error.
 */
int validate_body_and_ignore_last_spaces_c_type(int sess_idx)
{
  char *last_quotes_index, *ptr;
  char comma_found = '0';
  //int orig_post_buf_len = cur_post_buf_len;

  NSDL2_PARSING(NULL, NULL, "Method called, cur_post_buf_len = %d, sess_idx=%d", cur_post_buf_len, sess_idx);

  // Body must have at least two bytes as even empty body must be like
  //  "Body=",  (Quotes, comma and newline)
  //  "Body=" (Quotes and newline)
  if ((cur_post_buf_len < 2) || ((g_post_buf[cur_post_buf_len - 1]) != '\n'))
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);

  // Point to the last quote in the post buf
  last_quotes_index = rindex(g_post_buf, '"');

  if(last_quotes_index == NULL)
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);

  // Post buffer length from start to last character before quotes
  cur_post_buf_len = last_quotes_index - g_post_buf;
  g_post_buf[cur_post_buf_len] = 0; // Null terminate

  ptr = last_quotes_index + 1;  //Will start with next character after "
  // Check that after last quote, there is only max one comma and white space
  while(*ptr != '\n')
  {
    if(*ptr == ',')
    {
      if(comma_found == '1')
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012058_ID, CAV_ERR_1012058_MSG);

      comma_found = '1';
    }
    else if(!isspace(*ptr))
    {
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012057_ID, CAV_ERR_1012057_MSG);
    }
    ptr++;
  }

  NSDL4_PARSING(NULL, NULL, "Exiting Method, cur_post_buf_len = %d,\nBody=[%s]", cur_post_buf_len, g_post_buf);
  return NS_PARSE_SCRIPT_SUCCESS;
}

/* This function validate HTTP/HTTPS BODY. Body msut be terminated by ,\n
 * Here we are trying to skip spaces after last ',' found & also chking if after the last occurance
 * of ',' if we found any character execpt TABS or SPACE we give error.
 */
void validate_body_and_ignore_last_spaces() 
{
  char *last_comma_index;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, cur_post_buf_len = %d", cur_post_buf_len);

  if ((cur_post_buf_len < 2) || ((g_post_buf[cur_post_buf_len - 1]) != '\n') ) {
    // No ',' found
    NS_EXIT(-1, "Error: BODY must be terminated by ,\\n");
  }
	
  cur_post_buf_len--;
  g_post_buf[cur_post_buf_len] = 0;

  last_comma_index = rindex(g_post_buf, ',');

  if(last_comma_index == NULL) {
    NS_EXIT(-1, "Error: BODY must be terminated by ,\\n");
  }

  last_comma_index++;

  for(;last_comma_index != NULL && *last_comma_index != '\0'; last_comma_index++) {
    if(*last_comma_index == ' ' || *last_comma_index == '\t') {
      cur_post_buf_len--;
    }
     else {
       NS_EXIT(-1, "Error: BODY must be terminated by ,\\n");
    }
  }

  g_post_buf[cur_post_buf_len - 1] = 0;
}


int
post_process_post_buf(int req_index, int sess_idx, int *script_ln_no, char *cap_fname)
{
    char *fname, fbuf[8192];
    int ffd, rlen, rnum, noparam_flag = 0;

    NSDL2_PARSING(NULL, NULL, "Method called, sess_idx = %d, req_index = %d", sess_idx,req_index);

    if (cur_post_buf_len <= 0) return NS_PARSE_SCRIPT_SUCCESS; //No BODY, exit

    //Removing traing ,\n from post buf.
    
    if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_LEGACY)
    {
      validate_body_and_ignore_last_spaces();
    }
    else
    {
      validate_body_and_ignore_last_spaces_c_type(sess_idx);
    }

    //Check if BODY is provided using $CAVINCLUDE$= directive
    if((strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0) || (strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)) {

     if(strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
     {
        fname = g_post_buf + 21;
        noparam_flag = 1;
     }
     else
       fname = g_post_buf + 13;
        /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
        if (fname[0] != '/') {
            sprintf (fbuf, "%s/%s/%s", GET_NS_TA_DIR(),
                     get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
                     //Previously taking with only script name
                     //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), fname);
            fname = fbuf;
        }

        NSDL2_PARSING(NULL, NULL, "fbuf = %s", fbuf);
        ffd = open (fname, O_RDONLY|O_CLOEXEC);
        if (!ffd) {
            printf("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, fname);
            return NS_PARSE_SCRIPT_ERROR;
        }
        cur_post_buf_len = 0;
        while (1) {
            rlen = read (ffd, fbuf, 8192);
            if (rlen > 0) {
              if (copy_to_post_buf(fbuf, rlen)) {
                printf("%s(): Request BODY could not alloccate mem for %s\n", (char*)__FUNCTION__, fname);
                return NS_PARSE_SCRIPT_ERROR;
              }
              continue;
            } else if (rlen == 0) {
                break;
            } else {
                perror("reading CAVINCLUDE BODY");
                printf("%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, fname);
                return NS_PARSE_SCRIPT_ERROR;
            }
        }
        close (ffd);
    }

    if (cur_post_buf_len) {
      //TODO Error here set request type 23 in request table
      NSDL2_PARSING(NULL, NULL, " request type = %d, req_index = %d", requests[req_index].request_type, req_index);
      if((requests[req_index].request_type != JRMI_REQUEST))
      {
        save_and_segment_body(&rnum, req_index,
                            g_post_buf, noparam_flag, script_ln_no,
                            sess_idx, cap_fname);
      }
      else 
        save_and_segment_jrmi_body(&rnum, req_index,
                            g_post_buf, noparam_flag, script_ln_no,
                            sess_idx, cap_fname);

    }
    return NS_PARSE_SCRIPT_SUCCESS;
}

/*// return 1 if skip & continue else 0
int get_encoding_type_from_content_type_hdr (char *buf, int *encoding) {

  NSDL2_PARSING(NULL, NULL, "Method called. Hdr = %s", buf);

  if (strncasecmp(buf, "Content-Type:", 13) == 0) {
    if (strstr(&buf[14], "x-www-form-urlencoded"))
    {
      NSDL2_PARSING(NULL, NULL, "Body encoding is url encoded");
      *encoding = BODY_ENCODING_URL_ENCODED;
    }
    else if (strstr(&buf[14], "application/x-amf"))
    {
      NSDL2_PARSING(NULL, NULL, "Body encoding is AMF");
      *encoding = BODY_ENCODING_AMF;
    }
  }
  return 0;
}
*/
// Parse HTTP header 
// For legacy script, this is used for Main URL headers only
// For C script, this is used for both Main and inline URL headers
// This is called for each http header
// Arguments:
//  http_hdr: Pointer to header with value without \r\n 
//  all_http_hdrs: Buffer for concatenating the current header
//  fname: File name from where header is extracted (e.g. script.capture, flow1.c)
//  header_bits: Point to header bits for setting flags
//  url_idx: Index of the URL in http request table

// Return:
//  0 - OK, -1 error
//  It also concats passed header in the all_http_hdrs

int parse_http_hdr(char *http_hdr, char *all_http_hdrs, int line_num, char *fname,  int url_idx, int sess_idx)
{
char temp_buf[MAX_REQUEST_BUF_LENGTH + 1];

  NSDL2_PARSING(NULL, NULL, "Method called. http_hdr = %s, all_http_hdrs = %s, file= %s, line_num = %d,  url_idx = %d", http_hdr, all_http_hdrs, fname, line_num, url_idx);
  int j, disable_headers;
  for (j = 0; j < total_runprof_entries; j++){
    if(runProfTable[j].sessprof_idx == sess_idx){
      disable_headers = runProfTable[j].gset.disable_headers;
      break;
    } 
  }
  // User Agent is filled by netsorm based on the profile used. So ignore it 
  //TODO: GRPC for User-Agent Header
  if (strncasecmp(http_hdr, "User-Agent:", strlen("User-Agent:")) == 0)
  {
    if(!(disable_headers & NS_UA_HEADER) && !(GRPC))
      return 0;
    else
      SET_HDR_FLAG(NS_REQ_HDR_FLAGS_USER_AGENT);
  }

  if ((strncasecmp(http_hdr, "TE:", strlen("TE:")) == 0))
  {
    if(!GRPC)
      return 0;
    else
      SET_HDR_FLAG(NS_REQ_HDR_FLAGS_TE);
  }

  if ((strncasecmp(http_hdr, "grpc-accept-encoding:", strlen("grpc-accept-encoding:")) == 0))
  {
    if(!GRPC)
      return 0;
    else
      SET_HDR_FLAG(NS_REQ_HDR_FLAGS_GRPC_ACCEPT_ENCODING);
  }

  if ((strncasecmp(http_hdr, "grpc-encoding:", strlen("grpc-encoding:")) == 0) && (!GRPC)) 
    return 0;


  // Conetent-Length header, if allowed can only be put by NetStorm
  if (strncasecmp(http_hdr, "Content-Length:", strlen("Content-Length:")) == 0)
    return 0;

  
  // Connection header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Connection:", strlen("Connection:")) == 0) && (!(disable_headers & NS_CONNECTION_HEADER)))
    return 0;

  // Keep-Alive header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Keep-Alive:", strlen("Keep-Alive:")) == 0) && (!(disable_headers & NS_KA_HEADER)))
    return 0;

  // Accept header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Accept:", strlen("Accept:")) == 0) && (!(disable_headers & NS_ACCEPT_HEADER)))
    return 0;

  // Accept-Encoding header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Accept-Encoding:", strlen("Accept-Encoding:")) == 0) && (!(disable_headers & NS_ACCEPT_ENC_HEADER)))
    return 0;

  // If-Modified-Since header, if allowed can only be put by NetStorm (For caching)
  if ((strncasecmp(http_hdr, "If-Modified-Since:", strlen("If-Modified-Since:")) == 0))
  {
    NSDL2_PARSING(NULL, NULL, "Got If-Modified-Since at script_line %d in file %s \n",
                                             script_ln_no, flow_filename);
    return 0;
  }

  // If-None-Match header, if allowed can only be put by NetStorm (For caching)
  if ((strncasecmp(http_hdr, "If-None-Match:", strlen("If-None-Match:")) == 0)) 
  {
    NSDL2_PARSING(NULL, NULL, "Got If-Modified-Since at script_line %d in file %s \n",
                                             script_ln_no, flow_filename);
    return 0;
  }

  //Host header, if allowed can only be put by NetStorm
  if ((strncasecmp(http_hdr, "Host:", 5) == 0) && (!(disable_headers & NS_HOST_HEADER))) {
    return 0;
  }

  if (strstr(http_hdr, "multipart")) {
    NSDL2_PARSING(NULL, NULL, "Main header contains boundary multipart. this is a multipart body message. multipart_bdry_index %d", multipart_bdry_index);
    parse_main_multipart_hdr(http_hdr);
  }
  
  /* Any Headers after this will not be filtered and will be saved 'as is' */
  // Parse cache control header and set cacheRequestHeader flags
  cache_parse_req_cache_control(http_hdr, &(requests[url_idx].proto.http.cache_req_hdr), sess_idx, line_num, fname);    
  // We need to send this header in the HTTP request

  strcpy(temp_buf, all_http_hdrs);
  if((global_settings->whitelist_hdr) && (global_settings->whitelist_hdr->mode) &&
     (!strncasecmp(http_hdr, global_settings->whitelist_hdr->name, strlen(global_settings->whitelist_hdr->name))))
  {
    if(snprintf(all_http_hdrs, MAX_REQUEST_BUF_LENGTH, "%s%s; %s\r\n", temp_buf, http_hdr, global_settings->whitelist_hdr->value) > MAX_REQUEST_BUF_LENGTH)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012067_ID, CAV_ERR_1012067_MSG, MAX_REQUEST_BUF_LENGTH);
     requests[url_idx].flags |= NS_REQ_FLAGS_WHITELIST;
  }
  else
  {
    // Concat http header with \r\n
    if(snprintf(all_http_hdrs, MAX_REQUEST_BUF_LENGTH, "%s%s\r\n", temp_buf, http_hdr) > MAX_REQUEST_BUF_LENGTH)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012067_ID, CAV_ERR_1012067_MSG, MAX_REQUEST_BUF_LENGTH);
  }
  NSDL2_PARSING(NULL, NULL, "Exiting Method. http_hdr = %s, all_http_hdrs = %s, file= %s, line_num = %d, url_idx = %d", http_hdr, all_http_hdrs, fname, line_num, url_idx);
  return 1;
}

void init_web_url_page_id()
{
  web_url_page_id = 0;
}

int check_duplicate_pagenames(char *pagename, int sess_idx) 
{
  int num_pages, first_page, page_index;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called sess_idx=%d",sess_idx);
  num_pages = gSessionTable[sess_idx].num_pages;
  first_page = gSessionTable[sess_idx].first_page;
  NSDL2_PARSING(NULL, NULL, "Session # %d Name = %s num_pages = %d first_page = %d",
               sess_idx, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name),
               gSessionTable[sess_idx].num_pages, first_page);
  for (page_index = 0; page_index < num_pages; page_index++)
  {
    NSDL2_PARSING(NULL, NULL, "\tPage # %d Name = %s",
             page_index + first_page, RETRIEVE_BUFFER_DATA(gPageTable[page_index + first_page].page_name),
             gPageTable[page_index + first_page].num_eurls);
    if(strcmp(RETRIEVE_BUFFER_DATA(gPageTable[page_index + first_page].page_name), pagename) == 0)
      return NS_PARSE_SCRIPT_ERROR;
  }

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}


int parse_and_set_pagename(char *api_name, char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                  FILE *outfp,  char *flow_outfile, int sess_idx, char **page_end_ptr, char *pagename)
{
  int len;
  char str[MAX_LINE_LENGTH + 1];
  char *start_idx, *end_idx;
  char *flow_name = NULL;
  char flow_file_lol[4096];
  int page_norm_id;
  char pagename_local[MAX_LINE_LENGTH + 1];
  char *ptr;

  NSDL2_PARSING(NULL, NULL, "Method Called");

  sprintf(flow_file_lol, "%s", flow_file);
  strcpy(pagename_local, pagename);
  
  ptr = strchr(pagename, ':');
  if(ptr != NULL)
    *ptr = '\0';

  if(check_duplicate_pagenames(pagename, sess_idx) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR_EXIT_EX(str, CAV_ERR_1012068_ID, CAV_ERR_1012068_MSG, pagename, RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name));
     
  //Increment when if URL parse
  page_num_relative_to_flow++;

  //Allocating record for new page
  create_page_table_entry(&g_cur_page);

  // For fetching any characters before ns_web_url. For example:
  //         int ret = ns_web_url("Page1", ...
  start_idx = line_ptr;
  NSDL2_PARSING(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_name);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012070_ID, CAV_ERR_1012070_MSG);

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage
  NSDL3_PARSING(NULL, NULL, "line to write is str is (%s) , api to run is (%s) web_url_id (%d) pagename is  %s", 
                             str, api_to_run, web_url_page_id, pagename);

  sprintf(str, "%s%s(%d); // %s", str, api_to_run, web_url_page_id, pagename);
  web_url_page_id++;

  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR_EXIT_EX(str, CAV_ERR_1012071_ID, CAV_ERR_1012071_MSG);

  page_norm_id = get_norm_id_for_page(pagename_local, 
                    get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), 
                    gSessionTable[sess_idx].sess_norm_id);
  
  //Copy page name into big buffer
  if ((gPageTable[g_cur_page].page_name = copy_into_big_buf(pagename, 0)) == -1)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, pagename);

  //Extract flow name from flow_file which includes path n name of the flow file
  flow_name = basename(flow_file_lol);
  NSDL3_PARSING(NULL, NULL, "flow_name = %s", flow_name);

  //Copy flow file into big buffer
  if ((gPageTable[g_cur_page].flow_name = copy_into_big_buf(flow_name, 0)) == -1)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, flow_name);

  //Setting values for new page in page structure
  if (gSessionTable[sess_idx].num_pages == 0)
  {
    gSessionTable[sess_idx].first_page = g_cur_page;
    NSDL2_PARSING(NULL, NULL, "Current Page Number = %d", g_cur_page);
  }
 
  gSessionTable[sess_idx].num_pages++;
  // Note - num_eurls is total URLs including Main url
  gPageTable[g_cur_page].num_eurls = 0; 
  gPageTable[g_cur_page].head_hlist = -1;
  gPageTable[g_cur_page].tail_hlist = -1;
  gPageTable[g_cur_page].page_norm_id = page_norm_id;
  gPageTable[g_cur_page].page_num_relative_to_flow = page_num_relative_to_flow;
  
  NSDL2_PARSING(NULL, NULL, "Number of Pages = %d, page_num_relative_to_flow = %d, "
                            "gPageTable[g_cur_page].page_num_relative_to_flow = %d", 
                             gSessionTable[sess_idx].num_pages, page_num_relative_to_flow, 
                             gPageTable[g_cur_page].page_num_relative_to_flow);

  return NS_PARSE_SCRIPT_SUCCESS;
}


int set_url(char *url, char *flow_file, int sess_idx, int url_idx, char embedded_url, char inline_enabled, char *apiname)
{
  NSDL2_PARSING(NULL, NULL, "Method Called, Url=%s, sess_idx = %d, url_idx = %d, apiname = %s", url, sess_idx, url_idx, apiname);
  return (set_url_internal(url, flow_file, sess_idx, url_idx, embedded_url, inline_enabled, apiname)); 
}

int set_url_internal(char *url, char *flow_file, int sess_idx, int url_idx, char embedded_url, char inline_enabled, char *apiname)
{
  char hostname[MAX_LINE_LENGTH + 1];
  int  request_type;
  char request_line[MAX_LINE_LENGTH + 1];
  int get_no_inlined_obj_set_for_all = 1;

  NSDL2_PARSING(NULL, NULL, "Method Called Url=%s, sess_idx = %d, url_idx = %d", url, sess_idx, url_idx);
  //Parses Absolute/Relative URLs
  if(parse_url_param(url, "{/?#", &request_type, hostname, request_line) != RET_PARSE_OK)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012069_ID, CAV_ERR_1012069_MSG, url);

  //Request type should be from http, https, xhttp
  if(request_type == REQUEST_TYPE_NOT_FOUND)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012072_ID, CAV_ERR_1012072_MSG);

  // This is for netstorm specific case to get exact number of bytes 
  // It is configured using xhttp://host/url
  // See ns_http_process_resp.c for handling of response
  int exact = 0;
  int bytes_to_recv = 0;
  if(request_type ==  XHTTP_REQUEST) // XHTTP_REQUEST is used for parsing only
  {
    request_type = HTTP_REQUEST;
    exact = 1;
    bytes_to_recv = 40;
  }

  NSDL3_PARSING(NULL, NULL, "request_type = %d", request_type); 
  if(request_type == PARAMETERIZED_URL)
  {
    requests[url_idx].is_url_parameterized = NS_URL_PARAM_VAR;
    request_type = HTTP_REQUEST;
  } 
  NSDL3_PARSING(NULL, NULL, "requests[%d].is_url_parameterized = %d, request_type = %d", 
                             url_idx, requests[url_idx].is_url_parameterized, request_type); 
  proto_based_init(url_idx, request_type);

  requests[url_idx].proto.http.exact = exact;
  requests[url_idx].proto.http.bytes_to_recv = bytes_to_recv; 

  //Setting url type to Main/Embedded
  if(!embedded_url)
    requests[url_idx].proto.http.type = MAIN_URL;
  else
    requests[url_idx].proto.http.type = EMBEDDED_URL;

  NSDL3_PARSING(NULL, NULL, "type = %d", requests[url_idx].proto.http.type); 
  //TODO: #Deepika - need to handle RBU cases also ..........
  // In RBU there is no meaning of INLINE url so make csv file only for main url
  if((global_settings->protocol_enabled & RBU_API_USED) && (requests[url_idx].proto.http.type == MAIN_URL))
  {
    if(ns_rbu_set_csv_file_name(url_idx, hostname) == -1)
      NSDL3_PARSING(NULL, NULL, "Error: Null String Found in function ns_rbu_set_csv_file_name");
  }

  gPageTable[g_cur_page].num_eurls++; // Increment urls

  if(g_max_num_embed < gPageTable[g_cur_page].num_eurls) 
    g_max_num_embed = gPageTable[g_cur_page].num_eurls; //Get high water mark

  NSDL3_PARSING(NULL, NULL, "inline_enabled = %d, embedded_url = %d, num_eurls = %d", 
                             inline_enabled, embedded_url, gPageTable[g_cur_page].num_eurls);

  // Host will be resolved in following cases
  // 1. host is main url host
  // 2. host is inline url host and inline is enabled for the script at lest in one group
  if(embedded_url == 0 || inline_enabled == 1)
  {
    NSDL3_PARSING(NULL, NULL, "Main url host or inline is enabled. Going to add this host into gServerTable. hostname = %s," 
										"url = %s, script_name = %s", hostname, url, script_name);
    // TODO: How to handle "$CAVS{%s}" (see url.c)
    // check if the hostname exists in the server table, if not add it
    /* Shibani: Resolving Bug 17130:- In case of RBU, for click and script URL should be its predecessor ns_web_url() API.
         Ex: suppose one make script like -
             ns_web_url("Home",
                        "URL = www.XYZ.com")
 
             ns_link("Login",
                      "URL = www.PRQ.com")
            
             ns_submit("Submit",
                       "URL = www.123.com")
            
             In this cases, in DB entries goes like -
               PageName | HostName
               Home 	| XYZ
               Login	| PQR
               Submit   | 123
             
             Due to this reason in GUI (Average Page Report) we get wrong information - All the above three pages are of different Host.
             But logically/practicaly all the above three pages are part of same Web Page (here of XYZ).
             So NS should hanlde this type of situation automatically. 
             */

   /* This above design is changed, due to bug 27217. Now Host Name of click & script API may not same with host of its predecessor ns_web_url or ns_browser API*/

    if(requests[url_idx].is_url_parameterized)
    {
      requests[url_idx].index.svr_idx = get_parameterized_server_idx(hostname, requests[url_idx].request_type, script_ln_no);
    }
    else
    {
      //If test is running in RBU and CA API
       requests[url_idx].index.svr_idx = get_server_idx(hostname, requests[url_idx].request_type, script_ln_no);
    
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from set_url_internal");
    
      //Checking duplicate entries for host_idx in script
      if(apiname && (!strcmp(apiname, "ns_web_url") || !strcmp(apiname, "ns_browser")))
        rbu_web_url_host_id = requests[url_idx].index.svr_idx;
    
      NSDL3_PARSING(NULL, NULL, "rbu_web_url_host_id = %d, apiname = %s, hostname = %s", rbu_web_url_host_id, apiname, 
                                  RETRIEVE_BUFFER_DATA(gServerTable[rbu_web_url_host_id].server_hostname));
    
      if(apiname && (global_settings->protocol_enabled & RBU_API_USED) && (strcmp(apiname, "ns_web_url") && strcmp(apiname, "ns_browser")))
      {
        // Commenting the below code to resolve bug : 19863 - Getting error when we are using two flow in one script. 
        //If first API for any flow file is CA API then test should be exit after throughing below error 
        /*  if(first_flow_page == 1)
            SCRIPT_PARSE_ERROR_EXIT(NULL, "Error: First API in script must be ns_web_url or ns_browser, but in flow file %s first API is %s",
                                           flow_file, apiname);*/
    
        //Commenting below code to resolve bug 27217.
        /* if((url_idx >= 1) && strcmp(hostname, RETRIEVE_BUFFER_DATA(gServerTable[requests[url_idx - 1].index.svr_idx].server_hostname)))
           {
              fprintf(stdout, "Warring: Host name for click & script API %s is must be same as its predecessor ns_web_url or ns_browser API, "
                              "so changing host name of %s API from %s to %s\n", 
                     apiname, apiname, hostname, RETRIEVE_BUFFER_DATA(gServerTable[requests[url_idx - 1].index.svr_idx].server_hostname));
    
          requests[url_idx].index.svr_idx = requests[url_idx - 1].index.svr_idx;
          }*/
    
        //Resolve bug 27217
        if(rbu_web_url_host_id >= 0)
        {
          if(strcmp(hostname, RETRIEVE_BUFFER_DATA(gServerTable[rbu_web_url_host_id].server_hostname)))
          {
            fprintf(stdout, "Warning: Host name '%s' of API '%s' is not same with host name '%s' of its predecessor 'ns_web_url/ns_browser'." 
                            "Both host name may be need to same, if this then please change in your script\n", 
                             hostname, apiname, RETRIEVE_BUFFER_DATA(gServerTable[rbu_web_url_host_id].server_hostname));
            NS_DUMP_WARNING("Host name '%s' of API '%s' is not same with host name '%s' of its predecessor 'ns_web_url/ns_browser'." 
                            "Both host name should be same.", 
                             hostname, apiname, RETRIEVE_BUFFER_DATA(gServerTable[rbu_web_url_host_id].server_hostname));
          }
        } 
      }
     //reset flag first_flow_page
     //If first API for any flow file is ns_web_url or ns_browser then first_flow_page flag set to zero
     // first_flow_page=0;
    
    }
    NSDL3_PARSING(NULL, NULL, "Index of svr_idx = %d", requests[url_idx].index.svr_idx);
    if(requests[url_idx].index.svr_idx == -1) 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012073_ID, CAV_ERR_1012073_MSG);
    }
    else
    {
      if(gServerTable[requests[url_idx].index.svr_idx].main_url_host != 1)
      {
        if(embedded_url == 0) // Main url
          gServerTable[requests[url_idx].index.svr_idx].main_url_host = 1;
        else
          gServerTable[requests[url_idx].index.svr_idx].main_url_host = 0;
      }
    }
  }
  else 
  {
    NSDL3_PARSING(NULL, NULL, "Inline url host and inline is Disabled. "
                              "Going to skip this host. hostname = %s, url = %s, script_name = %s", 
                               hostname, url, script_name);
  } 

  NSDL2_PARSING(NULL, NULL, "url = %s", url);
  if(requests[url_idx].is_url_parameterized)
  {
    segment_line(&(requests[url_idx].proto.http.url_without_path), url, 0, script_ln_no, sess_idx, flow_file);
    NSDL2_PARSING(NULL, NULL, "http.url_without_path: url_idx = %d, seg_ptr = %lu, num_entries = %d", 
                               url_idx, requests[url_idx].proto.http.url_without_path.seg_start,
                               requests[url_idx].proto.http.url_without_path.num_entries);
  }

  //Do not add url in static url hash table in following cases
  //a. URL is parameterized if static_parm_url_as_dyn_url_mode is Set
  //b. Not to store embedded urls in hash table if Inlined urls not required for all groups

  StrEnt* segtable = &(requests[url_idx].proto.http.url);
  segment_line(segtable, request_line, 1, script_ln_no, sess_idx, flow_file);
  get_no_inlined_obj_set_for_all = get_no_inlined();
  
  NSDL3_PARSING(NULL, NULL, "segtable->num_entries=%d, static_parm_url_as_dyn_url_mode=%d"
             " requests[url_idx].proto.http.type=%s, get_no_inlined_obj_set_for_all=%d, url=%s",  
               segtable->num_entries, global_settings->static_parm_url_as_dyn_url_mode,
               ((requests[url_idx].proto.http.type==MAIN_URL)?"MAIN_URL": "EMBEDDED_URL"), 
                get_no_inlined_obj_set_for_all, request_line);  

  if (!(((segtable->num_entries > 1) && 
         (global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED))
       || ((requests[url_idx].proto.http.type == EMBEDDED_URL) && get_no_inlined_obj_set_for_all))) 
  {
    NSDL3_PARSING(NULL, NULL, "Storing the url in static url hash table");
    url_hash_add_url((u_ns_char_t *)request_line, url_idx, g_cur_page, RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

int set_http_method(char *method, char *flow_file, int url_idx, int line_number)
{
  int http_method;

  NSDL2_PARSING(NULL, NULL, "Method Called, Method=%s", method);
 
  // ISSUE: Both GET and Get are working. Need to check why 
  if(!strcmp(method, "GET"))  
    http_method = HTTP_METHOD_GET;
  else if (!strcmp(method, "POST"))
    http_method = HTTP_METHOD_POST;
  else if (!strcmp(method, "HEAD"))
    http_method = HTTP_METHOD_HEAD;
  else if (!strcmp(method, "PUT"))
    http_method = HTTP_METHOD_PUT;
  else if(!strcmp(method, "PATCH"))  
    http_method = HTTP_METHOD_PATCH;
  else if(!strcmp(method, "DELETE"))  
    http_method = HTTP_METHOD_POST; // treat like post
  else if(!strcmp(method, "OPTIONS"))  
    http_method = HTTP_METHOD_POST; // treat like post
  else if(!strcmp(method, "TRACE"))  
    http_method = HTTP_METHOD_POST; // treat like post
  else
  {
    // Dec 31, 2008 - We are allowing any method to go in request script_line for A10 customer to test negative case
    NSDL2_PARSING(NULL, NULL, "Warning: Unknown HTTP method %s at script_line %d in file %s."
                     " HTTP Post method will be used internally.",
                     method, line_number, flow_file);
    NS_DUMP_WARNING("Unknown HTTP method %s at script line %d in file %s. "
                     "Default HTTP post method will be used.",
                     method, line_number, flow_file);
    http_method = HTTP_METHOD_POST; // treat like post
  }

  requests[url_idx].proto.http.http_method = http_method; // Set HTTP Method
  requests[url_idx].proto.http.http_method_idx = find_http_method_idx(method);

  NSDL2_PARSING(NULL, NULL, "Method = %s, requests[%d].proto.http.http_method_idx = %d, http_method = %d", method, url_idx, requests[url_idx].proto.http.http_method_idx, requests[url_idx].proto.http.http_method);
  return NS_PARSE_SCRIPT_SUCCESS;
}

int set_fully_qualified_url(char *fully_qualified_url_value, char *flow_file, int url_idx, int script_ln_no)
{
  if(((strcasecmp(fully_qualified_url_value, "yes")) != 0) && ((strcasecmp(fully_qualified_url_value, "no") != 0)))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012074_ID, CAV_ERR_1012074_MSG, fully_qualified_url_value);
  }
  
  if(((strcasecmp(fully_qualified_url_value, "yes")) == 0))
  {
    requests[url_idx].proto.http.header_flags |= NS_FULLY_QUALIFIED_URL;
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_version(char *version, char *flow_file, int url_idx)
{
  NSDL2_PARSING(NULL, NULL, "Method Called, version=%s", version);
  //if (global_settings->use_http_10)
  if (!strcmp(version, "1.0"))
    requests[url_idx].proto.http.http_version = 0;  // HTTP/1.0
  else if (!strcmp(version, "1.1"))
    requests[url_idx].proto.http.http_version = 1;  // HTTP/1.1
  else
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012075_ID, CAV_ERR_1012075_MSG, version);

  return NS_PARSE_SCRIPT_SUCCESS;
}


int set_headers(FILE *flow_fp, char *flow_file, char *header_val, char *header_buf, int sess_idx, int url_idx)
{
  NSDL2_PARSING(NULL, NULL, "Method Called header_val=%s", header_val);

  if ((parse_http_hdr(header_val, header_buf, script_ln_no, flow_file, url_idx, sess_idx)) < 0)
    return NS_PARSE_SCRIPT_ERROR;

  if(set_header_flags(header_val, url_idx, sess_idx, flow_file) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_cookie(char *cookie_val, char *flow_file, int url_idx, int sess_idx)
{
  NSDL2_PARSING(NULL, NULL, "Method Called cookie_Value=%s", cookie_val);

  // We need to pass cookie values like name1;name2;
  add_main_url_cookie(cookie_val, script_ln_no, flow_file, url_idx, sess_idx);

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_compression(char *compression_val, char *flow_file, int url_idx)
{
  NSDL2_PARSING(NULL, NULL, "Method Called compression_val = %s", compression_val);
  int rx_ratio = 0, tx_ratio = 0;
  char *ptr, *slash_ptr = NULL;

  slash_ptr = strchr(compression_val, '/');

  //If slash not found in compression ratio
  if(!slash_ptr)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012142_ID, CAV_ERR_1012142_MSG, "Slash(/)");

  //Extracting rx_ratio
  ptr = compression_val;
  while(ptr != slash_ptr)
  {
    //If rx_ratio is not digit
    if(!isdigit(*ptr))
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012143_ID, CAV_ERR_1012143_MSG, "rx-ratio");

    ptr++;
  }

  CLEAR_WHITE_SPACE(compression_val);
  CLEAR_WHITE_SPACE_FROM_END(compression_val);

  //checking for length of rx-ratio
  if ((ptr - compression_val) <= 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012142_ID, CAV_ERR_1012142_MSG, "rx-ratio");

  rx_ratio = atoi(compression_val);
  NSDL2_PARSING(NULL, NULL, "rx_ratio in compression ratio is %d at %d in file %s", rx_ratio, script_ln_no, flow_file);

  //Extracting tx_ratio from start of / till end of string contained in compression_val
  ptr = slash_ptr + 1;
  while(ptr != '\0')
  {
    //If tx_ratio is not digit
    if(!isdigit(*ptr))
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012143_ID, CAV_ERR_1012143_MSG, "tx-ratio");

    ptr++;
  }

  if(ptr - (slash_ptr + 1) <= 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012142_ID, CAV_ERR_1012142_MSG, "tx-ratio");

  tx_ratio = atoi(slash_ptr + 1);

  NSDL2_PARSING(NULL, NULL, "tx_ratio in compression ratio is %d at %d in file %s", tx_ratio, script_ln_no, flow_file);

  requests[url_idx].proto.http.rx_ratio = rx_ratio;
  requests[url_idx].proto.http.tx_ratio = tx_ratio;
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_pre_url_callback(char *pre_url_callback, char *flow_file, int url_idx)
{
  NSDL2_PARSING(NULL, NULL, NULL, NULL, "Method Called, pre_url_callback = [%s], url_idx = [%d]", pre_url_callback, url_idx);

  if(val_fname(pre_url_callback, 31))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012144_ID, CAV_ERR_1012144_MSG, "PRE");

  strcpy(requests[url_idx].pre_url_fname, pre_url_callback);
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_post_url_callback(char *post_url_callback, char *flow_file, int url_idx)
{
  NSDL2_PARSING(NULL, NULL, NULL, NULL, "Method Called, post_url_callback = [%s], url_idx = [%d]", post_url_callback, url_idx);

  if(val_fname(post_url_callback, 31))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012144_ID, CAV_ERR_1012144_MSG, "POST");

  strcpy(requests[url_idx].post_url_fname, post_url_callback);
  return NS_PARSE_SCRIPT_SUCCESS;
}

/* This method is used to get a fully gualified url from a url.we pass parent_url, url and fully_qualified_url. parent_url is the parent 
of that url for we want to get fully qualified url.url is the url for which we want to get fully qualified url, it can be of three type:
	1.Fully qualified URL (e.g. http://www.abc.com/red.url)
	2.Absolute URL (e.g. /test/abc.html)
	3.Relative URL (e.g. test/abc.html)
In this method if url is not fully qualified then host and path is extracted from parent_url and added to url and in fully_qualified_url fully qualified url is written for url.  */  

static int make_fully_qualified_url(char *parent_url, char *url, char *fully_qualified_url)
{
int request_type;
char hostname[MAX_LINE_LENGTH];
char path[MAX_LINE_LENGTH];
//char init_url[MAX_LINE_LENGTH];
int ret;

  NSDL2_PARSING(NULL, NULL, "mthod called, parent_url = %s, url = %s", parent_url, url);  
 
  // Check if url is already fully qualified or not
  if((strncasecmp(url, "http://", 7) == 0) || (strncasecmp(url, "https://", 8) == 0))
  {
    NSDL2_PARSING(NULL, NULL, "url is already fully qualified,so returning the same url");
    if(url != fully_qualified_url)
    {
      strcpy(fully_qualified_url, url);
    }
    return 0;
  }

  // Save in temp buffer
  // strcpy(init_url, url);

  // Extract hostname and url path from the parent url
  ret = parse_url(parent_url, "{/?#", &request_type, hostname, path);
  if(ret == RET_PARSE_NOK)
  {
    return -1;
  }

  //TODO: #Deepika - need to handle parametrise URL here.......
  if(request_type == HTTP_REQUEST)
    strcpy(fully_qualified_url, "http://");  
  else if(request_type == HTTPS_REQUEST)
    strcpy(fully_qualified_url, "https://");  
  else
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012145_ID, CAV_ERR_1012145_MSG);

  // Check if url is absolute URL
  if(strncmp(url, "/", 1) == 0)
    sprintf(fully_qualified_url, "%s%s%s", fully_qualified_url, hostname, url);
  
  else
    sprintf(fully_qualified_url, "%s%s%s/%s", fully_qualified_url, hostname, path, url);


  NSDL2_PARSING(NULL, NULL, "returning url = %s", fully_qualified_url); 

  return 0;
}

// This must be called for all URLs event if redirect and location is not there
static int set_redirect_url(char *url, char is_redirect, char *location, char *flow_file, int url_idx)
{
int ret;
char fully_qualified_location[MAX_LINE_LENGTH] = "";


  NSDL2_PARSING(NULL, NULL, "method called, url = %s, is_redirect = %d, location = %s, flow_file = %s, url_idx = %d,", 
                             url, is_redirect, location, flow_file, url_idx);

  // For Inline URL and auto redirect is ON
  // Search if this URL is in the linked list of all locations.
  // If Yes, remove from the linked list and
  // reuse request table by reducing total_request_entries
  NSDL2_PARSING(NULL, NULL, "requests[url_idx].proto.http.type = %d", requests[url_idx].proto.http.type);
  if(requests[url_idx].proto.http.type == EMBEDDED_URL)
  {
    NSDL2_PARSING(NULL, NULL,"auto redirect = %d", global_settings->g_follow_redirects);
    if (global_settings->g_follow_redirects) {
      redirect_location *r1;
      r1 = search_redirect_location(url);
      if (r1) {
        NSDL2_PARSING(NULL, NULL,"%s found in linked list, auto redirect is on so deleting it from the list.", r1);
        delete_red_location(r1);
      /* We decrement it here so that we fill the vacant location left by the ignored data.
         The original value of total_request_entries is restored at the end of the while loop.
         This Process leaves holes in request array. Which are propagated to the shared array as well.
      */
        NSDL2_PARSING(NULL, NULL,"total_request_entries = %d", total_request_entries);
        total_request_entries--;
        gPageTable[g_cur_page].num_eurls--; 
      }
    }
  }

  if(is_redirect == 0)  return 0;

  if(location[0] == '\0')
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012146_ID, CAV_ERR_1012146_MSG);

  if(requests[url_idx].proto.http.type == MAIN_URL)
  {
    // For manual auto redirect, we need to mark Main url as redirect_url
    // For Auto redirect, it will be handled as per respose of main url
    if (!(global_settings->g_follow_redirects) && is_redirect)
    {
      NSDL2_PARSING(NULL, NULL, "Main url and is_redirect is 1 and auto redirect is Off, so making it redirect URL");
      requests[url_idx].proto.http.type = REDIRECT_URL;
    }
  }
  else
  {
    // Mark embedded url as redirect url. It will be changed back to embedded url
    // in arrange_page_urls(). Need to check if we can remote this
    NSDL2_PARSING(NULL, NULL, "Embedded url and is_redirect is 1, so making it redirect URL");
    requests[url_idx].proto.http.type = REDIRECT_URL;
  }

  // Convert location to fully qualified using current url
  // strcpy(fully_qualified_location, location);
  ret = make_fully_qualified_url(url, location, fully_qualified_location);
  NSDL2_PARSING(NULL, NULL, "fully_qualified_location = %s", fully_qualified_location);
  if(ret == -1)
  {
    NS_EXIT(-1, "make_fully_qualified_url failed");
  }

  if(global_settings->g_follow_redirects && is_redirect)
    add_redirect_location(is_redirect, fully_qualified_location);

  return NS_PARSE_SCRIPT_SUCCESS;
}

 int set_body(int url_idx, int sess_idx, char *body_start, char *body_end, char *flow_file) 
{
  NSDL2_PARSING(NULL, NULL,"Method Called, flow_file = [%s], sess_idx = %d, url_idx = %d", flow_file, sess_idx, url_idx);

  if(post_process_post_buf(url_idx, sess_idx, &script_ln_no, flow_file) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;
  
  return NS_PARSE_SCRIPT_SUCCESS;
}

void set_http_default_values(int url_idx, char inline_enabled)
{
  //Set default flags and method for GRPC type request if user will not use these parameter in the script.
  //TODO: need to change default http_version for grpc as per requirement document set its default value to 1.  
  if (inline_enabled == GRPC_CLIENT)
  {
    requests[url_idx].flags |= NS_REQ_FLAGS_GRPC_CLIENT;
    requests[url_idx].proto.http.body_encoding_flag = BODY_ENCODING_GRPC_PROTO;
    requests[url_idx].proto.http.http_method = HTTP_METHOD_POST; // Set HTTP Method
    requests[url_idx].proto.http.http_method_idx = HTTP_METHOD_POST;
    requests[url_idx].proto.http.http_version = HTTP_MODE_HTTP2;  // HTTP2
  }
  else
  {
    //Setting the defalut Method to GET
    requests[url_idx].proto.http.http_method = HTTP_METHOD_GET;
    requests[url_idx].proto.http.http_method_idx = HTTP_METHOD_GET; //for standred methods index is same as macros 
    requests[url_idx].proto.http.http_version = 1;  // HTTP/1.1
  }
}

/********************Multipart Body Start****************/
int extract_multipart_body(FILE *flow_fp, char *start_ptr, char **end_ptr, char *flow_file, int sess_idx, char **starting_quotes, int body_begin_flag, FILE *outfp)
{
  char *quotes_ptr = NULL;
  char *file_name, buf[8192];
  int fd, len;
  char *ptr;
  int quotes_found = 0;

  NSDL2_PARSING(NULL, NULL,"Method Called, flow_file = [%s], body_begin_flag = %d", flow_file, body_begin_flag);
  // If body_begin_flag is not present then check for start_ptr not have "BODY= return error
  if(!body_begin_flag) {
    if((start_ptr = strstr(start_ptr, "=")) == NULL)
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012149_ID, CAV_ERR_1012149_MSG);

    start_ptr = start_ptr + 1;
  }

/*
  multipart body ends with MULTIPART_BOUNDARY, MULTIPART_BODY_END or MULTIPART_BODY_END followed by  );
  MULTIPART_BODY_BEGIN,
  "HEADER=..." ,
  MULTIPART_BOUNDARY,
  "HEADER= ..",
   ...
  "BODY=Line 1
  Line 2
  Line 3"
  MULTIPART_BOUNDARY,
  ...
  "BODY= ..."
  ...
  MULTIPART_BODY_END
  );
 */

  copy_to_multipart_body_post_buf("\xd\xa",2, multipart_body_parts_count-1, 0);     //start body on new line
  //if the content-type is not explicitly given, leave a blank to indicate default (text/plain).
// TODO: check if we will add this newline or not 
/*  if (multipart_body_parts[multipart_body_parts_count-1].content_type == 0) {
    NSDL2_PARSING(NULL, NULL,"Adding new line as content type is not present");
    copy_to_multipart_body_post_buf("\xa",1, multipart_body_parts_count-1, 0);     //leave a blank line
  }
*/

  // To handle BODY_BEGIN and BODY_END blocks in body  
  if(body_begin_flag){
    if(read_line_and_comment(flow_fp, outfp) != NULL){
      *starting_quotes = script_line;
      start_ptr = script_line;
      CLEAR_WHITE_SPACE(start_ptr);
      if(*start_ptr == '"')
        start_ptr++;
    }
  } 

  NSDL2_PARSING(NULL, NULL,"start_ptr = %s", start_ptr);
  quotes_ptr = strrchr(start_ptr, '"');

  if (quotes_ptr != NULL) {
    NSDL2_PARSING(NULL, NULL,"\" found");
    //if its an include file, it should be on 1 line
    if ( (strncasecmp (start_ptr, "$CAVINCLUDE$=", 13) == 0) || (strncasecmp (start_ptr, "$CAVINCLUDE_NOPARAM$=", 21) == 0) ) {
      if (strncasecmp (start_ptr, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
        noparam_array[multipart_body_parts_count-1] = 1;
      file_name = strchr(start_ptr, '=');
      file_name++;
      CLEAR_WHITE_SPACE(file_name);
      if (file_name[0] != '/') {
        /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
        sprintf (buf, "%s/%s/%s", GET_NS_TA_DIR(),
          get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), file_name);
          //Previously taking with only script name
          //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), file_name);
      }
      NSDL2_PARSING(NULL, NULL, "buf = %s", buf);
      //remove ",\n at the end
      if ( (ptr = strchr(buf,'"')) != NULL)
        *ptr = 0;
      CLEAR_WHITE_SPACE_FROM_END(buf);
      file_name = buf;
      NSDL2_PARSING(NULL, NULL, "opening include file %s to read from",file_name);

      if ( (fd = open (file_name, O_RDONLY|O_CLOEXEC)) < 0) {
        printf("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, file_name);
        perror("open");
        return NS_PARSE_SCRIPT_ERROR;
      }

      while ( (len = read(fd, buf, 8192)) != 0) {
        copy_to_multipart_body_post_buf(buf, len, multipart_body_parts_count-1, 0);
      }
      close(fd);
      if (len < 0) {
        perror("reading CAVINCLUDE BODY");
        printf("%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, file_name);
        return NS_PARSE_SCRIPT_ERROR;
      }
      *end_ptr = quotes_ptr;
      if(read_line_and_comment(flow_fp, outfp) != NULL){
        CLEAR_WHITE_SPACE(script_line);
        *starting_quotes = script_line;
      } else 
         SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012148_ID, CAV_ERR_1012148_MSG);

      if(body_begin_flag)  {
        if(read_line_and_comment(flow_fp, outfp) != NULL){
          CLEAR_WHITE_SPACE(script_line);
          *starting_quotes = script_line;
          }
       }
       return NS_PARSE_SCRIPT_SUCCESS;
    } 
    //NSDL2_PARSING(NULL, NULL,"Single line found quotes_ptr = [%c][%c]", *(quotes_ptr+1), *(quotes_ptr+2));
    
    if(*(quotes_ptr + 1) == ','){ 
      NSDL2_PARSING(NULL, NULL,"' and new line found");
      copy_to_multipart_body_post_buf(start_ptr, strlen(start_ptr), multipart_body_parts_count-1, 1);
      quotes_found = 1;
    }else {
      copy_to_multipart_body_post_buf(start_ptr, strlen(start_ptr), multipart_body_parts_count-1, 0);
    }
  } else { // Multiline body copy first line in the buffer 
    copy_to_multipart_body_post_buf(start_ptr, strlen(start_ptr), multipart_body_parts_count-1, 0);
    *end_ptr = quotes_ptr;
  }

  int body_over = 0;
  char *tmp;
  //body is on > 1 lines. read until we find the end of BODY
  while (read_line_and_comment(flow_fp, outfp) != NULL)
  {
    if (quotes_found) {
      if((tmp = strstr(script_line, "MULTIPART_BOUNDARY")) != NULL){
        NSDL2_PARSING(NULL, NULL,"Found MULTIPART_BOUNDARY");
        body_over = 1;
      } else if(( tmp = strstr(script_line, "MULTIPART_BODY_END")) != NULL){
        NSDL2_PARSING(NULL, NULL,"Found MULTIPART_BODY_END");
        body_over = 1;
      } else if(( tmp = strstr(script_line, "BODY_END")) != NULL){
        NSDL2_PARSING(NULL, NULL,"Found BODY_END");
        continue; // continue to consume one more line
      }
   
      if(body_over){ 
        //*end_ptr = quotes_ptr;
        //*end_ptr = quotes_ptr;
        *starting_quotes = tmp;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
    }
    if(strrchr(script_line, '"'))
      quotes_found = 1;
    copy_to_multipart_body_post_buf(script_line, strlen(script_line), multipart_body_parts_count-1, 0);   //Subtracting 1 for skipping closing quotes

 }
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012147_ID, CAV_ERR_1012147_MSG);

  return NS_PARSE_SCRIPT_SUCCESS;
}
/********************Multipart Body End*******************/

// Segment line temporarily, to handle ITEMDATA parameterization
int encode_item_data_value(int url_idx, int sess_idx, char *body_buf, int *script_ln_no, char *file_name)
{
  int i=0, j=0, hash_code_idx;
  unsigned long len;
  int hash_array[ITEMDATA_HASH_ARRAY_SIZE]; //to store hash codes
  StrEnt *tmpPostTable = NULL;
  char *tmp, *varname, *encoded_string = NULL;

  NSDL1_PARSING(NULL, NULL, "Method called");

  if(!body_buf) return 0;

  // Allocate a StrEnt for local use only, will be freed at the end of this method
  MY_MALLOC(tmpPostTable, (sizeof(StrEnt)), "tmpPostTable", -1);
  if(!tmpPostTable)
    SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in allocating memory for tmpPostTable");

  tmpPostTable->seg_start = -1;
  tmpPostTable->num_entries = 0;

  //implementing local segments to support parameterization
  segment_line_int_int(tmpPostTable, body_buf, 0, *script_ln_no, sess_idx, file_name, 1, hash_array);

  if(tmpPostTable->seg_start < 0 || tmpPostTable->num_entries <= 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012151_ID, CAV_ERR_1012151_MSG);

  for(i = tmpPostTable->seg_start, j = 0; i < (tmpPostTable->seg_start + tmpPostTable->num_entries); i++, j++)
  {
    if(segTable[i].type == STR) //then encode and copy to post buf
    {
      // get big buffer pointer and segment length
      tmp = RETRIEVE_BUFFER_DATA(pointerTable[segTable[i].offset].big_buf_pointer);
      len = pointerTable[segTable[i].offset].size;
      NSDL4_PARSING(NULL, NULL, "Segment for idx %d is of type string, value = %*.*s", i, len, len, tmp);

      if(requests[url_idx].proto.http.body_encoding_flag == BODY_ENCODING_URL_ENCODED)
      {
        encoded_string = ns_encode_url(tmp, len);
        if(!encoded_string)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012152_ID, CAV_ERR_1012152_MSG, tmp);

        copy_to_post_buf(encoded_string, strlen(encoded_string)); 
        ns_encode_decode_url_free(encoded_string);
      }
      else //copy to post buf without encoding
      {
        copy_to_post_buf(tmp, len);
      }
    }
    else
    {
      hash_code_idx = hash_array[j];
      NSDL4_PARSING(NULL, NULL, "Segment is of variable type. Copying in to buffer without encoding. Segment hash code = %d", hash_code_idx);
      varname = (char *)gSessionTable[sess_idx].var_get_key(hash_code_idx);
      copy_to_post_buf("{", 1);
      copy_to_post_buf(varname, strlen(varname));
      if(segTable[i].data > 0)
      {
        char tmpbuf[20];
        char *data;
        int value;
        data = RETRIEVE_BUFFER_DATA(segTable[i].data);
        value = *(int *)data;
        sprintf(tmpbuf, "%d", value);
        copy_to_post_buf("_", 1);
        copy_to_post_buf(tmpbuf, strlen(tmpbuf));
      }
      copy_to_post_buf("}", 1);
    }
  }
  FREE_AND_MAKE_NULL(tmpPostTable, "Free tmpPostTable", -1);
  return 0;
}

/*
SM, parse ITEMDATA lines, encode one line, place it into post buffer, do same for all ITEMDATA lines
examples:-

ITEMDATA,
"name=n1","value=v1",
"name=n1 n2", "value=v1",
"name=n1\"n2\"n3"   , "value = "v1\"v2\"v3 "
ITEMDATA_END


ITEMDATA,
"name=n1","value=v1",
"name=n1 n2", "value=v1",
"name=n1\"n2\"n3"   , "value = "v1\"v2\"v3 "
ITEMDATA_END

*/
int parse_encode_set_bodydata(int url_idx, int sess_idx, FILE *flow_fp, char *start_ptr, char **end_ptr, char embedded_url, char *flow_file, char **starting_quotes, FILE *outfp)
{
  char *tbuf = NULL, *ptr, *ptr1;
  int first_line = 1, body_over = 0;
  int rnum = 0, noparam_flag = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, start_ptr = %s", start_ptr);

  CLEAR_WHITE_SPACE(start_ptr);
  if(strncasecmp(start_ptr, "ITEMDATA,", 8))
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012153_ID, CAV_ERR_1012153_MSG, "ITEMDATA");

  //read next line from script file
  while(read_line_and_comment(flow_fp, outfp))
  {
    CLEAR_WHITE_SPACE(script_line);
    CLEAR_WHITE_SPACE_FROM_END(script_line);

    //CAVREPEAT_BLOCK_START and CAVREPEAT_BLOCK_END will be on separate lines, in both cases copy it as it is
    if( !strncmp(script_line, "{$CAVREPEAT_BLOCK_START", 23) || !strncmp(script_line, "{$CAVREPEAT_BLOCK_END}", 22) )
    {
      copy_to_post_buf(script_line, strlen(script_line)); //copy line as it is, then skip to next line
      continue;
    }

    //read first quote, it can't be an escaped quote
    start_ptr = strchr(script_line, '"');
    if(!start_ptr)
    {
      if((*end_ptr = strstr(script_line, "ITEMDATA_END);")) != NULL) /* bracket is in the same line */
      {
        NSDL2_PARSING(NULL, NULL, "ITEMDATA block is over, *end_ptr = %s", *end_ptr);
        body_over = 1;
        break;
      } 
      else if((*end_ptr = strstr(script_line, "ITEMDATA_END,")) != NULL) // Handling of ITEMDATA_END, Bug 113229
      {
        NSDL2_PARSING(NULL, NULL, "ITEMDATA block is over, *end_ptr = %s", *end_ptr);
        body_over = 1;
        *end_ptr += 13;
        break;
      } 
      else if((*end_ptr = strstr(script_line, "ITEMDATA_END")) != NULL) // bracket is in new line, consume one extra line Bugid: 7629
      {
        NSDL2_PARSING(NULL, NULL, "ITEMDATA block is over, *end_ptr = %s", *end_ptr);
        body_over = 1;
        read_line_and_comment(flow_fp, outfp);
        *starting_quotes = script_line;
        //starting_quotes = script_line;
        break;
      }
      SCRIPT_PARSE_ERROR_EXIT_EX(start_ptr, CAV_ERR_1012150_ID, CAV_ERR_1012150_MSG);
    }
    start_ptr++; //skip to next char after quote

    //if there are white spaces between quote and beginning of name then skip those white spaces
    CLEAR_WHITE_SPACE(start_ptr);

    //look for NAME key
    if(strncasecmp(start_ptr, "name", 4))
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012153_ID, CAV_ERR_1012153_MSG, "NAME");
    
    //as NAME key has 4 chars, move 4 chars forward
    start_ptr += 4;

    //if there are white spaces between NAME tag and following = symbol then skip those white spaces
    CLEAR_WHITE_SPACE(start_ptr);

    //now, we are at = symbol, if not then it is an error
    if(*start_ptr != '=')
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012500_ID, CAV_ERR_1012500_MSG, "NAME");
    
    //move past the = symbol
    start_ptr++;
    //after = symbol, whichever come till " are name of NAME
    //need to take care of escaped quotes here

    ptr = strchr(start_ptr, '"'); //look for next or closure quote of name

    //if no next quote found then it is an error
    if(!ptr)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012501_ID, CAV_ERR_1012501_MSG, "NAME");

    //skip escaped quotes and look for closing quote
    while(1)
    {
      //check for this quote if it is escape quote, if yes then look for next unesaped quote
      if(*(ptr-1) == '\\') //if just previous symbol to this quote is escape char, then look for next quote
      {
        ptr1 = ptr + 1; //move next to this escaped quote
        ptr = strchr(ptr1, '"');

        //if no next quote found then it is an error
        if(!ptr)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012501_ID, CAV_ERR_1012501_MSG, "NAME");
      }
      else break;
    }
    //now after this loop, ptr is pointing to closing quote

    if(!first_line) copy_to_post_buf("&", 1);

    //get the value of NAME tag
    MY_MALLOC(tbuf, (ptr - start_ptr + 1), "tbuf", -1);
    if(!tbuf)
      SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in allocating memory for tbuf");
    
    memset(tbuf, 0, ptr - start_ptr + 1);
    memcpy(tbuf, start_ptr, ptr - start_ptr);

    //now, put the value of NAME tag into local segment table and copy to post buf
    encode_item_data_value(url_idx, sess_idx, tbuf, &script_ln_no, flow_file);
   
    FREE_AND_MAKE_NULL(tbuf, "Free tbuf", -1); 
    
    copy_to_post_buf("=", 1);
    ptr++; //past the closing quote of name
    start_ptr = ptr;
    //clear any white spaces after closing quote and comma
    if(start_ptr)
      CLEAR_WHITE_SPACE(start_ptr)
    else
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012154_ID, CAV_ERR_1012154_MSG, "Value part");

    //it should point to comma, else it is an error
    if(*start_ptr != ',')
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012502_ID, CAV_ERR_1012502_MSG, "Comma(,)");

    //move past the comma and clear any white space between comma and next quote which would be beginning of value part
    start_ptr++;
    if(start_ptr)
      CLEAR_WHITE_SPACE(start_ptr)
    else
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012154_ID, CAV_ERR_1012154_MSG, "Value part");

    //it is first quote of value part which can't be escaped
    if(*start_ptr != '"')
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012502_ID, CAV_ERR_1012502_MSG, "Starting quote");

    start_ptr++;

    //clear any white spaces between starting quote and VALUE tag
    if(start_ptr)
      CLEAR_WHITE_SPACE(start_ptr)
    else
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012154_ID, CAV_ERR_1012154_MSG, "Value part");

    if(strncasecmp(start_ptr, "value", 5))
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012153_ID, CAV_ERR_1012153_MSG, "VALUE");

    start_ptr += 5; //move past VALUE tag, clear any white spaces
    CLEAR_WHITE_SPACE(start_ptr);

    //next should be = symbol to have value
    if(*start_ptr != '=')
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012500_MSG, CAV_ERR_1012500_MSG, "VALUE");

    //move past = symbol
    start_ptr++; //after = symbol, whichever come till " is value of VALUE

    ptr = strchr(start_ptr, '"'); //look for next or closure quote of VALUE

    //if no quote found then it is an error
    if(!ptr)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012503_MSG, CAV_ERR_1012503_MSG, "Closing quote", "VALUE");

    //skip escaped quotes and look for closing quote
    while(1)
    {
      //check for this quote if it is escape quote, if yes then look for next unesaped quote
      if(*(ptr-1) == '\\') //if just previous symbol to this quote is escape char, then look for next quote
      {
        ptr1 = ptr + 1; //move next to this escaped quote
        ptr = strchr(ptr1, '"');

        //if no next quote found then it is an error
        if(!ptr)      
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012503_MSG, CAV_ERR_1012503_MSG, "Closing quote", "NAME");
      }
      else break;
    }
    //now after this loop, ptr is pointing to closing quote

    //get the value of VALUE tag
    if(ptr - start_ptr > 0)
    {
      MY_MALLOC(tbuf, (ptr - start_ptr + 1), "tbuf", -1);
      if(!tbuf)
        SCRIPT_PARSE_ERROR_EXIT(NULL, "Error in allocating memory for tbuf");

      memset(tbuf, 0, ptr - start_ptr + 1);
      memcpy(tbuf, start_ptr, ptr - start_ptr);
    }
    else tbuf = NULL;

    //now, put the value of VALUE tag into local segment table
    encode_item_data_value(url_idx, sess_idx, tbuf, &script_ln_no, flow_file);

    FREE_AND_MAKE_NULL(tbuf, "Free tbuf", -1); 

    first_line = 0; //now is the turn of next lines, so set it to FALSE
  }
  start_ptr = script_line;
  if(!body_over)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012155_ID, CAV_ERR_1012155_MSG);

  if (cur_post_buf_len <= 0) return NS_PARSE_SCRIPT_SUCCESS;

  NSDL2_PARSING(NULL, NULL, "g_post_buf = %s", g_post_buf);

  save_and_segment_body(&rnum, url_idx, g_post_buf, noparam_flag, &script_ln_no, sess_idx, flow_file);
  return NS_PARSE_SCRIPT_SUCCESS;
}


int extract_body(FILE *flow_fp, char *start_ptr, char **end_ptr, char embedded_url, char *flow_file, int body_begin_flag, FILE *outfp)
{
  //int fno;
  //fno = fileno(flow_fp);
  NSDL2_PARSING(NULL,NULL,"file name is [%d] ", fileno(flow_fp));
  //char *quotes_ptr = NULL;
  int body_over = 0;
  char *ptr;

  int end_flag = 0;
  int first = 1;
  //if start_ptr not have "BODY= return error
  //BODY must contain =, if not containgive error
  if(!body_begin_flag) {
    if((start_ptr = strstr(start_ptr, "=")) == NULL)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012504_ID, CAV_ERR_1012504_MSG);

    start_ptr = start_ptr + 1;
    //if(strrchr(script_line, '"')) // Find the last quote
      //quotes_ptr = strrchr(script_line, '"'); // No use of quotes_ptr
    copy_to_post_buf(start_ptr, strlen(start_ptr));
  }

/*
  "BODY=Line 1
Line 2
Line 3",
  INLINE_URLS, or ); or END_LINE or BODY_END
*/

/*  
  //Checking INLINE_URLS or ); found in case of Main Url
  //and END_INLINE is found in case of Embedded Url on first line
  if((strstr(script_line, "INLINE_URLS") ||  strstr(script_line, ");")) && (embedded_url == '0'))
    return NS_PARSE_SCRIPT_ERROR;
  else if (strstr(script_line, "END_INLINE") && (embedded_url == '1'))
    return NS_PARSE_SCRIPT_ERROR;
  else
*/


  
  //read the another line of the flow_fp
  while (read_line_and_comment(flow_fp, outfp) != NULL)
  {
    ptr = script_line; 
    // In case of BODY_BEGIN we need to skip white spaces before " and " also
    if(body_begin_flag && first) {
      CLEAR_WHITE_SPACE(ptr);
      if(*ptr == '"')
        ptr++;
      //body_begin_flag = 0;
      first = 0;
    }

    //if script_line is ITEMDATA, it means along with BODY, there is also ITEMDATA which is an error
    if(!strncasecmp(script_line, "ITEMDATA,", 9))
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012156_ID, CAV_ERR_1012156_MSG, "ITEMDATA");

    //If quotes found in script_line, quotes_ptr will point to the "
    //else will restore the previous quotes if quotes not line in the line
    //if(strrchr(ptr, '"')) // Find the last quote
      //quotes_ptr = strrchr(ptr, '"');

    *end_ptr = NULL;
    // Set the value of the end_ptr
    // check for BODY_END 
    *end_ptr = strstr(ptr, "BODY_END");
    if(*end_ptr != NULL){
      end_flag = 1;
      continue;
    }
    else {
      if(embedded_url)
      {

        *end_ptr = strstr(ptr, "END_INLINE");
      }
      else
      {
        *end_ptr = strstr(ptr, "INLINE_URLS");
        if(*end_ptr == NULL){
          if(body_begin_flag){
             if(end_flag)
               *end_ptr = strstr(ptr, ");");
          }else{
              *end_ptr = strstr(ptr, ");");
          }
        }
      }
    }
    //In case INLINE_URLS or ); found in case of Main Url
    //and END_INLINE is found in case of Embedded Url
    if(*end_ptr != NULL) 
    {
      NSDL2_PARSING(NULL, NULL, "Body is over");
      body_over = 1;
      break;
    }
    else
    {
      copy_to_post_buf(ptr, strlen(ptr));   //Subtracting 1 for skipping closing quotes
    }
  }
   
  if(body_over == 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012505_ID, CAV_ERR_1012505_MSG);
 
  return NS_PARSE_SCRIPT_SUCCESS;
}

int init_url(int *url_idx, int embedded_urls, char *flow_file, char inline_enabled)
{  
  NSDL2_PARSING(NULL, NULL, "Method Called. embedded_urls = %d", embedded_urls);

  //allocating memory for http_request entries
  create_requests_table_entry(url_idx);

  if(embedded_urls == 0) // For main URL, we need to set main url index as first_eurl
    gPageTable[g_cur_page].first_eurl = *url_idx; 

  // Here cacheRequestHeader is initialized. this is done to set the vale of max-age, min-fresh, max-stale to -1.
  cache_init_cache_req_hdr(&(requests[*url_idx].proto.http.cache_req_hdr));

  set_http_default_values(*url_idx, inline_enabled);
  init_post_buf(); 
  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;

}

static void override_url_parameters(int url_idx, int embedded_urls, char *flow_file)
{
  NSDL2_PARSING(NULL, NULL, "Method called");

  if (global_settings->use_http_10)
  {
    NSDL2_PARSING(NULL, NULL, "Overriding HTTP verion to 1.0");
    requests[url_idx].proto.http.http_version = 0;  // HTTP/1.0
  }

}

static int set_auth_ntlm_uname_pwd(char *val, char *flow_file, int url_idx, int sess_idx, int script_ln_no, int flag_uname_pwd)
{
  NSDL2_PARSING(NULL, NULL, "Method Called val = %s", val);

  if(!val){


    NSDL2_PARSING(NULL, NULL, "Error: %s value could not be parsed from flowfile %s", 
                               flag_uname_pwd?"HTTPAuthPassword":"HTTPAuthUserName",
                               flow_file?flow_file:"NULL");
   

    return NS_PARSE_SCRIPT_ERROR;
  }

  CLEAR_WHITE_SPACE(val);
  CLEAR_WHITE_SPACE_FROM_END(val);

  if(val[0] == '\0'){
    NSDL2_PARSING(NULL, NULL, "Error: %s value could not be parsed from flowfile %s", 
                               flag_uname_pwd?"HTTPAuthPassword":"HTTPAuthUserName",
                               flow_file?flow_file:"NULL");
 
    return NS_PARSE_SCRIPT_ERROR;
  }

  switch(flag_uname_pwd)
  {
    case 0: /* HTTPAuthUserName */
      segment_line(&requests[url_idx].proto.http.auth_uname, val, 0, script_ln_no, sess_idx, flow_file);
      break;

    case 1: /* HTTPAuthPassword */
      segment_line(&requests[url_idx].proto.http.auth_pwd, val, 0, script_ln_no, sess_idx, flow_file);
      break;

    default:
      NSDL2_PARSING(NULL, NULL, "Error: HTTPAuthPassword or HTTPAuthUserName value could not "
                                "be parsed from flowfile %s", flow_file?flow_file:"NULL");
 
      return NS_PARSE_SCRIPT_ERROR;

  }

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_repeat_inline_val(char *val, char *flow_file, int url_idx, int sess_idx, int script_ln_no)
{
  NSDL2_PARSING(NULL, NULL, "Method Called val = %s, url_idx = %d, sess_idx = %d, script_ln_no = %d", 
                             val?(val[0]?val:"NULL"):"NULL", url_idx, sess_idx, script_ln_no);

  if(!val)
  {
    NSDL2_PARSING(NULL, NULL, "Error: No value found in repeat inline URL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  CLEAR_WHITE_SPACE(val);
  CLEAR_WHITE_SPACE_FROM_END(val);

  if(val[0] == '\0')
  {
    NSDL2_PARSING(NULL, NULL, "Error: NO Value found in repeat inline URL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  if(atoi(val) < 0)
  {
    NSDL2_PARSING(NULL, NULL, "Error: Negative Value found in repeat inline URL");
    return NS_PARSE_SCRIPT_ERROR;
  }
  NSDL2_PARSING(NULL, NULL, "Insert repeat value in segment table at url_idx = %d", url_idx);
  segment_line(&requests[url_idx].proto.http.repeat_inline, val, 0, script_ln_no, sess_idx, flow_file);

  return NS_PARSE_SCRIPT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse RBU field of ns_web_url() API. 
 *             RBU fields are - BrowserUserProfile, HarLogDir, VncDisplayId, HarRenameFlag, PageLoadWaitTime 
 *
 * Input     : val             - provide value of RBU fields, 
 *                                Eg: if "HarLogDir={NS_PROF}" then in val={NS_PROF} 
 *
 *             flow_file       - provide name of flow_file 
 *
 *             url_idx         - provide url index in requests table 
 *
 *             sess_idx        - provide session index in gSession table
 *
 *             script_ln_no    - provide script line number 
 *
 *             rbu_param_flag  - provide which field has to be parsed
 *
 * Output    : On error     -1
 *             On success    0 
 *--------------------------------------------------------------------------------------------*/
int set_rbu_param(char *val, char *flow_file, int url_idx, int sess_idx, int script_ln_no, int rbu_param_flag)
{
  int num_fields = 0;
  int perf_timeout;
  char *fields[5];

  NSDL1_PARSING(NULL, NULL, "Method Called val = %s, flow_file = %s, url_idx = %d, sess_idx, = %d, script_ln_no = %d, rbu_param_flag = %d", 
                             val?val:NULL, flow_file, url_idx, sess_idx, script_ln_no, rbu_param_flag);

  //If atrribute value not given
  if(!val)
  {
    NSDL2_PARSING(NULL, NULL, "Error: BrowserUserProfile value could not be parsed from flowfile %s", flow_file?flow_file:"NULL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  CLEAR_WHITE_SPACE(val);
  CLEAR_WHITE_SPACE_FROM_END(val);

  if(val[0] == '\0')
  {
    NSDL2_PARSING(NULL, NULL, "Error: BrowserUserProfile value could not be parsed from flowfile %s", flow_file?flow_file:"NULL");
    return NS_PARSE_SCRIPT_ERROR;
  }

  //If user not provide any value then we set difault values 
  if(!strcasecmp(val, "NA"))
  {
    NSDL2_PARSING(NULL, NULL, "RBU - value %s", val);
    return NS_PARSE_SCRIPT_SUCCESS;
  }

  /* Since Thses fields are opyional so we should not store parametrise field into segment table*/
  switch(rbu_param_flag)
  {
    case RBU_PARAM_BROWSER_USER_PROFILE: /* BrowserUserProfile */
      if((requests[url_idx].proto.http.rbu_param.browser_user_profile = copy_into_big_buf(val, 0)) == -1) 
      {
        NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", val);
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;

    case RBU_PARAM_HAR_LOG_DIR: /* HarLogDir */
      if((requests[url_idx].proto.http.rbu_param.har_log_dir = copy_into_big_buf(val, 0)) == -1) 
      {
        NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", val);
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;

    case RBU_PARAM_VNC_DISPLAY_ID: /* VncDisplayId */
      if((requests[url_idx].proto.http.rbu_param.vnc_display_id = copy_into_big_buf(val, 0)) == -1) 
      {
        NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", val);
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;

    case RBU_PARAM_HAR_RENAME_FLAG: /* HarRenameFlag */
      requests[url_idx].proto.http.rbu_param.har_rename_flag = atoi(val);
      break;

    case RBU_PARAM_PAGE_LOAD_WAIT_TIME: /* PageLoagWaitTime */
      requests[url_idx].proto.http.rbu_param.page_load_wait_time = atoi(val);
      /* pageloadwait time should not be less then 60 second*/
      if(requests[url_idx].proto.http.rbu_param.page_load_wait_time < RBU_DEFAULT_PAGE_LOAD_WAIT_TIME)
      {
        NSDL2_PARSING(NULL, NULL, "Error: RBU Params  value could not be parsed from flow file %s", flow_file?flow_file:"NULL");
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012158_ID, CAV_ERR_1012158_MSG, "PageLoadWaitTime");
      }
      break;

    case RBU_PARAM_MERGE_HAR_FILES: /* MergeHarFile */
      requests[url_idx].proto.http.rbu_param.merge_har_file = atoi(val);
      break;
    
    case RBU_PARAM_SCROLL_PAGE_X: /* ScrollPageX */
      requests[url_idx].proto.http.rbu_param.scroll_page_x = atoi(val);
      if(requests[url_idx].proto.http.rbu_param.scroll_page_x < 0) 
      {
        NSDL2_PARSING(NULL, NULL, "Error: RBU Params  value could not be parsed from flow file %s", flow_file?flow_file:"NULL");
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;
   
    case RBU_PARAM_SCROLL_PAGE_Y: /* ScrollPageY */
      requests[url_idx].proto.http.rbu_param.scroll_page_x = atoi(val);
      if(requests[url_idx].proto.http.rbu_param.scroll_page_x < 0) 
      {
        NSDL2_PARSING(NULL, NULL, "Error: RBU Params  value could not be parsed from flow file %s", flow_file?flow_file:"NULL");
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;

    case RBU_PARAM_PRIMARY_CONTENT_PROFILE: /* PrimaryContentProfile */ 
      //we only parse tti.json file if we found primarycontentprofile in flow.
      if(!parse_tti_file_done)
      {
        if (ns_rbu_parse_tti_json_file(sess_idx) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;

        parse_tti_file_done = 1;  //set parse keyword flag to one so that it will not parse tti.json file again for the same flow.
                                  //we are resetting this flag at script level because we have to parse the tti.json file for each script. 
      }

      //chek value contain string or null 
      if((requests[url_idx].proto.http.rbu_param.primary_content_profile = get_tti_profile(val, url_idx, sess_idx, script_ln_no, flow_file)) == -1)
      {
         NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", val);
         return NS_PARSE_SCRIPT_ERROR;
      }
      break; 

     case RBU_PARAM_WAIT_FOR_NEXT_REQ:  /*TimeOut for next request*/
       requests[url_idx].proto.http.rbu_param.timeout_for_next_req = atoi(val);
       if(requests[url_idx].proto.http.rbu_param.timeout_for_next_req < 500)
       {
         NSDL2_PARSING(NULL, NULL, "Error: RBU_PARAM_WAIT_FOR_NEXT_REQ value is less than 500ms");
         return NS_PARSE_SCRIPT_ERROR;
       }
       break;

     case RBU_PARAM_PERFORMANCE_TRACE:  /* PerformanceTraceLog */
       //Input Format: <mode>:<timeout>:<memory_flag>:<screenshot_flag>:<duration_level>
       num_fields = get_tokens_ex2(val, fields, ":", 5);
       switch(num_fields)
       {
         case 5:
           if((ns_is_numeric(fields[4]) == 0) || (match_pattern(fields[4], "^[0-1]$") == 0))
           {
             NSDL2_PARSING(NULL, NULL, "Error: PerformanceTraceLog-Duration-Level (%s) is not valid.", fields[4]);
             SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012159_ID, CAV_ERR_1012159_MSG, "PerformanceTraceLog-Duration-Level", fields[4]);
           }
           else
             requests[url_idx].proto.http.rbu_param.performance_trace_duration_level = atoi(fields[4]);
         case 4:
           if((ns_is_numeric(fields[3]) == 0) || (match_pattern(fields[3], "^[0-1]$") == 0))
           {
             NSDL2_PARSING(NULL, NULL, "Error: PerformanceTraceLog-Screenshot Mode (%s) is not valid.", fields[3]);
             SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012159_ID, CAV_ERR_1012159_MSG, "PerformanceTraceLog-Screenshot", fields[3]);
           }
           else
             requests[url_idx].proto.http.rbu_param.performance_trace_screenshot_flag = atoi(fields[3]);
         case 3:
           if((ns_is_numeric(fields[2]) == 0) || (match_pattern(fields[2], "^[0-1]$") == 0))
           {
             NSDL2_PARSING(NULL, NULL, "Error: PerformanceTraceLog-Memory-Trace Mode (%s) is not valid.", fields[2]);
             SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012159_ID, CAV_ERR_1012159_MSG, "PerformanceTraceLog-Memory-Trace", fields[2]);
           }
           else
             requests[url_idx].proto.http.rbu_param.performance_trace_memory_flag = atoi(fields[2]);
         case 2:
           if(ns_is_numeric(fields[1]) == 0)
           {
             NSDL2_PARSING(NULL, NULL, "Error: PerformanceTraceLog-Timeout value (%s) is not numeric.", fields[1]);
             SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012159_ID, CAV_ERR_1012159_MSG, "PerformanceTraceLog-Timeout", fields[1]);
           }
           else
           {
             perf_timeout = atoi(fields[1]);
             if(perf_timeout < 10000 || perf_timeout > 120000)
             {
               NSDL2_PARSING(NULL, NULL, "Error: Timeout value (%d) should be between 10000 msec and 120000 msec.", perf_timeout);
               SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012161_ID, CAV_ERR_1012161_MSG, perf_timeout);
             }
             requests[url_idx].proto.http.rbu_param.performance_trace_timeout = perf_timeout;
           }
         case 1:
           if((ns_is_numeric(fields[0]) == 0) || (match_pattern(fields[0], "^[0-1]$") == 0))
           {
              NSDL2_PARSING(NULL, NULL, "Error: PerformanceTraceLog-Mode value (%s) is not valid.", fields[0]);
              SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012159_ID, CAV_ERR_1012159_MSG , "PerformanceTraceLog-Mode", fields[0]);
           }
           else
             requests[url_idx].proto.http.rbu_param.performance_trace_mode = atoi(fields[0]);
           break; 
         default:
           NSDL2_PARSING(NULL, NULL, "Error: Number of arguments provided with 'PerformanceTraceLog' is incorrect.");
           SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012162_ID, CAV_ERR_1012162_MSG, "PerformanceTraceLog");
       }
       g_rbu_create_performance_trace_dir = PERFORMANCE_TRACE_DIR_FLAG; 
       break;

    case RBU_PARAM_AUTH_CREDENTIAL: /* AuthCredential */ 
      //check value contain colon or not
      num_fields = get_tokens_ex2(val, fields, ":", 2);
      if(num_fields != 2)
      {
        NSDL2_PARSING(NULL, NULL, "Error: Format for AuthCredential is not valid.\n Format:\n \"AuthCredential=<username>:<password>\"", val);
        return NS_PARSE_SCRIPT_ERROR;
      } 

      if((requests[url_idx].proto.http.rbu_param.auth_username = copy_into_big_buf(fields[0], 0)) == -1)
      {
         NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", fields[0]);
         return NS_PARSE_SCRIPT_ERROR;
      }
      if((requests[url_idx].proto.http.rbu_param.auth_password = copy_into_big_buf(fields[1], 0)) == -1)
      {
         NSDL2_PARSING(NULL, NULL, "Error: failed copying data '%s' into big buffer", fields[1]);
         return NS_PARSE_SCRIPT_ERROR;
      }
      break;

    case RBU_PARAM_PHASE_INTERVAL: /* PhaseInterval */ 
      requests[url_idx].proto.http.rbu_param.phase_interval_for_page_load = atoi(val);
      if(requests[url_idx].proto.http.rbu_param.phase_interval_for_page_load < 2000)
      {
        NSDL2_PARSING(NULL, NULL, "Error: RBU_PARAM_PHASE_INTERVAL value is less than 2000ms");
        return NS_PARSE_SCRIPT_ERROR;
      }
      break;
    default:
      NSDL2_PARSING(NULL, NULL, "Error: RBU Params  value could not be parsed from flowfile %s", flow_file?flow_file:"NULL");
      return NS_PARSE_SCRIPT_ERROR;
  }

  /*NSDL2_PARSING(NULL, NULL, "RBU Dump - url_idx = %d, browser_user_profile = [%s], har_log_dir = [%s], vnc_display_id = [%s]", 
                             url_idx, requests[url_idx].proto.http.rbu_param.browser_user_profile, 
                             requests[url_idx].proto.http.rbu_param.har_log_dir, requests[url_idx].proto.http.rbu_param.vnc_display_id);*/

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int parse_multipart_body(FILE *flow_fp, char *flow_file, char **starting_quotes,
                                char **closing_quotes, int url_idx, int sess_idx, char *header_buf, FILE *outfp)
{
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  int  ret;
  int  next_boundary_flag = 1;
  int body_begin_flag = 0; //flag to check if body start from BODY_BEGIN

  NSDL2_PARSING(NULL, NULL, "Method Called starting_quotes=%s closing=%s ", *starting_quotes, *closing_quotes);
  //check if we had a single part body before this - we cant have this
  if (read_till_start_of_next_quotes(flow_fp, flow_file, *closing_quotes,  starting_quotes, 0, outfp) == NS_PARSE_SCRIPT_ERROR) {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012506_MSG, CAV_ERR_1012506_MSG);
  }

  while(1)
  {
    if(get_next_argument(flow_fp, *starting_quotes, attribute_name, attribute_value, closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    if(!strncasecmp(attribute_name, "ITEMDATA,", 9))
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012156_ID, CAV_ERR_1012156_MSG, "ITEMDATA");

    if (strcasecmp(attribute_name, "MULTIPART_BOUNDARY") == 0) {
      /* the first boundary goes into index multipart_bdry_index =0 because the index  starts at 0 and the boundary is extracted before the 
       * first BEGIN, from 
       *  the main header. This boundary applies to the first BEGIN/END block which follows. But we increment   multipart_bdry_index  at BEGIN. 
       *  So,  we must copy boundary value from multipart_bdry_index -1. The index from which to copy the boundary is always one behind
       */
      if(multipart_boundaries[multipart_bdry_index-1] == NULL) { // If boundary in content type is not saved
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012164_ID, CAV_ERR_1012164_MSG);
      }

      NSDL2_PARSING(NULL, NULL,"found %s in MULTIPART_BOUNDARY at script_line %d multipart_bdry_index %d  multipart_body_parts_count (before inc) %d", attribute_name, script_ln_no, multipart_bdry_index, multipart_body_parts_count);
      multipart_body =1;
      multipart_body_parts_count++;   //running index over all body parts - even nested

      /*
       * for the very first boundary after the main header, we already have 2 \r\n's so skip \r\n for boundary 
       * (multipart_body_parts_count is incremented here when BOUNDARY is seen, so value is atleast 1)
       */
      if (multipart_body_parts_count > 1)
        copy_to_multipart_body_post_buf("\xd\xa", 2, multipart_body_parts_count-1, 0);
      
      copy_to_multipart_body_post_buf(multipart_boundaries[multipart_bdry_index-1], strlen(multipart_boundaries[multipart_bdry_index-1]), multipart_body_parts_count-1, 0); 
      copy_to_multipart_body_post_buf("\xd\xa", 2, multipart_body_parts_count-1, 0);    //CRLF at the end of boundary
      next_boundary_flag = 0;
    } else if (!next_boundary_flag && strcasecmp(attribute_name, "HEADER") == 0)
        {
      NSDL2_PARSING(NULL, NULL, "Header in MULTIPART_BODY %s found at script_line %d\n", attribute_value, script_ln_no);
      if(parse_multipart_hdr(attribute_value, header_buf, sess_idx, flow_file) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      } else if (!next_boundary_flag && strcasecmp(attribute_name, "MULTIPART_BODY_BEGIN") == 0) {
      NSDL2_PARSING(NULL, NULL,"found %s at script_line %d multipart_bdry_index (before incr)%d", attribute_name, script_ln_no, multipart_bdry_index );
      multipart_body_begin =1;
      //multipart_bdry_index++;
    }  else if (strcasecmp(attribute_name, "MULTIPART_BODY_END") == 0) {
      /* check comments in MULTIPART_BOUNDARY above for explanation of multipart_bdry_index -1
      */
      NSDL2_PARSING(NULL, NULL,"found %s at script_line %d multipart_bdry_index %d", attribute_name, script_ln_no, multipart_bdry_index);
     
      /* This check is added to fix the issued if script have      
        MULTIPART_BODY_BEGIN,
        MULTIPART_BODY_END,
        wihtout any body and MULTIPART_BOUNDRY tag between. This issue is coming while recording toysrs script
      */
      if(multipart_body_parts_count == 0){
        NSDL2_PARSING(NULL, NULL, "Warning MULTIPART_BODY_END end found without MULTIPART_BOUNDRY tag, Ignoring it"); 
        NS_DUMP_WARNING("MULTIPART_BODY_END found without MULTIPART_BOUNDRY tag. So, ignoring MULTIPART_BODY_END"); 
        **starting_quotes = '\0';
        ret = read_till_start_of_next_quotes(flow_fp, flow_file, *closing_quotes,  starting_quotes, 0, outfp);
        multipart_bdry_index--;
        if(ret == NS_PARSE_SCRIPT_ERROR)
        {
          if(*starting_quotes == NULL)
            return NS_PARSE_SCRIPT_ERROR;
          else {
            NSDL2_PARSING(NULL, NULL, "Done processing all MULTIPART_BODY sections. exiting loop"); 
            break;
          }
        }
      }

      //add current boundary to the post buffer. ending boundary is --<boundary>-- on a new line
      copy_to_multipart_body_post_buf("\xd\xa", 2, multipart_body_parts_count-1, 0);
      copy_to_multipart_body_post_buf(multipart_boundaries[multipart_bdry_index-1], strlen(multipart_boundaries[multipart_bdry_index-1]), multipart_body_parts_count-1, 0); 

     //extra CRLF only at the last END
    // if (multipart_bdry_index == 1)
       copy_to_multipart_body_post_buf("--\xd\xa", 4, multipart_body_parts_count-1, 0);
    // else
      // copy_to_multipart_body_post_buf("--", 2, multipart_body_parts_count-1, 0);
         
      multipart_bdry_index--;
      if (multipart_bdry_index == -1) {
        NSDL2_PARSING(NULL, NULL, "Last %s found at script_line %d. should exit loop now\n", attribute_name, script_ln_no);
        break;
      }
    } else if (!next_boundary_flag && (((strcasecmp(attribute_name, "BODY") == 0) || (strcasecmp(attribute_name, "BODY_BEGIN") == 0)))) {
      NSDL2_PARSING(NULL, NULL,"found %s in MULTIPART_BODY at script_line %d multipart_bdry_index %d", attribute_name, script_ln_no, multipart_bdry_index);
      // Set flag for BODY_BEGIN
      if(strcasecmp(attribute_name, "BODY_BEGIN") == 0)
        body_begin_flag = 1;
      else
        body_begin_flag = 0;
      if(extract_multipart_body(flow_fp, script_line, closing_quotes, flow_file, sess_idx, starting_quotes, body_begin_flag, outfp) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      next_boundary_flag = 1;  
      continue;
    } else if((strcasecmp(attribute_name, "INLINE_URLS") == 0)){  // If we found start of inline url then return it to parse inline url 
      NSDL2_PARSING(NULL, NULL,"INLINE_URLS found after multipart end");
      return NS_PARSE_SCRIPT_SUCCESS;
    }else {
      if(next_boundary_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012165_ID, CAV_ERR_1012165_MSG);
      }
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012507_ID, CAV_ERR_1012507_MSG, attribute_name);
    }
    **starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, *closing_quotes,  starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=%s Starting quotes=%p Closing_quotes=%s Closing_quotes=%p   \
              after read_till_start_of_next_quotes()", *starting_quotes, *starting_quotes, *closing_quotes, *closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(*starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else {
        NSDL2_PARSING(NULL, NULL, "Done processing all MULTIPART_BODY sections. exiting loop"); 
        break;
      }
    }
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_inline_tx( char *attribute_name, char *attribute_value, int url_idx, int sess_idx)
{
  int tx_idx, start_tx, end_tx;
  char *attribute_value_ptr = attribute_value;
  char *tmp_ptr;
  char tx_name[512];
      
  NSDL2_PARSING(NULL, NULL, "Method Called");
  NSDL2_PARSING(NULL, NULL, "tx_name= [%s]", attribute_value_ptr);
  if(!attribute_value_ptr)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value_ptr, CAV_ERR_1012167_ID, CAV_ERR_1012167_MSG);
  }

  CLEAR_WHITE_SPACE(attribute_value_ptr);
  NSDL2_PARSING(NULL, NULL, "tx_name= [%s]", attribute_value_ptr);
    
  // Incase of Inline repeat urls TxName can also be given with postfix range like InLineTx[1-10]. These postfix will be used in case of
  // Repeat in inline url and will be used with _ (underscore). For example The transaction for 2nd url hit will be InLineTx_2.
  // In Following code we are parsing the postfix given with transaction name. Here:  
  // 1: Extract prefix 'InlineTx' and save in big buf
  // 2: Extract start_tx (like 1)
  // 3: Extract end_tx (like 10) 
  // 4: ADD InLineTx1, InLineTx2,...InLineTx10 in Hash Table 
  // Note: If repeat is more than postfix range then transaction for repeats number that is out of postfix range will not be executed 
  if((tmp_ptr = strchr(attribute_value_ptr, '[')))
  {
    // Extract prefix and copy in big buf
    *tmp_ptr = '\0';
    requests[url_idx].proto.http.tx_prefix = copy_into_big_buf(attribute_value_ptr, 0);

    // Extract start_tx 
    tmp_ptr++;
    CLEAR_WHITE_SPACE(tmp_ptr);
    if(tmp_ptr[0] == '-')
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012168_ID, CAV_ERR_1012168_MSG);
    }

    attribute_value_ptr = strchr(tmp_ptr, '-');
    if(attribute_value_ptr) {
      *attribute_value_ptr = '\0';
       if(ns_is_numeric(tmp_ptr) == 0)
       {
         SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012168_ID, CAV_ERR_1012168_MSG);
       }

       start_tx = atoi(tmp_ptr);   
          
       //Exract end_tx
       attribute_value_ptr++;
       CLEAR_WHITE_SPACE(attribute_value_ptr);
       tmp_ptr = strchr(attribute_value_ptr, ']');
       if(tmp_ptr) {
        *tmp_ptr = '\0';
        if(ns_is_numeric(attribute_value_ptr) == 0)
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012168_ID, CAV_ERR_1012168_MSG);
        }

        end_tx = atoi(attribute_value_ptr);
        } else {
            SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012508_ID, CAV_ERR_1012508_MSG);
          }         
        } else {
          SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012508_ID, CAV_ERR_1012508_MSG);
        }
   
        NSDL2_PARSING(NULL, NULL, "tx_prefix = [%s], start_tx = [%d], end_tx = [%d]",
                                   RETRIEVE_BUFFER_DATA(requests[url_idx].proto.http.tx_prefix), 
                                   start_tx, end_tx);
        if(start_tx >= end_tx) {
          SCRIPT_PARSE_ERROR_EXIT_EX(attribute_value, CAV_ERR_1012508_ID, CAV_ERR_1012508_MSG);
        }
        
        // Add transaction in hash table for the given range with prefix, these will be executed while hitting repeat url 
        while(start_tx <= end_tx)
        {
          sprintf(tx_name, "%s_%d", RETRIEVE_BUFFER_DATA(requests[url_idx].proto.http.tx_prefix), start_tx);
          NSDL3_PARSING(NULL, NULL, "Adding Tx [%s] in Hash table", tx_name);
          add_trans_name(tx_name, sess_idx);
          start_tx++; 
        }  
      } 
      else 
      { 
        tx_idx = add_trans_name(attribute_value_ptr, sess_idx);
        requests[url_idx].proto.http.tx_idx = tx_idx;
        requests[url_idx].proto.http.tx_prefix = -1;
      }
      return NS_PARSE_SCRIPT_SUCCESS;
}

//Parse ns_web_url API
//script_line will point to the starting quotes of the first argument
// Input - embedded_urls should be 0 for Main and 1 for embedded URL
// It will set it to 1 if we are called for Main and inline urls are found
static int parse_url_parameters(FILE *flow_fp, char *flow_file, char *starting_quotes,
                                int sess_idx, char *embedded_urls, char inline_enabled, char *api_name, FILE *outfp)
{
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char header_buf[MAX_REQUEST_BUF_LENGTH];
  int  ret;
  char *closing_quotes;
  char url_exists = 0;
  int url_idx;
  char is_redirect = 0; // set if redirect=yes is found
  // Need to keep copy as attribute_value gets overriten
  char location[MAX_LINE_LENGTH] = ""; // copy location value  if any
  char url[MAX_LINE_LENGTH] = ""; // Copy URL
  #ifndef CAV_MAIN
  static int cur_page_index = -1; //For keeping track of multiple main urls
  #else
  static __thread int cur_page_index = -1; //For keeping track of multiple main urls;
  #endif
  int singlepart_body =0;
  int body_begin_flag = 0; //flag to check if body start from BODY_BEGIN
  int url_count = 0 ; //for keeping track of url
  char *ptr1 = NULL; 
  int num_tok = 0;
  char *fields[100];

  NSDL2_PARSING(NULL, NULL, "Method Called starting_quotes=%p starting_quotes=%s, inline_enabled = %d", 
                             starting_quotes, starting_quotes, inline_enabled);
  header_buf[0] = '\0';

  if(init_url(&url_idx, *embedded_urls, flow_file, inline_enabled) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //starting_quotes will point to the starting of the argument quotes
  //In case ret is NS_PARSE_SCRIPT_SUCCESS, continue reading next argument
  //else if starting quotes is NULL, return
  //else check for INLINE_URLS, or ); or END_INLINE
  if(!*embedded_urls) // Main URL
  {
    NSDL2_PARSING(NULL, NULL, "Parsing Parameters for Main URL");
    closing_quotes = starting_quotes;
    starting_quotes = NULL;
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, *embedded_urls, outfp);
    NSDL2_PARSING(NULL, NULL, "starting_quotes = %s", starting_quotes);
   
    //Bug:-67138 - Core|Compilation error is not coming when there is space between "=" and "http" in the ns_web_url() and inline urls. 
    //Verify whether space is coming or not in URL=http://127.0.0.1/tours/index.html
    if(((ptr1=strcasestr(starting_quotes, "URL")) != NULL)) 
    {
      NSDL2_PARSING(NULL, NULL, "ptr1 = %s", ptr1);
      ptr1 = ptr1 + 3;
      if(*ptr1 != '=')
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012169_ID, CAV_ERR_1012169_MSG, "URL", "=");
      }
      else
      {
        NSDL2_PARSING(NULL, NULL, "ptr1 = %s", ptr1);
        ptr1++;
        if(*ptr1 == ' ')
        {
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012169_ID, CAV_ERR_1012169_MSG, "=", "scheme");
        }
      }
    }
 
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else  //Starting quotes not found and some text found
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012160_ID, CAV_ERR_1012160_MSG);
    }
  }
  //Embedded line will not have "Pagename" 
  else
  {
    NSDL2_PARSING(NULL, NULL, "Parsing Parameters for Embedded URL");
  }

  // line_ptr will point to the starting of the argument quotes
  while(1)
  {
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if (strcasecmp(attribute_name, "URL") == 0)
    {
      url_count++;
      NSDL2_PARSING(NULL, NULL, "URL [%s] found at script_line %d", attribute_value, script_ln_no);
      NSDL2_PARSING(NULL, NULL, "URL is [%s] & url_count is [%d]", attribute_value, url_count);
      NSDL2_PARSING(NULL, NULL, "cur_page_index=%d, g_cur_page=%d", cur_page_index, g_cur_page);
      #ifndef CAV_MAIN
      if(cur_page_index != -1)
      #else
      if(g_cur_page != 0)
      #endif
      {
        if(cur_page_index == g_cur_page && !*embedded_urls)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012170_ID, CAV_ERR_1012170_MSG);
      }
      cur_page_index = g_cur_page;
 
      if(set_url(attribute_value, flow_file, sess_idx, url_idx, *embedded_urls, inline_enabled, api_name) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      url_exists = 1;
      strcpy(url, attribute_value);
    }
    else if(strcasecmp(attribute_name, "TxName") == 0)
    {
      if(set_inline_tx(attribute_name, attribute_value, url_idx, sess_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Method
    else if (strcasecmp(attribute_name, "METHOD") == 0)
    {
      //Default METHOD for GRPC will be set POST if method will not be set in request   
      NSDL2_PARSING(NULL, NULL, "Method [%s] found at script_line %d\n", attribute_value, script_ln_no);
      if(set_http_method(attribute_value, flow_file, url_idx, script_ln_no) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Version
    else if (strcasecmp(attribute_name, "VERSION") == 0)
    {
      //Only for HTTP 1.0/1.1
      NSDL2_PARSING(NULL, NULL, "Version [%s] found at script_line %d\n", attribute_value, script_ln_no);
      if(set_version(attribute_value, flow_file, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Header
    else if (strcasecmp(attribute_name, "HEADER") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Header %s found at script_line %d\n", attribute_value, script_ln_no);
      if(set_headers(flow_fp, flow_file, attribute_value, header_buf, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Cookie
    else if (strcasecmp(attribute_name, "COOKIE") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Cookie found at script_line %d\n", script_ln_no);
      set_cookie(attribute_value, flow_file, url_idx, sess_idx);
    }
    //Extract Compression
    else if (strcasecmp(attribute_name, "COMPRESSION") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Compression found at script_line %d\n", script_ln_no);
      set_compression(attribute_value, flow_file, url_idx);
    }
    //Extract PreUrlCallback
    else if (strcasecmp(attribute_name, "PREURLCALLBACK") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "PreUrlCallBack found at script_line %d\n", script_ln_no);
      //check if script type is java then set error
      if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012171_ID, CAV_ERR_1012171_MSG, "PreUrl");
      }
      if(set_pre_url_callback(attribute_value, flow_file, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract PostUrlCallback
    else if (strncasecmp(attribute_name, "POSTURLCALLBACK", 15) == 0)
    {
      NSDL2_PARSING(NULL, NULL, "PostUrlCallBack found at script_line %d attribute name = %s \n", script_ln_no, attribute_name);
      //check if script type is java then set error
      int i;
 
      ptr1 =  attribute_name;
      num_tok = get_tokens(ptr1 , fields, ":" , MAX_REDIRECTION_DEPTH_LIMIT +1);
      NSDL2_PARSING(NULL, NULL, "Number of tokens = %d ptr1 = %s attribute_name = %s \n", num_tok , ptr1, attribute_name);
      
      int local_rdepth = 0;
      if (num_tok >1)
      {
        for(i = 1; i < num_tok; i++)
        {
          local_rdepth = atoi(fields[i]);
          if(check_redirection_limit(local_rdepth))
            return -1;
          requests[url_idx].postcallback_rdepth_bitmask |= (1 << (local_rdepth - 1 ));
          NSDL2_PARSING(NULL, NULL, "requests[url_idx].postcallback_rdepth_bitmask = %04x \n", requests[url_idx].postcallback_rdepth_bitmask);
        }
      }
      else {
        requests[url_idx].postcallback_rdepth_bitmask = 0xFFFFFFFF;
        NSDL2_PARSING(NULL, NULL, "requests[url_idx].postcallback_rdepth_bitmask = %04x \n", requests[url_idx].postcallback_rdepth_bitmask);
      }

      NSDL2_PARSING(NULL, NULL, "gSessionTable[sess_idx].script_type = %d \n", gSessionTable[sess_idx].script_type);
      
      if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_JAVA){
         SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012171_ID, CAV_ERR_1012171_MSG, "PostUrl");
      }
      if(set_post_url_callback(attribute_value, flow_file, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    // Extract Redirect for redirection
    // REDIRECT and LOCATION can come in any order
    else if (strcasecmp(attribute_name, "REDIRECT") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Redirect found at script_line %d with value = %s\n", script_ln_no, attribute_value);
      if(strcasecmp(attribute_value, "YES") == 0)
        is_redirect = 1;
    }
    // Extract Location     //for redirection
    else if (strcasecmp(attribute_name, "LOCATION") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Location found at script_line %d\n", script_ln_no);
      strcpy(location, attribute_value);
    }
    // Extract FullyQualifiedURL, its value will be either yes or no 
    else if( strcasecmp(attribute_name, "FullyQualifiedURL") == 0) 
    {
      NSDL2_PARSING(NULL, NULL, "FullyQualifiedURL found at script_line %d attribute_value is %s\n", script_ln_no, attribute_value);
      if(set_fully_qualified_url(attribute_value, flow_file, url_idx, script_ln_no) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Refferer
    else if (strcasecmp(attribute_name, "REFFERER") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Refferer found at script_line %d\n", script_ln_no);
      //set_refferer(attribute_value);
    }
    //Extract RecRespSize
    else if (strcasecmp(attribute_name, "RECRESPSIZE") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "RecRespSize found at script_line %d\n", script_ln_no);
      //set_rec_resp_size(attribute_value);
    }
    //Extract Snapshot
    else if (strcasecmp(attribute_name, "SNAPSHOT") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Snapshot found at script_line %d\n", script_ln_no);
      //set_snapshot(attribute_value);
    }

    else if (strcasecmp(attribute_name, "PRESNAPSHOT") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "PreSnapshot found at script_line %d\n", script_ln_no);
    }
     //Extract HTTP Auth Username
    else if (strcasecmp(attribute_name, "HTTPAuthUserName") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "HTTP Auth Username found at script_line %d\n", script_ln_no);
      if (set_auth_ntlm_uname_pwd(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, 0) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract HTTP Auth Password
    else if (strcasecmp(attribute_name, "HTTPAuthPassword") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "HTTP Auth Password found at script_line %d\n", script_ln_no);
      if (set_auth_ntlm_uname_pwd(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, 1) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    //Extract Repeat value for Inline URL 
    else if (strcasecmp(attribute_name, "Repeat") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Repeat found at script_line %d\n", script_ln_no);
      if (set_repeat_inline_val(attribute_value, flow_file, url_idx, sess_idx, script_ln_no) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      gPageTable[cur_page_index].flags |= PAGE_WITH_INLINE_REPEAT; 
    }

    else if (strcasecmp(attribute_name, "BrowserUserProfile") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "BrowserUserProfile at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_BROWSER_USER_PROFILE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "HarLogDir") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "HarLogDir at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_HAR_LOG_DIR) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "VncDisplayId") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "VncDisplayId at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_VNC_DISPLAY_ID) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "HarRenameFlag") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "HarRenameFlag at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_HAR_RENAME_FLAG) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "PageLoadWaitTime") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "PageLoadWaitTime at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PAGE_LOAD_WAIT_TIME) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "MergeHarFiles") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "MergeHarFiles at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_MERGE_HAR_FILES) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }   
    else if (strcasecmp(attribute_name, "ScrollPageX") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "ScrollPageX at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_SCROLL_PAGE_X) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "ScrollPageY") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "ScrollPageY at script_line %d\n", script_ln_no);
      if (set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_SCROLL_PAGE_Y) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "PrimaryContentProfile") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "PrimaryContentProfile at script_line %d\n", script_ln_no);
      if(set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PRIMARY_CONTENT_PROFILE) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "WaitForNextReq") == 0)
    { 
      NSDL2_PARSING(NULL, NULL, "WaitForNextRequest at script_line %d\n", script_ln_no);
      if(set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_WAIT_FOR_NEXT_REQ) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "PhaseInterval") == 0)
    { 
      NSDL2_PARSING(NULL, NULL, "PhaseInterval at script_line %d\n", script_ln_no);
      if(set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PHASE_INTERVAL) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }

    else if (strcasecmp(attribute_name, "PerformanceTraceLog") == 0)
    { 
      NSDL2_PARSING(NULL, NULL, "PerformanceTraceLog at script_line %d\n", script_ln_no);
      if(set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_PERFORMANCE_TRACE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "AuthCredential") == 0)
    { 
      NSDL2_PARSING(NULL, NULL, "AuthCredential at script_line %d\n", script_ln_no);
      if(set_rbu_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no, RBU_PARAM_AUTH_CREDENTIAL) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if (strcasecmp(attribute_name, "BODY_ENCRYPTION") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "BODY_ENCRYPTION at script_line %d\n", script_ln_no);
      if(set_body_encryption_param(attribute_value, flow_file, url_idx, sess_idx, script_ln_no) == NS_PARSE_SCRIPT_ERROR)
      {
        fprintf(stderr, "Usage: BODY_ENCRYPTION=<encryption_algo>,<base64_encode_option>,<key>,<ivec>.\n");
        return NS_PARSE_SCRIPT_ERROR;
      }
    }


    // Extract Body
    // Bosy can be present in two formats
    // First: "BODY=<singleLine/MultiLineBody>"
    //All protbuf related field come into this section
    else if((strcasecmp(attribute_name, "ReqProtoFile") == 0))
    {
      NSDL2_PARSING(NULL, NULL, "ReqProtoFile at script_line %d\n", script_ln_no);
      if(ns_protobuf_parse_urlAttr(attribute_name, attribute_value, flow_file, url_idx, sess_idx, script_ln_no, NS_PROTBUF_REQ_FILE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((strcasecmp(attribute_name, "RespProtoFile") == 0))
    {
      NSDL2_PARSING(NULL, NULL, "RespProtoFile at script_line %d\n", script_ln_no);
      if(ns_protobuf_parse_urlAttr(attribute_name, attribute_value, flow_file, url_idx, sess_idx, script_ln_no, NS_PROTBUF_RESP_FILE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((strcasecmp(attribute_name, "ReqProtoMessageType") == 0))
    {
      NSDL2_PARSING(NULL, NULL, "RespProtoFile at script_line %d\n", script_ln_no);
      if(ns_protobuf_parse_urlAttr(attribute_name, attribute_value, flow_file, url_idx, sess_idx, script_ln_no, NS_PROTOBUF_REQ_MESSAGE_TYPE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if((strcasecmp(attribute_name, "RespProtoMessageType") == 0))
    {
      NSDL2_PARSING(NULL, NULL, "RespProtoFile at script_line %d\n", script_ln_no);
      if(ns_protobuf_parse_urlAttr(attribute_name, attribute_value, flow_file, url_idx, sess_idx, script_ln_no, NS_PROTOBUF_RESP_MESSAGE_TYPE) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    // Second: BODY_BEGIN,
    // "<singleLine/MultiLineBody>"
    // BODY_END, 
    else if ((strcasecmp(attribute_name, "BODY") == 0) || (strcasecmp(attribute_name, "BODY_BEGIN") == 0))
    {
      if((strcasecmp(attribute_name, "BODY_BEGIN") == 0))
        body_begin_flag = 1;
      singlepart_body =1;
      // Body if present must be the last argument
      // Body termination is checked for termination of 
      // INLINE_URLS or ); in case of MAIN_URL and
      // END_INLINE or ); for EMBEDDED_URL
      if(requests[url_idx].proto.http.http_method  == HTTP_METHOD_GET) {
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__,(char *) __FUNCTION__,
                                            "Warning: Post body is given with GET method for page '%s'.",
                                            RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
        NS_DUMP_WARNING("Post body is provided with GET method for page '%s'. You should use POST method, if BODY has sensitive data",
                             RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
      }
 
      if(extract_body(flow_fp, script_line, &closing_quotes, *embedded_urls, flow_file, body_begin_flag, outfp) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      if(set_body(url_idx, sess_idx, starting_quotes, closing_quotes, flow_file) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      CLEAR_WHITE_SPACE(script_line);
      starting_quotes = script_line;
      break;
    }
    // Multipart change start
    else if (strncasecmp(attribute_name, "MULTIPART_BODY_BEGIN", strlen("MULTIPART_BODY_BEGIN")) == 0) {
      // if we already had a multipart body, we cant have another BODY outside that
      if (singlepart_body) {
        NSDL2_PARSING(NULL, NULL, "BODY was set before MULTIPART_BODY- for multipart body, all body parts have to be inside MULTIPART_BODY", script_ln_no);
        return NS_PARSE_SCRIPT_ERROR;
      }
      //check if the main header had boundary set before 
      NSDL2_PARSING(NULL, NULL,"req header flags 0x%x", requests[url_idx].proto.http.header_flags);
      if (! (requests[url_idx].proto.http.header_flags & NS_MULTIPART) ) {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012172_ID, CAV_ERR_1012172_MSG);
      }
      multipart_body_begin =1;
      multipart_bdry_index++;
      if (parse_multipart_body(flow_fp, flow_file, &starting_quotes, &closing_quotes, url_idx, sess_idx, header_buf, outfp) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      CLEAR_WHITE_SPACE(script_line);
      // comment this line as in case of inline we need to point starting_quotes to start of inline we found
      //starting_quotes = script_line; 
      break;
    }
    // Multipart change end 

    //ITEMDATA processing
    else if(strncasecmp(attribute_name, "ITEMDATA", 8) == 0)
    {
      if(parse_encode_set_bodydata(url_idx, sess_idx, flow_fp, script_line, &closing_quotes, *embedded_urls, flow_file, &starting_quotes, outfp) == NS_PARSE_SCRIPT_ERROR)
      {
        NSDL2_PARSING(NULL, NULL, "parse_encode_set_bodydata() returned error");
        return NS_PARSE_SCRIPT_ERROR;
      }
    }
    //

    else
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012509_ID, CAV_ERR_1012509_MSG, attribute_name);
    }

    //closing_quotes is an input argument - will point to next character after closing quotes of last argument
    //starting_quotes will point to the starting of the argument quotes
    //In case ret is NS_PARSE_SCRIPT_SUCCESS, continue reading next argument
    //else if starting quotes is NULL, return
    //else check for INLINE_URLS, or );
    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes,  &starting_quotes, *embedded_urls, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=%s, Starting quotes=%p, Closing_quotes=%s, Closing_quotes=%p, after read_till_start_of_next_quotes()", 
                  starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current URL parameters are processed, so break
        break;
    }
  }

  if(!url_exists)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012173_ID, CAV_ERR_1012173_MSG);
  
  //TODO: optimize all all_header_buf to check max length
  if(GRPC)
  {
    if (!(CHECK_HDR_FLAG(NS_REQ_HDR_FLAGS_CONTENT_TYPE)))    
    {
      strcat(header_buf, "content-type:application/grpc\r\n");
    }
    if (!(CHECK_HDR_FLAG(NS_REQ_HDR_FLAGS_USER_AGENT)))
    {
      strcat(header_buf, "user-agent:grpc-c++/1.30.0 grpc-c/10.0.0 (linux, chttp2)\r\n");
    }
    if (!(CHECK_HDR_FLAG(NS_REQ_HDR_FLAGS_TE)))    
    {
      strcat(header_buf, "te:trailers\r\n");
    }
    if (!(CHECK_HDR_FLAG(NS_REQ_HDR_FLAGS_GRPC_ACCEPT_ENCODING)))    
    {
      strcat(header_buf, "grpc-accept-encoding:gzip\r\n");
    }
  }
  
  if((global_settings->whitelist_hdr) && (global_settings->whitelist_hdr->mode) &&
     (strcasecmp(global_settings->whitelist_hdr->name, "User-Agent")) &&
     !(requests[url_idx].flags & NS_REQ_FLAGS_WHITELIST))
  {
    strcat(header_buf, global_settings->whitelist_hdr->name);
    strcat(header_buf, ":");
    strcat(header_buf, global_settings->whitelist_hdr->value);
    strcat(header_buf, "\r\n");
  }
  //strcat(header_buf, "\r\n");
  NSDL2_PARSING(NULL, NULL, "Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.http.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_file);
  NSDL2_PARSING(NULL, NULL, "After: segmenting Headers, url_idx = %d, seg_start = %d, num_entries = %d", 
                             url_idx, requests[url_idx].proto.http.hdrs.seg_start, requests[url_idx].proto.http.hdrs.num_entries);

  override_url_parameters(url_idx, *embedded_urls, flow_file);

  //TODO: #Deepika: need to Re-think again
  if(!requests[url_idx].is_url_parameterized) {
    if(set_redirect_url(url, is_redirect, location, flow_file, url_idx) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }
  
  // For multipart body copy all the data into g_post_buf 
  if (multipart_body) {    //copy everything into the main buffer
    //sanity check
    if (multipart_bdry_index != 0) {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012174_ID, CAV_ERR_1012174_MSG);
    }
    int i, body_len;
    int rnum, body_line_no =0;
    for (i=0; i<multipart_body_parts_count; i++) {
      //copy body part
      body_len = multipart_body_parts[i].cur_buf_len; 

      cur_post_buf_len = body_len; // we need to set cur_post_buf_len as it is used in save_and_segment_body
      if(i == 0)
        save_and_segment_body(&rnum, url_idx, g_multipart_body_post_buf[i], noparam_array[i], &body_line_no, sess_idx, flow_file);
      else 
        save_and_segment_body_multipart(rnum, url_idx, g_multipart_body_post_buf[i], noparam_array[i], &body_line_no, sess_idx, flow_file);
      if (g_multipart_body_post_buf[i]) { //should always be true
        FREE_AND_MAKE_NULL(g_multipart_body_post_buf[i], "g_multipart_body_post_buf", i);
        //g_multipart_body_post_buf[i] = NULL;
      }
    }
    //reset all multipart vars, before returning
    bzero(multipart_body_parts, sizeof(multipartBodyParts)* multipart_body_parts_count);
    bzero(noparam_array, (sizeof(int) * MAX_MULTIPART_BODIES));
    multipart_body = multipart_bdry_index = multipart_body_parts_count = multipart_body_begin =0;
    FREE_AND_MAKE_NOT_NULL(multipart_boundaries, "multipart_boundaries", -1);
    //return NS_PARSE_SCRIPT_SUCCESS;
  }


  if(requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file == -1)
  {
     requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file  = requests[url_idx].proto.http.protobuf_urlattr.req_pb_file; 
  } 

  if(requests[url_idx].proto.http.protobuf_urlattr.resp_pb_msg_type  == -1)
  {
    requests[url_idx].proto.http.protobuf_urlattr.resp_message = requests[url_idx].proto.http.protobuf_urlattr.req_message; 	
  }

  /*Bug 66956: Response proto object is created from save_segment_body code lag.
    It will not be created, if only response proto and MessageType will be given in script. */
  if((requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file != -1) && (requests[url_idx].proto.http.protobuf_urlattr.resp_pb_msg_type != -1))
  {
    char script_path[NS_PB_MAX_PARAM_LEN + 1];
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/ 
    snprintf(script_path, NS_PB_MAX_PARAM_LEN, "%s/%s", GET_NS_TA_DIR(), \
                 get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));

    NSDL2_PARSING(NULL, NULL, "script_path = %s", script_path);
    if(requests[url_idx].proto.http.protobuf_urlattr.resp_message == NULL)
      requests[url_idx].proto.http.protobuf_urlattr.resp_message = ns_create_protobuf_msgobj(RETRIEVE_BUFFER_DATA(requests[url_idx].proto.http.protobuf_urlattr.resp_pb_file), RETRIEVE_BUFFER_DATA(requests[url_idx].proto.http.protobuf_urlattr.resp_pb_msg_type), script_path);

    NSDL2_PARSING(NULL, NULL, "resp_message = %p", requests[url_idx].proto.http.protobuf_urlattr.resp_message);
  }

  NSDL2_PARSING(NULL, NULL, "Value received after parsing url arguments is [%s], embedded_urls = [%d] at Line [%d] Script File=[%s]", starting_quotes, *embedded_urls, script_ln_no, flow_file, starting_quotes);

  //Identifying INLINE_URLS received after MAIN_URL 
  if(((strncmp(starting_quotes, "INLINE_URLS", 11)) == 0) && (!*embedded_urls))
  {
    NSDL2_PARSING(NULL, NULL, "INLINE_URLS received at script_line %d in file %s",
                        script_ln_no, flow_file);
    *embedded_urls = 1;
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  //Identifying End of ns_web_url function in case of Main Url
  else if ((strncmp(starting_quotes, ");", 2) == 0) && (!*embedded_urls))
  {
    NSDL2_PARSING(NULL, NULL, "End of function found at script_line %d in file %s",
                        script_ln_no, flow_file);
    return NS_PARSE_SCRIPT_SUCCESS;
  }
    //Identifying End of embedded url 
  else if(((strncmp(starting_quotes, "END_INLINE", 10)) == 0) && (*embedded_urls) && url_count == 1)
  {
    NSDL2_PARSING(NULL, NULL, "END_INLINE received at script_line %d in file %s",
                        script_ln_no, flow_file);
    end_inline_url_count--;   
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  else if(((strncmp(starting_quotes, "END_INLINE", 10)) == 0) && (*embedded_urls))
  {
    NSDL2_PARSING(NULL, NULL, "END_INLINE received at script_line %d in file %s",
                        script_ln_no, flow_file);
   // end_inline_url_count--;
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012175_ID, CAV_ERR_1012175_MSG);
  }

  else if(strncasecmp(starting_quotes, "ITEMDATA_END", 12))
  {
    NSDL2_PARSING(NULL, NULL, "ITEMDATA_END received");
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  //Error cases includes END_INLINE received in MAIN_URL
  else
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012176_ID, CAV_ERR_1012176_MSG);
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

//This function is to parse comments after INLINE_URLS & start of first embedded_url
//Other comments in ns_web_url() api are handled in function read_till_start_of_next_quotes()
int parse_comments(FILE *flow_fp, char *line_ptr, char **starting_quotes, FILE *outfp)
{
  char *ptr;
  char multiline_comment = 0;

  NSDL3_PARSING(NULL, NULL, "Method Called line_ptr=%s starting_quotes = %s ", line_ptr, *starting_quotes);
  ptr = line_ptr;

  while(1)
  {
    if(*ptr == '\0') // End of line reached, so read next line
    {
      if(read_line_and_comment(flow_fp, outfp) == NULL)
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012180_ID, CAV_ERR_1012180_MSG);

      //Set ptr to the start of the new script_line when new line is read
      ptr = script_line;
      continue;
    }

    if(multiline_comment)
    {
      if(strncmp(ptr, "*/", 2) == 0)
      {
        NSDL2_PARSING(NULL, NULL, "End of multi-line comment");
        ptr = ptr + 2;
        multiline_comment = 0;
        continue;
      }
      ptr++;
    }
    else
    {
      //Handling starting of multi-line comments
      if(strncmp(ptr, "/*", 2) == 0)
      {
        NSDL2_PARSING(NULL, NULL, "Start of multi-line comment");
        ptr = ptr + 2;
        multiline_comment = 1;
        continue;
      }
      //Handling of single-line comments
      else if(strncmp(ptr, "//", 2) == 0)
      {
        NSDL2_PARSING(NULL, NULL, "Start of single-line comment");
        *ptr = '\0';
        continue;
      }
      //Ignore the white spaces and continue reading script_line
      else if(isspace(*ptr))
      {
        ptr++;
        continue;
      }
      else if(*ptr == '"') // Opening quotes
      {
        NSDL2_PARSING(NULL, NULL, "Starting quotes found. Start of argument begin");
        *starting_quotes = ptr;
        //break;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      else
      {
        *starting_quotes = ptr;
        return NS_PARSE_SCRIPT_ERROR;
      }
    }
  }
}


static int parse_embedded_urls(FILE *flow_fp, char *flow_file, int url_idx, int sess_idx, char inline_enabled, FILE *outfp)
{
  char *ptr;
  char embedded_urls = 1;
  //char first_embedded = 1;
  char *starting_quotes = NULL;

  NSDL2_PARSING(NULL, NULL, "Method Called, url_idx = %d, sess_idx = %d", url_idx, sess_idx);


  while(1)
  {
    NSDL2_PARSING(NULL, NULL, "Parsing Embedded Url %s at script_line %d in file %s",
                              script_line, script_ln_no, flow_file);
    if(read_line_and_comment(flow_fp, outfp) == NULL)
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012180_ID, CAV_ERR_1012180_MSG);
    
    starting_quotes = script_line;
    if(parse_comments(flow_fp, script_line, &starting_quotes, outfp) == NS_PARSE_SCRIPT_ERROR)
    {
      if((ptr = strstr(starting_quotes, ");")) != NULL)
      {
        NSDL2_PARSING(NULL, NULL, "Extiting Method - End of ns_web_url found at script_line %d in file %s", script_ln_no, flow_file);
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012109_ID, CAV_ERR_1012109_MSG);
    }
    end_inline_url_count++;
    NSDL2_PARSING(NULL, NULL, "end_inline_url_count = %d", end_inline_url_count);
    if(parse_url_parameters(flow_fp, flow_file, starting_quotes, sess_idx, &embedded_urls, inline_enabled, NULL, outfp) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    if (end_inline_url_count != 0)
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012178_ID, CAV_ERR_1012178_MSG);
    }
  }
}


//Parse ns_web_url API
//return NS_PARSE_SCRIPT_ERROR/NS_PARSE_SCRIPT_SUCCESS
int parse_web_url(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx, char inline_enabled)
{
  int  url_idx = 0;
  int ret;
  char *page_end_ptr = NULL;
  char embedded_urls = 0;
  char pagename[MAX_LINE_LENGTH + 1];
  //char pagename_local[MAX_LINE_LENGTH + 1];
  
  //API ns_grpc_client() will be process as ns_web_url, so .tmp/flow.c file contains ns_grpc_client() name as ns_web_url() 
  char *apiname = (inline_enabled == GRPC_CLIENT)?"ns_grpc_client":"ns_web_url"; 
  NSDL2_PARSING(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  //Extract PageName from script_line
  //Page name would always be the first mandatory argument in ns_web_url
  ret = extract_pagename(flow_fp, flow_file, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL4_PARSING(NULL, NULL, "pagename - [%s], page_end_ptr = [%s]", pagename, page_end_ptr);
  //Write function script_line read_till_start_of_next_quotes for checking ( and whitespaces
  if((parse_and_set_pagename(apiname, apiname, flow_fp, flow_file, script_line, outfp, flow_outfile, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  embedded_urls = 0; // Set to 0 for Main URL
  if(parse_url_parameters(flow_fp, flow_file, page_end_ptr, sess_idx, &embedded_urls, inline_enabled, "ns_web_url", outfp) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  if(embedded_urls) // At least one inline url is present
  {
    if(parse_embedded_urls(flow_fp, flow_file, url_idx, sess_idx, inline_enabled, outfp) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  // free any leftover redirect location from previous page
  free_leftover_red_location(flow_file);

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_jrmi_ex(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_file, char *flowout_file, int sess_idx)
{
   char pagename[MAX_LINE_LENGTH + 1];
   int ret;
   int  url_idx = 0;
   char *page_end_ptr = NULL;
   char embedded_urls = 0;
   char prev_line[MAX_LINE_LENGTH + 1] = "";

   NSDL2_JRMI(NULL, NULL, "Method Called. File: %s, api_to_run = %s", flow_filename, api_to_run);

  // In case of Jrmi script we modify script line .
  // Therefore we will extract if anything preceedes api_to_rum (ns_jrmi_sub_step_start in this case). So that it can be appended to script line     later. 

    page_end_ptr = strstr(script_line, api_to_run); 
    if (page_end_ptr == NULL){
      prev_line[0] = '\0';
    } 
    else {
      ret = page_end_ptr - script_line;
      strncpy(prev_line, script_line, ret); //Copying the return value first
      prev_line[ret] = '\0';
    }

  //Extract PageName from script_line
  //Page name would always be the first mandatory argument in ns_web_url
  ret = extract_pagename(flow_fp, flow_file, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  sprintf(script_line, "%s ns_web_url(\"%s\",\"URL=http://127.0.0.1/tours/index.html\");", prev_line, pagename);
  page_end_ptr = strchr(script_line, '"');
  page_end_ptr = strchr(++page_end_ptr, '"');

  NSDL4_PARSING(NULL, NULL, "pagename - [%s], page_end_ptr = [%s]", pagename, page_end_ptr);
  //Write function script_line read_till_start_of_next_quotes for checking ( and whitespaces
  if((parse_and_set_pagename("ns_web_url", api_to_run, flow_fp, flow_file, script_line, outfp, flowout_file, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  embedded_urls = 0; // Set to 0 for Main URL
  if(parse_url_parameters(flow_fp, flow_file, page_end_ptr, sess_idx, &embedded_urls, 0, NULL, outfp) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  if(embedded_urls) // At least one inline url is present
  {
    if(parse_embedded_urls(flow_fp, flow_file, url_idx, sess_idx, 0, outfp) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  // free any leftover redirect location from previous page
  free_leftover_red_location(flow_file);

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

