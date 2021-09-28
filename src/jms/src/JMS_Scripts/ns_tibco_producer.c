#include <stdio.h> #include <stdlib.h>
#include <sys/time.h>

#include "ns_string.h"
#include "ns_tibco_api.h"

void ns_tibco_producer()
{
  int jpid;                                     //JMS Pool Id
  int jcid;                                     //JMS Connection Id
  char error_msg[1024 + 1];              // Error string. Must be 1024 size
  int ret;                               // Error code
  char *msg = "Test Message";            // Message to be produced
  int msg_len = strlen(msg);             // Message Length

  // Initialize Tibco producer with given parameters and connection pool of pool_size
  // It returns JMS Pool ID which need to be passed for getting connection from the pool
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // (int ns_tibco_init_producer(tibco_hostname, tibco_port, tibco_topic_or_queue, topic_or_queue_name, tibco_userId, tibco_password, max_pool_size, error_msg)) 
  if((jpid = ns_tibco_init_producer("10.10.40.18", 9092, 1, "tibco_topic", "tibco", "cavisson", 10,error_msg)) < 0)
  { 
    fprintf(stderr, "Error in initializing Tibco producer. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }

  //This api will initialize tibco ssl ciphers
  //ns_tibco_set_ssl_ciphers(jcid, ciphers, error_msg) 
  if((ret = ns_tibco_set_ssl_ciphers(jcid, "abcd", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting tibco ssl ciphers. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will initialize tibco ssl pvtKey File path 
  //(ns_tibco_set_ssl_pvt_key_file(jcid, pvtKeyFilePath, error_msg) 
  if((ret = ns_tibco_set_ssl_pvt_key_file(jcid, "abcd", error_msg)) < 0)
  { 
    fprintf(stderr, "Error in setting tibco ssl pvt key. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
  
  //This api will initialize tibco ssl trustedCA certificate file path
  //(ns_tibco_set_ssl_trusted_ca(jcid, trustedCACertFilePath, error_msg)
  if((ret = ns_tibco_set_ssl_trusted_ca(jcid, "abcd",error_msg)) < 0)
  {
    fprintf(stderr, "Error in setting tibco ssl trustedCA. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
 
  //This api will initialize tibco ssl issuer certificatefile path
  //ns_tibco_set_ssl_issuer(jcid, issuerCertFilePath, error_msg)
  if((ret = ns_tibco_set_ssl_issuer(jcid, "abcd", error_msg)) < 0)
  {                                               
    fprintf(stderr, "Error in setting tibco ssl issuer. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
 
  //This api will initialize tibco ssl identity file path and password
  //(ns_tibco_set_ssl_identity(jcid, identityFilePath, ssl_pwd, error_msg)
  if((ns_tibco_set_ssl_identity(jcid, "abcd", "password", error_msg)) < 0)
  {
    fprintf(stderr, "Error in setting tibco ssl identity. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
  
  // Get connection from the connection pool.
  // This method will return connection is free or make new connection and return it.
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // All connections are busy
  // Error in making connection
  // If actual connection is made, then transaction is started with name passed if it is not NULL or empty
  // If connection fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, TibcoProducerConnectError2012
	                                          
  if((jcid = ns_tibco_get_connection(jpid, "TibcoProducerConnect",error_msg)) < 0)
  {
    fprintf(stderr, "Error in getting Tibco connention from the pool. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }

  //This api will set any custom header in message
  //ns_tibco_set_custom_header(jcid, error_msg, header_name, value_type, header_value);
  if((ret = ns_tibco_set_message_header(jcid, error_msg, "timestamp", NS_TIBCO_STRING, "10-05-2019")) < 0)
  {
    fprintf(stderr, "Error in Setting Tibco Msg header.jcid = %d,Error code = %d, Error Msg = %s\n", jcid, ret, error_msg);
    return;
  }  

  // Put message in the JMS queue/topic using connection taken using get connection API
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // Connection closed by JMS server
  // Invalid connection ID passed
  // Transaction is started with name passed if it is not NULL or empty
  // If put fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, TibcoPutMsgError2012 
  if((ret = ns_tibco_put_msg(jcid, msg, msg_len, "TibcoPutMsg", error_msg)) < 0)
  {
    fprintf(stderr, "Error in putting message using Tibco connention. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }
  
  //This api will release the  connection from connection pool and it should be called every time so that another user can reuse it.
  if((jcid = ns_tibco_release_connection(jcid, error_msg)) < 0)
  {
    fprintf(stderr, "Error in releasing Tibco connection.Error code = %d, Error Msg = %s \n", jcid, error_msg);
    return;
  }
/*
  //This api will close connection with Tibco server and it should not be called every time as it will impact performance. It should be called at the end of test.
  if(ns_tibco_close_connection(jcid, "TibcoProducerClose") == NS_TIBCO_ERROR)
  {
    fprintf(stderr, "Error in closing Tibco  connection %d\n", jcid);
    return;
  }
  
  //This api will remove the pool size 
  if(ns_tibco_shutdown(jcid, "TibcoProducerShutdown") == NS_TIBCO_ERROR)
  {
    fprintf(stderr, "Error in shutdowning Tibco connection %d\n", jcid);
    return;
  }
*/
  // Page think time in case of adding delay for next session
  ns_page_think_time(0.0);

   return;  
}

