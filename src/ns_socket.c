/***************************************************************************************
 Name		: ns_socket.c
 Purpose	: This file contain all functions related to socket API and used
                  internally not exposed to VUsers.
                  For more deatil please refer to Requirment Doc -
                    docs/Products/NetStorm/TechDocs/ProtoSocket/Req/ProtoSocketReq.docx
 
                  Supported APIs are -
                    1. ns_socket_open()
                    2. ns_socket_send()
                    3. ns_socket_recv()
                    4. ns_socket_close()

 Author(s)	: Nisha
 Mod. Hist.	: 5 June 2020
***************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <limits.h>
#include <math.h>

#include "nslb_get_norm_obj_id.h"
#include "nslb_sock.h"
#include "nslb_encode_decode.h"
#include "nslb_string_util.h"

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_log_req_rep.h"
#include "ns_string.h"
#include "ns_url_req.h"
#include "ns_vuser_tasks.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_thread.h"
#include "netstorm.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_socket.h"
#include "ns_websocket.h"
#include "ns_http_process_resp.h"
#include "ns_vuser_trace.h"
#include "ns_socket.h"
#include "ns_socket_io.h"
#include "ns_ssl.h"

#ifndef CAV_MAIN
GlobalSocketData g_socket_vars;
int cur_post_buf_len;
#else
__thread GlobalSocketData g_socket_vars;
extern __thread int cur_post_buf_len;
#endif
char *socket_send_buffer = NULL;
int send_buff_len = 0;

short ns_socket_errno = -1;

#if 0
const SocketErrors socket_errors[] = 
{
  //Socket API Ooen
  {0, "Socket API Open failed: "},
  {-1, "__LAST__",}
};
#endif

void init_global_timeout_values()
{
  NSDL2_HTTP(NULL, NULL, "Method called");

  g_socket_vars.socket_settings.conn_to = g_socket_vars.socket_settings.send_to = g_socket_vars.socket_settings.send_ia_to = g_socket_vars.socket_settings.recv_to = g_socket_vars.socket_settings.recv_ia_to = g_socket_vars.socket_settings.recv_fb_to = -1;
}

/* This function convert host to network and network to host */

#define is_bigendian(n) ((*(char *)&n) == 0)

/*Assumption:
    @in  =>  Input argument point to memory of int/short/long only
*/
void nslb_endianness(unsigned char *out, unsigned char *in, int bytes, char len_endianness)
{
  const int n = 0x01;  //To test endianness
  int m_arch = SLITTLE_ENDIAN;
  int i, j;

  if((&n)[0] == 0)  //It means machine is BigEndian
    m_arch = SBIG_ENDIAN;
  else
    m_arch = SLITTLE_ENDIAN;

  NSDL2_HTTP(NULL, NULL, "m_arch = %d, len_endianness = %d", m_arch, len_endianness);

  if(((m_arch == SBIG_ENDIAN) && (len_endianness == SBIG_ENDIAN)) ||
     ((m_arch == SLITTLE_ENDIAN) && (len_endianness == SLITTLE_ENDIAN)))
  {
    for(i = 0; i < bytes; i++)
    {
      out[i] = in[i];
    }
  }
  else   //Except BigEndian Arch always reverse the order
  {
    for(i = 0, j = (bytes - 1); i < bytes; i++, j--)
    {
      out[i] = in[j];
    }
  }
  #if NS_DEBUG_ON
  for(i = 0; i < bytes; i++)
    NSDL2_HTTP(NULL, NULL, "out[%d] = 0x%x, in[%d] = 0x%x", i, out[i], i, in[i]);
  #endif
}

void ns_segtobuf_cleanup()
{
  NSDL2_SOCKETS(NULL, NULL, "[SegToBufCleanup] method called");

  // Clean ns_nvm_scratch_buf
  ns_nvm_scratch_buf[0] = 0;
  ns_nvm_scratch_buf_len = 0;
  
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
}

char *ns_segtobuf(VUser *vptr, connection *cptr, const StrEnt_Shr* segptr, char **start, int *len)
{
  //VUser *vptr = cptr->vptr;
  char *buf_ptr = ns_nvm_scratch_buf + ns_nvm_scratch_buf_len;
  int i = g_scratch_io_vector.cur_idx;
  int delta = 4096; //4k
  int ret;

  if(!vptr)
    vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "[SegToBuf] Fill data form segment table to scratch buffer, "
             "vptr = %p, cptr = %p, g_scratch_io_vector.cur_idx = %d", 
              vptr, cptr, g_scratch_io_vector.cur_idx);

  if(!segptr->num_entries) 
  {
    NSDL2_SOCKETS(vptr, cptr, "[SegToBuf] Number of segments is 0 so returing, vptr = %p, cptr = %p", vptr, cptr);
    return NULL;
  }

  // Save start pointer of these segments
  if(start)
    *start = buf_ptr;

  *len = 0;

  //Get Segment value and fill into vector
  if((ret = insert_segments(vptr, cptr, segptr, &g_scratch_io_vector, 
                            NULL, 0, 1, REQ_PART_BODY, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
    NSDL2_SOCKETS(vptr, cptr, "[SegToBuf] Failed to get segment value, vptr = %p, cptr = %p", vptr, cptr);
    return NULL;
  }

  for(; i < g_scratch_io_vector.cur_idx; i++)
  {
    NSDL2_SOCKETS(vptr, cptr, "[SegToBuf] i = %d, g_scratch_io_vector.cur_idx = %d, "
               "ns_nvm_scratch_buf_size = %d, ns_nvm_scratch_buf_len = %d, iov_len = %d, iov_base = %s", 
               i, g_scratch_io_vector.cur_idx, ns_nvm_scratch_buf_size, ns_nvm_scratch_buf_len, 
               g_scratch_io_vector.vector[i].iov_len, g_scratch_io_vector.vector[i].iov_base);

    //Check size 
    if((ns_nvm_scratch_buf_size - ns_nvm_scratch_buf_len) < g_scratch_io_vector.vector[i].iov_len)
    {
      MY_REALLOC(ns_nvm_scratch_buf, (ns_nvm_scratch_buf_size + g_scratch_io_vector.vector[i].iov_len + delta), 
                 "SegToBuf: Realloc NVM's scratch buffer (ns_nvm_scratch_buf)", -1);

      ns_nvm_scratch_buf_size += g_scratch_io_vector.vector[i].iov_len + delta;
    }

    //Copy data into scratch buffer
    memcpy(buf_ptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
    buf_ptr += g_scratch_io_vector.vector[i].iov_len;
   
    ns_nvm_scratch_buf_len += g_scratch_io_vector.vector[i].iov_len;
    *len += g_scratch_io_vector.vector[i].iov_len;
  }

  ns_nvm_scratch_buf[ns_nvm_scratch_buf_len] = 0;

  NSDL2_SOCKETS(vptr, cptr, "[SegToBuf] ns_nvm_scratch_buf_len = %d, *len = %d", ns_nvm_scratch_buf_len, *len);
  return ns_nvm_scratch_buf; 
}

connection* nsi_get_cptr_from_sock_id(VUser *vptr, int action_idx)
{
  NSDL2_SOCKETS(vptr, NULL, "Method called, action_id = [%d]", action_idx);

  return vptr->conn_array[request_table_shr_mem[action_idx].proto.socket.norm_id];
}

int ns_socket_modify_epoll(connection *cptr, int action_idx, int event)
{
  VUser *vptr;
  struct epoll_event epinst;
  
  NSDL2_SOCKETS(NULL, cptr, "Method called, cptr = %p, action_idx = %d, event = %d", cptr, action_idx, event);

  if (!cptr)
  {
    NSDL2_SOCKETS(NULL, cptr, "[SocketAPI-MODIFY_EPOLL], Error: cptr is null hence returning.");
    return -1;
  }

  if(cptr == NULL)
  {
    vptr = TLS_GET_VPTR();
    if((cptr = nsi_get_cptr_from_sock_id(vptr, action_idx)) == NULL)
    {
      NSDL2_SOCKETS(vptr, cptr, "[SocketAPI-MODIFY_EPOLL], Error: unable to get cptr for action_idx = %d", action_idx);
      return -1;
    }
  }
  else
    vptr = cptr->vptr;


  bzero(&epinst, sizeof(struct epoll_event));  

  if(event == -1)
    epinst.events = EPOLLERR|EPOLLHUP|EPOLLET;
  else
    epinst.events = event|EPOLLERR|EPOLLHUP|EPOLLET;

  epinst.data.ptr = (void *)cptr;

  NSDL2_SOCKETS(vptr, cptr, "Modify Epoll event for cptr = %p, fd = %d, events = 0x%0x", cptr,  cptr->conn_fd, epinst.events);

  if(epoll_ctl(v_epoll_fd, EPOLL_CTL_MOD, cptr->conn_fd, &epinst) == -1)
  {
    fprintf(stderr, "Error:ns_socket_modify_epoll(), faild to modify epoll "
            "event 0x%0x for fd %d. error(%d) = %s", 
            epinst.events, cptr->conn_fd, errno, nslb_strerror(errno));
    return -1;
  }

  return 0;
}

//void ns_socket_nvmjob_done(connection *cptr, int job)
//{
  //cptr->conn_state = CNST_IDLE;
    
//}

#if 0
int init_socket_default_values(action_request* requests)
{
  total_socket_request_entries++;
  NSDL2_MISC(NULL, NULL, "total_socket_req_entries = %d", total_socket_request_entries);
  
  requests->proto.socket.id = -1;                     // Socket API ID

  //Open
  requests->proto.socket.protocol = TCP_PROTO;        //Protocol - TCP - 0, UDP - 1
  requests->proto.socket.ssl_flag = SSL_DISABLE;      //Non-SSL - 0, SSl - 1

  requests->proto.socket.host.seg_start = -1;
  requests->proto.socket.host.num_entries= 0;

  requests->proto.socket.conn_timeout = CONNECT_TIMEOUT;
  requests->proto.socket.backlog = -1;

  //Send
  requests->proto.socket.slen_bytes = -1;
  requests->proto.socket.slen_type = LEN_TYPE_TEXT;
  requests->proto.socket.smsg_type = MSG_TYPE_TEXT;
  requests->proto.socket.spref_type = MSG_TYPE_TEXT;
  requests->proto.socket.encoding = ENC_NONE;

  requests->proto.socket.smsg_prefix.seg_start = -1;
  requests->proto.socket.smsg_prefix.num_entries= 0;

  requests->proto.socket.smsg_suffix.seg_start = -1;
  requests->proto.socket.smsg_suffix.num_entries= 0;

  requests->proto.socket.smsg.seg_start = -1;
  requests->proto.socket.smsg.num_entries= 0;

  requests->proto.socket.stimeout = SEND_TIMEOUT;

  //Read
  requests->proto.socket.read_bytes = -1;
  requests->proto.socket.rlen_bytes = -1;
  requests->proto.socket.rlen_type = LEN_TYPE_TEXT;
  requests->proto.socket.rpref_type = MSG_TYPE_TEXT;
  requests->proto.socket.decoding = ENC_NONE;

  requests->proto.socket.rmsg_prefix.seg_start = -1;
  requests->proto.socket.rmsg_prefix.num_entries = 0;

  requests->proto.socket.rmsg_suffix.seg_start = -1;
  requests->proto.socket.rmsg_suffix.num_entries = 0;

  requests->proto.socket.rttfb_timeout = READ_TIMEOUT_FB;
  requests->proto.socket.rtimeout = READ_TIMEOUT_IDLE;
  requests->proto.socket.action = -1;

  return 0;
}
#endif
/*static int init_socket_uri(int *url_idx, char *flow_file)
{
  NSDL2_SOCKETS(NULL, NULL, "Method Called url_idx = %d, flow_file = %s", *url_idx, flow_file);

  //creating request table
  create_requests_table_entry(url_idx); // Fill request type inside create table entry

  gPageTable[g_cur_page].first_eurl = *url_idx;

  return NS_PARSE_SCRIPT_SUCCESS;
}*/

void socket_init(int row_num, int proto, int s_api)
{
  requests[row_num].request_type = proto;

  //if(!(global_settings->protocol_enabled & SOCKET_PROTOCOL_ENABLED))
    //  global_settings->protocol_enabled |= SOCKET_PROTOCOL_ENABLED;

  NSDL2_MISC(NULL, NULL, "total_socket_req_entries = %d", total_socket_request_entries);

  requests[row_num].proto.socket.operation = s_api;
  requests[row_num].proto.socket.flag = NOTEXCLUDE_SOCKET;
  requests[row_num].proto.socket.enc_dec_cb = NULL;
  requests[row_num].proto.socket.timeout_msec = -1;

  switch(s_api)
  {
    case SOPEN:
    {
      requests[row_num].proto.socket.open.protocol = TCP_PROTO;        //Protocol - TCP - 0, UDP - 1
      requests[row_num].proto.socket.open.ssl_flag = SSL_DISABLE;      //Non-SSL - 0, SSl - 1
      requests[row_num].proto.socket.open.backlog = -1;
     
      requests[row_num].proto.socket.open.local_host.seg_start = -1;
      requests[row_num].proto.socket.open.local_host.num_entries= 0;

      requests[row_num].proto.socket.open.remote_host.seg_start = -1;
      requests[row_num].proto.socket.open.remote_host.num_entries= 0;
      total_socket_request_entries++;
    }
    break;

    case SSEND:
    {
      requests[row_num].proto.socket.send.msg_fmt.len_bytes = 0;
      requests[row_num].proto.socket.send.msg_fmt.len_type = LEN_TYPE_TEXT;
      requests[row_num].proto.socket.send.msg_fmt.len_endian = SBIG_ENDIAN;

      requests[row_num].proto.socket.send.msg_fmt.msg_type = MSG_TYPE_TEXT;
      requests[row_num].proto.socket.send.msg_fmt.msg_enc_dec = ENC_NONE;
     
      requests[row_num].proto.socket.send.msg_fmt.prefix.seg_start = -1;
      requests[row_num].proto.socket.send.msg_fmt.prefix.num_entries= 0;
     
      requests[row_num].proto.socket.send.msg_fmt.suffix.seg_start = -1;
      requests[row_num].proto.socket.send.msg_fmt.suffix.num_entries= 0;
     
      requests[row_num].proto.socket.send.buffer.seg_start = -1;
      requests[row_num].proto.socket.send.buffer.num_entries= 0;
     
      requests[row_num].proto.socket.send.buffer_len = 0;
    }
    break; 

    case SRECV:
    {
      requests[row_num].proto.socket.recv.msg_fmt.len_bytes = 0;
      requests[row_num].proto.socket.recv.msg_fmt.len_type = LEN_TYPE_TEXT;
      requests[row_num].proto.socket.recv.msg_fmt.len_endian = SBIG_ENDIAN;

      requests[row_num].proto.socket.recv.msg_fmt.msg_type = MSG_TYPE_TEXT;
      requests[row_num].proto.socket.recv.msg_fmt.msg_enc_dec = ENC_NONE;
     
      requests[row_num].proto.socket.recv.msg_fmt.prefix.seg_start = -1;
      requests[row_num].proto.socket.recv.msg_fmt.prefix.num_entries= 0;
     
      requests[row_num].proto.socket.recv.msg_fmt.suffix.seg_start = -1;
      requests[row_num].proto.socket.recv.msg_fmt.suffix.num_entries= 0;

      requests[row_num].proto.socket.recv.fb_timeout_msec = -1;
      requests[row_num].proto.socket.recv.msg_contains_action = -1;

      requests[row_num].proto.socket.recv.buffer = 0;
      requests[row_num].proto.socket.recv.buffer_len = -1;
    }
    break;
  }
}

int set_socket_request_type(char *hostname, int url_idx, int sess_idx)
{
  if(!requests[url_idx].proto.socket.open.ssl_flag)
    requests[url_idx].request_type = SOCKET_REQUEST;
  else if(requests[url_idx].proto.socket.open.ssl_flag)
    requests[url_idx].request_type = SSL_SOCKET_REQUEST;

  if(requests[url_idx].is_url_parameterized == NS_URL_PARAM_VAR)
  {
    requests[url_idx].index.svr_idx = get_parameterized_server_idx(hostname, requests[url_idx].request_type, script_ln_no);
  }
  else
  {
    requests[url_idx].index.svr_idx = get_server_idx(hostname, requests[url_idx].request_type, script_ln_no);
    CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ns_socket_open");
  }
  if(requests[url_idx].index.svr_idx != -1)
  {
    if(gServerTable[requests[url_idx].index.svr_idx].main_url_host == -1)
    {
      gServerTable[requests[url_idx].index.svr_idx].main_url_host = 1; // For main url
    }
  }
  else
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012073_ID, CAV_ERR_1012073_MSG);
  }
  return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name		: ns_parse_socket_open()
  Purpose	: Parse API ns_socket_open()

                  ns_socket_open ( “ID = <socket_id>”,
                                   “PROTOCOL = <TCP/UDP>”, 
                                   “ENABLE_SSL = <enable/disable>”,
                                   “HOST = <IP/DomainName:Port>”,
                                   “TIMEOUT = <msg send timeout>”,
                                   “BACKLOG = <max no. waiting connection>” 
                                 );

                  Here Inputs are in the form of key & value pair, key is 
                  called API attribute and value is called attribute value.

                  API can be provided into single as well as in multi-lines,
                  Space and Tabs will be removed from Attribute Name and Value.

  Inputs	: flow_fp   => provide file discriptor for flow file  
                  outfp     => provide file discriptor for output file, 
                               Output file made in $NS_WDIR/.tmp/ns_insxx/ 
                  flow_filename => provide flow file name 
                  sess_idx => provide session index in gSessionTable 

  Return	: 0  => On Success
                  -1 => On Error 
  Mod. Hist.	: Nisha, 20 June 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int ns_parse_socket_open(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx)
{
  char *page_end_ptr = NULL;
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  char attribute_name[128 + 1];
  char attribute_value[SOCKET_MAX_ATTR_LEN + 1];
  char pagename[SOCKET_MAX_ATTR_LEN + 1];
  char hostname[SOCKET_MAX_ATTR_LEN + 1];
  int url_idx;
  int is_new_id_flag;
  int norm_id;
  int id_flag = -1;
  int step_flag = -1;
  int protocol_flag = -1;
  int ssl_flag = -1;
  int rhost_flag = -1;
  int lhost_flag = -1;
  int timeout_flag = -1;
  int backlog_flag = -1;
  int ret;
  char *ptr = NULL;
  int hash_code;

  NSDL2_SOCKETS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  page_end_ptr = strchr(script_line, '"');

  close_quotes = page_end_ptr;
  start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  //init_socket_uri(&url_idx, flow_filename);

  socket_init(url_idx, SOCKET_REQUEST, SOPEN);

  // Process other attribute one by one
  while(1)
  {
    NSDL3_SOCKETS(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
    if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "STEP"))
    {
      NSDL2_SOCKETS(NULL, NULL, "STEP = [%s] ", attribute_value);

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      int step_len = strlen(ptr);
      if(step_len > TX_NAME_MAX_LEN)
        SCRIPT_PARSE_ERROR(script_line, "STEP name (%s) length is (%d). It can be maximum of 1024 length", ptr, step_len);

      if(match_pattern(ptr, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
        SCRIPT_PARSE_ERROR(script_line, "Name of STEP (%s) should contain alphanumeric character including"
                           " '_' but first character should be an alphabet", ptr);

      if(step_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one STEP attribute in ns_socket_open API");

      step_flag = 1;

      if((parse_and_set_pagename("ns_socket_open", "ns_socket_open", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, ptr)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if(!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_SOCKETS(NULL, NULL, "ID =  [%s] ", attribute_value);

      /*Making STEP attribute mandatory for now as now design is inappropriate with default STEP*/
      if(step_flag != 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP is a mandatory argument and should be provided in the API");

      if(strlen(attribute_value) > MAX_ID_LEN)
        SCRIPT_PARSE_ERROR(script_line, "ID value can be max of 32 character long");

      //Check for special characters found in ID value
      if(strchr(attribute_value, '@') || strchr(attribute_value, ':') || strchr(attribute_value, '|'))
        SCRIPT_PARSE_ERROR(script_line, "ID cannot have @, :, | character in it. Provide ID value without this special characters");

      if(id_flag != -1)
      {
        if(id_flag == g_cur_page)
          SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");
      }

      //ID can be alphanumeric, so getting norm id for the ID
      norm_id = nslb_get_or_gen_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value), &is_new_id_flag);

      if(!is_new_id_flag)
        SCRIPT_PARSE_ERROR(script_line, "socket ID should be unique in script");

      if(step_flag == -1)
      {
        sprintf(pagename, "TCPClientConnect_%s", attribute_value);

        if((parse_and_set_pagename("ns_socket_open", "ns_socket_open", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      }

      id_flag = g_cur_page;

      //Setting first_eurl for current_page
      gPageTable[g_cur_page].first_eurl = url_idx;

      requests[url_idx].proto.socket.norm_id = norm_id;

      if(norm_id == g_socket_vars.max_api_id_entries)
      {
        MY_REALLOC(g_socket_vars.proto_norm_id_mapping_2_action_tbl, (g_socket_vars.max_api_id_entries + DELTA_API_ID_ENTRIES) * sizeof(int),
                         "Socket API ID's", -1);
        g_socket_vars.max_api_id_entries += DELTA_API_ID_ENTRIES;
      }
      g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id] = url_idx;

      NSDL2_SOCKETS(NULL, NULL, "max_socket_conn = %d, norm_id = %d",
                    g_socket_vars.max_socket_conn, norm_id);

      g_socket_vars.max_socket_conn++;
    }
    else if(!strcasecmp(attribute_name, "PROTOCOL"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "PROTOCOL =  [%s] ", attribute_value);

      if(protocol_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one PROTOCOL parameter");

      protocol_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      if(!strcasecmp(ptr, "TCP"))
        requests[url_idx].proto.socket.open.protocol = TCP_PROTO;
      else if(!strcasecmp(ptr, "UDP"))
        requests[url_idx].proto.socket.open.protocol = UDP_PROTO;
      else
        SCRIPT_PARSE_ERROR(script_line, "Wrong Protocol Value passed, can be TCP or UDP only ");
    }
    else if(!strcasecmp(attribute_name, "SSL"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "SSL = [%s] ", attribute_value);
      if(ssl_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one SSL parameter");

      ssl_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      if(!strcasecmp(ptr, "YES"))
        requests[url_idx].proto.socket.open.ssl_flag = SSL_ENABLE;
      else if(!strcasecmp(ptr, "NO"))
        requests[url_idx].proto.socket.open.ssl_flag = SSL_DISABLE;
      else
        SCRIPT_PARSE_ERROR(script_line, "Wrong SSL Value passed, can be YES or NO only ");
    }
    else if(!strcasecmp(attribute_name, "LOCAL_HOST"))
    {
      SCRIPT_PARSE_ERROR(script_line, "Currently LOCAL_HOST is not supported for Client socket. "
              "Please re-run the test again without LOCAL_HOST");

      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "LOCAL_HOST = [%s]", attribute_value);
      if(lhost_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one LOCAL_HOST parameter");

      lhost_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      segment_line(&(requests[url_idx].proto.socket.open.local_host), ptr, 0, script_ln_no, sess_idx, flow_filename);
    }
    else if(!strcasecmp(attribute_name, "REMOTE_HOST"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "REMOTE_HOST = [%s]", attribute_value);
      if(rhost_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one REMOTE_HOST parameter");

      rhost_flag = 1;

      if(!requests[url_idx].proto.socket.open.ssl_flag)
        requests[url_idx].request_type = SOCKET_REQUEST;
      else if(requests[url_idx].proto.socket.open.ssl_flag)
        requests[url_idx].request_type = SSL_SOCKET_REQUEST;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr); 

      int len = strlen(ptr);

      if(ptr[0] == '{' && ptr[len - 1] == '}')
      {
        strncpy(hostname, ptr + 1, len - 2);
        hostname[len - 2]='\0';

        if(gSessionTable[sess_idx].var_hash_func)
          hash_code = gSessionTable[sess_idx].var_hash_func(hostname, len - 2);
        if(hash_code == -1)
          SCRIPT_PARSE_ERROR(script_line, "NS Parameter '%s' is not declared", hostname);

        requests[url_idx].is_url_parameterized = NS_URL_PARAM_VAR;
      }
      else
      {
        strncpy(hostname, ptr, len);
        hostname[len] = '\0';

        if(!strchr(hostname, ':'))
          SCRIPT_PARSE_ERROR(script_line, "'IP/Domain name' and 'Port' both are mandatory for REMOTE_HOST"
                                          " field and should be separated by ':'");

        if((hostname[0] == ':') || (hostname[len - 1] == ':'))
          SCRIPT_PARSE_ERROR(script_line, "'IP/Domain name' and 'Port' both are mandatory for REMOTE_HOST"
                                          " field and should be separated by ':'");
      }
      segment_line(&(requests[url_idx].proto.socket.open.remote_host), ptr, 0, script_ln_no, sess_idx, flow_filename);

      NSDL2_SOCKETS(NULL, NULL, "url_idx = %d, ptr = %s", url_idx, ptr);
    }
    else if(!strcasecmp(attribute_name, "TIMEOUT"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "TIMEOUT =  [%s] ", attribute_value);
      if(timeout_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one TIMEOUT parameter");
      
      timeout_flag = 1;
     
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      float time_val = atof(ptr);

      if(time_val < 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value cannot be less than 0.010 secs in ns_socket_open API");

      if(ns_is_float(ptr) == 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value is non numeric in ns_socket_open API");

      if(time_val < 0.010f || time_val > 300)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value cannot be less than 0.010 secs and greater than 300 secs in ns_socket_open API");

      requests[url_idx].proto.socket.timeout_msec = time_val * 1000;  //Converting into msecs
    }
    else if(!strcasecmp(attribute_name, "BACKLOG"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
      NSDL2_SOCKETS(NULL, NULL, "BACKLOG = [%s] ", attribute_value);
      if(backlog_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one BACKLOG parameter");
    
      backlog_flag = 1;
      int backlog;
      
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      if(nslb_atoi(ptr, &backlog))
         SCRIPT_PARSE_ERROR(script_line, "Invalid backlog value");
    
      if(backlog < 0)
        SCRIPT_PARSE_ERROR(script_line, "BACKLOG value cannot be less than 0 in ns_socket_open API");

      requests[url_idx].proto.socket.open.backlog = backlog;
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] for ns_socket_open API", attribute_name);
    }
    
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SOCKETS(NULL, NULL, "Next attribute is not found");
      break;
    }
  } //End while

  if((rhost_flag == 1) && (lhost_flag == 1) && (backlog_flag == 1))
     SCRIPT_PARSE_ERROR(script_line, "LOCAL_HOST, REMOTE_HOST and BACKLOG attribute cannot be provided for one open connection");

  if((rhost_flag == -1) && (lhost_flag == -1))
     SCRIPT_PARSE_ERROR(script_line, "Either LOCAL_HOST or REMOTE_HOST should be provided to open socket as client or server");

  if((rhost_flag == 1) && (backlog_flag == 1))
     SCRIPT_PARSE_ERROR(script_line, "BACKLOG attribute should be provided with LOCAL_HOST in case of server socket.");

  set_socket_request_type(hostname, url_idx, sess_idx);

  //Set Global variable for proto type
  if (requests[url_idx].proto.socket.open.protocol == TCP_PROTO) 
  {
    if (rhost_flag && !(global_settings->protocol_enabled & SOCKET_TCP_CLIENT_PROTO_ENABLED))
    {
      global_settings->protocol_enabled |= SOCKET_TCP_CLIENT_PROTO_ENABLED; 
    }
    else if (!(global_settings->protocol_enabled & SOCKET_TCP_SERVER_PROTO_ENABLED))
      global_settings->protocol_enabled |= SOCKET_TCP_SERVER_PROTO_ENABLED; 
  }
  else 
  {
    if (rhost_flag && !(global_settings->protocol_enabled & SOCKET_UDP_CLIENT_PROTO_ENABLED))
    {
      global_settings->protocol_enabled |= SOCKET_UDP_CLIENT_PROTO_ENABLED; 
    }
    else if (!(global_settings->protocol_enabled & SOCKET_UDP_SERVER_PROTO_ENABLED))
      global_settings->protocol_enabled |= SOCKET_UDP_SERVER_PROTO_ENABLED; 
  }

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}

int parse_and_fill_recv_msg_fmt(char *attribute_value, action_request* requests)
{
  char *msg_fmt_field[2];
  int tot_msg_fields;
  char *ptr = NULL;

  tot_msg_fields = nslb_strtok(attribute_value, ":", 2, msg_fmt_field);

  if(tot_msg_fields > 2)
    SCRIPT_PARSE_ERROR(script_line, "Only Message-Type(T) and Message-Decoding(E) can be provided in MSG_FMT attribute");

  for(int i = 0; i < tot_msg_fields; i++)
  {
    ptr = msg_fmt_field[i];
    CLEAR_WHITE_SPACE(ptr);
    CLEAR_WHITE_SPACE_FROM_END(ptr);

    switch(i)
    {
      case 0:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Message-Type is not provided. Provide a valid message type value");

        if(!strcasecmp(ptr, "T"))
          requests->proto.socket.recv.msg_fmt.msg_type = MSG_TYPE_TEXT;
        else if(!strcasecmp(ptr, "B"))
          requests->proto.socket.recv.msg_fmt.msg_type = MSG_TYPE_BINARY;
        else if(!strcasecmp(ptr, "H"))
          requests->proto.socket.recv.msg_fmt.msg_type = MSG_TYPE_HEX;
        else if(!strcasecmp(ptr, "B64"))
          requests->proto.socket.recv.msg_fmt.msg_type = MSG_TYPE_BASE64;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Message-Type is provided, it can be Text(T), Binary(B), Hex(H) or Base64(B64)");
      }
      break;
      case 1:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Decoding-Type is not provided. Provide a valid decoding type value");

        if(!strcasecmp(ptr, "B"))
          requests->proto.socket.recv.msg_fmt.msg_enc_dec = ENC_BINARY;
        else if(!strcasecmp(ptr, "H"))
          requests->proto.socket.recv.msg_fmt.msg_enc_dec = ENC_HEX;
        else if(!strcasecmp(ptr, "B64"))
          requests->proto.socket.recv.msg_fmt.msg_enc_dec = ENC_BASE64;
        else if(!strcasecmp(ptr, "T"))
          requests->proto.socket.recv.msg_fmt.msg_enc_dec = ENC_TEXT;
        else if(!strcasecmp(ptr, "N"))
          requests->proto.socket.recv.msg_fmt.msg_enc_dec = ENC_NONE;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Decoding is provided, it can be Binary(B), Hex(H), Base64(B64) or None(N)");
      }
      break;
    }
  }
  if(requests->proto.socket.recv.msg_fmt.msg_enc_dec)
  {
    switch(requests->proto.socket.recv.msg_fmt.msg_enc_dec)
    {
      case ENC_BINARY:
        switch(requests->proto.socket.recv.msg_fmt.msg_type)
        {
          case MSG_TYPE_HEX: 
            requests->proto.socket.enc_dec_cb = nslb_encode_hex_to_bin;
            break;             
          case MSG_TYPE_BASE64: 
            requests->proto.socket.enc_dec_cb = nslb_encode_base64_to_bin;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Hex and Base64 Message-Type can be decoded into Binary");
        }
        break;

      case ENC_HEX:
        switch(requests->proto.socket.recv.msg_fmt.msg_type)
        {
          case MSG_TYPE_BINARY:
            requests->proto.socket.enc_dec_cb = nslb_encode_bin_to_hex;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Binary Message-Type can be decoded into Hex");
        }
        break;

      case ENC_BASE64:
        switch(requests->proto.socket.recv.msg_fmt.msg_type)
        {
          case MSG_TYPE_BINARY:
            requests->proto.socket.enc_dec_cb = nslb_encode_bin_to_base64;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Binary Message-Type can be decoded into Base64");
        }
        break;

      case ENC_TEXT:  
        switch(requests->proto.socket.recv.msg_fmt.msg_type)
        {
          case MSG_TYPE_BASE64:
            requests->proto.socket.enc_dec_cb = nslb_decode_base64_to_text;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Base64 Message-Type can be decoded into Text");
        }
    } 
  }
  return 0;
}

int parse_and_fill_len_fmt(char *attribute_value, action_request* requests, int api_type)
{
  char *len_fmt_field[3];
  int tot_len_fields, len_bytes, ret;
  char *ptr = NULL;
  ProtoSocket_MsgFmt *socket_fmt;

  tot_len_fields = nslb_strtok(attribute_value, ":", 3, len_fmt_field);

  if(tot_len_fields > 3)
    SCRIPT_PARSE_ERROR(script_line, "Only Length-Bytes, Length-Type and Length-Endianness can be provided in LEN_FMT");

  if(api_type == SSEND)
  {
    socket_fmt = &requests->proto.socket.send.msg_fmt;
  }
  else if(api_type == SRECV)
  {
    socket_fmt = &requests->proto.socket.recv.msg_fmt;
  }

  for(int i = 0; i < tot_len_fields; i++)
  {
    //Parsing each field for Length-Bytes, Length-Type and Length-Endianness
    ptr = len_fmt_field[i];
    CLEAR_WHITE_SPACE(ptr);
    CLEAR_WHITE_SPACE_FROM_END(ptr);

    switch(i)
    {
      case 0:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Length-Bytes value is not provided. Provide a valid length-byte value");

        ret = nslb_atoi(ptr, &len_bytes);
        if(ret == -1)
        {
          SCRIPT_PARSE_ERROR(script_line, "Invalid Length-Bytes value provided in LEN_FMT, it must be an integer");
        }
   
        if(len_bytes <= 0)
          SCRIPT_PARSE_ERROR(script_line, "Length-Bytes value provided in LEN_FMT attribute, must be greater than 0");
   
        socket_fmt->len_bytes = len_bytes;
      }
      break;
      case 1:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Length-Type is not provided. Provide a valid length-type value");

        if(!strcasecmp(ptr, "T"))
          socket_fmt->len_type = LEN_TYPE_TEXT;
        else if(!strcasecmp(ptr, "B"))
          socket_fmt->len_type = LEN_TYPE_BINARY;
        else if(!strcasecmp(ptr, "H"))
          socket_fmt->len_type = LEN_TYPE_HEX;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Length-Type is provided, it can be Text(T), Binary(B) or Hex(H)");
      }  
      break;
      case 2:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Length-Endianness is not provided. Provide a valid length-endianness value");

        if(!strcasecmp(ptr, "L"))
          socket_fmt->len_endian = SLITTLE_ENDIAN;
        else if(!strcasecmp(ptr, "B"))
          socket_fmt->len_endian = SBIG_ENDIAN;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Length-Endian provided, it can be Little(L) or Big(B)");
      }
      break;
    }
  }
  return 0;
}

int parse_and_fill_msg_fmt(char *attribute_value, action_request* requests)
{
  char *msg_fmt_field[2];
  int tot_msg_fields;
  char *ptr = NULL;

  tot_msg_fields = nslb_strtok(attribute_value, ":", 2, msg_fmt_field);

  if(tot_msg_fields > 2)
    SCRIPT_PARSE_ERROR(script_line, "Only Message-Type(T) and Message-Encoding(E) can be provided in MSG_FMT attribute");

  for(int i = 0; i < tot_msg_fields; i++)
  {
    ptr = msg_fmt_field[i];
    CLEAR_WHITE_SPACE(ptr);
    CLEAR_WHITE_SPACE_FROM_END(ptr);

    switch(i)
    {
      case 0:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Message-Type is not provided. Provide a valid message type value");

        if(!strcasecmp(ptr, "T"))
          requests->proto.socket.send.msg_fmt.msg_type = MSG_TYPE_TEXT;
        else if(!strcasecmp(ptr, "B"))
          requests->proto.socket.send.msg_fmt.msg_type = MSG_TYPE_BINARY;
        else if(!strcasecmp(ptr, "H"))
          requests->proto.socket.send.msg_fmt.msg_type = MSG_TYPE_HEX;
        else if(!strcasecmp(ptr, "B64"))
          requests->proto.socket.send.msg_fmt.msg_type = MSG_TYPE_BASE64;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Message-Type is provided, it can be Text(T), Binary(B), Hex(H) or Base64(B64)");
      }
      break;
      case 1:
      {
        if(ptr[0] == '\0')
          SCRIPT_PARSE_ERROR(script_line, "Encoding-Type is not provided. Provide a valid encoding type value");

        if(!strcasecmp(ptr, "B"))
          requests->proto.socket.send.msg_fmt.msg_enc_dec = ENC_BINARY;
        else if(!strcasecmp(ptr, "H"))
          requests->proto.socket.send.msg_fmt.msg_enc_dec = ENC_HEX;
        else if(!strcasecmp(ptr, "B64"))
          requests->proto.socket.send.msg_fmt.msg_enc_dec = ENC_BASE64;
        else if(!strcasecmp(ptr, "N"))
          requests->proto.socket.send.msg_fmt.msg_enc_dec = ENC_NONE;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid Encoding is provided, it can be Binary(B), Hex(H), Base64(B64) or None(N)");
      }
      break;
    }
  }
  if(requests->proto.socket.send.msg_fmt.msg_enc_dec)
  {
    switch(requests->proto.socket.send.msg_fmt.msg_enc_dec)
    {
      case ENC_BINARY:
        switch(requests->proto.socket.send.msg_fmt.msg_type)
        {
          case MSG_TYPE_HEX: 
            requests->proto.socket.enc_dec_cb = nslb_encode_hex_to_bin;
            break;             
          case MSG_TYPE_BASE64:
            requests->proto.socket.enc_dec_cb = nslb_encode_base64_to_bin;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Hex and Base64 Message-Type can be converted into Binary");
        }
        break;

      case ENC_HEX:
        switch(requests->proto.socket.send.msg_fmt.msg_type)
        {
          case MSG_TYPE_BINARY:
            requests->proto.socket.enc_dec_cb = nslb_encode_bin_to_hex;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Binary Message-Type can be converted into Hex");
        }
        break;

      case ENC_BASE64:
        switch(requests->proto.socket.send.msg_fmt.msg_type)
        {
          case MSG_TYPE_TEXT: 
            requests->proto.socket.enc_dec_cb = nslb_encode_text_to_base64;
            break;
          case MSG_TYPE_BINARY:
            requests->proto.socket.enc_dec_cb = nslb_encode_bin_to_base64;
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Only Text and Binary Message-Type can be converted into Base64");
        }
        break;
    } 
  }
  return 0;
}

void save_and_segment_socket_msg(int send_idx, char *msg_buf, int noparam_flag, int *script_ln_no, int sess_idx, char *file_name)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called, send_tbl_idx = %d, noparam_flag = %d", send_idx, noparam_flag);

  //Message can be included through CAVINCLUDE, CAVINCLUDE_NOPARAM and CAV_REPEAT
  if(noparam_flag)
  {
    segment_line_noparam(&requests[send_idx].proto.socket.send.buffer, msg_buf, sess_idx);
  }
  else
  {
    segment_line(&requests[send_idx].proto.socket.send.buffer, msg_buf, 0, *script_ln_no, sess_idx, file_name);
  }
}

int socket_set_post_msg(int send_idx, int sess_idx, int *script_ln_no, char *cap_fname)
{
  char *fname, fbuf[8192];
  int ffd, rlen, noparam_flag = 0;

  NSDL2_SOCKETS(NULL, NULL, "Method called, send_idx = %d, sess_idx = %d", send_idx, sess_idx);

  if(cur_post_buf_len <= 0)
    return NS_PARSE_SCRIPT_SUCCESS; //No BODY, exit

  //Removing trailing ,\n from post buf.

  if(validate_body_and_ignore_last_spaces_c_type(sess_idx) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //Check if BODY is provided using $CAVINCLUDE$= directive
  if((strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0) || (strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0))
  {
    if(strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
    {
       fname = g_post_buf + 21;
       noparam_flag = 1;
    }
    else
      fname = g_post_buf + 13;
    
    if(fname[0] != '/')
    {
      /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
      sprintf(fbuf, "%s/%s/%s", GET_NS_TA_DIR(),
              get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
      fname = fbuf;
    }
    
    ffd = open (fname, O_RDONLY|O_CLOEXEC);
    if (!ffd)
    {
      NSDL4_SOCKETS("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, fname);
      return NS_PARSE_SCRIPT_ERROR;
    }
    cur_post_buf_len = 0;
    while(1)
    {
      rlen = read (ffd, fbuf, 8192);
      if(rlen > 0)
      {
        if(copy_to_post_buf(fbuf, rlen))
        {
          NSDL4_SOCKETS(NULL, NULL,"%s(): Request BODY could not alloccate mem for %s\n", (char*)__FUNCTION__, fname);
          return NS_PARSE_SCRIPT_ERROR;
        }
        continue;
      }
      else if(rlen == 0)
      {
        break;
      }
      else
      {
        perror("reading CAVINCLUDE BODY");
        NSDL4_SOCKETS(NULL, NULL, "%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, fname);
        return NS_PARSE_SCRIPT_ERROR;
      }
    }
    close (ffd);
  }
    
  if (cur_post_buf_len)
  {
    save_and_segment_socket_msg(send_idx, g_post_buf, noparam_flag, script_ln_no, sess_idx, cap_fname);
    NSDL2_SOCKETS(NULL, NULL, "Send data at send_idx = %d, message.seg_start = %lu,"
                                "message.num_ernties = %d",
                                send_idx, requests[send_idx].proto.socket.send.buffer.seg_start,
                                requests[send_idx].proto.socket.send.buffer.num_entries);
  } 

  return NS_PARSE_SCRIPT_SUCCESS;
}

int parse_socket_send_parameters(FILE *flow_fp, FILE *outfp, char *flow_file, char *flow_outfile, char *start_quotes, int sess_idx)
{
  int norm_id;
  int step_flag = -1;
  int id_flag = -1;
  int len_format_flag = -1;
  int msg_format_flag = -1;
  int msg_prefix_flag = -1;
  int msg_suffix_flag = -1;
  int message_flag = -1;
  int timeout_flag = -1;
  int ret, send_idx;
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char pagename[SOCKET_MAX_ATTR_LEN + 1];
  char *close_quotes = NULL;
  char *ptr = NULL;

  NSDL2_SOCKETS(NULL, NULL, "Method Called  starting_quotes = %s", start_quotes);

  create_requests_table_entry(&send_idx);

  socket_init(send_idx, SOCKET_REQUEST, SSEND);

  while(1)
  {
    // It will parse from , to next starting quote.
    if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    NSDL2_SOCKETS(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "STEP"))
    {
      NSDL2_SOCKETS(NULL, NULL, "STEP = [%s] ", attribute_value);

      if(id_flag == 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP attribute should be at first position in API");

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      int step_len = strlen(ptr);
      if(step_len > TX_NAME_MAX_LEN)
        SCRIPT_PARSE_ERROR(script_line, "STEP name (%s) length is (%d). It can be maximum of 1024 length", ptr, step_len);

      if(match_pattern(ptr, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
        SCRIPT_PARSE_ERROR(script_line, "Name of STEP (%s) should contain alphanumeric character including"
                           " '_' but first character should be an alphabet", ptr);

      if(step_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one STEP attribute in ns_socket_open API");

      step_flag = 1;

      if((parse_and_set_pagename("ns_socket_send", "ns_socket_send", flow_fp, flow_filename,
                      script_line, outfp, flow_outfile, sess_idx, &start_quotes, ptr)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if(!strcasecmp(attribute_name, "ID"))
    {
      if(step_flag != 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP is a mandatory argument and should be provided in the API");

      NSDL2_SOCKETS(NULL, NULL, "ID =  [%s] ", attribute_value);

      if(id_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");

      norm_id = nslb_get_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value));

      if(norm_id == -1)
      {
        SCRIPT_PARSE_ERROR(script_line, "Invalid socket id [%s], provided", attribute_value);
      }
      else if(norm_id == -2)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no socket opened for provided ID value", attribute_value);
      }
      else
      {
        CHECK_AND_RETURN_IF_EXCLUDE_FLAG_SET;
   
        if(step_flag == -1)
        {
          sprintf(pagename, "TCPSend_%s", attribute_value);

          if((parse_and_set_pagename("ns_socket_send", "ns_socket_send", flow_fp, flow_filename,
                      script_line, outfp, flow_outfile, sess_idx, &start_quotes, pagename)) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;
        }

        requests[send_idx].proto.socket.norm_id = norm_id;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = send_idx;
      }
      id_flag = 1;
    }
    else if(!strcasecmp(attribute_name, "LEN_FMT") || !strcasecmp(attribute_name, "LEN_FORMAT") ) 
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "LEN_FMT = [%s] ", attribute_value);

      if(len_format_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one LEN_FMT parameter");

      len_format_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      parse_and_fill_len_fmt(ptr, &requests[send_idx], SSEND);

      if(requests[send_idx].proto.socket.send.msg_fmt.len_type == LEN_TYPE_BINARY)
      {
        NSDL4_SOCKETS(NULL, NULL, "len_bytes = %d", requests[send_idx].proto.socket.send.msg_fmt.len_bytes);

        switch(requests[send_idx].proto.socket.send.msg_fmt.len_bytes)
        {
          case NS_SOCKET_MSGLEN_BYTES_CHAR:
            requests[send_idx].proto.socket.send.buffer_len = UCHAR_MAX;             // UCHAR_MAX = 255
            break;
          case NS_SOCKET_MSGLEN_BYTES_SHORT:
            requests[send_idx].proto.socket.send.buffer_len = USHRT_MAX;            // USHRT_MAX = 65535 = ~65K
            break;
          case NS_SOCKET_MSGLEN_BYTES_INT:
            requests[send_idx].proto.socket.send.buffer_len = UINT_MAX;             // UINT_MAX = 4294967295 = ~4GB
            break;
          case NS_SOCKET_MSGLEN_BYTES_LONG:
            requests[send_idx].proto.socket.send.buffer_len = ULONG_MAX;            // ULONG_MAX = 18446744073709551615 = ~18446744 TB
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Wrong Length-Bytes value passed, it can be 1,2,4 or 8 only");
        }
      }
      else if(requests[send_idx].proto.socket.send.msg_fmt.len_type == LEN_TYPE_TEXT)
      {
        if(requests[send_idx].proto.socket.send.msg_fmt.len_bytes > 20)
          SCRIPT_PARSE_ERROR(script_line, "Maximum value of Length-Bytes for LEN_TYPE Text can be 20");

        // Max possible value in provided number of digits
        requests[send_idx].proto.socket.send.buffer_len = pow(10, requests[send_idx].proto.socket.send.msg_fmt.len_bytes) - 1;
      }
    }
    else if(!strcasecmp(attribute_name, "MSG_FMT") || !strcasecmp(attribute_name, "MSG_FORMAT") ) 
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "MSG_FORMAT = [%s] ", attribute_value);

      if(msg_format_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MSG_FMT parameter");

      msg_format_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      parse_and_fill_msg_fmt(ptr, &requests[send_idx]);
    }
    else if(!strcasecmp(attribute_name, "PREFIX"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "PREFIX = [%s] ", attribute_value);
      if(msg_prefix_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one PREFIX parameter");

      msg_prefix_flag = 1;
  
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      segment_line(&(requests[send_idx].proto.socket.send.msg_fmt.prefix), ptr, 0, script_ln_no, sess_idx, flow_filename);
    }
    else if(!strcasecmp(attribute_name, "SUFFIX"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "SUFFIX = [%s] ", attribute_value);
      if(msg_suffix_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one SUFFIX parameter");

      msg_suffix_flag = 1;
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      segment_line(&(requests[send_idx].proto.socket.send.msg_fmt.suffix), ptr, 0, script_ln_no, sess_idx, flow_filename);
    }
    else if(!strcasecmp(attribute_name, "MESSAGE"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "MESSAGE = [%s] ", attribute_value);

      if(message_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MESSAGE parameter");

      //Send data buffer can be parameterized.
      if(extract_buffer(flow_fp, script_line, &close_quotes, flow_filename) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

      if(socket_set_post_msg(send_idx, sess_idx, &script_ln_no, flow_filename) == NS_PARSE_SCRIPT_ERROR)
      {
        NSDL2_SOCKETS(NULL, NULL, "Send Table data at socket_send_id = %d, message.seg_start = %lu, message.num_ernties = %d",
                                   send_idx, requests[send_idx].proto.socket.send.buffer.seg_start,
                                   requests[send_idx].proto.socket.send.buffer.num_entries);

        return NS_PARSE_SCRIPT_ERROR;
      }
      message_flag = 1;
    }
    else if(!strcasecmp(attribute_name, "TIMEOUT"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "TIMEOUT = [%s] ", attribute_value);
      if(timeout_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one TIMEOUT parameter");

      timeout_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      float time_val = atof(ptr);

      if(time_val < 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value cannot be less than 0 in ns_socket_send API");

      if(ns_is_float(ptr) == 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value is non numeric in ns_socket_send API");

      requests[send_idx].proto.socket.timeout_msec = time_val * 1000;
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] in ns_socket_send API", attribute_name);
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SOCKETS(NULL, NULL, "Next attribute is not found");
      break;
    }
  } 

  if(id_flag == -1)
     SCRIPT_PARSE_ERROR(script_line, "ID parameter is mandatory, please provide a valid ID");

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_socket_send(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx)
{
  char *id_end_ptr = NULL;

  //To store send msg into global buffer g_post_buf
  init_post_buf(); 

  // This will point to the starting quote '"'.
  id_end_ptr = strchr(script_line, '"');  // Staring of API "
  NSDL4_SOCKETS(NULL, NULL, "id_end_ptr = [%s], script_line = [%s], flowout_filename = %s", id_end_ptr, script_line, flowout_filename);
        
  if(parse_socket_send_parameters(flow_fp, outfp, flow_filename, flowout_filename, id_end_ptr, sess_idx) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method");

  return NS_PARSE_SCRIPT_SUCCESS;
}

int parse_and_fill_contains_attribute(char *attribute_value, action_request* requests, char *flow_filename, int sess_idx)
{
  char *contains_fmt_field[3];
  int tot_fields;
  char *ptr = NULL;

  tot_fields = nslb_strtok(attribute_value, ":", 3, contains_fmt_field);

  if(tot_fields > 3)
    SCRIPT_PARSE_ERROR(script_line, "Only Pattern, Place(P) and Action(A) can be provided in CONTAINS attribute");

  for(int i = 0; i < tot_fields; i++)
  {
    ptr = contains_fmt_field[i];
    CLEAR_WHITE_SPACE(ptr);
    CLEAR_WHITE_SPACE_FROM_END(ptr);

    switch(i)
    {
      case 0:
      {
        segment_line(&(requests->proto.socket.recv.msg_contains), ptr, 0, script_ln_no, sess_idx, flow_filename);
      }
      break;
      case 1:
      {
        if((ptr[0] == 'S') || (ptr[0] == 's'))
          requests->proto.socket.recv.msg_contains_ord = CONTAINS_START;
        else if((ptr[0] == 'I') || (ptr[0] == 'i'))
          requests->proto.socket.recv.msg_contains_ord = CONTAINS_INSIDE;
        else if((ptr[0] == 'E') || (ptr[0] == 'e'))
          requests->proto.socket.recv.msg_contains_ord = CONTAINS_END;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid CONTAINS Place provided, it can be Start(S), Inside(I) or End(E)");
      }
      break;
      case 2:
      {
        if(!strcasecmp(ptr, "SF"))
          requests->proto.socket.recv.msg_contains_action = SAVE_ON_FOUND;
        else if(!strcasecmp(ptr, "SNF"))
          requests->proto.socket.recv.msg_contains_action = SAVE_ON_NOT_FOUND;
        else if(!strcasecmp(ptr, "DF"))
          requests->proto.socket.recv.msg_contains_action = DISCARD_ON_FOUND;
        else if(!strcasecmp(ptr, "DNF"))
          requests->proto.socket.recv.msg_contains_action = DISCARD_ON_NOT_FOUND;
        else
          SCRIPT_PARSE_ERROR(script_line, "Invalid CONTAINS Action provided, it can be SAVE_ON_FOUND(SF), SAVE_ON_NOT_FOUND(SNF),"
                             " DISCARD_ON_FOUND(DF) or DISCARD_ON_NOT_FOUND(DNF)");
      }
    }
  }
  return 0;
}

/****************************************************************************
ns_socket_recv ( “ID = <socket_id>”, 
                 “READ_BYTES = <no. of bytes read>”
		 “MESSAGE_FORMAT = Length-Bytes: <1/2/4/8>; 
                                   Length-Type: <text/binary>;
                                   Prefix-Type: <text/binary/hex/base64>;
                                   Decoding: <binary/hex/base64/none>;
                                   Partial-Handling: <continue/discard/close>”,
                 “MESSAGE_PREFIX = <prefix msg>”,
                 “MESSAGE_SUFFIX = <suffix msg or end marker>”,
                 “MESSAGE = <buffer to store message>”,
                 “TIMEOUT_TTFB = <max waiting time for first bytes>”,
                 “TIMEOUT_IDLE = <max waiting time after TTFB>”, 
                 “ACTION = <save/discard (found/not found)>”);
*****************************************************************************/

int ns_parse_socket_recv(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx)
{
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char pagename[SOCKET_MAX_ATTR_LEN + 1];
  int norm_id, id_flag = -1, msg_format_flag = -1, read_bytes_flag = -1, msg_suffix_flag = -1;
  int message_flag = -1, timeout_ttfb_flag = -1, timeout_flag = -1, contains_flag = -1, step_flag = -1, len_format_flag = -1;
  int epolicy_flag = -1;
  int ret, recv_idx;
  char end_policy = 0x00;
  char *ptr = NULL;

  start_quotes = strchr(script_line, '"');

  NSDL2_SOCKETS(NULL, NULL, "Method Called  starting_quotes = %s", start_quotes);

  create_requests_table_entry(&recv_idx);
  socket_init(recv_idx, SOCKET_REQUEST, SRECV);

  while(1)
  {
    // It will parse from , to next starting quote.
    if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    NSDL2_SOCKETS(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "STEP"))
    {
      NSDL2_SOCKETS(NULL, NULL, "STEP = [%s] ", attribute_value);

      if(id_flag == 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP attribute should be at first position in API");

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      int step_len = strlen(ptr);
      if(step_len > TX_NAME_MAX_LEN)
        SCRIPT_PARSE_ERROR(script_line, "STEP name (%s) length is (%d). It can be maximum of 1024 length", ptr, step_len);

      if(match_pattern(ptr, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
        SCRIPT_PARSE_ERROR(script_line, "Name of STEP (%s) should contain alphanumeric character including"
                           " '_' but first character should be an alphabet", ptr);

      if(step_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one STEP attribute in ns_socket_open API");

      step_flag = 1;

      if((parse_and_set_pagename("ns_socket_recv", "ns_socket_recv", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &start_quotes, ptr)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if(!strcasecmp(attribute_name, "ID"))
    {
      if(step_flag != 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP is a mandatory argument and should be provided in the API");

      NSDL2_SOCKETS(NULL, NULL, "ID =  [%s] ", attribute_value);

      if(id_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");

      norm_id = nslb_get_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value));

      if(norm_id == -1)
      {
        SCRIPT_PARSE_ERROR(script_line, "Invalid socket id [%s], provided", attribute_value);
      }
      else if(norm_id == -2)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no socket opened for provided ID value", attribute_value);
      }
      else
      {
        CHECK_AND_RETURN_IF_EXCLUDE_FLAG_SET;

        if(step_flag == -1)
        {
          sprintf(pagename, "TCPReceive_%s", attribute_value);

          if((parse_and_set_pagename("ns_socket_recv", "ns_socket_recv", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &start_quotes, pagename)) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;
        }
        requests[recv_idx].proto.socket.norm_id = norm_id;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = recv_idx;
      }
      id_flag = 1;
    }
    else if(!strcasecmp(attribute_name, "MSG_LEN"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "MSG_LEN = [%s]", attribute_value);

      if(read_bytes_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MSG_LEN parameter");

      read_bytes_flag = 1;
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      long bytes_val = atol(ptr);

      if(bytes_val <= 0)
        SCRIPT_PARSE_ERROR(script_line, "MSG_LEN value must be greater than 0 in ns_socket_recv API");

      if(ns_is_numeric(ptr) == 0)
        SCRIPT_PARSE_ERROR(script_line, "MSG_LEN value is non numeric in ns_socket_recv API");

      requests[recv_idx].proto.socket.recv.buffer_len = bytes_val;

      end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES;
    }
    else if(!strcasecmp(attribute_name, "LEN_FMT") || !strcasecmp(attribute_name, "LEN_FORMAT"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
 
      NSDL2_SOCKETS(NULL, NULL, "LEN_FMT = [%s] ", attribute_value);

      if(len_format_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MSG_FORMAT parameter");

       len_format_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      parse_and_fill_len_fmt(ptr, &requests[recv_idx], SRECV);

      end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES;

      if(requests[recv_idx].proto.socket.recv.msg_fmt.len_type == LEN_TYPE_BINARY)
      {
        NSDL4_SOCKETS(NULL, NULL, "requests[recv_idx].proto.socket.recv.rlen_bytes = %d", 
                      requests[recv_idx].proto.socket.recv.msg_fmt.len_bytes);
 
        switch(requests[recv_idx].proto.socket.recv.msg_fmt.len_bytes)
        {
          case NS_SOCKET_MSGLEN_BYTES_CHAR:
            requests[recv_idx].proto.socket.recv.buffer_len = UCHAR_MAX;             // UCHAR_MAX = 255
            break;
          case NS_SOCKET_MSGLEN_BYTES_SHORT:
            requests[recv_idx].proto.socket.recv.buffer_len = USHRT_MAX;            // USHRT_MAX = 65535 = ~65K
            break;
          case NS_SOCKET_MSGLEN_BYTES_INT:
            requests[recv_idx].proto.socket.recv.buffer_len = UINT_MAX;             // UINT_MAX = 4294967295 = ~4GB
            break;
          case NS_SOCKET_MSGLEN_BYTES_LONG:
            requests[recv_idx].proto.socket.recv.buffer_len = ULONG_MAX;            // ULONG_MAX = 18446744073709551615 = ~18446744 TB
            break;
          default:
            SCRIPT_PARSE_ERROR(script_line, "Wrong Length-Bytes value passed for LEN_TYPE Binary, it can be 1,2,4 or 8 only");
        }
      }
      else if(requests[recv_idx].proto.socket.recv.msg_fmt.len_type == LEN_TYPE_TEXT)
      {
        // Max possible value in provided number of digits
        if(requests[recv_idx].proto.socket.recv.msg_fmt.len_bytes > 20)
          SCRIPT_PARSE_ERROR(script_line, "Maximum Length-Bytes value for LEN_TYPE Text can be 20");

        requests[recv_idx].proto.socket.recv.buffer_len = pow(10, requests[recv_idx].proto.socket.recv.msg_fmt.len_bytes) - 1;
      }
    }
    else if(!strcasecmp(attribute_name, "MSG_FMT") || !strcasecmp(attribute_name, "MSG_FORMAT"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;
 
      NSDL2_SOCKETS(NULL, NULL, "MESSAGE_FORMAT = [%s] ", attribute_value);

      if(msg_format_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MSG_FORMAT parameter");

       msg_format_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      parse_and_fill_recv_msg_fmt(ptr, &requests[recv_idx]);
    }
    else if(!strcasecmp(attribute_name, "DELIMITER"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "DELIMITER = [%s]", attribute_value);
      if(msg_suffix_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one DELIMITER parameter");

      msg_suffix_flag = 1;
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      segment_line(&(requests[recv_idx].proto.socket.recv.msg_fmt.suffix), ptr, 0, script_ln_no, sess_idx, flow_filename);

      end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX;
    }
    else if(!strcasecmp(attribute_name, "END_POLICY"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "END_POLICY = [%s] ", attribute_value);

      if(epolicy_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one END_POLICY parameter");

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      if(!strcasecmp(ptr, "NONE"))
        end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_NONE;
      else if(!strcasecmp(ptr, "TIMEOUT"))
        end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_TIMEOUT;
      else if(!strcasecmp(ptr, "CLOSE"))
        end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_CLOSE;
      else
        SCRIPT_PARSE_ERROR(script_line, "Incorrect END_POLICY provided, it can be NONE, TIMEOUT or CLOSE");
    }
    else if(!strcasecmp(attribute_name, "MESSAGE"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "MESSAGE = [%s] ", attribute_value);

      if(message_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one MESSAGE parameter");

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      int len = strlen(ptr);
    
      if(ptr[0] == '{' && ptr[len - 1] == '}') 
      {
        ptr++;  //Point after '{'
        ptr[len - 2] = '\0';
        NSDL2_SOCKETS(NULL, NULL, "Value after removing '{' and '}' = %s", ptr);
      }
      else
      {
        SCRIPT_PARSE_ERROR(script_line, "MESSAGE value should be a NS parameter and should be passed within '{' and '}'");
      }

      if(gSessionTable[sess_idx].var_hash_func(ptr, strlen(ptr)) == -1)
      {
        SCRIPT_PARSE_ERROR(script_line, "Wrong MESSAGE value, not a NS Parameter. It should be a NS parameter only");
      }

      if((requests[recv_idx].proto.socket.recv.buffer = copy_into_big_buf(ptr, 0)) == -1)
      {
        NSDL2_SOCKETS(NULL, NULL, "Error: failed copying data '%s' into big buffer", ptr);
        SCRIPT_PARSE_ERROR(script_line, "failed copying data '%s' into big buffer", ptr);
      }
      NSDL2_SOCKETS(NULL, NULL, "After copying MESSAGE param name into big buf, requests[api_id].proto.socket.rmsg = %s",
                                RETRIEVE_BUFFER_DATA(requests[recv_idx].proto.socket.recv.buffer));
      message_flag = 1;
    }
    else if(!strcasecmp(attribute_name, "TTFB"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "TTFB = [%s] ", attribute_value);
      if(timeout_ttfb_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one TTFB parameter");
  
      timeout_ttfb_flag = 1;
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      float rttfb_timeout = atof(ptr);

      if(rttfb_timeout < 0)
        SCRIPT_PARSE_ERROR(script_line, "TTFB value cannot be less than 0 in ns_socket_recv API");

      if(ns_is_float(ptr) == 0)
        SCRIPT_PARSE_ERROR(script_line, "TTFB value is non numeric in ns_socket_recv API");

      requests[recv_idx].proto.socket.recv.fb_timeout_msec = rttfb_timeout * 1000;
    } 
    else if(!strcasecmp(attribute_name, "TIMEOUT"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "TIMEOUT = [%s] ", attribute_value);
      if(timeout_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one TIMEOUT parameter");
  
      timeout_flag = 1;
      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      float timeout = atof(ptr);

      if(timeout < 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value cannot be less than 0 in ns_socket_recv API");

      if(ns_is_float(ptr) == 0)
        SCRIPT_PARSE_ERROR(script_line, "TIMEOUT value is non numeric in ns_socket_recv API");

      requests[recv_idx].proto.socket.timeout_msec = timeout * 1000;
    } 
    else if(!strcasecmp(attribute_name, "CONTAINS"))
    {
      CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

      NSDL2_SOCKETS(NULL, NULL, "CONTAINS = [%s]", attribute_value);
      if(contains_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one CONTAINS parameter");
  
      contains_flag = 1;

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);
     
      segment_line(&(requests[recv_idx].proto.socket.recv.msg_contains), ptr, 0, script_ln_no, sess_idx, flow_filename);
      end_policy |= NS_PROTOSOCKET_READ_ENDPOLICY_CONTAINS;
    } 
    else
    { 
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] in ns_socket_recv API", attribute_name);
    }
  
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SOCKETS(NULL, NULL, "Next attribute is not found");
      break;
    }
  }

  if(id_flag == -1)
     SCRIPT_PARSE_ERROR(script_line, "ID parameter is mandatory, please provide a valid ID");

  if(message_flag == -1)
     SCRIPT_PARSE_ERROR(script_line, "MESSAGE parameter is mandatory for ns_socket_recv API to store output of read,"
                                     " please provide a valid NS parameter");

  switch(end_policy)
  {
    case 0x00:
      break;
    case 0x01:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES;
      break;
    case 0X02:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES;  
      break;
    case 0x04:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX;
      break;
    case 0x08:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_TIMEOUT;
      break;
    case 0x10:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_CLOSE;
      break;
    case 0x20:
      requests[recv_idx].proto.socket.recv.end_policy = NS_PROTOSOCKET_READ_ENDPOLICY_CONTAINS;
      break;
    default:
      SCRIPT_PARSE_ERROR(script_line, "End Policy cannot be given more than one. It can be one of 'MSG_LEN', 'LEN_FMT'"
                     ", 'DELIMITER', 'CONTAINS', 'ENDPOLICY=TIMEOUT' or 'ENDPOLICY=CLOSE'.");
  }

  NSDL2_SOCKETS(NULL, NULL, "PROTO-SOCKET: read end policy = %d, for api_id = %d", 
                             requests[recv_idx].proto.socket.recv.end_policy, recv_idx);
 
  return 0;
}

int ns_parse_socket_close(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  int id_flag = -1, step_flag = -1;
  int norm_id, close_idx;
  int ret;
  char *ptr;
  char pagename[SOCKET_MAX_ATTR_LEN + 1];

  NSDL2_SOCKETS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  start_quotes = strchr(script_line, '"'); //This will point to the starting quote '"'.

  create_requests_table_entry(&close_idx);
  socket_init(close_idx, SOCKET_REQUEST, SCLOSE);

  while(1)
  {
    NSDL3_SOCKETS(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);

    if((ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "STEP"))
    {
      NSDL2_SOCKETS(NULL, NULL, "STEP = [%s] ", attribute_value);

      if(id_flag == 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP attribute should be at first position in API");

      ptr = attribute_value;
      CLEAR_WHITE_SPACE(ptr);
      CLEAR_WHITE_SPACE_FROM_END(ptr);

      int step_len = strlen(ptr);
      if(step_len > TX_NAME_MAX_LEN)
        SCRIPT_PARSE_ERROR(script_line, "STEP name (%s) length is (%d). It can be maximum of 1024 length", ptr, step_len);

      if(match_pattern(ptr, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
        SCRIPT_PARSE_ERROR(script_line, "Name of STEP (%s) should contain alphanumeric character including"
                           " '_' but first character should be an alphabet", ptr);

      if(step_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Cannot have more than one STEP attribute in ns_socket_open API");

      step_flag = 1;

      if((parse_and_set_pagename("ns_socket_close", "ns_socket_close", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &start_quotes, ptr)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    else if(!strcasecmp(attribute_name, "ID"))
    {
      if(step_flag != 1)
        SCRIPT_PARSE_ERROR(script_line, "STEP is a mandatory argument and should be provided in the API");

      NSDL2_SOCKETS(NULL, NULL, "ID [%s]", attribute_value);

      if(id_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one socket ID parameter");

      norm_id = nslb_get_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value));

      if(norm_id == -1)
      {
        SCRIPT_PARSE_ERROR(script_line, "Invalid socket id [%s], provided", attribute_value);
      }
      else if(norm_id == -2)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no socket opened for provided ID value", attribute_value);
      }
      else
      {
        CHECK_AND_RETURN_IF_EXCLUDE_FLAG_SET;
   
        NSDL2_SOCKETS(NULL, NULL, "norm_id = %d", norm_id);

        if(step_flag == -1)
        {
          sprintf(pagename, "TCPClose_%s", attribute_value);

          if((parse_and_set_pagename("ns_socket_close", "ns_socket_close", flow_fp, flow_filename,
                      script_line, outfp, flowout_filename, sess_idx, &start_quotes, pagename)) == NS_PARSE_SCRIPT_ERROR)
          return NS_PARSE_SCRIPT_ERROR;
        }
        requests[close_idx].proto.socket.norm_id = norm_id;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = close_idx;
      }
      id_flag = 1;
    }
    else
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] in ns_socket_close API", attribute_name);

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SOCKETS(NULL, NULL, "Next attribute is not found");
      break;
    }
  }

  if(id_flag != 1)
    SCRIPT_PARSE_ERROR(script_line, "ID parameter does not exist in ns_socket_close");

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}


//======================= || Socket APIs ||====================================

int ns_socket_enable_ex(int action_idx, char *operation)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called, [SocketEnable] action_idx = %d, operation = %s", action_idx, operation);
  if(!strcmp(operation, "ENABLE_RECV"))
  {
    ns_socket_modify_epoll(NULL, action_idx, EPOLLIN);
    g_socket_vars.socket_disable &= ~SOCKET_DISABLE_RECV;
  }
  else if(!strcmp(operation, "ENABLE_SEND"))
  {
    //ns_socket_modify_epoll(NULL, sock_id, EPOLLOUT);
    g_socket_vars.socket_disable &= ~SOCKET_DISABLE_SEND;
  }
  else if(!strcmp(operation, "ENABLE_SEND_RECV"))
  {
    ns_socket_modify_epoll(NULL, action_idx, EPOLLIN|EPOLLOUT);
    g_socket_vars.socket_disable &= ~(SOCKET_DISABLE_RECV|SOCKET_DISABLE_SEND);
  }
  else
  {
    fprintf(stderr, "Operation [%s] is not valid. It can be "
            "'ENABLE_SEND', 'ENABLE_RECV' or 'ENABLE_SEND_RECV' only", 
             operation);
  }

  return 0;
}

int ns_socket_disable_ex(int action_idx, char *operation)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called, [SocketDisable] action_idx = %s, operation = %s", action_idx, operation);

  if(!strcmp(operation, "DISABLE_RECV"))
  {
    ns_socket_modify_epoll(NULL, action_idx, EPOLLOUT);
    g_socket_vars.socket_disable |= SOCKET_DISABLE_RECV;
  }
  else if(!strcmp(operation, "DISABLE_SEND"))
  {
    //TODO: On remving EPOLLOUT we are unable to disable writing as on disabling just remove event not restric to write on event
    //ns_socket_modify_epoll(NULL, sock_id, EPOLLIN);
    g_socket_vars.socket_disable |= SOCKET_DISABLE_SEND;
  }
  else if(!strcmp(operation, "DISABLE_SEND_RECV"))
  {
    ns_socket_modify_epoll(NULL, action_idx, EPOLLOUT);
    //ns_socket_modify_epoll(NULL, sock_id, -1);
    g_socket_vars.socket_disable |= SOCKET_DISABLE_RECV|SOCKET_DISABLE_SEND;
  }
  else
  {
    fprintf(stderr, "Operation [%s] is not valid. It can be "
            "'DISABLE_SEND', 'DISABLE_RECV' or 'DISABLE_SEND_RECV' only", 
             operation);
  }

  NSDL2_SOCKETS(NULL, NULL, "Method end, g_socket_vars.socket_disable = 0x%x", g_socket_vars.socket_disable);

  return 0;
}

int ns_socket_ext(VUser *vptr, int socket_api_id, int socket_api_flag)
{
  NSDL2_SOCKETS(vptr, NULL, "Method called, socket_api_id = %d, socket_api_flag = %d", socket_api_id, socket_api_flag);

  vptr->page_status = NS_REQUEST_OK;

  if(socket_api_flag == 0) //send api
    vut_add_task(vptr, VUT_SOCKET_SEND);
  else if(socket_api_flag == 1) //close api 
    vut_add_task(vptr, VUT_SOCKET_CLOSE);

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    switch_to_nvm_ctx(vptr, "[SocketStats] Switching VUser->NVM, Send/Close start");
  }

  return vptr->page_status;
}

void ns_vector_to_buf()
{
  int i, total_len;
  char *to_fill = NULL;

  NSDL2_SOCKETS(NULL, NULL, "Method called");

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    total_len += g_req_rep_io_vector.vector[i].iov_len;
  }

  if(socket_send_buffer == NULL || (send_buff_len <= total_len))
  {
    //+128 bytes extra memory just to maintain minium memory allocation
    send_buff_len += total_len;
    MY_REALLOC(socket_send_buffer, send_buff_len, "Realocate memory to socket_send_buffer", -1);
  }

  //Memset buffer before reuse
  memset(socket_send_buffer, 0, send_buff_len);

  to_fill = socket_send_buffer;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    bcopy(g_req_rep_io_vector.vector[i].iov_base, to_fill, g_req_rep_io_vector.vector[i].iov_len);
    to_fill += g_req_rep_io_vector.vector[i].iov_len;
  }
}

int socket_write(connection *cptr, u_ns_ts_t now, int norm_id)
{
  int bytes_sent;
  VUser *vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "Method called, Sending SocketAPI data, fd = %d, g_req_rep_io_vector.cur_idx = %d",
                          cptr->conn_fd, g_req_rep_io_vector.cur_idx);

  if(request_table_shr_mem[g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id]].proto.socket.open.protocol == TCP_PROTO)
  {
    if ((bytes_sent = writev(cptr->conn_fd, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx)) < 0)
    {
      NSDL2_SOCKETS(vptr, cptr, "Sending Socket Request Failed. Error = %s, fd = %d", nslb_strerror(errno), cptr->conn_fd);
      fill_tcp_client_failure_avg(vptr, NS_REQUEST_WRITE_FAIL);
      Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
      //retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      return -1;
    }
  }
  else if(request_table_shr_mem[g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id]].proto.socket.open.protocol == UDP_PROTO)
  {
    ns_vector_to_buf();
    NSDL2_SOCKETS(NULL, cptr, "socket type is UDP. server %s",nslb_sock_ntop((struct sockaddr *)&(cptr->cur_server)));
    if((bytes_sent = sendto(cptr->conn_fd, socket_send_buffer, send_buff_len, MSG_CONFIRM, &(cptr->cur_server), sizeof(struct sockaddr_in6))) < 0)
    {
      NSDL2_SOCKETS(vptr, cptr, "Sending Socket Request Failed. Error = %s, fd = %d", nslb_strerror(errno), cptr->conn_fd);
      //retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      return -1;
    }
  }

  if (set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    //retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return -1;
  }

  NSDL2_SOCKETS(vptr, cptr, "bytes_sent = %d, send_buffer_len= %d", bytes_sent, g_socket_vars.write_buf_len);
  if (bytes_sent < g_socket_vars.write_buf_len)
  {
    handle_incomplete_write(cptr, &g_req_rep_io_vector, g_req_rep_io_vector.cur_idx, g_socket_vars.write_buf_len, bytes_sent);
    return -2;
  }

#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    if(i == (NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector) - 1)) {
      NSDL4_SOCKETS(vptr, cptr, "Log req idx = %d", i);
      debug_log_http_req(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 1, 1);
    }
    else {
      NSDL4_SOCKETS(vptr, cptr, "Log req idx = %d", i);
      debug_log_http_req(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 0, 0);
    }
  }
#else
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    if(i == (NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector) - 1)) {
      LOG_HTTP_REQ(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 1, 1);
    }
    else {
      LOG_HTTP_REQ(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 0, 0);
    }
  }
#endif

  NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
  cptr->tcp_bytes_sent += bytes_sent;
  //average_time->tx_bytes += bytes_sent;

  on_request_write_done (cptr);
  return 0;
}

static void nsi_socket_send_timeout(ClientData client_data, u_ns_ts_t now)
{
  connection *cptr;
  cptr = (connection *)client_data.p;
  VUser *vptr = cptr->vptr;
  timer_type* tmr = cptr->timer_ptr;
  int remaining_time = 0, idle_timeout, max_send_timeout;
  int elaps_response_time;

  NSDL2_SOCKETS(NULL, cptr, "Method Called, cptr = %p conn state = %d now = %llu", cptr, cptr->conn_state, now);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }
 
  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  //ProtoSocket_Shr *socket_req = &request_table_shr_mem[vptr->next_pg_id].proto.socket;

  max_send_timeout = (socket_req->timeout_msec != -1) ? socket_req->timeout_msec : 
      ((g_socket_vars.socket_settings.send_to != -1) ? 
        g_socket_vars.socket_settings.send_to : SEND_TIMEOUT);

  idle_timeout = (g_socket_vars.socket_settings.send_ia_to != -1) ? 
      g_socket_vars.socket_settings.send_ia_to : 
      runprof_table_shr_mem[vptr->group_num].gset.idle_secs; 

  elaps_response_time = now - cptr->request_sent_timestamp;
  remaining_time = max_send_timeout - elaps_response_time;

  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] checking whether max timeout expired or not? "
      "API specific timeout = %d, Global timeout set by API = %d, Default "
      "(max_timeout = %d, idle timeout = %d), max_send_timeout = %d, idle_timeout = %d, remaining_time = %d, current = %lld", 
       socket_req->timeout_msec, g_socket_vars.socket_settings.send_to, SEND_TIMEOUT,  
       g_socket_vars.socket_settings.send_ia_to, max_send_timeout, idle_timeout, remaining_time, cptr->request_sent_timestamp);

  if(remaining_time > 0)
  {
    if((remaining_time > 0) && (remaining_time < idle_timeout))
      cptr->timer_ptr->actual_timeout = remaining_time;
    else
      cptr->timer_ptr->actual_timeout = idle_timeout;

    dis_idle_timer_reset(now, cptr->timer_ptr);

    NSDL2_SOCKETS(NULL, NULL, "Reset timer:[timer_type, timeout, actual_timeout, "
        "timer_status] = [%d, %llu, %d, %d]",
         tmr->timer_type, tmr->timeout, tmr->actual_timeout, tmr->timer_status);

    return;   // Since max timeout still remain so return from here
  }

  vptr->page_status = NS_REQUEST_TIMEOUT;

  NSDL2_SOCKETS(NULL, NULL, "timer: [timer_type, timeout, actual_timeout, timer_status] = [%d, %llu, %d, %d]",
                          tmr->timer_type, tmr->timeout, tmr->actual_timeout, tmr->timer_status);

  if(tmr->timer_type == AB_TIMEOUT_IDLE)
  {
    NSDL2_SOCKETS(NULL, cptr, "Deleting Idle timer for Socket partial write.");
    dis_timer_del(tmr);
  }
  else
  {
    NSDL2_SOCKETS(NULL, cptr, "Code should not come in this lag");
  }

  //Nisha: Need to check conn_state
  cptr->conn_state = CNST_IDLE;

  //ns_socket_modify_epoll(cptr, -1, -1);

  cptr->write_complete_time = now - cptr->ns_component_start_time_stamp; 

  if (IS_TCP_CLIENT_API_EXIST)
    fill_tcp_client_avg(vptr, SEND_TIME, cptr->write_complete_time);

  if (IS_UDP_CLIENT_API_EXIST)
    fill_udp_client_avg(vptr, SEND_TIME, cptr->write_complete_time);

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    switch_to_vuser_ctx(vptr, "[SocketStats] Switching NVM->VUser, Send timeout");
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_SEND_REP, 0);
  /*else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_READ_REP, 0);*/
}

int nsi_socket_send(VUser *vptr)
{
  int ret;
  unsigned long msglen = 0;
  connection *cptr;
  ClientData client_data;
  u_ns_ts_t now = get_ms_stamp();

  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  //ProtoSocket_Shr *socket_req = &request_table_shr_mem[vptr->next_pg_id].proto.socket;

  g_socket_vars.write_buf_len = 0;          //Must reset everytime

  cptr = vptr->conn_array[socket_req->norm_id];

  NSDL2_SOCKETS(vptr, cptr, "Method called, api_id = %d, vptr = %p, cptr = %p, api_type = %d, socket_norm_id = %d",
                          vptr->next_pg_id, vptr, cptr, socket_req->operation, socket_req->norm_id);

  if (IS_TCP_CLIENT_API_EXIST)
    fill_tcp_client_avg(vptr, NUM_SEND, 0);

  if (IS_UDP_CLIENT_API_EXIST)
    fill_udp_client_avg(vptr, NUM_SEND, 0);

  if(cptr == NULL)
  {
    NSTL1(NULL, NULL, "Error: cptr is NULL for conn_id = %d",
                       socket_req->norm_id);
    return -1;
  }

  NSDL2_SOCKETS(vptr, cptr, "cptr = %p, request type = %d, cptr->conn_fd = %d, cptr->req_ok = %d, cptr->conn_state = %d",
                          cptr, cptr->request_type, cptr->conn_fd, cptr->req_ok, cptr->conn_state);

  if(cptr->conn_fd < 0)
  {
    NSTL1(NULL, NULL, "Error: cannot send Socket message because conn_fd = %d, cptr->conn_state = %d, cptr = %p",
                       cptr->conn_fd, cptr->conn_state, cptr);

    NSDL2_SOCKETS(NULL, NULL, "Error: cannot send Socket message because conn_fd = %d, cptr->conn_state = %d, cptr = %p",
                        cptr->conn_fd, cptr->conn_state, cptr);
    return -1;
  }

  cptr->ns_component_start_time_stamp = now;

  /*==============================================
    MessageFormat:

    L - Length Bytes(Prefix+Message+Suffix)
    P - Prefix to be added
    M - Send Message
    S - Suffix to append after message

    +-+---+-----------+---+           
    |L| P |      M    | S |  
    +-+---+-----------+---+
  =============================================*/

  NS_RESET_IOVEC(g_req_rep_io_vector);

  NSDL4_SOCKETS(NULL, cptr, "max_msglen = %lu", socket_req->send.buffer_len);

  if(socket_req->send.buffer_len)
  {
    char *ptr = NULL;
    MY_REALLOC(ptr, socket_req->send.msg_fmt.len_bytes, "Allocate memory for Message Length Bytes", -1);
    NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, ptr, socket_req->send.msg_fmt.len_bytes);
  }

  NS_RESET_IOVEC(g_scratch_io_vector);

  if((ret = insert_segments(vptr, cptr, &socket_req->send.msg_fmt.prefix, 
            &g_scratch_io_vector, NULL, 0, 1, REQ_PART_BODY, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
    return ret;
  }

  if ((ret = insert_segments(vptr, cptr, &socket_req->send.buffer, &g_scratch_io_vector, NULL, 0, 1,
                             REQ_PART_BODY, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
    return ret;
  }

  if ((ret = insert_segments(vptr, cptr, &socket_req->send.msg_fmt.suffix, &g_scratch_io_vector, NULL, 0, 1,
                             REQ_PART_BODY, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
    return ret;
  }

  // Fill length
  //if(socket_req->send_msg_max_len)
  {
    int i, to_write_len = 0, written_amount = 0;

    // Total message length
    for (i = 0; i < g_scratch_io_vector.cur_idx; i++) //TODO: Use Macro for cur_idx and library code for cal len
    {
      msglen += g_scratch_io_vector.vector[i].iov_len;
    }

    NSDL2_SOCKETS(NULL, cptr, "Before Enc: SocketAPI:Send, fill length on first vector, num verctors = %d, "
                              "msglen = %lu, socket_req->send_msg_max_len = %lu",
                               g_scratch_io_vector.cur_idx, msglen, socket_req->send.buffer_len);

    if(socket_req->send.msg_fmt.msg_enc_dec)
    {
      char *to_enc_buffer = NULL; //TODO: Make static and malloc when needed
      char *ptr = NULL;
      int enc_buffer_len = 0;
      char *enc_buffer;  
  
      MY_REALLOC(to_enc_buffer, msglen, "Reallocate memory for encoded buffer", -1);

       ptr = to_enc_buffer;

      for(i = 0; i < g_scratch_io_vector.cur_idx; i++)
      {
        bcopy(g_scratch_io_vector.vector[i].iov_base, ptr, g_scratch_io_vector.vector[i].iov_len);
        ptr += g_scratch_io_vector.vector[i].iov_len;
      } 

      NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  
      enc_buffer = socket_req->enc_dec_cb(to_enc_buffer, msglen, &enc_buffer_len);

      NS_FILL_IOVEC_AND_MARK_FREE(g_scratch_io_vector, enc_buffer, enc_buffer_len);
     
      msglen = enc_buffer_len;
    }

    NSDL2_SOCKETS(NULL, cptr, "After Enc: SocketAPI:Send, fill length on first vector, num verctors = %d, "
                              "msglen = %lu, socket_req->send_msg_max_len = %lu",
                               g_scratch_io_vector.cur_idx, msglen, socket_req->send.buffer_len);

    // Fill Message Length at index 0
    if(socket_req->send.buffer_len)
    {
      if(msglen > socket_req->send.buffer_len)
        msglen = socket_req->send.buffer_len;

      if(socket_req->send.msg_fmt.len_type == LEN_TYPE_BINARY)
      {
        nslb_endianness((unsigned char*)g_req_rep_io_vector.vector[0].iov_base, (unsigned char*)&msglen, socket_req->send.msg_fmt.len_bytes,
                   socket_req->recv.msg_fmt.len_endian);
        //memcpy(g_req_rep_io_vector.vector[0].iov_base, &msglen, socket_req->send.msg_fmt.len_bytes);
      }
      else
      {
        char msg_len_in_text[20];
        sprintf(msg_len_in_text, "%0*ld", socket_req->send.msg_fmt.len_bytes, msglen);  //Padding 0's for sending len in slen_bytes
        memcpy(g_req_rep_io_vector.vector[0].iov_base, &msg_len_in_text, socket_req->send.msg_fmt.len_bytes);
      }

      g_socket_vars.write_buf_len += socket_req->send.msg_fmt.len_bytes;
    }

    // Copy vectors from g_scratch_io_vector.vector to g_req_rep_io_vector
    for(i = 0; i < g_scratch_io_vector.cur_idx; i++)
    {
      if((g_scratch_io_vector.vector[i].iov_len + written_amount) > msglen)
      {
        to_write_len = (msglen - written_amount);
        if(to_write_len == 0)
          break;
        else
          ((char *)(g_scratch_io_vector.vector[i].iov_base))[to_write_len] = '\0';
      }
      else
        to_write_len = g_scratch_io_vector.vector[i].iov_len;

      if(to_write_len == 0)
        break;

      g_req_rep_io_vector.vector[NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector)].iov_base = g_scratch_io_vector.vector[i].iov_base;
      g_req_rep_io_vector.vector[NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector)].iov_len = to_write_len;
      g_req_rep_io_vector.flags[NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector)] = g_scratch_io_vector.flags[i];

      NS_INC_IOVEC_CUR_IDX(g_req_rep_io_vector);
      written_amount += to_write_len;
    }

    // Free vectors of g_scratch_io_vector
    if(i < g_scratch_io_vector.cur_idx)
    {
      for(;i < g_scratch_io_vector.cur_idx; i++)
      {
        NS_FREE_IOVEC(g_scratch_io_vector, i);
      }
    }

    NS_RESET_IOVEC(g_scratch_io_vector);
  }

  g_socket_vars.write_buf_len += msglen;

  NSDL4_SOCKETS(NULL, cptr, "Socket request type = %d, Byte to send = %d, Number of vec = %d", 
                          cptr->request_type, g_socket_vars.write_buf_len, g_req_rep_io_vector.cur_idx);

  /* Handle SSL write message */
  if(cptr->request_type == SSL_SOCKET_REQUEST)
  {
    cptr->conn_state = CNST_SSL_WRITING;

    NSDL4_SOCKETS(NULL, cptr, "Sending SSL Connect request socket_req_size = %d, num_vectors = %d", 
                            g_socket_vars.write_buf_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));

    copy_request_into_buffer(cptr, g_socket_vars.write_buf_len, &g_req_rep_io_vector);

    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      //TODO: Handle retry connction 
      //retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      //websocket_close_connection(cptr, 1, now, NS_REQUEST_CONFAIL, NS_COMPLETION_WS_ERROR);
      //INC_WS_MSG_SEND_FAIL_COUNTER(vptr);
      return -1;
    }

    ret = handle_ssl_write(cptr, now);
    if(ret == -1)
      ret = -2;   //Partial write
    else if(ret == -2)
      ret = -1;
  }
  else
  {
    cptr->conn_state = CNST_WRITING;
    ret = socket_write(cptr, now, socket_req->norm_id);
  }

  NSDL4_SOCKETS(NULL, cptr, "SocketWrite: ret = %d, cptr->conn_state = %d", ret, cptr->conn_state);
  if(ret == 0)
  {
    cptr->conn_state = CNST_IDLE;
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "[SocketStats] Switching NVM->VUser, Send completed");
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_SEND_REP, 0);
  }
  else if(ret == -2)   //Add timer in case of partial write
  {
    /* Add timer for first byte */
    memset(&client_data, 0, sizeof(client_data));
    client_data.p = cptr;
 
    cptr->request_sent_timestamp = now; //Set it adjust timer
    cptr->timer_ptr->actual_timeout = (g_socket_vars.socket_settings.send_ia_to != -1)? 
                         g_socket_vars.socket_settings.send_ia_to: 
                         runprof_table_shr_mem[vptr->group_num].gset.idle_secs;
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, nsi_socket_send_timeout, client_data, 0, 0);
  }

  return ret;
}

static void nsi_socket_recv_timeout(ClientData client_data, u_ns_ts_t now)
{
  connection *cptr;
  cptr = (connection *)client_data.p;
  VUser *vptr = cptr->vptr;
  timer_type* tmr = cptr->timer_ptr;

  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  NSDL2_SOCKETS(NULL, cptr, "Method Called, cptr = %p conn state = %d now = %llu", cptr, cptr->conn_state, now);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }
 
  vptr->page_status = NS_REQUEST_TIMEOUT;

  NSDL2_SOCKETS(NULL, NULL, "timer: [timer_type, timeout, actual_timeout, timer_status] = [%d, %llu, %d, %d]",
                          tmr->timer_type, tmr->timeout, tmr->actual_timeout, tmr->timer_status);

  if(tmr->timer_type == AB_TIMEOUT_IDLE)
  {
    NSDL2_SOCKETS(NULL, cptr, "Deleting Idle timer of Frame reading.");
    dis_timer_del(tmr);
  }
  else
  {
    NSDL2_SOCKETS(NULL, cptr, "Code should not come in this lag");
  }

  // Remove event EPOLLIN
  cptr->conn_state = CNST_IDLE;
  ns_socket_modify_epoll(cptr, -1, EPOLLOUT);

  if (cptr->bytes)
  {
    copy_url_resp(cptr);
    ns_save_binary_val((char *)vptr->url_resp_buff, socket_req->recv.buffer, vptr->bytes);
  }

  cptr->request_complete_time = now - cptr->ns_component_start_time_stamp; 

  if (IS_TCP_CLIENT_API_EXIST)
  {
    fill_tcp_client_avg(vptr, RECV_TIME, cptr->request_complete_time);
    fill_tcp_client_failure_avg(vptr, NS_REQUEST_TIMEOUT);
    fill_tcp_client_avg(vptr, NUM_RECV_FAILED, 0);
  }

  if (IS_UDP_CLIENT_API_EXIST)
  {
    fill_udp_client_avg(vptr, RECV_TIME, cptr->request_complete_time);
    fill_udp_client_failure_avg(vptr, NS_REQUEST_TIMEOUT);
    fill_udp_client_avg(vptr, NUM_RECV_FAILED, 0);
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    switch_to_vuser_ctx(vptr, "[SocketStats] Switching NVM->VUser, Read timeout");
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_READ_REP, 0);
  /*else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_READ_REP, 0);*/
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name		: nsi_socket_recv()
  Purpose	: 1. Get cptr form socket ID 
                  2. Pre-read and Post-read initialization of -
                     vptr and cptr attributes.
                  2. Handle context and non context mode
                  3. Copy read data into User provided parameters
  Inputs	: vptr => pointer to VUser 
  Return	: On success = 0
                  On Failure = Non-zero value  
  Mod. Hist.	: First version, Nisha & Manish, 18 June 2020
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int nsi_socket_recv(VUser *vptr)
{
  connection *cptr = NULL;
  ClientData client_data;
  u_ns_ts_t now = 0;
  int sockid = vptr->next_pg_id;  //Get socket ID
  int max_timeout, fb_timeout;

  vptr->cur_page = vptr->sess_ptr->first_page + sockid;
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  NSDL2_SOCKETS(vptr, NULL, "Method called, vptr = %p, sockid = %d, norm_id = %d", 
      vptr, sockid, socket_req->norm_id);
  
  //Update counter for ns_socket_recv API call
  if (IS_TCP_CLIENT_API_EXIST)
    fill_tcp_client_avg(vptr, NUM_RECV, 0);

  if (IS_UDP_CLIENT_API_EXIST)
    fill_udp_client_avg(vptr, NUM_RECV, 0);

  cptr = vptr->conn_array[socket_req->norm_id];
  if(!cptr)
  { 
    NSDL2_SOCKETS(vptr, NULL, "[SOCKET_READ] cptr is not set for id = %d", sockid);

    vptr->page_status = NS_REQUEST_ERRMISC;

    if (IS_TCP_CLIENT_API_EXIST)
    {
      fill_tcp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_tcp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);
    }

    if (IS_UDP_CLIENT_API_EXIST)
    {
      fill_udp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_udp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);
    }

    return -1; 
  }

  NSDL2_SOCKETS(vptr, cptr, "[SOCKET_READ] vptr = %p, cptr = %p, cptr{fd = %d, conn_state = %d, req_ok = %d}", 
             vptr, cptr, cptr->conn_fd, cptr->conn_state, cptr->req_ok);

  if(cptr->conn_fd < 0)
  {
    NSTL1(NULL, NULL, "Error: cannot read Socket message because conn_fd = %d, cptr->conn_state = %d, cptr = %p",
                       cptr->conn_fd, cptr->conn_state, cptr);

    NSDL2_SOCKETS(vptr, NULL, "[SOCKET_READ] unable to read data on socket as connection fd has been colsed. "
                           "cptr->conn_fd = %d", cptr->conn_fd);

    vptr->page_status = NS_REQUEST_ERRMISC;

    if (IS_TCP_CLIENT_API_EXIST)
    {
      fill_tcp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_tcp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);
    }

    if (IS_UDP_CLIENT_API_EXIST)
    {
      fill_udp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_udp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);
    }

    return -1;
  }

  // Add event EPOLLIN 
  NSDL2_SOCKETS(vptr, NULL, "[SOCKET_READ] add EPOLLIN event, g_socket_diable = 0x%x", g_socket_vars.socket_disable);
  if(!(g_socket_vars.socket_disable & SOCKET_DISABLE_RECV))
    ns_socket_modify_epoll(cptr, -1, EPOLLIN);

  // Reset connection attributes
  cptr->conn_state = CNST_READING;
  cptr->bytes = 0;

  /* Add timer for first byte */
  memset(&client_data, 0, sizeof(client_data));
  client_data.p = cptr;
  now = get_ms_stamp();

  cptr->ns_component_start_time_stamp = now;

  cptr->request_sent_timestamp = now; //Set it adjust timer

  /*=========================================================================== 
    Timeout Calculation:
      1. Set First Byte timeout if any in API attribute, else
      2. Set Global First Byte timeout, if set by C API, else
      3. Set IDLE timeout given in G_IDLE keyword 
   ===========================================================================*/
  if(socket_req->timeout_msec != -1)
    max_timeout = socket_req->timeout_msec;
  else if(g_socket_vars.socket_settings.recv_to != -1)
    max_timeout = g_socket_vars.socket_settings.recv_to;
  else
    max_timeout = runprof_table_shr_mem[vptr->group_num].gset.response_timeout;

  if(socket_req->recv.fb_timeout_msec != -1)
    fb_timeout = socket_req->recv.fb_timeout_msec;
  else if(g_socket_vars.socket_settings.recv_fb_to != -1)
    fb_timeout = g_socket_vars.socket_settings.recv_fb_to;
  else
    fb_timeout = runprof_table_shr_mem[vptr->group_num].gset.idle_secs;
    

  cptr->timer_ptr->actual_timeout = (fb_timeout < max_timeout) ? fb_timeout : max_timeout; 

  dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, nsi_socket_recv_timeout, client_data, 0, 0);

  // Reset VUser attributes  
  vptr->page_status = NS_REQUEST_OK;

  // Read socket data now 
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) //For context mode
  {
    NSDL2_SOCKETS(vptr, cptr, "[SOCKET_READ] Context Mode, vptr = %p, cptr = %p, sockid = %d", vptr, cptr, sockid); 
    vut_add_task(vptr, VUT_SOCKET_READ);
    switch_to_nvm_ctx(vptr, "[SocketStats] Switching VUser->NVM, Start reading");
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD) // For Non-context mode
  {
    NSDL2_SOCKETS(vptr, cptr, "[SOCKET_READ] Thread Mode, vptr = %p, cptr = %p, sockid = %d", vptr, cptr, sockid); 
    Ns_api_req api_req_opcode;
    api_req_opcode.opcode = NS_API_SOCKET_READ_REQ;
    vptr->thdd_ptr->page_id = sockid;
    vptr->page_status = vutd_send_msg_to_nvm(NS_API_SOCKET_READ_REQ, (char *)(&api_req_opcode), sizeof(Ns_api_req));
  }

  NSDL2_SOCKETS(vptr, cptr, 
             "[SOCKET_READ] read done, vptr = %p, cptr = %p, "
             "vptr{page_status = %d, bytes = %d, url_resp_buff = %p, page_status = %d}", 
             vptr, cptr, vptr->page_status, vptr->bytes, vptr->url_resp_buff, vptr->page_status);

  // After data read check and set its status, copy data into provided parameter
  if((vptr->page_status == NS_REQUEST_OK) || (vptr->page_status == NS_REQUEST_CV_FAILURE))
  {
    ns_save_binary_val((char *)vptr->url_resp_buff, socket_req->recv.buffer, vptr->bytes);
  }
  else if (vptr->page_status != NS_REQUEST_TIMEOUT)
  {
    vptr->page_status = NS_REQUEST_NO_READ;
    
    if (IS_TCP_CLIENT_API_EXIST)
    {
      fill_tcp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_tcp_client_failure_avg(vptr, NS_REQUEST_NO_READ);
    }

    if(IS_UDP_CLIENT_API_EXIST)
    {
      fill_udp_client_avg(vptr, NUM_RECV_FAILED, 0);
      fill_udp_client_failure_avg(vptr, NS_REQUEST_NO_READ);
    }
  }
  
  return vptr->page_status;
}

void socket_close_connection(connection* cptr, int done, u_ns_ts_t now, int req_ok, int completion)
{
  int url_ok = 0;
  int status;
  int redirect_flag = 0;
  int request_type;
  char taken_from_cache = 0; // No
  VUser *vptr = cptr->vptr;

  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  NSDL2_SOCKETS(vptr, cptr, "Method called. cptr = %p, done = %d, req_ok = %d, completion = %d", cptr, done, req_ok, completion);

  request_type = cptr->request_type;

  if(!cptr->request_complete_time)
    cptr->request_complete_time = now;

  status = cptr->req_ok = req_ok;
  url_ok = !status;
  //forcefully closing connection if status is not OK 
  if (status != NS_REQUEST_OK)
    done = 1;

  NSDL2_SOCKETS(vptr, cptr, "request_type %d, urls_awaited = %d, done = %d", request_type, vptr->urls_awaited, done);

  if(done)
  {
    cptr->num_retries = 0;
    if(vptr->urls_awaited)
      vptr->urls_awaited--;
    NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed %s session with server %s",
         get_req_type_by_name(request_type),
         nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

    FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);

    vptr->conn_array[socket_req->norm_id] = NULL;
  }

  /* Do not close fd in WebSocket Handshake */
  //if(completion == NS_COMPLETION_CLOSE)
  if((cptr->conn_fd > 0) && ((cptr->conn_state != CNST_READING) || (status != NS_REQUEST_OK)))
    close_fd(cptr, done, now);

  if (status != NS_REQUEST_OK) { //if page not success
     NSDL2_SOCKETS(vptr, cptr, "aborting on main status=%d", status);
     abort_bad_page(cptr, status, redirect_flag);
     vptr->page_status = status;
     if(status == NS_REQUEST_BAD_HDR)
     {
       vptr->page_status = NS_REQUEST_BAD_HDR;
       fill_tcp_client_failure_avg(vptr, NS_REQUEST_BAD_HDR);
       NSDL2_SOCKETS(NULL, NULL, "vptr->page_status = %d", vptr->page_status);
     }
  }

  /* TODO: Need to proper handle this setting 1 just to do_data_processing */
  if(completion != NS_COMPLETION_CLOSE)
  {
    vptr->redirect_count = 1;
    handle_url_complete(cptr, request_type, now, url_ok,
                        redirect_flag, status, taken_from_cache);
  }

  /* Only Last will be handled here */
  if (!vptr->urls_awaited && (completion != NS_COMPLETION_CLOSE)) {
    handle_page_complete(cptr, vptr, done, now, request_type);
  } else {
    NSDL2_SOCKETS(NULL, cptr, "Handle handle_page_complete() not called as completion code is %d", completion);  // Error LOG
  }

  if ((cptr->conn_state == CNST_FREE)) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSTL1(vptr, cptr, "Connection slot is already in free connection list");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL2_SOCKETS(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
   }
   /* Change the context accordingly and send the response */
   if (!vptr->urls_awaited && (completion == NS_COMPLETION_CLOSE)) {
     if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
       switch_to_vuser_ctx(vptr, "[SocketStats] Switching NVM->VUser, Socket close connection done");
     else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
       send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_CLOSE_REP, 0);
   } 
   /*else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
     send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP, 0);*/
  }
}

int nsi_socket_close(VUser *vptr)
{
  connection *cptr;

  u_ns_ts_t now = get_ms_stamp();

  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  cptr = vptr->conn_array[socket_req->norm_id];

  if(cptr == NULL)
  { 
    NSTL2(NULL, NULL, "Error: cptr is NULL");
    NSDL2_SOCKETS(NULL, NULL, "Error: cptr is NULL");
    return -1;
  }

  if(cptr->conn_fd < 0)
  {
    NSDL2_SOCKETS(vptr, cptr, "Socket: cptr->conn_fd is -1");
    NSTL2(vptr, cptr, "Socket: cptr->conn_fd is -1");
    return -1;
  }

  socket_close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  NSDL2_SOCKETS(vptr, cptr, "nsi_socket_close(): is successfully closed with page_status = %d", vptr->page_status);
  return 0;
}

int ns_socket_get_num_msg_internal(char *str, int str_len, char *substr, int substr_len)
{
  char *str_ptr = str;
  int len = str_len;
  int num_recv_msg = 0;

  if(!str || !*str)
  {
    NSDL2_SOCKETS(NULL, NULL, "Input string is NULL OR EMPTY");
    return num_recv_msg;
  }

  if(!substr || !*substr)
  {
    NSDL2_SOCKETS(NULL, NULL, "Sub-string is NULL OR EMPTY");
    return num_recv_msg;  
  }

  while (len > substr_len)   //Loop untill complete buffer not parsed
  {
    NSDL2_SOCKETS(NULL, NULL, "str_ptr = %p, len = %d, substr = %s, substr_len = %d, num_recv_msg = %d", 
        str_ptr, len, substr, substr_len, num_recv_msg);
 
    if((str_ptr = memmem(str_ptr, len, substr, substr_len)) != NULL)
    {
      num_recv_msg++;
      len -= (str_ptr - str) + substr_len; 
      str_ptr += len;  //Skip parsed data + suffix
    }
  }

  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] number of messages = %d", num_recv_msg);

  return num_recv_msg;
}

int ns_socket_get_num_msg_ex(VUser *vptr)
{
  int num_msg;
  int len;

  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;
  char *sub_str = ns_segtobuf(vptr, NULL, &socket_req->recv.msg_fmt.suffix, NULL, &len);

  num_msg = ns_socket_get_num_msg_internal(vptr->url_resp_buff, vptr->bytes, sub_str, len);

  ns_segtobuf_cleanup();

  return num_msg;
}

#if 0
char *ns_socket_get_msg_internal(VUser *vptr)
{
  __thread static char *start = NULL:
  __thread static char *cur;
  __thread static int remaning_len;
  __thread static char *end;
  __thread static char end_char; 
  __thread static char delim[64 + 1];
  __thread static int delim_len = 0;
  
  int len;

  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  if(!start)
  {
    start = vptr->url_resp_buff;

    cur = ns_segtobuf(vptr, NULL, &socket_req->recv.msg_fmt.suffix, NULL, &delim_len);

    delim_len = (delim_len<64)?delim_len:64;
    strncpy(delim, cur, delim_len);
    delim[delim_len] = 0;  //NULL terminition
    
    ns_segtobuf_cleanup();
    
    cur = start;
    end = start;
    remaning_len = vptr->bytes; 
  }

  if(end_char != 0)
    *end = end_char;   //Issue - It will truncate original buffer need to use other buffer

  end = memmem(cur, remaning_len, delim, delim_len);

  if(end != NULL)
  {
    end_char = *end;
    *end = 0;
  }

  return cur;
}
#endif

/*
  Read end policy and their priority -
    1. If no policy is given, read data till timeout
    2. If Message format is given then read data till provided length in message  
    3. If read bytes is given, read only data till read bytes
    4. If suffix is gievn, read till suffix not found
*/
int process_socket_recv_data(void *cptr_in, char *buf, int bytes_read, int *read_offset, int msg_peek)
{
  int done = 0, err = 0, len = 0, len1 = 0;
  char payload_len[20] = {0};  //must be 0
  char *payload_len_ptr = payload_len;
  char *payload = buf;
  char *payload_ptr;
  char *tmp_ptr, *tmp_ptr1;
  int remaning_length_bytes = 0;
  int err_code;

  u_ns_ts_t now = get_ms_stamp();

  connection *cptr = (connection*)cptr_in;
  VUser *vptr = cptr->vptr;

  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  ProtoSocket_Shr *socket_req = (ProtoSocket_Shr *)(&vptr->cur_page->first_eurl->proto.socket);
  ProtoSocket_MsgFmt *msg_fmt = (ProtoSocket_MsgFmt *)(&socket_req->recv.msg_fmt);
  

  NSDL2_SOCKETS(vptr, cptr, "Method called, buf = %s, bytes_read = %d, "
      "len-bytes = %d, len_type = %d, rend_policy = %d, "
      "cptr->bytes = %d, cptr->content_length = %d, read_bytes = %d", 
       buf, bytes_read, socket_req->recv.msg_fmt.len_bytes, socket_req->recv.msg_fmt.len_type, 
       socket_req->recv.end_policy, cptr->bytes, cptr->content_length, socket_req->recv.buffer_len);

  if(msg_peek)
  {
    if(IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(vptr, RECV_THROUGHPUT, bytes_read);

    if (IS_UDP_CLIENT_API_EXIST)
      fill_udp_client_avg(vptr, RECV_THROUGHPUT, bytes_read);
  }

  switch(socket_req->recv.end_policy)
  {
    case NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES:
    {
      if((cptr->bytes + bytes_read) < msg_fmt->len_bytes) // It means EGAIN comes
      {
        //Partial Length Prefixed: Copy data into cptr->cur_buf till it is partial 
        handle_partial_recv(cptr, buf, bytes_read, cptr->bytes);  // cptr->bytes must be 0
        cptr->bytes += bytes_read;

        NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES: "
            "partial Length_Bytes read, cptr->bytes = %d", cptr->bytes);

        return 0; 
      }
      else if(cptr->content_length == -1)// Get content length first 
      {
        remaning_length_bytes = msg_fmt->len_bytes - cptr->bytes;

        NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES: "
            "remaning_length_bytes = %d, cptr->bytes = %d", 
             remaning_length_bytes, cptr->bytes);

        if(cptr->bytes) // If Length_Bytes is partially read
        {
          //Partial Length Prefixed: Concat remaining prefixed length into 
          //  cptr->cur_buf and get Payload length 
          memcpy(cptr->cur_buf, buf, remaning_length_bytes);
          payload = cptr->cur_buf->buffer;
        }

        // Read message length
        if(msg_fmt->len_type == LEN_TYPE_TEXT)
        {
          memcpy(payload_len_ptr, payload, msg_fmt->len_bytes);
          //Skip any leading blanks from starting of len_bytes
          if(nslb_atoi(payload_len_ptr, &(cptr->content_length)) < 0)
          {
            NSDL2_SOCKETS(vptr, cptr, "Length-Byte '%s' is not numeric.", payload_len_ptr);
            err = 1;
            err_code = NS_REQUEST_BADBYTES; 
            goto error;
          }
        }
        else
        {
          nslb_endianness((unsigned char*)payload_len_ptr, (unsigned char*)payload, 
              msg_fmt->len_bytes, msg_fmt->len_endian);
          memcpy(&(cptr->content_length), payload_len, msg_fmt->len_bytes);
        }
        NSDL2_SOCKETS(vptr, cptr, "cptr->content_length = %d", cptr->content_length);

        if(cptr->content_length <= 0)
        {
          NSDL2_SOCKETS(vptr, cptr, "Content-Length cannot be zero.");
          err = 1;
          err_code = NS_REQUEST_BADBYTES; 
          goto error;
        }

        // Now copy payload into cptr->cur_buf if exist
        payload = buf + remaning_length_bytes; //Skip payload len
        bytes_read -= remaning_length_bytes;  //Substract length size  
        cptr->bytes = 0;

        NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES: "
            "get payload length, content_length = %d, payload = %p, cptr->bytes = %d", 
             cptr->content_length, payload, cptr->bytes);
      }
      else
        NSDL2_SOCKETS(vptr, cptr, "Code shoud not come into this lag");

      //Check IS reading complete?
      if(cptr->content_length <= (cptr->bytes + bytes_read))
      {
        done = 1; 
      }

      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES:
    {
      if(cptr->content_length == -1)
      {
        cptr->content_length = socket_req->recv.buffer_len;
      }

      if(cptr->content_length <= (cptr->bytes + bytes_read))
      { 
        done = 1; 
      }
     
      NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES get payload length, cptr->content_length = %d", cptr->content_length);
      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX:
    {
      /*=======================================================================
        [HINT: ReadEndPolicyDelimiter]

           1. If read is done by MSG_PEEK then,
              1.1. Check for delimiter existence,
              1.2. If delim found then 
                   calculate size which need to without MSG_PEEK
              1.3. If delim not found then, 
                   Check how many characters of delimiter found
                   1.3.1. If no characters found, JUST return to read without 
                          MSG_PEEK
                   1.3.2. If some characters found, then save OFFSET in delim
                          from where next time need to check for delimiter 
                            
            ==> OFFSET , use this variable in following way
                1. To save how many delimiter bytes found in read data
                   OFFSET = (+)ve
                2. To save Delimter found flag
                   OFFSET = -1
                3. Not check OR in progress 
                   OFFSET = 0 
            2. If read done without MSG_PEEK then,
               2.1. If offset

           Cases: 1. Payload > Delim, Payload contains complete Delim
                  2. Payload > Delim, Payload contains partial Delim
                  3. Payload > Delim, Payload doesn't contain Delim
                  4. Payload < Delim, Payload contains partial Delim
                  5. Payload < Delim, Payload doesn't contain Delim
      =======================================================================*/

      payload_ptr = buf;
      len1 = bytes_read;

      //Suffix                                                                //Suffix Len
      tmp_ptr = ns_segtobuf(NULL, cptr, (const StrEnt_Shr *)(&msg_fmt->suffix), NULL, &len);

      //IF read with flag MSG_PEEK
      if(msg_peek)
      {
        if(cptr->body_offset > 0)
        {
          len = len - cptr->body_offset;
          tmp_ptr += cptr->body_offset;
        }

        if((tmp_ptr1 = memmem(payload_ptr, len1, tmp_ptr, len)) != NULL) //Found
        {
          *read_offset = (tmp_ptr1 - payload_ptr) + len;
          cptr->body_offset = -1;  //Found
        }
        else  // Not Found
        {
          *read_offset = bytes_read;
          //Check how many bytes of delim found 
          if(len1 > len)
          {
            payload_ptr += len1 - len; //check from last only
            len1 = len;
          }
          else
          {
            len = len1;
          }
         
          tmp_ptr1 = payload_ptr;  //Delim start
          NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX Peek else part,"
                        " len = %d, len1 = %d, tmp_ptr1 = %s, payload_ptr = %s", len, len1, tmp_ptr1, payload_ptr);

          for(int i = 0; i < len; i++)
          {
            if((tmp_ptr1 = memchr(tmp_ptr1, tmp_ptr[i], len1)) != NULL) //One byte matched   //tmp_ptr1 
            {
              if(cptr->body_offset == 0)
              {
                payload_ptr += len1 - 1;
                len = payload_ptr - tmp_ptr1 + 1; //Need to run for loop only for remaining bytes from where Delimiter first byte is found
              }
              cptr->body_offset++;
              tmp_ptr1++;
              len1 = 1;  //Now search for 1 char only
            }
            else
            {
              cptr->body_offset = 0;
              break;
            }
          }
        }
      }
      else
      {
        if (cptr->body_offset < 0)
        {
          cptr->body_offset = 0;         //MUST take care of this at the time of error case also
          done = 1;
        }
      }
      
      ns_segtobuf_cleanup();

      NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX, done = %d, "
          "payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d,"
          "msg_peek = %d, cptr->body_offset = %d", 
           done, payload_ptr, len1, tmp_ptr, len, msg_peek, cptr->body_offset);

      if(msg_peek)
      {
        return 0; 
      }

      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_CONTAINS:
    {
      payload_ptr = buf;
      len1 = bytes_read;

      tmp_ptr = ns_segtobuf(NULL, cptr, &socket_req->recv.msg_contains, &tmp_ptr1, &len);

      NSDL2_SOCKETS(vptr, cptr, "payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d", payload_ptr, len1, tmp_ptr, len);

      if((payload_ptr = memmem(payload_ptr, len1, tmp_ptr, len)) != NULL)
        done = 1;

      ns_segtobuf_cleanup();

      NSDL2_SOCKETS(vptr, cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_CONTAINS, "
          "done = %d, payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d", 
           done, payload_ptr, len1, tmp_ptr, len);

      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_TIMEOUT: 
    {
      // Timeout case will be handle form timeout calback
      // Here just read and save data into cptr->buf_head
      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_CLOSE:
    {
      if(bytes_read == 0)
        done = 1;

      break;
    }
    default:  // No policy 
    {
      NSDL2_SOCKETS(vptr, cptr, "NO policy is given");
      done = 1;
      break;
    }
  }

  if(cptr->content_length > 0)
    len = (cptr->content_length >= (cptr->bytes + bytes_read)) ? bytes_read:
            (bytes_read - ((cptr->bytes + bytes_read) - cptr->content_length));
  else
    len = bytes_read;

  NSDL2_SOCKETS(vptr, cptr, "Copy read data into cptr->buf_head, cptr = %p, payload = %p, "
                       "bytes_read = %d, cptr->bytes = %d, cptr->content_length = %d, len = %d", 
                        cptr, payload, bytes_read, cptr->bytes, cptr->content_length, len);

  //Copy data into cptr->cur_buf, In case of partial as well as in complete
  copy_retrieve_data(cptr, payload, len, cptr->bytes);
  cptr->bytes += len;

error:
  if(done || err)
  {
    // Remove event EPOLLIN 
    cptr->conn_state = CNST_IDLE;
    ns_socket_modify_epoll(cptr, -1, EPOLLOUT);

    if(done) // Data reading done
    {
      cptr->request_complete_time = now - cptr->ns_component_start_time_stamp; 

      if (IS_TCP_CLIENT_API_EXIST)
      {
        fill_tcp_client_avg(vptr, RECV_TIME, cptr->request_complete_time);
        fill_tcp_client_avg(vptr, NUM_RECV_MSG, 0);
      }

      if (IS_UDP_CLIENT_API_EXIST)
      {
        fill_udp_client_avg(vptr, RECV_TIME, cptr->request_complete_time);
        fill_udp_client_avg(vptr, NUM_RECV_MSG, 0);
      }

      if(!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation)
      {
        copy_url_resp(cptr);

        copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc as after this do_data_processing will be done
        do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
      }

      // Delete timer
      if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
        dis_timer_del(cptr->timer_ptr);

      //Making cptr->content_length = -1. Will create issue while connection is persistent
      cptr->content_length = -1;

      if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
        switch_to_vuser_ctx(vptr, "[SocketStats] Switching NVM->VUser, Read done");
      else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
        send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_READ_REP, 0);
    }
 
    if(err)
    {
      if (IS_TCP_CLIENT_API_EXIST)
      {
        fill_tcp_client_avg(vptr, NUM_RECV_FAILED, 0);
        fill_tcp_client_failure_avg(vptr, err_code);
      }

      if(IS_UDP_CLIENT_API_EXIST)
      {
        fill_udp_client_avg(vptr, NUM_RECV_FAILED, 0);
        fill_udp_client_failure_avg(vptr, err_code);
      }

      vptr->page_status = NS_REQUEST_BADBYTES;
      Close_connection(cptr, 1, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES); //Connection close for BADBYTES
    }

    free_cptr_buf(cptr);
  }

  NSDL2_SOCKETS(vptr, cptr, "done = %d, vptr->page_status = %d", done, vptr->page_status);

  return done;
}

/*******************************************************************
    Calculating how many bytes to be read for different read policy
       1. Read fix length
       2. Fix Length
       3. Delemiter
*******************************************************************/     
inline void ns_socket_bytes_to_read(connection *cptr, VUser *vptr, int *bytes_read, int *msg_peek)
{
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  NSDL2_SOCKETS(NULL, NULL, "Method Called: Bytes read = %d, cptr->bytes = %d, "
      "socket_req->recv.buffer_len = %d, socket_req->recv.end_policy = %d",
       *bytes_read, cptr->bytes, socket_req->recv.buffer_len, socket_req->recv.end_policy);

  switch(socket_req->recv.end_policy)
  {
    case NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES:
    {
      if(cptr->content_length == -1) //calculating length bytes
        *bytes_read = socket_req->recv.msg_fmt.len_bytes - cptr->bytes;
      else
      {
        //*bytes_read = cptr->content_length - cptr->bytes;
        *bytes_read = (((cptr->content_length - cptr->bytes) > *bytes_read)? *bytes_read: (cptr->content_length - cptr->bytes));
      }
      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES:
    {
      *bytes_read = ((socket_req->recv.buffer_len - cptr->bytes) > *bytes_read) ? 
                      *bytes_read : (socket_req->recv.buffer_len - cptr->bytes); 
      break;
    }
    case NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX:
    {
      *msg_peek = 1;
      break;
    }
  }
  NSDL2_SOCKETS(NULL, cptr, "Method Exit: Bytes read = %d", *bytes_read);
}

inline int before_read_start(connection *cptr, u_ns_ts_t now, action_request_Shr **url_num, int *bytes_read)
{
  if(reset_idle_connection_timer(cptr, now) < 0)
  {
    Close_connection(cptr, 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
    return 1;
  }

  if(!(cptr->data) || (cptr->request_type == DNS_REQUEST))
  {
    NSDL2_SOCKETS(NULL, cptr, "Returning url_num from cptr");
    *url_num = cptr->url_num;
  }
  else
  {
    NSDL2_SOCKETS(NULL, cptr, "Returning url_num from cptr->data");
    *url_num = ((pipeline_page_list *)(cptr->data))->url_num;
  }

  if (IS_REQUEST_HTTP_OR_HTTPS && (*url_num)->proto.http.exact)
  {
    handle_fast_read(cptr, *url_num, now);
    return 1;
  }

  // IF SOCKET and END POLICY SUFFIX or CONTENT
  // Resolve segment and make single buffer - store in scratch buffere
  return 0;
}

int ns_parse_socket_exclude(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  int id_flag = -1;
  int norm_id;
  int ret;

  NSDL2_SOCKETS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  start_quotes = strchr(script_line, '"'); //This will point to the starting quote '"'.

  while(1)
  {
    NSDL3_SOCKETS(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);

    if((ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_SOCKETS(NULL, NULL, "ID [%s]", attribute_value);

      if(id_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Can not have more than one socket ID parameter");
    
      norm_id = nslb_get_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value));

      if(norm_id == -1)
      {
        SCRIPT_PARSE_ERROR(script_line, "Invalid socket id [%s], provided", attribute_value);
      }
      else if(norm_id == -2)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no socket opened for provided ID value", attribute_value);
      }
      else
        requests[g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id]].proto.socket.flag = EXCLUDE_SOCKET;

      id_flag = 1;
    }
    else
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] in ns_socket_exclude API", attribute_name);

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_SOCKETS(NULL, NULL, "Next attribute is not found");
      break;
    }
  }

  if(id_flag != 1)
    SCRIPT_PARSE_ERROR(script_line, "ID parameter does not exist in ns_socket_exclude");

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_write_excluded_api_in_tmp_file(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                         FILE *outfp, char *flow_outfile, char *api_attributes)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  start_idx = line_ptr;
  NSDL2_SOCKETS(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_to_run);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012070_ID, CAV_ERR_1012070_MSG);

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage

  NSDL2_SOCKETS(NULL, NULL,"Before sprintf str is = %s ", str);
  NSDL2_SOCKETS(NULL, NULL, "Add api %s in tmp file", api_to_run);

  len = strlen(api_attributes);

  api_attributes[len - 1] = '\0';

  sprintf(str, "//%s%s(%s", str, api_to_run, api_attributes);
  NSDL2_SOCKETS(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(api_to_run, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_write_action_idx_in_tmp_file(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                         FILE *outfp,  char *flow_outfile, int action_idx, char *api_attributes)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  NSDL1_SOCKETS(NULL, NULL ,"Method Called. action_idx = %d", action_idx);
  start_idx = line_ptr;
  NSDL2_SOCKETS(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_to_run);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012070_ID, CAV_ERR_1012070_MSG);

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage

  NSDL2_SOCKETS(NULL, NULL,"Before sprintf str is = %s ", str);
  NSDL2_SOCKETS(NULL, NULL, "Add api %s in tmp file and action_idx = %d", api_to_run, action_idx);

  len = strlen(api_attributes);

  api_attributes[len - 1] = '\0';

  sprintf(str, "%s%s(%d%s ", str, api_to_run, action_idx, api_attributes);
  NSDL2_SOCKETS(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(api_to_run, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_socket_c_apis(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx, char *api_name)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  int id_flag = -1, norm_id, api_id;
  int ret;

  NSDL2_SOCKETS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  start_quotes = strchr(script_line, '"'); //This will point to the starting quote '"'.

  NSDL3_SOCKETS(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);

  if((ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL3_SOCKETS(NULL, NULL, "start_quotes = %s, close_quotes = %s", start_quotes, close_quotes);

  if(!strcasecmp(attribute_name, "ID"))
  {
    NSDL2_SOCKETS(NULL, NULL, "ID [%s]", attribute_value);

    if(id_flag != -1)
      SCRIPT_PARSE_ERROR(script_line, "Can not have more than one socket ID parameter");

    norm_id = nslb_get_norm_id(&(g_socket_vars.proto_norm_id_tbl), attribute_value, strlen(attribute_value));

    if(norm_id == -1)
    {
      SCRIPT_PARSE_ERROR(script_line, "Invalid socket id [%s], provided", attribute_value);
    }
    else if(norm_id == -2)
    {
      SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no socket opened for provided ID value", attribute_value);
    }
    else
    {
      api_id = g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id];

      CHECK_AND_RETURN_IF_EXCLUDE_FLAG_SET;

      //Writing action_idx inplace of ID in .tmp file
      ns_write_action_idx_in_tmp_file(api_name, flow_fp, flow_filename,
                  script_line, outfp, flowout_filename, api_id, close_quotes + 1);
    }

    id_flag = 1;
    NSDL2_SOCKETS(NULL, NULL, "ID [%s]", attribute_value);
  }

  CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED;

  NSDL2_SOCKETS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

//Calling only incase of Host Parameterisation
void parse_socket_host_and_port(char *hostname_port, int *request_type, char *hostname, int *port)
{
  char *tmp;

  NSDL1_SOCKETS(NULL, NULL, "hostname_port = %s", hostname_port);

  //Incase of Host Parameterisation ':' will always be escaped by '3A'
  if((tmp = strstr(hostname_port, "%3A")) != NULL)
  {
    *port = atoi(tmp + 3);
  }
  else if((tmp = strchr(hostname_port, ':')) != NULL)
  {
    NSDL1_HTTP(NULL, NULL, "tmp = %s, hostname_port = %s", tmp, hostname_port);
    *port = atoi(tmp + 1);
  }
  
  *tmp = 0;
  strcpy(hostname, hostname_port);

  *request_type = SOCKET_REQUEST;

  NSDL1_SOCKETS(NULL, NULL, "hostname = %s, port = %d", hostname, *port);
}

//Setting Level for Socket
int nsi_get_sock_level_from_level_string(char *level)
{
  if(!strcmp(level, "SOL_SOCKET"))
    return SOL_SOCKET;
  else if(!strcmp(level, "IPPROTO_TCP"))
    return IPPROTO_TCP;
  else if(!strcmp(level, "IPPROTO_UDP"))
  {
    NSTL1(NULL, NULL, "UDP protocol is not supported yet. Setting Level as IPPROTO_TCP");
    return IPPROTO_TCP;
  }
  else
  {
    fprintf(stderr, "Incorrect Socket level is provided. It can be SOL_SOCKET, IPPROTO_TCP or IPPROTO_UDP");
    return -1;
  }
}

//Setting optname for Socket
int nsi_get_sock_option_from_option_string(char *optname)
{
  if(!strcmp(optname, "SO_DEBUG"))
    return SO_DEBUG;
  else if(!strcmp(optname, "SO_BROADCAST"))
    return SO_BROADCAST;
  else if(!strcmp(optname, "SO_REUSEADDR"))
    return SO_REUSEADDR;
  else if(!strcmp(optname, "SO_KEEPALIVE"))
    return SO_KEEPALIVE;
  else if(!strcmp(optname, "SO_LINGER"))
    return SO_LINGER;
  else if(!strcmp(optname, "SO_OOBINLINE"))
    return SO_OOBINLINE;
  else if(!strcmp(optname, "SO_SNDBUF"))
    return SO_SNDBUF;
  else if(!strcmp(optname, "SO_RCVBUF"))
    return SO_RCVBUF;
  else if(!strcmp(optname, "SO_DONTROUTE"))
    return SO_DONTROUTE;
  else if(!strcmp(optname, "SO_RCVLOWAT"))
    return SO_RCVLOWAT;
  else if(!strcmp(optname, "SO_RCVTIMEO"))
    return SO_RCVTIMEO;
  else if(!strcmp(optname, "SO_SNDLOWAT"))
    return SO_SNDLOWAT;
  else if(!strcmp(optname, "SO_SNDTIMEO"))
    return SO_SNDTIMEO;
  else
    fprintf(stderr, "Incorrect Option name is provided"); 
    return -1;
}

int nsi_get_or_set_sock_opt(VUser *vptr, int action_idx, char *level, char *optname, void *optval, socklen_t optlen, int set_or_get)
{
  connection *cptr = NULL;
  int loc_level, loc_optname;
  socklen_t loc_optlen;

  NSDL2_SOCKETS(vptr, NULL, "Method called, action_idx = [%d], level = [%s], optname = [%s], optlen = %d", action_idx, level, optname, optlen);

  if((loc_level = nsi_get_sock_level_from_level_string(level)) == -1)
    return -1;

  if((loc_optname = nsi_get_sock_option_from_option_string(optname)) == -1)
    return -1;
  
  cptr = vptr->conn_array[request_table_shr_mem[action_idx].proto.socket.norm_id];

  switch(set_or_get)
  {
    case 0:
      if(setsockopt(cptr->conn_fd, loc_level, loc_optname, (const char *) &optval, optlen) < 0)
      {
        //Error
        return -1;
      }
      break;
    case 1:
      if(getsockopt(cptr->conn_fd, loc_level, loc_optname, &optval, &loc_optlen) < 0)
      {
        //Error
        return -1;
      }
      else
        return loc_optlen;
      break;
    default:
      return -1; //Should not come here
  }

  return 0;
}

int nsi_get_socket_attribute_from_name(char* attribute_name)
{
  if(!strcmp(attribute_name, "LOCAL_ADDRESS"))
    return SLOCAL_ADDRESS;
  else if(!strcmp(attribute_name, "LOCAL_HOSTNAME"))
    return SLOCAL_HOSTNAME;
  else if(!strcmp(attribute_name, "LOCAL_PORT"))
    return SLOCAL_PORT;
  else if(!strcmp(attribute_name, "REMOTE_ADDRESS"))
    return SREMOTE_ADDRESS;
  else if(!strcmp(attribute_name, "REMOTE_HOSTNAME"))
    return SREMOTE_HOSTNAME;
  else if(!strcmp(attribute_name, "REMOTE_PORT"))
    return SREMOTE_PORT;
  else
  {
    fprintf(stderr, "Error: Attribute name [%s] is invalid.\n", attribute_name);
    return -1;
  }

  return 0;
}

int ns_socket_set_version_cipher(VUser *vptr, char *version, char *ciphers)
{
  int cipher_length;

  GroupSettings gset = runprof_table_shr_mem[vptr->group_num].gset;

  if(version)
  {
    if(strcasecmp(version, "SSLv3") == 0)
      gset.tls_version = SSL3_0;
    else if(strcasecmp(version, "TLSv1_0") == 0)
      gset.tls_version = TLS1_0;
    else if(strcasecmp(version, "TLSv1_1") == 0)
      gset.tls_version = TLS1_1;
    else if(strcasecmp(version, "TLSv1_2") == 0)
      gset.tls_version = TLS1_2;
    else if(strcasecmp(version, "TLSv1_3") == 0)
      gset.tls_version = TLS1_3;
    else if(strcasecmp(version, "SSLv2n3") == 0)
      gset.tls_version = SSL2_3;
    else
    {
      return -1;
    }
  }
  else
    gset.tls_version = SSL2_3;

  if(ciphers)
  {
    cipher_length = strlen(ciphers);
    if(cipher_length < CIPHER_BUF_MAX_SIZE)
      cipher_length = CIPHER_BUF_MAX_SIZE;

    strncpy(group_default_settings->ssl_ciphers, ciphers, cipher_length);
    gset.ssl_ciphers[cipher_length] = '\0';
  }
  else
  {
    strcpy(gset.ssl_ciphers, "AES128-SHA:DES-CBC3-SHA");
  }
  return 0;
}

void upgrade_ssl(connection *cptr, u_ns_ts_t now)
{
  if(cptr->conn_state != CNST_SSLCONNECTING)
  {
    if(set_ssl_con (cptr))
    {
      retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
      return;
    }
    cptr->conn_state = CNST_SSLCONNECTING;
  }

  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
  if(global_settings->ssl_key_log)
  {
    SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(cptr->ssl);
    if (set_keylog_file(ssl_ctx, global_settings->ssl_key_log_file))
      fprintf(stderr,"UNABLE TO USE KEYLOGGING\n");
  }
  #endif

  SSL_connect(cptr->ssl);
}

int ns_socket_open_ex(VUser *vptr)
{
  int ret = 0;
  connection *cptr;

  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  ProtoSocket_Shr *socket_req = &vptr->cur_page->first_eurl->proto.socket;

  cptr = vptr->conn_array[socket_req->norm_id];

  if(cptr == NULL)
    ret = ns_web_url(vptr->next_pg_id);

  return ret;
}
