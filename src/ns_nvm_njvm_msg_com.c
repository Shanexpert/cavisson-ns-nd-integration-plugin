#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>


#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "init_cav.h"
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
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "nslb_sock.h"
#include "ns_alloc.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_vuser_tasks.h"
#include "ns_trace_level.h"
#include "ns_njvm.h"
#include "ns_session.h"
#include "nslb_time_stamp.h"
#include "ns_java_string_api.h"
//#include "ns_child_msg_com.h"


Msg_com_con *njvm_ceased_thread_info = NULL;
int num_njvm_ceased_thread = 0;

//TODO: Hanle error cases properly.

//This method will add used njvm thread connection to ceased list.
//This function will be called whenever connection with thread will be closed
// from njvm. Thread might be in busy or free list.
static inline void update_msg_com_con_ceased_list(Msg_com_con *mccptr) 
{
  NSDL4_MESSAGES(NULL, NULL, "Method called");
  //Adding used thread into ceased pool
  Msg_com_con *tmp_prev_thread = mccptr->prev;
  Msg_com_con *tmp_next_thread = mccptr->next;
  Msg_com_con *tmp_ceased_thread = njvm_ceased_thread_info;
  njvm_ceased_thread_info = mccptr;
  njvm_ceased_thread_info->next = tmp_ceased_thread;
  num_njvm_ceased_thread++;
  NSDL4_MESSAGES(NULL, NULL, "After adding thread to ceased list, num_njvm_ceased_thread = %d", num_njvm_ceased_thread);
	
  //Updating counter of busy list and calling nsi_end_session
  if(mccptr->vptr)
  {
    NSDL2_MESSAGES(NULL, NULL, "Got thread close conn from busy list. Going to decrement njvm_total_busy_thread = (%d) counter", 
                                njvm_total_busy_thread);
    VUser *vptr = mccptr->vptr; 
    vptr->mcctptr->vptr = NULL;
    vptr->mcctptr = NULL;
    njvm_total_busy_thread--;
    NSDL2_MESSAGES(NULL, NULL, "After decrementing busy thread counter njvm_total_busy_thread = (%d)", njvm_total_busy_thread);
    u_ns_ts_t now = get_ms_stamp();    
    nsi_end_session(vptr, now); 
  }
  else    //Updating counter of free list
  {
    NSDL2_MESSAGES(NULL, NULL, "Got thread close conn from free list. Going to decrement njvm_total_free_thread = (%d) counter", 
                                 njvm_total_free_thread);
    njvm_total_free_thread--;
    NSDL2_MESSAGES(NULL, NULL, "After decrementing free thread counter njvm_total_free_thread = (%d)", njvm_total_free_thread);
    if(tmp_prev_thread)
      tmp_prev_thread->next = tmp_next_thread; 
  }
  NSDL4_MESSAGES(NULL, NULL, "Method end: Thread Count,num_njvm_ceased_thread = %d, njvm_total_busy_thread = %d, njvm_total_free_thread = %d", 
                              num_njvm_ceased_thread, njvm_total_busy_thread, njvm_total_free_thread);
}


//Need to handle properly
void close_njvm_msg_com_con(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
 
  //remove from epoll 
  remove_select_msg_com_con(mccptr->fd); 
  //close fd
  if(close(mccptr->fd) < 0) 
    NSTL1_OUT(NULL, NULL, "Error in closing %s, Error: %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno)); 
  mccptr->fd = -1;
  NSDL2_MESSAGES(NULL, NULL, "Adding closed njvm thread connection to ceased_thread_info list");
  if(mccptr->con_type == NS_STRUCT_TYPE_NJVM_THREAD){
    update_msg_com_con_ceased_list(mccptr); 
  } else{
    NSTL1_OUT(NULL, NULL, "NJVM (%s), control connection closed from other side.\n", msg_com_con_to_str(mccptr));
    end_test_run();
  }
}


//Method to read message from njvm.
/* Format of message will be 
 * <4 bytes SIZE><4 bytes OPCODE><4 Bytes Future field>*4<Message>
 *
 * Initially read_buf of mccptr will have 512 bytes malloced if recieved message size is more than current then 
 * realloc it.
 */
static char* read_nvm_njvm_msg(Msg_com_con *mccptr, int *size)
{
  int bytes_read;  // Bytes read in one read call
  int fd = mccptr->fd;

  if (fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for %s.. returning.", msg_com_con_to_str(mccptr));
    close_njvm_msg_com_con(mccptr);
    return NULL;  // Issue - this is misleading as it means read is not complete
  }

  if (!(mccptr->state & NS_STATE_READING)) // Method called for first time to read message
  {
    NSDL1_MESSAGES(NULL, NULL, "Method called to read message for the first time. %s", msg_com_con_to_str(mccptr));
    mccptr->read_offset = 0;
    mccptr->read_bytes_remaining = -1;
  }
  else
    NSDL1_MESSAGES(NULL, NULL, "Method called to read message which was not read completly. buf_size = %d, offset = %d, bytes_remaining = %d, %s", 
              mccptr->read_buf_size, mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));

  //if size of message not recieved then first read message size.
  if (mccptr->read_offset < sizeof(int))  
  {
    while (1) /* Reading size */
    {
      NSDL2_MESSAGES(NULL, NULL, "Reading size of message, bytes recieved till now = %d", mccptr->read_offset);

      if ((bytes_read = read (fd, mccptr->read_buf + mccptr->read_offset, sizeof(int) - mccptr->read_offset)) < 0)
      {
        if (errno == EAGAIN)
        {
          NSDL2_MESSAGES(NULL, NULL, "Complete message size is not available for read. offset = %d, %s", 
                    mccptr->read_offset, msg_com_con_to_str(mccptr));
          mccptr->state |= NS_STATE_READING; // Set state to reading message
          return NULL;
        } else if (errno == EINTR) {   /* this means we were interrupted */
          NSDL2_MESSAGES(NULL, NULL, "Interrupted. continuing");
          continue;
        }
        else
        {
          NSTL1_OUT(NULL, NULL, "Error in reading njvm message size, from njvm (%s). error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
          //Currently we are adding this connection to ceased connection data.
          close_njvm_msg_com_con(mccptr);
          return NULL; /* This is to handle closed connection from tools */
        }
      }
      if (bytes_read == 0) {
        NSDL2_MESSAGES(NULL, NULL, "Connection closed by njvm (%s). error = %s", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Connection closed by njvm (%s). error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno)); 
        close_njvm_msg_com_con(mccptr); 
        return NULL; 
      }
      mccptr->read_offset += bytes_read;
      if(mccptr->read_offset == sizeof(int)) {
        NSDL2_MESSAGES(NULL, NULL, "Complete length of bytes recieved");
        break;  
      }
    }
  }

  //set message.
  //check if length is less than required.(NVM_NJVM_MSG_HDR_SIZE - sizeof(int))
  //check if length is less than required.(NVM_NJVM_MSG_HDR_SIZE - sizeof(int))
  int msg_size = *(int *)mccptr->read_buf;
  if(msg_size < (NVM_NJVM_MSG_HDR_SIZE - sizeof(int))){
    NSDL2_MESSAGES(NULL, NULL, "Invalid message recieved from njvm(%s), value of size bytes = %d", msg_com_con_to_str(mccptr), msg_size);
    NSTL1_OUT(NULL, NULL, "Invalid message recieved from njvm(%s)\n", msg_com_con_to_str(mccptr));
    close_njvm_msg_com_con(mccptr); 
    return NULL;
  } 
  NSDL2_MESSAGES(NULL, NULL, "Length of message = %d(including header opcode/future fields)", msg_size);
  mccptr->read_bytes_remaining = msg_size; //we already read size bytes.
  
   
  //check if we need to realloc read_buf
  if(mccptr->read_bytes_remaining + sizeof(int) > mccptr->read_buf_size) 
  {
    NSDL2_MESSAGES(NULL, NULL, "read buffer size is not enough to read message, reallocing read_buf");
    MY_REALLOC(mccptr->read_buf, (mccptr->read_bytes_remaining + sizeof(int)), "mccptr->read_buf", -1);
    mccptr->read_buf_size = (mccptr->read_bytes_remaining + sizeof(int));
  }

  while (mccptr->read_bytes_remaining > 0) /* Reading rest of the message */
  {
    NSDL2_MESSAGES(NULL, NULL, "Reading rest of the message. offset = %d, bytes_remaining = %d, %s",
              mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));

    if((bytes_read = read (fd, mccptr->read_buf + mccptr->read_offset, mccptr->read_bytes_remaining)) <= 0)
    {
      if(errno == EAGAIN)
      {
        NSDL2_MESSAGES(NULL, NULL, "Complete message is not available for read. offset = %d, bytes_remaining = %d, %s", 
                  mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));
        mccptr->state |= NS_STATE_READING; // Set state to reading message
        return NULL;// NS_EAGAIN_RECEIVED | NS_READING;
      }
      else
      {
        NSTL1_OUT(NULL, NULL, "Error in reading msg, from njvm %s. error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        close_njvm_msg_com_con(mccptr); 
        return NULL; 

      }
    }
    mccptr->read_offset += bytes_read;
    mccptr->read_bytes_remaining -= bytes_read;
  }

  NSDL2_MESSAGES(NULL, NULL, "Complete message read. Total message size read = %d, %s", 
            mccptr->read_offset, msg_com_con_to_str(mccptr));
  NSTL2(NULL, NULL, "njvm (%s), Complete message read. Total message size read = %d", msg_com_con_to_str(mccptr), mccptr->read_offset); 
  mccptr->state &= ~NS_STATE_READING; // Clear state as reading message is complete
  *size = mccptr->read_offset;
  return (mccptr->read_buf);
}

//method to send message to jthread or njvm.
static int write_nvm_njvm_msg(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, bytes to write = %d", mccptr->write_bytes_remaining);
  
  if (mccptr->fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for %s.. returning.", msg_com_con_to_str(mccptr));
    return 0;  // Return 0 to indicate write is nor partial
  }

  int ret;
  NS_VPTR_SET_USER_CONTEXT(mccptr->vptr);
  while(mccptr->write_bytes_remaining > 0) {
    ret = write(mccptr->fd, (mccptr->write_buf + mccptr->write_offset), (mccptr->write_bytes_remaining));
    if(ret < 0) {
      if(errno == EAGAIN) {
        NSDL2_MESSAGES(NULL, NULL, "njvm (%s), EAGAIN,  while writing message", msg_com_con_to_str(mccptr));
        if (!(mccptr->state & NS_STATE_WRITING)) {
          mod_select_msg_com_con((char *)mccptr, mccptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT);
        }
        mccptr->state |= NS_STATE_WRITING;
        return -1;
      } else{
        NSDL2_MESSAGES(NULL, NULL, "njvm (%s), Failed to write message to njvm, Error = %s", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "njvm (%s), Failed to write message to njvm, Error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        //close connection to jthread and add in ceased thread count. 
        //TODO: here you have to check connection if control connection or jthread connection
        close_njvm_msg_com_con(mccptr);
        NS_VPTR_SET_NVM_CONTEXT(mccptr->vptr);
        return -1;
      }
    }
 
    //update bytes remaining and offset.
    mccptr->write_bytes_remaining -= ret;
    mccptr->write_offset += ret;
  }

  //reset it's state if set to NS_STATE_WRITING 
  if (mccptr->state & NS_STATE_WRITING) {
    mccptr->state &= ~NS_STATE_WRITING;
    mod_select_msg_com_con((char *)mccptr, mccptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  }

  //when write complete then just reset other parameters
  NSDL2_MESSAGES(NULL, NULL, "njvm (%s) message successfully sent", msg_com_con_to_str(mccptr));
  NSTL2(NULL, NULL, "njvm (%s) message successfully sent", msg_com_con_to_str(mccptr));
  mccptr->write_offset = 0;
  mccptr->write_bytes_remaining = -1;
  return 0;
}


//this method will handle message recieved from njvm.
static int handle_response_from_njvm_cc(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  int opcode;

  //check what recieved in msg.
  if(!mccptr->read_offset) {
    NSDL2_MESSAGES(NULL, NULL, "No message recieved");
    return -1;
  }
  //get opcode 4th byte
  opcode = *(int *)(mccptr->read_buf + sizeof(int));
  switch(opcode){
    case NS_NJVM_START_USER_REP:
      NSDL3_MESSAGES(NULL, NULL, "NS_NJVM_START_USER_REP recieved");
      return(handle_njvm_start_user_response(mccptr)); 
    case NS_NJVM_STOP_USER_REP:
      NSDL3_MESSAGES(NULL, NULL, "NS_NJVM_STOP_USER_REP recieved");
      return(handle_njvm_stop_user_response(mccptr));
    case NS_NJVM_ERROR_MSG_FROM_NJVM:
      NSDL3_MESSAGES(NULL, NULL, "NS_NJVM_ERROR_MSG_FROM_NJVM recieved");
      return(handle_njvm_error_message(mccptr));
    case NS_NJVM_INCREASE_THREAD_POOL_REP:
      NSDL3_MESSAGES(NULL, NULL, "NS_NJVM_INCREASE_THREAD_POOL_REP recieved");
      //TODO: handle here what to do 
      return(handle_njvm_increase_thread_rep(mccptr));
    default: 
      NSDL3_MESSAGES(NULL, NULL, "Invalid opcode(%4d) recieved", opcode);
      return -1; 
  }
  return 0;
}

//msg_len passed in this macro is length of protobuf message.
//HDR -> (int<size>) + (int<opcode>) + 4*(int<futurefields>)
#define FILL_HDR_IN_JTHREAD_MSG(buf, opcode, proto_msg_len)	\
{		\
  *(int *)(buf) = proto_msg_len + (NVM_NJVM_MSG_HDR_SIZE - sizeof(int));	\
  *(int *)(buf + sizeof(int)) = opcode;	\
}	


//Message format <4 bytes SIZE><4 bytes OPCODE><4 Bytes Future field>*4<Message>
//second argument will tell about the event we caught
int handle_msg_from_njvm(Msg_com_con *mccptr, int epoll_events)
{
  int msg_len;

  VUser *vptr = mccptr->vptr;
  TLS_SET_VPTR(vptr);

  NSDL2_MESSAGES(vptr, NULL, "Method called");

  //this can be case of partial write
  if(epoll_events & EPOLLOUT) {
    if(mccptr->state & NS_STATE_WRITING) {
      NSDL2_MESSAGES(NULL, NULL, "Write event found on njvm (%s)", msg_com_con_to_str(mccptr));
      return(write_nvm_njvm_msg(mccptr));
    }
    else {
      NSDL2_MESSAGES(NULL, NULL, "Write state is not set, returing here.");
      return -1;
    }
  }

  //read message from jthread.
  if(read_nvm_njvm_msg(mccptr, &msg_len) == NULL)
  {
    //Checking whether case of partial read occured or error
    if(mccptr->fd == -1)
    {
      NSTL1(NULL, NULL, "Failed to read message from njvm thread(%s)", msg_com_con_to_str(mccptr)); 
    }
    else
    {
      NSDL2_MESSAGES(NULL, NULL, "Patial read occurred from njvm thread(%s)\n", msg_com_con_to_str(mccptr));
    }
    return -1;
  }    

  //generate opcode and generate call hanlder for that according to message.
  int opcode = *(int *)(mccptr->read_buf + sizeof(int));
  NSDL2_MESSAGES(NULL, NULL, "njvm thread(%s), opcode of message recieved = %4d", msg_com_con_to_str(mccptr), opcode);
  NSTL1(NULL, NULL, "njvm thread(%s), opcode of message recieved = %4d", msg_com_con_to_str(mccptr), opcode);
  
  NS_VPTR_SET_NVM_CONTEXT(mccptr->vptr);

  //This is the case when some response opcode recieve, handle response.
  if((opcode/1000) == 2) {
     NSDL2_MESSAGES(NULL, NULL, "njvm control connection(%s), response opcode received, handling response", msg_com_con_to_str(mccptr));
     NSTL1(NULL, NULL, "njvm control connection(%s), response opcode(%4d) received, handling response", msg_com_con_to_str(mccptr), opcode);
     return(handle_response_from_njvm_cc(mccptr));
  }

   // Except of Bind request from NJVM we need vptr
   if(!mccptr->vptr && opcode != NS_NJVM_BIND_NVM_REQ)
     return -1;
  
  //check for handler if present then call that otherwise return from here print error.
  jThreadMessageHandler msg_handler_func = jthraed_msg_handler[opcode % 1000]; 
  
  if(msg_handler_func == NULL) {
    NSDL2_MESSAGES(NULL, NULL, "njvm thread(%s) Handler not defined for this message.", msg_com_con_to_str(mccptr));
    NSTL1_OUT(NULL, NULL, "njvm thread(%s) Handler not defined for this message.\n", msg_com_con_to_str(mccptr));
    //? do we need to send error message to njvm thread.
    close_njvm_msg_com_con(mccptr);
    //end_test_run();
  } 

  int out_msg_len = 0;
  int ret = msg_handler_func(mccptr, &out_msg_len);
  
  if(ret == -1) {
    NSDL2_MESSAGES(NULL, NULL, "njvm thread(%s), Failed to handle message", msg_com_con_to_str(mccptr));
    NSTL1_OUT(NULL, NULL, "njvm thread(%s), Failed to handle message\n", msg_com_con_to_str(mccptr));
    //send error message to njvm.
    close_njvm_msg_com_con(mccptr);
    //end_test_run(); 
  }
 
  /* This is the case when nvm recieve api call which will take some time like ns_web_url. so we will add task for those apis,
     and when processing will complete we will send reply for them  */ 
  if(ret == 1) {
    NSDL2_MESSAGES(NULL, NULL, "njvm thread(%s), Task have been added successfully for current message", msg_com_con_to_str(mccptr));
    return 0;
  }
 
  //now send response to njvm. 
  //first prepare message to send.
  int out_msg_opcode = ((opcode % 1000) + 2000);
  NSDL3_MESSAGES(NULL, NULL, "njvm thread(%s), message output opcode = %4d, length of message = out_msg_len = %d",
                      msg_com_con_to_str(mccptr), out_msg_opcode, out_msg_len);
  FILL_HDR_IN_JTHREAD_MSG(mccptr->write_buf, out_msg_opcode, out_msg_len);
  NSDL3_MESSAGES(NULL, NULL, "njvm thread(%s), filled  size = %d, opcode = %4d", msg_com_con_to_str(mccptr), 
            *(int *)(mccptr->write_buf), *(int *)(mccptr->write_buf + sizeof(int)));

  mccptr->write_offset = 0;
  mccptr->write_bytes_remaining = out_msg_len + NVM_NJVM_MSG_HDR_SIZE; 

  write_nvm_njvm_msg(mccptr);  
  NSDL2_MESSAGES(NULL, NULL, "njvm (%s): Method completed", msg_com_con_to_str(mccptr));  
  return 0; 
}


//This method can be used to send response of web_url and page_think time 
//and request message of start_user, end_user, end_session, exit_test, etc.
//currently this method is being used by both contol/worker thread.
int send_msg_to_njvm(Msg_com_con *mccptr, int opcode, int output)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %4d", opcode);
  int proto_buf_len = 0;  
 
  switch(opcode)
  {
    /* These are request messages send to njvm(control/thread) */
    case NS_NJVM_START_USER_REQ:
      //this mode is to test if we don't send any big start user message.
      start_user_req_create_msg(mccptr, &proto_buf_len);
      break;
    case NS_NJVM_INCREASE_THREAD_POOL_REQ:
      //here output is cummulative thread count
      increase_thread_pool_create_msg(mccptr, output, &proto_buf_len);           
      break;
    case NS_NJVM_STOP_USER_REQ:
      stop_user_create_msg(mccptr, &proto_buf_len);
      break;
    case NS_NJVM_EXIT_TEST_REQ:
      NSDL2_MESSAGES(NULL, NULL, "Sending exit test run message to njvm(%s)", msg_com_con_to_str(mccptr));
      //it has void request so just need to add header.
      break;   
    /* THese are response messages of apis which was scheduled by task */
    case NS_NJVM_API_WEB_URL_REP:
      //here output is status of ns_web_url(pass/fail)
      ns_web_url_resp_create_msg(mccptr, output, &proto_buf_len);
      break;   
    case NS_NJVM_API_PAGE_THINK_TIME_REP:
      ns_page_think_time_resp_create_msg(mccptr, output, &proto_buf_len);    
      break;
    case NS_NJVM_API_WEB_WEBSOCKET_SEND_REP:
      ns_web_websocket_send_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP:
      ns_web_websocket_close_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_START_TRANSACTION_REP:
      ns_start_tx_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_SYNC_POINT_REP:
      ns_sync_point_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_WEB_WEBSOCKET_READ_REP:
      ns_web_websocket_read_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_CLICK_API_REP:
      ns_click_api_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    case NS_NJVM_API_END_SESSION_REP:
      ns_end_session_resp_create_msg(mccptr, output, &proto_buf_len);
      break;
    default: 
      NSDL2_MESSAGES(NULL, NULL, "Invalid message opcode %4d, request for njvm(%s)", opcode, msg_com_con_to_str(mccptr));
      return -1; 
  }

  //Now fill headers.
  FILL_HDR_IN_JTHREAD_MSG(mccptr->write_buf, opcode, proto_buf_len);
  NSTL1(NULL, NULL, "njvm(%s), sending msg opcode = %4d, size = %d",
         msg_com_con_to_str(mccptr), *(int *)(mccptr->write_buf + sizeof(int)), (int *)(mccptr->write_buf));
  NSDL2_MESSAGES(NULL, NULL, "njvm(%s), opcode = %4d, size = %d", 
        msg_com_con_to_str(mccptr), *(int *)(mccptr->write_buf + sizeof(int)), *(int *)(mccptr->write_buf));
  mccptr->write_offset = 0;
  mccptr->write_bytes_remaining = proto_buf_len + NVM_NJVM_MSG_HDR_SIZE; 

  return(write_nvm_njvm_msg(mccptr));
}

