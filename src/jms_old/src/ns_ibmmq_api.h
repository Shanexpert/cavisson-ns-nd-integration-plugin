#ifndef NS_IBMMQ_H
#define NS_IBMMQ_H

#include <cmqc.h>
#include <cmqxc.h>

#define NS_IBMMQ_ERROR   -1
#define NS_IBMMQ_SUCCESS  0

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


extern int ns_ibmmq_make_connection(IBMMQ_Conn_Info *ibmmq_put_ptr, char *ibmmq_queue_manager, char *channel_name, char *queue_or_topic_name, char *server_ip);
extern int ns_ibmmq_put(IBMMQ_Conn_Info *ibmmq_put_ptr, char *msg);
extern int ns_ibmmq_close_connection(IBMMQ_Conn_Info *ibmmq_put_ptr);
extern int ns_ibmmq_get(IBMMQ_Conn_Info *ibmmq_get_ptr, char *get_msg, int);

#endif 
