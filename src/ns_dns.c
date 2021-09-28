/**
 * FILE: ns_dns.c
 * PURPOSE: contains all DNS reading and state switching mechanism
 * AUTHOR: 
 */
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
#include "ns_common.h"
#include "nslb_util.h"
#include <ctype.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "nslb_dns.h"
#include "ns_dns.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "ns_log_req_rep.h"
#include "ns_sock_com.h"
#include "ns_dns_reporting.h"
#include "nslb_time_stamp.h"
#include "ns_group_data.h"
#include "nslb_cav_conf.h"

#define        MIN(a,b) (((a)<(b))?(a):(b))

 /* The length of each string can not be >= 0xff */
char g_dns_st_str[][0xff] = { 
 "ST_DNS_HEADER", 
 "ST_DNS_QNAME",
 "ST_DNS_QFIXED",
 "ST_DNS_RR_NAME",
 "ST_DNS_RR_FIXED",
 "ST_DNS_RR_RDATA"
};

/* 
 * check whether the data passed in is the same as the assert data in the cptr
 * assert field. If so, increment the counter for assert data matches
 * we assume that the assert data is a string and should compare the rdata in
 * string format as well.
 */
#define SET_CUR_SERVER(family, svr_entry, sin){\
        if ((svr_entry = get_svr_entry((VUser*)((connection*)(cptr->conn_link))->vptr, ((connection *)(cptr->conn_link))->url_num->index.svr_ptr)) == NULL) {\
          fprintf(stderr, "Start Udp Socket: Unknown host.\n");\
          end_test_run();\
      } else {\
         ((connection *)(cptr->conn_link))->old_svr_entry = svr_entry;\
      }\
      sin = (struct sockaddr_in *)&((connection *)(cptr->conn_link))->cur_server;\
      sin->sin_family = family;\
      sin->sin_port = svr_entry->saddr.sin6_port;\
  }

static void assert_rdata_check(connection *cptr, int type, void *data, int len)
{
  NSDL4_DNS(NULL, cptr, "Method Called");

  if (DNSDATA->dns_req.assert.assert_rr_data[0] != '\0') {
    NSDL4_DNS(NULL, cptr, "Doing Assertion for assert_rr_data = %s with response data = %s, Type = %s", 
                  DNSDATA->dns_req.assert.assert_rr_data, 
                  (char*)data, dns_type_name(type));

    if (!strcmp((char*)DNSDATA->dns_req.assert.assert_rr_data, (char*)data)) {
      NSDL2_DNS(NULL, cptr, "assert rdata found");
      DNSDATA->dns_resp.assert.rr_data_found++;
    } else {
      NSDL2_DNS(NULL, cptr, "assert rdata not found");
    }
  }
  return;
}

void dns_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;
  
  NSDL4_DNS(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d", vptr, cptr, cptr->conn_state);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }

  if(cptr->request_type == DNS_REQUEST && cptr->conn_link){
    if(cptr->conn_fd > 0)
       close(cptr->conn_fd);
    FREE_AND_MAKE_NULL(cptr->url_num, "free'ng dns nonblock url_num", -1);
    free_connection_slot(cptr->conn_link, now);
    free_connection_slot(cptr, now);
  }else{
    Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
  }
}

static u_char *print_inet_addr(connection *cptr, void *addr, int family)
{
  u_char *buf;
  int size;
  
  if (family == AF_INET) {
    size = INET_ADDRSTRLEN;
  }else if (family == AF_INET6){
    size = INET6_ADDRSTRLEN;
  }
  MY_MALLOC(buf,size,"malloc for inet addr string", 0);

  /* print the address  */
  if (inet_ntop(family, (struct in_addr*)addr, (char*)buf, size) == NULL) {
    NSDL2_DNS(NULL, cptr, "error in inet_ntop for type A/AAAA response family %d addr %p ", family, addr);
  }  
  NSDL4_DNS(NULL, cptr, "DNS response type A addr %p addr %s\n", addr, buf);
  return(buf);
}

void dns_init_connection_data(connection *cptr)
{
  int dns_data_size;
  //malloc data for DNS request and response now
  dns_data_size = sizeof(DnsData);
  MY_MALLOC(cptr->data, dns_data_size, "malloc for data in cptr for DNS", 0);
  NSDL4_DNS(NULL, cptr, "DNS data allocated: data %p size %d",      
                        cptr->data, dns_data_size);
  memset(cptr->data, 0, dns_data_size); 
}

void delete_dns_timeout_timer(connection *cptr) 
{
  NSDL4_DNS(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL4_DNS(NULL, cptr, "Deleting Idle timer for DNS response.");
    dis_timer_del(cptr->timer_ptr);
  }
}

void reset_dns_timeout_timer(connection *cptr, u_ns_ts_t now) {
  NSDL4_DNS(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  int type, periodic;
  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL4_DNS(NULL, cptr, "Deleting Idle timer for DNS response.");
    type = cptr->timer_ptr->timer_type;
    periodic = cptr->timer_ptr->periodic;
    dis_timer_del(cptr->timer_ptr);
    dis_timer_add_ex(type, cptr->timer_ptr, now, cptr->timer_ptr->timer_proc, 
                     cptr->timer_ptr->client_data, periodic, 0); 
  }
}

void dns_process_quit(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int status   = cptr->req_ok;
  
  NSDL4_DNS(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, cptr->req_code, buf, bytes_read, now);
    
  // Confail means req_ok is not filled by any error earliar, so its a success case
  if(status == NS_REQUEST_CONFAIL)
    status = NS_REQUEST_OK;

  Close_connection(cptr, 0, now, status, NS_COMPLETION_CLOSE);
}

/* This function converts the int state to STR */
char *dns_state_to_str(int state) 
{
  /* TODO bounds checking */
  return g_dns_st_str[state];
}

#ifdef NS_DEBUG_ON
void debug_log_dns_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type; // Ask BHAV

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == DNS_REQUEST && 
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_DNS))))
    return;


  char log_file[1024];
  int log_fd;

  // Log file name format is dns_session__<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
 
  SAVE_REQ_REP_FILES 
  sprintf(log_file, "%s/logs/%s/dns_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    
  //Since response can come partialy so this will print debug trace many time
  //tcp_bytes_recv = 0, means this response comes first time
  if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    // Do not change the debug trace message as it is parsed by GUI
    NS_DT4(vptr, cptr, DM_L1, MM_DNS, "Response is in file '%s'", log_file);
    
  if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging DNS response\n"
);
  else
  {
    write(log_fd, buf, size);
    close(log_fd);
  }
}

#endif  //NS_DEBUG_ON


/* Return the length of the expansion of an encoded domain name, or
 * -1 if the encoding is invalid.
 */
static int name_length(const unsigned char *encoded, const unsigned char *abuf,
                       int alen)
{
  int n = 0, offset, indir = 0;

  /* Allow the caller to pass us abuf + alen and have us check for it. */
  if (encoded == abuf + alen)
    return -1;

  while (*encoded)
  {
    if ((*encoded & INDIR_MASK) == INDIR_MASK)
    {
      /* Check the offset and go there. */
      if (encoded + 1 >= abuf + alen)
        return -1;
      offset = (*encoded & ~INDIR_MASK) << 8 | *(encoded + 1);
      if (offset >= alen)
        return -1;
      encoded = abuf + offset;

      /* If we've seen more indirects than the message length,
      * then there's a loop.
       */
      if (++indir > alen)
        return -1;
    }
    else
    {
      offset = *encoded;
      if (encoded + offset + 1 >= abuf + alen)
        return -1;
      encoded++;
      while (offset--)
      {
        n += (*encoded == '.' || *encoded == '\\') ? 2 : 1;
        encoded++;
      }
      n++;
    }
  }

  /* If there were any labels at all, then the number of dots is one
  * less than the number of labels, so subtract one.
   */
  return (n) ? n - 1 : n;
}


/* Simply decodes a length-encoded character string. The first byte of the
 * input is the length of the string to be returned and the bytes thereafter
 * are the characters of the string. The returned result will be NULL
 * terminated.
 */
int ares_expand_string(const unsigned char *encoded,
                       const unsigned char *abuf,
                       int alen,
                       unsigned char **s,
                       int *enclen)
{
  unsigned char *q;
  int len;
  if (encoded == abuf+alen)
    return -1;

  len = *encoded;
  if (encoded+len+1 > abuf+alen)
    return -1;

  encoded++;

  MY_MALLOC(*s, (len+1), "malloc for DNS name", 0);
  if (*s == NULL)
    return -1;
  q = *s;
  strncpy((char *)q, (char *)encoded, len);
  q[len] = '\0';

  *s = q;

  *enclen = len+1;

  return 0;
}

/* Expand an RFC1035-encoded domain name given by encoded.  The
 * containing message is given by abuf and alen.  The result given by
 * *s, which is set to a NUL-terminated allocated buffer.  *enclen is
 * set to the length of the encoded name (not the length of the
 * expanded name; the goal is to tell the caller how many bytes to
 * move forward to get past the encoded name).
 *
 * In the simple case, an encoded name is a series of labels, each
 * composed of a one-byte length (limited to values between 0 and 63
 * inclusive) followed by the label contents.  The name is terminated
 * by a zero-length label.
 *
 * In the more complicated case, a label may be terminated by an
 * indirection pointer, specified by two bytes with the high bits of
 * the first byte (corresponding to INDIR_MASK) set to 11.  With the
 * two high bits of the first byte stripped off, the indirection
 * pointer gives an offset from the beginning of the containing
 * message with more labels to decode.  Indirection can happen an
 * arbitrary number of times, so we have to detect loops.
 *
 * Since the expanded name uses '.' as a label separator, we use
 * backslashes to escape periods or backslashes in the expanded name.
 */

int ares_expand_name(const unsigned char *encoded, const unsigned char *abuf,
                     int alen, char **s, int *enclen)
{
  int len, indir = 0;
  char *q;
  const unsigned char *p;

  len = name_length(encoded, abuf, alen);
  if (len < 0)
    return -1;

  MY_MALLOC(*s, (len+1), "malloc for DNS name", 0);
//  *s = malloc(((size_t)len) + 1);
  if (!*s)
    return -1;
  q = *s;

  if (len == 0) {
    /* RFC2181 says this should be ".": the root of the DNS tree.
    * Since this function strips trailing dots though, it becomes ""
     */
    q[0] = '\0';
    *enclen = 1;  /* the caller should move one byte to get past this */
    return 0;
  }

  /* No error-checking necessary; it was all done by name_length(). */
  p = encoded;
  while (*p)
  {
    if ((*p & INDIR_MASK) == INDIR_MASK)
    {
      if (!indir)
      {
        *enclen = p + 2 - encoded;
        indir = 1;
      }
      p = abuf + ((*p & ~INDIR_MASK) << 8 | *(p + 1));
    }
    else
    {
      len = *p;
      p++;
      while (len--)
      {
        if (*p == '.' || *p == '\\')
          *q++ = '\\';
        *q++ = *p;
        p++;
      }
      *q++ = '.';
    }
  }
  if (!indir)
    *enclen = p + 1 - encoded;

  /* Nuke the trailing period if we wrote one. */
  if (q > *s)
    *(q - 1) = 0;
  else
    *q = 0; /* zero terminate */

  return 0;
}

static void set_dns_error_code(connection *cptr, int rcode) {
  NSDL2_DNS(NULL, cptr, "Error code = %d", rcode); 

  switch(rcode) {
    case 0:
     cptr->req_ok =  NS_REQUEST_OK;
     return;
    case 1:
     cptr->req_ok = NS_REQUEST_BAD_RESP; 
     return;
    case 2:
     cptr->req_ok = NS_REQUEST_BAD_RESP; 
     return;
    case 3:
     cptr->req_ok = NS_REQUEST_4xx; 
     return;
    case 4:
     cptr->req_ok = NS_REQUEST_BAD_RESP; 
     return;
  }
}

/*
******************************
 Validation routines 
******************************
*/

/* 
 * routine to validate header in the DNS query response
 *
 * inputs 
 * cptr - connection ptr
 *
 * outputs
 * none
 * return values and what they mean
 * 0 - data was valid
 * -1 - invalid data
 *
 * errors 
 *
 * Algo 
 * 
 */

static int dns_validate_header(connection *cptr, const u_char *abuf, const u_char *aptr)
{
  //VUser *vptr;
  //vptr = (VUser *)cptr->vptr;
  HEADER *qh = &DNSDATA->dns_req.header;
  // assign to header fields in response using this
  HEADER *rh = &DNSDATA->dns_resp.header;
  uint16_t qdcount, ancount, nscount, arcount,  id ;
  u_char qr, opcode, aa, tc, rd, ra, rcode;

  /* Parse the answer header. */
  rh->id = id = DNS_HEADER_QID(abuf);
  rh->qr = qr = DNS_HEADER_QR(abuf);
  rh->opcode = opcode = DNS_HEADER_OPCODE(abuf);
  rh->aa = aa = DNS_HEADER_AA(abuf);
  rh->tc = tc = DNS_HEADER_TC(abuf);
  rh->rd = rd = DNS_HEADER_RD(abuf);
  rh->ra = ra = DNS_HEADER_RA(abuf);
  rh->rcode = rcode = DNS_HEADER_RCODE(abuf);
  rh->qdcount = qdcount = DNS_HEADER_QDCOUNT(abuf);
  rh->ancount = ancount = DNS_HEADER_ANCOUNT(abuf);
  rh->nscount = nscount = DNS_HEADER_NSCOUNT(abuf);
  rh->arcount = arcount = DNS_HEADER_ARCOUNT(abuf);

  // compare fields in header buf to actual header stored in query- store 
  if (rh->id != qh->id) {
    NSDL2_DNS(NULL, cptr, "response ID %u query ID ",rh->id, qh->id); 
    return(-1);
  }

  if (rh->qr !=  1) {  //qr =0 for query, 1 for response
    NSDL2_DNS(NULL, cptr, "response QR %u query %u ",rh->qr, qh->qr);
    return(-1);
  }
  if ( rh->opcode != qh->opcode) {
    NSDL2_DNS(NULL, cptr, "response opcode %u (%s) query opcode %u (%s) ",
        rh->opcode, opcodes[rh->opcode], qh->opcode, opcodes[rh->opcode] ); 
    return(-1);
  }

  if (rh->tc ==  1) {
    NSDL2_DNS(NULL, cptr, "response TC %u",rh->tc);
    return(-1);
  }
  if ( rh->rd != qh->rd ) {
    NSDL2_DNS(NULL, cptr, "response RD %u query RD %u ", rh->rd, qh->rd);  
    return(-1);
  }

  if ( rh->rcode < 0 || rh->rcode > 5) {
    NSDL2_DNS(NULL, cptr, "response RCODE %u (%s)", rh->rcode,
        rcodes[rh->rcode]);
    return(-1);
  }

  if ( rh->ancount == 0  && rh->rcode == 0 ) {
    NSDL2_DNS(NULL, cptr, "response ancount %u" , rh->ancount);
    return(-1);
  }

  /* dump all header fields */

  NSDL2_DNS(NULL, cptr, 
  "\n -----DNS response header----- \n id %u flags %s%s%s%s%s opcode %s rcode %s  qdcount %u ancount %u nscount %u arcount %u", 
      id,
      qr ? "qr " : "",
      aa ? "aa " : "",
      tc ? "tc " : "",
      rd ? "rd " : "",
      ra ? "ra " : "",
  opcodes[opcode], rcodes[rcode], qdcount, ancount, nscount, arcount);

  set_dns_error_code(cptr, rh->rcode); 
  return(0);
}


static const u_char *dns_validate_rdata(connection *cptr, const u_char *aptr,
    const u_char *abuf, int alen, uint16_t type, uint16_t dlen)
{
  int status;
  
  const u_char *inetp, *p;
  typedef union {
    unsigned char * as_uchar;
             char * as_char;
  } Name;

  Name name, name1;
  int len;
  VUser* vptr = cptr->vptr;

  NSDL4_DNS(NULL, cptr, "Method called DNS rdata validation type %s dlen %d",
      dns_type_name(type), dlen );

  /* Display the RR data.  Don't touch aptr. */
  switch (type)
  {
#if 0 //not supported
    case T_MB:
    case T_MD:
    case T_MF:
    case T_MG:
    case T_MR:
#endif 
    case T_CNAME:
    case T_NS:
    case T_PTR:
      /* For these types, the RR data is just a domain name. */
      status = ares_expand_name(aptr, abuf, alen, &name.as_char, &len);
      if (status != 0) {
        NSDL2_DNS(NULL, cptr, "error expanding name in rdata");
        return NULL;
      }
      assert_rdata_check(cptr, type, name.as_char, dlen);
      NSDL2_DNS(NULL, cptr, "\n rdata type %s  domain name %s", dns_type_name(type), name.as_char);
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type PTR",0);
      break;

    case T_A:
      /* The RR data is a four-byte Internet address. */
      if (dlen != 4) {
        NSDL2_DNS(NULL, cptr, "Error rdata size type %s size %u\n",dns_type_name(type), dlen);
        return NULL;
      }
      inetp = print_inet_addr(cptr, (void*)aptr, AF_INET);
      NSDL2_DNS(NULL, cptr, "\n rdata type %s addr %s", dns_type_name(type), inetp);

      if(cptr->conn_link && !(cptr->flags & NS_CPTR_FLAGS_DNS_DONE)){
        PerHostSvrTableEntry_Shr* svr_entry;
        struct sockaddr_in *sin;
        SET_CUR_SERVER(AF_INET, svr_entry, sin);
        bcopy( aptr, &(sin->sin_addr), 4);
        cptr->flags |= NS_CPTR_FLAGS_DNS_DONE;
        NSDL2_DNS(NULL, cptr, "\n NSDNS: rdata type %s addr %s", dns_type_name(type), inetp);
        if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
          dns_resolve_log_write(g_partition_idx, "R", svr_entry->server_name, cptr->dns_lookup_time, &((connection*)(cptr->conn_link))->cur_server);
      }

      assert_rdata_check(cptr, type, (void*)inetp, dlen);
      FREE_AND_MAKE_NULL(inetp, "Freeing memory for inet addr type A",0);

      break;

    case T_AAAA:
      /* The RR data is a 16-byte IPv6 address. */
      if (dlen != 16){
        NSDL2_DNS(NULL, cptr, "Error rdata size type %s size %u\n",dns_type_name(type), dlen);
        return NULL;
      }
      inetp = print_inet_addr(cptr, (void*)aptr, AF_INET6);
      NSDL2_DNS(NULL, cptr, "\n rdata type %s addr %s", dns_type_name(type), inetp);
      if(cptr->conn_link && !(cptr->flags & NS_CPTR_FLAGS_DNS_DONE)){
        PerHostSvrTableEntry_Shr* svr_entry;
        struct sockaddr_in *sin;
        SET_CUR_SERVER(AF_INET6, svr_entry, sin);
        bcopy( aptr, &(sin->sin_addr), 4);
        cptr->flags |= NS_CPTR_FLAGS_DNS_DONE;
        NSDL2_DNS(NULL, cptr, "\n NSDNS: rdata type %s addr %s", dns_type_name(type), inetp);
        if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
          dns_resolve_log_write(g_partition_idx, "R", svr_entry->server_name, cptr->dns_lookup_time, &((connection*)(cptr->conn_link))->cur_server);
      }

      assert_rdata_check(cptr, type, (void*)inetp, dlen);
      FREE_AND_MAKE_NULL(inetp, "Freeing memory for inet addr type AAAA",0);
      break;

    case T_MX:
      /* The RR data is two bytes giving a preference ordering, and
       * then a domain name.
       */
      if (dlen < 2){
        NSDL2_DNS(NULL, cptr, "Error rdata size type %s size %u\n",dns_type_name(type), dlen);
        return NULL;
      }
      status = ares_expand_name(aptr + 2, abuf, alen, &name.as_char, &len);
      if (status != 0){
        NSDL2_DNS(NULL, cptr, "error expanding name in rdata type %s",
            dns_type_name(type));
        return NULL;
      }
      NSDL2_DNS(NULL, cptr, "\n type %s Preference %d domain %s", dns_type_name(type),
          DNS__16BIT(aptr),name.as_char);
      assert_rdata_check(cptr, type, name.as_char, len);
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type MX", 0);
      break;

    case T_SOA:
      /* The RR data is two domain names and then five four-byte
       * numbers giving the serial number and some timeouts.
       */
      p = aptr;
      status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
      if (status != 0){
        NSDL2_DNS(NULL, cptr, "error expanding name in rdata type %s",
            dns_type_name(type));
        return NULL;
      }
      assert_rdata_check(cptr, type, name.as_char, len);

      p += len;
      status = ares_expand_name(p, abuf, alen, &name1.as_char, &len);
      if (status != 0){
        NSDL2_DNS(NULL, cptr, "error expanding name in rdata type %s",
            dns_type_name(type));
        return NULL;
      }
      assert_rdata_check(cptr, type, name1.as_char, len);

      p += len;
      /* 20 bytes total for SERIAL, REFRESH, RETRY, EXPIRE, MINIMUM (all 4 byte
       * values 
       */
      if (p + 20 > aptr + dlen){
        NSDL2_DNS(NULL, cptr, 
            "Error rdata (no space for fields after MNAME and RNAME) type %s size %u\n",
            dns_type_name(type), dlen);
        return NULL;
      }
      // print these values  -- need to assert each of these -- TODO
      NSDL2_DNS(NULL, cptr, "\n rdata type %s MNAME %s RNAME %s %u %u %u %u %u",
          dns_type_name(type), name.as_char, name1.as_char, 
          (unsigned int)DNS__32BIT(p), (unsigned int)DNS__32BIT(p+4),
          (unsigned int)DNS__32BIT(p+8), (unsigned int)DNS__32BIT(p+12),
          (unsigned int)DNS__32BIT(p+16));

      //free both names
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type SOA MNAME ",0);
      FREE_AND_MAKE_NULL(name1.as_char, "Freeing buffer for name in DNS response type SOA RNAME ",0);
      if(cptr->conn_link && !(cptr->flags & NS_CPTR_FLAGS_DNS_DONE)){
        PerHostSvrTableEntry_Shr* svr_entry;
        char s_n[2048];
        char *ptr;
        struct sockaddr_in *sin;
        SET_CUR_SERVER(AF_INET, svr_entry, sin);
        strcpy(s_n, svr_entry->server_name);
        if((ptr = strchr(s_n, ':')) != NULL)
          *ptr = '\0';
        sin->sin_addr.s_addr = inet_addr(s_n);
        cptr->flags |= NS_CPTR_FLAGS_DNS_DONE;
        if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
          dns_resolve_log_write(g_partition_idx, "R", svr_entry->server_name, cptr->dns_lookup_time, &((connection*)(cptr->conn_link))->cur_server);
      }

      break;

#if 0 //we 're not supporting these currently, but keep it so we can enable when
      // needed
    case T_HINFO:
      /* The RR data is two length-counted character strings. */
      p = aptr;
      len = *p;
      if (p + len + 1 > aptr + dlen)
        return NULL;
      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != ARES_SUCCESS)
        return NULL;
      printf("\t%s", name.as_char);
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response ",0)
      p += len;
      len = *p;
      if (p + len + 1 > aptr + dlen)
        return NULL;
      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != 0)
        return NULL;
      printf("\t%s", name.as_char);
      ares_free_string(name.as_char);
      break;

    case T_MINFO:
      /* The RR data is two domain names. */
      p = aptr;
      status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
      if (status != 0)
        return NULL;
      NSDL2_DNS(NULL, cptr, "\t%s.", name.as_char);
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type MINFO ",0);
      p += len;
      status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
      if (status != 0)
        return NULL;
      NSDL2_DNS(NULL, cptr, "\t%s.", name.as_char);
      FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type MINFO",0);
      break;

    case T_TXT:
      /* The RR data is one or more length-counted character
       * strings. */
      p = aptr;
      while (p < aptr + dlen)
        {
          len = *p;
          if (p + len + 1 > aptr + dlen)
            return NULL;
          status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
          if (status != 0)
            return NULL;
          NSDL2_DNS(NULL, cptr, "\t%s", name.as_char);
          FREE_AND_MAKE_NULL(name.as_char, "Freeing buffer for name in DNS response type TXT",0);
          p += len;
        }
      break;

    case T_WKS:
      /* Not implemented yet */
      break;

    case T_SRV:
      /* The RR data is three two-byte numbers representing the
       * priority, weight, and port, followed by a domain name.
       */

      printf("\t%d", DNS__16BIT(aptr));
      printf(" %d", DNS__16BIT(aptr + 2));
      printf(" %d", DNS__16BIT(aptr + 4));

      status = ares_expand_name(aptr + 6, abuf, alen, &name.as_char, &len);
      if (status != ARES_SUCCESS)
        return NULL;
      printf("\t%s.", name.as_char);
      ares_free_string(name.as_char);
      break;

    case T_NAPTR:

      printf("\t%d", DNS__16BIT(aptr)); /* order */
      printf(" %d\n", DNS__16BIT(aptr + 2)); /* preference */

      p = aptr + 4;
      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != 0)
        return NULL;
      printf("\t\t\t\t\t\t%s\n", name.as_char);
      ares_free_string(name.as_char);
      p += len;

      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != 0)
        return NULL;
      printf("\t\t\t\t\t\t%s\n", name.as_char);
      ares_free_string(name.as_char);
      p += len;

      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != 0)
        return NULL;
      printf("\t\t\t\t\t\t%s\n", name.as_char);
      ares_free_string(name.as_char);
      p += len;

      status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
      if (status != 0)
        return NULL;
      printf("\t\t\t\t\t\t%s", name.as_char);
      ares_free_string(name.as_char);
      break;
#endif

    default:
      NSDL2_DNS(NULL, cptr, "\t[Unknown RR; cannot parse]");
      break;
    }

  return aptr + dlen;
}

static const u_char *dns_validate_question(connection *cptr, const u_char *aptr, const u_char
    *abuf, int alen)
{
  union {
    unsigned char * as_uchar;
             char * as_char;
  } name;
  //IW_UNUSED(int type);
  //IW_UNUSED(int dnsclass);
  int status;
  int len;

  NSDL4_DNS(NULL, cptr, "Method called aptr %p abuf %p alen %d", aptr, abuf,
      alen);

  /* Parse the question name. */
  status = ares_expand_name(aptr, abuf, alen, &name.as_char, &len);
  if (status != 0) {
    NSDL2_DNS(NULL, cptr, "error expanding qnamein response");
    return NULL;
  }
  aptr += len;

  /* Make sure there's enough data after the name for the fixed part
   * of the question.
   */
  if (aptr + QFIXEDSZ > abuf + alen)
  {
    FREE_AND_MAKE_NULL(name.as_char, "Freeing qname buffer in DNS response",0);
    return NULL;
  }

  /* Parse the question type and class. */
  #if 0
  type = DNS_QUESTION_TYPE(aptr);
  dnsclass = DNS_QUESTION_CLASS(aptr);
  #endif
  aptr += QFIXEDSZ;

  /* Display the question, in a format sort of similar to how we will
   * display RRs.
   */
  NSDL2_DNS(NULL, cptr, "\n qname %s len %d, qclass %s qtype %s", name.as_char,
      len, dns_class_name(DNS_QUESTION_CLASS(aptr)),dns_type_name(DNS_QUESTION_TYPE(aptr)));

  FREE_AND_MAKE_NULL(name.as_char, "Freeing qname buffer in DNS response",0);
  return aptr;
}

static const u_char *dns_validate_rr(connection *cptr, const u_char *aptr,
    const u_char *abuf, int alen)
{
  uint16_t  type;
  //uint16_t dnsclass;
  uint16_t dlen;
  int status;
  //uint32_t ttl;
  int len;

  union {
    unsigned char * as_uchar;
             char * as_char;
  } name;

  NSDL4_DNS(NULL, cptr, "Method called  aptr %p  abuf %p alen %d",aptr, abuf, alen);

  /* Parse the RR name. */
  status = ares_expand_name(aptr, abuf, alen, &name.as_char, &len);
  if (status != 0) {
    NSDL2_DNS(NULL, cptr, "error expanding rr name in response");
    return NULL;
  }
  aptr += len;

  /* Make sure there is enough data after the RR name for the fixed
   * part of the RR.
   */
  if (aptr + RRFIXEDSZ > abuf + alen)
  {
    FREE_AND_MAKE_NULL(name.as_char, 
        "Freeing buffer for rr name in DNS response",0);
    return NULL;
  }

  /* Parse the fixed part of the RR, and advance to the RR data
   * field. 
   */
  type = DNS_RR_TYPE(aptr);
  //dnsclass = DNS_RR_CLASS(aptr);
  //ttl = DNS_RR_TTL(aptr);
  dlen = DNS_RR_LEN(aptr);

  //check for the assert type
  if (type == DNSDATA->dns_req.assert.assert_rr_type) {
    NSDL2_DNS(cptr, NULL, "found rr assert type %s",dns_type_name(type));
    DNSDATA->dns_resp.assert.rr_type_found++;
  }

  aptr += RRFIXEDSZ;
  if (aptr + dlen > abuf + alen)
  {
    FREE_AND_MAKE_NULL(name.as_char, 
        "Freeing buffer for rr name in DNS response",0);
    return NULL;
  }

  /* Display the RR name, class, and type. */
  //NSDL2_DNS (NULL, cptr, "\n rr_name %s ttl %u rr_class %s rr_type %s rdlen %d", 
  //    name.as_char, DNS_RR_TTL(aptr), dns_class_name(DNS_RR_CLASS(aptr)), dns_type_name(type), dlen);

  FREE_AND_MAKE_NULL(name.as_char, 
        "Freeing buffer for rr_name in DNS response",0);

  aptr = dns_validate_rdata(cptr, aptr, abuf, alen, type, dlen);

  return(aptr);
}

/*
******************************
 Validation routines end
******************************
*/


static inline void handle_dns_bad_read (connection *cptr, u_ns_ts_t now, int req_status)
{
  NSDL1_DNS(NULL, cptr,  "Method called, now = %u, req_status = %d", now, req_status);
  //TODO:If DNS fails then need to handle only DNS connection. 
  //For this make new function which will be copy of retry_connection()
  //and that function will remove data specific to DNS and will not call start_new_socket()
  //This will call the function which handles the DNS request
  retry_connection(cptr, now, req_status);
}

static int dns_process_response(connection *cptr)
{
  const u_char *buf, *bufptr;
  uint16_t qdcount, ancount, nscount, arcount;
  int len, i;
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;

  buf = DNSDATA->dns_resp.buf;
  bufptr = buf + HFIXEDSZ;
  len = DNSDATA->dns_resp.len;

  NSDL4_DNS(NULL, cptr,  "Method called buf %p bufptr %p len %d", buf, bufptr, len);

  if (dns_validate_header(cptr, buf, bufptr) == -1) {
    NSEL_MAJ (vptr, cptr, ERROR_ID, ERROR_ATTR, "Error validation DNS header ");
    return(-1);
  } 

  qdcount =DNSDATA->dns_resp.header.qdcount;
  ancount =DNSDATA->dns_resp.header.ancount;
  nscount =DNSDATA->dns_resp.header.nscount;
  arcount =DNSDATA->dns_resp.header.arcount;

  for (i=0; i< qdcount; i++) {
  NSDL2_DNS(NULL, cptr,  "\n -----DNS display Question records-----");
    if ( (bufptr = dns_validate_question(cptr, bufptr, buf, len)) == NULL) {
      NSEL_MAJ (vptr, cptr, ERROR_ID, ERROR_ATTR, "Error validation DNS questions");
      return(-1);
    } 
  }

  for (i=0; i< ancount; i++) {
  NSDL2_DNS(NULL, cptr,  "\n -----DNS display Answer records-----");
    if ( (bufptr = dns_validate_rr(cptr, bufptr, buf, len)) == NULL) {
      NSEL_MAJ (vptr, cptr, ERROR_ID, ERROR_ATTR, "Error validation DNS rr \n");
      return(-1);
    } 
  }

  for (i=0; i< nscount; i++) {
  NSDL2_DNS(NULL, cptr,  "\n -----DNS display Authority records-----");
    if ( (bufptr = dns_validate_rr(cptr, bufptr, buf, len)) == NULL) {
      NSEL_MAJ (vptr, cptr, ERROR_ID, ERROR_ATTR, "Error validation DNS ns  \n");
      return(-1);
    } 
  }

  for (i=0; i< arcount; i++) {
  NSDL2_DNS(NULL, cptr,  "\n -----DNS display Additional records-----");
    if ( (bufptr = dns_validate_rr(cptr, bufptr, buf, len)) == NULL) {
      NSEL_MAJ (vptr, cptr, ERROR_ID, ERROR_ATTR, "Error validation DNS ar  \n");
      return(-1);
    } 
  }


  if (DNSDATA->dns_req.assert.assert_rr_data[0] != '\0') {
     if(DNSDATA->dns_resp.assert.rr_data_found == 0) {
       vptr->page_status = NS_REQUEST_CV_FAILURE; 
       NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE,
                                              vptr->user_index, vptr->sess_inst,
                                              EVENT_CORE, EVENT_MAJOR,
                                              vptr->sess_ptr->sess_name,
                                              vptr->cur_page->page_name,
                                              nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                              get_request_string(cptr),
                                              "Page failed with status %s due to assertion on resource record data.",
                                              get_error_code_name(NS_REQUEST_CV_FAILURE));
     }
  }

  if (DNSDATA->dns_req.assert.assert_rr_type) {
     if(DNSDATA->dns_resp.assert.rr_type_found == 0) {
       vptr->page_status = NS_REQUEST_CV_FAILURE; 
       NS_EL_4_ATTR(EID_HTTP_PAGE_ERR_START + NS_REQUEST_CV_FAILURE,
                                              vptr->user_index, vptr->sess_inst,
                                              EVENT_CORE, EVENT_MAJOR,
                                              vptr->sess_ptr->sess_name,
                                              vptr->cur_page->page_name,
                                              nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
                                              get_request_string(cptr),
                                              "Page failed with status %s due to assertion on resource record type.",
                                              get_error_code_name(NS_REQUEST_CV_FAILURE));
     }
  }

  return(0);
} 

static inline int dns_do_read(connection *cptr, u_ns_ts_t now, u_char *buf, int bytes_read)
{
  int ret, tmp_len, bytes_handled = 0;
  //VUser *vptr;
  //vptr = (VUser *)cptr->vptr;

  NSDL4_DNS(NULL, cptr, "Method called conn state=%d, proto_state = %d, now = %u, bytes_read = %d",
                        cptr->conn_state, cptr->proto_state, now, bytes_read);

  bytes_handled = 0;

#ifdef NS_DEBUG_ON
  debug_log_dns_res(cptr, (char*)buf, bytes_read); /* TODO:BHAV */ // NJ is it still TODO ?
#endif


  // In case of UDP, we do not get 2 bytes which has size of response as 
  // UDP is recieved as full in one pkt. If more than what UDP can support, it gets truncated
  // So we set tcp_count to 2 and dns_resp.len to length of datagram pkt
  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_UDP) {
    if(DNSDATA->dns_resp.tcp_count != 2) {
#if 0  // What minimum size AN-TODO, May be Query Size
      if(bytes_read <= XXXX) // UDP packet muse be of at least this size??
      {
          error log and return -1
      }
#endif
      DNSDATA->dns_resp.tcp_count = 2; // Making Fool to dns_do_read
      int tmp_len = DNSDATA->dns_resp.len = bytes_read; 
      MY_MALLOC(DNSDATA->dns_resp.buf, tmp_len, "malloc for DNS response", 0);
    }
  } else {
    average_time->dns_rx_bytes += bytes_read;
    average_time->dns_total_bytes += bytes_read;
  }
  for(; bytes_handled < bytes_read; ) {

    if (DNSDATA->dns_resp.tcp_count != 2) {
      DNSDATA->dns_resp.tcp_len_buf[DNSDATA->dns_resp.tcp_count++] = 
        buf[bytes_handled++];
      if (DNSDATA->dns_resp.tcp_count == 2) { //have the 2 byte length now
        DNSDATA->dns_resp.len = DNSDATA->dns_resp.tcp_len_buf[0] << 8
          | DNSDATA->dns_resp.tcp_len_buf[1] ;
        NSDL4_DNS(NULL, cptr, "DNS response len %d",DNSDATA->dns_resp.len);
        //malloc buffer to read the message 
        tmp_len = DNSDATA->dns_resp.len; 
        MY_MALLOC(DNSDATA->dns_resp.buf, tmp_len, "malloc for DNS response", 0);
      }
      continue;
    }

    //loop and read the entire length now
    DNSDATA->dns_resp.buf[DNSDATA->dns_resp.count++] = buf[bytes_handled++]; 
  } //for
  if (DNSDATA->dns_resp.count == DNSDATA->dns_resp.len) {
    ret = dns_process_response(cptr);
    return(ret);  //returns 0 or -1
  } 

  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_UDP) {
    NSDL4_DNS(NULL, cptr, "Something wrong with the DNS UDP read assuming failure");
    // As it is UDP it must completed dont go for read again
    return -1;  // Assume Failure
  } else {
    return(1);  //read again
  }
}

/* Here timout timer will be reset if EAGAIN comes
 * and deleted in case of dns_do_read  or Close_connection */
int
handle_dns_read( connection *cptr, u_ns_ts_t now ) 
{

  u_char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
  VUser *vptr;

  int bytes_read; // byte_consumed, bytes_handled = 0;
  struct sockaddr_in6 from;
  socklen_t fromlen = sizeof(struct sockaddr_in6);

  // for bug #15262, Resetting dns timer here instead of deleting . This will delete as well as add timer . 
  reset_dns_timeout_timer(cptr, now); 
  vptr = cptr->vptr;

  NSDL4_DNS(vptr, cptr, "Method called, conn state=%d, now = %u", cptr->conn_state, now);
  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */
  
  while (1) {

    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

    if(cptr->url_num->proto.dns.proto == USE_DNS_ON_UDP) {
      bytes_read = recvfrom(cptr->conn_fd, buf, bytes_read, 0, (struct sockaddr*)&from, &fromlen);
    } else {
      bytes_read = read(cptr->conn_fd, buf, bytes_read);
    }

    NSDL4_DNS(vptr, cptr, "req code %d bytes_read = %d\n", cptr->req_code, bytes_read);

    if ( bytes_read < 0 ) { // Some error in read
      if (errno == EAGAIN) { // No data available, so return
         //Moving reset_dns_timeout_timer from here 
        return 1;
      }
      // Some serious error, do cannot recover
    /*  NSDL2_DNS(NULL, cptr, "DNS read failed (%s) for host = %s [port = %d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);*/
      FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1); // In other case it will be done in close_fd
      handle_dns_bad_read (cptr, now, NS_REQUEST_BAD_HDR);
      return -1;
    } 

    if (bytes_read == 0) {      //connection closed by other end
      FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1); // In other case it will be done in close_fd
      //free_dns_cptr(cptr);
      handle_dns_bad_read (cptr, now, NS_REQUEST_BAD_HDR);
      //handle_server_close (cptr, now);
      return -1;
    }

    int ret = dns_do_read(cptr, now, buf, bytes_read);
    if(cptr->conn_link){
      u_ns_ts_t local_end_time_stamp = get_ms_stamp();//set end time
      cptr->dns_lookup_time = local_end_time_stamp - cptr->ns_component_start_time_stamp; //time taken while resolving host
      cptr->ns_component_start_time_stamp = local_end_time_stamp;//Update component time. 
      UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, cptr->dns_lookup_time);
      NSDL4_DNS(vptr, cptr, "DNS response processed - lookup time = [%d]", cptr->dns_lookup_time);
    }

    if(ret < 0) // Error Validation failed
    {
      if(cptr->conn_link){
        INCREMENT_DNS_LOOKUP_FAILURE_COUNTER(vptr);
      }
      FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1); // In other case it will be done in close_fd
      handle_dns_bad_read (cptr, now, NS_REQUEST_BAD_RESP);
      return -1;
    }
    if(ret == 0) // DNS response is done
    {
      NSDL4_DNS(vptr, cptr, "DNS response processed - closing connection now ");
      if(!cptr->conn_link){
        Close_connection(cptr, 0, now, cptr->req_ok, NS_COMPLETION_CLOSE);
      }
      else{
        if (global_settings->protocol_enabled & DNS_CACHE_ENABLED)
          memcpy(&(vptr->usr_entry[((connection *)(cptr->conn_link))->url_num->index.svr_ptr->idx].resolve_saddr), &(((connection *)(cptr->conn_link))->cur_server), sizeof(struct sockaddr_in6));
        FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1); // In other case it will be done in close_fd
        NSDL4_DNS(vptr, cptr, "DNS FD = %d", cptr->conn_fd);
        remove_select(cptr->conn_fd);
        //vptr->last_cptr = NULL;
        delete_dns_timeout_timer(cptr); //Adding delete timer here. This will delete timer in case of dns_lookup in http/https protocol  
        close(cptr->conn_fd);
        FREE_AND_MAKE_NULL(cptr->url_num, "free'ng dns nonblock url_num", -1);
      }
      //TODO - put other checks here 
      return(ret);
    }
    continue;
  }
}
