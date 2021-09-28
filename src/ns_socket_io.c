/***************************************************************************************
 Name           : ns_socket_io.c
 Purpose        : This file contain all functions related to SSL/non-ssl socket read and 
                  used internally not exposed to VUsers.
                  Functions mentioned are NS Core Engine specific only, 
                  Each protocol should make functions/macro and call here

 Author(s)      : Manish/Nisha
 Mod. Hist.     : 16 Aug 2020
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
#include "ns_socket_io.h"
#include "ns_socket.h"

static inline int ssl_read(connection *cptr, VUser *vptr, action_request_Shr* url_num, char *buf, u_ns_ts_t now, int *bytes_to_read, int peek_flag)
{
  int ret = 0;
  int bytes_read = *bytes_to_read;
  int err;
  unsigned long l;
  int r = 0;
  const char *file, *data;
  int line, flags;

  ERR_clear_error();

    /* Thing to watch out for: if the amount of data received is
     * larger than the sizeof(buf) but less than the ssl lib's internal
     * buffer size, some residual data may stay in that internal
     * buffer undetected by select() calls. That is, the internal
     * buffer should always be fully drained.
     */
  if(!peek_flag)
    bytes_read = SSL_read(cptr->ssl, buf, bytes_read);
  else
    bytes_read = SSL_peek(cptr->ssl, buf, bytes_read);

  if (bytes_read <= 0)
  {
    err = SSL_get_error(cptr->ssl, bytes_read);
    switch (err) 
    {
      case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
        //handle_server_close (cptr, now);
        //return;
      case SSL_ERROR_WANT_READ:
        /* It can but isn't supposed to happen */
        ret = NS_SOCKET_IO_READ_ERROR;
        break;
      case SSL_ERROR_WANT_WRITE:
        fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
        handle_bad_read (cptr, now);
        ret = NS_SOCKET_IO_READ_ERROR;
        break;
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
     
          ret = NS_SOCKET_IO_READ_ERROR;
          break;
        }
     
        if (errno == EINTR)
        {
          NSDL3_SSL(NULL, cptr, "SSL_read interrupted. Continuing...");
          ret = NS_SOCKET_IO_READ_CONTINUE;
          break;
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
          ret = NS_SOCKET_IO_READ_ERROR;
          break;
        }
        else
        {
          if ((bytes_read == 0) && (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.ssl_clean_close_only))
          {
            //CHECK_READ_END_POLICY_AND_SWITCH
            //handle_server_close (cptr, now);
          }
          else
              handle_bad_read (cptr, now);
          ret = NS_SOCKET_IO_READ_ERROR;
          break;
        }
    }
  }

  *bytes_to_read = bytes_read;

  return ret;
}

static inline int non_ssl_read(connection *cptr, VUser *vptr, action_request_Shr* url_num,
    char *buf, int *bytes_to_read, int peek_flag, u_ns_ts_t now)
{
  int ret = 0;
  int bytes_read = *bytes_to_read;

  if(!peek_flag)
    bytes_read = read(cptr->conn_fd, buf, bytes_read);
  else
    bytes_read = recv(cptr->conn_fd, buf, bytes_read, MSG_PEEK);

  #ifdef NS_DEBUG_ON
  if(bytes_read > 0)
    buf[bytes_read] = '\0'; // NULL terminate for printing/logging
 
  NSDL3_SOCKETS(NULL, cptr, "Non SSL Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
  #endif  //NS_DEBUG_ON
 
  if(bytes_read < 0)
  {
    if(errno == EAGAIN)
    {
      if(runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
          url_num->proto.http.type != MAIN_URL &&
          (cptr->num_pipe != -1) &&
          cptr->num_pipe < runprof_table_shr_mem[vptr->group_num].gset.max_pipeline)
      {
        pipeline_connection((VUser *)cptr->vptr, cptr, now);
      }
 
      #ifndef USE_EPOLL
      NSDL3_SOCKETS(NULL, cptr, "FD_SET for cptr->conn_fd = %d, wait fot reading more data", cptr->conn_fd);
      FD_SET( cptr->conn_fd, &g_rfdset );
      #endif
 
      ret = NS_SOCKET_IO_READ_ERROR;
    }
    else
    {
      NSDL3_SOCKETS(NULL, cptr, "read failed (%s) for main: host = %s [%d], req = %s", 
          nslb_strerror(errno), url_num->index.svr_ptr->server_hostname,
          url_num->index.svr_ptr->server_port, get_url_req_url(cptr));
      handle_bad_read (cptr, now);
      ret = NS_SOCKET_IO_READ_ERROR;
    }
  }

  *bytes_to_read = bytes_read;

  return ret;
}

inline void after_read_done(connection *cptr, char *buf, int bytes_read)
{
  VUser *vptr = cptr->vptr;

  //TODO: What to for Socket ??????
  /*For page dump, if inline is enabled then only dump the inline urls in page dump
   *Otherwise dump only main url*/
  if(NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL && IS_REQUEST_HTTP_OR_HTTPS && 
     (((cptr->url_num->proto.http.type == EMBEDDED_URL) && 
       runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) || 
     (cptr->url_num->proto.http.type != EMBEDDED_URL)))
  {
    ut_update_rep_file_for_page_dump(vptr, bytes_read, buf);
  }

  // Update througtput for Overall
  cptr->tcp_bytes_recv += bytes_read;
  average_time->rx_bytes += bytes_read;

  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->rx_bytes += bytes_read;
  }
}

void handle_recv(connection *cptr, u_ns_ts_t now)
{
  #ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
  #else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
  #endif

  int bytes_read;
  action_request_Shr* url_num;
  VUser *vptr = cptr->vptr;
  int read_offset = 0;
  int ret;
  int msg_peek = 0;

  NSDL2_SOCKETS(NULL, cptr, "Method called. cptr=%p, cptr->num_pipe = %d, cptr->req_code_filled = %d, "
      "cptr->req_code = %d, cptr->conn_state = %d, cptr->header_state = %d, cptr->content_length = %d",
       cptr, cptr->num_pipe, cptr->req_code_filled, cptr->req_code,
       cptr->conn_state, cptr->header_state, cptr->content_length);


  ret = before_read_start(cptr, now, &url_num, &bytes_read);
  if(ret == 1)
    return;

  while (1)
  {
    if(!msg_peek)
    {
      if (do_throttle)
        bytes_read = THROTTLE / 2;
      else
        bytes_read = sizeof(buf);
    }

    if(IS_REQUEST_SSL_OR_NONSSL_SOCKET)
    {
      if(!msg_peek)
        ns_socket_bytes_to_read(cptr, vptr, &bytes_read, &msg_peek);
      else 
      {
        msg_peek = 0;
        bytes_read = read_offset;
      }
    }
  
    NSDL1_SOCKETS(NULL, NULL, "If part of ENABLE_SSL ");

    /*========================================================================= 
      [HINT: SSLHandling]
          SSL connection MUST not made in following condingtions -
          1. Request type is HTTPS and Proxy is used, 
             (a) Proxy Handshake is not done i.e. 
                 proxy bit NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT is set
             (b) Proxy Authentication is enable 
    =========================================================================*/
    if(IS_SSL_READ)
    {
      ret = ssl_read(cptr, vptr, url_num, buf, now, &bytes_read, msg_peek);
      if(ret == NS_SOCKET_IO_READ_ERROR)
        return;
      else if(ret == NS_SOCKET_IO_READ_CONTINUE)
        continue;
    }
    else
    {
      NSDL1_SOCKETS(NULL, NULL, "msg_peek = %d, bytes_read = %d, read_offset = %d", msg_peek, bytes_read, read_offset);

      ret = non_ssl_read(cptr, vptr, url_num, buf, &bytes_read, msg_peek, now);
      if(ret == NS_SOCKET_IO_READ_ERROR)
        return;
    }

    if(bytes_read > 0)  
    {
      #ifdef NS_DEBUG_ON
      NSDL1_SOCKETS(NULL, NULL, "*******request type = %d", cptr->request_type);
      debug_log_http_res(cptr, buf, bytes_read);
      #else //NS_DEBUG_ON
      LOG_HTTP_RES(cptr, buf, bytes_read);
      #endif //NS_DEBUG_ON

      after_read_done(cptr, buf, bytes_read);
    }

    /* Register function will be called for this cptr
       Now, it is set only for socket API in execute_page fucntion
       after getting cptr from get_free_connection_slot
       Fucntion for socket: process_socket_recv_data()
    */

    NSDL1_SOCKETS(NULL, NULL, "Calling callback fun. proc_recv_data(), "
        "cptr = %p, bytes_read = %d, read_offset = %d", 
         cptr, bytes_read, read_offset);

    ret = cptr->proc_recv_data((struct connection *)cptr, buf, bytes_read, &read_offset, msg_peek);

    if(bytes_read == 0)
    {
      handle_server_close(cptr, now); 
      ret = 1;
    }
    
    if(ret == 1)
    {
      NSDL1_SOCKETS(NULL, NULL, "Goto NS core engine");
      return;
    }
    else
    {
      NSDL1_SOCKETS(NULL, NULL, "Continue to read...");
      continue;
    }
  } //end of while
}
