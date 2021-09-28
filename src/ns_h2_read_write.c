/*********************************************************************************************
* Name                   : ns_h2_read_write.c  
* Purpose                : This C file holds the function(s) required for reading /writing of http2 .
                           Also this procress frame  
* Author                 : Sanjana Joshi  
* Intial version date    : 6-October-2016 
* Last modification date : 19-December-2016
* Modification History 
* Author(s)              : Shalu Panwar / Sanjana Joshi
* Change Description     : changes Support for Partial read .


* Author                 : Anubhav

* Date                   : December 2017
* Change Description     : Handling of Server Push
* Date                   : April 2018
* Change Description     : Dumping Multiplexed response headers/ data frames
* Date                   : May 2018
* Change Description     : Handling of Flow Control
* Date                   : July 2018
* Change Description     : Handling of Response and Idle timeout

*********************************************************************************************/

#include "ns_h2_header_files.h"
#include "ns_gdf.h"
#include "ns_group_data.h"

/*
  Macro for Calling handle_connect() from read .  We need to call handle connect 

  i) In case of EAGAIN in read   
  ii) When read is complete ( cptr->proto_state = HTTP2_SETTING_DONE) 
*/
#define HTTP2_HANDLE_CONNECT(cptr, now) {\
 if ((cptr->http2_state == HTTP2_SETTINGS_ACK) && !(cptr->flags & NS_HTTP2_UPGRADE_DONE)){\
   NSDL2_HTTP2(NULL, NULL, "cptr->http2_state = %d cptr->url_num=%p", cptr->http2_state,cptr->url_num);\
   /*bug 80357 call HTTP2_HANDLE_CONNECT() only when cptr->url_num is available*/	\
   if(cptr->url_num)									\
     handle_connect(cptr, now, 0);\
 } else { \
  return HTTP2_ERROR;\
 }\
}

// Macro(s) used in Partial read 
// This Macro will retain previous data in cptr
#define COPY_PARTIAL_TO_CPTR(src_ptr) \
  NSDL2_HTTP2(NULL, NULL, "cptr->http2->partial_buff_max_size = %d, cptr->http2->partial_buff_size = %d", \
                                    cptr->http2->partial_buff_max_size, cptr->http2->partial_buff_size); \
  if (bytes_to_process > cptr->http2->partial_buff_max_size - cptr->http2->partial_buff_size){ \
    MY_REALLOC(cptr->http2->partial_read_buff, (cptr->http2->partial_buff_max_size + bytes_to_process), "cptr->http2->partial_read_buff", -1); \
    cptr->http2->partial_buff_max_size += bytes_to_process; \
  } \
  memcpy(cptr->http2->partial_read_buff + cptr->http2->partial_buff_size, src_ptr, bytes_to_process); \
  cptr->http2->partial_buff_size += bytes_to_process; \
  NSDL2_HTTP2(NULL, NULL, "bytes copied in partial buffer = %d, cptr->http2->partial_buff_size = %d", \
								  bytes_to_process, cptr->http2->partial_buff_size); 

// This Macro will not retain previous data in cptr.This is the case when we have read extra bytes and atmost 1 frame is cpmplete 
#define COPY_PARTIAL_TO_CPTR_AND_RESET(src_ptr) \
  if(cptr->http2){\
  NSDL2_HTTP2(NULL, NULL, "cptr->http2->partial_buff_max_size = %d", cptr->http2->partial_buff_max_size); \
  if (bytes_to_process > cptr->http2->partial_buff_max_size){ \
    MY_REALLOC(cptr->http2->partial_read_buff, bytes_to_process, "cptr->http2->partial_read_buff", -1); \
    cptr->http2->partial_buff_max_size = bytes_to_process;\
  }\
  memmove(cptr->http2->partial_read_buff, src_ptr, bytes_to_process);\
  cptr->http2->partial_buff_size = bytes_to_process;\
  NSDL2_HTTP2(NULL, NULL, "bytes copied in partial buffer = %d, cptr->http2->partial_buff_size = %d",\
								  bytes_to_process, cptr->http2->partial_buff_size);} 
  

// This Macro will COPY_BYTES_TO_SHORT .
#define COPY_BYTES_TO_SHORT(in_buffer, value) {\
  unsigned short tmp; \
  tmp = in_buffer[0];\
  tmp <<= 8; \
  tmp |= in_buffer[1];\
  *value = tmp;\
}

//This Macro function will COPY_BYTES_TO_INTEGER
#define COPY_BYTES_TO_INTEGER(in_buffer, value) { \
  unsigned int b, b8, b16, b24, val; \
  b = *(in_buffer + 3); \
  b8 = *(in_buffer + 2); \
  b16 = *(in_buffer + 1); \
  b24 = *in_buffer; \
  b8 <<= 8; \
  b16 <<= 16; \
  b24 <<= 24; \
  val = b | b8 | b16 | b24; \
  *value = val; \
}

/*bug 93672: gRPC - macro added to check for FRAME TYPE */
#define IS_DATA_OR_HEADER_FRAME(type) ((type != SETTINGS_FRAME) && (type != PING_FRAME))
#define NS_DEFAULT_STREAM_ID	1
#define NS_SEND_PING_ACK	1
#define NS_SEND_PING_FRAME	0

void set_id_in_hash_table(hash_table *hash_arr, char *url, int url_len, int promised_stream_id)
{
  /* FNV-1a algo
   *  http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
   */
  unsigned long long const offset_basis = 2166136261LL;
  unsigned long long const fnv_prime = 16777619LL;
  unsigned long long h = offset_basis;
  int prime = 251;
  //int prime = 499;
  //int prime = 997;
  int index, i;

  NSDL2_HTTP2(NULL, NULL, "Method called. url = %s, url_len = %d promise_stream_id is %u", url, url_len, promised_stream_id);
  if(!url || url_len <= 0)
  {
    fprintf(stderr,"Error: url in pushed request is not correct\n");
    return;
  }
    
  for (i = 0; i < url_len; i++)
  {
    h ^= url[i];
    h *= fnv_prime;
  }

  index = (int) (h % (unsigned long long) prime);
  NSDL2_HTTP2(NULL, NULL, "Hash_index = %d", index);
  
  //first time
  if(!hash_arr[index].url_len)
  {
    hash_arr[index].promised_stream_id = (short)promised_stream_id;
    hash_arr[index].url_len = (short)url_len;
    MY_MALLOC(hash_arr[index].url, sizeof(char)*(url_len + 1), "cptr->http2->hash_arr[index].url", -1);
    memcpy(hash_arr[index].url, url, url_len);
    return;
  }
  
  //hash collision 
  hash_table *curr_ht = &hash_arr[index];
  hash_table *prev_ht;
  while(curr_ht)
  {
    prev_ht = curr_ht;
    curr_ht = curr_ht->next;
  }
  
  MY_MALLOC_AND_MEMSET(curr_ht, sizeof(hash_table), "tmp_hash_table", -1);  
  prev_ht->next = curr_ht;
  curr_ht->url_len = (short)url_len;
  curr_ht->promised_stream_id = (short)promised_stream_id;
  MY_MALLOC(curr_ht->url, sizeof(char)*(url_len + 1), "curr_ht->url", -1);
  memcpy(curr_ht->url, url, url_len);
  curr_ht->next = NULL;
}

int get_id_from_hash_table(hash_table *hash_arr, char *url, int url_len)
{
  unsigned long long const offset_basis = 2166136261LL;
  unsigned long long const fnv_prime = 16777619LL;
  unsigned long long h = offset_basis;
  int prime = 251;
  //int prime = 499;
  //int prime = 997;
  int index, i, ret_val;

  NSDL2_HTTP2(NULL, NULL, "Method called. url = %s, url_len = %d", url, url_len);
  if(!url || url_len <= 0)
  {
    fprintf(stderr,"Error: url to be checked for pushed response is not correct\n");
    return 0;
  }

  for (i = 0; i < url_len; i++)
  {
    h ^= url[i];
    h *= fnv_prime;
  }
  
  index = (int) (h % (unsigned long long) prime);
  
  hash_table *curr_ht = &hash_arr[index];
 
  while(curr_ht)
  {
    if(curr_ht->url_len == url_len)
    {
      if(!strncmp(curr_ht->url, url, url_len)){
        NSDL2_HTTP2(NULL, NULL, "Hash_index = %d", index);
        ret_val = (int)(curr_ht->promised_stream_id);
        return ret_val;
      }      
    } 
    
    curr_ht = curr_ht->next;    
  }
  NSDL2_HTTP2(NULL, NULL, "Hash_index = %d", index);
  return 0;  
}

// This function copies data to partial buf if it reads extra bytes from close connection

inline void http2_copy_data_to_buf(connection *cptr, unsigned char *buf, int bytes_to_process){

  int cur_partial_buf_length = cptr->http2->partial_buff_size + cptr->http2->byte_processed;
  NSDL2_HTTP2(NULL, NULL, "bytes copied in partial buffer = %d, cptr->http2->partial_buff_size = %d", bytes_to_process, cptr->http2->partial_buff_size);
 
  if (bytes_to_process > cptr->http2->partial_buff_max_size - cptr->http2->partial_buff_size){ 
    MY_REALLOC(cptr->http2->partial_read_buff, (cptr->http2->partial_buff_max_size + bytes_to_process), "cptr->http2->partial_read_buff", -1); 
    cptr->http2->partial_buff_max_size += bytes_to_process; 
  } 
  memcpy(cptr->http2->partial_read_buff + cur_partial_buf_length, buf, bytes_to_process);          
  cptr->http2->partial_buff_size += bytes_to_process; 
}


inline void init_stream_map_http2(connection *cptr){

  unsigned int max_concurrent_streams;

  NSDL2_HTTP2(NULL, NULL, "Method Called. cptr->http2->settings_frame.settings_max_concurrent_streams = %d, MAX_CONCURRENT_STREAM = %d", 
				cptr->http2->settings_frame.settings_max_concurrent_streams, MAX_CONCURRENT_STREAM);
  if(cptr->http2->settings_frame.settings_max_concurrent_streams > MAX_CONCURRENT_STREAM)
    max_concurrent_streams = cptr->http2->settings_frame.settings_max_concurrent_streams;
  else 
    max_concurrent_streams = MAX_CONCURRENT_STREAM;

  cptr->http2->settings_frame.settings_map_size = 2 * max_concurrent_streams;

  // Calling methods to init stream map & queue used in it  
  init_stream_map(&cptr->http2->cur_stream_map, cptr->http2->settings_frame.settings_map_size, &cptr->http2->available_streams); 
  init_stream_queue(&cptr->http2->cur_map_queue, cptr->http2->settings_frame.settings_map_size);
}

/*--------------------------------------------------------------------------------
Function Allocates memory for structures for server push
----------------------------------------------------------------------------------*/

void allocate_http2_server_push_struct(connection *cptr)
{
  int old_promise_count;
  int new_promise_count;
  int i,j;

  NSDL2_HTTP2(NULL, NULL, "Method called");

  old_promise_count = cptr->http2->promise_count;
  cptr->http2->promise_count_max += PROMISE_BUF_DELTA;
  new_promise_count = cptr->http2->promise_count_max;

  MY_REALLOC_AND_MEMSET(cptr->http2->promise_buf, sizeof(promise)*new_promise_count, sizeof(promise)*old_promise_count, "cptr->http2->promise_buf", -1);
 
  for(i = old_promise_count; i < new_promise_count; i++){
    MY_MALLOC(cptr->http2->promise_buf[i].response_headers, sizeof(nghttp2_nv)*NUM_HEADERS_IN_PROMISE_BUF, "cptr->http2->promise_buf->response_headers", -1);
    cptr->http2->promise_buf[i].header_count_max = NUM_HEADERS_IN_PROMISE_BUF;
    for(j =0; j< NUM_HEADERS_IN_PROMISE_BUF; j++){
      MY_MALLOC(cptr->http2->promise_buf[i].response_headers[j].name, MAX_HEADER_NAME_LEN, "cptr->http2->promise_buf[i].response_headers[j].name", -1);
      MY_MALLOC(cptr->http2->promise_buf[i].response_headers[j].value, MAX_HEADER_VALUE_LEN, "cptr->http2->promise_buf[i].response_headers[j].name", -1);
    }
  }
}


/*--------------------------------------------------------------------------------
Function Allocates memory for structures  
----------------------------------------------------------------------------------*/
// TODO: optimize ProtoHTTP2, Http2SettingsFrames allocation and memset 
void init_cptr_for_http2(connection *cptr)
{
  size_t rv;
 
  NSDL2_HTTP2(NULL, NULL, "Method called");

  VUser *vptr = (VUser *)cptr->vptr;

  // Set http2_state to HTTP2_CON_PREFACE_CNST that indicates connection preface is not send yet
  cptr->http2_state = HTTP2_CON_PREFACE_CNST;

  MY_MALLOC_AND_MEMSET(cptr->http2, sizeof(ProtoHTTP2), "cptr->http2", -1);

  MY_MALLOC(cptr->http2->continuation_buf, PARTIAL_CONT_BUF_INIT_SIZE, "cptr->http2->continuation_buf", -1);
  cptr->http2->continuation_buf_len = 0;
  cptr->http2->continuation_max_buf_len = PARTIAL_CONT_BUF_INIT_SIZE;

  // Initialize max stream id 1 for upgrade mode as it will start stream id from 3.
  // These max stream id will be used by incrementing 2
  cptr->http2->max_stream_id = NS_DEFAULT_STREAM_ID;

  // Init partial_buff_size by 0
  cptr->http2->partial_buff_size = 0;

  cptr->http2->flow_control.local_window_size = 65535;
  cptr->http2->flow_control.remote_window_size = 65535;
  cptr->http2->flow_control.received_data_size = 0;
  cptr->http2->flow_control.data_left_to_send = 0;

/*
  // Mark host as http2
  idx = get_svr_ptr(cptr->url_num, vptr)->idx;
  (vptr->hptr + idx)->http_mode = HTTP_MODE_HTTP2;
  NSDL2_HTTP2(NULL, NULL, "Host is now set to HTTP2. (vptr->hptr + idx)->http_mode is [%d]. hptr = %p", (vptr->hptr + idx)->http_mode, vptr->hptr); 
*/
  //NSDL2_HTTP(NULL, cptr, "Setting host as HTTP2.  hptr->http_mode = %d", vptr->hptr->http_mode);

  // initialize objects for deflator
  rv = nghttp2_hd_deflate_new(&(cptr->http2->deflater), 4096);
  if (rv != 0) {
    fprintf(stderr, "initialisation of deflate failed with error: %s\n", nghttp2_strerror(rv));
    end_test_run();
  }

  // Initialize objects for inflator 
  rv = nghttp2_hd_inflate_new(&(cptr->http2->inflater));

  if (rv != 0) {
    fprintf(stderr, "nghttp2_hd_inflate_init failed with error: %s\n",
            nghttp2_strerror(rv));
  }
  NSDL2_HTTP2(NULL, NULL,"cptr->http2->inflater=%p cptr->http2->deflater=%p", cptr->http2->inflater, cptr->http2->deflater); 
  //Server Push will be disabled in case of GetNoInlineObj
    if(runprof_table_shr_mem[vptr->group_num].gset.get_no_inlined_obj)
      runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push = 0;
  
  NSDL2_HTTP2(NULL, NULL, "settings_enable_push = %d for group_num = %d", runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push, vptr->group_num);
 
  if(runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push){
    MY_MALLOC_AND_MEMSET(cptr->http2->hash_arr, sizeof(hash_table)*HASH_ARRAY_INIT_SIZE, "cptr->http2->hash_arr", -1);
    allocate_http2_server_push_struct(cptr); 
  }
  cptr->http2->main_resp_received = 1; 
}

// This method will free all the active streams in a connection
// It also free map pointer and queue pointer for that cptr 
static inline void clear_stream_map(connection *cptr){
 
  NsH2StreamMap *map_start_ptr = cptr->http2->cur_stream_map;
  unsigned int max_streams = cptr->http2->settings_frame.settings_map_size * 2;
  unsigned int i;
  NSDL2_HTTP2(NULL, NULL, "Method Called, cptr = %p, max_streams = %d", cptr, max_streams);

  // free all streams for this cptr
  if(map_start_ptr) {
    for(i = 0; i < max_streams; i++){
      if(map_start_ptr[i].stream_ptr)
        release_stream(cptr, map_start_ptr[i].stream_ptr);
    }
    // Free stream map for this cptr
    FREE_AND_MAKE_NULL(cptr->http2->cur_stream_map, "cptr->http2->cur_stream_map", -1);
  }

  // Free queue ns_h2_slot
  if(cptr->http2->cur_map_queue){
    if(cptr->http2->cur_map_queue->ns_h2_slot)
      FREE_AND_MAKE_NULL(cptr->http2->cur_map_queue->ns_h2_slot, "cptr->http2->cur_map_queue->ns_h2_slot", -1);

    // Free stream queue for this cptr
    FREE_AND_MAKE_NULL(cptr->http2->cur_map_queue, "cptr->http2->cur_map_queue", -1);
  }
}

void free_http2_server_push_struct(connection *cptr) {
    
  int i,j;
  int promise_buf_size;
  int num_headers;
  
  NSDL2_HTTP2(NULL, NULL, "Method Called");

  promise_buf_size = cptr->http2->promise_count_max;
  hash_table *curr_ht, *curr_next_ht;
 
  //Freeing Hash_table used to hash urls 
  for(i = 0; i < HASH_ARRAY_INIT_SIZE; i++){
    curr_ht = &cptr->http2->hash_arr[i];
    curr_next_ht = curr_ht->next;
    if(curr_ht->url_len){
      FREE_AND_MAKE_NULL(curr_ht->url,"curr_ht->url", -1);
    }
    while(curr_next_ht){
      hash_table *temp_ht = curr_next_ht;
      curr_next_ht = curr_next_ht->next;
      FREE_AND_MAKE_NULL(temp_ht->url,"temp_ht->url", -1);
      FREE_AND_MAKE_NULL(temp_ht,"temp_ht", -1);
    }
  }

  FREE_AND_MAKE_NULL(cptr->http2->hash_arr,"cptr->http2->hash_arr", -1);

  for(i = 0; i < promise_buf_size; i++)
  {
    //if(cptr->http2->promise_buf[i].free_flag)
   // {
      num_headers = cptr->http2->promise_buf[i].header_count_max;
      for(j = 0; j < num_headers; j++)
      {
        FREE_AND_MAKE_NULL(cptr->http2->promise_buf[i].response_headers[j].name, "cptr->http2->promise_buf[].response_headers[].name", -1);
        FREE_AND_MAKE_NULL(cptr->http2->promise_buf[i].response_headers[j].value, "cptr->http2->promise_buf[].response_headers[].value",-1);
      }
      FREE_AND_MAKE_NULL(cptr->http2->promise_buf[i].response_headers, "cptr->http2->promise_buf[i].response_headers", -1);
      FREE_AND_MAKE_NULL(cptr->http2->promise_buf[i].response_data, "cptr->http2->promise_buf[i].response_data", -1);
   // }
  }
  FREE_AND_MAKE_NULL(cptr->http2->promise_buf, "cptr->http2->promise_buf", -1);
}

/*----------------------------------------------------------------------
This function free http2 from cptr 
------------------------------------------------------------------------*/
inline void free_http2_data( connection *cptr) {

  NSDL2_HTTP2(NULL, NULL, "Method Called");

  VUser *vptr = (VUser *)cptr->vptr; 
  cptr->http2_state = HTTP2_CON_PREFACE_CNST;
  
  /*after free_http2_data, to ensure that ns must do  connection upgrade after any failure or connection closure, 
  instead of direct making request, as direct request leading to crash in bug 40306 after SSL write failure */
  cptr->http_protocol = HTTP_MODE_AUTO; /* bug 40306 - crash after ssl write fail*/  
  if(!cptr->http2)
    return;

  clear_stream_map(cptr);

  // free partial buff and reset its size
  if(cptr->http2->partial_read_buff)
    FREE_AND_MAKE_NULL(cptr->http2->partial_read_buff, "cptr->http2->partial_read_buff", -1) 
  cptr->http2->partial_buff_max_size = 0; 
  cptr->http2->partial_buff_size = 0;

  // free continuation buff and reset its size
  if(cptr->http2->continuation_buf)
    FREE_AND_MAKE_NULL(cptr->http2->continuation_buf, "cptr->http2->continuation_buf", -1);
  cptr->http2->continuation_buf_len = 0;
  cptr->http2->continuation_max_buf_len = 0;

  NSDL2_HTTP2(NULL, NULL,"free cptr->http2->inflater=%p cptr->http2->deflater=%p", cptr->http2->inflater, cptr->http2->deflater);
  // free inflater object: This will clean memory used by nghttp2 for header compression  
  if(cptr->http2->inflater)
    nghttp2_hd_inflate_del(cptr->http2->inflater);

  // free deflater object: This will clean memory used by nghttp2 for header compression  
  if(cptr->http2->deflater) 
    nghttp2_hd_deflate_del(cptr->http2->deflater);

  //Freeing memory malloced for server push
  if(runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push){
    free_http2_server_push_struct(cptr);  
  }
  // free cptr->http2
  FREE_AND_MAKE_NULL(cptr->http2, "cptr->http2", -1);
  
}

/*-------------------------------------------------------------------
This function is used to decode header and dumps header into file  
---------------------------------------------------------------------*/
int inflate_header_block(connection *cptr, nghttp2_hd_inflater *inflater, uint8_t *in,
                         size_t inlen, int final, unsigned int stream_id, u_ns_ts_t now) {
  ssize_t rv;
  NSDL2_HTTP2(NULL, NULL, "Method Called. inlen=%d inflater=%p in=%p", inlen, inflater, in);
  char *new_line = "\n";
  char *header_start_char = ": ";
#if 0
#ifdef NS_DEBUG_ON
  debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
  debug_log_http2_res(cptr,(unsigned char *)cptr->url, cptr->url_len);
  debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
 #else
  if(runprof_table_shr_mem[((VUser *)cptr->vptr)->group_num].gset.trace_level)
  {
    log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
    log_http2_res_ex(cptr, (unsigned char *)cptr->url, cptr->url_len);
    log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
  }
#endif
#endif
  // TODO add comments 
  for (;;) {
    nghttp2_nv nv;
    int inflate_flags = 0;
    size_t proclen;
    rv = nghttp2_hd_inflate_hd(inflater, &nv, &inflate_flags, in, inlen, final);
    NSDL2_HTTP2(NULL, NULL, " inflater=%p in=%p nv.name=%s, nv.namelen=%d, nv.value=%s, nv.valuelen=%d rv=%d", inflater, in, (char *)nv.name, nv.namelen, (char *)nv.value, nv.valuelen, rv);
    if (rv < 0) {
      fprintf(stderr, "inflate failed with error code %zd", rv);
      pack_goaway(cptr, stream_id, COMPRESSION_ERROR, "Received COMPRESSION_ERROR from peer", now);
      return -1;
    }

    proclen = (size_t)rv;

    in += proclen;
    inlen -= proclen;
    //if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
    //  ret = sprintf(decoded_hdr_buf, "%s%s:%s\n", decoded_hdr_buf, nv.name, nv.value);
    //}

    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
      nghttp2_hd_inflate_end_headers(inflater);
      break;
    }

    if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
      break;
    }
#ifdef NS_DEBUG_ON
    if(!strncmp((char *)nv.name, ":status", nv.namelen))
    {
      debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
      debug_log_http2_res(cptr,(unsigned char *)cptr->url, cptr->url_len);
      debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
    }
    debug_log_http2_res(cptr, (unsigned char *)nv.name, nv.namelen);
    debug_log_http2_res(cptr, (unsigned char *)header_start_char, 2);
    debug_log_http2_res(cptr, (unsigned char *)nv.value, nv.valuelen);
    debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
#else
  if(runprof_table_shr_mem[((VUser *)cptr->vptr)->group_num].gset.trace_level)
  {
    if(!strncmp((char *)nv.name, ":status", nv.namelen))
    {
      log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
      log_http2_res_ex(cptr, (unsigned char *)cptr->url, cptr->url_len);
      log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
    }
    log_http2_res_ex(cptr, (unsigned char *)nv.name, nv.namelen);
    log_http2_res_ex(cptr, (unsigned char *)header_start_char, 2);
    log_http2_res_ex(cptr, (unsigned char *)nv.value, nv.valuelen);
    log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
  }
#endif
   process_response_headers(cptr, nv, now);
  }
  return 0;
}

int ns_h2_inflate(connection *cptr, uint8_t *in, size_t inlen, unsigned int stream_id, u_ns_ts_t now)
{

  int rv;
  NSDL2_HTTP2(NULL, NULL, "Method Called. frame_length [%zu] cptr->http2->inflater=%p in=%p", inlen, cptr->http2->inflater, in);
  /*Decode header set */
  rv = inflate_header_block(cptr, cptr->http2->inflater, in, inlen, 1, stream_id, now);

  if (rv != 0) {
    pack_goaway(cptr, stream_id, COMPRESSION_ERROR, "Received COMPRESSION_ERROR from peer", now);
    return -1; 
  }

 // nghttp2_hd_inflate_del(inflater);

  return 0;
}


/*-----------------------------------------------------------------
Function checks frame type in response.
Possible Frame Type in response can be DATA, HEADER , SETTINGS (etc)
We are returning after 1 check because we will parse only 1 frame at a time
-------------------------------------------------------------------*/
unsigned char check_frame_type(unsigned char frame)
{
  NSDL2_HTTP2(NULL, NULL, "Method called, frame = %x", frame);
  if (frame == DATA_FRAME) {
    return DATA_FRAME; 
  } else if (frame == HEADER_FRAME) {
    return HEADER_FRAME; 
  } else if (frame == PRIORITY_FRAME) {
    return PRIORITY_FRAME; 
  } else if (frame == RESET_FRAME) {
    return RESET_FRAME; 
  } else if (frame == SETTINGS_FRAME) {
    return SETTINGS_FRAME; 
  } else if (frame == WINDOW_UPDATE_FRAME) {
    return WINDOW_UPDATE_FRAME;
  } else if (frame == GOAWAY_FRAME) {
    return GOAWAY_FRAME;
  } else if (frame == PUSH_PROMISE){
    return PUSH_PROMISE;
  } else if (frame == PING_FRAME) {
    return PING_FRAME;
  } else if (frame == CONTINUATION_FRAME) {
    return CONTINUATION_FRAME;
  } else {
    NSDL2_HTTP2(NULL, NULL, "Invalid frame, frame = %x", frame);
    return INVALID_FRAME;
  }
}

/*
This function calculate payload length from frame header . 
Payload Load length is in three bytes of the response buffer  
*/
int process_frame_length (connection* cptr, unsigned char *buffer , unsigned int *frame_size)
{
  int i; 
  unsigned int value = 0; 
  unsigned int frame_len;
  int offset = 0;
 
  NSDL2_HTTP2(NULL, NULL, "Method called. Buffer is [%x]", buffer);

  for (i = 0; i < 3; i++) 
  {
    NSDL2_HTTP2(NULL, NULL, "%dthbyte = %x", i, buffer[i]);
    value = value * 256 + (unsigned char)buffer[i];  
  } 
  frame_len = (unsigned int)(value);
  NSDL2_HTTP2(NULL, NULL, "frame_len = %d", frame_len);
  *frame_size = frame_len; 
  offset += 3;  // Offset is total number of bytes processed 
  return offset;
}

/*This function process goaway frame . In case of goaway we immediatelty close connection 
    +-+-------------------------------------------------------------+
    |R|                  Last-Stream-ID (31)                        |
    +-+-------------------------------------------------------------+
    |                      Error Code (32)                          |
    +---------------------------------------------------------------+
    |                  Additional Debug Data (*)                    |
    +---------------------------------------------------------------+
------------------------------------------------------------------------------------------*/

int process_goaway_frame(connection *cptr, unsigned char *in_buff, int frame_length , u_ns_ts_t now) 
{
  unsigned int last_stream_id; 
  unsigned int error_code ; 
  unsigned char debug_data[1024] = "";
  int len = 0;
  // Last stream id is
  COPY_BYTES_TO_INTEGER(in_buff, &last_stream_id);
  NSDL2_HTTP2(NULL, NULL, "Last stream id processed is [%d] ", last_stream_id);
  len += 4; 
  unsigned char *tmp = (in_buff + len);
  COPY_BYTES_TO_INTEGER(tmp, &error_code);
  len += 4;
  NSDL2_HTTP2(NULL, NULL, "frame_length is [%d] and len = [%d]",frame_length , len ); 
  if (frame_length > len)
  {
    MY_MEMCPY(debug_data, in_buff + len, (frame_length - len));
    NSDL2_HTTP2(NULL, NULL, "Debug data is [%s]\n ", debug_data);
  }
  
  // Get error codes 
  switch (error_code) {

    case NO_ERROR : 
      NSDL2_HTTP2(NULL, NULL, "Received no error from peer");
      break;

    case PROTOCOL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received PROTOCOL_ERROR from peer");
      break;

    case INTERNAL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received INTERNAL_ERROR from peer");
      break;  
       
    case FLOW_CONTROL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received FLOW_CONTROL_ERROR from peer");
      break;  
 
    case SETTINGS_TIMEOUT:
      NSDL2_HTTP2(NULL, NULL, "Received SETTINGS_TIMEOUT from peer");
      break;  
      
    case FRAME_SIZE_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received FRAME_SIZE_ERROR from peer");
      break; 
 
    case REFUSED_STREAM:
      NSDL2_HTTP2(NULL, NULL, "Received REFUSED_STREAM from peer");
      break;  

    case COMPRESSION_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received COMPRESSION_ERROR from peer");
      break;  

    case CONNECT_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received CONNECT_ERROR from peer");
      break;  

    case HTTP_1_1_REQUIRED:
      NSDL2_HTTP2(NULL, NULL, "Received HTTP_1_1_REQUIRED from peer");
      break; 
    
    default :
     NSDL2_HTTP2(NULL, NULL, "This should not come here");
     break;
  }
 
  #ifdef NS_DEBUG_ON
  VUser *vptr = (VUser *)cptr->vptr;  
  /*bug 54315 : get hptr and updte vptr, hptr's urls_awaited/left count accordingly*/
  HostSvrEntry *hptr = vptr->hptr + cptr->gServerTable_idx;
  NSDL2_HTTP2(NULL, NULL, "urls_left=%d urls_awaited=%d total_open_streams=%d  hptr[%p] hurl_left=%d num_parallel=%d",
             vptr->urls_left,vptr->urls_awaited,cptr->http2->total_open_streams, hptr, hptr->hurl_left, hptr->num_parallel);
  #endif
 if(cptr->conn_state > CNST_REUSE_CON)
 {
   NSDL2_HTTP2(NULL, NULL, "urls_left=%d urls_awaited [%d] vptr[%p]->head_hlist=%p cptr->conn_state=%d",
                  vptr->urls_left, vptr->urls_awaited, vptr, vptr->head_hlist, cptr->conn_state);

   NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "Received goaway frame cptr->http2_state %hd error_code = %d",cptr->http2_state, error_code);
   /*Close_connection only when connection state is other than CNST_FREE and REUSE in order to avoid crash,
    as url_num would be zero for these states */
   Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_CLOSE);
 }
 NSDL2_HTTP2(NULL, NULL, "Exiting....with  hptr->hurl_left=%d", hptr->hurl_left);
 return HTTP2_ERROR;
}
/*
    +---------------+
    |Pad Length? (8)|
    +-+-------------+-----------------------------------------------+
    |R|                  Promised Stream ID (31)                    |
    +-+-----------------------------+-------------------------------+
    |                   Header Block Fragment (*)                 ...
    +---------------------------------------------------------------+
    |                           Padding (*)                       ...
    +---------------------------------------------------------------+
                       PUSH_PROMISE Payload Format

This method processes the push promise frame and decompresses the pushed requests received in header block fragment.
*/

int process_push_promise_frame(connection *cptr, unsigned char *in_buffer, int frame_length, unsigned char flags, unsigned int stream_id, unsigned char frame_type, u_ns_ts_t now) {

  int promised_stream_id;
  int total_length=0;
  int pad_length=0;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_HTTP2(NULL, NULL, "Method called. Frame length is %d, flags %d stream_id is %u", frame_length, flags ,stream_id);  
  
 //Error handling left
 /* If the stream identifier field specifies the value 0x0, a recipient MUST respond with a connection error 
   of type PROTOCOL_ERROR.*/
  
 /*If PUSH_PROMISE frame is while setting enable push was disabled recieved :connection error (Section 5.4.1) of type
   PROTOCOL_ERROR.*/
  if(!runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push){
    NSDL2_HTTP2(NULL, NULL, "Resources are pushed even while setting_enable_push was disabled, hence ignoring this frame");
    pack_goaway(cptr, stream_id, PROTOCOL_ERROR, "Received pushed frames from peer even when settings_enable_push was disabled", now);
    return HTTP2_ERROR;
  }

 // Check if Padding is set. We will always ignore Padding.
  if (CHECK_FLAG(flags, PADDING_SET)) {
    pad_length = (int)in_buffer[0] ;
    NSDL2_HTTP2(NULL, NULL, "Padding is set pad length is %d", pad_length);
    total_length =  frame_length - pad_length -5;
    in_buffer++;
  }
  else
    total_length = frame_length -4 ;
    
   //promised_stream_id of resource to be pushed , should be even and not 0 
   COPY_BYTES_TO_INTEGER(in_buffer, &promised_stream_id);
   in_buffer +=4;
  
   //End Header is set ,going to call inflate API , Handling of continuation frames left
   if (CHECK_FLAG(flags,END_HEADER_SET)) { 
     //NS_DT4(NULL, NULL, DM_L1, MM_HTTP, "Pushed Request Headers on stream id = %d, promised_stream_id = %d", stream_id, promised_stream_id);
     NSDL2_HTTP2(NULL, NULL, "Pushed Request Headers on stream id = %d, promised_stream_id = %d", stream_id, promised_stream_id);
     for (;;) {
     
     size_t rv;
     nghttp2_nv nv;
     int inflate_flags = 0;
     size_t proclen;
 
     rv = nghttp2_hd_inflate_hd(cptr->http2->inflater, &nv, &inflate_flags, in_buffer, total_length, 1);
 
     if (rv < 0) {
       fprintf(stderr, "inflate failed with error code %zd", rv);
       return -1;
     }
 
     proclen = (size_t)rv;
 
     in_buffer += proclen;
     total_length -= proclen;
     
     //We will be creating a hash_code of the path value which will be used as  an index to store promised_stream_id in hash_array
     if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
       //:path length is 5, will only check when length is 5
       if(nv.namelen == 5)
       {
         //Only :path header is of length 5 with character p at index 1 hence not comparing whole string
         if((char)nv.name[1] == 'p')
         {
           set_id_in_hash_table(cptr->http2->hash_arr,(char *)nv.value, nv.valuelen, promised_stream_id);
         }
       }
       
       //NS_DT4(NULL, NULL, DM_L1, MM_HTTP, "%s : %s", nv.name, nv.value);
       NSDL2_HTTP2(NULL, NULL, "%s : %s", nv.name, nv.value); 
     }
 
     if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
       nghttp2_hd_inflate_end_headers(cptr->http2->inflater);
       break;
     }
 
     if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && total_length == 0) {
       break;
    }
   }
  }
  //To keep count of how many resources will be pushed after first request, handled in handle_http2_read() 
  cptr->http2->promise_count++;
  /*bug 70480 : update server push count*/
  INC_HTTP2_SERVER_PUSH_COUNTER(vptr)
  /*Unless all pushed resources are received and the response of main is received will not go in Close_connection*/
  cptr->http2->curr_promise_count++;
  //This will get set when response of stream_id(odd) on which resources are pushed is received
  cptr->http2->main_resp_received = 0;
  if(cptr->http2->promise_count == cptr->http2->promise_count_max){
    allocate_http2_server_push_struct(cptr); 
  }
  NSDL2_HTTP2(NULL, NULL, " cptr->http2->promise_count=%d  average_time->num_srv_push=%d",
				cptr->http2->promise_count, average_time->num_srv_push); /*bug 70480 */
  return frame_length;
}


/*--------------------------------------------------------------------------------
This function helps in process reset frame. On receive of reset frame we immediatly close current stream 
and change stream state to half-close(remote) 
----------------------------------------------------------------------------------*/
int process_reset_frame(connection *cptr, unsigned char *in_buff, int frame_length, unsigned char flags, unsigned int stream_id, u_ns_ts_t now)
{
  int bytes_handled = 0;
  unsigned int error_code = 0;
  
  NSDL2_HTTP2(NULL, NULL, "Method Called ");

  if (frame_length != 4)
    return HTTP2_ERROR;

  COPY_BYTES_TO_INTEGER(in_buff, &error_code);

  NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "Received reset frame cptr->http2_state %hd error_code = %d",cptr->http2_state, error_code);
 
  NSDL2_HTTP2(NULL, NULL, "error code is [%d]", error_code);
  bytes_handled += frame_length; 
  // check whether sid == 0 ; 
  if (stream_id == 0) 
  {
    NSDL2_HTTP2(NULL, NULL,"Error : Received Reset frame with streamid == 0 . Which is not possible"); 
    return HTTP2_ERROR; 
  }
  
  switch (error_code) {

    case NO_ERROR: 
      NSDL2_HTTP2(NULL, NULL, "Received no error from peer");
      break;

    case PROTOCOL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received PROTOCOL_ERROR from peer");
      break;

    case INTERNAL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received PROTOCOL_ERROR from peer");
      break;  
       
    case FLOW_CONTROL_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received FLOW_CONTROL_ERROR from peer");
      break;  
 
    case SETTINGS_TIMEOUT:
      NSDL2_HTTP2(NULL, NULL, "Received SETTINGS_TIMEOUT from peer");
      break;  
      
    case FRAME_SIZE_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received FRAME_SIZE_ERROR from peer");
      break; 
 
    case REFUSED_STREAM:
      NSDL2_HTTP2(NULL, NULL, "Received REFUSED_STREAM from peer");
      break;  

    case COMPRESSION_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received COMPRESSION_ERROR from peer");
      break;  

    case CONNECT_ERROR:
      NSDL2_HTTP2(NULL, NULL, "Received CONNECT_ERROR from peer");
      break;  

    case HTTP_1_1_REQUIRED:
      NSDL2_HTTP2(NULL, NULL, "Received HTTP_1_1_REQUIRED from peer");
      break; 
    
    default :
     NSDL2_HTTP2(NULL, NULL, "This should not come here");
     break;
  }

  //Received reset for push promise frame
  if(stream_id % 2 == 0)
  {
    cptr->http2->curr_promise_count --;
    return frame_length;
  }
  // Get stream ptr, copy it to cptr and release stream 
  stream *stream_ptr;
  if(stream_id >1) {
    stream_ptr = get_stream(cptr, stream_id);
    if(stream_ptr)
    {
      NSDL3_HTTP2(NULL, NULL, "Going to delete stream as received reset for that stream");
      copy_stream_to_cptr(cptr, stream_ptr);
      RELEASE_STREAM(cptr, stream_ptr);
    }
  }
 
 /* 
 // Change stream state  //TODO: make it stream_id specific (sptr[sid].state)
  NSDL2_HTTP2(NULL, NULL, "current stream state is [%d]", stream_ptr->state); 
  if (stream_ptr->state == NS_H2_OPEN ){
    // change current stream state to 
    stream_ptr->state = NS_H2_CLOSED;  
    remove_ifon_stream_inuse_list(stream_ptr);   
  }
  else if (stream_ptr->state == NS_H2_HALF_CLOSED_R) {
    // change it to  closed    
    stream_ptr->state = NS_H2_CLOSED; 
    remove_ifon_stream_inuse_list(stream_ptr); 
  }
 */
  return frame_length; 
}

/*************************************************************************************/
/* Name         :h2_make_and_send_ping_frame
*  Description  :It creates/make PING Frame and Send accrordingly
*  Arguments    :cptr, now=> current time
*  Result       :NA
*  Return       :
*		0 ==> success
*	       -1 ==> in case of error
*/
/**************************************************************************************/
int h2_make_and_send_ping_frame(connection *cptr, u_ns_ts_t now, int ack_flag){

  NSDL2_HTTP2(NULL, NULL, "Method called, ack_flag=%d", ack_flag);
  unsigned char *ping_frame;
  //ping frame size = 8 bytes for payload + 9 bytes for frame header
  MY_MALLOC_AND_MEMSET(ping_frame, HEADER_FRAME_SIZE + PING_FRAME_LENGTH, "ping_frame", -1);
  unsigned char payload[PING_FRAME_LENGTH]; //send 0 as ping payload
  memset(payload, 0, PING_FRAME_LENGTH);
  /*1st: create ping frame*/
  //as we are sending ping frame so set ack_flag as 0.
  if(HTTP2_ERROR == (cptr->bytes_left_to_send = make_ping_frame(cptr, payload, ping_frame, ack_flag)))
  {
    FREE_AND_MAKE_NULL(ping_frame, "ping frame", -1);
    cptr->bytes_left_to_send = 0;
    return HTTP2_ERROR;
  }
  cptr->free_array = (char *)ping_frame;
  /*reserve cptr and http2 states*/
  int http2_state = cptr->http2_state;
  NSDL2_HTTP2(NULL, cptr, "before cptr->conn_state=%d  cptr->http2_state=%d cptr->bytes_left_to_send=%d", cptr->conn_state, cptr->http2_state, cptr->bytes_left_to_send);
  //set http2_state
  cptr->http2_state = HTTP2_PING_CNST;
  /*2nd: send ping frame*/
  int ret = http2_handle_write(cptr, now);
  /*reset http2_state to its original state*/
  cptr->http2_state = http2_state;
  NSDL2_HTTP2(NULL, cptr, "reset back cptr->conn_state=%d cptr->http2_state=%d ret=%d", cptr->conn_state, cptr->http2_state, ret);
  return ret;
}


/*-----------------------------------------------------------------------------------
Function to Process ping frame .
In case ACK is received we will ignore ping frame . Otherwise we will response back
with Ping frame .
--------------------------------------------------------------------------------------*/
int process_ping_frame(connection *cptr, unsigned char *in_buff, int frame_length, unsigned char flags, unsigned int stream_id, u_ns_ts_t now){

  int len = 0;
  unsigned char *ping_frame = NULL;
  // check if acknowledge flag is set
  /*bug 78105:  replace END_STREAM by  END_STREAM_SET*/
  if (CHECK_FLAG(flags,END_STREAM_SET)) {
    NSDL2_HTTP2(NULL, cptr, "Ping frame received with Acknowlwdge flag, Hence ignoring ");
    return frame_length ;
  }

  // check if sid
  if (stream_id != 0) {
    NSDL2_HTTP2(NULL, cptr, "Stream Identifier for Ping frame must be 0. Hece going to close Connection");
    return frame_length ;
  }

  MY_MALLOC_AND_MEMSET(ping_frame, 32, "ping_frame", -1);  // Allocating 32 bytes because ping will receive 64 byte opaque -day which we will send back to server  and 9 bytes fixed header octates.
  NSDL2_HTTP2(NULL, cptr, "Ping frame received with Acknowledge flag unset. Therefore sending Response for Ping");
  len = make_ping_frame(cptr, in_buff , ping_frame, NS_SEND_PING_ACK);
  NSDL2_HTTP2(NULL, cptr, "len is [%d] ", len);

  cptr->http2_state = HTTP2_PING_RCVD;
  cptr->free_array = (char *)ping_frame;
  cptr->bytes_left_to_send = len;
  /*bug 54315 : reserve cptr->conn_state*/
  NSDL2_HTTP2(NULL, cptr, "before cptr->conn_state=%d ", cptr->conn_state);
  int local_state = cptr->conn_state;
  http2_handle_write(cptr, now);
  cptr->conn_state = local_state;
  NSDL2_HTTP2(NULL, cptr, "reset back to  cptr->conn_state=%d ", cptr->conn_state);

 return frame_length;
}

/*----------------------------------------------------------------------------------
 function process 9-octate fixed frame header . This is common to every frame.
------------------------------------------------------------------------------------*/
int process_header_frame(connection *cptr, unsigned char *in_buffer, int frame_length, unsigned char flags, unsigned int stream_id, unsigned char frame_type, u_ns_ts_t now) {

  int bytes_handled = 0; 
  stream *sptr = NULL;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_HTTP2(NULL, NULL, "Method called. Frame length is %d, flags %d stream_id is %u in_buffer=%p", frame_length, flags ,stream_id, in_buffer);

  //On even streams we will have pushed responses
  if((stream_id %2) != 0)
  {
    if (stream_id > 1) {
      sptr = get_stream(cptr, stream_id);
      copy_stream_to_cptr(cptr, sptr);
    }
    // Check whether end stream and end header flag is set . IF end_header is not set than we need to check for continuation frame 
    if (CHECK_FLAG(flags, END_STREAM_SET )) { // End stream is set 
      // Change stream state based on current state
      NSDL2_HTTP2(NULL, NULL, "End Stream flag is set. Stream will be closed after procesing this frame");
      // Close stream
      if (sptr)
        pack_and_send_reset_frame(cptr, sptr, NO_ERROR, now);
        //pack_reset(cptr, sptr, NO_ERROR);
        //TODO: what to do in case of reset no stream found 
    } else {
      NSDL2_HTTP2(NULL, NULL, "End Stream flag is not set. We Expect More Frame(s) on this stream"); 
    }
    if (CHECK_FLAG(flags, END_HEADER_SET)) { //END HEADER IS SET // We will not check continuatuion frame here ... continuation frame will never come if end header is set  
      if (frame_type == CONTINUATION_FRAME)
      {
        // TODO: move  continuation_buf to stream
        memcpy(cptr->http2->continuation_buf + cptr->http2->continuation_buf_len, in_buffer, frame_length);
        cptr->http2->continuation_buf_len += frame_length;
        cptr->tcp_bytes_recv += frame_length; 
        NSDL2_HTTP2(NULL, NULL, "Continuation frame received. Buf_length is [%d], cptr->tcp_bytes_recv = %d",
                                                     cptr->http2->continuation_buf_len, cptr->tcp_bytes_recv);
        // Decode header frame received 
        int ret = ns_h2_inflate(cptr, (uint8_t*) cptr->http2->continuation_buf, (size_t)cptr->http2->continuation_buf_len, stream_id, now); 
        if (ret == -1)
          return HTTP2_ERROR;
        else 
          bytes_handled += cptr->http2->continuation_buf_len;
      } else {
        // no more continuation frame & Decode header frame received 
        cptr->tcp_bytes_recv += frame_length;
        NSDL2_HTTP2(NULL, NULL, "No Continuation Frame received. frame_length is [%d], cptr->tcp_bytes_recv = %d stream_id=%d in_buffer=%p", 
                                                                               frame_length, cptr->tcp_bytes_recv, stream_id, in_buffer);
        int ret = ns_h2_inflate(cptr, (uint8_t*) in_buffer, (size_t)frame_length, stream_id, now);
        if (ret == -1 )
          return HTTP2_ERROR;
        else  
          bytes_handled += frame_length;
      }
      NSDL2_HTTP2(NULL, NULL, "sptr is %p and stream id is %u", sptr, stream_id);
      // Copy data to stream because here we dont know this response is complete or not, and we expect that response may be multiplexed 
      copy_cptr_to_stream(cptr, sptr);
      return bytes_handled;  
    } else {
      // parse_continuation_frame ; 
      NSDL2_HTTP2(NULL, NULL, "CONTINUATION FRAME IS EXPECTED");
      if (frame_length > (cptr->http2->continuation_max_buf_len - cptr->http2->continuation_buf_len))
      {
        MY_REALLOC(cptr->http2->continuation_buf, cptr->http2->continuation_max_buf_len + frame_length, "cptr->http2->continuation_buf", -1);
        cptr->http2->continuation_max_buf_len += frame_length;
      }
 
      MY_MEMCPY(cptr->http2->continuation_buf + cptr->http2->continuation_buf_len, cptr->http2->continuation_buf, frame_length);
      cptr->http2->continuation_buf_len += frame_length;
      cptr->tcp_bytes_recv += frame_length;
      NSDL2_HTTP2(NULL, NULL, "cptr->http2->continuation_buf_len = %d cptr->tcp_bytes_recv = %d",
                                         cptr->http2->continuation_buf_len, cptr->tcp_bytes_recv);
      return frame_length; 
    }
  }
  else
  { //Pushed resources may contain content type videos/mp3 , will not store their data (costly realloc)
    //Do we need to store these headers or we can just log them in rep file ?
    if(!runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push){
      NSDL2_HTTP2(NULL, NULL, "Pushed Header Frames while setting_enable_push was disabled, hence ignoring this frame");
      pack_goaway(cptr, stream_id, PROTOCOL_ERROR, "Received pushed frames from peer even when settings_enable_push was disabled", now);
      return HTTP2_ERROR;
    }
    uint8_t *in = (uint8_t*) in_buffer;
    size_t inlen = (size_t)frame_length;
    size_t rv;
    int idx = (stream_id /2) - 1; 
    /*bug 86575: align index and reset header count*/
    NS_H2_ALIGN_IDX(stream_id)
    NSDL2_HTTP2(NULL, NULL, "header_count = %d", cptr->http2->promise_buf[idx].header_count);
    cptr->http2->promise_buf[idx].header_count = 0;
    int i = 0; 
   
    NSDL2_HTTP2(NULL, NULL, "Pushed Response Headers, stream_id = %d, now header_count=%d", stream_id, cptr->http2->promise_buf[idx].header_count);
    //NS_DT4(NULL, NULL, DM_L1, MM_HTTP, "Pushed Response Headers, stream_id = %d", stream_id);
    for (;;) { 
      nghttp2_nv nv;
      int inflate_flags = 0;
      size_t proclen;
  
      rv = nghttp2_hd_inflate_hd(cptr->http2->inflater, &nv, &inflate_flags, in, inlen, 1);
  
      if (rv < 0) {
        fprintf(stderr, "inflate failed with error code %zd", rv);
        pack_goaway(cptr, stream_id, COMPRESSION_ERROR, "Received COMPRESSION_ERROR from peer", now);
        return -1;
      }
  
      proclen = (size_t)rv;
  
      in += proclen;
      inlen -= proclen;
      
      if(cptr->http2->promise_buf[idx].header_count == cptr->http2->promise_buf[idx].header_count_max)
      {
        MY_REALLOC(cptr->http2->promise_buf[idx].response_headers, sizeof(nghttp2_nv)*(cptr->http2->promise_buf[idx].header_count_max + NUM_HEADERS_IN_PROMISE_BUF_DELTA),"cptr->http2->promise_buf[idx].response_headers", -1);
        cptr->http2->promise_buf[idx].header_count_max += NUM_HEADERS_IN_PROMISE_BUF_DELTA; 
      }   

      if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
        
        if(nv.namelen + 1 <= MAX_HEADER_NAME_LEN) 
          memcpy(cptr->http2->promise_buf[idx].response_headers[i].name, nv.name, nv.namelen +1);
        else{
          MY_REALLOC(cptr->http2->promise_buf[idx].response_headers[i].name, nv.namelen +1,"cptr->http2->promise_buf[idx].response_headers", -1); 
          memcpy(cptr->http2->promise_buf[idx].response_headers[i].name, nv.name, nv.namelen +1);
        }
        cptr->http2->promise_buf[idx].response_headers[i].namelen = nv.namelen;
        if(nv.namelen + 1 <= MAX_HEADER_VALUE_LEN)
          memcpy(cptr->http2->promise_buf[idx].response_headers[i].value, nv.value, nv.valuelen +1);
        else{
          MY_REALLOC(cptr->http2->promise_buf[idx].response_headers[i].value, nv.valuelen +1,"cptr->http2->promise_buf[idx].response_headers", -1); 
          memcpy(cptr->http2->promise_buf[idx].response_headers[i].value, nv.value, nv.valuelen +1);
        }
        cptr->http2->promise_buf[idx].response_headers[i].valuelen = nv.valuelen;
        cptr->http2->promise_buf[idx].response_headers[i].flags = nv.flags;
        NSDL2_HTTP2(NULL, NULL, "%s : %s", nv.name, nv.value);
       // NS_DT4(NULL, NULL, DM_L1, MM_HTTP, "%s : %s", nv.name, nv.value);
        cptr->http2->promise_buf[idx].header_count++;
        i++;
      }
      if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
        nghttp2_hd_inflate_end_headers(cptr->http2->inflater);
        break;
      }
      if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
        break;
      }
     }

    NSDL2_HTTP2(NULL, NULL, "Pushed Response Headers Count = %hi for promised_stream_id = %d", cptr->http2->promise_buf[idx].header_count, stream_id);
    return frame_length;
  }  
}

/******************************************************************************************************
  Purpose: This method is called from ns_h2_process_resp.c, this is called for all the caching headers
           (last-modified, date, cache-control etc.)
           present in the response.
  Input  : 
           cache_buffer: It contains the nv.value(value of caching headers)
           cache_buffer_len: This is the length of the value of the caching headers.
           cache-header: This is the type of cache header.
  Output:  It calls cache_headers_parse_set method, which parse and save the headers value received. 
*******************************************************************************************************/
void http2_cache_save_header(connection *cptr, char *cache_buffer, int cache_buffer_len, cacheHeaders_et cache_header)
{
  CacheResponseHeader *crh;
  CacheTable_t *cacheptr;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_CACHE(NULL, cptr, "Method called, cache_buffer=%*.*s, cache_buffer_len=%d", cache_buffer_len, cache_buffer_len, cache_buffer, cache_buffer_len);

  //Check if user is enabled for cache
  if(vptr->flags & NS_CACHING_ON)
  { 
    cacheptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    NSDL3_CACHE(NULL, cptr, "in cache save header vptr->flags is NS_CACHING_ON");
    //Checking the cache flag for cacheability -- check for bit 0
    if(!(cacheptr->cache_flags & NS_CACHE_ENTRY_IS_CACHABLE))
    {
      NSDL2_CACHE(NULL, cptr, "NOT CACHEABLE url=%s, Not Processing Further Cacheable      \
                                      Headers as response is not cacheable", cacheptr->url);
      return ;
    }
  }
  else
  { 
    NSDL2_CACHE(NULL, cptr, "Caching Disabled for User");
    return ;
  }

  crh = ((CacheTable_t *)(cptr->cptr_data->cache_data))->cache_resp_hdr;

  if(NULL == crh->partial_hdr)
  {
    NSDL3_CACHE(NULL, cptr, "Complete cache header line received in one read");
    cache_headers_parse_set(cache_buffer, cache_buffer_len, cptr, cache_header);
  }

}
void
copy_to_sptr_buf(stream* sptr, char* buffer, int length, int total_size ) {
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer;
  int start_buf = 0;

  NSDL2_HTTP(NULL, NULL, "Method called. length = %d, total_size = %d, url = %s",
                          length, total_size, sptr->url);
  if (!total_size)
    start_buf = 1;
    
  while (length) {
    copy_offset = total_size % COPY_BUFFER_LENGTH;
    copy_length = COPY_BUFFER_LENGTH - copy_offset;

    if (!copy_offset) {
      MY_MALLOC(new_buffer, sizeof(struct copy_buffer), "new copy buffer", -1);
      if (new_buffer) {
        new_buffer->next = NULL;
        if (start_buf) {
          sptr->buf_head = sptr->cur_buf = new_buffer;
          start_buf = 0;
        }
        else {
          sptr->cur_buf->next = new_buffer;
          sptr->cur_buf = new_buffer;
        }
      }
    }
    if (length < copy_length)
    copy_length = length;
    memcpy(sptr->cur_buf->buffer+copy_offset, buffer, copy_length);
    total_size += copy_length;
    length -= copy_length;
    buffer += copy_length;
  }
}

/*-----------------------------------------------------------------------------------
Input   :  cptr , payload buffer , length , flags and stream .   
Purpose :  This function processs payload for data frame and dump resp body . 
            
Output  :  Returns bytes_processed for a payload 
-------------------------------------------------------------------------------------*/
int process_data_frame(connection *cptr, unsigned char *in_buff, int frame_length, unsigned char flags, unsigned int stream_id, u_ns_ts_t now){

  int processed_length = 0;
  int pad_length;
  int tot_len;
  stream *sptr = NULL;
  VUser *vptr = (VUser *)cptr->vptr;
  //int copy_to_sptr_buf_flag =0;
 
  // Check if frame length is greater than max_frame_size. In this case we will close stream.
  NSDL2_HTTP2(NULL, NULL, "Method caled. Frame length is %d, flags is %x", frame_length, flags);

  if (stream_id == 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Stream id Cannot be 0x0 for any other frame except settings. Hence sending goaway");
    pack_goaway(cptr, stream_id, PROTOCOL_ERROR, "PROTOCOL ERROR", now);
    return HTTP2_ERROR;
  }

  if(stream_id %2 !=0)
  {
    if(stream_id >1){
      sptr = get_stream(cptr, stream_id); 
      if(sptr)
        copy_stream_to_cptr(cptr, sptr);
      else{
        NSDL2_HTTP2(NULL, NULL, "Stream is not found in map, This should not happen, Returning from here");
        return HTTP2_ERROR;
      }
    }
    // Check if Padding is set. We will always ignore Padding.
    if (CHECK_FLAG(flags, PADDING_SET)) {
      pad_length = (int)in_buff[0] ;
      NSDL2_HTTP2(NULL, NULL, "Padding is set pad length is %d", pad_length);
      tot_len =  frame_length - pad_length -1;
      in_buff++;  
    }
    else 
      tot_len = frame_length; 

    NSDL2_HTTP2(NULL, NULL, "tot_len is %d", tot_len);
#if 0
    //dump resp body
    #ifdef NS_DEBUG_ON
      if(tot_len > 0){
        //debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
        //debug_log_http2_res(cptr,(unsigned char *)cptr->url, cptr->url_len);
        //debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
        //debug_log_http2_res(cptr, in_buff, tot_len);
      }
    #else
    if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
    { 
      if(tot_len > 0 ){
        //log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
        //log_http2_res(cptr, vptr, (unsigned char *)cptr->url, cptr->url_len);
        //log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
        //log_http2_res_ex(cptr, (unsigned char *)new_line, 1);
        //log_http2_res_ex(cptr, (unsigned char *)cptr->url, cptr->url_len);
        //log_http2_res_ex(cptr, (unsigned char *)new_line, 1); 
        //log_http2_res_ex(cptr, in_buff, tot_len);
      }
    }
    #endif
#endif

    //Storing Main response body in cptr->cur_buf
    int ret = copy_data_to_full_buffer(cptr);
    if (ret) {
      if(cptr->url && cptr->url_num->proto.http.type == MAIN_URL) 
        copy_retrieve_data(cptr, (char *)in_buff, tot_len, cptr->total_bytes); 
    }    
    cptr->total_bytes += tot_len;
    cptr->bytes += tot_len;
    cptr->tcp_bytes_recv += tot_len; 

    NSDL2_HTTP2(NULL, NULL, "cptr->total_bytes is %d cptr->bytes = %d cptr->tcp_bytes_recv = %d", 
                             cptr->total_bytes, cptr->bytes, cptr->tcp_bytes_recv);

     
    average_time->total_bytes += frame_length; /* bug 78135 - updated to fill data regarding body throughput */
    if (SHOW_GRP_DATA) {
      VUser *vptr = cptr->vptr;
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->total_bytes += frame_length; /* bug 78135 - updated to fill data regarding body throughput for group */
    }

    processed_length += frame_length;
 
    copy_cptr_to_stream(cptr, sptr);
    
    NSDL2_HTTP2(NULL, NULL, "Process Flow Control Frame");
    //Data frame received on stream
    if(sptr != NULL)
      ns_process_flow_control_frame(cptr, sptr, processed_length, now);
    
    // Here stream is in half-close local, and it got a frame with end-stream flag set, henece closing this stream
    // TODO : replace all hex values by macros and look at this code again
    if (CHECK_FLAG(flags, END_STREAM_SET)) {
     //In case this was unset on receiving push promise frames,response of stream_id on which they were pushed is received,set this
      cptr->http2->main_resp_received = 1;
      if (sptr != NULL)
        RELEASE_STREAM(cptr, sptr);
    }       
  
    return processed_length;
  } 
  else
  { 
    if(!runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push){
      NSDL2_HTTP2(NULL, NULL, "Pushed Data Frames while setting_enable_push was disabled, hence ignoring this frame");
      pack_goaway(cptr, stream_id, PROTOCOL_ERROR, "Received pushed frames from peer even when settings_enable_push was disabled", now);
      return HTTP2_ERROR;
    }
    int idx = (stream_id /2) -1; 
    NS_H2_ALIGN_IDX(stream_id) 
   //There will be a check for type of data also as there is no point of storing data of type mp3/mp4/img if pushed (Second phase)
    if(frame_length){ 
      
      if((cptr->http2->promise_buf[idx].data_offset + frame_length) >= cptr->http2->promise_buf[idx].data_offset_max){ 
        cptr->http2->promise_buf[idx].data_offset_max += (SERVER_PUSH_DATA_DELTA + frame_length);
        MY_REALLOC(cptr->http2->promise_buf[idx].response_data, cptr->http2->promise_buf[idx].data_offset_max, "cptr->http2->promise_buf[].response_data", -1);
      }
        memcpy(cptr->http2->promise_buf[idx].response_data + cptr->http2->promise_buf[idx].data_offset , in_buff, frame_length);
        cptr->http2->promise_buf[idx].data_offset += frame_length;
    }
     //This response is complete hence decrementing  cptr->http2->promise_count
    if (CHECK_FLAG(flags, END_STREAM_SET)) {  
      cptr->http2->curr_promise_count--;
      cptr->http2->promise_buf[idx].free_flag = 1;// that response was complete for this stream
    }
    return frame_length;
  }
}

/*------------------------------------------------------------------------------------- 
ANUBHAV
Input   :  cptr , payload buffer , length , flags and stream .   

Purpose :  This function processs payload for window update frame received and sets value either in 
           cptr or stream . 
           if (stream_id == 0 ) then set incremental_window_size received are set in cptr , 
           if (stream_id != 0 ) , then we set value of incremental_window_size  in stream ; 

Output  :  Returns bytes_processed for a payload 
---------------------------------------------------------------------------------------*/
int process_window_update_frame(connection *cptr , unsigned char *in_buffer , int frame_length, unsigned int stream_id, u_ns_ts_t now)
{
  unsigned int window_inc_size;
  int offset = 0;
  NSDL2_HTTP2(NULL, NULL, "Method called"); 
  COPY_BYTES_TO_INTEGER(in_buffer, &window_inc_size);
  // check for Stream identifier . If sid is 0 than the value incremental value in connection else it will be applicable for stream only
  if (stream_id == 0)  // Connection specific 
  {
    cptr->http2->flow_control.remote_window_size += window_inc_size;   
    NSDL2_HTTP2(NULL, NULL,"cptr->http2->flow_control.remote_window_size [%d]",cptr->http2->flow_control.remote_window_size);
    /*bytes left to be send on connection, not taking cptr->bytes_left_to_send as it is set to 0 in on_request_write_done()*/
    /*bug 93672: gRPC: call handle_write only when cptr->bytes_left_to_send > 0 */
    if((cptr->http2->flow_control.data_left_to_send > 0) && (cptr->bytes_left_to_send > 0) )
    {
      /*bug 89702: call h2_handle_write_ex()*/
      if((h2_handle_write_ex(cptr, now)) < HTTP2_SUCCESS)
        return HTTP2_ERROR;
    }
  }
  else {
    stream *sptr = get_stream(cptr, stream_id);
    if (sptr) {
      NSDL2_HTTP2(NULL, NULL,"Before sptr->flow_control.remote_window_size [%d] settings_max_frame_size=%d",
					 sptr->flow_control.remote_window_size,cptr->http2->settings_frame.settings_max_frame_size);
      /*bug 84661: get header count based on frame size received from serverxe*/
      unsigned short int header_frame_count = window_inc_size/cptr->http2->settings_frame.settings_max_frame_size;
      if(window_inc_size%cptr->http2->settings_frame.settings_max_frame_size)
        ++header_frame_count;
      /*add header size as well*/
      sptr->flow_control.remote_window_size += (window_inc_size + (header_frame_count*HEADER_FRAME_SIZE) );
      /*bug 84661: updated window_update_from_server size*/
      sptr->window_update_from_server += (window_inc_size + (header_frame_count*HEADER_FRAME_SIZE));
      NSDL2_HTTP2(NULL, NULL,"After sptr->flow_control.remote_window_size [%d]  window_inc_size = %d window_update_from_server =%d cptr->request_type=%d", sptr->flow_control.remote_window_size,window_inc_size, sptr->window_update_from_server, cptr->request_type);
      if(sptr->bytes_left_to_send > 0)
      { /*bug 89702: call h2_handle_write_ex()*/
        if((h2_handle_write_ex(cptr, now)) < HTTP2_SUCCESS)
          return HTTP2_ERROR;
      }
   }
  }
  offset += 4;
  return offset;
}


/*------------------------------------------------------------------------
Input   :  cptr , payload buffer , length , flags and stream .   

Purpose :  This function processs payload for settings received and sets value in cptr
           The payload of a SETTINGS frame consists of zero or more parameters.           
           SETTINGS frames always apply to a connection, never a single stream.

Output  :  Returns bytes_processed for a payload.

--------------------------------------------------------------------------*/
int process_settings_frame(connection *cptr, unsigned char *in_buffer, int frame_length, unsigned char flag, unsigned int stream_id ,u_ns_ts_t now) {

  int i, offset = 0;
  unsigned short identifier;
  unsigned int value = 0;
  unsigned char *tmp_value = NULL;
  int ret = 0;
  NSDL2_HTTP2(NULL, cptr, "Method Called sid is [%d] and in_buffer is %x", stream_id, in_buffer);
  // Check whether stream id is 0 or not  
  if (stream_id != 0) {
    fprintf(stderr,"Protocol Error : Stream Identifier for connection must be 0. \n");
    NSDL2_HTTP2(NULL, cptr,"Protocol Error : Stream Identifier for connection must be 0. \n");
    return HTTP2_ERROR;
  }
  // Check if ack is set . This means that server agree with  the client settings      
  if ((CHECK_FLAG(flag, END_STREAM_SET)))
  {
    NSDL2_HTTP2( NULL, cptr , "Since flag is SETTINGS ACKNOWLEDGE. We expect empty settings frames ");
    /*bug 93672: gRPC- set http2_state to HTTP2_SETTINGS_ACK*/
    cptr->http2_state = HTTP2_SETTINGS_ACK;
    NSDL2_HTTP2(NULL, NULL,"cptr->http2_state=%d", HTTP2_SETTINGS_ACK);
    // In this case frame length is empty
    if (frame_length != 0 )
    {
      NSDL2_HTTP2(NULL, NULL, "ERROR: we expect an empty settings frame in this case \n "); 
      return HTTP2_ERROR;
    }
    /* There can be 2 possibilities for receiving ack 
      1) Server send(s) ack after publishing its setting(S) to client
      2) Either server sent ack immediately after receiving client settings 
    */
    if ((cptr->http2_state == HTTP2_SETTINGS_DONE) && !(cptr->flags & NS_HTTP2_SETTINGS_ALREADY_RECV)) {
      cptr->http2_state = HTTP2_SETTINGS_ACK;
      if (cptr->flags & NS_HTTP2_UPGRADE_DONE) {
        cptr->http2_state = HTTP2_HANDSHAKE_DONE;
        NSDL2_HTTP2(NULL, NULL, "In this we will mark state to HANDSHAKE DONE");
      }
      /*bug 52092 : check if SETTING ACK has been recieved from PROXY server, in response to PRI + SETTING from client, */
      /* and then make PROXY CONNECT request and send accordingly */
      if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)
        make_proxy_and_send_connect(cptr,cptr->vptr, now);
    }
    offset += frame_length;  
  }
  else 
  {
    for(i= 0; i < frame_length; i += 6)
    {
      NSDL2_HTTP2(NULL, cptr, "offset = %d ", offset);
      tmp_value = in_buffer +offset ;
      COPY_BYTES_TO_SHORT(tmp_value, &identifier);

      NSDL2_HTTP2(NULL, cptr, " settings identifier is  = %d ", identifier);
      offset += 2;

      tmp_value = in_buffer + offset ;
       
      if (identifier== SETTINGS_HEADER_TABLE_SIZE){  
        COPY_BYTES_TO_INTEGER(tmp_value , &value);
        cptr->http2->settings_frame.settings_header_table_size = value; 
      } else if(identifier == SETTINGS_ENABLE_PUSH) {
        COPY_BYTES_TO_INTEGER(tmp_value , &value);
        cptr->http2->settings_frame.settings_enable_push = value;
      } else if(identifier == SETTINGS_MAX_CONCURRENT_STREAMS) {
        COPY_BYTES_TO_INTEGER(tmp_value , &value);
        NSDL2_HTTP2(NULL, cptr, "Recieved Max Concurrent Stream value =%d", value);
        //cptr->http2->settings_frame->settings_max_concurrent_streams= value;
        //setting it to 20 for testing purpose only . TODO change it later
       /*bug 93672:gRPC: as server can send settings_max_concurrent_streams up to 2147483647, so
         set the value to MAX_CONCURRENT_STREAM in case it is > MAX_CONCURRENT_STREAM or 0*/
        if( !value || (value > MAX_CONCURRENT_STREAM))
          cptr->http2->settings_frame.settings_max_concurrent_streams = MAX_CONCURRENT_STREAM;
        else
          cptr->http2->settings_frame.settings_max_concurrent_streams = value;
        NSDL2_HTTP2(NULL,cptr, " cptr->http2->settings_frame.settings_max_concurrent_streams set to =%d",  cptr->http2->settings_frame.settings_max_concurrent_streams);
      } else if(identifier== SETTINGS_INITIAL_WINDOW_SIZE) {
	COPY_BYTES_TO_INTEGER(tmp_value , &value);
        cptr->http2->settings_frame.settings_initial_window_size = value;
      } else if(identifier == SETTINGS_MAX_FRAME_SIZE) {
        COPY_BYTES_TO_INTEGER(tmp_value , &value);
        cptr->http2->settings_frame.settings_max_frame_size = value;
      } else if(identifier == SETTINGS_MAX_HEADER_LIST_SIZE){
        COPY_BYTES_TO_INTEGER(tmp_value , &value);
        cptr->http2->settings_frame.settings_max_header_list_size = value;
      } else {
        NSDL2_HTTP2(NULL, NULL, "Unknown Identifier for Settings Frame");
      }
      offset += 4;
    
      NSDL2_HTTP2(NULL, cptr, "offset = %d ", offset);
      NSDL2_HTTP2(NULL, cptr, "identifier for settings frame is [%d]  and value for settings frame is [%d] ", identifier, value);
    } 
  }

  NSDL2_HTTP2(NULL, cptr, "cptr->http2_state = %d", cptr->http2_state);
  if (cptr->http2_state == HTTP2_SETTINGS_SENT){
    cptr->http2_state = HTTP2_SETTINGS_DONE;

    NSDL2_HTTP2(NULL, cptr, "Settings received from Peer. Therefore sending acknowledgement to server. Now cptr->http2_state is [%d] ",
                             cptr->http2_state);
    // Init map here as we know both max streams here 
    init_stream_map_http2(cptr);
    ret = http2_make_handshake_frames(cptr, now);
    if (ret == HTTP2_ERROR)
      return HTTP2_ERROR;
  }
  NSDL2_HTTP2(NULL, cptr, "identifier for settings frame is [%d]  and value for settings frame is [%d] ", identifier, value);
  return offset; 
}

/*-----------------------------------------------------------------------------------------------
Purpose : This function Process frame payload (once completely received ) according to frame 
          received .
          This Function receives in  only payload for frame . 
-------------------------------------------------------------------------------------------------*/

int process_frame_payload(connection *cptr, unsigned char *frame_payload,int bytes_read ,unsigned int frame_length, unsigned int stream_id, unsigned char flags ,unsigned char frame_type, u_ns_ts_t now)
{
  int bytes_handled =0 ;

  NSDL2_HTTP2(NULL, NULL, "Method Called, frame_type  = %x, flags = %x, frame_len = %d frame_payload=%p", frame_type, flags, frame_length, frame_payload); 
  switch (frame_type) {

    case SETTINGS_FRAME :
      NSDL2_HTTP2(NULL, NULL, "SETTINGS_FRAME found going to parse it"); 
      bytes_handled = process_settings_frame(cptr, frame_payload , frame_length, flags, stream_id, now);
      return bytes_handled;

    case WINDOW_UPDATE_FRAME : 
      NSDL2_HTTP2(NULL, NULL, "WINDOW_UPDATE_FRAME found going to parse it"); 
      bytes_handled = process_window_update_frame(cptr, frame_payload, frame_length, stream_id, now);
      return bytes_handled;

    case DATA_FRAME :
       bytes_handled = process_data_frame(cptr, frame_payload, frame_length, flags , stream_id, now);
       return bytes_handled;

    case HEADER_FRAME : 
       bytes_handled = process_header_frame(cptr, frame_payload, frame_length, flags, stream_id, frame_type, now);
       return bytes_handled;

    case CONTINUATION_FRAME : 
       bytes_handled = process_header_frame(cptr, frame_payload, frame_length, flags, stream_id, frame_type, now);
      return bytes_handled;

    case GOAWAY_FRAME :
      NSDL2_HTTP2(NULL, NULL, "Received goaway frame ");
      bytes_handled = process_goaway_frame(cptr, frame_payload , frame_length , now);
      return bytes_handled;

    case RESET_FRAME : 
      bytes_handled = process_reset_frame(cptr, frame_payload, frame_length, flags , stream_id, now);  
     return bytes_handled; 

    case PING_FRAME:
      NSDL2_HTTP2(NULL, cptr, "Received Ping Frame");
      bytes_handled = process_ping_frame(cptr, frame_payload, frame_length, flags , stream_id, now);
      return bytes_handled;

    case PUSH_PROMISE:
     NSDL2_HTTP2(NULL, cptr, "Received Push Promise frame");
     bytes_handled = process_push_promise_frame(cptr, frame_payload, frame_length, flags, stream_id, frame_type, now);
     return bytes_handled;

   case PRIORITY_FRAME: 
     fprintf(stderr, "Received Priority Frame. Ignoring for now \n");
     return frame_length;
 
    default :
     NSTL4(NULL, cptr, "Invalid frame_type %x and frame_length is %d ", frame_type, frame_length);
     //fprintf(stderr, "Invalid frame_type %x and frame_length is %d \n", frame_type, frame_length);
     return HTTP2_ERROR;
  } 
  //cptr->req_code_filled = 0;
  return HTTP2_SUCCESS;
}

/*---------------------------------------------------------------
Function calculates stream id received in buffer 
-----------------------------------------------------------------*/
 
int process_stream_id(connection *cptr, unsigned char *buff){
  unsigned char stream_id[4];
  unsigned int sid;

  // COPY stream id from buff  
  MY_MEMCPY(stream_id, buff, SID_LEN);
  int i;
  for(i = 0; i < 4; i++ ){
    NSDL2_HTTP2(NULL, NULL, "%dth byte = %x", i, stream_id[i]);
  }

  COPY_BYTES_TO_INTEGER(stream_id, &sid);
  NSDL2_HTTP2(NULL, NULL, "sid = %d for cptr %p", sid, cptr);
  return sid;
}

/*---------------------------------------------------------------------------------
Input   :  This function receives unprocessed response in in_buffer .

Purpose :  This Function process frame header which is common to all received 
           frames in http2.
           Frame Header Contains : payload length , frame type , flags (if set . Some frames donot support any 
           flags)  and stream_identifier . 
          
Return type : HTTP2_SUCCESS / ERROR . 
              Error in case if something is unusual .  
-----------------------------------------------------------------------------------*/

int process_frame_header(connection *cptr, unsigned char *in_buffer, unsigned char *frame_type, unsigned int *stream_id, unsigned char *frame_flags )
{
  unsigned char frame; 
  int offset = 0; 
 
  NSDL2_HTTP2(NULL, NULL, "Method called, partial buff len = %d", cptr->http2->partial_buff_size);
  // *frame_length = process_frame_length(cptr, in_buffer);
  // check frame type . Frame type will be set in 4th byte of buffer
  frame = in_buffer[0];
  offset += 1;
    
  // validate frame type TODO check for error condition 
  *frame_type = check_frame_type(frame);
  // Here we will simply check and copy flag type in frame_flag .  We will procress flags while parsing  
  *frame_flags = in_buffer[1]; 
  offset+= 1 ;  
  // Parse stream id
  *stream_id = process_stream_id(cptr, in_buffer + offset);   
  offset += 4 ; 

  NSDL2_HTTP2(NULL, NULL,"offset=%d", offset);
 /* if (*frame_type == INVALID_FRAME)
    pack_reset(cptr, "0x1");
 */
 //offset += 3; // Three bytes for frame length 
 return offset;
}

// Return 0 : Success and -1: Failure
int reset_idle_connection_timer_http2(connection *cptr, u_ns_ts_t now, stream *sptr)
{
  NSDL2_HTTP2(NULL, cptr, "Method called cptr=0x%lx", (u_ns_ptr_t)cptr);
  VUser *vptr = cptr->vptr;
  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;
  PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

  GroupSettings *gset = &runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset;

  int elaps_response_time, next_idle_time ;

  elaps_response_time = next_idle_time = 0;

  if(runprof_table_shr_mem[vptr->group_num].page_reload_table)
    pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number];

  if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];

  if(pagereload_ptr || pageclickaway_ptr) return 0;

  NSDL2_HTTP2(NULL, cptr, "gset->idle_secs = %d cptr->timer_type = %d", gset->idle_secs, cptr->timer_ptr->timer_type);

  if(cptr->timer_ptr->timer_type == -1)
    cptr->timer_ptr->timer_type = AB_TIMEOUT_IDLE;
  
  if((gset->idle_secs > 0) && cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE) /* Safety */
  {
    NSDL2_HTTP2(NULL, cptr, "cptr=0x%lx Resetting timer for idle connection", (u_ns_ptr_t)cptr);

    dis_idle_timer_reset( now, cptr->timer_ptr); /*bug 68086 : moved reset timer up */
    //When Response time is greater then response_timer than setting timeOut and closing connection
    if(sptr){
      elaps_response_time = now - sptr->request_sent_timestamp;
      NSDL2_HTTP2(NULL, cptr, "stream_id = %d elaps_response_time = %d now = %llu sptr->request_sent_timestamp %llu",
                            sptr->stream_id, elaps_response_time, now, sptr->request_sent_timestamp);
    }
    else{
      //For stream_id = 1
      elaps_response_time = now - cptr->request_sent_timestamp;
      NSDL2_HTTP2(NULL, cptr, "stream_id = 1, elaps_response_time = %d now = %llu cptr->request_sent_timestamp %llu",
                               elaps_response_time, now, cptr->request_sent_timestamp);
    }
    if ( elaps_response_time >= gset->response_timeout) {
      NSDL2_HTTP2(NULL, cptr, "response time exceeded response_timer for stream id = %d", sptr?sptr->stream_id:1);
      NSTL1(NULL, cptr, "Response Timeout for stream id = %d", sptr?sptr->stream_id:1);
      //sptr will be null only for stream_id 1
      if(!sptr){
        cptr->http2->stream_one_timeout = 1;  
      }
     return -1;
    }
  }

  return 0;
}
/*----------------------------------------------------------------
 Purpose : This function is used to send Reset Stream Frame 
           with specified error condition
-----------------------------------------------------------------*/
int pack_and_send_reset_frame(connection *cptr, stream *sptr, int error_code, u_ns_ts_t now)
{
  unsigned char *reset_frame;
  NSDL1_HTTP2(NULL, NULL, "Method called");
 
  MY_MALLOC_AND_MEMSET(reset_frame, 13, "reset_frame", -1);
  pack_reset(cptr, sptr, reset_frame, error_code);
  cptr->free_array = (char*)reset_frame;
  cptr->bytes_left_to_send = 13;
  cptr->req_code_filled = 0;
  cptr->conn_state = CNST_HTTP2_WRITING;
  cptr->http2_state = HTTP2_SEND_RESET_FRAME;
  return http2_handle_write(cptr, now);
}

/*----------------------------------------------------------------
 Purpose : This function is used to read data from secure HTTPS TCP connection
-----------------------------------------------------------------*/
int NSU_TCP_READ_HTTPS(connection *cptr, unsigned char *buf, int nbyte, u_ns_ts_t now)
{
   if( (cptr == NULL ) || (buf == NULL)  )
        return HTTP2_ERROR;

   if( nbyte <= 0)
        return HTTP2_ERROR;
    
   int  bytes_read = SSL_read(cptr->ssl, buf, nbyte);
   NSDL2_SSL(NULL, cptr, "Read %d bytes HTTPS cptr->ssl[%p] buf[%p] nbyte=%d", bytes_read,cptr->ssl,buf,nbyte);
      
   /*return is bytes_read > 0*/
   if (bytes_read > 0)
     return bytes_read;

   /*Error handling incase bytes_read <= 0*/
   int err = SSL_get_error(cptr->ssl, bytes_read);
   switch (err) {
       case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
         handle_server_close(cptr, now);
         return HTTP2_ERROR;
       case SSL_ERROR_WANT_READ:
         NSDL1_SSL(NULL, cptr, "SSL_ERROR_WANT_READ Error. cptr->http2_state = %d cptr->url_num=%p", cptr->http2_state, cptr->url_num);
	 HTTP2_HANDLE_CONNECT(cptr, now);
         return HTTP2_ERROR;
          /* It can but isn't supposed to happen */
       case SSL_ERROR_WANT_WRITE:
         fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");

       case SSL_ERROR_SYSCALL: //Some I/O error occurred
          if (errno == EAGAIN) { // no more data available, return (it is like SSL_ERROR_WANT_READ)
              NSDL1_SSL(NULL, cptr, "SSL_read: No more data available. cptr->http2_state = %d cptr->url_num=%p",
                                      cptr->http2_state, cptr->url_num);
	      HTTP2_HANDLE_CONNECT(cptr, now);
              return HTTP2_ERROR;
            }
            if (errno == EINTR)
            {
              NSDL3_SSL(NULL, cptr, "SSL_read interrupted. Continuing...");
              //continue;
	      return NSU_TCP_READ_HTTPS(cptr, buf,  nbyte, now );	
            }
            break;
            /* FALLTHRU */
       case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
            /* FALLTHRU */
            NSDL1_SSL(NULL, cptr, "SSL_ERROR_SSL Error");
       default:
            //ERR_print_errors_fp(ssl_logs);
            NSDL1_SSL(NULL, cptr, "Default Case Error");
            char *err_buff = ERR_error_string(ERR_get_error(), NULL);
            /*bug 79057 moved fprintf to log file*/
            NSTL2(NULL, cptr,"ssl[%p] library error[%d] bytes_read[%d] -->  %s",cptr->ssl, err,bytes_read,err_buff );
            NSDL1_SSL(NULL, cptr,"ssl[%p] library error[%d] bytes_read[%d] -->  %s",cptr->ssl, err,bytes_read,err_buff );
            if ((bytes_read == 0) && (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.ssl_clean_close_only))
              handle_server_close(cptr, now);
            //return HTTP2_ERROR;
        }
  return HTTP2_ERROR;
}


/*----------------------------------------------------------------
 Purpose : This function is used to read data from HTTP TCP connection
-----------------------------------------------------------------*/
int NSU_TCP_READ_HTTP(connection *cptr, unsigned char *buf, int nbyte, u_ns_ts_t now)
{
   if( (cptr == NULL ) || (buf == NULL)  )
        return HTTP2_ERROR;

    if( nbyte <= 0)
        return HTTP2_ERROR;
     
      int bytes_read = read(cptr->conn_fd, buf,nbyte);
      NSDL3_SSL(NULL, cptr, "Read %d bytes HTTP", bytes_read);
      
      if(bytes_read > 0)
        return bytes_read;
      else if (bytes_read == 0) {
        handle_server_close(cptr, now);
        return HTTP2_ERROR;
      }
        switch(errno)
        {
          case EAGAIN:
             NSDL2_HTTP2(NULL, NULL, "Got EAGAIN while reading. cptr->http2_state = %d cptr->url_num=%p", cptr->http2_state, cptr->url_num);
	     /*bug 52092: avoid calling below handle connect-->http2_make_request() in case HTTP2_SETTINGS_ACK is done for PROXY connect*/
             if(!(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT))
               HTTP2_HANDLE_CONNECT(cptr, now);
             return HTTP2_ERROR;
          case EINTR:
             NSDL2_HTTP2(NULL, NULL, "Got EINTR while reading");
             return HTTP2_ERROR;
          default:
              NSDL1_HTTP2(NULL, cptr, "cptr bytes_read = %d. Hence going to close connection", bytes_read);
              handle_server_close(cptr, now);
             return HTTP2_ERROR;
        }
}

/*----------------------------------------------------------------
 Purpose : This function is used to read data from TCP connection
 (both HTTP and HTTPS)
-----------------------------------------------------------------*/
int NSU_TCP_READ(connection *cptr, unsigned char *buf, int nbyte, u_ns_ts_t now )
{

    if( (cptr == NULL ) || (buf == NULL)  )
	return HTTP2_ERROR;

    if( nbyte <= 0)
	return HTTP2_ERROR; 
     
     int bytes_read; 
     
     switch(cptr->request_type)
     {
      case HTTPS_REQUEST:
          bytes_read =  NSU_TCP_READ_HTTPS(cptr, buf, nbyte,now);
          break;
      default:
          bytes_read =  NSU_TCP_READ_HTTP(cptr,buf,nbyte,now);
          break;
     } 
    return bytes_read; 
}
/*----------------------------------------------------------------
 Purpose : Function reads Http2 response from server 
           Generic for all frame types .  since HTTP2 frame 
           is divided into 9 byte fixed header octates + 
           frame payload .

           We will process Frame header once we have read complete 
           9 octates.
           Frame payload will be processed once completed reading 
           till frame_length as in first 3 bytes of frame header . 

           There can be three possiblitiy while reading 
           1)Complete read in 1 buffer. 
           2)Either frame header is partial and complete read for payload .  
           3)Frame header is complete but frame payload is partial 
 
------------------------------------------------------------------*/

int http2_handle_read(connection *cptr, u_ns_ts_t now) {

  int bytes_read = 0;
  unsigned char *frame_ptr = NULL;
  unsigned char buf[READ_BUFF_SIZE + 1] = "";
  unsigned char frame_type;
  unsigned char flags;
  unsigned int frame_length;
  int processed_len;
  unsigned int stream_id;
  int /*err, */ret = 0;
  int bytes_to_process = 0;
  NSDL1_HTTP2(NULL, cptr, "Method called cptr is %p", cptr);

  // Read till no more data available.
  while (1) {
    processed_len = 0;
    frame_ptr = NULL;

    now = get_ms_stamp();
    
    if( (bytes_read = NSU_TCP_READ(cptr, buf, READ_BUFF_SIZE, now ) ) == HTTP2_ERROR )
	return HTTP2_ERROR;
    
   /* handle partial read 
    1) check if we have read more than 9 bytes 
    2) if yes -> calculate frame length 
       if no  -> copy data to cptr, increment length cptr, continue reading 
    3) if bytes_read > frame_length, copy data to cptr, increment length in cptr, continue reading  
    4) once complete length is read , process frame header and frame payload       

   */

    // Update average time 
    //cptr->tcp_bytes_recv += bytes_read;
    average_time->rx_bytes += bytes_read;
    //average_time->total_bytes += bytes_read; /* bug 78135 - moved to Data Frame processing */
    VUser *vptr = cptr->vptr;
    if (SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->rx_bytes += bytes_read;
      //lol_average_time->total_bytes += bytes_read;  /* bug 78135 - moved to Data Frame processing */
    }
 
    NSDL2_HTTP2(NULL, NULL, "bytes_read = %d buf=%p", bytes_read, buf);
    //bytes_to_process = cptr->http2->partial_buff_size + bytes_read;
    bytes_to_process = bytes_read;

    NSDL2_HTTP2(NULL, NULL, "bytes_to_process = %d", bytes_to_process); 
    if (cptr->http2->partial_buff_size){
      NSDL2_HTTP2(NULL, NULL, "copy partial in cptr");
      COPY_PARTIAL_TO_CPTR(buf)
      bytes_to_process = cptr->http2->partial_buff_size;
      frame_ptr = cptr->http2->partial_read_buff;
      //is_copied = 1;
    } else {
      frame_ptr = buf;
    }
 
    while (bytes_to_process > 0) { // bytes_to_process != 0
      NSDL2_HTTP2(NULL, NULL, "bytes_to_process = %d frame_ptr=%p", bytes_to_process, frame_ptr); // Initially process length must be 0 . 
      if (bytes_to_process <  MAX_FRAME_HEADER_LEN) {
	 NSDL2_HTTP2(NULL, NULL, "Frame header is partial. Going to copy data to cptr");
        // Frame header is partial. We can not process until we have received complete frame header, hence copying partial data to cptr
        NSDL2_HTTP2(NULL, NULL, "copy partial in cptr and reset");
        COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
        break; // Break bytes process while loop 
	} 

        NSDL2_HTTP2(NULL, NULL, "Going to calculate frame length");
        ret =  process_frame_length(cptr, frame_ptr, &frame_length);
        if (ret < 3) {
          NSDL3_HTTP2(NULL, NULL, "Error in processing frame length.");
          NSDL2_HTTP2(NULL, NULL, "copy partial in cptr and reset");
	  COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
          return HTTP2_ERROR;
        }
        //NSDL2_HTTP2(NULL, NULL, "frame_length = [%d] and processed_len = [%d] ", frame_length, processed_len);
        // Once frame length is processed increment frame ptr by 3 .
        NSDL2_HTTP2(NULL, NULL, "bytes_to_process = %d, frame_length=%d , processed_length=%d frame_ptr=%p", bytes_to_process, frame_length, processed_len, frame_ptr); 
        
	if (bytes_to_process < (frame_length + 9)) {
	  // This means frame paylaod is partial.  We will neither process frame header nor frame paylaod until complete frame is received . 
          NSDL2_HTTP2(NULL, NULL, "Frame payload is partial for frame [%d], frame_length is [%d], bytes_to_process %d", 
                                   frame_type, frame_length, bytes_to_process);
          NSDL2_HTTP2(NULL, NULL, "copy partial in cptr and reset");
          COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
          break ;
	  }

          frame_ptr = frame_ptr + 3;  // Increment for length bytes as it is allready processed 
          NSDL2_HTTP2(NULL, cptr, "Complete frame received. Going to Process frame %p", frame_ptr);
          int offset = process_frame_header(cptr, frame_ptr, &frame_type, &stream_id, &flags);
          /*bug 93672: gRPC -> check for PRIORITY FLAG*/
 	  if (CHECK_FLAG(flags, PRIORITY_SET))
  	  {
    	     offset += PRIORITY_SET;/*5 bytes = 1[1 byte for Weight of Priority] + 4[4 bytes for stream id]*/
	     frame_length -= PRIORITY_SET;
    	     bytes_to_process -= PRIORITY_SET;
             NSDL2_HTTP2(NULL, NULL,"Priority Flag is set. now offset=%d frame_length=%d bytes_to_process=%d", offset, frame_length, bytes_to_process);
  	  }
          NSDL2_HTTP2(NULL, cptr, "offset=%d, stream_id=%d bytes_to_process=%d", offset, stream_id, bytes_to_process);
          cptr->http2->last_stream_id = stream_id;
          /*update frame_length in case of PRIORITY FLAG*/
          
          // After processing_frame_header (9 octets) we will increment processed_len by same. 
          processed_len += MAX_FRAME_HEADER_LEN; 
          bytes_to_process -= MAX_FRAME_HEADER_LEN;
          frame_ptr = frame_ptr + offset;
          NSDL2_HTTP2(NULL, NULL, "processed_len after processing frame_header is [%d] frame_ptr=%p bytes_to_process=%d", processed_len, frame_ptr, bytes_to_process);
          //Handling for G_IDLE_MSECS, ignore for Server Push     
          if((stream_id > 0) && ((stream_id % 2) != 0)){
            stream *stream_ptr = NULL;
            stream_ptr = get_stream(cptr, stream_id);
            /*This is to handle response after we timeout a particular request, we are discarding the response
            For mode 0 stream_id = 1 we donot create a new stream as request goes in HTTP1 so this get_stream() will return NULL 
            so for stream_id 1 response is to be discarded when it is timeout already */
            if((!stream_ptr && stream_id !=1) || (stream_id == 1 && cptr->http2->stream_one_timeout == 1)){
              bytes_to_process -= frame_length;
              frame_ptr = frame_ptr + frame_length;
              NSDL2_HTTP2(NULL, NULL, " Discarding response after timeout of frame_length %d bytes_to_process=%d", frame_length, bytes_to_process); 
              continue; //continue process further bytes
            }
            //Resseting the timer 
            if(reset_idle_connection_timer_http2(cptr, now, stream_ptr) < 0){
              if (stream_id > 1){
                copy_stream_to_cptr(cptr, stream_ptr);
                int loc_state = cptr->conn_state;
                pack_and_send_reset_frame(cptr, stream_ptr, CANCEL_STREAM, now);
                cptr->conn_state = loc_state;
              }
              bytes_to_process -= frame_length;
              NSDL2_HTTP2(NULL, NULL, "bytes_to_process=%d", bytes_to_process);
              frame_ptr = frame_ptr + frame_length;
       
              if(cptr->url_num->proto.http.type == EMBEDDED_URL){
                cptr->http2->donot_release_cptr = 1;
                Close_connection( cptr, 1, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
                if(cptr->http2->total_open_streams == 0){ // All stream for this connection is processed so return from here 
                  NSDL2_HTTP2(NULL, NULL, "Timeout :All streams closed, hence return from here");
                  COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
                  return 0;
                }
              }
              else{
                Close_connection( cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
                NSDL2_HTTP2(NULL, NULL, "copy partial in cptr");
                COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
                return 0;
              }
              continue; 
            }
          }
          // Process Frame payload
          int payload_read = process_frame_payload(cptr, frame_ptr, bytes_read, frame_length, stream_id, flags, frame_type, now); 
        
  	  if (payload_read == HTTP2_ERROR){

           NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
           __FILE__, (char*)__FUNCTION__,
           "closing connection because process_frame_payload has return error ."
           " cptr->http2_state %hd frame_type %x bytes_read %d, stream_id %u \n ",cptr->http2_state, frame_type, bytes_read, stream_id);
            stream *stream_ptr;
            if(stream_id >1 && stream_id %2 != 0) {
              stream_ptr = get_stream(cptr, stream_id);
              if(stream_ptr)
              {
                NSDL3_HTTP2(NULL, NULL, "Going to delete stream as received reset for that stream");
                copy_stream_to_cptr(cptr, stream_ptr);
                RELEASE_STREAM(cptr, stream_ptr);
              }
            }
            bytes_to_process -= frame_length;
            NSDL2_HTTP2(NULL, NULL, "copy partial in cptr. bytes_to_process=%d", bytes_to_process);
            frame_ptr = frame_ptr + frame_length;
            COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr) 
            return HTTP2_ERROR;
          } 
        
          processed_len += payload_read;
          bytes_to_process -= frame_length;
          NSDL2_HTTP2(NULL, NULL, "processed_len is  [%d] bytes_to_process=%d\n", processed_len, bytes_to_process);
          frame_ptr = frame_ptr + frame_length;
          
          /*bug 93672: gRPC- avoid Close_connection in case of ACK recieved for PING Frame*/
          // Here it is checking for end_stream flag and not settings frame, as some times setting frame may use end stream bit for ack
          if (((CHECK_FLAG(flags, END_STREAM_SET)) && IS_DATA_OR_HEADER_FRAME(frame_type) ) || (frame_type == RESET_FRAME))  {
            // This function sets referrer header 
            validate_req_code(cptr);
            NSDL2_HTTP2(NULL, NULL, "End Stream Flag set. Hence Closing Connection");
            if(cptr->cur_buf && (cptr->total_bytes > 0))
            { 
              debug_log_http2_res(cptr, (unsigned char *)"\n", 1);
              debug_log_http2_res(cptr, (unsigned char *)cptr->cur_buf->buffer, cptr->total_bytes);
            }
            cptr->http2->partial_buff_size = 0;
            // setting this cptr->req_code_filled as 0 , beacuse we have reached End stream. 
            // this is needed in case of update_http_status_code to update status code.
            cptr->req_code_filled = 0;
            int req_sts = get_req_status(cptr);
            if (cptr->flags & NS_HTTP2_UPGRADE_DONE) {
              cptr->flags  &= ~NS_HTTP2_UPGRADE_DONE;
            }
            
	    int open_streams = cptr->http2->total_open_streams; 
        
            if(!cptr->http2->curr_promise_count && cptr->http2->main_resp_received){ 
              
              Close_connection(cptr, 1, now, req_sts, NS_COMPLETION_CONTENT_LENGTH);
              // handle goaway frame here 
              if(open_streams == 0){ // All stream for this connection is processed so return from here 
                NSDL2_HTTP2(NULL, NULL, "All streams closed, hence return from here");
                 return 0;
              }
            }
          }
    } // End of frame parsing loop
    if (cptr->http2)
             cptr->http2->partial_buff_size = bytes_to_process; /*bug 54315 : reset buf size once complete frame is processed*/ 
    //processed_len = 0;
  } // End of read while loop
  //cptr->req_code_filled = -2; // Reset it


  NSDL2_HTTP2(NULL, NULL, "cptr->http2_state = %d cptr->url_num=%p", cptr->http2_state, cptr->url_num);
  HTTP2_HANDLE_CONNECT(cptr, now); 
  return 0;
}

/********************************************************************************
  This function will handle http2 frames if just received after 101 
  Switching protocols in single read. 

  client -------(upgrade request) ---------->     server 
  client <-------(101 Switching protocols / Server settings frame) ---- server)
  
************************************************************************************/
int http2_process_read_bytes(connection *cptr, u_ns_ts_t now, int len, unsigned char *buf){

  int processed_len = 0;
  unsigned int frame_length = 0; 
  unsigned char frame_type = 0;
  unsigned char flags = 0;
  unsigned int stream_id =0;
  //int bytes_to_handle = len;
  unsigned char *frame_ptr = buf;
  int bytes_to_process = len;
 
  NSDL1_HTTP2(NULL, NULL, "Method Called");
  // Initialise cptr for HTTP2 
  init_cptr_for_http2(cptr); 
  // We will calculated frame length first . Assuming we have read frame header
  while (bytes_to_process) {
    processed_len = process_frame_length(cptr, frame_ptr, &frame_length);
    //bytes_to_handle -= processed_len;
    frame_ptr = frame_ptr + processed_len;
    NSDL2_HTTP2(NULL, NULL, "frame length is [%d] and bytes_to_process = [%d]", frame_length, bytes_to_process);
    // Check if we have read complete frame 
    if (bytes_to_process >= frame_length) {
      // Complete frame is received 
      processed_len = process_frame_header(cptr, frame_ptr, &frame_type, &stream_id, &flags);
      frame_ptr = frame_ptr + processed_len;
      bytes_to_process -= MAX_FRAME_HEADER_LEN;
      NSDL2_HTTP2(NULL, NULL, "processed_len after processing frame_header  [%d]", (processed_len + 3)); // 3 bytes for frame length 
      // After processing_frame_header (9 octets) we will increment processed_len by same. 
      // Process Frame payload 
      processed_len = process_frame_payload(cptr, frame_ptr, len, frame_length, stream_id, flags, frame_type, now);
      if (processed_len == HTTP2_ERROR)
        return HTTP2_ERROR;
    
      if(frame_type == SETTINGS_FRAME)
        cptr->flags |= NS_HTTP2_SETTINGS_ALREADY_RECV;
 
      frame_ptr = frame_ptr + processed_len;
      bytes_to_process -= processed_len;
      // Decrementing tot_bytes_processsed from cptr->bytes because this is giving error in inflating gzip response from server.
      if (frame_type != DATA_FRAME){ 
        cptr->bytes -= processed_len + MAX_FRAME_HEADER_LEN;
        NSDL2_HTTP2(NULL, NULL, "cptr->bytes =  [%d] ", cptr->bytes);
      }
      NSDL2_HTTP2(NULL, NULL, "processed_len is [%d] processed bytes = [%d]", processed_len, bytes_to_process);
    
    } else {
      // copy partial frame to cptr->http2
      // We have not received complete frame.
      COPY_PARTIAL_TO_CPTR_AND_RESET(frame_ptr)
      cptr->flags |= NS_HTTP2_SETTINGS_ALREADY_RECV; 
      return 0;
    }
  }
  NSDL2_HTTP2(NULL, NULL, "bytes processed is  [%d]", bytes_to_process);
  if (bytes_to_process == 0) {
    // This means we have processed complete length
    cptr->flags |= NS_HTTP2_SETTINGS_ALREADY_RECV;
    return 0;
  }
  return 0;
}


/*HTTP2 HANDSHAKE PROCRESS 
For HTTP2 it is mandatory to send a connection preface followed by settings frame 
*/

/*
This Function init settings frame 
*/

int init_settings_frame(connection *cptr, unsigned char *settings_frames)
{
  unsigned int settings_frame_len = 0 ; 
  NSDL2_HTTP2(NULL, NULL, "Method Called");
 
  settings_frame_len = pack_settings_frames(cptr, settings_frames);
  if (settings_frame_len == -1) 
  {
    NSDL2_HTTP2(NULL, NULL, "ERROR: Unable to make settings frame  \n "); 
    return -1 ; 
  }
  // We will not set any while sending ack . This is because settings has been negotiated earlier 
  if (cptr->http2_state == HTTP2_SETTINGS_DONE)
    return settings_frame_len ;
 
  // set default value in cptr 
  cptr->http2->settings_frame.settings_max_concurrent_streams = 100;  
  cptr->http2->settings_frame.settings_max_frame_size = 16384; 
  cptr->http2->settings_frame.settings_header_table_size = 4096;
  cptr->http2->settings_frame.settings_enable_push = 0;
  cptr->http2->settings_frame.settings_initial_window_size = 65535;
  
  
  return settings_frame_len ; 
}


/*-------------------------------------------------------------
Function will handle HTTP2 SSL write . 
This Will write cptr->free array to server 
(Free Array  comprises of Connection preface and settings frame initially ) 
-------------------------------------------------------------- */

static int handle_http2_ssl_write(connection *cptr, u_ns_ts_t now)
{
  int i;
  char *ptr_ssl_buff;
  int bytes_left_to_send = cptr->bytes_left_to_send;
  //VUser *vptr = cptr->vptr;

  NSDL2_SSL(NULL, cptr, "Method called. tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  ptr_ssl_buff = cptr->free_array + cptr->total_pop3_scan_list;
 // ptr_ssl_buff = cptr->free_array ;

  ERR_clear_error();
  i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
  int err = SSL_get_error(cptr->ssl, i);
  switch (err)
  {
    case SSL_ERROR_NONE:  // Here we will check for for complete write or partial write
      cptr->total_pop3_scan_list += i;
      cptr->http_payload_sent += i;
      NSDL2_SSL(NULL, cptr, "total_bytes_written = %d", cptr->total_pop3_scan_list);
      if (i >= bytes_left_to_send)  // Complete write set cptr->bytes_left_to_send and return   
      {
        cptr->bytes_left_to_send -= i;
        NSDL2_SSL(NULL, cptr, "Data is written completely .Bytes_written = %d", i);
        break;
      }
      else { // IF We are here this means write is not complete
        NSDL2_HTTP2(NULL, NULL, "Partial write");
        cptr->bytes_left_to_send -= i;
        break;
      }
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_READ:
      #ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset);
      #endif
      return HTTP2_ERROR;
    case SSL_ERROR_ZERO_RETURN:
      /*bug 54315: moved tarce to log file*/
      NS_DT1(NULL, cptr, DM_L1, MM_HTTP2,"SSL_write error: aborted");
      NSDL2_HTTP2(NULL, NULL, "SSL_write error: aborted");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return HTTP2_ERROR;
    case SSL_ERROR_SSL:
      /*bug 54315 : moved error to debug log*/
      //ERR_print_errors_fp(stderr);
      #ifdef NS_DEBUG_ON
      NSDL1_SSL(NULL, cptr,"error-%d", SSL_ERROR_SSL);
      char *err_buff = ERR_error_string(ERR_get_error(), NULL);
      NS_DT1(NULL, cptr, DM_L1, MM_SSL,"SSL_write error: error=[%d] text=%s", err, err_buff);
      NSDL1_SSL(NULL, cptr," SSL_write error:[%d] text= %s", err, err_buff );
      #endif
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return HTTP2_ERROR;
    default:
      /*bug 54315: moved tarce to log file*/
      NS_DT1(NULL, cptr, DM_L1, MM_HTTP2,"SSL_write error: errno=%d", err);
      NSDL2_SSL(NULL, NULL, "SSL_write error: errno=%d", err);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return HTTP2_ERROR;

  }
  return 0;
}


/*------------------------------------------------------------
Purpose : This function write(s) HTTP2 handshake frames only.
          Handles partial write and close connections in case 
          of error . 
          
-------------------------------------------------------------*/
int http2_handle_write(connection *cptr, u_ns_ts_t now){

  int bytes_written = 0;
  int ret;
  char *write_buff; 

  write_buff = cptr->free_array + cptr->total_pop3_scan_list; 

  NSDL2_HTTP2(NULL, cptr, "Method Called bytes_left_to_send  are [%d]", cptr->bytes_left_to_send);

  while (cptr->bytes_left_to_send != 0) { 
    // Check if request type is HTTPS 
    if (cptr->request_type == HTTPS_REQUEST) {
      ret = handle_http2_ssl_write(cptr, now);
      if(ret == HTTP2_ERROR){
        return HTTP2_ERROR; 
      }
    } else {
      bytes_written = write(cptr->conn_fd, write_buff, cptr->bytes_left_to_send);
      // Check error cases
      if (bytes_written < 0) { 
        if (errno == EAGAIN) // No more data is available at this time, so return
          return HTTP2_ERROR;
        if (errno == EINTR) // In case of interrupt we will return 
          return HTTP2_ERROR;
        if ( errno != EAGAIN) {  // In this case we will try to write request again 
          NSDL2_HTTP2(NULL, NULL, "Retrying connection");
          retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
          return HTTP2_ERROR;
        }
      }
      if (bytes_written == 0) { // We will continue writing in this case 
        NSDL1_HTTP2(NULL, cptr, "Total byte written = %d . Hence continue reading", bytes_written);
        continue;
      }
      // Decrement bytes_left_to_send by bytes_written 
      cptr->bytes_left_to_send -= bytes_written;
      cptr->total_pop3_scan_list += bytes_written;  
      cptr->http_payload_sent += bytes_written;
    }  
  } 
  if (cptr->request_type == HTTP_REQUEST) //  Increment tcp_bytes_sent only here for http only. 
    cptr->tcp_bytes_sent += cptr->bytes_left_to_send;

  NSDL2_HTTP2(NULL, cptr, "bytes_left_to_send = [%d]", cptr->bytes_left_to_send); 
 
  if (cptr->bytes_left_to_send != 0) {  // This means write is incomplete for HTTP2 set free array and return. 
    return HTTP2_SUCCESS;
  }
  /*bug 51330: set state to CNST_HTTP2_READING*/
  cptr->conn_state = CNST_HTTP2_READING;
  NSDL2_HTTP2(NULL, cptr, "cptr->conn_state = [%d] cptr->http2_state=[%d]", cptr->conn_state, cptr->http2_state);
  // This means that write  is complete . Change proto state and return
  cptr->total_pop3_scan_list = 0;  
  if (cptr->http2_state == HTTP2_CON_PREFACE_CNST) { 
    // Once Connection Preface is send , we will immedialtely send Settings Frame 
    cptr->http2_state = HTTP2_SETTINGS_CNST; 
    ret = http2_make_handshake_frames(cptr, now);
    if(ret == HTTP2_ERROR){
      retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      //Close_connection(cptr, 1, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
    }
  } else if (cptr->http2_state == HTTP2_SETTINGS_CNST) {
    // Once Control is here it we completely written settings frame. Free settings frame here
    FREE_AND_MAKE_NULL(cptr->free_array, "settings_frame", -1);
    cptr->http2_state = HTTP2_SETTINGS_SENT; 
  } else if (cptr->http2_state == HTTP2_SETTINGS_DONE) {
    FREE_AND_MAKE_NULL(cptr->free_array, "settings_frame", -1);
    //cptr->http2_state = HTTP2_SETTINGS_ACK;
    /*bug 93672: gRPC: added check for HTTP2_PING_CNST*/
  } else if ((cptr->http2_state == HTTP2_PING_RCVD) || (HTTP2_PING_CNST == cptr->http2_state)) {
    FREE_AND_MAKE_NULL(cptr->free_array, "ping frame", -1);
    cptr->http2_state = HTTP2_HANDSHAKE_DONE;
  } else if (cptr->http2_state == HTTP2_SEND_WINDOW_UPDATE) {
    FREE_AND_MAKE_NULL(cptr->free_array, "window update frame", -1);
    cptr->http2_state = HTTP2_HANDSHAKE_DONE;
  } else if (cptr->http2_state == HTTP2_SEND_RESET_FRAME) {
    FREE_AND_MAKE_NULL(cptr->free_array, "reset frame", -1);
    cptr->http2_state = HTTP2_HANDSHAKE_DONE;
  } else if (cptr->http2_state != HTTP2_HANDSHAKE_DONE) {
     NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "Handle HTTP2 write.Invalid proto state. cptr->http2_state %hd",cptr->http2_state);
    Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE); 
  } else {
    NSDL2_HTTP2(NULL, cptr, "Invalid Proto State. Hence closing connection from here");

    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "Handle HTTP2 write.Invalid proto state. cptr->http2_state %hd",cptr->http2_state);
     retry_connection(cptr, now, NS_HTTP2_REQUEST_ERROR);
    //Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
  }

  return 0;
}


#define HTTP2_CON_PREFACE_LEN 24

/*--------------------------------------------------------------------
Purpose : This Function makes and write connection preface followed by  
          settings frame required for the establishment of HTTP/2
          connections over cleartext TCP.
         
          cptr->proto_state and cptr->conn_state is also changed within 
          the function required later in decision making .  
           
----------------------------------------------------------------------*/
 
int http2_make_handshake_frames(connection *cptr, u_ns_ts_t now) {

  static unsigned char Connection_Preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
  unsigned char *client_settings_frame ;  // Settings frame will be maximum  9 octates + 48 bits for settings frame  
  unsigned int settings_len = 0;
  int tot_length = 0;
  int window_update_len = 0; 

  NSDL2_HTTP2(NULL, NULL, "Method Called. cptr->http2_state [%d]", cptr->http2_state);

  if (cptr->http2_state == HTTP2_CON_PREFACE_CNST) {
    cptr->free_array = (char *)Connection_Preface;  // We are using cptr->free_array to send connection_preface  
    cptr->bytes_left_to_send = HTTP2_CON_PREFACE_LEN; // make macro
  } else if (cptr->http2_state == HTTP2_SETTINGS_CNST) {
    // We will make settings frame here
    MY_MALLOC_AND_MEMSET(client_settings_frame, 128, "settings_frame", -1);// Mcro for len 
    settings_len = init_settings_frame(cptr, client_settings_frame);
    if (settings_len == -1) {
      NSDL2_HTTP2(NULL, cptr, "Unable to make settings frame. Hence Closing Connection");
      FREE_AND_MAKE_NULL(client_settings_frame, "settings_frame", -1);
      return HTTP2_ERROR;
    }
    // We will write window update frame along with settings frame 
    window_update_len = pack_window_update_frame(cptr, client_settings_frame + settings_len,INIT_WINDOW_UPDATE_SIZE,0) ;
    if (window_update_len == -1) {
      NSDL2_HTTP2(NULL, NULL, "Unable to make window update frame. Hence Closing Connection");
      FREE_AND_MAKE_NULL(client_settings_frame, "settings_frame", -1);
      return HTTP2_ERROR;
    }
    tot_length = settings_len + window_update_len; 
    NSDL2_HTTP2(NULL, cptr, "tot_length = %d", tot_length);
    cptr->free_array = (char *)client_settings_frame;
    cptr->bytes_left_to_send = tot_length;
    cptr->http2->flow_control.local_window_size = INIT_WINDOW_UPDATE_SIZE;
    //cptr->req_code_filled = 0;
  } else if (cptr->http2_state == HTTP2_SETTINGS_DONE) {
    // Send settings Acknowledge here 
    MY_MALLOC_AND_MEMSET(client_settings_frame, 9, "settings_frame", -1); 
    settings_len = init_settings_frame(cptr, client_settings_frame);
    if (settings_len == -1) {
      NSDL2_HTTP2(NULL, cptr, "Unable to make settings frame. Hence Closing Connection");
      FREE_AND_MAKE_NULL(client_settings_frame, "settings_frame", -1);

       NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "Closing connection from make handshake frame. cptr->http2_state %hd",cptr->http2_state);

      Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
      return HTTP2_ERROR;
    }
    cptr->free_array = (char *)client_settings_frame;
    cptr->bytes_left_to_send = settings_len; 
  } else {
    NSDL1_HTTP2(NULL, cptr, "Something is wrong . This must not be here. Hence closing connection.");
     NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "closing connection from make handshake frame. cptr->http2_state %hd",cptr->http2_state);

    Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
    return HTTP2_ERROR; 
  }

  // Set state to CNST_HTTP2_WRITING, so that any read event on this fd will bring it to http2_handle_read
  cptr->conn_state = CNST_HTTP2_WRITING;
  return http2_handle_write(cptr, now);
}

/* This method is used to handle http2 multiplex request write, as there can be multiple request queued on a single connection, This method 
 * gets request from queue and call 
  handle_write for wiriting the request. This method will try to write all the requset in the queue. 
 */
int h2_handle_incomplete_write(connection* cptr, u_ns_ts_t now ){
  int ret;
  NSDL1_HTTP2(NULL, cptr, "Method called, cptr->http2->front=%p", cptr->http2->front);

  while(cptr->http2->front){
    NSDL1_HTTP2(NULL, cptr, "Queue is avaliable");
    copy_stream_to_cptr(cptr, cptr->http2->front);
    /*bug 79062 assigned cptr->http2->front to cptr->http2->http2_cur_stream*/
    cptr->http2->http2_cur_stream = cptr->http2->front;
    ret = handle_write(cptr, now);
    if(ret == 0){
      NSDL1_HTTP2(NULL, cptr, "Going to write next request");
      cptr->http2->front = cptr->http2->front->q_next;
    }else if(ret == -1){
      NSDL1_HTTP2(NULL, cptr, "Eagain copy all the counters to scptr");
      copy_cptr_to_stream(cptr, cptr->http2->front);
      break;
    }else {
      NSDL1_HTTP2(NULL, cptr, "Error in writing request");
      break;
    }
  }
  NSDL1_HTTP2(NULL, cptr, "Method exit. status=%d",ret);
  return ret;
}

/* This method is used to handle http2 multiplex request write, as there can be multiple request queued on a single connection, This method 
 * gets request from queue and call 
  handle_write for wiriting the request. This method will try to write all the requset in the queue. 
 */
int h2_handle_non_ssl_write(connection* cptr, u_ns_ts_t now)
{
  NSDL1_HTTP2(NULL, cptr, "Method called. cptr->http2_state=%d",cptr->http2_state);
  int result;
  switch(cptr->http2_state)
  {
    case HTTP2_SETTINGS_CNST:
    case HTTP2_CON_PREFACE_CNST:
    case HTTP2_SETTINGS_DONE:
      result = http2_handle_write(cptr, now);
    break;

     default:
    if(IS_HTTP2_INLINE_REQUEST)
      result = h2_handle_incomplete_write(cptr, now);
    else
      result = handle_write(cptr, now);
    break;
  }
 NSDL1_HTTP2(NULL, cptr,"exiting...with status=%d", result);
 return result;
}

/*------------------------------------------------------------------------
Input   :  cptr , now .   

Purpose :  This function cal http2_writing based on request type

Output  : 0 --> in case of sucess or WANT_WRITE/READ Error
	  -1 --> in case of error 
--------------------------------------------------------------------------*/
int h2_handle_write_ex(connection* cptr, u_ns_ts_t now)
{
  NSDL1_HTTP2(NULL, cptr,"Method called. cptr->request_type=%d", cptr->request_type);
  int result = 0;
  switch(cptr->request_type)
  {
     case HTTPS_REQUEST:
     {
       /*bug 89702: updated code to return 0 incase of WANT_READ/WRITE error during handle_ssl_write*/
       result = handle_ssl_write (cptr, now);
       NSDL2_HTTP2(NULL, NULL,"handle_ssl_write status=%d",result);
       /*bug 78105 : return error in case of ssl write failure*/
       switch(result)
       {
         case HTTP2_SUCCESS:
         break;

	 case HTTP2_ERROR:
         result = 0;
         break;

         default:
         result = HTTP2_ERROR;
         break;
       }
     }
     break;

     default:
     handle_write(cptr, now);
     break;
  }
  NSDL1_HTTP2(NULL, cptr,"exiting...with status=%d", result);
  return  result;
}

/*************************************************************************************/
/* Name         :h2_make_handshake_and_ping_frames
*  Description  :It is an interface to call relative methods to create and send HandShake
*		 and Ping Frames
*  Arguments    :cptr, now=> current time
*  Result       :NA
*  Return       :
*		0 ==> success
*	       -1 ==> in case of error
*/
/**************************************************************************************/
/*bug 93672: gRPC - send ping frame after SETTING FRAME*/
int h2_make_handshake_and_ping_frames(connection *cptr, u_ns_ts_t now)
{
   NSDL1_HTTP2(NULL, cptr,"Method called");
   if(HTTP2_ERROR == http2_make_handshake_frames(cptr, now))
      return HTTP2_ERROR;

   NSDL1_HTTP2(NULL, cptr,"before sending ping frame cptr->http2_state=%d", cptr->http2_state);
   return h2_make_and_send_ping_frame(cptr, now, NS_SEND_PING_FRAME);
}
