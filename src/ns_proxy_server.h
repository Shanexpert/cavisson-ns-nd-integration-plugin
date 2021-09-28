#ifndef NS_PROXY_SERVER_H
#define NS_PROXY_SERVER_H

#include <arpa/inet.h>
#include <ifaddrs.h>

#define MAX_EXCPS_ACCEPTED 256
#define MAX_PROXY_KEY_LEN 32
#define MAX_GROUP_NAME_LEN MAX_PROXY_KEY_LEN
#define MAX_MODE_LEN MAX_PROXY_KEY_LEN
#define MAX_ADD_LIST_LEN 2 * 1024
#define MAX_BYPASS_CMD_LEN 1024
#define MAX_USER_LEN 32
#define MAX_PASSWORD_LEN 32

#define HTTP_REQUEST 1
#define HTTPS_REQUEST 2

#define HTTP_REQUEST_STR "http://"
#define HTTP_REQUEST_STR_LEN 7

#define HTTPS_REQUEST_STR "https://"
#define HTTPS_REQUEST_STR_LEN 8

#define WS_REQUEST_STR "ws://"
#define WS_REQUEST_STR_LEN 5

#define WSS_REQUEST_STR "wss://"
#define WSS_REQUEST_STR_LEN 6

#define HTTPS_DEFAULT_PORT_LEN 4

#define PROXY_SUCCESS 0
#define PROXY_ERROR -1
#define PROXY_WARNING 1

#define TOT_FIELDS 2

#define MAX_PROXY 8

#define NO_PROXY 0
#define SYSTEM_PROXY 1
#define MANUAL_PROXY 2

#define SYS_PROXY_IDX 0   //index into ProxyServertable
#define ALL_PROXY_IDX 1   //index into ProxyServertable

#define ALL_GROUP_IDX -1

#define RTC_DISABLE 0 
#define RTC_ENABLE 1 

#define NO_IPv 0
#define IPv4 1
#define IPv6 2

#define EXCP_IS_WITH_WILDCARD 4
#define EXCP_IS_DOMAIN_OR_HOSTNAME 3
#define EXCP_IS_IP_WITH_PORT 2
#define EXCP_IS_PORT_ONLY 1
#define EXCP_IS_IP_ONLY 0

#define HTTP_MODE	0x00000001 
#define HTTPS_MODE	0x00000002

//Checks if proxy is enabled/disabled
#define IS_PROXY_ENABLED \
  (global_settings->proxy_flag == 1)

//Decrementing connect_failure in case of connection timeout as its getting incremented in close_fd
#define DECREMENT_PROXY_FAILURE_COUNTER_IN_CASE_OF_TO \
    if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)  \
    {                                                        \
      DEC_CONNECT_FAILURE_COUNTERS(vptr);   \
      NSDL2_PROXY(NULL, cptr, "Connect failure counter decremented=%.3f", proxy_avgtime->connect_failure);  \
    }                                      

//Calculating Min Values for Times graph in case of Connect Reporting
#define SETTING_MIN_VALUE(http_connect_val, connect_response_time_min)\
{ \
     if(http_connect_val < connect_response_time_min) \
     {                                                \
        connect_response_time_min = http_connect_val; \
        NSDL2_PROXY(NULL, cptr, "proxy_avgtime->connect_response_time_min =%'.3f", (double) connect_response_time_min/1000.0);\
     }                                                \
}
  
//Calculating Max Values for Times graph in case of Connect Reporting
#define SETTING_MAX_VALUE(http_connect_val, connect_response_time_max)\
{ \
      if(http_connect_val > connect_response_time_max)  \
      {                                                 \
        connect_response_time_max = http_connect_val;   \
        NSDL2_PROXY(NULL, cptr, "connect_response_time_max=%'.3f", (double) connect_response_time_max/1000.0);\
      }                                                 \
}

#define UPDATE_CONNECT_FAILURE_COUNTERS(vptr)\
{                                                   \
  DEC_CONNECT_FAILURE_COUNTERS(vptr);                                                  \
  vptr->httpData->proxy_con_resp_time->http_connect_failure = (u_ns_4B_t) (get_ms_stamp() - (vptr->httpData->proxy_con_resp_time->http_connect_start));     \
  NSDL2_PROXY(vptr, NULL, "http connect failure time=%'.3f", (double)vptr->httpData->proxy_con_resp_time->http_connect_failure/1000.0);  \
  proxy_avgtime->connect_failure_response_time_total += vptr->httpData->proxy_con_resp_time->http_connect_failure; \
  SETTING_MIN_VALUE(vptr->httpData->proxy_con_resp_time->http_connect_failure, proxy_avgtime->connect_failure_response_time_min) \
  SETTING_MAX_VALUE(vptr->httpData->proxy_con_resp_time->http_connect_failure, proxy_avgtime->connect_failure_response_time_max) \
  NSDL2_PROXY(vptr, NULL, "http_connect_failure time stamp=%'.3f, failure_response_time_min=%.3f failure_response_time_max=%.3f", \
           (double) vptr->httpData->proxy_con_resp_time->http_connect_failure/1000.0, (double) proxy_avgtime->connect_failure_response_time_min/1000.0, (double) proxy_avgtime->connect_failure_response_time_max/1000.0); \
  INC_CONNECT_FAIL_RESP_TIME_TOT_FOR_GRP(vptr); \
  update_proxy_counters(vptr, status);                                                     \
}

#define UPDATE_CONNECT_SUCCESS_COUNTERS(vptr)\
{                                            \
  INC_CONNECT_SUCC_COUNTERS(vptr);                                                                               \
  vptr->httpData->proxy_con_resp_time->http_connect_success = (u_ns_4B_t) (get_ms_stamp() - (vptr->httpData->proxy_con_resp_time->http_connect_start));\
  NSDL2_PROXY(vptr, cptr, "http_connect_success time stamp=%'.3f", (double) vptr->httpData->proxy_con_resp_time->http_connect_success/1000.0 );\
                                                                                                                     \
  proxy_avgtime->connect_success_response_time_total += vptr->httpData->proxy_con_resp_time->http_connect_success;   \
  SETTING_MIN_VALUE (vptr->httpData->proxy_con_resp_time->http_connect_success , proxy_avgtime->connect_success_response_time_min); \
  SETTING_MAX_VALUE (vptr->httpData->proxy_con_resp_time->http_connect_success, proxy_avgtime->connect_success_response_time_max);  \
  NSDL2_PROXY(vptr, cptr, "success_response_time_min=%.3f success_response_time_max=%.3f, proxy_avgtime->connect_success_response_time_total=%'.3f", (double) proxy_avgtime->connect_success_response_time_min/1000.0, (double) proxy_avgtime->connect_success_response_time_max/1000.0, (double) proxy_avgtime->connect_success_response_time_total/1000.0);                                    \
  NSDL2_SCHEDULE(NULL, cptr, "status=%d, cptr flags = %x ,CONNECT bit=0X%x",status, cptr->flags ,NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT); \
  INC_CONNECT_SUCC_RESP_TIME_TOT_FOR_GRP(vptr); \
  update_proxy_counters(vptr, status);                                                                                     \
}


//Subnet is set in bypass_flag 
typedef struct ProxyExceptionTable
{
  char  excp_type;        //0 - Only IP, 1 - Only Port, 2 - IP & Port, 3 - domainname / hostname, 4 - wildcard
  union 
  {
    struct sockaddr_in6  excp_addr;   //This may contain IP & Port, IP Only or Port Only
    ns_bigbuf_t  excp_domain_name;
  } excp_val;
}ProxyExceptionTable;

typedef struct ProxyExceptionTable_Shr
{
  char  excp_type;        // 0 - Only IP, 1 - Only Port, 2 - IP & Port, 3 - domainname / hostname
  union
  {
    struct sockaddr_in6  excp_addr;
    char   *excp_domain_name;
  } excp_val;
} ProxyExceptionTable_Shr;

/*
typedef struct s_IPv4 {unsigned long subnet; unsigned long net_prefix;} s_IPv4; 
typedef struct s_IPv6 {unsigned char subnet[16]; unsigned char net_prefix[16];} s_IPv6;

typedef struct ProxyNetPrefix
{
  int addr_family;  //AF_INET, AF_INET6
  union { 
       s_IPv4 ipv4;
       s_IPv6 ipv6;
    } addr;
}ProxyNetPrefix;
*/
typedef struct ProxyNetPrefix
{
  int addr_family;  //AF_INET, AF_INET6
  union { 
      struct {unsigned long subnet; unsigned long net_prefix;} s_IPv4; 
      struct {unsigned char subnet[16]; unsigned char net_prefix[16];} s_IPv6;
    } addr;
}ProxyNetPrefix;

typedef struct ProxyNetPrefix_Shr
{
  int addr_family;  //AF_INET, AF_INET6
  union { 
      struct {unsigned long subnet; unsigned long net_prefix;} s_IPv4; 
      struct {unsigned char subnet[16]; unsigned char net_prefix[16];} s_IPv6;
    } addr;
}ProxyNetPrefix_Shr;


typedef struct ProxyServerTable
{
  //Proxy server can be ip or servername
  ns_bigbuf_t             http_proxy_server;    /* pointer into the big buffer, its initial value is -1*/
  short                   http_port;            /* it should be int not unsigned int because we are initializing it by -1*/ 
  struct sockaddr_in6     http_addr;
  ns_bigbuf_t             https_proxy_server;   /* pointer into the big buffer */
  short                   https_port; 
  struct sockaddr_in6     https_addr;
  char                    bypass_subnet;         /*1 to bypass proxy for local subnet addresses, 0 otherwise */
  int   		  excp_start_idx;       /* Index to start of exception list */
  int    	          num_excep;            /* Number of exceptions for this proxy */
  ns_bigbuf_t             username;             /* Username for proxy authentication */
  ns_bigbuf_t             password;             /* Password for proxy authentication */
}ProxyServerTable;

typedef struct ProxyServerTable_Shr
{
  //Proxy server can be ip or servername
  /* For HTTP */
  char                    *http_proxy_server;   /* pointer into the big buffer */
  short                   http_port; 
  struct sockaddr_in6     http_addr;

  /* For HTTPS */
  char                    *https_proxy_server;  /* pointer into the big buffer */
  short                   https_port; 
  struct sockaddr_in6     https_addr;

  /* For Exception */
  char                    bypass_subnet;          /*1 to bypass proxy for local subnet addresses, 0 otherwise */
  ProxyExceptionTable_Shr *excp_start_ptr;       /* Pointer to start of exception list */
  int    	          num_excep;            /* Number of exceptions for this proxy */

  /* For Authentication */
  char                    *username;            /* Username for proxy authetnication */
  char                    *password;            /* Password for proxy authetnication */
}ProxyServerTable_Shr;

extern int kw_set_system_proxy_server(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_system_proxy_auth(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_g_proxy_server(char *buf, int group_idx, char *err_msg, int runtime_flag);
extern int kw_set_g_proxy_auth(char *buf, int group_idx, char *err_msg, int runtime_flag);
extern void copy_proxySvr_table_into_shr_mem(ProxyServerTable_Shr *proxySvr_table_shr_mem_local);
extern void ns_proxy_shr_data_dump();
extern void ns_proxy_table_dump();
extern int update_proxy_index(int group_idx, int proxy_idx);
extern int kw_set_g_proxy_exceptions (char *buf, int group_idx, char *err_msg, int runtime_flag);
extern int kw_set_system_proxy_exceptions(char *buf, char *err_msg);
extern inline void copy_proxyExcp_table_into_shr_mem(ProxyExceptionTable_Shr *proxyException_table_shr_mem);
extern inline void copy_proxyNetPrefix_table_into_shr_mem(ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem);
extern inline int is_valid_ip(char *ip_addr); 
extern inline int is_proxy_enabled();
//extern void inline update_proxy_counters(VUser *vptr, int status);
//extern int kw_set_g_proxy_proto_mode(char *buf, GroupSettings *gset, char *err_msg);
#endif 
