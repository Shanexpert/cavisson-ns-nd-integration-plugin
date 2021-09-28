#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#include "ns_ibmmq_api.h"

int ns_ibmmq_make_connection(IBMMQ_Conn_Info *ibmmq_put_ptr, char *ibmmq_queue_manager, char *channel_name, 
                             char *queue_or_topic_name, char *server_ip)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_ibmmq_make_connection()\n");
  #endif

  MQCD     ClientConn = {MQCD_CLIENT_CONN_DEFAULT}; /*Client connection channel*/ /*add by Abhi*/

  /*   Declare MQI structures needed                                */
  MQOD     od = {MQOD_DEFAULT};    /* Object Descriptor             */
  MQMD     md = {MQMD_DEFAULT};    /* Message Descriptor            */
  MQPMO   pmo = {MQPMO_DEFAULT};   /* put message options           */
  MQCNO   cno = {MQCNO_DEFAULT};   /* connection options            */
  MQGMO   gmo = {MQGMO_DEFAULT};   /* get message options           */

  memcpy((void *)(&ibmmq_put_ptr->ClientConn),(void *) &ClientConn, sizeof(ClientConn));
  memcpy((void *)(&ibmmq_put_ptr->cno),(void *) &cno, sizeof(cno));
  memcpy((void *)(&ibmmq_put_ptr->od),(void *) &od, sizeof(od));
  memcpy((void *)(&ibmmq_put_ptr->md),(void *) &md, sizeof(md));
  memcpy((void *)(&ibmmq_put_ptr->pmo),(void *) &pmo, sizeof(pmo));
  memcpy((void *)(&ibmmq_put_ptr->gmo),(void *) &gmo, sizeof(gmo));

  /*------------------------------ Handle the Connection IP -----------------------------*/
  {
    strncpy(ibmmq_put_ptr->ClientConn.ConnectionName, server_ip, MQ_CONN_NAME_LENGTH);
    strncpy(ibmmq_put_ptr->ClientConn.ChannelName, channel_name, MQ_CHANNEL_NAME_LENGTH);
    /* Point the MQCNO to the client connection definition */
    ibmmq_put_ptr->cno.ClientConnPtr = &ibmmq_put_ptr->ClientConn;

    /* Client connection fields are in the version 2 part of the
        MQCNO so we must set the version number to 2 or they will
        be ignored */
    ibmmq_put_ptr->cno.Version = MQCNO_VERSION_2;
  
    #ifdef NS_DEBUG_ON
      fprintf(stderr, "Using the server connection channel %s on connection name %s.\n", 
                       ibmmq_put_ptr->ClientConn.ChannelName, ibmmq_put_ptr->ClientConn.ConnectionName);
    #endif

  }

  /******************************************************************/
  /*                                                                */
  /*   Connect to queue manager                                     */
  /*                                                                */
  /******************************************************************/

  MQCONNX(ibmmq_queue_manager,                 /* queue manager                  */
         &ibmmq_put_ptr->cno,                    /* connection options             */
         &ibmmq_put_ptr->Hcon,                   /* connection handle              */
         &ibmmq_put_ptr->CompCode,               /* completion code                */
         &ibmmq_put_ptr->CReason);               /* reason code                    */

  /* report reason and stop if it failed     */
  if (ibmmq_put_ptr->CompCode == MQCC_FAILED)
  {
    fprintf(stderr, "MQCONNX ended with reason code %d\n", ibmmq_put_ptr->CReason);
    return NS_IBMMQ_ERROR;
  }

  /* if there was a warning report the cause and continue */
  if (ibmmq_put_ptr->CompCode == MQCC_WARNING)
  {
    #ifdef NS_DEBUG_ON
      fprintf(stderr, "MQCONNX generated a warning with reason code %d\nContinue ...", ibmmq_put_ptr->CReason);
    #endif
  }

  /******************************************************************/
  /*                                                                */
  /*   Use parameter as the name of the target queue                */
  /*                                                                */
  /******************************************************************/
  strncpy(ibmmq_put_ptr->od.ObjectName, queue_or_topic_name, (size_t)MQ_Q_NAME_LENGTH);

  #ifdef NS_DEBUG_ON
    fprintf(stderr, "target queue is %s\n", ibmmq_put_ptr->od.ObjectName);
  #endif

  ibmmq_put_ptr->CompCode = ibmmq_put_ptr->OpenCode;        /* use MQOPEN result for initial test */

  memcpy(ibmmq_put_ptr->md.Format,           /* character string format            */
         MQFMT_STRING, (size_t)MQ_FORMAT_LENGTH);

  ibmmq_put_ptr->pmo.Options = MQPMO_NO_SYNCPOINT
                              | MQPMO_FAIL_IF_QUIESCING;

  return NS_IBMMQ_SUCCESS;
}

int ns_ibmmq_put(IBMMQ_Conn_Info *ibmmq_put_ptr, char *put_msg)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_ibmmq_put()\n");
  #endif

  MQLONG messlen = strlen(put_msg); 

  if (ibmmq_put_ptr->CompCode != MQCC_FAILED)
  {
    if (messlen > 0)
    {
      /**************************************************************/
      /* The following statement is not required if the             */
      /* MQPMO_NEW_MSG_ID option is used.                           */
      /**************************************************************/

      ibmmq_put_ptr->O_options = MQOO_OUTPUT              /* open queue for output     */
                                | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
                                ;                        /* = 0x2010 = 8208 decimal   */
    
    
      MQOPEN(ibmmq_put_ptr->Hcon,                      /* connection handle            */
             &ibmmq_put_ptr->od,                       /* object descriptor for queue  */
             ibmmq_put_ptr->O_options,                 /* open options                 */
             &ibmmq_put_ptr->Hobj,                     /* object handle                */
             &ibmmq_put_ptr->OpenCode,                 /* MQOPEN completion code       */
             &ibmmq_put_ptr->Reason);                  /* reason code                  */
    
      /* report reason, if any; stop if failed      */
      if (ibmmq_put_ptr->Reason != MQRC_NONE)
      {
        #ifdef NS_DEBUG_ON
          fprintf(stderr, "MQOPEN ended with reason code %d\n", ibmmq_put_ptr->Reason);
        #endif
      }
    
      if (ibmmq_put_ptr->OpenCode == MQCC_FAILED)
      {
        #ifdef NS_DEBUG_ON
          fprintf(stderr, "unable to open queue for output\n");
        #endif
      }

      memcpy(ibmmq_put_ptr->md.MsgId,           /* reset MsgId to get a new one    */
             MQMI_NONE, sizeof(ibmmq_put_ptr->md.MsgId) );

      MQPUT(ibmmq_put_ptr->Hcon,                /* connection handle               */
            ibmmq_put_ptr->Hobj,                /* object handle                   */
            &ibmmq_put_ptr->md,                 /* message descriptor              */
            &ibmmq_put_ptr->pmo,                /* default options (datagram)      */
            messlen,             /* message length                  */
            put_msg,              /* message buffer                  */
            &ibmmq_put_ptr->CompCode,           /* completion code                 */
            &ibmmq_put_ptr->Reason);            /* reason code                     */

      /* report reason, if any */
      if (ibmmq_put_ptr->Reason != MQRC_NONE)
      {
        #ifdef NS_DEBUG_ON
          fprintf(stderr, "MQPUT ended with reason code %d\n", ibmmq_put_ptr->Reason);
        #endif
      }
    }
    else   /* satisfy end condition when empty line is read */
      ibmmq_put_ptr->CompCode = MQCC_FAILED;
  }

  return NS_IBMMQ_SUCCESS;
}

int ns_ibmmq_close_connection(IBMMQ_Conn_Info *ibmmq_put_ptr)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_ibmmq_close()\n");
  #endif
  
  /******************************************************************/
  /*                                                                */
  /*   Close the target queue (if it was opened)                    */
  /*                                                                */
  /******************************************************************/
  if (ibmmq_put_ptr->OpenCode != MQCC_FAILED)
  {
    ibmmq_put_ptr->C_options = MQCO_NONE;        /* no close options             */

    MQCLOSE(ibmmq_put_ptr->Hcon,                   /* connection handle            */
            &ibmmq_put_ptr->Hobj,                  /* object handle                */
            ibmmq_put_ptr->C_options,
            &ibmmq_put_ptr->CompCode,              /* completion code              */
            &ibmmq_put_ptr->Reason);               /* reason code                  */

    /* report reason, if any     */
    if (ibmmq_put_ptr->Reason != MQRC_NONE)
    {
      #ifdef NS_DEBUG_ON
        fprintf(stderr, "MQCLOSE ended with reason code %d\n", ibmmq_put_ptr->Reason);
      #endif
    }
  }

  /******************************************************************/
  /*                                                                */
  /*   Disconnect from MQM if not already connected                 */
  /*                                                                */
  /******************************************************************/
  if (ibmmq_put_ptr->CReason != MQRC_ALREADY_CONNECTED)
  {
    MQDISC(&ibmmq_put_ptr->Hcon,                   /* connection handle            */
           &ibmmq_put_ptr->CompCode,               /* completion code              */
           &ibmmq_put_ptr->Reason);                /* reason code                  */

    /* report reason, if any     */
    if (ibmmq_put_ptr->Reason != MQRC_NONE)
    {
      #ifdef NS_DEBUG_ON
        fprintf(stderr, "MQDISC ended with reason code %d\n",ibmmq_put_ptr->Reason);
      #endif
    }
  }

  return NS_IBMMQ_SUCCESS;
}

int ns_ibmmq_get(IBMMQ_Conn_Info *ibmmq_get_ptr, char *get_msg, int get_msg_len)
{
  #ifdef NS_DEBUG_ON
    fprintf(stderr, "Method Entry. ns_ibmmq_get()\n");
  #endif

  /******************************************************************/
  /*                                                                */
  /*   Get messages from the message queue                          */
  /*   Loop until there is a failure                                */
  /*                                                                */
  /******************************************************************/
  ibmmq_get_ptr->O_options = MQOO_INPUT_AS_Q_DEF    /* open queue for input      */
                             | MQOO_FAIL_IF_QUIESCING /* but not if MQM stopping   */
                             ;                        /* = 0x2001 = 8193 decimal   */


  MQOPEN(ibmmq_get_ptr->Hcon,                      /* connection handle            */
         &ibmmq_get_ptr->od,                       /* object descriptor for queue  */
         ibmmq_get_ptr->O_options,                 /* open options                 */
         &ibmmq_get_ptr->Hobj,                     /* object handle                */
         &ibmmq_get_ptr->OpenCode,                 /* MQOPEN completion code       */
         &ibmmq_get_ptr->Reason);                  /* reason code                  */

  /* report reason, if any; stop if failed      */
  if (ibmmq_get_ptr->Reason != MQRC_NONE)
  {
    #ifdef NS_DEBUG_ON
      fprintf(stderr, "MQOPEN ended with reason code %d\n", ibmmq_get_ptr->Reason);
    #endif
  }

  if (ibmmq_get_ptr->OpenCode == MQCC_FAILED)
  {
    #ifdef NS_DEBUG_ON
      fprintf(stderr, "unable to open queue for output\n");
    #endif
  }


  ibmmq_get_ptr->CompCode = ibmmq_get_ptr->OpenCode;
  ibmmq_get_ptr->gmo.Options = MQGMO_NO_WAIT           /* wait for new messages       */
                                | MQGMO_NO_SYNCPOINT   /* no transaction              */
                                | MQGMO_CONVERT        /* convert if necessary        */
                                | MQGMO_ACCEPT_TRUNCATED_MSG;


  ibmmq_get_ptr->buflen = get_msg_len - 1; 
  ibmmq_get_ptr->messlen = 0;
  memcpy(ibmmq_get_ptr->md.MsgId, MQMI_NONE, sizeof(ibmmq_get_ptr->md.MsgId));
  memcpy(ibmmq_get_ptr->md.CorrelId, MQCI_NONE, sizeof(ibmmq_get_ptr->md.CorrelId));
  ibmmq_get_ptr->md.Encoding       = MQENC_NATIVE;
  ibmmq_get_ptr->md.CodedCharSetId = MQCCSI_Q_MGR;

  MQGET(ibmmq_get_ptr->Hcon,                /* connection handle                 */
        ibmmq_get_ptr->Hobj,                /* object handle                     */
        &ibmmq_get_ptr->md,                 /* message descriptor                */
        &ibmmq_get_ptr->gmo,                /* get message options               */
        ibmmq_get_ptr->buflen,              /* buffer length                     */
        get_msg,                            /* message buffer                    */
        &ibmmq_get_ptr->messlen,            /* message length                    */
        &ibmmq_get_ptr->CompCode,           /* completion code                   */
        &ibmmq_get_ptr->Reason);            /* reason code                       */

  /* report reason, if any     */
  if (ibmmq_get_ptr->Reason != MQRC_NONE)
  {
    if (ibmmq_get_ptr->Reason == MQRC_NO_MSG_AVAILABLE)
    {                         /* special report for normal end    */
      printf("MQGET has no msg %d\n", ibmmq_get_ptr->Reason);
      return 2;
    }
    else                      /* general report for other reasons */
    {
      #ifdef NS_DEBUG_ON 
        fprintf(stderr, "MQGET ended with reason code %d, as messlen = %ld\n", ibmmq_get_ptr->Reason, ibmmq_get_ptr->messlen);
      #endif
      /*   treat truncated message as a failure for this sample   */
      if (ibmmq_get_ptr->Reason == MQRC_TRUNCATED_MSG_FAILED)
      {
        ibmmq_get_ptr->CompCode = MQCC_FAILED;
        return NS_IBMMQ_ERROR;
      }
    }
  }

  if (ibmmq_get_ptr->CompCode != MQCC_FAILED)
  {
    get_msg[ibmmq_get_ptr->messlen] = '\0';
    #ifdef NS_DEBUG_ON 
      fprintf(stderr, "Received Message is '%s'\n", get_msg);
    #endif
  }
  else
  {
    #ifdef NS_DEBUG_ON 
      fprintf(stderr, "Error In Receiving IBMMQ Msg\n");
    #endif
    
    return NS_IBMMQ_ERROR;
  }

  return NS_IBMMQ_SUCCESS;
}
