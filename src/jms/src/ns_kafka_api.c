/******************************************************************************
 * Name    :    ns_kafka_api.c
 * Purpose :    This file contains Api's of Kafka producer and consumer clients
                for sending and receiving message from kafka server.
 * Author  :    Neha Rawat
 * Intial version date:    17/05/2019
 * Last modification date: 17/05/2019
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../ns_log.h"
#include "../../ns_alloc.h"
#include "../../ns_msg_def.h"
#include "ns_jms_conn_pool.h"
#include "ns_kafka_api.h"
#include "../../ns_string.h"
#include "ns_jms_error.h"
/* Added for Msg_com_con and buffer_key */
#include "../../util.h"
#include "../../netstorm.h"
#include "../../ns_vuser_thread.h"
#include "../../ns_debug_trace.h"
#include "../../ns_alloc.h"
#ifdef JMS_API_TEST
#define NSDL1_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL2_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL3_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL4_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }

int ns_start_transaction(char *transaction_name)
{
  return;
}
int ns_end_transaction(char *transaction_name, int status)
{
  return;
}
int ns_end_transaction_as(char *transaction_name, int status, char *end_tx)
{
  return;
}
#endif
extern void ns_kafka_log_req_resp(NSKafkaClientConn *ns_kaka_conn_ptr, int msg_type, char *msg, char *hdr_name, char *hdr_value);

/**
 * Error callback.
 * The error callback is used by librdkafka to signal warnings and errors
 * back to the application. 
 * See rdkafka.h for more information.
 */
static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
  NSKafkaClientConn *ns_kafka_conn_ptr = opaque;
  //printf("err = %d, reason = %s\n", err, reason);
  if(RD_KAFKA_RESP_ERR__TRANSPORT == err)
    ns_kafka_conn_ptr->is_broker_down = 1;
  else if(RD_KAFKA_RESP_ERR__SSL == err)
    ns_kafka_conn_ptr->is_ssl_error = 1;
}

/**
 * Connect callback.
 * The connect callback is responsible for connecting socket \p sockfd
 * to peer address \p addr
 * See rdkafka.h for more information.
 */
/*static int connect_cb(int sockfd, const struct sockaddr *addr, int addrlen, const char *id, void *opaque)
{
  //NSDL2_JMS(NULL, NULL, "Method called");
  NSKafkaClientConn *ns_kafka_conn_ptr = opaque;
  ns_kafka_conn_ptr->is_broker_down = 1;
}
*/
/**
 * Message delivery report callback.
 * Called once for each message.
 * See rdkafka.h for more information.
 */
static void msg_delivered(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
  NSKafkaClientConn *ns_kafka_conn_ptr = opaque;
  ns_kafka_conn_ptr->msg_delivery_flag = 1;
}

//This api will initialize connection pool of size pool_size
int ns_kafka_init_producer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *kafka_userId, char *kafka_password,
                            int max_pool_size, char *error_msg)
{
 
  NS_DT2(NULL, NULL, DM_L1, MM_JMS,"KAFKA-Initializing producer connection pool. Server ip/port: %s:%d, Kafka topic: %s, Username: %s, Password: ****,  Connection pool size: %d", kafka_hostname, kafka_port, kafka_topic, kafka_userId, max_pool_size); 
 
  NSDL2_JMS(NULL, NULL, "Method called.max_pool_size = %d", max_pool_size);
  
  int jpid;
  int is_new_key;
 
  if(!kafka_hostname || !kafka_port || !kafka_topic )
     HANDLE_ERROR_DT("KAFKA-Error in Producer connection intialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  int len = strlen(kafka_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s:%d", kafka_hostname, kafka_port);
  if(!kafka_userId)
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic,                                                                  "NA", "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else if(!kafka_password)
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic,                                                                  kafka_userId, "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic,                                                                  kafka_userId, kafka_password, "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  if(jpid < 0)
    return NS_JMS_ERROR_WRONG_USER_CONFIG;
  
  if(is_new_key)
  {
    KafkaConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(KafkaConfig));
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
  }
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Producer connection pool successfully initialized.", jpid);
  return jpid;
}


int ns_kafka_init_consumer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *consumer_group, char *kafka_userId, 
                           char *kafka_password, int max_pool_size, char *error_msg)
{
  
  NSDL2_JMS(NULL, NULL, "Method called.max_pool_size = %d", max_pool_size);
  NS_DT2(NULL, NULL, DM_L1, MM_JMS,"KAFKA-Initializing consumer connection pool. Server ip/port: %s:%d, Kafka topic: %s, Consumer_group: %s, Username: %s, Password: ****,  Connection pool size: %d", kafka_hostname, kafka_port, kafka_topic, consumer_group, kafka_userId, max_pool_size);
  int jpid;
  int is_new_key;

  if(!kafka_hostname || !kafka_port || !kafka_topic || !consumer_group)
    HANDLE_ERROR_DT("KAFKA-Error in Consumer connection initialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  int len = strlen(kafka_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s:%d", kafka_hostname, kafka_port);
  if(!kafka_userId)
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic, 
                             "NA", "NA", consumer_group, max_pool_size, CONSUMER, error_msg, &is_new_key);
  else if(!kafka_password)
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic,                                                                  kafka_userId, "NA", consumer_group, max_pool_size, CONSUMER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(kafka_hostname, kafka_port, hostname_port, "NA", "NA", kafka_topic,                                                                  kafka_userId, kafka_password, consumer_group, max_pool_size, CONSUMER, error_msg, &is_new_key);
   if(jpid < 0)
    return NS_JMS_ERROR_WRONG_USER_CONFIG;

  if(is_new_key)
  {
    KafkaConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(KafkaConfig));
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
  }
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Consumer connection pool successfully initialized.", jpid);
  return jpid;
}

int ns_kafka_set_sasl_properties(int jpid, char *sasl_mechanism, char *sasl_username,char *sasl_password, char *error_msg)
{ 
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, sasl_username = %s ", jpid, sasl_username);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting sasl properties", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting sasl properties. Sasl username: %s", jpid, sasl_username);
  if(!sasl_username || !(*sasl_username) || !sasl_mechanism || !(*sasl_mechanism))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting sasl properties", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(sasl_mechanism);
  if(len)
    NSLB_MALLOC_AND_COPY(sasl_mechanism, jms_specific_config->sasl_mechanism, len, "jms_specific_config->sasl_mechanism", -1, NULL)
  len = strlen(sasl_username);
  if(len)
    NSLB_MALLOC_AND_COPY(sasl_username, jms_specific_config->sasl_username, len, "jms_specific_config->sasl_username", -1, NULL)
  len = strlen(sasl_password);
  if(len)
    NSLB_MALLOC_AND_COPY(sasl_password, jms_specific_config->sasl_password, len, "jms_specific_config->sasl_password", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Sasl properties set successfully.", jpid);
  return 0;
}


//This api will initialize kafka security protocol
int ns_kafka_set_security_protocol(int jpid, char *security_protocol, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, security_protocol = %s ", jpid, security_protocol);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("Error in setting security protocol", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting security protocol: %s", jpid, security_protocol);
  if(!security_protocol || !(*security_protocol))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting security protocol", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(security_protocol);
  if(len)
    NSLB_MALLOC_AND_COPY(security_protocol, jms_specific_config->security_protocol, len, "jms_specific_config->security_protocol", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Security protocol set successfully.", jpid);
  return 0;
}

//SSL config apis are required only if ssl protocol is enabled for kafka
//This api will initialize kafka ssl ciphers
int ns_kafka_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, ciphers = %s", jpid, ciphers);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl ciphers", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting ssl ciphers: %s", jpid, ciphers);
  if(!ciphers || !(*ciphers))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl ciphers", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(ciphers);
  if(len)
    NSLB_MALLOC_AND_COPY(ciphers, jms_specific_config->ciphers, len, "jms_specific_config->ciphers", -1, NULL)
   NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Ssl ciphers set successfully.", jpid);
  return 0;
}

//This api will initialize kafka ssl key file path and it's password
int ns_kafka_set_ssl_key_file(int jpid, char *keyFilePath, char *keyPassword, char *error_msg)
{
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl Key file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting ssl Key file: %s", jpid, keyFilePath);
  if(!keyFilePath || !keyPassword || !(*keyFilePath) || !(*keyPassword))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl Key file", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(keyFilePath);
  if(len)
    NSLB_MALLOC_AND_COPY(keyFilePath, jms_specific_config->keyFilePath, len, "jms_specific_config->keyFilePath", -1, NULL)
  len = strlen(keyPassword);
  if(len)
    NSLB_MALLOC_AND_COPY(keyPassword, jms_specific_config->keyPassword, len, "jms_specific_config->keyPassword", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Ssl key file set successfully.", jpid);
  return 0;
}

//This api will initialize kafka ssl certificate file path
int ns_kafka_set_ssl_cert_file(int jpid, char *certificateFilePath, char *error_msg)
{
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl certificate file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting ssl certificate file path: %s", jpid, certificateFilePath);
  if(!certificateFilePath || !(*certificateFilePath))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, certificateFilePath = %s", jpid,  certificateFilePath);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(certificateFilePath);
  if(len)
    NSLB_MALLOC_AND_COPY(certificateFilePath, jms_specific_config->certificateFilePath, len, "jms_specific_config->certificateFilePath", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Ssl certificate file path set successfully.", jpid);
  return 0;
}

//This api will initialize kafka ssl ca certificate file path
int ns_kafka_set_ssl_ca_file(int jpid, char *caCertifcateFilePath, char *error_msg)
{
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl ca certificate file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting ssl ca certificate file path: %s", jpid, caCertifcateFilePath);
  if(!caCertifcateFilePath || !(*caCertifcateFilePath))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl ca certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, caCertifcateFilePath = %s", jpid, caCertifcateFilePath);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(caCertifcateFilePath);
  if(len)
    NSLB_MALLOC_AND_COPY(caCertifcateFilePath, jms_specific_config->caCertifcateFilePath, len, "jms_specific_config->caCertifcateFilePath", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Ssl ca certificate file path set successfully.", jpid);
  return 0;
}

//This api will initialize kafka ssl server certificate file path if verifiy broker is enabled
int ns_kafka_set_ssl_crl_file(int jpid, char *crlFilePath, char *error_msg)
{
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl crl file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting ssl crl file path: %s", jpid, crlFilePath);
  if(!crlFilePath || !(*crlFilePath))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting ssl crl file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, crlFilePath = %s", jpid, crlFilePath);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int len = strlen(crlFilePath);
  if(len)
    NSLB_MALLOC_AND_COPY(crlFilePath, jms_specific_config->crlFilePath, len, "jms_specific_config->crlFilePath", -1, NULL)
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Ssl crl file path set successfully.", jpid);
  return 0;
}

int ns_kafka_set_put_msg_mode(int jpid, int put_mode, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, put_mode = %d", jpid, put_mode);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting put message mode", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting put message mode: %d", jpid, put_mode);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  if(put_mode == 1)
    jms_specific_config->put_mode = put_mode; 
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Message mode set successfully", jpid);
  return 0;
}

//kafka set connection timeout
int ns_kafka_set_Connection_timeout(int jpid, double timeout, char *error_msg)
{ 
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting connection timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting connection timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->conn_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection timeout set successfully", jpid);
  return 0;
} 

//kafka set get msg timeout
int ns_kafka_set_getMsg_timeout(int jpid, double timeout, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting get message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting get message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->get_msg_timeout = timeout;
   NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Get message timeout set successfully", jpid);
  return 0;
}

// kafka set put msg timeout
int ns_kafka_set_putMsg_timeout(int jpid, double timeout, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("KAFKA-Error in setting put message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Setting put message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;             
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->put_msg_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Put message timeout set successfully", jpid);
  return 0;
}

static void ns_kafka_set_conf(KafkaConfig *jms_specific_config, NSKafkaClientConn *ns_kafka_conn_ptr)
{
  ns_kafka_conn_ptr->conf = rd_kafka_conf_new();
  //sasl mechanism can be "GSSAPI, PLAIN, SCRAM-SHA-256, SCRAM-SHA-512, OAUTHBEARER"
  if(jms_specific_config->sasl_mechanism)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "sasl.mechanisms", jms_specific_config->sasl_mechanism, NULL, 0);
  if(jms_specific_config->sasl_username)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "sasl.username", jms_specific_config->sasl_username, NULL, 0);
  if(jms_specific_config->sasl_password)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "sasl.password", jms_specific_config->sasl_password, NULL, 0);
  if(jms_specific_config->security_protocol)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "security.protocol", jms_specific_config->security_protocol, NULL, 0);
  if(jms_specific_config->ciphers)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.cipher.suites", jms_specific_config->ciphers, NULL, 0); 
  if(jms_specific_config->keyFilePath)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.key.location", jms_specific_config->keyFilePath, NULL, 0);
  if(jms_specific_config->keyPassword)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.key.password", jms_specific_config->keyPassword, NULL, 0);
  if(jms_specific_config->certificateFilePath)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.certificate.location", jms_specific_config->certificateFilePath, NULL, 0);
  if(jms_specific_config->caCertifcateFilePath)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.ca.location", jms_specific_config->caCertifcateFilePath, NULL, 0);
  if(jms_specific_config->crlFilePath)
    rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "ssl.crl.location", jms_specific_config->crlFilePath, NULL, 0);
  char tmp[50];
  snprintf(tmp, 50, "%ld", (long)(jms_specific_config->conn_timeout*1000));
  //rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "socket.blocking.max.ms", tmp, NULL, 0);
  rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "socket.timeout.ms", tmp, NULL, 0);
  snprintf(tmp, 50, "%ld", (long)(jms_specific_config->put_msg_timeout*1000));
  rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "message.timeout.ms", tmp, NULL, 0);
  snprintf(tmp, 50, "%i", SIGIO);
  rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "internal.termination.signal", tmp, NULL, 0);
  rd_kafka_conf_set_opaque(ns_kafka_conn_ptr->conf, (void *)ns_kafka_conn_ptr);
  //rd_kafka_conf_set_connect_cb(ns_kafka_conn_ptr->conf, connect_cb);
  rd_kafka_conf_set_error_cb(ns_kafka_conn_ptr->conf, error_cb);
  /* Set up a message delivery report callback.
  * It will be called once for each message, either on successful
  * delivery to broker, or upon failure to deliver to broker. */
  /* If offset reporting (-o report) is enabled, use the
   * richer dr_msg_cb instead. */
  if(jms_specific_config->put_mode)
  {
    //rd_kafka_conf_set_opaque(ns_kafka_conn_ptr->conf, (void *)ns_kafka_conn_ptr);
    rd_kafka_conf_set_dr_msg_cb(ns_kafka_conn_ptr->conf, msg_delivered);
  }
  /* Set logger */
  //rd_kafka_conf_set_log_cb(ns_kafka_conn_ptr->conf, logger);
}

static void end_error_transaction(char *transaction_name, char *operation)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  int transaction_name_len = strlen(transaction_name);
  int operation_len = 0;
  if(operation)
    operation_len = strlen(operation);
  char end_as_tx[transaction_name_len + operation_len + 6];  //MQLONG = 8
  snprintf(end_as_tx, transaction_name_len + operation_len + 6, "%s%sError", transaction_name, operation);
  ns_end_transaction_as(transaction_name, NS_REQUEST_ERRMISC, end_as_tx);
}

static int ns_kafka_make_producer_connection(NSKafkaClientConn *ns_kafka_conn_ptr, char *server_ip, char *topic_name, char *transaction_name, KafkaConfig *jms_specific_config, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method Entry. ns_make_kafka_connection()\n");
  char errstr[512];
  ns_kafka_conn_ptr->partition = RD_KAFKA_PARTITION_UA;
  if(transaction_name)
    ns_start_transaction(transaction_name);
  ns_kafka_conn_ptr->topic_conf = rd_kafka_topic_conf_new();
  ns_kafka_set_conf(jms_specific_config, ns_kafka_conn_ptr);

  /* Create Kafka handle */
  if (!(ns_kafka_conn_ptr->rk = rd_kafka_new(RD_KAFKA_PRODUCER, ns_kafka_conn_ptr->conf, errstr, sizeof(errstr))))
  {
    NSDL1_JMS(NULL, NULL, "Error: Failed to create new producer: %s\n", errstr);
    if(transaction_name)
      end_error_transaction(transaction_name, "CreateNewProducer");
    HANDLE_ERROR_DT("KAFKA-Error in making producer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, errstr);
  }

  /* Add brokers */
  if (rd_kafka_brokers_add(ns_kafka_conn_ptr->rk, server_ip) == 0)
  {
    NSDL1_JMS(NULL, NULL, "Error: No valid brokers specified\n");
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    HANDLE_ERROR_DT("KAFKA-Error in making producer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");
  }

  /* Create topic */
  ns_kafka_conn_ptr->rkt = rd_kafka_topic_new(ns_kafka_conn_ptr->rk, topic_name, ns_kafka_conn_ptr->topic_conf);
  ns_kafka_conn_ptr->topic_conf = NULL; /* Now owned by topic */
  strcpy(ns_kafka_conn_ptr->server_ip, server_ip);
  strcpy(ns_kafka_conn_ptr->topic_name, topic_name);
  //Commenting poll as it is slowing down the producer by 300 sec.
  rd_kafka_poll(ns_kafka_conn_ptr->rk, 1000);

  NSDL1_JMS(NULL, NULL, "ns_kafka_conn_ptr->is_broker_down = %d\n", ns_kafka_conn_ptr->is_broker_down);
  if(ns_kafka_conn_ptr->is_broker_down)
  {
    ns_kafka_conn_ptr->is_broker_down = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    HANDLE_ERROR_DT("KAFKA-Error in connecting broker", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");
  }
  if(ns_kafka_conn_ptr->is_ssl_error)
  {
    ns_kafka_conn_ptr->is_ssl_error = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidCert");
    HANDLE_ERROR_DT("KAFKA-Error in ssl certificate", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: Invalid certificate");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

static int ns_kafka_make_consumer_connection(NSKafkaClientConn *ns_kafka_conn_ptr, char *server_ip, char *consumer_group_name, char *topic_name, char *transaction_name, KafkaConfig *jms_specific_config, char *error_msg)
{
  NSDL1_JMS(NULL, NULL, "Method Entry. ns_make_kafka_connection()\n");
  char errstr[512];
  rd_kafka_resp_err_t err;
  ns_kafka_set_conf(jms_specific_config, ns_kafka_conn_ptr);
  if(transaction_name)
    ns_start_transaction(transaction_name);
  ns_kafka_conn_ptr->topic_conf = rd_kafka_topic_conf_new();
  /* Consumer groups require a group id */
  char *group = "rdkafka_consumer_example";

  if(consumer_group_name)
    group = consumer_group_name;
  if (rd_kafka_conf_set(ns_kafka_conn_ptr->conf, "group.id", group, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
  {
    NSDL1_JMS(NULL, NULL, "%% %s\n", errstr);
    if(transaction_name)
      end_error_transaction(transaction_name, "ConfSetFail");
    HANDLE_ERROR_DT("KAFKA-Error in making consumer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, errstr);
  }
  /* Consumer groups always use broker based offset storage */
  /*if (rd_kafka_topic_conf_set(ns_kafka_conn_ptr->topic_conf, "offset.store.method", "broker", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
  {
    NSDL1_JMS(NULL, NULL, "%% %s\n", errstr);
    if(transaction_name)
      end_error_transaction(transaction_name, "TopicConfSetFail");
    HANDLE_ERROR_DT("KAFKA-Error in making consumer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, errstr);
  }*/
  /* Set default topic config for pattern-matched topics. */
  rd_kafka_conf_set_default_topic_conf(ns_kafka_conn_ptr->conf, ns_kafka_conn_ptr->topic_conf);

  /* Callback called on partition assignment changes */
  //rd_kafka_conf_set_rebalance_cb(ns_kafka_conn_ptr->conf, rebalance_cb);
  if (!(ns_kafka_conn_ptr->rk = rd_kafka_new(RD_KAFKA_CONSUMER, ns_kafka_conn_ptr->conf, errstr, sizeof(errstr))))
  {
    NSDL1_JMS(NULL, NULL, "Error: Failed to create new consumer: %s\n", errstr);
    if(transaction_name)
      end_error_transaction(transaction_name, "CreateNewConsumer");
    HANDLE_ERROR_DT("KAFKA-Error in making producer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, errstr);
  }
  /* Add brokers */
  if (rd_kafka_brokers_add(ns_kafka_conn_ptr->rk, server_ip) == 0)
  {
    NSDL1_JMS(NULL, NULL, "Error: No valid brokers specified\n");
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    HANDLE_ERROR_DT("KAFKA-Error in making producer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");
  }
  rd_kafka_poll_set_consumer(ns_kafka_conn_ptr->rk);
  ns_kafka_conn_ptr->topics = rd_kafka_topic_partition_list_new(1);
  int32_t partition = -1;
  rd_kafka_topic_partition_list_add(ns_kafka_conn_ptr->topics, topic_name, partition);

  NSDL1_JMS(NULL, NULL, "%% Subscribing to %d topics\n", ns_kafka_conn_ptr->topics->cnt);
  if ((err = rd_kafka_subscribe(ns_kafka_conn_ptr->rk, ns_kafka_conn_ptr->topics)))
  {
    NSDL1_JMS(NULL, NULL, "%% Failed to start consuming topics: %s\n", rd_kafka_err2str(err));
    if(transaction_name)
      end_error_transaction(transaction_name, "Subscribe");
    HANDLE_ERROR_DT("KAFKA-Error in making producer connection", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, rd_kafka_err2str(err));
  }
  strcpy(ns_kafka_conn_ptr->server_ip, server_ip);
  strcpy(ns_kafka_conn_ptr->topic_name, topic_name);

  rd_kafka_poll(ns_kafka_conn_ptr->rk, 1000);
  NSDL1_JMS(NULL, NULL, "ns_kafka_conn_ptr->is_broker_down = %d\n", ns_kafka_conn_ptr->is_broker_down);
  if(ns_kafka_conn_ptr->is_broker_down)
  { 
    ns_kafka_conn_ptr->is_broker_down = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    HANDLE_ERROR_DT("KAFKA-Error in connecting broker", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");
  }
  if(ns_kafka_conn_ptr->is_ssl_error)
  {
    ns_kafka_conn_ptr->is_ssl_error = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidCert");
    HANDLE_ERROR_DT("KAFKA-Error in ssl certificate", error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: Invalid certificate");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will get the connection from connection pool and make it with Kafka server if not already made
int ns_kafka_get_connection(int jpid, char *transaction_name, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d", jpid);
  if(-1 == jms_cp_validate_jpid(jpid))
  {
    start_end_tx_on_error(transaction_name, "InvalidPoolId");
    HANDLE_ERROR_DT_JPID("KAFKA-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Creating connection with server.", jpid);
  int jpcid;
  JMSConnection *node = NULL;
  jpcid = jms_cp_get_connection(jpid, &node);
  NSDL4_JMS(NULL, NULL, "jpcid = %d", jpcid);
  if(jpcid >= 0)
  {
    NSDL4_JMS(NULL, NULL, "valid jpcid node = %p, node->conn_config = %p", node,node ? node->conn_config:NULL  );
    if(node && !(node->conn_config))
    {
      NSDL4_JMS(NULL, NULL, "Going to malloc node->conn_config");
      jms_cp_set_conn_specific_config(jpcid, sizeof(NSKafkaClientConn));
      JMSConfig *user_config = jms_cp_get_user_config(jpid);
      KafkaConfig *jms_specific_config = user_config->jms_specific_config;
      if(jms_cp_get_client_type(jpid, -1) == PRODUCER)
      {
        if(NS_JMS_ERROR_LIB_ERROR_CONN_FAIL == ns_kafka_make_producer_connection((NSKafkaClientConn *)(node->conn_config), user_config->hostname_port, user_config->queue, transaction_name, jms_specific_config, error_msg))
        {
          jms_cp_send_conn_to_nc_list(jpid, node);
          //start_end_tx_on_error(transaction_name, "LibConnFail");
          NS_DT4(NULL, NULL, DM_L1, MM_JMS,"KAFKA-Pool id: %d, Error: %s", jpid, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG); 
          return NS_JMS_ERROR_LIB_ERROR_CONN_FAIL;
        }
      }
      else
      {
        if(NS_JMS_ERROR_LIB_ERROR_CONN_FAIL == ns_kafka_make_consumer_connection((NSKafkaClientConn *)(node->conn_config), user_config->hostname_port, user_config->consumer_group, user_config->queue, transaction_name, jms_specific_config, error_msg))
        {
          jms_cp_send_conn_to_nc_list(jpid, node);
          //start_end_tx_on_error(transaction_name, "LibConnFail");
          NS_DT4(NULL, NULL, DM_L1, MM_JMS,"KAFKA-Pool id: %d, Error: %s", jpid, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG);
          return NS_JMS_ERROR_LIB_ERROR_CONN_FAIL;
        }
      }
    }
  }
  else
  {
    start_end_tx_on_error(transaction_name, "PoolFinished");
    HANDLE_ERROR_DT_JPID("KAFKA-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_CONN_POOL_FINISHED, NS_JMS_ERROR_CONN_POOL_FINISHED_MSG);
  }

  NS_JMS_SET_OWNER(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Connection successfully created.", jpid, GET_JCID(jpcid));
  return jpcid;
}

//This api will set any custom header in message
int ns_kafka_set_message_header(int jpcid, char *error_msg, char *header_name, char *header_value)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpcid = %d, header_name = %s, header_value = %s", jpcid, header_name, header_value);
  int ret;
  if((ret = jms_cp_validate_jpcid(jpcid, NULL, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%s", GET_JPID(jpcid), GET_JCID(jpcid), header_name, header_value);
  rd_kafka_resp_err_t err;
  if(!header_name || !(*header_name))
  {
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in setting message header", jpcid, error_msg, NS_JMS_ERROR_HEADER_NAME_NULL, NS_JMS_ERROR_HEADER_NAME_NULL_MSG);
   
  }
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  NSKafkaClientConn *ns_kafka_conn_ptr = (NSKafkaClientConn *)(node->conn_config);
  if (!ns_kafka_conn_ptr->hdrs)
    ns_kafka_conn_ptr->hdrs = rd_kafka_headers_new(8);
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->is_hdr_added)
  {
    err = rd_kafka_header_add(ns_kafka_conn_ptr->hdrs, header_name, -1, header_value, -1);
    if (ret) 
    {
      HANDLE_ERROR_DT_JPCID("KAFKA-Error in setting message header", jpcid, error_msg, NS_JMS_ERROR_HDR_ADD_FAIL, rd_kafka_err2str(err));
    }
  }
  #ifdef NS_DEBUG_ON
    ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "KAFKA", NULL, 0, NULL, header_name, header_value, jpcid); 
   // ns_kafka_log_req_resp(ns_kafka_conn_ptr, KAFKA_SENT_MSG, NULL, header_name, header_value);
  #endif
 NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Message header set successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

static int ns_kafka_produce_msg(int jpcid, NSKafkaClientConn *ns_kafka_conn_ptr, char *msg, int msg_len, char *key, int key_len, char *transaction_name, char *error_msg)
{
  rd_kafka_resp_err_t err = 0;
  int jpid = GET_JPID(jpcid);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if(ns_kafka_conn_ptr->is_broker_down)
  {
    ns_kafka_conn_ptr->is_broker_down = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    ns_kakfa_close_connection(jpcid, "KAFKAProducerClosePutError", error_msg);
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in connecting broker", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");
  }
  /* Send/Produce message. */
  if (ns_kafka_conn_ptr->hdrs) 
  {
    rd_kafka_headers_t *hdrs_copy;
    jms_specific_config->is_hdr_added = 1;
    hdrs_copy = rd_kafka_headers_copy(ns_kafka_conn_ptr->hdrs);
    err = rd_kafka_producev(
            ns_kafka_conn_ptr->rk,
            RD_KAFKA_V_RKT(ns_kafka_conn_ptr->rkt),
            RD_KAFKA_V_PARTITION(ns_kafka_conn_ptr->partition),
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            RD_KAFKA_V_VALUE(msg, msg_len),
            RD_KAFKA_V_KEY(key, key_len),
            RD_KAFKA_V_HEADERS(hdrs_copy),
            RD_KAFKA_V_END);
    if(err)
      rd_kafka_headers_destroy(hdrs_copy);
  } 
  else 
  {
    if (rd_kafka_produce(ns_kafka_conn_ptr->rkt, ns_kafka_conn_ptr->partition, RD_KAFKA_MSG_F_COPY,
                            /* Payload and length */
                            msg, msg_len,
                            /* Optional key and its length */
                            key, key_len,
                            /* Message opaque, provided in
                             * delivery report callback as
                             * msg_opaque. */
                            NULL))
    {
      err = rd_kafka_last_error();
    }
  }
  if(err)
  {
    NSDL1_JMS(NULL, NULL, "%% Failed to produce to topic %s\n"
                    "partition %i\n", rd_kafka_topic_name(ns_kafka_conn_ptr->rkt), ns_kafka_conn_ptr->partition);
    /* Poll to handle delivery reports */
    //rd_kafka_poll(ns_kafka_conn_ptr->rk, 0);
    if(transaction_name)
      end_error_transaction(transaction_name, "ProducerPut");
    if(err != RD_KAFKA_RESP_ERR__QUEUE_FULL)
      ns_kakfa_close_connection(jpcid, "KAFKAProducerClosePutError", error_msg);
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, rd_kafka_err2str(err));
  }
  if(jms_specific_config->put_mode)
  {
    //Commenting poll as it is slowing down the producer by 300 sec.
    rd_kafka_poll(ns_kafka_conn_ptr->rk, 0);
    int run = 1;
    /* Wait for messages to be delivered */
    while (run && rd_kafka_outq_len(ns_kafka_conn_ptr->rk) > 0)
      rd_kafka_poll(ns_kafka_conn_ptr->rk, 10);
    
    if(!ns_kafka_conn_ptr->msg_delivery_flag)
    {
      if(transaction_name)
        end_error_transaction(transaction_name, "msg_delivery");
      HANDLE_ERROR_DT_JPCID("KAFKA-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, "kafka_msg_delivery_ERROR");
    }
    else
      ns_kafka_conn_ptr->msg_delivery_flag = 0;
  }
 
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  #ifdef NS_DEBUG_ON
    if(key && *key)
    {
      NSDL1_JMS(NULL,NULL, "key value=[%s]",key);
      NS_DT3(NULL, NULL, DM_L1, MM_JMS, "MSG_KEY = [%s]",key);
      ns_jms_logs_req_resp(user_config->hostname_port, JMS_SENT_MSG, "KAFKA", user_config->queue, 1, NULL, "key", key, jpcid);
    }
    ns_jms_logs_req_resp(user_config->hostname_port, JMS_SENT_MSG, "KAFKA", user_config->queue, 1, msg, NULL, NULL, jpcid);
  #endif
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Message was put successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}


//This api will put message on kafka server
int ns_kafka_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg)
{
  return ns_kafka_put_msg_v2(jpcid, msg, msg_len, NULL, 0, transaction_name, error_msg);
}

int ns_kafka_put_msg_v2(int jpcid, char *msg, int msg_len, char *key, int key_len, char *transaction_name, char *error_msg)
{
  int ret;
  //Validate jpcid
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d, msg = %s, msg_len = %d, key = %s, key_len = %d", jpcid, msg, msg_len, key, key_len);
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
  
#ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Putting message to topic: %s", jpid, GET_JCID(jpcid), user_config->queue);
#endif

  if(!msg || !(*msg))
  {
    start_end_tx_on_error(transaction_name, "PutMessageNull");
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }

  // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != PRODUCER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG);
  }
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  return(ns_kafka_produce_msg(jpcid, (NSKafkaClientConn *)(node->conn_config), msg, msg_len, key, key_len, transaction_name, error_msg));
}

static int ns_kafka_get_header(NSKafkaClientConn *ns_kafka_conn_ptr, rd_kafka_message_t *rkmessage, char *header, int hdr_len, int jpcid)
{
  NSDL4_JMS(NULL, NULL, "Method Entry");
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Getting message header.", GET_JPID(jpcid), GET_JCID(jpcid));
  
  if (!rd_kafka_message_headers(rkmessage, &(ns_kafka_conn_ptr->hdrs))) 
  { 
    size_t idx = 0;
    const char *name;
    const void *val;
    size_t size;
    int header_len = 0;
    while (!rd_kafka_header_get_all(ns_kafka_conn_ptr->hdrs, idx++, &name, &val, &size)) 
    {
      NSDL4_JMS(NULL, NULL, "%s%s=", idx == 1 ? " " : ", ", name);
      if (val)
      {
        char buf[size+1];
        NSDL4_JMS(NULL, NULL, "\"%.*s\"", (int)size, (const char *)val);
        snprintf(buf, size+1, "%s", (const char *)val);
        header_len += snprintf(header + header_len, hdr_len - header_len, "%s:%s\n", name, buf);
        #ifdef NS_DEBUG_ON
          ns_jms_logs_req_resp(NULL, JMS_RECV_MSG, "KAFKA", NULL, 0, NULL, (char *)name,buf,0);
        #endif
      }
      else
      {
        NSDL4_JMS(NULL, NULL, "NULL");
        header_len += snprintf(header + header_len, hdr_len - header_len, "%s:NULL\n", name);
        #ifdef NS_DEBUG_ON
          ns_jms_logs_req_resp(NULL, JMS_RECV_MSG, "KAFKA", NULL, 0, NULL, (char *)name,NULL,0);
        #endif
      }
    }
  }
 NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Message header get successfully", GET_JPID(jpcid), GET_JCID(jpcid));
 return 0;
}

int ns_kafka_consume_msg(int jpcid, NSKafkaClientConn *ns_kafka_conn_ptr, char *msg, int get_msg_len, char *header, int hdr_len, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method Entry. ns_kafka_consume_msg()");
  rd_kafka_message_t *rkmessage;
  int jpid = GET_JPID(jpcid);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  int run = 1;
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if(ns_kafka_conn_ptr->is_broker_down)
  {
    ns_kafka_conn_ptr->is_broker_down = 0;
    if(transaction_name)
      end_error_transaction(transaction_name, "InvalidBroker");
    ns_kakfa_close_connection(jpcid, "KAFKAProducerClosePutError", error_msg);
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in connecting broker", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error: No valid brokers specified");  
  }
  while(run)
  {
    rkmessage = rd_kafka_consumer_poll(ns_kafka_conn_ptr->rk, (int)(jms_specific_config->get_msg_timeout * 1000));
    NSDL4_JMS(NULL, NULL, "rkmessage = %s", rkmessage);
    if (rkmessage)
    {
      if (rkmessage->err)
      {
        if (rkmessage->err == RD_KAFKA_RESP_ERR__TIMED_OUT)
        {
          NSDL1_JMS(NULL, NULL, "%% Consumer reached end of %s [%"PRId32"]" "message queue at offset %"PRId64"\n",
                           rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, rkmessage->offset);
          rd_kafka_message_destroy(rkmessage);
          if(transaction_name)
            end_error_transaction(transaction_name, "ConsumeMsgTimeout");
          ns_kakfa_close_connection(jpcid, "KAFKAConsumerCloseGetError", error_msg);
          HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "kafka_partition_ERROR");
        }
        if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
        {
          NSDL1_JMS(NULL, NULL, "%% Consumer reached end of %s [%"PRId32"]" "message queue at offset %"PRId64"\n",
                           rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, rkmessage->offset);
          rd_kafka_message_destroy(rkmessage);
          if(transaction_name)
            end_error_transaction(transaction_name, "ConsumerPartition");
          ns_kakfa_close_connection(jpcid, "KAFKAConsumerCloseGetError", error_msg);
          HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "kafka_partition_ERROR");
        }
        if (rkmessage->rkt)
        {
          NSDL1_JMS(NULL, NULL, "%% Consume error for topic \"%s\" [%"PRId32"] " "offset %"PRId64": %s\n",
                           rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                           rkmessage->offset, rd_kafka_message_errstr(rkmessage));
          rd_kafka_message_destroy(rkmessage);
          if(transaction_name)
            end_error_transaction(transaction_name, "ConsumerTopic");
          ns_kakfa_close_connection(jpcid, "KAFKAConsumerCloseGetError", error_msg);
          HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "kafka_msg_consume_ERROR");
        }
        else
        {
          NSDL1_JMS(NULL, NULL, "%% Consumer error: %s: %s\n",
                           rd_kafka_err2str(rkmessage->err), rd_kafka_message_errstr(rkmessage));
          rd_kafka_message_destroy(rkmessage);
          if(transaction_name)
            end_error_transaction(transaction_name, "ConsumerTopic");
          ns_kakfa_close_connection(jpcid, "KAFKAConsumerCloseGetError", error_msg);
          HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, rd_kafka_message_errstr(rkmessage));
        }
        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION || rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
        {
          NSDL1_JMS(NULL, NULL, "UNKNOWN_PARTITION or UNKNOWN_TOPIC");
          rd_kafka_message_destroy(rkmessage);
          if(transaction_name)
            end_error_transaction(transaction_name, "UnknownPartitionTopic");
          ns_kakfa_close_connection(jpcid, "KAFKAConsumerCloseGetError", error_msg);
          HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, rd_kafka_message_errstr(rkmessage));
        }
      }
      else
      {
        NSDL1_JMS(NULL, NULL, "Message = %.*s\n", (int)rkmessage->len, (char *)rkmessage->payload);
        if((int)rkmessage->len > get_msg_len)
          snprintf(msg, get_msg_len+1, "%s", (char *)rkmessage->payload);
        else
          snprintf(msg, (int)rkmessage->len+1, "%s", (char *)rkmessage->payload);
       
        if(header)
          ns_kafka_get_header(ns_kafka_conn_ptr, rkmessage, header, hdr_len, jpcid); 
        #ifdef NS_DEBUG_ON
          static char *key;
          NS_DT3(NULL, NULL, DM_L1, MM_JMS, "getting this MSG_KEY = [%s]",(char *)rkmessage->key);
          if((char *)rkmessage->key && *(char *)rkmessage->key)
          {
            MY_MALLOC_AND_MEMSET(key, (int)rkmessage->key_len+1, "key_size", -1);
            snprintf(key, (int)rkmessage->key_len+1, "%s", (char *)rkmessage->key);

            NSDL1_JMS(NULL,NULL, "key value=[%s]",key);
            NS_DT3(NULL, NULL, DM_L1, MM_JMS, "MSG_KEY = [%s]",key);
            ns_jms_logs_req_resp(user_config->hostname_port, JMS_RECV_MSG, "KAFKA", user_config->queue, 1, NULL, "key", key, jpcid);
            FREE_AND_MAKE_NULL(key, "key", -1);
          }
          ns_jms_logs_req_resp(user_config->hostname_port, JMS_RECV_MSG, "KAFKA", user_config->queue, 1, msg, NULL, NULL, jpcid); 
        #endif
        run = 0;
        if(transaction_name)
          ns_end_transaction(transaction_name, NS_AUTO_STATUS);
      }
      rd_kafka_message_destroy(rkmessage);
    }
    else
    {
      NSDL1_JMS(NULL, NULL, "There is no Messgae on\n");
      if(transaction_name)
        end_error_transaction(transaction_name, "NoMsg");
      HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "There is no Message");
    }
    return 0;
  }
   NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Message was get successfully", GET_JPID(jpcid), GET_JCID(jpcid));
   return 0;
}

//This api will get message on kafka server
int ns_kafka_get_msg(int jpcid, char *msg, int msg_len, char *header, int hdr_len,  char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d, msg = %s, msg_len = %d", jpcid, msg, msg_len);
  int ret;

  //Validate jpcid
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
#ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));

  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Getting message from topic: %s", jpid, GET_JCID(jpid), user_config->queue);
#endif
  if(!msg)
  {
    start_end_tx_on_error(transaction_name, "GetMsgBufPtrNull");
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }


  // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != CONSUMER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("KAFKA-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG);
  }

  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  return(ns_kafka_consume_msg(jpcid, (NSKafkaClientConn *)(node->conn_config), msg, msg_len, header, hdr_len, transaction_name, error_msg));
}

//This api will release the  connection from connection pool and it should be called every time so that another user can reuse it.
int ns_kakfa_release_connection(int jpcid, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  int ret;
  if((ret = jms_cp_validate_jpcid(jpcid, NULL, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAKFA-Pool id: %d, Connection id: %d, Releasing connection.", GET_JPID(jpcid), GET_JCID(jpcid));

  jms_cp_release_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Connection released successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

int ns_kakfa_close_conn(int jpcid, NSKafkaClientConn *ns_kafka_conn_ptr, char *transaction_name)
{
  NSDL4_JMS(NULL, NULL, "Method called");
  if(transaction_name)
    ns_start_transaction(transaction_name);

  /* Destroy topic */
  if(ns_kafka_conn_ptr->rkt)
    rd_kafka_topic_destroy(ns_kafka_conn_ptr->rkt);

  if((jms_cp_get_client_type(-1, jpcid)) != PRODUCER)
  {
    if(ns_kafka_conn_ptr->rk)
      rd_kafka_consumer_close(ns_kafka_conn_ptr->rk);
    if(ns_kafka_conn_ptr->topics)
      rd_kafka_topic_partition_list_destroy(ns_kafka_conn_ptr->topics);
  }
  else
  {
    if(ns_kafka_conn_ptr->hdrs)
      rd_kafka_headers_destroy(ns_kafka_conn_ptr->hdrs);
    ns_kafka_conn_ptr->hdrs = NULL;
  }
  /* Destroy the handle */
  if(ns_kafka_conn_ptr->rk)
    rd_kafka_destroy(ns_kafka_conn_ptr->rk);
  //not required as rd_kafka_destroy frees topic_conf
  //if(ns_kafka_conn_ptr->topic_conf)
    //rd_kafka_topic_conf_destroy(ns_kafka_conn_ptr->topic_conf);
  /* Let background threads clean up and terminate cleanly. */
  /*int run = 100;
  while(run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
    NSDL4_JMS(NULL, NULL, "Waiting for librdkafka to decommission\n");
  if(run <= 0)
    rd_kafka_dump(stdout, ns_kafka_conn_ptr->rk);*/

  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will close connection with Kafka server and it should not be called every time as it will impact performance. It should be called at the end of test.
int ns_kakfa_close_connection(int jpcid, char *transaction_name, char *error_msg)
{
  int ret, jpid;
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  if((ret = jms_cp_validate_jpcid_for_close_conn(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Closing connection.", GET_JPID(jpcid), GET_JCID(jpcid));
  
  jpid = GET_JPID(jpcid);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  KafkaConfig *jms_specific_config = user_config->jms_specific_config;
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  jms_specific_config->is_hdr_added = 0;
  ns_kakfa_close_conn(jpcid, (NSKafkaClientConn *)node->conn_config, transaction_name);
  jms_cp_close_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "KAFKA-Pool id: %d, Connection id: %d, Connection closed successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;  
}

//This api will remove the pool size 
int ns_kakfa_shutdown(int jpcid, char *transaction_name)
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  return 0;
}

#ifdef JMS_API_TEST

/*
To compile program :
--------------------------
gcc -DJMS_API_TEST -o kafka_test  -I./../../libnscore -I/usr/include/postgresql/ -I/usr/include/libxml2 -g ns_jms_conn_pool.c ns_kafka_api.c ../../libnscore/nslb_alloc.c ../../libnscore/nslb_mem_map.c ../../libnscore/nslb_get_norm_obj_id.c ../../libnscore/nslb_mem_pool.c ../../libnscore/nslb_util.c ../../libnscore/nslb_big_buf.c /home/cavisson/work/Master_work/cavisson/src/thirdparty/libkafka/lib/librdkafka.so.1 -I/home/cavisson/work/Master_work/cavisson/src/thirdparty/nghttp2/lib/includes -lpthread -lm 2>err
export LD_LIBRARY_PATH=/home/cavisson/work/Master_work/cavisson/src/thirdparty/libkafka/lib

*/

#define MAX_DATA_LINE_LENGTH 1024
int main(int argc, char *argv[])
{
  char error_buff[1024 + 1];
  int pid;
  int cid;
  char get_msg[1024 + 1];
  char buff[MAX_DATA_LINE_LENGTH + 1];

  FILE *fp_conf_file;
  char conf_file_path[1024] = "";
  char host[1024] = "";
  char topic_name[1024] = "";
  char consumer_group[1024] = "";
  char user_name[1024] = "";
  char password[1024] = "";
  char security_protocol[1024] = "";
  char ciphers[1024] = "";
  char key_file_path[1024] = "";
  char key_password[1024] = "";
  char cert_file_path[1024] = "";
  char ca_cert_file_path[1024] = "";
  char crl_file_path[1024] = "";
  char tx_name[1024] = "";
  int get_or_put = 1;
  int port;
  char opt;
  int num;
  char keyword[1024];
  char text1[1024];
  char text2[1024]; 

  while((opt = getopt(argc, argv, "f:")) != -1)
  {
    switch(opt)
    {
      case 'f':
        strcpy(conf_file_path, optarg);
        break;
      case '?':
        fprintf(stderr, "Error: Invalid arguments\n");
        break;
    }
  }
 
  fp_conf_file = fopen(conf_file_path, "r");
  if(fp_conf_file == NULL)
  {
    fprintf(stderr, "Unable to open configuration file.\n");
    return 0;
  }

  while(fgets(buff, MAX_DATA_LINE_LENGTH, fp_conf_file) != NULL)
  {
    buff[strlen(buff) - 1] = '\0';  
    if(strchr(buff, '#') || buff[0] == '\0')
      continue;
  
    if ((num = (sscanf(buff, "%s %s %s", keyword, text1, text2))) < 2)
      continue;

    if(strncmp(keyword, "HOST", 4) == 0) 
      strcpy(host, text1);
  
    if(strncmp(keyword, "PORT", 4) == 0) 
      port = atoi(text1);

    if(strncmp(keyword, "TOPIC", 5) == 0) 
      strcpy(topic_name, text1);

    if(strncmp(keyword, "CONSUMER", 8) == 0) 
      strcpy(consumer_group, text1);

    if(strncmp(keyword, "USER", 4) == 0) 
      strcpy(user_name, text1);

    if(strncmp(keyword, "PASSWORD", 8) == 0) 
      strcpy(password, text1);

    if(strncmp(keyword, "SECURITY_PROTOCOL", 17) == 0) 
      strcpy(security_protocol, text1);

    if(strncmp(keyword, "CIPHERS", 7) == 0) 
      strcpy(ciphers, text1);

    if(strncmp(keyword, "KEY_FILE_PATH", 13) == 0) 
      strcpy(key_file_path, text1);

    if(strncmp(keyword, "KEY_PASSWORD", 12) == 0) 
      strcpy(key_password, text1);

    if(strncmp(keyword, "CERT_FILE_PATH", 14) == 0) 
      strcpy(cert_file_path, text1);

    if(strncmp(keyword, "CA_CERT_FILE_PATH", 17) == 0) 
      strcpy(ca_cert_file_path, text1);

    if(strncmp(keyword, "CRL_FILE_PATH", 13) == 0) 
      strcpy(crl_file_path, text1);

    if(strncmp(keyword, "TX_NAME", 7) == 0) 
      strcpy(tx_name, text1);

    //if(strncmp(keyword, "MSG_FILE", 4) == 0) 
      //strcpy(topic_name, text1);

    if(strncmp(keyword, "GET_OR_PUT", 10) == 0) 
      get_or_put = atoi(text1);

  }
  fclose(fp_conf_file);

  if(get_or_put != 2)
    pid = ns_kafka_init_consumer(host, port, topic_name, consumer_group, user_name, password, 10, error_buff);
  else
    pid = ns_kafka_init_producer(host, port, topic_name, user_name, password, 10, error_buff);
  
  if(security_protocol)
    ns_kafka_set_security_protocol(pid, security_protocol,error_buff);
  
  if(ciphers)
    ns_kafka_set_ssl_ciphers(pid, ciphers, error_buff);
 
  if(key_file_path && key_password)
    ns_kafka_set_ssl_key_file(pid, key_file_path, key_password, error_buff);

  if(cert_file_path)
    ns_kafka_set_ssl_cert_file(pid, cert_file_path, error_buff);
  
  if(ca_cert_file_path)
    ns_kafka_set_ssl_ca_file(pid, ca_cert_file_path, error_buff);

  if(crl_file_path)
    ns_kafka_set_ssl_crl_file(pid, crl_file_path, error_buff);

  cid =ns_kafka_get_connection(pid, tx_name, error_buff);

  if(get_or_put == 2) 
    ns_kafka_put_msg(cid, "put kafka message", 1024, tx_name, error_buff);
  else
    ns_kafka_get_msg(cid, get_msg, 1024, tx_name, error_buff);  

  ns_kakfa_release_connection(cid, error_buff);

  return 0;
}

#endif
