/******************************************************************
 * Name    : ns_url_req.c 
 * Author  : Archana
 * Purpose : This file contains methods for processing of URL request 
 * Note:
 * Modification History:
 * 08/10/08 - Initial Version
*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>

#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
#include "ns_page_dump.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "amf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_url_req.h"
#include "ns_alloc.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_random_vars.h"
#include "ns_random_string.h"
#include "ns_index_vars.h"
#include "ns_unique_numbers.h"
#include "ns_date_vars.h"
#include "ns_http_cache.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_http_script_parse.h"
#include "ns_click_script.h"
#include "nslb_encode.h"
#include "nslb_hessian.h"
#include "ns_nd_kw_parse.h" //added for Net Diagnostics
#include "ns_http_auth.h"
#include "nslb_comman_api.h"
#include "ns_network_cache_reporting.h"

#include "nslb_hash_code.h"
#include "nslb_util.h"
#include "nslb_encode_decode.h"
#include "protobuf/nslb_protobuf_adapter.h"
#include "comp_decomp/nslb_comp_decomp.h"

#include "ns_url_resp.h"
#include "ns_vars.h"


#include "ns_trans.h"
#include "ns_trans_parse.h"
#include "ns_parent.h"
#include "ns_trace_log.h"
#include "ns_script_parse.h"
#include "ns_user_define_headers.h"
#include "ns_vuser_tasks.h"
#include "nslb_time_stamp.h"
#include "ns_java_obj_mgr.h"
#include "nslb_base64_url.h"
#include "nslb_cav_conf.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_nd_integration.h"
#include "ns_test_monitor.h"

/*Make it enums*/
char *http_version_buffers[] = {" HTTP/1.0\r\n", " HTTP/1.1\r\n", " HTTP/2.0\r\n"}; 
char http_version_buffers_len[] = { 11, 11, 11}; 

char* Host_header_buf = "Host: ";

//Host header's \r\n is set in next headers's begining
//#define USER_AGENT_STRING_LENGTH 12
char* User_Agent_buf = "User-Agent: ";

char* Referer_buf = "Referer: ";

char content_length_buf[POST_CONTENT_HDR_SIZE] = "Content-Length: ";
const int post_content_val_size = POST_CONTENT_HDR_SIZE - POST_CONTENT_VAR_SIZE;
char* post_content_ptr = content_length_buf + POST_CONTENT_VAR_SIZE;

char* Accept_buf = "Accept: text/xml,application/xml,application/json,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,video/x-mng,image/png,image/jpeg,image/gif;q=0.2,text/css,*/*;q=0.8\r\n";

char* Accept_enc_buf = "Accept-Encoding: gzip, deflate, br, compress;q=0.9\r\n";

char* keep_alive_buf = "Keep-Alive: 300\r\n";

char* connection_buf = "Connection: keep-alive\r\n";

char CRLFString[3] = "\r\n";

char* inline_prefix = "InLine.";
int inline_prefix_len = 7; 

int CRLFString_Length = 2;

#define NUM_ADDITIONAL_HEADERS 9
//static int num_additional_headers;

static char *connection_header_buf = "Connection: Upgrade, HTTP2-Settings\r\n";
#define CONNECTION_HDR_BUF_LEN 37

char *upgrade_header_buf = "Upgrade: h2c\r\n";
#define UPGRADE_HDR_BUF_LEN 14

char *http2_settings_buf = "HTTP2-Settings: AAMAAABkAARAAAAAAAIAAAAA\r\n";
#define HTTP2_SETTINGS_BUF_LEN 42

int make_http2_setting_frame(char **frame, int *frame_len);

char*
chk_ns_escape(int encode_type, int req_part, int body_encoding, int is_malloc_needed, char *in_buf, int in_size,
              int *out_size, int *free_array, char *specific_char, char *encodespaceby);

void add_http_checksum_header(VUser *vptr, int body_start_idx, int http_body_chksum_hdr_idx, int body_size);

void copy_iovec_to_buffer(char** buffer, size_t buffer_size, NSIOVector *nslb_iovec_ptr, const char* msg);
#ifdef NS_DEBUG_ON
char *show_seg_ptr_type(int seg_ptr)
{
  NSDL2_VARS(NULL, NULL, "Method called, seg_ptr = %d", seg_ptr);
  if (seg_ptr == STR)                 return ("STR");
  if (seg_ptr == VAR)                 return ("VAR");
  if (seg_ptr == COOKIE_VAR)          return ("COOKIE_VAR");
  if (seg_ptr == NSL_VAR)             return ("NSL_VAR");
  if (seg_ptr == TAG_VAR)             return ("TAG_VAR");
  if (seg_ptr == DYN_VAR)             return ("DYN_VAR");
  if (seg_ptr == SEARCH_VAR)          return ("SEARCH_VAR");
  if (seg_ptr == JSON_VAR)            return ("JSON_VAR");
  if (seg_ptr == RANDOM_VAR)          return ("RANDOM_VAR");
  if (seg_ptr == RANDOM_STRING)       return ("RANDOM_STRING");  
  if (seg_ptr == DATE_VAR)            return ("DATE_VAR");  
  if (seg_ptr == UNIQUE_RANGE_VAR)    return ("UNIQUE_RANGE_VAR");  
#ifdef RMI_MODE
  if (seg_ptr == UTF_VAR)             return ("UTF_VAR");
  if (seg_ptr == BYTE_VAR)            return ("BYTE_VAR");
  if (seg_ptr == LONG_VAR)            return ("LONG_VAR");
#endif
  if (seg_ptr == CLUST_VAR)           return ("CLUST_VAR");
  if (seg_ptr == GROUP_VAR)           return ("GROUP_VAR");
  if (seg_ptr == GROUP_NAME_VAR)      return ("GROUP_NAME_VAR");
  if (seg_ptr == CLUST_NAME_VAR)      return ("CLUST_NAME_VAR");
  if (seg_ptr == USERPROF_NAME_VAR)   return ("USERPROF_NAME_VAR");
  if (seg_ptr == HTTP_VERSION_VAR)    return ("HTTP_VERSION_VAR");
  if (seg_ptr == SEGMENT)             return ("SEGMENT");
  else                                return ("Invalid type of variable");
}
#endif /* NS_DEBUG_ON */

void set_num_additional_headers()
{
  NSDL2_HTTP(NULL, NULL, "Method called.");
  
  int num_additional_headers;
  int i;

  /* Here we will have to make them group wise  */
  for (i = 0; i < total_runprof_entries; i++) {
    num_additional_headers = NUM_ADDITIONAL_HEADERS;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_UA_HEADER)
      num_additional_headers -= 2;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_HOST_HEADER)
      num_additional_headers -= 3;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_ACCEPT_HEADER)
      num_additional_headers--;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_ACCEPT_ENC_HEADER)
      num_additional_headers--;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_CONNECTION_HEADER)
      num_additional_headers--;
    if (runprof_table_shr_mem[i].gset.disable_headers & NS_KA_HEADER)
      num_additional_headers--;

    // If Network Cache stats is enabled, then we need to send few headers as one iovector. So increament it
    if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(i))
      num_additional_headers++;

    runprof_table_shr_mem[i].gset.num_additional_headers = num_additional_headers;
    NSDL2_HTTP(NULL, NULL, "Num additional headers for scenario group [%d] = %d", i, num_additional_headers);
  }
}

int remove_newline_frm_param_val(char *value, int value_len, char *out_buf)
{
  NSDL1_VARS(NULL, NULL, "Method called, value_len = %d", value_len);
  char *rem_newline_val = out_buf;
  int new_len = 0, idx = 0;
  while(idx < value_len)
  {
    if (value[idx] == '\n') 
    {
      idx += 1;
      continue;
    }
    else if ((value[idx] == '\r') && (value[idx++] == '\n')) {
      idx += 2;
      continue;
    }
    rem_newline_val[new_len] = value[idx];
    new_len ++;
    idx ++;
  }
  rem_newline_val[new_len + 1] = 0;
  return (new_len);
}

char*
chk_ns_escape(int encode_type, int req_part, int body_encoding, int is_malloc_needed, char *in_buf, int in_size,
              int *out_size, int *free_array, char *specific_char, char *encodespaceby)
{
  char *ptr;
  int need_to_rem_newline = 0;
  
  NSDL2_VARS(NULL, NULL, "Method called. encode_type = %d, req_part = %d, body_encoding = %d, is_malloc_needed = %d, in_buf = %s, in_size = %d, specific_char = %s, encodespaceby = %s", encode_type, req_part, body_encoding, is_malloc_needed, in_buf, in_size, specific_char, encodespaceby);

  /*
   In Kohls enviornment URLs in response was having newline in URL href tag:
   For example: 
   <li><a href="/catalog/bed-and-bath.jsp?CN=4294719803&N=3000063429&cc=bed_bath-LN0.0-S-BedBathCloseouts
      /catalog/bed-and-bath.jsp?CN=4294719803&N=3000063429+4294719803&cc=bed_bath-LN0.0-S-BedBathCloseouts
      /catalog/bed-and-bath.jsp?CN=4294719803&N=3000063429+4294719803&cc=bed_bath-LN0.0-S-BedBathCloseouts">
   <b/>Closeouts</b></font></a> </li> 

  Browsers remove these newline from URL path similary in NS we need to remove newline from parameter value 
  in case of URL or Header parameterization
  */
  if ((req_part != REQ_PART_BODY) && ((strpbrk(in_buf, "\r\n")) != NULL))
  {
    need_to_rem_newline = is_malloc_needed = 1;
    NSDL2_VARS(NULL, NULL, "Newline found in parameter value set flag need_to_rem_newline = %d"
              " need to malloc buffer hence set flag is_malloc_needed = %d", need_to_rem_newline, is_malloc_needed);
  }
  if(encode_type == ENCODE_NONE || req_part == REQ_PART_HEADERS || (req_part == REQ_PART_BODY && body_encoding == 0))
  {
    if(is_malloc_needed == 1)
    {
      //fprintf(stderr, "Length of in_buf = %d\n", strlen(in_buf));
      /*in_size should not have added length for '\0'*/
      MY_MALLOC(ptr, in_size+1, "ptr", -1);
      if (need_to_rem_newline) // Remove new line from parameterized header
        *out_size = remove_newline_frm_param_val(in_buf, in_size, ptr);  
      else {  
        *out_size = in_size;       
        memcpy(ptr, in_buf, in_size);
      }
      *free_array = 1; 
      return ptr;
    }
    else
    { 
      *out_size = in_size;       
      *free_array = 0; 
      return in_buf; 
    }
  }

  *free_array = 1;
  return(ns_escape(in_buf, in_size, out_size, specific_char, encodespaceby, need_to_rem_newline));//In case of removing newline from parameterized URL
}

#if 0
char *ns_escape(const char *string, int length, int *out_len, char *specified_char, char *EncodeSpaceBy)
{
  int alloc = (length?length:(int)strlen(string))+1; // Allocate 1 extra for null termination
  char *ns;
  unsigned char in;
  int newlen = alloc;
  int index=0;

  NSDL1_VARS(NULL, NULL, "Method called. length = %d, alloc_len = %d, specified_char = %p, EncodeSpaceBy = %s", length, alloc, specified_char, EncodeSpaceBy);
  NSDL4_VARS(NULL, NULL, "Input string = %s", string);

  MY_MALLOC(ns, alloc, "ns", -1);
  length = alloc - 1; // Reduce be 1 as we are allocating one extra for NULL termination
  while(length--) {
    in = *string;
    NSDL1_VARS(NULL, NULL, "Char = %c", in);
    if((specified_char == NULL && !isalnum(in) && (in != '.') && (in != '+') && (in != '_') && (in != '-')) || (specified_char != NULL && specified_char[(int)in])) {
      /* encode it */
      NSDL2_VARS(NULL, NULL, "Entered inside to encode the char = %c", in);
      newlen += 2; /* the size grows with two, since this'll become a %XX */
      if(newlen > alloc) {
        int old_size = alloc;
        // Note this will realloc on first char to be escaped
        NSDL4_VARS(NULL, NULL, "Reallocating buffer. Old size = %d, new size = %d", alloc, alloc * 2);
        alloc *= 2;
        MY_REALLOC_EX(ns, alloc, old_size, "ns", -1); 
      }
      if (in != ' ') {

	sprintf(&ns[index], "%%%02X", in);
        NSDL1_VARS(NULL, NULL, "Replacing Char = %c to %s at index = %d", in, &ns[index], index);
	index += 3;
      }
      else {
	index += sprintf(&ns[index], "%s", EncodeSpaceBy); // it should not be > 3
        NSDL1_VARS(NULL, NULL, "Replacing Char = %c to %s at index = %d", in, &ns[index], index);
      }
    }
    else {
      /* just copy this */
      NSDL2_VARS(NULL, NULL, "Copying char = %c into ns without encoding at index = %d", in, index);
      ns[index++]=in;
    }
    string++;
  }

  ns[index]=0; /* terminate it */
  *out_len = index;
  NSDL3_VARS(NULL, NULL, "Encode values = %s, length of encoded string = %d", ns, *out_len);
  return ns;
}
#endif
 
/*int insert_segments(const StrEnt_Shr* seg_tab_ptr, NSIOVector *ns_iovec, VUser *vptr, int* content_size, 
                    int body_encoding, int var_val_flag, int req_part, int cur_seq, action_request_Shr* request) {

  NSDL2_VARS(vptr, NULL, "Method Called, start_idx=%d end_idx=%d content=%s", 
                         start_idx, end_idx, content_size?"Y":"N");

  ret = insert_segments_ex(cptr, seg_tab_ptr, vector, free_array, start_idx, end_idx, vptr, content_size, body_encoding, var_val_flag, req_part, request, cur_seq);
  return ret;

}*/

#define ENCODE_REQ_BODY(ns_iovec, body, bodysize, free_array_flag) \
{ \
  int amf_version; \
  if(body_encoding == BODY_ENCODING_AMF) \
  { \
    if(request->proto.http.header_flags & NS_URL_BODY_AMF0) \
      amf_version = 0; \
    else \
      amf_version = 3; \
    amf_encode_value ((ns_iovec).vector, (ns_iovec).cur_idx, body, bodysize, amf_version);\
    if(free_array_flag == 1) \
      (ns_iovec).flags[(ns_iovec).cur_idx] |= NS_IOVEC_FREE_FLAG; \
  } \
  else if(body_encoding == BODY_ENCODING_PROTOBUF) \
  { \
    unsigned char *buffer;\
    int buff_len = bodysize + 8 + 10 + 8 + 8; \
    MY_MALLOC(buffer, buff_len, "Allocate memory for Protobuf encoded field", -1); \
    buff_len = nslb_encode_protobuf_field(request->proto.http.protobuf_urlattr_shr.req_message, \
                                     seg_ptr->pb_field_number, seg_ptr->pb_field_type, body, bodysize, \
                                     buffer, buff_len, NULL, 0, 1); \
    NS_FILL_IOVEC_AND_MARK_FREE(ns_iovec, buffer, buff_len);\
  } \
}

/*this function should be called only if used_param table initiated */
//#define NS_SAVE_USED_PARAM(...) if(vptr->httpData->up_t.used_param) ns_save_used_param(__VA_ARGS__)  

/**
 * This function is called both for construction of request and for displaying
 * in logs (debug/trace). So, we will have to pass a flag var_val_flag in order
 * to not use value from parametrization pool in case we are using this for logging.
 */
/* req_part = REQ_PART_REQ_LINE or REQ_PART_HEADERS, REQ_PART_BODY */

/* Note - body_encoding is > 0 only for filling body*/
/* Returns with the number of segments inserted in case of success
   otherwise with negative error values:
   ret = -2 Use Once Once Abort
   ret = -5,-1 Maximum Limit of Vector reached
   ret = -3, 4 Stream Error HTTP2
*/
int insert_segments(VUser *vptr, connection *cptr, const StrEnt_Shr* seg_tab_ptr, NSIOVector *ns_iovec, 
                    int* content_size, int body_encoding, int var_val_flag, int req_part, 
                    action_request_Shr* request, int cur_seq) {

  int i;
  SegTableEntry_Shr* seg_ptr = seg_tab_ptr->seg_start;
  int num_seg = seg_tab_ptr->num_entries;
  PointerTableEntry_Shr* value;
  UserVarEntry* uservar_entry;
  ClustValTableEntry_Shr* clust_val;
  GroupValTableEntry_Shr* group_val;
  VarTableEntry_Shr *fparam_var = NULL;
  
  int num_entered = 0;
  int out_size;
  int j, search_var_idx, json_var_idx, tag_var_idx;
  char length_flag;
  unsigned long long now ;
  int prev_status;
  int free_memory;
  char *escape_buff;
  static char value_buffer[64];

  NSDL2_VARS(vptr, NULL, "Method Called, cur_idx = %d, total_size = %d, content=%s, num_seg=%d", 
                          NS_GET_IOVEC_CUR_IDX(*ns_iovec), ns_iovec->tot_size, content_size?"Y":"N", num_seg);

  if (content_size)
    *content_size = 0;

  NS_CHK_AND_GROW_IOVEC(vptr, *ns_iovec, num_seg);
  for (i = ns_iovec->cur_idx; (i < ns_iovec->tot_size) && (num_entered < num_seg) ; i++, seg_ptr++, num_entered++) {
    NSDL1_VARS(vptr, NULL, "vector index = %d, seg_ptr->type is %s, num_entered = %d", i, show_seg_ptr_type(seg_ptr->type), num_entered);
    length_flag = 1;
    free_memory = 0;
    switch (seg_ptr->type) {

    case STR:
      NS_FILL_IOVEC(*ns_iovec, seg_ptr->seg_ptr.str_ptr->big_buf_pointer, seg_ptr->seg_ptr.str_ptr->size); 
      break;

    case PROTOBUF_MSG:
    {
      char *ptr;
      int len = seg_ptr->seg_ptr.str_ptr->size;

      MY_MALLOC(ptr, len, "ProtoBufStartMsg", -1);
      memcpy(ptr, seg_ptr->seg_ptr.str_ptr->big_buf_pointer, len);
      NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, ptr, len);
      break;
    }

    case VAR:
    case INDEX_VAR:
      NSDL4_VARS(vptr, NULL, "type = %d, page_status = %d, cptr = %p, vptr->sess_status = %d", 
                              seg_ptr->type, vptr->page_status, cptr, vptr->sess_status);
      prev_status = vptr->page_status;
      NSDL4_VARS(vptr, NULL, "my_port_index = %u, seg_ptr.fparam_hash_code = %d", my_port_index, seg_ptr->seg_ptr.fparam_hash_code);
      if(seg_ptr->type == VAR) 
      {
        fparam_var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
        value = get_var_val(vptr, var_val_flag, seg_ptr->seg_ptr.fparam_hash_code);
      }
      else
      {
        value = get_index_var_val(seg_ptr->seg_ptr.var_ptr, vptr, var_val_flag, cur_seq);
      }
          
      now = get_ms_stamp(); 
      //in case of useonce, when all values are exhausted, check for OnUseOnce err
      if(vptr->page_status == NS_USEONCE_ABORT){
        //TODO: Need to do discuss
        NS_FILL_IOVEC(*ns_iovec, null_iovec, NULL_IOVEC_LEN); 
        if(cptr)
        {
          NSDL1_VARS(vptr, NULL, "Use once data is over for parameter. Aborting current session");

          if(vptr->sess_status == NS_USEONCE_ABORT && request->proto.http.type == MAIN_URL) // If we have to abort session and it is main url 
            vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION;

          if(vptr->page_status == NS_USEONCE_ABORT && request->proto.http.type != MAIN_URL){ //for inline url we are resetting page
            vptr->page_status = 0;
            if(vptr->sess_status == NS_USEONCE_ABORT)
              vptr->sess_status = 0;
          }

          if(cptr->conn_fd > 0) {// close connection in case of reuse connection
            // BugID:31704 : setting conn_state to CNST_WRITING, as in case of second or subsequent request on a connection, conn_state will be
            // CNST_REUSE and in close_fd idle timer is not deleted. In case user goes in session pacing after close connection, idle timer
            // expiry will take user again to Close connection, but here state of user is waiting and due to this nsi_end_session will be 
            // called again, that will decreament active user count (while user is already in waitng state). Due to this active users are
            // becoming negative.
            cptr->conn_state = CNST_WRITING;
            Close_connection(cptr, 1, now, NS_USEONCE_ABORT, NS_COMPLETION_USEONCE_ERROR);
          }
          else
          {
            vptr->urls_awaited--;
	    stop_user_immediately(vptr, now);
          }

          if(request->proto.http.type == EMBEDDED_URL)
            vptr->page_status = prev_status;
        }
        else //This will happen only in click and script cases we are passing NULL in cptr
        {
          NSDL1_VARS(vptr, NULL, "cptr is NULL: Use once data is over for parameter. Aborting current session");

          if(vptr->sess_status == NS_USEONCE_ABORT)
            vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION;

          vptr->urls_awaited--;
        }
        return MR_USE_ONCE_ABORT; // Do not continue below
      }
  
      if(value == NULL)
      {
        NSDL2_VARS(vptr, NULL, "NULL value for index variable, now sending blank   session_status = [%d]", vptr->sess_status);
        //TODO: Need to do discuss
        NS_FILL_IOVEC(*ns_iovec, null_iovec, NULL_IOVEC_LEN);
        /*do we need to save this entry ? */
        NS_SAVE_USED_PARAM(vptr, seg_ptr, NULL, 0, req_part, 0, 0); 
      }
      else
      {
        /*  save parameter used in used_param table  */
        NS_SAVE_USED_PARAM(vptr, seg_ptr, value->big_buf_pointer, value->size, req_part, 0, 0);

        NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s",
                              i, value->size, value->size, value->size, value->big_buf_pointer);
        if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF)) 
        {
          ENCODE_REQ_BODY(*ns_iovec, value->big_buf_pointer, value->size, 1);
        }
        else
        {
          GroupTableEntry_Shr* group_ptr = (seg_ptr->type == VAR)?fparam_var->group_ptr:seg_ptr->seg_ptr.var_ptr->group_ptr; 
          
          escape_buff = chk_ns_escape(group_ptr->encode_type, req_part, body_encoding, 0, 
                                             value->big_buf_pointer, value->size, &out_size, &free_memory, 
                                             group_ptr->encode_chars, 
                                             group_ptr->encode_space_by);

          NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
        }
      }
      break;

    case COOKIE_VAR: {
      // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
      // here only for Manual Cookies
      UserCookieEntry *cookie_entry = &vptr->uctable[seg_ptr->seg_ptr.cookie_hash_code];
      if (cookie_entry->length > 0) {
        NS_FILL_IOVEC(*ns_iovec, cookie_entry->cookie_value, cookie_entry->length);
      } else { //TODO:
        NS_FILL_IOVEC(*ns_iovec, null_iovec, NULL_IOVEC_LEN);
      }
      NS_SAVE_USED_PARAM(vptr, seg_ptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, req_part, 0, 0);
      break;
    }
#ifdef RMI_MODE
    case UTF_VAR:
      //fparam_var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
      value = get_var_val(vptr, var_val_flag, seg_ptr->seg_ptr.fparam_hash_code);
      NS_FILL_IOVEC(*ns_iovec, value->big_buf_pointer, value->size);
      break;
    case BYTE_VAR: {
      UserByteVarEntry* bytevar_entry = &vptr->ubvtable[seg_ptr->seg_ptr.bytevar_hash_code];
      if (bytevar_entry->length > 0) {
        NS_FILL_IOVEC(*ns_iovec, bytevar_entry->length, bytevar_entry->bytevar_value);
      }
      break;
    }
    case LONG_VAR:
      //fparam_var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
      value = get_var_val(vptr, var_val_flag, seg_ptr->seg_ptr.fparam_hash_code);
      NS_FILL_IOVEC(*ns_iovec, value->big_buf_pointer, value->size);
      break;
#endif
    case TAG_VAR:
    case SEARCH_VAR:
    case JSON_VAR:
    case NSL_VAR:
    {
      int val_index = (seg_ptr->data)?*(int *)seg_ptr->data:0;
      char *value = NULL;
      int value_len = 0;

      uservar_entry = &vptr->uvtable[seg_ptr->seg_ptr.var_idx];

      // vector_flag  will be used to check if variable is vector or scalor
      char vector_flag = vptr->sess_ptr->var_type_table_shr_mem[seg_ptr->seg_ptr.var_idx];
      // Repeat block, declare array/Search Var/Tag Var, used with out index in repeat block
      if(cur_seq != SEG_IS_NOT_REPEAT_BLOCK && vector_flag && !val_index){
         // If cur_seq is less than uservar_entry->length , then fill the value else fill the error messag at its index
         // "Error: Repeat block sequence-----"
         if(cur_seq < uservar_entry->length){
          NSDL2_VARS(vptr, NULL, " Inside NSL_VAR/TAG_VAR/SEARCH_VAR/JSON_VAR cur_seq = %d, uservar_entry->length = %d", cur_seq, uservar_entry->length);

          value = uservar_entry->value.array[cur_seq].value;
          value_len = uservar_entry->value.array[cur_seq].length;
          if(!value && seg_ptr->type == NSL_VAR) /*If no value set at that index then check for default value in case for declare array*/
            value = get_nsl_var_default_value(vptr->sess_ptr->sess_id, seg_ptr->seg_ptr.var_idx, &value_len, vptr->group_num);
        }
        else{ // Trying to index seq more than variable array sizein repeat block
          NSDL2_VARS(vptr, NULL, "Cur_seq = %d reached to max value(%d)", cur_seq, uservar_entry->length);
          value = value_buffer;
          value_len = sprintf(value, "Index(%d)ReachedOutOfBound", cur_seq);
        }
      }
      else{ 
        // Check if NSL_VAR/SEARCH/JSON/TAG  is scalar(In case of scalar val_index will be 0, case of count -1 and rest ..)
        if(!val_index){
          if(!vector_flag){ // This is the case for non repeat block and declare var is non array type & search/tag var is ord specific or any
            value = uservar_entry->value.value;
            value_len = uservar_entry->length;
            if(!value && seg_ptr->type == NSL_VAR) /*If no value set at that index then check for default value in case for declare array*/
              value = get_nsl_var_default_value(vptr->sess_ptr->sess_id, seg_ptr->seg_ptr.var_idx, &value_len, vptr->group_num);
          }else{
            /*This is the case when it is not repeatable block and vector is used without inex*/
            // That is if nsl_decl_array(dec, SIZE=10, DefaultValue="abs")is declared
            // Same for search param , if search param is vector ans user givevalue without index
            // If user gives dec without index then it will fill empty value
            //right now we are setting emptry string, but here should be error message*/
            value = NULL;
            value_len = 0;
           }
         }
         // This is the case for declare/search/tag var count       
         else if(val_index == -1){
          NSDL3_VARS(vptr, NULL, "Index _Count used");
          value = value_buffer;
          value_len = sprintf(value, "%d", uservar_entry->length);
        }
        else { // Searc/Tag/JSON/decalre arry
          if(val_index < 0 || val_index > uservar_entry->length){ /* check index out of bound */
            NSDL3_VARS(vptr, NULL, "Index (%d) is out of bound, max limit = %d", val_index, uservar_entry->length);
            value = value_buffer;
            value_len = sprintf(value, "Index(%d)OutofBound", val_index);
          }
          else { /*Valid index */
            value = uservar_entry->value.array[val_index - 1].value;
            value_len = uservar_entry->value.array[val_index - 1].length;
            if(!value && seg_ptr->type == NSL_VAR) /*If no value set at that index then check for default value*/
              value = get_nsl_var_default_value(vptr->sess_ptr->sess_id, seg_ptr->seg_ptr.var_idx, &value_len, vptr->group_num);
              NSDL3_VARS(vptr, NULL, "Index(%d) used, value = *.*s", val_index, value_len, value_len, value);
          }
        }
      }
      NSDL2_VARS(vptr, NULL, "NSL/Tag/Search/JSON Parameter Value = %*.*s, value lenght = %d", value_len, value_len, value, value_len);     

      //save to used_table
      if(value == value_buffer) { /* in this case we need to malloc valur */
        NS_SAVE_USED_PARAM(vptr, seg_ptr, value, value_len, req_part, 1, cur_seq + 1);
      }  else {
        NS_SAVE_USED_PARAM(vptr, seg_ptr, value, value_len, req_part, 0, cur_seq + 1); 
      }
 
      if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF))
      {
	if(value)
          ENCODE_REQ_BODY(*ns_iovec, value, value_len, 0);
      }
      else
      {
        if(seg_ptr->type == SEARCH_VAR  && value){
          //search var table index 
          int var_trans_table_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];
          search_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].var_idx;

          SearchVarTableEntry_Shr *search_var_entry =  &searchvar_table_shr_mem[search_var_idx]; 

          escape_buff = chk_ns_escape(search_var_entry->encode_type, req_part, body_encoding, 1, 
                                      value, value_len, &out_size, &free_memory, search_var_entry->encode_chars, 
                                      search_var_entry->encode_space_by);
          
          NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
          break;

        }else if(seg_ptr->type == JSON_VAR  && value){
          //json var table index 
          int var_trans_table_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];
          json_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].var_idx;

          JSONVarTableEntry_Shr *json_var_entry =  &jsonvar_table_shr_mem[json_var_idx];
       
          escape_buff = chk_ns_escape(json_var_entry->encode_type, req_part, body_encoding, 1, value, value_len, &out_size, &free_memory, json_var_entry->encode_chars, json_var_entry->encode_space_by);

          NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
          break;

        }else if(seg_ptr->type == TAG_VAR && value){
          int var_trans_table_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];
          tag_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].var_idx;

          NodeVarTableEntry_Shr *tag_var_entry =  &nodevar_table_shr_mem[tag_var_idx];
	  escape_buff = chk_ns_escape(tag_var_entry->encode_type, req_part, body_encoding, 1, value, value_len, &out_size, &free_memory, tag_var_entry->encode_chars, tag_var_entry->encode_space_by);

           NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
           break;
          }
        //In case of declare variable need to verify whether parameter value contains newline and used URL or Header
        NSDL2_VARS(vptr, NULL, "value_len = %d, req_part = %d", value_len, req_part);
        if (value_len && (req_part != REQ_PART_BODY) && ((strpbrk(value, "\r\n")) != NULL)) 
        {
          char *ptr;
          int len;
          MY_MALLOC(ptr, value_len + 1, "ptr", -1);
          len = remove_newline_frm_param_val(value, value_len, ptr); 
          NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, ptr, len);
        } else {  
          NS_FILL_IOVEC(*ns_iovec, value, value_len);
        }
        /* don't reset free_array as it have been set above*/
      }
      break;
    }

    /* when a variable is used multiple times and
    * Refresh = USE, all values of the variable need to be retained until
    * end of session. However, the values for the same variable point to the
    * same index in the uvtable, which then
    * retains only the last value - hence only this last value gets free'd
    * in clear_var_table(). If we set the bit in free_array[] for this
    * memory,it gets
    * freed after the request is sent - and is not available
    * for ns_eval_string(). Hence we need to use different memory for the
    * values in the vector[]. Along with this, we also free previous value
    * in uvtable[] in get_random_var_value(), at the index where we allocate a new value */
    //Above statement is true for RANDOM_VAR,UNIQUE_VAR, RANDOM_STRING,DATE_VAR.    
    //for random vars 
    case RANDOM_VAR: 
    {
      int len;
      char *random_value;

      random_value = get_random_var_value(seg_ptr->seg_ptr.random_ptr, vptr, var_val_flag, &len);
      NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s", i, len, len, len, random_value);

      //save in used_param 
      NS_SAVE_USED_PARAM(vptr, seg_ptr, random_value, len, req_part, 1, 0);
 
      if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF)) 
      {
        ENCODE_REQ_BODY(*ns_iovec, random_value, len, 1);
      }
      else
      {
        escape_buff = chk_ns_escape(ENCODE_ALL, req_part, body_encoding, 1, random_value, len, &out_size, &free_memory, NULL, "+");
        NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
      }
      break;
    }
    
    case UNIQUE_VAR:
    {
      int len;
      char *unique_value;

      unique_value = get_unique_var_value(seg_ptr->seg_ptr.unique_ptr, vptr, var_val_flag, &len);
      NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s", i, len, len, len, unique_value);

      //save in used param
      NS_SAVE_USED_PARAM(vptr, seg_ptr, unique_value, len, req_part, 1, 0);

      if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF)) 
      {
        ENCODE_REQ_BODY(*ns_iovec, unique_value, len, 1);
      }
      else
      {
        escape_buff = chk_ns_escape(ENCODE_ALL, req_part, body_encoding, 1, unique_value, len, &out_size, &free_memory, NULL, "+");
        NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
      }
      break;
    }

    case UNIQUE_RANGE_VAR:
    {
      int len;
      int unique_var_idx;
      char *unique_range_var_val;
      int var_trans_table_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];
      unique_var_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].var_idx;
      NSDL2_VARS(vptr, NULL, "unique_range_var idx  = %d", unique_var_idx);

      now = get_ms_stamp();
      unique_range_var_val = get_unique_range_var_value(vptr, var_val_flag, &len, unique_var_idx);
      NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s", i, len, len, len, unique_range_var_val);

      NS_SAVE_USED_PARAM(vptr, seg_ptr, unique_range_var_val, len, req_part, 1, 0);
      //in case of Action is SESSION_FAILURE, when range is exhausted
      HANDLE_UNIQUE_RANGE_EXHAUSTED(now);
      
      if(unique_range_var_val) {
        escape_buff = chk_ns_escape(ENCODE_ALL, req_part, body_encoding, 1, unique_range_var_val, len, &out_size, &free_memory, NULL, "+");
        NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
      }
      break;
    }
    case RANDOM_STRING:
    {
      int len;
      char *random_value;
      random_value = get_random_string_value(seg_ptr->seg_ptr.random_str, vptr, var_val_flag, &len);
      NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s", i, len, len, len, random_value);

      //save in used param
      NS_SAVE_USED_PARAM(vptr, seg_ptr, random_value, len, req_part, 1, 0);
 
      if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF)) 
      {
        ENCODE_REQ_BODY(*ns_iovec, random_value, len, 1);
      }
      else
      {
        escape_buff = chk_ns_escape(ENCODE_ALL, req_part, body_encoding, 1, random_value, len, &out_size, &free_memory, NULL, "+");
        NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
      }
      break;
    }

    case DATE_VAR:
    {
      int len;
      char *date_value;
      int out_size;
      date_value = get_date_var_value(seg_ptr->seg_ptr.date_ptr, vptr, var_val_flag, &len); 
      NSDL2_VARS(vptr, NULL, "Vector id = %d, value_len = %d, value = %*.*s", i, len, len, len, date_value);
      NS_SAVE_USED_PARAM(vptr, seg_ptr, date_value, len, req_part, 1, 0);
 
      if(req_part == REQ_PART_BODY && (body_encoding == BODY_ENCODING_AMF || body_encoding == BODY_ENCODING_PROTOBUF)) 
      {
        ENCODE_REQ_BODY(*ns_iovec, date_value, len, 1);
      }
      else
      {
        escape_buff = chk_ns_escape(ENCODE_ALL, req_part, body_encoding, 1, date_value, len, &out_size, &free_memory, NULL, "+");
        NS_CHK_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, escape_buff, out_size, free_memory);
      }
      break;
    }

    case CLUST_VAR:
      NSDL1_VARS(vptr, NULL, "clust id is %d, var idx is %d, the clust of the var is %d", vptr->clust_id,
      seg_ptr->seg_ptr.var_idx,
      seg_ptr->seg_ptr.var_idx * total_clust_entries + vptr->clust_id);
      clust_val = &clust_table_shr_mem[ seg_ptr->seg_ptr.var_idx * total_clust_entries + vptr->clust_id];
      NSDL1_VARS(vptr, NULL, "the value is %s, the clust id is %d, the seg_ptr idx is %d", clust_val->value, vptr->clust_id, seg_ptr->seg_ptr.var_idx);
      NS_FILL_IOVEC(*ns_iovec, clust_val->value, clust_val->length);

      NS_SAVE_USED_PARAM(vptr, seg_ptr, clust_val->value, clust_val->length, req_part, 0, 0);
      break;

    case GROUP_VAR:
      NSDL1_VARS(vptr, NULL, "group id is %d, var idx is %d, the group of the var is %d", vptr->group_num,
      seg_ptr->seg_ptr.var_idx,
      seg_ptr->seg_ptr.var_idx * total_runprof_entries + vptr->group_num);
      group_val = &rungroup_table_shr_mem[ seg_ptr->seg_ptr.var_idx * total_runprof_entries + vptr->group_num];
      NSDL1_VARS(vptr, NULL, "the value is %s, the group id is %d, the seg_ptr idx is %d", group_val->value, vptr->group_num, seg_ptr->seg_ptr.var_idx);
      NS_FILL_IOVEC(*ns_iovec, group_val->value, group_val->length);
   
      NS_SAVE_USED_PARAM(vptr, seg_ptr, group_val->value, group_val->length, req_part, 0, 0); 
      break;

    case CLUST_NAME_VAR:
      NSDL1_VARS(vptr, NULL, "clust id is %d, cluster name is %s", vptr->clust_id, runprof_table_shr_mem[vptr->group_num].cluster_name);
      NS_FILL_IOVEC(*ns_iovec, runprof_table_shr_mem[vptr->group_num].cluster_name, 
                              strlen(runprof_table_shr_mem[vptr->group_num].cluster_name));  //TODO: should fill in parsing time
      
      NS_SAVE_USED_PARAM(vptr, seg_ptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, req_part, 0, 0);
      break;

    case GROUP_NAME_VAR:
      NSDL1_VARS(vptr, NULL, "group id is %d, group name is %s", vptr->group_num, runprof_table_shr_mem[vptr->group_num].scen_group_name);
      NS_FILL_IOVEC(*ns_iovec, runprof_table_shr_mem[vptr->group_num].scen_group_name,
                              strlen(runprof_table_shr_mem[vptr->group_num].scen_group_name));  //TODO:

      NS_SAVE_USED_PARAM(vptr, seg_ptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, req_part, 0, 0);
      break;

    case USERPROF_NAME_VAR:
      NSDL1_VARS(vptr, NULL, "group id is %d, group name is %s userprof is %s", vptr->group_num,
      runprof_table_shr_mem[vptr->group_num].scen_group_name,
      runprof_table_shr_mem[vptr->group_num].userindexprof_ptr->name);
      NS_FILL_IOVEC(*ns_iovec, runprof_table_shr_mem[vptr->group_num].userindexprof_ptr->name,
                              strlen(runprof_table_shr_mem[vptr->group_num].userindexprof_ptr->name));

      NS_SAVE_USED_PARAM(vptr, seg_ptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, req_part, 0, 0);
      break;

    case HTTP_VERSION_VAR:
      NS_FILL_IOVEC(*ns_iovec, http_version_values[0], strlen(http_version_values[0])); //TODO:

      NS_SAVE_USED_PARAM(vptr, seg_ptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, req_part, 0, 0);
      break;
    case SEGMENT:
      {
        int num_repeat_count = 0; // For repeat Block
        length_flag = 0; // 
        int  var_idx_local;
        int local_con_len;
        char tmp_buf[256 + 1]; 
        RepeatBlock_Shr *rep_seg_entry = &repeat_block_shr_mem[seg_ptr->seg_ptr.var_idx]; 
        int agg_repeat_segment = rep_seg_entry->agg_repeat_segments; 
        NSDL1_HTTP(vptr, NULL, "Repeat Segment, agg_repeat_segment  = %d", agg_repeat_segment);
        //Check for count.
        if(rep_seg_entry->repeat_count_type == COUNT) {
          /* this should be removed, we should reset hash_code from var_idx to uvtable_idx */
          int uvtable_idx = session_table_shr_mem[vptr->sess_ptr->sess_id].vars_trans_table_shr_mem[rep_seg_entry->hash_code].user_var_table_idx;
          num_repeat_count = vptr->uvtable[uvtable_idx].length;
          NSDL1_HTTP(vptr, NULL, "Repeat Segment, Count on Variable length, var idx = %d, uvtable_idx = %d, cout = %d",
                                   rep_seg_entry->hash_code, uvtable_idx, num_repeat_count);
        }
        else if(rep_seg_entry->repeat_count_type == VALUE){
          NSDL1_HTTP(vptr, NULL, "Repeate Segment, Count on value of variable, variable string = %s", rep_seg_entry->data);
          //first get hashcode and then find type.
          int hashcode = vptr->sess_ptr->var_hash_func(rep_seg_entry->data, strlen(rep_seg_entry->data)); 
          if(hashcode < 0)
          {
             NSDL1_HTTP(vptr, NULL, "Repeate Segment, hash code not found for random parameter= %s", rep_seg_entry->data);
          }
          // Changes done for fixing bug in case of repeat block bug 7686, In case of random number, save the value 
          // 
          VarTransTableEntry_Shr* var_ptr;
          var_ptr = &vptr->sess_ptr->vars_trans_table_shr_mem[hashcode];
          var_idx_local = var_ptr->var_idx;
          NSDL1_HTTP(vptr, NULL, "Repeate Segment, var_idx_local = %d, var_ptr->var_type = %d", var_idx_local, var_ptr->var_type);
          if(var_ptr->var_type == RANDOM_VAR)
          {
            RandomVarTableEntry_Shr *random_var = &randomvar_table_shr_mem[var_idx_local]; 
            NSDL2_HTTP(vptr, NULL, "Random var found on count, hashcode = %d, var_idx = %d, var_name = %s", hashcode, var_idx_local, random_var->var_name);
            // Fetch the value
            int length;
            //TODO: handle error conditions.
            num_repeat_count = atoi(get_random_var_value(random_var, vptr, var_val_flag, &length));
          }
          else{
            sprintf(tmp_buf, "{%s}", rep_seg_entry->data);
            num_repeat_count = atoi(ns_eval_string(tmp_buf));
          }
        }
        else 
          num_repeat_count = rep_seg_entry->repeat_count;   

        NSDL2_HTTP(vptr, NULL, "Repeat Segment, Count value = %d", num_repeat_count);
        /*Now recurcivly call insert_segment for num_repeat_count*/ 
        StrEnt_Shr cur_seg_ptr;      
        cur_seg_ptr.seg_start = seg_ptr + 1;
        cur_seg_ptr.num_entries = agg_repeat_segment;  //
        //Calculating total body segments in case of CAVREPEAT_BLOCK prior so that we can check whether to grow vectors or not
        int tot_body_segs = (agg_repeat_segment + 1) * num_repeat_count;
        NS_CHK_AND_GROW_IOVEC(vptr, *ns_iovec, tot_body_segs);

        NSDL2_HTTP(vptr, NULL, "tot_body_segs = %d, i = %d, io_vector_size = %d",tot_body_segs, i, ns_iovec->tot_size); 
        for(j = 0; j < num_repeat_count; j++){
          if(rep_seg_entry->rep_sep){
	    NSDL2_HTTP(vptr, NULL, "Sep is %s", rep_seg_entry->rep_sep);
	    if(j != 0){ // add seprator before all segments except first one
              NS_FILL_IOVEC(*ns_iovec, rep_seg_entry->rep_sep, rep_seg_entry->rep_sep_len);
              if(content_size)
	        (*content_size) += rep_seg_entry->rep_sep_len;
	    }
          }
          i = insert_segments(vptr, cptr, &cur_seg_ptr, ns_iovec, &local_con_len, body_encoding, var_val_flag, req_part, request, j);
          if(i < 0) return i;

          if(content_size)
            (*content_size) += local_con_len; 
        }
        num_entered += agg_repeat_segment;
        seg_ptr += agg_repeat_segment;
        i--;
        break;
      } 
    }

    // Increament content size if this is called for Body
    if (length_flag && content_size) {
      (*content_size) += ns_iovec->vector[i].iov_len;
      NSDL2_HTTP(vptr, NULL, "setting Content Length = %d, current segment content length = %d", *content_size, ns_iovec->vector[i].iov_len);
    }
  }

  if (num_entered != seg_tab_ptr->num_entries)
  {
   /* fprintf(stderr, "ERROR- Number of segments inserted is %d. Max limit of io vector reached. To increase io vector size, increase io"
                              "vector size using keyword IO_VECTOR_SIZE. seg_tab_ptr->num_entries = (%d). This Request will be aborted.\n",
                               num_entered, seg_tab_ptr->num_entries);*/
    NSDL2_HTTP(vptr, NULL, "ERROR- Number of segments inserted = %d seg_tab_ptr->num_entries = %d", num_entered, seg_tab_ptr->num_entries);
    return -1;
  }

  NSDL2_HTTP(vptr, NULL, "number of segements , cur_idx = %d", ns_iovec->cur_idx);
  return ns_iovec->cur_idx;
}

inline int make_cookie_segments(connection* cptr, http_request_Shr* request, NSIOVector *ns_iovec)
{
  int ret = 0;
  VUser* vptr = cptr->vptr;

  ReqCookTab_Shr* url_cookies = &request->cookies;
  ReqCookTab_Shr* session_cookies = &(vptr->sess_ptr->cookies);

  NSDL2_HTTP(NULL, cptr, "Method called. global_settings->g_auto_cookie_mode = %d ret=%d\n", 
                          global_settings->g_auto_cookie_mode, ret);

  if(is_cookies_disallowed(vptr))
    return ret;

  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    /* insert cookies for Session*/
    if (session_cookies->num_cookies)
      ret = insert_cookies(session_cookies, vptr, ns_iovec);

    /* insert cookies for URL*/
    if (url_cookies->num_cookies)
      ret = insert_cookies(url_cookies, vptr, ns_iovec);
    NSDL2_HTTP(NULL, cptr, "ret = %d", ret);
    return ret;
  }
  /*bug 84661: added insert_h2_auto_cookie()*/
  if(cptr->http_protocol != HTTP_MODE_HTTP2)
    ret = insert_auto_cookie(cptr, vptr, ns_iovec);
  else
    ret = insert_h2_auto_cookie(cptr, vptr, 1);
  /* bug 84661 TODO: insert_h2_auto_cookie() need to  check if push_flag need to sent as 1 by default always or
            use cptr->http2->settings_frame.settings_enable_push */
  NSDL2_HTTP(NULL, cptr, "ret = %d", ret);
  return ret;
}

int do_encrypt_req_body(action_request_Shr* request, int group_num)
{
  int do_encrypt;

  if (request->proto.http.body_encryption_args.encryption_algo == AES_NONE)
  {
    NSDL2_HTTP(NULL, NULL, "BODY_ENCRYPTION disabled in script");
    do_encrypt = 0;
  }
  else if (request->proto.http.body_encryption_args.encryption_algo > AES_NONE)
  {
    NSDL2_HTTP(NULL, NULL, "Use script BODY_ENCRYPTION settings");
    do_encrypt = 1;
  }
  else if (runprof_table_shr_mem[group_num].gset.body_encryption.encryption_algo == AES_NONE)
  {
    NSDL2_HTTP(NULL, NULL, "BODY_ENCRYPTION disabled in scenario");
    do_encrypt = 0;
  }
  else
  {
    NSDL2_HTTP(NULL, NULL, "Use scenario BODY_ENCRYPTION settings");
    do_encrypt = 1;
  }
 
  return do_encrypt;
}

int make_encrypted_req_body(int group_num, action_request_Shr* request, connection *cptr, NSIOVector *ns_iovec)
{  
  char base64_encode_option, algo;
  int key_size, ivec_size, consumed_vector;
  char *key, *ivec;
  unsigned char *enc_body_buffer;
  char key_local_buf[ENC_KEY_IVEC_SIZE + 1] = "";
  char ivec_local_buf[ENC_KEY_IVEC_SIZE + 1] = "";
   
  NSDL2_HTTP(cptr, NULL, "Method called");

 if (request->proto.http.body_encryption_args.encryption_algo > AES_NONE)
  {
    NSDL2_HTTP(NULL, NULL, "Use script BODY_ENCRYPTION settings");
    algo = request->proto.http.body_encryption_args.encryption_algo;
    base64_encode_option = request->proto.http.body_encryption_args.base64_encode_option;
    NSDL2_HTTP(NULL, NULL, "insert_segments for key");
    NS_RESET_IOVEC(g_scratch_io_vector);
    consumed_vector = insert_segments(cptr->vptr, cptr, &(request->proto.http.body_encryption_args.key), &g_scratch_io_vector, 
                                      &key_size, 0, 1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK);

    if(consumed_vector == MR_USE_ONCE_ABORT)
      return consumed_vector;

    get_key_ivec_buf(consumed_vector, &g_scratch_io_vector, &request->proto.http.body_encryption_args.key, key_local_buf, key_size);
    key = key_local_buf;

    NSDL2_HTTP(NULL, NULL, "insert_segments for ivec");
    NS_RESET_IOVEC(g_scratch_io_vector);
    consumed_vector = insert_segments(cptr->vptr, cptr, &(request->proto.http.body_encryption_args.ivec), &g_scratch_io_vector, 
                                      &ivec_size, 0, 1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK);

    if(consumed_vector == MR_USE_ONCE_ABORT)
      return consumed_vector;

    get_key_ivec_buf(consumed_vector, &g_scratch_io_vector, &request->proto.http.body_encryption_args.key, ivec_local_buf, ivec_size);
    ivec = ivec_local_buf;

    NSDL2_HTTP(NULL, NULL, "key = %s, key_size = %d, ivec = %s, ivec_size = %d, algo = %d, base64_encode_option = %d", 
                            key, key_size, ivec, ivec_size, algo, base64_encode_option);

  }
  else
  {
    NSDL2_HTTP(NULL, NULL, "Use scenario BODY_ENCRYPTION settings");
    algo = runprof_table_shr_mem[group_num].gset.body_encryption.encryption_algo;
    base64_encode_option = runprof_table_shr_mem[group_num].gset.body_encryption.base64_encode_option;
    key = runprof_table_shr_mem[group_num].gset.body_encryption.key;
    ivec = runprof_table_shr_mem[group_num].gset.body_encryption.ivec;
    key_size = runprof_table_shr_mem[group_num].gset.body_encryption.key_size;
    ivec_size = runprof_table_shr_mem[group_num].gset.body_encryption.ivec_size;
    NSDL2_HTTP(NULL, NULL, "key = %s, key_size = %d, ivec = %s, ivec_size = %d, algo = %d, base64_encode_option = %d", 
                            key, key_size, ivec, ivec_size, algo, base64_encode_option);
  }  

  int content_length = 0;
  NS_RESET_IOVEC(g_scratch_io_vector);
  consumed_vector = insert_segments(cptr->vptr, cptr, request->proto.http.post_ptr, &g_scratch_io_vector,
                        	    &content_length, request->proto.http.body_encoding_flag,
                        	    1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK);
  if(consumed_vector == MR_USE_ONCE_ABORT)
    return consumed_vector;

  enc_body_buffer = make_encrypted_body(&g_scratch_io_vector, algo, base64_encode_option, key, key_size, ivec, ivec_size, &content_length);

  NS_FILL_IOVEC(*ns_iovec, enc_body_buffer, content_length);
  return content_length;
}

char *make_protobuf_post_body(const StrEnt_Shr* seg_tab_ptr, connection *cptr,
                              int* content_size, int body_encoding, int var_val_flag, int req_part)
{
  int num_vectors;

  static unsigned char *obuf = NULL;    //perfix 'o' => output 
  static unsigned long obuf_size = 0 ;

  int start_idx = 0;
  unsigned long olen;
  *content_size = 0;


  NSDL2_HTTP(cptr, NULL, "Method called, seg_tab_ptr = %p, cptr = %p, content_size = %d, body_encoding = %d, "
                         "var_val_flag = %d, req_part = %d",
                          seg_tab_ptr, cptr, *content_size, body_encoding, var_val_flag, req_part);

  NS_RESET_IOVEC(g_scratch_io_vector);

  /* Loop all segments of this post body and get parametrized value also encode that value into Protobuf
     And fill vectors */
  num_vectors = insert_segments(cptr->vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, content_size, body_encoding, 
                                var_val_flag, req_part, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(num_vectors == MR_USE_ONCE_ABORT)
  {
    NSDL2_HTTP(cptr, NULL, "After insert_segments(), num_vectors = -2 and hence returing.");
    return NULL;
  }

  if(obuf_size < *content_size)
  {
    obuf_size = *content_size;
    NSDL2_HTTP(cptr, NULL, "Allocating buffer to store protobuf encoded post body of size %lu", obuf_size);
    MY_REALLOC(obuf, obuf_size + 1, "Protobuf Encoded Post Body", -1);
  }
 
  olen = merge_and_make_protobuf_encoded_data(cptr->url_num->proto.http.protobuf_urlattr_shr.req_message,
                                              g_scratch_io_vector.vector, &start_idx, num_vectors, obuf, obuf_size);

  *content_size = olen;

  NSDL2_HTTP(cptr, NULL, "olen = %d", olen);

  NS_FREE_RESET_IOVEC(g_scratch_io_vector);

  return (char *)obuf;
}

/*
 * Arguments:
 *   cptr
 *   num_vectors
 *   vector
 *   free_array : Array of int which are memset with 0. So no need to set to 0 in this function
 */
char *make_hessian_req_body(const StrEnt_Shr* seg_tab_ptr, connection *cptr,
                       int* content_size, int body_encoding, int var_val_flag, int req_part)
{
  char *hessian_buff_xml_ptr;
  char *hessian_buff;
  int num_local_vectors;
  int encoded_buff_len;
  int i;
  int version; 
   
  NSDL2_HTTP(cptr, NULL, "Method called, seg_tab_ptr = %p, cptr = %p, content_size = %d, body_encoding = %d, "
                         "var_val_flag = %d, req_part = %d",
                          seg_tab_ptr, cptr, *content_size, body_encoding, var_val_flag, req_part);

  NS_RESET_IOVEC(g_scratch_io_vector);

  num_local_vectors = insert_segments(cptr->vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, content_size, body_encoding, var_val_flag, req_part, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(num_local_vectors == MR_USE_ONCE_ABORT)
    return NULL;

  if(*content_size > HESSIAN_MAX_BUF_SIZE){
    print_core_events((char *)__FUNCTION__, __FILE__, "Hessian request body size %d is more then 64 k. Stopping test.....", *content_size);  
    return NULL; 
  }
  if(hessian_buffer == NULL){
    MY_MALLOC(hessian_buffer, HESSIAN_MAX_BUF_SIZE, "hessian_buffer", -1);
  }  

  hessian_buff_xml_ptr = hessian_buffer;
  // Copy request body into buffer
  for(i = 0; i < num_local_vectors; i++){
    memcpy(hessian_buff_xml_ptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
    hessian_buff_xml_ptr += g_scratch_io_vector.vector[i].iov_len;
    NS_FREE_IOVEC(g_scratch_io_vector, i);
  }
  NS_RESET_IOVEC(g_scratch_io_vector);

  if(cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_PROTOBUF)
  {
    NSDL4_HTTP(cptr, NULL, "Protobuf encodding for request done.");
    return hessian_buffer;
  }

  NSDL4_HTTP(cptr, NULL, "Before encoding xml buffer = %s", hessian_buffer);
  MY_MALLOC(hessian_buff, (*content_size + 13)*2, "hessian_buff", -1);

  //  handling of amf in amf_seg_mode 1
  if(cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_AMF)
  {
    int buff_len = *content_size;

    // In case of non amf type parameterization we are passing noparam_flag as 2
    if(amf_encode ( 1, hessian_buffer, *content_size, hessian_buff , &buff_len, 1, 2, &version) == NULL)
    {
      fprintf(stderr, "Error in AMF xml format. Treating this as non amf body\n");
      strncpy(hessian_buff, hessian_buffer, *content_size);
      return hessian_buff;
    }
    *content_size = *content_size - buff_len;
    NSDL4_HTTP(cptr, NULL, " After encoding amf buffer pointer = %p, *content_size = %d", hessian_buff, *content_size);
    return hessian_buff;

  }else if (cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_JAVA_OBJ){

    //int buff_len = *content_size;
    int total_len = 0;
    u_ns_ts_t start_timestamp, end_timestamp, time_taken;
 
    if( (total_len = create_java_obj_msg(1, hessian_buffer, hessian_buff, content_size, &encoded_buff_len, 1)) > 0){
      start_timestamp = get_ms_stamp();
      if(send_java_obj_mgr_data(hessian_buff, encoded_buff_len, 1) != 0){
        fprintf(stderr, "Error in sending data to java object manager.\n");
        end_test_run(); 
      }

      memset(hessian_buff, 0, encoded_buff_len); 
      if(read_java_obj_msg(hessian_buff, content_size, 1) != 0){
        fprintf(stderr, "Error in reading data to java object manager.\n");
        end_test_run(); 
      }
    }else{
      fprintf(stderr, "Error in crating message data for java object manager.\n");
      end_test_run(); 
    }
    end_timestamp = get_ms_stamp();
    time_taken = end_timestamp - start_timestamp;
    NSDL3_HTTP(cptr, NULL, "Time taken = [%d]ms, Threshold = [%lld]ms", time_taken, global_settings->java_object_mgr_threshold);
    if (time_taken > global_settings->java_object_mgr_threshold)
      NS_DT1((VUser*)(cptr->vptr), NULL, DM_L1, MM_HTTP, "Time taken by Java object manager is exceeding threshold value. Time taken = [%d]ms, Threshold = [%lld]ms.", time_taken, global_settings->java_object_mgr_threshold);

    return hessian_buff;
  } 
  else {
    hessian_set_version(2);
    if(!(hessian_encode(1, hessian_buffer, *content_size, hessian_buff, &encoded_buff_len)))
    {
      print_core_events((char *)__FUNCTION__, __FILE__, "Error in encoding xml to hessian. Original req will be used. Error = %s", strerror(errno));
      strncpy(hessian_buff, hessian_buffer, *content_size);
      //hessian_buff[*content_size] = '\0';
      return hessian_buff;
    }
    *content_size = encoded_buff_len;
    NSDL4_HTTP(cptr, NULL, " After encoding hessian buffer pointer = %p, encoded_buff_len = %d", hessian_buff, encoded_buff_len);
    return hessian_buff; 
  }
}

static inline int netcache_fill_trans_hdr_in_req(VUser* vptr, connection* cptr){
  TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr; 
  if(node_ptr == NULL) {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                  __FILE__, (char*)__FUNCTION__, "No transaction is running for this user, so netcache transaction header will not be send.");
    return 0;
  }
 
  char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code); 
  // IF netcache header mode is set main and url is inline then do not send header, in all other cases send header 

  if(!((runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode == NS_TX_HTTP_HEADER_SEND_FOR_MAIN_URL) && (cptr->url_num->proto.http.type == EMBEDDED_URL)))
  {
    //Adding HTTP Header name is the name of HTTP header. Default value of http header name is "CavTxName".
    NSDL3_HTTP(NULL, cptr, "vptr->group_num = %d, name = %s, len = %d", vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name, runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len);

    NS_FILL_IOVEC(g_req_rep_io_vector,
                  runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name,
                  runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_len);

    if(cptr->url_num->proto.http.type == EMBEDDED_URL)
      NS_FILL_IOVEC(g_req_rep_io_vector, inline_prefix, inline_prefix_len);
  
    //Adding Transaction name, http req will have this header with the last tx started before this URL was send.
    if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable[0] != '\0')
    {
      NSDL3_HTTP(NULL, cptr, "tx var is not null..."); 
      tx_name = ns_eval_string(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable);
    }
    int tx_len = strlen(tx_name);
    NS_FILL_IOVEC(g_req_rep_io_vector, tx_name, tx_len);    

    NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);
  }
  return NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
}

void inline makeDtHeaderReqWithOptions(char *dtHeaderValueString, int *len, VUser* vptr, TxInfo *txPtr){

  long long uniqueReqId;
  NSDL3_HTTP(vptr, NULL, "dtHeaderValueString = [%s], vptr->location->name = [%s], script_name = [%s]",
                          dtHeaderValueString, vptr->location->name, runprof_table_shr_mem[vptr->group_num].sessindexprof_ptr->name);

  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.sourceID[0] != '\0')
    *len += sprintf(dtHeaderValueString + *len, ";SI=%s", runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.sourceID);

  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_REQUEST_ID_ENABLED){
    uniqueReqId = (txPtr)?((((long long)my_port_index) << 56) + (((long long)vptr->sess_inst) << 24) + ((long long)(txPtr->instance) << 8)):((((long long)my_port_index) << 56) + (((long long)vptr->sess_inst) << 24) + ((long long)(vptr->page_instance) << 8));
 
    *len += sprintf(dtHeaderValueString + *len, ";ID=%lld", uniqueReqId);
  }
  
  
  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_VIRTUAL_USER_ID_ENABLED){
    long long uniqueVirtualUserId; 
     
    uniqueVirtualUserId = (((long long)my_port_index) << 32) + vptr->user_index; 

    *len += sprintf(dtHeaderValueString + *len, ";VU=%lld", uniqueVirtualUserId);
  }
    
  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_LOCATION_ENABLED)
    *len += sprintf(dtHeaderValueString + *len, ";GR=%s", vptr->location->name);
   
  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_SCRIPT_NAME_ENABLED)
    *len += sprintf(dtHeaderValueString + *len, ";SN=%s", runprof_table_shr_mem[vptr->group_num].sessindexprof_ptr->name);

  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_AGENT_NAME_ENABLED)
    *len += sprintf(dtHeaderValueString + *len, ";AN=%s", g_cavinfo.NSAdminIP);

  if(runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.requestOptionsFlag & DT_PAGE_NAME_ENABLED)
    *len += sprintf(dtHeaderValueString + *len, ";PC=%s", vptr->cur_page->page_name);

  *len += sprintf(dtHeaderValueString + *len, "\r\n");
}

int fill_req_body_ex(connection* cptr, NSIOVector *ns_iovec, action_request_Shr *request)
{
  NSDL1_HTTP(NULL, cptr, "Method Called");                                 
  VUser* vptr = cptr->vptr;                                   
  int content_length = 0; 
  static char grpc_comp_flag;                                          
  int ret = 0, out_len;
  static unsigned int grpc_content_len;        
  static char* grpc_buffer = NULL;
  static int grpc_buffer_size = 0;                                                                
  int body_start_idx = NS_GET_IOVEC_CUR_IDX(*ns_iovec);
  char err_msg[1024];

  NSDL1_HTTP(NULL,cptr, "body_start_idx=%d content_length=%d",body_start_idx,content_length);
  if ((cptr->url_num->proto.http.redirected_url == NULL) && (request->proto.http.post_ptr))
  {

    NSDL2_HTTP(NULL, NULL, "request.encryption_algo = %d, runprof_table_shr_mem.encryption_algo = %d",
                            request->proto.http.body_encryption_args.encryption_algo,
                            runprof_table_shr_mem[vptr->group_num].gset.body_encryption.encryption_algo);

    if(do_encrypt_req_body(cptr->url_num, vptr->group_num))
    {
        if((content_length = make_encrypted_req_body(vptr->group_num, request, cptr, ns_iovec)) < 0)
         return -1;
    }
    else
    {
       char *body_ptr;
      //If request body encoding flag is set BODY_ENCODING_HESSIAN or AMF_SEGMENT_MODE and BODY_ENCODING_AMF flag is set
      if((cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_AMF && global_settings->amf_seg_mode)
                 || (cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_HESSIAN)
                 || (cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_JAVA_OBJ))
      {
        NSDL2_HTTP(NULL, NULL, "Body encoding is enabled, body_encoding_flag = %d", cptr->url_num->proto.http.body_encoding_flag);

        // Here we are passing 0 in body_encoding so that it will not go in amf leg in insert_segment method

        body_ptr = make_hessian_req_body(request->proto.http.post_ptr, cptr, &content_length, 0, 1, REQ_PART_BODY);

        if(body_ptr == NULL)
          return -1;
        NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, body_ptr, content_length);
      }
      else if(cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_PROTOBUF)
      {
        if(cptr->url_num->proto.http.protobuf_urlattr_shr.req_message == NULL)
        {
          NSDL2_HTTP(NULL, NULL, "Unable to encode post body in Protobuf, cptr = %p", cptr);
          return -1;
        }

        if((body_ptr = make_protobuf_post_body(request->proto.http.post_ptr, cptr, &content_length, BODY_ENCODING_PROTOBUF, 1, REQ_PART_BODY)) == NULL)
        {
          NSDL2_HTTP(NULL, NULL, "Unable to encode post body in Protobuf, cptr = %p", cptr);
          return -1;
        }
        NS_FILL_IOVEC(*ns_iovec, body_ptr, content_length);
      }
      else if(GRPC_REQ) 
      {
        if(GRPC_PROTO_REQUEST) 
        {
          NSDL2_HTTP(NULL, NULL, "GRPC_PROTO_REQUEST");
          if(cptr->url_num->proto.http.protobuf_urlattr_shr.req_message == NULL)
          {
            NSDL2_HTTP(NULL, NULL, "Unable to encode post body in Protobuf, cptr = %p", cptr);
            return -1;
          }

          if((body_ptr = make_protobuf_post_body(request->proto.http.post_ptr, cptr, &content_length, BODY_ENCODING_PROTOBUF, 1, REQ_PART_BODY)) == NULL)
          {
            NSDL2_HTTP(NULL, NULL, "Unable to encode post body in Protobuf, cptr = %p", cptr);
            return -1;
          }
        
          if(request->proto.http.protobuf_urlattr_shr.grpc_comp_type)
          {
            grpc_comp_flag = 1;
            //Comprassing request body in grpc_buffer
            nslb_compress(body_ptr, content_length, &grpc_buffer, (size_t *)&grpc_buffer_size, (size_t *)&out_len, request->proto.http.protobuf_urlattr_shr.grpc_comp_type, err_msg, 1024);
            body_ptr = grpc_buffer;   //This is allocated by GRPC comp
            content_length = out_len;
            NSDL2_HTTP(NULL, NULL, "grpc_buffer = %p, grpc_buffer_size (actual size of alloced buffer) = %d, content_length = %d,"
                                 " req_comp_type = %d", grpc_buffer, grpc_buffer_size, content_length,
                                 request->proto.http.protobuf_urlattr_shr.grpc_comp_type);
          }
          else {
           grpc_comp_flag = 0;
          }

          grpc_content_len = htonl(content_length);  //
          NSDL2_HTTP(NULL, NULL, "grpc_content_len (network byte order) = %d", grpc_content_len);
          //For grpc protocal we need to send <compress flag(0/1)> followed by <content length in network byte order> followed by actual msg
          //fill grpc_comp_flag and grpc_content_length into iovactors
          NS_FILL_IOVEC(*ns_iovec, &grpc_comp_flag, 1);
          NS_FILL_IOVEC(*ns_iovec, &grpc_content_len, 4);
          NS_FILL_IOVEC(*ns_iovec, body_ptr, content_length);
          content_length += 5; //Add 1 for grpc_comp_flag & 4 for grpc_content_len  
        } 
        else
        {
          NSDL2_HTTP(NULL, NULL, "grpc_comp_type =  %d", request->proto.http.protobuf_urlattr_shr.grpc_comp_type);
          /*If compression is not enable then first fill the flag, then reserve the index for length,
            do insert segment and fill request iovec dierctly, and fill the length in network byte order format.
          */
          if(!request->proto.http.protobuf_urlattr_shr.grpc_comp_type)
          {
             grpc_comp_flag = 0;
             NS_FILL_IOVEC(*ns_iovec, &grpc_comp_flag, 1);
             int len_idx = NS_GET_IOVEC_CUR_IDX(*ns_iovec);  //Length Index
             NS_INC_IOVEC_CUR_IDX(*ns_iovec);  //Skip Length , Will be filled latet

	     if ((ret = insert_segments(vptr, cptr, request->proto.http.post_ptr, ns_iovec,
                       &content_length, request->proto.http.body_encoding_flag,
                       1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
      	     {
               return ret;
             }
             grpc_content_len = htonl(content_length);  //Convert Byte Order
             NSDL2_HTTP(NULL, NULL, "grpc_content_len (network byte order) = %d", grpc_content_len);
             NS_FILL_IOVEC_IDX(*ns_iovec, &grpc_content_len, 4, len_idx); //
             content_length += 5; //Add 1 for grpc_comp_flag & 4 for grpc_content_len
            
          }
          /*If compression is enable then first fill the flag, then do insert segment and fill scratch vector.
            Copy iovec into buffer for compression then do compression.
            Fill length (Network Byte Order) & Compressed buffer in request iovec
          */
          else
          {

            grpc_comp_flag = 1;
            NS_FILL_IOVEC(*ns_iovec, &grpc_comp_flag, 1);

  	    NS_RESET_IOVEC(g_scratch_io_vector);
  
            if ((ret = insert_segments(vptr, cptr, request->proto.http.post_ptr, &g_scratch_io_vector,
                       &content_length, request->proto.http.body_encoding_flag,
                       1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
            {
               return ret;
            }

	    //Convert g_scratch_io_vector into single buffer	
            copy_iovec_to_buffer(&ns_nvm_scratch_buf, ns_nvm_scratch_buf_size, &g_scratch_io_vector, "gRPC Request");
    	
            //Comprassing request body in grpc_buffer
	    nslb_compress(ns_nvm_scratch_buf, content_length, &grpc_buffer, (size_t *)&grpc_buffer_size, (size_t *)&out_len, request->proto.http.protobuf_urlattr_shr.grpc_comp_type, err_msg, 1024);

            body_ptr = grpc_buffer;   //This is allocated by GRPC comp
            content_length = out_len;
            NSDL2_HTTP(NULL, NULL, "grpc_buffer = %p, grpc_buffer_size (actual size of alloced buffer) = %d, content_length = %d,"
                                 " req_comp_type = %d", grpc_buffer, grpc_buffer_size, content_length,
                                 request->proto.http.protobuf_urlattr_shr.grpc_comp_type);
            grpc_content_len = htonl(content_length);  //
            NSDL2_HTTP(NULL, NULL, "grpc_content_len (network byte order) = %d", grpc_content_len);
            //For grpc protocal we need to send <compress flag(0/1)> followed by <content length in network byte order> followed by actual msg
            //fill grpc_comp_flag and grpc_content_length into iovactors

            NS_FILL_IOVEC(*ns_iovec, &grpc_content_len, 4);
            //NS_FILL_IOVEC(*ns_iovec, body_ptr, content_length);
            NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, body_ptr, content_length);
            content_length += 5; //Add 1 for grpc_comp_flag & 4 for grpc_content_len
          }
        }
      } 
      else if ((ret = insert_segments(vptr, cptr, request->proto.http.post_ptr, ns_iovec,
                        &content_length, request->proto.http.body_encoding_flag,
                        1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
      {
        return ret;
      }
    }
    /* Expect 100 Continue case*/
    if(cptr->url_num->proto.http.header_flags & NS_HTTP_100_CONTINUE_HDR) {
      NSDL2_HTTP(NULL, cptr, "Setting Expect: 100-continue hdr state.  content_length = %d",
                                          content_length);
      cptr->body_index = body_start_idx;
      cptr->flags |= NS_HTTP_EXPECT_100_CONTINUE_HDR;  // Setting state as we will expect for 100 Contiue
      /* we need body size in case of 100-continue as we have to send first header than body*/
      cptr->content_length = content_length;
    }
  }else{
    if(cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_HESSIAN){
      print_core_events((char *)__FUNCTION__, __FILE__, "Hessian header is set with empty request body . Ignored....");
    }
    /* GRPC Protocol requires comp_flag<1 byte>  msg_len<4 byte> even if message_body is empty */
    else if(GRPC_REQ){
      NSDL1_HTTP(NULL,cptr, "GRPC body is empty");
      grpc_comp_flag = 0;
      grpc_content_len = 0;
      NS_FILL_IOVEC(*ns_iovec, &grpc_comp_flag, 1);
      NS_FILL_IOVEC(*ns_iovec, &grpc_content_len, 4);
      content_length = 5;
    }
  }
  NSDL1_HTTP(NULL,cptr, "Exiting. content_length=%d",content_length);
  return content_length;
}

inline int fill_req_body(VUser *vptr, connection* cptr, action_request_Shr *request)
{
  int http_body_chksum_hdr_idx = 0;
  int http_content_length_idx = 0;
  int content_length;
  /*bug 54315: argument passed in correct sesquence*/
  NSDL1_HTTP( vptr, cptr, "Method Called");
  
  /* Handle HTTP BODY check sum header , increasing body_start_idx by 2 */
  if(runprof_table_shr_mem[vptr->group_num].gset.http_body_chksum_hdr.mode && request->proto.http.post_ptr)
  {
    http_body_chksum_hdr_idx =  NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
    NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
    NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
    NSDL2_HTTP(NULL, NULL, "Increased g_req_rep_io_vector.cur_idx+=2; to handle HTTP body check sum header, "
                           "g_req_rep_io_vector.cur_idx = %d", NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
  }
  if (request->proto.http.http_method == HTTP_METHOD_POST ||
      request->proto.http.http_method == HTTP_METHOD_PUT ||
      request->proto.http.http_method == HTTP_METHOD_PATCH ||
      (request->proto.http.http_method == HTTP_METHOD_GET &&
       request->proto.http.post_ptr)) { // Content-Length is always send even if post_ptr is NULL
     http_content_length_idx = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
     NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
  }
  //Fill end of the header line 
  NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);

  int body_start_idx = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);

  if((request->proto.http.http_method == HTTP_METHOD_POST &&
     request->proto.http.header_flags & NS_URL_CLICK_TYPE) || ( request->proto.http.header_flags & NS_XMPP_UPLOAD_FILE))
  {
    NS_FILL_IOVEC(g_req_rep_io_vector, vptr->httpData->post_body, vptr->httpData->post_body_len);
  }
  /* Body needs to be filled before headers since we are have to calculate content length which will go into the headers */
  // Fill body if present
  // If URL is redirect, do not fill body

  if((content_length = fill_req_body_ex(cptr, &g_req_rep_io_vector, request)) < 0)
  {
    NSDL3_HTTP(NULL, NULL, "Returning as got content length value < 0");
    return content_length;
  }

  if((runprof_table_shr_mem[vptr->group_num].gset.http_body_chksum_hdr.mode) && (content_length > 0))
    add_http_checksum_header(vptr, body_start_idx, http_body_chksum_hdr_idx, content_length);

  if (request->proto.http.http_method == HTTP_METHOD_POST ||
      request->proto.http.http_method == HTTP_METHOD_PUT ||
      request->proto.http.http_method == HTTP_METHOD_PATCH ||
      (request->proto.http.http_method == HTTP_METHOD_GET &&
       request->proto.http.post_ptr))
  {
    int written_amt;
    if ((written_amt = snprintf(post_content_ptr, post_content_val_size, "%d\r\n", content_length)) > post_content_val_size) {
      fprintf(stderr, "make_request(): Error, writing too much into content_buf array\n");
      return -1;
    }
    cptr->http_payload_sent = content_length;
    int cur_idx  = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
    g_req_rep_io_vector.cur_idx = http_content_length_idx;
    NS_FILL_IOVEC(g_req_rep_io_vector, content_length_buf, (written_amt + POST_CONTENT_VAR_SIZE));
    g_req_rep_io_vector.cur_idx = cur_idx;
  }
  else
    cptr->http_payload_sent = 0;

  if(global_settings->monitor_type == HTTP_API)
    cavtest_http_avg->req_body_size += cptr->http_payload_sent;

  return 0;  
}

inline int make_request(connection* cptr, int *num_vectors, u_ns_ts_t now) 
{
  action_request_Shr* request = cptr->url_num;
  VUser* vptr = cptr->vptr;

  int body_start_idx; // Start index in vector for filling Body.
  int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  PerHostSvrTableEntry_Shr* svr_entry;
  
  /* Net diagnostics--BEGIN*/
  char *net_dignostics_buf = NULL;  
  long long CavFPInstance;
  /* Net diagnostics--END*/

  /* NS TX in HTTP Header */
  /* X-dynaTrace Header Buffer Pointer*/
  char *dynaTraceBuffer = NULL;
  int ret;

  GroupSettings *loc_gset = &runprof_table_shr_mem[vptr->group_num].gset;
  
  cptr->conn_state = CNST_WRITING; /*bug 51330: set conn_state here so that don't need to set in MAKE_REQUEST() macro*/
  NSDL3_HTTP(NULL, cptr, "cptr = %p, GET_SESS_ID_BY_NAME = %u, GET_PAGE_ID= %u, GET_URL_ID= %u, "
                         "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", 
                          cptr, vptr->sess_ptr->sess_id, vptr->cur_page->page_id, cptr->url_num->proto.http.url_index, 
                          GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

  NS_RESET_IOVEC(g_req_rep_io_vector);
  // Step1: Fill HTTP request line
  // Step1a: Fill Method in 0th Vector (Method is with one space at the end)
  NS_FILL_IOVEC(g_req_rep_io_vector, 
                http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name, 
                http_method_table_shr_mem[cptr->url_num->proto.http.http_method_idx].method_name_len);

  // Step1b: Fill Url
  
  // We need actual host to make URL and HOST header 
  svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

  NSDL2_HTTP(vptr, cptr, "Filling Url: svr_entry = [%p], cptr->url = [%p], cptr->url = [%s]", 
                          svr_entry, cptr->url, (cptr->url)?cptr->url:NULL);

  if(cptr->url) {
    NSDL3_HTTP(vptr, cptr, "cptr->flags = %0x, proxy_proto_mode = %0x, cptr->url_num->request_type = %d", 
                            cptr->flags, runprof_table_shr_mem[vptr->group_num].gset.proxy_proto_mode, cptr->url_num->request_type);

    /*Fill server information in case of proxy mode only*/
    if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY) 
    {
      NSDL3_HTTP(vptr, cptr, "Filling url (proxy case) [%s] of len = %d at %d vector idx",
                            cptr->url, cptr->url_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
      //We will make fully qualified url if we are using Proxy and -
      // 1) Request is http and G_PROXY_PROTO_MODE is 1 OR  
      // 2) Request is https and G_PROXY_PROTO_MODE is 1
      if((cptr->url_num->request_type == HTTP_REQUEST && runprof_table_shr_mem[vptr->group_num].gset.proxy_proto_mode & HTTP_MODE) ||
         (cptr->url_num->request_type == HTTPS_REQUEST && runprof_table_shr_mem[vptr->group_num].gset.proxy_proto_mode & HTTPS_MODE))
      {
        // In case of proxy, we need to send fully qualified URL e.g. http://www.cavisson.com:8080/index.html
        // Here host:port is the host/port of the Origin Server (Not proxy server)
      
        // Step1b.1: Fill schema part of URL (http:// or https://)
        NSDL3_HTTP(vptr, cptr, "Fill schema part of URL for request_type %d at index %d", 
                                cptr->url_num->request_type, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
        if(cptr->url_num->request_type == HTTP_REQUEST) 
        {
          NS_FILL_IOVEC(g_req_rep_io_vector, HTTP_REQUEST_STR, HTTP_REQUEST_STR_LEN);
        }
        else if(cptr->url_num->request_type == HTTPS_REQUEST) 
        {
          NS_FILL_IOVEC(g_req_rep_io_vector, HTTPS_REQUEST_STR, HTTPS_REQUEST_STR_LEN);
        }

        // Step1b.2: Fill host part of URL Eg: 192.168.1.66:8080. Host is actual host (Origin Server)
        // Port will be filled if it not default
        NSDL3_HTTP(NULL, cptr, "Fill host part of URL '%s', len = %d at index %d", 
                          svr_entry->server_name, svr_entry->server_name_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector)); 
        NS_FILL_IOVEC(g_req_rep_io_vector, svr_entry->server_name, svr_entry->server_name_len);
      }  
    }
    NS_FILL_IOVEC(g_req_rep_io_vector, cptr->url, cptr->url_len);
  } else {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Url is NULL");
      NSDL2_HTTP(NULL, cptr, "Url is NULL");
      return -1;
  }

  // Step1c: Fill HTTP Version

  NSDL2_HTTP(NULL, cptr, "HTTP Version = [%s]",
	                  http_version_buffers[(int)(cptr->url_num->proto.http.http_version)]);
  NS_FILL_IOVEC(g_req_rep_io_vector, 
                http_version_buffers[(int)(cptr->url_num->proto.http.http_version)],
                http_version_buffers_len[(int)(cptr->url_num->proto.http.http_version)]);
  
  if((runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_AUTO) && (cptr->url_num->request_type == HTTP_REQUEST))
  {
    NS_FILL_IOVEC(g_req_rep_io_vector, connection_header_buf , CONNECTION_HDR_BUF_LEN);
    NS_FILL_IOVEC(g_req_rep_io_vector, upgrade_header_buf , UPGRADE_HDR_BUF_LEN);
    NS_FILL_IOVEC(g_req_rep_io_vector, http2_settings_buf , HTTP2_SETTINGS_BUF_LEN);
  }
  else if (runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_HTTP1)
  {
    cptr->http_protocol = HTTP_MODE_HTTP1;
    NSDL2_HTTP(NULL, cptr, "Set protocol type HTTP1");
  }
  // Step2: Fill Cookie headers if any cookie to be send
  /* The cookie has to be filled before body since we dont know how many cookies will be filled in case of auto cookie. */
  // We are filling cookie after request line. So set cookie_start_idx to point after url segments
  // Now insert cookies as we do not know how many cookies are there for Auto Mode.
  // So we are filling cookies after request line
  if((ret = make_cookie_segments(cptr, &(request->proto.http), &g_req_rep_io_vector)) < 0)
  {
    return ret;
  }
  // Step3: Fill validation headers for caching if we need to validate
  if(NS_IF_CACHING_ENABLE_FOR_USER) {
     if(((CacheTable_t*)cptr->cptr_data->cache_data)->cache_flags & NS_CACHE_ENTRY_VALIDATE)
     {
       if((ret = cache_fill_validators_in_req(cptr->cptr_data->cache_data, vptr)) < 0)
       {
         return ret;
       }
     }
  }

  // Step: Fill validation headers for caching if we need to validate
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode != NS_TX_HTTP_HEADER_DO_NOT_SEND)
  {
    ret = netcache_fill_trans_hdr_in_req(vptr, cptr);
    NSDL2_CACHE(NULL, cptr, "After filling validators, next_idx = %d", NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
  }

  /* Authorization Header */
  //To send the previous proxy authorization headers again in case of proxy-chain
  if (cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers body_start_idx=%d", body_start_idx); 
    int ret = auth_create_authorization_hdr(cptr, &body_start_idx, vptr->group_num, 1, 0, 0);
    if (ret == AUTH_ERROR)
      NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
  }
  //To send current authorization headers 
  //Proxy authorization headers send in response to 407
  //Server authorization headers send in response to 401
  if(cptr->flags & NS_CPTR_AUTH_MASK)
  {
    /* Bug 43675 - NetStorm is sending two "Authorization" header with second request if server is giving "401 Unauthorised"
     * RCA           : Two "Authorization" headers were send when "Authorization" header was provided in script.
     *                 -> First "Authorization" header was generated using authentication credentials.
     *                 -> Second "Authorization" header was send from script.
     * Resolution    : If "Authorization" header is provided through script, then same will be used and 
     *                 NS will not generate "Authorization" header.
     * Recommendation: Do not use "Authorization" header in script, specially in case of DIGEST, NTLM & KERBEROS.
     *                 Use HTTPAuthUserName and HTTPAuthPassword attribute in script to provide credentials
     */
    if(!(request->proto.http.header_flags & NS_HTTP_AUTH_HDR))
    {
      NSDL2_AUTH(NULL, cptr, "Adding authorization headers body_start_idx=%d", body_start_idx); 
      int ret = auth_create_authorization_hdr(cptr, &body_start_idx, vptr->group_num, 0, 0, 0);
      if (ret == AUTH_ERROR)
        NSDL2_AUTH(NULL, cptr, "Error making auth headers: will not send auth headers");
    }
    else
    {
      check_and_set_single_auth(cptr, vptr->group_num);
    }
  }

  // Step6: Fill standard headers as required

  if(IS_REFERER_NOT_HTTPS_TO_HTTP) {
    NSDL2_HTTP(NULL, cptr, "vptr->referer = %s", vptr->referer);

    /* insert the Referer and copy to new one if needed */
    if (vptr->referer_size) {
      NS_FILL_IOVEC(g_req_rep_io_vector, Referer_buf, REFERER_STRING_LENGTH);

      NS_FILL_IOVEC(g_req_rep_io_vector, vptr->referer, vptr->referer_size);
    }
  }
  //save_referer is now called from validate_req_code, this is done to implement new referer design Bug 17161 

  /* insert the Hostr-Agent header */
  if (!(disable_headers & NS_HOST_HEADER)) {
      NS_FILL_IOVEC(g_req_rep_io_vector, Host_header_buf, HOST_HEADER_STRING_LENGTH);

      char *serv_name;
      int len;
      // Added by Anuj 08/03/08
      if (use_rec_host == 0) //Send actual host (mapped)
      {
        //Manish: here we directly get server_entry because it is already set in start_socket
        serv_name = svr_entry->server_name;
        len = svr_entry->server_name_len;
        NSDL2_HTTP(NULL, cptr, "Hostr-Agent header: server_name %s, len %d at index %d", 
                                svr_entry->server_name, svr_entry->server_name_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
      }
      else //Send recorded host
      {
        serv_name = cptr->url_num->index.svr_ptr->server_hostname;
        len  = cptr->url_num->index.svr_ptr->server_hostname_len;
      }
      NS_FILL_IOVEC(g_req_rep_io_vector, serv_name, len);
   
      NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);
  }

  /* insert the User-Agent header */
  //TODO:  
  if (!(disable_headers & NS_UA_HEADER) && (!GRPC_REQ)) {
    NS_FILL_IOVEC(g_req_rep_io_vector, User_Agent_buf, USER_AGENT_STRING_LENGTH);

    char *ua_string;
    int len;
    if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL) 
        && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
    {
      NSDL3_HTTP(NULL, cptr, "UA string was set by Browser shared memory. So copying this to HTTP vector");
      ua_string = vptr->browser->UA;
      len = strlen(vptr->browser->UA);
    }
    else
    {
      NSDL3_HTTP(NULL, cptr, "UA string was set by API. So copying this to HTTP vector, ua string = [%s] and length = %d", vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len);
      ua_string = vptr->httpData->ua_handler_ptr->ua_string;
      len = vptr->httpData->ua_handler_ptr->ua_len;
    }
    NS_FILL_IOVEC(g_req_rep_io_vector, ua_string, len);
  }

  /* insert the standard headers */
  if (!(disable_headers & NS_ACCEPT_HEADER)) {
      NS_FILL_IOVEC(g_req_rep_io_vector, Accept_buf, ACCEPT_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_ACCEPT_ENC_HEADER)) {//globals.no_compression == 0)
      NS_FILL_IOVEC(g_req_rep_io_vector, Accept_enc_buf, ACCEPT_ENC_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_KA_HEADER)) {
      NS_FILL_IOVEC(g_req_rep_io_vector, keep_alive_buf, KEEP_ALIVE_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_CONNECTION_HEADER)) {
      NS_FILL_IOVEC(g_req_rep_io_vector, connection_buf, CONNECTION_BUF_STRING_LENGTH);
  }

  /* Insert Net diagnostics Header --BEGIN*/
  if(vptr->flags & NS_ND_ENABLE && global_settings->net_diagnostics_mode == 1) // If net diagnostics keyword enable
  {
    //Calculate FlowPathInstance per nvm 
    CavFPInstance = compute_flowpath_instance(); 
        
    MY_MALLOC(net_dignostics_buf, 128, "Net Diagnostics buffer", -1);
    //sprintf(net_dignostics_buf, "%s: %016llx\r\n", runprof_table_shr_mem[vptr->group_num].gset.net_diagnostic_hdr, CavFPInstance);
    int nd_buf_len = sprintf(net_dignostics_buf, "%s: %llu\r\n", global_settings->net_diagnostic_hdr, CavFPInstance);

    //Updating flowpath instance for current cptr.
    cptr->nd_fp_instance = CavFPInstance;
    NSDL3_HTTP(NULL, cptr, "Flow Path instance at current cptr = %lld", cptr->nd_fp_instance);

    //Adding header data into vector 
    NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, net_dignostics_buf, nd_buf_len);
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
        NSDL2_HTTP(NULL, cptr, "Inside Main URL: used_len= %d, head_buffer = %s", vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff);
        NS_FILL_IOVEC(g_req_rep_io_vector, 
                      vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].hdr_buff, 
                      vptr->httpData->usr_hdr[NS_MAIN_URL_HDR_IDX].used_len);
      }
      break;

      case EMBEDDED_URL:
      if(vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len > 0)
      {
        NSDL2_HTTP(NULL, cptr, "Inside Embedded URL: used_len= %d, head_buffer = %s", vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff);
        NS_FILL_IOVEC(g_req_rep_io_vector,
                      vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].hdr_buff,
                      vptr->httpData->usr_hdr[NS_EMBD_URL_HDR_IDX].used_len);
      }
      break;
    }
    //stored the header data for ALL
    if(vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len > 0)
    {
      NSDL2_HTTP(NULL, cptr, "Inside for ALL(Main URL & Embedded): used_len= %d, head_buffer = %s", vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len, vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff);
      NS_FILL_IOVEC(g_req_rep_io_vector,
                    vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].hdr_buff,
                    vptr->httpData->usr_hdr[NS_ALL_URL_HDR_IDX].used_len);
    }

    //insert ns_web_add_auto_header() API content 
    ret = insert_auto_headers(cptr, vptr);
  }

  /* Insert X-DynaTrace header - Begin - */
  if(IS_DT_HEADER_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    int dtHeaderLen = 0;
    TxInfo *txPtr = (TxInfo *) vptr->tx_info_ptr;
    MY_MALLOC(dynaTraceBuffer, 1024, "X-DynaTrace buffer", -1);
    
    if(txPtr){
      char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, txPtr->hash_code);
      dtHeaderLen = sprintf(dynaTraceBuffer, "x-dynaTrace: TE=%d;NA=%s", testidx, tx_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }
    else {
      dtHeaderLen = sprintf(dynaTraceBuffer, "x-dynaTrace: TE=%d;NA=%s", testidx, vptr->cur_page->page_name);
      makeDtHeaderReqWithOptions(dynaTraceBuffer, &dtHeaderLen, vptr, txPtr);
    }

    NSDL3_HTTP(NULL, NULL, "dynaTraceBuffer = [%s]", dynaTraceBuffer);

    //Adding header data into vector 
    NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, dynaTraceBuffer, dtHeaderLen);
  }

  /* Insert Correlation ID in header - Begin - */
  //We will check if Correlation-ID is enabled, we will add Correlation-ID header in Request
  if(IS_CORRELATION_ID_ENABLED) 
  {
    static char *cor_id_buf = NULL;
    int corid_header_len = 0;

    if(!cor_id_buf)
      MY_MALLOC(cor_id_buf, CORR_ID_NAME_VALUE_BUFF_SIZE + 1, "Correlation-ID buffer", -1);

    // 1022 size bcoz, 2 bits for \r\n
    corid_header_len = ns_cor_id_header_opt(vptr, cptr, cor_id_buf, 1024+1, FLAG_HTTP1);
    cor_id_buf[corid_header_len++] = '\r';
    cor_id_buf[corid_header_len++] = '\n';
    NSDL3_HTTP(NULL, NULL, "cor_id_buf = [%s]", cor_id_buf);

    //Adding header data into vector 
    NS_FILL_IOVEC(g_req_rep_io_vector, cor_id_buf, corid_header_len);
  }

  /* Insert Network Cache Stats header - Begin - */
  if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    NSDL3_HTTP(NULL, cptr, "Network Cache Stats Headers = [%s], len =[%d]", network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
    NS_FILL_IOVEC(g_req_rep_io_vector, network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
  }
  /* Network Cache Stats header - End - */

  if(request->proto.http.header_flags & (NS_URL_CLICK_TYPE|NS_XMPP_UPLOAD_FILE) && vptr->httpData->formencoding)
  {
    char *enctype_header_buf = NULL;
    MY_MALLOC(enctype_header_buf, 14 + vptr->httpData->formenc_len + 2 + 1, "Content-Type header", -1);
    /* 14 characters for 'Content-Type: ', then the formenc_len, then 2 for \r\n and 1 for terminating null */

    sprintf(enctype_header_buf, "Content-Type: %s\r\n", vptr->httpData->formencoding);

    NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, enctype_header_buf, strlen(enctype_header_buf));
  }
/*
  fill_user_api_hdr();
  fill_dynatrace_hdr();
  fill_correlation_id_hdr(vptr, request);  
  fill_network_cache_stats_hdr(vptr, request);
  fill_content_type_hdr(vptr, request);
*/

  /* insert the rest of the headers */
  if ((ret = insert_segments(vptr, cptr, &request->proto.http.hdrs, &g_req_rep_io_vector,
                                  NULL, 0, 1, REQ_PART_HEADERS, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {

    return ret;
  }

  // Next three insert segments are done for G_HTTP_HDR
  // Table was filled in reference to relative page_id
  int pg_id = vptr->cur_page->page_id - session_table_shr_mem[vptr->sess_ptr->sess_id].first_page->page_id;

  if ((ret = insert_segments(vptr, cptr, 
                             &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].AllUrlHeaderBuf,&g_req_rep_io_vector,
                             NULL, 0, 1, REQ_PART_HEADERS, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {

    return ret;
  }
  if(cptr->url_num->proto.http.type == MAIN_URL)
  {
    if ((ret = insert_segments(vptr, cptr, 
                            &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].MainUrlHeaderBuf, &g_req_rep_io_vector,
                             NULL, 0, 1, REQ_PART_HEADERS, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {

       return ret;
    }
  }
  else if(cptr->url_num->proto.http.type == EMBEDDED_URL)
  {
    if ((ret = insert_segments(vptr, cptr, 
                           &runprof_table_shr_mem[vptr->group_num].addHeaders[pg_id].InlineUrlHeaderBuf,&g_req_rep_io_vector,
                             NULL, 0, 1, REQ_PART_HEADERS, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {

      return ret;
    }
  }
  

  /**********************************************************************/
  /*Body Should fill at last. Don't fill any header after fill_req_body */
  /**********************************************************************/
  if ((ret = fill_req_body(vptr, cptr, request)) < 0 )
      return ret;

  NS_DT3(vptr, cptr, DM_L1, MM_SOCKETS, "Starting fetching of page %s URL(%s) on fd = %d. Request line is %s",
         cptr->url_num->proto.http.type == MAIN_URL ? "main" : "inline",
         get_req_type_by_name(cptr->url_num->request_type),cptr->conn_fd,
         cptr->url);

  /*****************************************************************************************************************
   ABHAY : BUG 56344 : Core | Getting T.O for HTTP request when cav repeat block is used in a header .
                 RCA : Handling of cav_repeat block in header was missing. In this case we were setting 
                       *num_vectors = total_segs; but next_idx value was updated later and num_vectors 
                       becomes less than the actual segements due to cav_repeat block.
          Resolution : We will update the num_vectors with next_idx value if next_idx is greater than
                       total_segs.
  ******************************************************************************************************************/
  *num_vectors = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);

  return NS_NOT_CACHED;
}


#if 0
/* GET<space>/tours/index.html<space>HTTP/1.1*/
/* Fills url_only with /tours/index.html or NULL*/
static inline void get_url_only(char *request_line, char *url_only, int max_len, int *len) {

  char *first_space, *second_space;

  NSDL2_HTTP(NULL, NULL, "Method called."
			 " request_line = %s, url_only = %s, max_len  = %d, len = %d",
			 request_line, url_only, max_len, *len); 

  if (request_line) {  /* Redirection */
    first_space = index(request_line, ' ');
    if(first_space) {
      first_space = first_space + 1;
      second_space = index(first_space + 1, ' ');
    }
    if(first_space != NULL && second_space != NULL) {
      *len = second_space - first_space;
       if(*len > max_len) *len = max_len - 1;
      strncat(url_only, first_space, *len);
    } else {
      /*Some thing wrong here*/
      url_only[0] = '\0';
      *len = 0;
    }
  } else {
    url_only[0] = '\0';
    *len = 0;
  }

  NSDL2_HTTP(NULL, NULL, "url_only = %s, len = %d", url_only, *len);
}
#endif

/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : add_http_checksum_header 
 *
 * Purpose   : This function will generate md5 check sum for Request body and add header for this 
 *             Checksum will be generated by following way -
 *             1. Add provided prefix and suffix to the body
 *             2. Get md5sum by function ns_gen_md5_checksum() 
 *             Exampl: If body is - "My name is {name} and I am working in Cavisson", then md5sum will be calculate as
 *                Here {name} is parameter so fisrt expand this and then generate md5sum
 *                Suppose after expending {name} we got body - "My name is Foo and I am working in Cavisson" then
 *                add prifx and sufix (say { and }) and then generate md5sum
 *            
 *
 * Input     : 
 *
 * Output    : On error     -1
 *             On success    0
 *        
 * Author    : Abhay Singh
 * Build_v   : 4.1.11 & 4.1.12  
 *------------------------------------------------------------------------------------------------------------------*/

#define NS_MD5_CHECKSUM_SIZE    64
#define NS_MD5_CHECKSUM_LENGTH  32
void add_http_checksum_header(VUser *vptr, int body_start_idx, int http_body_chksum_hdr_idx, int body_size)
{
  int j;
  int grp_num = -1;
  char *write_ptr = NULL;
  int total_body_size = 0;
  char *ns_http_body_checksum_buffer;  //NS_MD5_CHECKSUM_BYTES + \r\n < 20

  NSDL2_VARS(vptr, NULL, "Method Called, body_start_idx = %d, body_size = %d",
                         body_start_idx, body_size);
  if(!vptr)
    return;

  grp_num = vptr->group_num;

  GroupSettings *lgset = &runprof_table_shr_mem[grp_num].gset;

  //check we have enough space into ns_nvm_scratch_buf
  total_body_size = body_size + lgset->http_body_chksum_hdr.pfx_len + lgset->http_body_chksum_hdr.sfx_len;
  NSDL2_VARS(vptr, NULL, "ns_nvm_scratch_buf_size = %d, total_body_size = %d", ns_nvm_scratch_buf_size, total_body_size);
  if(ns_nvm_scratch_buf_size < total_body_size)
  {
    MY_REALLOC(ns_nvm_scratch_buf, total_body_size, "reallocating for http_req_body md5 checksum ", -1);
    ns_nvm_scratch_buf_size = total_body_size;
  }

  write_ptr = ns_nvm_scratch_buf;
  write_ptr[0] = 0;
  //memset(write_ptr, 0, ns_nvm_scratch_buf_size);

  //Adding prifix into body
  if(lgset->http_body_chksum_hdr.if_pfx_sfx)
  {
    memcpy(write_ptr, lgset->http_body_chksum_hdr.pfx, lgset->http_body_chksum_hdr.pfx_len);
    write_ptr += lgset->http_body_chksum_hdr.pfx_len;
  }

  //Copy body into ns_nvm_scratch_buf
  for(j = body_start_idx; j < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); j++){
    memcpy(write_ptr, g_req_rep_io_vector.vector[j].iov_base, g_req_rep_io_vector.vector[j].iov_len);
    write_ptr += g_req_rep_io_vector.vector[j].iov_len;
  }

  //Adding suffix into body
  if(lgset->http_body_chksum_hdr.if_pfx_sfx)
  {
    memcpy(write_ptr, lgset->http_body_chksum_hdr.sfx, lgset->http_body_chksum_hdr.sfx_len);
    write_ptr += lgset->http_body_chksum_hdr.sfx_len;
  }

  NSDL4_VARS(vptr, NULL, "Body for which md5sum has to calculated = %s", ns_nvm_scratch_buf);
  MY_MALLOC(ns_http_body_checksum_buffer, NS_MD5_CHECKSUM_SIZE + 1, "ns_http_body_checksum_buffer", -1);

  ns_gen_md5_checksum((unsigned char *)ns_nvm_scratch_buf, (write_ptr - ns_nvm_scratch_buf), (unsigned char *)ns_http_body_checksum_buffer);

  strcat(ns_http_body_checksum_buffer, CRLFString);

  NSDL2_VARS(NULL, NULL, "total_body_size=%d, if_pfx_sfx=%hd ,"
                         "ns_http_body_checksum_buffer = %s",
                         total_body_size, lgset->http_body_chksum_hdr.if_pfx_sfx, ns_http_body_checksum_buffer);

  // Add Checksum header
  int cur_idx = NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector);
  g_req_rep_io_vector.cur_idx = http_body_chksum_hdr_idx; 
  NS_FILL_IOVEC(g_req_rep_io_vector, lgset->http_body_chksum_hdr.hdr_name, lgset->http_body_chksum_hdr.hdr_name_len);
  NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, ns_http_body_checksum_buffer, NS_MD5_CHECKSUM_LENGTH + 2);
  g_req_rep_io_vector.cur_idx = cur_idx;
  return;
}

/* save Url in following format:
 * http(s)://<Host>/<Page> 
 * url_offset is pointed to page of saved url
 * E.g:
 *     Url:        =>   http(s)://www.google.com/index.html
 * url_offset has  =>                           |
 *                                             Offset
 * */
// This method will call insert_segments and fill request body in a local io vector. After that it will copy the request
// body in to a buffer that will be used to encode xml to binary format. Body will be encoded to hessian
inline void get_abs_url_req_line(action_request_Shr *request, VUser  *vptr, char *url, 
                       char *full_url, int *full_url_len, int *url_offset, int max_len) {

  int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
  PerHostSvrTableEntry_Shr* svr_entry;
  int buffer_left = 0;
  
  NSDL2_HTTP(NULL, NULL,  "Method Called, max_len = %d", max_len);

  /*Initianlization*/
  *full_url_len = 0;
  full_url[0] = '\0';

  if(request->request_type == HTTP_REQUEST) {
    *full_url_len = sprintf(full_url, "%s", "http://");
     NSDL3_HTTP(NULL, NULL,  "full_url = %s, full_url_len = %d", full_url, *full_url_len);
  } else if (request->request_type == HTTPS_REQUEST) {
    *full_url_len = sprintf(full_url, "%s", "https://");
     NSDL3_HTTP(NULL, NULL,  "full_url = %s, full_url_len = %d", full_url, *full_url_len);
  } else {
    NSDL3_HTTP(NULL, NULL,  "request_type is neither HTTP_REQUEST nor HTTPS_REQUEST");
    return;
  }

  if (use_rec_host == 0) { //Send actual host (mapped)
     svr_entry = get_svr_entry(vptr, request->index.svr_ptr);
     *full_url_len = sprintf(full_url, "%s%s", full_url, svr_entry->server_name);
      NSDL3_HTTP(NULL, NULL,  "full_url = %s, full_url_len = %d", full_url, *full_url_len);
  } else { //Send recorded host
    *full_url_len = sprintf(full_url, "%s%*.*s", full_url, 
                                      request->index.svr_ptr->server_hostname_len,
                                      request->index.svr_ptr->server_hostname_len,
                                      request->index.svr_ptr->server_hostname);
    NSDL3_HTTP(NULL, NULL,  "full_url = %s, full_url_len = %d", full_url, *full_url_len);
  }

  *url_offset = *full_url_len;

  buffer_left = max_len - *full_url_len;
  //*full_url_len += sprintf(full_url + *full_url_len, url, buffer_left);
  *full_url_len += snprintf(full_url + *full_url_len, buffer_left, "%s", url);
  full_url[*full_url_len] = '\0';
  NSDL3_HTTP(NULL, NULL,  "full_url = %s, full_url_len = %d", full_url, *full_url_len);
}

char *http_get_url_version(connection *cptr) {
  int http_version = cptr->url_num->proto.http.http_version;
  NSDL2_HTTP(NULL, cptr, "Method Called, http_version = %d", http_version); 

  if(http_version == 0 || http_version == 1) {
    return(http_version_buffers[http_version]);
  } else {
    NSDL2_HTTP(NULL, cptr, "Invalid http_version");
    return NULL;
  }
}

// Returns the actual method name used
// For example, is PUT is treated like POST, then PUT is returned not POST
char *http_get_url_method(connection *cptr) {
  int method_idx = cptr->url_num->proto.http.http_method_idx;
  NSDL2_HTTP(NULL, cptr, "Method Called, method = %d", method_idx); 
//  if(method < HTTP_METHOD_GET || method > HTTP_METHOD_PUT) {
//    NSDL2_HTTP(NULL, cptr, "Invalid method");
//    return NULL;
//  }
  return(http_method_table_shr_mem[method_idx].method_name);
}

/* Returns url only
 * e.g: /tours/index.html
 * */
char *get_url_req_url(connection *cptr) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  if(cptr->url) {
    NSDL2_HTTP(NULL, cptr,  "cptr->url = [%s]", cptr->url);
  } else {
    NSDL2_HTTP(NULL, cptr, "cptr->url is NULL");
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                  __FILE__, (char*)__FUNCTION__, "cptr->url is NULL");
  }

  return cptr->url;
}

/* Returns url only
 * e.g: GET /tours/index.html HTTP/1.1
 * */
char *get_url_req_line(connection *cptr, char *req_line_buf, int *req_line_buf_len, int max_len) {

  char *url;
  NSDL2_HTTP(NULL, cptr, "Method Called");
  url = get_url_req_url(cptr);
  req_line_buf[0] = '\0';

  if(url) {
    *req_line_buf_len = snprintf(req_line_buf, max_len, "%s%s%s",
	      		                       http_get_url_method(cptr),
				               get_url_req_url(cptr),
				               http_get_url_version(cptr));
    req_line_buf[*req_line_buf_len] = '\0';
  } else {
    *req_line_buf_len = 0;
  }
  
  req_line_buf[*req_line_buf_len - 2] = '\0';   //-2 is for removing \r\n from the req_line_buf (for g_debug_script mode)
  NSDL2_HTTP(NULL, cptr, "req_line_buf = %s, req_line_buf_len = %d",
			  req_line_buf, *req_line_buf_len);
  return req_line_buf;
}

static int cache_check_for_new_req(connection *cptr, VUser *vptr,
                           u_ns_ts_t now, char *url_local, int url_local_len) {

  NSDL2_CACHE(NULL, cptr, "Method Called, url_local = [%s], url_local_len = %d",
			   url_local, url_local_len);

  int ret = cache_check_if_url_cached(vptr, cptr, now, url_local, &url_local_len);

  //BugID: 92230 - cptr url must free and reset it flag if it parametrised 
  FREE_CPTR_URL(cptr);

  // Point url in cptr to cache table entry
  cptr->url = (char*)(((CacheTable_t*)cptr->cptr_data->cache_data)->url +
              ((CacheTable_t*)cptr->cptr_data->cache_data)->url_offset);

  cptr->url_len = ((CacheTable_t*)cptr->cptr_data->cache_data)->url_len -
                  ((CacheTable_t*)cptr->cptr_data->cache_data)->url_offset;

  NSDL2_CACHE(NULL, cptr, "After saving url in cptr, cptr->url = %s"
			  " cptr->url_len = %hu", cptr->url, cptr->url_len);

  if(ret == CACHE_NEED_TO_VALIDATE) {
     NSDL2_CACHE(NULL, cptr, "Need to Validate, make-request must add the validators.");
   } else if (ret == CACHE_RESP_IS_FRESH) {
     NSDL2_CACHE(NULL, cptr, "Response is fresh. It will served from cache");

     if(!(((CacheTable_t*)cptr->cptr_data->cache_data)->cache_flags & NS_CACHE_ENTRY_NOT_FOR_CACHE)) {
#ifdef NS_DEBUG_ON
       //cache_debug_log_cache_req(cptr, buf, bytes_read);

       cache_debug_log_cache_req(cptr, url_local, url_local_len);
       cache_debug_log_cache_res(cptr);
#else
       CACHE_LOG_CACHE_REQ(cptr, url_local, url_local_len);
       CACHE_LOG_CACHE_RES(cptr);
#endif

       cache_set_resp_frm_cache(cptr, vptr);
       //vptr->flags |= NS_RESP_FRM_CACHING;
     } else {
       NSDL2_CACHE(NULL, cptr, "Response is fresh. NS_CACHE_ENTRY_NOT_FOR_CACHE");
       return CACHE_NOT_ENABLE; 
     }
   }

   return ret;
}

void make_part_of_relative_url_to_absolute(char *url, int url_len, char *out_url)
{
  char *prev_slash_ptr = NULL, *ptr = NULL;
  NSDL2_HTTP(NULL, NULL,  "Method called,url = %s, url_len = %d", url, url_len);

  strncpy(out_url, url, url_len);
  out_url[url_len] = '\0';
  ptr = out_url;

  while(*ptr)
  {
    if(!strncmp(ptr, "/../", 3))
    {
      NSDL2_HTTP(NULL, NULL,  "ptr = %s, prev_slash_ptr = %p", ptr, prev_slash_ptr);
      if(!prev_slash_ptr){
        out_url[0] = '\0';
        NSDL2_HTTP(NULL, NULL,  "ptr = %s, prev_slash_ptr = %p", ptr, prev_slash_ptr);
        return;
        //return NULL;
      }

      strcpy(prev_slash_ptr, ptr+3);
      // Restart from beginning
      prev_slash_ptr = NULL;
      ptr = out_url;
      continue;
    }

    if(*ptr == '/')
    {
      prev_slash_ptr = ptr;
      ptr++;
      continue;
    }

    ptr++;
  }
  NSDL2_HTTP(NULL, NULL,  "ptr = %s, out_url = %s", ptr, out_url);
}

/* Name: http_make_url_and_check_cache
 * Purpose: This function fills cptr->url & cptr->url_len 
 *   cptr->url may be malloced/pointed.
 *   Malloc is done and cptr->flags bit is set in following case:
 *     o URL is not cached
 *     o Number of segments is 1 & type is not STR (i.e. url is parametrized)
 *     or segment is > 1
 * Return:
 *   Caching not enabled or
 *   What ever is returned from cache_check_for_new_req()
 */ 	  
inline void http_make_url_and_check_cache(connection *cptr, u_ns_ts_t now, int *ret) {

  //Narendra: 16/Nov/2013 - removed stack variables, global variable will be used(changes for bug 6199)
  //struct iovec vector[IOVECTOR_SIZE];
  //int free_array[IOVECTOR_SIZE];
  int num_vectors = 0;
  int i;
  char url_buf[MAX_LINE_LENGTH + 1];  // Its 35000 
  VUser *vptr = cptr->vptr;
  char *url_local = NULL;
  unsigned short url_local_len = 0;
  char allocate_cptr_url = 0;
  char val_value_flag = 1;
  int need_to_update_url_len = 0;//Set flag in case of request->url.num_entries is 1 and request->url.seg_start->type is STR 

  NSDL2_HTTP(NULL, cptr,  "Method Called");

  NS_RESET_IOVEC(g_scratch_io_vector);
  http_request_Shr *request = &(cptr->url_num->proto.http);

  /* If this is redirect url created due to auto redirect, use this URL */
  if (request->redirected_url) {  
    NSDL2_HTTP(NULL, cptr,  "Getting url from redirected url = [%s]", request->redirected_url);
    url_local =  request->redirected_url;
    // TODO: Should we keep len in structure (use some field) 
    url_local_len = strlen(request->redirected_url);
  }
  else if (cptr->url_num->is_url_parameterized)
  {
    url_local = cptr->url;
    url_local_len = cptr->url_len;
    NSDL2_HTTP(NULL, cptr,  "pameterized_url, url_local_len = %d, url_local = %s", url_local_len, url_local);
  } 
  // In case of replay access log we will take url from ctpr->url, as we are setting it in make request. First time it will take from script
  else if((cptr->url_num->proto.http.type == MAIN_URL) && (global_settings->replay_mode) && (cptr->url)) {
    NSDL2_HTTP(NULL, cptr,  "Getting url from replay main url = [%s]", cptr->url);
    url_local = cptr->url;
    url_local_len = cptr->url_len;
  // Here we come for all other URLs including those from auto fetch
  } else {  /* we want segments attached */
    // Since there is only 1 seg and it is STR, we can point to it
    NSDL2_HTTP(NULL, cptr,  "num_entries = %d, type = %d",
                             request->url.num_entries,
                             request->url.seg_start->type);

    if(request->header_flags & NS_URL_CLICK_TYPE){
      //This is for Click and script.
      //Get URL from DOM

       NSDL4_HTTP(vptr, cptr,  "request->header_flags has NS_URL_CLICK_TYPE bit set. Reading clicked_url from vptr->httpData");

      url_buf[0] = url_buf[MAX_LINE_LENGTH] = '\0';
      url_local = url_buf;

      if(vptr->httpData->clicked_url != NULL)
      {
        strncpy(url_local, vptr->httpData->clicked_url, vptr->httpData->clicked_url_len);
        url_local_len = vptr->httpData->clicked_url_len;
        url_local[url_local_len] = '\0';

        NSDL4_HTTP(vptr, cptr,  "Retrieved Clicked URL from vptr->httpData; "
                                "vptr->httpData->clicked_url='%s', "
                                "vptr->httpData->clicked_url_len=%d ", 
                                vptr->httpData->clicked_url?vptr->httpData->clicked_url:"NULL", 
                                vptr->httpData->clicked_url_len);

        FREE_AND_MAKE_NULL(vptr->httpData->clicked_url, "vptr->httpData->clicked_url", 0);
        vptr->httpData->clicked_url_len = 0;

        extract_and_set_url_params(cptr, url_local, &url_local_len);

        set_server(cptr);
      }
      allocate_cptr_url = 1;  // Yes Do Allocate
    }
    else if (request->url.num_entries == 1 && request->url.seg_start->type == STR) {
      url_local = request->url.seg_start->seg_ptr.str_ptr->big_buf_pointer;
      url_local_len = request->url.seg_start->seg_ptr.str_ptr->size;
      need_to_update_url_len = 1;//Need to update URL length
    } else {
       num_vectors = insert_segments(vptr, cptr, &(request->url), &g_scratch_io_vector,
                                     NULL, 0, val_value_flag, REQ_PART_REQ_LINE, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

       // Why it can be <= 0
       if (num_vectors == 0)  {
        url_local = request->url.seg_start->seg_ptr.str_ptr->big_buf_pointer;
        url_local_len = strlen(request->url.seg_start->seg_ptr.str_ptr->big_buf_pointer);
       } else if(num_vectors > 0){
         // Combine all vectors in one big buf and malloc
         url_buf[0] = url_buf[MAX_LINE_LENGTH] = '\0';
         url_local = url_buf;
         for (i = 0; i < num_vectors; i++) {
           if (g_scratch_io_vector.vector[i].iov_len != 0) {
             /* abort filling it since it will go out of bounds. The resultant 
              * URL will be truncated. */
             if ((url_local_len + g_scratch_io_vector.vector[i].iov_len) > MAX_LINE_LENGTH) {
               memcpy(url_buf + url_local_len, g_scratch_io_vector.vector[i].iov_base, 
                      MAX_LINE_LENGTH - url_local_len); 
               url_local_len = MAX_LINE_LENGTH;
               NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, 
                              "Url is truncated due to bigger size( > %d), Url[%s]",
			      MAX_LINE_LENGTH, url_buf);
               break;  
             } else {
               memcpy(url_buf + url_local_len, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
               url_local_len += g_scratch_io_vector.vector[i].iov_len;
             }
           }
         }
         url_buf[url_local_len] = '\0';
         NS_FREE_RESET_IOVEC(g_scratch_io_vector);
         allocate_cptr_url = 1;  // Yes Do Allocate
       }else {  //If insert_segment returns -1(negative) then set error and end test.
         if(num_vectors == MR_USE_ONCE_ABORT){
           *ret = -10;
           return;
         }
       
         fprintf(stderr, "http_make_url_and_check_cache(): Failed in creating the url\n"); 
         end_test_run();
         return; 
       }
    }
  }
  // Here we are making allocate_cptr_url 1 to support fully qualified url we have to allocate url as we need more memory to save
  // fully qualified url 
  if(request->header_flags & NS_FULLY_QUALIFIED_URL){
    allocate_cptr_url = 1; // allocate url in case of FullyQualifiedURL 
  }

/********* Bug 3570 - Begin */
  /* if there is a '#' in the url, this is a sectional link in the page.
   * The part of the url after the # should not go to the network.
   */
  /* Performance issue: Using strchr in code decreased CPS by 300, this validation can be handled in 
   * redirection and auto fetch at init time, rather verifying URLs here,
   * For main urls, at the time of parsing we can remove #*/
  
  //Bhuvendra: Release#3.9.4 Build#3, In case of RBU, we need to pass complete URL. Because that is Required by
  //Browser. So We will not check for # in case of RBU.
  if((runprof_table_shr_mem[vptr->group_num].gset.ignore_hash) && (!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu))
  {
    char *ptr_hash_in_url = strchr(url_local, '#');
    if(ptr_hash_in_url)
    {
      NSDL2_HTTP(NULL, cptr,  "Found '#' in url_local='%s', truncating the url", 
                            url_local?url_local:"NULL");

      *ptr_hash_in_url = '\0';
      url_local_len = ptr_hash_in_url - url_local;
    /* Bug#3570: Changes were done to update request size, earlier only request URL got updated
     * and length remained unchanged. Hence for first session url request file was as per expected
     * whereas in second session url in request file was added with some junk value at the end*/
      if(need_to_update_url_len)
      {
        request->url.seg_start->seg_ptr.str_ptr->size = url_local_len; //Update request size
      } 
      NSDL2_HTTP(NULL, cptr,  "truncated url_local='%s', url_local_len=%d seg_ptr.str_ptr->size =%d",
                              url_local?url_local:"NULL", url_local_len,request->url.seg_start->seg_ptr.str_ptr->size);
    }
  }
/********* Bug 3570 - End */
  
  NSDL2_HTTP(NULL, cptr,  "url_local_len = [%d], url_local = [%s]",
                           url_local_len, url_local);
  // If caching is NOT enabled for the user, 
  //   - cptr->url is either pointed or malloced (as per logic above)
  // If caching is enabled for the user, 
  //   - url is ALWAYS stored in cache table only using malloced buffer
  //     (Either alrady in cache or we create tmp cache_table entry)
  //   - cptr will point to url in cache (done in cache_check_for_new_req)

  // Caching is not enabled
  if(!NS_IF_CACHING_ENABLE_FOR_USER) {

    cptr->url_len = url_local_len; // Set length
    if(allocate_cptr_url) {
      NSDL2_CACHE(NULL, cptr, "Allocating and copying url [%s] to cptr->url",
                  			     url_local);	
      
      // Make fully qualified url for FullyQualifiedURL in NS 
      if(request->header_flags & NS_FULLY_QUALIFIED_URL)
      {
        char request_type[10];
        short req_len;
        char *full_url_name_ptr;
        if(cptr->url_num->request_type == HTTP_REQUEST)
        {
          strcpy(request_type, "http://");
          req_len = 7;
        } else if(cptr->url_num->request_type == HTTPS_REQUEST){
          strcpy(request_type, "https://");
          req_len = 8;
        }
        PerHostSvrTableEntry_Shr* svr_entry;
        svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);
        cptr->url_len += req_len + svr_entry->server_name_len; 
        MY_MALLOC(cptr->url, cptr->url_len + 1, "cptr->url_len", -1);
        full_url_name_ptr = cptr->url;
        strncpy(full_url_name_ptr, request_type, req_len);
        full_url_name_ptr +=  req_len;
        strncpy(full_url_name_ptr, svr_entry->server_name, svr_entry->server_name_len);
        full_url_name_ptr +=  svr_entry->server_name_len;
        strcpy(full_url_name_ptr, url_local);
        NSDL2_CACHE(NULL, cptr, "In case of FullyQualifiedURL, FullyQualifiedURL is = %s", cptr->url);
      }
      else
      {
        MY_MALLOC(cptr->url, cptr->url_len + 1, "cptr->url_len", -1);
        strcpy(cptr->url, url_local);
      }
      cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
    } else {
      cptr->url = url_local;
    }
/***** Manmeet begin *****/
     copy_main_url_in_vptr_http_data(cptr, url_local, url_local_len);
/***** Manmeet end *****/
    *ret = CACHE_NOT_ENABLE;
    return;
  } else {
    *ret = cache_check_for_new_req(cptr, vptr, now, url_local, url_local_len);
    return;
  }
}

int make_http2_setting_frame(char **frame, int *frame_len)
{
  int offset = 0 ; // stores maximum size filled in (settings) buffer
  unsigned int length = 6; //length of frame payload 
  unsigned char type = 0x4; // this store type of frame to be sent , 
  unsigned char flag = 0x0 ; //flag in case unset 
  unsigned char reserved = 0; //Reserved must be set 0 while sending 
  unsigned int stream_identifier = 0x0 ; //stream identifier for connection  
  unsigned char settings_buffer[1024] = {0};
  unsigned int value = 100;
  unsigned short ident = 0x3;
 
  //copying length to settings frame 
  memcpy(settings_buffer + offset , &length , 3);
  offset+=3;

  //copying type to settings frame
  memcpy(settings_buffer +offset, &type, 1);
  offset += 1;

  //copying  flag to settings frame
  memcpy(settings_buffer + offset, &flag ,1);
  offset += 1;

  //Copying reserved to settings frame
  memcpy(settings_buffer + offset, &reserved , 1);
  offset += 1;

  //Copying stream identifier 
  memcpy(settings_buffer +offset, &stream_identifier,4);
  offset += 1;
 
  //copying settings identifier
  memcpy(settings_buffer+ offset, &ident ,2);
  offset += 2;

  //copying setting value
  memcpy(settings_buffer + offset, &value, 4);
  offset += 4;
 
  //encode settings frame
  nslb_base64_url_encode(settings_buffer, offset, (unsigned char**)frame, frame_len);
  return 0;
}
