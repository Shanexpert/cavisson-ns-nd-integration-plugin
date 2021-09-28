/**
 *  ns_http_process_resp.c (earliar ns_handle_read.c)
 *
 * This file contains HTTP/HTTPS date retrival methods.
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <regex.h>
#include <ctype.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
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

#include "netstorm.h"
#include "ns_http_process_resp.h"
#include "decomp.h"
#include "ns_sock_list.h"
#include "ns_sock_com.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include <string.h>
#include "ns_alloc.h"
#include "ns_debug_trace.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "wait_forever.h"
#include "ns_page.h" 
#include "ns_http_pipelining.h"
#include "comp_decomp/nslb_comp_decomp.h"
#include "nslb_http_state_transition_init.h"
#include "ns_http_hdr_states.h"
#include "ns_common.h"
#include "ns_http_cache_table.h"
#include "ns_http_cache.h"
#include "ns_http_cache_store.h"
#include "ns_http_cache_hdr.h"
#include "ns_data_types.h"
#include "ns_http_cache_reporting.h"
#include "ns_http_cache_hdr.h"
#include "ns_http_auth.h"
#include "ns_vuser_trace.h"
#include "ns_page_dump.h"
#include "ns_network_cache_reporting.h"
#include "ns_http_hdr_state_array.c"
#include "ns_url_resp.h"
#include "ns_websocket.h"
#include "ns_websocket_reporting.h"
#include "ns_vuser_ctx.h"
#include "ns_group_data.h"
#include "ns_trace_level.h"
#include "ns_trace_level.h"
#include "ns_exit.h"
#include "ns_sockjs.h"
#include "output.h"
#include "ns_child_thread_util.h"
#include "ns_script_parse.h"
#include "ns_socket.h"

// This method will alocate vptr->response_hdr and copy response header to vptr->response_hdr->hdr_buffer.
// This method handles partial header 
int response_header_init_size = 1024;

inline void save_header(VUser *vptr, char *buf, int bytes_read){
  int hdr_len;

  NSDL2_HTTP(vptr, NULL, "Method called.");

  // If response_hdr is not allocated, then allocate it 
  if(vptr->response_hdr == NULL){
    MY_MALLOC_AND_MEMSET(vptr->response_hdr, sizeof(ResponseHdr), "vptr->response_hdr", -1);
    MY_MALLOC(vptr->response_hdr->hdr_buffer, response_header_init_size, "vptr->response_hdr->hdr_buffer", -1);
    vptr->response_hdr->hdr_buf_len = response_header_init_size;
  }
 
  hdr_len = bytes_read;
  NSDL2_HTTP(vptr, NULL, "hdr_len = %d", hdr_len);

  // Reallocate if needed
  if((vptr->response_hdr->hdr_buf_len - vptr->response_hdr->used_hdr_buf_len) < hdr_len)  {
    MY_REALLOC(vptr->response_hdr->hdr_buffer, vptr->response_hdr->hdr_buf_len + hdr_len, "vptr->response_hdr->hdr_buffer", -1);
    vptr->response_hdr->hdr_buf_len += hdr_len;
  }

  strncpy((vptr->response_hdr->hdr_buffer + vptr->response_hdr->used_hdr_buf_len), buf, hdr_len); 
  vptr->response_hdr->used_hdr_buf_len += hdr_len;
  vptr->response_hdr->hdr_buffer[vptr->response_hdr->used_hdr_buf_len] = '\0';
}

void free_cptr_buf(connection* cptr)
{
  struct copy_buffer *cur_buf, *next_buf;

  NSDL2_HTTP(cptr->vptr, cptr, "Freeing cptr buffers, vptr = %p, cptr = %p, cptr->buf_head = %p", cptr->vptr, cptr, cptr->buf_head);

  cur_buf = cptr->buf_head;

  while(cur_buf)
  {
    next_buf = cur_buf->next; 
    FREE_AND_MAKE_NULL_EX(cur_buf, sizeof(cur_buf), "Freeing cptr->buf_head", -1);
    cur_buf = next_buf;
  }   

  cptr->buf_head = NULL;
}

// Before calling this adjust cptr->total_len
void handle_partial_recv(connection* cptr, char *buffer, int length, int total_size)
{
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer;
  int start_buf = 0;

  NSDL2_HTTP(cptr->vptr, cptr, "Method called, cptr = %p, buffer = %p, len = %d, total_len = %d", cptr, buffer, length, total_size);

  if(!total_size) 
    start_buf = 1; // setting start_buf flag

  while (length) {
    copy_offset = total_size % COPY_BUFFER_LENGTH; 
    copy_length = COPY_BUFFER_LENGTH - copy_offset;
    NSDL4_HTTP(NULL, cptr, "copy_offset = %d, copy_length = %d, length = %d, total_size = %d", 
                            copy_offset, copy_length, length, total_size);

    if (!copy_offset) {
      MY_MALLOC(new_buffer, sizeof(struct copy_buffer), "new copy buffer", -1);
      if (new_buffer) {
	new_buffer->next = NULL;
	if (start_buf) {
	  cptr->buf_head = cptr->cur_buf = new_buffer;
	  start_buf = 0;
	}
	else {
	  cptr->cur_buf->next = new_buffer;
	  cptr->cur_buf = new_buffer;
	}
      }
    }
    if (length <= copy_length) 
      copy_length = length;    // This sets the copy length if chunk recieved less than COPY_BUFFER_LENGTH

    memcpy(cptr->cur_buf->buffer+copy_offset, buffer, copy_length); 
    total_size += copy_length; 
    length -= copy_length;     
    buffer += copy_length;     
    NSDL4_HTTP(NULL, cptr, "After copy buffer of lengh %d, buffer = %s", copy_length, buffer);
  }
}

void free_cptr_read_buf(connection *cptr)
{
  struct copy_buffer *cur_buf = cptr->buf_head;
  struct copy_buffer *prev_buf = NULL;
  int bufid = 0;

  NSDL3_HTTP(NULL, cptr, "Method called, Going to free cptr read buf nodes");

  while(cur_buf)
  {
    prev_buf = cur_buf;
    cur_buf = cur_buf->next;
    FREE_AND_MAKE_NOT_NULL(prev_buf, "Freeing cptr read buf", bufid);
    bufid++;
  }

  NSDL3_HTTP(NULL, cptr, "Freed total number of cptr buffer nodes = %d", bufid);
}

/* 
  Function Name : copy_retrieve_data()
  Purpose       : This function will copy data from buffer node by node in chunk of 1022
  
  Inputs Args   : buffer     - Input buffer to copy in cptr->cur_buf->buffer
                  length     - Length of the input buffer
                  total_size - Total size of buffer length copied. 
                               Initially total_size is 0 for starting chunk of buffer 

      LENGTH      - 5000                (5000-1022)3978     (3978-1022)2956      (2956-1022)1934     (1934-1022)912
      COPY_LENGTH - 1022                1022                1022                 1022                912
                   ------ -----        ------ -----        ------ -----         ------ -----        ------ -----  
           HEAD-->| DATA |DEST | ---> | DATA |DEST | ---> | DATA |DEST | --->  | DATA |DEST | ---> | DATA |NULL | 
                   ------ -----        ------ -----        ------ -----         ------ -----        ------ -----  
                   TOTAL_SIZE - 1022  TOTAL_SIZE - 2044   TOTAL_SIZE - 3066    TOTAL_SIZE - 4088   TOTAL_SIZE - 5000 
*/


//total_size is the total_size saved so far prior to this call
//After call to this fucntion , source of total bytes must be incremted by length
void copy_retrieve_data( connection* cptr, char* buffer, int length, int total_size ) 
{
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer;
  int start_buf = 0;

  NSDL2_HTTP(NULL, cptr, "Method called. length = %d, total_size = %d, cptr->flags = %0x, url = %s", 
                          length, total_size, cptr->flags, cptr->url?cptr->url:"NULL");

  //Here we will save body into file if cptr flag is set 
  //We need to add the following check here because we cann't add this check outside of this function 
  if((cptr->flags & NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY) || (cptr->flags & NS_CPTR_CONTENT_TYPE_MEDIA))
  {
    save_http_resp_body(cptr, buffer, length, total_size); 
    return;
  }

  if(!total_size) 
    start_buf = 1; // setting start_buf flag

  while (length) {
    copy_offset = total_size % COPY_BUFFER_LENGTH; 
    copy_length = COPY_BUFFER_LENGTH - copy_offset;
    NSDL4_HTTP(NULL, cptr, "copy_offset = %d, copy_length = %d, length = %d, total_size = %d", 
                            copy_offset, copy_length, length, total_size);

    if (!copy_offset) {
      MY_MALLOC(new_buffer, sizeof(struct copy_buffer), "new copy buffer", -1);
      if (new_buffer) {
	new_buffer->next = NULL;
	if (start_buf) {
	  cptr->buf_head = cptr->cur_buf = new_buffer;
	  start_buf = 0;
	}
	else {
	  cptr->cur_buf->next = new_buffer;
	  cptr->cur_buf = new_buffer;
	}
      }
    }
    if (length <= copy_length) 
      copy_length = length;    // This sets the copy length if chunk recieved less than COPY_BUFFER_LENGTH

    memcpy(cptr->cur_buf->buffer+copy_offset, buffer, copy_length); 
    total_size += copy_length; 
    length -= copy_length;     
    buffer += copy_length;     
    NSDL4_HTTP(NULL, cptr, "After copy buffer of lengh %d, buffer = %s", copy_length, buffer);
  }
}

// This method will be used to save response for http2, as in multiplexing response can be multiplexed, it need to be save to its stream      
void
http2_copy_retrieve_data(connection *cptr, stream* sptr, char* buffer, int length, int total_size ) {
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer;
  int start_buf = 0;
  //VUser *vptr = cptr->vptr;

  NSDL2_HTTP(NULL, NULL, "Method called. length = %d, total_size = %d, sptr->flags = %0x, url = %s", length, total_size, sptr->flags, sptr->url?sptr->url:"NULL");

  //Here we will save body into file if sptr flag is set 
  //We need to add the following check here because we cann't add this check outside of this function 
  if(sptr->flags & NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY)
  {
    save_http_resp_body(cptr, buffer, length, total_size); 
    return;
  }

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

/* It is assumed after calling this function Close_connection will be called. */
static void
copy_retrieve_bad_data(connection* cptr, char* buffer, int length, int total_size, 
                        char *err_msg)
{
  int len;
  char ip_info[4098];            /* should also accomodate IPV6 */

  NSDL2_HTTP(NULL, cptr, "Method called. length = %d, total_size = %d, err_msg = %s", 
             length, total_size, err_msg);

  /* Save src IP */
  strcpy(ip_info, "SrcIP:");
  strcat(ip_info, nslb_get_src_addr(cptr->conn_fd));
  strcat(ip_info, ",");
  len = strlen(ip_info);
  copy_retrieve_data(cptr, ip_info, len, total_size);
  cptr->bytes += len;
  cptr->total_bytes += len;
  total_size += len;

  /* Save Dest IP */
  strcpy(ip_info, "DestIP:");
  strcat(ip_info, nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));
  strcat(ip_info, ",");
  len = strlen(ip_info);
  copy_retrieve_data(cptr, ip_info, len, total_size);
  cptr->bytes += len;
  cptr->total_bytes += len;
  total_size += len;

  /* Save error detected by netstorm for Tracing URL failures. */
  len = strlen(err_msg);
  copy_retrieve_data(cptr, err_msg, len, total_size);
  cptr->bytes += len;
  cptr->total_bytes += len;
  total_size += len;


  /* To save the un handled data received. */
  copy_retrieve_data(cptr, buffer, length, total_size);
  cptr->bytes += length;
  cptr->total_bytes += length;
  total_size += length;

  cptr->bytes = cptr->total_bytes;
  cptr->total_bytes = 0;
}

static void
save_embedded(connection *cptr, char *lbuf, int len, int tsize)
{
char *addr, *ustr, *ptr, *ptr1;
char dname[MAX_FILE_NAME];
char fname[MAX_FILE_NAME];
char buf[MAX_FILE_NAME];
int fd;
 action_request_Shr *url_num;
struct stat statbuf;

 url_num = get_top_url_num(cptr);
    NSDL2_HTTP(NULL, cptr, "Method called");

    if (url_num->proto.http.type != EMBEDDED_URL)
	return;

    if (!getcwd(buf, MAX_FILE_NAME)) {
        NS_EXIT(-1, "error in getting pwd");
    }

    //addr = inet_ntoa (cptr->cur_server.sin_addr);
    addr = nslb_sock_ntop((struct sockaddr *)&cptr->cur_server);
    if (!addr) {
	printf("Current address conversion fails\n");
	return ;
    }

    	NSDL3_HTTP(NULL, cptr, "address is %s\n", addr);

    //URL part will be in the format GET URL HTTP/1.1\r\n
    ustr = get_url_req_url(cptr);
    ptr = index (ustr, ' ');
    if (!ptr) {
	printf("url '%s' string format not OK (expected 'GET URL HTTP/1.x')\n", ustr);
	return ;
    }
    ptr++;
    if (*ptr == '/') ptr++;
    //ptr points to begining of URL. Set up the other end of URL
    ptr1 = index (ptr, ' ');
    if (!ptr1) {
	printf("url '%s' string format not OK (expected 'GET URL HTTP/1.x')\n", ptr);
	return ;
    }
    *ptr1 = 0;

    //ptr now points to a string containg whole URL. Just get base URL
    ptr1 = index (ptr, '?');
    if (ptr1)  *ptr1 = '\0';

    //Get the Directory and file name : fname is ....logs/TRxx/data/ip-addr/url-path
    ptr1 = rindex (ptr, '/');
    if (ptr1) {
	*ptr1 = '\0';
	ptr1++;
	sprintf(dname,"%s/logs/TR%d/data/%s/%s", buf, testidx, addr, ptr);
	sprintf(fname,"%s",ptr1);
    } else {
	sprintf(dname,"%s/logs/TR%d/data/%s", buf, testidx, addr);
	sprintf(fname,"%s",ptr);
    }
    sprintf (buf, "mkdir -p %s", dname);

        NSDL3_HTTP(NULL, cptr, "making dir using cmd '%s'", buf);
    system(buf);
    sprintf(buf, "%s/%s", dname, fname);

    	NSDL3_HTTP(NULL, cptr, "creating file '%s'", buf);

    //If file already exist with higher size, quite possibly this object is duplicate
    if ((stat (buf, &statbuf) == 0) && (statbuf.st_size > tsize)) {
	return;
    }

    fd = open (buf, O_CREAT|O_WRONLY|O_CLOEXEC|O_APPEND, 00666);
    if (!fd) {
	printf("open for '%s' failed\n", buf);
	return ;
    }

    write (fd, lbuf, len);

    close(fd);
}

static inline void
calculate_checksum(connection* cptr, char* buf, int start_byte, int last_byte) {
  int i;
  int checksum = cptr->checksum;

  NSDL2_HTTP(NULL, cptr, "Method called");
  for (i = start_byte; i < last_byte; i++ ) {
    if ( checksum & 1 )
      checksum = ( checksum >> 1 ) + 0x8000;
    else
      checksum >>= 1;
    checksum += buf[i];
    checksum &= 0xffff;
  }
  cptr->checksum = checksum;
}

#if 0
// This is used to decide if each chunk is to be uncompressed as 
// independent buffer or complete response is to be uncompressed
// Not used
int g_uncompress_chunk = 0;

static void 
copy_chunk_size_node(connection *cptr)
{
  chunk_size_node *new;

  NSDL2_HTTP(NULL, cptr, "Method called");
  MY_MALLOC(new, sizeof (chunk_size_node), "chunk_size_node", -1);

  new->chunk_sz = cptr->content_length;
  new->next = NULL;

  NSDL3_HTTP(NULL, cptr, "Method called. chunk_sz = 0x%x(%d)", new->chunk_sz, new->chunk_sz);

  if (cptr->chunk_size_head == NULL) {
    cptr->chunk_size_head = cptr->chunk_size_tail = new;
  } else {
    cptr->chunk_size_tail->next = new;
    cptr->chunk_size_tail=new;
  }
}
#endif

static inline void log_bad_data(connection *cptr, char *buf, int buf_len)
{
  NSDL1_HTTP(NULL, cptr, "Method called");

  char cptr_to_str[35000 + 1];
  int i;
  cptr_to_string(cptr, cptr_to_str, 35000);
  fprintf(stderr, "Error: Binary data in HTTP response header. Request will be aborted. Connection Info = %s\n", cptr_to_str);
  fprintf(stderr, ">>Response dump start (length = %d):\n", buf_len);
  for(i = 0; i < buf_len; i++)
    fprintf(stderr, "%X", buf[i]);

  fprintf(stderr, ">>Response Dump end\n");
}

inline void handle_bad_read (connection *cptr, u_ns_ts_t now)
{
   NSDL2_HTTP(NULL, cptr, "Method called");
   if (cptr->req_code_filled < 0) {
     retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
   } else {
     NSDL3_HTTP(NULL, cptr, "BADBYTE:2 err=%s con_state=%d content_length=%d,"
                            " req_code_filled=%d req_code=%d bytes=%lu "
                            " tcp_in=%lu, cur=%lu first=%lu start=%u "
                            " connect_time=%d write=%d",
                            nslb_strerror(errno), cptr->conn_state, cptr->content_length,
   		            cptr->req_code_filled, cptr->req_code, cptr->bytes,
                            cptr->tcp_bytes_recv, get_ms_stamp(),
                            cptr->first_byte_rcv_time, cptr->started_at, 
                            cptr->connect_time,
                            cptr->write_complete_time);

   if ((cptr->req_code_filled == 0) && (cptr->conn_state != CNST_HEADERS))
      Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_READ);
   else
      Close_connection(cptr, 0, now, NS_REQUEST_BAD_HDR, NS_COMPLETION_BAD_READ);
  }
}

/* 
* This is check function and used either to call copy_retrieve_data() 
  that is used to copy the data to the full_buffer for registration apis. 
* Returns 1, if copy_retrieve_data() is required to call for this api.
  Returns 0, if no need to call copy_retreive_data() for this api. 
*/
//before code was as:
/* if ((!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation) &&
       ((cptr->url_num->proto.http.type == MAIN_URL && !cptr->location_url) ||
        (cptr->url_num->proto.http.post_url_func_ptr != NULL)))
      copy_retrieve_data(cptr, chunk_start, bytes_read_in_chunk, cptr->total_bytes);
*/
inline int
copy_data_to_full_buffer(connection* cptr)
{
  action_request_Shr *url_num = get_top_url_num(cptr);

  NSDL2_HTTP(NULL, cptr, "Method called, no_validation = %hd, Url type = %d,"
			 " cptr->location_url = %s, post_url_func_ptr = %p,"
			 " redirection_depth_bitmask = %x, redirect_count = %d"
                         "url_num->proto.http.header_flags = %d",
			 runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation,
			 url_num->proto.http.type,
			 cptr->location_url,
			 url_num->post_url_func_ptr,
			 ((VUser *)(cptr->vptr))->cur_page->redirection_depth_bitmask,
			 cptr->redirect_count, url_num->proto.http.header_flags);

 /* copy the data to the full_buffer[is used by registration apis] in as follows cases:
    if no validation is 0 and any one of as follows is true 
       this is MAIN url and there is no redirect location OR 
       if this is main URL , redirect location  and depth bit is set
   Do not call icopy_retrieve_data() for embedded URLS
 */

if(((!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation) &&
   (cptr->req_code != 304) && // Added for caching
   ((url_num->proto.http.type == MAIN_URL && !cptr->location_url) ||  
    (url_num->post_url_func_ptr != NULL)  ||
    (url_num->proto.http.header_flags & NS_URL_KEEP_IN_CACHE) || 
    ((url_num->proto.http.type == MAIN_URL) && (cptr->location_url) &&
     (((VUser *)(cptr->vptr))->cur_page->redirection_depth_bitmask & (1 << (cptr->redirect_count)))
    )
   )
   &&
   (!(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM || cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST) || cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)) // Added for HTTP Auth NTLM in 3.8.2
   ||
   (cptr->flags & NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY)
   ||
   (cptr->flags & NS_CPTR_CONTENT_TYPE_MPEGURL)
   ||
   (cptr->flags & NS_CPTR_CONTENT_TYPE_MEDIA)
  )
    return 1;
  else
    return 0;
}

// Return 0 : Success and -1: Failure
int reset_idle_connection_timer(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;

  PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;
  PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

  GroupSettings *gset = &runprof_table_shr_mem[vptr->group_num].gset;

  int elaps_response_time = 0, next_idle_time = 0; 
  int max_timeout, idle_timeout;

  NSDL2_HTTP(vptr, cptr, 
             "[CPTR_TIMER] vptr = %p, cptr = %p, group_num = %d, "
             "cptr{request_type = %d, timer_type = %d}, idle_secs = %d", 
             vptr, cptr, vptr->group_num, cptr->request_type, cptr->timer_ptr->timer_type, gset->idle_secs);

  if(runprof_table_shr_mem[vptr->group_num].page_reload_table)
    pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number];
 
  if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table)
    pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];

   NSDL2_HTTP(vptr, cptr,
              "[CPTR_TIMER] vptr = %p, cptr = %p, pagereload_ptr = %p, pageclickaway_ptr = %p", 
              vptr, cptr, pagereload_ptr, pageclickaway_ptr);

  if(pagereload_ptr || pageclickaway_ptr) 
    return 0;

  if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
  {
    switch(cptr->request_type)
    {
      case HTTP_REQUEST:
      case HTTPS_REQUEST:
      {
        max_timeout = gset->response_timeout; 
        idle_timeout = gset->idle_secs;
        break;
      }
      case SOCKET_REQUEST: 
      case SSL_SOCKET_REQUEST: 
      {
        vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
        ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

        if(IS_TCP_CLIENT_API_EXIST)
        {
          fill_tcp_client_avg(vptr, RECV_FB_TIME, (now - cptr->ns_component_start_time_stamp));
          fill_tcp_client_avg(vptr, RECV_FB_TIME, (now - cptr->ns_component_start_time_stamp));
        }

        if(IS_UDP_CLIENT_API_EXIST)
        {
          fill_udp_client_avg(vptr, RECV_FB_TIME, (now - cptr->ns_component_start_time_stamp));
          fill_udp_client_avg(vptr, RECV_FB_TIME, (now - cptr->ns_component_start_time_stamp));
        }

        /* 1. Max timeout
              Priority-1            Recv API attribute
              Priority-2            Global API
              Priority-3            IDLE response timeout*/

        //Max should be greater than fb

        if(socket_req->timeout_msec != -1)
          max_timeout = socket_req->timeout_msec;
        else if(g_socket_vars.socket_settings.recv_to != -1)
          max_timeout = g_socket_vars.socket_settings.recv_to;
        else
          max_timeout = gset->response_timeout;

        /* 1. idle_timeout
                Priority-1            Global
                Priority-2            IDLE secs*/

        if (g_socket_vars.socket_settings.recv_ia_to != -1)
          idle_timeout = g_socket_vars.socket_settings.recv_ia_to;
        else
          idle_timeout = gset->idle_secs;

        cptr->timer_ptr->actual_timeout = idle_timeout;

        NSDL2_HTTP(vptr, cptr, "[CPTR_TIMER] vptr = %p, cptr = %p, cptr->bytes = %d, max_timeout = %f, idle_timeout = %d", 
                   vptr, cptr, cptr->bytes, max_timeout, idle_timeout, idle_timeout);

        break;
      }
      default:
        NSDL2_HTTP(vptr, cptr, "[CPTR_TIMER] vptr = %p, cptr = %p, in default case", vptr, cptr);
        return 0;
    }

    //When Response time is greater then response_timer than setting timeOut and closing connection
    elaps_response_time = now - cptr->request_sent_timestamp; 

    NSDL2_HTTP(vptr, cptr,
               "[CPTR_TIMER] vptr = %p, cptr = %p, now = %lu, cptr->request_sent_timestamp = %d, "
               "elaps_response_time = %d, max_timeout = %d", 
               vptr, cptr, now, cptr->request_sent_timestamp, elaps_response_time, max_timeout);
    
    if(elaps_response_time > max_timeout) 
    { 
      NSDL2_HTTP(vptr, cptr, "[CPTR_TIMER] read time exceeded from max excepted timeout %d", max_timeout);
      NSTL1(vptr, cptr, "Read Timeout, elaps_time = %d, max_timeout = %d", elaps_response_time, max_timeout);
      return -1;
    }

    //For Last sample;  if idle sec is 10 ms and response_timeout is 45 ms, 
    // then on 5th iteration we will make timer->actual_timeout as 5 ms

    next_idle_time = max_timeout - elaps_response_time;   
    NSDL2_HTTP(NULL, cptr, "[CPTR_TIMER] next_idle_time %d", next_idle_time);
    
    if((idle_timeout > 0) && (next_idle_time < idle_timeout))
      cptr->timer_ptr->actual_timeout = next_idle_time ;

    dis_idle_timer_reset(now, cptr->timer_ptr);
  }

  return 0;
}

int proc_http_hdr_content_length(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called, buf = %s, byte_left = %d", buf, byte_left);
  
  for(*bytes_consumed = 0; *bytes_consumed < byte_left; (*bytes_consumed)++) {
    char cur_byte = buf[*bytes_consumed];
    NSDL2_HTTP(NULL, cptr, "cur byte = %c", cur_byte);

    if(isdigit(cur_byte))
    {
      if(cptr->chunked_state != CHK_SIZE) {
        cptr->content_length = (cptr->content_length > 0 ? cptr->content_length:0) * 10 +
	                             cur_byte - '0';
        NSDL2_HTTP(NULL, cptr, "content_length processed so far = %d", cptr->content_length);
      } else {
        NSDL3_HTTP(NULL, cptr, "Content Length found along with Transfer-Encoding: chunked."
                                     "Ignoring Content Length");
      }
    }
    else if(cur_byte == '\r')
    {
        NSDL2_HTTP(NULL, cptr, "Final content length (%d) got, Setting header state to HDST_CR",
			        cptr->content_length);
        cptr->header_state = HDST_CR;
        return 0;
        break;

    }
    else
    {
      // TODO - Log warning event
      NSDL2_HTTP(NULL, cptr, "Setting header state to HDST_TEXT");
      cptr->header_state = HDST_TEXT;
    }
  } 
  return 0;
}

/*-----------------------------------------------------------------------------------------------------------------------
 * Function: proc_http_hdr_x_dynaTrace()
 * Purpose : This will process HTTP response header
 *  
 * Keyword : G_ENABLE_DT <grp_name> <mode>
 * where mode
 * 0: Disable dynaTrace Integration
 * 1: Enable dynaTrace Integration 
 * 3: Capture dynaTrace information from HTTP response header
 * 5: Enable session recording on start of test execution. Recording will stop after completion of the test.
 * 7: mode 3 & 5 (Both are applicable)
 -----------------------------------------------------------------------------------------------------------------------*/

int proc_http_hdr_x_dynaTrace(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called, buf = %s, byte_left = %d", buf, byte_left);

  int length;
  char *data_ptr = NULL, *end_ptr = NULL;
  length = byte_left;
  #define MAX_DYNATRACE_REP_HDR_SIZE 2048

  VUser *vptr = (VUser *)cptr->vptr;

  if((runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.mode != 3) && (runprof_table_shr_mem[vptr->group_num].gset.dynaTraceSettings.mode != 7))
  {
    /*Bug : 55140 (Doing -1 as we have to parse \r, 
     and this issue will come only when header position will be last 
     and value of header will be blank)*/

    (*bytes_consumed) = -1;
    cptr->header_state = HDST_TEXT; // Set state to TEXT so that is keep ignoring characters till next \r
    return 0;
  }

  data_ptr =  buf;

  end_ptr = memchr(data_ptr, '\r', length);

  if(!cptr->x_dynaTrace_data)
  {
    MY_MALLOC_AND_MEMSET(cptr->x_dynaTrace_data, sizeof(x_dynaTrace_data_t), "x_dynaTrace_data", 0);
  }
  x_dynaTrace_data_t *x_dynaTrace_data = cptr->x_dynaTrace_data;

  if(end_ptr) // Complete header value found
  {
    length = end_ptr - data_ptr;
    if (!(length + x_dynaTrace_data->use_len))
    {
      NSTL1(NULL, cptr, "Warning, received empty x-dynatrace response header = [x-dynatrace: %s], ChildId = %hd, UserId = %u, SessInstance = %u, PageInstance = %d", cptr->x_dynaTrace_data->buffer, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance);
    }
    (*bytes_consumed) += length;  // set consumed to length (Neeraj Jain)
    cptr->header_state = HDST_CR; // Set state to CR 
  }
  else
  {
    (*bytes_consumed) += length;
  }

  // We need to put max size of dynatrace header as this goes in URL record and whole URL record cannot be more than max log shr buf size which is 16K default
  // Bug Id - 55143 
  if(x_dynaTrace_data->use_len >= MAX_DYNATRACE_REP_HDR_SIZE)
  {
    NSTL1(NULL, cptr, "Received x-dynatrace response header more than %d bytes. Header saved so far = [x-dynatrace: %s], Ingnored = %*.*s", MAX_DYNATRACE_REP_HDR_SIZE, cptr->x_dynaTrace_data->buffer, length, length, data_ptr);
    return 0;
  }

  int offset = x_dynaTrace_data->use_len; // Offset in buffer to store the value
  int prev_buf_len = x_dynaTrace_data->len; // Length of buffer

  x_dynaTrace_data->use_len += length; // use len is used buffer for storing value

  if(x_dynaTrace_data->use_len > MAX_DYNATRACE_REP_HDR_SIZE)
  {
    // calculate how much to copy so that total length does not exceed the max value
    int ignored_length = (x_dynaTrace_data->use_len - MAX_DYNATRACE_REP_HDR_SIZE);
    length = length - ignored_length;
    x_dynaTrace_data->use_len = MAX_DYNATRACE_REP_HDR_SIZE;
    NSTL1(NULL, cptr, "Received x-dynatrace response header more than %d bytes. Header saved so far = [x-dynatrace: %s], Ingnored = %*.*s", MAX_DYNATRACE_REP_HDR_SIZE, cptr->x_dynaTrace_data->buffer, ignored_length, ignored_length, data_ptr + length);
  }

  if(x_dynaTrace_data->use_len >= prev_buf_len) // More data than buffer size
  {
    x_dynaTrace_data->len = x_dynaTrace_data->use_len + 1; // Increase buffer size
    MY_REALLOC_EX(x_dynaTrace_data->buffer, x_dynaTrace_data->len, prev_buf_len, "x_dynaTrace_data->buffer", -1);
  }

  bcopy(data_ptr, x_dynaTrace_data->buffer + offset, length);
  x_dynaTrace_data->buffer[x_dynaTrace_data->use_len] = '\0';

  if(end_ptr) // Complete header value found
  {
    NSDL2_HTTP(NULL, cptr, "Complete x-dynatrace header value (%s) got, use_len = %d, total buffer len = %d. Setting header state to HDST_CR",
                            cptr->x_dynaTrace_data->buffer, x_dynaTrace_data->use_len, x_dynaTrace_data->len);
    //Only for tracing purpose
    NSTL1(NULL, cptr, "Received Complete x-dynatrace response header = [x-dynatrace: %s], ChildId = %hd, UserId = %u, SessInstance = %u, PageInstance = %d", cptr->x_dynaTrace_data->buffer, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance);
  }
  else
  {
    NSDL2_HTTP(NULL, cptr, "Partial x-dynatrace header value (%s) got, use_len = %d, total buffer len = %d.",
                            cptr->x_dynaTrace_data->buffer, x_dynaTrace_data->use_len, x_dynaTrace_data->len);

    NSTL1(NULL, cptr, "Received Partial x-dynatrace response header = [x-dynatrace: %s], ChildId = %hd, UserId = %u, SessInstance = %u, PageInstance = %d", cptr->x_dynaTrace_data->buffer, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance);
  }

  return 0;
}

int proc_http_hdr_content_type_amf(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_FLAGS_AMF;

  return 0;
}

int proc_http_hdr_content_type_hessian(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_FLAGS_HESSIAN;

  return 0;
}

int proc_http_hdr_content_type_protobuf(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_CONTENT_TYPE_PROTOBUF;

  return 0;
}

int proc_http_hdr_content_type_octet_stream(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL1_HTTP(NULL, cptr, "Method Called");
  if(runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED)
  {
    cptr->flags |= NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY;
  }
  if(global_settings->use_java_obj_mgr)
    cptr->flags |= NS_CPTR_FLAGS_JAVA_OBJ;
  return 0;
}

// TO SUPPORT GRPC
int proc_http_hdr_content_type_grpc_proto(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_PROTO;

  return 0;
}

int proc_http_hdr_content_type_grpc_json(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_JSON;

  return 0;
}
/* not supporting right now.
int proc_http_hdr_content_type_grpc_custom(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->flags |= NS_CPTR_CONTENT_TYPE_GRPC_CUSTOM;

  return 0;
}
*/
int proc_http_hdr_grpc_encoding_gzip(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");
  cptr->compression_type = NSLB_COMP_GZIP;
  return 0;
}

int proc_http_hdr_grpc_encoding_deflate(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");
  cptr->compression_type = NSLB_COMP_DEFLATE;
  return 0;
}

int proc_hls_hdr_content_type_mpegurl(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL1_HTTP(NULL, cptr, "proc_hls_hdr_content_type_mpegurl Method Called cptr->flag = 0x%x, enable_m3u8 = %d",   
                         cptr->flags, runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.m3u8_gsettings.enable_m3u8);

  if(runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.m3u8_gsettings.enable_m3u8)
  {
    cptr->flags |= NS_CPTR_CONTENT_TYPE_MPEGURL;
  }

  NSDL1_HTTP(NULL, cptr, "proc_hls_hdr_content_type_mpegurl Method Called cptr->flag = 0x%x",   cptr->flags);

  return 0;
}

int proc_hls_hdr_content_type_media(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL1_HTTP(NULL, cptr, "proc_hls_hdr_content_type_media Method Called cptr->flag = 0x%x",   cptr->flags);

  if(runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.m3u8_gsettings.enable_m3u8)
  {
    cptr->flags |= NS_CPTR_CONTENT_TYPE_MEDIA;
  }

  NSDL1_HTTP(NULL, cptr, "proc_hls_hdr_content_type_media Method Called cptr->flag = 0x%x",   cptr->flags);

  return 0;
}

int proc_http_hdr_content_encoding_deflate(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->compression_type = NSLB_COMP_DEFLATE;

  // No need to set as calling method is calling with 0
  // *bytes_consumed = 0; // Set to 0 as we are not consuming any bytes

  return 0;
}

int proc_http_hdr_content_encoding_gzip(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->compression_type = NSLB_COMP_GZIP;

  return 0;
}

int proc_http_hdr_content_encoding_br(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  cptr->compression_type = NSLB_COMP_BROTLI;

  return 0;
}

int proc_http_hdr_location (connection *cptr, char *buf, int bytes_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method called");
  char* value_start = buf; 
  char* value_end;
  int new_value_length = bytes_left;
  int existing_value_length, total_value_length;

  //action_request_Shr *url_num;

  //url_num = get_top_url_num(cptr);

  value_end = memchr(value_start, '\r', new_value_length);
 
  if (cptr->location_url)
    existing_value_length = strlen (cptr->location_url);
  else
    existing_value_length = 0;

  if (value_end) {  /* we got the whole value */
    new_value_length = value_end - value_start;
    if (!(new_value_length+existing_value_length)) {
      fprintf(stderr, "Warning, getting a NULL value for Redirect LOCATION\n");
    }
    // doing -1 as we are not taking \r
    (*bytes_consumed) += (new_value_length-1);
    cptr->header_state = HDST_TEXT;
  }
  else{
    *bytes_consumed += new_value_length;
  }

  total_value_length = existing_value_length + new_value_length;
  MY_REALLOC_EX(cptr->location_url, total_value_length + 1, existing_value_length + 1, "cptr->location_url", -1);
  bcopy(value_start, cptr->location_url + existing_value_length, new_value_length);
  cptr->location_url[total_value_length] = '\0';

  if(value_end) 
  {
   
    // Added to trunctae space after location url 
    CLEAR_WHITE_SPACE_FROM_END_LEN(cptr->location_url, total_value_length);

    if (cptr->req_code == 301 || cptr->req_code == 302 || cptr->req_code == 303 || cptr->req_code == 307 || cptr->req_code == 308
       /*Location url need to save on any response code.*/
       /*Done for VISA, in VISA location was coming with 200 resp code.*/
       || runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.save_loc_hdr_on_all_rsp_code) {
      NSDL3_HTTP(NULL, cptr, "Got LOCATION header (Location = %s). Request URL = %s", 
                            cptr->location_url, get_url_req_url(cptr));
    } 
    else if(cptr->url_num->proto.http.header_flags & NS_XMPP_UPLOAD_FILE){
      FREE_LOCATION_URL(cptr, 0); // Force Free
    }
    else {
      // TODO - WARING EVent Log
      fprintf(stderr, "Warning: Got LOCATION header (Location = %s)"
                      " and response code is not 301, 302, 303, 307 or 308. Request URL = %s\n",
                cptr->location_url, get_url_req_url(cptr));
      FREE_LOCATION_URL(cptr, 0); // Force Free
    }
    NSDL3_HTTP(NULL, cptr, "Got Final location url = %s, bytes_consumed = %d",
			   cptr->location_url, *bytes_consumed);
  } else {
    NSDL3_HTTP(NULL, cptr, "Got location url so far = %s, bytes_consumed = %d", 
                           cptr->location_url, *bytes_consumed);
  }

  return 0;
}

int proc_http_hdr_link (connection *cptr, char *buf, int bytes_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method called");
  char* value_start = buf; 
  char* value_end;
  int new_value_length = bytes_left;
  int existing_value_length, total_value_length;

  value_end = memchr(value_start, '\r', new_value_length);
 
  if (cptr->link_hdr_val)
    existing_value_length = strlen (cptr->link_hdr_val);
  else
    existing_value_length = 0;

  if (value_end) {  /* we got the whole value */
    new_value_length = value_end - value_start;
    if (!(new_value_length+existing_value_length)) {
      fprintf(stderr, "Warning, getting a NULL value for Link header\n");
    }
    // doing -1 as we are not taking \r
    (*bytes_consumed) += (new_value_length-1);
    cptr->header_state = HDST_TEXT;
  }
  else{
    *bytes_consumed += new_value_length;
  }

  total_value_length = existing_value_length + new_value_length;
  MY_REALLOC_EX(cptr->link_hdr_val, total_value_length + 1, existing_value_length + 1, "cptr->link_hdr_val", -1);
  bcopy(value_start, cptr->link_hdr_val + existing_value_length, new_value_length);
  cptr->link_hdr_val[total_value_length] = '\0';

  if(value_end) 
  {
   
    // Added to trunctae space after location url 
    CLEAR_WHITE_SPACE_FROM_END_LEN(cptr->link_hdr_val, total_value_length);

  } else {
    NSDL3_HTTP(NULL, cptr, "Got link so far = %s, bytes_consumed = %d", 
                           cptr->link_hdr_val, *bytes_consumed);
  }

  return 0;
}

int proc_http_hdr_set_cookie(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called");
  // Commenting below switch case as in Kohls, NS had partial read of 2bytes "\r\n" and thus NS was ignoring set_cookie partial read data beforehand. Hence the cookie was not parsed and saved as it is not expected to have "Set-Cookie: \n" or "Set-Cookie: \r" or "Set-Cookie: \r\n"

  /*  switch ( buf[*bytes_consumed] ) {
    case '\n': // This is to handle SetCookie: followed by \n
      cptr->header_state = HDST_LF;
      *bytes_consumed = 1; // Set to 1 as we consume \n
      break;
    case '\r':
      cptr->header_state = HDST_CR; // This is to handle SetCookie: followed by \r
      *bytes_consumed = 1; // Set to 1 as we consume \r
      break;
    default: */
   // TODO: In case if it is required to process "Set-Cookie: \n" or "Set-Cookie: \r" or "Set-Cookie: \r\n", handle it in save_auto_cookie with consideration to partial read beforehand
     if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
       //*bytes_consumed = save_cookie_name(buf, buf + byte_left - 1, byte_left, cptr);
       *bytes_consumed = save_manual_cookie(buf, buf + byte_left - 1, byte_left, cptr);
     else
       *bytes_consumed = save_auto_cookie(buf, byte_left, cptr, 0);
    //}

  return 0;
}

int proc_http_hdr_transfer_encoding_chunked(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL3_HTTP(NULL, cptr, "Method called. [cptr=0x%lx]: Connection is in chunked format",(u_ns_ptr_t) cptr);
  //In JCPenny a PUT request was send to the server which in response gave 204 status code with Transfer-Encoding: chunked and 
  //Content-Length: 0
  if (cptr->req_code != 204)
  {
    cptr->chunked_state = CHK_SIZE;
    cptr->content_length = 0;
  } 
  else 
    NSDL1_HTTP(NULL, cptr, "Got Transfer chunked encoding with status code %d, ignoring chunked header.", cptr->req_code);
  return 0;
}

// Connection header is used to check if keep alive is on and we got close in connection header, then close the connection
int proc_http_hdr_connection_close(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now)
{
  NSDL1_HTTP(NULL, cptr, "Method Called. Setting connection mode to Not-Keep-Alive, buf = %s,"
                                          "bytes_consumed = %d, byte_left = %d", 
                                           buf, *bytes_consumed, byte_left);
  cptr->connection_type = NKA;
  /*
  char conn_hdr[1024] = "";
  char cur_byte;
  for(*bytes_consumed = 0; *bytes_consumed < byte_left; (*bytes_consumed)++) 
  {
    cur_byte = buf[*bytes_consumed];
    NSDL2_HTTP(vptr, cptr, "cur byte = %c", cur_byte);
    
    if(cur_byte == '\r') {
      cptr->header_state = HDST_CR;
      break;
    }
    else {
      NSDL2_HTTP(NULL, cptr, "Setting proc_http_hdr_connection_close(): header state to HDST_TEXT");
      cptr->header_state = HDST_TEXT;
      conn_hdr[*bytes_consumed] = cur_byte;
    }
  }

  NSDL2_HTTP(NULL, cptr, "Value of conn_hdr = [%s]", conn_hdr);
  if(cptr->url_num->request_type == WS_REQUEST)
  {
    proc_ws_hdr_connection;
  }
  else {
    if(!strcmp(conn_hdr, "close"))
      cptr->connection_type = NKA;
  }
  */
  return 0;
}

int change_state_to_hdst_bol(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  NSDL2_HTTP(NULL, cptr, "Method Called, byte_left = %d", byte_left);
  cptr->header_state  = HDST_BOL;
  *bytes_consumed = -1;  // So that last charcter handle with HDST_BOL
  return 0;
}

int body_starts(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {
  char *payload_ptr = NULL;
  int payload_len = 0;
  //VUser *vptr = cptr->vptr;

  NSDL2_HTTP(NULL, cptr, "Method Called, cptr = %p, byte_left = %d, bytes_consumed = %d", cptr, byte_left, *bytes_consumed);

  //CACHING -- Called for checking the cacheability of url based on method 

  //Method will set the bit in cache_flag in CacheTable Entry        #Shilpa 15Dec10
  cache_ability_on_caching_headers(cptr);


  if (cptr->chunked_state == CHK_SIZE)
      cptr->conn_state = CNST_CHUNKED_READING;
  else {
    cptr->conn_state = CNST_READING;
  }

  NSDL2_HTTP(NULL, cptr, "cptr->url_num->request_type = %d, cptr->flags = %0x, global_settings->protocol_enabled = 0x%0x, cptr->request_type = %d",
                          cptr->url_num->request_type, cptr->flags, global_settings->protocol_enabled, cptr->request_type);
  //here to set flag because if we get http response then in this we move to close connection
  //if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)
  if((cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) && (cptr->flags & NS_WEBSOCKET_UPGRADE_DONE))
  {
    NSDL2_WS(NULL, cptr, "Read Websocket data:");
    //VUser *vptr = cptr->vptr;
    cptr->flags |= NS_CPTR_DO_NOT_CLOSE_WS_CONN;
    cptr->ws_reading_state = NEW_FRAME;
    cptr->conn_state = CNST_WS_READING;

    //#if 0
    /* If Websocket Response has body with handshake then read body */
    unsigned char *frame_st = (unsigned char *)buf;
    int frame_len = byte_left;
    NSDL4_WS(NULL, NULL, "Body buf = [%s]", buf);
    if(*frame_st == '\n') //Skip \n before frame
    {
      NSDL2_WS(NULL, NULL, "Skip new line before reading frame");
      frame_st++;
      frame_len--;
      byte_left --;
    }
    NSDL2_WS(NULL, cptr, "bytes_consumed = %d, frame_len = %d", *bytes_consumed, frame_len);
    if(frame_len)
    {
      payload_len = handle_frame_read(cptr, frame_st, frame_len, &payload_ptr);

      *bytes_consumed = (payload_ptr - buf) + payload_len;
      NSDL2_WS(NULL, NULL, "payload_len = %d, bytes_consumed = %d", payload_len, *bytes_consumed);

      if(payload_len == 0) {
        cptr->ws_reading_state = NEW_FRAME;
        NSDL2_WS(NULL, NULL, "cptr->ws_reading_state = %d", cptr->ws_reading_state);
      }
      else {
         NSDL2_WS(NULL, NULL, "Read complete frame Done!, payload = [%s]", payload_ptr);
        //dump_ws_response(cptr, vptr, payload_ptr, payload_len);
      }
    }
    else
      *bytes_consumed = byte_left;
    //#endif
    NSDL2_WS(NULL, cptr, "Setting cptr->content_length = 0 , because in WebSocket request it may be possible frame comes with Handshake");
    cptr->content_length = 0;

  }
  NSDL2_HTTP(NULL, cptr, "Connection state is %d, frame_reading_state = %d", cptr->conn_state, cptr->ws_reading_state);
  cptr->body_offset = 0;
  //switch_to_vuser_ctx(vptr, "SwitcToVUser"); 
  return 0;
}

int is_chunked_done_if_any(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now) {

  int status;
  NSDL2_HTTP(NULL, cptr, "Method Called");

  switch (cptr->chunked_state) {
    case CHK_NO_CHUNKS:
      body_starts(cptr, buf, byte_left, bytes_consumed, now);
      break;
    case CHK_SIZE:
      body_starts(cptr, buf, byte_left, bytes_consumed, now);
      break;
    case CHK_FINISHED:
      cptr->chunked_state = CHK_NO_CHUNKS;
      if (!cptr->req_code_filled)
        status = get_req_status (cptr);
      else
 	status = NS_REQUEST_BAD_HDR;
      if (status == NS_REQUEST_OK) {
        cptr->bytes = cptr->total_bytes;
        cptr->total_bytes = 0;
        NSDL3_HTTP(NULL, cptr, "[cptr=%p]: Finished recieving %d bytes on chunked page.",
                               cptr, cptr->bytes);
        Close_connection( cptr , 1, now, status, NS_COMPLETION_CHUNKED);
      } else {
        NSDL3_HTTP(NULL, cptr, "BADBYTE:7");
        cptr->bytes = cptr->total_bytes;
        cptr->total_bytes = 0;
        NSDL3_HTTP(NULL, cptr, "cptr->bytes = %d", cptr->total_bytes);
        Close_connection( cptr , 0, now, status, NS_COMPLETION_BAD_BYTES);
      }
      return 1;
  }

  return 0;
}

/* This method clear fields in cptr which need to be reset when new reply comes
 */
static void http_init_cptr_for_new_rep(connection *cptr) {

  NSDL2_HTTP(NULL, cptr, "Method Called");

  if (global_settings->cookies == COOKIES_ENABLED) {
    NSDL2_HTTP(NULL, cptr, "Cookie is enabled.");
    if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE) {
      NSDL2_HTTP(NULL, cptr, "Setting cookie_hash_code & cookie_idx to -1.");
      cptr->cookie_hash_code = cptr->cookie_idx = -1;
    } else {
      NSDL2_HTTP(NULL, cptr, "Freeing cookie_hash_code if there for auto cookie &"
			     " making cookie_hash_code & cookie_idx to zero.");
      // as for enabled auto cookie we set it to null because
      // it used as pointers
      free_auto_cookie_line(cptr);
    }
  } else {
    NSDL2_HTTP(NULL, cptr, "Cookie is disabled.");
  }
}

void handle_fast_read(connection*cptr, action_request_Shr* url_num, u_ns_ts_t now ) {
  VUser *vptr = cptr->vptr;
#ifdef ENABLE_SSL
  /* must be larger than THROTTLE / 2 +1 to append '\0' for debugging/logging */
  char buf[65536 + 1];
#else
  /* must be larger than THROTTLE / 2 +1 to append '\0' for debugging/logging */
  char buf[4096 + 1];
#endif
  int bytes_read;

  while(1) {
    if ((bytes_read = read( cptr->conn_fd, buf, 4096 ))  > 0) {
      cptr->tcp_bytes_recv += bytes_read;

      if((cptr->request_type == WS_REQUEST) || (cptr->request_type == WSS_REQUEST)) {
        INC_WS_RX_BYTES_COUNTER(vptr, bytes_read);
      } else {
        average_time->rx_bytes += bytes_read;
        if(SHOW_GRP_DATA) {
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->rx_bytes += bytes_read;
        }
      }
    } else {
      break;
    }

    if(global_settings->high_perf_mode) // Anil - Why we need to break for this case?
      break;
  }

  if ((cptr->tcp_bytes_recv) < (url_num->proto.http.bytes_to_recv)) {
    if (cptr->tcp_bytes_recv == 0) {
	retry_connection (cptr, now, NS_REQUEST_CONFAIL);
    } else {
        Close_connection(cptr, 0, now, NS_REQUEST_INCOMPLETE_EXACT, NS_COMPLETION_EXACT); //BAD Bytes
    }
  } else {
    Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_EXACT);
  }
}

void
handle_read( connection *cptr, u_ns_ts_t now ) {
#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_read, bytes_handled = 0;
  register int checksum;
  action_request_Shr* url_num;
  int request_type;
  char err_msg[65545 + 1];
  int err, extra_bytes = 0;
  int cur_header_state;
  int bytes_consumed;
  //char *err_buff = NULL;
  VUser *vptr = cptr->vptr;
  int sess_id = -1;
  char save_url_resp_hdr = 0;
  unsigned long l;
  int r = 0;
  const char *file, *data;
  int line, flags;

  long resp_sz = 0;
  int ws_new_conn_state = -1; 


  NSDL2_HTTP(NULL, cptr, "Method called. cptr=%p, cptr->num_pipe = %d, cptr->req_code_filled = %d, "
                         "cptr->req_code = %d, cptr->conn_state = %d, cptr->header_state = %d, cptr->content_length = %d",
                          cptr, cptr->num_pipe, cptr->req_code_filled, cptr->req_code, 
                          cptr->conn_state, cptr->header_state, cptr->content_length);

  if (reset_idle_connection_timer(cptr, now) < 0)  {
    Close_connection( cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
    return;  
  }
             
  //url_num = get_top_url_num(cptr);
  // for DNS, we use cptr->data for DNS specific data
  if (!(cptr->data) || (cptr->url_num->request_type == DNS_REQUEST) ) {
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr");
    url_num =  cptr->url_num;
  } else { 
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr->data");
    url_num =  ((pipeline_page_list *)(cptr->data))->url_num;
  }
  
  //Fast read
  //Deepika: How it will work for WS ?? proto is uninon
  if (url_num->proto.http.exact) {
    handle_fast_read(cptr, url_num, now);
    return;
  } 

  while (1) {
  try_read_more:
 // printf ("0 Req code=%d\n", cptr->req_code);
  if ( do_throttle )
    bytes_read = THROTTLE / 2;
  else
    bytes_read = sizeof(buf) - 1;

#ifdef ENABLE_SSL
    /* Thing to watch out for: if the amount of data received is
     * larger than the sizeof(buf) but less than the ssl lib's internal
     * buffer size, some residual data may stay in that internal
     * buffer undetected by select() calls. That is, the internal
     * buffer should always be fully drained.
     */
/*   if (url_num->proto.http.is_redirect_url) */
/*     request_type = url_num->request_type; */
/*   else */
    request_type = url_num->request_type;

  /* make ssl connection in case request type is HTTPS & proxy connect bit is unset 
   * and proxy authentication handshake is complete in case enabled at the server */
  if ((request_type == HTTPS_REQUEST || request_type == WSS_REQUEST)&& 
          !(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT || 
              cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)) {
    ERR_clear_error();

    bytes_read = SSL_read(cptr->ssl, buf, bytes_read);

    //if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
      //NSDL3_SSL(NULL, cptr, "SSL Read %d bytes. msg=%s", bytes_read, (bytes_read>0)?buf:"-");

    if (bytes_read <= 0) {
      err = SSL_get_error(cptr->ssl, bytes_read);
      switch (err) {
      case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
	handle_server_close (cptr, now);
	return;
      case SSL_ERROR_WANT_READ:
	return;
	/* It can but isn't supposed to happen */
      case SSL_ERROR_WANT_WRITE:
	fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
	handle_bad_read (cptr, now);
	return;
      case SSL_ERROR_SYSCALL: //Some I/O error occurred
        /*Archana - Add this in 3.5.2 
          Calling SSL_read()/SSL_write() as needed. error code from both calls are checked 
          (SSL_ERROR_WANT_READ, SSL_ERROR_WANT_WRITE, SSL_ERROR_NONE, 
          SSL_ERROR_SYSCALL (when errno == EINTR or EAGAIN, it is consider as 
          again not error), all other cases considered as error and drop the 
          connection.*/
        if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
        {
          if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
              url_num->proto.http.type != MAIN_URL &&
              (cptr->num_pipe != -1) &&
              cptr->num_pipe < runprof_table_shr_mem[vptr->group_num].gset.max_pipeline) {
            pipeline_connection((VUser *)cptr->vptr, cptr, now);
          }

          NSDL1_SSL(NULL, cptr, "SSL_read: No more data available, return"); 
          return;
        }

        if (errno == EINTR)
        {
          NSDL3_SSL(NULL, cptr, "SSL_read interrupted. Continuing...");
          continue;
        }
	/* FALLTHRU */
      case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
	/* FALLTHRU */
      default:
    	/*
     	* We don't know what kind of thing CRYPTO_THREAD_ID is. Here is our best
     	* attempt to convert it into something we can print.
     	*/
        l = ERR_get_error_line_data(&file, &line, &data, &flags);
        r = ERR_GET_REASON(l);
        NSTL1(NULL, NULL, "SSl library error, %lu, %d", l, r);
 
        if (!(strcmp(SSL_get_version(cptr->ssl), "TLSv1.3")) && (r == 1116))
        {
          NSTL1(NULL, NULL, "SSl library error 116");
          retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); 
          return;
        }
        else
        {
	  if ((bytes_read == 0) && (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.ssl_clean_close_only))
	      handle_server_close (cptr, now);
	  else
	      handle_bad_read (cptr, now);
	  return;
        }
      }
    }
  } else {
    bytes_read = read( cptr->conn_fd, buf, bytes_read );
#ifdef NS_DEBUG_ON
    //if (bytes_read != 10306) printf("rcd only %d bytes\n", bytes_read);
        if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
        NSDL3_HTTP(NULL, cptr, "Non SSL Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {

        if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
            url_num->proto.http.type != MAIN_URL &&
            (cptr->num_pipe != -1) &&
            cptr->num_pipe < runprof_table_shr_mem[vptr->group_num].gset.max_pipeline) {
          pipeline_connection((VUser *)cptr->vptr, cptr, now);
        }

#ifndef USE_EPOLL
        NSDL3_HTTP(NULL, cptr, "FD_SET for cptr->conn_fd = %d, wait fot reading more data", cptr->conn_fd);
	FD_SET( cptr->conn_fd, &g_rfdset );
#endif
	return;
      } else {
/*         if (cptr->redirect_url_num) */
/*           NSDL3_HTTP(vptr, cptr, "read failed (%s) for redirected: host = %s [%d], req = %s", nslb_strerror(errno), */
/*                       url_num->proto.http.index.svr_ptr->server_hostname,  */
/*                       url_num->proto.http.index.svr_ptr->server_port, */
/*                       cptr->redirect_url_num->request_line); */
/*         else */
        //char request_buf[MAX_LINE_LENGTH];
        //request_buf[0] = '\0';
          NSDL3_HTTP(NULL, cptr, "read failed (%s) for main: host = %s [%d], req = %s", nslb_strerror(errno),
                      url_num->index.svr_ptr->server_hostname,
                      url_num->index.svr_ptr->server_port,
                     get_url_req_url(cptr));
	handle_bad_read (cptr, now);
	return;
      }
    } else if (bytes_read == 0) {
      handle_server_close (cptr, now);
      return;
    }
  }
#else
  bytes_read = read( cptr->conn_fd, buf, bytes_read );

  if ( bytes_read < 0 ) {
    if (errno == EAGAIN) {
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_rfdset );
#endif
      return;
    } else {
      handle_bad_read (cptr, now);
      return;
    }
  } else if (bytes_read == 0) {
    handle_server_close (cptr, now);
    return;
  }
#endif

#ifdef NS_DEBUG_ON
  NSDL1_HTTP(NULL, NULL, "*******request type = %d", cptr->url_num->request_type);
  if ((cptr->url_num->request_type != WSS_REQUEST) && (cptr->url_num->request_type != WS_REQUEST))
    debug_log_http_res(cptr, buf, bytes_read);
  else
  { 
    if (((cptr->url_num->request_type == WSS_REQUEST) || (cptr->url_num->request_type == WS_REQUEST)) 
             && cptr->conn_state != CNST_WS_FRAME_READING )
    {
      debug_log_http_res(cptr, buf, bytes_read);
    }
  }
#else
  if ((cptr->url_num->request_type != WSS_REQUEST) && (cptr->url_num->request_type != WS_REQUEST))
  {
    LOG_HTTP_RES(cptr, buf, bytes_read);
  }
  else
  {
    if (((cptr->url_num->request_type == WSS_REQUEST) || (cptr->url_num->request_type == WS_REQUEST))
             && cptr->conn_state != CNST_WS_FRAME_READING )
    {
      LOG_HTTP_RES(cptr, buf, bytes_read);
    }
  }
#endif

  /*For page dump, if inline is enabled then only dump the inline urls in page dump 
   *Otherwise dump only main url*/
  if(NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL && 
    ((cptr->url_num->proto.http.type == EMBEDDED_URL && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) || (cptr->url_num->proto.http.type != EMBEDDED_URL)))
    ut_update_rep_file_for_page_dump(cptr->vptr, bytes_read, buf);

  cptr->tcp_bytes_recv += bytes_read;
  average_time->rx_bytes += bytes_read;

  NSDL1_HTTP(NULL, NULL, "bytes_read = %d", bytes_read);
  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->rx_bytes += bytes_read;
  }

  pipelining:

  bytes_handled = 0;

  if(cptr->conn_state == CNST_WS_FRAME_READING)
  {
    NSDL2_WS(vptr, cptr, "Go to process frame CNST_WS_FRAME_READING");
    goto process_ws_frame;
  }
  else if(cptr->conn_state == CNST_WS_IDLE)
  {
    NSDL2_WS(vptr, cptr, "Read and ignore data as we are not reading data by websocket read API, it may be heart beat");
    char line_break[] = "\n--------------------------------------------------------\n";
    debug_log_http_res(cptr, line_break, strlen(line_break));
    return;
  }
  else if(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)
  {
     if(strstr(cptr->url, "xhr_streaming"))
     {
        NSDL2_HTTP(vptr, cptr, "xhr_streaming url response received");
        cptr->conn_state = CNST_WS_IDLE;
        vptr->sockjs_cptr[sockjs_idx_list[vptr->first_page_url->proto.http.sockjs.conn_id]] = cptr;

        char line_break[] = "\n--------------------------------------------------------\n";
        debug_log_http_res(cptr, line_break, strlen(line_break));

        //ns_switch_to_vuser(vptr, opcode, msg);
        if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
           switch_to_vuser_ctx(vptr, "handle_read(): sockjs connect response completely read");  
        /* To handle the case of SOCK_JS and send the response to NJVM */
        else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
           send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_URL_REP, 0);
        /* sockjs_connect internally gets mapped to ns_web_url so while sending response used opcode of
           WEB_URL */
        else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
           send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);

       return;
     }
  }

  // First process the response line 
  // HTTP/1.1 200 OK\r\n

  if (cptr->req_code_filled != 0) { /* either nothing read or partial response code read */
    if (cptr->req_code_filled < 0) {/* these are the first bytes being read */
      http_init_cptr_for_new_rep(cptr);
      now = get_ms_stamp();
      cptr->first_byte_rcv_time = now - cptr->ns_component_start_time_stamp; //Calculate first byte receive time diff
      cptr->ns_component_start_time_stamp = now;//Update ns_component_start_time_stamp
      SET_MIN (average_time->url_frst_byte_rcv_min_time, cptr->first_byte_rcv_time);
      SET_MAX (average_time->url_frst_byte_rcv_max_time, cptr->first_byte_rcv_time);
      average_time->url_frst_byte_rcv_tot_time += cptr->first_byte_rcv_time;
      average_time->url_frst_byte_rcv_count++;
      UPDATE_GROUP_BASED_NETWORK_TIME(first_byte_rcv_time, url_frst_byte_rcv_tot_time, url_frst_byte_rcv_min_time, 
                                        url_frst_byte_rcv_max_time, url_frst_byte_rcv_count);

      NSDL2_HTTP(NULL, cptr, "Calculate first byte receive time diff = %d, update ns_component_start_time_stamp = %u", cptr->first_byte_rcv_time, cptr->ns_component_start_time_stamp);
      if (bytes_read >= 9) // at least part of code is read
	cptr->req_code_filled = 9;
      else {
	cptr->req_code_filled = bytes_read;

        /* Getting session ID to retrieve URL RESP HDR */ 
        sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
        save_url_resp_hdr = session_table_shr_mem[sess_id].save_url_body_head_resp & SAVE_URL_HEADER;

        if(((vptr->cur_page->save_headers & SEARCH_IN_HEADER)|| save_url_resp_hdr) && (cptr->url_num->proto.http.type == MAIN_URL)){
          NSDL2_HTTP(NULL, cptr, "Read bytes is less the 9. Going to save headers");
          save_header(vptr, buf, bytes_read);
        }
	continue; // Read more till we get at least part of code
      }
      bytes_handled += 9; // Bytes before status code

      //printf ("1 bytes_handled =%d code_filled = %d buf=%s\n", bytes_handled, cptr->req_code_filled, buf);
    } else {  /* the response code is partially read */
      if (cptr->req_code_filled < 9) {  /* "HTTP/1.1" not fully read yet */
	if (bytes_read >= (9 - cptr->req_code_filled)) {
	  bytes_handled += (9 - cptr->req_code_filled);
	  cptr->req_code_filled = 9;
	  //printf ("2 bytes_handled =%d code_filled = %d buf=%s\n", bytes_handled, cptr->req_code_filled, buf);
	} else {
	  cptr->req_code_filled += bytes_read;
         
         /* Geeting session ID to retrieve URL RESP HDR */ 
       	  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
          save_url_resp_hdr = session_table_shr_mem[sess_id].save_url_body_head_resp & SAVE_URL_HEADER;

          if(((vptr->cur_page->save_headers & SEARCH_IN_HEADER)|| save_url_resp_hdr) && (cptr->url_num->proto.http.type == MAIN_URL)){
            NSDL2_HTTP(NULL, cptr, "Response code is partailly read. Going to save headers");
            save_header(vptr, buf, bytes_read);
          }
	  continue;
	}
      }
    }

    #ifdef NS_DEBUG_ON
    NSDL4_HTTP(NULL, NULL, "bytes_handled = %d, bytes_read = %d, req_code_filled = %d, buf = [%s]", 
                            bytes_handled, bytes_read, cptr->req_code_filled, buf);
    #endif
    //printf ("3 bytes_handled =%d code_filled = %d buf=%s\n", bytes_handled, cptr->req_code_filled, buf);
    for (;bytes_handled < bytes_read; bytes_handled++) {
      NSDL4_HTTP(NULL, NULL, "req_code_filled = %d", cptr->req_code_filled);
      switch (cptr->req_code_filled) {
      case 9:
	cptr->req_code = (buf[bytes_handled] - 48) * 100;
	cptr->req_code_filled++;
	//printf ("1 Req code=%d\n", cptr->req_code);
	break;
      case 10:
	cptr->req_code += (buf[bytes_handled] - 48) * 10;
	cptr->req_code_filled++;
	//printf ("2 Req code=%d\n", cptr->req_code);
	break;
      case 11:
	cptr->req_code += (buf[bytes_handled] - 48);
	cptr->req_code_filled = 0;
        /*As soo  we read REQ code we need to check cachability and validate the code for cache*/
        //CACHING -- Called for checking the cacheability of url based on user caching enabled & response code
        //Method will set the bit in cache_flag in CacheTable Entry        #Shilpa 15Dec10
        
        if(((VUser *)(cptr->vptr))->flags & NS_CACHING_ON)
          cache_ability_on_resp_code(cptr);

        // For caching
        validate_req_code(cptr);

	//printf ("3 Req code=%d\n", cptr->req_code);
	break;
      default:
	fprintf(stderr, "Got an invalid req_code_filled num: %d\n", cptr->req_code_filled);
      }
      if (cptr->req_code_filled == 0)
	break;
    }
  }

  /* Getting session ID to retrieve URL RESP HDR */ 
   sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
   save_url_resp_hdr = session_table_shr_mem[sess_id].save_url_body_head_resp & SAVE_URL_HEADER;

  //printf ("4 Req code=%d\n", cptr->req_code);
  if (cptr->req_code_filled){
    if(((vptr->cur_page->save_headers & SEARCH_IN_HEADER)|| save_url_resp_hdr) && (cptr->url_num->proto.http.type == MAIN_URL)){
      NSDL2_HTTP(NULL, cptr, "Response code is filled. Going to save headers");
      save_header(vptr, buf, bytes_read);
    }
    continue;
  }

  #ifdef NS_DEBUG_ON 
  NSDL4_HTTP(NULL, NULL, "Outside For Loop: handle_read(): cptr->conn_state = %d, content length = %d, "
                          "bytes_handled = %d, bytes_read = %d", 
                           cptr->conn_state, cptr->content_length, bytes_handled, bytes_read);
  #endif
 
  process_ws_frame:

  for (; (bytes_handled < bytes_read) || 
         ((cptr->conn_state == CNST_READING || cptr->conn_state == CNST_WS_READING || cptr->conn_state == CNST_WS_FRAME_READING) && (!cptr->content_length))  /* This AND statement for the case if we get a zero length http message */ ; )
    {
      switch ( cptr->conn_state ) {
	case CNST_HEADERS:
	  /* State machine to read until we reach the file part.  Looks for
	  ** Content-Length header too.
	  */
#define STATE_TRANSITION_TABLE http_hdr_states_shr_mem[cur_header_state].http_state_model[(int)buf[bytes_handled]]
	  for ( ; bytes_handled < bytes_read && cptr->conn_state == CNST_HEADERS; ++bytes_handled ) {
                
                if(!allowedCharInHttpHdr[(unsigned char)buf[bytes_handled]])
                {
                  log_bad_data(cptr, buf, bytes_read);
                  handle_bad_read (cptr, now);
                  return;
                }
                cur_header_state = cptr->header_state;
                cptr->header_state = STATE_TRANSITION_TABLE.nxt_state_id;

                /*NSTL1(NULL, NULL, "bytes_handled = %d, bytes_read = %d, buf[bytes_handled] = %c, cur_header_state = %d,"
                                  "cptr->header_state = %d, STATE_TRANSITION_TABLE.process_hdr_callback = %p", 
                                   bytes_handled, bytes_read, buf[bytes_handled], cur_header_state, cptr->header_state, 
                                   STATE_TRANSITION_TABLE.process_hdr_callback);*/

                /*NSDL4_HTTP(NULL, cptr, "Setting Header State: CurHeaderstate '%d' = %s, NextHeaderState '%d' = %s,"
                "CallBack = %p, bytes_handled = %d, bytes_read = %d, cur byte = '%c'", 
                cur_header_state, HdrStateArray[cur_header_state], cptr->header_state,
		HdrStateArray[cptr->header_state], STATE_TRANSITION_TABLE.process_hdr_callback, 
                 bytes_handled, bytes_read, buf[bytes_handled]);*/

                if(STATE_TRANSITION_TABLE.process_hdr_callback != NULL) {
                  NSDL2_HTTP(NULL, cptr, "Calling Callback function");
                  bytes_consumed = 0;
                  if(STATE_TRANSITION_TABLE.process_hdr_callback(cptr, buf + bytes_handled,
				   bytes_read - bytes_handled, &bytes_consumed, now)) {
                     NSDL2_HTTP(NULL, cptr, "Returning as callback returns non-zero value");
                     return;  // Got Some Error
                  }
                  NSDL2_HTTP(NULL, NULL, "CNST_HEADERS: bytes handle = %d, bytes_consumed = %d", bytes_handled, bytes_consumed);
                  bytes_handled += bytes_consumed;
                }
             }

             /* Getting session ID to retrieve URL RESP HDR */ 
             sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
             save_url_resp_hdr = session_table_shr_mem[sess_id].save_url_body_head_resp & SAVE_URL_HEADER;

             if((cptr->conn_state != CNST_HEADERS ) && (((vptr->cur_page->save_headers & SEARCH_IN_HEADER)|| save_url_resp_hdr) && (cptr->url_num->proto.http.type == MAIN_URL))){
               NSDL2_HTTP(NULL, cptr, "Body started. Going to save headers");
               save_header(vptr, buf, bytes_handled);
             }

	  break;  /* Break for case CNST_HEADERS */

	case CNST_CHUNKED_READING:
          //  Note that cptr->content_length has the chunk size to be read
          //  cptr->bytes is teh bytes read out of // a chunk so far.
          //  cptr->total_bytes is the total bytes reads all of chunks
	 /*
	chunked-Body   = *chunk
                        last-chunk
                        trailer
                        CRLF

       chunk          = chunk-size [ chunk-extension ] CRLF
                        chunk-data CRLF
       chunk-size     = 1*HEX
       last-chunk     = 1*("0") [ chunk-extension ] CRLF

       chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
       chunk-ext-name = token
       chunk-ext-val  = token | quoted-string
       chunk-data     = chunk-size(OCTET)
       trailer        = *(entity-header CRLF)
	Example: 9f\r\n.......\r\n2a\r\n........0\r\n\r\n
	*/

      for ( ; bytes_handled < bytes_read && cptr->conn_state == CNST_CHUNKED_READING; ++ bytes_handled)
	    {
	      switch (cptr->chunked_state)
		{
		case CHK_SIZE:
		  switch ( buf[bytes_handled] )
		    {
		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
		      if (cptr->content_length == -1)
			cptr->content_length = 0;
		      cptr->content_length = cptr->content_length * 16 + buf[bytes_handled] - '0';
		      break;
		    case 'A': case 'B': case 'C':
		    case 'D': case 'E': case 'F':
		      if (cptr->content_length == -1)
			cptr->content_length = 0;
		      cptr->content_length = cptr->content_length * 16 + 10 + (buf[bytes_handled] - 'A');
		      break;
		    case 'a': case 'b': case 'c':
		    case 'd': case 'e': case 'f':
		      if (cptr->content_length == -1)
			cptr->content_length = 0;
		      cptr->content_length = cptr->content_length * 16 + 10 + (buf[bytes_handled] - 'a');
		      break;
		    case '\r':
		      cptr->bytes = 0;
		      cptr->chunked_state = CHK_CR;

                      NSDL1_HTTP(NULL, cptr, "Received start of new chunk. chunk_sz = 0x%x(%d)", cptr->content_length, cptr->content_length);
#if 0
                      /* call if no_validation is 0, mail url and compresss is not 0 */
                      /* Curently uncompress is not done for body returned by ns_url_get_body_msg */
                      if (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation && 
                          url_num->proto.http.type == MAIN_URL && cptr->compression_type && 
                          g_uncompress_chunk) {
                        copy_chunk_size_node(cptr);
                      }
#endif
		      break;
		    case ';':
		      cptr->bytes = 0;
		      cptr->chunked_state = CHK_TEXT;
		      break;
		    case ' ': case '\t':
		      break;
		    default: {
		      cptr->bytes = 0;
		      fprintf(stderr, "Warning, recieved invalid character in the chunked header before, after or within the chunk size\n");
		      cptr->chunked_state = CHK_TEXT;
		      break;
		    }
		    }
		  break;

		case CHK_TEXT:
		  switch ( buf[bytes_handled] )
		    {
		    case '\r':
		      cptr->chunked_state = CHK_CR;
		      break;
		    default:
		      break;
		    }
		  break;

		case CHK_CR:
		  switch( buf[bytes_handled] )
		    {
		    case '\n':
		      switch ( cptr->content_length )
			{
			case 0:  /* this means that we recieved a zero length chunk, so its the last chunk */
                          NSDL1_HTTP(NULL, cptr, "Received last chunk. chunk_sz = 0x%x(%d)", cptr->content_length, cptr->content_length);
			  cptr->conn_state = CNST_HEADERS;   /* could be headers after the chunked data */
			  cptr->header_state = HDST_CRLF;   /* our chunked_state is not CHK_NO_CHUNKS, so header state machine will know what to do */
			  cptr->chunked_state = CHK_FINISHED;
			  break;
			default:  /* We have a non-zero length chunk, so we have to see if it was read already or not */
			  if (cptr->content_length < 0) {  /* should never happen */
			    printf("Content Length Nagative (%d)\n", cptr->content_length);
			    cptr->chunked_state = CHK_TEXT;
			    break;
			  }

			  if (cptr->bytes == 0) {   /* have not read any of the chunk, so go ahead and read it */
			    cptr->chunked_state = CHK_READ_CHUNK;
			    break;
			  } else {            /* finished reading the chunk, so go get the size for the next chunk */
			    if (cptr->bytes >= cptr->content_length) {
                              NSDL1_HTTP(NULL, cptr, "Chunk read complete");
			      cptr->content_length = 0;
			      cptr->chunked_state = CHK_SIZE;
			      break;
			    }
			  }
			  break;
			}
		      break;
		    default:
		      printf("Warning, recieved invalid character after '\\r', expecting '\\n' in chunked header\n");
                      error_log("Warning, recieved invalid character after '\\r', expecting '\\n' in chunked header\n");
                      
		      cptr->chunked_state = CHK_TEXT;
		      break;
		    }
		  break;

		case CHK_READ_CHUNK:
		  {
		    int left_to_read = cptr->content_length - cptr->bytes;
		    int bytes_read_in_chunk = 0;
		    char* chunk_start = &buf[bytes_handled];
		    //url_num = cptr->url_num;

		if (left_to_read < 0) {
			printf("Negative left to read bytes (%d) of a chunk\n", left_to_read);
                        error_log("Negative left to read bytes (%d) of a chunk\n", left_to_read);
	  		NSDL1_HTTP(NULL, cptr, "BADBYTE:9");
                        if(copy_data_to_full_buffer(cptr)) { // added this check in 3.8.5 for Bug Id 4704
                          sprintf(err_msg, "\nnagative left to read bytes (%d) of a chunk:", left_to_read);
                          copy_retrieve_bad_data(cptr, chunk_start, 
                                               bytes_read - bytes_handled, 
                                               cptr->total_bytes, err_msg);
                        }
			Close_connection( cptr , 0, now, NS_REQUEST_BADBYTES,  NS_COMPLETION_BAD_BYTES);
			return;
		} else if (left_to_read == 0) {
			if (buf[bytes_handled] == '\r') {
			  cptr->chunked_state = CHK_CR;
			  break;
			} else {
                          printf("Bad Format of chunked data. Expecting \\r after the chunk data, got = [%c]\n", buf[bytes_handled]);
                          error_log("Bad Format of chunked data. Expecting \\r after the chunk data, got = [%c]\n", buf[bytes_handled]);
                          if(copy_data_to_full_buffer(cptr)) { // added this check in 3.8.5 for Bug Id 4704
                            sprintf(err_msg, "Bad Format of chunked data. Expecting \\r after the chunk data:");
                            copy_retrieve_bad_data(cptr, chunk_start, 
                                                 bytes_read - bytes_handled, 
                                                 cptr->total_bytes, err_msg);
                          }


                          NSDL1_HTTP(NULL, cptr, "BADBYTE:10");
                          Close_connection( cptr , 0, now, NS_REQUEST_BADBYTES,  NS_COMPLETION_BAD_BYTES);
                          return;
			}
		} else {
		    if ((bytes_read - bytes_handled) >= left_to_read) {
		      cptr->bytes += left_to_read;
		      bytes_read_in_chunk = left_to_read;
		      average_time->total_bytes += bytes_read_in_chunk;
                      if(SHOW_GRP_DATA) {
                        avgtime *lol_average_time;
                        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
                        lol_average_time->total_bytes += bytes_read_in_chunk;
                      }
		    }
		    else {
		      cptr->bytes += (bytes_read - bytes_handled);
		      bytes_read_in_chunk = (bytes_read - bytes_handled);
		      average_time->total_bytes += bytes_read_in_chunk;
                      if(SHOW_GRP_DATA) {
                        avgtime *lol_average_time;
                        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
                        lol_average_time->total_bytes += bytes_read_in_chunk;
                      }
		    }

		    if (do_checksum)
		      calculate_checksum(cptr, buf, bytes_handled, bytes_handled+bytes_read_in_chunk);

		    bytes_handled += bytes_read_in_chunk;

		    bytes_handled--; //Reduce by one. as it would be increamented in the loop

 		    NSDL3_HTTP(NULL, cptr, "[cptr=0x%lx]: Read %d bytes from chunked data", (u_ns_ptr_t)cptr, bytes_read_in_chunk);
                   /* copy the data to the full_buffer[is used by registration apis]*/
                    if(copy_data_to_full_buffer(cptr)) {
                      copy_retrieve_data(cptr, chunk_start, bytes_read_in_chunk, cptr->total_bytes);
                    }

		    if ((url_num->proto.http.type != MAIN_URL) && (global_settings->interactive)) {
	    	  	save_embedded(cptr, chunk_start, bytes_read_in_chunk, cptr->total_bytes);
	  	    }
		    cptr->total_bytes += bytes_read_in_chunk;
		    if (cptr->ctxt) {
			    if (!htmlParseChunk(cptr->ctxt, chunk_start, bytes_read_in_chunk, 0)) {
                              // DL ISSUE
			      if (group_default_settings->debug >= 3)
				xmlParserWarning(cptr->ctxt, "Handle_read html parsing error\n");
			    }
		    }
		  }
		  }
		  break;
		}
	    }
	  break; /* break for case CNST_CHUNKED */

	case CNST_READING:

	  //url_num = cptr->url_num;

          NSDL3_HTTP(NULL, cptr, "CNST_READING: cptr = %p, cptr->buf_head = %p, bytes_read = %d, bytes_handled = %d, cptr->bytes = %d", 
                                    cptr, cptr->buf_head, bytes_read, bytes_handled, cptr->bytes);
          // In case of Upgrade mode copy_retrieve data was called twice which results in dumping of settings and header frame. 
          // For Http2 copy_retrieve_data is called after processing data_frame 
          if (cptr->http_protocol != 2) {
            if(copy_data_to_full_buffer(cptr)) {
	      copy_retrieve_data(cptr, &buf[bytes_handled], bytes_read - bytes_handled, cptr->bytes);
            }
          }
          NSDL3_HTTP(NULL, cptr, "cptr->buf_head = %p", cptr->buf_head);

	  if ((url_num->proto.http.type != MAIN_URL) && (global_settings->interactive)) {
	    save_embedded(cptr, &buf[bytes_handled], bytes_read - bytes_handled, cptr->bytes);
	  }

	  cptr->bytes += bytes_read - bytes_handled;
 
	  average_time->total_bytes += bytes_read - bytes_handled;
          if(SHOW_GRP_DATA) {
            avgtime *lol_average_time;
            lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
            lol_average_time->total_bytes += bytes_read - bytes_handled;
          }
	  NSDL3_HTTP(NULL, cptr, "[cptr=%p]: Read %d bytes from data, rcd=%d expt=%d",
		cptr, bytes_read - bytes_handled, cptr->bytes, cptr->content_length);

	  if (cptr->ctxt) {
	    if (!htmlParseChunk(cptr->ctxt, &buf[bytes_handled], bytes_read - bytes_handled, 0)) {
              // DL ISSUE
	      if (group_default_settings->debug >= 3)
		xmlParserWarning(cptr->ctxt, "Handle_read html parsing error\n");
	    }
	  }

          extra_bytes = cptr->bytes - cptr->content_length;
          if (extra_bytes < 0) extra_bytes = 0;

	  if ( do_checksum ) {
	    checksum = cptr->checksum;
	    for ( ; bytes_handled < bytes_read; ++bytes_handled ) {
	      if ( checksum & 1 )
		checksum = ( checksum >> 1 ) + 0x8000;
	      else
		checksum >>= 1;
	      checksum += buf[bytes_handled];
	      checksum &= 0xffff;
	    }
	    cptr->checksum = checksum;
	  }
	  else {
	    bytes_handled = bytes_read;
          }

          NSDL3_HTTP(NULL, cptr, "cptr->content_length = %d, cptr->bytes = %d extra_bytes %d", cptr->content_length, cptr->bytes, extra_bytes);
	  if ( (cptr->content_length != -1) && cptr->bytes >= cptr->content_length ) {
	    int req_sts = get_req_status(cptr);
            int num_pipe = cptr->num_pipe;
             
            if (cptr->http_protocol == HTTP_MODE_HTTP2 && (extra_bytes)) {
              int bh = (bytes_read - extra_bytes);
              NSDL4_HTTP2(NULL, NULL, "bh = %d", bh);
              int ret =  http2_process_read_bytes(cptr, now, extra_bytes, (unsigned char*)(buf+ bh));
              // Closing connection in case of upgrdae mode when server is either sending reset frame or goaway frame. 
              if (ret == -1 ){
                NSDL1_HTTP2(NULL, NULL, "ret is [%d]", ret);
                Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
                return;
              }
            } 
	    Close_connection(cptr, req_sts == NS_REQUEST_OK?1:0, now, req_sts, NS_COMPLETION_CONTENT_LENGTH);

            NSDL2_WS(NULL, cptr, "req_sts = %d", req_sts);
            /* if there is buffer remaining to be processed and pipelining is enabled on this
             * con then most likely there another response on the con. Process it. */
            if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && (num_pipe > 1)) {
              
              ClientData client_data;
               int bh;

              reset_cptr_attributes(cptr);
              cptr->conn_state = CNST_HEADERS;
              cptr->header_state = HDST_BOL;
              cptr->content_length = 0;

              client_data.p = cptr;
              cptr->timer_ptr->actual_timeout = 
                runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.idle_secs;
              dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, 
                            client_data, 0, global_settings->idle_timeout_all_flag);

              if (extra_bytes) {
                NSDL4_HTTP(NULL, NULL, "Extra bytes = %d", extra_bytes);
                bh = (bytes_read - extra_bytes);
                NSDL4_HTTP(NULL, NULL, "bh = %d", bh);
                NSDL4_HTTP(NULL, NULL, "buf + handled  = %*.*s", 
                           20,20, buf + bh);
                bcopy(buf + bh, buf, extra_bytes);
                NSDL4_HTTP(NULL, NULL, "Extra bytes (%d) cptr->bytes = %d, "
                           "cptr->content_length = %d, "
                           "buf[0:85] = %*.*s", extra_bytes, cptr->bytes,
                           cptr->content_length, 85, 85, buf);
                //cptr->req_code_filled = -2;
                bytes_read = extra_bytes;
                bh = 0;
                extra_bytes = 0;
                goto pipelining;
              } 
              goto try_read_more;
              //break;
            } else {/* return for non pipelineable */
              return;
            }
	  }
	  break; /* Break for case CNST_READING */
  
	  case CNST_WS_READING:
	  case CNST_WS_FRAME_READING:
            NSDL2_WS(NULL, cptr, "WS Conn State = %d, cptr = %p, cptr->buf_head = %p, bytes_read = %d, "
                                 "bytes_handled = %d, cptr->bytes = %d, ws_reading_state = %d", 
                                  cptr->conn_state, cptr, cptr->buf_head, bytes_read, bytes_handled, cptr->bytes, 
                                  cptr->ws_reading_state);
            int payload_len = 0;
            int bytes_left = bytes_read - bytes_handled;
            char *payload_ptr = NULL;
            int req_sts = get_req_status(cptr);
            int tot_bytes = 0;
            int cptr_old_conn_state = cptr->conn_state;
 
            if((cptr->conn_state == CNST_WS_READING) && (req_sts != NS_REQUEST_OK))
            {
              NSTL1(NULL, cptr, "Websocket request status is not NS_REQUEST_OK, switching to VUser context");
              cptr->flags &= ~NS_CPTR_DO_NOT_CLOSE_WS_CONN;
              websocket_close_connection(cptr, 1, now, req_sts, NS_COMPLETION_NOT_DONE);
              return;
            }
             
            /* If Websocket Response has body with handshake then read body */
            NSDL2_WS(NULL, cptr, "bytes_left = %d, cptr->bytes = %d", bytes_left, cptr->bytes);
            if(bytes_left > 0)
            {
              if(cptr->ws_reading_state == NEW_FRAME)  //Handle new frame reading
              {
                tot_bytes = 0;
              }
              else // this is for next frames read later
              {
                tot_bytes = cptr->bytes;
              }

              payload_len = handle_frame_read(cptr, (unsigned char *)&buf[bytes_handled], bytes_left, &payload_ptr);

              if(payload_ptr)
                bytes_handled += ((char*)payload_ptr + payload_len) - (char*)&buf[bytes_handled];
              else
                bytes_handled += payload_len;

              //partial_payload_len = cptr->bytes - ((char*)payload_ptr - (char*)&buf[bytes_handled]); //partial payload len for 1st frame
              //partial_payload_len = payload_len;

              NSDL2_WS(NULL, NULL, "After frame processing: cptr->bytes(i.e total payload len) = %d, "
                                   "payload_len = %d", 
                                    cptr->bytes, payload_len);
 
              //Bug 31966 - WebSocket: Need read and search API for websocket as in WebSocket there is no concept of Page 
              /* Dump Payload into url_rep_body_xxx */
  	      if(cptr->bytes)
              {
                debug_log_http_res_body(vptr, payload_ptr, payload_len); 
                debug_log_http_res(cptr, payload_ptr, payload_len);
                /* Frame body saperater */
                //if(cptr->ws_reading_state == NEW_FRAME)
                if(cptr->bytes >= cptr->content_length)
                {
                  char line_break[] = "\n------------------------------------------------------------\n";
                  debug_log_http_res_body(vptr, line_break, strlen(line_break));
                }

                NSDL2_WS(NULL, NULL, "Copy Websocket body into cptr->buf, cptr->bytes = %d, cptr_bytes = %d", 
                                      cptr->bytes, payload_len);
                
	        copy_retrieve_data(cptr, payload_ptr, payload_len, tot_bytes);
              }

              resp_sz += cptr->bytes;
            }
            else
              bytes_handled = bytes_read; 

	    NSDL2_WS(NULL, cptr, "[cptr=%p]: Read %d bytes from data, rcd=%d expt=%d",
                         	    cptr, bytes_read - bytes_handled, cptr->bytes, cptr->content_length);

            NSDL2_WS(NULL, cptr, "cptr->content_length = %d, cptr->bytes = %d, bytes_handled = %d, bytes_read = %d, resp_sz = %ld", 
                                  cptr->content_length, cptr->bytes, bytes_handled, bytes_read, resp_sz);

            //if((cptr->bytes >= cptr->content_length) && (bytes_handled >= bytes_read))
            if((cptr->content_length >= 0) && (cptr->bytes >= cptr->content_length))
            //if(bytes_handled >= bytes_read)
            {
              /********************************** 
                 Mohita:-  
                   Resolve Bug id 34333: Test is not stopped after completion of Test schedule
                   Root Cause: If API ns_page_think() apply in between API ns_web_websocket_send() and
                     ns_web_websocket_read(), NVM reads response data due ns_page_think_time() and switch to VUser contex
                     Due to this that user always be in thinking state but when same VUser goes for session end then reduces
                     active VUser count blindly and hence VUser count goes into -ve
                   Resolve:
                     In this case read response data and return if VUser is not in Active state. 
                     VUser context will be switched only if that VUser is in active state.
                   Suggession:
                     Client should apply API ns_page_think_time() after one pair of send and read (all reads against the same send). 
                     Eg:
                       Case 1: If single response message against send message
                         send 1 ----> 
                         read 1 <---
                         ns_page_think_time()
                  
                       Case 2: If multiple response message (say 2 messages) against send message
                         send 1 --->
                         read 1 <---
                         read 2 <---
                         ns_page_think_time() 
              ***********************************/
              if(vptr->vuser_state != NS_VUSER_ACTIVE)
              {
                NSTL1(NULL, cptr, "VUser is not in active state but reading data and returning, vptr->vuser_state = %d", vptr->vuser_state);
                return;
              }
 
              if(cptr->conn_state == CNST_WS_READING)
              {
                NSTL1(NULL, cptr, "WebSocket handshake pass for cptr = %p, vptr = %p", cptr, cptr->vptr);
                websocket_close_connection(cptr, 0, now, req_sts, NS_COMPLETION_CONTENT_LENGTH);
              }
              else
              {
                //cptr->bytes = resp_sz;
                copy_url_resp(cptr);
                INC_WS_RX_BYTES_COUNTER(vptr, cptr->bytes);
                free_cptr_buf(cptr);
              }
              
              /* Delete timer added in API ns_web_websocket_read() as read is complete */
              vptr->ws_status = 0;
              if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
              {
                dis_timer_del(cptr->timer_ptr);
              }
              
              NSDL2_WS(NULL, cptr, "WS: cptr = %p, resp_size = %d, resp = %s", cptr, cptr->bytes, url_resp_buff);
              
              cptr->conn_state = CNST_WS_IDLE;
              cptr->bytes = 0;  //Rest it for new frame
        
              if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
                switch_to_vuser_ctx(vptr, "WebSocket Connect response completely read");
              else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
                send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_READ_REP, 0);
              else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
              { /* In case of websocket_connect and websocket_read we have to
                 send message to NJVM based on the old state of CPTR.
                 In case of websocket_connect we will be in WS_READING state while in
                 case of websocket_read we will be WS_FRAME_READING state */ 
                if(cptr_old_conn_state == CNST_WS_READING)
                   send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_URL_REP, 0);
                else if(cptr_old_conn_state == CNST_WS_FRAME_READING)
                   send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_READ_REP, 0);
              }

              NSDL2_WS(NULL, cptr, "IN NVM contx: bytes_handled = %d, bytes_read = %d, conn_state = %d, ws_new_conn_state = %d", 
                                    bytes_handled, bytes_read, cptr->conn_state, ws_new_conn_state);
              if(bytes_handled < bytes_read)
              {
                if(cptr->conn_state != CNST_WS_FRAME_READING)
                {
                  ws_new_conn_state = cptr->conn_state;
                  cptr->conn_state = CNST_WS_FRAME_READING;
                }
                continue;
              }

              if(ws_new_conn_state != -1)
                cptr->conn_state = ws_new_conn_state;
              return;
            }

            if (bytes_handled < bytes_read)
              continue;
            else if(bytes_handled == bytes_read)
              return;          
  
	    break; /* Break for case CNST_WS_READING */
	}
        NSDL4_HTTP(NULL, NULL, "Inside for: cptr->conn_state = %d, content length = %d, "
                               "bytes_handled = %d, bytes_read = %d, ws_new_conn_state = %d", 
                                cptr->conn_state, cptr->content_length, bytes_handled, bytes_read, ws_new_conn_state);
    } 
    
    /* Geeting session ID to retrieve URL RESP HDR */ 
    sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
    save_url_resp_hdr = session_table_shr_mem[sess_id].save_url_body_head_resp & SAVE_URL_HEADER;

    if((cptr->conn_state == CNST_HEADERS ) && (((vptr->cur_page->save_headers & SEARCH_IN_HEADER)|| save_url_resp_hdr) && (cptr->url_num->proto.http.type == MAIN_URL))){
      NSDL2_HTTP(NULL, cptr, "Partial header read. Going to save headers");
      save_header(vptr, buf, bytes_handled);
    }
  }
}

void *ns_get_hdr_callback_fn_address(char *func_name) {

  NSDL2_HTTP(NULL, NULL, "Method Called, func_name = %s", func_name);

  if(strcmp(func_name, "body_starts") == 0)
    return body_starts;
  else if(strcmp(func_name, "change_state_to_hdst_bol") == 0)
    return change_state_to_hdst_bol;
  else if(strcmp(func_name, "is_chunked_done_if_any") == 0)
    return is_chunked_done_if_any;
  else if(strcmp(func_name, "proc_http_hdr_content_encoding_deflate") == 0)
    return proc_http_hdr_content_encoding_deflate;
  else if(strcmp(func_name, "proc_http_hdr_content_encoding_gzip") == 0)
    return proc_http_hdr_content_encoding_gzip;
  else if(strcmp(func_name, "proc_http_hdr_content_encoding_br") == 0)
    return proc_http_hdr_content_encoding_br;
  else if(strcmp(func_name, "proc_http_hdr_content_length") == 0)
    return proc_http_hdr_content_length;
  else if(strcmp(func_name, "proc_http_hdr_location") == 0)
    return proc_http_hdr_location;
  else if(strcmp(func_name, "proc_http_hdr_link") == 0)
    return proc_http_hdr_link;
  else if(strcmp(func_name, "proc_http_hdr_set_cookie") == 0)
    return proc_http_hdr_set_cookie;
  else if(strcmp(func_name, "proc_http_hdr_transfer_encoding_chunked") == 0)
    return proc_http_hdr_transfer_encoding_chunked;
  else if(strcmp(func_name, "proc_http_hdr_content_type_amf") == 0)
    return proc_http_hdr_content_type_amf;
  else if(strcmp(func_name, "proc_http_hdr_content_type_protobuf") == 0)
    return proc_http_hdr_content_type_protobuf;
  else if(strcmp(func_name, "proc_http_hdr_content_type_hessian") == 0)
    return proc_http_hdr_content_type_hessian;
  else if(strcmp(func_name, "proc_http_hdr_content_type_grpc_proto") == 0)
    return proc_http_hdr_content_type_grpc_proto;
  else if(strcmp(func_name, "proc_http_hdr_content_type_grpc_json") == 0)
    return proc_http_hdr_content_type_grpc_json;
  else if(strcmp(func_name, "proc_http_hdr_grpc_encoding_gzip") == 0)
    return proc_http_hdr_grpc_encoding_gzip;
  else if(strcmp(func_name, "proc_http_hdr_grpc_encoding_deflate") == 0)
    return proc_http_hdr_grpc_encoding_deflate;
  else if(strcmp(func_name, "proc_http_hdr_age") == 0)
    return proc_http_hdr_age;
  else if(strcmp(func_name, "proc_http_hdr_cache_control") == 0)
    return proc_http_hdr_cache_control;
  else if(strcmp(func_name, "proc_http_hdr_date") == 0)
    return proc_http_hdr_date;
  else if(strcmp(func_name, "proc_http_hdr_etag") == 0)
    return proc_http_hdr_etag;
  else if(strcmp(func_name, "proc_http_hdr_expires") == 0)
    return proc_http_hdr_expires;
  else if(strcmp(func_name, "proc_http_hdr_last_modified") == 0)
    return proc_http_hdr_last_modified;
  else if(strcmp(func_name, "proc_http_hdr_pragma") == 0)
    return proc_http_hdr_pragma;
  else if(strcmp(func_name, "proc_http_hdr_auth_ntlm") == 0)
  {
    return proc_http_hdr_auth_ntlm;
  }
  else if(strcmp(func_name, "proc_http_hdr_auth_proxy") == 0)
    return proc_http_hdr_auth_proxy;
  else if(strcmp(func_name, "proc_http_hdr_x_cache") == 0)
    return proc_http_hdr_x_cache;
  else if(strcmp(func_name, "proc_http_hdr_x_cache_remote") == 0)
    return proc_http_hdr_x_cache_remote;
  else if(strcmp(func_name, "proc_http_hdr_x_check_cacheable") == 0)
    return proc_http_hdr_x_check_cacheable;
  else if(strcmp(func_name, "proc_http_hdr_content_type_octet_stream") == 0)
    return proc_http_hdr_content_type_octet_stream;
  else if(strcmp(func_name, "proc_http_hdr_connection_close") == 0)
    return proc_http_hdr_connection_close;
  else if(strcmp(func_name, "proc_http_hdr_x_dynaTrace") == 0)
    return proc_http_hdr_x_dynaTrace;
  else if(strcmp(func_name, "proc_ws_hdr_upgrade") == 0)
   return proc_ws_hdr_upgrade;
  else if(strcmp(func_name, "proc_hls_hdr_content_type_mpegurl") == 0)
   return proc_hls_hdr_content_type_mpegurl;
  else if(strcmp(func_name, "proc_hls_hdr_content_type_media") == 0)
   return proc_hls_hdr_content_type_media;
  //TODO: when connection type is already exist in machine state trans java tool
  //else if(strcmp(func_name, "proc_ws_hdr_connection") == 0)
  // return proc_ws_hdr_connection;
  else if(strcmp(func_name, "proc_ws_hdr_sec_websocket_accept") == 0)
    return proc_ws_hdr_sec_websocket_accept;
  else if(strcmp(func_name, "NULL") == 0)
    return NULL;
  else {
    NS_EXIT(-1, "Error: Invalid function name '%s' given to load http state transition.", func_name);
  }
  return NULL;
}
