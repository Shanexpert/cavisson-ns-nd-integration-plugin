/******************************************************************************
 * Name    :    ns_tibco_api.h
 * Purpose :    This header file contains description of Data Structure for
                ns_tibco_api.c file .
 * Author  :    Neha Rawat
 * Intial version date:    20/05/2019
 * Last modification date: 20/05/2019
 ******************************************************************************/


#ifndef NS_TIBCO_API_H
#define NS_TIBCO_API_H

#include "tibems.h"

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

typedef struct
{
  tibemsSSLParams sslParams;
  char            set_default_conf;
  char            is_msg_created;
  char            *ssl_pwd;
  char            is_verify_host;
  int             t_or_q;
  long            timetolive;
  int             timestamp_flag;
  int		  put_mode;
  double	  conn_timeout;
  double 	  get_msg_timeout;
  double	  put_msg_timeout;
}TibcoConfig;

typedef union {
  int int_val;
  char char_val;
  short short_val;
  float float_val;
  long long_val;
  double double_val;
  char *string_val;
}headerValue;

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
#define NS_TIBCO_SHORT 17
#define NS_TIBCO_STRING 18

extern int ns_tibco_init_producer(char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                                 char *tibco_password, int max_pool_size, char *error_msg);
extern int ns_tibco_init_consumer( char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                                 char *tibco_password, int max_pool_size, char *error_msg);
extern int ns_tibco_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg);
extern int ns_tibco_set_ssl_pvt_key_file(int jpid, char *pvtKeyFilePath, char *error_msg);
extern int ns_tibco_set_ssl_trusted_ca(int jpid, char *trustedCACertFilePath, char *error_msg);
extern int ns_tibco_set_ssl_issuer(int jpid, char *issuerCertFilePath, char *error_msg);
extern int ns_tibco_set_ssl_identity(int jpid, char *identityFilePath, char *ssl_pwd, char *error_msg);
extern int ns_tibco_get_connection(int jpid,  char *transaction_name, char *error_msg);
extern int ns_tibco_set_custom_header(int jpcid, char *error_msg, char *header_name, int value_type, ...);
extern int ns_tibco_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg );
extern int ns_tibco_get_msg(int jpcid, char *msg, int msg_len, char *header, int header_len, char *transaction_name, char *error_msg);
extern int ns_tibco_release_connection(int jpcid, char *error_msg);
int ns_tibco_close_connection(int jpcid,  char *transaction_name, char *error_msg);

#endif
