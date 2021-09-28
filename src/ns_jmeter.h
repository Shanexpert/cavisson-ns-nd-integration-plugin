#ifndef __ns_jmeter_h
#define __ns_jmeter_h

/****

https://jmeter.apache.org/api/org/apache/jmeter/samplers/SampleResult.html

int	getAllThreads() 
AssertionResult[]	getAssertionResults() - Gets the assertion results associated with this sample.
long	getBodySizeAsLong() 
long	getBytesAsLong() - return the bytes returned by the response.
long	getConnectTime() 
String	getContentType() 
String	getDataEncodingNoDefault() - Returns the dataEncoding.
String	getDataEncodingWithDefault() - Returns the dataEncoding or the default if no dataEncoding was provided.
protected String	getDataEncodingWithDefault(String defaultEncoding) - Returns the dataEncoding or the default if no dataEncoding was provided.
String	getDataType() - Returns the data type of the sample.
long	getEndTime() 
int	getErrorCount() - Returns the count of errors.
String	getFirstAssertionFailureMessage() 
int	getGroupThreads() 
int	getHeadersSize() - Get the headers size in bytes
long	getIdleTime() 
long	getLatency() 
String	getMediaType() - Get the media type from the Content Type
SampleResult	getParent() 
String	getRequestHeaders() 
String	getResponseCode() 
byte[]	getResponseData() - Gets the responseData attribute of the SampleResult object.
String	getResponseDataAsString() - Gets the responseData of the SampleResult object as a String
String	getResponseHeaders() 
String	getResponseMessage() 
String	getResultFileName() 
int	getSampleCount() - return the sample count.
String	getSampleLabel() 
String	getSampleLabel(boolean includeGroup) - Get the sample label for use in summary reports etc.
String	getSamplerData() - SampleSaveConfiguration	getSaveConfig() 
List<String>	getSearchableTokens() - Get a list of all tokens that should be visible to searching
long	getSentBytes() 
long	getStartTime() 
SampleResult[]	getSubResults() - Gets the subresults associated with this sample.
JMeterContext.TestLogicalAction	getTestLogicalAction() 
String	getThreadName() 
long	getTime() - Get the time it took this sample to occur.
long	getTimeStamp() - Get the sample timestamp, which may be either the start time or the end time.
URL	getURL() 
String	getUrlAsString() - Get a String representation of the URL (if defined).

static boolean	isBinaryType(String ct) 
boolean	isIgnore() 
static boolean	isRenameSampleLabel() - see https://bz.apache.org/bugzilla/show_bug.cgi?id=63055
boolean	isResponseCodeOK() 
boolean	isStampedAtStart() 
boolean	isStopTest() 
boolean	isStopTestNow() 
boolean	isStopThread() 
boolean	isSuccessful() 

***/

#include "ns_alloc.h"
#include <sys/epoll.h>
#include "ns_msg_def.h"
#include "ns_msg_com_util.h"

#define JMETER_MAX_KEYWORD_LEN                              4096
#define JMETER_MAX_ARGS_LEN                                 512 
#define JMETER_SETTINGS_MAX_OPT_ARGS_LEN                    1024
#define JMETER_MAX_USAGE_LEN                                4096 
#define JMETER_JTL_TOKEN                                    100
#define JMETER_CSV_DATA_SET_SPLIT_NO_FILES                  0
#define JMETER_CSV_DATA_SET_SPLIT_ALL_FILES                 1
#define JMETER_CSV_DATA_SET_SPLIT_FILE_WITH_PATTERN         2

#define JMETER_TYPE_SCRIPT                                  100
#define JMETER_CSV_DATA_SET_SPLIT_MAX_PATTERN_LEN           256

#define JMETER_TX_ID_NOT_SET                               -1
#define JMETER_TX_SUCCESS                                   0

// JMeter Flags bit definition
#define JMETER_FLAGS_NEW_OBJ                             0x01  // Not used
#define JMETER_FLAGS_PAGE                                0x02  // URL record is also a page
#define	JMETER_FLAGS_MAIN_REDURL                         0x04  // URL record is for Main URL which is redirected
#define	JMETER_FLAGS_MAIN_URL                            0x08  // URL record is for Main URL which is final response
#define	JMETER_FLAGS_INLINE_URL                          0x10  // URL record is for Inline URL
#define JMETER_FLAGS_ALL_URL                             0x1c  // URL record for Main, Inline and Redirected 
typedef enum
{
  JMETER_OPCODE_URL_REC,
  JMETER_OPCODE_TX_REC,  
  JMETER_OPCODE_PAGE_DUMP_REC,
  JMETER_OPCODE_SESS_REC,
  JMETER_HEART_BEAT,
  JMETER_OPCODE_NEW_OBJECT,
  __JMETER_OPCODE_LAST__
} JMeterOpcodes_t;

typedef struct
{
  int is_vusers_split;
  int csv_file_split_mode;
  char csv_file_split_pattern[JMETER_CSV_DATA_SET_SPLIT_MAX_PATTERN_LEN];
}jmeter_vusers_csv_settings;

typedef struct
{
  int threadnum;
  int duration;
  int ramp_up_time;
}jmeter_schedule_settings;

typedef struct 
{
  float version;
  int min_heap_size;
  int max_heap_size;
  unsigned short port;
  ns_bigbuf_t jmeter_add_args;// For Saving index in big buffer
  ns_bigbuf_t jmeter_java_add_args; // For Saving index in big buffer
  short gen_jmeter_report;
}jmeter_group_settings;

// sz = 24
typedef struct
{
  uint32_t msg_size;      // Size of the message on the wire excluding 4 bytes of msg_size.
  uint16_t opcode;        // Determines the C struct for the rest of the message
  uint8_t  version;       // Version for message format - 1 for version 1. For fuure use
  uint8_t  flags;         // For future use
  long     sample_num;    // Sample number is a global counter, It will be come with all records.
  int      user_index;    // Contains thread Id(1 byte) + user id(3 bytes)
  char     reserved[4];   // for future use
} JMeterMsgHdr;

// obj_type
#define JMETER_OBJECT_PAGE         1
#define JMETER_OBJECT_TRANSACTION  2

// New Object
typedef struct
{
  int      obj_id;        // Object Id
  char     obj_type;      // Object Type
  char     pad;
  uint16_t obj_name_len;  // Object name length
  char     obj_name[1];   // Object name with null termination
} JMeterNewObjRec;

// URL Record from JMeter listener to CVM
typedef struct 
{
  long time_stamp;      // Timestamp when sample was collected
  long start_time;      // start timestamp of the request (this may be either the start time or the end time.)
  long end_time;
  int page_id;
  int transaction_id;   // This must be set to current tx id
  int session_inst;
  int running_vusers;
  int  duration;        // long	getTime() - Get the time it took this sample to occur.diff of end_time and start_time. Should be sum of all component times
  // Component timing
  int  dns_time;        //NA
  int  connect_time;    //On each connect
  int  ssl_time;        //NA
  int  first_byte_time; // Is it idleTime()?
  int  download_time;   //NA

  char status;          //PASS 0, FAILED 1 
  char padding[3];
  // server timing
  int32_t nw_cache_state; // Hit, miss, revalidate, nw_cache_stats_update_counter()
  int  edge_time;       // Time taken by edge server. It include origin time (need to check)
  int  origin_time;
  
  // Size
  int  req_hdr_size;    //NA
  int  req_body_size;   //HTTP Body Total Send Bytes
  int  req_total_size;  //Overall, MUST be req_hdr_size +  req_body_size , need to check..
  
  int  rep_hdr_size;
  int  rep_body_size;   //HTTP Body Total Receive Bytes
  int  rep_total_size;  //Overall, MUST be req_hdr_size +  req_body_size , need to check..
  
  int  error_count;
  int  status_code;
  short page_instance;    
  short tx_inst;       // This must be set to current tx instance. 0 if not part of a transaction
  short url_len;           //Length of the URL without NULL termination
  short sample_label_len;  //This is the length of label used in JMeter Script. Label will come after the url.
  char url[1];             //url will be of url_len + NULL termination
  char label[1];           //Label will be url label len + NULL termination
 
} JMeterPageUrlRec;

//JMeter Vuser
typedef struct
{
  int running_vusers;       //Running vuser count
  int active_vusers;        //Active vuser count
} JMeterVuser;

typedef struct
{
  long req_tot_bytes;     // If parent sample is ON, ...  
  long rep_tot_bytes;     // 
  long start_time;
  long end_time;
  int transaction_id;
  int session_instance; 
  int running_vusers;
  int duration;          // transaction time 
  int total_edge_time;   // this is sum of all http sample's origin edge time 
  int total_origin_time; // this is sum of all http sample's origin time
  int32_t nw_cache_state;
  int  error_count;       // 
  int8_t status;          // pass 0 or fail 1 , -1 not available 
  int8_t error_code;
  int8_t last_http_status; //  - this is last http sample status if available, -1 not available
  uint8_t pad1;
  short tx_instance;      
  short num_sub_sample;    // NA - this is count of sub sample
  short tx_name_len;       // Tx name length excluding null termination
  short num_pages;     // Number of pages part of transaction. It can be 0 if Generate Parent sample is OFF
  short page_instance[1];
  char tx_name[1];          // Variable len txn name with NULL termination  
} JMeterTxRec;
  
typedef struct
{
  long  start_time; 
  long  end_time; 
  long  partition;
  int   sess_instance; 
  int   page_index;                // 0 for now
  int   page_instance;             // 0 for now
  int   page_status;               // 0 - success, 1, 2, 3, .. (as per NS Error codes)
  int   page_response_time; 
  
  short page_name_len;
  short assertion_results_len;
  short tx_name_len;
  short fetched_param_len;        // 0 for now, send NULL (1 byte)
  short flow_name_len;            // 0 for now, send NULL (1 byte)
  short log_file_sfx_len;
  short res_body_orig_name_len;
  short parameter_len;            // 0 for now, send NULL (1 byte)
  
  char  page_name[1];             // page name followed by Null. 
  char  assertion_results[1];     // getAssertionResults()
  char  tx_name[1]; 
  char  fetched_param[1];
  char  flow_name[1]; 
  char  log_file_sfx[1];
  char  res_body_orig_name[1]; 
  char  parameter[1]; 
} JmeterMsgPageDumpRec;

typedef struct
{
  long  start_time;
  long  end_time;
  int  sess_inst;
  int8_t sess_status;
}JmeterMsgSessRec;

//Tx normalized structure
typedef struct{
  int total_entries;  // Total number of entries used in the tx_map
  int max_entries;  // max entries allocated in the tx_map
  int *tx_map;  // pointer to array of integers
} JmeterTxIdMap;

extern int g_total_jmeter_entries;
extern int g_jmeter_ports[];
extern time_t g_ts_init_time_in_sec;

extern int g_jmeter_avgtime_idx;
extern void ns_copy_jmeter_scripts_to_generator();
extern int create_jmeter_listen_socket(int epfd, Msg_com_con *mccptr, int con_type, int grp_num);
extern int collect_data_from_jmeter_listener(int accept_fd, struct epoll_event *pfds, Msg_com_con *mccptr);
extern void add_socket_fd(int epfd, char* data_ptr, int fd, int event);
extern int kw_set_jmeter_vusers_split(char *buf, char *err_msg);
extern int kw_set_jmeter_csv_files_split_mode(char *buf, char *err_msg);
extern void jmeter_init();
extern int jmeter_get_running_vusers_by_sgrp(int sgrp_id);
extern int jmeter_get_active_vusers_by_sgrp(int sgrp_id);
extern int jmeter_get_active_vusers();
extern int jmeter_get_running_vusers();
#endif
