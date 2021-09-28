#ifndef FC2_
#define FC2_

#pragma pack(push,1) //to disable padding

/* Byte order mark value */
#define FC2_BOM 0x001F

#define FC2_VERSION1_0  0x0100  /* version 1.0 */
/* protocol version */
#define FC2_VERSION1_1  0x0101  /* version 1.1 */
#define FC2_VERSION     FC2_VERSION1_1  /* current version */

/* magic number */
#define FC2MARK 0xEF3C

/* buffer sizes */
#define FC2_MSG_NAME_BUFFER_SIZE 36
#define FC2_RESERVED_BUFFER_SIZE 64
#define FC2_UUID_BUFFER_SIZE (128/8)
#define FC2_RESP_MSG_TEXT_SIZE 128
#define FC2_ROUTE_ID_BUFFER_SIZE 16

/* supported application data code pages */
/* EBCDIC "US English" */
#define FC2_CCSID_037   37
/* ASCII code page on Windows 95 NT and up */
#define FC2_CCSID_1252  1252

/* flags */
#define FC2_FLG_NONE            0x00000000
#ifdef USE_FC2_VERSION_1_1
#define FC2_FLG_ROUTE_ID        0x00000001
#ifdef WIN32
#define FC2_FLG_FEDPASS         0x00000002
#endif
#define FC2_FLG_RESP_COUNT      0x00000004
#endif

/* message types */
#define FC2_MSG_TYPE_HANDSHAKE          0x01
#define FC2_MSG_TYPE_TERMINATE          0x02
#define FC2_MSG_TYPE_APPLICATION        0x00


/* response status codes */
#define FC2_SUCCESS 0
#define _FC2_FAILED(v) (v != FC2_SUCCESS)

// Error Codes     
#define FC2_ERROR     -1
#define FC2_PARTIAL   -2

#define FC2_SEND_HANDSHAKE 0
#define FC2_RECEIVE_HANDSHAKE 1
#define FC2_SEND_MESSAGE 2
#define FC2_RECEIVE_MESSAGE 3
#define FC2_END 4

#define HTTP_URL_BUFFER_SIZE    256
#define Q_NAME_LEN 20

/* Client handshake message body
 * if ulIdleTimeout is any value other than FC2_IDLE_TIMEOUT_INVALID
 * the client is asking the server to adjust it's own idle timeout value
 * for the connection. The server may honor the request and adjust it's own timeout value.
 * If the client sets the ucResetIdleTimeout to FC2_IDLE_TIMEOUT_RESET_TRUE it is
 * asking the server to reset it's timeout timer. The server responds with 
 * FC2_IDLE_TIMEOUT_RESET_TRUE in the response ucResetIdleTimeout field if the timer was reset.
 */


typedef struct
{
   unsigned int        ulIdleTimeout;  /* client connection idle timeout in seconds */
   unsigned char        ucResetIdleTimeout;     /* request to server to reset idle timer 0 = FALSE */
} FC2_Handshake_Req_t;

typedef struct
{
   unsigned int        ulIdleTimeout;  /* server connection idle timeout in seconds */
   unsigned char        ucResetIdleTimeout;     /* server has reset idle timer */
} FC2_Handshake_Resp_t;

/* This is the only message that can originate on the server 
 * Client or server can send this message body to notify the peer about a connection 
 * termination. If the message originated on the server, the client must check the 
 * FC2_TERM_CLIENT_RECONNECT_MASK bit. If the bit is off the client must not re-connect
 * to the server until an active application request is ready to be send. if the bit is set
 * the client may re-connect as soon as possible, unless the ucReconnectDelay field contains 
 * a non-zero value. In this case the client must wait at least ucReconnectDelay milliseconds 
 * before initiating a new IP connection
 */
typedef struct
{
   unsigned char        ucReason;       /* FC2_TERM_... reason codes  */
   unsigned int        ucReconnectDelay;       /* Re-connect delay in milliseconds  */
} FC2_Terminate_Msg_t;

typedef struct
{
  union
  {
        struct
        {
                unsigned char   szRouteId[FC2_ROUTE_ID_BUFFER_SIZE];
#ifdef WIN32
                FedPassTicket_t FedpassTicket;
#endif
        } AppData;
        unsigned char   ucReserved[FC2_RESERVED_BUFFER_SIZE];
  };
} FC2_Req_HdrCtx_t;


typedef struct
{
   union
   {
                unsigned short  usBOM;                                                  /* Byte Order Mark */
                unsigned char   ucBOM[sizeof(unsigned short)];  /* 0x00 0x1F == Big-Endian  */
   };
   unsigned short       usVersion;  /* FC2_VERSION  */
   unsigned short       usFC2Mark;      /* Bit pattern FC2MARK */
   unsigned short       usDataCodePage; /* Message body code page */
   unsigned int        ulFlags;        /* flags */
   unsigned char        szMsgName[FC2_MSG_NAME_BUFFER_SIZE];    /* message name, other FedConnect components 
                                                                                                                   may place restrictions on name length or
                                                                                                                   require some naming conventions */
   unsigned int        ulExHdrSize;    /* Extention header size */
   unsigned int        ulInDataSize;   /* Request message body size */
   unsigned int        ulOutDataSize;  /* Expected response message body size */
   unsigned int        ulTimeout;              /* client timeout in seconds */
   union                                                        /* RefId may be a 4 byte value or a 128 bit UUID/GUID */
   {                                                            /* this element must not be changed by the server and 
                                                                         sizeof(uuidRefId) must be returned to the client */
           unsigned int        ulRefId;        
           unsigned char        uuidRefId[FC2_UUID_BUFFER_SIZE];
   };
   unsigned char        ucMessageType;  /* FC2_MSG_TYPE_... value */
   FC2_Req_HdrCtx_t ReqCntxt;
}FC2_Req_Hdr_t;

typedef struct
{
   union
   {
                unsigned short  usBOM;                                                  /* Byte Order Mark */
                unsigned char   ucBOM[sizeof(unsigned short)];  /* 0x00 0x1F == Big-Endian  */
   };
   unsigned short   usVersion;  /* FC2_VERSION1_1  */
   unsigned short       usFC2Mark;      /* Bit pattern FC2MARK */
   unsigned short       usDataCodePage; /* Message body code page */
   unsigned int        ulFlags;        /* flags */
   unsigned int        ulResponseCode; /* Response status code */
   unsigned char        szResponseMsgText[FC2_RESP_MSG_TEXT_SIZE];
   unsigned char        szMsgName[FC2_MSG_NAME_BUFFER_SIZE];    /* message name, other FedConnect components 
                                                                                                                   may place restrictions on name length or
                                                                                                                   require some naming conventions */
   unsigned int        ulExHdrSize;    /* Extention header size */
   unsigned int        ulOutDataSize;  /* Expected response message body size */
   union                                                        /* RefId may be a 4 byte value or a 128 bit UUID/GUID */
   {                                                            /* this element must not be changed by the server and 
                                                                         sizeof(uuidRefId) must be returned to the client */
           unsigned int        ulRefId;        
           unsigned char        uuidRefId[FC2_UUID_BUFFER_SIZE];
   };
   unsigned char        ucMessageType;  /* FC2_MSG_TYPE_... value */
#ifdef WIN32
   FedPassTicket_t  FedpassTicket;
#else
   unsigned char        FedpassTicketDummy[45];
#endif
} FC2_Resp_Hdr2_t;

typedef struct
{
   unsigned short usLocation;
   unsigned short usTerm;
   unsigned short usTran;
   unsigned short usSeqNum;
   unsigned  char OptInfo;
   unsigned short Type;
   unsigned short SubType;
   unsigned  char ucVersion;
   unsigned short usMiscFlags;
   unsigned  char ucAcctNum[23];
   unsigned  char ClientId[100];
} TRANS_LVL_INFO;

typedef struct
{
   int            protocol;                  /* Protocol (FC2, HTTP, HTTPS) to use.  */
   unsigned char  szMsgName[FC2_MSG_NAME_BUFFER_SIZE];  /* FC2 - Msg Schema Name.    */
   unsigned char  szMsgURL[HTTP_URL_BUFFER_SIZE];       /* HTTP - URL Name           */
   unsigned int  ulRequestDataSize;         /* FC2 / HTTP Request msg body size.    */
   unsigned int  ulReplyDataSize;           /* FC2 / HTTP Response msg body size.   */
   unsigned int  ulTimeout;                 /* FC2 / HTTP Client timeout in seconds.*/
   unsigned int  ulGWRespCode;              /* Response code returned by the GW.    */
   unsigned char  szReplyQ[Q_NAME_LEN];      /* Queue to send the reply to.          */
   TRANS_LVL_INFO TransLvlInfo;              /* Data needed to send reply            */
                                             /*  to the correct destination.         */
   int            cc;                        /* Completion code.                     */
   int            ec;                        /* Error code.                          */
} FED_CON_APP_HDR_T;

typedef struct COMMON_HEADER_t {
  unsigned short Length;
  unsigned short Division;
  unsigned short Store;
  unsigned char Pad;
  unsigned long MediaDate;
  unsigned short Term;
  unsigned short Trans;
  unsigned long StoreSerialNum;
  unsigned short MiscFlag;
  unsigned short MsgType;
  unsigned char Version;
  unsigned char LateDay;
} COMMON_HEADER_t;

#pragma pack(pop)

#ifndef CAV_MAIN
extern int g_fc2_avgtime_idx;
extern int g_fc2_cavgtime_idx;
#else
extern __thread int g_fc2_avgtime_idx;
extern __thread int g_fc2_cavgtime_idx;
#endif
typedef struct{

  /*FC2*/
  u_ns_8B_t fc2_num_hits;
  u_ns_8B_t fc2_num_tries;
  u_ns_8B_t fc2_fetches_started;
  u_ns_8B_t fc2_fetches_sent;
  u_ns_8B_t fc2_total_bytes;

  // fc2 Success
  u_ns_8B_t fc2_avg_time;
  u_ns_4B_t fc2_min_time;
  u_ns_4B_t fc2_max_time;
  u_ns_8B_t fc2_tot_time;

  //fc2 Overall
  u_ns_8B_t fc2_overall_avg_time;
  u_ns_4B_t fc2_overall_min_time;
  u_ns_4B_t fc2_overall_max_time;
  u_ns_8B_t fc2_overall_tot_time;

  //fc2 Failed
  u_ns_8B_t fc2_failure_avg_time;
  u_ns_4B_t fc2_failure_min_time;
  u_ns_4B_t fc2_failure_max_time;
  u_ns_8B_t fc2_failure_tot_time;

}FC2AvgTime;

typedef struct{

  /*FC2*/
  u_ns_8B_t fc2_succ_fetches;
  u_ns_8B_t fc2_fetches_completed;
  u_ns_8B_t fc2_c_min_time;                  //Overall fc2 time - fc2_overall_min_time
  u_ns_8B_t fc2_c_max_time;                  //Overall fc2 time - fc2_overall_max_time             
  u_ns_8B_t fc2_c_tot_time;                  //Overall fc2 time - fc2_overall_tot_time
  u_ns_8B_t fc2_c_avg_time;                  //Overall fc2 time - fc2_overall_avg_time
  u_ns_8B_t fc2_c_fetches_started;
  u_ns_8B_t fc2_c_tot_total_bytes;

}FC2CAvgTime;

#define SET_MIN_MAX_FC2_CUMULATIVES(a, b){\
  SET_MIN ((a)->fc2_c_min_time, (b)->fc2_overall_min_time);\
  SET_MAX ((a)->fc2_c_max_time, (b)->fc2_overall_max_time);\
}\

#define SET_MIN_MAX_FC2_CUMULATIVES_PARENT(a, b){\
  SET_MIN ((a)->fc2_c_min_time, (b)->fc2_c_min_time);\
  SET_MAX ((a)->fc2_c_max_time, (b)->fc2_c_max_time);\
}\

#define ACC_FC2_CUMULATIVES(a, b) {\
  (a)->fc2_c_fetches_started += (b)->fc2_fetches_started;\
  (a)->fc2_fetches_completed += (b)->fc2_num_tries;\
  (a)->fc2_succ_fetches += (b)->fc2_num_hits;\
  (a)->fc2_c_tot_time += (b)->fc2_overall_tot_time;\
  (a)->fc2_c_tot_total_bytes += (b)->fc2_total_bytes;\
}\

#define ACC_FC2_CUMULATIVES_PARENT(a, b) {\
  (a)->fc2_fetches_completed += (b)->fc2_fetches_completed;\
  (a)->fc2_succ_fetches += (b)->fc2_succ_fetches;\
  (a)->fc2_c_tot_time += (b)->fc2_c_tot_time;\
}\
  
#define SET_MIN_MAX_FC2_PERIODICS(a, b){\
  SET_MIN ((a)->fc2_min_time, (b)->fc2_min_time);\
  SET_MAX ((a)->fc2_max_time, (b)->fc2_max_time);\
  SET_MIN ((a)->fc2_overall_min_time, (b)->fc2_overall_min_time);\
  SET_MAX ((a)->fc2_overall_max_time, (b)->fc2_overall_max_time);\
  SET_MIN ((a)->fc2_failure_min_time, (b)->fc2_failure_min_time);\
  SET_MAX ((a)->fc2_failure_max_time, (b)->fc2_failure_max_time);\
}\

#define ACC_FC2_PERIODICS(a, b){\
  (a)->fc2_fetches_started += (b)->fc2_fetches_started;\
  (a)->fc2_fetches_sent += (b)->fc2_fetches_sent;\
  (a)->fc2_num_hits += (b)->fc2_num_hits;\
  (a)->fc2_num_tries += (b)->fc2_num_tries;\
  (a)->fc2_overall_tot_time += (b)->fc2_overall_tot_time;\
  (a)->fc2_tot_time += (b)->fc2_tot_time;\
  (a)->fc2_failure_tot_time += (b)->fc2_failure_tot_time;\
}\

#define CHILD_RESET_FC2_STAT_AVGTIME(a){\
  FC2AvgTime *loc_fc2_avgtime; \
  loc_fc2_avgtime = (FC2AvgTime*)((char*)a + g_fc2_avgtime_idx); \
  loc_fc2_avgtime->fc2_num_hits = 0;\
  loc_fc2_avgtime->fc2_num_tries = 0;\
  loc_fc2_avgtime->fc2_min_time = 0xFFFFFFFF;\
  loc_fc2_avgtime->fc2_avg_time = 0;\
  loc_fc2_avgtime->fc2_max_time = 0;\
  loc_fc2_avgtime->fc2_tot_time = 0;\
  loc_fc2_avgtime->fc2_fetches_started = 0;\
  loc_fc2_avgtime->fc2_fetches_sent = 0;\
  loc_fc2_avgtime->fc2_overall_avg_time = 0;\
  loc_fc2_avgtime->fc2_overall_min_time = 0xFFFFFFFF;\
  loc_fc2_avgtime->fc2_overall_max_time = 0;\
  loc_fc2_avgtime->fc2_overall_tot_time = 0;\
  loc_fc2_avgtime->fc2_failure_avg_time = 0;\
  loc_fc2_avgtime->fc2_failure_min_time = 0xFFFFFFFF;\
  loc_fc2_avgtime->fc2_failure_max_time = 0;\
  loc_fc2_avgtime->fc2_failure_tot_time = 0;\
  loc_fc2_avgtime->fc2_total_bytes = 0;\
}   

#define INC_FC2_NUM_TRIES_COUNTER(vptr){ \
  FC2AvgTime *loc_fc2_avgtime; \
  loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx); \
  loc_fc2_avgtime->fc2_num_tries++; \
  if(SHOW_GRP_DATA) { \
    FC2AvgTime *local_fc2_avg; \
    local_fc2_avg = (FC2AvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_fc2_avgtime_idx);\
    local_fc2_avg->fc2_num_tries++; \
  }\
}  

#define INC_FC2_FETCHES_STARTED_COUNTER(vptr){ \
  FC2AvgTime *loc_fc2_avgtime; \
  loc_fc2_avgtime = (FC2AvgTime*)((char*)average_time + g_fc2_avgtime_idx); \
  loc_fc2_avgtime->fc2_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    FC2AvgTime *local_fc2_avg; \
    local_fc2_avg = (FC2AvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_fc2_avgtime_idx);\
    local_fc2_avg->fc2_fetches_started++; \
  }\
}

#define FILL_FC2_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count){ \
  if(SHOW_GRP_DATA) \
   { \
    FC2AvgTime *local_fc2_avg; \
    local_fc2_avg = (FC2AvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_fc2_avgtime_idx);\
    SET_MIN (local_fc2_avg->min, download_time); \
    SET_MAX (local_fc2_avg->max, download_time); \
    local_fc2_avg->tot += download_time; \
    if(count) \
      local_fc2_avg->fc2_num_hits++; \
  }\
}

extern int sgrp_used_genrator_entries;   
extern int ns_parse_fc2_send(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int sgrp_used_genrator_entries;    
extern inline void update_fc2_avgtime_size();
extern inline void update_fc2_cavgtime_size();
extern void fill_fc2_gp (avgtime **g_avg, cavgtime **g_cavg);
#endif
