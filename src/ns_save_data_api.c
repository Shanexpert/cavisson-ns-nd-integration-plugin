/*This file contain APIs for saving data*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/epoll.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "nslb_time_stamp.h"
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
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "amf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_url_req.h"
#include "ns_alloc.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_smtp.h"
#include "ns_smtp_send.h"
//#include "ns_handle_read.h"
#include "init_cav.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "wait_forever.h"
#include "nslb_hash_code.h"
#include "ns_event_filter.h"
#include "nslb_util.h"

static int first_time_flag = 0; // Remove after when save data API support without nsa_log_mgr 

static int fill_opcode_and_file_name(char *send_buf, char *file, int mode)
{
  unsigned short opcode;
  unsigned short file_name_len;
  unsigned int size_to_send = 0;
  char *file_len_ptr;

  VUser *vptr = TLS_GET_VPTR();

  NSDL2_API(vptr, NULL, "Method called. mode = %d, file = %s", mode, file);

  opcode = (SAVE_DATA_MSG);

  //Fill Opcode
  memcpy(send_buf, &opcode, UNSIGNED_SHORT);
  send_buf += UNSIGNED_SHORT;
  size_to_send += UNSIGNED_SHORT;
 
  file_len_ptr = send_buf; // Point to file name length in the send_buf

  /*Point send_buf to file name*/
  send_buf += UNSIGNED_SHORT;
  size_to_send += UNSIGNED_SHORT;

  // if file name is absolute path or data dir not used, send file as is
  if((file[0] == '/') || (runprof_table_shr_mem[vptr->group_num].gset.data_dir[0] == '\0'))
  {
    file_name_len = strlen(file);
    memcpy(send_buf, file, file_name_len);
  }
  else
  {
    // Convert file to absolute path in the data directory
    file_name_len = sprintf(send_buf, "%s/data/%s/%s", GET_NS_TA_DIR(), runprof_table_shr_mem[vptr->group_num].gset.data_dir, file);
  }
  /*For file name - set length of the file name */  
  memcpy(file_len_ptr, &file_name_len, UNSIGNED_SHORT);
  NSDL2_API(vptr, NULL, "Save data API file name = %*.*s", file_name_len, file_name_len, send_buf);

  send_buf += file_name_len;
  size_to_send += file_name_len;
  
  /*For mode of open*/
  memcpy(send_buf, &mode, UNSIGNED_INT);
  send_buf += UNSIGNED_INT;
  size_to_send += UNSIGNED_INT;

  return size_to_send;
}

//#define MAX_SEND_BUFF_SIZE (7 * 1024)
#define MAX_SEND_BUFF_SIZE 6196 //Manish: Here size is depends on size of EventMsg so whenever change max buf size 
                                //please be keep in mind dependency otherwise nsa_log_mgr will not work correctly. 
#define MAX_VAR_NAME_SIZE  128

int ns_save_data_eval(char *file_name, int mode, char *eval_string)
{
  char send_buf[MAX_SEND_BUFF_SIZE + 1];
  char *send_ptr = send_buf;
  char *var_value = NULL;
  unsigned char save_char;
  unsigned int var_len, no_var_len, total_len = 0;
  unsigned int size_to_send;
  
  IW_UNUSED(VUser *vptr = TLS_GET_VPTR());
 
  if(global_settings->enable_event_logger == 0) {
     if(first_time_flag == 0) {
      first_time_flag++;
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                               __FILE__, (char*)__FUNCTION__,
                               "NetStorm Log Manager is disabled,"
			       " save data APIs can not save data");
     }
     return -1;
  } 
 
  /*nsa_log_mgr died*/
  //In NetcLoud case nsa_log_mgr_pid will be -1 so this will be not be any error
  if((!send_events_to_master && (nsa_log_mgr_pid <= 0)) || g_el_subproc_msg_com_con.fd <= 0 || ((g_el_subproc_msg_com_con.fd > 0) && (g_el_subproc_msg_com_con.state & NS_CONNECTING))) {
     if(first_time_flag == 0) {
      first_time_flag++;
      fprintf(stderr, "Warning: Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.\n", my_port_index);
      NS_DUMP_WARNING("Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.", my_port_index);
     }
    return -1;
  }
  NSDL2_API(vptr, NULL, "Method called");

  size_to_send = UNSIGNED_INT;  // First we write total length
  /*Here size_to_send will be used as offset
   * to fill the send buffer*/
  size_to_send += fill_opcode_and_file_name(send_ptr + UNSIGNED_INT, file_name, mode); // + UNSIGNED_INT so that we can write to next position

  var_value = ns_eval_string(eval_string); 
  
  var_len = strlen(var_value);

  /*Manish: Fri Sep  7 01:42:24 EDT 2012 
      If var_len is greater than or equal to left buffer size(till now) then we will tarncate it and generate a even log.
      remained_buf_size = (MAX_SEND_BUFF_SIZE - UNSIGNED_INT) - size_to_send
      Note:- here we substract UNSIGNED_INT from MAX_SEND_BUFF_SIZE because starting 4 byte reserved for total length 
   */
  unsigned int remained_buf_size = (MAX_SEND_BUFF_SIZE - UNSIGNED_INT) - size_to_send; 
  if(remained_buf_size <= var_len)
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Data truncated as data length (%u) in save data API either equal or exceeded maximum allowed length (%u)",
                               var_len, remained_buf_size); 
    var_len = remained_buf_size - 1;
  } 

  save_char = var_value[var_len];
  var_value[var_len] = '\n'; // We are writing on Netstorm memory we need to reset
  no_var_len = ++var_len;  // ++ for new line

  /*Fill length of variable value len*/
  memcpy(send_ptr+size_to_send, &no_var_len, UNSIGNED_INT);
  size_to_send += UNSIGNED_INT;
  
  memcpy(send_ptr+size_to_send, var_value, var_len);
  size_to_send += var_len;

  total_len = (size_to_send - UNSIGNED_INT);
  memcpy(send_ptr, &total_len, UNSIGNED_INT);

  write_msg(&g_el_subproc_msg_com_con, send_ptr, size_to_send, 1, CONTROL_MODE);
  var_value[var_len] = save_char; // Reset as we copied in NetStorm memory
  return 0;
}

int ns_save_data_ex(char *file_name, int mode, char *format, ...){
  va_list ap;
  unsigned int  amt_written = 0;
  unsigned int  total_len = 0;
  char send_buf[MAX_SEND_BUFF_SIZE + 1];
  char *send_ptr = send_buf;
  unsigned int size_to_send = UNSIGNED_INT;
  char *ptr = NULL; //This is ptr to fill the size of buffer
  
  IW_UNUSED(VUser *vptr = TLS_GET_VPTR());

  if(global_settings->enable_event_logger == 0) {
     if(first_time_flag == 0) {
      first_time_flag++;
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                               __FILE__, (char*)__FUNCTION__,
                               "NetStorm Log Manager is disabled,"
			       " save data APIs can not save data");
     }
     return -1;
  } 

  /*nsa_log_mgr died*/
  //In NetcLoud case nsa_log_mgr_pid will be -1 so this will be not be any error
  if((!send_events_to_master && (nsa_log_mgr_pid <= 0))|| g_el_subproc_msg_com_con.fd <= 0 || ((g_el_subproc_msg_com_con.fd > 0) && (g_el_subproc_msg_com_con.state & NS_CONNECTING))) {
  //if(nsa_log_mgr_pid <= 0 || g_el_subproc_msg_com_con.fd <= 0) {
     if(first_time_flag == 0) {
      first_time_flag++;
      fprintf(stderr, "Warning: Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.\n", my_port_index);
      NS_DUMP_WARNING("Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.", my_port_index);
     }
    return -1;
  }
  NSDL2_API(vptr, NULL, "Method called");
 
  /*Here size_to_send will be used as offset
   * to fill the send buffer*/
  size_to_send += fill_opcode_and_file_name(send_ptr + UNSIGNED_INT, file_name, mode);
  
  send_ptr += size_to_send;
   
  //Save pointer to fill the data len
  ptr = send_ptr;
  /*Move pointer 4 byte forward to fill data*/
  ptr += UNSIGNED_INT;
  size_to_send += UNSIGNED_INT;
  
  va_start(ap, format);
  /*Manish: Fri Sep  7 01:42:24 EDT 2012 
      If var_len is greater than or equal to left buffer size(till now) then we will tarncate it and generate a even log.
      remained_buf_size = (MAX_SEND_BUFF_SIZE - UNSIGNED_INT) - size_to_send
      Note:- here we substract UNSIGNED_INT from MAX_SEND_BUFF_SIZE because starting 4 byte reserved for total length 
   */
  unsigned int remained_buf_size  = (MAX_SEND_BUFF_SIZE - UNSIGNED_INT) - size_to_send;
  amt_written = vsnprintf(ptr, remained_buf_size, format, ap);
  va_end(ap);

  ptr[remained_buf_size] = 0; // In case there is space in ptr for vsnprintf

  if(amt_written < 0) {
    amt_written = strlen(ptr);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Data is not in correct format.");
  } 
  else if (amt_written >= remained_buf_size) {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Data truncated as data length (%u) in save data API either equal or exceeded maximum allowed length (%u)",
                               amt_written, remained_buf_size);
    amt_written = remained_buf_size - 1; 
  }

  ptr[amt_written] = '\n';
  /*Now write teh data size*/
  (++amt_written); // ++ for new line
  memcpy(send_ptr, &amt_written, UNSIGNED_INT);
  
  /*Adjust total size to send*/
  size_to_send += amt_written;

  total_len = size_to_send -UNSIGNED_INT;
  memcpy(send_buf, &total_len, UNSIGNED_INT);

  write_msg(&g_el_subproc_msg_com_con, send_buf, size_to_send, 1, CONTROL_MODE);
  return 0;
}

// Save value of var_name in the file
int ns_save_data_var(char *file_name, int mode, char *var_name)
{
  // Get the value of var
  // If var name is not correct, return -1
  char var[MAX_VAR_NAME_SIZE + 1];
  unsigned int  amt_written = 0;
  
  IW_UNUSED(VUser *vptr = TLS_GET_VPTR());

  if(global_settings->enable_event_logger == 0) {
     if(first_time_flag == 0) {
      first_time_flag++;
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                               __FILE__, (char*)__FUNCTION__,
                               "NetStorm Log Manager is disabled,"
			       " save data APIs can not save data");
     }
     return -1;
  } 

  /*nsa_log_mgr died*/
  //In NetcLoud case nsa_log_mgr_pid will be -1 so this will be not be any error
  if((!send_events_to_master && (nsa_log_mgr_pid <= 0))|| g_el_subproc_msg_com_con.fd <= 0 || ((g_el_subproc_msg_com_con.fd > 0) && (g_el_subproc_msg_com_con.state & NS_CONNECTING))) {
  //if(nsa_log_mgr_pid <= 0 || g_el_subproc_msg_com_con.fd <= 0) {
     if(first_time_flag == 0) {
      first_time_flag++;
      fprintf(stderr, "Warning: Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.\n", my_port_index);
      NS_DUMP_WARNING("Either NetStorm Log Manager got killed or connection is in progress, save data APIs can not save data for NVM '%d'.", my_port_index);
     }
    return -1;
  }
  NSDL2_API(vptr, NULL, "Method called");

  //Make var compatible to ns_eval_string API.
  amt_written = snprintf(var, MAX_VAR_NAME_SIZE, "{%s}", var_name);
  if(amt_written >= MAX_VAR_NAME_SIZE)
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Parameter name length (%u) in data save API exceeded maximum allowed length (%u)",
                               amt_written, MAX_VAR_NAME_SIZE);
    return -1;
  }
  
  var[MAX_VAR_NAME_SIZE] = 0; // In case there is space in ptr for vsnprintf

  if(amt_written < 0) {
    amt_written = strlen(var);
  }

  ns_save_data_eval(file_name, mode, var);
  return 0;
}
