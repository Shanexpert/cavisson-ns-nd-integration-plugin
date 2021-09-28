/******************************************************************************
 * Name    :    ns_kafka_api.h
 * Purpose :    This header file contains description of Data Structure for
                ns_kafka_api.c file .
 * Author  :    Neha Rawat
 * Intial version date:    20/05/2019
 * Last modification date: 20/05/2019
 ******************************************************************************/


#ifndef NS_KAFKA_API_H
#define NS_KAFKA_API_H

#include "rdkafka.h"


typedef struct
{
  rd_kafka_topic_t                *rkt;
  rd_kafka_topic_conf_t           *topic_conf;
  rd_kafka_t                      *rk;
  rd_kafka_topic_partition_list_t *topics;
  rd_kafka_conf_t                 *conf;
  int                             partition;
  rd_kafka_headers_t              *hdrs;
  char			          server_ip[64];
  char			          topic_name[255];
  char                            msg_delivery_flag;
  char                            is_broker_down;
  char                            is_ssl_error;
}NSKafkaClientConn;

typedef struct
{
  char                    *sasl_mechanism;
  char                    *sasl_username;
  char                    *sasl_password;
  char                    *security_protocol;
  char                    *ciphers;
  char                    *keyFilePath;
  char                    *keyPassword;
  char                    *certificateFilePath;
  char                    *caCertifcateFilePath;
  char                    *crlFilePath;
  char                    is_hdr_added;
  int                     put_mode;
  double                  conn_timeout;
  double		  get_msg_timeout; 
  double		  put_msg_timeout;
}KafkaConfig;

int ns_kafka_init_producer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *kafka_userId, char *kafka_password,
                            int max_pool_size, char *error_msg);
int ns_kafka_init_consumer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *consumer_group, char *kafka_userId,
                           char *kafka_password, int max_pool_size, char *error_msg);
int ns_kafka_set_security_protocol(int jpid, char *security_protocol, char *error_msg);
int ns_kafka_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg);
int ns_kafka_set_ssl_key_file(int jpid, char *keyFilePath, char *keyPassword, char *error_msg);
int ns_kafka_set_ssl_cert_file(int jpid, char *certificateFilePath, char *error_msg);
int ns_kafka_set_ssl_ca_file(int jpid, char *caCertifcateFilePath, char *error_msg);
int ns_kafka_set_ssl_crl_file(int jpid, char *crlFilePath, char *error_msg);
int ns_kafka_set_put_msg_mode(int jpid, int put_mode, char *error_msg);
int ns_kafka_get_connection(int jpid, char *transaction_name, char *error_msg);
int ns_kafka_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
int ns_kafka_put_msg_v2(int jpcid, char *msg, int msg_len, char *key, int key_len, char *transaction_name, char *error_msg);
int ns_kafka_get_msg(int jpcid, char *msg, int msg_len, char *header, int hdr_len,  char *transaction_name, char *error_msg);
int ns_kakfa_release_connection(int jcid, char *error_msg);
int ns_kakfa_close_connection(int jpcid, char *transaction_name, char *error_msg);

#endif
