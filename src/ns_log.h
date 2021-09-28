/*****************************
ns_log.h
*****************************/

#ifndef __NS_LOG_H__
#define __NS_LOG_H__

#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_DEBUG_ERR_LOG_BUF_SIZE  64000
#define MAX_GRP_NAME_LEN            1024

#define DM_L1          0x000000FF // level 1 

// Debug level 1
#define DM_WARN1       0x00000001 // value 1
#define DM_REQ_REP1    0x00000002 // value 2
#define DM_LOGIC1      0x00000004 // value 4
#define DM_EXECUTION   0x00000008 //???? // value 8 //uncomment it

// Debug level 2
#define DM_WARN2       0x00000100 // value 256
#define DM_REQ_REP2    0x00000200 // value 512
#define DM_LOGIC2      0x00000400 // value 1028
#define DM_METHOD      0x00000800 //???? // value 1 // uncomment it

// Debug level 3
#define DM_WARN3       0x00010000 // value 65536 
#define DM_REQ_REP3    0x00020000 // value 131072
#define DM_LOGIC3      0x00040000 // value 262144
//#define DM_EXE         0x00080000 //???? // value 524288

// Debug level 4
#define DM_WARN4       0x01000000 // value 16777216
#define DM_REQ_REP4    0x02000000 // value 33554432
#define DM_LOGIC4      0x04000000 // value 67108864
//#define DM_XXXX      0x00008000 // value 134217728


// Module Masks
#define MM_HTTP       0x0000000000000001ULL // Value 1
#define MM_SSL        0x0000000000000002ULL // Value 2 
#define MM_POLL       0x0000000000000004ULL // Value 4
#define MM_CONN       0x0000000000000008ULL // Value 8
#define MM_TESTCASE   0x0000000000000010ULL // Value 16
#define MM_VARS       0x0000000000000020ULL // Value 32
#define MM_TRANS      0x0000000000000040ULL // Value 64  // This module mask is handling the TX_PARSING (64) + TX_EXECUTION (128)
#define MM_MON        0x0000000000000080ULL // Value 128
#define MM_RUNLOGIC   0x0000000000000100ULL // Value 256
#define MM_MESSAGES   0x0000000000000200ULL // Value 512
#define MM_REPORTING  0x0000000000000400ULL // Value 1024
#define MM_GDF        0x0000000000000800ULL // Value 2048

#define MM_OAAM       0x0000000000001000ULL // Value 4096
#define MM_COOKIES    0x0000000000002000ULL // Value 8192
#define MM_IPMGMT     0x0000000000004000ULL // Value 16384
#define MM_SOCKETS    0x0000000000008000ULL // Value 32768
#define MM_LOGGING    0x0000000000010000ULL // Value 65536
#define MM_API        0x0000000000020000ULL // Value 131072 
#define MM_SCHEDULE   0x0000000000040000ULL // Value 262144
#define MM_ETHERNET   0x0000000000080000ULL // Value 524288
#define MM_HASHCODE   0x0000000000100000ULL // Value 1048576
#define MM_FREE       0x0000000000200000ULL // Value 2097152
#define MM_FREE2      0x0000000000400000ULL // Value 4194304
#define MM_WAN        0x0000000000800000ULL // Value 8388608
#define MM_CHILD      0x0000000001000000ULL // Value 16777216
#define MM_PARENT     0x0000000002000000ULL // Value 33554432
#define MM_TIMER      0x0000000004000000ULL // Value 67108864
#define MM_MEMORY     0x0000000008000000ULL // Value 134217728

#define MM_REPLAY     0x0000000010000000ULL
#define MM_SCRIPT     0x0000000020000000ULL // Must be used only if called from script c files
#define MM_SMTP       0x0000000040000000ULL
#define MM_POP3       0x0000000080000000ULL
#define MM_FTP        0x0000000100000000ULL
#define MM_DNS        0x0000000200000000ULL
#define MM_MISC       0x0000000400000000ULL // Value 2147483648
#define MM_CACHE      0x0000000800000000ULL // Value 2147483648

#define MM_JAVA_SCRIPT 0x0000001000000000ULL
#define MM_PARSING    0x0000002000000000ULL // For all parsing debug log e.g. script, scenario
#define MM_TR069      0x0000004000000000ULL
#define MM_USER_TRACE 0x0000008000000000ULL

#define MM_RUNTIME    0x0000010000000000ULL
#define MM_AUTH       0x0000020000000000ULL
#define MM_PROXY      0x0000040000000000ULL  //4398046511104

#define MM_SP         0x0000080000000000ULL

#define MM_NJVM       0x0000100000000000ULL

#define MM_RBU        0x0000200000000000ULL // Decimal Value: 35184372088832 

#define MM_LDAP       0x0000400000000000ULL
 
#define MM_IMAP       0x0000800000000000ULL
 
#define MM_JRMI       0x0001000000000000ULL 

#define MM_WS         0x0002000000000000ULL 
#define MM_PERCENTILE 0x0004000000000000ULL 
#define MM_HTTP2      0x0008000000000000ULL 
#define MM_RTE        0x0010000000000000ULL 
#define MM_DB_AGG     0x0020000000000000ULL 
#define MM_SVRIP      0x0040000000000000ULL // Value 18014398509481984
#define MM_HLS        0x0080000000000000ULL 
#define MM_XMPP       0x0100000000000000ULL 
#define MM_FC2        0x0200000000000000ULL 
#define MM_JMS        0x0400000000000000ULL 
#define MM_NH         0x0800000000000000ULL 
#define MM_RDP        0x1000000000000000ULL 

#define MM_ALL        0xFFFFFFFFFFFFFFFFULL

#define _FL_  __FILE__, __LINE__
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__


#define ERROR_LOG_CRITICAL      0x000000FF
#define ERROR_LOG_MAJOR         0x0000FF00
#define ERROR_LOG_MINOR         0x00FF0000
#define ERROR_LOG_WARNING       0xFF000000
#define ERROR_ID        ""
#define ERROR_ATTR      ""

void *void_vptr, *void_cptr;

/* error_log_ex */
#define NSEL_CRI(void_vptr, void_cptr, error_id, error_attr, ...)                 \
  if (global_settings->error_log) error_log_ex(ERROR_LOG_CRITICAL, _FLN_, void_vptr, void_cptr, error_id, error_attr, __VA_ARGS__);

#define NSEL_MAJ(void_vptr, void_cptr, error_id, error_attr, ...)                 \
  if (global_settings->error_log) error_log_ex(ERROR_LOG_MAJOR, _FLN_, void_vptr, void_cptr, error_id, error_attr, __VA_ARGS__);

#define NSEL_MIN(void_vptr, void_cptr, error_id, error_attr, ...)                 \
  if (global_settings->error_log) error_log_ex(ERROR_LOG_MINOR, _FLN_, void_vptr, void_cptr, error_id, error_attr, __VA_ARGS__);

#define NSEL_WAR(void_vptr, void_cptr, error_id, error_attr, ...)                 \
  if (global_settings->error_log) error_log_ex(ERROR_LOG_WARNING, _FLN_, void_vptr, void_cptr, error_id, error_attr, __VA_ARGS__);


#define error_log(...)                                  \
  error_log_ex(ERROR_LOG_CRITICAL, "", 0, "",           \
               NULL, NULL, ERROR_ID, ERROR_ATTR,        \
               __VA_ARGS__)                             \

// New Vesion of debug log 
#ifdef NS_DEBUG_ON 

  #define NSDL(void_vptr, void_cptr, DM, MD, ...)  ns_debug_log_ex(DM, MD, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  // Macros for MM_HTTP
  #define NSDL1_HTTP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_HTTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_HTTP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_HTTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_HTTP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_HTTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_HTTP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_HTTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_POLL
  #define NSDL1_POLL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_POLL, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_POLL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_POLL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_POLL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_POLL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_POLL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_POLL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_CONN
  #define NSDL1_CONN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_CONN, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_CONN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_CONN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_CONN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_CONN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_CONN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_CONN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_TESTCASE
  #define NSDL1_TESTCASE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_TESTCASE, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_TESTCASE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_TESTCASE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_TESTCASE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_TESTCASE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_TESTCASE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_TESTCASE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_VARS
  #define NSDL1_VARS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_VARS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_VARS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_VARS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_VARS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_VARS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_VARS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_VARS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_TRANS
  #define NSDL1_TRANS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_TRANS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_TRANS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_TRANS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_TRANS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_TRANS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL4_TRANS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_TRANS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_RUNLOGIC
  #define NSDL1_RUNLOGIC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_RUNLOGIC, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_RUNLOGIC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_RUNLOGIC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_RUNLOGIC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_RUNLOGIC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_RUNLOGIC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_RUNLOGIC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_MESSAGES
  #define NSDL1_MESSAGES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_MESSAGES, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_MESSAGES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_MESSAGES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_MESSAGES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_MESSAGES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_MESSAGES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_MESSAGES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_REPORTING
  #define NSDL1_REPORTING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_REPORTING, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_REPORTING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_REPORTING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_REPORTING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_REPORTING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_REPORTING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_REPORTING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_GDF
  #define NSDL1_GDF(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_GDF, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_GDF(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_GDF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_GDF(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_GDF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_GDF(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_GDF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_OAAM
  #define NSDL1_OAAM(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_OAAM, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_OAAM(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_OAAM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_OAAM(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_OAAM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_OAAM(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_OAAM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_COOKIES
  #define NSDL1_COOKIES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_COOKIES, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_COOKIES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_COOKIES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_COOKIES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_COOKIES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_COOKIES(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_COOKIES, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_IPMGMT
  #define NSDL1_IPMGMT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_IPMGMT, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_IPMGMT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_IPMGMT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_IPMGMT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_IPMGMT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_IPMGMT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_IPMGMT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_SOCKETS
  #define NSDL1_SOCKETS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_SOCKETS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_SOCKETS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_SOCKETS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SOCKETS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_SOCKETS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SOCKETS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_SOCKETS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_LOGGING
  #define NSDL1_LOGGING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_LOGGING, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_LOGGING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_LOGGING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_LOGGING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_LOGGING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_LOGGING(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_LOGGING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_API
  #define NSDL1_API(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_API, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_API(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_API, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_API(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_API, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_API(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_API, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_SCHEDULE
  #define NSDL1_SCHEDULE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_SCHEDULE, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_SCHEDULE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_SCHEDULE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SCHEDULE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_SCHEDULE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SCHEDULE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_SCHEDULE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_ETHERNET
  #define NSDL1_ETHERNET(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_ETHERNET, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_ETHERNET(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_ETHERNET, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_ETHERNET(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_ETHERNET, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_ETHERNET(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_ETHERNET, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_HASHCODE
  #define NSDL1_HASHCODE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_HASHCODE, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_HASHCODE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_HASHCODE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_HASHCODE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_HASHCODE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_HASHCODE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_HASHCODE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_SSL
  #define NSDL1_SSL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_SSL, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_SSL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_SSL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SSL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_SSL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SSL(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_SSL, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_MON
  #define NSDL1_MON(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_MON, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_MON(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_MON, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_MON(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_MON, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_MON(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_MON, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_WAN
  #define NSDL1_WAN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_WAN, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_WAN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_WAN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_WAN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_WAN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_WAN(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_WAN, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_CHILD
  #define NSDL1_CHILD(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_CHILD, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_CHILD(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_CHILD, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_CHILD(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_CHILD, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_CHILD(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_CHILD, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_PARENT
  #define NSDL1_PARENT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_PARENT, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_PARENT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_PARENT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_PARENT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_PARENT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_PARENT(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_PARENT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_TIMER
  #define NSDL1_TIMER(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_TIMER, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_TIMER(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_TIMER, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_TIMER(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_TIMER, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_TIMER(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_TIMER, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  // Macros for MM_MISC
  #define NSDL1_MISC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_MISC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_MISC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_MISC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_MISC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_MISC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_MISC(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_MISC, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // Macros for MM_MEMORY
  #define NSDL1_MEMORY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_MEMORY, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_MEMORY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_MEMORY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_MEMORY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_MEMORY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_MEMORY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_MEMORY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
  
  #define NSDL1_REPLAY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_REPLAY, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_REPLAY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_REPLAY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_REPLAY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_REPLAY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_REPLAY(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_REPLAY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  #define NSDL1_SMTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_SMTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
 
  #define NSDL2_SMTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_SMTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SMTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_SMTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SMTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_SMTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_POP3(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_POP3, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
 
  #define NSDL2_POP3(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_POP3, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_POP3(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_POP3, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_POP3(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_POP3, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_FTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_FTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
 
  #define NSDL2_FTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_FTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_FTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_FTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_FTP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_FTP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_DNS(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_DNS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
 
  #define NSDL2_DNS(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_DNS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_DNS(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_DNS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_DNS(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_DNS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_CACHE(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_CACHE, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
 
  #define NSDL2_CACHE(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_CACHE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_CACHE(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_CACHE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_CACHE(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_CACHE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

   #define NSDL1_JAVA_SCRIPT(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_JAVA_SCRIPT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_JAVA_SCRIPT(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x0000FF00, MM_JAVA_SCRIPT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_JAVA_SCRIPT(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x00FF0000, MM_JAVA_SCRIPT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_JAVA_SCRIPT(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_JAVA_SCRIPT, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

   #define NSDL1_PARSING(void_vptr, void_cptr, ...)            ns_debug_log_ex(0x000000FF, MM_PARSING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_PARSING(void_vptr, void_cptr, ...)             ns_debug_log_ex(0x0000FF00, MM_PARSING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_PARSING(void_vptr, void_cptr, ...)             ns_debug_log_ex(0x00FF0000, MM_PARSING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_PARSING(void_vptr, void_cptr, ...)             ns_debug_log_ex(0xFF000000, MM_PARSING, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

 #define NSDL1_TR069(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_TR069, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_TR069(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x0000FF00, MM_TR069, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_TR069(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x00FF0000, MM_TR069, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_TR069(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_TR069, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_USER_TRACE(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_USER_TRACE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_USER_TRACE(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x0000FF00, MM_USER_TRACE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_USER_TRACE(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x00FF0000, MM_USER_TRACE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_USER_TRACE(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_USER_TRACE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_RUNTIME(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x000000FF, MM_RUNTIME, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_RUNTIME(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x0000FF00, MM_RUNTIME, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_RUNTIME(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x00FF0000, MM_RUNTIME, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_RUNTIME(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_RUNTIME, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL1_AUTH(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x000000FF, MM_AUTH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_AUTH(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x0000FF00, MM_AUTH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_AUTH(void_vptr, void_cptr, ...)         ns_debug_log_ex(0x00FF0000, MM_AUTH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_AUTH(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_AUTH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  //For PROXY 
  #define NSDL1_PROXY(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_PROXY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_PROXY(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_PROXY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_PROXY(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_PROXY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_PROXY(void_vptr, void_cptr, ...)         ns_debug_log_ex(0xFF000000, MM_PROXY, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  #define NSDL1_SCRIPT(...)              ns_debug_log_scr(0x000000FF, MM_SCRIPT, _FLN_,  __VA_ARGS__) 

  #define NSDL2_SCRIPT(...)              ns_debug_log_scr(0x0000FF00, MM_SCRIPT, _FLN_,  __VA_ARGS__)

  #define NSDL3_SCRIPT(...)              ns_debug_log_scr(0x00FF0000, MM_SCRIPT, _FLN_,  __VA_ARGS__)

  #define NSDL4_SCRIPT(...)              ns_debug_log_scr(0xFF000000, MM_SCRIPT, _FLN_,  __VA_ARGS__)


  //For Sync Point
  #define NSDL1_SP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_SP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_SP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_SP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_SP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_SP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // For NJVM
  #define NSDL1_NJVM(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_NJVM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_NJVM(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_NJVM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_NJVM(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_NJVM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_NJVM(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_NJVM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // For RBU
  #define NSDL1_RBU(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_RBU, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_RBU(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_RBU, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_RBU(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_RBU, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_RBU(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_RBU, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // For LDAP
  #define NSDL1_LDAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_LDAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_LDAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_LDAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_LDAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_LDAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_LDAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_LDAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
  
  //for IMAP

  #define NSDL1_IMAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_IMAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_IMAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_IMAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_IMAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_IMAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_IMAP(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_IMAP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

//for RMI
 
  #define NSDL1_JRMI(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_JRMI, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_JRMI(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_JRMI, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_JRMI(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_JRMI, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_JRMI(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_JRMI, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  //for WS

  #define NSDL1_WS(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_WS(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_WS(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_WS(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  //for PERCENTILE

  #define NSDL1_PERCENTILE(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x000000FF, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL2_PERCENTILE(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x0000FF00, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_PERCENTILE(void_vptr, void_cptr, ...)        ns_debug_log_ex(0x00FF0000, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_PERCENTILE(void_vptr, void_cptr, ...)        ns_debug_log_ex(0xFF000000, MM_WS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  //for HTTP2
   
  // Macros for MM_HTTP2
  #define NSDL1_HTTP2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_HTTP2, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_HTTP2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_HTTP2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_HTTP2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_HTTP2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_HTTP2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_HTTP2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  //for FC2
   
  // Macros for MM_FC2
  #define NSDL1_FC2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_FC2, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_FC2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_FC2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_FC2(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_FC2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_FC22(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_FC2, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
 

  //for RTE
  #define NSDL1_RTE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_RTE, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_RTE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_RTE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_RTE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_RTE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_RTE(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_RTE, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  //for DB Aggregator
  #define NSDL1_DB_AGG(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_DB_AGG, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_DB_AGG(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_DB_AGG, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_DB_AGG(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_DB_AGG, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_DB_AGG(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_DB_AGG, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // for SERVER IP DATA
  
  #define NSDL1_SVRIP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x000000FF, MM_SVRIP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_SVRIP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x0000FF00, MM_SVRIP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_SVRIP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0x00FF0000, MM_SVRIP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_SVRIP(void_vptr, void_cptr, ...)               ns_debug_log_ex(0xFF000000, MM_SVRIP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // for HLS
  #define NSDL1_HLS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_HLS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_HLS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_HLS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_HLS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_HLS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_HLS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_HLS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)


  // for XMPP
  #define NSDL1_XMPP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_XMPP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_XMPP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_XMPP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_XMPP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_XMPP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_XMPP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_XMPP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
  

 // for RDP bug 70149
  #define NSDL1_RDP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_RDP, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 
  #define NSDL2_RDP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_RDP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
  #define NSDL3_RDP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_RDP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
  #define NSDL4_RDP(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_RDP, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  
   // for JMS
  #define NSDL1_JMS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_JMS, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_JMS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_JMS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_JMS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_JMS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_JMS(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_JMS, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  // for netHavoc
  #define NSDL1_NH(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x000000FF, MM_NH, _FLN_, void_vptr, void_cptr, __VA_ARGS__) 

  #define NSDL2_NH(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x0000FF00, MM_NH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL3_NH(void_vptr, void_cptr, ...)  ns_debug_log_ex(0x00FF0000, MM_NH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

  #define NSDL4_NH(void_vptr, void_cptr, ...)  ns_debug_log_ex(0xFF000000, MM_NH, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
#else

  #define NSDL(DM, MD, void_vptr, void_cptr, ...)

  #define NSDL1_HTTP(void_vptr, void_cptr, ...) 

  #define NSDL2_HTTP(void_vptr, void_cptr, ...)

  #define NSDL3_HTTP(void_vptr, void_cptr, ...)

  #define NSDL4_HTTP(void_vptr, void_cptr, ...)

  #define NSDL1_POLL(void_vptr, void_cptr, ...)

  #define NSDL2_POLL(void_vptr, void_cptr, ...)

  #define NSDL3_POLL(void_vptr, void_cptr, ...)

  #define NSDL4_POLL(void_vptr, void_cptr, ...)

  #define NSDL1_CONN(void_vptr, void_cptr, ...)

  #define NSDL2_CONN(void_vptr, void_cptr, ...)

  #define NSDL3_CONN(void_vptr, void_cptr, ...)

  #define NSDL4_CONN(void_vptr, void_cptr, ...)

  #define NSDL1_TESTCASE(void_vptr, void_cptr, ...)

  #define NSDL2_TESTCASE(void_vptr, void_cptr, ...)

  #define NSDL3_TESTCASE(void_vptr, void_cptr, ...)

  #define NSDL4_TESTCASE(void_vptr, void_cptr, ...)

  #define NSDL1_VARS(void_vptr, void_cptr, ...)

  #define NSDL2_VARS(void_vptr, void_cptr, ...)

  #define NSDL3_VARS(void_vptr, void_cptr, ...)

  #define NSDL4_VARS(void_vptr, void_cptr, ...)

  #define NSDL1_TRANS(void_vptr, void_cptr, ...)

  #define NSDL2_TRANS(void_vptr, void_cptr, ...)

  #define NSDL3_TRANS(void_vptr, void_cptr, ...)

  #define NSDL4_TRANS(void_vptr, void_cptr, ...)

  #define NSDL1_RUNLOGIC(void_vptr, void_cptr, ...)

  #define NSDL2_RUNLOGIC(void_vptr, void_cptr, ...)

  #define NSDL3_RUNLOGIC(void_vptr, void_cptr, ...)

  #define NSDL4_RUNLOGIC(void_vptr, void_cptr, ...)

  #define NSDL1_MESSAGES(void_vptr, void_cptr, ...)

  #define NSDL2_MESSAGES(void_vptr, void_cptr, ...)

  #define NSDL3_MESSAGES(void_vptr, void_cptr, ...)

  #define NSDL4_MESSAGES(void_vptr, void_cptr, ...)

  #define NSDL1_REPORTING(void_vptr, void_cptr, ...)

  #define NSDL2_REPORTING(void_vptr, void_cptr, ...)

  #define NSDL3_REPORTING(void_vptr, void_cptr, ...)

  #define NSDL4_REPORTING(void_vptr, void_cptr, ...)

  #define NSDL1_GDF(void_vptr, void_cptr, ...)

  #define NSDL2_GDF(void_vptr, void_cptr, ...)

  #define NSDL3_GDF(void_vptr, void_cptr, ...)

  #define NSDL4_GDF(void_vptr, void_cptr, ...)

  #define NSDL1_OAAM(void_vptr, void_cptr, ...)

  #define NSDL2_OAAM(void_vptr, void_cptr, ...)

  #define NSDL3_OAAM(void_vptr, void_cptr, ...)

  #define NSDL4_OAAM(void_vptr, void_cptr, ...)

  #define NSDL1_COOKIES(void_vptr, void_cptr, ...)

  #define NSDL2_COOKIES(void_vptr, void_cptr, ...)

  #define NSDL3_COOKIES(void_vptr, void_cptr, ...)

  #define NSDL4_COOKIES(void_vptr, void_cptr, ...)

  #define NSDL1_IPMGMT(void_vptr, void_cptr, ...)

  #define NSDL2_IPMGMT(void_vptr, void_cptr, ...)

  #define NSDL3_IPMGMT(void_vptr, void_cptr, ...)

  #define NSDL4_IPMGMT(void_vptr, void_cptr, ...)

  #define NSDL1_SOCKETS(void_vptr, void_cptr, ...)

  #define NSDL2_SOCKETS(void_vptr, void_cptr, ...)

  #define NSDL3_SOCKETS(void_vptr, void_cptr, ...)

  #define NSDL4_SOCKETS(void_vptr, void_cptr, ...)

  #define NSDL1_LOGGING(void_vptr, void_cptr, ...)

  #define NSDL2_LOGGING(void_vptr, void_cptr, ...)

  #define NSDL3_LOGGING(void_vptr, void_cptr, ...)

  #define NSDL4_LOGGING(void_vptr, void_cptr, ...)

  #define NSDL1_API(void_vptr, void_cptr, ...)

  #define NSDL2_API(void_vptr, void_cptr, ...)

  #define NSDL3_API(void_vptr, void_cptr, ...)

  #define NSDL4_API(void_vptr, void_cptr, ...)

  #define NSDL1_SCHEDULE(void_vptr, void_cptr, ...)

  #define NSDL2_SCHEDULE(void_vptr, void_cptr, ...)

  #define NSDL3_SCHEDULE(void_vptr, void_cptr, ...)

  #define NSDL4_SCHEDULE(void_vptr, void_cptr, ...)

  #define NSDL1_ETHERNET(void_vptr, void_cptr, ...)

  #define NSDL2_ETHERNET(void_vptr, void_cptr, ...)

  #define NSDL3_ETHERNET(void_vptr, void_cptr, ...)

  #define NSDL4_ETHERNET(void_vptr, void_cptr, ...)

  #define NSDL1_HASHCODE(void_vptr, void_cptr, ...)

  #define NSDL2_HASHCODE(void_vptr, void_cptr, ...)

  #define NSDL3_HASHCODE(void_vptr, void_cptr, ...)

  #define NSDL4_HASHCODE(void_vptr, void_cptr, ...)

  #define NSDL1_SSL(void_vptr, void_cptr, ...)

  #define NSDL2_SSL(void_vptr, void_cptr, ...)

  #define NSDL3_SSL(void_vptr, void_cptr, ...)

  #define NSDL4_SSL(void_vptr, void_cptr, ...)

  #define NSDL1_MON(void_vptr, void_cptr, ...)

  #define NSDL2_MON(void_vptr, void_cptr, ...)

  #define NSDL3_MON(void_vptr, void_cptr, ...)

  #define NSDL4_MON(void_vptr, void_cptr, ...)

  #define NSDL1_WAN(void_vptr, void_cptr, ...)

  #define NSDL2_WAN(void_vptr, void_cptr, ...)

  #define NSDL3_WAN(void_vptr, void_cptr, ...)

  #define NSDL4_WAN(void_vptr, void_cptr, ...)

  #define NSDL1_CHILD(void_vptr, void_cptr, ...)

  #define NSDL2_CHILD(void_vptr, void_cptr, ...)

  #define NSDL3_CHILD(void_vptr, void_cptr, ...)

  #define NSDL4_CHILD(void_vptr, void_cptr, ...)

  #define NSDL1_PARENT(void_vptr, void_cptr, ...)

  #define NSDL2_PARENT(void_vptr, void_cptr, ...)

  #define NSDL3_PARENT(void_vptr, void_cptr, ...)

  #define NSDL4_PARENT(void_vptr, void_cptr, ...)

  #define NSDL1_TIMER(void_vptr, void_cptr, ...)

  #define NSDL2_TIMER(void_vptr, void_cptr, ...)

  #define NSDL3_TIMER(void_vptr, void_cptr, ...)

  #define NSDL4_TIMER(void_vptr, void_cptr, ...)

  #define NSDL1_MISC(void_vptr, void_cptr, ...)

  #define NSDL2_MISC(void_vptr, void_cptr, ...)

  #define NSDL3_MISC(void_vptr, void_cptr, ...)

  #define NSDL4_MISC(void_vptr, void_cptr, ...)

  #define NSDL1_MEMORY(void_vptr, void_cptr, ...)

  #define NSDL2_MEMORY(void_vptr, void_cptr, ...)

  #define NSDL3_MEMORY(void_vptr, void_cptr, ...)

  #define NSDL4_MEMORY(void_vptr, void_cptr, ...)

  #define NSDL1_REPLAY(void_vptr, void_cptr, ...)

  #define NSDL2_REPLAY(void_vptr, void_cptr, ...)

  #define NSDL3_REPLAY(void_vptr, void_cptr, ...)

  #define NSDL4_REPLAY(void_vptr, void_cptr, ...)

  #define NSDL1_SCRIPT(...)

  #define NSDL2_SCRIPT(...)

  #define NSDL3_SCRIPT(...)

  #define NSDL4_SCRIPT(...)

  #define NSDL1_SMTP(void_vptr, void_cptr, ...)

  #define NSDL2_SMTP(void_vptr, void_cptr, ...)

  #define NSDL3_SMTP(void_vptr, void_cptr, ...)

  #define NSDL4_SMTP(void_vptr, void_cptr, ...)

  #define NSDL1_POP3(void_vptr, void_cptr, ...)

  #define NSDL2_POP3(void_vptr, void_cptr, ...)

  #define NSDL3_POP3(void_vptr, void_cptr, ...)

  #define NSDL4_POP3(void_vptr, void_cptr, ...)

  #define NSDL1_FTP(void_vptr, void_cptr, ...)

  #define NSDL2_FTP(void_vptr, void_cptr, ...)

  #define NSDL3_FTP(void_vptr, void_cptr, ...)

  #define NSDL4_FTP(void_vptr, void_cptr, ...)

  #define NSDL1_DNS(void_vptr, void_cptr, ...)

  #define NSDL2_DNS(void_vptr, void_cptr, ...)

  #define NSDL3_DNS(void_vptr, void_cptr, ...)

  #define NSDL4_DNS(void_vptr, void_cptr, ...)

  #define NSDL1_CACHE(void_vptr, void_cptr, ...)

  #define NSDL2_CACHE(void_vptr, void_cptr, ...)

  #define NSDL3_CACHE(void_vptr, void_cptr, ...)

  #define NSDL4_CACHE(void_vptr, void_cptr, ...)
 
  #define NSDL1_JAVA_SCRIPT(void_vptr, void_cptr, ...)

  #define NSDL2_JAVA_SCRIPT(void_vptr, void_cptr, ...)

  #define NSDL3_JAVA_SCRIPT(void_vptr, void_cptr, ...)

  #define NSDL4_JAVA_SCRIPT(void_vptr, void_cptr, ...)

  #define NSDL1_PARSING(void_vptr, void_cptr, ...)

  #define NSDL2_PARSING(void_vptr, void_cptr, ...)

  #define NSDL3_PARSING(void_vptr, void_cptr, ...)

  #define NSDL4_PARSING(void_vptr, void_cptr, ...)

  #define NSDL1_TR069(void_vptr, void_cptr, ...)

  #define NSDL2_TR069(void_vptr, void_cptr, ...)

  #define NSDL3_TR069(void_vptr, void_cptr, ...)

  #define NSDL4_TR069(void_vptr, void_cptr, ...)

  #define NSDL1_USER_TRACE(void_vptr, void_cptr, ...)

  #define NSDL2_USER_TRACE(void_vptr, void_cptr, ...)

  #define NSDL3_USER_TRACE(void_vptr, void_cptr, ...)

  #define NSDL4_USER_TRACE(void_vptr, void_cptr, ...)

  #define NSDL1_RUNTIME(void_vptr, void_cptr, ...)

  #define NSDL2_RUNTIME(void_vptr, void_cptr, ...)

  #define NSDL3_RUNTIME(void_vptr, void_cptr, ...)

  #define NSDL4_RUNTIME(void_vptr, void_cptr, ...)

  #define NSDL1_AUTH(void_vptr, void_cptr, ...)

  #define NSDL2_AUTH(void_vptr, void_cptr, ...)

  #define NSDL3_AUTH(void_vptr, void_cptr, ...)

  #define NSDL4_AUTH(void_vptr, void_cptr, ...)

  #define NSDL1_PROXY(void_vptr, void_cptr, ...)

  #define NSDL2_PROXY(void_vptr, void_cptr, ...)

  #define NSDL3_PROXY(void_vptr, void_cptr, ...)

  #define NSDL4_PROXY(void_vptr, void_cptr, ...)

  // For Sync Point

  #define NSDL1_SP(void_vptr, void_cptr, ...)

  #define NSDL2_SP(void_vptr, void_cptr, ...)

  #define NSDL3_SP(void_vptr, void_cptr, ...)

  #define NSDL4_SP(void_vptr, void_cptr, ...)

  // For NJVM

  #define NSDL1_NJVM(void_vptr, void_cptr, ...)

  #define NSDL2_NJVM(void_vptr, void_cptr, ...)

  #define NSDL3_NJVM(void_vptr, void_cptr, ...)

  #define NSDL4_NJVM(void_vptr, void_cptr, ...)

// For RBU
  #define NSDL1_RBU(void_vptr, void_cptr, ...)

  #define NSDL2_RBU(void_vptr, void_cptr, ...)

  #define NSDL3_RBU(void_vptr, void_cptr, ...)

  #define NSDL4_RBU(void_vptr, void_cptr, ...)

  // For LDAP
  #define NSDL1_LDAP(void_vptr, void_cptr, ...)

  #define NSDL2_LDAP(void_vptr, void_cptr, ...)

  #define NSDL3_LDAP(void_vptr, void_cptr, ...)

  #define NSDL4_LDAP(void_vptr, void_cptr, ...)

  //for IMAP
  #define NSDL1_IMAP(void_vptr, void_cptr, ...)

  #define NSDL2_IMAP(void_vptr, void_cptr, ...)

  #define NSDL3_IMAP(void_vptr, void_cptr, ...)

  #define NSDL4_IMAP(void_vptr, void_cptr, ...)

 // for JRMI
 #define NSDL1_JRMI(void_vptr, void_cptr, ...)

 #define NSDL2_JRMI(void_vptr, void_cptr, ...)

 #define NSDL3_JRMI(void_vptr, void_cptr, ...)

 #define NSDL4_JRMI(void_vptr, void_cptr, ...)

  // for WS
  #define NSDL1_WS(void_vptr, void_cptr, ...)

  #define NSDL2_WS(void_vptr, void_cptr, ...)

  #define NSDL3_WS(void_vptr, void_cptr, ...)

  #define NSDL4_WS(void_vptr, void_cptr, ...)
 
  // for PERCENTILE
  #define NSDL1_PERCENTILE(void_vptr, void_cptr, ...)

  #define NSDL2_PERCENTILE(void_vptr, void_cptr, ...)

  #define NSDL3_PERCENTILE(void_vptr, void_cptr, ...)

  #define NSDL4_PERCENTILE(void_vptr, void_cptr, ...)

  // for HTTP2

  #define NSDL1_HTTP2(void_vptr, void_cptr, ...) 

  #define NSDL2_HTTP2(void_vptr, void_cptr, ...)

  #define NSDL3_HTTP2(void_vptr, void_cptr, ...)

  #define NSDL4_HTTP2(void_vptr, void_cptr, ...)

  // for HTTP2

  #define NSDL1_FC2(void_vptr, void_cptr, ...) 

  #define NSDL2_FC2(void_vptr, void_cptr, ...)

  #define NSDL3_FC2(void_vptr, void_cptr, ...)

  #define NSDL4_FC2(void_vptr, void_cptr, ...)

  //for RTE

  #define NSDL1_RTE(void_vptr, void_cptr, ...) 

  #define NSDL2_RTE(void_vptr, void_cptr, ...)

  #define NSDL3_RTE(void_vptr, void_cptr, ...)

  #define NSDL4_RTE(void_vptr, void_cptr, ...)
  
  //for DB Aggregator

  #define NSDL1_DB_AGG(void_vptr, void_cptr, ...) 

  #define NSDL2_DB_AGG(void_vptr, void_cptr, ...)

  #define NSDL3_DB_AGG(void_vptr, void_cptr, ...)

  #define NSDL4_DB_AGG(void_vptr, void_cptr, ...)

  //for SERVER IP DATA

  #define NSDL1_SVRIP(void_vptr, void_cptr, ...)

  #define NSDL2_SVRIP(void_vptr, void_cptr, ...)

  #define NSDL3_SVRIP(void_vptr, void_cptr, ...)

  #define NSDL4_SVRIP(void_vptr, void_cptr, ...)

  //for HLS

  #define NSDL1_HLS(void_vptr, void_cptr, ...)

  #define NSDL2_HLS(void_vptr, void_cptr, ...)

  #define NSDL3_HLS(void_vptr, void_cptr, ...)

  #define NSDL4_HLS(void_vptr, void_cptr, ...)

  //for XMPP

  #define NSDL1_XMPP(void_vptr, void_cptr, ...)

  #define NSDL2_XMPP(void_vptr, void_cptr, ...)

  #define NSDL3_XMPP(void_vptr, void_cptr, ...)

  #define NSDL4_XMPP(void_vptr, void_cptr, ...)
  
  //for JMS

  #define NSDL1_JMS(void_vptr, void_cptr, ...)

  #define NSDL2_JMS(void_vptr, void_cptr, ...)

  #define NSDL3_JMS(void_vptr, void_cptr, ...)

  #define NSDL4_JMS(void_vptr, void_cptr, ...)

  //for netHavoc
  #define NSDL1_NH(void_vptr, void_cptr, ...)

  #define NSDL2_NH(void_vptr, void_cptr, ...)

  #define NSDL3_NH(void_vptr, void_cptr, ...)

  #define NSDL4_NH(void_vptr, void_cptr, ...)

  //for RDPP  bug 79149
  #define NSDL1_RDP(void_vptr, void_cptr, ...)
  #define NSDL2_RDP(void_vptr, void_cptr, ...)
  #define NSDL3_RDP(void_vptr, void_cptr, ...)
  #define NSDL4_RDP(void_vptr, void_cptr, ...)
 
#endif


//#define debug_log ns_debug_log
#define debug_log ns_debug_log_ex

#define DEBUG_HEADER "Absolute Time Stamp|Relative Time Stamp|File|Line|Function|Group|Parent/Child|User Index|Session Instance|Page|Instance|Logs"

#define NS_DUMP_WARNING(...)  ns_sess_warning_log(__VA_ARGS__);

extern int debug_fd;
extern int error_fd;
extern int kw_set_debug(char *text, int *to_change, char *err_msg, int runtime_flag);

//extern void error_log(char *format, ...);
//extern void ns_debug_log(int log_level, unsigned int mask, char *file, int line, char *fname, char *format, ...);
//extern void ns_debug_log_ex(int log_level, unsigned int mask, char *file, int line, char *fname, VUser *void_vptr, connection *void_cptr, char *format, ...); 
extern void ns_debug_log_ex(int log_level, unsigned long long mask, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...); 
extern int ns_debug_log_scr(int log_level, unsigned int mask, char *file, int line, char *fname, char *format, ...);

extern void error_log_ex(int log_level, char *file, int line, char *fname, 
                         //VUser *void_vptr, connection *void_cptr, char *error_id, 
                         void *void_vptr, void *void_cptr, char *error_id, 
                         char *error_attr, char *format, ...);

extern void kw_set_debug_mask(char *text, int *to_change);
extern void kw_set_debug_test_settings(char *buf);
extern int kw_set_modulemask(char *buf, unsigned long long *to_change, char *err_msg, int runtime_flag);
extern int kw_set_max_debug_log_file_size(char *buf, char *err_msg, int runtime_flag);
int kw_set_max_error_log_file_size(char*buf, char *err_msg, int runtime_flag);
extern int give_debug_error(int runtime_flag, char *err_msg);
extern int debug_log_value; //using for debug log file in nsa_logger and nsu_logging_reader:Ajeet Singh
extern void open_log(char *name, int *fd, unsigned int max_size, char *header);
extern void dns_resolve_log_write(long long cur_partition_idx, char *dns_status, char *server_name, int dns_lookup_time, struct sockaddr_in6 *resolved_ip);
extern int dns_resolve_log_switch_partition();
extern void dns_resolve_log_open_file();
extern int dns_resolve_log_flush_buf();
extern int dns_resolve_log_close_file();

extern char script_execution_fail_msg[];
extern int sess_warning_fd;
extern void open_sess_file(int *fd);
extern void ns_sess_warning_log(char *format, ...);
#endif

//End of file
