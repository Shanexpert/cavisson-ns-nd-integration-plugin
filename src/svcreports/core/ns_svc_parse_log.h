#ifndef NS_SVC_PARSE_LOG_H
#define NS_SVC_PARSE_LOG_H

#define MAX_SVC_NAME_LEN 128
#define MAX_STATUS_STRLEN 1024

#define SVC_TYPE_MAIN 0
#define SVC_TYPE_COMPONENT 1

#define INIT_NUM_PARALLEL_SESSIONS_PER_THREAD 256
#define DELTA_NUM_PARALLEL_SESSIONS_PER_THREAD 256
#define INIT_NUM_COMPONENT_TRANSACTIONS_PER_SESSION 256
#define DELTA_NUM_COMPONENT_TRANSACTIONS_PER_SESSION 256

#define LOGFILE_ROLLOVER_SIZE_IN_MB 100

#define INIT_NUM_SESS_MAP_TABLE_ENTRIES 1024
#define DELTA_NUM_SESS_MAP_TABLE_ENTRIES 512

#define SVC_RECORD_MAX_BUFFER_SIZE 16*1024

#define SVC_RECORD_HEADER "InstanceId,SvcInstance,SvcIndex,SvcSessionIndex,SvcSignatureIndex,SvcCPSignatureIndex,SvcStatusIndex,SvcStartTime,SvcRespTime,SvcSORRespTime,SvcQueueWaitTime,SvcAppTime,SvcTotalTime,SvcElapsedTime"
#define SVC_COMP_RECORD_HEADER "InstanceId,SvcParentInstance,SvcParentIndex,SvcInstance,SvcIndex,SvcSessionIndex,SvcSignatureIndex,SvcStatusIndex,SvcStartTime,SvcParentStartTime,SvcRespTime,SvcSORRespTime,SvcTotalTime,SvcQueueWaitTime,SvcElapsedTime,SvcCritPathFlag"
#define HEADER "SvcParentInstance,SvcParentIndex,SvcInstance,SvcIndex,SvcType,SvcSessionIndex,SvcSignatureIndex,SvcStatusIndex,SvcStartTime,SvcParentStartTime,SvcRespTime,SvcSORRespTime,SvcTotalTime,SvcQueueWaitTime,SvcElapsedTime,SvcCritPathFlag"
#define SVC_SIGNATURE_HEADER "SvcSignatureIndex,SvcSignatureName" 
#define SVC_ERRCODE_HEADER "SvcStatusIndex,SvcStatus"
#define SVC_TABLE_HEADER "SvcIndex,SvcName"
#define MONITOR_CSV_HEADER "Date,Time,TotalRequest,CumlativeRequest,ReqPsec,AvgResTime,AvgAppTime,AvgSorTime,AvgQWTime,SucessPSec,FailPsec"
#define SVC_INSTANCE_HEADER "InstanceID|InstanceName"

typedef struct svc_instance_t
{
  int svcparentinstance;
  int svcparentindex;
  int svcinstance;
  int svcindex;
  char svcname[MAX_SVC_NAME_LEN];
  char svctype;
  char svcsessionindex[64];
  int svcnamelen;
  int svcsignatureindex;
  int svccpsignatureindex;
  int svcstatusindex;
  long long int svcstarttime;
  long long int svcendtime;
  long long int svcparentstarttime;
  long long int svcresptime;
  long long int svcsorresptime;
  long long int svctotaltime;
  long long int svcqueuewaittime;
  long long int svcapptime;
  long long int svcelapsedtime;
  char svccritpathflag;
} svc_instance_t;

typedef struct {
  char sessionId[64];
  int parentInstance;
  int parentIndex; /* svcIndex of the parent */
  long long int parentStartTime;
  int num_components; /* Including the parent svc */
  int num_allocated_components;
  svc_instance_t *component_data;
} sess_map_table_t;

typedef struct component_table{
  int cumRequest; /* Cumulative count since beginning of TR */
  int numRequest; /*Count in current sample*/
  int cumDBRespTime; /* This is SOR time, sum of all the component instances in current sample*/
  int cumQWTime;
  int cumAppTime; /* Self time */
  int numSuccessReq; /* Current sample*/
  int numFailReq; /* Current sample*/
  char *name;
  int namelen;
  char *disp_name;
} component_table;

typedef struct service_table{
  int cumRequest;
  int numRequest;
  int cumDBRespTime;
  int cumQWTime;
  int cumAppTime;
  int numSuccessReq;
  int numFailReq;
  component_table  *comp_table; /* Address of component array belonging to this service */
  int numComp;
  char *name;
  int namelen;
  char *disp_name;
} service_table;

/* svc_info_t structure is used for array, each element contains
 * information for each thread
 */
typedef struct {

  char first_line_flag;
  long long int time_offset;
  char server[1024];
  char file[1024];
  char instance[1024];
  int cmon_fd;
  pthread_t threadid;
  char first;
  char *partial_buffer;
  int partial_buffer_len;

  PGconn *svc_db_con;
  FILE *raw_file_fp;
  char raw_filename[512];
  char *svcRecordBuf;
  char *svcCompRecordBuf;
  int svcRecordIndex; 
  int svcCompRecordIndex; 
  int threadIndex;
  FILE *svc_record_fp;
  FILE *svc_comp_record_fp;
  FILE *svc_table_fp;
  FILE *svc_signature_table_fp;
  FILE *svc_error_code_fp;

  //sess_map_table_t *sess_map_table;
  char first_time;
  int num_svc_sessions;
  int allocated_num_svc_sessions;

  FILE *logfp;
  char *logfilename;

  void *bufpool_key;

  service_table *svc_table;

  void *ss_mon_info;
} svc_info_t; 

typedef struct {
  int count;
  char **value;
}keyword_value_t;

#define TOTAL_SVC_KEYWORD 9

typedef struct service_stats_info {
  char agg_graph;
  char comp_graph;
  char instance_graph;
  char ddr;
  char debug;
  char dbmode;
  char create_raw_data;
  char vector_flag;
  char field_seperator;

  int total_svc_info;
  int svc_mon_data_interval;
  char conf_file[4 * 1024];

  int svc_data_mon_fd;
  char *svc_ns_wdir;
  int svc_testidx;
  int svc_ns_time_stamp;

  svc_info_t *svc_info;

  pthread_t svc_control_thread;
  pthread_mutex_t svc_mon_data_mutex;

  //For Svc Table
  NormObjKey key_svc_name;
  NormObjKey key_svc_error_code;
  NormObjKey key_svc_signature;

  keyword_value_t svc_keywords[TOTAL_SVC_KEYWORD];
  int component_session_id_field;

  pthread_mutex_t instance_count_mutex;
  char ownername[512];
  char grpname[512];
  int total_service;

  FILE *logfp;
  char *logfilename;
  FILE *svc_instance_fp;
}service_stats_info;

extern int process_svc_data(char *buf, int len, svc_info_t *svc_info); 
extern inline void svc_init(service_stats_info *ss_mon_info, char *t_ns_wdir, int t_testidx);
extern inline void svc_instance_init(svc_info_t *svc_info);
extern inline void svc_close(service_stats_info *ss_mon_info);
extern void svc_send_vector_to_ns(service_stats_info *ss_mon_info);
extern void free_service_table(service_stats_info *ss_mon_info);
extern void svc_send_vector_data_to_ns(service_stats_info *ss_mon_info);
#endif /* NS_SVC_PARSE_LOG_H */
