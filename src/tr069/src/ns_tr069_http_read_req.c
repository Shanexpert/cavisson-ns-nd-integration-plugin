#define _GNU_SOURCE

#include "ns_tr069_includes.h"
#include "ns_tr069_acs_con.h"

#define MOD_SELECT_AFTER_WRITE_BLOCKS() mod_select(-1, cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP);

#define STATIC_HANDLE_EAGAIN \
  cptr->bytes_left_to_send = to_send; \
  NSDL2_TR069(NULL, cptr, "Failed due to EAGAIN. Returning back. Bytes_remaining = %d, offset = %d and to_send = %d", cptr->bytes_left_to_send, cptr->body_offset, to_send); \
  if(cptr->conn_state == CNST_READING) \
  {\
    cptr->conn_state = CNST_WRITING; \
    MOD_SELECT_AFTER_WRITE_BLOCKS();\
  } \
  return;

// This macro will call if there was error in sending response
// And error in reading from file or sending on socket connection
#define STATIC_HANDLE_ERROR \
  close_fd(cptr, 1, now); \
  return;

static void process_request(connection *cptr, u_ns_ts_t now);
static void process_rfc_request(connection *cptr, u_ns_ts_t now);
static void send_rfc_response(connection *cptr, char *buf, u_ns_ts_t now);

static void handle_hdr(connection *cptr, char *buf, int len, int module_mask) {
  char *ptr;

  NSDL2_TR069(NULL, cptr, "Method called. Remaining Header Buffer = %s and length of header = %d", buf, len);

  if(cptr->cur_hdr_value != NULL) {
    NSDL2_TR069(NULL, cptr, "Reallocating buffer. content_length = %d", cptr->content_length + len);
    // Realloc and append buf  (allocatr for + 1 bytes) and  adjust content_length
    MY_REALLOC(cptr->cur_hdr_value, cptr->content_length + len + 1, "cptr->cur_hdr_value", -1);
    ptr = cptr->cur_hdr_value + cptr->content_length; // Points to address where we need to copy
  } else {
    NSDL2_TR069(NULL, cptr, "Allocating buffer. content_length = %d", len);
    // Alloc and copy
    MY_MALLOC(cptr->cur_hdr_value , len + 1, "cptr->cur_hdr_value", -1);
    ptr = cptr->cur_hdr_value; // Points to address where we need to copy
  }
  memcpy(ptr, buf, len);
  cptr->content_length += len;
  cptr->cur_hdr_value[cptr->content_length] = '\0';
  // printf("Hdr=%s\n", cptr->cur_hdr_value);
}

static inline char *get_url_from_req_line(char *buf, int *len, connection *cptr, int bytes_read, u_ns_ts_t now) {
  char *method_ptr;
  char *req_url_ptr;

  /* Extract URL by Request Header line "GET/POST <URL> HTTP/1.1"
     Note - strtok will replace space by '\0'
  */
  method_ptr = strtok (buf, " "); //ptr pointing to method

  if (strcasecmp(method_ptr, "GET") != 0)  {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR,"CPE RFC received a bad request method %s",method_ptr);
    return (NULL);
  }
  req_url_ptr = strtok (NULL, " "); //req_url_ptr pointing to URL and it is null terminated
  if (!req_url_ptr)
  {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR,"CPE RFC received a bad or NULL request line");
    return(NULL);
  }
  NSDL2_TR069(NULL, cptr, "Method = %s, URL by request line = %s", method_ptr, req_url_ptr);
  *len = strlen(req_url_ptr);

  return(req_url_ptr);
}

static inline int get_content_len(char *buf) {
  int con_len = 0;  //local variable for content length
  char *len_ptr;

  // ISSUE: Header are case-insensitive, Need to handle this later
  len_ptr = strcasestr(buf, "content-length:");  // Must pass lower case name to search
  if(len_ptr) 
  {
    len_ptr += 15;
    if(*len_ptr == ' ') len_ptr++;
    con_len = atoi(len_ptr);
    NSDL2_TR069(NULL, NULL, "Content-Length = %d", con_len);
  }
  else
  {
   NSDL2_TR069(NULL, NULL, "Since Content-Length: is not present, so Content-Length = 0");
  }
  NSDL2_TR069(NULL, NULL, "Content-Length = %d", con_len);
  return(con_len);
}

#define MAX_HDR_SIZE 8096
void http_read_request (connection *cptr, u_ns_ts_t now) {

  int bytes_read = 0,total_read = 0;
  int offset = 0;
  char buf[MAX_HDR_SIZE + MAX_HDR_SIZE + 1];
  char *req_url_ptr;
 
  
  char *msg_end_ptr;
  int con_len; // Content Length
  int url_len;
  char http_dlimiter[] = "\r\n\r\n";

  NSDL2_TR069(NULL, cptr, "Method called. conn_fd = %d", cptr->conn_fd);

  while(1) {
    bytes_read = read( cptr->conn_fd, buf + offset, MAX_HDR_SIZE);
    NSDL2_TR069 (NULL, cptr, "read(). Bytes reads = %d", bytes_read);
    NSDL2_TR069 (NULL, cptr, "Bytes  = %.*s", bytes_read, buf + offset);
    if (bytes_read == 0) { //Connection closed
      NSDL1_HTTP(NULL, cptr, "connection closed by ACS");
      close_fd(cptr, 1, now);
      return;
    } else if (bytes_read < 0) {
      if (errno == EAGAIN) { // no more data available, break from while loop
        break;
        //continue; // As we want to read whole data
      } else if (errno == EINTR) {
        NSDL2_TR069(NULL, cptr, "Read interrupted. Continuing...");
        continue;
      } else {
        if((total_read == 0) && (errno == ECONNRESET)) { // This should be a debug log as hpd get it on read only
          NSDL2_TR069(NULL, cptr, "Connection closed by ACS by RST instead of FIN");
        } else {
          NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR,
                   "Error in read()."
                   " Total bytes read so far in this call = %d. Error = %s",
                   total_read, strerror(errno));
        }
        close_fd(cptr, 1, now);
        return;
      }
    }

    total_read += bytes_read;
    NSDL2_TR069(NULL, cptr, "Total bytes reads = %d,"
                           " byte reads = %d, bytes_left_to_send = %d",
                           total_read, bytes_read, cptr->bytes_left_to_send);

    // bytes_left_to_send -> Must be set to -1 on accept or somehere
    if(cptr->bytes_left_to_send == -1) { // header is not processed yet
      buf[offset+bytes_read] ='\0';
      // cptr->cur_hdr_value

      if (cptr->cur_hdr_value == NULL) { // first time seeing header
        msg_end_ptr = strstr(buf, http_dlimiter);
        if(!msg_end_ptr) { //not all headers read yet
          NSDL2_TR069(NULL, cptr, "Not all header read yet in one loop.");
          // for appending the header if header doesnot recived in one loop.
          offset += bytes_read;
          continue;
        }
        //header is complete the first time itself
        NSDL2_TR069(NULL, cptr, "Headers received in the first read_request().");
        con_len = get_content_len(buf);
        req_url_ptr = get_url_from_req_line(buf, &url_len, cptr, total_read, now);
      } else { //we have partial header already
        NSDL2_TR069(NULL, cptr, "Headers read was not complete in the previous read_request(), so appending headers.");

        handle_hdr(cptr, buf + offset, bytes_read /* msg_end_ptr - buf */, MM_HTTP);
        // is header complete ? if not, append again and read more
        if ((msg_end_ptr = strstr(cptr->cur_hdr_value, http_dlimiter)) == NULL) {
          NSDL2_TR069(NULL, cptr, "Not all header read yet in one loop.");
          // for appending the header if header doesnot recived in one loop.
          offset += bytes_read;
          continue;
        }
        //partial read, but header is complete this time
        // Get content length, url request  
        con_len = get_content_len(cptr->cur_hdr_value);
        req_url_ptr = get_url_from_req_line(cptr->cur_hdr_value, &url_len, cptr, cptr->content_length, now);
      }
      //All header recv, process header.
      offset = 0;
      msg_end_ptr += 4; //skiping "\r\n\r\n" string

      if (!req_url_ptr) {    //illegal RFC request 
        close_fd(cptr, 1, now);
        return; 
      }

      if ( cptr->conn_state == CNST_LISTENING && con_len) { // error . there should be no body
        NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "content length is not zero for CPE RFC.");
      }

      if(con_len) {
	int hdrsize;
	int body_recv;
	int left;
	
	if (cptr->cur_hdr_value == NULL) {
	  hdrsize = msg_end_ptr - buf;
	  body_recv = total_read - hdrsize; //this may be zero at this point.
	  left = con_len - body_recv;
	} else {
	  hdrsize = msg_end_ptr - cptr->cur_hdr_value;
	  body_recv = cptr->content_length - hdrsize; //this may be zero at this point.
	  left = con_len - body_recv;
	}

        NSDL2_TR069(NULL, cptr, "hdrsize =%d, body_recv =%d, left =%d", hdrsize, body_recv, left);
        if(left < 0) {
          left = 0;
          NSDL2_TR069(NULL, cptr, "Body recieved is more than expected. Extra data ignored. body_recv =%d", body_recv);
        }

        if (left > 0) { //body not completly recv.
           cptr->bytes_left_to_send = left;
           NSDL2_TR069(NULL, cptr, "Body Rcd = %d, left = %d", body_recv, left);
           continue;
        }
      } else {
        cptr->bytes_left_to_send = 0;
        break;
      }
    } else {
      NSDL2_TR069(NULL, cptr, "cptr->bytes_left_to_send = %d", cptr->bytes_left_to_send);
      //handle_read(cptr->host_name, cptr, now, buf, bytes_read, cptr->request_url);
      cptr->bytes_left_to_send -= bytes_read;
      if(cptr->bytes_left_to_send < 0) {
        cptr->bytes_left_to_send = 0;
        NSDL2_TR069(NULL, cptr, "Body recieved is more than expected. Extra data ignored. bytes_read =%d", bytes_read);
      }
      if(cptr->bytes_left_to_send == 0) {
        break;
      }
    }
  } // while

  //Code for handling EAGAIN when headers are not complete yet
  if(total_read == 0)  //if get EAGAIN and no data read, then return
  {
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "handle_read() called but no data is available. conn_fd = %d", cptr->conn_fd);
    return;
  }
  if(cptr->bytes_left_to_send == -1) { //All headers not recv. Save hdr and return
    NSDL2_TR069(NULL, cptr, "Headers not received completely in one read_request(), so saving headers and return back."); 
    if (cptr->cur_hdr_value != NULL)
      handle_hdr(cptr, buf + offset, total_read - offset, MM_HTTP);
    else 
      handle_hdr(cptr, buf, total_read, MM_HTTP);
    return;
  }

  //  TODO this need to be reset in ERROR case also
  cptr->bytes_left_to_send = cptr->content_length = -1;
  process_request(cptr, now);  // need to authenticate and respond with 200/204 ok
}


/*  we get here when the CPE has accepted an RFC request 
 * 1. authenticate ACS request with digest or basic authentication
 * 2. if the CPE is not already in a session and no of pending RFCs (authenticated) is < max allowed
 * Respond with a 200/204 on this connection
 * increment a counter in the vptr which the user can check to see if there are pending ACS requests using get_rfc()
 * else
 * Respond with a 503
 *	    
 */

static char rfc_static_response_200[] = "HTTP/1.1 200 OK\r\n\r\n";
static char rfc_static_response_503[] = "HTTP/1.1 503 ServiceUnavailable\r\n\r\n";

static void process_request(connection *cptr, u_ns_ts_t now) {

  NSDL2_TR069(NULL, cptr, "Method Called");

  process_rfc_request(cptr, now);
}

/*This is a dummy function to authenticate*/
static int authenticate_rfc (connection *cptr) {

   return 0;
}

static void process_rfc_request(connection *cptr, u_ns_ts_t now)
{
  char *bufptr;
  int status;

  NSDL2_TR069(NULL, cptr, "Method Called. Number of pending ACS RFC requests = %d", ((VUser*)(cptr->vptr))->num_requests);

  status = authenticate_rfc(cptr); // 0 for success
  if (status) {
    return;
  }

  ((VUser*)(cptr->vptr))->num_requests++;

  if ((((VUser*)(cptr->vptr))->num_requests) < CPE_MAX_RFC_REQUESTS) { 
    //send 200 OK
    NSDL3_TR069(NULL, cptr, "Sending 200 OK for ACS RFC");
    bufptr = rfc_static_response_200; 
  } else { //respond with 503
    NSDL3_TR069(NULL, cptr, "Sending 503 OK for ACS RFC");
    bufptr = rfc_static_response_503;
  }

  cptr->bytes_left_to_send = strlen(bufptr);
  NSDL4_TR069(NULL, cptr, "Buffer (len = %d) = %s", cptr->bytes_left_to_send, bufptr);
  send_rfc_response(cptr, bufptr, now);
}

#if 0
send_rfc_response(Connection *cptr, char *buf)
{
  vector[0].iov_base = buf;
  vector[0].iov_len = strlen(buf);

  if ((bytes_sent = writev(cptr->conn_fd, vector, 1)) < 0) {
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error: (%s) in writing vector for RFC response. conn_fd %d" ,strerror(errno), cptr->conn_fd);

  while(1) {
    write(cptr->conn_fd, buf, sizeof(buf))
    //handle errors 

    if (write is complete) 
      close connection
  } //while
    EAGAIN:
    Save offset in tr069_data->rfc_static_response_offset
   
  }
}
#endif

#define MAX_READ_SIZE 1024*1024
#define NUM_VECTOR_TO_SEND 1
static void send_rfc_response(connection *cptr, char *buf, u_ns_ts_t now)
{  
  int bytes_sent = 0;
  action_request_Shr *url_num = cptr->url_num; 

  NSDL2_TR069(NULL, cptr, "Method Called");
 
  NS_RESET_IOVEC(g_scratch_io_vector);

  cptr->body_offset = 0; // Must start with 0
  NS_FILL_IOVEC(g_scratch_io_vector, buf, cptr->bytes_left_to_send);

  bytes_sent = writev(cptr->conn_fd, g_scratch_io_vector.vector, NUM_VECTOR_TO_SEND);

  if (bytes_sent < g_scratch_io_vector.vector->iov_len) {
    NSDL3_TR069(NULL, cptr, "Complete size not send hence returning. "
                    "bytes_sent = %d, size to send = %d",
                     bytes_sent, g_scratch_io_vector.vector->iov_len);

    handle_incomplete_write(cptr, &g_scratch_io_vector, g_scratch_io_vector.cur_idx, g_scratch_io_vector.vector->iov_len, bytes_sent);
    return;
  }

  NSDL3_TR069(NULL, cptr, "bytes_sent = %d", bytes_sent);
  
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);

  // Note: This gets called even in case of sending 503 response
  tr069_proc_rfc_from_acs(cptr); 

  close_fd(cptr, 1, now);
  FREE_AND_MAKE_NULL(url_num, "url_num", -1);
}
