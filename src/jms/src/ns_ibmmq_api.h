#ifndef NS_IBMMQ_H
#define NS_IBMMQ_H

#include "cmqc.h"
#include "cmqxc.h"

/*IBMMQ connection structure*/
typedef struct
{
  MQCD     ClientConn;	           /*Client connection channel*/
  MQOD     od;
  MQOD     oid;                    /* Object Descriptor             */
  MQMD     md;                     /* Message Descriptor            */
  MQPMO    pmo;                    /* put message options           */
  MQCNO    cno;                    /* connection options            */
  MQHCONN  Hcon;                   /* connection handle             */
  MQHOBJ   Hobj;                   /* object handle                 */
  MQLONG   O_options;              /* MQOPEN options                */
  MQLONG   C_options;              /* MQCLOSE options               */
  MQLONG   CompCode;               /* completion code               */
  MQLONG   OpenCode;               /* MQOPEN completion code        */
  MQLONG   Reason;                 /* reason code                   */
  MQLONG   CReason;                /* reason code for MQCONNX       */
  MQGMO    gmo;                    /* get message options           */
  MQLONG   buflen;
  MQLONG   messlen;
}IBMMQ_Conn_Info;

typedef struct
{
  char                    set_default_conf;
  int                     put_mode;
  double                  conn_timeout;
  double                  put_msg_timeout;
  double                  get_msg_timeout;
}IbmmqConfig;

extern int ns_ibmmq_init_producer(char *ibmmq_hostname, int ibmmq_port, char *queue_manager, char *channel,
                            char *ibmmq_queue, char *ibmmq_userId,  char *ibmmq_password, int max_pool_size, char *error_msg);
extern int ns_ibmmq_init_consumer(char *ibmmq_hostname, int ibmmq_port,  char *queue_manager,  char *channel,                                                           char *ibmmq_queue, char *ibmmq_userId, char *ibmmq_password, int max_pool_size, char *error_msg);
extern int ns_ibmmq_get_connection(int jpid, char *transaction_name, char *error_msg);
extern int ns_ibmmq_release_connection(int jpcid, char *error_msg);
extern int ns_ibmmq_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
extern int ns_ibmmq_get_msg( int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
extern int ns_ibmmq_close_connection(int jpcid, char *transaction_name, char *error_msg);
extern int ns_ibmmq_shutdown(int jpcid, char *transaction_name, char *error_msg);

#endif 
