#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ns_string.h"

void kafka_put()
{
  int jpid;                              //JMS Pool Id
  int jcid;                              //JMS Connection Id
  char error_msg[1024 + 1];              // Error string. Must be 1024 size
  int ret;                               // Error code
  char *msg = "Test Message";            // Message to be produced
  int msg_len = strlen(msg);             // Message Length

  // Initialize KAFKA producer with given parameters and connection pool of pool_size
  // It returns JMS Pool ID which need to be passed for getting connection from the pool
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // ((int ns_kafka_init_producer(kafka_hostname, kafka_port, kafka_topic, kafka_userId, kafka_password, max_pool_size, error_msg)) 
  if((jpid = ns_kafka_init_producer("10.10.40.18", 9092,"kaf_topic", "kafka", "cavisson", 10, error_msg)) < 0)
  {
    fprintf(stderr, "Error in initializing Kafka producer. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }

  //This api will initialize kafka security protocol
  //ns_kafka_set_security_protocol(jpid, security_protocol, error_msg)
  if((ret = ns_kafka_set_security_protocol(jpid, "ssl_plaintext", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting Kafka security protocol. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //SSL config apis are required only if ssl protocol is enabled for kafka
  //This api will initialize kafka ssl ciphers
  //ns_kafka_set_ssl_ciphers(key, ciphers, error_msg)
  if((ret = ns_kafka_set_ssl_ciphers(jpid, "abcd", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting Kafka ssl ciphers. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will initialize kafka ssl key file path and it's password
  //ns_kafka_set_ssl_keyFile(key, keyFilePath, keyPassword, error_msg)
  if((ret = ns_kafka_set_ssl_key_file(jpid, "abcd","password", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting Kafka ssl key.Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will initialize kafka ssl certificate file path
  //ns_kafka_set_ssl_certFile(key, certificateFilePath, error_msg)
  if((ret = ns_kafka_set_ssl_cert_file(jpid, "abcd", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting Kafka ssl certificate. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
  
  //This api will initialize kafka ssl ca certificate file path
  //ns_kafka_set_ssl_caFile(key, caCertifcateFilePath, error_msg)
  if((ret = ns_kafka_set_ssl_caFile(jpid, "abcd", error_msg)) < 0)
  {
    fprintf(stderr, "Error in setting Kafka ssl ca certificate.Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will initialize kafka ssl server certificate file path if verifiy broker is enabled
  //ns_kafka_set_ssl_crlFile(key, crlFilePath, error_msg)
  if((ret = ns_kafka_set_ssl_crlFile(jpid, "abcd", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting Kafka ssl certificate.Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  // Get connection from the connection pool.
  // This method will return connection is free or make new connection and return it.
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // All connections are busy
  // Error in making connection
  // If actual connection is made, then transaction is started with name passed if it is not NULL or empty
  // If connection fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, kafkaProducerConnectError2012

  if((jcid = ns_kafka_get_connection(jpid, "KafkaProducerConnect",error_msg)) < 0)
  {
    fprintf(stderr, "Error in getting Kafka connention from the pool. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }


  // Put message in the JMS queue/topic using connection taken using get connection API
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // Connection closed by JMS server
  // Invalid connection ID passed
  // Transaction is started with name passed if it is not NULL or empty
  // If put fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, kafkaPutMsgError2012

  if((ret = ns_kafka_put_msg(jcid, msg, msg_len, "KafkaPutMsg",error_msg)) < 0)
  {
    fprintf(stderr, "Error in putting message using Kafka connention. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will release the Producer connection from connection pool and it should be called every time so that another user can reuse it.
  if((jcid = ns_kakfa_release_connection(jcid,error_msg)) < 0)
  {
    fprintf(stderr, "Error in releasing Kafka connection.Error code = %d, Error Msg = %s \n", jcid, error_msg);
    return;
  }
/*
  //This api will close connection with Kafka server and it should not be called every time as it will impact performance. It should be called at the end of test.
  if(ns_kakfa_close_connection(jcid, "KafkaProducerClose") == NS_KAFKA_ERROR)
  {
    fprintf(stderr, "Error in closing Kafka connection %d\n", jcid);
    return;
  }

  //This api will remove the pool size 
  if(ns_kakfa_shutdown(jcid, "KafkaProducerShutdown") == NS_KAFKA_ERROR)
  {
    fprintf(stderr, "Error in shutdowning Kafka connection %d\n", jcid);
    return;
  }
 */
  // Page think time in case of adding delay for next session
  ns_page_think_time(0.0);

  return;
}
