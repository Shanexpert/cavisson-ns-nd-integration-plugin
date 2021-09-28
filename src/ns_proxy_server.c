/****************************************************************************
 * Name	     : ns_proxy_server.c 
 * Purpose   : This file contains all the functions related to http, https, etc... protocol request vai Proxy.  
 * Code Flow : 
 * Author(s) : Manish Kumar Mishra and Naveen Raina
 * Date      : 13 Oct. 2012
 * Copyright : (c) Cavisson Systems
 * Modification History :
 *     Author: Manish
 *      Date : 3 Nov. 2012 - add functions for HTTPS support 
 *****************************************************************************/

/* ################ Start: Header Declaration ###################### */
#define _GNU_SOURCE
#include "ns_cache_include.h"
#include "ns_http_script_parse.h"
#include "ns_proxy_server.h"
#include "ipmgmt_utils.h"
#include "ns_http_auth.h"
#include "ns_proxy_server_reporting.h"
#include "nslb_util.h"
#include "ns_data_types.h"
#include "ns_group_data.h"
#include "ns_h2_header_files.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
/* ################ End :  Header ################################# */

static inline void get_net6_pre(unsigned char addr[], unsigned char subnet[], unsigned char net_pre[]);
/* ################ Start: Global Varibale Declaration ############ */ 
static int g_p_idx = -1;  /* Since case of ALL group parse before SGRP 
                             so we need a gobal variable to track proxy index */

char *proxy_connection_buf = "Proxy-Connection: keep-alive\r\n";
char *https_connect_method = "CONNECT ";

char *end_connect_marker = "\r\n";

extern char *http_version_buffers[];
extern char http_version_buffers_len[];
extern char* Host_header_buf;
extern char* User_Agent_buf;

#define PROXY_CONNECTION_LEN 30
#define HTTPS_CONNECT_METHOD_LEN 8
#define END_CONNECT_MARKER_LEN 2 
/* ################ End: Global Varibale Declaration ############## */ 

/* ################ Start: Functions Definition ################### */
//This function will show the usage of the keyword SYSTEM_PROXY_AUTH
static int ns_system_proxy_auth_usage(char *msg, char *err_msg){
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "SYSTEM_PROXY_AUTH <UserName> <PassWord>\n");
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR;
}

//This function will show the usage of the keyword G_PROXY_SERVER
/*
static int ns_proxy_usage (char *msg, char *err_msg){
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "G_PROXY_SERVER <Group Name|ALL> <Mode> <Address List>\n");
  strcat(err_msg, "\tGroup Name:This field can have a valid group name or ALL.\n");
  strcat(err_msg, "\tMode: Mode can have three values:\n");
  strcat(err_msg, "\t\tMode 0: No proxy\n");
  strcat(err_msg, "\t\tMode 1: Use the proxy from system settings\n");
  strcat(err_msg, "\t\tMode 2: Use manual proxy configuration\n");
  strcat(err_msg, "\tAddress List: Can have a valid proxy address and is used for mode 2\n");
  strcat(err_msg, "\t\ti.e. 1) HTTP/HTTPS protocol (e.g http=192.168.1.66;https=cavissonproxy.com)\n");
  strcat(err_msg, " \t\tOr  2) We can specify all standalone(e.g all=192.168.1.68)\n"); 
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR; 
}
*/

#if 0
//This function will show the usage of the keyword SYSTEM_PROXY_SERVER
static int ns_system_proxy_usage (char *msg, char *err_msg){
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "SYSTEM_PROXY_SERVER <address_list>\n");
  strcat(err_msg, "\tAddress List: Mode 1 can have a valid proxy address\n");
  strcat(err_msg, "\t\ti.e. 1) HTTP/HTTPS protocol (e.g http=192.168.1.66;https=cavissonproxy.com)\n");
  strcat(err_msg, "\t\tOr   2) We can specify all standalone(e.g all=192.168.1.68) ");
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR;
}
#endif

//This function will show the usage of the keyword G_PROXY_EXCEPTIONS
static int ns_proxy_excp_usage (char *msg, char *err_msg)
{
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "G_PROXY_EXCEPTIONS <GroupName|ALL> <Bypass> <Exception_List>\n");
  strcat(err_msg, "\tGroup Name: This field can have a valid group name or ALL.\n");
  strcat(err_msg, "\tBypass: Can have two valid values:\n");
  strcat(err_msg, "\t\t0: Do not bypass proxy server for local addresses\n");
  strcat(err_msg, "\t\t1: Bypass proxy server for local addresses\n");
  strcat(err_msg, "\tException List: Can have a valid semicolon separated exceptions.\n\tExceptions can be any of the following \n");
  strcat(err_msg, "\t\ti.e 1) Domain name/Host name (www.cavisson.com)\n");
  strcat(err_msg, "\t\t    2) Ip address of host names that is resolved (67.218.96.251)\n");
  strcat(err_msg, "\t\t    3) Port number (:8800)\n");
  strcat(err_msg, "\t\t    4) IP & Port combination(67.218.96.251:8800)\n");
  strcat(err_msg, "\t\t    5) Subnet (192.168.1.0/24 or 2001::10:10:10:10/64)\n");
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR;
}

//This function will show the usage of the keyword SYSTEM_PROXY_EXCEPTIONS
static int ns_system_proxy_excp_usage (char *msg, char *err_msg)
{
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "SYSTEM_PROXY_EXCEPTIONS <Bypass> <Exception_List>\n");
  strcat(err_msg, "\tBypass: Can have two valid values:\n");
  strcat(err_msg, "\t\t0: Do not bypass proxy server for local addresses\n");
  strcat(err_msg, "\t\t1: Bypass proxy server for local addresses\n");
  strcat(err_msg, "\tException List: Can have a valid semicolon seperated exceptions.\n\tExceptions can be any of the following \n");
  strcat(err_msg, "\t\ti.e 1) Domain name/Host name (www.cavisson.com)\n");
  strcat(err_msg, "\t\t    2) Ip address of host names that is resolved (67.218.96.251)\n");
  strcat(err_msg, "\t\t    3) Port number (:8800)\n");
  strcat(err_msg, "\t\t    4) IP & Port combination(67.218.96.251:8800)\n");
  strcat(err_msg, "\t\t    5) Subnet (192.168.1.0/24 or 2001::10:10:10:10/64)\n");
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR;
}


static int ns_g_proxy_proto_mode_usage (char *msg, char *err_msg){
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "\tG_PROXY_PROTO_MODE <Group Name|ALL> <HTTP Mode> <HTTPS Mode>\n");
  strcat(err_msg, "\tGroup Name:This field can have a valid group name or ALL.\n");
  strcat(err_msg, "\tHTTP Mode: Can have two valid values 0 or 1.\n");
  strcat(err_msg, "\tHTTPS Mode: Can have two valid values 0 or 1.\n");
  fprintf(stderr, "%s", err_msg);
  return PROXY_ERROR;
}
/*bug 52092: reset cptr->http2 attributes after proxy connect*/
#define RESET_HTTP2_ATTRIBUTES_AFTER_PROXY_CONNECT()	\
if(cptr->http_protocol == HTTP_MODE_HTTP2)		\
 { 							\
   cptr->request_type = HTTPS_REQUEST;			\
   cptr->http2_state = HTTP2_CON_PREFACE_CNST;		\
 } 

/*-------------------------------------------------------------------
 * Purpose : Will handle proxy connection for https connection 
 *
 * Input   : now :- 
 * 
 * Output  : None 
 *-----------------------------------------------------------------*/
int handle_proxy_connect(connection *cptr, u_ns_ts_t now)
{
  NSDL1_PROXY(NULL, cptr, "Method Called."); 

  reset_cptr_attributes_after_proxy_connect(cptr);    
  /*bug 52092 : reset cptr->http2_state here, so that Priface can be sent after SSL HANDSHAKE*/
  RESET_HTTP2_ATTRIBUTES_AFTER_PROXY_CONNECT()
  handle_connect(cptr, now, 0);
  
  return PROXY_SUCCESS; 
}

/*bug 52092 : h2_proxy_make_and_send_connect */
/* Purpose : this method make CONNECT request and send to PROXY using HTTP2 

*******:scheme" and ":path" pseudo-header fields MUST be omitted. *********
:method ---> CONNECT
:authority" pseudo-header field contains the host and port to  connect to
*/
inline int h2_proxy_make_and_send_connect(connection* cptr, VUser *vptr, u_ns_ts_t now)
{

  NSDL2_HTTP2(vptr, cptr, "Method Called."); 
  int num_vec = 0;
  //header creation
  /*CONNECT request to be sent to PROXY via NON SSL write*/
  if(cptr->http2_state < HTTP2_SETTINGS_ACK)
  {
    cptr->request_type = HTTP_REQUEST;
    /*init cptr->http2*/
    init_cptr_for_http2(cptr);
    init_stream_map_http2(cptr);
 
    /*send Priface and SETTINGs frame to proxy server in non ssl mode*/
    if( http2_make_handshake_frames(cptr, now) != HTTP2_SUCCESS ) {
       return HTTP2_ERROR;
    }

    return HTTP2_SUCCESS;
  }
  /*make proxy CONNECT request and send to proxy server, 
    only when HTTP2_SETTINGS_ACK recieved from PROXY server*/
  int http_size;
  /*make HTTP2 CONNECT Header request */
  if((http_size = http2_make_proxy_request(cptr, &num_vec, now) ) < HTTP2_SUCCESS ) {
     return HTTP2_ERROR;
  }
  NSDL2_PROXY(vptr, cptr, "http2_connect_start time stamp=%'.3f", (double) vptr->httpData->proxy_con_resp_time->http_connect_start/1000.0);
  /*send HTTP2 CONNECT request*/
  return send_http_req(cptr, http_size, &g_req_rep_io_vector, now); 
}

/*-------------------------------------------------------------------
 * Purpose  : (1) Set bit for proxy connect NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT 
 *        
 * 	      (2) make Connect request. Connect request will be like..
 *                
 *                -------------------------------------------
 *                CONNECT server.example.com:80 HTTP/1.1
 *                User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.2; en-US; rv:1.9.0.4) Gecko/2008102920 Firefox/3.0.4
 *                Host: server.example.com:80
 *                Proxy-Connection: Keep-Alive
 *                Pragma: no-cache
 *                --------------------------------------------
 *
 *                To do this we have to follow following steps..
 *                (1)  : Fill Request line i.e. CONNECT server.example.com:80 HTTP/1.1
 *                (1.1): Filling Method(i.e. CONNECT ) in 0th vector
 *                (1.2): Fill Url i.e. server.example.com:80
 *                (1.3): Fill HTTP Version i.e. HTTP/1.1
 *               
 *                (2)  : Fill User-Agent 
 *             i.e. User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.2; en-US; rv:1.9.0.4) Gecko/2008102920 Firefox/3.0.4 
 *                (2.1): Fill header User-Agent
 *                (2.2): Fill User-Agent
 *
 *                (3)  : Fill Host-Agent i.e. Host: server.example.com:80
 *                (3.1): Fill header Host-Agent
 *                (3.2): Fill host    
 *
 *
 *            (3) Send this request to Proxy server by calling method send_http_req()  
 *
 *
 * Inputs   : cptr        :-
 *
 *            num_vectors :-
 *
 *            vector      :-
 *
 *            free_array  :-
 *
 *            now         :-
 *
 * Outputs  : on failure -1
 *            on success  0 
 *------------------------------------------------------------------*/
inline int proxy_make_and_send_connect(connection* cptr, VUser *vptr, u_ns_ts_t now)
{
  int http_size = 0;
  int body_start_idx = 0;
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  
  static char https_default_port[] = ":443";
  
  action_request_Shr* request = cptr->url_num;
  PerHostSvrTableEntry_Shr* svr_entry;   
  
  NSDL1_PROXY(vptr, cptr, "Method Called, cptr = %p", cptr);
  NSDL2_PROXY(vptr, cptr, "GET_PAGE_ID= %u, GET_URL_ID= %u", vptr->cur_page->page_id, request->proto.http.url_index);

  NSDL3_PROXY(vptr, cptr, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

  svr_entry = get_svr_entry(vptr, request->index.svr_ptr);

  /* Step (1.1): Filling Method (i.e. CONNECT ) in 0th vector (Method is with one space at the end) */
  NS_FILL_IOVEC(g_req_rep_io_vector, https_connect_method, HTTPS_CONNECT_METHOD_LEN);
  http_size += HTTPS_CONNECT_METHOD_LEN;  
  
  /* Step (1.2): Fill Host (i.e. server.example.com:80) */
  NS_FILL_IOVEC(g_req_rep_io_vector, svr_entry->server_name, svr_entry->server_name_len);
  http_size += svr_entry->server_name_len;  

  //If host is given without port like server.example.com then we need to add default port as 443
  if(svr_entry->server_flags & NS_SVR_FLAG_SVR_WITHOUT_PORT)
  {
    NS_FILL_IOVEC(g_req_rep_io_vector, https_default_port, HTTPS_DEFAULT_PORT_LEN);
    http_size += HTTPS_DEFAULT_PORT_LEN;  
  }

  /* Step (1.3): Fill HTTP Version (i.e. HTTP/1.1) */
  NS_FILL_IOVEC(g_req_rep_io_vector, 
                http_version_buffers[(int)(request->proto.http.http_version)], 
                http_version_buffers_len[(int)(request->proto.http.http_version)]);
  http_size += http_version_buffers_len[(int)(request->proto.http.http_version)];  

  /* Step (2): Fill User-Agent if enable
   * User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.2; en-US; rv:1.9.0.4) Gecko/2008102920 Firefox/3.0.4*/
  if (!(disable_headers & NS_UA_HEADER)) {
    NSDL3_PROXY(vptr, cptr, "Filling User-Agent header [%s] of len %d at vector index %d",
                             User_Agent_buf, USER_AGENT_STRING_LENGTH, g_req_rep_io_vector.cur_idx);
    NS_FILL_IOVEC(g_req_rep_io_vector, User_Agent_buf, USER_AGENT_STRING_LENGTH);
    http_size += USER_AGENT_STRING_LENGTH;  

    NSDL3_PROXY(vptr, cptr, "Filling User-Agent [%s] of len %d at index %d", 
                             vptr->browser->UA, strlen(vptr->browser->UA), g_req_rep_io_vector.cur_idx);
    NS_FILL_IOVEC(g_req_rep_io_vector, vptr->browser->UA, strlen(vptr->browser->UA));
    http_size += strlen(vptr->browser->UA);  
  }

  /* Step (3)   : Fill Hostr-Agent*/
  /* Step (3.1.): Fill Hostr-Agent header*/
  NSDL3_PROXY(vptr, cptr, "Filling Hostr-Agent header [%s] of len %d at vector index %d", 
                           Host_header_buf, HOST_HEADER_STRING_LENGTH, g_req_rep_io_vector.cur_idx);
  NS_FILL_IOVEC(g_req_rep_io_vector, Host_header_buf, HOST_HEADER_STRING_LENGTH);
  http_size += HOST_HEADER_STRING_LENGTH;  
  
  //svr_entry = get_svr_entry(vptr, request->index.svr_ptr);
  NSDL3_PROXY(vptr, cptr, "Filling Hostr-Agent (actual host)[%s] of len %d at vector index %d, svr_entry = %p",
                           svr_entry->server_name, svr_entry->server_name_len, g_req_rep_io_vector.cur_idx, svr_entry);
  NS_FILL_IOVEC(g_req_rep_io_vector, svr_entry->server_name, svr_entry->server_name_len);
  http_size += svr_entry->server_name_len;  

  NSDL3_PROXY(vptr, cptr, "Filling carrige return [%s] of len %d at vector index %d",
                           CRLFString, CRLFString_Length, g_req_rep_io_vector.cur_idx);
  NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length);
  http_size += CRLFString_Length;  
 
  //Fill only in case HTTP Ver 1.0
  /* Step (4)  : Fill Proxy-Connection i.e. Proxy-Connection: Keep-Alive */
  NSDL3_PROXY(vptr, cptr, "Filling Proxy-Connection header [%s] of length %d at vector index %d",
                           proxy_connection_buf, PROXY_CONNECTION_LEN, g_req_rep_io_vector.cur_idx);
  NS_FILL_IOVEC(g_req_rep_io_vector, proxy_connection_buf, PROXY_CONNECTION_LEN);
  http_size += PROXY_CONNECTION_LEN;  

  //Proxy Authentication header
  if(cptr->flags & NS_CPTR_AUTH_MASK)
  {
    int ret = auth_create_authorization_hdr(cptr, &body_start_idx, vptr->group_num, 0, 0, 0);
    if (ret == AUTH_ERROR)
    {
      NSDL2_PROXY(vptr, cptr, "Error making auth headers: will not send auth headers");
      return PROXY_ERROR;
    }
  }

  //Add end marker
  NS_FILL_IOVEC(g_req_rep_io_vector, end_connect_marker, END_CONNECT_MARKER_LEN);

  vptr->httpData->proxy_con_resp_time->http_connect_start = get_ms_stamp(); 
  NSDL2_PROXY(vptr, cptr, "http_connect_start time stamp=%'.3f", (double) vptr->httpData->proxy_con_resp_time->http_connect_start/1000.0);

  send_http_req(cptr, http_size, &g_req_rep_io_vector, now);
 
  return PROXY_SUCCESS; 
}

/*bug 52092: make_proxy_and_send_connect */
inline int make_proxy_and_send_connect(connection* cptr, VUser *vptr, u_ns_ts_t now)
{
  NSDL2_PROXY(vptr, cptr, "Method called cptr->http_protocol=%d", cptr->http_protocol);
  int status;
  switch(cptr->http_protocol)
  {
    case HTTP_MODE_AUTO :
    case HTTP_MODE_HTTP1:
      status = proxy_make_and_send_connect(cptr, vptr, now);
      break;
    case HTTP_MODE_HTTP2:  
      status = h2_proxy_make_and_send_connect(cptr, vptr, now);
      break;
    default:
      status = HTTP2_ERROR; 
      break;
  }
  if(status != PROXY_SUCCESS)
      NSTL1(vptr, cptr,"Error: error in making CONNECT");
  return status;  
}
/*-------------------------------------------------------------------
 * Purpose : Will initialize members of ProxyServerTable one by one.
 *
 * Input   : svr_idx :- indicate which entry have to be initialized
 * 
 * Output  : None 
 *-----------------------------------------------------------------*/
static void init_proxy(int svr_idx)
{
  NSDL1_PROXY(NULL, NULL, "Method Called.");

  //HTTPS
  proxySvrTable[svr_idx].http_proxy_server = -1;
  proxySvrTable[svr_idx].http_port = -1;
  memset(&proxySvrTable[svr_idx].http_addr, 0, sizeof(struct sockaddr_in6));

  //HTTPS
  proxySvrTable[svr_idx].https_proxy_server = -1;
  proxySvrTable[svr_idx].https_port = -1;
  memset(&proxySvrTable[svr_idx].https_addr, 0, sizeof(struct sockaddr_in6));

  //Exception
  proxySvrTable[svr_idx].bypass_subnet = '0';
  proxySvrTable[svr_idx].excp_start_idx = -1;
  proxySvrTable[svr_idx].num_excep = -1;

  //Authentication
  proxySvrTable[svr_idx].username = -1;
  proxySvrTable[svr_idx].password = -1;
}

/*-------------------------------------------------------------------
 * Purpose : Will initialize members of ProxyServerExcpTable one by one.
 *
 * 
 * Output  : None 
 *-----------------------------------------------------------------*/
static void init_proxy_excp(int row_no)
{
  NSDL1_PROXY(NULL, NULL, "Method Called, row_no = %d", row_no);

  proxyExcpTable[row_no].excp_type = -1; 
  //proxyExcpTable[row_no].excp_val.excp_domain_name = -1;
  memset(&proxyExcpTable[row_no].excp_val.excp_addr, 0, sizeof(struct sockaddr_in6));
}

/* This function calculates the network prefix of the ip address to be hit. Firstly converting ip to unsigned int form and then
 * anding it with the subnet mask obtained from the system for all interfaces. And then comparing them to the network prefixes
 * obtained in the function cal_net_prefix_of_system() for each interfaces.*/ 
//Returns 1 if found in same subnet, 0 in case actual ip not falling in any subnet for all interfaces

static inline int cmp_net_prefixes(char *buf, char is_bypass_set)
{ 
  unsigned long out = 0;
  int i;
  unsigned long net_pre_for_host = 0;
  unsigned char net_pre_for_ipv6[16];
  struct in6_addr conv_ipv6;  //

  NSDL1_PROXY(NULL, NULL,"Method Called.buf=[%s], bypass value =%c", buf, is_bypass_set);

  //Compare subnets (local & external) only in case bypass_subnet is set
  //We explicitily set bypass_subnet in case subnet is external one.
  if (is_bypass_set != '1') 
  {
   NSDL1_PROXY(NULL, NULL,"Compare subnets ignored as bypass_local is unset");
   return 0;
  }

  //Checking for IP to be in IPV4 format
  int int_value_for_host = inet_pton(AF_INET, buf, &out);//Converting the ip address to unsigned int value. 
  out = ntohl(out);
  NSDL2_PROXY(NULL, NULL,"int_value_for_host=%d, network value for host=%ld", int_value_for_host, out);
 
  //IP is found which is in IPV4 format
  if(int_value_for_host)
  {
    NSDL2_PROXY(NULL, NULL,"total_proxy_ip_interfaces=%d", total_proxy_ip_interfaces);
    for(i=0; i < total_proxy_ip_interfaces; i++)
    {
      net_pre_for_host = out & proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.subnet; 

      NSDL2_PROXY(NULL, NULL,"Interface %d: net prefix=%ld, subnet=%ld, net prefix for host=%ld", i, proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.net_prefix, proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.subnet, net_pre_for_host);

      if(net_pre_for_host == proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.net_prefix) {//comparing the network prefix obtained above with the network prefixes of all interfaces
        NSDL2_PROXY(NULL, NULL,"Network prefix %ld found and matched with table entry %ld at index=%d", net_pre_for_host,proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.net_prefix, i);
        return 1; 
      } 
    }
  }
  else
  {
    //Assuming IP to be in IPV6 format
    NSDL2_PROXY(NULL, NULL,"return value of inet_pton is=%d", int_value_for_host); 
    
    //Checking if the ip in the buffer is ipv6 or not. If it is ipv6 then filling the structure conv_ipv6 (of type struct in6_addr)so that we can get the value of ipv6 in the unsigned character array. 
    int int_val_for_ipv6 = inet_pton(AF_INET6, buf, &conv_ipv6);

    NSDL2_PROXY(NULL, NULL,"Ipv6 comparison going to happen, total_proxy_ip_interfaces=%d, int_val_for_ipv6 = %d", total_proxy_ip_interfaces, int_val_for_ipv6);

    if(int_val_for_ipv6){
      for(i=0; i < total_proxy_ip_interfaces; i++)
      {
        //Calculating the network prefix and then comparing it with system ip network prefixes
        get_net6_pre((&conv_ipv6)->s6_addr, proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.subnet, net_pre_for_ipv6);

        if(!memcmp(net_pre_for_ipv6, proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.net_prefix, 16)) {//comparing the network prefix obtained above with the network prefixes of all interfaces
          NSDL2_PROXY(NULL, NULL,"Ipv6 ip address network prefix matched");
          return 1;
        }
      }
    }
  }
  
  NSDL2_PROXY(NULL, NULL,"Network prefix %ld not found in the list", net_pre_for_host);
  return 0;
}

//Checks if proxy is enabled/disabled
//                 enabled : proxy enabled for even a single group
//                 disabled : proxy disabled for all the groups
//Returns 0 - proxy disabled
//        1 - proxy enabled
//Check is performed once (in case proxy_flag = -1) and stored in proxy_flag) and returned further without checking proxy for each group
inline int is_proxy_enabled()
{
  NSDL2_PROXY(NULL, NULL,"Method called.proxy_flag =%hd", global_settings->proxy_flag);
  int i;

  //Checks computation is made for proxy already set or not. If set, no further computation done, and proxy_flag value returned 
  NSDL2_PROXY(NULL, NULL,"total_runprof_entries=%d", total_runprof_entries);
  for (i=0; i < total_runprof_entries; i++)
  {
    if(runProfTable)
    {
      if(runProfTable[i].proxy_idx != -1)
      {
        global_settings->proxy_flag = 1;
        NSDL2_PROXY(NULL, NULL,"Setting global_settings->proxy_flag=%hd as proxy index is set for a group",global_settings->proxy_flag);
        return global_settings->proxy_flag;
      }
    }
    //Else part may not be called. Just for safety check
    else
    {
      if(runprof_table_shr_mem[i].proxy_ptr != NULL)
      {
        global_settings->proxy_flag = 1;
        return global_settings->proxy_flag;
      }
    }
  }
  global_settings->proxy_flag = 0;
  NSDL2_PROXY(NULL, NULL,"Proxy Flag = %hd", global_settings->proxy_flag);
  return global_settings->proxy_flag;
}
 

#define GET_IP_VERSION_AND_SET_ADDR(in_addr, out4_addr, out6_addr, ip_version, ip_buf) \
{ \
  if(in_addr.sin6_family == AF_INET) \
  { \
    out4_addr = (struct sockaddr_in *)(&in_addr); \
    strcpy(ip_buf, nslb_sock_ntop((struct sockaddr *)out4_addr)); \
    ip_version = IPv4; \
  } \
  else if(in_addr.sin6_family == AF_INET6)\
  { \
    out6_addr = (struct sockaddr_in6 *)(&in_addr); \
    strcpy(ip_buf, nslb_sock_ntop((struct sockaddr *)out6_addr)); \
    ip_version = IPv6; \
  } \
  else \
  { \
    ip_version = NO_IPv; \
  } \
}

//This function just places '^' at start and '$' at end so that the exact match is made.
//And also adding '\\' before '.', '[' and ']' so that their original meaning is intact
static void handle_wild_cards(char *domain_excp, char *output_domain_excp)
{
  char *input_buf, *output_buf;
  input_buf = domain_excp;
  output_buf = output_domain_excp;
  int count = 0;
  
  NSDL3_PROXY(NULL, NULL,"Method Called, input_buf=%s", input_buf); 
  //Adding '^' at start and '$' at end to make the exact match
  //And also adding '\\' before '.', '[' and ']' so that their original meaning remains intact.
  while(*input_buf)
  {
    if (!count)
    {
      *output_buf = '^';
      output_buf++; 
    }
    if (*input_buf == '.')
    {
      *output_buf = '\\';
      output_buf++;
    }
    else if(*input_buf == '[')
    {
      *output_buf = '\\';
      output_buf++;
    }
    else if(*input_buf == ']')
    {
      *output_buf = '\\';
      output_buf++;
    }
    *output_buf = *input_buf;
    input_buf++;
    output_buf++;
    count++;
  }
  *output_buf = '$';
  *(output_buf+1) = '\0';
  NSDL3_PROXY(NULL, NULL,"output_buf=%s", output_buf);

}

/*-------------------------------------------------------------------
 * Purpose : (1) Will check that is given url is in exception list or not 
 *
 * Input   : 
 * 
 * Output  : 
 *-----------------------------------------------------------------*/
static int is_host_in_exception_list(VUser *vptr, connection *cptr, ProxyServerTable_Shr *proxy_ptr, PerHostSvrTableEntry_Shr* svr_entry)
{
  int i = 0;
  int ip_found = 0, ip_port_found = 0, port_found = 0, domain_found = 0;
  int host_ip_version = -1, excp_ip_version = -1; 
  char host_ip_buf[1024], ecxp_ip_buf[1024];
  struct sockaddr_in *sin4_host;
  struct sockaddr_in *sin4_excp;
  struct sockaddr_in6 *sin6_host;
  struct sockaddr_in6 *sin6_excp;

  ProxyExceptionTable_Shr *excp_ptr = proxy_ptr->excp_start_ptr;

  host_ip_buf[0] = 0; ecxp_ip_buf[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method called, proxy_ptr = %p, excp_ptr = %p, Proxy Exceptions = %d", 
                           proxy_ptr, excp_ptr, proxy_ptr->num_excep);

  //Check IP version for host and find out host ip 
  GET_IP_VERSION_AND_SET_ADDR(svr_entry->saddr, sin4_host, sin6_host, host_ip_version, host_ip_buf); 
  NSDL2_PROXY(NULL, NULL, "host_ip_version = %d, host_ip_buf = %s", 
                           host_ip_version, host_ip_buf);

  char *tmp_excp_ptr;
  char tmp_buf[2048];
  char tmp_domain_buf[2048];
  for(i=0; i < proxy_ptr->num_excep; i++)
  {
    NSDL3_PROXY(NULL, NULL, "excp_index = %d, excp_ptr = %p, excp_type = %d", i, excp_ptr, excp_ptr->excp_type);

    //Check IP version for exceptions and find out host ip  
    GET_IP_VERSION_AND_SET_ADDR(excp_ptr->excp_val.excp_addr, sin4_excp, sin6_excp, excp_ip_version, ecxp_ip_buf); 
    NSDL3_PROXY(NULL, NULL, "excp_ip_version = %d, ecxp_ip_buf = %s", excp_ip_version, ecxp_ip_buf);

    switch (excp_ptr->excp_type)
    {
      case EXCP_IS_DOMAIN_OR_HOSTNAME:
        NSDL3_PROXY(NULL, NULL, "Case: EXCP_IS_DOMAIN_OR_HOSTNAME, server_flags = 0X%x, excp_domain =%s", svr_entry->server_flags, excp_ptr->excp_val.excp_domain_name);
        tmp_excp_ptr = excp_ptr->excp_val.excp_domain_name;
        if(svr_entry->server_flags & NS_SVR_FLAG_SVR_IS_DOMAIN)
        {
          NSDL3_PROXY(NULL, NULL, "server_name = [%s], excp_domain_name = [%s], "
                           "excp_domain_name start char= [%c]",
                           svr_entry->server_name, tmp_excp_ptr, tmp_excp_ptr[0]);                     

          if(!strcasecmp(svr_entry->server_name, tmp_excp_ptr))
            domain_found = 1;
          break;

          if(domain_found)
            NSDL4_PROXY(NULL, NULL, "Domain %s found in exception list.", svr_entry->server_name); 
        }
        break;
   
      case EXCP_IS_IP_WITH_PORT:
        NSDL3_PROXY(NULL, NULL, "Case: EXCP_IS_IP_WITH_PORT");
        if((host_ip_version == IPv4 && excp_ip_version == IPv4) &&
           (!memcmp(&sin4_host->sin_addr, &sin4_excp->sin_addr, 8) &&
                      (sin4_host->sin_port == sin4_excp->sin_port)))
        {
          NSDL3_PROXY(NULL, NULL, "Case (IP:Port): Host ip %s:%hd found in exception list.", 
                                   host_ip_buf, ntohs(sin4_host->sin_port));
          ip_port_found = 1;
        }
        else if ((host_ip_version == IPv6 && excp_ip_version == IPv6) && 
                (!memcmp(&sin6_host->sin6_addr, &sin6_excp->sin6_addr, 16) &&
                      (sin6_host->sin6_port == sin6_excp->sin6_port)))
        {
          NSDL3_PROXY(NULL, NULL, "Case (IP:Port): Host ip %s:%hd found in exception list.", 
                                   host_ip_buf, ntohs(sin6_host->sin6_port));
          ip_port_found = 1;
        }
        break;
      
      case EXCP_IS_PORT_ONLY:
        NSDL3_PROXY(NULL, NULL, "Case: EXCP_IS_PORT_ONLY - Host port = %hd, Exception port = %d", 
                          ntohs(sin4_host->sin_port), sin6_excp->sin6_port);
        if((host_ip_version == IPv4) && (ntohs(sin4_host->sin_port) == sin6_excp->sin6_port))
        {
          NSDL3_PROXY(NULL, NULL, "Case (Ip port): Host port %hd found in exception list.", ntohs(sin4_host->sin_port));
          port_found = 1;
        }
        else if((host_ip_version == IPv6) && (ntohs(sin6_host->sin6_port) == sin6_excp->sin6_port))
        {
          NSDL3_PROXY(NULL, NULL, "Case (Ip port): Host port %hd found in exception list.", ntohs(sin6_host->sin6_port));
          port_found = 1;
        }
        break;

      case EXCP_IS_IP_ONLY:
        NSDL3_PROXY(NULL, NULL, "Case: EXCP_IS_IP_ONLY");
        if((host_ip_version == IPv4 && excp_ip_version == IPv4) && 
             (!memcmp(&sin4_host->sin_addr, &sin4_excp->sin_addr, 8)))
        {
          NSDL3_PROXY(NULL, NULL, "Case (Ip only): Host ip %s found in exception list.", host_ip_buf);
          ip_found = 1;
        }
        else if((host_ip_version == IPv6 && excp_ip_version == IPv6) && 
                  (!memcmp(&sin6_host->sin6_addr, &sin6_excp->sin6_addr, 16)))
        {
          NSDL3_PROXY(NULL, NULL, "Case (Ip only): Host ip %s found in exception list.", host_ip_buf);
          ip_found = 1;
        }
        break;

      case EXCP_IS_WITH_WILDCARD: 
        NSDL3_PROXY(NULL, NULL,"Case: EXCP_IS_WITH_WILDCARD. server entry =%s", svr_entry->server_name);
        tmp_excp_ptr = excp_ptr->excp_val.excp_domain_name;
        NSDL4_PROXY(NULL, NULL,"tmp_excp_ptr=%s", tmp_excp_ptr);
        int ret = 0;
        //Checking if actual server is domain name, matching the actual server name with exception pattern
        if(svr_entry->server_flags & NS_SVR_FLAG_SVR_IS_DOMAIN)
        {
          NSDL3_PROXY(NULL, NULL,"Actual server is domain name so exception list must have domain name.");
          handle_wild_cards(tmp_excp_ptr, tmp_buf); 
          nslb_regex_convert(tmp_buf, tmp_domain_buf); 

          //Calling the function to compile and execute regular expression 
          ret = compile_and_execute_regex (tmp_domain_buf, svr_entry->server_name);
          if (ret)
          {
            //Since compilation and execution was successfully done so setting domain flag
            domain_found = 1;
          }
          NSDL3_PROXY(NULL, NULL,"svr_entry->server_name=%s", svr_entry->server_name);
        }
        else
        {
          //Here it can be the case of only ip(ipv4 or ipv6) in exception list
          NSDL3_PROXY(NULL, NULL,"Case of ip found.");
          char *ptr;
          int ipv6 = 0;
	  if ((ptr = index (svr_entry->server_name, ':')) && index (ptr+1, ':')) {
	    //becuase there are two colons, it seems like an IPV6 address so setting flags
	    ipv6 = 1;
            NSDL3_PROXY(NULL, NULL,"As actual server is ipv6 so setting flag");
          }

          if(!ipv6) {
            //Case of ipv4 with or without port.
            if ((ptr = rindex (tmp_excp_ptr, ':')) == NULL)
            {
              //Case of ipv4 without port in exception
              NSDL3_PROXY(NULL, NULL,"Exception list has no colon so removing colon from origin server.");
              char *new_ptr = svr_entry->server_name;
              //Since ipv4 is without port so we need to remove in actual server the port value so that we can successfully use regex
              //So we replace actual server ':' value with '\0' and then undo it in the end.
              while(*new_ptr)
              {
                if(*(new_ptr) == ':'){
                  *new_ptr = '\0';  
                  break;
                }
                new_ptr++;
              }
    
              NSDL3_PROXY(NULL, NULL,"server name =%s", svr_entry->server_name);
              handle_wild_cards(tmp_excp_ptr, tmp_buf); 
              nslb_regex_convert(tmp_buf, tmp_domain_buf); 

              //Calling the function to compile and execute regular expression
              ret = compile_and_execute_regex (tmp_domain_buf, svr_entry->server_name);
              if(ret)
              {
                //Since compilation and execution was successfully done so setting domain flag
                domain_found = 1;
              }
              //Undoing the ':' in actual server entry.
              *new_ptr = ':';
              NSDL3_PROXY(NULL, NULL,"svr_entry->server_name=%s", svr_entry->server_name);
            }
            else
            {
              //Case of ipv4 with port 
              NSDL3_PROXY(NULL, NULL,"server name =%s", svr_entry->server_name);
              handle_wild_cards(tmp_excp_ptr, tmp_buf); 
              nslb_regex_convert(tmp_buf, tmp_domain_buf); 

              //Calling the function to compile and execute regular expression
              ret = compile_and_execute_regex (tmp_domain_buf, svr_entry->server_name);
              if(ret)
              {
                //Since compilation and execution was successfully done so setting domain flag
                domain_found = 1;
              }
            }
          }
          else
          {
            //Case of ipv6 with or without port.
            if ((ptr = rindex (tmp_excp_ptr, ':')) != NULL){
              if(*(ptr-1) == ']')
              {
                //Case of ipv6 with port in exception.
                NSDL3_PROXY(NULL, NULL,"server name =%s", svr_entry->server_name);
                handle_wild_cards(tmp_excp_ptr, tmp_buf);
                nslb_regex_convert(tmp_buf, tmp_domain_buf);

                //Calling the function to compile and execute regular expression
                ret = compile_and_execute_regex (tmp_domain_buf, svr_entry->server_name);
                if(ret)
                {
                  //Since compilation and execution was successfully done so setting domain flag
                  domain_found = 1;
                }
              }
              else
              {
                //Case of ipv6 without port in exception.
                NSDL3_PROXY(NULL, NULL,"Exception list has no colon so removing colon from origin server.");
                char *ptr = svr_entry->server_name + 1;
                //Since exception doesnt contain port so the actual server need to be modified.
                //So we remove square brackets and replace it with '\0'.
                while(*ptr)
                {
                  if(*(ptr) == ':'){
                    if(*(ptr-1) == ']')
                    {
                     *(ptr-1) = '\0';  
                     break;
                    }
                  }
                  ptr++;
                }
                NSDL3_PROXY(NULL, NULL,"Server entry=%s", svr_entry->server_name);
   
                handle_wild_cards(tmp_excp_ptr, tmp_buf); 
                nslb_regex_convert(tmp_buf, tmp_domain_buf); 
               
                //Calling the function to compile and execute regular expression
                ret = compile_and_execute_regex (tmp_domain_buf, svr_entry->server_name+1);
                if(ret)
                {
                  //Since compilation and execution was successfully done so setting domain flag
                  domain_found = 1;
                }
                //Undoing the '\0' in actual server to ']'.
                *(ptr-1) = ']';
              }
            } 
          }
        }
        default :
        //TODO:
        break;
    }
    excp_ptr++;
    NSDL2_PROXY(NULL, NULL, "In Excp=%d domain_found = %d, ip_port_found = %d, port_found = %d, ip_found = %d", 
                           i, domain_found, ip_port_found, port_found, ip_found);
  }

  NSDL2_PROXY(NULL, NULL, "domain_found = %d, ip_port_found = %d, port_found = %d, ip_found = %d", 
                           domain_found, ip_port_found, port_found, ip_found);
  if(domain_found || ip_port_found || port_found || ip_found)
    return 1;

  return 0;
}

/*-------------------------------------------------------------------
 * Purpose : (1) Will check proxy is used or not
 *           (2) If proxy used then set proxy bit NS_CPTR_FLAGS_CON_USING_PROXY 
 *               and set desired server into cptr->conn_server  
 *
 * Input   : vptr :-
 *           cptr :-  
 * 
 * Output  : None 
 *-----------------------------------------------------------------*/
void inline proxy_check_and_set_conn_server(VUser *vptr, connection *cptr)
{
  action_request_Shr* request = cptr->url_num;
  ProxyServerTable_Shr *proxy_ptr = runprof_table_shr_mem[vptr->group_num].proxy_ptr;
  PerHostSvrTableEntry_Shr* svr_entry;
  char host_ip_buf[1024];

  NSDL1_PROXY(vptr, cptr, "Method Called, group_num=%d, runprof_table_shr_mem[%d].proxy_ptr = %p", 
                           vptr->group_num,
                           vptr->group_num,
                           proxy_ptr);

  svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);
  struct sockaddr_in *sin_host = (struct sockaddr_in *)(&svr_entry->saddr);
  strcpy(host_ip_buf, nslb_sock_ntop((struct sockaddr *)sin_host));

  //If proxy is not used make connection to Host
  {
    NSDL2_PROXY(vptr, cptr, "Proxy is using, request_type = %d", request->request_type);
    switch(request->request_type) 
    { 
      case HTTP_REQUEST:
      case WS_REQUEST:
      case SOCKET_REQUEST:
        NSDL2_PROXY(vptr, cptr, "Group %d http_proxy=%s:%hi", 
                         vptr->group_num, proxy_ptr->http_proxy_server?proxy_ptr->http_proxy_server:NULL, proxy_ptr->http_port);

        INC_HTTP_PROXY_INSPECTED_REQ(vptr);
        if ((proxy_ptr->http_proxy_server != NULL) && 
                  !(cmp_net_prefixes(nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 0), proxy_ptr->bypass_subnet) || 
                    is_host_in_exception_list(vptr, cptr, proxy_ptr, svr_entry)))
        {
          NSDL2_PROXY(vptr, cptr, "Copying http_addr to cptr->conn_server for HTTP request.");
          memcpy(&(cptr->conn_server), &(proxy_ptr->http_addr), 
                                           sizeof(struct sockaddr_in6));
          INC_HTTP_PROXY_REQ(vptr);
          NSDL2_PROXY(vptr, cptr, "tot_http_proxy_requests=%llu", proxy_avgtime->tot_http_proxy_requests);
          INC_TOT_HTTP_PROXY_REQ(vptr);
          cptr->flags |= NS_CPTR_FLAGS_CON_USING_PROXY;
        }
        else
        {
          NSDL2_PROXY(vptr, cptr, "Copying cptr->cur_server to cptr->conn_server for HTTP request.");
          memcpy(&(cptr->conn_server), &(cptr->cur_server), sizeof(struct sockaddr_in6));
          INC_HTTP_PROXY_EXCP_REQ(vptr);
        }
        break;
      case HTTPS_REQUEST:
      case WSS_REQUEST:
      case SSL_SOCKET_REQUEST:
        NSDL2_PROXY(vptr, cptr, "Group %d proxy_ptr->http_addr = %p, proxy_ptr->https_proxy_server = %p", 
                                 vptr->group_num, proxy_ptr->http_addr, proxy_ptr->https_proxy_server);

        INC_HTTPS_PROXY_INSPECTED_REQ(vptr);
        if ((proxy_ptr->https_proxy_server != NULL) && 
               !(cmp_net_prefixes(nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 0), proxy_ptr->bypass_subnet) ||
                  is_host_in_exception_list(vptr, cptr, proxy_ptr, svr_entry)))
        { 
          NSDL2_PROXY(vptr, cptr, "Copying https_addr to cptr->conn_server for HTTPS request.");
          memcpy(&(cptr->conn_server), &(proxy_ptr->https_addr),
                                          sizeof(struct sockaddr_in6)); 
          cptr->flags |= NS_CPTR_FLAGS_CON_USING_PROXY;
          cptr->flags |= NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT;
          NSDL2_PROXY(vptr, cptr, "tot_http_proxy_requests=%llu", proxy_avgtime->tot_http_proxy_requests);
          INC_HTTPS_PROXY_REQ(vptr);
          INC_TOT_HTTPS_PROXY_REQ(vptr);
        }
        else
        {
          NSDL2_PROXY(vptr, cptr, "Copying cptr->cur_server to cptr->conn_server for HTTPS request.");
          memcpy(&(cptr->conn_server), &(cptr->cur_server), sizeof(struct sockaddr_in6));
          INC_HTTPS_PROXY_EXCP_REQ(vptr);
        }
        break;
      default:
        NSDL3_PROXY(vptr, cptr, "Inside default case.");
        //TODO
        break;
    }
  } 
}

void ns_proxy_table_dump()
{
  int i = 0;
  NSDL1_PROXY(NULL, NULL, "ProxyServerData Dump:\n\ttotal_runprof_entries = %d, total_proxy_svr_entries = %d\n", 
                           total_runprof_entries, total_proxy_svr_entries);

  for (i=0; i < total_runprof_entries; i++)
  {
    NSDL2_PROXY(NULL, NULL, "Proxy Server Index = %d, ", i); 
    if(runProfTable[i].proxy_idx != -1)
      NSDL2_PROXY(NULL, NULL, "group_num = %d, group_name = %s, proxy_idx = %d, "
              "http_address = %s, http_port = %hd, "
              "https_address = %s, https_port = %hd, "
              "bypass_subnet = %c, excp_start_idx = %d, num_excep = %d\n",
              runProfTable[i].group_num, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name), 
              runProfTable[i].proxy_idx,
              RETRIEVE_BUFFER_DATA(proxySvrTable[runProfTable[i].proxy_idx].http_proxy_server),
              proxySvrTable[runProfTable[i].proxy_idx].http_port,
              RETRIEVE_BUFFER_DATA(proxySvrTable[runProfTable[i].proxy_idx].https_proxy_server),
              proxySvrTable[runProfTable[i].proxy_idx].https_port,
              proxySvrTable[runProfTable[i].proxy_idx].bypass_subnet,
              proxySvrTable[runProfTable[i].proxy_idx].excp_start_idx,
              proxySvrTable[runProfTable[i].proxy_idx].num_excep
            ); 
  }  
}

/*-------------------------------------------------------------------
 * Purpose : Proxy Data dump from shared memory
 * Input   : None
 * Output  : show Proxy data from shared memory
 *-----------------------------------------------------------------*/
void ns_proxy_shr_data_dump()
{
  int i = 0, j = 0;

  NSDL1_PROXY(NULL, NULL, "Method called, total_runprof_entries = %d", total_runprof_entries);
  
  NSDL2_PROXY(NULL, NULL, "Proxy Shared Memory Data Dump: start :-");
  for (i=0; i < total_runprof_entries; i++)
  {
    NSDL2_PROXY(NULL, NULL,"RunProfShrIdx = %d", i);
    if(runprof_table_shr_mem[i].proxy_ptr != NULL)
    {
      ProxyExceptionTable_Shr *excp_ptr = runprof_table_shr_mem[i].proxy_ptr->excp_start_ptr;
      NSDL2_PROXY(NULL, NULL,"RunProfShrIdx = %d, Group_ID = %d, Proxy_Id = %p, "
                            "http_address = %s, http_port = %d, "
                            "https_address = %s, https_port = %d, "
                            "bypass_subnet = %c, excp_start_idx = %p, num_excep = %d",
                            i, runprof_table_shr_mem[i].group_num, runprof_table_shr_mem[i].proxy_ptr,
                            runprof_table_shr_mem[i].proxy_ptr->http_proxy_server,
                            runprof_table_shr_mem[i].proxy_ptr->http_port,
                            runprof_table_shr_mem[i].proxy_ptr->https_proxy_server,
                            runprof_table_shr_mem[i].proxy_ptr->https_port,
                            runprof_table_shr_mem[i].proxy_ptr->bypass_subnet,
                            runprof_table_shr_mem[i].proxy_ptr->excp_start_ptr,
                            runprof_table_shr_mem[i].proxy_ptr->num_excep
               );
      for(j=0; j < runprof_table_shr_mem[i].proxy_ptr->num_excep; j++)
      {
       NSDL2_PROXY(NULL, NULL,"Excp Shared memory Data Dump per group: "
                                "excp idex = %d, excp_ptr=%p, excp_type = %d, excp_addr/excp_domain_name = %s\n",
                           j, excp_ptr,
                           excp_ptr->excp_type,
                           ((excp_ptr->excp_type == EXCP_IS_DOMAIN_OR_HOSTNAME) || (excp_ptr->excp_type == EXCP_IS_WITH_WILDCARD))?
                           (excp_ptr->excp_val.excp_domain_name):
                           (nslb_sock_ntop((struct sockaddr *)&excp_ptr->excp_val.excp_addr))
              );
        excp_ptr++;
      }
    }
  }
  NSDL2_PROXY(NULL, NULL, "Proxy Data Dump: End :-");
}

//Copies ProxyServerTable into shared memory
inline void copy_proxySvr_table_into_shr_mem(ProxyServerTable_Shr *proxySvr_table_shr_mem)
{
  int i = 0;
  NSDL1_PROXY(NULL, NULL, "Method Called, proxySvr_table_shr_mem = %p, total_proxy_svr_entries = %d", 
                           proxySvr_table_shr_mem, total_proxy_svr_entries);
  for(i = 0; i < total_proxy_svr_entries; i++)
  {  
    /*For HTTP*/
    if(proxySvrTable[i].http_proxy_server != -1)
      proxySvr_table_shr_mem[i].http_proxy_server = BIG_BUF_MEMORY_CONVERSION(proxySvrTable[i].http_proxy_server);
    else
      proxySvr_table_shr_mem[i].http_proxy_server = NULL;

    proxySvr_table_shr_mem[i].http_port = proxySvrTable[i].http_port;
    memcpy(&(proxySvr_table_shr_mem[i].http_addr), &(proxySvrTable[i].http_addr), sizeof(struct sockaddr_in6)); 

    /*For HTTPS*/
    if(proxySvrTable[i].https_proxy_server != -1)
      proxySvr_table_shr_mem[i].https_proxy_server = BIG_BUF_MEMORY_CONVERSION(proxySvrTable[i].https_proxy_server);
    else
      proxySvr_table_shr_mem[i].https_proxy_server = NULL;

    proxySvr_table_shr_mem[i].https_port = proxySvrTable[i].https_port;
    memcpy(&(proxySvr_table_shr_mem[i].https_addr), &(proxySvrTable[i].https_addr), sizeof(struct sockaddr_in6));

    /* Exception */
    proxySvr_table_shr_mem[i].bypass_subnet = proxySvrTable[i].bypass_subnet;
   
    if(proxySvrTable[i].excp_start_idx != -1)
    {
      proxySvr_table_shr_mem[i].excp_start_ptr = PROXY_EXCP_TABLE_MEMORY_CONVERSION(proxySvrTable[i].excp_start_idx);
      NSDL1_PROXY(NULL, NULL, "i=%d, excp_start_idx=%d, excp_start_ptr=%p", i, proxySvrTable[i].excp_start_idx, proxySvr_table_shr_mem[i].excp_start_ptr);
    }
    else
      proxySvr_table_shr_mem[i].excp_start_ptr = NULL;
     

    proxySvr_table_shr_mem[i].num_excep = proxySvrTable[i].num_excep;

    /* Proxy Authentication */
    if(proxySvrTable[i].username != -1) 
      proxySvr_table_shr_mem[i].username = BIG_BUF_MEMORY_CONVERSION(proxySvrTable[i].username); 
    else
      proxySvr_table_shr_mem[i].username = NULL;
   
    if(proxySvrTable[i].password != -1)
      proxySvr_table_shr_mem[i].password = BIG_BUF_MEMORY_CONVERSION(proxySvrTable[i].password); 
    else
      proxySvr_table_shr_mem[i].password = NULL;
  }  
  NSDL2_PROXY(NULL, NULL, "Exitting Method.");
}

//Copying exception into shared memory 
inline void copy_proxyExcp_table_into_shr_mem(ProxyExceptionTable_Shr *proxyExcp_table_shr_mem)
{
  int i = 0;
  NSDL1_PROXY(NULL, NULL, "Method Called, proxyException_table_shr_mem = %p, total_proxy_excp_entries = %d",
                           proxyExcp_table_shr_mem, total_proxy_excp_entries);

  for(i = 0; i < total_proxy_excp_entries; i++)
  {
    proxyExcp_table_shr_mem[i].excp_type = proxyExcpTable[i].excp_type;
   
    NSDL2_PROXY(NULL, NULL, "Index = %d, Copying into excption shared memory excp_type=%d", 
                             i, proxyExcp_table_shr_mem[i].excp_type);
    if(proxyExcpTable[i].excp_type == EXCP_IS_DOMAIN_OR_HOSTNAME || proxyExcpTable[i].excp_type == EXCP_IS_WITH_WILDCARD)
    {
      proxyExcp_table_shr_mem[i].excp_val.excp_domain_name = BIG_BUF_MEMORY_CONVERSION(proxyExcpTable[i].excp_val.excp_domain_name);
      NSDL2_PROXY(NULL, NULL, "Index=%d, excp_domain_name=%s", i,  proxyExcp_table_shr_mem[i].excp_val.excp_domain_name);
    }
    else
    {
      memcpy(&(proxyExcp_table_shr_mem[i].excp_val.excp_addr), &(proxyExcpTable[i].excp_val.excp_addr), 
                       sizeof(struct sockaddr_in6)); 
      NSDL2_PROXY(NULL, NULL, "Index=%d, excp_addr = %s", 
                  i, nslb_sock_ntop((struct sockaddr *)&(proxyExcp_table_shr_mem[i].excp_val.excp_addr)));
    }
  }
  NSDL1_PROXY(NULL, NULL, "Exitting Method.");
}

//Copying proxy ip interfaces into shared memory 
inline void copy_proxyNetPrefix_table_into_shr_mem(ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem)
{
  int i;
  NSDL1_PROXY(NULL, NULL, "Method Called, proxyNetPrefix_table_shr_mem = %p, total_proxy_ip_interfaces = %d",
                           proxyNetPrefix_table_shr_mem, total_proxy_ip_interfaces);
  //memcpy(proxyNetPrefix_table_shr_mem, proxyNetPrefixId, sizeof(proxyNetPrefixId) * total_proxy_ip_interfaces);

  for(i = 0; i < total_proxy_ip_interfaces; i++)
  {
    if(proxyNetPrefixId[i].addr_family == AF_INET)
    {
      proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.subnet = proxyNetPrefixId[i].addr.s_IPv4.subnet;
      proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.net_prefix = proxyNetPrefixId[i].addr.s_IPv4.net_prefix;
      NSDL1_PROXY(NULL, NULL, "IPv4: Index = %d, subnet = %ld, net_prefix = %ld", 
                              i, proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.subnet,
                              proxyNetPrefix_table_shr_mem[i].addr.s_IPv4.net_prefix);
    }
    else
    {
      memcpy(proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.subnet, proxyNetPrefixId[i].addr.s_IPv6.subnet, 16);
      memcpy(proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.net_prefix, proxyNetPrefixId[i].addr.s_IPv6.net_prefix, 16);
      NSDL1_PROXY(NULL, NULL, "IPv6: Index = %d, subnet = %s, net_prefix = %s", 
                              i, proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.subnet,
                              proxyNetPrefix_table_shr_mem[i].addr.s_IPv6.net_prefix);
    }
  }
}


/*-------------------------------------------------------------------
 * Purpose : Will create table ProxyServerTable for storing information about proxy server
 * 	     * Starting two index is reserved for system and group ALL proxy
 * 	     * 0th index for system proxy
 * 	     * 1st index for case group ALL
 *
 * Input   : row_num :- provide to fill proxy index 
 * 
 * Output  : proxy index
 *-----------------------------------------------------------------*/

static int create_proxy_server_table_entry(int *row_num) 
{
  NSDL1_PROXY(NULL, NULL, "Method Called.");

  NSDL2_PROXY(NULL, NULL, "total_proxy_svr_entries = %d, max_proxy_svr_entries = %d", 
                           total_proxy_svr_entries, max_proxy_svr_entries);
  if (total_proxy_svr_entries == max_proxy_svr_entries) {
    MY_REALLOC_EX(proxySvrTable, (max_proxy_svr_entries + DELTA_PROXY_SVR_ENTRIES) * sizeof(ProxyServerTable), (max_proxy_svr_entries * sizeof(ProxyServerTable)), "proxySvrTable", -1); 
    if (!proxySvrTable) 
    {
      fprintf(stderr,"create_proxy_server_table_entry(): Error allocating more memory for proxy server entries\n");
      return PROXY_ERROR;
    } 
    else 
      max_proxy_svr_entries += DELTA_PROXY_SVR_ENTRIES;
  }
  *row_num = total_proxy_svr_entries++;

  NSDL2_PROXY(NULL, NULL, "row_num = %d", *row_num);
  return PROXY_SUCCESS;
}

/*-------------------------------------------------------------------
 * Purpose : Will create table ProxyServerExcpTable for storing information about proxy server exceptions
 *
 * Input   : row_num :- provide to fill proxy index
 *         : num_excp :- number of exceptions 
 * 
 * Output  : proxy index
 *-----------------------------------------------------------------*/
static int create_proxy_server_excp_table_entry(int *row_num) 
{
  NSDL1_PROXY(NULL, NULL, "Method Called.");

  NSDL2_PROXY(NULL, NULL, "total_proxy_excp_entries = %d, max_proxy_excp_entries = %d", 
                           total_proxy_excp_entries, max_proxy_excp_entries);

  if (total_proxy_excp_entries == max_proxy_excp_entries)
  {
    MY_REALLOC_EX(proxyExcpTable, (max_proxy_excp_entries + DELTA_PROXY_EXCP_ENTRIES) * sizeof(ProxyExceptionTable), (max_proxy_excp_entries * sizeof(ProxyExceptionTable)), "proxyExcpTable", -1); 
    if (!proxyExcpTable) 
    {
      fprintf(stderr,"create_proxy_server_excp_table_entry(): Error allocating more memory for proxy server entries\n");
      return PROXY_ERROR;
    } 
    else 
      max_proxy_excp_entries += DELTA_PROXY_EXCP_ENTRIES;
  }
  *row_num = total_proxy_excp_entries++;

  init_proxy_excp(*row_num); 
  NSDL2_PROXY(NULL, NULL, "row_num = %d", *row_num);
  return PROXY_SUCCESS;
}

//This is used to store the network prefix in the table that will be accessed by the pointer
static int create_proxy_bypass_table_net_pre_entry(int *row_num) 
{
  NSDL1_PROXY(NULL, NULL, "Method Called.");

  NSDL2_PROXY(NULL, NULL, "total_proxy_ip_interfaces = %d, max_proxy_svr_entries = %d", 
                           total_proxy_ip_interfaces, max_proxy_ip_interfaces);

  if (total_proxy_ip_interfaces == max_proxy_ip_interfaces) {
    MY_REALLOC_EX(proxyNetPrefixId, (max_proxy_ip_interfaces + DELTA_PROXY_INTERFACE_ENTRIES) * sizeof(ProxyNetPrefix), (max_proxy_ip_interfaces * sizeof(ProxyNetPrefix)), "proxyNetPrefixId", -1); 
    if (!proxyNetPrefixId) 
    {
      fprintf(stderr,"create_proxy_bypass_table_net_pre_entry(): Error allocating more memory for proxy server entries\n");
      return PROXY_ERROR;
    } 
    else 
      max_proxy_ip_interfaces += DELTA_PROXY_INTERFACE_ENTRIES;
  }
  *row_num = total_proxy_ip_interfaces++;

  NSDL2_PROXY(NULL, NULL, "row_num = %d", *row_num);
  return PROXY_SUCCESS;
}
/*-------------------------------------------------------------------
 * Purpose : Updates proxy index in runprof table
 *           * proxy_idx = 0 for System proxy
 *           * proxy_idx = 1 for ALL(group)
 *           * proxy_idx = -1 for no proxy
 *           * proxy_idx > 1 group wise specified proxy 
 *           
 * Input   : group_idx :- group index into runprof table
 *
 *           proxy_idx :- proxy index in ProxyServerTable 
 *
 * Output  : None           
 *-----------------------------------------------------------------*/
int update_proxy_index(int group_idx, int proxy_idx)
{
  int i = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, group_idx = %d, proxy_idx = %d, g_p_idx = %d, "
                          "proxySvrTable[0].http_proxy_server = %d, "
                          "proxySvrTable[0].https_proxy_server = %d," 
                          "proxySvrTable[1].http_proxy_server = %d, "
                          "proxySvrTable[1].https_proxy_server = %d", 
                           group_idx, proxy_idx, g_p_idx, 
                           proxySvrTable[0].http_proxy_server, proxySvrTable[0].https_proxy_server,
                           proxySvrTable[1].http_proxy_server, proxySvrTable[1].https_proxy_server);

  //In case group is ALL, update proxy_index for all groups to the proxy_index
  //proxy index in this case will be 0 (System Proxy) or 1(All Groups)
  if(group_idx == -1) //ALL
  {
    proxy_idx = g_p_idx;

    NSDL1_PROXY(NULL, NULL, "total_runprof_entries=%d, g_p_idx =%d", total_runprof_entries, g_p_idx);
    for (i = 0; i < total_runprof_entries; i++) 
    runProfTable[i].proxy_idx = proxy_idx; 
  }
  else
  {
    //Updates proxy index for a particular group
    runProfTable[group_idx].proxy_idx = proxy_idx;
  }

  if((proxy_idx == SYS_PROXY_IDX) && 
     (proxySvrTable[0].http_proxy_server == -1 && proxySvrTable[0].https_proxy_server == -1))
  {
    fprintf(stderr, "Error: System proxy used but not defined.\n");
    write_log_file(NS_SCENARIO_PARSING, "System proxy not defined, please define system proxy then re-run the test");
    return PROXY_ERROR; 
  }
  return PROXY_SUCCESS;
}

/*-------------------------------------------------------------------
 * Purpose : (1) Will segregate the port and server name from given address list (here hosttext)
 *
 * Input   : (1) req_type  :- this will tell the type of request i.e. request is http or https 
 *            
 *           (2) hosttext  :- will point to address list i.e. 192.168.1.69:80 or www.cavisson.com 
 *
 *           (4) hport     :- it is buffer to fill the error message 
 *
 * Output  : On success   0 
 *           On failure  -1 
 *-----------------------------------------------------------------*/
static char *check_and_add_default_port (int req_type, char *hosttext, int *hport)
{
  char *hptr;
  char *ptr;
  char *ip_ptr;
  int is_ipv6 = 0;
  char buff[512];

  buff[0] = '\0';

  strcpy(buff, hosttext);//Used this buffer just to show the wrong port value

  NSDL1_PROXY(NULL, NULL,"Method Called, req_type = %d, hosttext = [%s]", req_type, hosttext);

  if ((ptr = index(hosttext, ':')) && (index(ptr+1, ':')))
    is_ipv6 = 1;

  NSDL2_PROXY(NULL, NULL,"ipv6 flag value =%d, hosttext = [%s]", is_ipv6, hosttext);
  //Handling for ipv6 port checking
  if((ip_ptr = index(hosttext , ']')) && !strncmp(ip_ptr+1, ":",1))
  {
    NSDL2_PROXY(NULL, NULL,"ip_ptr=%s", ip_ptr);
    if (ip_ptr+2 != NULL)
    {
      if((!ns_is_numeric(ip_ptr + 2) || atoi(ip_ptr + 2) > 65535) && is_ipv6)
      {
        fprintf(stderr, "Error: Port given in address %s is not valid. Port must be positive integer and in between 1 - 65535 \n",  buff);
        return NULL;
      }
    }
  }  

  if(ptr != NULL)
  {
    if((!ns_is_numeric(ptr + 1) || atoi(ptr + 1) > 65535) && !is_ipv6)
    {
      fprintf(stderr, "Error: Port given in address %s is not valid. Port must be positive integer and in between 1 - 65535 \n",  buff);
      return NULL;
    }
  }
  hptr = nslb_split_host_port(hosttext, hport);

  if(!isdigit(*hport) && (*hport > 65535))
  {
    fprintf(stderr, "Not a valid value of port given in address=%s\nPort must be numeric and less than 65535\n", hosttext);
    return NULL; 
  }

  if (*hport == 0) {
    switch(req_type) {
      case HTTP_REQUEST:
        if(is_ipv6) {
          *hport = 6880;
        } else {
          *hport = 80;
        }
        NSDL2_PROXY(NULL, NULL, "Setting HTTP Port=%d", *hport);
        break;
      case HTTPS_REQUEST:
        if (is_ipv6) {
          *hport = 6443;
        }  else {
          *hport = 443;
        }
        NSDL2_PROXY(NULL, NULL, "Setting HTTPS Port=%d", *hport);
        break;
      default:
        fprintf(stderr, "read_keywords(): In SERVER_HOST, unknown request type\n");
        return NULL;
    }
  }
  return hptr;
}

/*-------------------------------------------------------------------
 * Purpose : (1) Will fill entries into ProxyServerTable 
 *
 * Input   : (1) req_type  :- this will tell the type of request i.e. request is http or https 
 *            
 *           (2) proxy_idx :- will indicate proxy index for which entry have to be filled
 *
 *           (3) adds_ptr  :- will point to address list i.e. 192.168.1.69:80 or www.cavisson.com 
 *
 *           (4) err_msg   :- it is buffer to fill the error message 
 *
 * Output  : On success   0 
 *           On failure  -1 
 *-----------------------------------------------------------------*/
static int fill_proxy_table(int req_type, int proxy_idx, char *adds_ptr, char *err_msg)
{
  int port = 0, ret;
  char *ip_ptr;
  
  NSDL1_PROXY(NULL, NULL, "Method called, req_type = %d, proxy_idx = %d, adds_ptr = [%s]", req_type, proxy_idx, adds_ptr); 
  if(req_type == HTTP_REQUEST)
  {
    if((ip_ptr = check_and_add_default_port(req_type, adds_ptr, &port)) == NULL)
      return PROXY_ERROR;

    if((proxySvrTable[proxy_idx].http_proxy_server = copy_into_big_buf(ip_ptr, 0)) == -1) {
      sprintf(err_msg, "Failed in copying input file name into big buf\n");
      return PROXY_ERROR;
    }

    proxySvrTable[proxy_idx].http_port = port;
    //Handle return value for error
    ret = nslb_fill_sockaddr(&proxySvrTable[proxy_idx].http_addr, ip_ptr, port);
    if (ret == 0)
      return PROXY_ERROR;  
  }
  else if(req_type == HTTPS_REQUEST)
  {
    if((ip_ptr = check_and_add_default_port(req_type, adds_ptr, &port)) == NULL)
      return PROXY_ERROR;

    if ((proxySvrTable[proxy_idx].https_proxy_server = copy_into_big_buf(ip_ptr, 0)) == -1) {
      sprintf(err_msg, "Failed in copying input file name into big buf\n");
      return PROXY_ERROR;
    }

    proxySvrTable[proxy_idx].https_port = port;
    ret = nslb_fill_sockaddr(&proxySvrTable[proxy_idx].https_addr, ip_ptr, port);
    if (ret == 0)
      return PROXY_ERROR;
  }
  else
  {
    fprintf(stderr, "Warning: Not supported protocol for proxy.");
    NS_DUMP_WARNING("Not supported protocol for proxy.");
  }
  
  return PROXY_SUCCESS;
}

/*-------------------------------------------------------------------
 * Purpose : (1) Will parse the address list
 * 	         * In address list any protocol cann't repeate 
 * 	            Eg: http=192.168.1.39:80;https=192.168.1.66;http=192.168.1.69:8800
 * 	         * Space is not allowed within address list
 *
 *           (2) Validate the the ip and port
 *               * port should be whole number less than 65535
 *
 *           (3) Fill the ProxyServerTable    
 *
 * Input   : (1) proxy_svr_idx :- proxy index into ProxyS 
 *
 *           (2) add_list      :- contains proxy address list
 *               Eg:
 *                http=192.168.1.66:80;https=192.168.1.69:8800
 *                http=www.cavisson.com:80;https=192.168.1.69:8800
 *                all=192.168.1.66:80;
 *
 *           (3) err_msg       :- buffer for error message
 *
 * Output  : On success   0 
 *           On failure  -1 
 *-----------------------------------------------------------------*/
static int parse_and_validate_addr_list(int proxy_svr_idx, char *add_list, char *err_msg, char *buf, int runtime_flag)
{
  int tot_adds_fields = 0;
  char *adds_fields[5];
  int i = 0;
  char *adds_ptr = NULL;
  int aflag = 0;        /* Here these three flags aflag, hflag, hsflag are taken to check the */
  int hflag = 0;        /* duplicacy of the keywords http, https and to check that all keyword */ 
  int hsflag = 0;       /* is specified standalone. */
  char tmp_buff[512];   /*This buffer is taken because we are filling both http and https addresses 
                          and if we send the original buffer first time it is segregated and then if
                          we resend it second time  then core dump happens */

  NSDL1_PROXY(NULL, NULL, "Method Called., proxy_svr_idx = %d, add_list = [%s]", 
                           proxy_svr_idx, add_list?add_list:NULL);

  CLEAR_WHITE_SPACE_FROM_END(add_list);

  /*
  if((add_list[strlen(add_list) - 1] != ';') && (!isalnum(add_list[strlen(add_list) - 1]))) 
  {
    ns_proxy_usage("Address list must end with semicolon(;) or with nothing",err_msg);
    return PROXY_ERROR; 
  }

  //If in add_list semicolon is at last then terminate it
  if(add_list[strlen(add_list) - 1] == ';')
    add_list[strlen(add_list) - 1] = '\0';  
 */
  //Here we are calling get_tokens function that will return the number of tokens found in buffer.  
  tot_adds_fields = get_tokens(add_list, adds_fields, ";", 5);

  NSDL2_PROXY(NULL, NULL, "total fields=%d", tot_adds_fields);

  //Address-List cannot have more protocols than supported 
  if(tot_adds_fields > TOT_FIELDS || tot_adds_fields <= 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "Address List cannot have more than 2 protocols");
  }

  for(i=0; i < tot_adds_fields; i++)
  {
    NSDL2_PROXY(NULL, NULL, "token at %d  =%s", i, adds_fields[i]);
    adds_ptr = adds_fields[i];
    CLEAR_WHITE_SPACE(adds_ptr); 
    CLEAR_WHITE_SPACE_FROM_END(adds_ptr);
    //TODO:HTTP, HTTPs should not be repeated 
    //TODO:ALL=xxx ; HTTP=yyy;   - Should not be allowed
    //Semicolon should be used to separate protocols.
    //http=xxx https=yyy
    //TODO: Add flag check in if
    NSDL3_PROXY(NULL, NULL, "adds_ptr = [%s]", adds_ptr);
    if(!strncasecmp(adds_ptr, "all", 3))   
    {
     
      if(aflag == 1)
      {
        fprintf(stderr, "Error: In address list 'ALL' is used standalone.\n");
        return PROXY_ERROR; 
      }
     
      aflag++;
      //'ALL' will be occuring only first time
      if(i)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "In addess list, 'ALL' cannot be clubbed with any other protocol");
      }
  
      adds_ptr += 3;
      CLEAR_WHITE_SPACE(adds_ptr); 
    
      //If the delimiter is not '=' then error will be displayed 
      if (*adds_ptr != '=')
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "Invalid delimeter provided");
      }
 
      adds_ptr++;  //escaping the '=' sign
      CLEAR_WHITE_SPACE(adds_ptr); 

      strcpy(tmp_buff, adds_ptr);

      //For Http
      if(fill_proxy_table(HTTP_REQUEST, proxy_svr_idx, adds_ptr, err_msg) == PROXY_ERROR)
        return PROXY_ERROR;

      //For Https
      if(fill_proxy_table(HTTPS_REQUEST, proxy_svr_idx, tmp_buff, err_msg) == PROXY_ERROR)
        return PROXY_ERROR;
    }
    else if (!strncasecmp(adds_ptr, "https", 5))
    {
      //Checking if the HTTPS protocol is specified after 'ALL' keyword or not
      if(aflag == 1)
      {
        fprintf(stderr, "Error: HTTPS protocol cannot be specified after the 'ALL' keyword is given.\n");   
        return PROXY_ERROR; 
      }
 
      if(hsflag == 1)
      { 
        fprintf(stderr, "Error: In address list 'HTTPS' protocol cannot be specified more than once.\n");
        return PROXY_ERROR; 
      }

      hsflag++;
      adds_ptr += 5;
      CLEAR_WHITE_SPACE(adds_ptr);
      adds_ptr++;//escaping the '=' sign here
      CLEAR_WHITE_SPACE(adds_ptr);

      if(fill_proxy_table(HTTPS_REQUEST, proxy_svr_idx, adds_ptr, err_msg) == PROXY_ERROR)
        return PROXY_ERROR;
    }
    else if (!strncasecmp(adds_ptr, "http", 4))
    {
      //Checking if HTTP keyword is specified after 'ALL' or not
      if(aflag == 1)
      {
        fprintf(stderr, "Error: HTTP protocol cannot be specified after the 'ALL' keyword is given.\n");    
        return PROXY_ERROR; 
      }
 
      if(hflag == 1) 
      {
        fprintf(stderr, "Error: In address list 'HTTP' protocol cannot be specified more than once.\n");
        return PROXY_ERROR; 
      }
     
      hflag++;
      adds_ptr += 4;
      CLEAR_WHITE_SPACE(adds_ptr);
      adds_ptr++;//escaping the '=' sign here.
      CLEAR_WHITE_SPACE(adds_ptr);
    
      if(fill_proxy_table(HTTP_REQUEST, proxy_svr_idx, adds_ptr, err_msg) == PROXY_ERROR)
        return PROXY_ERROR;
    }
    else
    { 
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "Invalid/Unsupported protocol provided in adress list");
    }
  }
  
  return PROXY_SUCCESS;
}

/* This function will tokenize the given exception list and return the number of tokens & fill excp_tok.*/
static int get_proxy_excp_tokens (char *excp_tok[], char *excp_list, char *err_msg)
{
  int tot_ex_fields = 0;

  NSDL1_PROXY(NULL, NULL,"Method Called. Exception list = %s", excp_list);

  //Checking if excp_list ends with semicolon or not
  //if((excp_list[strlen(excp_list) - 1] != ';') && (!isalnum(excp_list[strlen(excp_list) - 1])) && (excp_list[strlen(excp_list) - 1] != '*'))
   // return PROXY_ERROR; 

  //If in ex_list semicolon is at last then terminate it
  //if((excp_list[strlen(excp_list) - 1] == ';'))
  //  excp_list[strlen(excp_list) - 1] = '\0';  

  //Here we are calling get_tokens function that will return the number of tokens found in buffer.  
  tot_ex_fields = get_tokens(excp_list, excp_tok, ";", MAX_EXCPS_ACCEPTED);  

  NSDL2_PROXY(NULL, NULL, "total fields=%d, first token=%s", 
                           tot_ex_fields, excp_tok[0]);

  return tot_ex_fields;
}

/* This function will check for hostname or domain name and port and populates them into structures.*/    
static int check_proxy_excp_and_populate_structures(int row_idx, char *excp_tok, char *err_msg)
{
  char *hptr;
  int excp_port;
  int hport = 0;
  char buff[512];

  strcpy(buff, excp_tok);

  NSDL1_PROXY(NULL, NULL,"Method Called. row_idx=%d, excp_tok=%s", row_idx, excp_tok); 

  // 1. Only port is provide in exception list eg: (:80)
  if(excp_tok[0] == ':')   
  {
    //Checking for a valid port.
    //Make MACRO - IS_VALID_PORT
    if(!ns_is_numeric(excp_tok + 1))
    {
      fprintf(stderr, "Error:Not a valid Port No. in %s. Port number should be numeric", excp_tok);
      return PROXY_ERROR;
    }

    excp_port = atoi(excp_tok + 1);
    if ((excp_port == 0) || (excp_port > 65535))
    {
      fprintf(stderr, "Error:Not a valid Port No. in %s. Valid port no. can be between 1-65535", excp_tok);
      return PROXY_ERROR;
    }
    NSDL2_PROXY(NULL, NULL,"Exception token is port %d", excp_port);
    //Setting the proxy exception table type to 1 means the case of port.  
    proxyExcpTable[row_idx].excp_type = EXCP_IS_PORT_ONLY;
    proxyExcpTable[row_idx].excp_val.excp_addr.sin6_port = excp_port;
    proxyExcpTable[row_idx].excp_val.excp_addr.sin6_family = AF_INET6;
    NSDL2_PROXY(NULL, NULL,"exception type=%d(port) for exception index=%d, excp_val=%d", 
                         proxyExcpTable[row_idx].excp_type, row_idx, 
                        proxyExcpTable[row_idx].excp_val.excp_addr.sin6_port);
  }

  char *tmp_ptr = strchr(buff, '*');
  if(tmp_ptr != NULL )
  {
    NSDL2_PROXY(NULL, NULL,"Case of wildcard found. Exception given=%s", excp_tok);
    if((proxyExcpTable[row_idx].excp_val.excp_domain_name = copy_into_big_buf(excp_tok, 0)) == -1) {
      sprintf(err_msg, "Failed in copying input file name into big buf\n");
      return PROXY_ERROR;
    }  
    //Setting the proxy exception table type to 4 means its a case of domain. 
    proxyExcpTable[row_idx].excp_type = EXCP_IS_WITH_WILDCARD;
  } 
  else   //Checking for IP, IP & Port & domainname
  {
    char orig_buf[512];
    //Taken this buffer as we need the original buffer in case the domain name or hostname with port is given.
    strcpy(orig_buf, excp_tok);
    NSDL2_PROXY(NULL, NULL,"Checking for IP, IP & Port & domainname in excp_token [%s]", excp_tok);
    if((hptr = nslb_split_host_port(excp_tok, &hport)) == NULL)
    {
      NSDL2_PROXY(NULL, NULL,"In case of ip address hptr value is = %s", hptr); 
      return PROXY_ERROR;
    }
    NSDL2_PROXY(NULL, NULL,"excp_token splitted=%s, hptr=%s hport=%d", excp_tok, hptr, hport);

    //MACRO IS_VALID_PORT
    
    if(!isdigit(hport) && (hport > 65535))
    {
      fprintf(stderr, "Not a valid value of port given in address=%s\nPort must be numeric and less than 65535\n", excp_tok);
      return PROXY_ERROR; 
    }
    NSDL2_PROXY(NULL, NULL,"The exception token value is: %s", excp_tok);
    int check_val = is_valid_ip(hptr);
    NSDL2_PROXY(NULL, NULL,"Valid IP %s", check_val?"Present":" Not Present");
      
    if(check_val && (hport > 0))
    { 
      NSDL2_PROXY(NULL, NULL,"Both IP & Port are present in excp token [%s]", excp_tok);
      //2. Case of both ip and port.
      //proxyExcpTable[row_idx].excp_val.excp_addr = hport;
      //Setting the proxy exception table type to 2 means case of ip and port given in exception list.

      NSDL2_PROXY(NULL, NULL,"exception_port=%c", proxyExcpTable[row_idx].excp_type);

      int ret = nslb_fill_sockaddr(&proxyExcpTable[row_idx].excp_val.excp_addr, hptr, hport);
      if (ret == 0)
      {
        NSDL2_PROXY(NULL, NULL,"ret value for ip and port in ns_fill_sockaddr = %d", ret); 
        return PROXY_ERROR; 
      }
      proxyExcpTable[row_idx].excp_type = EXCP_IS_IP_WITH_PORT;
      NSDL2_PROXY(NULL, NULL,"exception type=%d(IP & Port) for exception index=%d, excp_val=%s", 
                         proxyExcpTable[row_idx].excp_type, row_idx, 
                        nslb_sock_ntop((struct sockaddr *)&proxyExcpTable[row_idx].excp_val.excp_addr));
    }
    // 3. Only IP present
    else if(check_val && (hport == 0))
    {
      //Setting the proxy exception table type to 0 means case of ip only
      int ret = nslb_fill_sockaddr(&proxyExcpTable[row_idx].excp_val.excp_addr, hptr, 0);
      if (ret == 0){
        NSDL2_PROXY(NULL, NULL,"ret value for ip in ns_fill_sockaddr = %d", ret); 
        return PROXY_ERROR;
      }
      proxyExcpTable[row_idx].excp_type = EXCP_IS_IP_ONLY;
      NSDL2_PROXY(NULL, NULL,"exception type=%d(IP Only) for exception index=%d, excp_val=%s", 
                         proxyExcpTable[row_idx].excp_type, row_idx, 
                        nslb_sock_ntop((struct sockaddr *)&proxyExcpTable[row_idx].excp_val.excp_addr));
    }
    // 4. Assume domain name or hostname presence 
    else
    {
      NSDL3_PROXY(NULL, NULL,"Copying domain name '%s' into Exception list.", orig_buf);
      if((proxyExcpTable[row_idx].excp_val.excp_domain_name = copy_into_big_buf(orig_buf, 0)) == -1) {
        sprintf(err_msg, "Failed in copying input file name into big buf\n");
        return PROXY_ERROR;
      }  
      //Setting the proxy exception table type to 3 means its a case of domain. 
      proxyExcpTable[row_idx].excp_type = EXCP_IS_DOMAIN_OR_HOSTNAME;
      NSDL2_PROXY(NULL, NULL,"exception type=%d(Domain Name) for exception index=%d, excp_val=%s", 
                         proxyExcpTable[row_idx].excp_type, row_idx, 
                         RETRIEVE_BUFFER_DATA(proxyExcpTable[row_idx].excp_val.excp_domain_name));
    }
    NSDL2_PROXY(NULL, NULL,"proxyExcpTable[row_idx].excp_type =%d", proxyExcpTable[row_idx].excp_type); 
  }
  return PROXY_SUCCESS;
}

/* This function will check if System proxy is defined for the group or not.*/ 
static int is_sys_proxy_set()
{
  NSDL1_PROXY(NULL, NULL,"Method Called, g_p_idx = %d", g_p_idx?"ALL_PROXY_IDX":"ALL_PROXY_IDX");
  if(g_p_idx == SYS_PROXY_IDX)
    if((proxySvrTable[0].http_proxy_server == -1) && (proxySvrTable[0].https_proxy_server == -1))
    {
      fprintf(stderr,"Warning: System proxy not defined so exceptions/authetication cannot be applied" );
      NS_DUMP_WARNING("System proxy not defined so exceptions/authetication cannot be applied");
      return -1; 
    }
  NSDL1_PROXY(NULL, NULL,"System proxy is defined..Will parse other SYSTEM PROXY keywords");
  return 0;
}

/* This function will check if group proxy is defined for the group or not.*/
static int is_gp_proxy_set(int group_idx)
{
  NSDL1_PROXY(NULL, NULL,"Method Called, group_idx = %d", group_idx);

  if(group_idx == -1) 
  { 
    //In case of ALL, check if proxy is defined (system or manual)
    //In case proxy is not defined, give warning msg an ......
    //if(g_p_idx == -1 || g_p_idx == 0)
    if(g_p_idx != 1)
    {
      //Not throwing warning as G_PROXY_AUTH has default username/passwd.
      //To add warning need to add check for hardcoded values of default username/pwd 
      fprintf(stderr,"Warning: Default proxy not defined so exceptions/authentication cannot be applied\n"); 
      return -1;
    }
    NSDL1_PROXY(NULL, NULL,"Returning 1 as all case is found.");
    return 1;
  }
 
  if(group_idx > 0)
   if(runProfTable[group_idx].proxy_idx < 0)
   {
     fprintf(stderr, "Warning: Group proxy not defined so exceptions/authetnication cannot be applied\n");
     return -1;  
   }
  
  //In case group goes through system proxy its exception cannot be defined by G_PROXY_EXCEPTIONS    
  if(runProfTable[group_idx].proxy_idx == 0){
    fprintf(stderr, "Warning: Group goes through system proxy so its exception/authetnication must be defined with the keyword SYSTEM_PROXY_EXCEPTIONS\n");
    return -1;
  }
   
  if(runProfTable[group_idx].proxy_idx == 1){
    fprintf(stderr, "Warning: Exceptions/Authentications can't be set on a particular group as we are using default proxy."); 
    return -1;
  }

  NSDL2_PROXY(NULL, NULL,"Retrns Proxy index = %d", runProfTable[group_idx].proxy_idx); 
  return runProfTable[group_idx].proxy_idx; 
}


#if 0
/* This function will calculate the network prefix of the system interface ip addresses*/
int cal_net_prefix_of_system(char *ip_buf, char *subnet_mask)
{
  char *tok;
  unsigned int out_ip = 0;
  unsigned int out_mask = 0;
  int row_no = 0;

  NSDL1_PROXY(NULL, NULL,"Method Called.");
  unsigned int int_ip = ns_ip_addr (ip_buf, &out_ip);//Storing the unsigned int value of ip address.
  unsigned int int_mask = ns_ip_addr (subnet_mask, &out_mask);//Storing the unsigned int value of mask.

  NSDL2_PROXY(NULL, NULL,"int_ip=%u, int_mask=%u", int_ip, int_mask); 
  if((int_ip == -1) || (int_mask == -1)) 
    return PROXY_ERROR;
  else
  {
    unsigned int net_pre_for_sys_ip = out_ip & out_mask;//Anding the unsigned int value of ip address with the mask.
    //This function will create the table for storing the network prefix of system ip.
    create_proxy_bypass_table_net_pre_entry(&row_no);
    proxyNetPrefixId[row_no].net_prefix = net_pre_for_sys_ip; 
    proxyNetPrefixId[row_no].subnet = int_mask; 
    NSDL2_PROXY(NULL, NULL,"proxyNetPrefixId[%d].net_prefix=%u, proxyNetPrefixId[%d].subnet=%u, net_pre_index=%d", row_no, proxyNetPrefixId[row_no].net_prefix, row_no, proxyNetPrefixId[row_no].subnet, row_no);
    continue;
  }
  return 0;
}

char * ns_char_ip (unsigned int addr, int keepBits)
{
  int mask = (0xffffffff >> (32 - keepBits )) << (32 - keepBits);
  static char str_address[16];
  unsigned int a, b, c,d;
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  printf("netmask:%s\n", str_address);
  return str_address;
}
#endif


//This function will check if the network prefix already exists in the network prefix table or not.
//Returns 0 if teh match is found and -1 if not. 
static int check_if_subnet_is_in_list_ipv4(unsigned long net_prefix)
{
  int i;

  NSDL2_PROXY(NULL, NULL, "Method Called. Network prefix of network to be searched=%ld", net_prefix);
  
  for(i=0; i < total_proxy_ip_interfaces; i++) 
  {
    NSDL2_PROXY(NULL, NULL, "Network Prefix=%ld at %d in proxyNetPrefixId list", proxyNetPrefixId[i].addr.s_IPv4.net_prefix, i); 
    if(net_prefix == proxyNetPrefixId[i].addr.s_IPv4.net_prefix)
    {
      NSDL2_PROXY(NULL, NULL, "Network Prefix=%ld matched at index=%d", proxyNetPrefixId[i].addr.s_IPv4.net_prefix, i); 
      return 0;   
    }
  } 
  NSDL2_PROXY(NULL, NULL, "Network Prefix=%ld not found in proxyNetPrefixId list", net_prefix);
  return -1;
}

//This function will check if the network prefix already exists in the network prefix table or not.
//Returns 0 if the match is found and -1 if not.
static int check_if_subnet_is_in_list_ipv6(unsigned char net_prefix[])
{
  int i;
  //char addressOutputBuffer[INET6_ADDRSTRLEN];
  NSDL2_PROXY(NULL, NULL, "Method Called. Network prefix of network to be searched=%ld", net_prefix);

  for(i=0; i < total_proxy_ip_interfaces; i++)
  {
    //NSDL2_PROXY(NULL, NULL, "total_proxy_ip_interfaces = %d, Network Prefix=%s at %d in proxyNetPrefixId list", total_proxy_ip_interfaces, inet_ntop(AF_INET6, net_prefix, addressOutputBuffer, sizeof(addressOutputBuffer)), i);
    //if(net_prefix == proxyNetPrefixId[i].addr.s_IPv4.net_prefix)
    if(!memcmp(net_prefix, proxyNetPrefixId[i].addr.s_IPv6.net_prefix, 16))
    {
      NSDL2_PROXY(NULL, NULL, "Memcmp successfull as ipv6 matched an entry.so skipping this prefix");
      return 0;
    }
  }
  //NSDL2_PROXY(NULL, NULL, "Network Prefix=%s not found in proxyNetPrefixId list", inet_ntop(AF_INET6, net_prefix, addressOutputBuffer, sizeof(addressOutputBuffer)));
  return -1;
}


//This function will do byte by byte anding of the ip and subnet of ipv6 and thus calculate network prefix.
static inline void get_net6_pre(unsigned char addr[], unsigned char subnet[], unsigned char net_pre[])
{
  int i;
  NSDL1_PROXY(NULL, NULL, "Method Called.");
  for(i = 0; i < 16; i++)
  {
    net_pre[i] = addr[i] & subnet[i];
    NSDL4_PROXY(NULL, NULL, "byte=%d, addr=0x%x, subnet=0x%x, net_pre=0x%x", i, addr[i], subnet[i], net_pre[i]); 
  }
}

/* This function is used to bypass the local subnet addresses */
// Will get the unique network mask for all interfaces configured on the local system
static int cal_net_prefix_of_system()
{
  struct ifaddrs *interfaceArray = NULL, *nodeIfAddr = NULL;
  int rc = 0, row_no;
  unsigned long net_addr, net_pre, net_subnet;
  unsigned char net_addr6[16], net_pre6[16], net_subnet6[16];

  NSDL1_PROXY(NULL, NULL, "Method Called.");
  //getifaddrs creates linked  list (interfaceArray) of structures describing the network interfaces of the local system
  rc = getifaddrs(&interfaceArray);  /* retrieve the current interfaces */
  if(rc)
  {    
    fprintf(NULL, "Failed to fetch interface addresses. getifaddrs() failed with errno =  %d %s \n",
            errno, nslb_strerror(errno));
    return rc;
  }
  
  //Fetching subnet & IP & calculating network prefix for each interface  & storing it in ProxyNetPrefix
  for(nodeIfAddr = interfaceArray; nodeIfAddr != NULL; nodeIfAddr = nodeIfAddr->ifa_next)
  {
    NSDL3_PROXY(NULL, NULL, "nodeIfAddr = %p, sa_family = %d", nodeIfAddr, nodeIfAddr->ifa_addr->sa_family);
    //Checking for the family
    if(nodeIfAddr->ifa_addr->sa_family == AF_INET)
    {
      //Calculating the unsigned long value of ip and its subnet and anding them to get the network prefix of the ip. 
      net_addr = ntohl(((struct sockaddr_in *)nodeIfAddr->ifa_addr)->sin_addr.s_addr);
      net_subnet = ntohl(((struct sockaddr_in *)nodeIfAddr->ifa_netmask)->sin_addr.s_addr);
      net_pre = net_addr & net_subnet;
      NSDL2_PROXY(NULL, NULL, "IPV4 Address :net_addr=%ld, net_subnet=%ld, net_pre=%ld", net_addr, net_subnet, net_pre);
    
      //Checking here for the unique network prefix.
      if(check_if_subnet_is_in_list_ipv4(net_pre))
      {
        //Filling the subnet and network prefix into the structures.
        create_proxy_bypass_table_net_pre_entry(&row_no);
        proxyNetPrefixId[row_no].addr.s_IPv4.subnet = net_subnet; 
        proxyNetPrefixId[row_no].addr.s_IPv4.net_prefix= net_pre; 
        NSDL2_PROXY(NULL, NULL, "Subnet added in list. Subnet=%ld, Network Prefix=%ld", proxyNetPrefixId[row_no].addr.s_IPv4.subnet, proxyNetPrefixId[row_no].addr.s_IPv4.net_prefix);
      }
    }
    else if(nodeIfAddr->ifa_addr->sa_family == AF_INET6)
    {
      //Ipv6 found.
      NSDL2_PROXY(NULL, NULL, "IPv6 found so will calculate its net prefix");
      memcpy ((char *)net_addr6 ,(char *)(((struct sockaddr_in6 *)nodeIfAddr->ifa_addr)->sin6_addr.s6_addr), 16);
      memcpy ((char *)net_subnet6,((struct sockaddr_in6 *)nodeIfAddr->ifa_netmask)->sin6_addr.s6_addr, 16);
      
      //Calculating the network prefix by doing byte to byte anding.
      get_net6_pre(net_addr6, net_subnet6, net_pre6);
     
      if(check_if_subnet_is_in_list_ipv6(net_pre6))
      {
        //Filling the subnet and network prefix into the structures.
        create_proxy_bypass_table_net_pre_entry(&row_no);
        memcpy(proxyNetPrefixId[row_no].addr.s_IPv6.subnet, net_subnet6, 16);
        memcpy(proxyNetPrefixId[row_no].addr.s_IPv6.net_prefix ,net_pre6, 16);
#ifdef NS_DEBUG_ON       
        char addressOutBuf[INET6_ADDRSTRLEN];
        NSDL2_PROXY(NULL, NULL, "Subnet added in list. Subnet=%s, Network Prefix=%s", inet_ntop(AF_INET6, proxyNetPrefixId[row_no].addr.s_IPv6.subnet, addressOutBuf, sizeof(addressOutBuf)), inet_ntop(AF_INET6, proxyNetPrefixId[row_no].addr.s_IPv6.net_prefix, addressOutBuf, sizeof(addressOutBuf)));
#endif
        
      }
    }
  }
  freeifaddrs(interfaceArray);             /* free the dynamic memory */
  interfaceArray = NULL;                   /* prevent use after free  */
  return PROXY_SUCCESS;
}

//Return: -1 on error and 0 on success
//Description: This function will check for the total proxy interfaces and if found then will calculate network prefix of all the IPs assigned to the system. 
static int set_bypass_value(int grp_idx)
{
  int ret = 0;

  //Add check for ALL
  NSDL2_PROXY(NULL, NULL,"Method Called, grp_idx = %d", grp_idx);

  //Getting network prefix in case not done earlier 
  //in which case total_proxy_ip_interfaces will be 0
  if(!total_proxy_ip_interfaces)
  {
    if((ret = cal_net_prefix_of_system()) == PROXY_ERROR)
    {
      NSDL2_PROXY(NULL, NULL,"Invalid IP address found");
      return PROXY_ERROR;
    }
    NSDL2_PROXY(NULL, NULL, "Total interfaces parsed = %d", total_proxy_ip_interfaces);
  }
  else
  {
    NSDL2_PROXY(NULL, NULL, "Already parsed interfaces = %d", total_proxy_ip_interfaces);
  }

  return PROXY_SUCCESS;
} 

//Checks if a given ip address is valid or not.
//Returns 1 in case of valid IP, 0 otherwise
inline int is_valid_ip(char *ip_addr)
{
  struct in_addr inaddr;
  struct in6_addr in6addr;
  int ret;

  NSDL2_PROXY(NULL, NULL, "Method called, ip_addr = [%s]", ip_addr);

  if((ret = inet_pton(AF_INET, ip_addr, &inaddr)) == 1)
  {
    NSDL3_PROXY(NULL, NULL, "Ip '%s'(%ld) has version IPv4", ip_addr, inaddr);
    return IPv4;
  }
  else if((ret = inet_pton(AF_INET6, ip_addr, &in6addr)) == 1)
  { 
    NSDL3_PROXY(NULL, NULL, "Ip '%s'(%ld) has version IPv6", ip_addr, in6addr);
    return IPv6;
  }
  else
    return 0;
}


//conversion of net mask from int to hex and then to dotted form
static unsigned int get_subnet_bits(unsigned long addr, int keepbits, unsigned long *mask)
{
  unsigned long out;

  *mask = (0xffffffff >> (32 - keepbits )) << (32 - keepbits);
  NSDL2_PROXY(NULL, NULL,"subnet mask :0X%x", mask);
  out = addr & (*mask); 
  NSDL2_PROXY(NULL, NULL,"out :0X%x, (%ld)", out, out);

  return out;
/*
  unsigned int a, b, c,d;
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  NSDL2_PROXY(NULL, NULL,"netmask:%s\n", str_address);
  return str_address;
*/
}
                    
//Can be simplified later
//This function changes the integer value of netbits to unsigned char value.
static int get_ipv6_subnet_bits(unsigned char subnet[], int net_bits)
{
  NSDL2_PROXY(NULL, NULL,"Method Called");
  int quo=0, rem=0;
  int i,j;

  quo = net_bits/8;
  rem = net_bits%8;

  NSDL2_PROXY(NULL, NULL,"quotient = %d, remainder=%d", quo, rem);
  for(i=0; i< quo; i++)
    subnet[i] |= 255;

  for(j=0; j< rem; j++)
    subnet[i] |= (unsigned char)pow(2,(7-j));

  return PROXY_SUCCESS;
}

/*
static int get_ipv6_subnet_bits(unsigned char subnet[], int net_bits)
{

  NSDL2_PROXY(NULL, NULL,"Method Called");
  int quo, rem, i;
  quo = net_bits / 8;
  rem = net_bits % 8;

  NSDL2_PROXY(NULL, NULL,"quotient = %d, remainder=%d", quo, rem);
  for(i = 0; i <= quo ; i++)
  {
    if(i < quo)
    subnet[i] |= 0xFF;
    else if( i == quo )
    {
      int quo1, rem1;
      quo1 = rem / 4;
      rem1 = rem % 4;
      if(quo1 != 0)
      {
        switch(rem1)
        {
          case 0: subnet[i] |= 0xF0;break;
          case 1: subnet[i] |= 0xF8;break;
          case 2: subnet[i] |= 0xFC;break;
          case 3: subnet[i] |= 0xFE;break;
        }
      }
      else
      {
        switch(rem1)
        {
          case 0: subnet[i] |= 0x00;break;
          case 1: subnet[i] |= 0x08;break;
          case 2: subnet[i] |= 0x0C;break;
          case 3: subnet[i] |= 0x0E;break;
        }
      }
    }
  }
  return PROXY_SUCCESS;
}
*/

//Tokenizes the exception list and populates values into structures.
static int tokenize_excp_list_and_populate_structures(char *excp_list, int ret_proxy_idx, char *err_msg)
{
  int num_excp = 0; 
  char *excp_tok[256];
  int row_no = 0;
  int num_subnets = 0;
  unsigned long out_host, subnet;
  unsigned char net_pre6[16], subnet_mask6[16];
  struct in6_addr ipv6_addr;
  int i; 
  char *ptr;

  NSDL2_PROXY(NULL, NULL,"Method Called.");
  num_excp = get_proxy_excp_tokens(excp_tok, excp_list, err_msg);  
  if(num_excp == PROXY_ERROR) 
  {
    ns_proxy_excp_usage("Exception list must end with semicolon(;) or blank",err_msg);
    return PROXY_ERROR;
  }

  NSDL2_PROXY(NULL, NULL,"number of exceptions = %d", num_excp);
  proxySvrTable[ret_proxy_idx].excp_start_idx = total_proxy_excp_entries;
  for (i = 0; i < num_excp; i++)
  {
    CLEAR_WHITE_SPACE(excp_tok[i]);
    CLEAR_WHITE_SPACE_FROM_END(excp_tok[i]);
    //Checking if any subnet address exists in the address list or not.
    if ((ptr = strchr(excp_tok[i], '/')) != NULL)
    {
      NSDL2_PROXY(NULL, NULL,"excp_tok[i]=%s, ptr=%s", excp_tok[i], ptr);
      int bits = atoi(ptr + 1);
      *ptr = 0;
      //Check here for IPv4/IPv6
      //if(is_valid_ip(excp_tok[i]) == IPv4)
      if(!ns_ip_addr (excp_tok[i], (unsigned int *)&out_host)) //IPv4
      {
        NSDL2_PROXY(NULL, NULL,"excp_tok = [%s], bit = %d", excp_tok[i], bits);
        if (!bits)
        {
          ns_proxy_excp_usage("Invalid Subnet in exception list", err_msg);
          return PROXY_ERROR;
        }
        if ((bits > 0) || (bits < 32))
        {
          NSDL2_PROXY(NULL, NULL, "out_host = %ld", out_host); 
          unsigned long net_prefix = get_subnet_bits(out_host , bits, &subnet);
          NSDL2_PROXY(NULL, NULL,"subnet = %ld", subnet);
          create_proxy_bypass_table_net_pre_entry(&row_no);
          proxyNetPrefixId[row_no].addr_family = AF_INET;  
          proxyNetPrefixId[row_no].addr.s_IPv4.subnet = subnet; 
          proxyNetPrefixId[row_no].addr.s_IPv4.net_prefix= net_prefix; 
          proxySvrTable[ret_proxy_idx].bypass_subnet = '1';//Setting the proxy table bypass flag

          NSDL2_PROXY(NULL, NULL, "fill netPrefixId table at index (%d) with subnet val (%ld) and net_prefix val (%ld) ", row_no, proxyNetPrefixId[row_no].addr.s_IPv4.subnet, proxyNetPrefixId[row_no].addr.s_IPv4.net_prefix);
        }
      }
      else //IPv6
      {
        NSDL2_PROXY(NULL, NULL,"Token may be IPv6 :excp_token = [%s]", excp_tok[i]);
        if(!bits)
        {
          ns_proxy_excp_usage("Invalid Subnet in exception list", err_msg);
          return PROXY_ERROR;
        }
        if(bits < 128)
        {
          int ret = inet_pton(AF_INET6, excp_tok[i], &ipv6_addr);
          if(ret != 1)
          {
            NSDL2_PROXY(NULL, NULL,"Ipv6 format not recoznized");
            ns_proxy_excp_usage("Wrong subnet defined", err_msg);
            return PROXY_ERROR;
          }
          //This function will get the subnet in unsigned character form.
          if(get_ipv6_subnet_bits(subnet_mask6, bits) == 0)
          {
            //This function get the network prefix of ip by doing byte to byte anding.
            get_net6_pre((&ipv6_addr)->s6_addr, subnet_mask6, net_pre6);
            proxySvrTable[ret_proxy_idx].bypass_subnet = '1';//Setting the proxy table bypass flag
            if(check_if_subnet_is_in_list_ipv6(net_pre6))
            {
              //Filling the subnet value and network prefix in the structure.
              create_proxy_bypass_table_net_pre_entry(&row_no);
              memcpy(proxyNetPrefixId[row_no].addr.s_IPv6.subnet, subnet_mask6, 16);
              memcpy(proxyNetPrefixId[row_no].addr.s_IPv6.net_prefix ,net_pre6, 16);
              proxyNetPrefixId[row_no].addr_family = AF_INET6;
              NSDL2_PROXY(NULL, NULL,"IPv6 values filled in the structures bypass value set =%c, having index proxy_idx=%d", proxySvrTable[ret_proxy_idx].bypass_subnet, ret_proxy_idx);
            }
          }
        }
        else
        {
          ns_proxy_excp_usage("Invalid Subnet in exception list for IPv6", err_msg);
          return PROXY_ERROR;
        }
      }   
      num_subnets++;
      continue;
    }
 
    create_proxy_server_excp_table_entry(&row_no); 

    if(check_proxy_excp_and_populate_structures(row_no, excp_tok[i], err_msg) == PROXY_ERROR)
      return PROXY_ERROR;
  }
  
  proxySvrTable[ret_proxy_idx].num_excep = num_excp - num_subnets;
  NSDL2_PROXY(NULL, NULL,"ret_proxy_idx = %d, excp_start_index=%d, number_of_exceptions=%d, num_subnets = %d", 
                          ret_proxy_idx, 
                          proxySvrTable[ret_proxy_idx].excp_start_idx, 
                          proxySvrTable[ret_proxy_idx].num_excep - num_subnets, num_subnets);
  return PROXY_SUCCESS;

}

/* This function will parse the group proxy exceptions and fill the values in the structure.*/
int kw_set_g_proxy_exceptions (char *buf, int group_idx, char *err_msg, int runtime_flag)
{
  char key[MAX_PROXY_KEY_LEN];
  char g_name[MAX_PROXY_KEY_LEN];
  char bypass_mode[MAX_MODE_LEN]; 
  char excp_list[MAX_ADD_LIST_LEN];  
  int num_fld = 0;

  char *cur_buf_ptr, *buf_ptr;

  NSDL1_PROXY(NULL, NULL,"Method Called, group_idx = %d, buf = [%s]", group_idx, buf);

  key[0] = 0, g_name[0] = 0, bypass_mode[0] = 0, excp_list[0] = 0;
  num_fld = sscanf(buf, "%s %s %s", key, g_name, bypass_mode);
  
  NSDL2_PROXY(NULL, NULL,"buffer=%s, Keyword=%s, group_name=%s, bypass_mode=%s", buf, key, g_name, bypass_mode);

  if( num_fld < 3 ) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_EXCEPTIONS_USAGE, CAV_ERR_1011099, CAV_ERR_MSG_1);
  }

  val_sgrp_name(buf, g_name, 0); //This function checks the valid group name

  if(!ns_is_numeric(bypass_mode))
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_EXCEPTIONS_USAGE, CAV_ERR_1011099, CAV_ERR_MSG_2);

  if((atoi(bypass_mode)) != 0 && (atoi(bypass_mode) != 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_EXCEPTIONS_USAGE, CAV_ERR_1011099, CAV_ERR_MSG_3);
  }

  //Handling spaces in Exception List
  //Get exception list G_PROXY_EXCEPTIONS G3 1 192.168.1.66
  buf_ptr = buf;
  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;            //pointing group G3
  CLEAR_WHITE_SPACE(buf_ptr);    

  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;           //pointing mode
  CLEAR_WHITE_SPACE(buf_ptr); 

  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  if(cur_buf_ptr != NULL)
    buf_ptr = cur_buf_ptr;           //pointing exception list
  else
    buf_ptr++;
 
  CLEAR_WHITE_SPACE(buf_ptr);
  
  NSDL2_PROXY(NULL, NULL,"buf_ptr = [%s]", buf_ptr);
 
  strcpy(excp_list, buf_ptr); 
  CLEAR_WHITE_SPACE_FROM_END(excp_list);

  NSDL2_PROXY(NULL, NULL,"group_idx = %d, bypass_mode=%d", group_idx, atoi(bypass_mode));
  if((cur_buf_ptr == NULL) && (atoi(bypass_mode) == 0 ))
  {
    NSDL2_PROXY(NULL, NULL,"Since the default value has no address list so skipping it(i.e. G_PROXY_EXCEPTIONS ALL 0)");  
    return PROXY_SUCCESS;
  }
  
  int ret_proxy_idx = is_gp_proxy_set(group_idx);
  
  if (ret_proxy_idx == -1)
  {
    NSDL2_PROXY(NULL, NULL,"ret_proxy_idx = %d, so returning as group|ALL proxy might not be set", ret_proxy_idx);
    return PROXY_SUCCESS;
  }
 
  if((atoi(bypass_mode)) == 1)
  {
    if(set_bypass_value(group_idx) == PROXY_ERROR)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_EXCEPTIONS_USAGE, CAV_ERR_1011099, "Invalid IP address");

    if(group_idx == -1)
    {
      proxySvrTable[1].bypass_subnet = '1';
      NSDL2_PROXY(NULL, NULL,"All case found structure filled with value=%c", proxySvrTable[1].bypass_subnet); 
    }
    else
    {
      proxySvrTable[runProfTable[group_idx].proxy_idx].bypass_subnet = '1';//Setting the proxy table bypass flag
      NSDL2_PROXY(NULL, NULL,"Setting the bypass value for particular group"); 
    }
  }

  if(excp_list[strlen(excp_list) - 1] == '\n')
    excp_list[strlen(excp_list) - 1] = 0;
  
  NSDL1_PROXY(NULL, NULL,"Parsing Proxy Exception list for Group=%d", group_idx);
  if(cur_buf_ptr != NULL){
    if(tokenize_excp_list_and_populate_structures(excp_list, ret_proxy_idx, err_msg) == PROXY_ERROR)
      return PROXY_ERROR;
  }

  return PROXY_SUCCESS;
}

/* This function will parse the System proxy exceptions and fill the corresponding values in the structure.*/
int kw_set_system_proxy_exceptions (char *buf, char *err_msg)
{  
  char key[MAX_PROXY_KEY_LEN];
  char excp_list[MAX_ADD_LIST_LEN];  
  char bypass_mode[MAX_MODE_LEN]; 
  int num_fld = 0;
  int ret_val = 0;
  char *cur_buf_ptr, *buf_ptr;
   
  NSDL1_PROXY(NULL, NULL,"Method Called");  

  key[0] = 0, bypass_mode[0] = 0, excp_list[0] = 0;
  num_fld = sscanf(buf, "%s %s", key, bypass_mode);

  NSDL2_PROXY(NULL, NULL,"Buffer=%s, key=%s, bypass_mode=%s", buf, key ,bypass_mode);

  if( num_fld < 2) 
  {
    ns_system_proxy_excp_usage("Need atleast 1 fields after the keyword SYSTEM_PROXY_EXCEPTIONS", err_msg);
    return PROXY_ERROR;
  }

  if(!ns_is_numeric(bypass_mode))
    if(ns_system_proxy_excp_usage("Mode must be numeric.\n", err_msg) == -1)
      return PROXY_ERROR;

  if((atoi(bypass_mode) != 0) && (atoi(bypass_mode) != 1))
  {
    ns_system_proxy_excp_usage("Not a valid value of bypass mode", err_msg);
    return PROXY_ERROR; 
  }

  if(atoi(bypass_mode) == 1)
  { 
    if(set_bypass_value(SYS_PROXY_IDX) == -1)
      return PROXY_ERROR;
    proxySvrTable[0].bypass_subnet = '1';
  }

  //Get exception list G_PROXY_EXCEPTIONS 1 192.168.1.66
  buf_ptr = buf;
  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;            //pointing group G3
  CLEAR_WHITE_SPACE(buf_ptr);    

  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  if(cur_buf_ptr != NULL)
    buf_ptr = cur_buf_ptr;           //pointing exception list
  else
    buf_ptr++;

  CLEAR_WHITE_SPACE(buf_ptr);
 
  strcpy(excp_list, buf_ptr);
  CLEAR_WHITE_SPACE_FROM_END(excp_list);

  if(excp_list[strlen(excp_list) - 1] == '\n')
    excp_list[strlen(excp_list) - 1] = 0;


  NSDL2_PROXY(NULL, NULL,"excp_list = [%s], bypass_mode=%d", excp_list, atoi(bypass_mode));
  if((cur_buf_ptr == NULL) && (atoi(bypass_mode)) == 0)
  {
    NSDL2_PROXY(NULL, NULL, "Since the default value has no address list so skipping it(i.e. SYSTEM_PROXY_EXCEPTIONS 0)");
    return PROXY_SUCCESS;
  }

  if((ret_val = is_sys_proxy_set()) == -1)
  {
    NSDL2_PROXY(NULL, NULL,"Since return value is = %d so returning as sytem proxy might not be set.", ret_val);
    return PROXY_SUCCESS;
  }

  NSDL1_PROXY(NULL, NULL,"Parsing Proxy Exception list for SYSTEM Proxy");
  if(cur_buf_ptr != NULL){
    if(tokenize_excp_list_and_populate_structures(excp_list, ret_val, err_msg) == PROXY_ERROR)
      return PROXY_ERROR;
  }  

  return PROXY_SUCCESS;
}


/*-------------------------------------------------------------------
 * Purpose : will parse the keyword SYSTEM_PROXY_SERVER <Address List>
 *
 * Input   : buf => contains the whole keyword line
 *           err_msg => message to contain error message
 * 
 * Output  : On Success   0
 *           On Failure  -1
 *-----------------------------------------------------------------*/
int kw_set_system_proxy_server(char *buf, char *err_msg, int runtime_flag)
{
  char key[MAX_PROXY_KEY_LEN];
  char add_list[MAX_ADD_LIST_LEN];
  //int no_of_tokens = 0;
  char *cur_buf_ptr, *buf_ptr;
 
  key[0] = 0, add_list[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, buf = [%s]", buf);

  sscanf(buf, "%s", key);

  //NSDL2_PROXY(NULL, NULL,"no_of_tokens=%d, key=%s", no_of_tokens, key);

  //Get address list 
  buf_ptr = buf;
  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;        
  CLEAR_WHITE_SPACE(buf_ptr);    

  if(cur_buf_ptr != NULL)
    buf_ptr = cur_buf_ptr;     
  else
    buf_ptr++;

  CLEAR_WHITE_SPACE(buf_ptr);
 
  strcpy(add_list, buf_ptr);
  CLEAR_WHITE_SPACE_FROM_END(add_list);

  if(add_list[strlen(add_list) - 1] == '\n')
    add_list[strlen(add_list) - 1] = 0;
  
  NSDL1_PROXY(NULL, NULL,"System proxy parsing add_list = [%s]", add_list);
  //Calling the function to parse the address list 
  if(parse_and_validate_addr_list(SYS_PROXY_IDX, add_list, err_msg, buf, runtime_flag) == PROXY_ERROR)
    return PROXY_ERROR;
  
  return PROXY_SUCCESS;  
}

/*------------------------------------------------------------------- 
 * Purpose   : (1) Will parse the keyword - G_PROXY_SERVER <Group Name|ALL> <Mode> <Address List>
 *                 Imp. Remarks:
 *                 * Here we are assuming that input buffer will conatin white space as token so Note that white space
 *                     is only allowed for token. If some one use white space within address list then it will generate   
 *                     an error
 *                 * At the end of input buffer only space, tab, samicolon(;), alphbets(a-z, A-Z), number(0-9) are 
 *                   allowed. Semicolon is optional 
 *                 * 
 *
 * Input     : (1) buf          :- This will contains keyword line G_PROXY_SERVER
 *                 Eg: 
 *                  * G_PROXY_SERVER G1 0 
 *                  * G_PROXY_SERVER G1 1
 *                  * G_PROXY_SERVER G1 2 http=192.168.1.66:80;https=192.168.1.66:8000
 *                  * G_PROXY_SERVER G1 2 
 *                  * G_PROXY_SERVER ALL http=192.168.1.66:80;http=192.168.1.69:80 
 *
 *             (2) group_idx    :- indecate group index into RunProfTableEntry. It contains two possible values
 *                   group_idx  = -1 for case ALL group
 *                   group_idx != -1 for specified group
 *
 *             (3) err_msg      :- A buffer for error message
 *
 *             (4) runtime_flag :- Currently it is not supported in future it may be supported 
 *
 * Output    : On success 0
 * 	       On Failure -1 
 *-------------------------------------------------------------------*/
int kw_set_g_proxy_server(char *buf, int group_idx, char *err_msg, int runtime_flag)
{
  char keyword[MAX_PROXY_KEY_LEN];
  char gp_name[MAX_GROUP_NAME_LEN];
  char mode[MAX_MODE_LEN];
  char add_list[MAX_ADD_LIST_LEN];
  char *cur_buf_ptr, *buf_ptr;
  int num = 0, svr_idx = -1, proxy_idx = -1, p_mode = -1;
  
  keyword[0] = 0, gp_name[0] = 0, mode[0] = 0, add_list[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, buf = [%s], group_idx = %d", buf, group_idx);
  
  num = sscanf(buf, "%s %s %s %s", keyword, gp_name, mode, add_list);

  NSDL2_PROXY(NULL, NULL, "num = %d, keyword = [%s], group name = [%s], mode = [%s], address list = [%s]", 
                           num, keyword, gp_name, mode, add_list);
  if(num > 4)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, CAV_ERR_MSG_1);
  
  //Check mode is numeric or not
  if(!ns_is_numeric(mode))
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, CAV_ERR_MSG_2);

  p_mode = atoi(mode);

  /* Address list must be comes with mode 2 only*/
  if(p_mode == MANUAL_PROXY && !add_list[0])
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "Proxy address list must be specified");

  if(p_mode != MANUAL_PROXY && add_list[0] != 0)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, "Proxy address list should not be provided");

  val_sgrp_name(buf, gp_name, 0);//This function checks the valid group name
 
  if(p_mode != NO_PROXY && p_mode != SYSTEM_PROXY && p_mode != MANUAL_PROXY) 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_SERVER_USAGE, CAV_ERR_1011285, CAV_ERR_MSG_3);

  //Get address list 
  buf_ptr = buf;
  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;            //pointing group G3
  CLEAR_WHITE_SPACE(buf_ptr);    

  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  buf_ptr = cur_buf_ptr;           //pointing mode
  CLEAR_WHITE_SPACE(buf_ptr); 

  cur_buf_ptr = strpbrk(buf_ptr, "/t ");
  if(cur_buf_ptr != NULL)
    buf_ptr = cur_buf_ptr;           //pointing exception list
  else
    buf_ptr++;
 
  CLEAR_WHITE_SPACE(buf_ptr);
  
  NSDL2_PROXY(NULL, NULL,"buf_ptr = [%s]", buf_ptr);
 
  strcpy(add_list, buf_ptr); 
  CLEAR_WHITE_SPACE_FROM_END(add_list);

  if(add_list[strlen(add_list) - 1] == '\n')
    add_list[strlen(add_list) - 1] = 0;
  
  NSDL2_PROXY(NULL, NULL,"add_list = [%s]", add_list);

  if(group_idx != -1)
  {
    create_proxy_server_table_entry(&svr_idx);

    //initilization for the newly used entry
    init_proxy(svr_idx);
  }
 
  /* Set proxy_idx:
   * System proxy will always be stored at 0th index in ProxyServerTable
   * Proxy for all groups will always be stored at 1st in ProxyServerTable
   * In case G_PROXY_SERVER ALL 1, proxy_idx in all runprof table entries will be set to 0
   * In case G_PROXY_SERVER <Group_name> 0 proxy_idx in run prof table shuold be -1 
   */
  if(p_mode == SYSTEM_PROXY) //System Proxy
  {
    //Check if proxy defined at index 0
    proxy_idx = 0;
    g_p_idx = 0;
  }
  else if(group_idx == -1 && p_mode == MANUAL_PROXY)  //ALL and manual proxy setting
  {
    proxy_idx = 1;
    g_p_idx = 1;
  }
  else if(p_mode == NO_PROXY)
    proxy_idx = -1;
  else
    proxy_idx = svr_idx; 

  //Parse the address list, fill Proxy server table and update proxy_index in RunProfTable
  if(p_mode == MANUAL_PROXY)
    if(parse_and_validate_addr_list(proxy_idx, add_list, err_msg, buf, runtime_flag) == PROXY_ERROR)
      return PROXY_ERROR;

  //Updating proxy index into RunProfTable
  //Here in case of ALL group we will not update proxy index in RunProfTable becase it have already updated in url.c
  if(group_idx != -1)
    if(update_proxy_index(group_idx, proxy_idx) == PROXY_ERROR)
      return PROXY_ERROR;

  //Bug 33244,33889 - Not getting the transaction details in progress report when firing test from back-end
  if(p_mode != NO_PROXY)
    global_settings->proxy_flag = 1;

  NSDL4_PROXY(NULL, NULL, "proxy_flag = %d", global_settings->proxy_flag);
  return PROXY_SUCCESS;
}

int kw_set_g_proxy_auth(char *buf, int group_idx, char *err_msg, int runtime_flag)
{
  char keyword[MAX_PROXY_KEY_LEN];
  char username[MAX_USER_LEN];
  char password[MAX_PASSWORD_LEN];
  char gp_name[MAX_GROUP_NAME_LEN];
  char tmp[MAX_PROXY_KEY_LEN];
  int num = 0;

  keyword[0] = 0, gp_name[0] = 0, username[0] = 0, password[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, buf = [%s], group_idx = %d", buf, group_idx);
  num = sscanf(buf, "%s %s %s %s %s", keyword, gp_name, username, password, tmp);
  NSDL2_PROXY(NULL, NULL, "num = %d, keyword = [%s], group name = [%s], username = [%s], password = [%s]",
                           num, keyword, gp_name, username, password);

  if (username[0] != '-'){
    NSDL2_PROXY(NULL, NULL,"Its not a default case. Checking for the number of arguments.");
    if(num != 4){
      NSDL2_PROXY(NULL, NULL,"No of arguments are less so error.");
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_AUTH_USAGE, CAV_ERR_1011044, CAV_ERR_MSG_1);
    }
  }
  else
  {
    NSDL2_PROXY(NULL, NULL,"Escaping as '-' found in username so it is a default case.");
    return PROXY_SUCCESS;
  }

  val_sgrp_name(buf, gp_name, 0);//This function checks the valid group name
  
  int proxy_idx = is_gp_proxy_set(group_idx);
  NSDL2_PROXY(NULL, NULL,"proxy_idx = %d", proxy_idx);
  if (proxy_idx == -1)
    return PROXY_SUCCESS;

  if((proxySvrTable[proxy_idx].username = copy_into_big_buf(username, 0)) == -1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_AUTH_USAGE, CAV_ERR_1000018, username);
  }
   if((proxySvrTable[proxy_idx].password = copy_into_big_buf(password, 0)) == -1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_PROXY_AUTH_USAGE, CAV_ERR_1000018, password);
  }
  return PROXY_SUCCESS;
}

int kw_set_system_proxy_auth(char *buf, char *err_msg, int runtime_flag)
{
  char key[MAX_PROXY_KEY_LEN];
  char username[MAX_USER_LEN];
  char password[MAX_PASSWORD_LEN];
  char tmp[MAX_PROXY_KEY_LEN];
  int no_of_tokens = 0;
  int ret_val;

  key[0] = 0, username[0] = 0, password[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, buf = [%s]", buf);

  /*In case system proxy not defined, not to parse SYSTEM_PROXY_AUTH keyword*/
  no_of_tokens = sscanf(buf, "%s %s %s %s", key, username, password, tmp);

  NSDL2_PROXY(NULL, NULL,"no_of_tokens=%d, key=%s,username=%s, password=%s", no_of_tokens, key, username, password);

  if(username[0] != '-'){
    if(no_of_tokens != 3)
    {
      ns_system_proxy_auth_usage("Need 2 field after the keyword SYSTEM_PROXY_AUTH", err_msg);
      return PROXY_ERROR;
    }
  }
  else
  {
    NSDL2_PROXY(NULL, NULL,"Escaping as '-' found in username so it is a default case in system proxy.");
    return PROXY_SUCCESS; 
  }

  if((ret_val = is_sys_proxy_set()) == -1)
    return PROXY_SUCCESS;

  if((proxySvrTable[SYS_PROXY_IDX].username = copy_into_big_buf(username, 0)) == -1) {
    sprintf(err_msg, "Failed in copying user name into big buf\n");
    return PROXY_ERROR;
  }
   if((proxySvrTable[SYS_PROXY_IDX].password = copy_into_big_buf(password, 0)) == -1) {
    sprintf(err_msg, "Failed in copying password into big buf\n");
    return PROXY_ERROR;
  }
 return PROXY_SUCCESS;
}

void inline update_proxy_counters(VUser *vptr, int status)
{
  NSDL1_PROXY(NULL, NULL, "Method Called, status = [%d]", status);
    switch (status)
    {
      case NS_REQUEST_1xx:
        INC_CONNECT_1XX_COUNTERS(vptr);
        break;
      case NS_REQUEST_OK:
        INC_CONNECT_2XX_COUNTERS(vptr);
        break;
      case NS_REQUEST_3xx:
        INC_CONNECT_3XX_COUNTERS(vptr);
        break;
      case NS_REQUEST_4xx:
        INC_CONNECT_4XX_COUNTERS(vptr);
        break;
      case NS_REQUEST_5xx:
        INC_CONNECT_5XX_COUNTERS(vptr);
        break;
      //case NS_REQUEST_ERRMISC:
      case NS_REQUEST_CONFAIL:
        INC_CONNECT_CONFAIL_COUNTERS(vptr);
        break;
      case NS_REQUEST_TIMEOUT:
        INC_CONNECT_TO_COUNTERS(vptr);
        break;
      default:
        INC_CONNECT_OTHERS_COUNTERS(vptr);
    }

  NSDL1_PROXY(NULL, NULL, "connect_1xx=%d, connect_2xx=%d, connect_3xx=%d, connect_4xx=%d, connect_5xx=%d"
                          "connect_others=%d, connect_confail=%d, connect_TO=%d",
                          proxy_avgtime->connect_1xx, proxy_avgtime->connect_2xx, proxy_avgtime->connect_3xx, 
                          proxy_avgtime->connect_4xx, proxy_avgtime->connect_5xx, 
                          proxy_avgtime->connect_others, proxy_avgtime->connect_confail, 
                          proxy_avgtime->connect_TO);

}

int kw_set_g_proxy_proto_mode(char *buf, GroupSettings *gset, char *err_msg)
{
  char key[MAX_PROXY_KEY_LEN];
  char gp_name[MAX_GROUP_NAME_LEN];
  char http_mode[MAX_MODE_LEN];
  char https_mode[MAX_MODE_LEN];
  char tmp[MAX_PROXY_KEY_LEN];
  int num_fields = 0;
  int http_mode_val = 0;
  int https_mode_val = 0;

  key[0] = 0, http_mode[0] = 0, https_mode[0] = 0;

  NSDL1_PROXY(NULL, NULL, "Method Called, buf = [%s]", buf);

  num_fields = sscanf(buf, "%s %s %s %s %s", key, gp_name, http_mode, https_mode, tmp);

  NSDL2_PROXY(NULL, NULL,"num_fields=%d, key=%s,http_mode=%s, https_mode=%s", num_fields, key, http_mode, https_mode);

  if(num_fields != 4 )
    if(ns_g_proxy_proto_mode_usage("Need 3 fields after the keyword G_PROXY_PROTO_MODE", err_msg) == -1)
      return PROXY_ERROR;

  //Check mode is numeric or not
  if(!ns_is_numeric(http_mode))
    if(ns_g_proxy_proto_mode_usage("HTTP Mode must be numeric.\n", err_msg) == -1)
      return PROXY_ERROR;
  

  if(!ns_is_numeric(https_mode))
    if(ns_g_proxy_proto_mode_usage("HTTPS Mode must be numeric.\n", err_msg) == -1)
      return PROXY_ERROR;

  http_mode_val = atoi(http_mode); 
  https_mode_val = atoi(https_mode);

  if(http_mode_val != 0 && http_mode_val != 1)
  {
    ns_g_proxy_proto_mode_usage("Not a valid value of HTTP Mode", err_msg);
    return PROXY_ERROR; 
  }
    
  if(https_mode_val != 0 && https_mode_val != 1)
  {
    ns_g_proxy_proto_mode_usage("Not a valid value of HTTPS Mode", err_msg);
    return PROXY_ERROR; 
  }
    
  if(http_mode_val == 1)
    gset->proxy_proto_mode |= HTTP_MODE;

  if(https_mode_val == 1)
    gset->proxy_proto_mode |= HTTPS_MODE; 

  NSDL2_PROXY(NULL, NULL, "gset->proxy_proto_mode = 0x%x", gset->proxy_proto_mode);
  return 0;
}
