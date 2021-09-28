/******************************************************************
 * Name    :    ns_jms_conn_pool.h
 * Purpose :    This file contains data structures and function 
 *              declaration of JMS connection pool
 * Author  :    Neha Rawat
 * Intial version date:    16/05/2019
 * Last modification date: 16/05/2019
 *****************************************************************/
#ifndef NS_JMS_CONN_POOL_H
#define NS_JMS_CONN_POOL_H

#include "../../ns_tls_utils.h"

typedef struct
{
  char *hostname;
  int port;
  char *hostname_port;
  char *queue_manager;  //used in ibmmq
  char *channel;
  char *queue;
  char *userId;
  char *password;
  char *consumer_group;
  void *jms_specific_config;
}JMSConfig;

typedef struct jms_connection
{
  struct jms_connection *next;    /* next node in connection pool */
  struct jms_connection *prev;    /* previous node in connection pool */
  unsigned char conn_state;       /* Connection state busy, free or not connected */
  void *conn_config;              /*It is based on JMS type*/
  void *vptr;                     /* pointer to vptr of user. Used for safety check if same user using the connectection or not */
} JMSConnection;

typedef struct 
{ 
  JMSConnection *start;           /* Start pointer to the array of connection pool. Used only for calculating offset */
  JMSConnection *free_head;       /* Free List head pointer */
  JMSConnection *free_tail;       /* Free List tail Pointer */
  JMSConnection *busy_head;       /* Busy List head pointer */
  JMSConnection *busy_tail;       /* Busy List tail Pointer */
  JMSConnection *not_conn_head;   /* Not connected List head pointer */
  JMSConnection *not_conn_tail;   /* Not connected List tail pointer */
  unsigned short free_list_counter;          /* No. of nodes in free list */  
  int busy_list_counter;          /* No. of nodes in busy list */  
  int not_conn_list_counter;      /* No. of nodes in not connected list */  
  int   max_pool_size;            /* Maximum size of pool given by user */
  JMSConfig *user_config;         /* User provided configuration */ 
  char jms_client_type;           /* producer or consumer */
}JMSConnectionPool;

#define JPID_MASK                     0x0000FFFF
#define JCID_MASK                     0xFFFF0000
#define SET_JPCID(jcid, jpid)         jcid<<16|jpid
#define GET_JPID(jpcid)               jpcid & JPID_MASK
#define GET_JCID(jpcid)               (jpcid & JCID_MASK)>>16
#define DELTA_CONN_POOL_ENTRIES       10
#define MAX_KEY_LENGTH                2048
#define MAX_POOL_LIMIT                0xFFFF 
#define MAX_ERROR_STRING_LEN          1024
#define NOT_CONNECTED_LIST            0
#define FREE_LIST                     1
#define BUSY_LIST                     2
#define JCP_HASH_TABLE_SIZE           10
#define PRODUCER 1
#define CONSUMER 2
 
#define JMS_SENT_MSG		      1
#define JMS_RECV_MSG                  2

#define JMS_TOPIC	              1
#define JMS_QUEUE                     2

#define GET_LOCK     \
           if(ISCALLER_NVM_THREAD) \
           {   \
            NSDL1_JMS(NULL, NULL, "Get pool lock\n");  \
            pthread_mutex_lock(&jms_cp_pool_lock);       \
           }                                          
#define RELEASE_LOCK  \
          if(ISCALLER_NVM_THREAD) \
          { \
            NSDL1_JMS(NULL, NULL, "Release pool lock\n");  \
            pthread_mutex_unlock(&jms_cp_pool_lock);  \
          }
#define HANDLE_ERROR_UNLOCK_DT_JPID(dt_msg, jpid, error_msg, err_code, err_str)  \
         { \
          NS_DT4(NULL, NULL, DM_L1, MM_JMS, "%s, Pool id: %d. Error=%s", dt_msg, jpid, err_str); \
          strncpy(error_msg, err_str, MAX_ERROR_STRING_LEN); \
          error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
          RELEASE_LOCK;  \
          NSDL1_JMS(NULL, NULL, error_msg);  \
          return err_code; \
         } 

// Handle errror macro with debug trace logging

#define HANDLE_ERROR_DT_JPCID(dt_msg, jpcid, error_msg, err_code, err_str)  \
        {   \
          NS_DT4(NULL, NULL, DM_L1, MM_JMS, "%s, Pool id: %d, Connection id: %d. Error=%s", dt_msg, GET_JPID(jpcid),GET_JCID(jpcid), err_str); \
          strncpy(error_msg, err_str, MAX_ERROR_STRING_LEN); \
          error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
          NSDL1_JMS(NULL, NULL,error_msg );  \
          return err_code; \
        }
 
#define HANDLE_ERROR_DT_JPID(dt_msg, jpid, error_msg, err_code, err_str)  \
        {   \
          NS_DT4(NULL, NULL, DM_L1, MM_JMS, "%s, Pool id: %d. Error=%s", dt_msg, jpid, err_str); \
          strncpy(error_msg, err_str, MAX_ERROR_STRING_LEN); \
          error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
          NSDL1_JMS(NULL, NULL,error_msg );  \
          return err_code; \
        }

#define HANDLE_ERROR_DT(dt_msg, error_msg, err_code, err_str)  \
        {   \
          NS_DT4(NULL, NULL, DM_L1, MM_JMS, "%s. Error=%s", dt_msg, err_str); \
          strncpy(error_msg, err_str, MAX_ERROR_STRING_LEN); \
          error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
          NSDL1_JMS(NULL, NULL,error_msg );  \
          return err_code; \
         }
#if 0       
#define HANDLE_ERROR(error_msg, err_code, err_str)  \
        {  strncpy(error_msg, err_str, MAX_ERROR_STRING_LEN); \
          error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
          NSDL1_JMS(NULL, NULL,error_msg );  \
          return err_code; } 
#endif

/* This macro sets the vuser as the owner of the connection */
#ifdef JMS_API_TEST
/*
typedef struct
{
  void *vptr; 
} Msg_com_con;

static char *buffer_key = "DummyForTesting";
static void *cur_vptr = 1234567890;
*/
#endif

#define NS_JMS_SET_OWNER(jpcid) \
        { \
          JMSConnection *conn_node = jms_cp_get_conn_from_jpcid(jpcid); \
          NSDL3_JMS(NULL, NULL, "NS_JMS_SET_OWNER: virutal context mode");  \
          conn_node->vptr = TLS_GET_VPTR(); \
        }

/* This macro checks if API called by the vuser is the owner of the connection
   In case of thread mode, vptr is to taken from thread specific key
*/
#define NS_JMS_CHECK_OWNER(node, transaction_name, error_msg) \
        { \
          NSDL3_JMS(NULL, NULL, "NS_JMS_CHECK_OWNER: virutal context mode");  \
          if(node->vptr != g_tls.vptr) \
          { \
            strncpy(error_msg, NS_JMS_ERROR_DIFFERENT_VPTR_MSG, MAX_ERROR_STRING_LEN); \
            error_msg[MAX_ERROR_STRING_LEN] = '\0';  \
            NSDL1_JMS(NULL, NULL,error_msg );  \
            start_end_tx_on_error(transaction_name, "DifferentConnOwner"); \
            return NS_JMS_ERROR_DIFFERENT_VPTR; \
          }  \
        }
extern void ns_jms_logs_req_resp(char *hostname_port,int msg_type,char *jms_type, char *t_q_name, int t_or_q, char *msg, char *hdr_name, char *hdr_value, int jpcid);
extern int jms_cp_release_connection(int jpcid);
extern JMSConnection *jms_cp_get_conn_from_jpcid(int jpcid);
extern void jms_cp_send_conn_to_nc_list(int jpid, JMSConnection *node);
extern int jms_cp_get_connection(int jpid, JMSConnection **node);
extern int jms_cp_get_pool_id(char *hostname, int port, char *hostname_port, char *queue_manager, char *channel, char *queue, 
       char *userId,  char *password, char *consumer_group, int max_pool_size, char jms_client_type, char *error_msg, int *is_new_key);
extern int jms_cp_validate_jpid(int jpid);
extern int jms_cp_validate_jpcid(int jpcid, char *transaction_name, char *error_msg);
extern JMSConfig *jms_cp_get_user_config(int jpid);
extern char jms_cp_get_client_type(int jpid, int jpcid);
extern int jms_cp_validate_conn(int jpid);
extern void *jms_cp_set_jms_specific_config(int jpid, size_t jms_specific_config_size);
extern void jms_cp_set_conn_specific_config(int jpcid, size_t conn_specific_config_size);
extern void jms_cp_close_connection(int jpcid); 
extern int jms_cp_validate_jpcid_for_close_conn(int jpcid, char *transaction_name, char *error_msg);
extern void start_end_tx_on_error(char *transaction_name, char *operation);
//extern int jms_cp_validate_jpcid_ex(int jpcid, char *error_msg);
#endif
