#ifndef NS_ND_INTEGRATION_H
#define NS_ND_INTEGRATION_H

#define IS_CORRELATION_ID_ENABLED \
  ((((loc_gset->correlationIdSettings.mode == 1) && (cptr->url_num->proto.http.type == MAIN_URL)) || (loc_gset->correlationIdSettings.mode == 2)) && (loc_gset->correlationIdSettings.header_query_mode != 2))

#define COR_ID_REQUEST_ID_ENABLED       0x0001 // ID -> Request ID
#define COR_ID_VIRTUAL_USER_ID_ENABLED  0x0002 // VU -> Virtual User ID
#define COR_ID_LOCATION_ENABLED         0x0004 // GR -> Location
#define COR_ID_SCRIPT_NAME_ENABLED      0x0008 // SN -> Script Name
#define COR_ID_AGENT_NAME_ENABLED       0x0010 // AN -> Agent Name
#define COR_ID_PAGE_NAME_ENABLED        0x0020 // PC -> Page Name
#define COR_ID_TIMESTAMP_ENABLED        0x0040 // TS -> TimeStamp
#define COR_ID_CLIENT_IP_ENABLED        0x0080 // CI -> Client ID
#define COR_ID_DEST_IP_ENABLED          0x0100 // DI -> Destination ID

#define FLAG_HTTP1			1
#define FLAG_HTTP2			2

#define CORR_ID_NAME_VALUE_BUFF_SIZE	1024+2 //For \r\n

typedef struct { 
  char mode;
  char header_query_mode;
  char *header_name;
  int header_name_len;
  char *query_name;
  char *prefix;
  int custom_corr_id_flag; 
  char *suffix;
  char sourceID[32];
} CorrelationIdSettings;

extern CorrelationIdSettings *correlationIdSettings;
extern int ns_cor_id_header_opt();
extern int kw_set_g_enable_corr_id();
#endif
