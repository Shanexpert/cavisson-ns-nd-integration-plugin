/**********************************************************************
 * File Name            : ns_http_auth.c
 * Author(s)            : Shilpa Sethi
 * Date                 : 18 Feb 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Parsing & Processing HTTP Authenticate NTLM Packet
 *                        
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 **********************************************************************/
 /* Proxy-Auth-Chain
 * In case both proxy and server has authentication, 
 * i.e. 401 is received in response to 407, 
 * authorization headers for both server and proxy should be sent together
 */
 
#include "ns_cache_include.h"
#include "ns_http_auth.h"
#include "ns_http_script_parse.h"
#include "ntlm.h"
#include "nslb_http_auth.h"

//#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  //#include "nslb_ssl_lib.h"
//#endif

#include "ns_proxy_server.h"
#include "ns_proxy_server_reporting.h"
#include "ns_h2_req.h"
#include "netstorm.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"


char auth_basic[] = "BASIC ";
int auth_basic_len = sizeof(auth_basic) - 1;
char auth_digest[] = "DIGEST ";
int auth_digest_len = sizeof(auth_digest) - 1;
char auth_ntlm[] = "NTLM ";
int auth_ntlm_len = sizeof(auth_ntlm) - 1;
char auth_negotiate[] = "Negotiate ";
int auth_negotiate_len = sizeof(auth_negotiate) - 1;
char *authorization_proxy_hdr_http2 = "proxy-authorization";
int authorization_proxy_hdr_http2_len = 19;
char *authorization_hdr_http2 = "authorization";
int authorization_hdr_http2_len = 13;

extern int get_full_element(VUser *vptr, const StrEnt_Shr* seg_tab_ptr, char *elm_buf, int *elm_size);

#define MAX_NTLM_PKT_SIZE  256*1024
#define PROXY_CHAIN 1 
const char authorization_hdr[] = "Authorization: ";
const int authorization_hdr_len = sizeof(authorization_hdr) - 1;
/*Proxy Authorization: Add new Proxy-Authorization header*/
const char authorization_proxy_hdr[] = "Proxy-Authorization: ";
const int authorization_proxy_hdr_len = sizeof(authorization_proxy_hdr) - 1;

//Can be moved to log.h from log.c
#if 0
#define DEFAULT_MAX_DEBUG_FILE_SIZE 1000000
static unsigned int max_debug_log_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;  
#define DEBUG_HEADER "Absolute Time Stamp|Relative Time Stamp|File|Line|Function|Group|Parent/Child|User Index|Session Instance|Page|Instance|Logs"
#endif
//const char hdr_end[] = "\r\n";
//const int hdr_end_len = 2;


#define CHECK_OTHER_AUTH_AND_RETURN(x)  {   \
  if ( (cptr->flags & NS_CPTR_AUTH_TYPE_FIXED) != 0) {      \
    if ( (cptr->flags & NS_CPTR_AUTH_MASK) != (x)) {      \
      NSDL2_AUTH(vptr, cptr, "cptr->flags 0x%x auth-flags 0x%x found other auth %s is set, ignoring %s",cptr->flags, (cptr->flags & NS_CPTR_AUTH_MASK), auth_to_string(cptr->flags & NS_CPTR_AUTH_MASK),auth_to_string(x));     \
        cptr->header_state = HDST_TEXT;         \
        return; \
    }       \
  }       \
}

#define IS_KERBEROS_SET(x)  \
  ((x) & NS_CPTR_RESP_FRM_AUTH_KERBEROS)

#define IS_NTLM_SET(x)  \
  ((x) & NS_CPTR_RESP_FRM_AUTH_NTLM)

#define IS_DIGEST_SET(x)  \
  ((x) & NS_CPTR_RESP_FRM_AUTH_DIGEST)

#define IS_BASIC_SET(x)  \
  ((x) & NS_CPTR_RESP_FRM_AUTH_BASIC)


/* macro to check one or more auth types set and leave only 1 set at the end.
 * logic -
 * if only auth type is set, leave this set 
 * if there is more than 1, the order of preference is --
 * if (kerberos and any other auth)
 * if kerberos keyword is set use kerberos
 * else 
 * use other auth that is set in this order -- ntlm, digest, basic
 * if any other combination of auths is set, check in this order
 * ntlm, digest, basic
 *
 * we also need to set the proto state here as this can be done only after the decision on 
 * the auth that will be used is done. if  we do this where the flag for any of the auths is
 * set the first time, the state gets overwritten as there is only 1 state (unlike flags)
 */


#define CHECK_AND_SET_SINGLE_AUTH()  {   \
  int auth_set =0;            \
  int kerb_enabled = runprof_table_shr_mem[grp_idx].gset.kerb_settings.enable_kerb;    \
  NSDL2_AUTH(NULL, cptr, "CHECK_AND_SET_SINGLE_AUTH: cptr->flags 0x%x kerb_enabled %d",cptr->flags, kerb_enabled);        \
  if ( (cptr->flags & NS_CPTR_AUTH_MASK) == NS_CPTR_RESP_FRM_AUTH_KERBEROS) \
  auth_set =   NS_CPTR_RESP_FRM_AUTH_KERBEROS;    \
  else if  ( (cptr->flags & NS_CPTR_AUTH_MASK) == NS_CPTR_RESP_FRM_AUTH_NTLM)  \
  auth_set =   NS_CPTR_RESP_FRM_AUTH_NTLM;    \
  else if  ( (cptr->flags & NS_CPTR_AUTH_MASK) == NS_CPTR_RESP_FRM_AUTH_DIGEST)\
  auth_set =   NS_CPTR_RESP_FRM_AUTH_DIGEST;    \
  else if  ( (cptr->flags & NS_CPTR_AUTH_MASK) == NS_CPTR_RESP_FRM_AUTH_BASIC) \
  auth_set =   NS_CPTR_RESP_FRM_AUTH_BASIC;    \
  else if (cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)  {\
    if (kerb_enabled)  \
    auth_set =  NS_CPTR_RESP_FRM_AUTH_KERBEROS;    \
    else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)    \
    auth_set =  NS_CPTR_RESP_FRM_AUTH_NTLM;    \
    else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)  \
    auth_set =  NS_CPTR_RESP_FRM_AUTH_DIGEST;    \
    else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)   \
    auth_set =  NS_CPTR_RESP_FRM_AUTH_BASIC;    \
  }    \
  else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)    \
  auth_set =  NS_CPTR_RESP_FRM_AUTH_NTLM;    \
  else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST) \
  auth_set =  NS_CPTR_RESP_FRM_AUTH_DIGEST;    \
  else if  (cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC) \
  auth_set =  NS_CPTR_RESP_FRM_AUTH_BASIC;    \
  \
  cptr->flags &= ~NS_CPTR_AUTH_MASK;    \
  cptr->flags |= auth_set;        \
  cptr->flags |= NS_CPTR_AUTH_TYPE_FIXED;   \
  if (IS_KERBEROS_SET(cptr->flags))         \
  cptr->proto_state = ST_AUTH_KERBEROS_RCVD;  \
  else if (IS_NTLM_SET(cptr->flags))         \
  cptr->proto_state = ST_AUTH_NTLM_RCVD;      \
  else if (IS_DIGEST_SET(cptr->flags))         \
  cptr->proto_state = ST_AUTH_DIGEST_RCVD;      \
  else if (IS_BASIC_SET(cptr->flags))         \
  cptr->proto_state = ST_AUTH_BASIC_RCVD;     \
}



char *auth_to_string(int auth_flag)
{
  if ( (auth_flag & NS_CPTR_RESP_FRM_AUTH_KERBEROS) == NS_CPTR_RESP_FRM_AUTH_KERBEROS) {
    return("AUTH_KERBEROS");
  } else if ( (auth_flag & NS_CPTR_RESP_FRM_AUTH_NTLM) == NS_CPTR_RESP_FRM_AUTH_NTLM) {
    return("AUTH_NTLM");
  } else if ( (auth_flag &  NS_CPTR_RESP_FRM_AUTH_DIGEST) == NS_CPTR_RESP_FRM_AUTH_DIGEST)  {
    return("AUTH_DIGEST");
  } else if ( (auth_flag & NS_CPTR_RESP_FRM_AUTH_BASIC) == NS_CPTR_RESP_FRM_AUTH_BASIC)  {
    return("AUTH_BASIC");
  } else {
    return("AUTH_UNKNOWN");
  }
}

//----------------ERROR HANDLING && LOGGING SECTION BEGINS------------------------------------------
void dump_pkt(connection *cptr, VUser *vptr, u_ns_char_t *ntlm_pkt, int ntlm_pkt_size)
{
  int i;
  char dump[MAX_NTLM_PKT_SIZE] = {0};

  for(i=0; i < ntlm_pkt_size; ++i)
  {
    sprintf(dump, "%s %02X ", dump, ntlm_pkt[i]);
    if(i/4 == 0)
      strcat(dump, "\t");
  }
  NSDL4_AUTH(vptr, cptr, "%02X ", ntlm_pkt[i]);

}

static void free_auth_data(connection *cptr)
{

  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);
  NSDL2_AUTH(vptr, cptr, "Method called."); 

  FREE_AND_MAKE_NULL_EX(ntlm->partial_hdr, ntlm->hdr_len, "cptr->vptr->httpdata->ntlm_t->partial_hdr", -1);
  FREE_AND_MAKE_NULL_EX(ntlm->challenge, ntlm->hdr_len, "HTTP AUTH NTLM - Type2 Msg", -1);
}


static void auth_handle_invalid_msg(connection *cptr, VUser *vptr, 
                                   char *err_msg, u_ns_char_t *ntlm_pkt, int ntlm_pkt_size) 
{
  NSDL2_AUTH(vptr, cptr, "Method called. Error = %s", err_msg); 

  dump_pkt(cptr, vptr, ntlm_pkt, ntlm_pkt_size);

  cptr->proto_state = ST_AUTH_HANDSHAKE_FAILURE;
//  cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_NTLM;
  NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, err_msg);

  free_auth_data(cptr);
}

//----------------ERROR HANDLING && LOGGING SECTION ENDS ------------------------------------------


//----------------HEADER PARSING SECTION BEGINS ------------------------------------------
/*
*************************************************************************************************
Description       : This method is used in case auth ntlm header is partially read

Input Parameters  :
     header_buffer: Pointer in the received buffer for the auth ntlm without NULL termination
            length: Number of bytes read in latest read (without NULL)
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/

static void auth_save_partial_headers(char* header_buffer, connection *cptr, int length)
{
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);

  NSDL2_AUTH(vptr, cptr, "Method Called");
  int prev_length = ntlm->hdr_len;

  ntlm->hdr_len += length;

  //Adding 1 for NULL first time only
  if(prev_length == 0)
       ++ntlm->hdr_len;

  MY_REALLOC_EX(ntlm->partial_hdr, ntlm->hdr_len, prev_length, "Set NTLM Partial Header Line", -1);
  memset(ntlm->partial_hdr + prev_length, 0, length);

  strncpy((char *)ntlm->partial_hdr + prev_length, header_buffer, length);
  NSDL2_AUTH(vptr, cptr, "Exiting Method = %s", ntlm->partial_hdr);
}

/*************************************************************************************************
Description       : This method is called to free the buffer used in partial read of auth ntlm header,

Input Parameters  :
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/

static void auth_free_partial_hdr(connection *cptr)
{  
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);

  NSDL2_AUTH(vptr, cptr, "Method Called");

  FREE_AND_MAKE_NULL_EX(ntlm->partial_hdr, ntlm->hdr_len, "cptr->vptr->httpdata->ntlm_t->partial_hdr", -1);
  ntlm->hdr_len = 0; // is used to keep length of auth ntlm header
  NSDL2_AUTH(vptr, cptr, "Exiting Method");
}

static void auth_basic_headers_parse_set_value(char *basic_pkt, int basic_pkt_len, connection *cptr)
{
  VUser *vptr = cptr->vptr;
  
  NSDL2_AUTH(vptr, cptr, "Method Called. basic_pkt=[%s], basic_pkt_len=%d, cptr->proto_state=0X%x", basic_pkt, basic_pkt_len, cptr->proto_state);

  //Case1: 401 Unauthorized received with 'Authenticate: BASIC' header received 
  //after sending Authorization: Basic <uid:pwd>
  if(cptr->proto_state == ST_AUTH_BASIC_RCVD)
  {
    NSDL2_AUTH(vptr, cptr, "Method Called. basic_pkt=[%s], basic_pkt_len=%d, cptr->proto_state=0X%x", basic_pkt, basic_pkt_len, cptr->proto_state);
    HANDLE_INVALID_AUTH_MSG((u_ns_char_t*)basic_pkt, basic_pkt_len , 
        "401 Unauthorized received with 'Authenticate: BASIC' header received after sending Authorization: Basic <uid:pwd>");
    return;
  }
  
  //Case1: 401 Unauthorized received, with 'Authenticate: BASIC' 
  if(!basic_pkt_len)
  {
    NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Realm not received as part of 'WWW-Authenticate: BASIC header'");
    return;
  }

  cptr->flags |= NS_CPTR_RESP_FRM_AUTH_BASIC;
     
  NSDL2_AUTH(vptr, cptr, "Exitting Method. proto_state=%d", cptr->proto_state);
}

static void auth_digest_headers_parse_set_value(char *digest_pkt, int digest_pkt_len, connection *cptr)
{
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  //u_ns_char_t digest_pkt_decoded[MAX_NTLM_PKT_SIZE]; 
  //int decoded_pkt_len;
  
  NSDL2_AUTH(vptr, cptr, "Method Called. digest_pkt=[%s], digest_pkt_len=%d", digest_pkt, digest_pkt_len);

  //Case1: 401 Unauthorized received with 'Authenticate: DIGEST' header received 
  //after sending Authorization: Digest <uid:pwd>
  if(cptr->proto_state == ST_AUTH_DIGEST_RCVD)
  {
    HANDLE_INVALID_AUTH_MSG((u_ns_char_t*)digest_pkt, digest_pkt_len , 
        "401 Unauthorized received with 'Authenticate: DIGEST' header received after sending Authorization: Basic <uid:pwd>");
    return;
  }
  
  //Case1: 401 Unauthorized received, with 'Authenticate: DIGEST' 
  if(!digest_pkt_len)
  {
    NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Realm not received as part of 'WWW-Authenticate: DIGEST header'");
    return;
  }

  //decoded_pkt_len = from64tobits((char *)digest_pkt_decoded, digest_pkt); 
  httpdata->ntlm.hdr_len = digest_pkt_len;

  NSDL2_AUTH(vptr, cptr, "digest_pkt=[%s], digest_pkt_len=%d", digest_pkt, digest_pkt_len);
  MY_MALLOC_AND_MEMSET(httpdata->ntlm.challenge, digest_pkt_len + 1, "HTTP AUTH DIGEST Msg", -1);
  memcpy(httpdata->ntlm.challenge, digest_pkt, httpdata->ntlm.hdr_len); 
  httpdata->ntlm.challenge[httpdata->ntlm.hdr_len] = 0;
 
  cptr->flags |= NS_CPTR_RESP_FRM_AUTH_DIGEST;
     
  NSDL2_AUTH(vptr, cptr, "Exitting Method. proto_state=%d, digest challenge=[%s], challenge_len=%d", cptr->proto_state, httpdata->ntlm.challenge, httpdata->ntlm.hdr_len);
}

/*************************************************************************************************
Description       : This method is used to set the appropriate NTLM state and store NTLM Type2 msg
                    in HTTPData

Input Parameters  :
          ntlm_pkt: NTLM packet received in WWW-Authenticate Response Header
      ntlm_pkt_len: ntlm_pkt length
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None
*************************************************************************************************/
static void auth_ntlm_headers_parse_set_value(char *ntlm_pkt, int ntlm_pkt_len, connection *cptr)
{
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  u_ns_char_t ntlm_pkt_decoded[MAX_NTLM_PKT_SIZE]; 
  int decoded_pkt_len;
  
  NSDL2_AUTH(vptr, cptr, "Method Called. ntlm_pkt=[%s], ntlm_pkt_len=%d", ntlm_pkt, ntlm_pkt_len);

  //Case1: NTLM header received after sending Type3 Msg 
  if(cptr->proto_state == ST_AUTH_NTLM_TYPE2_RCVD)
  {
    HANDLE_INVALID_AUTH_MSG((u_ns_char_t*)ntlm_pkt, ntlm_pkt_len , 
                          "'Authenticate: NTLM' Header Received after sending Type3 Msg, when expecting 200 OK");
    return;
  }
  
  //Case1: NTLM header received after sending Type3 Msg
  //if(cptr->proto_state == ST_AUTH_NTLM_TYPE2_RCVD)
  if(cptr->proto_state == ST_AUTH_NTLM_RCVD)
  {
    //Case 2a. some NTLM data received in packet (Assuming it is NTLM Type2 message)
    if(ntlm_pkt_len)
    {
      decoded_pkt_len = from64tobits((char *)ntlm_pkt_decoded, ntlm_pkt); 
      httpdata->ntlm.hdr_len = decoded_pkt_len;

      if(httpdata->ntlm.hdr_len != 0)
      {
        MY_MALLOC_AND_MEMSET(httpdata->ntlm.challenge, httpdata->ntlm.hdr_len + 1, "HTTP AUTH NTLM - Type2 Msg", -1);
        memcpy(httpdata->ntlm.challenge, ntlm_pkt_decoded, httpdata->ntlm.hdr_len); 
        memcpy(httpdata->ntlm.challenge + httpdata->ntlm.hdr_len, "0", 1);

        cptr->proto_state = ST_AUTH_NTLM_TYPE2_RCVD;
        cptr->flags |= NS_CPTR_RESP_FRM_AUTH_NTLM;
     
        NSDL2_AUTH(vptr, cptr, "NTLM Type2 Packet Received. proto_state=%d", cptr->proto_state);
      }
      else
      {
       //Case 2b. Invalid NTLM packet received
      NSDL2_AUTH(vptr, cptr, "Invalid NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server. proto_state=%d, ", cptr->proto_state);
      HANDLE_INVALID_AUTH_MSG((u_ns_char_t *)ntlm_pkt, ntlm_pkt_len, 
                     "Invalid NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server");
      }
    }
    else
    {
      //Case 2b. Blank NTLM packet received
      NSDL2_AUTH(vptr, cptr, "Empty NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server. proto_state=%d, ", cptr->proto_state);
      HANDLE_INVALID_AUTH_MSG((u_ns_char_t *)ntlm_pkt, ntlm_pkt_len, 
                     "Empty NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server");
    }
  }
  else
  {
    //1st Unauthorized message received
    if(ntlm_pkt_len) 
    {
       //Case 2b. Blank NTLM packet received
      NSDL2_AUTH(vptr, cptr, "NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server. proto_state=%d, ", cptr->proto_state);
      HANDLE_INVALID_AUTH_MSG((u_ns_char_t *)ntlm_pkt, ntlm_pkt_len, 
                     "Empty NTLM packet received in Header 'Authenticate: NTLM' when expecting Type2 Packet from server");

    }
    cptr->flags |= NS_CPTR_RESP_FRM_AUTH_NTLM;
    NSDL2_AUTH(vptr, cptr, "401 Unauthorized received. proto_state=%d", cptr->proto_state);
  }
 
  NSDL2_AUTH(vptr, cptr, "Exiting Method");
}

static void auth_kerberos_headers_parse_set_value(char *kerberos_pkt, int kerberos_pkt_len, connection *cptr)
{
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  u_ns_char_t kerberos_pkt_decoded[MAX_NTLM_PKT_SIZE]; 
  int decoded_pkt_len;

  NSDL2_AUTH(vptr, cptr, "Method Called. kerberos_pkt=[%s], kerberos_pkt_len=%d", kerberos_pkt, kerberos_pkt_len);

  if(cptr->proto_state == ST_AUTH_KERBEROS_RCVD) {
  //we expect another exchange when the context is not established for kerberos
    if(kerberos_pkt_len) {
      decoded_pkt_len = from64tobits((char *)kerberos_pkt_decoded, kerberos_pkt); 
      httpdata->ntlm.hdr_len = decoded_pkt_len;

      MY_MALLOC_AND_MEMSET(httpdata->ntlm.challenge, httpdata->ntlm.hdr_len + 1, "HTTP AUTH KERB - token", -1);
      memcpy(httpdata->ntlm.challenge, kerberos_pkt_decoded, httpdata->ntlm.hdr_len); 
      memcpy(httpdata->ntlm.challenge + httpdata->ntlm.hdr_len, "0", 1);

      cptr->proto_state = ST_AUTH_KERBEROS_CONTINUE;
      cptr->flags |= NS_CPTR_RESP_FRM_AUTH_KERBEROS;

      NSDL2_AUTH(vptr, cptr, "Kerberos continue Packet Received. proto_state=%d", cptr->proto_state);
    } else {
      NSDL2_AUTH(vptr, cptr, "Empty kerberos packet received when context is not yet complete .proto_state=%d, ", cptr->proto_state);
      HANDLE_INVALID_AUTH_MSG((u_ns_char_t *)kerberos_pkt, kerberos_pkt_len, 
          "Empty kerberos packet received when context is not yet complete");
    }
  } else {  //first 401 response for kerberos
    cptr->flags |= NS_CPTR_RESP_FRM_AUTH_KERBEROS;
  }
  NSDL2_AUTH(vptr, cptr, "Exiting Method. proto_state=%d", cptr->proto_state);
}

static void auth_headers_parse_set_value(char *auth_hdr, int auth_hdr_len, connection *cptr)
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  //HTTPData_t *httpdata = vptr->httpData;
  char *auth_pkt; 
  int auth_pkt_len;

  NSDL2_AUTH(vptr, cptr, "Method Called. auth_hdr=[%s], auth_hdr_len=%d", auth_hdr, auth_hdr_len);

  CLEAR_WHITE_SPACE(auth_hdr);
  CLEAR_WHITE_SPACE_FROM_END(auth_hdr);  

  if(strncasecmp(auth_hdr, "Negotiate", 9) == 0) 
  {
    //CHECK_OTHER_AUTH_AND_RETURN(NS_CPTR_RESP_FRM_AUTH_KERBEROS);
    auth_pkt = auth_hdr + 9;
    CLEAR_WHITE_SPACE(auth_pkt);
    auth_pkt_len = strlen(auth_pkt);
    NSDL1_AUTH(vptr, cptr, "WWW-Authenticate: 'Negotiate' received in header auth_pkt=[%s], auth_pkt_len=%d", auth_pkt, auth_pkt_len);
    auth_kerberos_headers_parse_set_value(auth_pkt, auth_pkt_len, cptr);
  } 
  else if (strncasecmp(auth_hdr, "BASIC", 5) == 0) 
  {
    //CHECK_OTHER_AUTH_AND_RETURN(NS_CPTR_RESP_FRM_AUTH_BASIC);
    auth_pkt = auth_hdr + 5;
    CLEAR_WHITE_SPACE(auth_pkt);
    auth_pkt_len = strlen(auth_pkt);
    NSDL1_AUTH(vptr, cptr, "WWW-Authenticate: 'BASIC' received in header auth_pkt=[%s], auth_pkt_len=%d", 
                                                           auth_pkt, auth_pkt_len);
    auth_basic_headers_parse_set_value(auth_pkt, auth_pkt_len, cptr);
  }
  else if(strncasecmp(auth_hdr, "DIGEST", 6) == 0) 
  {
    //CHECK_OTHER_AUTH_AND_RETURN(NS_CPTR_RESP_FRM_AUTH_DIGEST);
    auth_pkt = auth_hdr + 6;
    CLEAR_WHITE_SPACE(auth_pkt);
    auth_pkt_len = strlen(auth_pkt);
    NSDL1_AUTH(vptr, cptr, "WWW-Authenticate: 'DIGEST' received in header auth_pkt=[%s], auth_pkt_len=%d", 
                                                           auth_pkt, auth_pkt_len);
    auth_digest_headers_parse_set_value(auth_pkt, auth_pkt_len, cptr);
  }
  else if(strncasecmp(auth_hdr, "NTLM", 4) == 0) 
  {
    //CHECK_OTHER_AUTH_AND_RETURN(NS_CPTR_RESP_FRM_AUTH_NTLM);
    auth_pkt = auth_hdr + 4;
    CLEAR_WHITE_SPACE(auth_pkt);
    auth_pkt_len = strlen(auth_pkt);
    NSDL1_AUTH(vptr, cptr, "WWW-Authenticate: 'NTLM' received in header auth_pkt=[%s], auth_pkt_len=%d", 
                                                           auth_pkt, auth_pkt_len);
    auth_ntlm_headers_parse_set_value(auth_pkt, auth_pkt_len, cptr);
  }
 else
  {
    NSDL1_AUTH(vptr, cptr, "Ignoring as unhandled protocol received with header WWW-Authenticate.");
    cptr->header_state = HDST_TEXT; // Complete line processed. Set state to CR to parse next header line
    return;
  }
}

/*************************************************************************************************
Description       : This method will extract Authenticate header from the response and store
                    the packet, if received in HTTPData, if required
                    It uses memchr() for \r to find the end of the packet
                    It will handle all cases:
                    1.If end of header (\r) received
                      Case 1 [No earlier and complete now (Complete in one go)]
                        - Parse Authenticate header value and add it in HTTPData, in case ntlm packet received
                      Case 2 [Partial earlier, complete now]
                        – Realloc buffer
                        - Append with existing Authenticate:NTLM header value.
                        - Parse Authenticate header value and add it in HTTPData, in case ntlm packet received
                    Set state HANDLE_READ_NL_CR
                    2.If end of header (\r) not received
                      Case 3 [No earlier and partial now (Not complete in one read. It will go to next read)]
                       – Realloc buffer
                       - Save Authenticate packet.
                      Case 4 [Partial earlier, partial now]
                       - Realloc buffer
                       - Append with previous
                       - Save Authenticate packet.

Input Parameters  :
     header_buffer: Pointer in the received buffer for the Authenticate Header
 header_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response
   consumed_bytes : Number of bytes consumed in the header
             now  : current time stamp

Return Value      : None
*************************************************************************************************/


inline void parse_authenticate_hdr(connection *cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now)
{
  char *header_end;
  int length;
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);
 
  if(cptr->http_protocol == HTTP_MODE_HTTP2)
  {
    if(ntlm->partial_hdr == NULL)
    {
      NSDL3_AUTH(vptr, cptr, "Complete Authenticate NTLM header line received in one read");
      // +1 in lenght is so that we send NULL termination also
      auth_headers_parse_set_value(header_buffer, header_buffer_len, cptr);
    }
  } 
  else
  {
    // Check if end of auth ntlm header line is received
    // if((header_end = memchr(header_buffer, "\r\n", header_buffer_len)))
    // Issue - memchr only take one char. So we are only checking \r.
    // Also if header is terminated by \n only, then this will not work
    if((header_end = memchr(header_buffer, '\r', header_buffer_len)))
    {
      length = header_end - header_buffer; // Length is without \r
      //A. Replacing \r with \0 to identify the end-of-the-header-line
      //for facilitating the string search and string copy functions,
      //as they operate on \0
      header_buffer[length] = 0;

      // Case 1 - Complete Authenticate header line received in one read
      if(ntlm->partial_hdr == NULL)
      {
        NSDL3_AUTH(vptr, cptr, "Complete Authenticate NTLM header line received in one read");
        // +1 in lenght is so that we send NULL termination also
        auth_headers_parse_set_value(header_buffer, length, cptr);
      }
      // Case 2 - Complete Authenticate header line received now and was partially received earlier
      else
      {
        NSDL3_AUTH(vptr, cptr, "Complete Authenticate NTLM header line received now and it was partial earlier");
        auth_save_partial_headers(header_buffer, cptr, length);
        auth_headers_parse_set_value(ntlm->partial_hdr, ntlm->hdr_len, cptr);
        auth_free_partial_hdr(cptr);
      }
      //*consumed_bytes = length + 1;   // add 1 as we have checked till /r
      *consumed_bytes = length;   // add 1 as we have checked till /r
      cptr->header_state = HDST_CR; // Complete set-auth ntlm header line processed. Set state to CR to parse next header line
      //Reverting \0 back with \r for reasons mentioned in (A) above
      header_buffer[length] = '\r';
    }
    else
    {
      // Case 3 - Parital auth ntlm header line received first time
      if(ntlm->partial_hdr == NULL)
      {
        NSDL3_AUTH(vptr, cptr, "Partial Authenticate NTLM header line received first time");
      }
      // Case 4 - Parital auth ntlm header line received and was partially received earlier
      else
      {
        NSDL3_AUTH(vptr, cptr, "Partial Authenticate NTLM header line received and was partially received earlier");
      }

      auth_save_partial_headers(header_buffer, cptr, header_buffer_len);
      *consumed_bytes = header_buffer_len; // All bytes are consumed
    }
  }
  NSDL2_AUTH(vptr, cptr, "Exiting Method, consumed_bytes = %d", *consumed_bytes);
}

/*************************************************************************************************
Description       : This method will extract Authenticate header from the response and store
                    the packet, if received in HTTPData, if required
                    It uses memchr() for \r to find the end of the packet
                    It will handle all cases:
                    1.If end of header (\r) received
                      Case 1 [No earlier and complete now (Complete in one go)]
                        - Parse Authenticate header value and add it in HTTPData, in case ntlm packet received
                      Case 2 [Partial earlier, complete now]
                        – Realloc buffer
                        - Append with existing Authenticate:NTLM header value.
                        - Parse Authenticate header value and add it in HTTPData, in case ntlm packet received
                    Set state HANDLE_READ_NL_CR
                    2.If end of header (\r) not received
                      Case 3 [No earlier and partial now (Not complete in one read. It will go to next read)]
                       – Realloc buffer
                       - Save Authenticate packet.
                      Case 4 [Partial earlier, partial now]
                       - Realloc buffer
                       - Append with previous
                       - Save Authenticate packet.

Input Parameters  :
     header_buffer: Pointer in the received buffer for the Authenticate Header
 header_buffer_len: Number of bytes available
              cptr: Connection pointer which is used for received response
   consumed_bytes : Number of bytes consumed in the header
             now  : current time stamp

Output Parameters : Set cptr->header_state
                    HANDLE_READ_NL_CR (set if get whole NTLM packet)

Return Value      : 0 on success
*************************************************************************************************/

int proc_http_hdr_auth_ntlm(connection *cptr, char *header_buffer, int header_buffer_len, 
                                             int *consumed_bytes, u_ns_ts_t now) 
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif

  *consumed_bytes = 0; // Set to 0 in case of header is to be ignored
  NSDL2_AUTH(vptr, cptr, "Method called, cptr->req_code = %d.", cptr->req_code); 
  if (cptr->req_code != 401)
  {
    //Complete Authenticate line processed. Ignoring Header
    //Set state to CR to parse next header line
    cptr->header_state = HDST_TEXT;     
    NSDL2_AUTH(vptr, cptr, "Ignoring 'WWW-Authenticate' Header as response code is not 401"); 
    return AUTH_SUCCESS; // Must return 0
  }  

  // Bug:36326| 401 response is recieved with header 'WWW-Authenticate', hence setting the flag on cptr
  cptr->flags |= NS_CPTR_AUTH_HDR_RCVD; 

  if(cptr->proto_state == ST_AUTH_HANDSHAKE_FAILURE)
  {
    // Some error occurred. So ignore header
    cptr->header_state = HDST_TEXT;     
    NSDL2_AUTH(vptr, cptr, "Ignoring 'WWW-Authentcate' Header as there was Authenticate handshake failed"); 
    return AUTH_SUCCESS; // Must return 0
  }
  parse_authenticate_hdr(cptr, header_buffer, header_buffer_len, consumed_bytes, now);
  return AUTH_SUCCESS;
}



//----------------HEADER PARSING SECTION ENDS ------------------------------------------


//----------------NTLM MESSAGE PROCESSING SECTION BEGINDS ------------------------------------
static int try_url_on_same_con (connection* cptr, action_request_Shr* cur_url, u_ns_ts_t now)
{
  //VUser *vptr;

  NSDL2_AUTH(NULL, cptr, "Method called: cptr = %p cur_url=%p at %u", cptr,cur_url, now);

  //vptr = cptr->vptr;

  if (cptr->conn_state == CNST_REUSE_CON) 
  {
    NSDL2_AUTH(NULL, cptr, "cptr->conn_state = %d, renewing connection", cptr->conn_state );
    renew_connection(cptr, now);
    return AUTH_SUCCESS;
  }
  
  NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Authentication Handshake Failure: Connection Error");

  if (cptr->conn_state == CNST_FREE )    /* This might have been allocated for reuse and think timer */
  {
    NSDL2_CONN(NULL, cptr, "Authentication Handshake Failure: cptr->conn_state = %d, freeing connection", cptr->conn_state );
    free_connection_slot(cptr, now);
  } 
  else 
  {
    NSDL2_SCHEDULE(NULL, cptr, "Authentication Handshake Failure: Connection is in wrong state %d", cptr->conn_state);
  }
  return AUTH_ERROR;
}

void get_usr_name_pwd(connection *cptr, char *auth_ntlm_uname, char *auth_ntlm_pwd, int proxy_chain)
{
  VUser *vptr = cptr->vptr;
  int size = 0;

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  //Fetch proxy credentials in following cases
  //1. Proxy authtentiction
  //2. To fetch previous proxy credentils used to send proxy authorization headers 
  //   along with server authorization headers in case of proxy-auth-chain
  if(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH || proxy_chain)
  {
    ProxyServerTable_Shr *proxy_ptr;
    proxy_ptr = runprof_table_shr_mem[vptr->group_num].proxy_ptr;
    if (proxy_ptr == NULL)
    {
      NSDL2_AUTH(NULL, cptr, "Proxy Not defined for group%d", vptr->group_num);
      return;
    }
    else
    {
      if(proxy_ptr->username) strcpy(auth_ntlm_uname, proxy_ptr->username);
      if(proxy_ptr->password) strcpy(auth_ntlm_pwd, proxy_ptr->password);
    }
    NSDL3_AUTH(vptr, cptr, "auth_ntlm_uname=%s, auth_ntlm_pwd=%s for group_num=%d",
                           auth_ntlm_uname, auth_ntlm_pwd, vptr->group_num);
    return;
  } 

  if (cptr->url_num->proto.http.auth_uname.num_entries == 1 && cptr->url_num->proto.http.auth_uname.seg_start->type == STR)
  {

    NSDL4_AUTH(vptr, cptr, "auth_uname: CASE 1 entry=1 and type STR (%d) ", 
                             cptr->url_num->proto.http.auth_uname.seg_start->type);

    size = cptr->url_num->proto.http.auth_uname.seg_start->seg_ptr.str_ptr->size;
    strncpy(auth_ntlm_uname, cptr->url_num->proto.http.auth_uname.seg_start->seg_ptr.str_ptr->big_buf_pointer, size);
    auth_ntlm_uname[size] = '\0';
  } 
  else if (cptr->url_num->proto.http.auth_uname.num_entries >= 1)
  {
      /* In case it is parameterized using {<paramname>} in script file, num_entries is always 2 (VAR and STR) */
    NSDL4_AUTH(vptr, cptr, "auth_uname: CASE 2 entries=%d and type=%d", 
                              cptr->url_num->proto.http.auth_uname.num_entries,
                              cptr->url_num->proto.http.auth_uname.seg_start->type);

    if(get_full_element(cptr->vptr, &cptr->url_num->proto.http.auth_uname, auth_ntlm_uname, &size) < 0)
      return;
    auth_ntlm_uname[size]='\0';
  }

  if (cptr->url_num->proto.http.auth_pwd.num_entries == 1 && cptr->url_num->proto.http.auth_pwd.seg_start->type == STR)
  {
    NSDL4_AUTH(vptr, cptr, "auth_pwd: CASE 1 entry=1 and type =STR(%d)", 
                              cptr->url_num->proto.http.auth_pwd.seg_start->type);

    size = cptr->url_num->proto.http.auth_pwd.seg_start->seg_ptr.str_ptr->size;
    strncpy(auth_ntlm_pwd, cptr->url_num->proto.http.auth_pwd.seg_start->seg_ptr.str_ptr->big_buf_pointer, size);
    auth_ntlm_pwd[size] = '\0';
  } 
  else if (cptr->url_num->proto.http.auth_pwd.num_entries >= 1)
  {
    /* In case it is parameterized using {<paramname>} in script file, num_entries is always 2 (VAR and STR) */
    NSDL4_AUTH(vptr, cptr, "auth_pwd: CASE 2 entries=%d and type %d", 
                            cptr->url_num->proto.http.auth_pwd.num_entries, 
                              cptr->url_num->proto.http.auth_pwd.seg_start->type);

    if(get_full_element(cptr->vptr, &cptr->url_num->proto.http.auth_pwd, auth_ntlm_pwd, &size) < 0)
      return;
    auth_ntlm_pwd[size]='\0';
  }

  NSDL3_AUTH(vptr, cptr, "auth_ntlm_uname=%s, auth_ntlm_pwd=%s",
                           auth_ntlm_uname, auth_ntlm_pwd);
}

int auth_kerberos_create_authorization_hdr(connection *cptr, int grp_idx, u_ns_char_t *kerb_msg, int *kerb_msg_len)
{
  int fill_hdr = 0;
  //u_ns_4B_t flags = 0; 
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;

  //int debug_fd = -1;
  //FILE *debug_fp = NULL;
  gss_name_t server_name, name;
  gss_cred_usage_t cred_usage;
  gss_OID_set mechs; 
  SvrTableEntry_Shr* svr_ptr_current;
  char *host;
  gss_ctx_id_t context = GSS_C_NO_CONTEXT;
  OM_uint32  minor_status, major_status, lifetime;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  int ret;
  struct sockaddr_in sa, *sap;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  if (cptr->proto_state == ST_AUTH_KERBEROS_RCVD) {
    //emtpy
  } else if (cptr->proto_state == ST_AUTH_KERBEROS_CONTINUE) {
    input_token.value  = httpdata->ntlm.challenge;
    input_token.length  = httpdata->ntlm.hdr_len;
  } else {
    NSDL3_AUTH(vptr, cptr, "entered routine in wrong state %d",cptr->proto_state);
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__, 
        "entered routine in wrong state %d",cptr->proto_state);
    return(-1);
  }
  memset((void*)kerb_msg, 0, MAX_NTLM_PKT_SIZE);

  svr_ptr_current = vptr->cur_page->first_eurl->index.svr_ptr;
  host = vptr->ustable[svr_ptr_current->idx].svr_ptr->server_name;
#if 0
  {
    char *p;
    if ( (p = strchr(host, ':')) != NULL) {
      *p =0;
    }
    if ( (inet_pton (AF_INET, host, &sa.sin_addr)) == -1) {
      NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
          __FILE__, (char*)__FUNCTION__,
          "error getting in_addr for %s",host);
      return(-1);
    }
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(p+1));
    sap = &sa;
  }
#else
  sap = (struct sockaddr_in*) &(vptr->ustable[svr_ptr_current->idx].svr_ptr->saddr);

#endif
  memset(hbuf, 0, sizeof(hbuf));
  if  (getnameinfo((struct sockaddr*)sap, sizeof(sa), hbuf, sizeof(hbuf), sbuf,
        sizeof(sbuf), NI_NAMEREQD) < 0) {
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "error getting hostname for %s port %d",host, sap->sin_port);
    return(-1);
  }

  NSDL3_AUTH(vptr, cptr, "host name before get_gss_name %s\n",hbuf);


  if ( (ret = get_gss_name(hbuf, &server_name)) ) {
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__, 
        "error converting hostname %s to gss format", hbuf);
    return(-1); 
  }

  //error out if we dont have right credentials before we init the context
  if ( (major_status = gss_inquire_cred(&minor_status, GSS_C_NO_CREDENTIAL, &name, &lifetime, &cred_usage, &mechs)) != GSS_S_COMPLETE) {
    fprintf(stderr, "gss_inquire_cred failed: status 0x%08x possibly we dont have credentials (check debug log for details)\n", major_status);
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__, 
        "gss_inquire_cred() failed: returned major 0x%08x minor 0x%08x",major_status, minor_status);
    log_gss_error(major_status, GSS_MAJOR, "gss_inquire_cred() failed");
    log_gss_error(minor_status, GSS_MINOR, NULL);
    return(-1); 
  }


  if  ( (major_status = GSSInitSecContext(server_name, &context, &minor_status, &input_token, &output_token)) ==-1 ) {
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__, 
        "GSSInitSecContext returned major 0x%08x minor 0x%08x",major_status, minor_status);
    if(server_name != GSS_C_NO_NAME)
      gss_release_name(&minor_status, &server_name);
    return(-1); 
  }
  if (major_status &  GSS_S_CONTINUE_NEEDED) {
    NSDL3_AUTH(vptr, cptr, "major_status 0x%08x has GSS_S_CONTINUE_NEEDED", major_status);
  }

  //copy the token into our buffer
  memcpy(kerb_msg, output_token.value, output_token.length);
  *kerb_msg_len = output_token.length;

  if(output_token.length != 0)
    gss_release_buffer(&minor_status, &output_token);

  fill_hdr = 1;
  return fill_hdr;
}

//To get Basic Authorization headers
int auth_basic_create_authorization_hdr(connection *cptr, int grp_idx, u_ns_char_t *basic_msg, int *basic_msg_len, int proxy_chain)
{
  int fill_hdr = 0;
  //u_ns_4B_t flags = 0; 
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif
  //HTTPData_t *httpdata = vptr->httpData;
  //ntlm_t *ntlm = &(httpdata->ntlm);
  //int debug_fd = -1;
  //FILE *debug_fp = NULL;
  char auth_basic_uname[1024] = "netstorm";
  char auth_basic_pwd[1024] = "netstorm";

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  //SS: Commented for proxy-chaining
  //if (cptr->proto_state == ST_AUTH_BASIC_RCVD)
  {
    NSDL4_AUTH(vptr, cptr, "auth_uname.num_entries=%d, auth_pwd.num_entries=%d", 
                            cptr->url_num->proto.http.auth_uname.num_entries,
                            cptr->url_num->proto.http.auth_pwd.num_entries);
    get_usr_name_pwd(cptr, auth_basic_uname, auth_basic_pwd, proxy_chain);
    NSDL3_AUTH(vptr, cptr, "auth_basic_uname=%s, auth_basic_pwd=%s",
                           auth_basic_uname, auth_basic_pwd);

    //do a encode of  <username>:<password>
    memset((void*)basic_msg, 0, MAX_NTLM_PKT_SIZE);
    sprintf((char*)basic_msg,"%s:%s",auth_basic_uname, auth_basic_pwd);

    *basic_msg_len = strlen((char *)basic_msg);
    NSDL3_AUTH(vptr, cptr, "Basic Packet = %s, Len=%d",basic_msg, *basic_msg_len);

    fill_hdr = 1;
  }
  return fill_hdr;
}


int auth_digest_create_authorization_hdr(connection *cptr, int grp_idx, u_ns_char_t *digest_msg, int *digest_msg_len, int proxy_chain)
{
  int fill_hdr = 0;
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);
  //int debug_fd = -1;
  //FILE *debug_fp = NULL;
  char auth_digest_uname[1024] = "netstorm";
  char auth_digest_pwd[1024] = "netstorm";
  char method_str[16];

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  //SS: Commented for proxy-chaining
  //if (cptr->proto_state == ST_AUTH_DIGEST_RCVD)
  {
    NSDL4_AUTH(vptr, cptr, "auth_uname.num_entries=%d, auth_pwd.num_entries=%d", 
                            cptr->url_num->proto.http.auth_uname.num_entries,
                            cptr->url_num->proto.http.auth_pwd.num_entries);

    get_usr_name_pwd(cptr, auth_digest_uname, auth_digest_pwd, proxy_chain);
    
    memset(digest_msg, 0, MAX_NTLM_PKT_SIZE);
    NSDL3_AUTH(vptr, cptr, "Digest challenge [%s], Len=%d", ntlm->challenge, ntlm->hdr_len);
    grp_idx = vptr->group_num;
    //runprof_table_shr_mem[grp_idx].proto.http.http_method;
    int method_idx = cptr->url_num->proto.http.http_method_idx;
    NSDL3_AUTH(vptr, cptr, "digest Request Method - [%s]", http_method_table_shr_mem[method_idx].method_name);
    strncpy(method_str, http_method_table_shr_mem[method_idx].method_name, strlen( http_method_table_shr_mem[method_idx].method_name) -1);

    //Getting proxy digest msg in case of proxy-chain, required to be send with server auth header
    if(proxy_chain && cptr->prev_digest_msg != NULL)
    {
      strcpy((char *)digest_msg, (char *)cptr->prev_digest_msg);
      *digest_msg_len = cptr->prev_digest_len;
      NSDL2_AUTH(vptr, cptr, "Prev (proxy) Digest msg=%s, Len=%d", digest_msg, *digest_msg_len);
    }    
    else
    {
      if(MakeDigestResponse ((u_ns_char_t *)ntlm->challenge, cptr->url, method_str, auth_digest_uname, auth_digest_pwd, "", 0 , digest_msg))
      {
        NSDL2_AUTH(vptr, cptr, "Error in creating digest authorization request for url=[%s]", cptr->url);
        NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
          __FILE__, (char*)__FUNCTION__, "Handshake Failure: Error in Creating digest authorization request for url=%s", cptr->url);
        return AUTH_ERROR;
      }
      FREE_AND_MAKE_NULL_EX(ntlm->challenge, ntlm->hdr_len, "HTTP AUTH Digset Msg", -1);
      *digest_msg_len = strlen((char *)digest_msg);
      NSDL2_AUTH(vptr, cptr, "Digest msg=%s, Len=%d", digest_msg, digest_msg_len);

      if(cptr->prev_digest_msg == NULL)
      {
        MY_MALLOC_AND_MEMSET(cptr->prev_digest_msg, *digest_msg_len + 1, "HTTP AUTH DIGEST Prev. Msg", -1);
        memcpy(cptr->prev_digest_msg, digest_msg, *digest_msg_len);
        cptr->prev_digest_msg[*digest_msg_len] = '\0';
        cptr->prev_digest_len = *digest_msg_len;
        NSDL2_AUTH(vptr, cptr, "Prev (proxy) Digest msg=%s, Len=%d", cptr->prev_digest_msg, cptr->prev_digest_len);
      }
    }
    
    NSDL3_AUTH(vptr, cptr, "Digest Packet, Len=%d", *digest_msg_len);

    fill_hdr = 1;
  }
  return fill_hdr;
}

/*************************************************************************************************
Description       : This method is used to 
                    1. create NTLM Type1/Type3 Message
                    2. Create Authorization Header

Input Parameters  :
              cptr: Connection pointer which is used for received response
            vector:
        free_array:

Output Parameters : body_start_idx

Return Value      : start_idx

Calling Method    : make_request()
*************************************************************************************************/


int auth_ntlm_create_authorization_hdr(connection *cptr, int grp_idx, u_ns_char_t *ntlm_msg, int *ntlm_msg_len)
{
  int fill_hdr = 0;
  VUser *vptr = cptr->vptr;
  HTTPData_t *httpdata = vptr->httpData;
  ntlm_t *ntlm = &(httpdata->ntlm);
  u_ns_4B_t flags = 0; 
  //int debug_fd = -1;
  //FILE *debug_fp = NULL;
  char auth_ntlm_uname[1024] = "netstorm";
  char auth_ntlm_pwd[1024] = "netstorm";

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);
  memset(ntlm_msg, 0, MAX_NTLM_PKT_SIZE);

  switch (cptr->proto_state)
  {
    case ST_AUTH_NTLM_RCVD: 
      //a. Create NTLM Type1 Msg 
      flags = (NTLM_NEGOTIATE_ALWAYS_SIGN|NTLM_NEGOTIATE_NTLM_KEY|NTLM_NEGOTIATE_UNICODE|NTLM_NEGOTIATE_OEM|NTLM_REQUEST_TARGET);

      if (runprof_table_shr_mem[grp_idx].gset.ntlm_settings.ntlm_version == NTLM_VER_NTLM2)
        flags |= NTLM_NEGOTIATE_NTLM2_KEY;

      if (runprof_table_shr_mem[grp_idx].gset.ntlm_settings.workstation[0])
        flags |= NTLM_NEGOTIATE_WKS_SUPPLIED;
      if (runprof_table_shr_mem[grp_idx].gset.ntlm_settings.domain[0])
        flags |= NTLM_NEGOTIATE_DOMAIN_SUPPLIED;

        buildSmbNtlmAuthRequest((tSmbNtlmAuthRequest*)ntlm_msg, runprof_table_shr_mem[grp_idx].gset.ntlm_settings.workstation, runprof_table_shr_mem[grp_idx].gset.ntlm_settings.domain, flags);

      *ntlm_msg_len = SmbLength((tSmbNtlmAuthRequest*)ntlm_msg);
      NSDL2_AUTH(vptr, cptr, "NTLM Type1 Packet, Len=%d", *ntlm_msg_len);

      fill_hdr = 1;
      break;
      
     case ST_AUTH_NTLM_TYPE2_RCVD:

       NSDL4_AUTH(vptr, cptr, "auth_uname.num_entries=%d, auth_pwd.num_entries=%d", 
                            cptr->url_num->proto.http.auth_uname.num_entries,
                            cptr->url_num->proto.http.auth_pwd.num_entries);

        get_usr_name_pwd(cptr, auth_ntlm_uname, auth_ntlm_pwd, !PROXY_CHAIN);
        NSDL3_AUTH(vptr, cptr, "auth_ntlm_uname=%s, auth_ntlm_pwd=%s",
                           auth_ntlm_uname, auth_ntlm_pwd);

        int ver = 0;
        if (runprof_table_shr_mem[grp_idx].gset.ntlm_settings.ntlm_version == NTLM_VER_NTLMv2)
          ver = NTLM_VER_NTLMv2;
        //Create NTLM Type3 Msg
        buildSmbNtlmAuthResponse((tSmbNtlmAuthChallenge*)ntlm->challenge,
                              (tSmbNtlmAuthResponse*)ntlm_msg,
                              auth_ntlm_uname, auth_ntlm_pwd, ver);
    

        FREE_AND_MAKE_NULL_EX(ntlm->challenge, ntlm->hdr_len, "HTTP AUTH NTLM - Type2 Msg", -1);

        *ntlm_msg_len = SmbLength((tSmbNtlmAuthResponse*)ntlm_msg);
        NSDL2_AUTH(vptr, cptr, "NTLM Type3 Packet, Len=%d", *ntlm_msg_len);

        fill_hdr = 1;
        break;
   }
   return fill_hdr;
}

//For handling creation of previous proxy authentcation headers. 
//Current suppport for - BASIC & DIGEST protocol only
int auth_get_proxy_authorization_hdr_http2(connection *cptr, int grp_idx, int push_flag)
{  
  u_ns_char_t auth_msg[MAX_NTLM_PKT_SIZE + 1]; 
  u_ns_char_t auth_pkt_encoded[MAX_NTLM_PKT_SIZE + 1];
  char *auth_protocol;
  int auth_msg_len = 0, auth_protocol_len = 0;
  int fill_hdr = 0;
  char *hdr_name = NULL;
  int hdr_len = 0;
  char *hdr_value = NULL;
  int hdr_value_len = 0;
  int auth_pkt_encoded_len = 0;
  int digest_b64_encoded = 0;

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s push_flag = %d", cptr->url, push_flag);
  if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC)
  {
    NSDL2_AUTH(NULL, cptr, "Fetching previous proxy autorization headers (BASIC) in case of proxy-auth-chain");
    fill_hdr = auth_basic_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, PROXY_CHAIN);  
    auth_protocol = auth_basic;
    auth_protocol_len = auth_basic_len;
  }
  else if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Fetching previous proxy autorization headers (DIGEST) in case of proxy-auth-chain");
    fill_hdr = auth_digest_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, PROXY_CHAIN);
    auth_protocol = auth_digest;
    auth_protocol_len = auth_digest_len;
    NSDL2_AUTH(NULL, cptr, "auth_protocol_len=%d", auth_protocol_len);
  }
  else
  {
    NSDL2_AUTH(NULL, cptr, "Protocol not supproted for authentication chaining.");
    return AUTH_ERROR;
  }

  NSDL2_AUTH(NULL, cptr, "fill_hdr = %d, auth_msg = [%s] auth_msg_len=%d", fill_hdr, auth_msg, auth_msg_len);
  if(fill_hdr == AUTH_ERROR)
    return AUTH_ERROR;
   
   //Add Additional header in case of Auth:NTLM Type1 & Type3 Msg is required to send
   if(fill_hdr)
   {
     NSDL2_AUTH(NULL, cptr, "auth_protocol = [%s%s], auth_protocol_len = %d, authorization_hdr_len=%d", authorization_proxy_hdr, auth_protocol, auth_protocol_len, authorization_proxy_hdr_len);
       
     hdr_name = authorization_proxy_hdr_http2;
     MY_MALLOC(hdr_value, MAX_NTLM_PKT_SIZE + auth_protocol_len, "Proxy-authorization header value", -1);
     strncpy(hdr_value, auth_protocol, auth_protocol_len);
     hdr_value_len = auth_protocol_len;

     //Packets which get encoded in b64
     if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC)
     {
       // TODO - Optimize size of this malloc 
       to64frombits(auth_pkt_encoded, auth_msg, auth_msg_len);
       auth_pkt_encoded_len = strlen((char *)auth_pkt_encoded);
       digest_b64_encoded = 1;
       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", strlen((char *)auth_pkt_encoded), (char *)auth_pkt_encoded);
     }
     else
     {
       // Digest packets are not encoded in b64
       auth_pkt_encoded_len = auth_msg_len;
       NSDL2_AUTH(NULL, cptr, "Digest packets are not encoded in b64");
     }
     if(auth_pkt_encoded_len > MAX_NTLM_PKT_SIZE)
       auth_pkt_encoded_len = MAX_NTLM_PKT_SIZE;
     
     if(digest_b64_encoded) 
       strncpy(hdr_value + auth_protocol_len, (char *)auth_pkt_encoded, auth_pkt_encoded_len);
     else 
       strncpy(hdr_value + auth_protocol_len, (char *)auth_msg, auth_pkt_encoded_len);
     
     hdr_value_len += auth_pkt_encoded_len;
     if(!push_flag){
       FILL_HEADERS_IN_NGHTTP2(hdr_name, hdr_len, hdr_value, hdr_value_len, 0, 1);
     }
     else
     {
       LOG_PUSHED_REQUESTS(hdr_name, hdr_len, hdr_value, hdr_value_len);
       /*bug 86575: release hdr_value in case of server_push*/
       NSDL2_AUTH(NULL, cptr, "releasing hdr_value = %p", hdr_value);
       FREE_AND_MAKE_NULL(hdr_value, "Proxy-authorization header value", -1);
     }
   }
   return 0;
}

//For handling creation of previous proxy authentcation headers. 
//Current suppport for - BASIC & DIGEST protocol only
int auth_get_proxy_authorization_hdr(connection *cptr, int *body_start_idx, int grp_idx)
{  
  u_ns_char_t auth_msg[MAX_NTLM_PKT_SIZE]; 
  u_ns_char_t *auth_pkt_encoded;
  char *auth_protocol;
  int auth_msg_len = 0, auth_protocol_len = 0;
  int fill_hdr = 0;


  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);
  if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC)
  {
    NSDL2_AUTH(NULL, cptr, "Fetching previous proxy autorization headers (BASIC) in case of proxy-auth-chain");
    fill_hdr = auth_basic_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, PROXY_CHAIN);  
    auth_protocol = auth_basic;
    auth_protocol_len = auth_basic_len;
  }
  else if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    NSDL2_AUTH(NULL, cptr, "Fetching previous proxy autorization headers (DIGEST) in case of proxy-auth-chain");
    fill_hdr = auth_digest_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, PROXY_CHAIN);
    auth_protocol = auth_digest;
    auth_protocol_len = auth_digest_len;
    NSDL2_AUTH(NULL, cptr, "auth_protocol_len=%d", auth_protocol_len);
  }
  else
  {
    NSDL2_AUTH(NULL, cptr, "Protocol not supproted for authentication chaining.");
    return AUTH_ERROR;
  }

  NSDL2_AUTH(NULL, cptr, "fill_hdr = %d, auth_msg = [%s] auth_msg_len=%d", fill_hdr, auth_msg, auth_msg_len);
  if(fill_hdr == AUTH_ERROR)
    return AUTH_ERROR;
   
   //Add Additional header in case of Auth:NTLM Type1 & Type3 Msg is required to send
   if(fill_hdr)
   {
     NSDL2_AUTH(NULL, cptr, "auth_protocol = [%s%s], auth_protocol_len = %d, authorization_hdr_len=%d", authorization_proxy_hdr, auth_protocol, auth_protocol_len, authorization_proxy_hdr_len);

     NS_FILL_IOVEC(g_req_rep_io_vector, (char *) authorization_proxy_hdr, authorization_proxy_hdr_len);

     NS_FILL_IOVEC(g_req_rep_io_vector, (char *)auth_protocol, auth_protocol_len);

     //Packets which get encoded in b64
     MY_MALLOC(auth_pkt_encoded, MAX_NTLM_PKT_SIZE, "AUTH Pkt", -1);
     if(cptr->proxy_auth_proto == NS_CPTR_RESP_FRM_AUTH_BASIC)
     {
       // TODO - Optimize size of this malloc 
       to64frombits(auth_pkt_encoded, auth_msg, auth_msg_len);

       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", strlen((char *)auth_pkt_encoded), (char *)auth_pkt_encoded);
       NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, (char *)auth_pkt_encoded, strlen((char *)auth_pkt_encoded));
     }
     else
     {
       //Digest packets are not encoded in b64
       strcpy((char *)auth_pkt_encoded, (char *)auth_msg);
       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", auth_msg_len, auth_msg);
       NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, (char *)auth_pkt_encoded, auth_msg_len);
     }

     NS_FILL_IOVEC(g_req_rep_io_vector, (char *)hdr_end, hdr_end_len);

     *body_start_idx = *body_start_idx + 4;
   }
   return g_req_rep_io_vector.cur_idx;
}

int auth_get_authorization_hdr_http2(connection *cptr, int grp_idx, int push_flag)
{  
  u_ns_char_t auth_msg[MAX_NTLM_PKT_SIZE + 1]; 
  u_ns_char_t auth_pkt_encoded[MAX_NTLM_PKT_SIZE + 1];  //Packets which get encoded in b64
  char *auth_protocol;
  int auth_msg_len = 0, auth_protocol_len = 0;
  int fill_hdr = 0;
  char *hdr_name = NULL;
  int hdr_len = 0;
  char *hdr_value = NULL;
  int hdr_value_len = 0;
  int auth_pkt_encoded_len = 0;
  int digest_b64_encoded = 0;

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s push_flag = %d", cptr->url, push_flag);
  if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
  {
    fill_hdr = auth_basic_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, !PROXY_CHAIN);
    auth_protocol = auth_basic;
    auth_protocol_len = auth_basic_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    fill_hdr = auth_digest_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, !PROXY_CHAIN);
    auth_protocol = auth_digest;
    auth_protocol_len = auth_digest_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)
  {
    fill_hdr = auth_ntlm_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len);
    auth_protocol = auth_ntlm;
    auth_protocol_len = auth_ntlm_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
  {
    fill_hdr = auth_kerberos_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len);
    auth_protocol = auth_negotiate;
    auth_protocol_len = auth_negotiate_len;
  }
  else {
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "no auth flags set. cptr->flags 0x%x",cptr->flags);
    return AUTH_ERROR;
  }

  NSDL2_AUTH(NULL, cptr, "fill_hdr = %d, auth_msg = [%s] auth_msg_len=%d", fill_hdr, auth_msg, auth_msg_len);
  if(fill_hdr == AUTH_ERROR)
    return AUTH_ERROR;
   
   //Add Additional header in case of Auth:NTLM Type1 & Type3 Msg is required to send
   if(fill_hdr)
   {
     NSDL2_AUTH(NULL, cptr, "auth_protocol = [%s%s], auth_protocol_len = %d, authorization_hdr_len=%d",  authorization_hdr_http2, auth_protocol, auth_protocol_len, authorization_hdr_http2_len);

     if(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)
     {
       hdr_name = authorization_proxy_hdr_http2;
       hdr_len = authorization_proxy_hdr_http2_len;
     }
     else
     { 
       hdr_name = authorization_hdr_http2;
       hdr_len = authorization_hdr_http2_len;
     }
     MY_MALLOC(hdr_value, auth_protocol_len + MAX_NTLM_PKT_SIZE + 1, "Authorization header value", -1);
     strncpy(hdr_value, auth_protocol, auth_protocol_len);
     hdr_value_len = auth_protocol_len;

     if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM || cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
     {
       // TODO - Optimize size of this malloc 
       to64frombits(auth_pkt_encoded, auth_msg, auth_msg_len);
       auth_pkt_encoded_len = strlen((char *)auth_pkt_encoded);
       digest_b64_encoded = 1;

       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", auth_pkt_encoded_len, (char *)auth_pkt_encoded);
     } else {
       //Digest packets are not encoded in b64
       NSDL2_AUTH(NULL, cptr, "Digest packets are not encoded in b64");
       auth_pkt_encoded_len = auth_msg_len;
     }

     if (auth_pkt_encoded_len > MAX_NTLM_PKT_SIZE)
       auth_pkt_encoded_len = MAX_NTLM_PKT_SIZE;

     if(digest_b64_encoded)
       strncpy(hdr_value + auth_protocol_len, (char *)auth_pkt_encoded, auth_pkt_encoded_len);
     else 
       strncpy(hdr_value + auth_protocol_len, (char *)auth_msg, auth_pkt_encoded_len);
      
     hdr_value_len += auth_pkt_encoded_len;
     if(!push_flag){
       FILL_HEADERS_IN_NGHTTP2(hdr_name, hdr_len, hdr_value, hdr_value_len, 0, 1);
     }
     else
     {
       LOG_PUSHED_REQUESTS(hdr_name, hdr_len, hdr_value, hdr_value_len);
       /*bug 86575: release hdr_value in case of server_push*/
       NSDL2_AUTH(NULL, cptr, "releasing hdr_value = %p", hdr_value);
       FREE_AND_MAKE_NULL(hdr_value, "Authorization header value", -1);
     }
   }
   return 0;
}

int auth_get_authorization_hdr(connection *cptr, int *body_start_idx, int grp_idx)
{  
  u_ns_char_t auth_msg[MAX_NTLM_PKT_SIZE]; 
  u_ns_char_t *auth_pkt_encoded;
  char *auth_protocol;
  int auth_msg_len = 0, auth_protocol_len = 0;
  int fill_hdr = 0;

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);
  if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
  {
    fill_hdr = auth_basic_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, !PROXY_CHAIN);
    auth_protocol = auth_basic;
    auth_protocol_len = auth_basic_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
  {
    fill_hdr = auth_digest_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len, !PROXY_CHAIN);
    auth_protocol = auth_digest;
    auth_protocol_len = auth_digest_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)
  {
    fill_hdr = auth_ntlm_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len);
    auth_protocol = auth_ntlm;
    auth_protocol_len = auth_ntlm_len;
  }
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
  {
    fill_hdr = auth_kerberos_create_authorization_hdr(cptr, grp_idx, auth_msg, &auth_msg_len);
    auth_protocol = auth_negotiate;
    auth_protocol_len = auth_negotiate_len;
  }
  else {
    NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
        __FILE__, (char*)__FUNCTION__,
        "no auth flags set. cptr->flags 0x%x",cptr->flags);
    return AUTH_ERROR;
  }

  NSDL2_AUTH(NULL, cptr, "fill_hdr = %d, auth_msg = [%s] auth_msg_len=%d", fill_hdr, auth_msg, auth_msg_len);
  if(fill_hdr == AUTH_ERROR)
    return AUTH_ERROR;
   
   //Add Additional header in case of Auth:NTLM Type1 & Type3 Msg is required to send
   if(fill_hdr)
   {
     NSDL2_AUTH(NULL, cptr, "auth_protocol = [%s%s], auth_protocol_len = %d, authorization_hdr_len=%d", authorization_hdr, auth_protocol, auth_protocol_len, authorization_hdr_len);

     if(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)
     {
       NS_FILL_IOVEC(g_req_rep_io_vector, (char *) authorization_proxy_hdr, authorization_proxy_hdr_len);
     }
     else
     { 
       NS_FILL_IOVEC(g_req_rep_io_vector, (char *)authorization_hdr, authorization_hdr_len);
     }

     NS_FILL_IOVEC(g_req_rep_io_vector, (char *)auth_protocol, auth_protocol_len);

     //Packets which get encoded in b64
     MY_MALLOC(auth_pkt_encoded, MAX_NTLM_PKT_SIZE + 1, "AUTH Pkt", -1);

     if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC || cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM || cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
     {
       // TODO - Optimize size of this malloc 
       to64frombits(auth_pkt_encoded, auth_msg, auth_msg_len);

       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", strlen((char *)auth_pkt_encoded), (char *)auth_pkt_encoded);
       NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, auth_pkt_encoded, strlen((char *)auth_pkt_encoded));
     }
     else
     {
       //Digest packets are not encoded in b64
       NSDL2_AUTH(NULL, cptr, "Auth Packet (len = %d) = %s", auth_msg_len, auth_msg);
       strcpy((char *)auth_pkt_encoded, (char *)auth_msg);
       
       NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, auth_pkt_encoded, auth_msg_len);
     }

     NS_FILL_IOVEC(g_req_rep_io_vector, (char *)hdr_end, hdr_end_len);

     *body_start_idx = *body_start_idx + 4;
   }
   return g_req_rep_io_vector.cur_idx;
}

int check_and_set_single_auth(connection *cptr, int grp_idx)
{
  NSDL2_AUTH(cptr->vptr, cptr, "Method called: req_code = %d proto_state 0x%x flags 0x%x", cptr->req_code, cptr->proto_state, cptr->flags);
  //make decision on which auth to use - only the very first time
  if ( (cptr->flags & NS_CPTR_AUTH_TYPE_FIXED) == 0) {
    CHECK_AND_SET_SINGLE_AUTH() ;   //will leave only 1 auth set in the cptr flags
    //nothing is set - this is a fatal error
    if ( (cptr->flags & NS_CPTR_AUTH_MASK) == 0) {
      NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
          __FILE__, (char*)__FUNCTION__,
          "No auth flag is set after auth decision ");
      return AUTH_ERROR;
    }
    if (cptr->proto_state == 0) {
      NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
          __FILE__, (char*)__FUNCTION__,
          "No proto state set after auth decision ");
      return AUTH_ERROR;
    }
    NSDL2_AUTH(NULL, cptr, "Final auth flag after auth decision first time 0x%x (%s) proto_state 0x%x",cptr->flags, auth_to_string(cptr->flags), cptr->proto_state );
  } else {
    NSDL2_AUTH(NULL, cptr, "Verify auth flag on subsequent calls 0x%x (%s)",cptr->flags, auth_to_string(cptr->flags));
  }
  return AUTH_SUCCESS;  
}

//proxy_chain: Used to send the previous proxy authorization headers again in case of proxy-chain
//proxy_chain - 0: To send the current authorization headers
//              1: To send the previous proxy authorization headers
int auth_create_authorization_h2_hdr(connection *cptr, int grp_idx,  int proxy_chain, int push_flag)
{

  if(cptr == NULL)
    return AUTH_ERROR;

  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  if(check_and_set_single_auth(cptr, grp_idx) == AUTH_ERROR)
    return AUTH_ERROR;

  if (proxy_chain)
  {
    NSDL2_AUTH(NULL, cptr, "Adding proxy authorization headers in case of proxy-chaining");
    cptr->proxy_auth_proto = 0;
    return auth_get_proxy_authorization_hdr_http2(cptr, grp_idx, push_flag);
  }

  NSDL2_AUTH(NULL, cptr, "Adding authorization headers");
  return  auth_get_authorization_hdr_http2(cptr, grp_idx, push_flag);
}


//proxy_chain: Used to send the previous proxy authorization headers again in case of proxy-chain
//proxy_chain - 0: To send the current authorization headers
//              1: To send the previous proxy authorization headers
int auth_create_authorization_hdr(connection *cptr, int *body_start_idx, int grp_idx, int proxy_chain, int http2, int push_flag)
{
  NSDL2_AUTH(NULL, cptr, "Method called: url = %s", cptr->url);

  int ret;
  if(check_and_set_single_auth(cptr, grp_idx) == AUTH_ERROR)
  {
    return AUTH_ERROR;
  }

  if (proxy_chain)
  {
    NSDL2_AUTH(NULL, cptr, "Adding proxy authorization headers in case of proxy-chaining");
    if(!http2)
      ret = auth_get_proxy_authorization_hdr(cptr, body_start_idx, grp_idx);
    else {
      ret = auth_get_proxy_authorization_hdr_http2(cptr, grp_idx, push_flag); 
    }
    cptr->proxy_auth_proto = 0;
  }
  else
  {
    NSDL2_AUTH(NULL, cptr, "Adding authorization headers"); 
    if(!http2)
      ret = auth_get_authorization_hdr(cptr, body_start_idx, grp_idx);
    else{
      ret = auth_get_authorization_hdr_http2(cptr, grp_idx, push_flag);
    }
  }

  return ret;
}



/*************************************************************************************************
Description       : This method is used to create NTLM Type1 & Type2 Message and 

Input Parameters  :
              cptr: Connection pointer which is used for received response

Output Parameters : None

Return Value      : None

Calling Method    : http_close_connection()
*************************************************************************************************/

int auth_handle_response(connection *cptr, int status, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;

  NSDL2_AUTH(cptr->vptr, cptr, "Method called. status = %d", status); 
    
  //cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_NTLM; // Reset flag as we are done

  vptr->urls_awaited++; // Need to inccreament as it was decreamented before this method is called

  //send request on same connection
  if ((status != NS_REQUEST_OK) || try_url_on_same_con (cptr, cptr->url_num, now)) 
  {
    //In case we are not able to send request on same connection, return error
    NSDL2_AUTH(NULL, cptr, "Auth Handshake could not be completed on same connection"); 
    vptr->urls_awaited--; // Need to decreament as new request was not sent
    // Reset flag as we are done
    if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
    {
      cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_BASIC;     
      if(cptr->proto_state == ST_AUTH_BASIC_RCVD)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to Authorization: Basic not received from server. url=%s", cptr->url);
      }

    }
    else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
    {      
      cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_DIGEST; // Reset flag as we are done
      if(cptr->proto_state == ST_AUTH_DIGEST_RCVD)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to Authorization: Digest not received from server. url=%s", cptr->url);
      }
    }

    else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)
    {
      cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_NTLM; // Reset flag as we are done
      if(cptr->proto_state == ST_AUTH_NTLM_RCVD)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to T1 request not received from server. url=%s", cptr->url);
      }
      else if(cptr->proto_state == ST_AUTH_NTLM_TYPE2_RCVD)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to T3 request not received from server. url=%s", cptr->url);
      }
    }
    if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS)
    {
      cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_KERBEROS;     
      if(cptr->proto_state == ST_AUTH_KERBEROS_RCVD)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to Authorization: Kerberos not received from server. url=%s", cptr->url);
      }
      else if(cptr->proto_state == ST_AUTH_KERBEROS_CONTINUE)
      {
        NS_EL_2_ATTR(EID_AUTH_NTLM_CONN_ERR, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, 
              "Handshake Failure: Response to continued Authorization: Kerberos not received from server. url=%s", cptr->url);
      }
    }
    cptr->proto_state = 0;
    free_auth_data(cptr);
    return AUTH_ERROR; 
  }
  NSDL2_AUTH(NULL, cptr, "Auth packet sent OK");
  return AUTH_SUCCESS; 
}

// This is called for the checking response of NTLM T3 message
void auth_validate_handshake_complete(connection *cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_AUTH(cptr->vptr, cptr, "Method called. Checking respsonce code after final authentication message, req_code = %d proto_state 0x%x flags 0x%x", cptr->req_code, cptr->proto_state, cptr->flags); 

/*
  // Reset flag as we are done sending all request for NTLM authentication
  if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
    cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_BASIC;
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_NTLM)
    cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_NTLM;
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
    cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_DIGEST;
  else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
    cptr->flags = cptr->flags & ~NS_CPTR_RESP_FRM_AUTH_DIGEST;
*/
  
    //cptr->flags = cptr->flags & ~NS_CPTR_AUTH_MASK;
    //cptr->flags = cptr->flags & ~NS_CPTR_AUTH_TYPE_FIXED;

  //for kerberos, error out if we're in CONTINUE state only - this is the 2nd exchange
  if (cptr->flags & NS_CPTR_RESP_FRM_AUTH_KERBEROS) {
    if ((cptr->req_code == 401) || ((cptr->req_code == 407) && (cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH))) 
    {
      if  (cptr->proto_state  ==  ST_AUTH_KERBEROS_CONTINUE)  {
        cptr->proto_state = ST_AUTH_HANDSHAKE_FAILURE;
        NSDL2_AUTH(cptr->vptr, cptr, "Handshake Failure: Received 401/407 when expecting some success response code");
        NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
            __FILE__, (char*)__FUNCTION__, "Handshake Failure: Received 401/407 when expecting some success response code");
        cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 
      } else {
        //this may also be a failure, but we'll decide when the header is processed later
        NSDL2_AUTH(cptr->vptr, cptr, "Received status code 401/407 for kerberos state 0x%x. ignoring ..",cptr->proto_state);
      }
      return;
    } else if (cptr->req_code == 100) {
      NSDL2_AUTH(cptr->vptr, cptr, "Got status code 100 for kerberos state 0x%x. returning ..",cptr->proto_state);
      return;
    }
  }

  NSDL2_AUTH(cptr->vptr, cptr, "req_code=%d, cptr->flags:NS_CPTR_FLAGS_CON_PROXY_AUTH=0X%x", cptr->req_code, cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH);
  //When Server Authentication is received after Proxy Authentication
  //401 is received in Authenticate Req even if its successful
  if(cptr->req_code == 401 && (cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH))
  {
    NSDL2_AUTH(cptr->vptr, cptr, "Server Authentication followed Proxy Authentication. 401 received in response to 407 message");
    if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_BASIC)
    {
      cptr->proxy_auth_proto = NS_CPTR_RESP_FRM_AUTH_BASIC;
      NSDL2_AUTH(cptr->vptr, cptr, "Proxy Protocol BASIC set for authorization to be send with server. proxy_auth_proto=0x%x", cptr->proxy_auth_proto);
      cptr->proto_state = 0;
      INC_PROXY_AUTH_SUCC_COUNTERS(vptr);
    }
    else if(cptr->flags & NS_CPTR_RESP_FRM_AUTH_DIGEST)
    {
      cptr->proxy_auth_proto = NS_CPTR_RESP_FRM_AUTH_DIGEST;
      NSDL2_AUTH(cptr->vptr, cptr, "Proxy Protocol DIGEST set for authorization to be send with server. proxy_auth_proto=0x%x", cptr->proxy_auth_proto);
      cptr->proto_state = 0;
      INC_PROXY_AUTH_SUCC_COUNTERS(vptr);
    }
    else
    {
      NSDL2_AUTH(cptr->vptr, cptr, "Proxy chaining not supported for protocols other then BASIC & DIGEST");
      cptr->proto_state = ST_AUTH_HANDSHAKE_FAILURE;
      INC_PROXY_AUTH_FAILURE_COUNTERS(vptr);
      NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Handshake Failure: Server Authentication followed Proxy Authentication. 401 received in response to 407 message for ptotocols other than BASIC & DIGEST");
    }

    cptr->flags &= ~NS_CPTR_FLAGS_CON_PROXY_AUTH; 
    cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 
  }
  else if(cptr->req_code == 401)
  {
    cptr->proto_state = ST_AUTH_HANDSHAKE_FAILURE;
    cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 
    FREE_AND_MAKE_NULL_EX(cptr->prev_digest_msg, cptr->prev_digest_len, "HTTP AUTH Digset Prev. Msg", -1);
    NSDL2_AUTH(cptr->vptr, cptr, "Handshake Failure: Received 401 when expecting some success response code");
    NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Handshake Failure: Received 401 when expecting some success response code");
  }
  else if(cptr->req_code == 407 && (cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH))
  {
    cptr->proto_state = ST_AUTH_HANDSHAKE_FAILURE;
    INC_PROXY_AUTH_FAILURE_COUNTERS(vptr);
    cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 
    cptr->flags &= ~NS_CPTR_FLAGS_CON_PROXY_AUTH; 
    FREE_AND_MAKE_NULL_EX(cptr->prev_digest_msg, cptr->prev_digest_len, "HTTP AUTH Digset Prev. Msg", -1);
    NSDL2_AUTH(cptr->vptr, cptr, "Handshake Failure: Received 407 when expecting some success response code");
    NS_EL_2_ATTR(EID_AUTH_NTLM_INVALID_PKT, -1, -1, EVENT_CORE, EVENT_MAJOR,
               __FILE__, (char*)__FUNCTION__, "Handshake Failure: Received 407 when expecting some success response code");
  }

  else // Any other code is succesful from Authentication point of view
  {
    cptr->proto_state = 0;
    cptr->flags &= ~NS_CPTR_AUTH_MASK;
    cptr->flags &= ~NS_CPTR_AUTH_TYPE_FIXED; 

    if(cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)
    {
      INC_PROXY_AUTH_SUCC_COUNTERS(vptr);
      NSDL2_AUTH(cptr->vptr, cptr, "proxy_avgtime->proxy_auth_success=%'.3f", proxy_avgtime->proxy_auth_success);
    }
    cptr->flags &= ~NS_CPTR_FLAGS_CON_PROXY_AUTH; 
    FREE_AND_MAKE_NULL_EX(cptr->prev_digest_msg, cptr->prev_digest_len, "HTTP AUTH Digset Prev. Msg", -1);
    NSDL2_AUTH(cptr->vptr, cptr, "Handshake Successful (Final message)");
  }
}

//----------------NTLM MESSAGE PROCESSING SECTION ENDS ------------------------------------


//----------------SCRIPT PARSING & KEYWORK PARSING SECTION BEGINGS ------------------------------------

/*Purpose: Parse G_HTTP_AUTH_NTLM keyword used for HTTP Authentication using NTLM
G_HTTP_AUTH_NTLM <grp> <0/1> <ntlm_version> <domain> <workstation>
*/
int kw_set_g_http_auth_ntlm(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int enable_ntlm;
  int ntlm_version;
  char domain[MAX_NTLM_PARAMETER_VAL];
  char workstation[MAX_NTLM_PARAMETER_VAL];
  int num;
  int sg_fields = 4;

  NSDL2_AUTH(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %d %d %s %s", dummy, sg_name, &enable_ntlm, &ntlm_version, domain, workstation)) < (sg_fields))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_AUTH_NTLM_USAGE, CAV_ERR_1011029, CAV_ERR_MSG_1);
  }

  val_sgrp_name(buf, sg_name, 0);//validate group name

  if(enable_ntlm != 0 && enable_ntlm != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_AUTH_NTLM_USAGE, CAV_ERR_1011029, CAV_ERR_MSG_3);
  }

  if(ntlm_version != NTLM_VER_NTLMv1 && ntlm_version != NTLM_VER_NTLM2 && ntlm_version != NTLM_VER_NTLMv2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_AUTH_NTLM_USAGE, CAV_ERR_1011029, CAV_ERR_MSG_3);
  }

  if((strlen(domain) > MAX_NTLM_PARAMETER_VAL) || (strlen(workstation) > MAX_NTLM_PARAMETER_VAL)){
    sprintf(err_msg, "Warning: Domain/Workstation provide with keyword G_HTTP_AUTH_NTLM is too large. Will get truncated to size %d",  MAX_NTLM_PARAMETER_VAL);
    NS_DUMP_WARNING("Domain or workstation provide with keyword G_HTTP_AUTH_NTLM is too large. So, setting it's size to %d",  MAX_NTLM_PARAMETER_VAL);
  }
 
  gset->ntlm_settings.enable_ntlm = enable_ntlm;
  gset->ntlm_settings.ntlm_version = ntlm_version;
  if (domain[0] == 'N' && domain[1] == 'A')
   gset->ntlm_settings.domain[0] = '\0';
  else
    strncpy(gset->ntlm_settings.domain, domain, MAX_NTLM_PARAMETER_VAL);

  if (workstation[0] == 'N' && workstation[1] == 'A')
   gset->ntlm_settings.workstation[0] = '\0';
  else
    strncpy(gset->ntlm_settings.workstation, workstation, MAX_NTLM_PARAMETER_VAL);
  
  NSDL2_RUNTIME(NULL, NULL, "After Parsing G_HTTP_AUTH_NTLM GroupName = %s, enable_ntlm = %d, ntlm_version = %d, domain= %s, workstation = %s",
                                    sg_name, gset->ntlm_settings.enable_ntlm, 
                                    gset->ntlm_settings.ntlm_version, gset->ntlm_settings.domain, 
                                    gset->ntlm_settings.workstation);
  return AUTH_SUCCESS;
}

/*Purpose: Parse G_HTTP_AUTH_NTLM keyword used for HTTP Authentication using NTLM
G_HTTP_AUTH_KERB <grp> <0/1>
*/
int kw_set_g_http_auth_kerb(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int enable_kerb;
  int num;
  int sg_fields = 3;

  NSDL2_AUTH(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %d", dummy, sg_name, &enable_kerb)) < (sg_fields))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_AUTH_KERB_USAGE, CAV_ERR_1011030, CAV_ERR_MSG_1);
  }

  val_sgrp_name(buf, sg_name, 0);//validate group name

  if(enable_kerb != 0 && enable_kerb != 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_HTTP_AUTH_KERB_USAGE, CAV_ERR_1011030, CAV_ERR_MSG_3);
  }

  gset->kerb_settings.enable_kerb = enable_kerb;

  NSDL2_RUNTIME(NULL, NULL, "After Parsing G_HTTP_AUTH_KERB GroupName = %s, enable_kerb = %d",
                                    sg_name, gset->kerb_settings.enable_kerb); 
  return AUTH_SUCCESS;
}

/*
int set_auth_ntlm_uname(char *username, char *flow_file, int sess_idx, int url_idx)
{
  segment_line();
  copy_into_big_buf(username, strlen(username))
  strcpy(requests[url_idx].proto.http.auth_ntlm_uname.seg_start->seg_ptr.str_ptr->big_buf_pointer, username);

}



void cache_copy_req_hdr_shr(int i)
{
  memcpy(&(request_table_shr_mem[i].proto.http.cache_req_hdr), &(requests[i].proto.http.cache_req_hdr), sizeof(requests[i].proto.http.cache_req_hdr));
  NSDL2_CACHE(NULL, NULL, "values in share memory:min-fresh value = %u, max-age value = %u, max-stale = %u", request_table_shr_mem[i].proto.http.cache_req_hdr.min_fresh, request_table_shr_mem[i].proto.http.cache_req_hdr.max_age, request_table_shr_mem[i].proto.http.cache_req_hdr.max_stale);
}
*/

//----------------SCRIPT PARSING & KEYWORK PARSING SECTION BEGINGS ------------------------------------

//---------------------------- PROXY AUTHENTICATION SECTION BEGINNS --------------------

int proc_http_hdr_auth_proxy(connection *cptr, char *header_buffer, int header_buffer_len, 
                                             int *consumed_bytes, u_ns_ts_t now) 
{
#ifdef NS_DEBUG_ON
  VUser *vptr = cptr->vptr;
#endif

  *consumed_bytes = 0; // Set to 0 in case of header is to be ignored
  NSDL2_AUTH(vptr, cptr, "Method called"); 

  if (cptr->req_code != 407)
  {
    //Complete Authenticate line processed. Ignoring Header
    //Set state to CR to parse next header line
    cptr->header_state = HDST_TEXT;     
    NSDL2_AUTH(vptr, cptr, "Ignoring 'Proxy-Authenticate' Header as response code is not 407"); 
    return AUTH_SUCCESS; // Must return 0
  }  
 
  if(cptr->proto_state == ST_AUTH_HANDSHAKE_FAILURE)
  {
    // Some error occurred. So ignore header
    cptr->header_state = HDST_TEXT;     
    NSDL2_AUTH(vptr, cptr, "Ignoring 'Proxy-Authenticate' Header as there was Authenticate handshake failed"); 
    return AUTH_SUCCESS; // Must return 0
  }
  parse_authenticate_hdr(cptr, header_buffer, header_buffer_len, consumed_bytes, now); 
  cptr->flags |= NS_CPTR_FLAGS_CON_PROXY_AUTH;

  return AUTH_SUCCESS;
}

//---------------------------- PROXY AUTHENTICATION SECTION ENDS --------------------
