/******************************************************************
 * Name    : ns_dns_req.c 
 * Purpose : This file contains methods for processing of DNS request 
 * Note:
 * Modification History:
 * 08/10/08 - Initial Version
*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <errno.h>

#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>

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

#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "ns_dns.h"
#include "nslb_dns.h"
#include "rc4.h"
#include "nslb_util.h"
#include "ns_log_req_rep.h"
#include "nslb_cav_conf.h"

/********************************************************
 header format as in RFC 1035 
                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      ID                       |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    QDCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ANCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    NSCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                    ARCOUNT                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

	Question format

                                    1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                     QNAME                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QTYPE                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     QCLASS                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

    RR format

                                   1  1  1  1  1  1
      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                                               |
    /                                               /
    /                      NAME                     /
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TYPE                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                     CLASS                     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                      TTL                      |
    |                                               |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |                   RDLENGTH                    |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
    /                     RDATA                     /
    /                                               /
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

*****************************************************/

/* encode domain name (qname) in the format which has length and label as shown below - 

   www.microsft.com --> 0x09microsoft0x03com0x00
   www.abc. --> 0x03www0x03abc (Last dot is ignored)
   .www.abc --> invalid
   www..abc --> Invalid
   www.ca\visson.com --> 0x03www0x08cavisson0x03com
   www.cavisson.com\ --> 0x03www0x08cavisson0x04com\ (Not to check this)

   return encoded length
 */

// have to initialize this elsewhere during startup
//rc4_key dns_global_id_key;

#if 0 // already in nslb_dns.c
int dns_encode_name(char* src, int src_len, char *dest, int dest_len)
{
  // get length of encoded string 
  int label_len = 0;
  char *q,*p,*buf;


  // Step 1 - Find the encoded length
  int enc_len = 0; // Encoded length
  for (p = src; *p; p++) {
    //skip backslash if not at the end so that we do not count it
    if (*p == '\\' && *(p + 1) != 0)
      p++;
    enc_len++;
  }

  /* the dots will get replaced by a 1 byte each for the label length. 
   * these have been counted already.
   * the end needs a 0 byte - if the last char is not a dot, add 1 for this.
   *  the begining needs a byte too - add 1 for this.
   */
  if (*src  && *(p-1) != '.')
    enc_len++;
  // we also need a byte at the begining
  enc_len++;

  // total size of labels and length octets cannot exceed dest_len
  if (enc_len > dest_len){
    NSDL2_DNS (NULL,NULL,"domain name length exceeds MAXCDNAME after encoding %d\n",dest);
    return(-1);	
  }

  // Step 2 - Break in lables and copy encoded name in dest
  q = dest;

  while (*src) {
    if (*src == '.'){ // cannot have . again
      printf("invalid domain name \n");	
      return(NULL); 
    }

    /* Count the number of bytes in this label. */
    label_len = 0;	
    /* labels are separated by dots,dont count backslashes in the middle of name
     */
    for (p = src; *p && *p != '.'; p++) { 
      if (*p == '\\' && *(p + 1) != 0)
        p++;
      label_len++;
    }
    if (label_len > MAXLABEL){
      NSDL2_DNS(NULL,NULL,"label exceeds MAXLABEL \n");
      return(NULL); 
    }

    /* Encode the length and copy the data. */
    // Set length of lable as first octet for that label
    *q++ = (unsigned char)label_len; 
    for (p = src; *p && *p != '.'; p++)
    {
      if (*p == '\\' && *(p + 1) != 0)
        p++;
      *q++ = *p;
    }

    /* Go to the next label and repeat, unless we hit the end. */
    if (!*p)
      break;
    src = p + 1;
  } 	//while	

  // add 0 at the end
  *q++ = 0;
  return(enc_len);
}

#endif 

/* get a unique 16 bit id for dns queries. This function uses an 
 * rc4 cipher implementation (from c-ares) in rc4.c */

static uint16_t dns_get_query_ID(connection *cptr)
{
  unsigned short r;
  /* the ID key must have been inited earlier using init_id_key() in rc4.c
   * inited in child_init() 
   */
  ares__rc4(&dns_global_id_key, (unsigned char *)&r, sizeof(r));
  NSDL4_DNS(NULL, cptr, "dns_global_id_key (addr) %p", &dns_global_id_key);
  return(r);
}

static u_char *dns_build_header(connection *cptr)
{
  action_request_Shr* request = cptr->url_num;
  unsigned char *header_buf; 
  HEADER *hdr;
  uint16_t id;

  header_buf = ((DnsData *)(cptr->data))->dns_req.header_buf;
  hdr = &((DnsData *)(cptr->data))->dns_req.header;

  memset(header_buf, 0, NS_HFIXEDSZ);
  memset(hdr, 0, NS_HFIXEDSZ);

  // Set unique ID
  id = dns_get_query_ID(cptr);
  NSDL4_DNS(NULL, cptr, "DNS unique query ID %u",id);
  DNS_HEADER_SET_QID(header_buf, id);
  request->proto.dns.recursive = 1;
  //if(request->proto.dns.recursive)	//recursion desired
  if(request->proto.dns.recursive)	//recursion desired
    DNS_HEADER_SET_RD(header_buf, 1);

  DNS_HEADER_SET_QDCOUNT(header_buf, 1);
  // set fields in the header struct for ease of checking later
  hdr->id = id;
  hdr->rd = request->proto.dns.recursive;
  hdr->qdcount = 1;

  return(header_buf);	
}

/* 
 *  This method is a generic method and can be used by any module
 *  Move to  ns_segments.c
 */

inline int get_value_from_segments(connection *cptr, char *buf, StrEnt_Shr* seg_tab_ptr, char *for_which, int max) 
{
  int i, total_len = 0;
  int ret;
  char *to_fill = buf;
  //IW_UNUSED(VUser *vptr = cptr->vptr);
  VUser *vptr = cptr->vptr;

  NSDL4_MISC(vptr, NULL, "Method Called, for_which = %s, max = %d", for_which, max);
 
  // Get all segment values in a vector
  // Note that some segment may be parameterized

  NS_RESET_IOVEC(g_scratch_io_vector); 

  if((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  { 
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
        "Error in insert_segments() for %s, return value = %d\n", for_which, NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector));
   
     if(ret == MR_USE_ONCE_ABORT)
       return ret;

     return(-1);
  }

  // Calculate total lenght of all components which are in vector
  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    total_len += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }
  
  NSDL4_MISC(vptr, NULL, "total_len = %d", total_len);

  if(total_len <= 0 || total_len > max) { 
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
        "Total length (%d) of %s is either 0 or > than max (%d) value", 
        total_len, for_which, max);
    NS_FREE_RESET_IOVEC(g_scratch_io_vector);
    return -1;
  }
 
  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    bcopy(NS_GET_IOVEC_VAL(g_scratch_io_vector, i), to_fill, NS_GET_IOVEC_LEN(g_scratch_io_vector, i));
    to_fill += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }
  *to_fill = 0; // NULL terminate

  NSDL4_MISC(vptr, NULL, "Concated value = %s", to_fill);
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);

  return total_len;
}

void debug_log_dns_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

    if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
          (request_type == DNS_REQUEST &&
           (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_DNS))))
      return;

    if((global_settings->replay_mode)) return;

    NSDL4_DNS(vptr, cptr, "Method called. bytes_size = %d", bytes_to_log);
    char log_file[4096] = "\0";
    FILE *log_fp;
    char line_break[] = "\n------------------------------------------------------------\n";

    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if((buf == NULL) || (bytes_to_log == 0)) return;  

    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    SAVE_REQ_REP_FILES
    sprintf(log_file, "%s/logs/%s/dns_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                      g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index,
                      vptr->sess_inst, vptr->page_instance, vptr->group_num, 
                      GET_SESS_ID_BY_NAME(vptr), 
                      GET_PAGE_ID_BY_NAME(vptr));

    // Do not change the debug trace message as it is parsed by GUI
    if(first_trace_write_flag)
      NS_DT4(vptr, cptr, DM_L1, MM_SMTP, "Request is in file '%s'", log_file);

    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL)
    {
      fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
      return;
    }

    //write for both ssl and non ssl url
    if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
    {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
      return;
    }
    if (complete_data) fwrite(line_break, strlen(line_break), 1, log_fp);

    if(fclose(log_fp) != 0)
    {
      fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
      return;
    }
}


/* Functions to send messages to server */

static void dns_send_req(connection *cptr, int query_size, int num_vectors, NSIOVector *ns_iovec, u_ns_ts_t now, int dns_lookup)
{
  int bytes_sent;

  NSDL2_DNS(NULL, cptr, "Method called cptr=%p conn state=%d,"
                        " proto_state = %d, query_size = %d, now = %u", 
                        cptr, cptr->conn_state, cptr->proto_state,
                        query_size, now);

  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP) {
    if ((bytes_sent = writev(cptr->conn_fd, ns_iovec->vector, NS_GET_IOVEC_CUR_IDX(*ns_iovec))) < 0) {
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,
                     "Sending DNS request failed, fd = %d,"
                     " num_vector = %d, Error=%s",
                     cptr->conn_fd, NS_GET_IOVEC_CUR_IDX(*ns_iovec),
                     nslb_strerror(errno));
      NS_FREE_RESET_IOVEC(*ns_iovec);
      retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      return;
    }

    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0) {
      NS_FREE_RESET_IOVEC(*ns_iovec);
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }

    if (bytes_sent < query_size ) {
      handle_incomplete_write( cptr, ns_iovec, NS_GET_IOVEC_CUR_IDX(*ns_iovec), query_size, bytes_sent);
      return;
    }
  } else {

    /* For UDP currently we are not handling PARTIAL data we are assuming
     * that either a complete UDP pkt will go or it will not*/
    struct msghdr dns_qmsg;
    memset(&dns_qmsg, 0, sizeof(dns_qmsg));
    if(!dns_lookup)
      dns_qmsg.msg_name = &(cptr->old_svr_entry->saddr);
    else
      dns_qmsg.msg_name = &(cptr->conn_server);

    dns_qmsg.msg_namelen = sizeof(struct sockaddr_in6);
    dns_qmsg.msg_iov = ns_iovec->vector;
    dns_qmsg.msg_iovlen = NS_GET_IOVEC_CUR_IDX(*ns_iovec);
    
    bytes_sent = sendmsg(cptr->conn_fd, &dns_qmsg, 0);

    NSDL2_DNS(NULL, cptr, "UDP bytes send = [%d] error = [%s] fd = [%d]", bytes_sent, nslb_strerror(errno), cptr->conn_fd);
    if(bytes_sent != query_size) {
      NS_FREE_RESET_IOVEC(*ns_iovec);
      if(bytes_sent < 0) {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,
                     "sendmsg failed, Error=%s",
                     nslb_strerror(errno));
        retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      } else {
        NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,
                     "Unable to send complete dns msg ,"
                     "bytes_sent = %d, query_size = %d",
                     bytes_sent, query_size);
        Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
      }
      return;
    }
  }
#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(*ns_iovec); i++)
  {
    if(i == NS_GET_IOVEC_CUR_IDX(*ns_iovec) - 1) 
      debug_log_dns_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 1);
    else 
      debug_log_dns_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 0);
  }
#endif

#if 0
  //NJ - add comments why we are doing it
  // I dont think we need this AN-TODO 
  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;
#endif

  NS_FREE_RESET_IOVEC(*ns_iovec);

  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP) {
    cptr->tcp_bytes_sent = bytes_sent;
    average_time->dns_tx_bytes += bytes_sent;
  }
  on_request_write_done (cptr);
}

inline int 
dns_make_request(connection *cptr, u_ns_ts_t now, int dns_lookup, char *server_name)
{
  action_request_Shr* request = cptr->url_num;
  u_char *header, *qtype_buf, *qclass_buf ,*p;
  uint16_t v;

  // tmp_buf is used for getting qname, qtype etc
  // Qtype etc will be always smaller than NS_MAXCDNAME
  char tmp_buf[NS_MAXCDNAME]; 
  int tmp_len = 0;

  int query_size;

  NSDL4_DNS(NULL, cptr, "Method Called, cptr = [%p]", cptr);

  /* allocate and init DNS specific data in the connection  cptr */
  dns_init_connection_data(cptr);
  DnsData *dnsData = (DnsData *)(cptr->data);

  query_size = 0;
  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP){
    /* leave 2 bytes for the length of the query in the first vector */
    NS_FILL_IOVEC(g_req_rep_io_vector, (void*)DNSDATA->dns_req.tcp_len_buf, 2);
    query_size += 2;
  }

  /* build the header */
  header = dns_build_header(cptr);
  NS_FILL_IOVEC(g_req_rep_io_vector, (void*)header, NS_HFIXEDSZ);

  query_size += NS_HFIXEDSZ;
	
  // Since qname is paramterized, we need to build it in a vector
  // We cannot use same vector which is used for writev as we need to 
  // concat all vectors to get qname and then encode qname
  // So we need to use ths method to concat it

  if(!dns_lookup){
    if((tmp_len = get_value_from_segments(cptr, tmp_buf, &request->proto.dns.name, "QNAME", NS_MAXCDNAME - 2)) <= 0)
      return -1;

    NSDL4_DNS(NULL, cptr, "DNS qname  from segments %s len %d query_size %d", tmp_buf, tmp_len, query_size);
  }else{
      strcpy(tmp_buf, server_name);
      tmp_len = strlen(server_name); 
  }
	
  // Fill encoded name  in the vector, the call returns encoded length
  tmp_len = dns_encode_name(tmp_buf, tmp_len, dnsData->dns_req.qname, NS_MAXCDNAME);

  NSDL4_DNS(NULL, cptr, "DNS qname after encoding %s len %d\n",
     dnsData->dns_req.qname, tmp_len);
  if(tmp_len <= 0) {
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "Failed to encode qname.");
    return -1;
  }

  NS_FILL_IOVEC(g_req_rep_io_vector, dnsData->dns_req.qname, tmp_len);

  query_size += tmp_len;

  // qytpe in proto is already an int from the parsing routine -use as is
  if(!dns_lookup)
    dnsData->dns_req.qtype = request->proto.dns.qtype;
  else //in nonblocking case we are taking ipv4 type as default
   dnsData->dns_req.qtype = T_A;
 
  dnsData->dns_req.qclass = C_IN; // Currently only IN is supported

  NSDL4_DNS(NULL, cptr, "before BE conversion qclass %u (%s) qtype %u (%s) query size %d",
                         dnsData->dns_req.qclass, dns_class_name(dnsData->dns_req.qclass),
                         dnsData->dns_req.qtype, dns_type_name(dnsData->dns_req.qtype),
                         query_size);

  MY_MALLOC(qtype_buf, 2, "malloc for qtype in the message", 0);

  p = qtype_buf;
  v = dnsData->dns_req.qtype;
  DNS__SET16BIT(p,v);
  NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, p, 2);
  //free_array[next_idx++] = 1; // Do not mark it for free as we using part of struct

  MY_MALLOC(qclass_buf, 2, "malloc for qclass in the message", 0);

  p = qclass_buf;
  v = dnsData->dns_req.qclass;
  DNS__SET16BIT(p,v);
  NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, p, 2);
/* next_idx is used as the size of the vector, so increment it here. mark it for
 * freeing as we're allocating.
 */
  //free_array[next_idx++] = 1; 
	
  query_size += QFIXEDSZ;

  NSDL4_DNS(NULL, cptr, "after BE conversion qclass %u (%s) qtype %u (%s) query size %d",
                         dnsData->dns_req.qclass,
                         dns_class_name(dnsData->dns_req.qclass),
                         dnsData->dns_req.qtype,
                         dns_type_name(dnsData->dns_req.qtype),
                         query_size);

  /* 
  * write the length into the first 2 bytes - this should exclude the 2 bytes
  * we preprended
  */
  //if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP && !dns_lookup){
  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP){
    DNSDATA->dns_req.tcp_len_buf[0] = (unsigned char)(( (query_size-2) >> 8) & 0xff);
    DNSDATA->dns_req.tcp_len_buf[1]  = (unsigned char)( (query_size-2) & 0xff);
  }

  // get values of assert_rr_type and assert_rr_data and store in the dns data for checking later
  // if given 
  if(request->proto.dns.assert_rr_type.seg_start && !dns_lookup) {
    tmp_len = get_value_from_segments(cptr, tmp_buf, &request->proto.dns.assert_rr_type, "ASSERT_RR_TYPE", NS_MAXCDNAME);
    // Convert type to numeric value
    if(tmp_len > 0)
      dnsData->dns_req.assert.assert_rr_type = dns_qtype_to_int(tmp_buf);
  }

  // if given 
  if(request->proto.dns.assert_rr_data.seg_start && !dns_lookup) {
     tmp_len = get_value_from_segments(cptr, tmp_buf, &request->proto.dns.assert_rr_data, "ASSERT_RR_DATA", NS_MAXCDNAME);
     // Convert type to numeric value
    if(tmp_len > 0)
      memcpy(dnsData->dns_req.assert.assert_rr_data, tmp_buf, tmp_len);
  }

  NSDL4_DNS(NULL, cptr, "assert_rr_type %d data %s",
                         dnsData->dns_req.assert.assert_rr_type,
                         dnsData->dns_req.assert.assert_rr_data);
  dns_send_req(cptr, query_size, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector), &g_req_rep_io_vector, now, dns_lookup);

  return(0);
}	
