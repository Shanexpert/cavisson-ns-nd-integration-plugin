
/************************************************************************************
 * Name      : ns_rte_ssh.c 
 * Purpose   : This file contains functions related to rte protocol 
 * Author(s) : Devendra Jain/Atul Sharma 
 * Date      : 12 January 2017 
 * Copyright : (c) Cavisson Systems
 * Modification History : 1. Vikas Verma: Use channel and session of vptr instead of ns_rte(Bug 48059)
 ***********************************************************************************/

#include "ns_rte_ssh.h"
/*--------------------------------------------------------------------------------------------- 
 * Name      : ns_rte_ssh_connect
 * Purpose   : This function will do following - 
 *             (1) Will create new ssh session by given host name and user
 *             (2) Authenticate the user name and password  
 *
 * Input     :  rte structure - provide scructure of rte type
 *              host name     - provide host name from which we wants to connect
 *              user name     - provide user name
 *              password      - password 
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int ns_rte_ssh_connect(ns_rte *rte, char *host , char *user , char *password)
{
  int rc;
  ssh_session session;
  VUser *vptr = TLS_GET_VPTR();
  char host_buf[128];
  char *port;
  NSDL2_RTE(vptr, NULL,"Method called, host = %s , user = %s , password = %s", host , user, password);
  if(!rte || !host || !host[0])
  { 
    NSDL2_RTE(vptr, NULL,"NULL input received");
    NSTL1(vptr, NULL,"NULL input received");
    return -1;
  }

  session = ssh_new();
  if(session == NULL)
  { 
    NSDL2_RTE(vptr, NULL,"failed to create session");
    NSTL1(vptr, NULL,"failed to create session");
    return -1;
  }

  strcpy(host_buf, host);
  host = host_buf;
  if((port = strchr(host, ':')) != NULL)
  {
    *port = '\0';
    port++;
    ssh_options_set(session, SSH_OPTIONS_PORT_STR, port);
  }
  ssh_options_set(session, SSH_OPTIONS_HOST, host);

  if(user && user[0])
    ssh_options_set(session, SSH_OPTIONS_USER, user);

  if(rte->kex[0])
    ssh_options_set(session, SSH_OPTIONS_KEY_EXCHANGE, rte->kex);

  rc = ssh_connect(session);
  if (rc != SSH_OK) 
  {
    NSDL2_RTE(vptr, NULL,"failed to connect session");
    NSTL1(vptr, NULL,"failed to connect session");
    ssh_free(session);
    return -1;
  }

  if(password && password[0])
  {
    rc = ssh_userauth_password(session, NULL, password);
    if (rc != SSH_AUTH_SUCCESS)
    {
      NSDL3_RTE(vptr, NULL,"failed to authenticate session rc = %d", rc);
      NSTL1(vptr, NULL,"failed to authenticate session");
      ssh_disconnect(session);
      ssh_free(session);
      return -1;
    }
  }
  else
    ssh_userauth_none(session, NULL);

  vptr->session = (void*)session;
  return 0;
}

int ns_rte_ssh_disconnect(ns_rte *rte)
{
  ssh_session session;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called");
  if(!rte || !vptr->session)
  {
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }
  if(ns_rte_ssh_close(rte) < 0)
  {
    NSTL1(NULL,NULL,"failed to close session");
    return -1;
  }
  session = (ssh_session)vptr->session;
  ssh_disconnect(session);
  ssh_free(session);
  vptr->session=NULL;
  return 0;
}

int ns_rte_ssh_open(ns_rte *rte)
{
  int rc ;
  ssh_channel channel;
  ssh_session session;
  VUser *vptr = TLS_GET_VPTR(); 

  NSDL3_RTE(vptr, NULL, "Method called");
  if(!rte || !vptr->session)
  {
    NSDL2_RTE(vptr, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }
  session = (ssh_session)vptr->session;

  channel = ssh_channel_new(session);
  if (channel == NULL)
  {
    NSDL2_RTE(NULL,NULL,"failed to create channel");
    NSTL1(NULL,NULL,"failed to create channel");
    return -1;
  }

  rc = ssh_channel_open_session(channel);
  if (rc != SSH_OK)
  {
    ssh_channel_free(channel);
    NSDL2_RTE(NULL,NULL,"failed to open channel");
    NSTL1(NULL,NULL,"failed to open channel");
    return -1;
  }
  vptr->channel = (void*) channel;

  return 0;
}

int ns_rte_ssh_login(ns_rte *rte)
{
  int rc;  
  ssh_channel channel;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called");
  if(!rte)
  {
    NSDL2_RTE(NULL, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }

  if(ns_rte_ssh_open(rte) < 0)
  {
    NSTL1(NULL,NULL,"failed to open session");
    return -1;
  }

  channel = (ssh_channel)vptr->channel;
  if(rte->ttype[0])
  {
     ssh_channel_request_pty_size(channel, rte->ttype, 80, 24);
  }
  else
  {
    rc = ssh_channel_request_pty(channel);
    if (rc != SSH_OK) 
    {
      NSTL1(NULL,NULL,"failed to open session pty");
      return -1;
    }
    rc = ssh_channel_change_pty_size(channel, 80, 24);
    if (rc != SSH_OK) 
    {
      NSTL1(NULL,NULL,"failed to change session pty size");
      return -1;
    }
  }
  rc = ssh_channel_request_shell(channel);
  if (rc != SSH_OK) 
  {  
    NSTL1(NULL,NULL,"failed to open session shell");
    return -1;
  }
  return 0;
}

int ns_rte_ssh_close(ns_rte *rte)
{
  ssh_channel channel;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called");
  if(!rte || !vptr->channel)
  {
    NSDL2_RTE(vptr, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }
  channel = (ssh_channel)vptr->channel;
  ssh_channel_send_eof(channel);
  ssh_channel_close(channel);
  ssh_channel_free(channel);
  return 0;
}

int ns_rte_ssh_send_cmd(ns_rte *rte , char *cmd)
{
  ssh_channel channel;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called, cmd = %s", cmd);
  if(!rte || !vptr->channel)
  {
    NSDL2_RTE(vptr, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }
  channel = (ssh_channel)vptr->channel;
  if (ssh_channel_request_exec(channel, cmd) < 0) 
  {
    NSTL1(NULL,NULL,"failed to execute command");
    return -1;
  }
  return 0;
}

int ns_rte_ssh_send_text(ns_rte *rte , char *text)
{
  ssh_channel channel;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called, text = %s", text);

  if(!rte || !vptr->channel)
  {
    NSDL2_RTE(vptr, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return -1;
  }
  channel = (ssh_channel)vptr->channel;
  if (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
  {   
    int nbytes = strlen(text);
    int nwritten = ssh_channel_write(channel, text, nbytes);
    if (nwritten != nbytes) 
    {
      NSTL1(NULL,NULL,"failed to write text");
      return -1;
    }
  }
  return 0;
}

int ns_rte_ssh_wait_sync(ns_rte *rte)
{
  return 0;
}

int ns_rte_ssh_wait_text(ns_rte *rte, char *text , int duration)
{
  time_t start , current;
  int interval;
  int status = -1;
  int nbyte;
  unsigned int r;
  char buffer[1024];
  ssh_channel channel;
  VUser *vptr = TLS_GET_VPTR();

  NSDL3_RTE(vptr, NULL, "Method called, text = %s, duration = %d", text, duration);
  if(!rte || !vptr->channel)
  {
    NSDL2_RTE(vptr, NULL, "NULL input received");
    NSTL1(NULL,NULL,"NULL input received");
    return status;
  }
  channel = (ssh_channel)vptr->channel;
 
  time(&start);

  while(1)
  {
    while(ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel) && (r = ssh_channel_poll(channel,0))!=0)
    {
      memset(buffer,0,1024);
      nbyte = ssh_channel_read(channel,buffer,sizeof(buffer),0);
      if(nbyte < 0)
      {
        NSDL2_RTE(NULL,NULL,"failed to read text");
        NSTL1(NULL,NULL,"failed to read text");
        return status; //Error in Read
      }  
      NSDL4_RTE(NULL,NULL,"Data Received : %s",buffer);
      if(nbyte == 0)
      {
        NSDL2_RTE(NULL,NULL,"Read Complete");
        return status; //Read Complete
      }
      if(status)
      {
        char *p = strstr(buffer,text);
        if(p)
        {
          status = 0; //Matched
        }
      }
    }
    if(!status)
    {
      NSDL2_RTE(NULL,NULL,"Input Matched!!!");
      return status;
    } 
    ns_page_think_time(0.5);//In seconds
    time(&current); 
    interval = current - start;
    if(interval > duration)
    {
      NSDL2_RTE(NULL,NULL,"Input Timeout");
      return status; //Timeout
    }
  }
  return status;
}


int ns_rte_ssh_config(ns_rte *rte, char *input)
{
   char *ptr;
   char* tokens[8];
   int field_count;
   int i;
   NSDL3_RTE(NULL, NULL, "Method called, input = %s", input);
   //KeyAlgorithms=diffie-hellman-group1-sha1,TerminalType=vt100
   field_count = nslb_get_tokens(input, tokens,",", 8);
   rte->kex[0] = '\0';
   rte->ttype[0] = '\0';
   for(i=0; i<field_count; i++)
   { 
     NSDL3_RTE(NULL, NULL, "Argument = %s", tokens[i]);
     if((ptr = strstr(tokens[i], "KeyAlgorithms=")))
     {
       ptr+=14;
       strncpy(rte->kex, ptr, 128);
       rte->kex[128] = '\0';
     }
     else if((ptr = strstr(tokens[i], "TerminalType=")))
     {
       ptr+=13;
       strncpy(rte->ttype, ptr, 128);
       rte->ttype[128] = '\0';
     }
   }
   return 0;
}
