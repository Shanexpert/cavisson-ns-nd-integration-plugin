/******************************************************************
 * Name    :    ns_gdf.h
 * Author  :    Anuj
 * Purpose :    Contain data structure and variable declaration for ns_gdf.c
 * Modification History:
 * 08/09/07:  Anuj - Initial Version
*****************************************************************/

#ifndef _ns_gdf_h
#define _ns_gdf_h

#include "ns_data_types.h"
#include "ns_monitor_metric_priority.h"
#include "nslb_get_norm_obj_id.h"
extern NormObjKey *g_gdf_hash;
// Static Groups
#define VUSER_RPT_GRP_ID                    1
#define SSL_RPT_GRP_ID                      2
#define URL_HITS_RPT_GRP_ID                 3
#define PG_DOWNLOAD_RPT_GRP_ID              4
#define SESSION_RPT_GRP_ID                  5
#define TRANSDATA_RPT_GRP_ID                6
#define URL_FAIL_RPT_GRP_ID                 7
#define PAGE_FAIL_RPT_GRP_ID                8
#define SESSION_FAIL_RPT_GRP_ID             9
#define TRANS_FAIL_RPT_GRP_ID               10
#define HTTP2_SERVER_PUSH_ID		    11  /*bug 70480 : http2_server_push id added*/
// SMTP Related
#define SMTP_NET_TROUGHPUT_RPT_GRP_ID       13
#define SMTP_HITS_RPT_GRP_ID                14
#define SMTP_FAIL_RPT_GRP_ID                15

/* POP3 */
#define POP3_NET_TROUGHPUT_RPT_GRP_ID       16
#define POP3_HITS_RPT_GRP_ID                17
#define POP3_FAIL_RPT_GRP_ID                18

/* FTP */
#define FTP_NET_TROUGHPUT_RPT_GRP_ID       19
#define FTP_HITS_RPT_GRP_ID                20
#define FTP_FAIL_RPT_GRP_ID                21

/* DNS */
#define DNS_NET_TROUGHPUT_RPT_GRP_ID       22 /* varify with GDF: TODO:JAI */
#define DNS_HITS_RPT_GRP_ID                23
#define DNS_FAIL_RPT_GRP_ID                24

#define HTTP_CACHING_RPT_GRP_ID		   26 /*Group for HTTP Caching*/

//DOS Attacks Group
#define DOS_ATTACK_RPT_GRP_ID	           27 //Group for Doss Attack

//NS Diagnosis
#define NS_DIAGNOSIS_RPT_GRP_ID            28

//HTTP Proxy
#define HTTP_PROXY_RPT_GRP_ID                29

#define HTTP_NETWORK_CACHE_RPT_GRP_ID       30

//DNS Lookup
#define DNS_LOOKUP_RPT_GRP_ID               31
// Dynamic Groups
#define SERVER_STAT_RPT_GRP_ID              101
#define TRANS_STATS_RPT_GRP_ID              102
#define TRANS_CUM_STATS_RPT_GRP_ID          103
#define GRP_DATA_STAT_RPT_GRP_ID            106
#define RBU_PAGE_STAT_GRP_ID                107    // RBU : pageStat Graph Id
#define PAGE_BASED_STAT_GRP_ID              108    // Page Based Stat
#define RBU_DOMAIN_STAT_GRP_ID              109

/* group for http status codes. */
//#define HTTP_STATUS_CODE_RPT_GRP_ID         110  // New for making this vector grp in 4.1.13

/* LDAP */
#define LDAP_NET_TROUGHPUT_RPT_GRP_ID       32
#define LDAP_HITS_RPT_GRP_ID                33
#define LDAP_FAIL_RPT_GRP_ID                34

/* IMAP */
#define IMAP_NET_TROUGHPUT_RPT_GRP_ID       35
#define IMAP_HITS_RPT_GRP_ID                36
#define IMAP_FAIL_RPT_GRP_ID                37

/* JRMI */
#define JRMI_NET_TROUGHPUT_RPT_GRP_ID       38
#define JRMI_HITS_RPT_GRP_ID                39
#define JRMI_FAIL_RPT_GRP_ID                40

/* WS */
#define WS_RPT_GRP_ID                       41
#define WS_STATUS_CODES_RPT_GRP_ID          42
#define WS_FAILURE_STATS_RPT_GRP_ID         43

/*  XMPP */
#define XMPP_STAT_GRP_ID                    44
/* FC2 */
#define FC2_RPT_GRP_ID                      45

#define TCP_CLIENT_GRP_ID                   46
#define UDP_CLIENT_GRP_ID                   47
#define TCP_SERVER_GRP_ID                   48
#define UDP_SERVER_GRP_ID                   49

#define IP_BASED_STAT_GRP_ID		    10294
#define SHOW_VUSER_FLOW_STAT_GRP_ID         10469
#define SRV_IP_STAT_GRP_ID		    10506
#define HTTP_STATUS_CODE_RPT_GRP_ID         10519  // New for making this vector grp in 4.1.15
#define TCP_CLIENT_FAILURES_RPT_GRP_ID      10520  
#define UDP_CLIENT_FAILURES_RPT_GRP_ID      10521  

// Opcode values
#define MSG_START_PKT  0
#define MSG_DATA_PKT   1
#define MSG_END_PKT    2
#define MSG_PRE_TEST_PKT 3
#define MSG_POST_TEST_PKT 4

#define PAUSE_TEST     11
#define RESUME_TEST    12

#define GROUP_GRAPH_TYPE_SCALAR  0
#define GROUP_GRAPH_TYPE_VECTOR  1

#define GRAPH_NOT_EXCLUDED                     0
#define GRAPH_EXCLUDED                         100 // Since METRIC_PRIORITY_MEDIUM also has 1 so we are taking it as 100 to ignore conflict 

#define DATA_TYPE_SAMPLE                       0
#define DATA_TYPE_RATE                         1
#define DATA_TYPE_CUMULATIVE                   2
#define DATA_TYPE_TIMES                        3
#define DATA_TYPE_TIMES_STD                    4
#define DATA_TYPE_SUM 	                       5
#define DATA_TYPE_RATE_4B_1000                 6
#define DATA_TYPE_TIMES_4B_1000                7
#define DATA_TYPE_SAMPLE_2B_100                8
#define DATA_TYPE_SAMPLE_4B_1000               9
#define DATA_TYPE_SAMPLE_2B_100_COUNT_4B       10
#define DATA_TYPE_SUM_4B                       11
#define DATA_TYPE_TIMES_STD_4B_1000            12
#define DATA_TYPE_SAMPLE_4B                    13
#define DATA_TYPE_SAMPLE_1B                    14
#define DATA_TYPE_SAMPLE_4B                    13
#define DATA_TYPE_SAMPLE_1B                    14
#define DATA_TYPE_TIMES_4B_10                  15
#define DATA_TYPE_T_DIGEST                     16
#define FORMULA_SEC   0
#define FORMULA_PM    1
#define FORMULA_PS    2
#define FORMULA_KBPS  3
#define FORMULA_DBH   4
#define DATA_TYPE_SAMPLE_4B                    13
#define DATA_TYPE_SAMPLE_1B                    14
#define MAX_LONG_LONG_VALUE 0xffffffffffffffffll

#define MAX_INT_16BIT_THOUSAND   65535000
#define MAX_VALUE_4B_U  0xFFFFFFFF
#define MAX_VALUE_8B_U  0xFFFFFFFFFFFFFFFF

//Macro for Graph Elements for Times type
#define TIMES_AVG_DATA          0
#define TIMES_MIN_DATA          1
#define TIMES_MAX_DATA          2
#define TIMES_COUNT_DATA        3

#define TIMES_MAX_NUM_ELEMENTS       4
#define TIMES_STD_MAX_NUM_ELEMENTS   5

// Monitor deleted by user, runtime changes resulting in monitor delete. This will only start when start monitor is done
// Previously we are setting monitor state INACTIVE in any server case but this is wrong now we are setting DELETED state for any_server case
// So that we donot apply moonitor again in outbound case for previously active server i.e done in send_monitor_request_to_NDC 
#define DELETED_MONITOR		1
//Monitor running, message already sent to NDC
#define RUNNING_MONITOR		2
//Monitor inactive due to server inactive
#define INACTIVE_MONITOR	3
//Initial state, denotes monitor request not sent to NDC
#define INIT_MONITOR		4
//This is set when monitor is deleted at partition switch
#define CLEANED_MONITOR		5
#define RESTART_MON_ON_INVALID_VECTOR   6

//All Vector formats for hierarchical view
//'T' stands fir 'TIER' and 'S' stands for 'SERVER'
#define BREADCRUMB_FORMAT_T                                 1
#define BREADCRUMB_FORMAT_T_S                               2
#define BREADCRUMB_FORMAT_T_S_APP                           3
#define BREADCRUMB_FORMAT_T_S_DESTINATION                   4
#define BREADCRUMB_FORMAT_T_S_DEVICE                        5
#define BREADCRUMB_FORMAT_T_S_DISKPARTITION                 6
#define BREADCRUMB_FORMAT_T_S_GLOBAL                        7
#define BREADCRUMB_FORMAT_T_S_INSTANCE                      8
#define BREADCRUMB_FORMAT_T_S_INSTANCE_APPNAME              9
#define BREADCRUMB_FORMAT_T_S_INSTANCE_CACHENAME            10
#define BREADCRUMB_FORMAT_T_S_INSTANCE_CLUSTERNAME          11
#define BREADCRUMB_FORMAT_T_S_INSTANCE_CONDITION            12
#define BREADCRUMB_FORMAT_T_S_INSTANCE_FAMILY               13
#define BREADCRUMB_FORMAT_T_S_INSTANCE_METHOD               14
#define BREADCRUMB_FORMAT_T_S_INSTANCE_POOLNAME             15
#define BREADCRUMB_FORMAT_T_S_INSTANCE_SERVLETNAME          16
#define BREADCRUMB_FORMAT_T_S_INSTANCE_THREADPOOL           17
#define BREADCRUMB_FORMAT_T_S_INTERFACE                     18
#define BREADCRUMB_FORMAT_T_S_PAGENAME                      19
#define BREADCRUMB_FORMAT_T_S_PROCESSOR                     20
#define BREADCRUMB_FORMAT_T_S_QUEUENAME                     21
#define BREADCRUMB_FORMAT_T_S_SERVICENAME                   22
#define BREADCRUMB_FORMAT_T_S_TOPICNAME                     23
#define BREADCRUMB_FORMAT_T_S_TOTAL                         24
#define BREADCRUMB_FORMAT_UNKNOWN                           -1
#define BREADCRUMB_FORMAT_NONE                              -2



// RTG Array Index 
#define VUSER 										0
#define NET_THROUGHPUT 							                1
#define URL_HITS 									2
#define PAGE_DOWNLOAD 							                3
#define SESSION_PTR 								        4
#define TRANS_OVERALL 							                5
#define URL_FAIL 									6
#define PAGE_FAIL 									7
#define SESSION_FAIL 								        8
#define TRANS_FAIL 									9
#define NO_SYSTEM_STATS 								10
#define NO_NETWORK_STATS 								11
#define SERVER_STATS 									12
#define TRANS_STATS 									13
#define TRANS_CUM_STATS 								14
#define TUNNEL_STATS 									15
#define SMTP_NET_THROUGHPUT 								16
#define SMTP_HITS 									17
#define SMTP_FAIL 									18
#define POP3_NET_THROUGHPUT 								19
#define POP3_HITS 									20
#define POP3_FAIL 									21
#define FTP_NET_THROUGHPUT 								22
#define FTP_HITS 									23
#define FTP_FAIL 									24
#define LDAP_NET_THROUGHPUT 								25
#define LDAP_HITS 									26
#define LDAP_FAIL 									27
#define IMAP_NET_THROUGHPUT 								28
#define IMAP_HITS 									29
#define IMAP_FAIL 									30
#define JRMI_NET_THROUGHPUT 								31
#define JRMI_HITS 									32
#define JRMI_FAIL 									33
#define DNS_NET_THROUGHPUT 								34
#define DNS_HITS 									35
#define DNS_FAIL 									36
#define HTTP_STATUS_CODES 								37
#define HTTP_CACHING 									38
#define NS_DIAG 									39
#define DOS_ATTACK 									40
#define HTTP_PROXY 									41
#define HTTP_NETWORK_CACHE_STATS 							42
#define DNS_LOOKUP_STATS 								43
#define GROUP_DATA 									44
#define RBU_PAGE 									45
#define PAGE_BASED 									46
#define VUSER_FLOW_BASED                                                                47
#define SRV_IP_STAT                                                                     48
#define IP_BASED 									49
#define WS_STATS                                                                        50
#define WS_STATUS_CODES                                                                 51
#define WS_FAILURE_STATS                                                                52
#define RBU_DOMAIN                                                                      53
#define XMPP_STATS                                                                      54
#define FC2_STATS                                                                       55
#define SERVER_PUSH_STATS								56  /*bug 70480 HTTP2 SERVER PUSH*/
#define TCP_CLIENT_RTG_IDX                                                              57 
#define TCP_CLIENT_FAILURES_RTG_IDX                                                     58 
#define TCP_SERVER_RTG_IDX                                                              59
#define TCP_SERVER_FAILURES_RTG_IDX                                                     60 
#define UDP_CLIENT_RTG_IDX                                                              61
#define UDP_CLIENT_FAILURES_RTG_IDX                                                     62 
#define UDP_SERVER_RTG_IDX                                                              63
#define UDP_SERVER_FAILURES_RTG_IDX                                                     

#define SHOW_GRP_DATA ((global_settings->show_group_data == SHOW_GROUP_DATA_ENABLED)?SHOW_GROUP_DATA_ENABLED:0)

#define TOTAL_ENTERIES ((global_settings->show_group_data == SHOW_GROUP_DATA_ENABLED)?((sgrp_used_genrator_entries + 1) * (total_runprof_entries + 1)):(sgrp_used_genrator_entries + 1))

#define TOTAL_GRP_ENTERIES_WITH_GRP_KW ((global_settings->show_group_data == SHOW_GROUP_DATA_ENABLED)?(total_runprof_entries + 1):1)
#define NO_GROUP_PROCESSED "NoGroupProcessed"


#define RTG_INDEX_NOT_SET                  -1
#define FILL_RTG_INDEX_AT_INITTIME          0
#define FILL_RTG_INDEX_AT_RUNTIME           1

#define IS_HML_APPLIED (global_settings->enable_hml_group_in_testrun_gdf && info_ptr && !is_user_monitor)

typedef struct Graph_Data{
  Long_long_data c_avg;               // Cumulative avg value of data since start of test run
  Long_long_data c_min;               // min time (millisec)
  Long_long_data c_max;               // max time (millisec)
  Long_long_data c_count;             // Same value as in succ
  Long_long_data c_sum_of_sqr;   // ??
} Graph_Data;

/* Graph_Info: This structure stores graph related informations. 
   Graph information line
   Graph|graphName|rptId|graphType|dataType|graphDataIndex|formula|numVectors|graphState|PDF_ID|Percentile_Data_Idx|future2|future3|GRAPH_DESCRIPTION
   Graph|Number of requests/second|1|scalar|times_4B_1000|-|-|0|AS|-1|-1|H|NA|Number of requests per second 
*/
typedef struct Graph_Info
{
  char graph_type;
    /* graph_type: 4th field of Graph line. It provides graph's type i.e scalar or vector. */
  char data_type;
    /* data_type: 5th field of Graph line. It provides data's type i.e sample, rate, cummulative, times, etc..*/
  char formula;
    /* formula: 7th field of Graph line. It provides formula associated with graph*/ 
  char graph_state;
    /* graph_state: 9th field of Graph line. It provide whether graph is excluded or not? 
                    0 for including the graph and 1 for excluding the graph */ 
  char metric_priority;
    /* metric_priority: 12th field of Graph line. It provides graph metric priority */ 
  unsigned char next_hml;
    /* next_hml: This filed will keep track of next H/M/L graph i.e. array linked list of H/M/L */ 
  unsigned short rpt_id;
    /* rpt_id: 3rd field of Graph line. It provides graph ID. */ 
  unsigned short num_vectors;     
    /* num_vectors: 8th field of Graph line. It provides number of graph's vectors. Default number of vector is 0. */ 
  unsigned int graph_msg_index;  
    /* graph_msg_index: Provide index in msg_data_ptr for this graph. It is used only for user monitor. */ 
  unsigned int pdf_data_index;  
    /* pdf_data_index: Provide index in pdf_shm_info for this graph. */ 
  unsigned int cm_index;
    /* cm_index: Provide index in CM_info (i.e. cm_info_ptr). */ 
  int pdf_info_idx;
    /* pdf_info_idx: Provide index in Pdf_Lookup_Data (i.e. pdf_lookup_data). By this graph and their pdf linked. */ 
  int pdf_id;
    /* pdf_id: Provide pdf id gathered from pdf file. */ 
  char *graph_name;

  char *derived_formula;
    /* graph_name: 2nd field of Graph line. It provides graph name */ 
  char *gline;
    /* gline: Provides complete original graph line for this graph. */ 
  //char *field_13;
    /* field_13: This implies future field*/
  char *graph_discription;
    /* graph_discription: 14th field in Graph line */ 
  char ***graph_data;
    /* graph_data: 3D pointer leafs of which lead to Graph_Data struct */ 
  char **vector_names;
    /* vector_names: 2D array to store graph's vector name list. */ 
} Graph_Info;

//Group|groupName|rptGrpId|groupType|numGraphs|numVectors|future1|future2|Group_Description
//Group|MPStat Solaris|10087|vector|15|0|System Metrics|Tier>Server>Processor|Dynamic mpstat monitor
typedef struct Group_Info
{
  char group_type;               //Field 4th in Group line
  int rpt_grp_id;                //Field 3rd in Group line
  unsigned int num_vectors[MAX_METRIC_PRIORITY_LEVELS + 1];      //Field 6th in Group line, Its default value is 0.
  int graph_info_index;    //Index in graph info table for this group
  unsigned short num_graphs;    //Field 5th in Gruop line, itd default value is 0
  unsigned short num_actual_graphs[MAX_METRIC_PRIORITY_LEVELS + 1];  //stores the actual number of graphs to be shown of a group Last field will store total
  unsigned short tot_hml_graph_size[MAX_METRIC_PRIORITY_LEVELS + 1];
  int pdf_graph_data_size;
  int breadcrumb_format;
  char *group_name;              //Field 2nd in Group line
  char *group_description;       //Field 9th in Group line
  char *Hierarchy;               //Field 8th in Group line
  char *groupMetric;
  char *excluded_graph;		 //an array which keeps track of graph to be shown [ 0 - exclude and 1 - include ] 
  char **vector_names;           //2D array to store vector list of this group
  int *hml_relative_idx;         //array to store relative index of HML graphs
  long mg_gid;  
} Group_Info;


#pragma pack(push, 1)        //Avoid auto padding by gcc
// This is the msg data, which will be there for each graph
typedef struct
{
  Long_data opcode;                       // The opcode value
  Long_data test_run_num;                 // The test run no
  Long_data interval;                     // This is the interval betweeen two sampling periods
  Long_data seq_no;                       // This will specifies the data pkt no
  Long_data partition_idx;  
  Long_data abs_timestamp;  
  Long_data gdf_seq_no;
  Long_data pdf_seq_no;
  Long_data cav_main_pid;
  double    future[5];                   // For future (Whole struct must be divisible by 4)  //TODO ONLY TESTING 
} Msg_data_hdr;
// This struct is for timesStd data element for objects of the desired type in a sampling period
typedef struct
{
  Long_data avg_time;                   // avg time (millisec)
  Long_data min_time;                   // min time (millisec)
  Long_data max_time;                   // max time (millisec)
  Long_data succ;                       // Same value as in succ
  Long_data sum_of_sqr;            // ??
} Times_std_data;

typedef struct
{
  Int_data avg_time;                   // avg time (millisec)
  Int_data min_time;                   // min time (millisec)
  Int_data max_time;                   // max time (millisec)
  Int_data succ;                       // Same value as in succ
  Int_data sum_of_sqr;            
} Times_std_data_4B;

// This struct is for times data element for objects of the desired type in a sampling period
typedef struct
{
  Long_data avg_time;                   // avg time (millisec)
  Long_data min_time;                   // min time (millisec)
  Long_data max_time;                   // max time (millisec)
  Long_data succ;                       // Same value as in succ
} Times_data;

typedef struct
{
  Int_data avg_time;                   // avg time (millisec)
  Int_data min_time;                   // min time (millisec)
  Int_data max_time;                   // max time (millisec)
  Int_data succ;                       // Same value as in succ
} Times_data_4B;

// Virutal Users Gruop graph data
typedef struct
{
  Long_data num_running_users;         // Average number of running users in the sampling period. (avg_users)
  Long_data num_active_users;          // Average number of active users in the sampling period.
  Long_data num_thinking_users;        // Average number of thinking users in the sampling period.
  Long_data num_waiting_users;          // Average number of waiting users in the sampling period.   // added by Anuj: 12/09/07: the previous "num_thinking_users" is been broken up in to "num_thinking_users" and "num_waiting_users".
  Long_data num_idling_users;          // Average number of idling users in the sampling period.
  Long_data num_blocked_users;         // Average number of blocked users in the sampling period.
  Long_data num_paused_users;         // Average number of paused users in the sampling period.
  Long_data num_sp_users;              // Average number of SyncPoint users in the sampling period.
  Long_data num_connection;            // Average number of connection in the sampling period.
  Long_data avg_open_cps;              // connections/sec
  Long_data avg_close_cps;             // connections/sec
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Times_data sock_bind;                // Socket bind graph data 
} Vuser_gp;

// Net Throughput Group
typedef struct
{
  Long_data ssl_new;                   // (SSL new sessions/Sec)
  Long_data tot_ssl_new;
  Long_data ssl_reused;                // (SSL Reused session/sec)
  Long_data tot_ssl_reused;
  Long_data ssl_reuse_attempted;       // (SSL Reuse attempted/sec)
  Long_data tot_ssl_reuse_attempted;
  Long_data ssl_write_fail;            // SSL Write Failures/Sec
  Long_data ssl_handshake_fail;        // SSL Handshake Failures/Sec
   
} SSL_gp;

// Net Throughput Group
typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data smtp_body_throughput;
  Long_data tot_smtp_body;
} SMTP_Net_throughput_gp;

// Net Throughput Group
typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data pop3_body_throughput;
  Long_data tot_pop3_body;
} POP3_Net_throughput_gp;

typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data ftp_body_throughput;
  Long_data tot_ftp_body;
} FTP_Net_throughput_gp;

typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data ldap_body_throughput;
  Long_data tot_ldap_body;
} LDAP_Net_throughput_gp;

typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data imap_body_throughput;
  Long_data tot_imap_body;
} IMAP_Net_throughput_gp;

typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data jrmi_body_throughput;
  Long_data tot_jrmi_body;
} JRMI_Net_throughput_gp;

typedef struct
{
  Long_data avg_send_throughput;       // tcp bytes/sec
  Long_data tot_send_bytes;            // tcp bytes
  Long_data avg_recv_throughput;       // tcp bytes/sec
  Long_data tot_recv_bytes;            // tcp bytes
  Long_data avg_open_cps;              // connections/sec
  Long_data tot_conn_open;
  Long_data avg_close_cps;             // connections/sec
  Long_data tot_conn_close;
   
  Long_data dns_body_throughput;
  Long_data tot_dns_body;
} DNS_Net_throughput_gp;

// URL Hits Group
typedef struct
{
  Long_data url_req;                      // URL requests started/sec
  Long_data url_sent;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Times_data succ_response;               // 'Average Successful URL Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure URL Response Time (Seconds)'
  Times_data dns;                         // 'Average URL DNS lookup Time (Seconds)'
  Times_data conn;                        // 'Average URL Connect Time(Seconds)'
  Times_data ssl;                         // 'Average SSL Handshake Time (Seconds)'
  Times_data frst_byte_rcv;               // 'Average First Byte Recieve Time (Seconds)'
  Times_data dwnld;                       // 'Average Download Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
  SampleCount_data failure;               /*bug  103688  */// Shows the Number of failures in percentage corresponding to tries and succ 
  Long_data http_body_throughput;
  Long_data tot_http_body;
} Url_hits_gp;

// FC2 Group
typedef struct
{   
  Long_data fc2_req;                      // FC2 requests started/sec
  Long_data fc2_sent;                      // FC2 requests sent/sec
  Long_data fc2_tries;                        // 'FC2 Hits' - Total tries in a sampling period
  Long_data fc2_succ;                    // 'Success FC2 Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data fc2_response;                    // 'Average FC2 Response Time (Seconds)'
  Times_data fc2_succ_response;               // 'Average Successful FC2 Response Time (Seconds)'
  Times_data fc2_fail_response;               // 'Average Failure FC2 Response Time (Seconds)'
  Long_data fc2_cum_tries;                    // Total FC2 completed (tried or Hits) since start of test run
  Long_data fc2_cum_succ;                     // Total FC2 Success since start of test run
  Long_data fc2_failure;                      // Shows the Number of failures in percentage corresponding to tries and succ 
  Long_data fc2_body_throughput;
  Long_data fc2_body;
} fc2_gp;

/* SMTP */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} SMTP_hits_gp;

/* pop3 */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Times_data succ_response;               // 'Average Successful POP3 Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure POP3 Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} POP3_hits_gp;


/* ftp */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Times_data succ_response;               // 'Average Successful FTP Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure FTP Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} FTP_hits_gp;

/* ldap */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} LDAP_hits_gp;

/* imap */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Times_data succ_response;               // 'Average Successful IMAP Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure IMAP Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} IMAP_hits_gp;

/* jrmi */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} JRMI_hits_gp;

/* dns */
typedef struct
{
  Long_data url_req;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Times_data response;                    // 'Average URL Response Time (Seconds)'
  Times_data succ_response;               // 'Average Successful DNS Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure DNS Response Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
} DNS_hits_gp;

// Page Download Group
typedef struct
{
  Long_data pg_dl_started;
  Long_data tries;                        // Total page tries in the sampling period (Page Download/Minute)
  Long_data succ;                         // Success Page Responses/Minute
  Times_data response;                    // Total page responses in the sampling period
  Times_data succ_response;               // 'Average Successful Page Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure Page Response Time (Seconds)'
  Long_data cum_tries;                    // Total Page Completed(tried or Hits) since start of test run
  Long_data cum_succ;                     // Total Page Hits since start of test run
  Times_data page_js_proc_time;           // Time taken in processing the JS
  Times_data page_proc_time;              // Time taken to process the page response
  SampleCount_data failure;               /*bug  103688  */        // Page failure per Minute in percentage
} Page_download_gp;

// Session Group
typedef struct
{
  Long_data sess_started;
  Long_data tries;                        // Total session tries in the sampling period (Session/Minute)
  Long_data succ;                         // Successful Sessions/Minute
  Times_data response;                    // Average Session Response Time (Seconds)
  Times_data succ_response;               // 'Average Successful Session Response Time (Seconds)'
  Times_data fail_response;               // 'Average Failure Session Response Time (Seconds)'
  Long_data cum_tries;			  // Total Session Completed(tried or Hits) since start of test run
  Long_data cum_succ;			  // Total Session Hits since start of test run
  SampleCount_data failure;               /*bug  103688  */        // Session failure per Minute in percentage
} Session_gp;

// Transaction (Overall) Group
typedef struct
{
  Long_data trans_started;
  Long_data tries;                        // Total transaction tries in the sampling period (Transactions/Minute)
  Long_data succ;                         // Toatl transaction responses in the sampling period
  Times_data response;                    // Average Transaction Response Time (Seconds)
  Times_data succ_response;               // Successful Transaction Response Time (Seconds)
  Times_data fail_response;               // Failure Transaction Response Time (Seconds)
  Long_data cum_tries;                    // Total Transactions Completed(tried or Hits) since start of test run
  Long_data cum_succ;                     // Total Transactions Hits since start of test run
  SampleCount_data failure;                /*bug 85621 */      // Transaction Failure in percentage
  Times_data think_time;                  // Think Time (Seconds)
  Long_data tx_tx_throughput;
  Long_data tx_rx_throughput;
} Trans_overall_gp;
// URL Failures Group
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;  // Failed URL Responses/
  Long_data failure_count;
} URL_fail_gp;

/* SMTP */
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;
} SMTP_fail_gp;

/* pop3 */
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;
  Long_data failure_count;
} POP3_fail_gp;

/* ftp */
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;
  Long_data failure_count;
} FTP_fail_gp;

/* ldap */
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;
} LDAP_fail_gp;

/* imap */
typedef struct
{
  //Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;  // Failed URL Responses/
  Long_data failure_count;
} IMAP_fail_gp;

/* JRMI */
typedef struct
{
 // Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;  // Failed URL Responses/
} JRMI_fail_gp;
/* dns */
typedef struct
{
//  Long_data all_failures;                      // Failed URL Responses (All Errors) - Sum of all errors
  //Long_data failures[TOTAL_URL_ERR - 1];  // Failed URL Responses/
  Long_data failures;  // Failed URL Responses/
  Long_data failure_count;
} DNS_fail_gp;

// Page Failures Group
typedef struct
{
  Long_data failures;
  Long_data failure_count;
} Page_fail_gp;

// Session Failures Group
typedef struct
{
  //Long_data all_failures;                     // Failed Sessions (All Errors)/Minute
  //Long_data failures[TOTAL_SESS_ERR - 1]; // Failed Sessions/Minute
  Long_data failures;
  Long_data failure_count;
} Session_fail_gp;

// Transaction Failures (Overall) Group
typedef struct
{
  //Long_data all_failures;                       // Failed Transactions (All Errors)/Minute
  //Long_data failures[TOTAL_TX_ERR - 1];     // Failed Transactions/Minute
  Long_data failures;     // Failed Transactions/Minute
  Long_data failure_count;
} Trans_fail_gp;

// NetOcean System Stats Group
// This struct is no Longer used for filling GDF as we doing by pointer increament
// GDF and data coming from createserver must be in same sequence
typedef struct
{
  Long_data cpuUser;                     // Percentage of cpu occupied by user in the sampling period
  Long_data cpuSys;                      // Percentage of cpu occupied by system in the sampling period
  Long_data cpuTotalBusy;                // Percentage of cpu occupied by user and system in the sampling period
  Long_data pageIn;                      // PageIn per second
  Long_data pageOut;                     // PageOut per second
  Long_data swapIn;                      // SwapIn per second
  Long_data swapOut;                     // SwapIn per second
  Long_data interrupts;                  // Interrupts per second
  Long_data freeMem;                     // Free memory during the sampling period
  Long_data diskIn;                      // DiskIn per second
  Long_data diskOut;                     // DiskIn per second
  Long_data loadAvg1m;                   // load avg over 1 minute
  Long_data loadAvg5m;                   // load avg over 5 minute
  Long_data loadAvg15m;                  // load avg over 15 minute
} NO_system_stats_gp;

// NetOcean Network (TCP) Stats Group
// This struct is no Longer used for filling GDF as we doing by pointer increament
// GDF and data coming from createserver must be in same sequence
typedef struct
{
  Long_data ActiveOpens;                 // Active Opens ?
  Long_data PassiveOpens;                // Passive Opens
  Long_data AttemptFails;                // Attempt Failures
  Long_data EstabResets;                 // Establishment Resets
  Long_data CurrEstab;                   // Current Establishments
  Long_data InSegs;                      // In Segments
  Long_data OutSegs;                     // Out Segments
  Long_data RetransSegs;                 // Retransmitted Segments
  Long_data InErrs;                      // In Errors
  Long_data OutRsts;                     // Out Restarts
} NO_network_stats_gp;

// Server Stats Group
typedef struct
{
  Long_data cpuUser;                       // rstat->cp_time[?]
  Long_data cpuSys;                        // rstat->cp_time[?]
  Long_data cpuTotalBusy;                  // rstat->cp_time[3]
  Long_data pageIn;                        // rstat->v_pgpgin
  Long_data pageOut;                       // rstat->v_pgpgout
  Long_data swapIn;                        // rstat->v_pswpin
  Long_data swapOut;                       // rstat->v_pswpout
  Long_data diskIn;                        // rstat->dk_xfer[DK_NDRIVE ??]
  Long_data diskOut;                       // rstat->dk_xfer[DK_NDRIVE ??]
  Long_data interrupts;                    // rstat->v_intr
  Long_data loadAvg1m;                     // rstat->avenrun[?]
  Long_data loadAvg5m;                     // rstat->avenrun[?]
  Long_data loadAvg15m;                    // rstat->avenrun[?]
  Long_data InSegs;                        // In Ethernet Packets
  Long_data OutSegs;                       // In Ethernet Packets
  Long_data InErrs;                        // In Ethernet Errors
  Long_data OutRsts;                       // In Ethernet Errors
  Long_data collisions;                    // Collisions
  Long_data v_swtch;                       // Context Switches/Sec
} Server_stats_gp;

// Transaction Stats Group 
typedef struct
{
  Times_std_data time;     // Transaction time in seconds
  Long_data completed_ps;  // Transaction completed/sec in the sampling period
  Long_data netcache_pct;   // Transaction served from netcache percentage
  SampleCount_data failures_pct;  /*bug  103688  */ // Transaction served from failure percentage
  Times_std_data succ_time;   // Successful Transaction time in seconds
  Times_std_data fail_time;   // Failure Transaction time in seconds
  Times_std_data netcache_hits_time;   // Failure Transaction time in seconds
  Times_std_data netcache_miss_time;   // Failure Transaction time in seconds
  Times_data think_time;           // Transaction think time in seconds
  Long_data tx_tx_throughput;
  Long_data tx_rx_throughput;
} Trans_stats_gp;

// Transaction Cum Stats Group 
typedef struct
{
  Long_data completed_cum; // Cumulative Transaction completed since start of the test
  Long_data succ_cum;      // Cumulative Transaction sucessfull since start of the test
  Long_data failures_cum;  // Cumulative Transaction failures since start of the test
} Trans_cum_stats_gp;


// Tunnel stats (Net Channel) Group
typedef struct
{
  Long_data send_throughput;     // Send Throughput (Kbps)
  Long_data recv_throughput;     // Receive Throughput (Kbps)
  Long_data send_pps;            // Send Packets/Sec
  Long_data recv_pps;            // Receive Packets/Sec
  Long_data send_pps_drop;       // Send Packets Drop/Sec
  Long_data recv_pps_drop;       // Receive Packets Drop/Sec
  Long_data send_latency;        // Send Latency
  //unsigned int recv_latency;     // Receive Latency
} Tunnel_stats_gp;


//XMPP Stats gp 
typedef struct
{
  Long_data login_attempted;                       //Rate graph for login attempted
  Long_data login_attempted_count;                 //Sum graph for login attempted
  Long_data login_succ;                            //Rate graph for login success
  Long_data login_succ_count;                      //Sum graph for login success
  Long_data login_failed;                          //Rate graph for login failed
  Long_data login_failed_count;                    //Sum graph for login failed
  Long_data msg_sent;                              //Rate graph for msg sent
  Long_data msg_sent_count;                        //Sum graph for msg sent count
  Long_data msg_send_failed;                       //Rate graph for msg send failed
  Long_data msg_send_failed_count;                 //Sum graph for msg send failed count
  Long_data msg_rcvd;                              //Rate graph for msg receive 
  Long_data msg_rcvd_count;                        //Sum graph for msg receive count
  Long_data msg_dlvrd;                             //Rate graph for msg delivered 
  Long_data msg_dlvrd_count;                       //Sum graph for msg delivered count
  Long_data send_throughput;                       //Send Throughput 
  Long_data rcvd_throughput;                       //Send Throughput 
} XMPP_gp;
#pragma pack(pop)

#define GDF_COPY_SCALAR_DATA(grp_idx, graph_idx, src, dest) \
  {                                                         \
    dest = (Long_data) src;                                 \
    update_gdf_data(grp_idx, graph_idx, 0, 0, dest);         \
  }

#define GDF_COPY_VECTOR_DATA(grp_idx, graph_idx, grp_vec_id, grph_vec_id, src, dest) \
  {                                                                     \
    dest = (Long_data) src;                                             \
    update_gdf_data(grp_idx, graph_idx, grp_vec_id, grph_vec_id, dest);  \
  }

#define GDF_COPY_VECTOR_DATA_THROUGHPUT_KBPS(grp_idx, graph_idx, grp_vec_id, grph_vec_id, src, dest) \
{                                                                                               \
  double throughput_kbps  = (src * 8) / ((double)global_settings->progress_secs/1000);          \
  dest = (Long_data) throughput_kbps;                                                           \
  update_gdf_data(grp_idx, graph_idx, grp_vec_id, grph_vec_id, dest);                           \
}

#define GDF_COPY_TIMES_SCALAR_DATA(grp_idx, graph_idx, src_avg, src_min, src_max, src_succ, dest_avg, dest_min, dest_max, dest_succ) \
  {                                                                     \
    dest_avg = (Long_data) src_avg;                                     \
    dest_min = (Long_data) src_min;                                     \
    dest_max = (Long_data) src_max;                                     \
    dest_succ = (Long_data) src_succ;                                   \
    update_gdf_data_times(grp_idx, graph_idx, 0, 0, dest_avg, dest_min, dest_max, dest_succ); \
  }

#define GDF_COPY_TIMES_VECTOR_DATA(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, src_avg, src_min, src_max, src_succ, dest_avg, dest_min, dest_max, dest_succ) \
  {                                                                     \
    dest_avg = (Long_data) src_avg;                                     \
    dest_min = (Long_data) src_min;                                     \
    dest_max = (Long_data) src_max;                                     \
    dest_succ = (Long_data) src_succ;                                   \
    update_gdf_data_times(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, dest_avg, dest_min, dest_max, dest_succ); \
  }

#define GDF_COPY_TIMES_STD_VECTOR_DATA(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, src_avg, src_min, src_max, src_succ, src_sum_of_sqr, dest_avg, dest_min, dest_max, dest_succ, dest_sum_of_sqr) \
  {                                                                     \
    dest_avg = (Long_data) src_avg;                                     \
    dest_min = (Long_data) src_min;                                     \
    dest_max = (Long_data) src_max;                                     \
    dest_succ = (Long_data) src_succ;                                   \
    dest_sum_of_sqr = (Long_data) src_sum_of_sqr;                       \
    update_gdf_data_times_std(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, \
                              dest_avg, dest_min, dest_max, dest_succ, dest_sum_of_sqr); \
  }

// Convert time in milli-secs to seconds
#define NS_MS_TO_SEC(time_ms) (double)(((double)(time_ms))/1000.0)
// Convert time in milli-secs square to seconds square
#define NS_MS_SQR_TO_SEC_SQR(time_ms) (double)(((double)(time_ms))/1000000.0)

// Convert number of hits in a sample to rate per sec
#define NS_NUM_TO_RATE_PS(num) (double)(((double)(num * 1000.0))/(double )global_settings->progress_secs)

/*bug 85621*/
#define GDF_COPY_SAMPLE_2B_100_COUNT_4B_DATA(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, src_avg, src_succ, dest_avg, dest_succ) \
 {                                                                     \
   dest_avg = (Short_data) src_avg;                                     \
   dest_succ = (Int_data) src_succ;                                   \
   update_gdf_data_times(grp_idx, graph_idx, grp_vec_idx, gph_vec_idx, dest_avg, 0, 0, dest_succ); \
 }

/*bug 103688*/
#define NS_COPY_SAMPLE_2B_100_COUNT_4B_DATA(pct_double, count_long, gp_idx, g_idx, gv_idx, gph_vec_idx, gp_ptr_sample, gp_ptr_count) \
{ \
    short pct = pct_double * 100; \
    int count = count_long; \
    NSDL1_GDF(NULL, NULL, "pct=%ld count =%d", pct, count); \
    GDF_COPY_SAMPLE_2B_100_COUNT_4B_DATA(gp_idx, g_idx, gv_idx, gph_vec_idx, pct, count, gp_ptr_sample, gp_ptr_count); \
}
extern ns_ptr_t hml_start_rtg_idx;
extern Vuser_gp             *vuser_gp_ptr;
extern unsigned int          vuser_group_idx;
extern SSL_gp               *ssl_gp_ptr;
extern unsigned int          ssl_gp_idx;
extern Url_hits_gp          *url_hits_gp_ptr;
extern unsigned int          fc2_gp_idx;
extern fc2_gp               *fc2_gp_ptr;
extern unsigned int          url_hits_gp_idx;
extern Page_download_gp     *page_download_gp_ptr;
extern unsigned int          page_download_gp_idx;
extern Session_gp           *session_gp_ptr;
extern unsigned int          session_gp_idx;
extern Trans_overall_gp     *trans_overall_gp_ptr;
extern unsigned int          trans_overall_gp_idx;
extern URL_fail_gp          *url_fail_gp_ptr;
extern unsigned int          url_fail_gp_idx;
extern Page_fail_gp         *page_fail_gp_ptr;
extern unsigned int          page_fail_gp_idx;
extern Session_fail_gp      *session_fail_gp_ptr;
extern unsigned int          session_fail_gp_idx;
extern Trans_fail_gp        *trans_fail_gp_ptr;
extern unsigned int          trans_fail_gp_idx;
extern NO_system_stats_gp   *no_system_stats_gp_ptr;
extern unsigned int          no_system_stats_gp_idx;
extern NO_network_stats_gp  *no_network_stats_gp_ptr;
extern unsigned int          no_network_stats_gp_idx;

extern Server_stats_gp      *server_stats_gp_ptr;
extern unsigned int          server_stats_gp_idx;

extern Trans_stats_gp        *trans_stats_gp_ptr;
extern unsigned int          trans_stats_gp_idx;
extern Trans_cum_stats_gp    *trans_cum_stats_gp_ptr;
extern unsigned int          trans_cum_stats_gp_idx;

extern Tunnel_stats_gp      *tunnel_stats_gp_ptr;
extern unsigned int          tunnel_stats_gp_idx;

/* SMTP */
extern SMTP_Net_throughput_gp* smtp_net_throughput_gp_ptr;
extern unsigned int smtp_net_throughput_gp_idx;
extern SMTP_hits_gp* smtp_hits_gp_ptr;
extern unsigned int smtp_hits_gp_idx;
extern SMTP_fail_gp* smtp_fail_gp_ptr;
extern unsigned int smtp_fail_gp_idx;

/* POP3 */
extern POP3_Net_throughput_gp* pop3_net_throughput_gp_ptr;
extern unsigned int pop3_net_throughput_gp_idx;
extern POP3_hits_gp* pop3_hits_gp_ptr;
extern unsigned int pop3_hits_gp_idx;
extern POP3_fail_gp* pop3_fail_gp_ptr;
extern unsigned int pop3_fail_gp_idx;

/* FTP */
extern FTP_Net_throughput_gp* ftp_net_throughput_gp_ptr;
extern unsigned int ftp_net_throughput_gp_idx;
extern FTP_hits_gp* ftp_hits_gp_ptr;
extern unsigned int ftp_hits_gp_idx;
extern FTP_fail_gp* ftp_fail_gp_ptr;
extern unsigned int ftp_fail_gp_idx;

/* LDAP */
extern LDAP_Net_throughput_gp* ldap_net_throughput_gp_ptr;
extern unsigned int ldap_net_throughput_gp_idx;
extern LDAP_hits_gp* ldap_hits_gp_ptr;
extern unsigned int ldap_hits_gp_idx;
extern LDAP_fail_gp* ldap_fail_gp_ptr;
extern unsigned int ldap_fail_gp_idx;

/* IMAP */
extern IMAP_Net_throughput_gp* imap_net_throughput_gp_ptr;
extern unsigned int imap_net_throughput_gp_idx;
extern IMAP_hits_gp* imap_hits_gp_ptr;
extern unsigned int imap_hits_gp_idx;
extern IMAP_fail_gp* imap_fail_gp_ptr;
extern unsigned int imap_fail_gp_idx;

/* JRMI */
extern JRMI_Net_throughput_gp* jrmi_net_throughput_gp_ptr;
extern unsigned int jrmi_net_throughput_gp_idx;
extern JRMI_hits_gp* jrmi_hits_gp_ptr;
extern unsigned int jrmi_hits_gp_idx;
extern JRMI_fail_gp* jrmi_fail_gp_ptr;
extern unsigned int jrmi_fail_gp_idx;

/* DNS */
extern DNS_Net_throughput_gp* dns_net_throughput_gp_ptr;
extern unsigned int dns_net_throughput_gp_idx;
extern DNS_hits_gp* dns_hits_gp_ptr;
extern unsigned int dns_hits_gp_idx;
extern DNS_fail_gp* dns_fail_gp_ptr;
extern unsigned int dns_fail_gp_idx;


extern XMPP_gp *xmpp_stat_gp_ptr;
extern unsigned int xmpp_stat_gp_idx;

extern FILE *write_gdf_fp;
extern char *msg_data_ptr;    // to point the Msg Data Buffer, the start point of the buffer
extern ns_ptr_t msg_data_size;     // to store the size of Msg Data Buffer
extern ns_ptr_t nc_msg_data_size; //NetCloud: added to save rtgMessage.dat size without adding monitor data
extern ns_ptr_t tmp_msg_data_size;

extern void process_all_gdf(int runtime_flag);
extern void fill_msg_start_hdr ();
extern void fill_msg_data_hdr (avgtime *avg);
extern void fill_msg_end_hdr (avgtime *avg);

extern double convert_sec (double row_data);
extern double convert_ps (double row_data);
extern double convert_pm (double row_data);
extern double convert_kbps (double row_data);
extern double convert_dbh(double row_data);

extern double convert_sec_long_long (char *row_data);
extern double convert_ps_long_long (char *row_data);
extern double convert_pm_long_long (char *row_data);
extern double convert_kbps_long_long (char *row_data);
extern double convert_8B_bytes_Kbps(u_ns_8B_t row_data); 
extern double convert_dbh_long_long(char *row_data);
extern double convert_long_data_to_ps_long_long (unsigned int row_data);

extern void log_cm_gp_data_ptr(int custom_mon_id, int gp_info_index, int num_group, int print_mode, char* server_IP);
extern void update_gdf_data(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, Long_long_data cur_val);
extern void update_gdf_data_times(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, Long_long_data cur_val, Long_long_data cur_min, Long_long_data cur_max, Long_long_data cur_count);
extern void update_gdf_data_times_std(int gdf_group_idx, int gdf_graph_num, int group_vec_idx, int graph_vec_idx, Long_long_data cur_val, Long_long_data cur_min, Long_long_data cur_max, Long_long_data cur_count, Long_long_data cur_sum_of_sqr);

extern unsigned int get_gdf_group_info_idx(int rpt_grp_id); // Not used right now
extern int get_gdf_group_graph_info_idx(int rpt_grp_id, int rpt_id, int *group_info_idx, int *graph_num);
extern int get_gdf_vector_idx(int group_info_idx, int graph_num, char *vector_name, int *gdf_group_vector_idx, int *gdf_graph_vector_idx);
extern int get_gdf_vector_num(int group_info_idx, int graph_num, int *gdf_group_vectors_num, int *gdf_graph_vectors_num);
extern double get_gdf_data_c_avg(int group_info_idx, int graph_num, int gdf_group_vector_idx, int gdf_graph_vector_idx, int vector_opt);
extern void free_gdf(void);
extern void free_gdf_data(void);

extern void init_gdf_all_data(void);
extern void log_gdf_all_data(void);
extern char *get_gdf_group_name(int group_info_idx);
extern char *get_gdf_graph_name(int group_info_idx, int graph_num);
extern char get_gdf_graph_formula(int group_info_idx, int graph_num);
extern char *get_gdf_group_vector_name(int group_info_idx, int group_vector_idx);
extern char *get_gdf_graph_vector_name(int group_info_idx, int graph_num, int graph_vector_idx);
extern void create_gdf_summary_data(void);

extern int get_rpt_grp_id_by_gp_info_index(int group_info_idx);
extern double convert_long_long_data_to_ps_long_long (unsigned long long row_data);
extern char version[6];
extern int group_count;
extern inline void fill_msg_data_seq (avgtime *avg, int testidx);

extern void free_gdf_tables();
extern char g_runtime_flag;

extern int append_testrun_all_gdf_info();
#define GDF_MAX_FIELDS 25             // Max number of fields possible in one record (it is much less than 25)
extern inline char data_type_to_num(char *d_type);
extern inline int getSizeOfGraph(char type, int *num_element);
extern int element_count;
extern inline FILE *open_gdf(char* fname);

extern void check_dup_grp_id_in_CM_and_DVM();
extern void check_rtg_size();
extern void process_gdf_wrapper(int runtime_flag);
extern void close_gdf(FILE* fp);
 
extern Graph_Info *graph_data_ptr;
extern int total_graphs_entries;
extern int ns_graph_end_idx;
extern int ns_graph_count;

extern Group_Info *group_data_ptr;
extern void getNCPrefix(char *prefix, int sgrp_gen_idx, int grp_idx, char*ang, int if_show_group_data_kw_enabled);
extern void fill_2d(char **TwoD, int i, char *fill_data);
extern void process_custom_gdf(int runtime_flag);
extern char *testrungdf_buffer;
extern long fsize; 
extern int total_groups_entries;
extern int ns_gp_end_idx;
extern int ns_gp_count;
extern ns_ptr_t ns_gp_msg_data_size;
extern int g_gdf_processing_flag;
extern int test_run_gdf_count;
extern int max_rtg_index_tbl_row_entries;
extern u_ns_ts_t g_testrun_rtg_timestamp;
extern int check_if_realloc_needed_for_dyn_obj(int dyn_obj_idx);
extern void set_rtg_index_for_dyn_objs_discovered();
extern void set_rtg_index_in_cm_info(int id, void *info_ptr, ns_ptr_t rtg_pkg_size, char runtime_flag);
extern int get_actual_no_of_graphs(char *gdf_name, char *groupName, int *num_actual_data, void *cm_info, int num_hml_graphs[]);
extern int set_group_idx_for_txn;
extern Graph_Data* get_graph_data_ptr(int gdf_group_vector_idx, int gdf_graph_vector_idx, Graph_Info *local_graph_data_ptr);
extern void get_no_of_elements_of_gdf(char *gdf_name, int *num_data, int *rtg_group_size, int *pdf_id, int *graphId, char *graph_desc);
extern char* fill_data_by_graph_type(int gp_info_index, int c_graph, int c_group_numvec, int c_graph_numvec, char type, char *block_ptr, Long_long_data *data, int *indx, char *doverflow);
extern char* print_data_by_graph_type(FILE *fp1, FILE *fp2, Graph_Info *grp_data_ptr, char *block_ptr, int print_mode, Graph_Data *gdata, char *vector_name);
extern void set_rtg_index_for_dyn_objs_discovered();
extern int check_if_cm_vector(const void *cm1, const void *cm2);
extern void write_rtc_details_for_dyn_objs();
extern void free_group_graph_data_ptr();
extern void free_group_graph_data_ptr();
extern void write_rtc_details_for_dyn_objs();
extern void fill_pre_post_test_msg_hdr(int opcode);
extern void send_pre_post_test_msg_to_tomcat();
extern void add_gdf_in_hash_table(char *gdf_name, int row_num);
#endif
// End of file
