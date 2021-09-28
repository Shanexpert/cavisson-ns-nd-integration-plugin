/********************************************************************
 * Name            : nsu_get_errors.h  
 * Purpose         : Definition file of error codes 
 * Initial Version : Wednesday, November 04 2009 
 * Modification    : -
 ********************************************************************/

#ifndef NSU_GET_ERRORS_H
#define NSU_GET_ERRORS_H

/* Error codes are bucketed as follows
 * if any thing change here should be changed in nsu_get_errors
 * Url Errors         => 0 to 31
 * Page Errors        => URL Errors + (32 to 63)
 * Transaction Errors => PAGE Errors + 32 Users Errors
 * Session Errors     => 0 to 2 + 13 Users Errors  
 */

/* Since Session status in VUser and Tx status in Tx Node is uchar, error code cannot be >= 255
 * as we are using 255 for not set in case of tx error
 */

#define NS_REQUEST_OK                 0
#define NS_REQUEST_ERRMISC            1
#define NS_REQUEST_1xx                2
#define NS_REQUEST_2xx                3
#define NS_REQUEST_3xx                4
#define NS_REQUEST_4xx                5
#define NS_REQUEST_5xx                6
#define NS_REQUEST_BADBYTES           7 // Partial Body
#define NS_REQUEST_TIMEOUT            8
#define NS_REQUEST_CONFAIL            9
#define NS_REQUEST_CLICKAWAY          10
#define NS_REQUEST_WRITE_FAIL         11

// SSL -- Start
#define NS_REQUEST_SSLWRITE_FAIL      12
#define NS_REQUEST_SSL_HSHAKE_FAIL    13
// SSL -- End

#define NS_REQUEST_INCOMPLETE_EXACT   14
#define NS_REQUEST_NO_READ            15
#define NS_REQUEST_BAD_HDR            16 // Partial header
#define NS_REQUEST_BAD_BODY_NOSIZE    17
#define NS_REQUEST_BAD_BODY_CHUNKED   18
#define NS_REQUEST_BAD_BODY_CONLEN    19
#define NS_REQUEST_RELOAD             20
#define NS_REQUEST_STOPPED            21   // URL Stopped due to ramp down

#define NS_REQUEST_BAD_RESP           22 /* -ve response */
#define NS_REQUEST_MBOX_ERR           23 /* mail box err */
#define NS_REQUEST_MBOX_STORAGE_ERR   24 /* mail box storage err */
#define NS_REQUEST_AUTH_FAIL          25 /*Auth Fail for SMTP*/

#define NS_USEONCE_ABORT              26
#define NS_REDIRECT_EXCEED            27
#define NS_UNIQUE_RANGE_ABORT_SESSION 28
#define NS_SOCKET_BIND_FAIL           29
#define NS_DNS_LOOKUP_FAIL            30
#define NS_SYSTEM_ERROR               31
// Out of TOTAL_URL_ERR, how many are used
// Currently it includes protocol specific also
#define TOTAL_USED_URL_ERR            32 //
/* Undefined for Future
 * Update TOTAL_USED_URL_ERR after using these errors
 * /

#define NS_REQUEST_UNDEF28            28
#define NS_REQUEST_UNDEF29            29
#define NS_REQUEST_UNDEF30            30
#define NS_REQUEST_UNDEF31            31
*/

// Only Page error code
#define NS_REQUEST_URL_FAILURE        32 // If embedded URL failure fails the page
#define NS_REQUEST_CV_FAILURE         33 // Checkoint, search var failures
#define NS_UNCOMP_FAIL                34
//page error for nsl_check_reply_size() api
#define NS_REQUEST_SIZE_TOO_SMALL     35
#define NS_REQUEST_SIZE_TOO_BIG       36
#define NS_REQUEST_ABORT              37 //Page Aborted on Failure 

// Out of TOTAL_PAGE_ERR, how many are used
// Includes all URL errors include unused URL error codes
#define TOTAL_USED_PAGE_ERR           38   //New Page code is added 'NS_REQUEST_ABORT'. Hence, incremented by one


/* Undfined
#define NS_REQUEST_UNDEF34            37
#define NS_REQUEST_UNDEF35            38
#define NS_REQUEST_UNDEF36            39
......
......
#define NS_REQUEST_UNDEF62            62
#define NS_REQUEST_UNDEF63            63
*/

// Only Session error code first two are same as URL
#define NS_SESSION_STOPPED            2   // Session Stopped due to ramp down
#define NS_SESSION_ABORT              3   // Session Aborted on Page Failure 

//Make request return error codes
#define MR_USE_ONCE_ABORT      -2
#define NS_STREAM_ERROR        -3
#define NS_STREAM_ERROR_NULL   -4
#define MR_IO_VECTOR_MAX_LIMIT -5

//#define NS_NUM_URL_STATUS       64
//#define NS_NUM_PAGE_STATUS      64
//#define NS_NUM_TRANS_STATUS     64
//#define NS_NUM_SESS_STATUS      16

/*
  #define NS_NUM_URL_SYSTEM_DEFINED       24
  #define NS_NUM_PAGE_SYSTEM_DEFINED      32
  #define NS_NUM_TRANS_SYSTEM_DEFINED     16
  #define NS_NUM_SESS_SYSTEM_DEFINED      16
*/

//This defines the minimim valid page status for content varification/search vars etc
#define NS_REQUEST_MIN_VALID TOTAL_URL_ERR


#define TOTAL_URL_ERR      32
#define TOTAL_PAGE_ERR     64
#define TOTAL_TX_ERR       96
#define TOTAL_SESS_ERR     16

//indices to the start of Undef in the error codes array
#define INDEX_UNDEF_PAGE_ERR 5 //start index of Undef in page_errors_codes

#define USER_DEF_TX_ERR    32
#define USER_DEF_SESS_ERR  12

#define OBJ_URL_ID         0
#define OBJ_PAGE_ID        1
#define OBJ_TRANS_ID       2
#define OBJ_SESS_ID        3

#define RETRY_ON_TIMEOUT_OFF 0
#define RETRY_ON_TIMEOUT_SAFE 1

//Not under url/page/session code
#define NS_REQ_PROTO_NOT_SUPPORTED_BY_SERVER    200        //WebSocket, when server is not supported with prototype. Previously 38
#define NS_HTTP2_REQUEST_ERROR                  201        //HTTP2, at time of connection close, used for retry. Previously 39

typedef struct ErrorCodeTableEntry_Shr {
  short error_code;
  char* error_msg;
} ErrorCodeTableEntry_Shr;

extern int input_error_codes(void);
extern void copy_errorCodeTable_to_errorcode_table_shr_mem();
extern char *get_error_code_name(int error_code);
extern char *get_session_error_code_name(int error_code);
extern int create_errorcode_table_entry(int *row_num);

extern int pg_error_code_start_idx;
extern int tx_error_code_start_idx;
extern int sess_error_code_start_idx;
#ifndef CAV_MAIN
extern ErrorCodeTableEntry_Shr *errorcode_table_shr_mem;
extern int total_errorcode_entries;
#else
extern __thread ErrorCodeTableEntry_Shr *errorcode_table_shr_mem;
extern __thread int total_errorcode_entries;
#endif
extern char **get_error_codes_ex(int arg, int num_err);
#endif
