/*********************************************************************************************
* Name                   : ns_h2_stream.c  
* Purpose                : This C file holds the function(s) required for stream allocation and  .
                           Also this process frame  
* Author                 : Meenakshi Sachdeva/Anubhav 
* Intial version date    : 6-October-2016 
* Last modification date : 20-October-2016 
*********************************************************************************************/

/*
 Stream transition supported in NS

                                           idle
                                             |
                                             | s/r H
					     |
                                  r ES       V      s ES
         half closed (remote) <----------- open ---------> half closed (local) 
		|		             |                      |              
                |                            | s/r R                |                           
                |                            |			    |
                |       s ES                 V	     r ES	    |
                \---------------------->  closed <------------------/
                       s/r R                        s/r R
H  :- HEADERS frame
ES :- END STREAM flag
R  :- RESET STREAM frame
s  :- endpoint sends this frame
r  :- endpoint receives this frame

We are not supporting push promise frames hence state reserved_l and reserved_r will not b there
                                                                                                */
#include "ns_h2_header_files.h"

/*Global Stream pointer*/
stream* gFreeStreamHead = NULL;
stream* gFreeStreamTail = NULL;

static unsigned long total_allocated_sptrs = 0;
static unsigned long total_free_sptrs = 0;

// Purpose :- This function allocate a stream pool of chunk 1024 and creates a link list of the nodes by setting their next 
static stream* allocate_stream_pool()
{
  int i;
  stream* streams_chunk = NULL;
  int stream_size;

  stream_size = INIT_STREAM_BUFFER * sizeof(stream);
  NSDL2_HTTP2(NULL, NULL, "Method called. Sizeof stream structure = %lu. Allocating stream pool %d streams and size of %lu bytes", sizeof(stream), INIT_STREAM_BUFFER, stream_size);

  MY_MALLOC_AND_MEMSET(streams_chunk, stream_size, "Allocating stream pool", -1);

  // Not doing memset as only two fields need to be set to NULL
  //Counters to keep record of free and allocated streams
  total_free_sptrs += INIT_STREAM_BUFFER;
  total_allocated_sptrs += INIT_STREAM_BUFFER;
  NSDL2_HTTP2(NULL, NULL, "Total free streams: total_free_sptrs = %lu, total allocated streams: total_allocated_sptrs = %lu", total_free_sptrs, total_allocated_sptrs);

  for(i = 0; i < INIT_STREAM_BUFFER; i++)
  {
    /* Linking stream entries within a pool and making last entry NULL*/
    if(i < (INIT_STREAM_BUFFER - 1)) {
      streams_chunk[i].next = (struct stream *)&streams_chunk[i + 1];
    }
    streams_chunk[i].state = NS_H2_ON_FREE_LIST;
  }

  // total_conn_list_tail->next_in_list = NULL; commented as it is done in memset
  gFreeStreamTail = &streams_chunk[INIT_STREAM_BUFFER - 1];
  NSDL2_HTTP2(NULL, NULL, "streams_chunk = %p", streams_chunk);

  return(streams_chunk);
}

//This function checks the state of sptr and unsets the inuse state if it is set
inline void remove_ifon_stream_inuse_list(stream* sptr)
{
  if (!(sptr->state & NS_H2_ON_INUSE_LIST)) {
    NSDL2_HTTP2(NULL, NULL, "Stream slot %p is not on stream inuse list of the Vuser.", sptr);
  }
  else {
    NSDL2_HTTP2(NULL, NULL, "Removing sptr from inuse stream link list");
    sptr->state &= ~NS_H2_ON_INUSE_LIST;
  }
  NSDL2_HTTP2(NULL, NULL, "Exiting remove_ifon_stream_inuse_list");
}

/*
inline void free_stream_body_buffer(stream *sptr){

  struct copy_buffer* curr = sptr->buf_head;
  struct copy_buffer* temp;
  NSDL2_HTTP2(NULL, NULL, "Method called stream_id = %d sptr->buf_head = %p", sptr->stream_id, sptr->buf_head);
  while(curr){
    temp = curr;
    curr = curr->next;
    FREE_AND_MAKE_NULL(temp, "sptr->cur_buf", -1);
  }
}
*/
//This function takes a stream ptr as an input and removes it from the inuse list of sptr and adds it up to the free list tail
void release_stream(connection *cptr, stream * sptr)
{
  NSDL2_HTTP2(NULL, NULL, "Method called. cptr = %p", cptr);

  /*
  if (sptr->state & NS_H2_ON_FREE_LIST)
  {
    NSDL2_HTTP2(NULL, NULL, "free_stream_slot() called for stream (%p) which is already in stream free list. Ignored", sptr);
    return;
  }
  */
  //Update free counter of stream slot
  total_free_sptrs++;
  cptr->http2->total_open_streams--;
  NSDL2_HTTP2(NULL, NULL, "Increment free stream counter: total_free_sptrs = %lu, total_allocated_sptrs = %lu,"
			" cptr->http2->total_open_streams = %u", total_free_sptrs, total_allocated_sptrs, cptr->http2->total_open_streams);

  remove_ifon_stream_inuse_list(sptr);
  /*add free sptr at head of stream pool*/
  NSDL2_HTTP2(NULL, NULL, "Last stream entry, gFreeStreamTail = %p", gFreeStreamTail);

  if(gFreeStreamTail)
  {
    gFreeStreamTail->next = (struct stream *)sptr;
  } else {
    NSDL2_HTTP2(NULL, NULL, "Something went wrong while freeing stream pool");
    gFreeStreamHead = sptr;
  }

  gFreeStreamTail = (stream *)sptr;
  /*if(sptr->stream_id >1){
      free_stream_body_buffer(sptr); // Freeing data buffers 
  }*/
  memset(sptr, 0, sizeof(stream));
  sptr->next = NULL;
  NSDL2_HTTP2(NULL, NULL, "Free Stream available, gFreeStreamTail = %p", gFreeStreamTail);
}

//This function is used to take a new stream for a connection
stream* get_free_stream(connection *cptr, int *error_code) 
{
  stream *free;
  VUser *vptr = (VUser *)cptr->vptr;
 
  NSDL2_HTTP2(NULL, NULL, "Method called");

  if(cptr->http2->total_open_streams < (cptr->http2->settings_frame.settings_max_concurrent_streams)) {
    if(cptr->http2->max_stream_id < MAX_VALUE_FOR_SID - 2) {
      cptr->http2->max_stream_id += 2;                     
      cptr->http2->total_open_streams++;
      NSDL2_HTTP2(NULL, NULL, "stream_id %d , free->max_stream_id %d, cptr->http2->total_open_streams = %u", 
					cptr->http2->max_stream_id, cptr->http2->max_stream_id, cptr->http2->total_open_streams);
    } else {
      NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
           __FILE__, (char*)__FUNCTION__,
           "Max Value for Stream Identifier reached, Hence closing connection form here as it can not go further." 
           "Ns will try to send this request using retry connection. max_stream_id = [%u] ", cptr->http2->max_stream_id);

      *error_code = NS_STREAM_ERROR; 
      return NULL;
    }
  }                                      
  else {   
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
           __FILE__, (char*)__FUNCTION__,
           "Max open streams reached max_concurrent_streams, Hence closing connection form here as it can not go further." 
           "Ns will try to send this request using retry connection. total_open_stream = [%u], max-concurrent_streams [%u]  ", 
            cptr->http2->total_open_streams, cptr->http2->settings_frame.settings_max_concurrent_streams ); 
    *error_code = NS_STREAM_ERROR;             
    return NULL;
  }
  /* If gFreeVuserHead is NULL then realloc stream pool otherwise draw stream from existing pool*/
  if (gFreeStreamHead == NULL)
  {
    NSDL2_HTTP2(NULL, NULL, "Allocating stream pool");
    gFreeStreamHead = allocate_stream_pool();
    if(gFreeStreamHead == NULL) // If we are not able to allocate stream pool
    {
      fprintf(stderr, "Stream allocation failed\n");
      // exit(-1);  // Instead of exit we will return NULL here
      *error_code = NS_STREAM_ERROR_NULL; 
      return NULL; 
    }
  }

  /* Stream Pool Design: Traverse stream list and update gFreeStreamHead and update
   * vptr pointer of stream struct*/
  free = gFreeStreamHead;
  gFreeStreamHead = (stream*)free->next; // Move free head to next sptr on pool

  if(gFreeStreamHead == NULL) // If we took the last sptr of the pool, then both head and tail need to be set to NULL
  {
    NSDL2_HTTP2(NULL, NULL, "Last stream fetch from pool, then both head and tail need to be set to NULL");
    gFreeStreamTail = NULL;
  }

  //Update free counter for stream slot
  total_free_sptrs--;
  NSDL2_HTTP2(NULL, NULL, "Updated free stream counter: total_free_sptrs = %lu, total_allocated_sptrs = %lu", total_free_sptrs, total_allocated_sptrs);

  free->next = NULL; // Set next to NULL. May not be important ??
  free->state = NS_H2_ON_INUSE_LIST; 
  
  // save stream id to sptr
  free->q_next = NULL;
  free->is_cptr_data_saved = 0;
  free->stream_id = cptr->http2->max_stream_id;
  free->flow_control.local_window_size = runprof_table_shr_mem[vptr->group_num].gset.http2_settings.initial_window_size;
  //free->flow_control.remote_window_size = cptr->http2->settings_frame.settings_initial_window_size;
  //free->flow_control.remote_window_size = 65536;
  
  /*bug 84661 ToDo use MACRO below  update remote_window_size to default value i.e 65535*/
  free->flow_control.remote_window_size = DEFAULT_FRAME_SIZE;
  //free->flow_control.remote_window_size = 49143; //flow control window set to 75% (65536 - 16384 -9)
  free->flow_control.received_data_size = 0;
  /*bug 84661: init  window_update_from_server with default value*/
  free->window_update_from_server = DEFAULT_FRAME_SIZE; 
  NSDL2_HTTP2(NULL, NULL, "Exiting get_free_stream_slot %p, stream_id = %u ",free, free->stream_id);
  return free;
}

//Code for state transition while parsing response
/*
static inline void parse_response_on_stream(stream *sptr, unsigned char *buf)
{
  switch(sptr->state)
  {
    case NS_H2_IDLE:
      // recieve headers
      if(header_frame)
        sptr->state = NS_H2_OPEN;
      // recieve priority
      else if(priority_frame)
        NSDL2_HTTP2(NULL, NULL, "No state change: These are used to reprioritize streams that depend on the identified stream");
      else
        fprintf(NULL, NULL, "Connection Error: So sending GOAWAY frame and terminating the connection\n");
//        http_close_connection();
      break;

    case NS_H2_OPEN:
      if(ES_flag) 
        sptr->state = NS_H2_HALF_CLOSED_R;
      else if(reset_frame)
        sptr->state = NS_H2_CLOSED;  
      else {
        fprintf(NULL, NULL, "Connection Error: of type PROTOCOL_ERROR\n");
        http_close_connection();
      }
      break;

    case NS_H2_HALF_CLOSED_L:
      if(ES_flag || reset_frame)
        sptr->state = NS_H2_CLOSED;
      else if(priority_frame)
        NSDL2_HTTP2(NULL, NULL, "No state change: These are used to reprioritize streams that depend on the identified stream");
      else {
        NSDL2_HTTP2(NULL, NULL, "Connection Error: of type PROTOCOL_ERROR\n");
        http_close_connection();
      }
      break;

    case NS_H2_HALF_CLOSED_R: 
      if(reset_frame)
        sptr->state = NS_H2_CLOSED;
      else if(priority_frame)
        NSDL2_HTTP2(NULL, NULL, "No state change: These are used to reprioritize streams that depend on the identified stream");
      else if(window_update_frame)
        NSDL2_HTTP2(NULL, NULL, "window update frame found"); 
      else {
        NSDL2_HTTP2(NULL, NULL, "Stream Error: of type STREAM_CLOSED\n");
        http_close_connection();
      }
      break;

    case NS_H2_CLOSED:
      if (priority_frame)
        NSDL2_HTTP2(NULL, NULL, "No state change: These are used to reprioritize streams that depend on the identified stream");
      else
        NSDL2_HTTP2(NULL, NULL, "Stream Error: of type STREAM_CLOSED\n");
      break;

    default:
       NSDL2_HTTP2(NULL, NULL, "Unknown stream state\n");
  }  
} 
*/
/*
int main()
{
  stream *sid, *sid1;

  sid = get_free_stream();
  sid1 = get_free_stream();
  NSDL2_HTTP2(NULL, NULL, "sid for s is %d, s1 is %d\n\n", sid->stream_id, sid1->stream_id); 
  release_stream(sid1); 
  release_stream(sid); 

return 0;
}
*/

/*ANUBHAV

Code that follows manages conflicting stream ids as the stream identifier of a newly established stream MUST be numerically greater than all  streams that the initiating endpoint has opened. So we are maintaining a data structure of twice the size of Max_Concurrent_Streams, first is the primary for storing the stream ids & ptrs directly if free and the secondary for mapping the conflicting streams. Next_slot is used for mapping of primary to secondary.A queue of free indexes in secondary gives the free_slot in secondary map.

 Structure of Map:

	idx _|_ Stream_id _|_ Next_slot _|_ Stream_ptr
	     |             |             |
	     |             |             |
	     |             |             |
	     |             |             |

*/



// This method will initialize queue of free indexes in secondary array(map) and will be called just once for one cptr
void init_stream_queue(NsH2StreamQueue **stream_queue1, int max_concurrent_stream)
{
  int i, j;
  NSDL2_HTTP2(NULL, NULL, "Method called, max_concurrent_stream = %d", max_concurrent_stream);
 
 NsH2StreamQueue *stream_queue = NULL;
 MY_MALLOC(stream_queue, (sizeof(NsH2StreamQueue)), "cptr->http2->cur_map_queue", -1); 
 MY_MALLOC(stream_queue->ns_h2_slot, (sizeof(int) * max_concurrent_stream), "cptr->http2->cur_map_queue->ns_h2_slot", -1);
  
  // Init stream_queue.ns_h2_slot as all slots are free at init time
  for(i = 0, j = max_concurrent_stream; i < max_concurrent_stream ; i++, j++){
    stream_queue->ns_h2_slot[i] = j;
  }
  stream_queue->front = 0;
  stream_queue->rear = max_concurrent_stream - 1;
  stream_queue->ns_h2_max_slot = stream_queue->ns_h2_available_slot = max_concurrent_stream; 
  *stream_queue1 = stream_queue;
}

// This method will add index of the stream freed(secondary array(map)) to the queue.
int ns_h2_add_slot(NsH2StreamQueue *stream_queue, int slot_value)
{
  NSDL2_HTTP2(NULL, NULL, "Method called");
  if(stream_queue->ns_h2_available_slot < stream_queue->ns_h2_max_slot)
  {
    // Rear will move crcular as it reaches at the end of ns_h2_slot, In case queue rear is less then ns_h2_max_slot modulus will not impact
    // queue rear 
    stream_queue->rear = (stream_queue->rear +1) % stream_queue->ns_h2_max_slot;
    stream_queue->ns_h2_slot[stream_queue->rear] = slot_value;
    stream_queue->ns_h2_available_slot++;
    NSDL2_HTTP2(NULL, NULL, "rear = %d ns_h2_available_slot = %d",stream_queue->rear, stream_queue->ns_h2_available_slot);    
    return 0;
  }
  else  {
    // Add event log
    NSDL2_HTTP2(NULL, NULL, "ns_h2_add_slot failed. ns_h2_available_slot [%d] is equal or gretaer than ns_h2_max_slot [%d]." 
		"This should not happen", stream_queue->ns_h2_available_slot, stream_queue->ns_h2_max_slot);
    return -1;
  }
}

// It will return the free slot/index in the secondary array maintained by queue. 
int ns_h2_get_free_slot(NsH2StreamQueue *stream_queue)
{ 
  int free_slot;
  NSDL2_HTTP2(NULL, NULL, "Method called. ns_h2_available_slot = %d", stream_queue->ns_h2_available_slot);

  if(stream_queue->ns_h2_available_slot) {
    free_slot = stream_queue->ns_h2_slot[stream_queue->front];
    stream_queue->front = (stream_queue->front + 1) % stream_queue->ns_h2_max_slot;
    stream_queue->ns_h2_available_slot--;
    NSDL2_HTTP2(NULL, NULL, "front = %d ns_h2_available_slot = %d free_slot = %d", 
					stream_queue->front, stream_queue->ns_h2_available_slot, free_slot);	
    return free_slot;
  }
  else  {  
    NSDL2_HTTP2(NULL, NULL, "ns_h2_get_free_slot failed. ns_h2_available_slot is 0."); 
    return -1; 
  }
}

//Initialize the map for storing the stream id, next_slot in case not free, and stream pointer
void init_stream_map(NsH2StreamMap **map1, int max_concurrent_stream, int *available_streams)
{
  int i;

  NSDL2_HTTP2(NULL, NULL, "Method called. max_concurrent_stream = %d cptr->http2->cur_stream_map=%p", max_concurrent_stream, *map1);	
  /*bug 52092 : init only when memory is NULL, else not, in case of PROXY we already doing init during CONNECT*/
  if (*map1)
    return;
  // We are allocating four times of max concurrent streams because client uses odd no streams only, so we need array of
  // 2*max_concurrent_stream and we allocate 2*max_concurrent_stream for conflict handling 
  
  NsH2StreamMap *map = NULL;
  MY_MALLOC(map , (sizeof(NsH2StreamMap) * max_concurrent_stream * 2), "cptr->http2->cur_stream_map", -1);

  for(i = 0; i < (2 * max_concurrent_stream); i++)
  {
    (map + i)->stream_id = -1;
    (map + i)->next_slot = -1;	
    (map + i)->stream_ptr = NULL;
  }
  *available_streams = max_concurrent_stream;
  *map1 = map;
}

// Getter for the stream
stream *get_stream(connection *cptr, unsigned int strm_id)
{
   
   NSDL2_HTTP2(NULL, NULL, "Method called. cptr->http2 = %p",cptr->http2);
   if (cptr->http2 == NULL) /* bug 78135 avoid crash if cptr->http2 == null*/
     return NULL;
  NsH2StreamMap *map = cptr->http2->cur_stream_map;
  int max_concurrent_stream = cptr->http2->settings_frame.settings_max_concurrent_streams * 2;
  int index_value;
  int i;
  index_value = strm_id % max_concurrent_stream;
 
  NSDL2_HTTP2(NULL, NULL, " cptr=%p http2=%p map=%p max_concurrent_stream = %d, index_value = %d, stream_id = %u",cptr, cptr->http2,map,
				 max_concurrent_stream, index_value, strm_id);
 
  //Stream is present in primary array return index in primary array
  if (map) {
    if ((map + index_value)->stream_id == strm_id) {
      NSDL2_HTTP2(NULL, NULL, "Stream id found in Primary array");	
      return (map + index_value)->stream_ptr;
    }
  } else {
    NSDL2_HTTP2(NULL, NULL, "Map not found . Hence returning");
    return NULL; 
  }
  
 
  NSDL2_HTTP2(NULL, NULL, "(map + index_value)->stream_id = %u", (map + index_value)->stream_id);
 
  i = index_value;

  // If stream id not found search in secondary array
  while((map + i)->stream_id != strm_id  && i < (2 * max_concurrent_stream))
  {
    i = (map + i)->next_slot;
    if (i == -1) {
      NSDL2_HTTP2(NULL, NULL, "Stream not available");	
      return NULL; 
    } 
  }
  
  NSDL2_HTTP2(NULL, NULL, "index at which stream_id = %d was indexed = %d",strm_id, i);

  if(i < 2 * max_concurrent_stream)
    return (map + i)->stream_ptr;
  else
    return NULL;
}

//deletes the stream from map and updates queue of indexes of secondary array
void delete_stream_from_map(connection *cptr, stream * sptr)
{
  NsH2StreamMap *map = cptr->http2->cur_stream_map;
  NsH2StreamQueue *stream_queue = cptr->http2->cur_map_queue;
  int strm_id = sptr->stream_id;
  int max_concurrent_stream = cptr->http2->settings_frame.settings_max_concurrent_streams * 2;
  int index_value;
  int i, next_slot_prev;

  NSDL2_HTTP2(NULL, NULL, "Method called");

  index_value = strm_id % max_concurrent_stream;
  NSDL2_HTTP2(NULL, NULL, "index_value = %d",index_value);
  
  // if stream found in primary array mark map index as free and return form here
  if((map + index_value)->stream_id == strm_id)
  {
    (map + index_value)->stream_id =- 1;
    (map + index_value)->stream_ptr = NULL;
    cptr->http2->available_streams += 1;
    NSDL2_HTTP2(NULL, NULL, "available_steams = %d", cptr->http2->available_streams);
    return;
  }
 
  // when stream id was mapped to secondary array called queue, in this case we will traverse all the indexes which have same index_value until
  // we dont find a match for stream id

  // get next index 
  i = (map + index_value)->next_slot;
  if(i == -1)
  {
    NSDL2_HTTP2(NULL, NULL, "Error case, Cannot delete as stream for stream id [%d] as it is not in primary Map and next is set -1. This shoud not happen", strm_id);
    return;
  }
  next_slot_prev = index_value;
  while((map + i)->stream_id != strm_id && i < 2 * max_concurrent_stream)
  {
    next_slot_prev = i;
    i = (map + i)->next_slot;
    if(i == -1)
    {
      NSDL2_HTTP2(NULL, NULL, "Error case, Strean id %d not found in Map. next slot became -1, This shoud not happen", strm_id);
      return;
    }
  }

  if(i == 2* max_concurrent_stream){
    NSDL2_HTTP2(NULL, NULL, "Error case, Strean id %d not found in primary and secondary map This shoud not happen", strm_id);
    return;
  }
 
  (map + next_slot_prev)->next_slot = (map + i)->next_slot;
  (map + i)->stream_ptr = NULL;
  (map + i)->stream_id = -1;
  (map + i)->next_slot = -1;
  ns_h2_add_slot(stream_queue, i);
  cptr->http2->available_streams += 1;
  NSDL2_HTTP2(NULL, NULL, "available_steams = %d", cptr->http2->available_streams);
  return ;
}

//Fills the stream into map and maintains the secondary array and queue accordingly in case of conflicting stream_ids

 //void ns_h2_fill_stream_into_map(NsH2StreamMap *map, NsH2StreamQueue *stream_queue, stream *strm_ptr, int strm_id, 
							// int max_concurrent_stream, int *available_streams)
int ns_h2_fill_stream_into_map(connection *cptr, stream *strm_ptr)
{
  NsH2StreamMap *map = cptr->http2->cur_stream_map;
  NsH2StreamQueue *stream_queue = cptr->http2->cur_map_queue; 
  unsigned int strm_id  = strm_ptr->stream_id;
  //int max_concurrent_stream = cptr->http2->settings_frame.settings_max_concurrent_streams *2;
  int max_concurrent_stream = cptr->http2->settings_frame.settings_map_size;
  int *available_streams = &cptr->http2->available_streams;
  int index_value;
  int next_free_slot,i;

  NSDL2_HTTP2(NULL, NULL, "Method called. strm_id = %d, max_concurrent_stream = %d", strm_id, max_concurrent_stream);
  // ??	
  if(*available_streams == 0) {
    NSDL2_HTTP2(NULL, NULL, "Avaliable streams is 0");
    return HTTP2_ERROR;
  }
  
  index_value = strm_id % max_concurrent_stream;
  NSDL2_HTTP2(NULL, NULL, "index_value = %d", index_value);

  // Check if stream index is free in primary array and fill it.	
  if((map + index_value)->stream_ptr == NULL) 
  {	
    (map + index_value)->stream_id = strm_id; // Fill stream id
    (map + index_value)->stream_ptr = strm_ptr; // Fill stream ptr 
    *available_streams -= 1; // ??
    NSDL2_HTTP2(NULL, NULL, " cptr=%p, cptr->htt2=%p ,map=%p stream id = %d stream_ptr = %p available_streams = %d", cptr, cptr->http2, map,
                            (map + index_value)->stream_id, (map + index_value)->stream_ptr,*available_streams);
    return HTTP2_SUCCESS;
  }
 
  // array index at index_value is not free, find index in queue and fill stream in secondary array.
  i = index_value;
  next_free_slot = ns_h2_get_free_slot(stream_queue);
  if(next_free_slot == -1){
    NSDL2_HTTP2(NULL, NULL, "next_free_slot = %d", index_value);
    release_stream(cptr, strm_ptr);
    return HTTP2_ERROR;		
  }
  (map + next_free_slot)->stream_ptr = strm_ptr;
  (map + next_free_slot)->stream_id = strm_id;	
  
  // Here we are setting next_slot in primary array index at index_value. We are using loop because next of primary array index
  // may be already used by some other stream id, in this case we need to find the last node where next is -1
  while((map + i)->next_slot != -1)
  {	
    i = (map + i)->next_slot;
  }
  (map + i)->next_slot = next_free_slot;
  *available_streams -= 1;

  NSDL2_HTTP2(NULL, NULL, " previous index = %d next_free_slot = %d stream id = %d stream_ptr = %p available_streams = %d", i, (map+index_value)->stream_id, (map + index_value)->stream_ptr, *available_streams);
 
  return HTTP2_SUCCESS;		
}

void add_node_to_queue(connection *cptr, stream *node){
  NSDL2_HTTP2(NULL, NULL, "Method Called,node=%p cptr->http2->front=%p cptr->http2->rear=%p", node, cptr->http2->front, cptr->http2->rear);

  if(!cptr->http2->front){
    cptr->http2->front = node;
    cptr->http2->rear = node;
    NSDL2_HTTP2(NULL, NULL, "Adding node at front");
    return;
  } else if (node != cptr->http2->front) { /*bug 79062 add node  only if not added already*/
    NSDL2_HTTP2(NULL, NULL, "Adding node at rear");
    cptr->http2->rear->q_next = (struct stream *)node;
    cptr->http2->rear = node; 
  }
  else {
    NSDL2_HTTP2(NULL, NULL, "node not added, either it is equal to front or rear node"); /*bug 79062 else block added*/
  }
  NSDL2_HTTP2(NULL, NULL, "after add node=%p cptr->http2->front=%p cptr->http2->rear=%p", node, cptr->http2->front, cptr->http2->rear);
}

// Function to delete node from front of a queue 
int delete_node_from_queue(connection *cptr) {
  stream *tmp_node;  // This node is temp points to front
  NSDL2_HTTP2(NULL, NULL, "Methd called,cptr->http2->front=%p",cptr->http2->front);  
  // Check whether queue exists or not  
  if (cptr->http2->front == NULL){
    NSDL2_HTTP2(NULL, cptr, "Queue does not exists");
    return HTTP2_ERROR;
  }

  // Delete front node and move front to its next
  tmp_node = cptr->http2->front;
  cptr->http2->front = (struct stream *)(tmp_node->q_next);
  //cptr->http2->front = tmp_node->q_next;
  //FREE_AND_MAKE_NULL(tmp_node, "cptr->http2->front", -1);
  NSDL2_HTTP2(NULL, NULL, "after delete  cptr->http2->front=%p",cptr->http2->front);
  return HTTP2_SUCCESS;
}
