#include "ns_h2_header_files.h"

static inline int ns_send_window_update(connection *cptr , stream *sptr ,u_ns_ts_t now)
{
  unsigned char *window_update_frame;
  Http2FlowControl *fptr;
  unsigned int stream_id;
  int window_update_len;

  NSDL1_HTTP2(NULL, NULL, "Method called");

  MY_MALLOC_AND_MEMSET(window_update_frame, NS_HTTP2_WINDOW_UPDATE_FRAME_LEN, "window_update_frame", -1);

  if(!sptr)
  {
     NSDL2_HTTP2(NULL, cptr, "Send WINDOW UPDATE on Connection");
     stream_id = 0;
     fptr = &cptr->http2->flow_control;
  }
  else
  {
    NSDL2_HTTP2(NULL, cptr, "Send WINDOW UPDATE on Stream id = %d", sptr->stream_id);
    stream_id = sptr->stream_id;
    fptr = &sptr->flow_control;
  }
  window_update_len = pack_window_update_frame(cptr, window_update_frame, fptr->received_data_size, stream_id);
  if(window_update_len < 0)
  {
     FREE_AND_MAKE_NULL(window_update_frame, "window_update_frame", -1);
     return HTTP2_ERROR;
  }
   fptr->received_data_size = 0;
   
   cptr->free_array = (char*)window_update_frame;
   cptr->bytes_left_to_send = window_update_len;
   cptr->req_code_filled = 0;
   cptr->conn_state = CNST_HTTP2_WRITING;
   cptr->http2_state = HTTP2_SEND_WINDOW_UPDATE;
   return http2_handle_write(cptr, now);
 }
 
 int ns_process_flow_control_frame(connection *cptr, stream *sptr, int size, u_ns_ts_t now)
 { 
   Http2FlowControl *fptr;
   int rc;
  
   NSDL1_HTTP2(NULL, NULL, "Method called");
 
   /*Connection Flow Control*/
   fptr = &cptr->http2->flow_control;
   fptr->received_data_size += size;
   NSDL2_HTTP2(NULL, cptr, "Connection received_data_size %d , local_window_size %d",fptr->received_data_size,fptr->local_window_size);
   if(fptr->received_data_size >= fptr->local_window_size/2)
   {
     rc = ns_send_window_update(cptr,NULL,now);
     if(rc < 0)
     {
       NSDL2_HTTP2(NULL, cptr, "Failed to send WINDOW UPDATE on connection");
       return rc;
     }
   }
 
   /*Stream Flow Control*/
   if(sptr)
   {
     fptr = &sptr->flow_control;
     fptr->received_data_size += size;
     NSDL2_HTTP2(NULL, cptr, "Stream received_data_size %d , local_window_size %d",fptr->received_data_size,fptr->local_window_size);
     if(fptr->received_data_size >= fptr->local_window_size/2)
     {
       rc = ns_send_window_update(cptr,sptr,now);
       if(rc < 0)
       {
         NSDL2_HTTP2(NULL, cptr, "Failed to send WINDOW UPDATE on stream");
         return rc;
       }
     }
   }
   return rc;
 }
