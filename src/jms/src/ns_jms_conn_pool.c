/******************************************************************
 * Name    :    ns_jms_conn_pool.c
 * Purpose :    This file contains methods of JMS connection pool
 * Note    :    It is generic file for creating and maintaining JMS
                connection pool which should not change for any new 
                JMS type support addition in NS.
 * Author  :    Vaibhav Mittal
 * Intial version date:    16/05/2019
 * Last modification date: 16/05/2019
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include "../../ns_log.h"
#include "../../ns_msg_def.h"
#include "ns_jms_conn_pool.h"
#include "../../ns_string.h"
#include "../../libnscore/nslb_get_norm_obj_id.h"
#include "../../libnscore/nslb_alloc.h"
#include "../../ns_alloc.h"
#include "ns_jms_error.h"
/* Added for Msg_com_con and buffer_key */
#include "../../util.h"
#include "../../netstorm.h"
#include "../../ns_vuser_thread.h"
#include "../../ns_debug_trace.h"

#ifdef JCP_CP_TEST
#define NSDL1_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL2_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL3_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#define NSDL4_JMS(vptr, cptr, ...)  { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }
#endif

static NormObjKey jcp_cp_norm_tbl; //Hash map
static JMSConnectionPool *jms_connection_pool_ptr = NULL; //jms connection pool pointer
static int jms_cp_total_key_structs = 0; //total entries used in hash map
static int jms_cp_max_key_structs = 0; //total entried malloc'ed in hash map
static pthread_mutex_t jms_cp_pool_lock = PTHREAD_MUTEX_INITIALIZER;

/* Function used to malloc hash table key structure 
 *
 * Returns: It returns error or success of api
 * */
static int jms_cp_create_jms_conn_pool_key_structure()
{ 
  NSDL2_JMS(NULL, NULL, "Method called. jms_cp_total_key_structs = %d, jms_cp_max_key_structs = %d\n", jms_cp_total_key_structs, jms_cp_max_key_structs);
  
  if (jms_cp_total_key_structs == jms_cp_max_key_structs)  
  {
     NSLB_REALLOC_AND_MEMSET(jms_connection_pool_ptr, ((jms_cp_max_key_structs + DELTA_CONN_POOL_ENTRIES) * sizeof(JMSConnectionPool)), (jms_cp_max_key_structs * sizeof(JMSConnectionPool)), "jms_connection_pool_ptr", -1, NULL);
     jms_cp_max_key_structs += DELTA_CONN_POOL_ENTRIES;
  }
  jms_cp_total_key_structs++;
  NSDL2_JMS(NULL, NULL, "jms_cp_total_key_structs = %d, jms_cp_max_key_structs = %d\n", jms_cp_total_key_structs, jms_cp_max_key_structs);
  return 0;
}

/* Function used to allocate jms connections dynamically
 * Allocate connections_chunk with size of max pool size given by user of connection structure
 * Link connection list and made last entry NULL
 * Updated connection structure members
 * Use global pointer to connect different connection chunks, need to save link list
 * jpid: index in hash map i.e jms connection pool id
 * max_pool_size: maximum pool size initialized by user
 *
 * Returns: void
 * */
static void jms_cp_allocate_jms_connection_pool(int jpid, int max_pool_size)
{
  int i;
  JMSConnection* connections_chunk = NULL;

  NSDL2_JMS(NULL, NULL, "Method called. Sizeof connection structure = %d. Allocating connection pool of %d connections and size of %d bytes\n", sizeof(JMSConnection), max_pool_size, (max_pool_size * sizeof(JMSConnection)));

  // Doing memset to make fiels 0/NULL as it will be faster than doing field by filed
  NSLB_MALLOC_AND_MEMSET(connections_chunk, (max_pool_size * sizeof(JMSConnection)), "connections_chunk", -1, NULL);

  for(i = 0; i <max_pool_size; i++)
  {
    /* Linking next connection entries within a pool and making last entry NULL*/
    if(i < (max_pool_size - 1)) {
      connections_chunk[i].next = (JMSConnection *)&connections_chunk[i+1];
    }
    else
      connections_chunk[i].next = NULL;
  }
  for(i = max_pool_size-1; i >= 0; i--)
  {
    /* Linking previous connection entries within a pool and making 1st entry NULL*/
    if(i > 0)
      connections_chunk[i].prev = (JMSConnection *)&connections_chunk[i-1];
    else
      connections_chunk[i].prev = NULL;
  }
  jms_connection_pool_ptr[jpid].start = jms_connection_pool_ptr[jpid].not_conn_head = &connections_chunk[0];
  jms_connection_pool_ptr[jpid].not_conn_tail = &connections_chunk[max_pool_size - 1];
  jms_connection_pool_ptr[jpid].max_pool_size = max_pool_size;
  //Counters to keep record of free ans allocated connections
  jms_connection_pool_ptr[jpid].not_conn_list_counter = max_pool_size;
  NSDL2_JMS(NULL, NULL, "connections_chunk = %p", connections_chunk);
  return;
}

/* Function used to add unique key in hash map and allocate pool for that key
 * key: unique key for hash map Entry is made by
 * hostname_port_queueManager_channel_queue/topic_userId_password_consumerGroup(only in Tibco)
 * len: key length
 * jms_client_type: It tells whether client type is jms producer or jms consumer
 * max_pool_size: maximum pool size initialized by user
 *
 * Returns: It returns index in hash map known as jms pool id(jpid).
 * */
static int jms_cp_add_key_in_hash_map(char *key, int len, char jms_client_type, int max_pool_size, int *is_new_key)
{
  static char init_done = 0;
  NSDL4_JMS(NULL, NULL, "Method calling key = %s, len = %d, jms_client_type = %c, max_pool_size = %d\n", key, len, jms_client_type, max_pool_size);
  if(!init_done)
  {
    nslb_init_norm_id_table_ex(&jcp_cp_norm_tbl, JCP_HASH_TABLE_SIZE);   
    init_done = 1;
  }
  return(nslb_get_or_set_norm_id(&jcp_cp_norm_tbl, key, len, is_new_key)); //add unique key in hash table
}
#if 0
/* Function used to get index in hash map on the basis of unique key
 * key: unique key for hash map
 * len: key length
 *
 * Returns: It returns index in hash map.
 * */
static int jms_cp_get_index_in_hash_map(char *key, int len)
{
  int ret;
  NSDL4_JMS(NULL, NULL, "Method calling key = %s, len = %d\n", key, len);
  ret = nslb_get_norm_id(&jcp_cp_norm_tbl, key, len);
  return ret;
}

/* Function used to free hash map
 * Allocates jms_hash_map
 *
 * Returns: It returns error or success 
 * */
static int jms_cp_delete_key()
{
  int ret;
  NSDL2_JMS(NULL, NULL, "Method called.\n");
  //ret = nslb_delete_norm_id_ex(&jcp_cp_norm_tbl, key, len);
  return ret;
}
#endif

/* Function used to validate jms pool id
 * jpid: jms pool id
 *
 * Returns: It returns success or fail
 * */
int jms_cp_validate_jpid(int jpid)
{
  NSDL2_JMS(NULL, NULL, "Method called.\n");
  if((jpid >= 0) && (jpid < jms_cp_total_key_structs) && jms_connection_pool_ptr)
    return 0;
  else
    return -1;
}

int jms_cp_validate_conn(int jpid)
{
  if(jms_connection_pool_ptr[jpid].free_list_counter || jms_connection_pool_ptr[jpid].busy_list_counter)
    return -1;
  return 0;
}

/* this is extended version with owner check */

int jms_cp_validate_jpcid(int jpcid, char *transaction_name, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.");
  int jpid, jcid;
  JMSConnection *node;

  if((jpcid >= 0) && (jpcid < 0xFFFFFFFF))
  {
    jpid = GET_JPID(jpcid);
    jcid = GET_JCID(jpcid);
    if(jms_cp_validate_jpid(jpid))
    {
      start_end_tx_on_error(transaction_name, "InvalidPoolId");
      HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
    }
    if((jcid >= 0) && (jcid < jms_connection_pool_ptr[jpid].max_pool_size))
    {
      node = jms_connection_pool_ptr[jpid].start + jcid;
      if(node->conn_state != BUSY_LIST)
      {
        start_end_tx_on_error(transaction_name, "IdNotInBusyList");
        HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
      }
      else
      {
        NS_JMS_CHECK_OWNER(node, transaction_name, error_msg);
      }
    }
    else
    {
      start_end_tx_on_error(transaction_name, "InvalidConnId");
      HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
    }
  }
  else
  {
    start_end_tx_on_error(transaction_name, "InvalidPoolConnId");
    HANDLE_ERROR_DT_JPCID("CONN-POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
  }
    return 0;  
}


/* Function used to validate jms pool + conn id
 * jpcid: jms pool + conn id
 *
 * Returns: It returns success or fail
 * */
/*
int jms_cp_validate_jpcid(int jpcid)
{
  NSDL2_JMS(NULL, NULL, "Method called.\n");
  int jpid, jcid;
  JMSConnection *node;

  if((jpcid >= 0) && (jpcid < 0xFFFFFFFF))
  {
    jpid = GET_JPID(jpcid);
    jcid = GET_JCID(jpcid);
    if(jms_cp_validate_jpid(jpid))
      return -1;
    if((jcid >= 0) && (jcid < jms_connection_pool_ptr[jpid].max_pool_size))
    {
      node = jms_connection_pool_ptr[jpid].start + jcid;
      if(node->conn_state != BUSY_LIST)
        return -1;
    }
    else
      return -1;
  }
  else
    return -1;

  return 0;
}
*/

void *jms_cp_set_jms_specific_config(int jpid, size_t jms_specific_config_size)
{
  GET_LOCK;
  NSLB_MALLOC_AND_MEMSET(jms_connection_pool_ptr[jpid].user_config->jms_specific_config,jms_specific_config_size , "jms_specific_config", -1, NULL);
  RELEASE_LOCK;
  return jms_connection_pool_ptr[jpid].user_config->jms_specific_config;
}

void jms_cp_set_conn_specific_config(int jpcid, size_t conn_specific_config_size)
{
  int jpid = GET_JPID(jpcid);
  int jcid = GET_JCID(jpcid);
  GET_LOCK;
  JMSConnection *node = jms_connection_pool_ptr[jpid].start + jcid;
  NSLB_MALLOC_AND_MEMSET(node->conn_config, conn_specific_config_size, "conn_specific_config", -1, NULL);
  RELEASE_LOCK;
}


/* Function used to make key in hash map and initialize connection pool for this key
 * hostname: jms hostname
 * port: jms port
 * queue_manager: It is used in ibmmq jms
 * channel: It is used in ibmmq jms
 * queue: jms queue or topic
 * userId: jms user name
 * password: password of above user name
 * consumer_group: It is used in kafka
 * max_pool_size: maximum pool size initialized by user
 * jms_client_type: It tells whether client type is jms producer or jms consumer
 *
 * Returns: It returns index in hash map i.e jms connection pool id.
 * */

int jms_cp_get_pool_id(char *hostname, int port, char *hostname_port, char *queue_manager, char *channel, char *queue, 
    char *userId,  char *password, char *consumer_group, int max_pool_size, char jms_client_type, char *error_msg, int *is_new_key)
{
  char key[MAX_KEY_LENGTH];
  int len, jpid;
  JMSConfig *user_config;
  NSDL4_JMS(NULL, NULL, "Method calling hostname = %s, port = %d, queue_manager = %s, channel = %s, queue = %s, userId = %s, consumer_group = %s, max_pool_size = %d, jms_client_type = %d\n", hostname, port, queue_manager, channel, queue, userId, consumer_group, max_pool_size, jms_client_type);
  len = snprintf(key, MAX_KEY_LENGTH, "%s_%d_%s_%s_%s_%s_%s_%s_%d", hostname, port, queue_manager, channel, queue, userId, password, consumer_group, jms_client_type);
  if (len >= MAX_KEY_LENGTH)
    HANDLE_ERROR_DT("CONN POOL-Error in getting pool id", error_msg, NS_JMS_ERROR_MAX_KEY_LEN_RCHD, NS_JMS_ERROR_MAX_KEY_LEN_RCHD_MSG);
  if ((max_pool_size <= 0) && (max_pool_size > MAX_POOL_LIMIT))
    HANDLE_ERROR_DT("CONN POOL-Error in getting pool id", error_msg, NS_JMS_ERROR_MAX_POOL_SIZE_RCHD, NS_JMS_ERROR_MAX_POOL_SIZE_RCHD_MSG);
  GET_LOCK;
  jpid = jms_cp_add_key_in_hash_map(key, len, jms_client_type, max_pool_size, is_new_key);
  if(jpid < 0)
  {
    HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_ADD_ENTRY_HASH_MAP_FAIL, NS_JMS_ERROR_ADD_ENTRY_HASH_MAP_FAIL_MSG);
  }
  else
  {
    if(*is_new_key)
    {
      jms_cp_create_jms_conn_pool_key_structure(); //create hash table structure
      jms_connection_pool_ptr[jpid].jms_client_type = jms_client_type;
      jms_cp_allocate_jms_connection_pool(jpid, max_pool_size); //allocating connection pool for each unique key in hash table
      /*set user configuration*/
      NSLB_MALLOC_AND_MEMSET(jms_connection_pool_ptr[jpid].user_config, sizeof(JMSConfig), "user_cofig malloc'ed", -1, NULL);
      user_config = jms_connection_pool_ptr[jpid].user_config;
      len = strlen(hostname);
      if(len)
        NSLB_MALLOC_AND_COPY(hostname, user_config->hostname, len, "user_config->hostname", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      user_config->port = port;
      len = strlen(hostname_port);
      if(len)
        NSLB_MALLOC_AND_COPY(hostname_port, user_config->hostname_port, len, "user_config->hostname_port", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      len = strlen(queue_manager);
      if(len)
        NSLB_MALLOC_AND_COPY(queue_manager, user_config->queue_manager, len, "user_config->queue_manager", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      len = strlen(channel);
      if(len)
        NSLB_MALLOC_AND_COPY(channel, user_config->channel, len, "user_config->channel", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      len = strlen(consumer_group);
      if(len)
        NSLB_MALLOC_AND_COPY(consumer_group, user_config->consumer_group, len, "user_config->consumer_group", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      len = strlen(queue);
      if(len > 255)
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Queue/Topic length cannot be greater than 255", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_QT_LENGTH_MSG);
      if(len)
        NSLB_MALLOC_AND_COPY(queue, user_config->queue, len, "user_config->queue", -1, NULL)
      else
        HANDLE_ERROR_UNLOCK_DT_JPID("CONN POOL-Error in getting pool id", jpid, error_msg, NS_JMS_ERROR_WRONG_USER_CONFIG, NS_JMS_ERROR_WRONG_USER_CONFIG_MSG);
      len = strlen(userId);
      if(len)
        NSLB_MALLOC_AND_COPY(userId, user_config->userId, len, "user_config->userId", -1, NULL)
      else
        NSLB_MALLOC_AND_COPY(userId, user_config->userId, 2, "NA", -1, NULL)
      len = strlen(password);
      if(len)
        NSLB_MALLOC_AND_COPY(password, user_config->password, len, "user_config->password", -1, NULL)
      else
        NSLB_MALLOC_AND_COPY(userId, user_config->password, 2, "NA", -1, NULL)
    }
  }
  RELEASE_LOCK; //release lock after accessing pool variables
  return jpid;
}

/* Function used to get JMSConnection node from not connected list
 * jpid: index in hash map i.e jms connection pool id
 *
 * Returns: It returns connection node from not connected list
 * */
static JMSConnection *jms_cp_get_conn_from_nc_list(int jpid)
{
  JMSConnection *node;
  NSDL2_JMS(NULL, NULL, "Method called jpid = %d\n", jpid);
  node = jms_connection_pool_ptr[jpid].not_conn_head;
  jms_connection_pool_ptr[jpid].not_conn_head = jms_connection_pool_ptr[jpid].not_conn_head->next;
  if(jms_connection_pool_ptr[jpid].not_conn_head)
    jms_connection_pool_ptr[jpid].not_conn_head->prev = NULL;
  jms_connection_pool_ptr[jpid].not_conn_list_counter--;
  if(!jms_connection_pool_ptr[jpid].not_conn_list_counter)
  {
    jms_connection_pool_ptr[jpid].not_conn_head = NULL;
    jms_connection_pool_ptr[jpid].not_conn_tail = NULL;
  }
  return node;
}

/* Function used to get JMSConnection node from free list
 * jpid: index in hash map i.e jms connection pool id
 *
 * Returns: It returns connection node from not connected list
 * */
static JMSConnection *jms_cp_get_conn_from_free_list(int jpid)
{
  JMSConnection *node;
  NSDL2_JMS(NULL, NULL, "Method called jpid = %d\n", jpid);
  node = jms_connection_pool_ptr[jpid].free_head;
  jms_connection_pool_ptr[jpid].free_head = jms_connection_pool_ptr[jpid].free_head->next;
  if(jms_connection_pool_ptr[jpid].free_head)
    jms_connection_pool_ptr[jpid].free_head->prev = NULL;
  jms_connection_pool_ptr[jpid].free_list_counter--;
  if(!jms_connection_pool_ptr[jpid].free_list_counter)
  {
    jms_connection_pool_ptr[jpid].free_head = NULL;
    jms_connection_pool_ptr[jpid].free_tail = NULL;
  }
  return node;
}
#if 0
/* Function used to get JMSConnection node from busy list
 * jpid: index in hash map i.e jms connection pool id
 *
 * Returns: It returns connection node from not connected list
 * */
static JMSConnection *jms_cp_get_conn_from_busy_list(int jpid)
{
  JMSConnection *node;
  NSDL2_JMS(NULL, NULL, "Method called jpid = %d\n", jpid);
  node = jms_connection_pool_ptr[jpid].busy_head;
  jms_connection_pool_ptr[jpid].busy_head = jms_connection_pool_ptr[jpid].busy_head->next;
  if(jms_connection_pool_ptr[jpid].busy_head)
    jms_connection_pool_ptr[jpid].busy_head->prev = NULL;
  jms_connection_pool_ptr[jpid].busy_list_counter--;
  if(!jms_connection_pool_ptr[jpid].busy_list_counter)
  {
    jms_connection_pool_ptr[jpid].busy_head = NULL;
    jms_connection_pool_ptr[jpid].busy_tail = NULL;
  }
  return node;
}
#endif

/* Function used to send JMSConnection node to busy list
 * node: It is JMSConnection node to be added in busy list
 *
 * Returns: It returns index in hash map+index of connection in pool i.e jms connection id.(jpcid)
 * */
static int jms_cp_send_conn_to_busy_list(int jpid, JMSConnection *node)
{
  int jpcid;
  int jcid;
  NSDL2_JMS(NULL, NULL, "Method called node = %p\n", node);
  if(!jms_connection_pool_ptr[jpid].busy_head)
  {
    jms_connection_pool_ptr[jpid].busy_head = node;
    node->prev = NULL;
  }
  else
  {
    jms_connection_pool_ptr[jpid].busy_tail->next = node;
    node->prev = jms_connection_pool_ptr[jpid].busy_tail;
  }
  jms_connection_pool_ptr[jpid].busy_tail = node;
  jms_connection_pool_ptr[jpid].busy_tail->conn_state = BUSY_LIST;
  jms_connection_pool_ptr[jpid].busy_tail->next = NULL;
  jms_connection_pool_ptr[jpid].busy_list_counter++;

  //Calculation of jpcid
  jcid = jms_connection_pool_ptr[jpid].busy_tail - jms_connection_pool_ptr[jpid].start;
  jpcid = SET_JPCID(jcid, jpid);
  return jpcid;
}

/* Function used to send JMSConnection node to not connected list
 * node: It is JMSConnection node to be added in not connected list
 *
 * Returns: It returns index in hash map+index of connection in pool i.e jms connection id.(jpcid)
 * */
static void jms_cp_send_conn_to_nc_list_no_lock(int jpid, JMSConnection *node)
{
  NSDL2_JMS(NULL, NULL, "Method called node = %p\n", node);
  if(!jms_connection_pool_ptr[jpid].not_conn_head)
  {
    jms_connection_pool_ptr[jpid].not_conn_head = node;
    node->prev = NULL;
  }
  else
  {
    jms_connection_pool_ptr[jpid].not_conn_tail->next = node;
    node->prev = jms_connection_pool_ptr[jpid].not_conn_tail;
  }
  jms_connection_pool_ptr[jpid].not_conn_tail = node;
  jms_connection_pool_ptr[jpid].not_conn_tail->conn_state = NOT_CONNECTED_LIST;
  jms_connection_pool_ptr[jpid].not_conn_tail->next = NULL;
  jms_connection_pool_ptr[jpid].not_conn_list_counter++;
  return;
}

/* Function used to send JMSConnection node to not connected list
 * node: It is JMSConnection node to be added in not connected list
 *
 * Returns: It returns index in hash map+index of connection in pool i.e jms connection id.(jpcid)
 * */
void jms_cp_send_conn_to_nc_list(int jpid, JMSConnection *node)
{ 
  NSDL2_JMS(NULL, NULL, "Method called node = %p\n", node);
  GET_LOCK;
  if(!jms_connection_pool_ptr[jpid].not_conn_head)
  {
    jms_connection_pool_ptr[jpid].not_conn_head = node;
    node->prev = NULL;
  }
  else
  {
    jms_connection_pool_ptr[jpid].not_conn_tail->next = node;
    node->prev = jms_connection_pool_ptr[jpid].not_conn_tail;
  }
  jms_connection_pool_ptr[jpid].not_conn_tail = node;
  jms_connection_pool_ptr[jpid].not_conn_tail->conn_state = NOT_CONNECTED_LIST;
  jms_connection_pool_ptr[jpid].not_conn_tail->next = NULL;
  jms_connection_pool_ptr[jpid].not_conn_list_counter++;
  NSLB_FREE_AND_MAKE_NULL(node->conn_config, "node->conn_config", -1, NULL);
  RELEASE_LOCK;
  return;
}

/* Function used to send JMSConnection node to free list
 * node: It is JMSConnection node to be added in free list
 *
 * Returns: It returns index in hash map+index of connection in pool i.e jms connection id.(jpcid)
 * */
static void jms_cp_send_conn_to_free_list(int jpid, JMSConnection *node)
{
  NSDL2_JMS(NULL, NULL, "Method called node = %p\n", node);
  if(!jms_connection_pool_ptr[jpid].free_head)
  { 
    jms_connection_pool_ptr[jpid].free_head = node;
    node->prev = NULL;
  }
  else
  {
    jms_connection_pool_ptr[jpid].free_tail->next = node;
    node->prev = jms_connection_pool_ptr[jpid].free_tail;
  }
  jms_connection_pool_ptr[jpid].free_tail = node;
  jms_connection_pool_ptr[jpid].free_tail->conn_state = FREE_LIST;
  jms_connection_pool_ptr[jpid].free_tail->next = NULL;
  jms_connection_pool_ptr[jpid].free_list_counter++;
  return;
}

/* Function used to make key in hash map and initialize connection pool for this key
 * jpid: index in hash map i.e jms connection pool id
 *
 * Returns: It returns index in hash map+index of connection in pool i.e jms pool id + connection id.
 * */
int jms_cp_get_connection(int jpid, JMSConnection **node)
{
  int jpcid;
  NSDL2_JMS(NULL, NULL, "Method called jpid = %d\n", jpid);
  GET_LOCK;
  if(jms_connection_pool_ptr[jpid].free_list_counter)
  {
    jpcid = jms_cp_send_conn_to_busy_list(jpid, jms_cp_get_conn_from_free_list(jpid));
  }
  else if(jms_connection_pool_ptr[jpid].not_conn_list_counter)
  {
    *node = jms_cp_get_conn_from_nc_list(jpid);
    jpcid = jms_cp_send_conn_to_busy_list(jpid, *node);
  }
  else
  {
    jpcid = NS_JMS_ERROR_CONN_POOL_FINISHED;
  }
  RELEASE_LOCK;
  return jpcid;
}

/* Function used to get connection node from jms pool+conn id
 * jpcid: It is index in hash map+index of connection in pool i.e jms connection id.
 * 
 * Returns: It returns JMSConnection node from respective jpcid
 * */
JMSConnection *jms_cp_get_conn_from_jpcid(int jpcid)
{
  /*checks for hash map size, max pool size and busy list*/
  int jpid, jcid;
  NSDL2_JMS(NULL, NULL, "Method called jpcid = %p\n", jpcid);
  GET_LOCK;
  jpid = GET_JPID(jpcid);
  jcid = GET_JCID(jpcid);
  JMSConnection *node = jms_connection_pool_ptr[jpid].start + jcid;
  RELEASE_LOCK;
  return node;
}

/* Function used to make key in hash map and initialize connection pool for this key
 * jpcid: index in hash map+index of connection in pool i.e jms connection id.
 *
 * Returns: It returns error or success
 * */
int jms_cp_release_connection(int jpcid)
{
  int jpid, jcid;
  NSDL4_JMS(NULL, NULL, "Method calling jpcid = %d\n", jpcid);
  jpid = GET_JPID(jpcid); //get jpid from jpcid
  jcid = GET_JCID(jpcid);
  JMSConnection *node = jms_connection_pool_ptr[jpid].start + jcid;
  GET_LOCK;
  if(node->prev)
    node->prev->next = node->next;
  else
    jms_connection_pool_ptr[jpid].busy_head = jms_connection_pool_ptr[jpid].busy_head->next;  /*It is busy head*/

  if(node->next)
    node->next->prev = node->prev;
  else
    jms_connection_pool_ptr[jpid].busy_tail = jms_connection_pool_ptr[jpid].busy_tail->prev; /*It is busy tail*/
  jms_connection_pool_ptr[jpid].busy_list_counter--;

  jms_cp_send_conn_to_free_list(jpid, node);
  RELEASE_LOCK;
  return 0;
}

JMSConfig *jms_cp_get_user_config(int jpid)
{
  return(jms_connection_pool_ptr[jpid].user_config);
}

char jms_cp_get_client_type(int jpid, int jpcid)
{
  if(jpid >= 0)
    return(jms_connection_pool_ptr[jpid].jms_client_type);
  if(jpcid >= 0)
  {
    jpid = GET_JPID(jpcid);
    return(jms_connection_pool_ptr[jpid].jms_client_type);
  }
  return -1;  
}

/*
int jms_cp_validate_jpcid_for_close_conn(int jpcid)
{
  NSDL2_JMS(NULL, NULL, "Method called.\n");
  int jpid, jcid;
  if((jpcid >= 0) && (jpcid < 0xFFFFFFFF))
  {
    jpid = GET_JPID(jpcid);
    jcid = GET_JCID(jpcid);
    if(jms_cp_validate_jpid(jpid))
      return -1;
    if((jcid >= 0) && (jcid < jms_connection_pool_ptr[jpid].max_pool_size))
    {
      JMSConnection *node = jms_connection_pool_ptr[jpid].start + jcid;
      if(node->conn_state == NOT_CONNECTED_LIST)
        return -1;
    }
    else
      return -1;
  }
  else
    return -1;
  return 0;
}
*/

int jms_cp_validate_jpcid_for_close_conn(int jpcid, char *transaction_name, char *error_msg)
{
  NSDL2_JMS(NULL, NULL, "Method called.");
  int jpid, jcid;
  JMSConnection *node;

  if((jpcid >= 0) && (jpcid < 0xFFFFFFFF))
  {
    jpid = GET_JPID(jpcid);
    jcid = GET_JCID(jpcid);
    if(jms_cp_validate_jpid(jpid))
    {
      start_end_tx_on_error(transaction_name, "InvalidPoolId");
      HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
    }
    if((jcid >= 0) && (jcid < jms_connection_pool_ptr[jpid].max_pool_size))
    {
      node = jms_connection_pool_ptr[jpid].start + jcid;
      if(node->conn_state == NOT_CONNECTED_LIST)
      {
        start_end_tx_on_error(transaction_name, "IdInNotConnList");
        HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid,  error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
      }
      else
      {
        NS_JMS_CHECK_OWNER(node, transaction_name, error_msg);
      }
    }
    else
    {
      start_end_tx_on_error(transaction_name, "InvalidConnId");
      HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid,  error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
    }
  }
  else
  {
    start_end_tx_on_error(transaction_name, "InvalidPoolConnId");
    HANDLE_ERROR_DT_JPCID("CONN POOL-Error in connection pool", jpcid, error_msg, NS_JMS_ERROR_INVALID_JPCID, NS_JMS_ERROR_INVALID_JPCID_MSG);
  }

    return 0;
}

void jms_cp_close_connection(int jpcid)
{
  NSDL4_JMS(NULL, NULL, "Method calling jpcid = %d\n", jpcid);
  int jpid, jcid; 
  jpid = GET_JPID(jpcid);
  jcid = GET_JCID(jpcid);
  JMSConnection *node = jms_connection_pool_ptr[jpid].start + jcid;
  GET_LOCK;
  if(node->conn_state == BUSY_LIST)
  {
    if(node->prev)
      node->prev->next = node->next;
    else
      jms_connection_pool_ptr[jpid].busy_head = jms_connection_pool_ptr[jpid].busy_head->next;  /*It is busy head*/
    if(node->next)
      node->next->prev = node->prev;
    else
      jms_connection_pool_ptr[jpid].busy_tail = jms_connection_pool_ptr[jpid].busy_tail->prev; /*It is busy tail*/
    jms_connection_pool_ptr[jpid].busy_list_counter--;
  }
  else if(node->conn_state == FREE_LIST)
  {
    if(node->prev)
      node->prev->next = node->next;
    else
      jms_connection_pool_ptr[jpid].free_head = jms_connection_pool_ptr[jpid].free_head->next;  /*It is free head*/
    if(node->next)
      node->next->prev = node->prev;
    else
      jms_connection_pool_ptr[jpid].free_tail = jms_connection_pool_ptr[jpid].free_tail->prev; /*It is free tail*/
    jms_connection_pool_ptr[jpid].free_list_counter--;
  }
  NSLB_FREE_AND_MAKE_NULL(node->conn_config, "node->conn_config", -1, NULL);
  jms_cp_send_conn_to_nc_list_no_lock(jpid, node);
  RELEASE_LOCK;
  return;
}

void start_end_tx_on_error(char *transaction_name, char *operation)
{
  NSDL4_JMS(NULL, NULL, "Method calling");
  int operation_len = 0;
  if(transaction_name)
  {
    int transaction_name_len = strlen(transaction_name);

    ns_start_transaction(transaction_name);
    if(operation)
      operation_len = strlen(operation);

    char end_as_tx[transaction_name_len+operation_len+6];
    snprintf(end_as_tx, transaction_name_len+operation_len+6, "%s%sError", transaction_name, operation);
    ns_end_transaction_as(transaction_name, NS_REQUEST_ERRMISC, end_as_tx);
  }
}

void ns_jms_logs_req_resp(char *hostname_port,int msg_type, char *jms_type, char *t_q_name, int t_or_q, char *msg, char *hdr_name, char *hdr_value, int jpcid)
{
  FILE *log_fp = NULL;
  char log_file[256];
  char *req_type_string = (msg_type == JMS_SENT_MSG)?"put":"get";
  char *ns_wdir = NULL;
  char log_dir[128];
  struct stat st; 
  char base_dir[128];
  ns_wdir = getenv("NS_WDIR");
  if(!ns_wdir)
  { 
    fprintf(stderr, "Error; Error in getting system variable \'NS_WDIR\'.\n");
    return;
  }

  sprintf(base_dir, "%s/logs/TR%d", ns_wdir, ns_get_testid());
  sprintf(log_dir, "%s/%lld/ns_logs/req_rep", base_dir, nslb_get_cur_partition(base_dir));

  if(stat(log_dir, &st) < 0)
  {
    if(mkdir(log_dir, 0777))
    {
      fprintf(stderr, "Error: Unable to create JMS_LOG dir for logging Jms Msg\n, Error String: %s\n", strerror(errno));
      return;
    }
  }
  sprintf(log_file, "%s/%s_%s_msg_%d_%u_%u.dat", log_dir, jms_type, req_type_string, ns_get_nvmid(), ns_get_userid(), ns_get_sessid());


  log_fp = fopen(log_file, "a+");

  if(log_fp)
  { 
    if(hdr_name)
    {
      if(hdr_value)
      { 
        if(fprintf(log_fp, "%s: %s\n",hdr_name, hdr_value) < 0)
        { 
          fprintf(stderr, "Error: Unable to write in JMS_LOG file for logging Jms Msg\n, Error String: %s\n", strerror(errno));
          return;
        }
      }
      else
      {
        if(fprintf(log_fp, "%s:\n",hdr_name) < 0)
        { 
          fprintf(stderr, "Error: Unable to write in JMS_LOG file for logging Jms Msg\n, Error String: %s\n", strerror(errno));
          return;
        }
        
      }
    }
    else
    { 
      if(t_or_q == JMS_TOPIC)
        fprintf(log_fp, "TOPIC: %s\n", t_q_name);
      else if(t_or_q == JMS_QUEUE)
        fprintf(log_fp, "QUEUE: %s\n", t_q_name);
      else
        fprintf(log_fp, "UNKNOWN: %s\n", t_q_name);
      
      if(msg)
      { 
        if(fprintf(log_fp, "HostName: %s\n%s Msg: %s\n",
                          hostname_port, req_type_string, msg) < 0)
     
        {  
          fprintf(stderr, "Error: Unable to write in JMS_LOG file for logging Jms Msg\n, Error String: %s\n", strerror(errno));
          return;
        }
      }

      if(msg_type == JMS_SENT_MSG)
      {
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "%s-Pool id: %d Connection id: %d, Put message is in '%s'", jms_type, GET_JPID(jpcid), GET_JCID(jpcid), log_file);
      }
      else 
      {
        NS_DT3(NULL, NULL, DM_L1, MM_JMS, "%s-Pool id: %d Connection id: %d, Get message is in '%s'",jms_type, GET_JPID(jpcid), GET_JCID(jpcid), log_file);
      }
        
     }
  }  
  else
  {
    fprintf(stderr, "Error: Unable to create JMS_LOG file for logging Jms Msg\n, Error String: %s\n", strerror(errno));
    return;
  }

   fclose(log_fp);
}



#ifdef JCP_CP_TEST

/*
To make test program:
 cc -DJCP_CP_TEST -o kafka_test ns_kafka_api.c -l <kafkalib>
*/
int main()
{
  jms_cp_get_pool_id("192.178.20.9", 9093, "queue_manager", "channel",
                     "queue", "userId", "password", "consumer_group", 10, " jms_client_type", 0, NULL);

  return 0;
}

#endif
