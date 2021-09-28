#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ns_string.h"

#define MSG_LEN 1024
void ibmmq_get()
{
  int jpid;                              //JMS Pool Id
  int jcid;                              //JMS Connection Id
  char error_msg[1024 + 1];              // Error string. Must be 1024 size
  int ret;                          // Error code
  char msg[MSG_LEN + 1];            // Message to be produced
  int msg_len = MSG_LEN;             // Message Length
 

  // Initialize IBMMQ consumer with given parameters and connection pool of pool_size
  // It returns JMS Pool ID which need to be passed for getting connection from the pool
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // int ns_ibmmq_init_consumer(ibmmq_hostname, ibmmq_port, queue_manager, channel, ibmmq_queue, ibmmq_userId, ibmmq_password, max_pool_size, error_msg)

  if((jpid = ns_ibmmq_init_consumer("10.10.30.13", 9092, "ansh_qu_mgr", "ANSH_cha", "ansh_queue", "ibmmq","cavisson", 10, error_msg)) < 0)
  {
    fprintf(stderr, "Error in initializing IBMMQ consumer. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }

  // Get connection from the connection pool.
  // This method will return connection is free or make new connection and return it.
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // All connections are busy
  // Error in making connection
  // If actual connection is made, then transaction is started with name passed if it is not NULL or empty
  // If connection fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, IbmmqConsumerConnectError2012

  if((jcid = ns_ibmmq_get_connection(jpid, "IbmmqConsumerConnect", error_msg)) < 0)
  {
    fprintf(stderr, "Error in getting IBMMQ connention from the pool. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }
  
  // Get message from the JMS queue/topic using connection taken using get connection API
  // In case of error, error code of value < 0 is returned and error_msg (max 1014) is set with error message
  // Few possible errors are:
  // Connection closed by JMS server
  // Invalid connection ID passed
  // Transaction is started with name passed if it is not NULL or empty
  // If get fails, then transaction is ended with <TxName>Error<JMSErrorCode>. For example, IbmmqGetMsgError2012

  if((ret = ns_ibmmq_get_msg(jcid, msg, msg_len, "IbmmqGetMsg", error_msg)) < 0)
  {
    fprintf(stderr, "Error in gettting message using IBMMQ connention. Error code = %d, Error Msg = %s\n", ret, error_msg);
    return;
  }

  //This api will release the Consumer connection from connection pool and it should be called every time so that another user can reuse it.
  if((jcid = ns_ibmmq_release_connection(jcid, error_msg)) < 0)
  {
    fprintf(stderr, "Error in releasing IBMMQ connection.Error code = %d, Error Msg = %s\n", jcid, error_msg);
    return;
  }
/*
 //This api will close connection with Ibmmq server and it should not be called every time as it will impact performance. It should be called at the end of test.
  if(ns_ibmmq_close_connection(jcid, "IbmmqConsumerClose") == NS_IBMMQ_ERROR)
  {
    fprintf(stderr, "Error in closing Ibmmq connection %d\n", jcid);
    return;
  }

  //This api will remove the pool size 
  if(ns_ibmmq_shutdown(jcid, "IbmmqConsumerShutdown") == NS_IBMMQ_ERROR)
  {
    fprintf(stderr, "Error in shutdowning Ibmmq connection %d\n", jcid);
    return;
  }
*/

// Page think time in case of adding delay for next session
  ns_page_think_time(0.0);

  return;
}
