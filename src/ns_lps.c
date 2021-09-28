/************************************************************************************************
 * File Name            : ns_lps.c
 * Author(s)            : Abhishek Mittal
 * Date                 :
 * Copyright            : (c) Cavisson Systems
 * Purpose              : 
 *
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "url.h"
#include "util.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_parent.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_msg_com_util.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_check_monitor.h"
#include "ns_user_monitor.h"
#include "ns_mon_log.h"
#include "nslb_util.h"
#include "ns_ndc.h"
#include "ns_lps.h"
#include "wait_forever.h"
#include "nslb_sock.h"
#include "ns_kw_usage.h"
#include "ns_error_msg.h"
#include "ns_monitor_profiles.h"
#include "nslb_cav_conf.h"

#define LPS_READ_BUF_SIZE ((16 * 1024) - 1)

void create_lps_msg(char *msg_buf, int *len);

/* LPS_SERVER <SERVER> <PORT> <MODE>
 * MODE is optional field. Default value is 2.
 * Mode 0 -> Disable. will run only Service monitor by setting default ip port for Service monitor
 * Mode 1 -> Enable.  will run Service monitor, Special monitor and Log monitor
 * Mode 2 -> Enable (default). will run only service monitor 
*/
int kw_set_log_server(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char lps_ip[MAX_DATA_LINE_LENGTH] = "\0";
  char lps_port[MAX_DATA_LINE_LENGTH] = "\0";
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  char tmp_buf[MAX_DATA_LINE_LENGTH] = "\0";
  char *val;
  char fname[MAX_DATA_LINE_LENGTH] = "\0";
  int ret = 0; 

  int num;
  int lps_mode = 1;

  sprintf(fname, "%s/lps/conf/%s", g_ns_wdir, LPS_CONFIG);

  NSDL4_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  if ((num = sscanf(buf, "%s %s %s %d %s", keyword, lps_ip, lps_port, &lps_mode, tmp_buf)) < 3) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, LPS_SERVER_USAGE, CAV_ERR_1011359, keyword);
  }

  NSDL3_PARSING(NULL, NULL, "lps_ip = %s, lps_port = %s, lps_mode = %d", lps_ip, lps_port, lps_mode);
  // TODO: add validation of IP
  val = lps_port;
  if(val != NULL)
  {
    CLEAR_WHITE_SPACE(val);
    NSDL3_PARSING(NULL, NULL, "lps_port = %s", lps_port);
    if(val == NULL)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, LPS_SERVER_USAGE, CAV_ERR_1060050);
    }

    //Now, if user gives NA in place of port in NET_DIAGNOSTIC_SERVER keyword, then 
    // port will be read from ndc.conf file
    if(strcmp(lps_ip, "NA") == 0)
    {
      if(is_outbound_connection_enabled)
      {
        strcpy(lps_ip,global_settings->net_diagnostics_server);
        NSDL3_PARSING(NULL, NULL, "LPS_SERVER= %s  in outbound case", lps_ip);
      }
      else
      {
        strcpy(lps_ip,"127.0.0.1");
        NSDL3_PARSING(NULL, NULL, " LPS_SERVER= %s in inbound case", lps_ip);
      }
    }
    if(strcmp(lps_port, "NA") == 0)
    {
      if(is_outbound_connection_enabled)
      { 
        ret = nslb_parse_keyword(fname, "NDC_LPS_PORT", lps_port);
      }
      else
      {
        ret = nslb_parse_keyword(fname, "PORT", lps_port);
        if(ret == -1)
          ret = nslb_parse_keyword(fname, "LPS_PORT", lps_port);
      }
      if(ret == -1)
      {
        NS_KW_PARSING_ERR(keyword, 0, err_msg, LPS_SERVER_USAGE, CAV_ERR_1060051);
      }
      global_settings->lps_port = (int )atoi(lps_port);
    }
    else if(ns_is_numeric(lps_port) == 0)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, LPS_SERVER_USAGE, CAV_ERR_1060052); 
    }
    else
      global_settings->lps_port = (int )atoi(lps_port);
  }
  if((lps_mode != 0) && (lps_mode != 1) && (lps_mode != 2))
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, LPS_SERVER_USAGE, CAV_ERR_1060053);
  }  

  NSDL3_PARSING(NULL, NULL, " LPS_SERVER= %s, %d", lps_ip, atoi(lps_port));

  strcpy(global_settings->lps_server, lps_ip);

  global_settings->lps_mode = lps_mode;

  NSDL4_PARSING(NULL, NULL, "Method exiting, global_settings->lps_server = %s , global_settings->lps_port = %d", global_settings->lps_server, global_settings->lps_port);
  return 0;
}

////////////////////////////////////////////

//lps_con_success
static inline void lps_con_succ()
{
  char SendMsg[4096] = "\0";
  int len;

  NSDL1_MON(NULL, NULL, "Method called.");
  create_lps_msg(SendMsg, &len);
  NSTL1(NULL, NULL, "Sending Message to LPS after recovery. SendMessage : %s", SendMsg);
  //Send message to LPS
  write_msg(&lps_mccptr, (char *)&SendMsg, len, 0, CONTROL_MODE);
  NSDL1_MON(NULL, NULL, "Method End.");
}


static int connect_to_lps_nb(char *ip, unsigned short port)
{ 
  int fd;
  char err_msg[2 * 1024] = "\0";
  int con_state;
  
  //Opening a non-blocking socket.
  if((fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  //If We need local connection, then we need to open a socket fo AF_UNIX family. Data transfer takes fast as it is fully dedicated to connect to local machines rather than a TCP connection which is able to make connection with all types of machines. Here NS is a client and trying to connect LPS. If we are opening a UNIX socket it means NDC must also have an open UNIX socket.
  { 
    NSDL1_MON(NULL, NULL, "Error: Error in opening socket");
    return -1;
  }

  /* Initialize lps_mccptr */
  init_msg_con_struct(&lps_mccptr, fd, CONNECTION_TYPE_OTHER, global_settings->lps_server, NS_LPS_TYPE);
  NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d",
                         fd, ip, port);
  //Calling non-blocking connect
  int con_ret = nslb_nb_connect(lps_mccptr.fd, global_settings->lps_server, global_settings->lps_port, &con_state, err_msg);
  
  NSDL3_MON(NULL, NULL, "con_ret = %d, con_state = %d", con_ret, con_state);
  if(con_ret == 0)
  {
    //Connected Ok. 
    add_select_msg_com_con((char *)&lps_mccptr, lps_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
    lps_mccptr.state |= NS_CONNECTED;
    lps_con_succ();
  }
  else if(con_ret > 0)
  {
    if(con_state == NSLB_CON_CONNECTED)
    {
      //Connect Ok.
      add_select_msg_com_con((char *)&lps_mccptr, lps_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
      lps_con_succ();
    }
    else if(con_state == NSLB_CON_CONNECTING)
    {
      //Connecting state, need to add fd on EPOLLOUT
       lps_mccptr.state |= NS_CONNECTING;
       NSTL1(NULL, NULL, "Connecting to LPS at ip address %s and port %d\n", ip, port);
       // Note - Connection event comes as EPOLLOUT
       add_select_msg_com_con((char *)&lps_mccptr, lps_mccptr.fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP);
    }
    else
    {
      add_select_msg_com_con((char *)&lps_mccptr, lps_mccptr.fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP);
      NSTL1(NULL, NULL, "Unknown status of connections while connecting with LPS at ip address %s and port %d\n", ip, port);
    }
  }
  else //Error case. We need to restart again
  {
    NSTL1(NULL, NULL, "Error: Unable to connect LPS hence returning. IP = %s, port = %d", ip, port);
    close(lps_mccptr.fd);
    lps_mccptr.fd = -1;
    return -1;
  }
  return 0;
}

//Connect to LPS by NS
int lps_recovery_connect()
{
  //Connect NS to LPS with non blocking connect
  NSTL1(NULL, NULL, "Connecting to LPS at ip address %s and port %d\n", global_settings->lps_server, global_settings->lps_port);

  if ((connect_to_lps_nb(global_settings->lps_server, global_settings->lps_port)) < 0)
  {
    NSTL1(NULL, NULL, "Error in creating the TCP socket to communicate with the LPS (%s:%d).",
           global_settings->lps_server, global_settings->lps_port);
  }
  return 0;
}




//Function Name :- start_ns_lps
//Parameter     :- null
//Purpose       :- It will return if runing test is not CMT or enable_health_monitor is not enable.
//                 If all good then it will call connect_to_lps_nb();                               
void start_ns_lps()
{
  //sleep(30); why this delay is added for lps ???
  if(!(global_settings->enable_health_monitor) || !(global_settings->continuous_monitoring_mode))
  {
    NSTL3(NULL, NULL, "Not making control connection with LPS because of enable_health_monitor = %d or continuous_monitoring_mode = %d \n",
                                                               global_settings->enable_health_monitor, global_settings->continuous_monitoring_mode);
    return;
  }

  if(!(global_settings->lps_port) || (global_settings->lps_server[0] == '\0')) 
  {
    NSTL3(NULL, NULL, "Error: Unable to connect because of lps_port = %d or lps_server[0] = %s \n",
                                                               global_settings->lps_port, global_settings->lps_server[0]); 
    return;
  }
  connect_to_lps_nb (global_settings->lps_server, global_settings->lps_port);

  return;
}

void create_lps_msg(char *msg_buf, int *len)
{
  pid_t ns_pid = getpid();
  NSDL1_MON(NULL, NULL, "Method called");
  //g_ns_wdir -> use this or get controller name using strrchr 
  *len = sprintf(msg_buf, "ns_lps_control_conn:TEST_RUN=%d;NS_PORT=%hu;CONTROLLER=%s;NS_PID=%d;NS_INTERVAL=%d\n",testidx, parent_port_number, g_ns_wdir, ns_pid, global_settings->progress_secs );
   NSDL1_MON(NULL, NULL, "msg_buff is %s",msg_buf);
}


///////////////////////////////////////////

/* Function is used to connect non blocking connection, returns -1 on error and 0 on success
 * */
int chk_connect_to_lps() //to be change for lps
{
  int con_state;
  char err_msg[2 * 1024] = "\0";

  NSTL1(NULL, NULL, "LPS:State is  NS_CONNECTING so try to reconnect for fd %d and to server ip = %s, port = %d", lps_mccptr.fd, global_settings->lps_server, global_settings->lps_port);

  nslb_nb_connect(lps_mccptr.fd, global_settings->lps_server, global_settings->lps_port, &con_state, err_msg);

  if(con_state != NSLB_CON_CONNECTED)
  {
    NSTL1(NULL, NULL, "LPS:Still not connected. err_msg = %s", err_msg);
    close_msg_com_con(&lps_mccptr);
    return -1;
  }
  //Else socket is connected, add socket to EPOLL IN
  NSTL1(NULL, NULL, "LPS:Connected successfully with fd %d and to server ip = %s", lps_mccptr.fd, global_settings->lps_server);
  lps_mccptr.state &= ~NS_CONNECTING;
  mod_select_msg_com_con((char *)&lps_mccptr, lps_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  return 0; //Success case
}
///////////////////////////////

inline void read_lps_reply_msg()
{
  NSDL1_MON(NULL, NULL, "Method Called");
  static char read_msg[4095 + 1] = {0};
  int read_bytes;
  //Not handling any data received from LPS, as LPS will not send any data on thios connection.
  if((read_bytes = read(lps_mccptr.fd, read_msg, 4095)) < 0)
  {
    NSTL1(NULL, NULL, "Error: Their is no data in the read_msg");   // Error case
    return ;
  }
  else if(read_bytes == 0)  //Connection closed
  {
    close_msg_com_con(&lps_mccptr);
  }
  NSDL1_MON(NULL, NULL, "read_msg = %s, read_bytes = %d", read_msg, read_bytes);
  return;
}

/////////////////////
inline void handle_lps(struct epoll_event *pfds, int i)
{   
  //char SendMsg[2048] = "\0";
  //Msg_com_con *mccptr =  (Msg_com_con *)pfds[i].data.ptr;  

  NSDL1_MON(NULL, NULL, "Method Called");

  if (pfds[i].events & EPOLLOUT)
  {   
    NSDL1_MON(NULL, NULL, "Received EPOLLOUT event");
    /*In case of recovery we are creating a non blocking connection therefore need to verify connection state*/
    if (lps_mccptr.state & NS_CONNECTING)
    {   
      int ret = chk_connect_to_lps();
      NSDL1_MON(NULL, NULL, "Return value of chk_connect_to_lps, ret = %d", ret);
      if (ret == -1)
        return;
      //This point connection is established. Currently we are sending same message for recovery and partiton switching
      //so no need to check for state
      //Here we do not need to do add_select bec we have already done in case of recovery 
      lps_con_succ();
      return;
    }

    if (lps_mccptr.state & NS_STATE_WRITING)
     write_msg(&lps_mccptr, NULL, 0, 0, CONTROL_MODE);

  } else if (pfds[i].events & EPOLLIN) {
     NSDL1_MON(NULL, NULL, "Received EPOLLIN event");
     read_lps_reply_msg(); 
  } else if (pfds[i].events & EPOLLHUP){
    NSTL1(NULL, NULL, "EPOLLHUP occured on sock %s. error = %s",
                    msg_com_con_to_str(&lps_mccptr), nslb_strerror(errno));
    NSDL1_MON(NULL, NULL, "EPOLLHUP occured on sock %s. error = %s",
                    msg_com_con_to_str(&lps_mccptr), nslb_strerror(errno));
    close_msg_com_con_and_exit(&lps_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
  } else if (pfds[i].events & EPOLLERR){
    NSTL1(NULL, NULL, "EPOLLERR occured on sock %s. error = %s",
                msg_com_con_to_str(&lps_mccptr), nslb_strerror(errno));
     NSDL3_MON(NULL, NULL, "EPOLLERR occured on sock %s. error = %s",
                msg_com_con_to_str(&lps_mccptr), nslb_strerror(errno));
     close_msg_com_con_and_exit(&lps_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
  } else {    
    NSTL1(NULL, NULL, "This should not happen.");
    NSDL3_MON(NULL, NULL, "This should not happen.");

  }      
}




