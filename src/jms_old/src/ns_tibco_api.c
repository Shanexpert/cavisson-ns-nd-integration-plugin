#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#include "ns_string.h"
#include "ns_tibco_api.h"

// TODO - Add timeout so that NS does not hang for long
/*---------------------------------------------------------------------
 * ns_tibco_log_error
 *---------------------------------------------------------------------*/
static int ns_tibco_log_error(
  const char*         message, 
  tibemsErrorContext* errorContext, TibcoClientInfo *tci_ptr)
{
  tci_ptr->status = TIBEMS_OK;
  const char*         str = NULL;

  printf("ERROR: %s\n",message);

  tci_ptr->status = tibemsErrorContext_GetLastErrorString(errorContext, &str);
  printf("\nLast error message =\n%s\n", str);
  tci_ptr->status = tibemsErrorContext_GetLastErrorStackTrace(errorContext, &str);
  printf("\nStack trace = \n%s\n", str);
  return -1;
}

#ifdef NS_DEBUG_ON

#define TIBCO_SENT_MSG    0
#define TIBCO_RECV_MSG    1
void log_tibco_req(tibemsDestination destination, tibemsMsg msg)
{
  log_tibco_req_rep(destination, msg, TIBCO_SENT_MSG);
}

void log_tibco_resp(tibemsDestination destination, tibemsMsg msg)
{
  log_tibco_req_rep(destination, msg, TIBCO_RECV_MSG);
}
#endif

#define FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr) \
{ \
  if(tri_ptr) {\
    if(tri_ptr->sslParams)  \
      tibemsSSLParams_Destroy(tri_ptr->sslParams); \
    free((void *)tri_ptr); \
    tri_ptr = NULL;   \
  } \
  return(NULL); \
}


#define FREE_AND_RETURN_ERROR(tci_ptr) \
{ \
  if(tci_ptr) {\
    if(tci_ptr->sslParams)  \
      tibemsSSLParams_Destroy(tci_ptr->sslParams); \
    free((void *)tci_ptr); \
    tci_ptr = NULL;  \
  }    \
  return(NULL); \
}

// Setting Default value for the producer
void set_default_value_for_producer(TibcoClientInfo *tci_ptr)
{
  tci_ptr->factory = NULL;
  tci_ptr->connection = NULL;
  tci_ptr->session = NULL;
  tci_ptr->msgProducer = NULL;
  tci_ptr->destination = NULL;
  tci_ptr->status = TIBEMS_OK;
  tci_ptr->msg = NULL;
  tci_ptr->errorContext = NULL;
}

/*-----------------------------------------------------------------------
 * ns_tibco_send_message
 *----------------------------------------------------------------------*/
TibcoClientInfo *ns_tibco_create_producer_ssl(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, long timetolive, int timestamp_flag, tibemsSSLParams sslParams, char *pk_password)
{
  TibcoClientInfo *tci_ptr = NULL;

#ifdef NS_DEBUG_ON  
  printf("Method called. serverUrl = %s, username = %s, password = %s, t_or_q = %d, t_q_name = %s\n", 
           serverUrl, username, password, t_or_q, t_q_name);
#endif

  if (serverUrl == NULL || username == NULL|| !t_q_name || t_q_name == NULL) 
    printf("Error: Invalid arguments defined\n");
    
  tci_ptr= malloc(sizeof(TibcoClientInfo));
  memset(tci_ptr, 0, sizeof(TibcoClientInfo)); 
  set_default_value_for_producer(tci_ptr);

#ifdef NS_DEBUG_ON  
  printf("%s Method called Creating ErrorContext\n", (char *)__FUNCTION__);
#endif
  
  // This method will enable additional error tracking
  tci_ptr->status = tibemsErrorContext_Create(&(tci_ptr->errorContext));
#ifdef NS_DEBUG_ON  
    printf("%s Method called.. Status for tibemsErrorContext_Create = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    printf("Context creation failed: %s\n", tibemsStatus_GetText(tci_ptr->status));
    FREE_AND_RETURN_ERROR(tci_ptr);
  }
#ifdef NS_DEBUG_ON
  printf("%s Method called Context created succesfully\n", (char *)__FUNCTION__);
#endif
  // Administer object for craeting server connection
  tci_ptr->factory = tibemsConnectionFactory_Create();
  if (!tci_ptr->factory)
  {
    ns_tibco_log_error("Error creating tibemsConnectionFactory", tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }

  /* Setting the TIMEOUT */
  tci_ptr->status = tibemsConnectionFactory_SetConnectAttemptTimeout(tci_ptr->factory, 20000);
#ifdef NS_DEBUG_ON  
    printf("%s Method called.. Status for tibemsConnectionFactory_SetConnectAttemptTimeout = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting TIMEOUT", tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }
  // Sets the server url
  tci_ptr->status = tibemsConnectionFactory_SetServerURL(tci_ptr->factory,serverUrl);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for tibemsConnectionFactory_SetServerURL = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif
  if (tci_ptr->status != TIBEMS_OK) 
  {
    ns_tibco_log_error("Error setting server url", tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }
  if(sslParams)
  {
    tci_ptr->status = tibemsConnectionFactory_SetSSLParams(tci_ptr->factory, sslParams);
    if(tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting sslParams", tci_ptr->errorContext, tci_ptr);
      FREE_AND_RETURN_ERROR(tci_ptr);
    }
    tci_ptr->sslParams = sslParams;
#ifdef NS_DEBUG_ON  
    printf("Successfully set ssl params for producer\n");
#endif
    if(pk_password)
    {
      tci_ptr->status = tibemsConnectionFactory_SetPkPassword(tci_ptr->factory,pk_password);
      if(tci_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error setting ssl private key password", tci_ptr->errorContext, tci_ptr);
        FREE_AND_RETURN_ERROR(tci_ptr);
      }
    }
    
  }
  // Creates the connection object
  tci_ptr->status = tibemsConnectionFactory_CreateConnection(tci_ptr->factory,&(tci_ptr->connection),
                            username,password);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for tibemsConnectionFactory_CreateConnection = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
    ns_tibco_log_error("Error creating tibemsConnection", tci_ptr->errorContext, tci_ptr);

  /* create the destination */
  if (t_or_q == NS_TIBCO_USE_TOPIC)
    tci_ptr->status = tibemsTopic_Create(&(tci_ptr->destination),t_q_name);
  else
    tci_ptr->status = tibemsQueue_Create(&(tci_ptr->destination),t_q_name);

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsDestination",  tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }

  /* create the session */
  tci_ptr->status = tibemsConnection_CreateSession(tci_ptr->connection,
                                                    &(tci_ptr->session),TIBEMS_FALSE,TIBEMS_NO_ACKNOWLEDGE);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for tibemsConnection_CreateSession = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsSession",  tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }

  // create the producer 
  tci_ptr->status = tibemsSession_CreateProducer(tci_ptr->session,
                                                  &(tci_ptr->msgProducer), tci_ptr->destination);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsSession_CreateProducer = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsMsgProducer",  tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }

/* set the delivery mode */
    tci_ptr->status = tibemsMsgProducer_SetDeliveryMode(tci_ptr->msgProducer,
                                              TIBEMS_RELIABLE);
    if (tci_ptr->status != TIBEMS_OK)
    {
        //fail("Error setting delivery mode", tci_ptr->errorContext);
    }

    /* performance settings */
    tci_ptr->status = tibemsMsgProducer_SetDisableMessageID(tci_ptr->msgProducer,
                                                   TIBEMS_TRUE) ||
             tibemsMsgProducer_SetDisableMessageTimestamp(tci_ptr->msgProducer,
                                                          TIBEMS_TRUE);
    if (tci_ptr->status != TIBEMS_OK)
    {
        //fail("Error configuring tibemsMsgProducer", tci_ptr->errorContext);
    }
  //set time stamp header flag.     
/*  if (timestamp_flag == NS_TIBCO_DISABLE_TIMESTAMP)
  {
    tci_ptr->status = tibemsMsgProducer_SetDisableMessageTimestamp(tci_ptr->msgProducer, TIBEMS_TRUE);
    if (tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting tibemsMsgProducer_SetDisableMessageTimestamp", tci_ptr->errorContext, tci_ptr);
      FREE_AND_RETURN_ERROR(tci_ptr);
    }
  }*/
 
  tci_ptr->status = tibemsMsgProducer_SetTimeToLive(tci_ptr->msgProducer, timetolive);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for tibemsMsgProducer_SetTimeToLive = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsMsgProducer",  tci_ptr->errorContext, tci_ptr);
    FREE_AND_RETURN_ERROR(tci_ptr);
  }

  return(tci_ptr);
}
//For Creating message 
int ns_tibco_create_message(TibcoClientInfo *tci_ptr, char *data)
{
  /* create the text message */
  tci_ptr->status = tibemsTextMsg_Create(&(tci_ptr->msg));
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsTextMsg_Create = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    tci_ptr->msg = NULL;
    ns_tibco_log_error("Error creating tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }
  /* set the message text */
  tci_ptr->status = tibemsTextMsg_SetText(tci_ptr->msg, data);

#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsTextMsg_SetText = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting tibemsTextMsg text", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }
  return 0;  
}
// Setting Timestamp header alongwith message
int ns_tibco_set_timestamp_header(TibcoClientInfo *tci_ptr, long timestamp)
{
  if(!tci_ptr || !tci_ptr->msg)
  {
   // ns_tibco_log_error("message not initialized");
    return -1;
  }

  //set timestamp flag is greater than 0 then set timestamp header.
  tibems_bool disable_timestamp_flag;
  tci_ptr->status = tibemsMsgProducer_GetDisableMessageTimestamp(tci_ptr->msgProducer, &disable_timestamp_flag);
  if(tci_ptr->status != TIBEMS_OK)
  { 
    ns_tibco_log_error("Error getting timestamp flag", tci_ptr->errorContext, tci_ptr);
    return -1; 
  }
  if((disable_timestamp_flag != TIBEMS_TRUE) && timestamp > 0)
  {
    tci_ptr->status = tibemsMsg_SetTimestamp(tci_ptr->msg, timestamp);
    if (tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting tibemsMsg_SetTimestamp", tci_ptr->errorContext, tci_ptr);
      return(-1);
    }
  } 
  return 0;
}
typedef union HeaderValue{
  int int_val;
  char char_val;
  short short_val;
  float float_val;
  long long_val;
  double double_val;
  char *string_val;
}headerValue;

int ns_tibco_set_custom_header_ex(TibcoClientInfo *tci_ptr, char *header_name, int value_type, ...)
{
  va_list ap;
  headerValue value; 
  
  if(!tci_ptr || !tci_ptr->msg)
  {
    ns_tibco_log_error("message not initialized", tci_ptr->errorContext, tci_ptr);
    return -1;
  }
  if(!header_name)
  {
    ns_tibco_log_error("Header name missing", tci_ptr->errorContext, tci_ptr);
    return -1;
  }
  tci_ptr->status = TIBEMS_OK;
  va_start(ap, value_type);
  switch(value_type)
  { 
    case NS_TIBCO_BOOLEAN:
      value.int_val = va_arg(ap, int);
      if(value.int_val != 0)
        value.int_val = TRUE;
      else
        value.int_val = FALSE;
      tci_ptr->status = tibemsMsg_SetBooleanProperty(tci_ptr->msg, header_name, value.int_val);
      break;
    case NS_TIBCO_STRING:
      value.string_val= va_arg(ap, char *);
      tci_ptr->status = tibemsMsg_SetStringProperty(tci_ptr->msg, header_name, value.string_val);
      break;
    case NS_TIBCO_INTEGER:
      value.int_val = va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetIntProperty(tci_ptr->msg, header_name, value.int_val); 
      break;
    case NS_TIBCO_DOUBLE:
      value.double_val = va_arg(ap, double);
      tci_ptr->status = tibemsMsg_SetDoubleProperty(tci_ptr->msg, header_name, value.double_val);
      break;
    case NS_TIBCO_FLOAT:
      value.float_val = (float)va_arg(ap, double);
      tci_ptr->status = tibemsMsg_SetFloatProperty(tci_ptr->msg, header_name, value.float_val);
      break;
    case NS_TIBCO_SORT:
      value.short_val = (short)va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetShortProperty(tci_ptr->msg, header_name, value.short_val);
      break;
    case NS_TIBCO_BYTE:
      value.char_val = (char)va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetByteProperty(tci_ptr->msg, header_name, value.char_val);
      break;       
    default:
      //ns_tibco_log_error("Header value type is not valid");
      return -1;
  }
  va_end(ap);
  if(tci_ptr->status != TIBEMS_OK)
  {
    //ns_tibco_log_error("Failed to set header value");
    return -1;
  } 
  return 0; 
}
 
//int ns_tibco_send_message(TibcoClientInfo *tci_ptr, char *data, long timestamp, char *header, char *value) 
int ns_tibco_send_message(TibcoClientInfo *tci_ptr, char *data) 
{
  #if 0
  /* publish messages */
  /* create the text message */
  tci_ptr->status = tibemsTextMsg_Create(&(tci_ptr->msg));
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsTextMsg_Create = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    tci_ptr->msg = NULL;
    ns_tibco_log_error("Error creating tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }

  /* set the message text */
  tci_ptr->status = tibemsTextMsg_SetText(tci_ptr->msg, data);

#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsTextMsg_SetText = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting tibemsTextMsg text", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }
  //set timestamp flag is greater than 0 then set timestamp header.
  tibems_bool disable_timestamp_flag;
  tci_ptr->status = tibemsMsgProducer_GetDisableMessageTimestamp(tci_ptr->msgProducer, &disable_timestamp_flag);
  if(tci_ptr->status != TIBEMS_OK)
  { 
    ns_tibco_log_error("Error getting timestamp flag", tci_ptr->errorContext, tci_ptr);
    return -1; 
  }
  if((disable_timestamp_flag != TIBEMS_TRUE) && timestamp > 0)
  {
    tci_ptr->status = tibemsMsg_SetTimestamp(tci_ptr->msg, timestamp);
    if (tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting tibemsMsg_SetTimestamp", tci_ptr->errorContext, tci_ptr);
      return(-1);
    }
  }
#endif 
  /* publish the message */
  tci_ptr->status = tibemsMsgProducer_Send(tci_ptr->msgProducer, tci_ptr->msg);

#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsMsgProducer_Send = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error publishing tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }
  else {
#ifdef NS_DEBUG_ON
    printf("Published message: %s\n",data);
    log_tibco_req(tci_ptr->destination, tci_ptr->msg);  
#endif
  }

  /* destroy the message */
  tci_ptr->status = tibemsMsg_Destroy(tci_ptr->msg);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsMsg_Destroy = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }
  return 0;
}

int ns_tibco_close_producer(TibcoClientInfo *tci_ptr)
{  
  /* destroy the destination */
  tci_ptr->status = tibemsDestination_Destroy(tci_ptr->destination);

#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsDestination_Destroy = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsDestination", tci_ptr->errorContext, tci_ptr);
  }

  /* close the connection */
  tci_ptr->status = tibemsConnection_Close(tci_ptr->connection);
#ifdef NS_DEBUG_ON  
  printf("%s Method called.. Status for  tibemsConnection_Close = %d\n", (char *)__FUNCTION__, tci_ptr->status);
#endif
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsConnection", tci_ptr->errorContext, tci_ptr);
    return(-1);
  }

  tibemsErrorContext_Close(tci_ptr->errorContext);
  return 0;
}

// Setting Default value for the consumer
void set_default_value_for_consumer(TibcoClientReceiveInfo *tri_ptr)
{
  tri_ptr->factory = NULL;
  tri_ptr->connection = NULL;
  tri_ptr->session = NULL;
  tri_ptr->msgConsumer = NULL;
  tri_ptr->destination = NULL;
  tri_ptr->receive = 1;
  tri_ptr->status = TIBEMS_OK;
  tri_ptr->msg = NULL;
  tri_ptr->errorContext = NULL;
  tri_ptr->txt = NULL;
  tri_ptr->msgType = TIBEMS_MESSAGE_UNKNOWN;
  tri_ptr->ackMode = TIBEMS_AUTO_ACKNOWLEDGE;
  tri_ptr->msgTypeName = "UNKNOWN";
  
}


static TibcoClientReceiveInfo *ns_tibco_create_consumer_ex(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, int consumerType, char *durableName, tibemsSSLParams sslParams, char *pk_password);


//TODO: do we need to add verifyhost and verifyhostname flag in arguments.
//pk password will be called from ns_tibco_create_consumer_ssl()
tibemsSSLParams ns_tibco_set_ssl_param
(char *ciphers, char *private_key_file, char *hostname, char *trustedCA, char *issuer, char *identity)
{
#define RUN_CMD(cmd, ...) \
{  \
  status = cmd(sslParams, __VA_ARGS__); \
  if(status != TIBEMS_OK) \
  { \
    /*print error.*/ \
    printf("Failed to set ssl param.\n"); \
    tibemsSSLParams_Destroy(sslParams); \
    return NULL; \
  } \
}

  int status;
  tibemsSSLParams sslParams = tibemsSSLParams_Create();
  int verify_host = TIBEMS_FALSE; 
  if(ciphers)
  {
    RUN_CMD(tibemsSSLParams_SetCiphers, ciphers);
  }

  if(private_key_file)
  {
    RUN_CMD(tibemsSSLParams_SetPrivateKeyFile, private_key_file, TIBEMS_SSL_ENCODING_AUTO);
  }

  if(hostname)
  {
    RUN_CMD(tibemsSSLParams_SetExpectedHostName, hostname);
  }

  if(trustedCA)
  {
    RUN_CMD(tibemsSSLParams_AddTrustedCertFile, trustedCA, TIBEMS_SSL_ENCODING_AUTO);
    //Only in this case we will verify host. 
    verify_host = TIBEMS_TRUE;
  }

  if(issuer)
  {
    RUN_CMD(tibemsSSLParams_AddIssuerCertFile, issuer, TIBEMS_SSL_ENCODING_AUTO);
  }

  if(identity)
  {
    RUN_CMD(tibemsSSLParams_SetIdentityFile, identity, TIBEMS_SSL_ENCODING_AUTO);
  }
  //set verify host to false.
  if(!verify_host)
    tibemsSSLParams_SetVerifyHost(sslParams,TIBEMS_FALSE);
#ifdef NS_DEBUG_ON  
  printf("ssl params set successfully \n");
#endif
  return sslParams; 
} 

TibcoClientReceiveInfo *ns_tibco_create_consumer_ssl
(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, tibemsSSLParams sslParams, char *pk_password)
{
  return(ns_tibco_create_consumer_ex(serverUrl, username, password, t_or_q, t_q_name, NON_DURABLE_CONSUMER, NULL, sslParams, pk_password));
}


TibcoClientReceiveInfo *ns_tibco_create_durable_consumer_ssl
(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, char *durableName, tibemsSSLParams sslParams, char *pk_password)
{
  return(ns_tibco_create_consumer_ex(serverUrl, username, password, t_or_q, t_q_name, DURABLE_CONSUMER, durableName, sslParams, pk_password));
}


TibcoClientReceiveInfo *ns_tibco_create_consumer(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name)
{
  return(ns_tibco_create_consumer_ex(serverUrl, username, password, t_or_q, t_q_name, NON_DURABLE_CONSUMER, NULL, NULL, NULL));
}


TibcoClientReceiveInfo *ns_tibco_create_durable_consumer(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, char *durableName)
{
  return(ns_tibco_create_consumer_ex(serverUrl, username, password, t_or_q, t_q_name, DURABLE_CONSUMER, durableName, NULL, NULL));
}


// Creating the wrapper for create consumer api for durable and non durable consumer 
static TibcoClientReceiveInfo *ns_tibco_create_consumer_ex(char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, int consumerType, char *durableName, tibemsSSLParams sslParams, char *pk_password)
{
   TibcoClientReceiveInfo *tri_ptr = NULL;
   if (serverUrl == NULL || username == NULL|| !t_q_name || t_q_name == NULL)
     printf("Error: Invalid arguments defined\n");

   tri_ptr= malloc(sizeof(TibcoClientReceiveInfo));
   memset(tri_ptr, 0, sizeof(TibcoClientReceiveInfo));
   set_default_value_for_consumer(tri_ptr);
   
#ifdef NS_DEBUG_ON
  printf("Subscribing to destination: '%s'\n\n", t_q_name);
#endif
  // This method will enable additional error tracking 
  tri_ptr->status = tibemsErrorContext_Create(&(tri_ptr->errorContext));
#ifdef NS_DEBUG_ON
  printf("%sMethod called.. Status for tibemsErrorContext_Create = %d\n", (char *)__FUNCTION__, tri_ptr->status);
#endif
  if (tri_ptr->status != TIBEMS_OK)
  {
    printf("ErrorContext create failed: %s\n", tibemsStatus_GetText(tri_ptr->status));
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }
  // Administer object for craeting server connection
  tri_ptr->factory = tibemsConnectionFactory_Create();
  if (!tri_ptr->factory)
  {
    ns_tibco_log_error("Error creating tibemsConnectionFactory", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }
  // Set the server url 
  tri_ptr->status = tibemsConnectionFactory_SetServerURL(tri_ptr->factory,serverUrl);
#ifdef NS_DEBUG_ON
   printf("%sMethod called.. Status for tibemsConnectionFactory_SetServerURL = %d\n", (char *)__FUNCTION__, tri_ptr->status);
#endif
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting server url", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }

  //set ssl params.
  if(sslParams) 
  {
    tri_ptr->status = tibemsConnectionFactory_SetSSLParams(tri_ptr->factory, sslParams);
    if(tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting sslParams", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr); 
    }
    tri_ptr->sslParams = sslParams;
#ifdef NS_DEBUG_ON  
    printf("Successfully set ssl params \n");
#endif
    //set private key password.
    if(pk_password)
    {
      tri_ptr->status = tibemsConnectionFactory_SetPkPassword(tri_ptr->factory,pk_password); 
      if(tri_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error setting ssl private key password", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
        FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
      }
    }
  }
  // Creates the connection
  tri_ptr->status = tibemsConnectionFactory_CreateConnection(tri_ptr->factory,&(tri_ptr->connection),
                                                    username,password);
#ifdef NS_DEBUG_ON
   printf("%sMethod called.. Status for tibemsConnectionFactory_CreateConnection = %d\n", (char *)__FUNCTION__, tri_ptr->status);
#endif

  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsConnection", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }
  
  /* set the exception listener */
  #if 0
  status = tibemsConnection_SetExceptionListener(connection,
          onException, NULL);
  if (status != TIBEMS_OK)
  {
      ns_tibco_log_error("Error setting exception listener", errorContext);
      return(-1);
  }
  #endif
  
  /* create the destination */
  if (t_or_q == NS_TIBCO_USE_TOPIC)
    tri_ptr->status = tibemsTopic_Create(&(tri_ptr->destination), t_q_name);
  else
      tri_ptr->status = tibemsQueue_Create(&(tri_ptr->destination), t_q_name);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsDestination", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }

  /* create the session */
  tri_ptr->status = tibemsConnection_CreateSession(tri_ptr->connection,
                                                      &(tri_ptr->session),TIBEMS_FALSE, tri_ptr->ackMode);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsSession", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
  }

  /* create the consumer */
  if(consumerType == NON_DURABLE_CONSUMER)
  {
    tri_ptr->status = tibemsSession_CreateConsumer(tri_ptr->session,
                                         &(tri_ptr->msgConsumer),tri_ptr->destination, NULL, TIBEMS_FALSE);
#ifdef NS_DEBUG_ON
    printf("Status for tibemsSession_CreateConsumer = %d\n", tri_ptr->status);
#endif
    if (tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error creating tibemsMsgConsumer", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
    } 
  }
  else
  {
    tri_ptr->status = tibemsSession_CreateDurableSubscriber(tri_ptr->session,
                                         &(tri_ptr->msgConsumer),tri_ptr->destination, durableName, NULL, TIBEMS_FALSE);
#ifdef NS_DEBUG_ON
    printf("Status for tibemsSession_CreateDurableSubscriber = %d", tri_ptr->status);
#endif
    if (tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error creating DurableSubscriber", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      FREE_AND_RETURN_ERROR_RECEIVE(tri_ptr);
    } 
  }

  tri_ptr->status = tibemsConnection_Start(tri_ptr->connection);
  if(tri_ptr->status != TIBEMS_OK)
  {
    #ifdef NS_DEBUG_ON 
      printf("Error starting tibemsConnection for recieving %s\n", tri_ptr->errorContext);
    #endif
    return(NULL);
  }

  return(tri_ptr);
}

// Creating durable consumer that supports TTL, to set the expiry time of message
int ns_tibco_unsubscribe_durable_consumer(TibcoClientReceiveInfo *tri_ptr, char *durableName)
{
  if(!tri_ptr)
  {
    return -1;
  }
  tri_ptr->status = tibemsSession_Unsubscribe(tri_ptr->session, durableName);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error tibemsSession_Unsubscribe", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return -1;
  }
  return 0; 
}


int ns_tibco_receive_message(TibcoClientReceiveInfo *tri_ptr, char *buff, int *break_flag)
{
  /* start the connection */
/*
  tri_ptr->status = tibemsConnection_Start(tri_ptr->connection);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error starting tibemsConnection", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return(-1);
  }

  */
   //char *ptr1 = *ptr;
   // int total_wrt = 0;
     //int amt_wrt = 0;

  /* read messages */
  //while(tri_ptr->receive)
  //{
    /* receive the message */
    tri_ptr->status = tibemsMsgConsumer_ReceiveTimeout(tri_ptr->msgConsumer,&(tri_ptr->msg),10000);
    if(tri_ptr->status != TIBEMS_OK)
    {
      if(tri_ptr->status == TIBEMS_INTR)
      {
        ns_tibco_log_error("Error receiving message", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
        return(-1);
      }
      else if(tri_ptr->status == TIBEMS_TIMEOUT)
      {
#ifdef NS_DEBUG_ON
        printf("Message Recieve Timeout\n");
#endif
        return(-1);
      }
    }
    if(!(tri_ptr->msg)){
      *break_flag = 1;
      return(-1);
    }

    /* acknowledge the message if necessary */
    if(tri_ptr->ackMode == TIBEMS_CLIENT_ACKNOWLEDGE ||
            tri_ptr->ackMode == TIBEMS_EXPLICIT_CLIENT_ACKNOWLEDGE ||
            tri_ptr->ackMode == TIBEMS_EXPLICIT_CLIENT_DUPS_OK_ACKNOWLEDGE)
    {
      tri_ptr->status = tibemsMsg_Acknowledge(tri_ptr->msg);
      if(tri_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error acknowledging message", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
        return(-1);
      }
    }

    /* check message type */
    tri_ptr->status = tibemsMsg_GetBodyType(tri_ptr->msg,&(tri_ptr->msgType));
    if(tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error getting message type", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      return(-1);
    }
        
    switch(tri_ptr->msgType)
    {
      case TIBEMS_MESSAGE:
           tri_ptr->msgTypeName = "MESSAGE";
           break;

      case TIBEMS_BYTES_MESSAGE:
           tri_ptr->msgTypeName = "BYTES";
           break;

      case TIBEMS_OBJECT_MESSAGE:
           tri_ptr->msgTypeName = "OBJECT";
           break;

      case TIBEMS_STREAM_MESSAGE:
           tri_ptr->msgTypeName = "STREAM";
           break;
      case TIBEMS_MAP_MESSAGE:
           tri_ptr->msgTypeName = "MAP";
           break;

      case TIBEMS_TEXT_MESSAGE:
           tri_ptr->msgTypeName = "TEXT";
           break;

      default:
           tri_ptr->msgTypeName = "UNKNOWN";
           break;
     }

     if(tri_ptr->msgType != TIBEMS_TEXT_MESSAGE)
      {
#ifdef NS_DEBUG_ON
  printf("Received %s message:\n",tri_ptr->msgTypeName);
#endif
        tibemsMsg_Print(tri_ptr->msg);
        strcpy(buff, tri_ptr->txt);

        // printf("*****************************UPPER LAG\n");
          //if(total_wrt < 2000*1024){
            //  amt_wrt = sprintf(ptr1, "%s|", tri_ptr->txt);
            // total_wrt += amt_wrt;
              // ptr1 += amt_wrt;
               //amt_wrt = 0;
          // }else{
           //             printf("Message exceeds the limit of 8K\n");
          // }
      }
      else
      {
        /* get the message text */
        tri_ptr->status = tibemsTextMsg_GetText(tri_ptr->msg,&(tri_ptr->txt));
        strcpy(buff, tri_ptr->txt);
        if (tri_ptr->status != TIBEMS_OK)
        {
            ns_tibco_log_error("Error getting tibemsTextMsg text", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
            return(-1);
        }
//  printf("Received TEXT message: %s\n",            tri_ptr->txt ? tri_ptr->txt : "<text is set to NULL>");
         //printf("*****************************LOWER LAG\n");
          //if(total_wrt < 2000*1024){
            //  amt_wrt = sprintf(ptr1, "%s|", tri_ptr->txt);
            // total_wrt += amt_wrt;
              // ptr1 += amt_wrt;
              // amt_wrt = 0;
         //  }else{
           //             printf("Message exceeds the limit of 8K\n");
          // }
      }
      
#ifdef NS_DEBUG_ON
       log_tibco_resp(tri_ptr->destination, tri_ptr->msg);
#endif 
      /* destroy the message */
       tri_ptr->status = tibemsMsg_Destroy(tri_ptr->msg);
      if (tri_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error destroying tibemsMsg",  tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
        return(-1);
      }
   //}
   // ptr1[0] = '\0';
  if(tri_ptr->receive == 0){
    *break_flag = 1;
  }
 return 0;
}

// Closing the Consumer Side
int ns_tibco_close_consumer(TibcoClientReceiveInfo *tri_ptr)
{
  /* destroy the destination */
  tri_ptr->status = tibemsMsgConsumer_Close(tri_ptr->msgConsumer);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsMsgConsumer", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return(-1);
  }
    
  tri_ptr->status = tibemsSession_Close(tri_ptr->session);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing consumer session", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return(-1);
  }

  tri_ptr->status = tibemsDestination_Destroy(tri_ptr->destination);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsDestination", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return(-1);
  }

  /* close the connection */
  tri_ptr->status = tibemsConnection_Close(tri_ptr->connection);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsConnection", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    return(-1);
  }

  tibemsErrorContext_Close(tri_ptr->errorContext);
  return 0;  
}

#ifdef NS_DEBUG_ON

void log_tibco_req_rep(tibemsDestination destination, tibemsMsg msg, int msg_type)
{
  const char *data = NULL;
  tibemsDestinationType t_or_q;
  char t_q_name[256] = "\0";
  char log_file[4096] = "\0";
  char *req_type_string = (msg_type==TIBCO_SENT_MSG)?"sent":"recv";
  int log_fd;
  FILE *log_fp = NULL;
  char end_marker[64];
  tibems_status status;
  char *ns_wdir = NULL ;
  long timestamp;
  struct tm *timeinfo = NULL;
  //char time_string[50] = "\0";
 
  ns_wdir = getenv("NS_WDIR");
  if(!ns_wdir)
  {
    fprintf(stderr, "Error; Error in getting system variable \'NS_WDIR\'.\n");
    return;
  }
  sprintf(log_file, "%s/logs/TR%d/jms_msg_%s_%d_%u_%u.dat", 
          ns_wdir, ns_get_testid(), req_type_string, ns_get_nvmid(), ns_get_userid(), ns_get_sessid());
  

  if((log_fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND, 0666)) < 0)
      fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    log_fp = fdopen(log_fd, "a");
    if(!log_fp){ 
      fprintf(stderr, "Error: Error in opening file for logging jms message\n");
      return;
    }
    status = tibemsDestination_GetType(destination, &t_or_q); 
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Error: Error in getting detination type.\n");
      return;
    }
    status = tibemsDestination_GetName(destination, t_q_name, 256);
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Error: Error in getting detination name.\n");
      return;
    }
    //set topic or queue name.
    if(t_or_q == TIBEMS_TOPIC)
      fprintf(log_fp, "TOPIC: %s\n", t_q_name);
    else if(t_or_q == TIBEMS_QUEUE)
      fprintf(log_fp, "QUEUE: %s\n", t_q_name);
    else
      fprintf(log_fp, "UNKNOWN: %s\n", t_q_name);
   
    //get timestamp.
    status = tibemsMsg_GetTimestamp(msg, (tibems_long *)&timestamp);
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Error: Error in getting timestamp header from jms message.\n");
      return;
    }
    timestamp = timestamp / 1000;
		if(timestamp != 0)
		{
      timeinfo = localtime(&timestamp);
      fprintf(log_fp, "TIME-STAMP: %s", asctime(timeinfo)); 
    }
    tibemsMsgEnum propertyList;
    status = tibemsMsg_GetPropertyNames(msg, &propertyList);
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Error: Error in getting property list from the message .\n");
      if(status == TIBEMS_NOT_FOUND)
      {
        fprintf(stderr, "No property found in this message\n");
      }
      return;
    }
    const char *propertyName;
    tibemsMsgField value;
    char valueBuffer[512] = "";
    int i = 0;
    char *tmp;
    while(1)
    {
      status = tibemsMsgEnum_GetNextName(propertyList, &propertyName);
      if(status == TIBEMS_NOT_FOUND){
        //fprintf(stderr, "No more property remained\n");
        break;
      }
      if(propertyName == NULL)
        break;
      status = tibemsMsg_GetProperty(msg, propertyName, &value);
      if(status != TIBEMS_OK){
        fprintf(stderr, "Failed to get property %s's value\n", propertyName);
        break;
      }
      status = tibemsMsgField_PrintToBuffer(&value, valueBuffer, 512);
      if(status != TIBEMS_OK){
        fprintf(stderr, "Failed to copy property value to buffer\n");
        break;
      }
      /*As value will come in Type:Value form and we just need value,so we will take second field */
      tmp = strchr(valueBuffer, ':');
      if(!tmp)
        tmp = valueBuffer;
      else
        tmp ++; 
      fprintf(log_fp, "%s:%s\n", propertyName, tmp); 
      i++;
      /*Break if max fields reached */
      if(i == 128)
        break;
    }
    /*Free enumuration*/
    status = tibemsMsgEnum_Destroy(propertyList);
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Failed to release tibco memory\n");
    }
    status = tibemsTextMsg_GetText(msg, &data);
    if(status != TIBEMS_OK)
    {
      fprintf(stderr, "Error: Error in getting data from jms message .\n");
      return;
    }
    //Check if message recieved or not.
    if(data == NULL)
      data = "Binary Message(Can not be displayed)";
    //write message. 
    memset(end_marker, '-', 63);
    end_marker[63] = 0;
    fprintf(log_fp, "%s\n%s\n", data, end_marker);  
    fclose(log_fp); 
  }  
}
#endif

