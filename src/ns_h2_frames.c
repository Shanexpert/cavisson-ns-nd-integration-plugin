/*********************************************************************************************
* Name                   : ns_h2_frames.c 
* Purpose                : This C file holds the function(s) required for packing data.
                           In Http2 data is 
                     
* Author                 : Sanjana Joshi  
* Intial version date    : 6-October-2016 
* Last modification date : 22-December-2016
* Modification History 
* Author(s)              : Shalu Panwar 
* Date                   : 23-Nov-2016
* Change Description     : Added Macro to cpoy bytes to buffer 
* Author(s)              : Sanjana Joshi 
* Date                   : 9-December-2016
* Change Description     : Added Window update frame 
* Author(s)              : NISHI 
* Date                   : 22-December-2016
* Change Description     : Added release frames 


* Author                 : ANUBHAV

* Date                   : December-2017
* Change Description     : Handling of Server Push
* Date                   : May-2018
* Change Description     : Handling of Flow Control
* Date                   : July-2018
* Change Description     : Handling of Reset Frame

*********************************************************************************************/

#include "ns_h2_header_files.h"
#include "ns_exit.h"

// Macro to set bit 
#define SET_FLAG(x,y) x |= y;

#define HTONL_4BYTE(x, y) {\
  char *temp = (char *)&y; \
  x[0] = *temp;\
  temp++; \
  x[1] = *temp;\
  temp++; \
  x[2] = *temp;\
  temp++; \
  x[3] = *temp;\
}

#define HTONL_3BYTE(x, y) {\
  char *temp = (char *)&y; \
  temp++; \
  x[0] = *temp;\
  temp++; \
  x[1] = *temp;\
  temp++; \
  x[2] = *temp;\
}

/**********************************************************************************
 Nishi : Adding function to make free data frame Pool . 
*********************************************************************************/

data_frame_hdr *gFreeDatafrHead = NULL;
data_frame_hdr *gFreeDatafrTail = NULL;

static unsigned long total_allocated_dframes = 0;
static unsigned long total_free_dframes = 0;

static data_frame_hdr* allocate_data_frame_pool()
{
  int i;
  data_frame_hdr* frame_chunk = NULL;
  int data_frame_hdr_size = 0;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  data_frame_hdr_size = INIT_DFRAME_BUFFER * sizeof(data_frame_hdr);
  // Doing memset to make fiels 0/NULL as it will be faster than doing field by filed
  MY_MALLOC_AND_MEMSET(frame_chunk, data_frame_hdr_size, "frame_chunk", -1);

  // Not doing memset as only two fields need to be set to NULL
  //Counters to keep record of free and allocated frames

  total_free_dframes += INIT_DFRAME_BUFFER;
  total_allocated_dframes += INIT_DFRAME_BUFFER;

  NSDL2_HTTP2(NULL, NULL, "Total free frame: total_free_dframes = %lu, total allocated frames: total_allocated_dframes = %lu \n", total_free_dframes, total_allocated_dframes);

  for (i = 0; i < INIT_DFRAME_BUFFER; i++)
  {
    /* Linking frame entries within a pool and making last entry NULL*/
    if (i < (INIT_DFRAME_BUFFER - 1)) {
      frame_chunk[i].next = (struct data_frame_hdr *)&frame_chunk[i + 1];
    }
  }

  // total_conn_list_tail->next_in_list = NULL; commented as it is done in memset
  gFreeDatafrTail = &frame_chunk[INIT_DFRAME_BUFFER - 1];
  NSDL2_HTTP2(NULL, NULL, "frame_chunk = %p", frame_chunk);

  return(frame_chunk);
}

/*-------------------------------------------------------------------------------------
Nishi : 
--------------------------------------------------------------------------------------*/
data_frame_hdr* get_free_data_frame()
{
  data_frame_hdr *free;
  /* If gFreeVuserHead is NULL then realloc frame pool otherwise draw frame from existing pool*/
  if (gFreeDatafrHead == NULL)
  {
    NSDL2_HTTP2(NULL, NULL, "Allocating frame pool");
    gFreeDatafrHead = allocate_data_frame_pool();
    if(gFreeDatafrHead == NULL) // If we are not able to allocate frame pool
    {
      NSDL2_HTTP2(NULL, NULL, "Frame allocation failed\n");
      NS_EXIT(-1, "Frame allocation failed");
    }
  }
  // Update free counter for frame slot
  total_free_dframes--;
  NSDL2_HTTP2(NULL, NULL, "Updated free data frame counter: total_free_dframes = %lu, total_allocated_dframes = %lu", total_free_dframes, total_allocated_dframes);

  /* Frame Pool Design: Traverse frame list and update gFreeDatafrHead and update*/

  free = gFreeDatafrHead;
  gFreeDatafrHead = (data_frame_hdr *)free->next; // Move free head to next sptr on pool
  if (gFreeDatafrHead == NULL) // If we took the last sptr of the pool, then both head and tail need to be set to NULL
  {
    NSDL2_HTTP2(NULL, NULL, "Last frame fetch from pool, then both head and tail need to be set to NULL");
    gFreeDatafrTail = NULL;
  }
  free->next = NULL; 
  NSDL2_HTTP2(NULL, NULL, "Exiting get_free_frame %p", free);

  return free;
}


/*----------------------------------------------------------------------
Purpose:  This function will fill fixed 9-byte header. This header is common for all frames . 

These 9 bytes have following information.

1. Length(24):  The length of the frame payload expressed as an unsigned 
24-bit integer.  Values greater than 2^14 (16,384) MUST NOT be
sent unless the receiver has set a larger value for
SETTINGS_MAX_FRAME_SIZE.
2. Type(8): This byte define the type of frame we are sending . Frame type may include data frame , header frame etc.
3. Flags(8): This byte set bit if present .(They are also used in state transaction.)  Flags may include end header , end stream etc . 
4. Stream Identifier(1+31): The 1 bit stands for reserved bit and is always 0.  Reamining 31 bit is the stream identifier . (In  case of client                             stream is always odd).   

Frame format:
    +-----------------------------------------------+
    |                 Length (24)                   |
    +---------------+---------------+---------------+
    |   Type (8)    |   Flags (8)   |
    +-+-------------+---------------+-------------------------------+
    |R|                 Stream Identifier (31)                      |
    +=+=============================================================+
    |                   Frame Payload (0...)                      ...
    +---------------------------------------------------------------+


Input : This function takes length type flags and stream_identifier as input ...... 
Output : Returns amount of bytes written.
----------------------------------------------------------------------------------*/
int pack_frame (connection *cptr, unsigned char *frame, unsigned int frame_length, unsigned char frame_type, int end_stream_flag, int end_header_flag, unsigned int stream_identifier, unsigned char padding, unsigned char *flag)
{
  int tot_len = 0;
  unsigned char *tmp = NULL; 

  NSDL1_HTTP2(NULL, NULL, "Method called");

  // Copy Length to header 
  NSDL4_HTTP2(NULL, NULL, "frame_length is [%d] cptr->http2=%p", frame_length, cptr->http2);
  //bug 78144 : return error if cptr->http2 is released
  if(cptr->http2 == NULL)  
   {
     return HTTP2_ERROR;   
   }
   // Handle this case while creating data frame, remove this code from here
  // TODO simplify it 
  if (cptr->http2->settings_frame.settings_max_frame_size == NS_HTTP2_MAX_FRAME_SIZE)
  {
    if (frame_length > NS_HTTP2_MAX_FRAME_SIZE) {
      NSDL2_HTTP2(NULL, NULL, "ERROR: We can not send payload length greater than 16777215");
      return HTTP2_ERROR; 
    }
  } 
  else if (frame_length > NS_HTTP2_INITIAL_FRAME_SIZE)
  {
    NSDL2_HTTP2(NULL, NULL,"ERROR: We can not send payload length greater than 16,384 without negotiation SETTINGS_MAX_FRAME_SIZE");
    return HTTP2_ERROR; 
  }

  // Convert it to host to network order
  frame_length = ntohl(frame_length); 
  HTONL_3BYTE(frame, frame_length);
  tot_len += 3; 
  // Copy frame Type to header 
  MY_MEMCPY(frame + tot_len, &frame_type, 1);  
  tot_len += 1; 
  // Going to set flag(s)
  if (end_stream_flag) 
    SET_FLAG(*flag, END_STREAM);
  if (end_header_flag) 
    SET_FLAG(*flag, END_HEADER);
  if (padding)
    SET_FLAG(*flag, PADDING);
  MY_MEMCPY(frame + tot_len, flag, 1);
  tot_len += 1; 
   
  //Copy reserved to frame header. This will always remain unset .
  stream_identifier = ntohl(stream_identifier);
  tmp = frame +tot_len;
  HTONL_4BYTE(tmp, stream_identifier);
  tot_len += 4; 
  return tot_len; 
}

/**********************************************************************
 The payload of a SETTINGS frame consists of zero or more parameters . 
 We do not need base64_url encoding for settings frames if we are 
 sending it after connection preface . 
    +-------------------------------+
    |       Identifier (16)         |
    +-------------------------------+-------------------------------+
    |                        Value (32)                             |
    +---------------------------------------------------------------+

Where : 
Identifier (2) :This  identify setting type to be published to peer. 
                There are 6 defined identifier for settings frame . 
Value (32)  : Value for corresponding identifier is stored in value . 

Currently we are publishing two settings to corrosponding peer .
1) SETTINGS_MAX_CONCURRENT_STREAMS (0x3)
2) SETTINGS_MAX_FRAME_SIZE (0x5)
 
***********************************************************************/
int pack_settings(connection *cptr, unsigned char *settings_frame)
{
  VUser *vptr = (VUser *)cptr->vptr;

  unsigned char *tmp = NULL; // This will point to settings_frame + amount_written . 
  unsigned int length = 0;
  unsigned short identifier_max_concurrent_stream = 0x3;
  //unsigned short tmp_settings = 0;
 // unsigned int value = 100;
  unsigned int max_concurrent_stream_value = runprof_table_shr_mem[vptr->group_num].gset.http2_settings.max_concurrent_streams;
  unsigned short identifier_max_frame_size = 0x5;
  //unsigned int max_frame_size_value = 16777215;
  unsigned int max_frame_size_value = runprof_table_shr_mem[vptr->group_num].gset.http2_settings.max_frame_size;
  unsigned short identifier_enable_server_push = 0x2;
 // unsigned int enable_server_push_value = cptr->http2->settings_frame.settings_enable_push;
  unsigned int enable_server_push_value = runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push;
  /*bug 84661 -- assigned server psuh value*/
  cptr->http2->settings_frame.settings_enable_push = enable_server_push_value;
  unsigned short identifier_settings_initial_window_size = 0x4;
  unsigned int initial_window_size_value = runprof_table_shr_mem[vptr->group_num].gset.http2_settings.initial_window_size; 
 
  NSDL2_HTTP2(NULL, NULL, "Method called, enable_push = %hi max_concurrent_stream = %d max_frame_size = %d initial_window_size_value = %d",enable_server_push_value, max_concurrent_stream_value, max_frame_size_value, initial_window_size_value); 
  // Copy identifier to settings frame 
  identifier_max_concurrent_stream = htons(identifier_max_concurrent_stream); 
  MY_MEMCPY(settings_frame, &identifier_max_concurrent_stream, 2);
  length += 2;
 
  // Copy value of identifier to frame .Sending MAX_CONCURRENT_STREAMS with value 100 . ( As 100 is minimum for parellism ).
  tmp = settings_frame + length;
  max_concurrent_stream_value = htonl(max_concurrent_stream_value);
  HTONL_4BYTE(tmp, max_concurrent_stream_value);
  length += 4;

  // Copy identifier to settings frame . MAx Frame length in this case
  identifier_max_frame_size = htons(identifier_max_frame_size); 
  MY_MEMCPY(settings_frame + length, &identifier_max_frame_size, 2);
  length += 2; 
  tmp = settings_frame + length;
  max_frame_size_value = htonl(max_frame_size_value);
  HTONL_4BYTE(tmp, max_frame_size_value);
  length += 4;

  // Copy identifier to settings frame. ENABLE SERVER PUSH
  identifier_enable_server_push = htons(identifier_enable_server_push);
  MY_MEMCPY(settings_frame + length, &identifier_enable_server_push, 2);
  length += 2; 
  tmp = settings_frame + length;
  enable_server_push_value = htonl(enable_server_push_value);
  HTONL_4BYTE(tmp, enable_server_push_value);
  length += 4;

 
// Copy identifirt to settings frame. Intial window update frame
  identifier_settings_initial_window_size = htons(identifier_settings_initial_window_size);
  MY_MEMCPY(settings_frame + length, &identifier_settings_initial_window_size, 2);
  length += 2;
  tmp = settings_frame + length;
  initial_window_size_value = htonl(initial_window_size_value);
  HTONL_4BYTE(tmp, initial_window_size_value);
  length += 4;


  NSDL2_HTTP2(NULL, NULL, "frame length is %d", length);
  return length; 
}

/*********************************************************************************
Purpose: This function make complete header frame . 
Header frame consists of : 
1) 9 byte Frame Header 
2) Frame payload 
Frame Payload consists of Prority , Padding and header payload .

Input : Compressed header (compressed using nghttp2)
        Compressed header length. 
        
Output : amount of bytes written . 


**********************************************************************************/
int pack_settings_frames(connection *cptr, unsigned char *settings_frames)
{
  unsigned int amt_written = 0; 
  unsigned int length = 0; 
  int tot_length = 0;
  unsigned char flag = 0;

  NSDL2_HTTP2(NULL, NULL, "Method called");  
  
  if (cptr->http2_state == HTTP2_SETTINGS_DONE) {
    amt_written = pack_frame(cptr, settings_frames, 0, SETTINGS_FRAME, 1, 0, 0x0, 0, &flag);
    return amt_written;
  }
 
 /* We are sending SETTINGS_FRAME_LENGTH as 24, beacuse we are sending max_frame_size (length 6), max concurrent streams 
    (length = 6) and enable_server_push with value 0 to server. We are also sending settings initial window size . 
    This settings is require once 65k mmeory is used in streams.
 */   
  amt_written = pack_frame(cptr, settings_frames, SETTINGS_FRAME_LEN, SETTINGS_FRAME, 0, 0, 0x0, 0, &flag);

  NSDL2_HTTP2(NULL, NULL, "amt_written = %d", amt_written);

  if (amt_written < 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing frame header for settings frame");
    return -1; 
  }
  
  length = pack_settings(cptr, settings_frames + amt_written);
  if (length == 0) 
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing settings frame"); 
    return -1; 
  }
  tot_length = amt_written + length;  
  NSDL2_HTTP2(NULL, NULL, "tot_length = %d, length is %d", tot_length, length);
  return tot_length; 
}


/***************************************************************************************
  
Purpose : This function will make header_frame 
Input   : header_frament_buffer 
        : header_fragment_buffer_len

    +---------------+
    |Pad Length? (8)|
    +-+-------------+-----------------------------------------------+
    |E|                 Stream Dependency? (31)                     |
    +-+-------------+-----------------------------------------------+
    |  Weight? (8)  |
    +-+-------------+-----------------------------------------------+
    |                   Header Block Fragment (*)                 ...
    +---------------------------------------------------------------+
    |                           Padding (*)                       ...
    +---------------------------------------------------------------+ 
 
Output : returns amount written 
***************************************************************************************/
int pack_hdr_payload( unsigned  char * header_payload , unsigned char *header_fragment , unsigned int header_fragment_len , unsigned char pad_len , unsigned char flag)
{
 /* 
   While making header paylod it is neceesary to check for pad flag and priority flag. 
   We will copy Pad length and priority only ifflag is required . 
   We will cover(priority) later . 
 */
   
  unsigned char stream_descriptor[4] = {0}; 
  unsigned char exclusive = 0; 
  unsigned int stream_dependency = 0; 
  unsigned char weight = 0x0; 
  unsigned int offset = 0;
  char pad_buf[7] = {0};

  NSDL3_HTTP2(NULL, NULL, "Method called header_fragment len is [%d], pad_len is [%d]", header_fragment_len, pad_len);
  
  // Check for Pad flag . If pad flag is set we will copy pad length and padding in payload buffer 
  if ((CHECK_FLAG(flag, 3))) {
    NSDL2_HTTP2(NULL, NULL, "PADDING");
    MY_MEMCPY(header_payload + offset, &pad_len, 1);
    offset += 1; 
  }
 
  // check for priority flag.
   if (flag == PRIORITY) {
    NSDL2_HTTP2(NULL, NULL, "PRIORITY");
    // Make stream descriptor field and than copy it to header_payload   
    stream_descriptor[3] |= exclusive; 
    MY_MEMCPY(stream_descriptor + 1, &stream_dependency, 4);
    // Copy this to header payload
    MY_MEMCPY(header_payload + offset, stream_descriptor, sizeof(stream_descriptor));
    offset += sizeof(stream_descriptor); 
    // TODO: calculate weight
    MY_MEMCPY(header_payload + offset, &weight, 1);
    offset += 1;  
  }
   
  // Copy header framgment to header payload 
  MY_MEMCPY(header_payload + offset, header_fragment, header_fragment_len);
  offset += header_fragment_len; 
  if (pad_len && (CHECK_FLAG(flag, 3)))
  { 
    NSDL2_HTTP2(NULL, NULL, "PADD");
    MY_MEMCPY(pad_buf, "0000000", pad_len);
    MY_MEMCPY(header_payload + offset, pad_buf, pad_len); //TODO:: ERROR CHECK HERE 
    offset += pad_len; 
  }

  // if end header flag is set or not / IF not than call continuation frame from here 
  return offset; 
}

/*************************************************************
Purpose: This function make complete header frame . 
Header frame consists of : 
1) 9 byte Frame Header 
2) Frame payload 
Frame Payload consists of Prority , Padding and header payload .

Input : Compressed header (compressed using nghttp2)
        Compressed header length. 
        
Output : amount of bytes written . 
**************************************************************/
int pack_header(connection *cptr, unsigned char *header_fragment, unsigned int header_fragment_len, char padding, unsigned char pad_length, stream* stream_ptr, int content_length)
{
  unsigned int amt_used = 0; 
  unsigned int tot_length = 0;
  unsigned char flag = 0; 
  unsigned int end_header = 1; 
  unsigned int end_stream = 1; 
  
  NSDL2_HTTP2(NULL, NULL, "Method Called");
  // Check Whether request is POST or get . In case of POST METHOD end_stream flag = 0 , as futher frames are expected on stream 
  if ((cptr->url_num->proto.http.http_method == HTTP_METHOD_POST || cptr->url_num->proto.http.http_method == HTTP_METHOD_PUT ||
     cptr->url_num->proto.http.http_method == HTTP_METHOD_PATCH || (cptr->url_num->proto.http.http_method == HTTP_METHOD_GET && 
     cptr->url_num->proto.http.post_ptr)) && content_length) {
     end_stream = 0; 
  }  
  // This is total length of header frame 
  tot_length = header_fragment_len + pad_length;  
  
  NSDL2_HTTP2(NULL, NULL, "stream_ptr->stream_id is %d and end_stream is [%d]", stream_ptr->stream_id, end_stream);
  amt_used = pack_frame(cptr, header_fragment, tot_length, HEADER_FRAME, end_stream, end_header, stream_ptr->stream_id, padding, &flag);
  NSDL2_HTTP2(NULL, NULL, "amt_used  is %d", amt_used);
  if (amt_used <= 0)
  {
   NSDL2_HTTP2(NULL, NULL, "Error in writing frame data");
   return -1; 
  }
  return (amt_used + header_fragment_len); 
}

/**********************************************************************
 Purpose : This function fills in payload for goaway frame .  
 Input   : Goaway frame , Sid , Error code and debug_data 
           Debug_data is optional .  
    +-+-------------------------------------------------------------+
    |R|                  Last-Stream-ID (31)                        |
    +-+-------------------------------------------------------------+
    |                      Error Code (32)                          |
    +---------------------------------------------------------------+
    |                  Additional Debug Data (*)                    |
    +---------------------------------------------------------------+

Output : Returns amount written 
***********************************************************************/
int make_goaway_frame(unsigned char *goaway_frame, unsigned int sid, int error_code, char *debug_data)
{
  unsigned char stream_info[4] = {0};
  int frame_len = 0;

  NSDL2_HTTP2(NULL, NULL, "Method Called");

  stream_info[3] |= 0 << 7;
  MY_MEMCPY(stream_info, &sid, 1);

 // Copying stream info to the goaway_frame
  MY_MEMCPY(goaway_frame, stream_info, 4);
  frame_len += 4;

 //Copying error code to the goaway frame
  MY_MEMCPY(goaway_frame + frame_len, &error_code, 4);
  frame_len += 4;

 // Copying errors of respective error codes to debug data;
  if (debug_data != NULL) {
    MY_MEMCPY(goaway_frame + frame_len, debug_data, strlen(debug_data));
    frame_len += strlen(debug_data);
  }
  return frame_len;
}

/*************************************************************
Purpose: This function make complete goaway frame .
         Goaway frame close(s) connection gracefully . 
         Goaway frame is send to peer when received irrelevant data 
         like invalid stream_id for settings frame . Data size to large etc.
         

Goaway frame consists of : 
1) 9 byte Frame Header 
2) Payload 
Frame Payload consists of Last_processed_stream_id, Error code .  Debug data in payload is optional . 

Input : Connection cptr, errorcode , sid , debug_data(optional) , now . 
Output : Amount of bytes written . 
**************************************************************/
int pack_goaway(connection *cptr, unsigned int stream_identifier, int error_code, char *debug_data, u_ns_ts_t now)
{
  unsigned char flag = 0; 
  unsigned char *goaway_frame = NULL;
  int amt_used = 0; 
  int len = 0;  
  int frame_len; 
  
  //Calculate frame length
  if (debug_data)
    frame_len = 8 + strlen(debug_data);
  NSDL2_HTTP2(NULL, NULL, "Method called frame_length %d", frame_len);
  
  MY_MALLOC_AND_MEMSET(goaway_frame, (frame_len + 9), "goaway_frame", -1);
  amt_used = pack_frame(cptr, goaway_frame, frame_len, GOAWAY_FRAME, 0, 0, 0, 0, &flag);
 
  /*bug 78144 : return error if cptr->http2 is not available*/ 
  if( amt_used == HTTP2_ERROR)  
    return HTTP2_ERROR;  
 
  if (amt_used == 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing frame header for goaway frame");
  } 
  len = make_goaway_frame(goaway_frame + amt_used, stream_identifier, error_code, debug_data);
  if (len == 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing goaway frame"); 
  } 

  // Check if it should be done before or after closing conn
  cptr->http2->continuation_buf = NULL; 
  cptr->http2->continuation_buf_len = 0;
  cptr->free_array = (char *)goaway_frame;                                   
  cptr->bytes_left_to_send = (len + amt_used);                         
  http2_handle_write(cptr, now); 
  return HTTP2_ERROR; 
}

/**********************************************************************
  
Purpose: This function make complete reset frame .
         Reset frame close(s) stream gracefully . 
         In case of reset stream  
         

RESET frame consists of : 
1) 4 byte Error Code. 

Input : Connection cptr, errorcode . 
Output : Amount of bytes written . 
**********************************************************************/
int pack_reset(connection *cptr, stream *sptr, unsigned char *reset_frame, int error_code)
{ 
  //unsigned char reset_frame[64] = "";
  unsigned char flag = 0;  
  unsigned int amt_used = 0;
  
  NSDL2_HTTP2(NULL, NULL, "Method Called");
  
  amt_used = pack_frame(cptr, reset_frame, RESET_FRAME_LEN, RESET_FRAME, 0, 0, sptr->stream_id, 0, &flag);
  if (amt_used == -1) 
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing frame header for reset frame"); 
  }
 
  error_code = ntohl(error_code); 
  HTONL_4BYTE((reset_frame + amt_used), error_code);

  // Free Stream  and from map
  // TODO:check if stream ptr is NULL
  if (sptr) {
    RELEASE_STREAM(cptr, sptr);
  }
   
  return 0;
} 

int pack_window_update_frame(connection *cptr, unsigned char *window_update , int window_update_value , unsigned int stream_id){
  int amt_written = 0;
  int tot_length = 0;
  unsigned char *tmp = NULL; 
  unsigned char flag = 0;

  NSDL2_HTTP2(NULL, NULL, "Method called"); 
 
  amt_written = pack_frame(cptr, window_update, WINDOW_UPDATE_LEN, WINDOW_UPDATE_FRAME, 0, 0, stream_id, 0, &flag);
  NSDL2_HTTP2(NULL, NULL, "amt_written = %d", amt_written);
  if (amt_written < 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing frame header for settings frame");
    return HTTP2_ERROR; 
  }
 
  tmp = window_update + amt_written;
  window_update_value = ntohl(window_update_value);
  HTONL_4BYTE(tmp, window_update_value); 
  tot_length = amt_written + WINDOW_UPDATE_LEN; 
 
  NSDL2_HTTP2(NULL, NULL, "tot_length = %d", tot_length);
  return tot_length; 
}

/*bug 93672: gRPC: argument ack_flag added, so that frame can be created accordingly*/
/**********************************************************************
Purpose: This function make ping frame.
         9 bytes header + payload . 
         Ping frame payload consists of data received from peer. 

**********************************************************************/
int make_ping_frame(connection *cptr, unsigned char *data, unsigned char *ping_frame, int ack_flag) {

  unsigned int amt_written = 0;
  unsigned char flag = 0;

  NSDL2_HTTP2(NULL, NULL, "Method Called");
  amt_written = pack_frame(cptr, ping_frame, PING_FRAME_LENGTH, PING_FRAME, ack_flag, 0, 0x0, 0, &flag);
  if (amt_written < 0)
  {
    NSDL2_HTTP2(NULL, NULL, "Error in writing frame header for Ping Frame");
    return HTTP2_ERROR;
  }
  NSDL2_HTTP2(NULL, NULL, "amt_written is [%d] ", amt_written);

  // Copy data to Ping Frame
  if (data != NULL) { 
    MY_MEMCPY(ping_frame + amt_written, data, PING_FRAME_LENGTH);
  }
  return (amt_written + PING_FRAME_LENGTH);
}

void release_frame(data_frame_hdr* frame_chunk)
{
  NSDL2_HTTP2(NULL, NULL, "Method called. cptr frame_chunk=%p",frame_chunk);
  if(!frame_chunk) /*bug 84661*/
   return;
  total_free_dframes++;
  NSDL2_HTTP2(NULL, NULL, "Increment free frame counter: total_free_frames = %lu", total_free_dframes);
  NSDL2_HTTP2(NULL, NULL, "Last frame entry, gFreeFrameTail = %p", gFreeDatafrTail);
  if(gFreeDatafrTail)
  {
    gFreeDatafrTail->next = (struct data_frame_hdr*)frame_chunk;
  } else {
    NSDL2_HTTP2(NULL, NULL, "Something went wrong while freeing frame pool");
    gFreeDatafrHead = frame_chunk;
  }
  gFreeDatafrTail = frame_chunk;
  frame_chunk->next = NULL;
  NSDL2_HTTP2(NULL, NULL, "Free frame available, gFreeDatafrTail = %p", gFreeDatafrTail);

}
