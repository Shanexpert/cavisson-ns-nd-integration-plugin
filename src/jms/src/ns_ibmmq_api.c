/******************************************************************************
 * Name    :    ns_ibmmq_api.c
 * Purpose :    This file contains Api's of IBMMQ producer and consumer clients
                for sending and receiving message from IBMMQ server.
 * Author  :    Vaibhav Mittal
 * Intial version date:    16/05/2019
 * Last modification date: 16/05/2019
 ******************************************************************************/


#include <stdio.h>
#include <string.h>
#include "../../ns_log.h"
#include "../../ns_alloc.h"
#include "../../ns_msg_def.h"
#include "ns_jms_conn_pool.h"
#include "ns_ibmmq_api.h"
#include "../../ns_string.h"
#include "ns_jms_error.h"
/* Added for Msg_com_con and buffer_key */
#include "../../util.h"
#include "../../netstorm.h"
#include "../../ns_vuser_thread.h"
#include "../../ns_debug_trace.h"
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


int ns_ibmmq_init_producer(char *ibmmq_hostname, int ibmmq_port, char *queue_manager, char *channel, 
                            char *ibmmq_queue, char *ibmmq_userId,  char *ibmmq_password, int max_pool_size, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method calling.max_pool_size = %d\n", max_pool_size);
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Initializing producer connection pool. Server ip/port: %s:%d, Queue manager: %s, Channel: %s, Queue: %s, Username: %s, Password: ****, Connection pool size: %d", ibmmq_hostname, ibmmq_port, queue_manager, channel, ibmmq_queue, ibmmq_userId, max_pool_size);
  int jpid;
  int is_new_key;
 
  if(!ibmmq_hostname || !ibmmq_port || !queue_manager || !channel || !ibmmq_queue)
  {
    HANDLE_ERROR_DT("IBMMQ-Error in producer connection pool initialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  }

  int len = strlen(ibmmq_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s(%d)", ibmmq_hostname, ibmmq_port); 
  if(!ibmmq_userId)
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, "NA", "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else if(!ibmmq_password)
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, ibmmq_userId, "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, ibmmq_userId, ibmmq_password, "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);

  if(is_new_key)
  {
    IbmmqConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(IbmmqConfig));
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
  }
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Producer connection pool successfully initialized.", jpid); 
  return jpid; 
}  

int ns_ibmmq_init_consumer(char *ibmmq_hostname, int ibmmq_port,  char *queue_manager,  char *channel,                                                           char *ibmmq_queue, char *ibmmq_userId, char *ibmmq_password, int max_pool_size, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method calling.max_pool_size = %d\n", max_pool_size);
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Initializing consumer connection pool. Server ip/port: %s:%d, Queue manager: %s, Channel: %s, Queue: %s, Username: %s, Password: ****, Connection pool size: %d", ibmmq_hostname, ibmmq_port, queue_manager, channel, ibmmq_queue, ibmmq_userId, max_pool_size);
  
  int jpid;
  int is_new_key;

  if(!ibmmq_hostname || !ibmmq_port || !queue_manager || !channel || !ibmmq_queue)
    HANDLE_ERROR_DT("IBMMQ-Error in consumer connection pool initialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  int len = strlen(ibmmq_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s(%d)", ibmmq_hostname, ibmmq_port);
  if(!ibmmq_userId)
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, "NA", "NA", "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);
  else if(!ibmmq_password)
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, ibmmq_userId, "NA", "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(ibmmq_hostname, ibmmq_port, hostname_port, queue_manager, channel,
                     ibmmq_queue, ibmmq_userId, ibmmq_password, "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);

  if(is_new_key)
  {
    IbmmqConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(IbmmqConfig));
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
  }
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Consumer connection pool successfully initialized.", jpid); 
  return jpid;
}  

int ns_ibmmq_set_put_msg_mode(int jpid, int put_mode, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, put_mode = %d", jpid, put_mode);
  if(-1 == jms_cp_validate_jpid(jpid))
  {
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in setting put message mode", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Setting put message mode: %d", jpid, put_mode);

  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  IbmmqConfig *jms_specific_config = user_config->jms_specific_config;
  if(put_mode == 1)
    jms_specific_config->put_mode = put_mode;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Message mode set successfully",jpid); 
  return 0;
}

int ns_ibmmq_set_Connection_timeout(int jpid, double timeout, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in setting connection timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Setting connection timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  IbmmqConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->conn_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection timeout set successfully", jpid);
  return 0;
}

int ns_ibmmq_set_putMsg_timeout(int jpid, double timeout, char *error_msg)
{ 
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in setting put message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Setting put message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  IbmmqConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->put_msg_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBBMQ-Pool id: %d, Put message timeout set successfully", jpid);
  return 0;
} 

int ns_ibmmq_set_getMsg_timeout(int jpid, double timeout, char *error_msg)
{ 
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in setting get message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Setting get message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  IbmmqConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->get_msg_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBBMQ-Pool id: %d, Get message timeout set successfully", jpid);
  return 0;
}  

static void end_ibmmq_error_transaction(char *transaction_name, char *operation, MQLONG reason_code)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  int transaction_name_len = strlen(transaction_name);
  int operation_len = strlen(operation);
  char end_as_tx[transaction_name_len+operation_len+10];  //MQLONG = int
  snprintf(end_as_tx, transaction_name_len+operation_len+10, "%s%sError%d", transaction_name, operation, reason_code);
  ns_end_transaction_as(transaction_name, NS_REQUEST_ERRMISC, end_as_tx);
}

static int ns_ibmmq_make_connection(IBMMQ_Conn_Info *ibmmq_conn_ptr, IbmmqConfig *jms_specific_config, char *ibmmq_queue_manager, char *channel_name, char *queue_or_topic_name, char *server_ip, char *transaction_name)
{
  NSDL4_JMS(NULL, NULL, "Method calling ibmmq_queue_manager = %s, channel_name = %s, queue_or_topic_name = %s, server_ip = %s", ibmmq_queue_manager, channel_name,queue_or_topic_name,server_ip );
  static MQCD     ClientConn = {MQCD_CLIENT_CONN_DEFAULT}; /*Client connection channel*/ /*add by Abhi*/
  /*   Declare MQI structures needed                                */
  static MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
  static MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
  static MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
  static MQCNO   cno = {MQCNO_DEFAULT};   /* connection options            */
  static MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */
  
  memcpy((void *)(&ibmmq_conn_ptr->ClientConn),(void *) &ClientConn, sizeof(ClientConn));
  memcpy((void *)(&ibmmq_conn_ptr->cno),(void *) &cno, sizeof(cno));
  memcpy((void *)(&ibmmq_conn_ptr->od),(void *) &od, sizeof(od));
  memcpy((void *)(&ibmmq_conn_ptr->md),(void *) &md, sizeof(md));
  memcpy((void *)(&ibmmq_conn_ptr->pmo),(void *) &pmo, sizeof(pmo));
  memcpy((void *)(&ibmmq_conn_ptr->gmo),(void *) &gmo, sizeof(gmo));

  /*------------------------------ Handle the Connection IP -----------------------------*/
  {
    strncpy(ibmmq_conn_ptr->ClientConn.ConnectionName, server_ip, MQ_CONN_NAME_LENGTH);
    strncpy(ibmmq_conn_ptr->ClientConn.ChannelName, channel_name, MQ_CHANNEL_NAME_LENGTH);
    /* Point the MQCNO to the client connection definition */
    ibmmq_conn_ptr->cno.ClientConnPtr = &ibmmq_conn_ptr->ClientConn;

#ifndef JMS_API_TEST
    if(ISCALLER_NVM_THREAD)
      ibmmq_conn_ptr->cno.Options = MQCNO_HANDLE_SHARE_NO_BLOCK;
#endif

    /* Client connection fields are in the version 2 part of the
        MQCNO so we must set the version number to 2 or they will
        be ignored */
    ibmmq_conn_ptr->cno.Version = MQCNO_VERSION_2;
    NSDL4_JMS(NULL, NULL, "Using the server connection channel %s on connection name %s.\n",
                       ibmmq_conn_ptr->ClientConn.ChannelName, ibmmq_conn_ptr->ClientConn.ConnectionName);

  }

  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/
  if(transaction_name)
    ns_start_transaction(transaction_name);
  //TODO: Configure timeout
  MQCONNX(ibmmq_queue_manager,                 // queue manager         
         &ibmmq_conn_ptr->cno,                  // connection options  
         &ibmmq_conn_ptr->Hcon,                 // connection handle   
         &ibmmq_conn_ptr->CompCode,             // completion code      
         &ibmmq_conn_ptr->CReason);             // reason code            

  /* report reason and stop if it failed     */
  if (ibmmq_conn_ptr->CompCode == MQCC_FAILED)
  {
    NSDL4_JMS(NULL, NULL, "MQCONNX ended with reason code %d\n", ibmmq_conn_ptr->CReason);
    if(transaction_name)
      end_ibmmq_error_transaction(transaction_name, "", ibmmq_conn_ptr->CReason);
    return NS_JMS_ERROR_LIB_ERROR_CONN_FAIL;
  }

  /* if there was a warning report the cause and continue */
  if (ibmmq_conn_ptr->CompCode == MQCC_WARNING)
  {
    NSDL4_JMS(NULL, NULL, "MQCONNX generated a warning with reason code %d\nContinue ...", ibmmq_conn_ptr->CReason);
  }

  /******************************************************************/
  /*                                                                */
  /*   Use parameter as the name of the target queue                */
  /*                                                                */
  /******************************************************************/
  strncpy(ibmmq_conn_ptr->od.ObjectName, queue_or_topic_name, (size_t)MQ_Q_NAME_LENGTH);
  NSDL4_JMS(NULL, NULL, "target queue is %s\n", ibmmq_conn_ptr->od.ObjectName);
  ibmmq_conn_ptr->CompCode = ibmmq_conn_ptr->OpenCode;        // use MQOPEN result for initial test 

  memcpy(ibmmq_conn_ptr->md.Format,                          // character string format 
         MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);
  
  if(jms_specific_config->put_mode)
    ibmmq_conn_ptr->pmo.Options = MQPMO_NO_SYNCPOINT
                              | MQPMO_FAIL_IF_QUIESCING
                              | MQPMO_SYNC_RESPONSE;
  else
    ibmmq_conn_ptr->pmo.Options = MQPMO_NO_SYNCPOINT
                              | MQPMO_FAIL_IF_QUIESCING
                              | MQPMO_ASYNC_RESPONSE;

  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will get the connection from connection pool and make it with Ibmmq server if not already made
int ns_ibmmq_get_connection(int jpid, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  if(-1 == jms_cp_validate_jpid(jpid))
  {
    start_end_tx_on_error(transaction_name, "InvalidPoolId"); 
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Creating connection with server.", jpid);
  int jpcid;
  JMSConnection *node = NULL;
  jpcid = jms_cp_get_connection(jpid, &node);
  NSDL4_JMS(NULL, NULL, "jpcid = %d", jpcid);
  if(jpcid >= 0) // Got connection from free pool or not connected pool
  {
    NSDL4_JMS(NULL, NULL, "valid jpcid node = %p, node->conn_config = %p", node,node ? node->conn_config:NULL  );
    if(node && !(node->conn_config)) // Not connected
    {
      NSDL4_JMS(NULL, NULL, "Going to malloc node->conn_config");
      jms_cp_set_conn_specific_config(jpcid, sizeof(IBMMQ_Conn_Info));
      JMSConfig *user_config = jms_cp_get_user_config(jpid); 
      if(NS_JMS_ERROR_LIB_ERROR_CONN_FAIL == ns_ibmmq_make_connection((IBMMQ_Conn_Info *)(node->conn_config), user_config->jms_specific_config, user_config->queue_manager, user_config-> channel, user_config->queue, user_config->hostname_port, transaction_name))
      {
        jms_cp_send_conn_to_nc_list(jpid, node);
        //start_end_tx_on_error(transaction_name, "LibConnFail");
        HANDLE_ERROR_DT_JPID("IBMMQ-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG);
      }
    } 
    else
    {
      NSDL4_JMS(NULL, NULL, "Returning connection from free list");
    }
  }
  else  // pool is exhausted
  {
    start_end_tx_on_error(transaction_name, "PoolFinished"); 
    HANDLE_ERROR_DT_JPID("IBMMQ-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_CONN_POOL_FINISHED, NS_JMS_ERROR_CONN_POOL_FINISHED_MSG);
   
  }
   // Here we come for success case
  NS_JMS_SET_OWNER(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Connection successfully created.", jpid, GET_JCID(jpcid));
  
  return jpcid;
}

//This api will release the Consumer connection from connection pool and it should be called every time so that another user can reuse it.
int ns_ibmmq_release_connection(int jpcid, char *error_msg)
{
  int ret;

  NSDL4_JMS(NULL, NULL, "Method calling");
  if((ret = jms_cp_validate_jpcid(jpcid, NULL, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Releasing connection.", GET_JPID(jpcid), GET_JCID(jpcid));

  jms_cp_release_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Connection released successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

int ns_ibmmq_put(int jpcid, IBMMQ_Conn_Info *ibmmq_conn_ptr, char *put_msg, int msg_len, char *transaction_name, char *error_msg)
{
  MQLONG messlen = msg_len;
  NSDL4_JMS(NULL, NULL, "Method calling");
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if (ibmmq_conn_ptr->CompCode != MQCC_FAILED)
  {
    if (messlen > 0)
    {
      /**************************************************************/
      /* The following statement is not required if the             */
      /* MQPMO_NEW_MSG_ID option is used.                           */
      /**************************************************************/
      ibmmq_conn_ptr->O_options = MQOO_OUTPUT              /* open queue for output     */
                                | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
                                ;                        /* = 0x2010 = 8208 decimal   */
      MQOPEN(ibmmq_conn_ptr->Hcon,                      /* connection handle            */
             &ibmmq_conn_ptr->od,                       /* object descriptor for queue  */
             ibmmq_conn_ptr->O_options,                 /* open options                 */
             &ibmmq_conn_ptr->Hobj,                     /* object handle                */
             &ibmmq_conn_ptr->OpenCode,                 /* MQOPEN completion code       */
             &ibmmq_conn_ptr->Reason);                  /* reason code                  */

      if (ibmmq_conn_ptr->Reason != MQRC_NONE)
      {
        if(transaction_name)
          end_ibmmq_error_transaction(transaction_name, "OpenQ", ibmmq_conn_ptr->Reason);
        ns_ibmmq_close_connection(jpcid, "IBMMQProducerClosePutError", error_msg);
        HANDLE_ERROR_DT_JPCID("IBMMQ-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL, NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL_MSG);
      }
      memcpy(ibmmq_conn_ptr->md.MsgId,           /* reset MsgId to get a new one    */
             MQMI_NONE, sizeof(ibmmq_conn_ptr->md.MsgId) );

      MQPUT(ibmmq_conn_ptr->Hcon,                /* connection handle               */
            ibmmq_conn_ptr->Hobj,                /* object handle                   */
            &ibmmq_conn_ptr->md,                 /* message descriptor              */
            &ibmmq_conn_ptr->pmo,                /* default options (datagram)      */
            messlen,                            /* message length                  */
            put_msg,                            /* message buffer                  */
            &ibmmq_conn_ptr->CompCode,           /* completion code                 */
            &ibmmq_conn_ptr->Reason);            /* reason code                     */
      /* report reason, if any */
      if (ibmmq_conn_ptr->Reason != MQRC_NONE)
      {
        if(transaction_name)
          end_ibmmq_error_transaction(transaction_name, "", ibmmq_conn_ptr->Reason);
        if(ibmmq_conn_ptr->Reason != MQRC_Q_FULL)
          ns_ibmmq_close_connection(jpcid, "IBMMQProducerClosePutError", error_msg);
        HANDLE_ERROR_DT_JPCID("IBMMQ-Error in putting message",jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL_MSG);
      }
      #ifdef NS_DEBUG_ON
          int jpid = GET_JPID(jpcid);
          JMSConfig *user_config = jms_cp_get_user_config(jpid);
          ns_jms_logs_req_resp(user_config->hostname_port, JMS_SENT_MSG, "IBMMQ", user_config->queue, 2, put_msg, NULL, NULL, jpcid);
      #endif 
    MQCLOSE(ibmmq_conn_ptr->Hcon,                   /* connection handle            */
            &ibmmq_conn_ptr->Hobj,                  /* object handle                */
            ibmmq_conn_ptr->C_options,
            &ibmmq_conn_ptr->CompCode,              /* completion code              */
            &ibmmq_conn_ptr->Reason);               /* reason code                  */
        /* report reason, if any     */
      if (ibmmq_conn_ptr->Reason != MQRC_NONE)
      {
        if(transaction_name)
          end_ibmmq_error_transaction(transaction_name, "CloseQ", ibmmq_conn_ptr->Reason);
        ns_ibmmq_close_connection(jpcid, "IBMMQProducerClosePutError", error_msg);
        HANDLE_ERROR_DT_JPCID("IBMMQ-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL, NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL_MSG);
      }
    }
    else   /* satisfy end condition when empty line is read */
      ibmmq_conn_ptr->CompCode = MQCC_FAILED;
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Message was put successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

//This api will put message on Ibmmq server
int ns_ibmmq_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg) 
{
  int ret;

  //Validate jpcid
  NSDL4_JMS(NULL, NULL, "Method calling jpcid = %d", jpcid);
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
#ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Putting message to queue: %s", jpid, GET_JCID(jpcid), user_config->queue);
#endif
  if(!msg || !(*msg))
  {
    start_end_tx_on_error(transaction_name, "PutMessageNull");
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }
 
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
   // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != PRODUCER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG);
    
  }
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  return(ns_ibmmq_put(jpcid, (IBMMQ_Conn_Info *)(node->conn_config), msg, msg_len, transaction_name, error_msg)); 
}

int ns_ibmmq_get(int jpcid, IBMMQ_Conn_Info *ibmmq_conn_ptr, char *get_msg, int get_msg_len, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  /******************************************************************/
  /*                                                                */
  /*   Get messages from the message queue                          */
  /*   Loop until there is a failure                                */
  /*                                                                */
  /******************************************************************/
  if(transaction_name)
    ns_start_transaction(transaction_name);
  ibmmq_conn_ptr->O_options = MQOO_INPUT_AS_Q_DEF    /* open queue for input      */
                             | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
                             ;                        /* = 0x2001 = 8193 decimal   */

  MQOPEN(ibmmq_conn_ptr->Hcon,                      /* connection handle            */
         &ibmmq_conn_ptr->od,                       /* object descriptor for queue  */
         ibmmq_conn_ptr->O_options,                 /* open options                 */
         &ibmmq_conn_ptr->Hobj,                     /* object handle                */
         &ibmmq_conn_ptr->OpenCode,                 /* MQOPEN completion code       */
         &ibmmq_conn_ptr->Reason);                  /* reason code                  */

  /* report reason, if any; stop if failed      */
  if (ibmmq_conn_ptr->Reason != MQRC_NONE)
  {
    if(transaction_name)
      end_ibmmq_error_transaction(transaction_name, "OpenQ", ibmmq_conn_ptr->Reason);
    ns_ibmmq_close_connection(jpcid, "IBMMQConsumerCloseGetError", error_msg);
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL, NS_JMS_ERROR_LIB_ERR_OPEN_QUEUE_FAIL_MSG);
  }
  if (ibmmq_conn_ptr->OpenCode == MQCC_FAILED)
    NSDL4_JMS(NULL, NULL, "unable to open queue for output\n");
  ibmmq_conn_ptr->CompCode = ibmmq_conn_ptr->OpenCode;
  //ibmmq_conn_ptr->gmo.Options = MQGMO_NO_WAIT           /* wait for new messages       */
  ibmmq_conn_ptr->gmo.Options = MQGMO_WAIT
                                | MQGMO_NO_SYNCPOINT   /* no transaction              */
                                | MQGMO_CONVERT        /* convert if necessary        */
                                | MQGMO_ACCEPT_TRUNCATED_MSG;
  int jpid = GET_JPID(jpcid);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  IbmmqConfig *jms_specific_config = user_config->jms_specific_config;
  ibmmq_conn_ptr->gmo.WaitInterval = (int)(jms_specific_config->get_msg_timeout * 1000);
  ibmmq_conn_ptr->buflen = get_msg_len - 1;
  ibmmq_conn_ptr->messlen = 0;
  memcpy(ibmmq_conn_ptr->md.MsgId, MQMI_NONE, sizeof(ibmmq_conn_ptr->md.MsgId));
  memcpy(ibmmq_conn_ptr->md.CorrelId, MQCI_NONE, sizeof(ibmmq_conn_ptr->md.CorrelId));
  ibmmq_conn_ptr->md.Encoding       = MQENC_NATIVE;
  ibmmq_conn_ptr->md.CodedCharSetId = MQCCSI_Q_MGR;
  MQGET(ibmmq_conn_ptr->Hcon,                /* connection handle                 */
        ibmmq_conn_ptr->Hobj,                /* object handle                     */
        &ibmmq_conn_ptr->md,                 /* message descriptor                */
        &ibmmq_conn_ptr->gmo,                /* get message options               */
        ibmmq_conn_ptr->buflen,              /* buffer length                     */
        get_msg,                            /* message buffer                    */
        &ibmmq_conn_ptr->messlen,            /* message length                    */
        &ibmmq_conn_ptr->CompCode,           /* completion code                   */
        &ibmmq_conn_ptr->Reason);            /* reason code                       */

  NSDL4_JMS(NULL, NULL, "Reason = %d, CompCode = %d\n", ibmmq_conn_ptr->Reason, ibmmq_conn_ptr->CompCode);
  /* report reason, if any     */
  if(ibmmq_conn_ptr->CompCode == MQCC_FAILED)
  {
    if(transaction_name)
      end_ibmmq_error_transaction(transaction_name, "", ibmmq_conn_ptr->Reason);
    if(ibmmq_conn_ptr->Reason != MQRC_NO_MSG_AVAILABLE)
          ns_ibmmq_close_connection(jpcid, "IBMMQConsumerCloseGetError", error_msg);
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL_MSG);
  }
  else
  {
    get_msg[get_msg_len] = '\0';
    NSDL4_JMS(NULL, NULL, "Received Message is '%s'\n", get_msg);
    #ifdef NS_DEBUG_ON
      ns_jms_logs_req_resp(user_config->hostname_port, JMS_RECV_MSG, "IBMMQ", user_config->queue, 2, get_msg, NULL, NULL, jpcid);
    #endif
  }
  MQCLOSE(ibmmq_conn_ptr->Hcon,                   /* connection handle            */
            &ibmmq_conn_ptr->Hobj,                  /* object handle                */
            ibmmq_conn_ptr->C_options,
            &ibmmq_conn_ptr->CompCode,              /* completion code              */
            &ibmmq_conn_ptr->Reason);               /* reason code                  */
    /* report reason, if any     */
  if (ibmmq_conn_ptr->Reason != MQRC_NONE)
  { 
    if(transaction_name)
      end_ibmmq_error_transaction(transaction_name, "CloseQ", ibmmq_conn_ptr->Reason);
    ns_ibmmq_close_connection(jpcid, "IBMMQConsumerCloseGetError", error_msg);
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL, NS_JMS_ERROR_LIB_ERR_CLOSE_QUEUE_FAIL_MSG);
  } 
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Message was Get successfully", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

//This api will get message from Ibmmq server
int ns_ibmmq_get_msg( int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg)
{
  int ret;
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret); 
#ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));
    
  NSDL4_JMS(NULL, NULL, "Method calling");
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ-Pool id: %d, Connection id: %d, Getting message from queue: %s", jpid, GET_JCID(jpid), user_config->queue);
#endif
  if(!msg)
  {
    start_end_tx_on_error(transaction_name, "GetMessageNull");
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }
    
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
   // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != CONSUMER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("IBMMQ-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG);
    
  }
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  
  return(ns_ibmmq_get(jpcid, (IBMMQ_Conn_Info *)(node->conn_config), msg, msg_len, transaction_name, error_msg));  
}

int ns_ibmmq_close_conn(IBMMQ_Conn_Info *ibmmq_conn_ptr, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  /******************************************************************/
  /*                                                                */
  /*   Close the target queue (if it was opened)                    */
  /*                                                                */
  /******************************************************************/
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if (ibmmq_conn_ptr->OpenCode != MQCC_FAILED)
  { 
    ibmmq_conn_ptr->C_options = MQCO_NONE;        /* no close options             */
    MQCLOSE(ibmmq_conn_ptr->Hcon,                   /* connection handle            */
            &ibmmq_conn_ptr->Hobj,                  /* object handle                */
            ibmmq_conn_ptr->C_options,              
            &ibmmq_conn_ptr->CompCode,              /* completion code              */
            &ibmmq_conn_ptr->Reason);               /* reason code                  */
    /* report reason, if any     */
    if (ibmmq_conn_ptr->Reason != MQRC_NONE)
    {
      NSDL4_JMS(NULL, NULL, "MQCLOSE ended with reason code %d\n", ibmmq_conn_ptr->Reason);
      if(transaction_name)
        end_ibmmq_error_transaction(transaction_name, "CloseFail", ibmmq_conn_ptr->Reason);
      HANDLE_ERROR_DT("IBMMQ-Error in closing connection", error_msg, NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL, NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL_MSG);
    }
  }
  /******************************************************************/
  /*                                                                */
  /*   Disconnect from MQM if not already connected                 */
  /*                                                                */
  /******************************************************************/
  if (ibmmq_conn_ptr->CReason != MQRC_ALREADY_CONNECTED)
  {
    MQDISC(&ibmmq_conn_ptr->Hcon,                   /* connection handle            */
           &ibmmq_conn_ptr->CompCode,               /* completion code              */
           &ibmmq_conn_ptr->Reason);                /* reason code                  */

    /* report reason, if any     */
    if (ibmmq_conn_ptr->Reason != MQRC_NONE)
    {
      NSDL4_JMS(NULL, NULL, "MQDISC ended with reason code %d\n",ibmmq_conn_ptr->Reason);
      if(transaction_name)
        end_ibmmq_error_transaction(transaction_name, "CloseDiscFail", ibmmq_conn_ptr->Reason);
      HANDLE_ERROR_DT("IBMMQ-Error in closing connection", error_msg, NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL, NS_JMS_ERROR_LIB_ERR_CLOSE_FAIL_MSG);
    }
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will close connection with Ibmmq server and it should not be called every time as it will impact performance. It should be called at the end of test.
int ns_ibmmq_close_connection(int jpcid, char *transaction_name, char *error_msg) 
{ 
  int ret; 
  NSDL4_JMS(NULL, NULL, "Method calling");
  if((ret = jms_cp_validate_jpcid_for_close_conn(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ - Pool id: %d, Connection id: %d, Closing connection.", GET_JPID(jpcid), GET_JCID(jpcid));

  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid

  ns_ibmmq_close_conn((IBMMQ_Conn_Info *)node->conn_config, transaction_name, error_msg);
  jms_cp_close_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "IBMMQ - Pool id: %d, Connection id: %d, Connection closed successfully..", GET_JPID(jpcid), GET_JCID(jpcid));
  
  return 0;
}

//This api will remove the pool size 
int ns_ibmmq_shutdown(int jpcid, char *transaction_name, char *error_msg) 
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  return 0;
}


#ifdef JMS_API_TEST

/*
 
To compile program :
-------------------------
gcc -DJMS_API_TEST -o kafka_test  -I./../../libnscore -I/usr/include/postgresql/ -I/usr/include/libxml2 -g ns_jms_conn_pool.c ns_kafka_api.c ../../libnscore/nslb_alloc.c ../../libnscore/nslb_mem_map.c ../../libnscore/nslb_get_norm_obj_id.c ../../libnscore/nslb_mem_pool.c ../../libnscore/nslb_util.c ../../libnscore/nslb_big_buf.c -lmqic -lmqe -L /home/cavisson/work/Master_work/cavisson/src/thirdparty/libibmmq64/lib64  -lpthread -lm 2>err

export LD_LIBRARY_PATH=/home/cavisson/work/Master_work/cavisson/src/thirdparty/libibmmq64/lib64

*/


int main()
{
  char *test = "DummyValue"; 

  FILE *fp_conf_file;
  char conf_file_path[1024] = "";
  char host[1024] = "";
  char queue_topic_name[1024] = "";
  char queue_manager[1024] = "";
  char channel[1024] = "";
  char user_name[1024] = "";
  char password[1024] = "";
  char tx_name[1024] = "";

  int get_or_put = 1;
  int port;
  char opt;
  int num;
  char keyword[1024];
  char text1[1024];
  char text2[1024];

  int jpid;                              // JMS Pool Id
  int jcid;                              // JMS Connection Id
  char error_msg[1024 + 1];              // Error string. Must be 1024 size
  int ret;                               // Error code
  char *msg = "Test Message";            // Message to be produced
  int msg_len = strlen(msg);             // Message Length

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

    if(strncmp(keyword, "Q_MANAGER", 9) == 0)
      strcpy(queue_manager, text1);

    if(strncmp(keyword, "CHANNEL", 7) == 0)
      strcpy(channel, text1);

    if(strncmp(keyword, "QUEUE_TOPIC", 11) == 0)
    {
      if(atoi(text1) == 1)
        strcpy(queue_topic_name, text2);
      else if(atoi(text1) == 2)
        strcpy(queue_topic_name, text2);
      else
        fprintf(stderr, "Error : 1. Queue, 2. Topic");
    }
 
    if(strncmp(keyword, "USER", 4) == 0)
      strcpy(user_name, text1);
     
    if(strncmp(keyword, "PASSWORD", 8) == 0)
      strcpy(password, text1);

    if(strncmp(keyword, "TX_NAME", 7) == 0)
      strcpy(tx_name, text1);

    if(strncmp(keyword, "GET_OR_PUT", 10) == 0)
      get_or_put = atoi(text1);
  }
  fclose(fp_conf_file);

  pthread_setspecific(buffer_key, test);

  if(get_or_put == 2)
  {
    if((jpid = ns_ibmmq_init_producer(host, port, queue_manager, channel, queue_topic_name, user_name, password, 10, error_msg)) < 0)
    {
      fprintf(stderr, "Error in initializing IBMMQ producer. Error code = %d, Error Msg = %s\n", jpid, error_msg);
      return;
    }
  }
  else
  {
    if((jpid = ns_ibmmq_init_consumer(host, port, queue_manager, channel, queue_topic_name, user_name, password, 10, error_msg)) < 0)
    {
      fprintf(stderr, "Error in initializing IBMMQ consumer. Error code = %d, Error Msg = %s\n", jpid, error_msg);
      return;
    }
  }

  if((jcid = ns_ibmmq_get_connection(jpid, tx_name, error_msg)) < 0)
  {
    fprintf(stderr, "Error in getting IBMMQ connention from the pool. Error code = %d, Error Msg = %s\n", jpid, error_msg);
    return;
  }
 
  if(get_or_put == 2)
  { 
    if((ret = ns_ibmmq_put_msg(jcid, msg, msg_len, tx_name, error_msg)) < 0)
    {
      fprintf(stderr, "Error in putting message using IBMMQ connention. Error code = %d, Error Msg = %s\n", ret, error_msg);
      return;
    }
  }
  else
  {
    if((ret = ns_ibmmq_get_msg(jcid, msg, msg_len, tx_name, error_msg)) < 0)
    {
      fprintf(stderr, "Error in etting message using IBMMQ connention. Error code = %d, Error Msg = %s\n", ret, error_msg);
      return;
    }
  }
  //This api will release the  connection from connection pool and it should be called every time so that another user can reuse it.

  if((jcid = ns_ibmmq_release_connection(jcid, error_msg)) < 0)
  {
    fprintf(stderr, "Error in releasing IBMMQ connection.Error code = %d, Error Msg = %s \n", jcid, error_msg);
    return;
  }

  return 0;
}

#endif

