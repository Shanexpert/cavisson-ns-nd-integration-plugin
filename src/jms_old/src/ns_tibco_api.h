#ifndef NS_TIBCO_API_H
#define NS_TIBCO_API_H

#include <tibems.h>

typedef struct {
  tibemsConnectionFactory     factory;
  tibemsConnection            connection;
  tibemsSession               session;
  tibemsMsgProducer           msgProducer;
  tibemsDestination           destination ;
  tibemsSSLParams             sslParams;
  tibemsErrorContext          errorContext;
  tibems_status               status;
  //tibemsTextMsg               msg;
  tibemsMsg                   msg;
} TibcoClientInfo;

typedef struct {
  tibemsConnectionFactory         factory;
  tibemsConnection                connection;
  tibemsSession                   session;
  tibemsMsgConsumer               msgConsumer;
  tibemsDestination               destination;
  tibemsSSLParams                 sslParams;
  tibems_int                      receive;
  tibemsErrorContext              errorContext;
  tibems_status                   status;
  tibemsMsg                       msg;
  const char*                     txt;
  tibemsMsgType                   msgType;
  tibemsAcknowledgeMode           ackMode;
  char*                           msgTypeName;
} TibcoClientReceiveInfo;


#define NS_TIBCO_USE_TOPIC 1
#define NS_TIBCO_USE_QUEUE 2

#define NS_TIBCO_DEFAULT_TIMESTAMP   -1
#define NS_TIBCO_DISABLE_TIMESTAMP    0

#define NON_DURABLE_CONSUMER    0
#define DURABLE_CONSUMER        1

/*Property type */
#define NS_TIBCO_BOOLEAN 11
#define NS_TIBCO_BYTE 12
#define NS_TIBCO_DOUBLE 13
#define NS_TIBCO_FLOAT 14
#define NS_TIBCO_INTEGER 15
#define NS_TIBCO_LONG 16
#define NS_TIBCO_SORT 17
#define NS_TIBCO_STRING 18

extern TibcoClientInfo *ns_tibco_create_producer_ssl(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, long timetolive, int timestamp_flag, tibemsSSLParams sslParams, char *pk_password);
extern int ns_tibco_create_message(TibcoClientInfo *tci_ptr, char *data);
//extern int ns_tibco_send_message(TibcoClientInfo *tci_ptr, char *data, long timestamp, char *header, char *value);
extern int ns_tibco_set_timestamp_header(TibcoClientInfo *tci_ptr, long timestamp);
extern int ns_tibco_set_custom_header(TibcoClientInfo *tci_ptr, char *header_name, int value_type, ...);
extern int ns_tibco_send_message(TibcoClientInfo *tci_ptr, char *data); 
//extern int ns_tibco_send_message(TibcoClientInfo *tci_ptr, char *data, long timestamp);
extern int ns_tibco_close_producer(TibcoClientInfo *tci_ptr);
extern TibcoClientReceiveInfo *ns_tibco_create_consumer(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name);
extern TibcoClientReceiveInfo *ns_tibco_create_durable_consumer(char *service_url, char *username, char *password, int t_or_q, char *t_q_name, char *durableName);
extern TibcoClientReceiveInfo *ns_tibco_create_consumer_ssl
 (char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, tibemsSSLParams sslParams, char *pk_password);
extern TibcoClientReceiveInfo *ns_tibco_create_durable_consumer_ssl
 (char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, char *durableName, tibemsSSLParams sslParams, char *pk_password);
extern int ns_tibco_receive_message(TibcoClientReceiveInfo *tri_ptr, char *buff, int *break_flag);
//extern int ns_tibco_receive_message(TibcoClientReceiveInfo *tri_ptr);
extern int ns_tibco_unsubscribe_durable_consumer(TibcoClientReceiveInfo *tri_ptr, char *durableName);
extern int ns_tibco_close_consumer(TibcoClientReceiveInfo *tri_ptr);
#ifdef NS_DEBUG_ON 
extern void log_tibco_req_rep(tibemsDestination destination, tibemsMsg msg, int msg_type);
#endif
extern int ns_tibco_set_custom_header_ex(TibcoClientInfo *tci_ptr, char *header_name, int value_type, ...);
extern tibemsSSLParams ns_tibco_set_ssl_param(char *ciphers, char *private_key_file, char *hostname, char *trustedCA, char *issuer, char *identity);
#define ns_tibco_set_custom_header(tci_ptr, header_name, value_type, ...) ns_tibco_set_custom_header_ex(tci_ptr, header_name, value_type, __VA_ARGS__)
#define TRUE 1
#define FALSE 2

#define NS_TIBCO_ERROR -1

#endif
