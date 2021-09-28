/*
 *
 * 1.	NetStorm System Events (all internal modules) - these events concern how NetStorm or NetStorm infrastructure  (1 to 10,000)
 * 2.	Application events - data/events which concern application under test (app server, unix host, network .... this will cover most monitors) (10,001 to 19,000)
 * 3.	Custom Application events - like 2 , except defined by the user (20,001 onwards)
 *
 * */


/* Data monitor Event IDs*/
#define EID_DATAMON_INV_DATA		1
#define EID_DATAMON_ERROR		2
#define EID_DATAMON_API			3
#define EID_DATAMON_GENERAL		4

/* Check monitor Event IDs*/
#define EID_CHKMON_ERROR		11
#define EID_CHKMON_API			12
#define EID_CHKMON_GENERAL		13

#define EID_NS_INIT			25
#define EID_FOR_API			26

#define EID_CONN_FAILURE		30
#define EID_WAN_ENV                     31

//HTTP Authentication
#define EID_AUTH_NTLM_INVALID_PKT       50
#define EID_AUTH_NTLM_CONN_ERR          51

#define EID_TRANS_BY_NAME               70 // Two atributes (script and tx name)
#define EID_TRANS                       71 // One atributes (script)

#define EID_HTTP_CACHE                  72 
#define EID_RUNTIME_CHANGES_OK          81 
#define EID_RUNTIME_CHANGES_ERROR       82 

#define EID_FILTER_MSG			99

// JavaScript related eventd
#define EID_JS_EVALUATE_SCRIPT                 100
#define EID_JS_URL_NOT_IN_CACHE                101
#define EID_JS_DECOMPRESS                      102
#define EID_JS_INVALID_URL		       103
#define EID_JS_XML_ERROR                       104
#define EID_JS_ERROR			       105
#define EID_JS_DOM                             106
#define EID_JS_ENGINE                          107
#define EID_JS_CALL_FUNCTION_FAILED            108
#define EID_JS_NEW_STRING_COPY                 109



/*START: User context switch events*/
#define EID_VUSER_CTX                          251
/*END: User context switch events*/

/*START: User Thread context events*/
#define EID_VUSER_THREAD                       252
/*END: User Thread context events*/

/* Protocol based URL/action failure (1 to 31)
 * Page
 * Tx
 * Sess
 * TOTAL = 96
*/
/* For time being we are using all errro code for all Protocols*/
#define EID_HTTP_URL_ERR_START    1000
#define EID_SMTP_ERR_START        EID_HTTP_URL_ERR_START 
#define EID_POP3_ERR_START        EID_HTTP_URL_ERR_START
#define EID_DNS_ERR_START         EID_HTTP_URL_ERR_START
#define EID_FTP_ERR_START         EID_HTTP_URL_ERR_START
#define EID_WS_ERR_START          EID_HTTP_URL_ERR_START

#define EID_HTTP_PAGE_ERR_START   5000
#define EID_SMTP_PAGE_ERR_START   EID_HTTP_PAGE_ERR_START 
#define EID_POP3_PAGE_ERR_START   EID_HTTP_PAGE_ERR_START
#define EID_DNS_PAGE_ERR_START    EID_HTTP_PAGE_ERR_START
#define EID_FTP_PAGE_ERR_START    EID_HTTP_PAGE_ERR_START

#define EID_SESS_ERR_START        5001

#define EID_DOS_ATTACK            6001

#define EID_SP_RELEASE            	   7001
#define EID_SP_ADD_USER           	   7002
#define EID_SP_RUNTIME_CHANGE     	   7003
#define EID_SP_TIMEOUT            	   7004
#define EID_SP_ADD_FIRST_OR_LAST_USER      7005

#define EID_TX_ERR_START          10001

#define EID_DYNAMIC_HOST          301 

#define EID_LOGGING_BUFFER        401

#define EID_NDCOLLECTOR        501
#define EID_CONN_HANDLING      601
#define EID_MISC      701
