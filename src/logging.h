#ifndef LOGGING_H
#define LOGGING_H

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "ns_data_types.h"

#define BLOCK_SIZE 131072
//#define NUM_BUFFERS 16

#define DELIMINATOR ','
#define LOGGING_SIZE 8192
#define NUM_INITIAL_BUFFERS 10

#define TX_PG_RECORD            1
#define SESSION_RECORD		2
#define TX_RECORD		3
#define PAGE_RECORD		4
#define URL_RECORD		5
#define DATA_RECORD		100 //In release 3.9.6, for page dump redesign we are using macro value of DATA_RECORD as this code is obsolete. Hence making its value 11, because in c files this macro is been used(code cleanup require)   
#define PAGE_DUMP_RECORD	6
#define DATA_LAST_RECORD	7
#define MSG_RECORD		8
#define MSG_LAST_RECORD		9
#define RBU_PAGE_DETAIL_RECORD  10
#define TX_PG_RECORD_V2         11
#define PAGE_RECORD_V2	        12
#define TX_RECORD_V2		13
#define RBU_LIGHTHOUSE_RECORD   14
#define RBU_MARK_MEASURE_RECORD 15
/*This is total type of records*/
#define TOTAL_RECORDS           14

/* Since we not using shared memory addresses in dlog, INDEX_SIZE is now same on both machines
 * (Now index is the id (not address)
 */

#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
  #define INDEX_SIZE			4
//  #define UNSIGNED_LONG_SIZE		8
#else
  #define INDEX_SIZE			4
// #define UNSIGNED_LONG_SIZE		4
#endif

#define UNSIGNED_CHAR_SIZE  1
#define SHORT_SIZE          2
#define UNSIGNED_INT_SIZE   4
#define UNSIGNED_LONG_SIZE  8

#define TEST_START_SIZE 		9 /*Not Used*/

#define SESSION_RECORD_SIZE	(UNSIGNED_CHAR_SIZE + /*record_num*/	\
				INDEX_SIZE         + /*Session Idx*/	\
				UNSIGNED_INT_SIZE  + /*Session Inst*/	\
				UNSIGNED_INT_SIZE  + /*User Idx*/	\
				UNSIGNED_INT_SIZE  + /*Group Num*/	\
				SHORT_SIZE         + /*Child Id*/	\
				UNSIGNED_CHAR_SIZE + /*Is run phase*/	\
				INDEX_SIZE         + /*Access*/		\
				INDEX_SIZE         + /*Location*/	\
				INDEX_SIZE         + /*Browser*/	\
				INDEX_SIZE         + /*Freq*/		\
				INDEX_SIZE         + /*Machine Attr*/	\
				NS_TIME_DATA_SIZE  + /*Started at*/	\
				NS_TIME_DATA_SIZE  + /*Started at*/	\
				NS_TIME_DATA_SIZE  + /*Think Dur*/	\
				UNSIGNED_CHAR_SIZE +  /*Sess Status*/   \
                                SHORT_SIZE            /*Phase id*/)


#define PAGE_DUMP_RECORD_SIZE   (UNSIGNED_CHAR_SIZE + /*Record_num*/     \
                                NS_TIME_DATA_SIZE   + /*Start time*/     \
                                NS_TIME_DATA_SIZE   + /*End time*/       \
                                UNSIGNED_INT_SIZE   + /*Generator Id*/   \
                                SHORT_SIZE          + /*NVM Id*/         \
                                UNSIGNED_INT_SIZE   + /*User Id*/        \
                                UNSIGNED_INT_SIZE   + /*Session Inst*/   \
                                UNSIGNED_INT_SIZE   + /*Page Id*/        \
                                SHORT_SIZE          + /*Page Inst*/      \
                                SHORT_SIZE          + /*Page Status*/    \
                                UNSIGNED_INT_SIZE   + /*Page Response time*/\
                                UNSIGNED_INT_SIZE   + /*Group Id*/       \
                                UNSIGNED_INT_SIZE   + /*Session Id*/     \
                                UNSIGNED_LONG_SIZE  + /*Partition*/      \
                                SHORT_SIZE          + /*Trace Level*/    \
                                UNSIGNED_INT_SIZE   + /*Future field 1*/ \
                                SHORT_SIZE          + /*TX Name*/        \
                                SHORT_SIZE          + /*Fetched Param*/  \
                                UNSIGNED_CHAR_SIZE  + /*Flow name size*/ \
                                SHORT_SIZE          + /*Log file sfx size*/\
                                SHORT_SIZE          + /*Response body orig name size*/\
				SHORT_SIZE          + /*Parameters size*/\
                                SHORT_SIZE            /*CV fail check point*/)
                                

// RecordType, TransactionIndex, SessionIndex, SessionInstance, ChildIndex, TxInstance, PageInstance, StartTime, EndTime, Status, PhaseIndex, 
#define TX_PG_RECORD_SIZE       (UNSIGNED_CHAR_SIZE + /*record_num*/    \
				SHORT_SIZE         + /*Child Idx*/	\
				UNSIGNED_INT_SIZE  + /*Tx Index*/	\
				INDEX_SIZE         + /*Session Idx*/	\
				UNSIGNED_INT_SIZE  + /*Session Inst*/	\
				SHORT_SIZE         + /*Tx Insatnce*/	\
				SHORT_SIZE         + /* PageInstance*/	\
				NS_TIME_DATA_SIZE  + /*Tx Begin at*/	\
				NS_TIME_DATA_SIZE  + /*Tx Begin at*/	\
				UNSIGNED_CHAR_SIZE +  /*Tx Status*/    \
				SHORT_SIZE            /*Phase id*/)


#define TX_RECORD_SIZE		(UNSIGNED_CHAR_SIZE + /*record_num*/	\
				SHORT_SIZE         + /*Child Idx*/	\
				UNSIGNED_INT_SIZE  + /*Tx Index*/	\
				INDEX_SIZE         + /*Session Idx*/	\
				UNSIGNED_INT_SIZE  + /*Session Inst*/	\
				SHORT_SIZE         + /*Tx Insatnce*/	\
				NS_TIME_DATA_SIZE  + /*Tx Begin at*/	\
				NS_TIME_DATA_SIZE  + /*Tx Begin at*/	\
				NS_TIME_DATA_SIZE  + /*Tx Think time*/	\
				UNSIGNED_CHAR_SIZE +  /*Tx Status*/    \
				SHORT_SIZE            /*Phase id*/)
                            

#define PAGE_RECORD_SIZE	(UNSIGNED_CHAR_SIZE + /*record_num*/	\
				SHORT_SIZE         + /*Child id*/	\
				INDEX_SIZE         + /*Cur Page*/	\
				INDEX_SIZE         + /*Session Idx*/	\
				UNSIGNED_INT_SIZE  + /*Session Inst*/	\
				UNSIGNED_INT_SIZE  + /*Cur Tx*/		\
				SHORT_SIZE         + /*Tx Inst*/	\
				SHORT_SIZE         + /*Page Inst*/	\
				NS_TIME_DATA_SIZE  + /*Page Begin at*/	\
				NS_TIME_DATA_SIZE  + /*Page Begin at*/	\
				UNSIGNED_CHAR_SIZE +  /*Page Status*/    \
    				SHORT_SIZE            /*Phase id*/)
                           
                         
#define URL_RECORD_SIZE		(UNSIGNED_CHAR_SIZE + /*record_num*/   \
				UNSIGNED_INT_SIZE  + /*X-dynatrace header string*/ \
				SHORT_SIZE         + /*Child id*/     \
				INDEX_SIZE         + /*Url num*/      \
				INDEX_SIZE         + /*Session Idx*/  \
				UNSIGNED_INT_SIZE  + /*Session Inst*/ \
				UNSIGNED_INT_SIZE  + /*Cur Tx*/       \
				INDEX_SIZE         + /*Cur Page*/     \
				SHORT_SIZE         + /*Tx Insatnce*/  \
				SHORT_SIZE         + /*Page Inst*/    \
				UNSIGNED_CHAR_SIZE + /*Bit mask*/     \
				NS_TIME_DATA_SIZE  + /*Started at*/   \
				UNSIGNED_INT_SIZE  + /*DNS strt time*/\
				UNSIGNED_INT_SIZE  + /*Connect time*/ \
				UNSIGNED_INT_SIZE  + /*Handshake done time*/ \
				UNSIGNED_INT_SIZE  + /*Write complete time*/ \
				UNSIGNED_INT_SIZE  + /*byte rcv time*/ \
				UNSIGNED_INT_SIZE  + /*Req complete time*/ \
				UNSIGNED_INT_SIZE  + /*Rendering time*/\
				NS_TIME_DATA_SIZE  + /*Url end time*/ \
				SHORT_SIZE         + /*Resp Code*/    \
				UNSIGNED_INT_SIZE  + /*Payload sent*/\
				UNSIGNED_INT_SIZE  + /*TCP Byte sent*/\
				UNSIGNED_INT_SIZE  + /*Eth Byte sent*/\
				UNSIGNED_INT_SIZE  + /*cptr bytes*/\
				UNSIGNED_INT_SIZE  + /*TCP Bytes recv*/\
				UNSIGNED_INT_SIZE  + /*Eth Byte recv*/\
				UNSIGNED_CHAR_SIZE + /*Compression mode*/     \
				UNSIGNED_CHAR_SIZE + /*Record Status*/     \
				UNSIGNED_INT_SIZE + /*Connetion number*/     \
				UNSIGNED_CHAR_SIZE + /*Connetion type*/     \
				UNSIGNED_CHAR_SIZE + /*Connetion Retries*/ \
                                UNSIGNED_LONG_SIZE +  /*ND instance*/     \
				SHORT_SIZE          /*Phase id*/)        
  
#define DATA_RECORD_HDR_SIZE	(UNSIGNED_CHAR_SIZE + /*record_num*/   \
				UNSIGNED_INT_SIZE  + /*Data Len*/     \
				SHORT_SIZE         + /*Child id*/     \
				UNSIGNED_INT_SIZE  + /*Session Inst*/ \
				INDEX_SIZE           /*Buf*/)


#define MSG_RECORD_HDR_SIZE	(UNSIGNED_CHAR_SIZE + /*record_num*/   \
				UNSIGNED_INT_SIZE  + /*Total Len*/     \
				UNSIGNED_INT_SIZE  + /*Data Len*/     \
				SHORT_SIZE         + /*Child id*/     \
				UNSIGNED_INT_SIZE  + /*Msg num*/      \
				NS_TIME_DATA_SIZE    /*Now*/)

#define RBU_RECORD_SIZE         (UNSIGNED_CHAR_SIZE + /*record_num*/           \
                                UNSIGNED_LONG_SIZE  + /*PartitionId*/          \
                                UNSIGNED_INT_SIZE   + /*HarFileNameSize*/      \
                                UNSIGNED_INT_SIZE   + /*PageIndex*/            \
                                INDEX_SIZE          + /*SessionIndex*/         \
                                SHORT_SIZE          + /*ChildIndex*/           \
                                UNSIGNED_INT_SIZE   + /*GeneratorId*/          \
                                SHORT_SIZE          + /*GroupNum*/             \
                                SHORT_SIZE          + /*HostId*/               \
                                SHORT_SIZE          + /*BrowserType*/          \
                                SHORT_SIZE          + /*ScreenHeight*/         \
                                SHORT_SIZE          + /*ScreenWidth*/          \
                                UNSIGNED_INT_SIZE   + /*DOMContentTime*/       \
                                UNSIGNED_INT_SIZE   + /*OnLoadTime*/           \
                                UNSIGNED_INT_SIZE   + /*PageLoadTime*/         \
                                UNSIGNED_INT_SIZE   + /*StartRenderTime*/      \
                                UNSIGNED_INT_SIZE   + /*TimeToInteract*/       \
                                UNSIGNED_INT_SIZE   + /*VisuallyCompleteTime*/ \
                                UNSIGNED_INT_SIZE   + /*ReqServerCount*/       \
                                UNSIGNED_INT_SIZE   + /*ReqBrowserCacheCount*/ \
                                UNSIGNED_INT_SIZE   + /*ReqTextTypeCount*/     \
                                UNSIGNED_INT_SIZE   + /*ReqTextTypeCumSize*/   \
                                UNSIGNED_INT_SIZE   + /*ReqJSTypeCount*/       \
                                UNSIGNED_INT_SIZE   + /*ReqJSTypeCumSize*/     \
                                UNSIGNED_INT_SIZE   + /*ReqCSSTypeCount*/      \
                                UNSIGNED_INT_SIZE   + /*ReqCSSTypeCumSize*/    \
                                UNSIGNED_INT_SIZE   + /*ReqImageTypeCount*/    \
                                UNSIGNED_INT_SIZE   + /*ReqImageTypeCumSize*/  \
                                UNSIGNED_INT_SIZE   + /*ReqOtherTypeCount*/    \
                                UNSIGNED_INT_SIZE   + /*ReqOtherTypeCumSize*/  \
                                UNSIGNED_INT_SIZE   + /*BytesReceived*/        \
                                UNSIGNED_INT_SIZE   + /*BytesSent*/            \
                                UNSIGNED_INT_SIZE   + /*SpeedIndex*/           \
                                UNSIGNED_LONG_SIZE  + /*MainUrlStartDateTime*/ \
                                UNSIGNED_LONG_SIZE  + /*HarFileDateTime*/      \
                                UNSIGNED_INT_SIZE   + /*ReqCountBeforeDomContent*/ \
                                UNSIGNED_INT_SIZE   + /*ReqCountBeforeOnLoad*/ \
                                UNSIGNED_INT_SIZE   + /*ReqCountBeforeStartRendering*/ \
                                UNSIGNED_INT_SIZE   + /*BrowserCachedReqCountBeforeDomContent*/ \
                                UNSIGNED_INT_SIZE   + /*BrowserCachedReqCountBeforeOnLoad*/     \
                                UNSIGNED_INT_SIZE   + /*BrowserCachedReqCountBeforeStartRendering*/ \
                                UNSIGNED_INT_SIZE   + /*CookiesCavNVSize*/                          \
                                UNSIGNED_INT_SIZE   + /*GroupName*/                                 \
                                UNSIGNED_INT_SIZE   + /*ProfileName */ \
                                UNSIGNED_INT_SIZE   + /*Page Status */ \
                                UNSIGNED_INT_SIZE   + /*Session Instance */ \
                                UNSIGNED_INT_SIZE   + /*Device Info */ \
                                SHORT_SIZE          + /*Performance Trace Dump */ \
                                UNSIGNED_INT_SIZE   + /*DOM Element */ \
                                UNSIGNED_INT_SIZE   + /*Backend Response Time */ \
                                UNSIGNED_INT_SIZE   + /*Byte Received Before DomContent Event Fired */ \
                                UNSIGNED_INT_SIZE   + /*Byte Received Before OnLoad Event Fired */ \
                                UNSIGNED_INT_SIZE   + /*First Paint Time */ \
                                UNSIGNED_INT_SIZE   + /*First Contentful Paint Time */ \
                                UNSIGNED_INT_SIZE   + /*Largest Contentful Paint Time */ \
                                UNSIGNED_INT_SIZE   + /*Cumulative Layout Shift */ \
                                UNSIGNED_INT_SIZE     /*Total Blocking Time */ )

#define RBU_LH_RECORD_SIZE      (UNSIGNED_CHAR_SIZE + /*record_num*/                    \
                                UNSIGNED_LONG_SIZE  + /*PartitionId*/                   \
                                UNSIGNED_INT_SIZE   + /*LighthouseReportFileName*/      \
                                UNSIGNED_INT_SIZE   + /*PageIndex*/                     \
                                INDEX_SIZE          + /*SessionIndex*/                  \
                                SHORT_SIZE          + /*ChildIndex*/                    \
                                UNSIGNED_INT_SIZE   + /*GeneratorId*/                   \
                                SHORT_SIZE          + /*GroupNum*/                      \
                                SHORT_SIZE          + /*HostId*/                        \
                                SHORT_SIZE          + /*BrowserType*/                   \
                                UNSIGNED_INT_SIZE   + /*GroupName*/                     \
                                UNSIGNED_LONG_SIZE  + /*LighthouseFileDateTime*/        \
                                UNSIGNED_INT_SIZE   + /*PerformanceScore*/              \
                                UNSIGNED_INT_SIZE   + /*PWAScore*/                      \
                                UNSIGNED_INT_SIZE   + /*AccessibilityScore*/            \
                                UNSIGNED_INT_SIZE   + /*BestPracticeScore*/             \
                                UNSIGNED_INT_SIZE   + /*SEOScore*/                      \
                                UNSIGNED_INT_SIZE   + /*FirstContentfulPaint*/          \
                                UNSIGNED_INT_SIZE   + /*FirstMeaningfulPaint*/          \
                                UNSIGNED_INT_SIZE   + /*SpeedIndex*/                    \
                                UNSIGNED_INT_SIZE   + /*FirstCPUIdle*/                  \
                                UNSIGNED_INT_SIZE   + /*TimeToInteract*/                \
                                UNSIGNED_INT_SIZE   +  /*InputLatency */                \
                                UNSIGNED_INT_SIZE   + /*LargestContentfulPaint*/        \
                                UNSIGNED_INT_SIZE   + /*CumulativeLayoutShift*/         \
                                UNSIGNED_INT_SIZE     /*TotalBlockingTime*/ )


#define MARK_MEASURE_RECORD_SIZE 	(UNSIGNED_CHAR_SIZE	+ /*record num*/		\
					UNSIGNED_INT_SIZE	+ /*PageIndex*/			\
					INDEX_SIZE		+ /*SessionIndex*/		\
					SHORT_SIZE		+ /*ChildIndex*/		\
					UNSIGNED_INT_SIZE	+ /*GeneratorId*/ 		\
					SHORT_SIZE		+ /*GroupNum*/			\
					SHORT_SIZE		+ /*HostId*/			\
					UNSIGNED_LONG_SIZE	+ /*abs start time*/		\
					SHORT_SIZE		+ /*name size*/			\
					UNSIGNED_CHAR_SIZE	+ /*record type*/		\
					UNSIGNED_INT_SIZE	+ /*startTime*/			\
					UNSIGNED_INT_SIZE	+ /*duration*/			\
					UNSIGNED_INT_SIZE 	+ /*session instance*/		\
					SHORT_SIZE		 /*page instance*/)		
	

/* DATA_RECORD_HDR_SIZE + at least 1 byte of data */
#define DATA_RECORD_MIN_SIZE	(DATA_RECORD_HDR_SIZE + 1)

/* MSG_RECORD_HDR_SIZE + at least 1 byte of data */
#define MSG_RECORD_MIN_SIZE	(MSG_RECORD_HDR_SIZE + 1 + 1)

typedef struct write_buffer {
  short memory_flag;
  short disk_flag;
  unsigned int block_num;  // Running counter (starting wit 1) to just give seq number to every block to be written
  //char data_buffer[BLOCK_SIZE];
  char *data_buffer;
  sem_t sem;
} write_buffer;

typedef struct shr_logging {
  int write_flag;
  int current_buffer_idx;
  int cur_lw_idx;
  int bytes_written;
  u_ns_ts_t prev_disk_timestamp;//This timestamp is set when shared memory block is marked for writting into disk
  write_buffer buffer[1];
} shr_logging;

typedef struct local_buffer {
  struct local_buffer* next;
  //char data_buffer[BLOCK_SIZE];
  char *data_buffer;
} local_buffer;
extern int nslb_write_process_pid(int pid, char *programm_name, char *ns_wdir, int trnum, char *mode, char *process_name,char *err_msg);
extern shr_logging* logging_shr_mem;

extern int log_shr_buffer_size;
extern int static_logging_fd;
extern FILE *static_logging_fp;
extern int runtime_logging_fd;
extern int session_status_fd;

extern int writer_pid;
extern int reader_pid;
extern int nia_req_rep_uploader_pid;
extern int *new_test_run_num;
extern key_t shm_base_mem;
extern unsigned char my_port_index;
extern int logging_shr_blk_count;
extern int log_message_record(unsigned int msg_num, u_ns_ts_t now, char* buf, int buf_length);
extern int log_url(char *url_str, int len, unsigned int url_index, unsigned int page_id, unsigned int url_hash_index, unsigned int url_hash_code, char *page_name);

extern char shr_mem_new_tr_key[];

extern const int url_record_size;
extern const int page_record_size;
extern const int tx_record_size;
extern const int tx_pg_record_size;

extern const int zero_int;
extern const unsigned char zero_unsigned_char;
extern unsigned int  total_url_records;
extern long long total_url_record_size;
extern unsigned int  total_page_records;
extern long long total_page_record_size;
extern unsigned int  total_tx_records;
extern long long total_tx_record_size;
extern unsigned int  total_tx_pg_records;
extern long long total_tx_pg_record_size;
// These variable are moved from logging.c (STATIC=>GLOBAL)
// Reason: We are using it in ns_jmeter.c
extern long long total_page_dump_record_size;
extern unsigned int total_page_dump_records;
extern unsigned int  total_session_records;
extern long long total_session_record_size;

//extern inline char* forward_buffer(char* buf, int amt_written, int max_size, int* space_left);
extern int  init_nvm_logging_shr_mem();
extern void kw_logging_writer_debug(char *buf);
extern int kw_set_ns_logging_writer_arg(char *buf);
extern int kw_set_nirru_trace_level(char *buf, char *err_msg, int runtime_flag);
extern void ns_logging_writer_arg_usage(char *err_msg, char *buf);
extern void kw_set_max_logging_bufs(char *buf);
extern void handle_chk_nsa_logger_recovery();
extern int nsa_logger_recovery();
extern int req_rep_uploader_recovery(); 
extern int start_req_rep_uploader(); 

extern inline void fill_up_buffer(int size_written, char* buffer);
extern inline void flush_shm_buffer();
extern int open_dlog_file_in_append_mode (int init_or_partition_call);
#define LOGGING_HEADER_SIZE(blk_count) (sizeof(shr_logging) + ((blk_count - 1) * sizeof(write_buffer))) 

#define LOGGING_PER_CHILD_SHR_MEM_SIZE(shr_buf_size, blk_count) \
	(LOGGING_HEADER_SIZE(blk_count) + (shr_buf_size * blk_count))

#define LOGGING_DATA_BUFFER_PTR(base_addr, chld_idx, buf_idx, shr_buf_size, blk_count)  \
	(char *)(base_addr + (chld_idx * (LOGGING_HEADER_SIZE(blk_count) + (shr_buf_size * blk_count)))  \
  +  \
  (LOGGING_HEADER_SIZE(blk_count) + (buf_idx*shr_buf_size)))


extern int log_page_rbu_detail_record();
extern int log_rbu_light_house_detail_record();
extern inline void log_rbu_mark_and_measure_record();
extern inline char* get_mem_ptr(int size);
#endif
