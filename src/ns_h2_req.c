/********************************************************************************************
 *Name: ns_h2_req.c
 *Perpose: This C file holds the functions for making the http2 request.
 *Author: Shalu/Nishi
 *Initial version date: 6th October 2016
 *Last Modification date: 30th December 2016

 * Author                 : Anubhav

 * Date                   : December 2017
 * Change Description     : Handling of Server Push
 * Date                   : April 2018
 * Change Description     : Dumping Multiplexed response headers/ data frames
 * Date                   : May 2018
 * Change Description     : Handling of Flow Control
 * Date                   : July 2018
 * Change Description     : Handling of Response and Idle timeout
********************************************************************************************/

#include "ns_h2_header_files.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "ns_nd_integration.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

char *Http2MethodStr  = ":method";
#define HTTP2_METHOD_STR_LEN 7  

char *Http2SchemeStr = ":scheme";
#define HTTP2_SCHEME_STR_LEN 7 

char *Http2HttpStr = "http";
#define HTTP2_HTTP_STR_LEN 4 

char *Http2HttpsStr = "https";
#define HTTP2_HTTPS_STR_LEN 5 

char *Http2PathStr = ":path";
#define HTTP2_PATH_STR_LEN 5 

char *Http2RefererStr = "referer";
#define HTTP2_REF_STR_LEN 7 

char *Http2HostStr = ":authority";
#define HTTP2_HOST_STR_LEN 10 

char *Http2UserAgentStr = "user-agent";
#define HTTP2_USER_AGENT_STR_LEN 10

char *Http2AceptStr = "accept";
#define HTTP2_ACCEPT_STR_LEN 6 

char *Http2AceptValueStr = "text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,video/x-mng,image/png,image/jpeg,image/gif;q=0.2,text/css,*/*;q=0.";
#define HTTP2_ACCEPT_VALUE_STR_LEN strlen(Http2AceptValueStr)

char *Http2AcceptEncStr = "accept-encoding";
#define HTTP2_ACCEPT_ENC_STR_LEN 15 

/*bug 84661: removed compress and added br*/
char *Http2AceptEncValueStr = "gzip, deflate, br;q=0.9";
#define HTTP2_ACCEPT_ENC_VALUE_STR_LEN strlen(Http2AceptEncValueStr) 

char *Http2KeepAliveStr = "keep-alive";
#define HTTP2_KEEP_ALIVE_STR_LEN 10

char *Http2KeepAliveValueStr = "300";
#define HTTP2_KEEP_ALIVE_VALUE_STR_LEN 3 

char *Http2ConnectionStr = "connection";
#define HTTP2_CONNECTION_STR_LEN 10 

char *Http2ConnectionValueStr = "keep-alive";
#define HTTP2_CONNECTION_VALUE_STR_LEN 10

char *Http2ContentLengthStr = "content-length";
#define HTTP2_CONTENTLENGTH_STR_LEN 14
 
char *Http2CookieString = "cookie";
#define HTTP2_COOKIE_STRING_LEN 6

/*bug 52092 : added MACROS CONNECT */
#define H2_CONNECT "CONNECT"
#define H2_CONNECT_LEN 7

FILE *log_hdr_fp;

FreeArrayHttp2 *free_array_http2;

extern int *body_array; /*bug 78106*/

// Needed for http2 
nghttp2_nv *http2_hdr_ptr;
int nghttp2_nv_max_entries = 0;
int nghttp2_nv_tot_entries = 0;
size_t idx = 0;

data_frame_hdr *frame_hdr;

#define MAKE_NV(K, V)                                                          \
  {                                                                            \
    (uint8_t *) K, (uint8_t *)V, sizeof(K) - 1, sizeof(V) - 1,                 \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

/* Keyword parsing code start */
int kw_set_g_http_mode(char *buf, short *http_mode, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  short mode;
  int num_fields;
  NSDL2_SCHEDULE(NULL, NULL, "Method called. buf = %s", buf);

  num_fields = sscanf(buf, "%s %s %hi", keyword, sgrp_name, &mode);
  NSDL2_SCHEDULE(NULL, NULL, "Method called. num_fields = %d", num_fields);
  if (num_fields != 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_MODE_USAGE, CAV_ERR_1011026, CAV_ERR_MSG_1);
  }

  // Checking for mode here . It should not be les than 0 or greater than 2 . 
  if(mode < 0 || mode > 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_MODE_USAGE, CAV_ERR_1011026, CAV_ERR_MSG_3);
  }
 
  *http_mode = mode; 
  NSDL2_MISC(NULL, NULL, "HTTP MODE = %d", *http_mode);
  return 0;
}

/* Keyword parsing code start */
int kw_set_g_http2_settings(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  short enable_push_loc;
  int max_concurrent_streams_loc;
  int initial_window_size_loc;
  int max_frame_size_loc;
  short header_table_size_loc;
  int num_fields;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. buf = %s", buf);

  num_fields = sscanf(buf, "%s %s %hi %d %d %d %hi", keyword, sgrp_name, &enable_push_loc, &max_concurrent_streams_loc, &initial_window_size_loc, &max_frame_size_loc, &header_table_size_loc);
  
  NSDL2_SCHEDULE(NULL, NULL, "num_fields = %d", num_fields);
  
  if (num_fields != 7) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011091, CAV_ERR_MSG_1);
  }

  // Checking for mode here . It should not be les than 0 or greater than 1 . 
  if(enable_push_loc < 0 || enable_push_loc > 1) 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011091, CAV_ERR_MSG_3);
  
  gset->http2_settings.enable_push = enable_push_loc;
  
  if(max_concurrent_streams_loc < 1 || max_concurrent_streams_loc > 500)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011092, "");

  gset->http2_settings.max_concurrent_streams = max_concurrent_streams_loc;
  
  if(initial_window_size_loc == 2147483648)
    initial_window_size_loc -= 1;  

  if(initial_window_size_loc < 65535 || initial_window_size_loc > 2147483647)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011093, "");

  gset->http2_settings.initial_window_size = initial_window_size_loc;
  
  if(max_frame_size_loc == 16777216)
    max_frame_size_loc -= 1;

  if(max_frame_size_loc < 16384 || max_frame_size_loc > 16777215)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011094, "");

  gset->http2_settings.max_frame_size = max_frame_size_loc;
   
  if(header_table_size_loc == 65536)
    header_table_size_loc -= 1;
  
  if(header_table_size_loc < 4096 || header_table_size_loc > 65535)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP2_SETTINGS_USAGE, CAV_ERR_1011095, "");

  gset->http2_settings.header_table_size = header_table_size_loc;
  /*bug 70480 set protocol_enabled to HTTP2_SERVER_PUSH_ENABLED*/
  if(gset->http2_settings.enable_push)
    global_settings->protocol_enabled |= HTTP2_SERVER_PUSH_ENABLED;

  NSDL2_MISC(NULL, NULL, " HTTP2 settings passed in keyword Server Push MODE = %hi max_concurrent_streams = %d initial_window_size = %d max_frame_size = %d header_table_size = %hi", gset->http2_settings.enable_push, gset->http2_settings.max_concurrent_streams, 
     	gset->http2_settings.initial_window_size, gset->http2_settings.max_frame_size, gset->http2_settings.header_table_size);
  return 0;
}


// Macro for converting upper case to lower case
#define UPPER_TO_LOWER(header_name_buf, len)                            \
{                                                                       \
  int index;                                                            \
  for(index = 0; index < len; index++ )                                 \
  {                                                                     \
    header_name_buf[index] = tolower(header_name_buf[index]);           \
  }                                                                     \
  NSDL4_HTTP(NULL, NULL, "lower case header name = %*.*s", len, len, header_name_buf);\
}




/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : ns_get_checksum 
 *
 * Purpose   : This function will generate md5 check sum for Request body and add header for this 
 *             Checksum will be generated by following way -
 *             1. Add provided prefix and suffix to the body
 *             2. Get md5sum by function ns_gen_md5_checksum() 
 *             Exampl: If body is - "My name is {name} and I am working in Cavisson", then md5sum will be calculate as
 *                Here {name} is parameter so fisrt expand this and then generate md5sum
 *                Suppose after expending {name} we got body - "My name is Foo and I am working in Cavisson" then
 *                add prefix and sufix (say { and }) and then generate md5sum
 *            
 *
 * Input     : 
 *
 * Output    : On error     -1
 *             On success    0
 *        
 * Author    : Abhay Singh
 * Build_v   : 4.1.12  
 *------------------------------------------------------------------------------------------------------------------*/

#define NS_MD5_CHECKSUM_SIZE    64
#define NS_MD5_CHECKSUM_LENGTH  32
char* ns_get_checksum(VUser *vptr, NSIOVector *ns_iovec, int body_start_idx, int body_size)
{

  NSDL2_VARS(vptr, NULL, "Method Called, body_size = %d", body_size);
  int grp_num = vptr->group_num;

  GroupSettings *lgset = &runprof_table_shr_mem[grp_num].gset;

  //check we have enough space into ns_nvm_scratch_buf
  int total_body_size = body_size + lgset->http_body_chksum_hdr.pfx_len + lgset->http_body_chksum_hdr.sfx_len;
  NSDL2_VARS(vptr, NULL, "ns_nvm_scratch_buf_size = %d, total_body_size = %d", ns_nvm_scratch_buf_size, total_body_size);
  if(ns_nvm_scratch_buf_size < total_body_size)
  {
    MY_REALLOC(ns_nvm_scratch_buf, total_body_size, "reallocating for http_req_body md5 checksum ", -1);
    ns_nvm_scratch_buf_size = total_body_size;
  }

  char* write_ptr = ns_nvm_scratch_buf;
  write_ptr[0] = 0;

  //Adding prefix into body
  if(lgset->http_body_chksum_hdr.if_pfx_sfx)
  {
    memcpy(write_ptr, lgset->http_body_chksum_hdr.pfx, lgset->http_body_chksum_hdr.pfx_len);
    write_ptr += lgset->http_body_chksum_hdr.pfx_len;
  }

  //Copy body into ns_nvm_scratch_buf
  for(int j = 0; j < NS_GET_IOVEC_CUR_IDX(*ns_iovec) ;j++){
    NSDL4_VARS(vptr, NULL, "j = %d, vector[j].iov_base = %s", j, ns_iovec->vector[j].iov_base);
      NSDL4_VARS(vptr, NULL, "inside condition j = %d, vector[j].iov_base = %s", j, ns_iovec->vector[j].iov_base);
      memcpy(write_ptr, vector[j].iov_base, ns_iovec->vector[j].iov_len);
      write_ptr += ns_iovec->vector[j].iov_len;
    }
  
  //Adding suffix into body
  if(lgset->http_body_chksum_hdr.if_pfx_sfx)
  {
    memcpy(write_ptr, lgset->http_body_chksum_hdr.sfx, lgset->http_body_chksum_hdr.sfx_len);
    write_ptr += lgset->http_body_chksum_hdr.sfx_len;
  }
  static char ns_http_body_checksum_buffer[NS_MD5_CHECKSUM_SIZE + 1];
  NSDL4_VARS(vptr, NULL, "Body for which md5sum has to calculated = %s", ns_nvm_scratch_buf);

  ns_gen_md5_checksum((unsigned char *)ns_nvm_scratch_buf, (write_ptr - ns_nvm_scratch_buf), (unsigned char *)ns_http_body_checksum_buffer);

  NSDL2_VARS(NULL, NULL, "total_body_size=%d, if_pfx_sfx=%hd ,"
                         "ns_http_body_checksum_buffer = %s",
                         total_body_size, lgset->http_body_chksum_hdr.if_pfx_sfx, ns_http_body_checksum_buffer);

  return ns_http_body_checksum_buffer;
}


int handle_http2_upgraded_connection(connection *cptr, u_ns_ts_t now, int chk, int req_type) {

  NSDL2_HTTP(NULL, cptr, "Method Called, req_code = %d, chk = %d, req_type = %d",
					 cptr->req_code, chk, req_type);
  update_http_status_codes(cptr, average_time);
 
/*
  if(SHOW_GRP_DATA)
  {
    VUser *vptr = cptr->vptr;
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    update_http_status_codes(cptr, lol_average_time);
  }
*/
  //cptr->conn_state = CNST_WRITING;
  if(cptr->req_code ==  101){
    cptr->req_code_filled = -2;  // Reset as we will expect for next response code
    cptr->content_length = -1;
    cptr->req_code = 0;
  }

  NSDL2_HTTP(NULL, cptr, "Received 101 Switching Protocol in response from server. Proceeding with HTTP2 handshake frames");

    
  if (req_type == HTTP_REQUEST) {
    if ((cptr->flags & NS_HTTP2_SETTINGS_ALREADY_RECV) != NS_HTTP2_SETTINGS_ALREADY_RECV) { 
      init_cptr_for_http2(cptr);
    }  
    /*bug 93672: gRPC - send Ping Frame after Setting Frame*/
    h2_make_handshake_and_ping_frames(cptr, now);
    if (cptr->flags & NS_HTTP2_SETTINGS_ALREADY_RECV ) {
      NSDL2_HTTP2(NULL, NULL, "Since Http2 settings are already received, sending ACK for received settings."
                              "Setting cptr->http2_state to HTTP2_SETTINGS_DONE");
      cptr->http2_state = HTTP2_SETTINGS_DONE;
      init_stream_map_http2(cptr);
      http2_make_handshake_frames(cptr, now); // Sending ack 
      NSDL2_HTTP2(NULL, NULL, "Marking state to handshake done . As in case of 101 it is not necessary that ACK is received from server");
      cptr->http2_state = HTTP2_HANDSHAKE_DONE;
      cptr->flags  &= ~NS_HTTP2_SETTINGS_ALREADY_RECV;  // Unset after Processing response
    }
  }

 if(cptr->req_code ==  200)
   return -1;

 return 0;
}

/*******************************************************************************************
  Purpose : This function is used for initialising the deflate and compressing the 
            http2 headers

  Input   : It takes the cptr and compress_buflen as input.

  Output  : It will return compressed buffer and fill the compressed header buffer length
            in compress_buflen.
*******************************************************************************************/
uint8_t *hdr_deflate_init(connection *cptr, size_t *compress_buflen)
{
  //nghttp2_hd_deflater *deflater;
  int rv;
  uint8_t *buf;
  NSDL3_HTTP(NULL, NULL, "Method called.");

 // For debug
  size_t i;
  for(i = 0; i < nghttp2_nv_tot_entries; i++){
    NSDL3_HTTP(NULL, NULL, "http2_hdr_ptr[i].namelen = %d, http2_hdr_ptr[%d].name = %*.*s", 
			http2_hdr_ptr[i].namelen, i, http2_hdr_ptr[i].namelen, http2_hdr_ptr[i].namelen, http2_hdr_ptr[i].name);
    NSDL3_HTTP(NULL, NULL, "http2_hdr_ptr[i].valuelen = %d, http2_hdr_ptr[%d].value = %*.*s", 
			http2_hdr_ptr[i].valuelen, i, http2_hdr_ptr[i].valuelen, http2_hdr_ptr[i].valuelen, http2_hdr_ptr[i].value);
  }

  size_t outlen;
  *compress_buflen = nghttp2_hd_deflate_bound(cptr->http2->deflater, http2_hdr_ptr, nghttp2_nv_tot_entries);
  MY_MALLOC(buf, *compress_buflen + 9, "Allocating buffer for deflate", -1);
  rv = nghttp2_hd_deflate_hd(cptr->http2->deflater, buf + 9, *compress_buflen, http2_hdr_ptr, nghttp2_nv_tot_entries);
  if (rv < 0) {
    fprintf(stderr, "nghttp2_hd_deflate_hd() failed with error: %s\n", nghttp2_strerror((int)rv));
    FREE_AND_MAKE_NOT_NULL_EX(buf, *compress_buflen + 9, "Free buffer for deflate", -1);
    end_test_run();// Close_connection or END_TEST_RUN
  }
  outlen = (size_t)rv;

  NSDL3_HTTP(NULL, NULL, "outlen  = %d", outlen);

  *compress_buflen = outlen;
  return buf;
}

/****************************************************************************
  Purpose : This function is for allocating memory for http2_hdr_ptr and 
            incrementing the index of nghttp2_nv table.

  Input  : It takes the index of nghttp2_nv entries.
*****************************************************************************/
inline void create_ng_http_nv_entries(size_t *rnum){

  NSDL3_HTTP(NULL, NULL, "Method called");

  if(nghttp2_nv_max_entries == nghttp2_nv_tot_entries){
    MY_REALLOC(http2_hdr_ptr, (nghttp2_nv_max_entries +  NS_HTTP2_DELTA)*sizeof(nghttp2_nv), "http2_hdr_ptr", -1);
    MY_REALLOC(free_array_http2, (nghttp2_nv_max_entries +  NS_HTTP2_DELTA)*sizeof(FreeArrayHttp2), "FreeArrayHttp2", -1);
    memset(free_array_http2 + nghttp2_nv_max_entries, 0, NS_HTTP2_DELTA*sizeof(FreeArrayHttp2)); // Set free array to 0
    nghttp2_nv_max_entries += NS_HTTP2_DELTA;
  }
  *rnum = nghttp2_nv_tot_entries++;
}

/******************************************************************************
  Purpose : This funnction is used for tokenising the headers by using \r\n
            as a delimeter. .e.g. header1:value1\r\nheader2:value2
            First it separate header1:value1 to header2:value2, then it 
            seperate header to its value by using : as a delimeter and fill
            the name value pair to http table.

  Input   : It takes header_buf as input which contains headers in the form
            of header1:value1\r\nheader2:value2
******************************************************************************/
void tokenise_and_fill_multihdr_http2(char *header_buf, int push_flag)
{
  char *start_ptr = NULL;
  char *value_ptr = NULL;
  char *hdr_name = NULL;
  char *hdr_value = NULL;
  int namelen = 0, valuelen = 0;

  NSDL2_HTTP(NULL, NULL, "Method Called. push_flag = %d", push_flag);
  while((start_ptr = strstr(header_buf, "\r\n"))!= NULL)
  {
    NSDL2_HTTP(NULL, NULL, "header received is %s", header_buf);
    *start_ptr = '\0';
    if((value_ptr = strchr(header_buf, ':')) != NULL)
    {
      *value_ptr = '\0';
      namelen = value_ptr - header_buf;
      MY_MALLOC(hdr_name, namelen+1, "buffer for header name", -1);
      strncpy(hdr_name, header_buf, namelen);
      hdr_name[namelen] = '\0';
      *value_ptr = ':';
      value_ptr++;
      CLEAR_WHITE_SPACE(value_ptr);
      valuelen = start_ptr - value_ptr;
      MY_MALLOC(hdr_value, valuelen, "buffer for header value", -1);
      strncpy(hdr_value, value_ptr, valuelen);
      UPPER_TO_LOWER(hdr_name, namelen);
      if(!strncmp(hdr_name, "proxy-connection", 16)){
        *start_ptr = '\r';
        header_buf = start_ptr + 2;
        continue; 
      }    
      if(!push_flag){ 
        FILL_HEADERS_IN_NGHTTP2(hdr_name, namelen, hdr_value, valuelen, 1, 1);
      }
      else{
        LOG_PUSHED_REQUESTS(hdr_name, namelen, hdr_value, valuelen);
        /*bug 86575: release hdr_name/value in case of server_push*/
        NSDL2_HTTP(NULL, NULL, "releasing hdr_name = %p and hdr_value = %p", hdr_name, hdr_value);
        FREE_AND_MAKE_NULL(hdr_name, "buffer for header name", -1);
        FREE_AND_MAKE_NULL(hdr_value, "buffer for header value", -1);
      }
    }
    NSDL2_HTTP(NULL, NULL, "header received is %s", header_buf);
    *start_ptr = '\r';
    header_buf = start_ptr + 2;
  }
}

/********************************************************************************
 Purpose: This function is used to tokenise the header name to header value 
          by using : as a delimeter. THis function is called for
          ns_web_add_auto_header API.
********************************************************************************/
void tokenise_and_fill_hdr_http2(connection* cptr, VUser* vptr, int push_flag)
{
  NSDL2_HTTP(NULL, cptr, "Method called. push_flag = %d", push_flag);
  User_header *cnode;
  cnode = vptr->httpData->usr_hdr[NS_AUTO_HDR_IDX].next;
  char *end_ptr;
  char *start_ptr = NULL;
  int namelen = 0;
  int valuelen = 0;
  char *hdr_name = NULL;
  char *hdr_value = NULL;
 
  while(cnode != NULL)
  {
    //check when node contain main url bit
    if((cptr->url_num->proto.http.type == MAIN_URL) && (cnode->flag & MAIN_URL_HDR))
    {
      NSDL2_HTTP2(NULL, cptr, "Main URL: used_len = %d, head_buffer = %s", cnode->used_len, cnode->hdr_buff);
      if((end_ptr = strstr(cnode->hdr_buff, "\r\n"))) {
        if((start_ptr = strchr(cnode->hdr_buff, ':')) != NULL)
        {
          namelen = start_ptr - cnode->hdr_buff;
          NSDL2_HTTP2(NULL, NULL, "namelen = %d", namelen);
          MY_MALLOC(hdr_name, namelen, "buffer for header name", -1);
          strncpy(hdr_name, cnode->hdr_buff, namelen);
          start_ptr++;
          CLEAR_WHITE_SPACE(start_ptr);
          valuelen = end_ptr - start_ptr;
          MY_MALLOC(hdr_value, valuelen, "buffer for header value", -1);
          strncpy(hdr_value, start_ptr, valuelen);
          UPPER_TO_LOWER(hdr_name, namelen);
          if(!strncmp(hdr_name, "proxy-connection", 16)){
            continue;
          }
          if(!push_flag){
            FILL_HEADERS_IN_NGHTTP2(hdr_name, namelen, hdr_value, valuelen, 1, 1);
          }
          else{
            LOG_PUSHED_REQUESTS(hdr_name, namelen, hdr_value, valuelen);
            NSDL2_HTTP(NULL, NULL, "releasing hdr_name = %p and hdr_value = %p", hdr_name, hdr_value);
            /*bug 86575: release hdr_name/value in case of server_push*/
            FREE_AND_MAKE_NULL(hdr_name, "buffer for header name", -1);
            FREE_AND_MAKE_NULL(hdr_value, "buffer for header value", -1);
         }
        }
      }
    }
    else if((cptr->url_num->proto.http.type == EMBEDDED_URL) && (cnode->flag & EMBD_URL_HDR))
    {
                                                                                                                             
     //check when node contain embedded url bit 
      NSDL2_HTTP(NULL, cptr, "Embedded URL: idx = %d, used_len = %d, head_buffer = %s", idx, cnode->used_len, cnode->hdr_buff);
      if((end_ptr = strstr(cnode->hdr_buff, "\r\n"))){
        if((start_ptr = strchr(cnode->hdr_buff, ':')) != NULL)
        {
          namelen = start_ptr - cnode->hdr_buff;
          MY_MALLOC(hdr_name, namelen, "buffer for header name", -1);
          strncpy(hdr_name, cnode->hdr_buff, namelen);
          start_ptr++;
          CLEAR_WHITE_SPACE(start_ptr);
          valuelen = end_ptr - start_ptr;
          MY_MALLOC(hdr_value, valuelen, "buffer for header value", -1);
          strncpy(hdr_value, start_ptr, valuelen);
          UPPER_TO_LOWER(hdr_name, namelen);
          if(!strncmp(hdr_name, "proxy-connection", 16)){
            continue;
          }
          if(!push_flag){
            FILL_HEADERS_IN_NGHTTP2(hdr_name, namelen, hdr_value, valuelen, 1, 1);
          }
          else{
          LOG_PUSHED_REQUESTS(hdr_name, namelen, hdr_value, valuelen);
          /*bug 86575: release hdr_name/value in case of server_push*/
          NSDL2_HTTP(NULL, NULL, "releasing hdr_name = %p and hdr_value = %p", hdr_name, hdr_value);
          FREE_AND_MAKE_NULL(hdr_name, "buffer for header name", -1);
          FREE_AND_MAKE_NULL(hdr_value, "buffer for header value", -1);
          }
        }
      }
    }
    cnode = cnode->next;
  }
  NSDL2_HTTP(NULL, cptr, "No node exist");
}

/*************************************************************************
  Purpose: This function is used for filling the transaction header in 
           nghttp2 table.
  
*************************************************************************/
void netcache_fill_trans_hdr_in_req_http2(VUser* vptr, connection* cptr, int push_flag)
{
  int tx_hdr_buf_len = 0;
  char *tx_hdr_buf = NULL;
  char *tx_hdr_name_buf = NULL;
  char *str_txvar = NULL;
  int str_txvar_len;
  char* inline_prefix = "InLine.";
  int inline_prefix_len = 7;  

  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
  if(node_ptr == NULL) {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                  __FILE__, (char*)__FUNCTION__, "No transaction is running for this user, so netcache transaction header will not be send.");
   return ;
  }
  
  char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code);
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable[0] == '\0') 
  {
    str_txvar = tx_name;
    str_txvar_len = strlen(str_txvar);
  }
  else
  {
    str_txvar = ns_eval_string(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable);
    str_txvar_len = strlen(str_txvar);
  }
  
  MY_MALLOC(tx_hdr_name_buf, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len, "Netcache transaction header name", -1);
  MY_MALLOC(tx_hdr_buf, inline_prefix_len + str_txvar_len, "Netcache transaction header", -1);
  // IF netcache header mode is set main and url is inline then do not send header, in all other cases send header 

  if(!((runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode == NS_TX_HTTP_HEADER_SEND_FOR_MAIN_URL) && (cptr->url_num->proto.http.type == EMBEDDED_URL)))
  {

    //Adding HTTP Header name is the name of HTTP header. Default value of http header name is "CavTxName".
    NSDL2_HTTP2(NULL, cptr, "vptr->group_num = %d, name = %s, len = %d", vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len);

    NSDL2_HTTP2(NULL, cptr, "Adding Netstorm HTTP Header name for Transaction header in HTTP vector to be send - %s",
                          runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name);

    if(cptr->url_num->proto.http.type == EMBEDDED_URL)
    {
      strcpy(tx_hdr_buf, inline_prefix); 
      tx_hdr_buf_len = inline_prefix_len;
      NSDL3_HTTP(NULL, cptr, "INLINE BASE - %s, LEN = %d ", tx_hdr_buf, tx_hdr_buf_len);
    }

    //Adding Transaction name, http req will have this header with the last tx started before this URL was send.
    if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable[0] == '\0')
    {
        NSDL3_HTTP(NULL, cptr, "tx var is null...");
        // We are sending head node of link list, that is the last transaction started before ns_web_url 
        if(tx_hdr_buf_len) 
        {
          strncpy(tx_hdr_buf + tx_hdr_buf_len, str_txvar, str_txvar_len); 
          tx_hdr_buf_len+= str_txvar_len;
        }
        else{
         strncpy(tx_hdr_buf, str_txvar, str_txvar_len); 
         tx_hdr_buf_len+= str_txvar_len; 
        }
        NSDL3_HTTP(NULL, cptr, "tx_name = %s, tx_hdr_buf = %*.*s",
                                tx_name, tx_hdr_buf_len, tx_hdr_buf_len, tx_hdr_buf);
   }
   else{
     NSDL3_HTTP(NULL, cptr, "tx var is not null...");
     if(tx_hdr_buf_len) 
     {
       strncpy(tx_hdr_buf + tx_hdr_buf_len, str_txvar, str_txvar_len); 
       tx_hdr_buf_len+= str_txvar_len; // len?
     }
     else{
       strncpy(tx_hdr_buf, str_txvar, str_txvar_len);
       tx_hdr_buf_len+= str_txvar_len; 
     }
   }
 
   strncpy(tx_hdr_name_buf, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len -2); 
   UPPER_TO_LOWER(tx_hdr_name_buf, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len - 2);
   if(!push_flag){
   FILL_HEADERS_IN_NGHTTP2(tx_hdr_name_buf, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len - 2, tx_hdr_buf, tx_hdr_buf_len, 1, 1);
   }
   else{
     LOG_PUSHED_REQUESTS(tx_hdr_name_buf, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len - 2, tx_hdr_buf, tx_hdr_buf_len);
     /*bug 86575: release tx_hdr_buf in case of server_push*/
     NSDL2_HTTP(NULL, NULL, "releasing tx_hdr_name_buf = %p and tx_hdr_buf = %p", tx_hdr_name_buf, tx_hdr_buf);
     FREE_AND_MAKE_NULL(tx_hdr_name_buf, "Netcache transaction header name", -1);
     FREE_AND_MAKE_NULL(tx_hdr_buf, "Netcache transaction header", -1);
   }                                          
  } 
}

/*****************************************************************************
  Purpose : This method is used for making frame header for data frame

  Input   : It takes content length and end stream flag as input.

  Output  : It will pack frame in fr_header buffer of data_frame_hdr structure
******************************************************************************/
int make_frame_header(connection *cptr,int content_length, int frame_hdr_idx, data_frame_hdr *frame_hdr, int end_stream, stream *stream_ptr)
{
  unsigned int amt_used;
  int pad_length = 0;
  unsigned char flag = 0 ; // Default flag will be reset .
  unsigned char padding = 0;

  NSDL2_HTTP(NULL, NULL, "Method called cptr->http2->stream_id is %d", stream_ptr->stream_id);

  if(!end_stream)
    amt_used = pack_frame (cptr, frame_hdr->fr_header, content_length , DATA_FRAME, 0, 0, stream_ptr->stream_id, padding, &flag);
  else
    amt_used = pack_frame (cptr, frame_hdr->fr_header, content_length , DATA_FRAME, 1, 0, stream_ptr->stream_id, padding, &flag);
  if (amt_used == -1)
  {
    printf("error in making frame header for data frame \n ");
    return -1;
  }

  if((flag >> 3) & 1) 
    frame_hdr->fr_header[amt_used] = pad_length;
  NSDL2_HTTP(NULL, NULL, "frame_hdr_idx=%d ",frame_hdr_idx);
  NS_FILL_IOVEC_AND_MARK_HTTP2_HEADER_IDX(g_req_rep_io_vector, frame_hdr->fr_header, 9, frame_hdr_idx);
  return 0;
}
/******************************************************************************************************************
  Purpose: This function is used for filling the data in data frames, if size of data 
           > cptr->http2->settings_frame->settings_max_frame_size, then new data frame is created.

  Input : cptr, ptr = payload, data_len is the length of the payload, frame_size is the length of data filled in
          data frames, to_free is the value that is filled in free_array, num_frame is the number of frame created.
          bytes_written is the total amount of data filled in data frame, current_idx is the index of the io_vector.

 Output : data frame of payload of body
This is how big frame is devided in multiple frames of max_frame_size
---------------------------------------------------------------
Data Header Frame|| Data Frame||Data Header Frame|| Data Frame
---------------------------------------------------------------
*******************************************************************************************************************/
static int http2_check_and_fill_data_frame(connection *cptr, NSIOVector *ns_data_iovec )
{

  int iovec_index;
  int frame_size = 0;
  int total_size = 0;
  unsigned int max_frame_size = cptr->http2->settings_frame.settings_max_frame_size;
  //Reserve index for HDR Frame
  int frame_hdr_idx = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
  NSDL2_HTTP(NULL, NULL,"Methodc called");
  NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
  for(iovec_index = 0; iovec_index < NS_GET_IOVEC_CUR_IDX(*ns_data_iovec); iovec_index++)
  {
    /*bug 84661: updated to split big data in number of data frame of size <= max_frame_size */
    /*and set end_stream_flag as set for last dat frame header */
    unsigned short int data_frame_count;
    char* currdataptr = NS_GET_IOVEC_VAL(*ns_data_iovec, iovec_index);
    unsigned int iovec_size = NS_GET_IOVEC_LEN(*ns_data_iovec, iovec_index);
    unsigned int last_data_frame_size, current_data_frame_size;
    /*check if ioec_size is > max_frame_size*/
    if(iovec_size > max_frame_size )
    {
      /*get frame count*/
      data_frame_count = iovec_size/max_frame_size;
      last_data_frame_size = (iovec_size % max_frame_size);
      if(last_data_frame_size)
        ++data_frame_count;
      current_data_frame_size = max_frame_size;
    }
    else {
     data_frame_count = 1;
     last_data_frame_size = current_data_frame_size = iovec_size;
    }
    NSDL2_HTTP(NULL, NULL, "iovec_size=%d, max_frame_size=%d,data_frame_count=%d,current_data_frame_size=%d",
					iovec_size, max_frame_size,data_frame_count,current_data_frame_size);
    /*split frame as per data_frame_count*/
    for(int ix =0; ix < data_frame_count; ix++ )
    {
     /*check if it is last data frame*/
     if(ix == (data_frame_count -1))
       current_data_frame_size = last_data_frame_size;

     /*get a new data header frame*/
     if( frame_size + current_data_frame_size >  max_frame_size)
     {
       NSDL2_HTTP(NULL, NULL, "DataFrameHeader frame_size=%d frame_hdr_idx=%d frame_count=%d ", frame_size, frame_hdr_idx, ix);
       data_frame_hdr *hdr_ptr = get_free_data_frame();
       make_frame_header(cptr, frame_size, frame_hdr_idx, hdr_ptr, 0, cptr->http2->http2_cur_stream);
       total_size += frame_size;
       frame_size = 0;
  
       //Reserve index for HDR Frame
       frame_hdr_idx = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
       NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
     }
     #ifdef NS_DEBUG_ON
     unsigned int  free_flag;
     free_flag = NS_IS_IOVEC_FREE(*ns_data_iovec, iovec_index);
     NSDL2_HTTP(NULL, NULL, "DataFrame frame_size=%d data_ptr=%p vec idx=%d frame no=%d  free_flag=%d ",
			current_data_frame_size, currdataptr, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector),ix, free_flag);
     #endif
     NS_CHK_FILL_IOVEC_AND_MARK_FREE_BODY(g_req_rep_io_vector, currdataptr,
                                current_data_frame_size,(ix == 0) ? NS_IS_IOVEC_FREE(*ns_data_iovec, iovec_index) : 0);
     frame_size += current_data_frame_size;
     currdataptr += current_data_frame_size;
    } /*internal for loop end here*/
  } /*external for loop end here*/
  NSDL2_HTTP(NULL, NULL,"iovec_index=%d",iovec_index);
  /*in case of GET, iovec_index would be zero*/
  if(!iovec_index) {
    /*reset value here as we have increased the same in the begining*/
    NS_DEC_IOVEC_CUR_IDX(g_req_rep_io_vector);
    return iovec_index;
  }

  /*create header frame with end_stream flag set to 1*/
  data_frame_hdr *hdr_ptr = get_free_data_frame();
  NSDL2_HTTP(NULL, NULL, "DataFrameHeader frame_size=%d frame_hdr_idx=%d ", frame_size, frame_hdr_idx);
  make_frame_header(cptr, frame_size, frame_hdr_idx, hdr_ptr, 1, cptr->http2->http2_cur_stream);
  total_size += frame_size;
  return total_size;
}

#define CHECK_COOKIE_BUF_LEN(cookie_buf, cookie_tot_buff_size, string_Length, cookie_buf_len)                \
{                                                                                                            \
  if(string_Length > cookie_tot_buff_size - cookie_buf_len){                                                 \
    MY_REALLOC(cookie_buf, cookie_tot_buff_size + string_Length, "Cookie_buffer", -1);                       \
    cookie_tot_buff_size = cookie_tot_buff_size + string_Length;                                             \
  }                                                                                                          \
}
inline void insert_h2_cookie_in_buf(VUser* vptr, CookieNode *cnode, char **cookie_buf, int *cookie_buf_len, int *cookie_tot_buff_size)
{
  char EQString[2] = "=";
  int EQString_Length = 1;

  NSDL2_HTTP2(vptr, NULL, "Method Called");
  NSDL2_HTTP2(vptr, NULL, "Insert cookie name = %s in http2 table for request", cnode->cookie_name);

  CHECK_COOKIE_BUF_LEN(*cookie_buf, *cookie_tot_buff_size, cnode->cookie_name_len, *cookie_buf_len);
  memcpy(*cookie_buf + *cookie_buf_len, cnode->cookie_name, cnode->cookie_name_len);
  *cookie_buf_len += cnode->cookie_name_len;

  CHECK_COOKIE_BUF_LEN(*cookie_buf, *cookie_tot_buff_size, EQString_Length, *cookie_buf_len);
  memcpy(*cookie_buf + *cookie_buf_len, EQString, EQString_Length);
  *cookie_buf_len += EQString_Length;

  NSDL2_HTTP2(vptr, NULL, "Insert Cookie value = %s in vector for request", cnode->cookie_val);
  CHECK_COOKIE_BUF_LEN(*cookie_buf, *cookie_tot_buff_size, cnode->cookie_val_len, *cookie_buf_len);
  memcpy(*cookie_buf + *cookie_buf_len, cnode->cookie_val, cnode->cookie_val_len);
  *cookie_buf_len += cnode->cookie_val_len;
}


/***************************************************************************************
  Purpose : This method is used for filling the Cookie header in nghhtp2 table.
***************************************************************************************/
inline int insert_h2_auto_cookie(connection* cptr, VUser* vptr, int push_flag)
{
  char *req_url_domain, *req_url_path;
  CookieNode *cnode;
  int first_time = 1;
  char SCString[2] = "; ";
  int SCString_Length = 2;
  char *cookie_buf = NULL;
  int cookie_buf_len = 0;
  int cookie_tot_buff_size;
  int if_cookie = 0; // Flag for checking cookie is present or not

  NSDL2_HTTP2(NULL, cptr, "Method called.");
  req_url_domain = cptr->old_svr_entry->server_name; // Domain for requested url (Must use Mapped host)
  //Path for request

  req_url_path = get_url_req_url(cptr);
  NSDL2_HTTP2(vptr, cptr, "HTTP Request URL Path = %s, URL Domain = %s", req_url_path, req_url_domain);

  cnode = (CookieNode *)vptr->uctable;

  // Cookie: <name>=<value>; <name2>=<value2>\r\n
  //Search cookie name and value from list depend on auto cookie mode
  while ((cnode = search_auto_cookie(cnode, req_url_domain, req_url_path, cptr->old_svr_entry->server_name_len)))
  {
    NSDL2_HTTP2(vptr, cptr, "Cookie Node => %s", cookie_node_to_string(cnode, s_cookie_buf));
    if_cookie = 1;
    if (first_time)  //check for first time to add Cookie header
    {
      NSDL2_HTTP2(vptr, cptr, "Insert 'Cookie: ' in vector for request");
      MY_MALLOC(cookie_buf, 1024, "Cookie_buffer", -1);
      cookie_tot_buff_size = 1024;
  
      first_time = 0;
      if(!push_flag){
        create_ng_http_nv_entries(&idx);
        //Insert "Cookie: " in http2 structure for request, this will insert as cookie header only for first time
        http2_hdr_ptr[idx].name = (uint8_t *)Http2CookieString; // "Cookie: "
        http2_hdr_ptr[idx].namelen = HTTP2_COOKIE_STRING_LEN; // Length of "Cookie: " 
      }
      else{
        fprintf(log_hdr_fp, "%*.*s ", HTTP2_COOKIE_STRING_LEN, HTTP2_COOKIE_STRING_LEN, Http2CookieString);
      }
    }
    else // This means we are adding another cookie (not first cookie)
    {
      NSDL2_HTTP2(vptr, cptr, "Insert '; ' in vector for request");
      //If next node found in list then insert "; " in vector for request
      CHECK_COOKIE_BUF_LEN(cookie_buf, cookie_tot_buff_size, SCString_Length, cookie_buf_len);
      memcpy(cookie_buf + cookie_buf_len, SCString, SCString_Length);
      cookie_buf_len += SCString_Length;
    }
    insert_h2_cookie_in_buf(vptr, cnode, &cookie_buf, &cookie_buf_len, &cookie_tot_buff_size);
    NSDL2_HTTP2(vptr, cptr, "Cookie buf = %*.*s", cookie_buf_len, cookie_buf_len, cookie_buf);
    cnode = cnode->next;
  }

  if(if_cookie)
  {
    if(!push_flag){
      http2_hdr_ptr[idx].value = (uint8_t *)cookie_buf; // "Value of cookie"
      http2_hdr_ptr[idx].valuelen = cookie_buf_len;
    }
    else{
      fprintf(log_hdr_fp, "%*.*s ", cookie_buf_len, cookie_buf_len, cookie_buf);
      /*bug 86575: release cookie_buf in case of server_push*/
      NSDL2_HTTP(NULL, NULL, "releasing cookie_buf = %p", cookie_buf);
      FREE_AND_MAKE_NULL(cookie_buf, "Cookie_buffer", -1);
    }
    NSDL2_HTTP2(NULL, NULL, "Adding cookie header - %s %*.*s at idx = %d", Http2CookieString, http2_hdr_ptr[idx].valuelen, http2_hdr_ptr[idx].valuelen, http2_hdr_ptr[idx].value, idx);
  }
  if(!first_time)  // At least one cookie was added in the vector
  {
    if(!push_flag){
      free_array_http2[idx].name = 0;
      free_array_http2[idx].value = 1;
    }
  }
 
  NSDL2_COOKIES(vptr, cptr, "Returning from insert auto cookie");
  return HTTP2_SUCCESS;
}
inline void make_h2_cookie_segments(int start_idx, connection* cptr, http_request_Shr* request, int push_flag)
{
  VUser* vptr = cptr->vptr;

//  ReqCookTab_Shr* url_cookies = &request->cookies;
//  ReqCookTab_Shr* session_cookies = &(vptr->sess_ptr->cookies);

  NSDL2_HTTP2(NULL, cptr, "Method called. start_idx = %d, global_settings->g_auto_cookie_mode = %d\n",
             start_idx, global_settings->g_auto_cookie_mode);
  if(!is_cookies_disallowed(vptr))
  {
    /*if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
    {
      // NSDL3_HTTP(vptr, cptr, "About to fill cookies, session_cookies = %d, url_cookies = %d", session_cookies->num_cookies, url_cookies->num_cookies);
      // insert cookies for Session
      if (session_cookies->num_cookies)
        next_idx = insert_cookies_http2(session_cookies, next_idx, io_vector_size, vptr);

      // insert cookies for URL
      if (url_cookies->num_cookies)
        next_idx = insert_cookies_http2(url_cookies, next_idx, io_vector_size, vptr);
    }*/
    insert_h2_auto_cookie(cptr, vptr, push_flag);
  }
}

//This method will dump request headers of pushed requests, pushed requests will never have post body
void debug_log_http2_dump_pushed_hdr(connection *cptr, int promised_stream_id)
{
  NSDL1_HTTP2(NULL, NULL, "Method called");
  action_request_Shr* request = cptr->url_num;
  VUser* vptr = cptr->vptr;
  int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  IW_UNUSED(int next_idx = 0;)

  PerHostSvrTableEntry_Shr* svr_entry;

  /* Net diagnostics--BEGIN*/
  char *net_dignostics_buf = NULL;
  char *net_dignostics_hdr_buf;
  long long CavFPInstance;
  /* Net diagnostics--END*/

  /* X-dynaTrace Header Buffer Pointer*/
  char *dynaTrace_hdr_buf = "x-dynatrace";
  char *dynaTraceBuffer = NULL;

  int header_buflen = 1024;
  char *header_buf = NULL;

  //FILE *log_hdr_fp;
  char log_hdr_dump[1024];
  char line_break[] = "\n---------------------------------------------------------------------------------------------------\n";

  GroupSettings *loc_gset = &runprof_table_shr_mem[vptr->group_num].gset;

  MY_MALLOC(header_buf, 1024, "http2 header buf", -1);

  SAVE_REQ_REP_FILES
  sprintf(log_hdr_dump, "%s/logs/%s/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  log_hdr_fp = fopen(log_hdr_dump, "a+");
  if(log_hdr_fp == NULL)
    fprintf(stderr, "Error: Error in opening file for logging pushed request headers\n");
  else
  {
    NSDL2_HTTP2(NULL, NULL, "Logging HTTP2 pushed headers for promised_stream_id = %d", promised_stream_id);

    fprintf(log_hdr_fp, "REQUEST WAS PUSHED\n");
    // Step1a: Fill Method name and value
    LOG_PUSHED_REQUESTS(Http2MethodStr, HTTP2_METHOD_STR_LEN,  http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name, http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name_len -1);       

     // Step1b: Fill scheme 
    if(cptr->url_num->request_type == HTTP_REQUEST){
     LOG_PUSHED_REQUESTS(Http2SchemeStr, HTTP2_SCHEME_STR_LEN, Http2HttpStr, HTTP2_HTTP_STR_LEN);
    } else if(cptr->url_num->request_type == HTTPS_REQUEST) {
     LOG_PUSHED_REQUESTS(Http2SchemeStr, HTTP2_SCHEME_STR_LEN, Http2HttpsStr, HTTP2_HTTPS_STR_LEN);
    }

     // We need actual host to make URL and HOST header 
   svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

   NSDL2_HTTP(vptr, cptr, "Filling Url: svr_entry = [%p], cptr->url = [%p], cptr->url = [%s]",
                          svr_entry, cptr->url, (cptr->url)?cptr->url:NULL);

   if(cptr->url) {
     NSDL3_HTTP(vptr, cptr, "cptr->flags = %0x, proxy_proto_mode = %0x, cptr->url_num->request_type = %d",
                            cptr->flags, runprof_table_shr_mem[vptr->group_num].gset.proxy_proto_mode, cptr->url_num->request_type);
     if(!(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY))
     {
       NSDL3_HTTP(vptr, cptr, "Filling url (non proxy) [%s] of len = %d at %d vector idx",
                            cptr->url, cptr->url_len, next_idx);
       LOG_PUSHED_REQUESTS(Http2PathStr, HTTP2_PATH_STR_LEN, cptr->url, cptr->url_len);
     }
   }

   if(!(disable_headers & NS_HOST_HEADER)) {
     if (use_rec_host == 0) //Send actual host (mapped)
     {
       LOG_PUSHED_REQUESTS(Http2HostStr, HTTP2_HOST_STR_LEN, svr_entry->server_name, svr_entry->server_name_len);
       NSDL2_HTTP(NULL, cptr, "Hostr-Agent header: server_name %s, len %d at index %d",
                               svr_entry->server_name, svr_entry->server_name_len, next_idx);
     }
    else //Send recorded host
    {
      LOG_PUSHED_REQUESTS(Http2HostStr, HTTP2_HOST_STR_LEN, cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_hostname_len);
    }
    NSDL2_HTTP2(NULL, cptr, "The USE_REC_HOST=%d, and server_name=%s", use_rec_host, cptr->url_num->index.svr_ptr->server_hostname);
  }
   // Fill Cookie headers if any cookie to be send
  //For HTTP2 fill all the cookie headers in to vector and copy in buffer and tokenize these to arrange in name value pair
  make_h2_cookie_segments(0, cptr, &(request->proto.http), 1);
  // Step3: Fill validation headers for caching if we need to validate
  if(NS_IF_CACHING_ENABLE_FOR_USER) {
    if(((CacheTable_t*)cptr->cptr_data->cache_data)->cache_flags & NS_CACHE_ENTRY_VALIDATE)
      cache_fill_validators_in_req_http2(cptr->cptr_data->cache_data, 1);
  }

  // Step4: Fill validation headers for caching if we need to validate
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode != NS_TX_HTTP_HEADER_DO_NOT_SEND)
  {
    netcache_fill_trans_hdr_in_req_http2(vptr, cptr, 1);
  } 
  // Authorization Header 
  //To send the previous proxy authorization headers again in case of proxy-chain
  if (cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers");
    int ret = auth_create_authorization_h2_hdr(cptr,vptr->group_num, 1, 1);
    if (ret == AUTH_ERROR)
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  //To send current authorization headers 
  //Proxy authorization headers send in response to 407
  //Server authorization headers send in response to 401
  if(cptr->flags & NS_CPTR_AUTH_MASK)
  { 
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers");
    int ret = auth_create_authorization_h2_hdr(cptr,vptr->group_num, 0, 1);
    if (ret == AUTH_ERROR)
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  
   /* insert the Referer and copy to new one if needed */
  //save_referer is now called from validate_req_code, this is done to implement new referer design Bug 17161
  if(IS_REFERER_NOT_HTTPS_TO_HTTP) {
    if (vptr->referer_size > 3){
      NSDL2_HTTP(NULL, cptr, "vptr->referer = %s", vptr->referer);
      LOG_PUSHED_REQUESTS(Http2RefererStr, HTTP2_REF_STR_LEN, vptr->referer, vptr->referer_size - 2);
    }
  }
  /* insert the User-Agent header */
  if (!(disable_headers & NS_UA_HEADER) && (!GRPC_REQ)) {
    if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
          && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
    {
      int user_agent_val_len = strlen(vptr->browser->UA);
      NSDL3_HTTP(NULL, cptr, "UA string was set by Browser shared memory. So copying this to HTTP vector");
      LOG_PUSHED_REQUESTS(Http2UserAgentStr, HTTP2_USER_AGENT_STR_LEN, vptr->browser->UA, user_agent_val_len -2);

    } else {
      NSDL3_HTTP(NULL, cptr, "UA string was set by API. So copying this to HTTP vector, ua string = [%s] and length = %d", vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len);
      LOG_PUSHED_REQUESTS(Http2UserAgentStr, HTTP2_USER_AGENT_STR_LEN, vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len -2);
      }

  }

  /* insert the standard headers */
  if (!(disable_headers & NS_ACCEPT_HEADER)) {
    LOG_PUSHED_REQUESTS(Http2AceptStr, HTTP2_ACCEPT_STR_LEN, Http2AceptValueStr, HTTP2_ACCEPT_VALUE_STR_LEN);
  }

  if (!(disable_headers & NS_ACCEPT_ENC_HEADER))
    LOG_PUSHED_REQUESTS(Http2AcceptEncStr, HTTP2_ACCEPT_ENC_STR_LEN, Http2AceptEncValueStr, HTTP2_ACCEPT_ENC_VALUE_STR_LEN);           

  /* Insert Net diagnostics Header --BEGIN*/
  if(vptr->flags & NS_ND_ENABLE && global_settings->net_diagnostics_mode == 1) // If net diagnostics keyword enable
  {
    //Calculate FlowPathInstance per nvm 
    CavFPInstance = compute_flowpath_instance();
    MY_MALLOC(net_dignostics_hdr_buf, 128, "Net Diagnostics buffer", -1);
    MY_MALLOC(net_dignostics_buf, 64, "Net Diagnostics buffer", -1);
    int nd_hdr_buf_len = sprintf(net_dignostics_hdr_buf, "%s", global_settings->net_diagnostic_hdr);
    UPPER_TO_LOWER(net_dignostics_hdr_buf, nd_hdr_buf_len - 1);
    int nd_buf_len = sprintf(net_dignostics_buf, "%llu", CavFPInstance);

    LOG_PUSHED_REQUESTS(net_dignostics_hdr_buf, nd_hdr_buf_len,  net_dignostics_buf, nd_buf_len);                                    

    //Updating flowpath instance for current cptr.
    cptr->nd_fp_instance = CavFPInstance;
    NSDL3_HTTP(NULL, cptr, "Flow Path instance at current cptr = %lld", cptr->nd_fp_instance);
    NSDL3_HTTP(NULL, cptr, "Adding Net Diagnostics header in HTTP vector to be send - %s%s", net_dignostics_hdr_buf, net_dignostics_buf);
  }
  /* Net dignostics --END*/

  //insert ns_web_add_header() and ns_web_add_auto_header() API content
  if(vptr->httpData->usr_hdr != NULL)
  {
    switch(cptr->url_num->proto.http.type)
    {
      case MAIN_URL:
      if(vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len > 0)
      {
        NSDL2_HTTP(NULL, cptr, "Inside Main URL: idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff);
       if(vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len > header_buflen){
         MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len, "http2_header_buf", -1);
         header_buflen = vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len;
       }
       strncpy(header_buf, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff, header_buflen);
       tokenise_and_fill_multihdr_http2(header_buf, 1);
      }
      break;

      case EMBEDDED_URL:
      if(vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len > 0)
      {
        NSDL2_HTTP(NULL, cptr, "Inside Embedded URL: idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff);
        if (vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len > header_buflen)
        {
          MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len, "http2_header_buf", -1);
          header_buflen = vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len;
        }
        strncpy(header_buf, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff, header_buflen);
        tokenise_and_fill_multihdr_http2(header_buf, 1);
      }
      break;
    }
    //stored the header data for ALL
    if(vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len > 0)
    {
      NSDL2_HTTP(NULL, cptr, "Inside for ALL(Main URL & Embedded): idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff);
      if (vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len > header_buflen)
      {
        MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len, "http2_header_buf", -1);
        header_buflen = vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len;
      }
      strncpy(header_buf, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff, header_buflen);

      tokenise_and_fill_multihdr_http2(header_buf, 1);
    }

    //insert ns_web_add_auto_header() API content 
    tokenise_and_fill_hdr_http2(cptr, vptr, 1);
  }

  NSDL2_HTTP(NULL, cptr, "releasing header_buf");
  FREE_AND_MAKE_NULL_EX(header_buf, header_buflen, "http2 header buf", -1);
  NSDL2_HTTP(NULL, cptr, "releasing header_buf done");
  /* Insert X-DynaTrace header - Begin - */
  if(IS_DT_HEADER_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    int dtHeaderLen = 0;
    TxInfo *txPtr = (TxInfo *) vptr->tx_info_ptr;
    MY_MALLOC(dynaTraceBuffer, 1024, "X-DynaTrace buffer", -1);
    if(txPtr){
      char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, txPtr->hash_code);
      dtHeaderLen = sprintf(dynaTraceBuffer, "TE=%d;NA=%s", testidx, tx_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }
    else {
      dtHeaderLen = sprintf(dynaTraceBuffer, "TE=%d;NA=%s", testidx, vptr->cur_page->page_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }

    NSDL3_HTTP(NULL, NULL, "dynaTraceBuffer = [%s]", dynaTraceBuffer);

    //Adding header data into http2 structure
    LOG_PUSHED_REQUESTS(dynaTrace_hdr_buf, 11,  dynaTraceBuffer, dtHeaderLen -2);
  }

  /************************** Correlation-ID start ******************************/
  if(IS_CORRELATION_ID_ENABLED)
  {
    static char *cor_id_buf = NULL;
    int corid_header_len = 0;
    //It will be header name buffwe size, which will be 1024(Header+name) - header_name size
    int loc_cor_id_hdr_val_size = (CORR_ID_NAME_VALUE_BUFF_SIZE - loc_gset->correlationIdSettings.header_name_len);

    if(!cor_id_buf)
      MY_MALLOC(cor_id_buf, CORR_ID_NAME_VALUE_BUFF_SIZE, "Correlation-ID buffer", -1);

    //As name value for H2 request are dumped separately
    corid_header_len = ns_cor_id_header_opt(vptr, cptr, cor_id_buf, loc_cor_id_hdr_val_size + 1, FLAG_HTTP2);

    NSDL3_HTTP(NULL, NULL, "cor_id_buf = [%s]", cor_id_buf);

    UPPER_TO_LOWER(loc_gset->correlationIdSettings.header_name, loc_gset->correlationIdSettings.header_name_len);

    //Adding header data into http2 structure
    LOG_PUSHED_REQUESTS(loc_gset->correlationIdSettings.header_name, loc_gset->correlationIdSettings.header_name_len, 
                        cor_id_buf, corid_header_len);
  }
  /************************** Correlation-ID end ******************************/

  /* Insert Network Cache Stats header - Begin - */
  if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    NSDL3_HTTP(NULL, cptr, "Network Cache Stats Headers = [%s], len =[%d]", network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
    tokenise_and_fill_multihdr_http2(network_cache_stats_header_buf_ptr, 1);
  }
  /* Network Cache Stats header - End - */
  if(request->proto.http.header_flags & NS_URL_CLICK_TYPE && vptr->httpData->formencoding)
  {
    char *enctype_header_value_buf = NULL;
    char *enctype_buf_hdr = "content-type";
    MY_MALLOC(enctype_header_value_buf, vptr->httpData->formenc_len + 1, "Content-Type header", -1);
    /* 14 characters for 'Content-Type: ', then the formenc_len, then 2 for \r\n and 1 for terminating null */

    int enctype_header_value_buf_len = sprintf(enctype_header_value_buf, "%s", vptr->httpData->formencoding);
    LOG_PUSHED_REQUESTS(enctype_buf_hdr, 14,  enctype_header_value_buf, enctype_header_value_buf_len);                               
  }

  }
   fprintf(log_hdr_fp, "%s", line_break);
   fclose(log_hdr_fp);
   NSDL1_HTTP2(NULL, NULL, "returning");
}

/*
* name      : fill_h2_req_body
* decription:
* input     :
* output    :
* return    :
*
*/
inline int fill_h2_req_body(connection* cptr, action_request_Shr *request, int *num_vectors)
{

  if((cptr == NULL)  || (request == NULL) || (num_vectors == NULL) )
    return HTTP2_ERROR;

  stream * stream_ptr = cptr->http2->http2_cur_stream;
  VUser* vptr = cptr->vptr;

  if( (stream_ptr == NULL) || (vptr == NULL))
    return HTTP2_ERROR;
  NSDL2_HTTP2(NULL, cptr, "Method called");
  //Fill Body , CheckSum, Content Length
  NS_RESET_IOVEC(g_scratch_io_vector);
  int content_length = fill_req_body_ex(cptr, &g_scratch_io_vector,request);  
  if((runprof_table_shr_mem[vptr->group_num].gset.http_body_chksum_hdr.mode) && (content_length > 0))
  {
    char *checksum_buffer = ns_get_checksum(vptr, &g_scratch_io_vector, 0, content_length); //next_idx not required
    FILL_HEADERS_IN_NGHTTP2(runprof_table_shr_mem[vptr->group_num].gset.http_body_chksum_hdr.h2_hdr_name,
                            runprof_table_shr_mem[vptr->group_num].gset.http_body_chksum_hdr.h2_hdr_name_len,  
                            checksum_buffer, NS_MD5_CHECKSUM_LENGTH, 0, 0);
  }

  /*insert the content length, if there is post content */
  // Changed this condition on July 14, 08 for fixing Bug #240
  // Even if Post data is empty, content length header should go with 0 size
  if (request->proto.http.http_method == HTTP_METHOD_POST ||
      request->proto.http.http_method == HTTP_METHOD_PUT ||
      request->proto.http.http_method == HTTP_METHOD_PATCH ||
      (request->proto.http.http_method == HTTP_METHOD_GET &&
       request->proto.http.post_ptr)) {
    NSDL2_HTTP2(NULL, cptr, "Filling content length header");
    char content_len_value[64];
    cptr->http_payload_sent = content_length;
    int content_len_value_len = sprintf(content_len_value, "%d", content_length);
    FILL_HEADERS_IN_NGHTTP2(Http2ContentLengthStr, HTTP2_CONTENTLENGTH_STR_LEN, content_len_value , content_len_value_len, 0, 0);               
  } else
    cptr->http_payload_sent = 0;


  size_t compress_buflen;
  uint8_t * compress_buf = hdr_deflate_init(cptr, &compress_buflen);

  NSDL3_HTTP(NULL, cptr, "compress_buflen = %d", compress_buflen);

  int ret = pack_header(cptr, compress_buf, compress_buflen, 0, 0, stream_ptr, content_length);
  if (ret == HTTP2_ERROR)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in making frames \n ");
    return ret;
  }

  //Reset
  NS_RESET_IOVEC(g_req_rep_io_vector);
  //fill headers
  NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, compress_buf, ret);
  //fill body
  ret += http2_check_and_fill_data_frame(cptr, &g_scratch_io_vector);
  *num_vectors = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
  NSDL2_HTTP2(NULL, cptr, "num vectors = %d", *num_vectors);
  return ret;
}


/*bug 86023 : todo:the method is part of nslb_iovec. need to remove during upgrading to nslb_iovec*/
void copy_iovec_to_buffer(char** buffer, size_t buffer_size, NSIOVector *nslb_iovec_ptr, const char* msg)
{
  NSDL2_HTTP2(NULL,NULL, "Method called. vctor_ptr=%p msg=%p buffer=%p", nslb_iovec_ptr, msg, *buffer);

  if(!(nslb_iovec_ptr) || !(msg))
    return;

  NSDL2_HTTP2(NULL,NULL,"buffer=%p buffer_size=%d num_vectors=%d", *buffer, buffer_size,NS_GET_IOVEC_CUR_IDX(*nslb_iovec_ptr));
  if(!buffer_size)
  {
    for(int count = 0; count < NS_GET_IOVEC_CUR_IDX(*nslb_iovec_ptr); ++count)
    {
      buffer_size += NS_GET_IOVEC_LEN(*nslb_iovec_ptr, count);
    }
  }

  if( !(*buffer) )
    MY_MALLOC_AND_MEMSET(*buffer, buffer_size +1 , msg, NULL);

  NSDL2_HTTP2(NULL,NULL, " now buffer=%p buffer_size,=%d", *buffer, buffer_size);
  char *tmp_input_buffer = *buffer;
  for(int count = 0; count < NS_GET_IOVEC_CUR_IDX(*nslb_iovec_ptr); ++count)
  {
    memcpy(tmp_input_buffer,  NS_GET_IOVEC_VAL(*nslb_iovec_ptr, count),  NS_GET_IOVEC_LEN(*nslb_iovec_ptr, count));
    tmp_input_buffer +=  NS_GET_IOVEC_LEN(*nslb_iovec_ptr, count);
  }
  buffer[0][buffer_size] = '\0';
  NSDL2_HTTP2(NULL,NULL, " buffer[%p]=%s", *buffer, *buffer);
}


/*bug 52092: added macro to free nghttp2 headers*/
#define FREE_NGHTTP2_HEADERS()														\
for(int i = 0; i < nghttp2_nv_tot_entries; i++)												\
 {\
   NSDL3_HTTP2(NULL, cptr, " Going to free header len = %d, header value len = %d", http2_hdr_ptr[i].namelen, http2_hdr_ptr[i].valuelen);\
   NSDL3_HTTP2(NULL, cptr, " Going to free header name = %*.*s header value %*.*s", http2_hdr_ptr[i].namelen, http2_hdr_ptr[i].namelen,	\
               http2_hdr_ptr[i].name, http2_hdr_ptr[i].valuelen, http2_hdr_ptr[i].valuelen, http2_hdr_ptr[i].value);			\
   if(free_array_http2[i].name)														\
     FREE_AND_MAKE_NOT_NULL_EX(http2_hdr_ptr[i].name, http2_hdr_ptr[i].namelen, "nghttp2 header name", i);				\
   if(free_array_http2[i].value)													\
     FREE_AND_MAKE_NOT_NULL_EX(http2_hdr_ptr[i].value, http2_hdr_ptr[i].valuelen, "nghttp2 header value", i);				\
 }																	\
  NS_DT3(vptr, cptr, DM_L1, MM_SOCKETS, "Starting fetching of page %s URL(%s) on fd = %d. Request line is %s",				\
     cptr->url_num->proto.http.type == MAIN_URL ? "main" : "inline",									\
     get_req_type_by_name(cptr->url_num->request_type),cptr->conn_fd,									\
     cptr->url);															\
 nghttp2_nv_tot_entries = 0;

/***************************************************************************************
  Purpose : This function is used for filling the http2 request headers in nghttp2 
            table, and filling the data in frames.
  
  Input:   It takes cptr, num_vectors and now as input.
  Output:  It will pack the headers in header frame and body in data frame
****************************************************************************************/
inline int http2_make_request (connection* cptr, int *num_vectors, u_ns_ts_t now) {
 
  int next_idx = 0;
  VUser* vptr = cptr->vptr;
  cptr->conn_state = CNST_HTTP2_WRITING; /*bug 51330: set conn_state here so that don't need to set in MAKE_REQUEST() macro*/ 
  NSDL2_HTTP2(vptr, cptr, "Method Called. cptr->conn_state=%d", cptr->conn_state);

  nghttp2_nv_tot_entries = 0;

  // Step1a: Fill Method name and value
  FILL_HEADERS_IN_NGHTTP2(Http2MethodStr, HTTP2_METHOD_STR_LEN,  http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name, http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name_len -1, 0, 0);                                             

  // Step1b: Fill scheme 
  if(cptr->url_num->request_type == HTTP_REQUEST){
    FILL_HEADERS_IN_NGHTTP2(Http2SchemeStr, HTTP2_SCHEME_STR_LEN, Http2HttpStr, HTTP2_HTTP_STR_LEN, 0, 0);
  } else if(cptr->url_num->request_type == HTTPS_REQUEST) {
    FILL_HEADERS_IN_NGHTTP2(Http2SchemeStr, HTTP2_SCHEME_STR_LEN, Http2HttpsStr, HTTP2_HTTPS_STR_LEN, 0, 0);
  }

  // Step1c start: Fill URL 
  // We need actual host to make URL and HOST header 
  PerHostSvrTableEntry_Shr* svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

  NSDL2_HTTP(vptr, cptr, "Filling Url: svr_entry = [%p], cptr->url = [%p], cptr->url = [%s]",
                          svr_entry, cptr->url, (cptr->url)?cptr->url:NULL);

  if(cptr->url) {
    NSDL3_HTTP(vptr, cptr, "cptr->flags = %0x, proxy_proto_mode = %0x, cptr->url_num->request_type = %d",
                            cptr->flags, runprof_table_shr_mem[vptr->group_num].gset.proxy_proto_mode, cptr->url_num->request_type);
    /*bug 52092: add header  :path, irrespective of proxy*/ 
    NSDL3_HTTP(vptr, cptr, "Filling url (non proxy) [%s] of len = %d at %d vector idx",
                            cptr->url, cptr->url_len, next_idx);
    FILL_HEADERS_IN_NGHTTP2(Http2PathStr, HTTP2_PATH_STR_LEN, cptr->url, cptr->url_len, 0, 0);
  }
  // Step1End: Fill method, scheme and path

  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  /* insert the authority header */
  if (!(disable_headers & NS_HOST_HEADER)) {

    int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
     // Added by Anuj 08/03/08
    if (use_rec_host == 0) //Send actual host (mapped)
    {
      //Manish: here we directly get server_entry because it is already set in start_socket
      FILL_HEADERS_IN_NGHTTP2(Http2HostStr, HTTP2_HOST_STR_LEN, svr_entry->server_name, svr_entry->server_name_len, 0, 0);                      
      NSDL2_HTTP(NULL, cptr, "Hostr-Agent header: server_name %s, len %d at index %d",
                               svr_entry->server_name, svr_entry->server_name_len, next_idx);
    }
    else //Send recorded host
    {
      FILL_HEADERS_IN_NGHTTP2(Http2HostStr, HTTP2_HOST_STR_LEN, cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_hostname_len, 0, 0);
    }
    NSDL2_HTTP2(NULL, cptr, "The USE_REC_HOST=%d, and server_name=%s", use_rec_host, cptr->url_num->index.svr_ptr->server_hostname);
  }


  // Fill Cookie headers if any cookie to be send
  // For HTTP2 fill all the cookie headers in to vector and copy in buffer and tokenize these to arrange in name value pair
  action_request_Shr* request = cptr->url_num;
  make_h2_cookie_segments(0, cptr, &(request->proto.http), 0);
  // Step3: Fill validation headers for caching if we need to validate
  if(NS_IF_CACHING_ENABLE_FOR_USER) {
    if(((CacheTable_t*)cptr->cptr_data->cache_data)->cache_flags & NS_CACHE_ENTRY_VALIDATE)
      cache_fill_validators_in_req_http2(cptr->cptr_data->cache_data, 0);
  }

  // Step4: Fill validation headers for caching if we need to validate
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode != NS_TX_HTTP_HEADER_DO_NOT_SEND)
  {
    netcache_fill_trans_hdr_in_req_http2(vptr, cptr, 0);
  }
  int ret;
  /* Authorization Header */
  //To send the previous proxy authorization headers again in case of proxy-chain
  if (cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers");
    ret =  auth_create_authorization_h2_hdr(cptr, vptr->group_num, 1, 0 );
               //auth_create_authorization_hdr(cptr, &body_start_idx, vector, free_array, hdrs_start_idx, vptr->group_num, 1, 1, 0);
    if (ret == AUTH_ERROR)
     // hdrs_start_idx = ret; 
    //else
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  //To send current authorization headers 
  //Proxy authorization headers send in response to 407
  //Server authorization headers send in response to 401
  if(cptr->flags & NS_CPTR_AUTH_MASK)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers ");
    ret =  auth_create_authorization_h2_hdr(cptr, vptr->group_num, 0, 0 );
               //auth_create_authorization_hdr(cptr, &body_start_idx, vector, free_array, hdrs_start_idx, vptr->group_num, 0, 1, 0);
    if (ret == AUTH_ERROR)
      //hdrs_start_idx = ret; 
    //else
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }

  /* insert the Referer and copy to new one if needed */
  //save_referer is now called from validate_req_code, this is done to implement new referer design Bug 17161
  if(IS_REFERER_NOT_HTTPS_TO_HTTP) {
    if (vptr->referer_size > 3){
      NSDL2_HTTP(NULL, cptr, "vptr->referer = %s", vptr->referer);
      FILL_HEADERS_IN_NGHTTP2(Http2RefererStr, HTTP2_REF_STR_LEN, vptr->referer, vptr->referer_size - 2 , 0, 0);
    }
  }

  /* insert the User-Agent header */
  if (!(disable_headers & NS_UA_HEADER) && (!GRPC_REQ)) {
    if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
          && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
    {
      int user_agent_val_len = strlen(vptr->browser->UA);
      NSDL3_HTTP(NULL, cptr, "UA string was set by Browser shared memory. So copying this to HTTP vector");
      FILL_HEADERS_IN_NGHTTP2(Http2UserAgentStr, HTTP2_USER_AGENT_STR_LEN, vptr->browser->UA, user_agent_val_len -2, 0, 0);
    } else {
      NSDL3_HTTP(NULL, cptr, "UA string was set by API. So copying this to HTTP vector, ua string = [%s] and length = %d", vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len);
      FILL_HEADERS_IN_NGHTTP2(Http2UserAgentStr, HTTP2_USER_AGENT_STR_LEN, vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len -2, 0 , 0);
      }

  }

  /* insert the standard headers */
  if (!(disable_headers & NS_ACCEPT_HEADER)) {
    FILL_HEADERS_IN_NGHTTP2(Http2AceptStr, HTTP2_ACCEPT_STR_LEN, Http2AceptValueStr, HTTP2_ACCEPT_VALUE_STR_LEN, 0, 0);
  }

  if (!(disable_headers & NS_ACCEPT_ENC_HEADER)) {//globals.no_compression == 0)
    FILL_HEADERS_IN_NGHTTP2(Http2AcceptEncStr, HTTP2_ACCEPT_ENC_STR_LEN, Http2AceptEncValueStr, HTTP2_ACCEPT_ENC_VALUE_STR_LEN, 0, 0);           }

  /* Insert Net diagnostics Header --BEGIN*/
  if(vptr->flags & NS_ND_ENABLE && global_settings->net_diagnostics_mode == 1) // If net diagnostics keyword enable
  {
    char *net_dignostics_buf;
    char * net_dignostics_hdr_buf;
    //Calculate FlowPathInstance per nvm 
    long long CavFPInstance = compute_flowpath_instance();
    MY_MALLOC(net_dignostics_hdr_buf, 128, "Net Diagnostics buffer", -1);
    MY_MALLOC(net_dignostics_buf, 64, "Net Diagnostics buffer", -1);
    int nd_hdr_buf_len = sprintf(net_dignostics_hdr_buf, "%s", global_settings->net_diagnostic_hdr);
    UPPER_TO_LOWER(net_dignostics_hdr_buf, nd_hdr_buf_len - 1);
    int nd_buf_len = sprintf(net_dignostics_buf, "%llu", CavFPInstance);

    FILL_HEADERS_IN_NGHTTP2(net_dignostics_hdr_buf, nd_hdr_buf_len,  net_dignostics_buf, nd_buf_len, 1, 1);                            
    //Updating flowpath instance for current cptr.
    cptr->nd_fp_instance = CavFPInstance;
    NSDL3_HTTP(NULL, cptr, "Flow Path instance at current cptr = %lld", cptr->nd_fp_instance);
    NSDL3_HTTP(NULL, cptr, "Adding Net Diagnostics header in HTTP vector to be send - %s%s", net_dignostics_hdr_buf, net_dignostics_buf);
  }
  /* Net dignostics --END*/
  /*bug 54315: moved header_buf inside if block and added 1 in reaaloc and replaced strncpy by strcpy*/
   //insert ns_web_add_header() and ns_web_add_auto_header() API content
  if(vptr->httpData->usr_hdr != NULL)
  {
    int header_buflen = 1024;
    char *header_buf = NULL;
    MY_MALLOC(header_buf, 1024, "http2 header buf", -1);
    switch(cptr->url_num->proto.http.type)
    {
      case MAIN_URL:
      if(vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len > 0)
      {
        NSDL2_HTTP(NULL, cptr, "Inside Main URL: idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff);
       if(vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len > header_buflen){
         MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len + 1, "http2_header_buf", -1);
         header_buflen = vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len;
       }
       header_buf[header_buflen] = '\0';
       strcpy(header_buf, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff);
       tokenise_and_fill_multihdr_http2(header_buf, 0);
      }
      break;

      case EMBEDDED_URL:
      if(vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len > 0)
      {
        NSDL2_HTTP(NULL, cptr, "Inside Embedded URL: idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff);
        if (vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len > header_buflen)
        {
          MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len + 1, "http2_header_buf", -1);
          header_buflen = vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len;
        }
        header_buf[header_buflen] = '\0';
        strcpy(header_buf, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff);
        tokenise_and_fill_multihdr_http2(header_buf, 0);
        }
      break;
    }
    //stored the header data for ALL
    if(vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len > 0)
    {
      NSDL2_HTTP(NULL, cptr, "Inside for ALL(Main URL & Embedded): idx = %d, used_len= %d, head_buffer = %s", idx, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff);
      if (vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len > header_buflen)
      {
        MY_REALLOC(header_buf, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len + 1, "http2_header_buf", -1);
        header_buflen = vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len;
      }
      header_buf[header_buflen] = '\0';
      strcpy(header_buf, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff);
      tokenise_and_fill_multihdr_http2(header_buf, 0);
    }

    //insert ns_web_add_auto_header() API content 
    tokenise_and_fill_hdr_http2(cptr, vptr, 0);
    FREE_AND_MAKE_NULL_EX(header_buf, header_buflen, "http2 header buf", -1);
  }

  /* Insert X-DynaTrace header - Begin - */
  if(IS_DT_HEADER_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    char *dynaTrace_hdr_buf = "x-dynatrace";
    static char *dynaTraceBuffer = NULL;

    int dtHeaderLen = 0;
    TxInfo *txPtr = (TxInfo *) vptr->tx_info_ptr;
    if(!dynaTraceBuffer)
      MY_MALLOC(dynaTraceBuffer, 1024, "X-DynaTrace buffer", -1);

    if(txPtr){
      char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, txPtr->hash_code);
      dtHeaderLen = sprintf(dynaTraceBuffer, "TE=%d;NA=%s", testidx, tx_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }
    else {
      dtHeaderLen = sprintf(dynaTraceBuffer, "TE=%d;NA=%s", testidx, vptr->cur_page->page_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }

    NSDL3_HTTP(NULL, NULL, "dynaTraceBuffer = [%s]", dynaTraceBuffer);

    //Adding header data into http2 structure
    FILL_HEADERS_IN_NGHTTP2(dynaTrace_hdr_buf, 11,  dynaTraceBuffer, dtHeaderLen -2, 0 , 0);
  }

  /************************** Correlation-ID start ******************************/
  GroupSettings *loc_gset = &runprof_table_shr_mem[vptr->group_num].gset;
  if(IS_CORRELATION_ID_ENABLED)
  {
    static char *cor_id_buf = NULL;
    int corid_header_len = 0;

    //It will be header name buffer size, which will be 1024(Header+name) - header_name size
    int loc_cor_id_hdr_val_size = (CORR_ID_NAME_VALUE_BUFF_SIZE - loc_gset->correlationIdSettings.header_name_len);

    if(!cor_id_buf)
      MY_MALLOC(cor_id_buf, CORR_ID_NAME_VALUE_BUFF_SIZE, "Correlation-ID buffer", -1);

    //As name value for H2 request are dumped separately
    corid_header_len = ns_cor_id_header_opt(vptr, cptr, cor_id_buf, loc_cor_id_hdr_val_size+1,FLAG_HTTP2);

    NSDL3_HTTP(NULL, NULL, "cor_id_buf = [%s]", cor_id_buf);

    UPPER_TO_LOWER(loc_gset->correlationIdSettings.header_name, loc_gset->correlationIdSettings.header_name_len);

    //Adding header data into http2 structure
    FILL_HEADERS_IN_NGHTTP2(loc_gset->correlationIdSettings.header_name, loc_gset->correlationIdSettings.header_name_len,
                            cor_id_buf, corid_header_len, 0 , 0);
  }
  /************************** Correlation-ID end ******************************/

  /* Insert Network Cache Stats header - Begin - */
  if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    NSDL3_HTTP(NULL, cptr, "Network Cache Stats Headers = [%s], len =[%d]", network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
    tokenise_and_fill_multihdr_http2(network_cache_stats_header_buf_ptr, 0);
  }
  /* Network Cache Stats header - End - */
  if(request->proto.http.header_flags & NS_URL_CLICK_TYPE && vptr->httpData->formencoding)
  {
    char *enctype_header_value_buf = NULL;
    char *enctype_buf_hdr = "content-type";
    MY_MALLOC(enctype_header_value_buf, vptr->httpData->formenc_len + 1, "Content-Type header", -1);
    /* 14 characters for 'Content-Type: ', then the formenc_len, then 2 for \r\n and 1 for terminating null */

    int enctype_header_value_buf_len = sprintf(enctype_header_value_buf, "%s", vptr->httpData->formencoding);
    FILL_HEADERS_IN_NGHTTP2(enctype_buf_hdr, 14,  enctype_header_value_buf, enctype_header_value_buf_len, 0, 1);                                }

  NS_RESET_IOVEC(g_scratch_io_vector); /* bug 86023: NS crash*/
  //Use g_scrach_io_vector
  int content_length = 0;   
  if ((next_idx = insert_segments(vptr, cptr, &(request->proto.http.hdrs), &g_scratch_io_vector,
                        &content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
    
    return next_idx; // Insert segment failed with error
  }

  // Next three insert segments are done for G_HTTP_HDR
  // Table was filled in reference to relative page_id
  int pg_id = vptr->cur_page->page_id - session_table_shr_mem[vptr->sess_ptr->sess_id].first_page->page_id;

  // Next Three insert segments are done for G_HTTP_HDR 
  if ((next_idx = insert_segments(vptr, cptr, &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].AllUrlHeaderBuf, 
                        &g_scratch_io_vector, &content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
    
    return next_idx; // Insert segment failed with error
  }
  if(cptr->url_num->proto.http.type == MAIN_URL)
  {
    if ((next_idx = insert_segments(vptr, cptr, &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].MainUrlHeaderBuf, 
                        &g_scratch_io_vector,&content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
    
      return next_idx; // Insert segment failed with error
    }
  }
  if(cptr->url_num->proto.http.type == EMBEDDED_URL)
  {    
    if ((next_idx = insert_segments(vptr,cptr, &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].InlineUrlHeaderBuf, 
                    &g_scratch_io_vector, &content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
    
      return next_idx; // Insert segment failed with error
    }
  }
  //int i;
  int http_size = 0;
  NSDL2_HTTP2(NULL, NULL, "Num vector is [%d] ", *num_vectors);
  /*bug 86023 : todo: the for loop will be replaced by http_size = g_scratch_io_vector.iov_buf_len , during upgrading to nslb_iovec*/
  for (int i=0; i < next_idx; i++) {
    http_size +=  g_scratch_io_vector.vector[i].iov_len;
  }

  
  NSDL2_HTTP2(NULL, NULL, "ns_nvm_scratch_buf_size = %d, total_body_size = %d", ns_nvm_scratch_buf_size, http_size);
  if(ns_nvm_scratch_buf_size < http_size)
  {
    MY_REALLOC(ns_nvm_scratch_buf, http_size, "reallocating for custom header http_size", -1);
    ns_nvm_scratch_buf_size = http_size;
  }

  char* buffer = ns_nvm_scratch_buf;
  copy_iovec_to_buffer(&buffer, ns_nvm_scratch_buf_size, &g_scratch_io_vector, "Custome Headers"); 

  //copy_request_into_buffer(cptr, http_size, &g_scratch_io_vector);
  tokenise_and_fill_multihdr_http2(/*cptr->free_array*/buffer, 0);

  // From here onwards we will communicate on streams  
  int error_code = 0;
  stream * stream_ptr = get_free_stream(cptr, &error_code);
  if (!stream_ptr) {
    if (error_code == NS_STREAM_ERROR_NULL){
      NSDL2_HTTP2(NULL, NULL, "Unable to get free stream");
      return HTTP2_ERROR;
    }
    if (error_code == NS_STREAM_ERROR) {
      NSDL2_HTTP2(NULL, NULL, "Going to call retry connection. Stream id has ececeed limit on connection.");
     return NS_STREAM_ERROR;
    }
  }
  cptr->http2->http2_cur_stream = stream_ptr;
  cptr->http2->http2_cur_stream->url = cptr->url;
  cptr->http2->http2_cur_stream->url_len = cptr->url_len;

  if ((ns_h2_fill_stream_into_map(cptr, stream_ptr) == HTTP2_ERROR))
  {
    NSDL2_HTTP2(NULL, NULL, "Error : Unable to fill stream stream into map ");
    return HTTP2_ERROR;
  }

  ret = fill_h2_req_body(cptr, request, num_vectors);
    if ( ret < HTTP2_SUCCESS )
    {
      return ret;
    }
  //Todo: to fix below mem leak in case there is any error during fill_h2_req_body()
  NSDL3_HTTP2(NULL, cptr, "complete Data Frame length = %d nghttp2_nv_tot_entries = %d Frame Len=%d", ret, nghttp2_nv_tot_entries,MAX_FRAME_HEADER_LEN);
 
  #ifdef NS_DEBUG_ON  
  debug_log_http2_dump_req(cptr, g_scratch_io_vector.vector, 0 , NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector));
  #else
  if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
    log_http2_dump_req(cptr, g_scratch_io_vector.vector, 0 , NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector));
  #endif

  /*bug 84661 --> avoid adding ret value*/
  if(cptr->request_type == HTTP_REQUEST) { //TODO
    /*bug 51330 --> only added HeaderFrame Size*/
    stream_ptr->flow_control.remote_window_size +=  MAX_FRAME_HEADER_LEN;
    /*bug 84661 init window_update_from_server to flow_control.remote_window_size*/
    stream_ptr->window_update_from_server = stream_ptr->flow_control.remote_window_size;
  }
  cptr->conn_state = CNST_HTTP2_WRITING; /*bug 51330: set state to CNST_HTTP2_WRITING*/
  NSDL2_HTTP2(NULL, NULL, " sptr->flow_control.remote_window_size=%d ret[%d] cptr->conn_state=%d", stream_ptr->flow_control.remote_window_size,ret, cptr->conn_state);
  // Freeing nghttp2 header name and header value, if value of free array is 1
  /*bug 52092 : added macro . todo remove below commented code*/
  FREE_NGHTTP2_HEADERS()
  return NS_NOT_CACHED;
}


/***************************************************************************************
  Purpose :bug 52092  This function is used for filling the http2 request headers in nghttp2 
            table, and filling the data in frames.
  Fill only below Headers:
    :method -->  CONNECT
    : authority --> Destination Server I.P:PORT 
  Input:   It takes cptr, num_vectors and now as input.
  Output:  It will pack the headers in header frame and body in data frame
****************************************************************************************/
inline int http2_make_proxy_request (connection* cptr, int *num_vectors, u_ns_ts_t now) {
 
  VUser* vptr = cptr->vptr;

  NSDL2_HTTP2(vptr, cptr, "Method Called. cptr->conn_state=%d", cptr->conn_state);
  /*reset total header count to 0*/
  nghttp2_nv_tot_entries = 0;

  // Step1: Fill Method name and value
  FILL_HEADERS_IN_NGHTTP2(Http2MethodStr, HTTP2_METHOD_STR_LEN,H2_CONNECT,H2_CONNECT_LEN, 0, 0);                                             


  // Step2  start: Fill  header authority
  // We need actual host to make URL and HOST header 
  PerHostSvrTableEntry_Shr* svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

  NSDL2_HTTP(vptr, cptr, "Filling Url: svr_entry = [%p], cptr->url = [%p], cptr->url = [%s]",
                          svr_entry, cptr->url, (cptr->url)?cptr->url:NULL);

  /* insert the authority header */
  int next_idx = 0;
  FILL_HEADERS_IN_NGHTTP2(Http2HostStr, HTTP2_HOST_STR_LEN, svr_entry->server_name, svr_entry->server_name_len, 0, 0);                      
  NSDL2_HTTP(NULL, cptr, "Hostr-Agent header: server_name %s, len %d at index %d",
                               svr_entry->server_name, svr_entry->server_name_len, next_idx);

  int ret;
  /* Authorization Header */
  if (cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers");
    ret =  auth_create_authorization_h2_hdr(cptr, vptr->group_num, 1, 0 );
    if (ret == AUTH_ERROR)
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  //To send current authorization headers 
  //Proxy authorization headers send in response to 407
  //Server authorization headers send in response to 401
  if(cptr->flags & NS_CPTR_AUTH_MASK)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers ");
    ret =  auth_create_authorization_h2_hdr(cptr, vptr->group_num, 0, 0 );
    if (ret == AUTH_ERROR)
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  action_request_Shr* request = cptr->url_num;
  //Use g_scrach_io_vector
  int content_length = 0;   
  if ((next_idx = insert_segments(vptr, cptr, &(request->proto.http.hdrs), &g_scratch_io_vector,
                        &content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
    return next_idx; // Insert segment failed with error
  } 

  // From here onwards we will communicate on streams  
  int error_code = 0;
  //cptr->http2->max_stream_id = -1;
  stream * stream_ptr = get_free_stream(cptr, &error_code);
  if (!stream_ptr) {
    if (error_code == NS_STREAM_ERROR_NULL){
      NSDL2_HTTP2(NULL, NULL, "Unable to get free stream");
      return HTTP2_ERROR;
    }
    if (error_code == NS_STREAM_ERROR) {
      NSDL2_HTTP2(NULL, NULL, "Going to call retry connection. Stream id has ececeed limit on connection.");
     return NS_STREAM_ERROR;
    }
  }
  cptr->http2->http2_cur_stream = stream_ptr;
  cptr->http2->http2_cur_stream->url = cptr->url;
  cptr->http2->http2_cur_stream->url_len = cptr->url_len;

  if ((ns_h2_fill_stream_into_map(cptr, stream_ptr) == HTTP2_ERROR))
  {
    NSDL2_HTTP2(NULL, NULL, "Error : Unable to fill stream stream into map ");
    return HTTP2_ERROR;
  }

  ret = fill_h2_req_body(cptr, request, num_vectors);
  if ( ret < HTTP2_SUCCESS )
      return ret;
  
  stream_ptr->num_vectors = *num_vectors;
  cptr->conn_state = CNST_HTTP2_WRITING; /*bug 51330: set state to CNST_HTTP2_WRITING*/
  NSDL3_HTTP2(NULL, cptr, "complete headers frame length = %d nghttp2_nv_tot_entries = %d cptr->conn_state=%d", ret, nghttp2_nv_tot_entries,cptr->conn_state);
 
  // Freeing nghttp2 header name and header value, if value of free array is 1
  FREE_NGHTTP2_HEADERS()
  return NS_GET_IOVEC_LEN(g_req_rep_io_vector, 0) ;//NS_NOT_CACHED;
}

/*bug 54315: removed copy for location_url*/
int copy_cptr_to_stream(connection *cptr,  stream *stream_ptr){

  NSDL2_HTTP2(NULL, NULL, "Mehtod Called, tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);
 
  if(!stream_ptr){
    return -1;
  }
  stream_ptr->is_cptr_data_saved = 1;
  NSDL2_HTTP2(NULL, NULL, "stream_ptr->is_cptr_data_saved = [%d] and stream_ptr is [%p] stream id is [%d] ",                                                              stream_ptr->is_cptr_data_saved, stream_ptr, stream_ptr->stream_id);
  stream_ptr->compression_type = cptr->compression_type;
  stream_ptr->url_num = cptr->url_num;
  stream_ptr->link_hdr_val = cptr->link_hdr_val; 
  if(!stream_ptr->url){
    stream_ptr->url = cptr->url; 
    stream_ptr->url_len = cptr->url_len; 
  }
  stream_ptr->redirect_count = cptr->redirect_count; 
  stream_ptr->conn_state = cptr->conn_state; 
  stream_ptr->header_state = cptr->header_state; 
  stream_ptr->chunked_state = cptr->chunked_state; 
  stream_ptr->proto_state = cptr->proto_state; 
  stream_ptr->cookie_hash_code = cptr->cookie_hash_code; 
  stream_ptr->cookie_idx = cptr->cookie_idx; 
  stream_ptr->gServerTable_idx = cptr->gServerTable_idx; 
  stream_ptr->started_at = cptr->started_at;
  stream_ptr->ns_component_start_time_stamp = cptr->ns_component_start_time_stamp;
  stream_ptr->dns_lookup_time = cptr->dns_lookup_time;
  stream_ptr->write_complete_time = cptr->write_complete_time;
  stream_ptr->first_byte_rcv_time = cptr->first_byte_rcv_time;
  stream_ptr->request_complete_time = cptr->request_complete_time;
  stream_ptr->http_payload_sent = cptr->http_payload_sent;
  stream_ptr->tcp_bytes_sent = cptr->tcp_bytes_sent;
  stream_ptr->req_ok = cptr->req_ok;
  stream_ptr->completion_code = cptr->completion_code;
  stream_ptr->num_retries = cptr->num_retries;
  stream_ptr->request_type = cptr->request_type;
  stream_ptr->req_code = cptr->req_code;
  stream_ptr->req_code_filled = cptr->req_code_filled;
  stream_ptr->content_length = cptr->content_length;
  stream_ptr->bytes = cptr->bytes;
  stream_ptr->tcp_bytes_recv = cptr->tcp_bytes_recv;
  stream_ptr->total_bytes = cptr->total_bytes;
  stream_ptr->body_offset = cptr->body_offset;
  stream_ptr->checksum = cptr->checksum;
  stream_ptr->dir = cptr->dir;
  stream_ptr->ctxt = cptr->ctxt;
  stream_ptr->timer_ptr = cptr->timer_ptr;
  //stream_ptr->buf_head = cptr->buf_head;
  //stream_ptr->cur_buf = cptr->cur_buf;
  stream_ptr->free_array = cptr->free_array;
  stream_ptr->send_vector = cptr->send_vector;
  stream_ptr->bytes_left_to_send = cptr->bytes_left_to_send;
  stream_ptr->num_send_vectors = cptr->num_send_vectors;
  stream_ptr->first_vector_offset = cptr->first_vector_offset;
  stream_ptr->last_iov_base = cptr->last_iov_base;
  stream_ptr->req_file_fd = cptr->req_file_fd;
  stream_ptr->flags = cptr->flags;
  stream_ptr->prev_digest_len = cptr->prev_digest_len;
  stream_ptr->prev_digest_msg = cptr->prev_digest_msg;
  stream_ptr->cptr_data = (struct cptr_data_t *)cptr->cptr_data;
  //stream_ptr->cptr_data = cptr->cptr_data;
  //stream_ptr->x_dynaTrace_data = cptr->x_dynaTrace_data;
  NSDL2_HTTP2(NULL, NULL,  "stream_ptr[%p]->bytes_left_to_send=%d stream_ptr->gServerTable_idx=%d", stream_ptr, 
							stream_ptr->bytes_left_to_send,stream_ptr->gServerTable_idx);
  return 0;
}

/*bug 54315: removed copy for location_url*/
int copy_stream_to_cptr(connection *cptr, stream *stream_ptr){

  NSDL2_HTTP2(NULL, NULL, "Mehtod Called");
  if(!stream_ptr){
    NSDL2_HTTP2(NULL, NULL, "stream_ptr is null");
    return -1;
  }
  if(!stream_ptr->is_cptr_data_saved) {
    NSDL2_HTTP2(NULL, NULL, "is_cptr_data_saved is not set %p", stream_ptr);
    return 0;
  }
  NSDL2_HTTP2(NULL, NULL, "stream ptr is %p for stream id %d stream_ptr->bytes_left_to_send=%d",
								 stream_ptr, stream_ptr->stream_id,stream_ptr->bytes_left_to_send);
  cptr->compression_type = stream_ptr->compression_type;
  cptr->url_num = stream_ptr->url_num;
  cptr->link_hdr_val = stream_ptr->link_hdr_val; 
  cptr->url = stream_ptr->url; 
  cptr->url_len = stream_ptr->url_len; 
  cptr->redirect_count = stream_ptr->redirect_count; 
  cptr->conn_state = stream_ptr->conn_state; 
  cptr->header_state = stream_ptr->header_state; 
  cptr->chunked_state = stream_ptr->chunked_state; 
  cptr->proto_state = stream_ptr->proto_state; 
  cptr->cookie_hash_code = stream_ptr->cookie_hash_code; 
  cptr->cookie_idx = stream_ptr->cookie_idx; 
  cptr->gServerTable_idx = stream_ptr->gServerTable_idx; 
  cptr->started_at = stream_ptr->started_at;
  cptr->ns_component_start_time_stamp = stream_ptr->ns_component_start_time_stamp;
  cptr->dns_lookup_time = stream_ptr->dns_lookup_time;
  cptr->write_complete_time = stream_ptr->write_complete_time;
  cptr->first_byte_rcv_time = stream_ptr->first_byte_rcv_time;
  cptr->request_complete_time = stream_ptr->request_complete_time;
  cptr->http_payload_sent = stream_ptr->http_payload_sent;
  cptr->tcp_bytes_sent = stream_ptr->tcp_bytes_sent;
  cptr->req_ok = stream_ptr->req_ok;
  cptr->completion_code = stream_ptr->completion_code;
  cptr->num_retries = stream_ptr->num_retries;
  cptr->request_type = stream_ptr->request_type;
  cptr->req_code = stream_ptr->req_code;
  cptr->req_code_filled = stream_ptr->req_code_filled;
  cptr->content_length = stream_ptr->content_length;
  cptr->bytes = stream_ptr->bytes;
  cptr->tcp_bytes_recv = stream_ptr->tcp_bytes_recv;
  cptr->total_bytes = stream_ptr->total_bytes;
  cptr->body_offset = stream_ptr->body_offset;
  cptr->checksum = stream_ptr->checksum;
  cptr->dir = stream_ptr->dir;
  cptr->ctxt = stream_ptr->ctxt;
  cptr->timer_ptr = stream_ptr->timer_ptr;
  //cptr->buf_head = stream_ptr->buf_head;
  //cptr->cur_buf = stream_ptr->cur_buf;
  cptr->free_array = stream_ptr->free_array;
  cptr->send_vector = stream_ptr->send_vector;
  cptr->bytes_left_to_send = stream_ptr->bytes_left_to_send;
  cptr->num_send_vectors = stream_ptr->num_send_vectors;
  cptr->first_vector_offset = stream_ptr->first_vector_offset;
  cptr->last_iov_base = stream_ptr->last_iov_base;
  cptr->req_file_fd = stream_ptr->req_file_fd;
  cptr->flags = stream_ptr->flags;
  cptr->prev_digest_len = stream_ptr->prev_digest_len;
  cptr->prev_digest_msg = stream_ptr->prev_digest_msg;
  cptr->cptr_data = (struct cptr_data_t *)stream_ptr->cptr_data;
  //cptr->x_dynaTrace_data = stream_ptr->x_dynaTrace_data;
  NSDL2_HTTP2(NULL, NULL, "tcp_bytes_sent = %d, bytes_left_to_send = %d cptr->gServerTable_idx =%d",
                                                                     cptr->tcp_bytes_sent, cptr->bytes_left_to_send,cptr->gServerTable_idx);
  return 0;
}
