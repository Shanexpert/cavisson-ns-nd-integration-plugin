#ifndef _NS_COMMON_H 
#define _NS_COMMON_H

#define MAX_TNAME_LENGTH 128
#define MAX_EVENT_DEFINITION_FILE 10
#define CONNS_PER_CLIENT 50
#define MAX_FILE_NAME 10*1024
#define MAX_SCENARIO_LEN 1024 
#define MAX_NAME_LENGTH 32
//#define MAX_DATA_LINE_LENGTH 512
#define MAX_NS_VARIABLE_NAME 128
#define MAX_UNIX_FILE_NAME 255

#define INIT_USER_ENTRIES 10
#define DELTA_USER_ENTRIES 10
#define INIT_IP_ENTRIES 100
#define DELTA_IP_ENTRIES 100
#define INIT_CLIENT_ENTRIES 10
#define DELTA_CLIENT_ENTRIES 10
//#define INIT_SERVER_ENTRIES 5
//#define DELTA_SERVER_ENTRIES 5
#define INIT_SESSION_ENTRIES 128
#define DELTA_SESSION_ENTRIES 128
#define DELTA_TX_ENTRIES 16
#define INIT_PAGE_ENTRIES 1024
#define DELTA_PAGE_ENTRIES 1024
#define INIT_SVR_ENTRIES 1024
#define DELTA_SVR_ENTRIES 1024
#define INIT_VAR_ENTRIES 1024
#define DELTA_VAR_ENTRIES 1024
#define INIT_GROUP_ENTRIES 128
#define INIT_REPEAT_BLOCK_ENTRIES 128
#define DELTA_REPEAT_BLOCK_ENTRIES 64
#define DELTA_GROUP_ENTRIES 128
/* This contains number of entries of values (4K) and number of segments (46K)*/
#define INIT_POINTER_ENTRIES 50*1024
#define DELTA_POINTER_ENTRIES 8*1024
/* This contains number of vars (46K) and weights (4K) */
#define INIT_WEIGHT_ENTRIES 50*1024
#define DELTA_WEIGHT_ENTRIES 8*1024
#define INIT_LOCATTR_ENTRIES 32
#define DELTA_LOCATTR_ENTRIES 32
#define INIT_LINECHAR_ENTRIES 32*32
#define DELTA_LINECHAR_ENTRIES 32*32
#define INIT_ACCATTR_ENTRIES 24
#define DELTA_ACCATTR_ENTRIES 24
#define INIT_ACCLOC_ENTRIES 24
#define DELTA_ACCLOC_ENTRIES 24
#define DELTA_SCREEN_SIZE_ENTRIES 24
#define INIT_BROWATTR_ENTRIES 8
#define DELTA_BROWATTR_ENTRIES 8
#define INIT_MACHATTR_ENTRIES 8
#define DELTA_MACHATTR_ENTRIES 8
#define INIT_FREQATTR_ENTRIES 8
#define DELTA_FREQATTR_ENTRIES 8
#define INIT_SESSPROF_ENTRIES 128
#define DELTA_SESSPROF_ENTRIES 128
#define INIT_SESSPROFINDEX_ENTRIES 24
#define DELTA_SESSPROFINDEX_ENTRIES 24
#define INIT_USERPROF_ENTRIES 128
#define DELTA_USERPROF_ENTRIES 128
#define INIT_USERINDEX_ENTRIES 24
#define DELTA_USERINDEX_ENTRIES 24
#define INIT_RUNPROF_ENTRIES 128
#define DELTA_RUNPROF_ENTRIES 128
#define INIT_RUNINDEX_ENTRIES 24
#define DELTA_RUNINDEX_ENTRIES 24
#define INIT_SERVERORDER_ENTRIES 512
#define DELTA_SERVERORDER_ENTRIES 512
#define DELTA_SLA_ENTRIES 8
#define INIT_METRIC_ENTRIES 8
#define DELTA_METRIC_ENTRIES 8
#define INIT_REQUEST_ENTRIES 10*1024
#define DELTA_REQUEST_ENTRIES 4*1024
#define INIT_METHOD_ENTRIES 7  // Number of HTTP methods as per RFC
#define INIT_POST_ENTRIES 128
#define DELTA_POST_ENTRIES 128
#define INIT_HOST_ENTRIES 1024
#define DELTA_HOST_ENTRIES 1024
#define INIT_REPORT_ENTRIES 8
#define DELTA_REPORT_ENTRIES 8
#define INIT_DYNVAR_ENTRIES 16
#define DELTA_DYNVAR_ENTRIES 16
#define INIT_REQDYNVAR_ENTRIES 16
#define DELTA_REQDYNVAR_ENTRIES 16
#define INIT_ERRORCODE_ENTRIES 64
#define DELTA_ERRORCODE_ENTRIES 32
#define INIT_TAG_ENTRIES 64
#define DELTA_TAG_ENTRIES 32
#define INIT_TAGPAGE_ENTRIES 64
#define DELTA_TAGPAGE_ENTRIES 32
#define INIT_ATTRQUAL_ENTRIES 128
#define DELTA_ATTRQUAL_ENTRIES 64
#define INIT_SEG_ENTRIES 10*1024*2
#define DELTA_SEG_ENTRIES 4*1024*2
#define INIT_INUSESVR_ENTRIES 8
#define DELTA_INUSESVR_ENTRIES 8
#define INIT_INUSEUSER_ENTRIES 8
#define DELTA_INUSEUSER_ENTRIES 8
#define INIT_THINKPROF_ENTRIES 1024
#define DELTA_THINKPROF_ENTRIES 1024
#define INIT_INLINE_DELAY_ENTRIES 4
#define DELTA_INLINE_DELAY_ENTRIES 4
#define INIT_AUTOFETCH_ENTRIES 32 // for auto fetch embedded
#define DELTA_AUTOFETCH_ENTRIES 32 // for auto fetch embedded
#define INIT_CONT_ON_ERR_ENTRIES 32 // for continue on page error
#define INIT_OVERRIDE_RECORDED_THINK_ENTRIES 32 // for override recorded think
#define DELTA_CONT_ON_ERR_ENTRIES 32 // for continue on page error 
#define DELTA_RECORDED_THINK_TIME_ENTRIES 32 // for override recorded think time 
#define INIT_CLICKACTION_ENTRIES 128 // for click script user actions
#define DELTA_CLICKACTION_ENTRIES 128 // for click script user actions
#define INIT_PACING_ENTRIES 128
#define DELTA_PACING_ENTRIES 128
#define INIT_PERPAGESERVAR_ENTRIES 64
#define INIT_PERPAGEJSONVAR_ENTRIES 64  //for JSON Var
#define INIT_PERPAGERANDVAR_ENTRIES 64
#define DELTA_PERPAGERANDVAR_ENTRIES 64
#define INIT_PROXY_SVR_ENTRIES 2     //One entry kept for system proxy
#define DELTA_PROXY_SVR_ENTRIES 32  
#define DELTA_PROXY_EXCP_ENTRIES 32 
#define DELTA_PROXY_INTERFACE_ENTRIES 8 

#define DELTA_PERPAGESERVAR_ENTRIES 32
#define INIT_PERPAGECHKPT_ENTRIES 64
#define DELTA_PERPAGECHKPT_ENTRIES 32
#define INIT_PERPAGECHK_REPSIZE_ENTRIES 64
#define DELTA_PERPAGECHK_REPSIZE_ENTRIES 32
//#define INIT_TEMPARRAYVAL_ENTRIES 256 
//#define DELTA_TEMPARRAYVAL_ENTRIES 128
#define INIT_CLUSTVAR_ENTRIES 32
#define DELTA_CLUSTVAR_ENTRIES 16
#define INIT_CLUSTVAL_ENTRIES 128
#define DELTA_CLUSTVAL_ENTRIES 64
#define INIT_CLUST_ENTRIES 32
#define DELTA_CLUST_ENTRIES 16
#define INIT_GROUPVAR_ENTRIES 32
#define DELTA_GROUPVAR_ENTRIES 16
#define INIT_GROUPVAL_ENTRIES 128
#define DELTA_GROUPVAL_ENTRIES 64
#define INIT_RUNGROUP_ENTRIES 32
#define DELTA_RUNGROUP_ENTRIES 16
#define INIT_BIGBUFFER 1024*1024
#define DELTA_BIGBUFFER 10*1024
#define INIT_TEMPBUFFER 32*1024
#define DELTA_TEMPBUFFER 5*1024
#define STATUS_CODE_ARRAY_SIZE 1000   // array size to hold status code settings from 0-999

#define DELTA_ADD_HEADER_ENTRIES 16
#define MAX_HEADER_LENGTH        64*1024
#define MAX_HEADER_NAME_LEN      255
#define MAX_HEADER_VALUE_LEN     1024
#define DEFAULT_PORT_NUM 1025
#define SUCCESS 1
#define FAILURE -1
#define INVALID_VECTOR -2
// Moved from ns_parent.c
#define STAND_ALONE 0 
#define MASTER_LOADER 1
#define CLIENT_LOADER 2
#define MAX_GRANULES 1000
#define MAX_BUCKETS 10
#define DEFAULT_GRANULE_SIZE 10
#define DEFAULT_GRANULES 1000
#define DEFAULT_BUCKETS 10

#define USE_SPECIFIED_DATA_DIR 1

/*Above is moved form util.h*/

//Bits for protocol_enabled (used in global_settings)
#define HTTP_PROTOCOL_ENABLED		        0x00000001
#define HTTP_CACHE_ENABLED		        0x00000002
#define SMTP_PROTOCOL_ENABLED		        0x00000004
#define POP3_PROTOCOL_ENABLED		        0x00000008
#define FTP_PROTOCOL_ENABLED		        0x00000010
#define DNS_PROTOCOL_ENABLED		        0x00000020
#define TR069_PROTOCOL_ENABLED		        0x00000040
#define DOS_ATTACK_ENABLED 		        0x00000080
#define YAHOO_PROTOCAL_ENABLED 		        0x00000100
#define CLICKSCRIPT_PROTOCOL_ENABLED		0x00000200
#define RBU_API_USED	                        0x00000400
#define USER_SOCKET_PROTOCOL_ENABLED            0x00000800
#define NETWORK_CACHE_STATS_ENABLED             0x00001000  //Bit set indicates any of the group has network cache stats enabled
#define TX_END_NETCACHE_ENABLED                 0x00002000  //Bit set indicates any of the group has tx end with network cache enabled
#define DNS_CACHE_ENABLED                       0x00004000  //Dns cached enabled.
#define LDAP_PROTOCOL_ENABLED                   0x00008000
#define IMAP_PROTOCOL_ENABLED                   0x00010000
#define JRMI_PROTOCOL_ENABLED                   0x00020000         // JRMI
#define WS_PROTOCOL_ENABLED                     0x00040000         // WS
#define MONGODB_PROTOCOL_ENABLED                0x00080000  //Binary: 1000 0000 0000 0000 0000
#define JMETER_PROTOCOL_ENABLED                 0x00100000  //Binary: 1000 0000 0000 0000 0000
#define RTE_PROTOCOL_ENABLED                    0x00200000  //RTE
#define CASSDB_PROTOCOL_ENABLED                 0x00400000  //Binary: 0100 0000 0000 0000 0000 0000
#define SOCKJS_PROTOCOL_ENABLED                 0x00800000  //SockJS
#define XMPP_PROTOCOL_ENABLED                   0x01000000  //For xmpp
#define FC2_PROTOCOL_ENABLED                    0x02000000  //For fc2
#define JMS_PROTOCOL_ENABLED                    0x04000000  //For jms
#define HTTP2_SERVER_PUSH_ENABLED	        0x08000000  /* bug 70480 for HTTP2 Server Push*/
#define SOCKET_TCP_CLIENT_PROTO_ENABLED         0x10000000 //IF this bit set then API ns_socket_()is used as CLIENT for TCP protocol
#define SOCKET_UDP_CLIENT_PROTO_ENABLED         0x20000000 // IF this bit set then API ns_socket_() is used as CLIENT for UDP protocol
#define SOCKET_TCP_SERVER_PROTO_ENABLED         0x40000000 // IF this bit set then API ns_socket_() is used as SERVER for UDP protocol
#define SOCKET_UDP_SERVER_PROTO_ENABLED         0x80000000 // IF this bit set then API ns_socket_() is used as SERVER for UDP protocol

//#define IMAPS_PROTOCOL_ENABLED    0x00020000
#define THROTTLE 3360

#endif