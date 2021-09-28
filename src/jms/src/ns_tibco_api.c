/******************************************************************************
 * Name    :    ns_tibco_api.c
 * Purpose :    This file contains Api's of Tibko producer and consumer clients
                for sending and receiving message from Tibko server.
 * Author  :    Neha Rawat
 * Intial version date:    16/05/2019
 * Last modification date: 16/05/2019
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include "../../ns_log.h"
#include "../../ns_alloc.h"
#include "../../ns_msg_def.h"
#include "ns_jms_conn_pool.h"
#include "ns_tibco_api.h"
#include "../../ns_string.h"
#include "ns_jms_error.h"
/* Added for Msg_com_con and buffer_key */
#include "../../util.h"
#include "../../netstorm.h"
#include "../../ns_vuser_thread.h"
#include "../../ns_debug_trace.h"
//This api will initialize connection pool of size pool_size

#ifdef JMS_API_TEST
#define NSDL1_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL2_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL3_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL4_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
int ns_start_transaction(char *transaction_name)
{
  return 0;
}
int ns_end_transaction(char *transaction_name, int status)
{
  return 0;
}
int ns_end_transaction_as(char *transaction_name, int status, char *end_tx)
{
  return 0;
}
#endif

static int ns_tibco_get_header(TibcoClientReceiveInfo *tci_ptr, char *header, int hdr_len, int jpcid);

/*---------------------------------------------------------------------
 * ns_tibco_log_error
 *---------------------------------------------------------------------*/
static int ns_tibco_log_error(
  const char*         message,
  tibemsErrorContext* errorContext, TibcoClientInfo *tci_ptr)
{ 
  tci_ptr->status = TIBEMS_OK;
  const char*         str = NULL;
  
  NSDL1_JMS(NULL, NULL, "ERROR: %s\n",message);
  tci_ptr->status = tibemsErrorContext_GetLastErrorString(errorContext, &str);
  NSDL1_JMS(NULL, NULL, "\nLast error message =\n%s\n", str);
  tci_ptr->status = tibemsErrorContext_GetLastErrorStackTrace(errorContext, &str);
  NSDL1_JMS(NULL, NULL, "\nStack trace = \n%s\n",str);
  return -1;
}

//This api will initialize connection pool of size pool_size
int ns_tibco_init_producer(char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                                 char *tibco_password, int max_pool_size, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called. max_pool_size = %d", max_pool_size);
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Initializing producer connection pool. Server ip/port: %s:%d, Topic/Queue: %s, Username: %s, Password: ****, Connection pool size: %d", tibco_hostname, tibco_port, tibco_topic_or_queue, tibco_userId, max_pool_size);
  int jpid; 
  int is_new_key;

  if(!tibco_hostname || !tibco_port || !tibco_topic_or_queue)
    HANDLE_ERROR_DT("TIBCO-Error in Producer connection initialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  int len = strlen(tibco_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s:%d", tibco_hostname, tibco_port);
  if(!tibco_userId)
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, "NA", "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else if(!tibco_password)
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, tibco_userId, "NA", "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, tibco_userId, tibco_password, "NA", max_pool_size, PRODUCER, error_msg, &is_new_key);
  if(jpid < 0)
    return NS_JMS_ERROR_WRONG_USER_CONFIG;

  if(is_new_key)
  {
    TibcoConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(TibcoConfig));
    jms_specific_config->t_or_q = t_or_q;
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
    jms_specific_config->put_mode = TIBEMS_RELIABLE;
  }

  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Producer connection pool successfully initialized.", jpid);
  return jpid;
}

int ns_tibco_init_consumer( char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                                 char *tibco_password, int max_pool_size, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.max_pool_size = %d ", max_pool_size);
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Initializing consumer connection pool. Server ip/port: %s:%d, Topic/Queue: %s, Username: %s, Password: ****, Connection pool size: %d", tibco_hostname, tibco_port, tibco_topic_or_queue, tibco_userId, max_pool_size);
  int jpid;
  int is_new_key;

  if(!tibco_hostname || !tibco_port || !tibco_topic_or_queue)
    HANDLE_ERROR_DT("TIBCO-Error in consumer connection initialization", error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
  int len = strlen(tibco_hostname);
  char hostname_port[len+50];
  snprintf(hostname_port, len+50, "%s:%d", tibco_hostname, tibco_port);
  if(!tibco_userId)
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, "NA", "NA", "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);
  else if(!tibco_password)
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, tibco_userId, "NA", "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);
  else
    jpid = jms_cp_get_pool_id(tibco_hostname, tibco_port, hostname_port, "NA", "NA", tibco_topic_or_queue, tibco_userId, tibco_password, "NA", max_pool_size, CONSUMER, error_msg, &is_new_key);
  if(jpid < 0)
    return NS_JMS_ERROR_WRONG_USER_CONFIG;

  if(is_new_key)
  {
    TibcoConfig *jms_specific_config = jms_cp_set_jms_specific_config(jpid, sizeof(TibcoConfig));
    jms_specific_config->t_or_q = t_or_q;
    jms_specific_config->conn_timeout = 5;
    jms_specific_config->put_msg_timeout = 5;
    jms_specific_config->get_msg_timeout = 5;
    jms_specific_config->put_mode = TIBEMS_RELIABLE;
  }
  NS_DT2(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Consumer connection pool successfully initialized.", jpid);
  return jpid;
}

//This api will initialize tibco ssl ciphers
int ns_tibco_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg) 
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, ciphers = %s", jpid, ciphers);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl ciphers", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting ssl ciphers: %s", jpid, ciphers);
  if(!ciphers || !(*ciphers))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl ciphers", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->set_default_conf)
  {
    jms_specific_config->set_default_conf = 1;
    jms_specific_config->sslParams = tibemsSSLParams_Create();
  }
  if(TIBEMS_OK != tibemsSSLParams_SetCiphers(jms_specific_config->sslParams, ciphers))
  {
    tibemsSSLParams_Destroy(jms_specific_config->sslParams);
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl ciphers", jpid, error_msg, NS_JMS_ERROR_SETTING_CONFIG, NS_JMS_ERROR_SETTING_CONFIG_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, ssl ciphers set successfully", jpid);
  return 0;
}

//This api will initialize tibco ssl pvtKey File path 
int ns_tibco_set_ssl_pvt_key_file(int jpid, char *pvtKeyFilePath, char *error_msg) 
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, pvtKeyFilePath = %s", jpid, pvtKeyFilePath);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl private key", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting ssl private key: %s", jpid, pvtKeyFilePath);
  if(!pvtKeyFilePath || !(*pvtKeyFilePath))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl private key", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->set_default_conf)
  { 
    jms_specific_config->set_default_conf = 1;
    jms_specific_config->sslParams = tibemsSSLParams_Create();
  }
  if(TIBEMS_OK != tibemsSSLParams_SetPrivateKeyFile(jms_specific_config->sslParams, pvtKeyFilePath, TIBEMS_SSL_ENCODING_AUTO))
  { 
    tibemsSSLParams_Destroy(jms_specific_config->sslParams);
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl private key", jpid, error_msg, NS_JMS_ERROR_SETTING_CONFIG, NS_JMS_ERROR_SETTING_CONFIG_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Private key set successfully", jpid);
  return 0;
}

//This api will initialize tibco ssl trustedCA certificate file path
int ns_tibco_set_ssl_trusted_ca(int jpid, char *trustedCACertFilePath, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, trustedCACertFilePath = %s", jpid, trustedCACertFilePath);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl trusted ca certificate file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting ssl trusted ca certificate file path: %s", jpid, trustedCACertFilePath);
  if(!trustedCACertFilePath || !(*trustedCACertFilePath))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl trusted ca certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->set_default_conf)
  { 
    jms_specific_config->set_default_conf = 1;
    jms_specific_config->sslParams = tibemsSSLParams_Create();
  }
  jms_specific_config->is_verify_host = 1;
  if(TIBEMS_OK != tibemsSSLParams_AddTrustedCertFile(jms_specific_config->sslParams, trustedCACertFilePath, TIBEMS_SSL_ENCODING_AUTO))
  { 
    tibemsSSLParams_Destroy(jms_specific_config->sslParams);
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl trusted ca certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_CONFIG, NS_JMS_ERROR_SETTING_CONFIG_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Ssl trusted ca certificate file path set successfully", jpid);
  return 0;
}

//This api will initialize tibco ssl issuer certificatefile path
int ns_tibco_set_ssl_issuer(int jpid, char *issuerCertFilePath, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, issuerCertFilePath = %s", jpid, issuerCertFilePath);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl issuer certificate file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting ssl issuer certificate file path: %s", jpid, issuerCertFilePath);
  if(!issuerCertFilePath || !(*issuerCertFilePath))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl issuer certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->set_default_conf)
  { 
    jms_specific_config->set_default_conf = 1;
    jms_specific_config->sslParams = tibemsSSLParams_Create();
  }
  if(TIBEMS_OK != tibemsSSLParams_AddIssuerCertFile(jms_specific_config->sslParams, issuerCertFilePath, TIBEMS_SSL_ENCODING_AUTO))
  { 
    tibemsSSLParams_Destroy(jms_specific_config->sslParams);
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl issuer certificate file path", jpid, error_msg, NS_JMS_ERROR_SETTING_CONFIG, NS_JMS_ERROR_SETTING_CONFIG_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Ssl issuer certificate file path set successfully", jpid);
  return 0; 
}

//This api will initialize tibco ssl identity file path and password
int ns_tibco_set_ssl_identity(int jpid, char *identityFilePath, char *ssl_pwd, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, identityFilePath = %s", jpid, identityFilePath);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl identity file path", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting ssl identity file path: %s", jpid, identityFilePath);
  if(!identityFilePath || !(*identityFilePath))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl identity file path", jpid, error_msg, NS_JMS_ERROR_SETTING_VALUE_NULL, NS_JMS_ERROR_SETTING_VALUE_NULL_MSG);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
    //HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE, NS_JMS_ERROR_CONN_POOL_ALREADY_IN_USE_MSG);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->set_default_conf)
  { 
    jms_specific_config->set_default_conf = 1;
    jms_specific_config->sslParams = tibemsSSLParams_Create();
  }
  jms_specific_config->ssl_pwd = ssl_pwd;
  if(TIBEMS_OK != tibemsSSLParams_SetIdentityFile(jms_specific_config->sslParams, identityFilePath, TIBEMS_SSL_ENCODING_AUTO))
  { 
    tibemsSSLParams_Destroy(jms_specific_config->sslParams);
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting ssl identity file path", jpid, error_msg, NS_JMS_ERROR_SETTING_CONFIG, NS_JMS_ERROR_SETTING_CONFIG_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Ssl identity file path set successfully", jpid);
  return 0;
}

//Tibco set put msg mode
int ns_tibco_set_put_msg_mode(int jpid, int put_mode, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, put_mode = %d", jpid, put_mode);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting put message mode", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting put message mode: %d", jpid, put_mode);
  if(-1 == jms_cp_validate_conn(jpid))
    return 0;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(put_mode == 1)
    jms_specific_config->put_mode = put_mode;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Put message mode set successfully", jpid);
  return 0;
}

//Tibco set connection timeout
int ns_tibco_set_Connection_timeout(int jpid, double timeout , char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting connection timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting connection timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->conn_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection timeout set successfully", jpid);
  return 0;
}

//Tibco set get msg  timeout    
int ns_tibco_set_getMsg_timeout(int jpid, double timeout , char *error_msg)
{ 
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
  if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting get message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting get message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->get_msg_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Get message timeout set successfully", jpid);
  return 0;
} 

//Tibco set put msg  timeout    
int ns_tibco_set_putMsg_timeout(int jpid, double timeout , char *error_msg)
{
   NSDL2_JMS(NULL, NULL, "Method called.jpid = %d, timeout = %lf", jpid, timeout);
   if(-1 == jms_cp_validate_jpid(jpid))
    HANDLE_ERROR_DT_JPID("TIBCO-Error in setting put message timeout", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
   NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Setting put message timeout: %lf", jpid, timeout);
  if(timeout <= 0)
    timeout = 5;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->put_msg_timeout = timeout;
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Put message timeout set successfully", jpid); 
  return 0;
} 


static void end_tibco_error_transaction(char *transaction_name, char *operation)
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
 
static int ns_tibco_make_producer_connection(int jpcid, TibcoClientInfo *tci_ptr, char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, long timetolive, int timestamp_flag, tibemsSSLParams sslParams, char *pk_password, char is_verify_host, char *transaction_name, char *error_msg)
{ 
  NSDL4_JMS(NULL, NULL, "Method called. serverUrl = %s, username = %s, password = %s, t_or_q = %d, t_q_name = %s\n",
           serverUrl, username, password, t_or_q, t_q_name);
  // This method will enable additional error tracking
  tci_ptr->status = tibemsErrorContext_Create(&(tci_ptr->errorContext));
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsErrorContext_Create = %d\n", tci_ptr->status);
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if (tci_ptr->status != TIBEMS_OK)
  {
    NSDL1_JMS(NULL, NULL, "Context creation failed: %s\n", tibemsStatus_GetText(tci_ptr->status));
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "Context_Create");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, tibemsStatus_GetText(tci_ptr->status));
  }
  NSDL4_JMS(NULL, NULL, "Method called Context created succesfully\n");
  // Administer object for craeting server connection
  tci_ptr->factory = tibemsConnectionFactory_Create();
  if (!tci_ptr->factory)
  {
    ns_tibco_log_error("Error creating tibemsConnectionFactory", tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "ConnectionFactoryCreate");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsConnectionFactory");
  }

  /* Setting the TIMEOUT */
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  tci_ptr->status = tibemsConnectionFactory_SetConnectAttemptTimeout(tci_ptr->factory, (int)(jms_specific_config->conn_timeout * 1000));
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsConnectionFactory_SetConnectAttemptTimeout = %d\n", tci_ptr->status);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting TIMEOUT", tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "SetConnectAttemptTimeout");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting TIMEOUT");
  }
  // Sets the server url
  tci_ptr->status = tibemsConnectionFactory_SetServerURL(tci_ptr->factory,serverUrl);
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsConnectionFactory_SetServerURL = %d\n", tci_ptr->status);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting server url", tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "SetServerURL");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting server url");
  }
  if(sslParams)
  {
    NSDL1_JMS(NULL, NULL, "sslParams set");
    if(!is_verify_host)
      tibemsSSLParams_SetVerifyHost(sslParams,TIBEMS_FALSE);
    tci_ptr->status = tibemsConnectionFactory_SetSSLParams(tci_ptr->factory, sslParams);
    if(tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting sslParams", tci_ptr->errorContext, tci_ptr);
      tibemsSSLParams_Destroy(sslParams);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "SetSSLParams");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting sslParams");
    }
    tci_ptr->sslParams = sslParams;
    NSDL4_JMS(NULL, NULL, "Successfully set ssl params for producer\n");
    if(pk_password)
    {
      tci_ptr->status = tibemsConnectionFactory_SetPkPassword(tci_ptr->factory,pk_password);
      if(tci_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error setting ssl private key password", tci_ptr->errorContext, tci_ptr);
        tibemsSSLParams_Destroy(sslParams);
        if(transaction_name)
          end_tibco_error_transaction(transaction_name, "SetPkPassword");
        HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting ssl private key password");
      }
    }

  }
  // Creates the connection object
  tci_ptr->status = tibemsConnectionFactory_CreateConnection(tci_ptr->factory,&(tci_ptr->connection),
                            username,password);
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsConnectionFactory_CreateConnection = %d\n", tci_ptr->status);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsConnection", tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateConnection");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsConnection");
  }

  /* create the destination */
  if (t_or_q == NS_TIBCO_USE_TOPIC)
    tci_ptr->status = tibemsTopic_Create(&(tci_ptr->destination),t_q_name);
  else
    tci_ptr->status = tibemsQueue_Create(&(tci_ptr->destination),t_q_name);

  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsDestination",  tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateTibemsDestination");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsDestination");
  }

  /* create the session */
  tci_ptr->status = tibemsConnection_CreateSession(tci_ptr->connection,
                                                    &(tci_ptr->session),TIBEMS_FALSE,TIBEMS_NO_ACKNOWLEDGE);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsSession",  tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateSession");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsSession");
  }

  // create the producer 
  tci_ptr->status = tibemsSession_CreateProducer(tci_ptr->session,
                                                  &(tci_ptr->msgProducer), tci_ptr->destination);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsMsgProducer",  tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateProducer");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsMsgProducer");
  }

/* set the delivery mode */
    tci_ptr->status = tibemsMsgProducer_SetDeliveryMode(tci_ptr->msgProducer, jms_specific_config->put_mode);
    if (tci_ptr->status != TIBEMS_OK)
    {
        //fail("Error setting delivery mode", tci_ptr->errorContext);
      NSDL4_JMS(NULL, NULL, "Error setting delivery mode");
    }

    /* performance settings */
    tci_ptr->status = tibemsMsgProducer_SetDisableMessageID(tci_ptr->msgProducer,
                                                   TIBEMS_TRUE) ||
             tibemsMsgProducer_SetDisableMessageTimestamp(tci_ptr->msgProducer,
                                                          TIBEMS_TRUE);
    if (tci_ptr->status != TIBEMS_OK)
    {
        //fail("Error configuring tibemsMsgProducer", tci_ptr->errorContext);
      NSDL4_JMS(NULL, NULL, "Error configuring tibemsMsgProducer");
    }
  //set time stamp header flag.     
/*  if (timestamp_flag == NS_TIBCO_DISABLE_TIMESTAMP)
  {
    tci_ptr->status = tibemsMsgProducer_SetDisableMessageTimestamp(tci_ptr->msgProducer, TIBEMS_TRUE);
    if (tci_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting tibemsMsgProducer_SetDisableMessageTimestamp", tci_ptr->errorContext, tci_ptr);
      tibemsSSLParams_Destroy(sslParams);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name);
      HANDLE_ERROR_DT("",error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting tibemsMsgProducer_SetDisableMessageTimestamp");
    }
  }*/

  tci_ptr->status = tibemsMsgProducer_SetTimeToLive(tci_ptr->msgProducer, timetolive);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsMsgProducer",  tci_ptr->errorContext, tci_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "MsgProducerSetTimeToLive");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making producer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsMsgProducer");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
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

// Creating the wrapper for create consumer api for durable and non durable consumer 
static int ns_tibco_make_consumer_connection(int jpcid, TibcoClientReceiveInfo *tri_ptr, char *serverUrl, char *username, char *password, int t_or_q, char *t_q_name, int consumerType, char *durableName, tibemsSSLParams sslParams, char *pk_password, char is_verify_host, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called. serverUrl = %s, username = %s, t_or_q = %d, t_q_name = %s\n",
           serverUrl, username, t_or_q, t_q_name);
  set_default_value_for_consumer(tri_ptr);
  // This method will enable additional error tracking
  tri_ptr->status = tibemsErrorContext_Create(&(tri_ptr->errorContext));
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsErrorContext_Create = %d\n", tri_ptr->status);
  if(transaction_name)
    ns_start_transaction(transaction_name);
  if (tri_ptr->status != TIBEMS_OK)
  {
    NSDL1_JMS(NULL, NULL, "Context creation failed: %s\n", tibemsStatus_GetText(tri_ptr->status));
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "ContextCreate");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, tibemsStatus_GetText(tri_ptr->status));
  }
  NSDL4_JMS(NULL, NULL, "Method called Context created succesfully\n");
  // Administer object for craeting server connection
  tri_ptr->factory = tibemsConnectionFactory_Create();
  if (!tri_ptr->factory)
  {
    ns_tibco_log_error("Error creating tibemsConnectionFactory", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "TibemsConnectionFactory");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsConnectionFactory");
  }
  /* Setting the TIMEOUT */
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  tri_ptr->status = tibemsConnectionFactory_SetConnectAttemptTimeout(tri_ptr->factory, (int)(jms_specific_config->conn_timeout * 1000));
  NSDL4_JMS(NULL, NULL, "Method called.. Status for tibemsConnectionFactory_SetConnectAttemptTimeout = %d\n", tri_ptr->status);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting TIMEOUT", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "SetConnectAttemptTimeout");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting TIMEOUT");
  }
  // Set the server url 
  tri_ptr->status = tibemsConnectionFactory_SetServerURL(tri_ptr->factory,serverUrl);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting server url", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "SetServerURL");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting server url");
  }
  //set ssl params.
  if(sslParams)
  {
    NSDL1_JMS(NULL, NULL, "sslParams set");
    if(!is_verify_host)
    {
      NSDL1_JMS(NULL, NULL, "Verify Tibco host disabled");
      tibemsSSLParams_SetVerifyHost(sslParams,TIBEMS_FALSE);
    }
    tri_ptr->status = tibemsConnectionFactory_SetSSLParams(tri_ptr->factory, sslParams);
    if(tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error setting sslParams", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      tibemsSSLParams_Destroy(sslParams);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "SetSSLParams");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting sslParams");
    }
    tri_ptr->sslParams = sslParams;
    //set private key password.
    if(pk_password)
    {
      tri_ptr->status = tibemsConnectionFactory_SetPkPassword(tri_ptr->factory,pk_password);
      if(tri_ptr->status != TIBEMS_OK)
      {
        ns_tibco_log_error("Error setting ssl private key password", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
        tibemsSSLParams_Destroy(sslParams);
        if(transaction_name)
          end_tibco_error_transaction(transaction_name, "SetPkPassword");
        HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error setting ssl private key password");
      }
    }
  }
  // Creates the connection
  tri_ptr->status = tibemsConnectionFactory_CreateConnection(tri_ptr->factory,&(tri_ptr->connection),
                                                    username,password);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsConnection", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateConnection");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsConnection");
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
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreatingTibemsDestination");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsDestination");
  }

  /* create the session */
  tri_ptr->status = tibemsConnection_CreateSession(tri_ptr->connection,
                                                      &(tri_ptr->session),TIBEMS_FALSE, tri_ptr->ackMode);
  if (tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error creating tibemsSession", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CreateSession");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsSession");
  }

  /* create the consumer */
  if(consumerType == NON_DURABLE_CONSUMER)
  {
    tri_ptr->status = tibemsSession_CreateConsumer(tri_ptr->session,
                                         &(tri_ptr->msgConsumer),tri_ptr->destination, NULL, TIBEMS_FALSE);
    if (tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error creating tibemsMsgConsumer", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      tibemsSSLParams_Destroy(sslParams);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "CreateConsumer");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating tibemsMsgConsumer");
    }
  }
  else
  {
    tri_ptr->status = tibemsSession_CreateDurableSubscriber(tri_ptr->session,
                                         &(tri_ptr->msgConsumer),tri_ptr->destination, durableName, NULL, TIBEMS_FALSE);
    if (tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error creating DurableSubscriber", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      tibemsSSLParams_Destroy(sslParams);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "CreateDurableSubscriber");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error creating DurableSubscriber");
    }
  }
  tri_ptr->status = tibemsConnection_Start(tri_ptr->connection);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error starting tibemsConnection for recieving", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    tibemsSSLParams_Destroy(sslParams);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "ConnectionStart");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in making consumer connection", jpcid, error_msg, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL, "Error starting tibemsConnection for recieving");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will get the connection from connection pool and make it with Tibco server if not already made
int ns_tibco_get_connection(int jpid,  char *transaction_name, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpid = %d", jpid);
  if(-1 == jms_cp_validate_jpid(jpid))
  { 
    start_end_tx_on_error(transaction_name, "InvalidPoolId");
    HANDLE_ERROR_DT_JPID("TIBCO-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_INVALID_JPID, NS_JMS_ERROR_INVALID_JPID_MSG);
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Creating connection with server.", jpid);
  int jpcid;
  JMSConnection *node = NULL;
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  jpcid = jms_cp_get_connection(jpid, &node);
  NSDL4_JMS(NULL, NULL, "jpcid = %d", jpcid);
  if(jpcid >= 0)
  {
    NSDL4_JMS(NULL, NULL, "valid jpcid node = %p, node->conn_config = %p", node,node ? node->conn_config:NULL  );
    if(node && !(node->conn_config))
    {
      NSDL4_JMS(NULL, NULL, "Going to malloc node->conn_config");
      if(jms_cp_get_client_type(jpid, -1) == PRODUCER)
      {
        jms_cp_set_conn_specific_config(jpcid, sizeof(TibcoClientInfo));
        if(NS_JMS_ERROR_LIB_ERROR_CONN_FAIL == ns_tibco_make_producer_connection(jpcid, (TibcoClientInfo *)(node->conn_config), user_config->hostname_port, user_config->userId, user_config->password, jms_specific_config->t_or_q, user_config->queue, jms_specific_config->timetolive, jms_specific_config->timestamp_flag, jms_specific_config->sslParams, jms_specific_config->ssl_pwd, jms_specific_config->is_verify_host, transaction_name, error_msg))
        {
          jms_specific_config->set_default_conf = 0;
          jms_cp_send_conn_to_nc_list(jpid, node);
          //start_end_tx_on_error(transaction_name, "LibConnFail");
          NS_DT4(NULL, NULL, DM_L1, MM_JMS,"TIBCO-Pool id: %d, Error: %s", jpid, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG);
          return NS_JMS_ERROR_LIB_ERROR_CONN_FAIL;
        }
      }
      else
      {
        jms_cp_set_conn_specific_config(jpcid, sizeof(TibcoClientReceiveInfo));
        if(NS_JMS_ERROR_LIB_ERROR_CONN_FAIL == ns_tibco_make_consumer_connection(jpcid, (TibcoClientReceiveInfo *)(node->conn_config), user_config->hostname_port, user_config->userId, user_config->password, jms_specific_config->t_or_q, user_config->queue, NON_DURABLE_CONSUMER, NULL, jms_specific_config->sslParams, jms_specific_config->ssl_pwd, jms_specific_config->is_verify_host, transaction_name, error_msg))
        {
          jms_specific_config->set_default_conf = 0;
          jms_cp_send_conn_to_nc_list(jpid, node);
          //start_end_tx_on_error(transaction_name, "LibConnFail");
          NS_DT4(NULL, NULL, DM_L1, MM_JMS,"TIBCO-Pool id: %d, Error: %s", jpid, NS_JMS_ERROR_LIB_ERROR_CONN_FAIL_MSG);
          return NS_JMS_ERROR_LIB_ERROR_CONN_FAIL;
        }
      }
    }
  }
  else
  {
    start_end_tx_on_error(transaction_name, "PoolFinished");
    HANDLE_ERROR_DT_JPID("TIBCO-Error in getting connection", jpid, error_msg, NS_JMS_ERROR_CONN_POOL_FINISHED, NS_JMS_ERROR_CONN_POOL_FINISHED_MSG);
  }

  NS_JMS_SET_OWNER(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Connection successfully created.", jpid, GET_JCID(jpcid));
  return jpcid;
}

//This api will set any custom header in message
int ns_tibco_set_message_header(int jpcid, char *error_msg, char *header_name, int value_type, ...)
{
  NSDL2_JMS(NULL, NULL, "Method called.jpcid = %d, header_name = %s, value_type = %d", jpcid, header_name, value_type);
  int ret;
  if((ret = jms_cp_validate_jpcid(jpcid, NULL, error_msg)) < 0)
    return (ret);
  #ifdef NS_DEBUG_ON
  char log[128]; 
  #endif
  if(!header_name || !(*header_name))
  {
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting connection", jpcid, error_msg, NS_JMS_ERROR_HEADER_NAME_NULL, NS_JMS_ERROR_HEADER_NAME_NULL_MSG);
  }
  va_list ap;
  headerValue value;


  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  TibcoClientInfo *tci_ptr = (TibcoClientInfo *)(node->conn_config);
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->is_msg_created)
  {
    tci_ptr->status = tibemsTextMsg_Create(&(tci_ptr->msg));
    if (tci_ptr->status != TIBEMS_OK)
    {
      tci_ptr->msg = NULL;
      ns_tibco_log_error("Error creating tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting connection", jpcid, error_msg, NS_JMS_ERROR_HEADER_VALUE_SET_FAIL, "Error creating tibemsTextMsg");
    }
    jms_specific_config->is_msg_created = 1;
  }
  tci_ptr->status = TIBEMS_OK;
  va_start(ap, value_type);
  switch(value_type)
  {
    case NS_TIBCO_BOOLEAN:
      value.int_val = va_arg(ap, int);
      if(value.int_val != 0)
        value.int_val = 1;
        //change here
      else
        value.int_val = 2;
        tci_ptr->status = tibemsMsg_SetBooleanProperty(tci_ptr->msg, header_name, value.int_val);
	#ifdef NS_DEBUG_ON
          sprintf(log,"%d", value.int_val);
          NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%d", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
          ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid); 
        #endif 
      break;
    case NS_TIBCO_STRING:
      value.string_val= va_arg(ap, char *);
      NSDL2_JMS(NULL, NULL, "value.string_val = %s", value.string_val);
      tci_ptr->status = tibemsMsg_SetStringProperty(tci_ptr->msg, header_name, value.string_val);
      #ifdef NS_DEBUG_ON
          NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%s", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
      ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, value.string_val, jpcid);
      #endif
       break;
    case NS_TIBCO_INTEGER:
      value.int_val = va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetIntProperty(tci_ptr->msg, header_name, value.int_val);
      #ifdef NS_DEBUG_ON
        sprintf(log,"%d",value.int_val);
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%d", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
        ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid);
      #endif
      break;
    case NS_TIBCO_DOUBLE:
      value.double_val = va_arg(ap, double);
      tci_ptr->status = tibemsMsg_SetDoubleProperty(tci_ptr->msg, header_name, value.double_val);
      #ifdef NS_DEBUG_ON
        sprintf(log,"%lf",value.double_val);
          NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%lf", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
        ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid);
      #endif
      break;
    case NS_TIBCO_FLOAT:
      value.float_val = (float)va_arg(ap, double);
      tci_ptr->status = tibemsMsg_SetFloatProperty(tci_ptr->msg, header_name, value.float_val);
      #ifdef NS_DEBUG_ON
        sprintf(log,"%f",value.float_val);
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%f", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
        ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid);
      #endif
      break;
    case NS_TIBCO_SHORT:
      value.short_val = (short)va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetShortProperty(tci_ptr->msg, header_name, value.short_val);
      #ifdef NS_DEBUG_ON
        sprintf(log,"%u",value.short_val);
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%u", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
        ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid);
      #endif
      break;
    case NS_TIBCO_BYTE:
      value.char_val = (char)va_arg(ap, int);
      tci_ptr->status = tibemsMsg_SetByteProperty(tci_ptr->msg, header_name, value.char_val);
      #ifdef NS_DEBUG_ON
        sprintf(log,"%c", value.char_val);
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Setting message header. header name/value: %s:%c", GET_JPID(jpcid), GET_JCID(jpcid), header_name, log);
        ns_jms_logs_req_resp(NULL, JMS_SENT_MSG, "TIBCO", NULL, 0, NULL, header_name, log, jpcid);
      #endif
      break;
    default:
      NSDL2_JMS(NULL, NULL, "Header value type is not valid");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting connection", jpcid, error_msg, NS_JMS_ERROR_HEADER_VALUE_TYPE_INVALID, NS_JMS_ERROR_HEADER_VALUE_TYPE_INVALID_MSG);
  }
  va_end(ap);
  if(tci_ptr->status != TIBEMS_OK)
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting connection", jpcid, error_msg, NS_JMS_ERROR_HEADER_VALUE_SET_FAIL, NS_JMS_ERROR_HEADER_VALUE_SET_FAIL_MSG);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Message header set successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

static int ns_tibco_send_message(int jpcid, TibcoClientInfo *tci_ptr, char *data, int msg_len, char *transaction_name, char *error_msg)
{
  
  /* publish messages */
  /* create the text message */
  NSDL2_JMS(NULL, NULL, "Method called.jpcid = %d, Transaction name = %s, Error_msg =%s", jpcid, transaction_name, error_msg);
  if(transaction_name)
    ns_start_transaction(transaction_name);
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  if(!jms_specific_config->is_msg_created)
  {
    tci_ptr->status = tibemsTextMsg_Create(&(tci_ptr->msg));
    if (tci_ptr->status != TIBEMS_OK)
    {
      tci_ptr->msg = NULL;
      ns_tibco_log_error("Error creating tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "TextMsgCreate");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, "Error creating tibemsTextMsg");
    }
    jms_specific_config->is_msg_created = 1;
  }
  /* set the message text */
  tci_ptr->status = tibemsTextMsg_SetText(tci_ptr->msg, data);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error setting tibemsTextMsg text", tci_ptr->errorContext, tci_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "TextMsgSetText");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, "Error setting tibemsTextMsg text");
  }
#if 0
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
  NSDL4_JMS(NULL, NULL, "Method called.. Status for  tibemsMsgProducer_Send = %d\n", tci_ptr->status);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error publishing tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "MsgProducerSend");
    if (tci_ptr->status != TIBEMS_DESTINATION_LIMIT_EXCEEDED)
      ns_tibco_close_connection(jpcid, "TIBCOProducerClosePutError", error_msg);
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, "Error setting tibemsTextMsg text");
  }
  else 
  {
    #ifdef NS_DEBUG_ON
      ns_jms_logs_req_resp(user_config->hostname_port, JMS_SENT_MSG, "TIBCO", user_config->queue, jms_specific_config->t_or_q, data, NULL, NULL, jpcid);
    #endif

    NSDL4_JMS(NULL, NULL, "Published message: %s\n",data);
  }
  /* destroy the message */
  tci_ptr->status = tibemsMsg_Destroy(tci_ptr->msg);
  jms_specific_config->is_msg_created = 0; 
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsTextMsg", tci_ptr->errorContext, tci_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "DestroyTibemsTextMsg");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_PUT_MSG_FAIL, "Error destroying tibemsTextMsg");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Message was sent successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

//This api will put message on Tibco server
int ns_tibco_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg )
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d, msg = %s, msg_len = %d, Transaction_name = %s", jpcid, msg, msg_len, transaction_name);
  int ret; 
  //Validate jpcid
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret);

#ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Putting message to topic/queue: %s", jpid, GET_JCID(jpcid), user_config->queue);
#endif

  if(!msg || !(*msg))
  {
    start_end_tx_on_error(transaction_name, "PutMessageNull");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }

  // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != PRODUCER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in putting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG); 
  }

  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  return (ns_tibco_send_message(jpcid, (TibcoClientInfo *)(node->conn_config), msg, msg_len, transaction_name, error_msg));
}

static int ns_tibco_get_header(TibcoClientReceiveInfo *tci_ptr, char *header, int hdr_len, int jpcid)
{
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Getting message header.", GET_JPID(jpcid), GET_JCID(jpcid));
  NSDL4_JMS(NULL, NULL, "Method called\n");
  long timestamp;
  tibems_status status;
  int header_len = 0;
  //get timestamp.
  status = tibemsMsg_GetTimestamp(tci_ptr->msg, (tibems_long *)&timestamp);
  if(status != TIBEMS_OK)
  {
    NSDL2_JMS(NULL, NULL, "Error in seaching timestamp header in jms message\n");
    return -1;
  }
  timestamp = timestamp / 1000;
  if(timestamp != 0)
  {
    struct tm *timeinfo = NULL;
    timeinfo = localtime(&timestamp);
    header_len = snprintf(header, hdr_len, "TIME-STAMP: %s", asctime(timeinfo));
  }
  tibemsMsgEnum propertyList;
  status = tibemsMsg_GetPropertyNames(tci_ptr->msg, &propertyList);
  if(status != TIBEMS_OK)
  {
    NSDL2_JMS(NULL, NULL, "Error in getting property list from the message\n");
    if(status == TIBEMS_NOT_FOUND)
      NSDL2_JMS(NULL, NULL, "No property found in this message\n");
    return header_len;
  }
  const char *propertyName;
  tibemsMsgField value;
  char valueBuffer[512] = "";
  int i = 0;
  char *tmp;
  while(1)
  {
    status = tibemsMsgEnum_GetNextName(propertyList, &propertyName);
    if(status == TIBEMS_NOT_FOUND)
      break;
    if(propertyName == NULL)
      break;
    status = tibemsMsg_GetProperty(tci_ptr->msg, propertyName, &value);
    if(status != TIBEMS_OK)
    {
      NSDL2_JMS(NULL, NULL, "Failed to get property %s's value\n", propertyName);
      break;
    }
    status = tibemsMsgField_PrintToBuffer(&value, valueBuffer, 512);
    if(status != TIBEMS_OK)
    {
      NSDL2_JMS(NULL, NULL, "Failed to copy property value to buffer\n");
      break;
    }
    /*As value will come in Type:Value form and we just need value,so we will take second field */
    tmp = strchr(valueBuffer, ':');
    if(!tmp)
      tmp = valueBuffer;
    else
      tmp ++;
    header_len += snprintf(header + header_len, hdr_len - header_len, "%s:%s\n", propertyName, tmp);
    i++;
    #ifdef NS_DEBUG_ON
      ns_jms_logs_req_resp(NULL, JMS_RECV_MSG, "TIBCO", NULL, 0, NULL, (char *)propertyName, tmp, 0);
    #endif 
    /*Break if max fields reached */
    if(i == 128)
      break;
  }
  /*Free enumuration*/
  status = tibemsMsgEnum_Destroy(propertyList);
  if(status != TIBEMS_OK)
  {
    NSDL2_JMS(NULL, NULL, "Failed to release tibco memory\n");
  }
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Message header get successfully", GET_JPID(jpcid), GET_JCID(jpcid));
  return header_len;
}

static int ns_tibco_receive_message(int jpcid, TibcoClientReceiveInfo *tri_ptr, char *buff, int msg_len, char *header, int header_len, char *transaction_name, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called\n");
  if(transaction_name)
    ns_start_transaction(transaction_name);
  /* receive the message */
  JMSConfig *user_config = jms_cp_get_user_config(GET_JPID(jpcid));
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  tri_ptr->status = tibemsMsgConsumer_ReceiveTimeout(tri_ptr->msgConsumer, &(tri_ptr->msg), (int)(jms_specific_config->get_msg_timeout * 1000));
  if(tri_ptr->status != TIBEMS_OK)
  {
    if(tri_ptr->status == TIBEMS_INTR)
    {
      ns_tibco_log_error("Error receiving message", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "MessageReceive");
      ns_tibco_close_connection(jpcid, "TIBCOConsumerCloseGetError", error_msg);
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error in receiving message");
    }
    else if(tri_ptr->status == TIBEMS_TIMEOUT)
    {
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "ConsumeMsgTimeout");
      //ns_tibco_close_connection(jpcid, "TIBCOConsumerCloseGetError", error_msg);
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Message Recieve Timeout");
    }
  }
  if(!(tri_ptr->msg))
  {
    if(transaction_name)
        end_tibco_error_transaction(transaction_name, "NoMsg");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "receiving message is NULL");
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
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "AcknowledgingMessage");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error acknowledging message");
    }
  }
  /* check message type */
  tri_ptr->status = tibemsMsg_GetBodyType(tri_ptr->msg,&(tri_ptr->msgType));
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error getting message type", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "GettingMsgType");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error getting message type");
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
    NSDL2_JMS(NULL, NULL, "Received %s message:\n",tri_ptr->msgTypeName);
    tibemsMsg_Print(tri_ptr->msg);
    strncpy(buff, tri_ptr->txt, msg_len);
  }
  else
  {
    /* get the message text */
    tri_ptr->status = tibemsTextMsg_GetText(tri_ptr->msg,&(tri_ptr->txt));
    strncpy(buff, tri_ptr->txt, msg_len);
    if (tri_ptr->status != TIBEMS_OK)
    {
      ns_tibco_log_error("Error getting tibemsTextMsg text", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "TibemsTextMsg");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error getting tibemsTextMsg text");
    }
  }
  buff[msg_len] = '\0';
  if(header)
  {
    if(ns_tibco_get_header(tri_ptr, header, header_len, jpcid) == -1)
    {
      ns_tibco_log_error("Error in getting Tibco Headers", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
      if(transaction_name)
        end_tibco_error_transaction(transaction_name, "GettingHeaders");
      HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error in getting Tibco Headers");
    }
  }

  #ifdef NS_DEBUG_ON
    ns_jms_logs_req_resp(user_config->hostname_port, JMS_RECV_MSG, "TIBCO", user_config->queue, jms_specific_config->t_or_q, buff, NULL, NULL, jpcid);
  #endif

  /* destroy the message */
  tri_ptr->status = tibemsMsg_Destroy(tri_ptr->msg);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsMsg",  tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "DestroyTibemsMsg");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_LIB_ERR_GET_MSG_FAIL, "Error destroying tibemsMsg");
  }
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Message was get successfully", GET_JPID(jpcid), GET_JCID(jpcid));  
  return 0;
}


//This api will get  message from Tibco server
int ns_tibco_get_msg(int jpcid, char *msg, int msg_len, char *header, int header_len, char *transaction_name, char *error_msg) 
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d, msg = %s, msg_len = %d, header = %s,header_len = %d ", jpcid, msg, msg_len, header, header_len);
  int ret;
  //Validate jpcid
  if((ret = jms_cp_validate_jpcid(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
  #ifdef NS_DEBUG_ON
  IW_UNUSED(int jpid = GET_JPID(jpcid));
  IW_UNUSED(JMSConfig *user_config = jms_cp_get_user_config(jpid));
  
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d, msg = %s, msg_len = %d", jpcid, msg, msg_len);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Getting message from topic/queue: %s", jpid, GET_JCID(jpid), user_config->queue);
#endif
  if(!msg)
  {
    start_end_tx_on_error(transaction_name, "GetMsgBufPtrNull");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_MESSAGE_NULL, NS_JMS_ERROR_MESSAGE_NULL_MSG);
  }
  // check jms_client_type is either PRODUCER or CONSUMER before put message 
  // if jms_client_type is CONSUMER then get error 
  if((jms_cp_get_client_type(-1, jpcid)) != CONSUMER)
  {
    start_end_tx_on_error(transaction_name, "InvalidClientType");
    HANDLE_ERROR_DT_JPCID("TIBCO-Error in getting message", jpcid, error_msg, NS_JMS_ERROR_INVALID_CLIENT_TYPE , NS_JMS_ERROR_INVALID_CLIENT_TYPE_MSG); 
  }

  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  return (ns_tibco_receive_message(jpcid, (TibcoClientReceiveInfo *)(node->conn_config), msg, msg_len, header, header_len, transaction_name, error_msg));
}

//This api will release the  connection from connection pool and it should be called every time so that another user can reuse it.
int ns_tibco_release_connection(int jpcid, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  int ret; 
  if((ret = jms_cp_validate_jpcid(jpcid, NULL, error_msg)) < 0)
    return (ret);
 
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Releasing connection.", GET_JPID(jpcid), GET_JCID(jpcid));
  jms_cp_release_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Connection released successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0;
}

// Closing the Consumer Side
static int ns_tibco_close_consumer(TibcoClientReceiveInfo *tri_ptr, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called");
  /* destroy the destination */
  if(transaction_name)
    ns_start_transaction(transaction_name);
  tri_ptr->status = tibemsMsgConsumer_Close(tri_ptr->msgConsumer);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsMsgConsumer", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CloseTibemsMsgConsumer");
    HANDLE_ERROR_DT("TIBCO-Error in closing consumer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error closing tibemsMsgConsumer");
  }
  tri_ptr->status = tibemsSession_Close(tri_ptr->session);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing consumer session", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CloseConsumerSession");
    HANDLE_ERROR_DT("TIBCO-Error in closing consumer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error closing consumer session");
  }
  tri_ptr->status = tibemsDestination_Destroy(tri_ptr->destination);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsDestination", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "DestroyTibemsDestination");
    HANDLE_ERROR_DT("TIBCO-Error in closing consumer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error destroying tibemsDestination");
  }
  /* close the connection */
  tri_ptr->status = tibemsConnection_Close(tri_ptr->connection);
  if(tri_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsConnection", tri_ptr->errorContext, (TibcoClientInfo *)tri_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CloseTibemsConn");
    HANDLE_ERROR_DT("TIBCO-Error in closing consumer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error closing tibemsConnection");
  }
  tibemsErrorContext_Close(tri_ptr->errorContext);
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

static int ns_tibco_close_producer(TibcoClientInfo *tci_ptr, char *transaction_name, char *error_msg)
{
  NSDL4_JMS(NULL, NULL, "Method called");
  /* destroy the destination */
  if(transaction_name)
    ns_start_transaction(transaction_name);
  tci_ptr->status = tibemsDestination_Destroy(tci_ptr->destination);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error destroying tibemsDestination", tci_ptr->errorContext, tci_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "DestroyTibemsDestination");
    HANDLE_ERROR_DT("TIBCO-Error in closing producer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error destroying tibemsDestination");
  }
  /* close the connection */
  tci_ptr->status = tibemsConnection_Close(tci_ptr->connection);
  if (tci_ptr->status != TIBEMS_OK)
  {
    ns_tibco_log_error("Error closing tibemsConnection", tci_ptr->errorContext, tci_ptr);
    if(transaction_name)
      end_tibco_error_transaction(transaction_name, "CloseTibemsConn");
    HANDLE_ERROR_DT("TIBCO-Error in closing producer connection", error_msg, NS_JMS_ERROR_CLOSE_CONNECTION, "Error closing tibemsConnection");
  }
  tibemsErrorContext_Close(tci_ptr->errorContext);
  if(transaction_name)
    ns_end_transaction(transaction_name, NS_AUTO_STATUS);
  return 0;
}

//This api will close connection with Tibco server and it should not be called every time as it will impact performance. It should be called at the end of test.
int ns_tibco_close_connection(int jpcid,  char *transaction_name, char *error_msg)
{
  int ret, jpid;
  NSDL4_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  if((ret = jms_cp_validate_jpcid_for_close_conn(jpcid, transaction_name, error_msg)) < 0)
    return (ret);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Closing connection.", GET_JPID(jpcid), GET_JCID(jpcid));
  jpid = GET_JPID(jpcid);
  JMSConfig *user_config = jms_cp_get_user_config(jpid);
  TibcoConfig *jms_specific_config = user_config->jms_specific_config;
  jms_specific_config->set_default_conf = 0;
  JMSConnection *node = jms_cp_get_conn_from_jpcid(jpcid); //get conn pointer from jpcid
  // check jms_client_type is either PRODUCER or CONSUMER
  if((jms_cp_get_client_type(-1, jpcid)) == PRODUCER)
    ns_tibco_close_producer((TibcoClientInfo *)node->conn_config, transaction_name, error_msg);
  else
    ns_tibco_close_consumer((TibcoClientReceiveInfo *)node->conn_config, transaction_name, error_msg);
  jms_cp_close_connection(jpcid);
  NS_DT3(NULL, NULL, DM_L1, MM_JMS, "TIBCO-Pool id: %d, Connection id: %d, Connection closed successfully.", GET_JPID(jpcid), GET_JCID(jpcid));
  return 0; 
}

//This api will remove the pool size 
int ns_tibco_shutdown(int jpcid,  char *transaction_name) 
{
  NSDL2_JMS(NULL, NULL, "Method called.jpcid = %d", jpcid);
  return 1;
}

#if 0
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

#endif

#ifdef JMS_API_TEST

/*

To compile Program :
----------------------------
gcc -DJMS_API_TEST -o tibco_test  -I../../ns_global_settings.h -I./../../libnscore -I/usr/include/postgresql/ -I/usr/include/libxml2 -I../../thirdparty/nghttp2/lib/includes/ -I../../ns_global_settings.h -g ns_jms_conn_pool.c ns_tibco_api.c ../../libnscore/nslb_alloc.c ../../libnscore/nslb_mem_map.c ../../libnscore/nslb_get_norm_obj_id.c ../../libnscore/nslb_mem_pool.c ../../libnscore/nslb_util.c ../../libnscore/nslb_big_buf.c ../../ns_tls_utils.c -lpthread -ltibems64 -L /home/cavisson/work/Master_work/cavisson/src/thirdparty/libtibcoems-8.3/lib -lm 2>err
export LD_LIBRARY_PATH=/home/cavisson/work/Master_work/cavisson/src/thirdparty/libtibcoems-8.3/lib

*/

int main(int argc, char *argv[])
{
  int jid;
  int cid;
  char error_buff[1024 + 1]; 
  char get_msg[1024 + 1];
  char get_header[1024 + 1];
  char buff[MAX_DATA_LINE_LENGTH + 1];

  FILE *fp_conf_file;
  char conf_file_path[1024] = "";
  char host[1024] = "";
  char queue_topic_name[1024] = "";
  char user_name[1024] = "";
  char password[1024] = "";
  char tx_name[1024] = "";
  char ca_cert_file_path[1024];
  char issuer_cert_file_path[1024];
  char identity_file_path[1024];
  char private_key_file_path[1024];

  int get_or_put = 1;
  int queue_or_topic = 1;
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

    if(strncmp(keyword, "QUEUE_TOPIC", 11) == 0)
    {
      queue_or_topic = atoi(text1);
      if(queue_or_topic == 1)
        strcpy(queue_topic_name, text2);
      else if(queue_or_topic == 2)
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

    if(strncmp(keyword, "PVT_KEY_FILE_PATH", 17) == 0)
      strcpy(private_key_file_path, text1);

    if(strncmp(keyword, "CA_CERT_FILE_PATH", 17) == 0)
      strcpy(ca_cert_file_path, text1);    

    if(strncmp(keyword, "ISSUER_CERT_FILE_PATH", 21) == 0)
      strcpy(issuer_cert_file_path, text1);

    if(strncmp(keyword, "IDENTITY_FILE_PATH", 18) == 0)
      strcpy(identity_file_path, text1);
  }
  fclose(fp_conf_file);
  
  if(get_or_put == 2)
    jid = ns_tibco_init_producer(host, port, queue_or_topic, queue_topic_name, user_name, password, 10, error_buff);
  else
    jid = ns_tibco_init_consumer(host, port, queue_or_topic, queue_topic_name, user_name, password, 10, error_buff);

  if(private_key_file_path)
    ns_tibco_set_ssl_pvt_key_file(jid, "charpvtKeyFilePath", error_buff);
  if(ca_cert_file_path)
    ns_tibco_set_ssl_trusted_ca(jid, "trustedCACertFilePath", error_buff);
  if(issuer_cert_file_path)
    ns_tibco_set_ssl_issuer(jid, "issuerCertFilePath", error_buff);
  if(identity_file_path)
    ns_tibco_set_ssl_identity(jid, "identityFilePath", "ssl_pwd", error_buff);

  cid = ns_tibco_get_connection(jid,  tx_name, error_buff);
        ns_tibco_set_message_header(cid, error_buff, "abcd", 18, "1234");  
  if(get_or_put == 2)
    ns_tibco_put_msg(cid, "tibco put Message", 1024, tx_name, error_buff );
  else
    ns_tibco_get_msg(cid, get_msg, 1024, get_header, 1024 , "transaction_name", error_buff);

  printf(" received message is %s\n",get_msg);
  ns_tibco_release_connection(cid, error_buff);
  return 0;
}

#endif

