/******************************************************************
 * Name    :    ns_ssl.c
 * Purpose :    This file contains methods related to SSL
 * Note    :
 * Author  :    Archana
 * Intial version date:    19/06/08
 * Last modification date: 13/07/09
*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "url.h"
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

#include "netstorm.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
#include "ns_msg_com_util.h"
#include "ns_sock_com.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "ns_debug_trace.h"
#include "ns_page.h"
#include "ns_page_dump.h"
#include "nslb_util.h"
#include "ns_parent.h"
#include "ns_imap.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_pop3.h"
#include "ns_smtp.h"
#include "ns_ssl_key_log.c"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_url_req.h"
#include "ns_socket.h"
#include "ns_h2_req.h"
#include "nslb_cav_conf.h"

#ifdef ENABLE_SSL

#ifndef CAV_MAIN
SSL_CTX **g_ssl_ctx;
#else
__thread SSL_CTX **g_ssl_ctx;
#endif
NormObjKey cert_key_norm_tbl;
SSLCertKeyData *ssl_cert_key_data;
SSLCertKeyData_Shr *ssl_cert_key_data_shr;

void err_exit(char *string)
{
  NS_EXIT(0, "%s",string);
}

/**********************************************************************************
SSL context is created only once using ssl_main_init() per server host.
These context are never destroyed.
Following possibilities exists for contexts:

Context may need to be: 
       - per client address (if client Auth is used),
       - per server address
       - per client Cipher choices

Client address choices are: No Aurth, 1 Address, Multiple addresses
Server Address            : 1 Address, multi address

1. Client = No Auth or 1 address and server = 1 address, can just have one conext
2. Client = No auth or 1 address auth and server = Multi can be supported with context for each host
3. Multi client auth or multi brwoser based cipher choive need conext for each user

Note: Case 3 is not supported yet.

Sessions:
Session is saved on per user per host basis. and can be reused.
Session is freeed after every get on connection shutdown.

Only one session is kept for a user/host combo. Last session is saved and previous freeed.
Following situatation may occour:
c1,a2,a3 are parallel connections of an user
s1 is session id from first (c0) connection.

following horizontal starting point on a line on on elapsed time axis.
also, number on left side shows the sequence in time

1)c1 try to use s1
6)    session us reused. dont free anything, keep last (s3)
2)c2 try to use s1
4)    session not reused by server . new session s2  : free s1 - keep s2
3)c3 try to use s1 NR - s3  (dont free s1, free s2) - keep3
5)    session not reused by server . new session s3:  dont free free s1, free s2 (last) and save s3

session are saved in ssl_save_sess() and freed by ssl_free_see
when user is cleaned up.

SSL_AVG_REUSE is the % os users with Reuse session. Default is 100%.

**********************************************************************************/

//Called only for https after TCP connect
//Called  once by each child
//Can be called by parent itself abd childern inharit
inline void ssl_main_init()
{
  /* Initialize the SSL stuff */
  NSDL2_SSL(NULL, NULL, "Method called, g_cur_server = %d", g_cur_server);

  short tls_version = 0; // setting value to default sslv2_3 
  int g = 0;
  SSLeay_add_ssl_algorithms();
  SSL_load_error_strings();

  MY_MALLOC (g_ssl_ctx , sizeof (SSL_CTX *) * (g_cur_server+1) * (total_runprof_entries), "g_ssl_ctx ", -1);
  NSDL3_SSL(NULL, NULL, "Allocated %d SSL CTX bytes for %d hosts", (sizeof (SSL_CTX *) * (g_cur_server+1)), (g_cur_server+1));

  int hnum;
  SSL_CTX *my_ssl_ctx;
  SSL_CTX **g_ssl_ctx_ptr;
  GroupSettings *gset;

  for(g = 0 ; g < total_runprof_entries; g++)
  {

    g_ssl_ctx_ptr = g_ssl_ctx + ((g_cur_server+1) * (g));
    gset = &runprof_table_shr_mem[g].gset;
    
    for (hnum=0; hnum <= g_cur_server; hnum++) 
    {
       tls_version = gserver_table_shr_mem[hnum].tls_version;
       if (tls_version == -1){
         NSDL1_SSL(NULL, NULL, "HOST_TLS_VERSION is not given . Going to check if TLS_VERSION is specified in scenario");
         tls_version = gset->tls_version; 
       }
       NSDL1_SSL(NULL, NULL, "tls_version is %hd", tls_version);
       
       #if OPENSSL_VERSION_NUMBER >= 0x10100000L
       my_ssl_ctx = SSL_CTX_new(TLS_client_method());
       switch (tls_version)
       {
         case SSL3_0:
              NSDL2_SSL(NULL, NULL, "Method called SSLv3_client_method");
              SSL_CTX_set_min_proto_version(my_ssl_ctx, SSL3_VERSION);
              SSL_CTX_set_max_proto_version(my_ssl_ctx, SSL3_VERSION);
              break;
         case TLS1_0:
              NSDL2_SSL(NULL, NULL, "Method called TLSv1.0_client_method");
              SSL_CTX_set_min_proto_version(my_ssl_ctx, TLS1_VERSION);
              SSL_CTX_set_max_proto_version(my_ssl_ctx, TLS1_VERSION);
              break;
         case TLS1_1:
              NSDL2_SSL(NULL, NULL, "Method called TLSv1.1_client_method");
              SSL_CTX_set_min_proto_version(my_ssl_ctx, TLS1_1_VERSION);
              SSL_CTX_set_max_proto_version(my_ssl_ctx, TLS1_1_VERSION);
              break;
         case TLS1_2:
              NSDL2_SSL(NULL, NULL, "Method called TLSv1.2_client_method");
              SSL_CTX_set_min_proto_version(my_ssl_ctx, TLS1_2_VERSION);
              SSL_CTX_set_max_proto_version(my_ssl_ctx, TLS1_2_VERSION);
              break;
         case TLS1_3:
              NSDL2_SSL(NULL, NULL, "Method called TLSv1.3_client_method");
              SSL_CTX_set_min_proto_version(my_ssl_ctx, TLS1_3_VERSION);
              SSL_CTX_set_max_proto_version(my_ssl_ctx, TLS1_3_VERSION);
              break;
         default:
              NSDL2_SSL(NULL, NULL, "Method called TLS_client_method");
      }
      #else
      switch (tls_version)
      {
         case SSL3_0:
           NSDL2_SSL(NULL, NULL, "Method called SSLv3_client_method");
           my_ssl_ctx = SSL_CTX_new(SSLv3_client_method());
         break;
         case TLS1_0:
           NSDL2_SSL(NULL, NULL, "Method called TLSv1_client_metho");
           my_ssl_ctx = SSL_CTX_new(TLSv1_client_method());
         break;
         case TLS1_1:
           NSDL2_SSL(NULL, NULL, "Method called TLSv1_1_client_method");
           my_ssl_ctx = SSL_CTX_new(TLSv1_1_client_method());
         break;
         case TLS1_2:
           NSDL2_SSL(NULL, NULL, "Method called TLSv1_2_client_method");
           my_ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());
         break;
         default:
           NSDL2_SSL(NULL, NULL, "Method called SSLv23_client_method");
           my_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
      }
      #endif  

      NSDL3_SSL(NULL, NULL, "my_ssl_ctx[%d] = %p", hnum, my_ssl_ctx);
      g_ssl_ctx_ptr[hnum] = my_ssl_ctx;
      
      if (g_ssl_ctx_ptr[hnum] == NULL) 
      {
        NS_EXIT(1, "Can't allocate SSL context");
      }
    }
  }
  NSDL3_SSL(NULL, NULL, "Client SSL Cipher choices are : %s", group_default_settings->ssl_ciphers);
}
//Set h2 protocol 
inline void ns_set_h2_protocol(SSL *ssl , int mode)
{
  NSDL2_SSL(NULL,NULL, "Method called");
  unsigned char proto_list[] = {
       2, 'h', '2', // h2 protocol length = 3
       8, 'h', 't', 't', 'p', '/', '1', '.', '1' // http/1.1 protocol length = 9
  };
  /* Set h2 & http/1.1 protocol in case mode is 0 otherwise set h2 only */
  unsigned short proto_len = 3 + ((mode == 0) ? 9 : 0);  

  NSDL2_SSL(NULL,NULL, "Protocol = %.*s",proto_len,proto_list);
  //SSL_CTX_set_next_proto_select_cb(ssl_ctx, SSL_select_next_proto,NULL);    
  //SSL_CTX_set_alpn_protos(ssl_ctx, (const unsigned char *)proto_list,proto_len);
  /*Moved ALPN to SSL connection instead of SSL Cotext*/
  SSL_set_alpn_protos(ssl, (const unsigned char *)proto_list, proto_len);
}

//Check for h2 protocol
inline int ns_check_h2_protocol(SSL *ssl)
{
  const unsigned char *next_proto = NULL;
  unsigned int next_proto_len;

  NSDL2_SSL(NULL,NULL, "Method called");

  SSL_get0_next_proto_negotiated(ssl, &next_proto, &next_proto_len);
  if(next_proto == NULL)
  {
    SSL_get0_alpn_selected(ssl, &next_proto, &next_proto_len);
  }
  /*check for h2*/
  if (next_proto== NULL || next_proto_len != 2 || memcmp("h2", next_proto,2) != 0) 
  {
    NSDL2_SSL(NULL,NULL, "h2 is not negotiated");
    return 0;
  }
  NSDL2_SSL(NULL,NULL, "h2 is negotiated");
  return 1;
}

//0 :Success
//-1 : Error
int set_ssl_con (connection *cptr)
{
  /* SSL stuff */
  int hnum;
  int ii;
  X509 *cert_ptr;
  EVP_PKEY *key_ptr;
  HostSvrEntry *hptr;
  //SvrTableEntry *LocSvrTableEntry;
  VUser *vptr = cptr->vptr;
  SSLExtraCert *tmp_extra_cert;
  SSL_CTX **g_ssl_ctx_ptr = g_ssl_ctx + ((g_cur_server+1) * (vptr->group_num));

  NSDL2_SSL(NULL, cptr, "Method called. fd = %d", cptr->conn_fd);

  if (vptr->ssl_mode == NS_SSL_UNINIT) 
  {
    for (ii=0; ii<= g_cur_server; ii++) 
    {
      NSDL1_SSL(NULL, cptr, "ctx %d is %p\n", ii, g_ssl_ctx_ptr[ii]);
      hptr = vptr->hptr + ii;
      hptr->sess = NULL;
    }
    if (runprof_table_shr_mem[vptr->group_num].gset.avg_ssl_reuse > (rand()%100))
    {
      vptr->ssl_mode = NS_SSL_REUSE;
      NSDL2_SSL(NULL, cptr, "Setting virtual user ssl_mode to reuse");
      NS_DT4(vptr, cptr, DM_L1, MM_SSL, "Setting virtual user ssl_mode to reuse");
    }
    else
    {
      vptr->ssl_mode = NS_SSL_NO_REUSE;
      NSDL2_SSL(NULL, cptr, "Setting virtual user ssl_mode to no reuse");
      NS_DT4(vptr, cptr, DM_L1, MM_SSL, "Setting virtual user ssl_mode to no reuse");
    }
  }

  hnum = cptr->gServerTable_idx;
  NSDL3_SSL(NULL, cptr, "Host index is %d", hnum);
  hptr = vptr->hptr + hnum;

  /********************* SSL Certificate key code ****************/
  NSDL2_SSL(NULL, cptr, "VPTR : SSL Setting are cert_idx = %d, and key_idx = %d", vptr->httpData->ssl_cert_id, vptr->httpData->ssl_key_id);
  NSDL2_SSL(NULL, cptr, "CPTR : SSL Setting are cert_idx = %d, and key_idx = %d", cptr->ssl_cert_id, cptr->ssl_key_id);
  if (vptr->httpData->ssl_cert_id != cptr->ssl_cert_id)
  {
    if(vptr->httpData->ssl_cert_id >=0)
    {
      NSDL4_SSL(NULL, NULL, "SSL Certificate Found, using certificate on context");
      cert_ptr = (X509 *)ssl_cert_key_data[vptr->httpData->ssl_cert_id].ssl_cert_key_addr;  
      if(!SSL_CTX_use_certificate(g_ssl_ctx_ptr[hnum], cert_ptr))
      {
        NS_EXIT(1, "Couldn't read certificate file");
      }
      if(ssl_cert_key_data[vptr->httpData->ssl_cert_id].extra_cert)
      {
        tmp_extra_cert = ssl_cert_key_data[vptr->httpData->ssl_cert_id].extra_cert;
        while(tmp_extra_cert != NULL)
        {
          NSDL2_SSL(NULL, NULL, "Chain certificate found while using on ctx");
          cert_ptr = (X509 *)tmp_extra_cert->ssl_cert_key_addr;
          NSDL2_SSL(NULL, NULL, "chain cert pointer is %p", cert_ptr);
          SSL_CTX_add_extra_chain_cert(g_ssl_ctx_ptr[hnum], cert_ptr);
          tmp_extra_cert = tmp_extra_cert->next;
        }
      }
    }
    if(vptr->httpData->ssl_key_id >=0)
    {
      key_ptr = (EVP_PKEY *)ssl_cert_key_data[vptr->httpData->ssl_key_id].ssl_cert_key_addr;
      if(!SSL_CTX_use_PrivateKey(g_ssl_ctx_ptr[hnum], key_ptr))
      {
        NS_EXIT(1, "Couldn't read key file");
      }
    }  
    /*Free Any Exsitng SSL Setting*/ 
    NSDL3_SSL(NULL, cptr, "Free any exiting ssl settings");

    cptr->ssl_cert_id = vptr->httpData->ssl_cert_id;
    cptr->ssl_key_id = vptr->httpData->ssl_key_id;
  }

  NSDL2_SSL(NULL, cptr, "runprof_table_shr_mem[vptr->group_num].gset.ssl_ciphers = %s cptr->url_num->proto.http.http_version=%d\n", runprof_table_shr_mem[vptr->group_num].gset.ssl_ciphers, cptr->url_num->proto.http.http_version);
  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    if (SSL_CTX_get_min_proto_version(g_ssl_ctx_ptr[hnum]) == 772)
      SSL_CTX_set_ciphersuites(g_ssl_ctx_ptr[hnum], runprof_table_shr_mem[vptr->group_num].gset.ssl_ciphers);
    else
      SSL_CTX_set_cipher_list(g_ssl_ctx_ptr[hnum], runprof_table_shr_mem[vptr->group_num].gset.ssl_ciphers);
  #else
    SSL_CTX_set_cipher_list(g_ssl_ctx_ptr[hnum], runprof_table_shr_mem[vptr->group_num].gset.ssl_ciphers);
  #endif
 /*Free Any Exsitng SSL Setting*/ 
  NSDL3_SSL(NULL, cptr, "Free any exiting ssl settings");
  ssl_struct_free(cptr);

  /* Create new ssl */
  NSDL3_SSL(NULL, cptr, "Creating new SSL using SSL_new()");
  cptr->ssl = SSL_new(g_ssl_ctx_ptr[hnum]);
  if (cptr->ssl == NULL) 
  {
    fprintf(stderr, "Can't allocate SSL connection handle\n");
    return -1;
  } 
  NSDL3_SSL(NULL, cptr, "Started new ssl %p with ctx=%p\n", cptr->ssl, g_ssl_ctx_ptr[hnum]);
  //Set h2 protocol in case if http_mode is auto or http2)
  if((cptr->request_type == HTTPS_REQUEST) && ((runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_AUTO) || 
		(runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_HTTP2) ||
                   (HTTP_MODE_HTTP2 == cptr->url_num->proto.http.http_version))) /*bug 95325: added OR for gRPC*/
  {
      ns_set_h2_protocol(cptr->ssl, runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode);
  }
 
  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    if (SSL_CTX_get_min_proto_version(g_ssl_ctx_ptr[hnum]) == 772)
      if(runprof_table_shr_mem[vptr->group_num].gset.post_hndshk_auth_mode)
        SSL_set_post_handshake_auth(cptr->ssl, 1); 
  #endif

  SSL_set_fd(cptr->ssl, cptr->conn_fd);
  SSL_load_error_strings();
  SSL_set_connect_state(cptr->ssl);
  
  if (vptr->ssl_mode == NS_SSL_NO_REUSE) 
  {
    (average_time->ssl_new)++;
    NS_DT4(vptr, cptr, DM_L1, MM_SSL, "New SSL session to be created as ssl_mode is no reuse. ssl_new = %d", average_time->ssl_new);
    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      (lol_average_time->ssl_new)++;
    }
  } 
  else 
  {
    //NSDL3_SSL(NULL, cptr, "host %s with port=%d\n", cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_port);
    //snprintf(host,1024, "%s:%d", cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_port);
    /* Reuse session */
    if (hptr->sess) 
    {
      SSL_set_session(cptr->ssl, hptr->sess);
      (average_time->ssl_reuse_attempted)++;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        (lol_average_time->ssl_reuse_attempted)++;
      }
      NS_DT4(vptr, cptr, DM_L1, MM_SSL, "SSL session is reused attempted with sess=%p. ssl_reuse_attempted = %d", hptr->sess, average_time->ssl_reuse_attempted);
    } 
    else 
    {
      (average_time->ssl_new)++;
      NS_DT4(vptr, cptr, DM_L1, MM_SSL, "New SSL session to be created as no SSL session yet created. ssl_new = %d", average_time->ssl_new);
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        (lol_average_time->ssl_new)++;
      }
    }
  }
  return 0;
}

//Called at the time of user cleanup
void ssl_sess_free (VUser *vptr)
{
  int ii;
  HostSvrEntry *hptr;

  NSDL2_SSL(vptr, NULL, "Method called");
  if (vptr->ssl_mode == NS_SSL_REUSE) 
  {
    for (ii=0; ii<= g_cur_server; ii++) 
    {
      // NSDL1_SSL(vptr, cptr, "ctx %d is 0x%X\n", ii, (unsigned int)g_ssl_ctx[ii]);
      hptr = vptr->hptr + ii;
      if (hptr->sess) 
      {
        NS_DT4(vptr, NULL, DM_L1, MM_SSL, "Freeing SSL session for host index = %d. sess = %p", ii, hptr->sess);
        SSL_SESSION_free (hptr->sess);
        hptr->sess = NULL;
      }
    }
  }
}

inline void ssl_free_send_buf(connection *cptr)
{
  NSDL2_SSL(NULL, cptr, "Method called. cptr->http2_state=%d", cptr->http2_state);
  /*bug 86575: moved line inside if block */
  if (cptr->http2_state != HTTP2_CON_PREFACE_CNST)
  {
    NSDL2_SSL(NULL, cptr, "Freeing %p", cptr->free_array);
    FREE_AND_MAKE_NULL_EX (cptr->free_array,  cptr->free_array_size, "cptr->free_array", -1); 
    cptr->free_array_size = 0;
  }
}

/*------------------------------------------------------------
Function to handle SMTP SSL write.
we will write cptr->free_array to server
as we have already converted vectors to buffer(cptr->free_array)

------------------------------------------------------------*/

void handle_smtp_ssl_write(connection *cptr, u_ns_ts_t now)
{
  int i;
  char *ptr_ssl_buff;
  int bytes_left_to_send = cptr->bytes_left_to_send;
  VUser *vptr = cptr->vptr;

  NSDL2_SSL(NULL, cptr, "Method called. tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  ptr_ssl_buff = cptr->free_array + cptr->tcp_bytes_sent;

  ERR_clear_error();
  i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
  switch (SSL_get_error(cptr->ssl, i)) 
  {
    case SSL_ERROR_NONE:
      cptr->tcp_bytes_sent += i;
      average_time->tx_bytes += i;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_bytes += i;
      }
      //if (i >= cptr->bytes_left_to_send) 
      if (i >= bytes_left_to_send) 
      {
        cptr->bytes_left_to_send -= i;
        //all sent
#ifdef NS_DEBUG_ON
        debug_log_smtp_req(cptr, ptr_ssl_buff, i, 1, 1);
#endif
        break;
      } 
      else
      {
        cptr->bytes_left_to_send -= i;
        debug_log_smtp_req(cptr, ptr_ssl_buff, i, 0, 1);
      }
    /* Next two errors can but are not supposed to happen */
    case SSL_ERROR_WANT_WRITE:
      //Go back to read more data
      //Keep buffer position same for SSL_ERROR_WANT_WRITE
      //fprintf(stderr, "SSL_write warn: SSL_WANT_WRITE\n");
      cptr->conn_state = CNST_SSL_WRITING;
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset );
#endif
      return;
    case SSL_ERROR_WANT_READ:
      printf("SSL_write error: SSL_ERROR_WANT_READ\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_ZERO_RETURN:
      printf("SSL_write error: aborted\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_SSL:
      ERR_print_errors_fp(stderr);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    default:
      printf("SSL_write error: errno=%d\n", errno);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
  }

  ssl_free_send_buf(cptr);

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  on_request_write_done (cptr);
}

void handle_pop3_ssl_write (connection *cptr, u_ns_ts_t now)
{
  int i;
  char *ptr_ssl_buff;
  int bytes_left_to_send = cptr->bytes_left_to_send;
  VUser *vptr = cptr->vptr;

  NSDL2_SSL(NULL, cptr, "Method called. tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  ptr_ssl_buff = cptr->free_array + cptr->tcp_bytes_sent;

  ERR_clear_error();
  i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
  switch (SSL_get_error(cptr->ssl, i)) 
  {
    case SSL_ERROR_NONE:
      cptr->tcp_bytes_sent += i;
      average_time->tx_bytes += i;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_bytes += i;
      }
      //if (i >= cptr->bytes_left_to_send) 
      if (i >= bytes_left_to_send) 
      {
        cptr->bytes_left_to_send -= i;
        //all sent
#ifdef NS_DEBUG_ON
        debug_log_pop3_req(cptr, ptr_ssl_buff, i, 1, 1);
#endif
        break;
      } 
      else
      {
        cptr->bytes_left_to_send -= i;
        debug_log_pop3_req(cptr, ptr_ssl_buff, i, 0, 1);
      }
    /* Next two errors can but are not supposed to happen */
    case SSL_ERROR_WANT_WRITE:
      //Go back to read more data
      //Keep buffer position same for SSL_ERROR_WANT_WRITE
      //fprintf(stderr, "SSL_write warn: SSL_WANT_WRITE\n");
      cptr->conn_state = CNST_SSL_WRITING;
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset );
#endif
      return;
    case SSL_ERROR_WANT_READ:
      printf("SSL_write error: SSL_ERROR_WANT_READ\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_ZERO_RETURN:
      printf("SSL_write error: aborted\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_SSL:
      ERR_print_errors_fp(stderr);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    default:
      printf("SSL_write error: errno=%d\n", errno);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
  }

  ssl_free_send_buf(cptr);

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  on_request_write_done (cptr);
}

void handle_imap_ssl_write (connection *cptr, u_ns_ts_t now)
{
  int i;
  char *ptr_ssl_buff;
  int bytes_left_to_send = cptr->bytes_left_to_send;
  VUser *vptr = cptr->vptr;

  NSDL2_SSL(NULL, cptr, "Method called. tcp_bytes_sent = %d, bytes_left_to_send = %d", cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  ptr_ssl_buff = cptr->free_array + cptr->tcp_bytes_sent;

  ERR_clear_error();
  i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
  switch (SSL_get_error(cptr->ssl, i)) 
  {
    case SSL_ERROR_NONE:
      cptr->tcp_bytes_sent += i;
      average_time->tx_bytes += i;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_bytes += i;
      }
      //if (i >= cptr->bytes_left_to_send) 
      if (i >= bytes_left_to_send) 
      {
        cptr->bytes_left_to_send -= i;
        //all sent
#ifdef NS_DEBUG_ON
        debug_log_imap_req(cptr, ptr_ssl_buff, i, 1, 1);
#endif
        break;
      } 
      else
      {
        cptr->bytes_left_to_send -= i;
        debug_log_imap_req(cptr, ptr_ssl_buff, i, 0, 1);
      }
    /* Next two errors can but are not supposed to happen */
    case SSL_ERROR_WANT_WRITE:
      //Go back to read more data
      //Keep buffer position same for SSL_ERROR_WANT_WRITE
      //fprintf(stderr, "SSL_write warn: SSL_WANT_WRITE\n");
      cptr->conn_state = CNST_SSL_WRITING;
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset );
#endif
      return;
    case SSL_ERROR_WANT_READ:
      printf("SSL_write error: SSL_ERROR_WANT_READ\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_ZERO_RETURN:
      printf("SSL_write error: aborted\n");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    case SSL_ERROR_SSL:
      ERR_print_errors_fp(stderr);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
    default:
      printf("SSL_write error: errno=%d\n", errno);
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return;
  }

  ssl_free_send_buf(cptr);

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  on_request_write_done (cptr);
}
/*bug 84661: h2_update_flow_control() defined*/
static inline void h2_update_flow_control(connection *cptr, stream *sptr, int bytes_sent)
{
  NSDL2_SSL(NULL, cptr, "Method called. bytes_sent=%d",bytes_sent );
  NSDL2_SSL(NULL, cptr, "before sptr[%p] bytes_left_to_send=%d, sptr remote_window_size=%d, cptr data_left_to_send=%d, cptr remote_window_size=%d", sptr, sptr->bytes_left_to_send, sptr->flow_control.remote_window_size, cptr->http2->flow_control.data_left_to_send, cptr->http2->flow_control.remote_window_size);

  sptr->bytes_left_to_send -= bytes_sent;
  cptr->http2->flow_control.data_left_to_send -= bytes_sent;
 // if(sptr->flow_control.remote_window_size > 0)
  sptr->flow_control.remote_window_size -= bytes_sent;
  //if(cptr->http2->flow_control.remote_window_size > 0)
  cptr->http2->flow_control.remote_window_size -= bytes_sent;

  NSDL2_SSL(NULL, cptr, " after sptr bytes_left_to_send=%d, sptr remote_window_size=%d, cptr data_left_to_send=%d, cptr remote_window_size=%d", sptr->bytes_left_to_send, sptr->flow_control.remote_window_size, cptr->http2->flow_control.data_left_to_send, cptr->http2->flow_control.remote_window_size);

}

static inline int h2_get_bytes_left_to_send(connection *cptr, stream *sptr, int bytes_left_to_send)
{
  NSDL2_SSL(NULL, cptr, "Method called. sptr->bytes_left_to_send=%d bytes_left_to_send=%d",sptr->bytes_left_to_send, bytes_left_to_send );
  //This will be set First time only : Stream level
  if(!sptr->bytes_left_to_send)
    sptr->bytes_left_to_send = bytes_left_to_send;

  if(!cptr->http2->flow_control.data_left_to_send)
    cptr->http2->flow_control.data_left_to_send = cptr->http2->flow_control.remote_window_size;

   NSDL2_SSL(NULL, cptr, "Before cptr = %p sptr = %p bytes_left_to_send = %d sptr->bytes_left_to_send = %d sptr->flow_control.remote_window_size = %d cptr->http2->flow_control.remote_window_size = %d", cptr, sptr, bytes_left_to_send, sptr->bytes_left_to_send, sptr->flow_control.remote_window_size, cptr->http2->flow_control.remote_window_size);

  //Stream Level flow control
  bytes_left_to_send = sptr->bytes_left_to_send; //left bytes at stream

  if(bytes_left_to_send >= sptr->flow_control.remote_window_size)
  {
    bytes_left_to_send = sptr->flow_control.remote_window_size;
    //sptr->flow_control.remote_window_size = 0;
  }

  //Connection level flow control
  if(bytes_left_to_send >= cptr->http2->flow_control.remote_window_size)
  {
    bytes_left_to_send = cptr->http2->flow_control.remote_window_size;
    //cptr->http2->flow_control.remote_window_size = 0;
  }

  if(bytes_left_to_send < 0)
    bytes_left_to_send = 0;

  cptr->bytes_left_to_send = sptr->bytes_left_to_send;

  NSDL2_SSL(NULL, cptr, "After flow control cptr = %p sptr = %p bytes_left_to_send = %d sptr->bytes_left_to_send = %d sptr->flow_control.remote_window_size = %d cptr->http2->flow_control.remote_window_size = %d", cptr, sptr, bytes_left_to_send, sptr->bytes_left_to_send, sptr->flow_control.remote_window_size, cptr->http2->flow_control.remote_window_size);

 return bytes_left_to_send;
}
/**** bug 84661 END ***/

//Called to write ssl request. May be called multiple times, if request is big
int handle_ssl_write (connection *cptr, u_ns_ts_t now)
{
  int i;
  int bytes_left_to_send;
  int errorCode;
  VUser *vptr = cptr->vptr;
  stream *sptr;

  NSDL2_SSL(NULL, cptr, "Method called. cptr = %p, tcp_bytes_sent = %d, bytes_left_to_send = %d, cptr->ssl = %d",
                         cptr, cptr->tcp_bytes_sent, cptr->bytes_left_to_send, cptr->ssl);

  /* A soon as we create request we mark flag to expect continue hdr
   * so here it we did not send complete header in case of expect
   * 100-continue  hence we have to fill bytes_left_to_send with the 
   * header size*/
  if(!(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR)) {
    bytes_left_to_send = cptr->bytes_left_to_send;
    NSDL2_SSL(NULL, cptr, "bytes_left_to_send = %d", bytes_left_to_send);
  } else {
    bytes_left_to_send = cptr->bytes_left_to_send - cptr->content_length;  // Header size only
    NSDL2_SSL(NULL, cptr, "Rearranging bytes_left_to_send (%d) as we are in"
			  " Expect:100-Continue. content_length = %d",
			  bytes_left_to_send, cptr->content_length);
  }

  //Handling of flow control in case of HTTP2
  if((cptr->http_protocol == HTTP_MODE_HTTP2) && cptr->http2)
  {
    sptr = cptr->http2->http2_cur_stream;
    /*bug 84661 - moved code to inline method h2_get_bytes_left_to_send()*/
    bytes_left_to_send = h2_get_bytes_left_to_send(cptr, sptr, bytes_left_to_send);
  }

  NSDL2_SSL(NULL, cptr, "cptr->tcp_bytes_sent = %d bytes_left_to_send=%d", cptr->tcp_bytes_sent, bytes_left_to_send);
  char *ptr_ssl_buff = cptr->free_array + cptr->tcp_bytes_sent;
  NSDL2_SSL(NULL, cptr, "ptr_ssl_buff=%p cptr->free_array=%p cptr->tcp_bytes_sent=%d",ptr_ssl_buff,cptr->free_array,cptr->tcp_bytes_sent);
  if(cptr->ssl){
    NSDL2_SSL(NULL, cptr, "SSL write: ");
    ERR_clear_error();
    i = SSL_write(cptr->ssl, ptr_ssl_buff, bytes_left_to_send);
    errorCode = SSL_get_error(cptr->ssl, i);
  } else{ 
    // Here we are putting a safety check for cptr->ssl, in case http url is redirected to https and same port as http is coming in location
    // header. In this case its getting same server entry and coming here without doing ssl setup. 
    errorCode = SSL_ERROR_SSL;
    NS_DT1(cptr->vptr, cptr, DM_L1, MM_SSL, "Got cptr->ssl NULL, hence setting error to SSL_ERROR_SSL. This may be a case of redirect"
                                    " url going http to https and port for both protocol is same, Calling retry_connection");
  }

  NSDL2_SSL(NULL, cptr, "errorCode = %d, i = %d", errorCode, i);
  IW_UNUSED(char *err_buff);
  switch (errorCode) 
  {
    case SSL_ERROR_NONE:   //errorCode == 0
      NSDL4_SSL(NULL, cptr, "SSL_ERROR_NONE: byes_sent = %d, cptr->tcp_bytes_sent = %d, bytes_left_to_send = %d", 
                             i, cptr->tcp_bytes_sent, bytes_left_to_send);
      cptr->tcp_bytes_sent += i;
      average_time->tx_bytes += i;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->tx_bytes += i;
      }

      if (IS_TCP_CLIENT_API_EXIST)
        fill_tcp_client_avg(vptr, SEND_THROUGHPUT, cptr->tcp_bytes_sent);

      //if (i >= cptr->bytes_left_to_send)
      /*bug 84661 -> moved common code block from if-else to outside*/ 
      cptr->bytes_left_to_send -= i;
      if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2){
        /*bug 84661: moved deuplicate code block to a method*/
        h2_update_flow_control(cptr, sptr, i);
      }

      if (i >= bytes_left_to_send) 
      {
         //all sent
#ifdef NS_DEBUG_ON
        debug_log_http_req(cptr, ptr_ssl_buff, i, 1, 1);
#else
        if (cptr->http_protocol != HTTP_MODE_HTTP2){
          VUser* vptr;
          LOG_HTTP_REQ(cptr, ptr_ssl_buff, i, 1, 1);
        }
#endif
        break;
      } 
      else
      {
       //#ifdef NS_DEBUG_ON
        debug_log_http_req(cptr, ptr_ssl_buff, i, 0, 1);
//#endif
        //FALL THRU
      }
     NSDL3_HTTP(NULL, cptr, "cptr->bytes_left_to_send = %d", cptr->bytes_left_to_send);
    /* Next two errors can but are not supposed to happen */
    case SSL_ERROR_WANT_WRITE:
      //Go back to read more data
      //Keep buffer position same for SSL_ERROR_WANT_WRITE
      //fprintf(stderr, "SSL_write warn: SSL_WANT_WRITE\n");
      cptr->conn_state = CNST_SSL_WRITING;
#ifndef USE_EPOLL
      FD_SET( cptr->conn_fd, &g_wfdset );
#endif
     NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_WRITE occurred", cptr->bytes_left_to_send);
      return -1;
    case SSL_ERROR_WANT_READ:
     NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_READ occurred", cptr->bytes_left_to_send);
     /*
     if(global_settings->ssl_regenotiation == 0 )
     {
       printf("SSL_write error: SSL_ERROR_WANT_READ\n");
       ssl_free_send_buf(cptr);
       retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
       return -2;
     }
     else
     */
     {
       cptr->conn_state = CNST_SSL_WRITING;
       return -1;
     }
    case SSL_ERROR_ZERO_RETURN:
      /*bug 54315: moved trace to log file*/
      NS_DT1(cptr->vptr, cptr, DM_L1, MM_SSL,"SSL_write error: aborted");
      NSDL1_SSL(NULL, cptr,"SSL_write error: aborted");
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -2;
    case SSL_ERROR_SSL:
     // ERR_print_errors_fp(stderr);
      /*bug 54315 : moved error to debug log*/
      NSDL1_SSL(NULL, cptr,"error=%d ", SSL_ERROR_SSL);
      //err_buff = ERR_error_string(ERR_get_error(), NULL);
      IW_UNUSED(err_buff = ERR_error_string(ERR_get_error(), NULL));
      NS_DT1(NULL, cptr, DM_L1, MM_SSL,"SSL_write error: error=[%d] text=%s", errorCode, err_buff);
      NSDL1_SSL(NULL, cptr," SSL_write error:[%d] text= %s", errorCode, err_buff );
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -2;
    default:
      /*bug 54315: moved trace to log file*/
      NSDL1_SSL(NULL, cptr,"error ");
      IW_UNUSED(err_buff = ERR_error_string(ERR_get_error(), NULL));
      NS_DT1(NULL, cptr, DM_L1, MM_SSL,"SSL_write error: error=[%d] text=%s", errorCode, err_buff);
      NSDL1_SSL(NULL, cptr," SSL_write error:[%d] text= %s", errorCode, err_buff );
      ssl_free_send_buf(cptr);
      retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
      return -2;
  }

  /* We are done with writing in case of Expect: 100-continue,
   * so we need to return as we can not free free_array & some
   * other things done after follwoing block  
   */
  if(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) {
     /*we will come to this point only if we are done with header writin*/
     reset_after_100_continue_hdr_sent(cptr);
     //cptr->conn_state = CNST_READING;    
     NSDL3_HTTP(NULL, cptr, "Headers for Expect: 100-continue completely written, returning");
     return 0;
  }

  if(cptr->http_protocol != HTTP_MODE_HTTP2)
  {
    ssl_free_send_buf(cptr);
  }
  else if(!sptr->bytes_left_to_send)
  { 
      ssl_free_send_buf(cptr);
      sptr->free_array=NULL;    /*bug 54315  --> make sptr->free_array zero as well*/
  }
  NSDL3_HTTP(NULL, cptr, "conn_state = %d", cptr->conn_state);

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  cptr->conn_state = CNST_HEADERS;

  NSDL3_HTTP(NULL, cptr, "cptr = %p, cptr->http_protocol = %d", cptr, cptr->http_protocol); 
  if (cptr->http_protocol == HTTP_MODE_HTTP2)
  {
    /*bug 89702: set state to CNST_HTTP2_READING for reading WINDOW_UPDATE
	 or any read event and added copy_cptr_to_stream*/
    NSDL3_HTTP(NULL, cptr, "Setting state to CNST_HTTP2_READING");
    cptr->conn_state = CNST_HTTP2_READING;
    copy_cptr_to_stream(cptr, sptr);
  }

  on_request_write_done (cptr);

  // Force Reload or Click Away 
  if (cptr->url_num->proto.http.type == MAIN_URL)
   chk_and_force_reload_click_away(cptr, now);

  return 0;
}
#endif

/**********************************************************************************/
//Called before  a connection is closed
inline void ssl_struct_free (connection *cptr)
{
 
/* Comented by Neeraj on 03/21/09  as  we found that closed_fd() was getting caaled from ns_child.c
   and url_num may not be set.
    Also there may not be nbeed to checj this.
  if (cptr->url_num->request_type == HTTPS_REQUEST || 
      (cptr->url_num->proto.http.request_line && cptr->url_num->request_type == HTTPS_REQUEST))
*/ 
  {
    if (cptr->ssl)
    {
      NS_DT4(cptr->vptr, cptr, DM_L1, MM_SSL, "Shutting down and freeing SSL. ssl = %p", cptr->ssl);
      /*bug 79057 call SSL_shutdown() only if SSL_in_init() is NOT TRUE*/
      if(!( SSL_in_init(cptr->ssl))) {
        SSL_shutdown(cptr->ssl);
       }
      SSL_free(cptr->ssl);
      cptr->ssl = NULL;
    }
  }
#if 0
  if (cptr->sess) 
  {
    if (global_settings->debug) printf ("Freeing sess 0x%X\n", cptr->sess);
    SSL_SESSION_free(cptr->sess);
    cptr->sess = NULL;
  }
  if (cptr->ctx) 
  {
    SSL_CTX_free(cptr->ctx);
    cptr->ctx = NULL;
  }
#endif
}

inline void ssl_sess_save (connection *cptr)
{
  VUser *vptr=cptr->vptr;
  HostSvrEntry *hptr;
  //SSL_SESSION  *sess;

  NSDL2_SSL(NULL, cptr, "Method called");

  // DL_ISSUE
#ifdef NS_DEBUG_ON
  //if (global_settings->debug && DM_LOGIC2) && (global_settings->modulemask & MM_SSL)
  {
    SSL_CIPHER *cipher;
    char buf[1024];
    NSDL3_SSL(NULL, cptr, "Entered ssl_sess_save\n");
    cipher = (SSL_CIPHER *) SSL_get_current_cipher((const SSL *)cptr->ssl);
    if (!cipher)
    {
      NSDL3_SSL(NULL, cptr, "No SSL session established\n");
      return;
    }
    buf[0] = 0;
    SSL_CIPHER_description(cipher, buf, 1024);
    NSDL3_SSL(NULL, cptr, "Cipher: %s\n", buf);
  }
#endif

  hptr = vptr->hptr + cptr->gServerTable_idx;

  if (vptr->ssl_mode == NS_SSL_REUSE)
  {
    if (SSL_session_reused (cptr->ssl))  //Session reused
    {
      //hptr = vptr->hptr + get_svr_ptr(cptr->url_num, vptr)->idx;
      (average_time->ssl_reused)++;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->ssl_reused++;
      }
      cptr->flags |= NS_CPTR_FLAGS_SSL_REUSED; // Set resue bit as ir is used in logging of url record for drill down
      NSDL3_SSL(NULL, cptr, "SSL session is reused as expected. ssl_reused = %d", average_time->ssl_reused);
      NS_DT4(vptr, cptr, DM_L1, MM_SSL, "SSL session is reused as expected. ssl_reused = %d", average_time->ssl_reused);
    }
    else  //A new session is used
    {
      //hptr = vptr->hptr + get_svr_ptr(cptr->url_num, vptr)->idx;
      if (hptr->sess)
      {
        //free old session
        SSL_SESSION_free(hptr->sess);
        (average_time->ssl_new)++;
        NS_DT4(vptr, cptr, DM_L1, MM_SSL, "SSL session reuse attempted but rejected by server and new SSL session started by server. ssl_new = %d", average_time->ssl_new);
        NSDL3_SSL(NULL, cptr, "SSL session reuse attempted but rejected by server and new SSL session started by server. ssl_new = %d", average_time->ssl_new);
        if(SHOW_GRP_DATA) {
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->ssl_new++;
        }
      }
      else
      {
        NS_DT4(vptr, cptr, DM_L1, MM_SSL, "New SSL session started as expected");
        NSDL3_SSL(NULL, cptr, "New SSL session started as expected");
      }
      hptr->sess = SSL_get1_session(cptr->ssl);
      NSDL2_SSL(NULL, cptr, "SSL session id used %p", hptr->sess);
      #if OPENSSL_VERSION_NUMBER < 0x10100000L
      // New Connection, capture keys here
      if(global_settings->ssl_key_log)
      {
        NSDL2_SSL(NULL, cptr, "New SSL Connection, capture keys here");
        ns_ssl_key_log(cptr->ssl);
      }
      #endif
    }
  }  

 NSDL2_SSL(NULL, cptr, "cptr->url_num->proto.http.http_version=%d", cptr->url_num->proto.http.http_version);
 /*if(((runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_AUTO) || 
		(runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode == HTTP_MODE_HTTP2) ||
             (HTTP_MODE_HTTP2 == cptr->url_num->proto.http.http_version))) //bug 95325: Added OR check for gRPC
 {*/
   /* Check to h2 protocol and set the same in cptr*/
   if(ns_check_h2_protocol(cptr->ssl))
   {
      NSDL2_SSL(NULL, cptr, "Set protocol type HTTP2");
      cptr->http_protocol = HTTP_MODE_HTTP2;
      if(hptr)
        hptr->http_mode = HTTP_MODE_HTTP2;
   }
   else
   {
      NSDL2_SSL(NULL, cptr, "Set protocol type HTTP1");
      cptr->http_protocol = HTTP_MODE_HTTP1;
      if(hptr)
       hptr->http_mode = HTTP_MODE_HTTP1;
   }
/* }
 else
 {
    NSDL2_SSL(NULL, cptr, "Set protocol type HTTP1");
    cptr->http_protocol = HTTP_MODE_HTTP1;
    if(hptr)
     hptr->http_mode = HTTP_MODE_HTTP1;
 } */
}

static int inline ns_memncpy(char* dest, char* source, int num) 
{
  int i;

  NSDL2_MISC(NULL, NULL, "Method called");

  if (!source)
    return 0;

  for (i = 0; i < num; i++, dest++, source++) 
  {
    *dest = *source;
  }
  return i;
}


/*Function to allocate memory for SSLCertKeyData*/
void create_cert_key_entry()
{
  NSDL3_SSL(NULL, NULL, "Method called, total_ssl_cert_key_entries = %d, max_ssl_cert_key_entries = %d",
                         total_ssl_cert_key_entries, max_ssl_cert_key_entries);

  if(total_ssl_cert_key_entries == max_ssl_cert_key_entries)
  {
    max_ssl_cert_key_entries += DELTA_CERT_KEY_ENTRIES;
    MY_REALLOC(ssl_cert_key_data, sizeof(SSLCertKeyData) * max_ssl_cert_key_entries, "SSLCertKeyData", -1);
    NSDL3_SSL(NULL, NULL, "Memory allocation process done for SSLCertKeyData, max_ssl_cert_key_entries = %d", max_ssl_cert_key_entries);
  }
  total_ssl_cert_key_entries++;
  NSDL3_SSL(NULL, NULL, "Method Done");
}

int create_ssl_cert_key_shr()
{
  int index;

  NSDL3_SSL(NULL, NULL, "Method called");

  //making shared memory of main memory
  ssl_cert_key_data_shr = (SSLCertKeyData_Shr*) do_shmget(sizeof(SSLCertKeyData) * total_ssl_cert_key_entries, "SSL Cert key Data Shared Table");
  for (index = 0; index < total_ssl_cert_key_entries; index++)
  {
    ssl_cert_key_data_shr[index].ssl_cert_key_addr = ssl_cert_key_data[index].ssl_cert_key_addr;
    ssl_cert_key_data_shr[index].ssl_cert_key_file_size = ssl_cert_key_data[index].ssl_cert_key_file_size;
  }

  NSDL3_SSL(NULL, NULL, "Method Done");
  return 0;
}


//called before writing SSL request to prepare message
/****************************************************************
Note:
Following 3 fields of cptr used for managing SSL data write
Firstly, all vector is conc\vrted to flat buffer and is held in
cptr->free_array.

This field is used entirely for different purpose for HTTP case.
It is an overload use.

cptr->tcp_bytes_sent and cptr->bytes_left_to_send is used in
usual sense as used for HTTP.
****************************************************************/
void copy_request_into_buffer(connection *cptr, int http_size, NSIOVector *ns_iovec)
{
  struct iovec* v_ptr;
  int num_left;
  int amt_writ, i;
  int buf_offset;
  char *send_buf;

  /* This is not needed later if we have SSL_writev */
  num_left = http_size;
  buf_offset = 0;

  NSDL2_SSL(NULL, cptr, "Method called, http_size = %d", http_size);
  MY_MALLOC (send_buf , num_left+1, "send_buf ", -1);
  cptr->tcp_bytes_sent = 0;
  cptr->free_array = send_buf;
  NSDL2_SSL(NULL, cptr,"cptr->free_array=%p", cptr->free_array);
  cptr->bytes_left_to_send = num_left;
  for (i = 0, v_ptr = ns_iovec->vector; i < ns_iovec->cur_idx; i++, v_ptr++)
  {
    //This block should not be needed, as size is calculated apriori
    if (num_left < v_ptr->iov_len)
    {
      fprintf(stderr, "Handle_Connent(): SSL Request is too big\n");
      NS_FREE_RESET_IOVEC(*ns_iovec);
      end_test_run();   /* Mind as well end the test run because this will always fail */
    }
    amt_writ = ns_memncpy(send_buf+buf_offset, v_ptr->iov_base, v_ptr->iov_len);
    num_left -= amt_writ;
    buf_offset += amt_writ;
  }
  send_buf[buf_offset] = '\0';
  NS_FREE_RESET_IOVEC(*ns_iovec);

  // DL_ISSUE
#ifdef NS_DEBUG_ON
  {
    VUser *vptr = cptr->vptr;
    struct sockaddr_in sin; socklen_t len = sizeof (struct sockaddr_in);
    getsockname(cptr->conn_fd, (struct sockaddr *)&sin, &len);
    //printf("[cptr=0x%x]: Sending SSL request:\n%s\n", (unsigned int)cptr, send_buf);
    NSDL1_SSL(NULL, cptr, "[cptr=%p]: Sending SSL request "
                          "(nvm=%d sees_inst=%u user_index=%u src_ip=0x%x port=%hd):\n%s\n",
                          cptr, my_child_index, vptr->sess_inst, 
                          vptr->user_index, htonl(sin.sin_addr.s_addr),
                          htons(sin.sin_port), send_buf);
  }
#endif
}

#if 0   //Currently not using this method
/* Check certificate chain and check that the common name matches the host name*/
inline void check_cert_chain(SSL *ssl, char *host)
{
  X509 *peer;
  char peer_CN[256];
  NSDL2_SSL(vptr, cptr, "Method called");

   /*Check the common name*/
  if ((peer = SSL_get_peer_certificate(ssl)) == NULL)
    err_exit("No certificate was presented by the peer or no connection was established");

  if(SSL_get_verify_result(ssl) != X509_V_OK)
    berr_exit("Certificate doesn't verify");
  
  X509_NAME_get_text_by_NID(X509_get_subject_name (peer), NID_commonName, peer_CN, 256);
  if(strcasecmp(peer_CN, host))
    err_exit("Common name doesn't match host name");
}
#endif


/***********************************************************************************************
 |  • NAME:     
 |      load_cert_key_file() - to load provided certificate and key file, at parsing time
 |
 |  • SYNOPSIS: 
 |      int load_cert_key_file(char *file, int file_len, int flag)
 |
 |      Arguments:
 |        file         - certificate name stored at $NS_WDIR/cert or relative file path from cert dir
 |                       Example : custom/cert.pem custom/key.pem
 |        file_len     - Length of file 
 |        flag         - flag will have value 0 or 1, 0 - CERT and 1 - KEY
 |
 |  • DESCRIPTION:      
 |      @ This function will load cert and key file into X509 and EVP_PKEY memory 
 |        and return pointer which will be further used for using certificate
 |        
 |
 |  • RETURN VALUE:
 |      norm_id
 ************************************************************************************************/


/**************** Functions to load Cert in X509 and Key in EVP_PKEY******************/
int load_cert_key_file(char *file, int file_len, int flag)
{
  char file_path[MAX_SSL_FILE_LENGTH];

  int is_new_file = 0;
  int norm_id = -1;
  struct stat stat_buf;
  FILE *cert_key_fp = NULL;
  void *ssl_extra_cert_addr = NULL;
  SSLExtraCert *ssl_extra_cert = NULL;
  SSLExtraCert *tail_node = NULL;

  NSDL2_SSL(NULL, NULL, "Method Called");    

  snprintf(file_path, MAX_SSL_FILE_LENGTH, "%s/cert/%s", g_ns_wdir, file);

  if(stat(file_path, &stat_buf) == -1)
  {
    if(flag == CERT_FILE)
      NSDL2_SSL(NULL, NULL, "Certficate file is not provided with correct relative path %s", file_path);    
    else
      NSDL2_SSL(NULL, NULL, "Key file is not provided with correct relative path %s", file_path);    

    return -1;
  }

  NSDL2_SSL(NULL, NULL, "cert_key_norm_tbl - %p, file_len - %d", cert_key_norm_tbl, file_len);

  //calculate norm_id for existing file path only
  norm_id = nslb_get_or_set_norm_id(&cert_key_norm_tbl, file, file_len, &is_new_file);

  NSDL2_SSL(NULL, NULL, "File is [%s] for norm_id = %d, is_new_file = %d with file_path [%s]", file, norm_id, is_new_file, file_path);

  if(is_new_file)
  {
    create_cert_key_entry();

    //no need to do check for file, as stat is checking the status
    cert_key_fp = fopen(file_path, "r");

    if(!cert_key_fp)
    {
      NSTL1(NULL, NULL, "unable to open cert file = %s", file_path);
      return -1;
    }

    ssl_cert_key_data[norm_id].ssl_cert_key_file_size = stat_buf.st_size;

    //certificate and key are loaded at 'ssl_cert_key_addr'
    if (flag == CERT_FILE)
    {
      ssl_cert_key_data[norm_id].ssl_cert_key_addr = PEM_read_X509(cert_key_fp, NULL, NULL, NULL); 
      ssl_cert_key_data[norm_id].extra_cert = NULL; 
      while((ssl_extra_cert_addr = PEM_read_X509(cert_key_fp, NULL, NULL, NULL)) != NULL)
      {
         NSDL2_SSL(NULL, NULL, "Chain certificate found");
         MY_MALLOC_AND_MEMSET(ssl_extra_cert, sizeof(struct SSLExtraCert), "Cert Extra Malloc", (int)-1);
         ssl_extra_cert->ssl_cert_key_addr = ssl_extra_cert_addr;
         ssl_extra_cert->next = NULL;
         if(ssl_cert_key_data[norm_id].extra_cert == NULL)
         {
             ssl_cert_key_data[norm_id].extra_cert = ssl_extra_cert; 
         }
         else
         { 
           tail_node->next = ssl_extra_cert;
         } 
         tail_node = ssl_extra_cert;
         NSDL2_SSL(NULL, NULL, "Local extra cert pointer is %p, ssl_cert_key_data cert pointer is %p", 
                                        ssl_extra_cert, ssl_cert_key_data[norm_id].extra_cert);
      }
    }
    else
    {
      ssl_cert_key_data[norm_id].ssl_cert_key_addr = PEM_read_PrivateKey(cert_key_fp, NULL, NULL, NULL);  
    }
    fclose(cert_key_fp);
  }  
  return norm_id; 
} 

/***********************************************************************************************
 |  • NAME:     
 |      load_all_cert_key_files() - to load all certificate and key file, at time of 'C' variable and parameterisation
 |
 |  • SYNOPSIS: 
 |      void load_all_cert_key_files(char *path, char *prefix)  
 |
 |      Arguments:
 |        path         - provide full directory path  
 |                       Example : /home/cavisson/Controller_Shikha/cert
 |        prefix       - this will be relative diretory path to cert key file
 |                       Example : custom
 |
 |  • DESCRIPTION:      
 |      @ This function will load cert and key file into X509 and EVP_PKEY memory 
 |        and return pointer which will be further used for using certificate
 |        
 |
 |  • RETURN VALUE:
 |      nothing
 ************************************************************************************************/
void load_all_cert_key_files(char *path, char *prefix)
{
  NSDL2_SSL(NULL, NULL, "Method Called Path = %s", path);

  char file_name[MAX_SSL_FILE_LENGTH];
  DIR *drctry = opendir(path);                           
  int file_len = 0;
  NSDL2_SSL(NULL, NULL, "drctry = %p, err = %s", drctry, nslb_strerror(errno));
  char buf[MAX_SSL_FILE_LENGTH];

  size_t path_len = strlen(path);
  void *ssl_extra_cert_addr = NULL;
  SSLExtraCert *ssl_extra_cert = NULL;
  SSLExtraCert *tail_node = NULL;

  if(drctry)                                             //If directory stream pointer found
  { 
    struct dirent *p;
      
    while ((p=readdir(drctry)))                  //readdir, checks for next dir in path
    { 
      size_t len , dir_len;
      
        
      /* Skip the names "." and ".." as we don't want to recurse on them. */
      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
      {
        NSDL2_SSL(NULL, NULL,"Found Directory with . n ..");
        continue;
      }

      dir_len = strlen(p->d_name);
      len = path_len + dir_len + 2;

      struct stat statbuf;

      snprintf(buf, len, "%s/%s", path, p->d_name);    //making path for newly found directory on path

      if (!stat(buf, &statbuf))
      {   
        if (S_ISDIR(statbuf.st_mode))                  //if directory, recursion starts
        {
          NSDL2_SSL(NULL, NULL, "At isdir buf %s", buf);
          load_all_cert_key_files(buf, p->d_name);
        }
        else
        {
          int norm_id = 0, is_new_file = 0;
          FILE *cert_key_fp;
          if(prefix)
            snprintf(file_name, MAX_SSL_FILE_LENGTH, "%s/%s", prefix, p->d_name);
          else
            strncpy(file_name, p->d_name, MAX_SSL_FILE_LENGTH);
            
          file_len = strlen(file_name);

          norm_id = nslb_get_or_set_norm_id(&cert_key_norm_tbl, file_name, file_len, &is_new_file);
 
          NSDL2_SSL(NULL, NULL, "File is [%s] for norm_id = %d, is_new_file = %d, buf - %s,file_len = %d", 
                                   file_name, norm_id, is_new_file, buf, file_len);
      
          if(is_new_file)
          {
            create_cert_key_entry();
      
            //no need to do check for file, as stat is checking the status
            cert_key_fp = fopen(buf, "r");

            if(!cert_key_fp)
            {
              NSTL1(NULL, NULL, "unable to open cert file = %s", buf);
              return;
            }

            ssl_cert_key_data[norm_id].ssl_cert_key_file_size = statbuf.st_size;
      
            //If certificate read fails, read for Key
            if((ssl_cert_key_data[norm_id].ssl_cert_key_addr = PEM_read_X509(cert_key_fp, NULL, NULL, NULL)) == NULL)
            {
              //Need new file pointer for reading Key, as previous was exhausted by Cert
              FILE *fp = fopen(buf, "r");;
              NSDL2_SSL(NULL, NULL, "PEM_read_X509 failed, for norm id -[%d] reading for Key ", norm_id);
              ssl_cert_key_data[norm_id].ssl_cert_key_addr = PEM_read_PrivateKey(fp, NULL, NULL, NULL);  
              NSDL2_SSL(NULL, NULL, "Key : ssl_cert_key_addr = %p", ssl_cert_key_data[norm_id].ssl_cert_key_addr);
              fclose(fp);
            }
            else
            {
              NSDL2_SSL(NULL, NULL, "PEM_read_X509 succeded, for norm id -[%d] reading for cert ", norm_id);
              //ssl_cert_key_data[norm_id].ssl_cert_key_addr = PEM_read_X509(cert_key_fp, NULL, NULL, NULL);
              NSDL2_SSL(NULL, NULL, "Cert : ssl_cert_key_addr = %p", ssl_cert_key_data[norm_id].ssl_cert_key_addr);
              ssl_cert_key_data[norm_id].extra_cert = NULL;
              while((ssl_extra_cert_addr = (PEM_read_X509(cert_key_fp, NULL, NULL, NULL))) != NULL)
              {
                 NSDL2_SSL(NULL, NULL, "Chain certificate found");
                 MY_MALLOC_AND_MEMSET(ssl_extra_cert, sizeof(struct SSLExtraCert), "Cert Extra Malloc", (int)-1);
                 ssl_extra_cert->ssl_cert_key_addr = ssl_extra_cert_addr;
                 ssl_extra_cert->next = NULL;
                 if(ssl_cert_key_data[norm_id].extra_cert == NULL)
                 {
                     ssl_cert_key_data[norm_id].extra_cert = ssl_extra_cert; 
                 }
                 else
                 { 
                   tail_node->next = ssl_extra_cert;
                 } 
                 tail_node = ssl_extra_cert;
                 NSDL2_SSL(NULL, NULL, "Local extra cert pointer is %p, ssl_cert_key_data cert pointer is %p", 
                                        ssl_extra_cert, ssl_cert_key_data[norm_id].extra_cert);
              }

            }
      
            fclose(cert_key_fp);
          } /*End of if*/   
        } /*End of else*/
      } /*End of if*/
      else   //When File not found
        NSDL2_SSL(NULL, NULL, "Certificate or Key file does not exist!!!");
    }  /*End of while*/
    closedir(drctry);
  }
  return;
}

//Following methods called from read_keywords() method when conf file read in util.c
 
void set_ssl_default()
{
  NSDL2_SSL(NULL, NULL, "Method called");
  
  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
    strcpy (group_default_settings->ssl_ciphers, "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_8_SHA256:TLS_AES_128_CCM_SHA256");
  #else
    strcpy (group_default_settings->ssl_ciphers, "RC4-MD5:AES256-SHA:DES-CBC3-SHA:DES-CBC3-MD5:AES128-SHA");
  #endif

  NSDL2_SSL(NULL, NULL, "group_default_settings->ssl_ciphers = %s", group_default_settings->ssl_ciphers);
//  global_settings->ssl_cert_file[0] = 0;  // 0;   Need to ask Neeraj Sir
//  global_settings->ssl_key_file[0] = 0;   // 0;   Need to ask Neeraj Sir
  group_default_settings->avg_ssl_reuse = 100;
  nslb_init_norm_id_table_ex(&cert_key_norm_tbl, SSL_CERT_KEY_TABLE_SIZE);
}

void ssl_data_check()
{
  if ((group_default_settings->avg_ssl_reuse < 0) || (group_default_settings->avg_ssl_reuse > 100))
  {
    NSDL2_SSL(NULL, NULL, "Method called");
    group_default_settings->avg_ssl_reuse = 100;
    NSDL2_SSL(NULL, NULL, "AVG_SSL_REUSE has to be between 0-100, setting it to 100");
  }
}

/*
AVG_SSL_REUSE 
*/
int kw_set_avg_ssl_reuse(char *buf, int *to_change, char *err_msg, int runtime_flag)
{ 
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char text[100];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num, val;
  text[0]=100;
 
  num = sscanf(buf, "%s %s %s %s ", keyword, sg_name, text, tmp);

  if(num != 3) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_AVG_SSL_REUSE_USAGE, CAV_ERR_1011050, CAV_ERR_MSG_1);
  }
  if(ns_is_numeric(text) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_AVG_SSL_REUSE_USAGE, CAV_ERR_1011050, CAV_ERR_MSG_2);
  }   
  val = atoi(text);
  *to_change = val;
  return 0;
}

/*
SSL_CIPHERS
*/
int kw_set_ssl_cipher_list(char *buf, char *to_change, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char cipher_list[CIPHER_BUF_MAX_SIZE + 1];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num, cipher_length;

  NSDL2_SSL(NULL, NULL, "Method Called");    
  num = sscanf(buf, "%s %s %s %s ", keyword, sg_name, cipher_list, tmp);
    
  if(num != 3) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_CIPHER_LIST_USAGE, CAV_ERR_1011049, CAV_ERR_MSG_1);
  }
  cipher_length = strlen(cipher_list);
  if (cipher_length < CIPHER_BUF_MAX_SIZE) 
    cipher_length = CIPHER_BUF_MAX_SIZE; 
  
  strncpy(to_change, cipher_list, cipher_length);
  to_change[cipher_length] = '\0';

  return 0;
}

/*
SSL_CERT_FILE_PATH
*/
/*
void kw_set_ssl_cert_file(char *keyword, char *text)
{
  if (strcasecmp(keyword, "SSL_CERT_FILE_PATH") == 0) 
  {
    NSDL2_SSL(NULL, NULL, "Method called");
    if (strlen(text) > 255)
      NSDL2_SSL(NULL, NULL, "Max SSL CLIENT CERT FILE PATH  can be 255 bytes. Ignoring");
    else
      strcpy(global_settings->ssl_cert_file, text);
    NSDL3_SSL(NULL, NULL, "global_settings->ssl_cert_file = %s", global_settings->ssl_cert_file);
  }
}
*/

/***********************************************************************************************
 |  • NAME:     
 |      kw_g_set_ssl_cert_file() - G_SSL_CERT_FILE_PATH Keyword Parsing
 |
 |  • SYNOPSIS: 
 |     int kw_g_set_ssl_cert_file(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes) 
 |
 |      Arguments:
 |        buf         - Line of scenario file containing keyword
 |                       Example : G_SSL_CERT_FILE_PATH ALL client_cert.pem
 |        gset        - pointer to GroupSettings structure 
 |        err_msg     - store the error message for keyword
 |        runtime_changes - if set, keyword can be runtime changed else not runtime changable
 |
 |  • DESCRIPTION:      
 |      @ This function will parse the keyword and store variable value in stuctures 
 |        
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int kw_g_set_ssl_cert_file(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH];
  char cert_file[MAX_SSL_FILE_LENGTH];        //can have name or relative path
  int num_args = 0, cert_file_len = 0, norm_id = 0;
  
  NSDL2_SSL(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, cert_file);

  NSDL2_SSL(NULL, NULL, "num_args = %d, cert_file = %s", num_args, cert_file);

  if(num_args != 3 )
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_CERT_FILE_PATH_USAGE, CAV_ERR_1011045, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  gset->ssl_cert_id = -1; 

  if(!strcmp(cert_file, "-"))
    return 0;
 
  cert_file_len = strlen(cert_file); 

  if (cert_file_len > 255)
    NSDL2_SSL(NULL, NULL, "Max SSL CLIENT KEY FILE PATH can be 255 bytes. Ignoring");
  else
  {
    NSDL2_SSL(NULL, NULL, "Certificate file length is [%d]", cert_file_len);
    norm_id = load_cert_key_file(cert_file, cert_file_len, CERT_FILE);
    if (norm_id < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_CERT_FILE_PATH_USAGE, CAV_ERR_1011046, g_ns_wdir, "");
    }
    gset->ssl_cert_id = norm_id; 
  }
  NSDL2_SSL(NULL, NULL, "After scenario Parse GroupSettings cert_id is [%d]",  gset->ssl_cert_id);
  
  return 0;
}

/***********************************************************************************************
 |  • NAME:     
 |      kw_set_ssl_key_file() - G_SSL_KEY_FILE_PATH Keyword Parsing
 |
 |  • SYNOPSIS: 
 |     int kw_g_set_ssl_cert_file(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes) 
 |
 |      Arguments:
 |        buf         - Line of scenario file containing keyword
 |                       Example : G_SSL_KEY_FILE_PATH ALL client_key.pem
 |        gset        - pointer to GroupSettings structure 
 |        err_msg     - store the error message for keyword
 |        runtime_changes - if set, keyword can be runtime changed else not runtime changable
 |
 |  • DESCRIPTION:      
 |      @ This function will parse the keyword and store variable value in stuctures 
 |        
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int kw_set_ssl_key_file(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH];
  char key_file[MAX_DATA_LINE_LENGTH];  //can have name or relative path
  int num_args = 0, key_file_len = 0, norm_id = 0;
  
  NSDL2_SSL(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s", keyword, group_name, key_file);
  NSDL2_SSL(NULL, NULL, "num_args = %d, cert_file = %s", num_args, key_file);

  if(num_args != 3 )
  {
    //NSDL3_SSL(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args, usages);
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_KEY_FILE_PATH_USAGE, CAV_ERR_1011047, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  gset->ssl_key_id = -1; 

  if(!strcmp(key_file, "-"))
    return 0;

  key_file_len = strlen(key_file); 

  if (key_file_len > 255)
    NSDL2_SSL(NULL, NULL, "Max SSL CLIENT KEY FILE PATH  can be 255 bytes. Ignoring");
  else
  {
    NSDL2_SSL(NULL, NULL, "Key file length is [%d]", key_file_len);
    
    norm_id = load_cert_key_file(key_file, key_file_len, KEY_FILE);
    if (norm_id < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_KEY_FILE_PATH_USAGE, CAV_ERR_1011048, g_ns_wdir, "");
    }
    gset->ssl_key_id = norm_id;
  }
  NSDL2_SSL(NULL, NULL, "After scenario Parse GroupSettings key_id is [%d]",  gset->ssl_key_id);
  
  return 0;
}

/*
SSL_CLEAN_CLOSE_ONLY
*/
int kw_set_ssl_clean_close_only(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char text[100];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields
  int num;
  short val;
  text[0]=0;
  num = sscanf(buf, "%s %s %s %s ", keyword, sg_name, text, tmp);

  if(num != 3) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SSL_CLEAN_CLOSE_ONLY_USAGE, CAV_ERR_1011051, CAV_ERR_MSG_1);
  }
  if(ns_is_numeric(text) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SSL_CLEAN_CLOSE_ONLY_USAGE, CAV_ERR_1011051, CAV_ERR_MSG_2);
  }
  val = atoi(text);
  *to_change = val;
  return 0;
}

/*
SSL_ATTACK_FILE
*/
void kw_set_ssl_attack_file(char *keyword, char *buf)
{
#ifdef CAV_SSL_ATTACK
  char file_name[1024];
  NSDL2_SSL(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
  sscanf(buf, "%s %s", keyword, file_name);
  NSDL3_SSL(NULL, NULL, "SSL ATTACK File name is '%s'", file_name);
  kw_set_ssl_attack(file_name);
#else
  NS_EXIT(-1, "Error: Keyword '%s' not supported.", keyword);
#endif

}

/***********************************************************************************************
 |  • NAME:     
 |      kw_set_tls_version() - G_TLS_VERSION Keyword Parsing
 |
 |  • SYNOPSIS: 
 |     int kw_set_tls_version(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes) 
 |
 |      Arguments:
 |        buf         - Line of scenario file containing keyword
 |                       Example : G_TLS_VERSION ALL tls_version
 |        gset        - pointer to GroupSettings structure 
 |        err_msg     - store the error message for keyword
 |        runtime_changes - if set, keyword can be runtime changed else not runtime changable
 |
 |  • DESCRIPTION:      
 |      @ This function will parse the keyword and store variable value in stuctures 
 |        
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int kw_set_tls_version(char *buf, GroupSettings *gset, char *msg, int runtime_changes)
{
  NSDL3_SSL(NULL, NULL, "Method called");

  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH] = "ALL";
  char version[MAX_DATA_LINE_LENGTH];
  int num; 
    
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %s %s", keyword, group_name, version); 
  
  if (num != 3){
    NS_KW_PARSING_ERR(buf, runtime_changes, msg, G_TLS_VERSION_USAGE, CAV_ERR_1011054, CAV_ERR_MSG_1);
  } 
  
 /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
  
  if (strcasecmp(version, "ssl3") == 0){
    gset->tls_version = SSL3_0;
  }else if (strcasecmp(version, "tls1") == 0){
    gset->tls_version = TLS1_0;
  }else if (strcasecmp(version, "tls1_1") == 0){
    gset->tls_version = TLS1_1;
  }else if (strcasecmp(version, "tls1_2") == 0){
    gset->tls_version = TLS1_2;
  }else if (strcasecmp(version, "tls1_3") == 0){
    gset->tls_version = TLS1_3;
  }else{
    gset->tls_version = SSL2_3;
  }
  NSDL3_SSL(NULL, NULL, "gset->tls_version = %d", gset->tls_version);

  return 0;
}

/* G_ENABLE_POST_HANDSHAKE_AUTH */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
static void kw_enable_post_handshake_auth_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of G_ENABLE_POST_HANDSHAKE_AUTH: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: unable to enable G_ENABLE_POST_HANDSHAKE_AUTH for  <mode>\n");
  NSTL1_OUT(NULL, NULL, "  Where:\n");
  NSTL1_OUT(NULL, NULL, "         group : Any valid group\n");
  NSTL1_OUT(NULL, NULL, "         mode : Is used to specify whether user want to start G_ENABLE_POST_HANDSHAKE_AUTH  or do not want G_ENABLE_POST_HANDSHAKE_AUTH:\n");
  NS_EXIT(-1, "Error: Invalid value of G_ENABLE_POST_HANDSHAKE_AUTH: %s", err_msg);
}
#endif

int kw_enable_post_handshake_auth(char *buf, GroupSettings *gset, char *msg)
{
  #if OPENSSL_VERSION_NUMBER >= 0x10100000L
  NSDL3_SSL(NULL, NULL, "Method called");

  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH] = "ALL";
  int mode, num; 
    
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %s %d", keyword, group_name, &mode); 
  
  if (num != 3){
    kw_enable_post_handshake_auth_usage("Invaid number of arguments", buf);
  } 
  
 /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
  
  if (mode < 0 || mode > 1){
    kw_enable_post_handshake_auth_usage("Mode can have value 0 or 1", buf);
  }

  gset->post_hndshk_auth_mode = mode;
  
  NSDL3_SSL(NULL, NULL, "gset->post_hndshk_auth_mode = %d", gset->post_hndshk_auth_mode);
  #else
  NSTL1(NULL, NULL, "Error: G_ENABLE_POST_HANDSHAKE_AUTH not supported for TLSv1.2 or below");
  #endif

  return 0;
}

/* This keyword sets ssl_method for specified Recorded host.  
   There was a Requirement from kohls  to send specific ssl methord for particular host. 
   Syntax : HOST_TLS_VERSION <hostname:port> <tls_version>
*/
int kw_set_host_tls_version(char *keyword, char *buf, char *err_msg, int runtime_flag)
{
  char tls_version[8] = "";
  char hostname[MAX_LINE_LENGTH + 1] ="";
  int hostname_len; 
  unsigned short server_port = 0;
  int idx = 0;
  int num = 0; 
  NSDL3_SSL(NULL, NULL, "Method called");
  
  num = sscanf(buf, "%s %s %s", keyword, hostname, tls_version);
  if (num < 3) {
    NSDL1_SSL(NULL, NULL, "Invalid arguments for keyword %s. ", keyword); 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, HOST_TLS_VERSION_USAGE, CAV_ERR_1011265, CAV_ERR_MSG_1);
  }

  NSDL1_SSL(NULL, NULL, "keyword  is %s and hostname is  %s Tls version is %s", keyword, hostname, tls_version); 
  // Going to find hostname length . We require hostname length while finding server idx . 
  hostname_len = find_host_name_length_without_port(hostname, &server_port);
  NSDL1_SSL(NULL, NULL, "server_port is %hd and hostname len = %d", server_port, hostname_len); 
  //hostname[hostname_len] = '\0';
  find_gserver_idx(hostname, server_port, &idx, hostname_len);
  if (idx == -1)
  {
    NSDL1_SSL(NULL, NULL, "Recorded Host (%s) not found for keyword %s. Returning.", hostname, keyword);
    NS_DUMP_WARNING("Recorded Host (%s) not found for keyword %s.", hostname, keyword);
    return 0;
  }

  if (strcasecmp(tls_version, "ssl3") == 0){
    gServerTable[idx].tls_version = SSL3_0;
  }else if (strcasecmp(tls_version, "tls1") == 0){
    gServerTable[idx].tls_version = TLS1_0;
  }else if (strcasecmp(tls_version, "tls1_1") == 0){
    gServerTable[idx].tls_version = TLS1_1;
  }else if (strcasecmp(tls_version, "tls1_2") == 0){
    gServerTable[idx].tls_version = TLS1_2;
  }else if (strcasecmp(tls_version, "tls1_3") == 0){
    gServerTable[idx].tls_version = TLS1_3;
  }else{
    gServerTable[idx].tls_version = SSL2_3;
  }
  NSDL3_SSL(NULL, NULL, "gServerTable[idx].tls_version = %d", gServerTable[idx].tls_version);

  return 0;
}

/***********************************************************************************************
 |  • NAME:     
 |      start_ssl_renegotiation() - G_SSL_RENEGOTIATION Keyword Parsing
 |
 |  • SYNOPSIS: 
 |     int start_ssl_renegotiation(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes) 
 |
 |      Arguments:
 |        buf         - Line of scenario file containing keyword
 |                       Example : G_SSL_RENEGOTIATION ALL tls_version
 |        gset        - pointer to GroupSettings structure 
 |        err_msg     - store the error message for keyword
 |        runtime_changes - if set, keyword can be runtime changed else not runtime changable
 |
 |  • DESCRIPTION:      
 |      @ This function will parse the keyword and store variable value in stuctures 
 |        
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int start_ssl_renegotiation(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH] = "ALL";
  int mode;
  int num; 
 
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %d", keyword, group_name, &mode);
 
  if (num != 3){
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_RENEGOTIATION_USAGE, CAV_ERR_1011053, CAV_ERR_MSG_1);
  }
  
  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);
  
  if (mode < 0 || mode > 1){
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SSL_RENEGOTIATION_USAGE, CAV_ERR_1011053, CAV_ERR_MSG_3);
  }
  gset->ssl_regenotiation = mode;

  NSDL3_SSL(NULL, NULL, "gset->ssl_regenotiation = %d", gset->ssl_regenotiation);

  return 0;
}

int kw_set_ssl_settings(char *buf, int *to_change, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int num, sni_mode;
  char text[100];
  char tmp[MAX_DATA_LINE_LENGTH];//This is used to check extra fields

  num = sscanf(buf, "%s %s %s %s ", keyword, sg_name, text, tmp);

  if(num != 3) { //Check for extra arguments.
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SSL_SETTINGS_USAGE, CAV_ERR_1011052, CAV_ERR_MSG_1);
  }
  sni_mode = atoi(text);
  if(sni_mode < 0 || sni_mode >1 ) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SSL_SETTINGS_USAGE, CAV_ERR_1011052, CAV_ERR_MSG_3);
  }
  if (sni_mode == 1)
    *to_change |= SSL_SNI_ENABLED;
  
  return 0;
}

/***********************************************************************************************
 |  • NAME:     
 |      ns_parse_set_ssl_setting() - This function will parse APIs of SSL
 |
 |  • SYNOPSIS: 
 |    int ns_parse_set_ssl_setting(char *buf_ptr) 
 |
 |     Arguments:
 |        buf_ptr         - pointer to the API arguments 
 |
 |  • DESCRIPTION:      
 |      @ This function will parse the API, checks
 |                 1) if NS API(), Load particular certificate
 |                 2) if C API(), Load ALL certificate
 |                 3) if Parameterized, Load all certificate
 |
 |  • Callled From : parse_flow_file() of ns_script_parse.c 
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int ns_parse_set_ssl_setting(char *buf_ptr)
{
  static int load_all_cert_key = 1;
  int cert_file_len = 0, key_file_len = 0;
  char cert_file[MAX_SSL_FILE_LENGTH], key_file[MAX_SSL_FILE_LENGTH];
  char path[MAX_SSL_FILE_LENGTH];
  char *cert, *key, *ptr, *ptr2;
  char api[1024];
  int k = 0;

  NSDL2_SSL(NULL, NULL, "Method Called, buf_ptr = %s", buf_ptr);
  //Need to pass path for parameterized value
  snprintf(path, MAX_SSL_FILE_LENGTH, "%s/cert", g_ns_wdir);

  /************** Clearing White Space ***********/
  while(*buf_ptr)
  {
    if(*buf_ptr != ' ' &&  *buf_ptr != '\t' && *buf_ptr != '\n')
    api[k++] = *buf_ptr; 
    buf_ptr++;
  }
  api[k] = 0;
  buf_ptr = api;

  NSDL2_SSL(NULL, NULL, "After clearing space, buf_ptr = %s", buf_ptr);

  //or ("cert","key") or ("{cert}","{key}")
  ptr = strchr(buf_ptr, '(');
  if(!ptr)
    return -1;
  ptr++;
  ptr2 = strchr(ptr, ',');
  if(!ptr2)
    return -1;

  cert_file_len = ptr2 - ptr;
  strncpy(cert_file, ptr, cert_file_len);
  cert_file[cert_file_len] = '\0';
  NSDL2_SSL(NULL, NULL, "certificate file is = %s of len = %d", cert_file, cert_file_len);

  ptr = ptr2+1;
  if(!ptr)
    return -1;
  ptr2 = strchr(ptr, ')');
  if(!ptr2)
    return -1;
  
  key_file_len = ptr2 - ptr;
  strncpy(key_file, ptr, ptr2-ptr);
  key_file[key_file_len] = '\0';
  
  NSDL2_SSL(NULL, NULL, "key file is = %s of len = %d", key_file, key_file_len);

  
  if(!cert_file[0] || !key_file[0])
    return -1;

  /*For Cert*/
  if(cert_file[0] == '"' && cert_file[cert_file_len-1] == '"')
  {
     if(cert_file[1] == '{' && cert_file[cert_file_len-2] == '}')
     {
       //Parametrized values
       if(load_all_cert_key)
       {
         load_all_cert_key_files(path, NULL);
         load_all_cert_key = 0;
       }
       //return 0;
     }
     else
     {     
       cert = cert_file;
       cert++;
       cert_file[cert_file_len-1] = 0;
       //To remove quotes from both end
       cert_file_len -= 2;
       NSDL2_SSL(NULL, NULL, "cert = %s", cert);
       NSDL2_SSL(NULL, NULL, "Certificate file length is [%d]", cert_file_len);
       load_cert_key_file(cert, cert_file_len, CERT_FILE);
     }
  }

  /*For Key*/
  if(key_file[0] == '"' && key_file[key_file_len-1] == '"')
  {
     if(key_file[1] == '{' && key_file[key_file_len-2] == '}')
     {
       NSDL2_SSL(NULL, NULL, "Key loaded already");
       //Parametrized values
     }
     else
     { 
       key = key_file;
       key++;
       key_file[key_file_len-1] = 0;
       //To remove quotes from both end
       key_file_len -= 2;
       NSDL2_SSL(NULL, NULL, "key = %s", key);
       NSDL2_SSL(NULL, NULL, "Key file length is [%d]", key_file_len);
       load_cert_key_file(key, key_file_len, KEY_FILE);
     }
  }

  /* C variable */
  if(load_all_cert_key)
  { 
    load_all_cert_key_files(path, NULL);
    load_all_cert_key = 0;
  }
  
  return 0;
}

/***********************************************************************************************
 |  • NAME:     
 |      ns_set_ssl_setting_ex() - This function will work at runtime
 |
 |  • SYNOPSIS: 
 |     int ns_set_ssl_setting_ex(VUser *vptr, char *cert_file, char *key_file) 
 |
 |     Arguments:
 |        vptr         - pointer to VUser structure
 |        cert_file    - User 1st input, i.e. 1st argument in script for ns_set_ssl_settings()
 |        key_file     - User 2nd input, i.e. 2nd argument in script for ns_set_ssl_settings()
 |
 |  • DESCRIPTION:      
 |      @ This function will do following
 |                 1) if NS API(), get norm_id of certificate which is already loaded at compile time and store norm_id at vptr->httpData
 |                 2) if C API(), get norm_id of certificate which is already loaded at compile time and store norm_id at vptr->httpData
 |                 3) if Parameterized, first ns_eval_string_flag_internal the string, 
 |                                      find the name of file then 
 |                                      get norm_id of certificate which is already loaded at compile time and store norm_id at vptr->httpData
 |
 |  • Callled From : ns_set_ssl_settings() of ns_string_api.c
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int ns_set_ssl_setting_ex(VUser *vptr, char *cert_file, char *key_file)
{
  int cert_norm_id = 0, key_norm_id = 0;
  long cert_file_len = 0, key_file_len = 0;
  char cert_file_name[MAX_SSL_FILE_LENGTH];
  char key_file_name[MAX_SSL_FILE_LENGTH];
  
  NSDL2_SSL(vptr, NULL, "Method Called, cert_file = %s and key_file = %s", cert_file, key_file);

  //NSDL2_SSL(vptr, NULL, "After clearing white space, cert_file = %s and key_file = %s", cert_file, key_file);

  if(!cert_file || !key_file || !cert_file[0] || !key_file[0] )
  {
    fprintf(stderr, "Error: certificate or key file not provided\n");
    return -1;
  }

  cert_file_len = strlen(cert_file);
  if(cert_file[0] == '{' && cert_file[cert_file_len -1] == '}')
  {
     NSDL2_SSL(vptr, NULL, "Parameterized value found for cert");
     strncpy(cert_file_name, ns_eval_string_flag_internal(cert_file, 0, &cert_file_len, vptr), MAX_SSL_FILE_LENGTH);
     cert_file = cert_file_name;
  } 

  cert_norm_id = nslb_get_norm_id(&cert_key_norm_tbl, cert_file, cert_file_len);

  key_file_len = strlen(key_file);
  if(key_file[0] == '{' && key_file[key_file_len -1] == '}')
  {
     NSDL2_SSL(vptr, NULL, "Parameterized value found for key");
     strncpy(key_file_name, ns_eval_string_flag_internal(key_file, 0, &key_file_len, vptr), MAX_SSL_FILE_LENGTH);
     key_file = key_file_name;
  }


  key_norm_id = nslb_get_norm_id(&cert_key_norm_tbl, key_file, key_file_len); 

  NSDL2_SSL(vptr, NULL, "cert_key_norm_tbl - %p, cert_file_len - %d, key_file_len - %d", cert_key_norm_tbl, cert_file_len, key_file_len);

/*
  cert_norm_id = nslb_get_norm_id(&cert_key_norm_tbl, cert_file, cert_file_len);
  key_norm_id = nslb_get_norm_id(&cert_key_norm_tbl, key_file, key_file_len); 
*/

  //If new entry of certificate or key is found, then dont load newly added 
  if (cert_norm_id < 0 || key_norm_id < 0)
  {
    fprintf(stderr, "Error: Provided Certificate/Key file in ns_set_ssl_settings() does not exist. Hence, ignoring applied SSL certificate and key setting\n");
    return -1;
  }

  NSDL2_SSL(vptr, NULL, "cert_norm_id - %d, key_norm_id - %d", cert_norm_id, key_norm_id);

  vptr->httpData->ssl_cert_id = cert_norm_id;
  vptr->httpData->ssl_key_id = key_norm_id;

  NSDL2_SSL(vptr, NULL, "Exiting Method");

  return 0;
}


/***********************************************************************************************
 |  • NAME:     
 |      ns_unset_ssl_setting_ex() - This function will work at runtime
 |
 |  • SYNOPSIS: 
 |     int ns_unset_ssl_setting_ex(VUser *vptr) 
 |
 |     Arguments:
 |        vptr         - pointer to VUser structure
 |
 |  • DESCRIPTION:      
 |      @ This function will set GroupSettings ssl settings to vptr->httpData
 |
 |  • Callled From : ns_unset_ssl_settings() of ns_string_api.c
 |
 |  • RETURN VALUE:
 |      0 : Success
 |     -1 : Failure 
 ************************************************************************************************/
int ns_unset_ssl_setting_ex(VUser *vptr)
{
  NSDL2_SSL(vptr, NULL, "Method Called");

  vptr->httpData->ssl_cert_id = runprof_table_shr_mem[vptr->group_num].gset.ssl_cert_id; 
  vptr->httpData->ssl_key_id = runprof_table_shr_mem[vptr->group_num].gset.ssl_key_id; 
 
  NSDL2_SSL(vptr, NULL, "Exiting Method with cert_id - %d, key_id - %d, ssl_mode = %d", 
                            vptr->httpData->ssl_cert_id, vptr->httpData->ssl_key_id, vptr->ssl_mode);
  return 0;
}

void set_ssl_recon (connection *cptr)
{
  /* SSL stuff */
  X509 *cert_ptr;
  EVP_PKEY *key_ptr;
  VUser *vptr = cptr->vptr;

  NSDL2_SSL(NULL, cptr, "Method called. fd = %d, Host index is %d", cptr->conn_fd, cptr->gServerTable_idx);

  /********************* SSL Certificate key code ****************/
  NSDL2_SSL(NULL, cptr, "VPTR : SSL Setting are cert_idx = %d, and key_idx = %d", vptr->httpData->ssl_cert_id, vptr->httpData->ssl_key_id);
  NSDL2_SSL(NULL, cptr, "CPTR : SSL Setting are cert_idx = %d, and key_idx = %d", cptr->ssl_cert_id, cptr->ssl_key_id);
  if (vptr->httpData->ssl_cert_id != cptr->ssl_cert_id || vptr->httpData->ssl_key_id != cptr->ssl_key_id)
  {
    if(vptr->httpData->ssl_cert_id >=0)
    {
      cert_ptr = (X509 *)ssl_cert_key_data[vptr->httpData->ssl_cert_id].ssl_cert_key_addr;  
      if(!SSL_use_certificate(cptr->ssl, cert_ptr))
      {
        NS_EXIT(1, "Couldn't read certificate file");
      }
    }
    if(vptr->httpData->ssl_key_id >=0)
    {
      key_ptr = (EVP_PKEY *)ssl_cert_key_data[vptr->httpData->ssl_key_id].ssl_cert_key_addr;
      if(!SSL_use_PrivateKey(cptr->ssl, key_ptr))
      {
        NS_EXIT(1, "Couldn't read key file");
      }
    }  

    cptr->ssl_cert_id = vptr->httpData->ssl_cert_id;
    cptr->ssl_key_id = vptr->httpData->ssl_key_id;
  }

  NSDL3_SSL(NULL, cptr, "Going to renegotiate");
  #if OPENSSL_VERSION_NUMBER < 0x10100000L
  if(SSL_renegotiate(cptr->ssl) <= 0)
    NSTL1_OUT(NULL, cptr, "Certficate key changed unsuccessful on ssl connection");
  #endif
  NSDL3_SSL(NULL, cptr, "Certficate key changed successfully on ssl connection");
}

