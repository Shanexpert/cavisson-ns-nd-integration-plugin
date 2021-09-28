/******************************************************************
 * Name    :    ns_msg_com_util.c
 * Purpose :    Utility method of message commnucation between
 *              parent, child and master.
 * Note    :
 * Author  :    Neeraj/Bhav
 * Intial version date:    04/13/08
 * Last modification date: 04/13/08
*****************************************************************/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <sys/epoll.h>
#include <regex.h>
#include <openssl/md5.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "wait_forever.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"

#include  "ns_parent.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_vuser.h"
#include "ns_trace_level.h"
#include "ns_pre_test_check.h"
#include "ns_ip_data.h"
#include "nslb_cav_conf.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_server_ip_data.h"
#include "ns_dynamic_avg_time.h"
#include "ns_exit.h"
#include "ns_data_handler_thread.h"
#include "ns_error_msg.h"
#include "nslb_http_auth.h"
#include "decomp.h"
#include "comp_decomp/nslb_comp_decomp.h"

//Changing macro value to 5000 as it is timeout in millisecons which is to be set in epoll.
#define TIMEOUT_FOR_ACCEPT_FROM_TOOLS 5 * 1000
int g_msg_com_epfd = 0;
int g_dh_msg_com_epfd = 0;
int listen_fd = -1;  // Parent or master listen fd
int data_handler_listen_fd = -1;  //Parent to NVM data listen fd
//Client_data *client_data = NULL;
Msg_com_con *g_master_msg_com_con = NULL; // For Master <-> Parent message communication connection data(Parent maintains it)
Msg_com_con *g_msg_com_con = NULL; // For Parent <-> Child message communication connection data
Msg_com_con *g_msg_com_con_nvm0 = NULL; // For Parent <-> Child message communication connection data
Msg_com_con *parent_listen_msg_com_con;

Msg_com_con *g_dh_msg_com_con = NULL;
//Msg_com_con *g_dh_msg_com_con_nvm0 = NULL;
Msg_com_con *dh_listen_msg_com_con;

//static int num_msg_com_con = 0;
//static int num_data_handler_msg_com_con = 0;
unsigned short parent_port_number;
unsigned short g_dh_listen_port;
unsigned short event_logger_port_number = 0; //Netomni Changes:Setting default value to 0
extern int total_pdf_data_size;
extern int total_dynamic_vector_mon_entries; // for num_connections_estimate
extern int g_rtc_msg_seq_num;
/*bug 92660: declare parent_listen_fd*/
extern int parent_listen_fd;

#if 0
static void check_continue_test_on_gen_fail()
{
  int idx, killed_gen = 0, active_gen = 0;
  ContinueTestOnGenFailure *CTOGF_setting = &global_settings->con_test_gen_fail_setting;
  NSDL2_PARENT(NULL, NULL, "Method called, num_connected = %d, percentage of gen to start test = %d, "
                           " num_process = %d, Total timeout = %d, gen expected = %d",
                           g_data_control_var.num_connected, CTOGF_setting->percent_started,
                           global_settings->num_process, CTOGF_setting->start_timeout, num_gen_expected);

  for(idx = 0;idx < global_settings->num_process; idx++) {
    if(generator_entry[idx].flags & IS_GEN_ACTIVE) {
      active_gen++;
    }
    else {
      killed_gen++;
    }
  }

  NSTL1(NULL, NULL, "Active Generator = %d, InActive Generator = %d", active_gen, killed_gen); 
  if(active_gen < num_gen_expected) //Failure
  {
    NS_EXIT(-1, "Failed to start %d%% of generators in %d seconds, Active/Expected generator [%d/%d]",
                 CTOGF_setting->percent_started, CTOGF_setting->start_timeout, active_gen, num_gen_expected);
  }
  
  //Ignoring killed gen as they are unable to connect
  g_data_control_var.total_killed_gen = killed_gen; g_data_control_var.num_connected = active_gen;
}
#endif

// This is used by child to connect to parent
int connect_to_parent_dh()
{
  int fd;

  NSDL1_MESSAGES(NULL, NULL, "Method called");

  if ((fd = nslb_tcp_client("127.0.0.1", g_dh_listen_port)) < 0)
    return fd; /* error handled at the caller side */

  /* Anil - we need to set it to no-blocking */
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    return -1;

  init_msg_con_struct(&g_dh_child_msg_com_con, fd, CONNECTION_TYPE_CHILD_OR_CLIENT, "127.0.0.1", NS_STRUCT_TYPE_NVM_PARENT_DATA_COM);
  sprintf(g_dh_child_msg_com_con.conn_owner, "[DATA] NVM_TO_PARENT");
  return fd;
}

// This is used by child to connect to parent
int connect_to_parent()
{
  int fd;

  NSDL1_MESSAGES(NULL, NULL, "Method called");

  if((fd = nslb_tcp_client("127.0.0.1", parent_port_number)) < 0)
    return fd; /* error handled at the caller side */

  /* Anil - we need to set it to no-blocking */
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
  {
    fprintf(stderr, CAV_ERR_1000014, fd, errno, nslb_strerror(errno));
    return -1;
  }

  NSDL1_MESSAGES(NULL, NULL, "fd=%d g_child_msg_com_con=%p parent_port_number=%d", fd, &g_child_msg_com_con, parent_port_number);
  init_msg_con_struct(&g_child_msg_com_con, fd, CONNECTION_TYPE_CHILD_OR_CLIENT, "127.0.0.1", NS_STRUCT_TYPE_NVM_PARENT_COM);
  sprintf(g_child_msg_com_con.conn_owner, "[CONTROL] NVM_TO_PARENT");
  return fd;
}

// Do a non blocking connect to nsa_log_mgr
int connect_to_event_logger_nb(char *ip, unsigned short port) {
  
  int fd;
  char err_msg[2 * 1024] = "\0";
  int con_state;

  //Opening a non-blocking socket.
  if((fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  {
    NSDL3_MESSAGES(NULL, NULL, "Error: Error in opening socket");
    return -1;
  }
  else //Socket opened sucessfully
  {
    init_msg_con_struct(&g_el_subproc_msg_com_con, fd, CONNECTION_TYPE_OTHER, ip, NS_STRUCT_TYPE_LOG_MGR_COM);
    //Doing add_select before connect because we can get event in between 'connect & add_select
    add_select_msg_com_con((char*)&g_el_subproc_msg_com_con, fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT);
    //Connecting NVM with event logger
    sprintf(g_el_subproc_msg_com_con.conn_owner, "NVM_TO_EVENT_LOGGER");
    NSDL3_MESSAGES(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d",
                           fd, ip, port);
    //Calling non-blocking connect
    int con_ret = nslb_nb_connect(fd, ip, port, &con_state, err_msg);

    NSDL3_MESSAGES(NULL, NULL, "con_ret = %d", con_ret);
    if(con_ret == 0)
    {
      //Connected Ok. Init msg_con_struct
      g_el_subproc_msg_com_con.state |= NS_CONNECTED;
      NSTL1(NULL, NULL, "Connection established with event logger at ip address %s and port %d\n", ip, port);
    }
    else if(con_ret > 0)
    {
      if(con_state == NSLB_CON_CONNECTED)
      {
        //Connect Ok. Init msg_con_struct
        g_el_subproc_msg_com_con.state |= NS_CONNECTED;
        NSTL1(NULL, NULL, "Connection established with event logger at ip address %s and port %d\n", ip, port);
      }
      else if(con_state == NSLB_CON_CONNECTING)
      {
        //Connecting state, need to add fd on EPOLLOUT
         g_el_subproc_msg_com_con.state |= NS_CONNECTING;
         NSTL1(NULL, NULL, "Connecting to event logger at ip address %s and port %d\n", ip, port);
      }
      else
      {
        NSTL1(NULL, NULL, "Unknown status of connections while connecting with event logger at ip address %s and port %d\n", ip, port);
      }
    }
    else //Error case. We need to restart again
    {
      close(fd);
      fd = -1;
      return -1;
    }
  }

  return 0;
}


int connect_to_event_logger_ex(char *ip, unsigned short port, int con_mode) {
  int fd;
  char err_msg[1024]="\0";
  int attempt = NC_MAX_RETRY_ON_CONN_FAIL + 1; //Do it one plus max retry, as using do-while.
  
 
  NSDL1_MESSAGES(NULL, NULL, "Method called, ip = %s:%hu", ip, port);
  NSTL1(NULL, NULL, "Connecting to event logger at ip address %s and port %d", ip, port);

  //On Failure retry for 5 time,
  //On success, break the loop
  //On failure after all attempt, do as previous
  do{
    if((fd = nslb_tcp_client_ex(ip, port, 30, err_msg)) < 0)
    {  
      attempt--;
      if(attempt == 0)
        return fd; /* error handled at the caller side */
      NSTL1(NULL, NULL, "Re-Connecting to event logger after delay of 1sec. Remaining attempts = %d, error = %s", attempt, err_msg);
      sleep(1); 
    }
    else
      break;
  } while(attempt > 0);

  /* Anil - we need to set it to no-blocking */
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
  {
    NSTL1(NULL, NULL, "Failed to set fd = %d to non-blocking", fd);
    return -1;
  }

  Msg_com_con *mccptr = ((!con_mode)?&g_el_subproc_msg_com_con:&g_dh_el_subproc_msg_com_con);
  NSTL1(NULL, NULL, "Connected to event logger at ip address %s and port %d. Connection = %s", ip, port,
                    nslb_get_src_addr(fd));
  init_msg_con_struct(mccptr, fd, CONNECTION_TYPE_OTHER, ip, NS_STRUCT_TYPE_LOG_MGR_COM);
  mccptr->state |= NS_CONNECTED;

  return fd;
}

inline int connect_to_event_logger(char *ip, unsigned short port) {
  NSDL1_MESSAGES(NULL, NULL, "Method called, ip = %s:%hu", ip, port);
  return connect_to_event_logger_ex(ip, port, 0);
}
/*=================================================================
send_child_registration function is used to send registration msg for control and data connection.
The idea of using this function is to confirm data and control connection on basis of gen_id, 
previously we were confirming connection on basis of gen_IP

In this msg we are sending 
1. opcode(CHILD_REGISTRATION),
2. gen_id, 
3. loc_id and 
4. gen_token(MD5 checksum)


Algorithum of function is 

1. fill reristration msg (child_reg) with the values mentioned above.
2. set timeout for sending msg and receiving acknowledgement.
3.   while timeout
3.1  first send msg and then wait for reading acknowledgement so connection get confirmed. 

==================================================================*/
static void send_child_registration(int fd, unsigned char *gen_name, int loc_idx, int gen_id, unsigned char *token)
{
  ChildRegistration child_reg;
  NSDL3_MESSAGES(NULL, NULL, "Method called = %d", fd);

  //filling structure for sending msg.
  child_reg.opcode = CHILD_REGISTRATION;
  child_reg.gen_id = gen_id;
  child_reg.gen_loc_idx = loc_idx;
  strcpy(child_reg.token,(char*)token);
  child_reg.msg_len = sizeof(ChildRegistration) - sizeof(int);
  
  NSTL1(NULL, NULL, "Sending child registration. fd = %d, gen_name = %s, gen_opcode = %d, loc_idx = %d, gen_id = %d, token = %s",
                     fd, gen_name, CHILD_REGISTRATION,loc_idx, gen_id, token);

  //set timeout for sending msg and receive ack for msg.
  int timeout = global_settings->parent_child_con_timeout * 2;
  //flag for msg send done.
  int write_done = 0;
  // times for start and finish send msg or receive ack
  time_t epoll_start_time, epoll_end_time;

  //run untill timout expires
  while(timeout > 0)
  {
    epoll_start_time = time(NULL);                                                             // start send time or start receive time
    if((!write_done) && (write(fd, (char *)&child_reg, sizeof(ChildRegistration)) < 0))        // sending registration msg.
    {
      if(errno == EINTR)                                                                       //until EINTR comes, reduce
      {                                                                                        // time and continue for write
        epoll_end_time = time(NULL);
        timeout -= epoll_end_time - epoll_start_time;
        continue;
      }
      // if error is not EINTR then fail to send msg.
      NSDL1_MESSAGES(NULL, NULL, "connect_to_master: failed to send registration message to controller error = %s\n", nslb_strerror(errno));
      NS_EXIT(-1, "connect_to_master: failed to send registration message to controller. fd = %d, error = %s\n", fd, nslb_strerror(errno));
    }
    else if(read(fd, (char *)&child_reg, sizeof(ChildRegistration)) <= 0)                    // once send is done wait for receive ack
    {
      write_done =1;                                                                        // mark write flag so will not go again for send 
      if(errno == EINTR)                                                                    // until EINTR comes, reduce
      {                                                                                     // time and continue for read
        epoll_end_time = time(NULL);      
        timeout -= epoll_end_time - epoll_start_time;
        continue;
      }
      // if error is not EINTR then fail to read ack.
      NSDL1_MESSAGES(NULL, NULL, "connect_to_master: failed to send registration message to controller error = %s\n", nslb_strerror(errno));
      NS_EXIT(-1, "connect_to_master: failed to send registration message to controller. fd = %d, error = %s\n", fd, nslb_strerror(errno));
    }
    else
    {
      //if control comes here means send registration is successful
      NSTL1(NULL, NULL, "Child registration send successful.");
      break;
    }
  }
}

// This is used by child to connect to parent
int connect_to_master()
{
  int fd;
  int attempt = NC_MAX_RETRY_ON_CONN_FAIL + 1;
  char first_time_tcpdump = 1;
  char err_msg[1024]="\0";
  char path_buf [MAX_VAR_SIZE + 1];
  path_buf[0] = '\0';
  NCTcpDumpSettings *nc_tcpdump_settings = global_settings->nc_tcpdump_settings;
  int conn_port;
  int mode;
  Msg_com_con *msg_com_con_ptr = NULL;
  if(ISCALLER_DATA_HANDLER)
  {
    MY_MALLOC(g_dh_master_msg_com_con, sizeof(Msg_com_con), "g_dh_master_msg_com_con", -1);
    conn_port = dh_master_port;
    msg_com_con_ptr = g_dh_master_msg_com_con;
    msg_com_con_ptr->fd = -1;
    mode = DATA_MODE;
    sprintf(msg_com_con_ptr->conn_owner, "GENERATOR_TO_CONTROLLER_DATA");
  }
  else
  {
    MY_MALLOC(g_master_msg_com_con, sizeof(Msg_com_con), "g_master_msg_com_con", -1);
    conn_port = master_port;
    msg_com_con_ptr = g_master_msg_com_con;
    msg_com_con_ptr->fd = -1;
    mode = CONTROL_MODE;
    sprintf(msg_com_con_ptr->conn_owner, "GENERATOR_TO_CONTROLLER");
  }
  NSDL1_MESSAGES(NULL, NULL, "Method called. Master IP = %s, port = %d", master_ip, conn_port);
  snprintf(path_buf, MAX_VAR_SIZE, "%s/logs/TR%d/ns_logs/tcpdump", g_ns_wdir, testidx);

  // If ENABLE_NC_TCPDUMP, gen_mode val is 1
  if(IS_LAST_TCPDUMP_DUR_ENDED && IS_ENABLE_NC_TCPDUMP(ALWAYS))
  {
    NSTL1(NULL, NULL, "Forever taking TCPDUMP on Generator at path %s", path_buf);
    g_tcpdump_started_time = get_ms_stamp();
    nslb_start_tcp_dump(master_ip, conn_port, nc_tcpdump_settings->tcpdump_duration, path_buf);
  }

  do {
    if ((fd = nslb_tcp_client_ex2(master_ip, conn_port, 30, global_settings->event_generating_ip , err_msg)) < 0)
    {
      // If ENABLE_NC_TCPDUMP gen_mode val is 2
      // Only first time, take tcpdump
      // On Conn failure retry five time 
      // On success, break the loop
      if(IS_ENABLE_NC_TCPDUMP(CONFAIL))
      {
        if(first_time_tcpdump)
        {
          NSTL1(NULL, NULL, "Started TCPDUMP on Generator for connection failure");
          NSTL1(NULL, NULL, "Path for tcp file %s", path_buf);
          g_tcpdump_started_time =  get_ms_stamp();
          nslb_start_tcp_dump(master_ip, conn_port, nc_tcpdump_settings->tcpdump_duration, path_buf);
          NSDL1_MESSAGES(NULL, NULL, "Connection failed, on retry, connecting for first time");
          NSTL1(NULL, NULL, "Connection failed, on retry, connecting for first time");
          first_time_tcpdump = 0;
        }
      }
      attempt--;   
      if (attempt == 0) //On last attempt, return fd
        return fd; /* error handled at the caller side */
      NSDL1_MESSAGES(NULL, NULL, "Error in making connection with controller, controller ip = %s, controller port = %d", 
                                    master_ip, conn_port);
      NSTL1(NULL, NULL, "Re-Connecting to master after delay of 1sec. Remaining attempts = %d, error = %s", attempt, err_msg);
      sleep(1);   //One second sleep before retry
    }
    else
      break;
  } while(attempt > 0);

  /*Make Token*/
  unsigned char token[2*MD5_DIGEST_LENGTH +1];
  nslb_md5(global_settings->event_generating_host, token);
  send_child_registration(fd, global_settings->event_generating_host, global_settings->loc_id, global_settings->gen_id, token);

  //This fd should be non blocking afer sending child registration because child reg is sent in blocking
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
    NSTL1_OUT(NULL, NULL, "connect_to_master: fnctl failed\n");
    return -1;
  }

  /* add to epoll */
  NSTL1(NULL, NULL, "Connected to master at ip address %s and port %d. Connection = %s", master_ip, conn_port,
                    nslb_get_src_addr(fd));
  init_msg_con_struct(msg_com_con_ptr, fd, CONNECTION_TYPE_CHILD_OR_CLIENT, master_ip, NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM);
  ADD_SELECT_MSG_COM_CON((char *)(msg_com_con_ptr), fd, EPOLLIN | EPOLLERR | EPOLLHUP, mode);
  return fd;
}

/** 
 * This function call by both parent and master
 */
//backlog should be passed on argument
int
init_parent_listner_socket_new_v2(int *lfd, unsigned short port, int flag)
{
  char err_msg[1024];
  int backlog = PARENT_OR_THREAD_BACKLOG; 

  NSDL1_MESSAGES(NULL, NULL, "Method called. for port = %d", port);

  /*Changed backlog from 200 to 10000. This is done bcoz in controller mode we connect
 * Conntroller's event logger with every NVM of generators. In NetCloud we wioll be using 100 generators and evry genrator will have 48 NVMs so total connections will be:
 *    (48 * 100) + 100(controller parent) = 4900. So makiking it 10K*/
  NSTL1(NULL, NULL, "Going to open listening socket on port = %hu", port);
  
  if (flag == 1)  // flag will set in case of event log then backlog will be 10000 
    backlog = EVENT_LOG_BACKLOG;

  if ((*lfd = nslb_tcp_listen(port, backlog, err_msg)) < 0)
  {
    NSTL1_OUT(NULL, NULL, "netstorm:  Error in creating Parent/Controller TCP listen socket.  Going to try second time to create the socket.\n");
    perror("Error:");
    sleep(4);
    if ((*lfd = nslb_tcp_listen(port, backlog, err_msg)) < 0)
    {
      perror("netstorm:  Error in creating Parent TCP listen socket. It will try for next port if configured.");
      return 0;
    }
  }

  if (!port) {
    // Get the parent port allocated by the system
    struct sockaddr_in sockname;
    socklen_t socksize = sizeof (sockname);
    if (getsockname(*lfd, (struct sockaddr *)&sockname, &socksize) < 0)
      {
        NSTL1(NULL, NULL, "getsockname error ");
        perror("getsockname error ");
        NS_EXIT(-1, CAV_ERR_1000036, errno, nslb_strerror(errno));
      }
    port = ntohs (sockname.sin_port);
    NSDL2_MESSAGES(NULL, NULL, "Parent port = %d", port);
  }
  return port;
}

int
init_parent_listner_socket_new(int *lfd, unsigned short port)
{
  int flag = 0;
  return init_parent_listner_socket_new_v2(lfd, port, flag);
} 
/*Added connection type argument in function*/
//TODO:Remove type argument as its not used 
extern int total_pdf_msg_size;
static int mccptr_buf_size;

void check_if_need_to_realloc_connection_read_buf (Msg_com_con *mccptr, int nvm_id, int old_avg_size, int type) 
{
   int new_size = 0;
 
   NSDL2_MESSAGES(NULL, NULL, "Method Called, dynamic_feature_name = %s, mccptr = %p, mccptr_buf_size = %d, "
                              "g_avgtime_size = %d, nvm_id = %d, old_avg_size = %d, mccptr->read_buf_size = %d, "
                              "total_pdf_msg_size = %d", 
                               dynamic_feature_name[type], mccptr, mccptr_buf_size, g_avgtime_size, 
                               nvm_id, old_avg_size, mccptr->read_buf_size, total_pdf_msg_size);
  
  new_size = g_avgtime_size > (total_pdf_msg_size + sizeof(PercentileMsgHdr))?
                        g_avgtime_size:(total_pdf_msg_size + sizeof(PercentileMsgHdr));
 
  if(mccptr->read_buf_size < new_size)
  {
    //Here mccptr will be 
    //NS: Parent and NVM 
    //NC: Controller->gen Parent, Gen Parent->Controller parent, NVM->GenParent
    mccptr_buf_size = new_size;
    MY_REALLOC_EX(mccptr->read_buf, mccptr_buf_size, old_avg_size, "mccptr->read_buf", -1);
    mccptr->read_buf_size = mccptr_buf_size;
    NSTL1(NULL, NULL, "Allocate read_buf on NS/Master: new mccptr_buf_size = %d, old size = %d", mccptr_buf_size, old_avg_size);
 
    //If generator parent then need to allocate the buffers for NVM connection also
    if(loader_opcode == CLIENT_LOADER)
    {
      NSTL1(NULL, NULL, "Allocate read_buf on generator: new mccptr_buf_size = %d, old size = %d", mccptr_buf_size, old_avg_size); 
      //Here g_dh_msg_com_con is NVM->gen Parent
      MY_REALLOC_EX(g_dh_msg_com_con[nvm_id].read_buf, mccptr_buf_size, old_avg_size, "mccptr->read_buf", -1);
      g_dh_msg_com_con[nvm_id].read_buf_size = mccptr_buf_size;
    }
  }
}

void init_msg_con_struct(Msg_com_con *mccptr, int fd, signed char type, char *ip, char conn_type)
{
  /*   char *ip; */ /* IP is not coming correct - need to fix later */
  /*   ip = nslb_get_src_addr(fd); */

  mccptr_buf_size = g_avgtime_size > (total_pdf_msg_size + sizeof(PercentileMsgHdr))?g_avgtime_size: (total_pdf_msg_size + sizeof(PercentileMsgHdr));

  NSDL1_MESSAGES(NULL, NULL, "Method called, FD = %d, type = %d, ip = %s, g_avgtime_size = %d, "
                             "total_pdf_msg_size = %d, mccptr_buf_size = %d", 
                              fd, type, ip, g_avgtime_size, total_pdf_msg_size, mccptr_buf_size);
  memset(mccptr, 0, sizeof(Msg_com_con));
  /*Set connection type*/
  mccptr->con_type = conn_type;
  mccptr->fd = fd;
  mccptr->type = type;
  mccptr->overflow_count = 1;
  mccptr->nvm_index = -1;
  
  MY_MALLOC(mccptr->ip, strlen(ip) + 1, "mccptr->ip", -1);
  strcpy(mccptr->ip, ip);

  if(((mccptr->con_type == NS_NDC_DATA_CONN) || (mccptr->con_type == NS_LPS_TYPE)) && (is_outbound_connection_enabled))
  {
    MY_MALLOC(mccptr->read_buf, MAX_BUFFER_SIZE_FOR_MONITOR + 1, "Initial memory allocation of read buffer", -1);
    mccptr->read_buf_size = MAX_BUFFER_SIZE_FOR_MONITOR;
    MY_MALLOC(mccptr->write_buf, MAX_BUFFER_SIZE_FOR_MONITOR + 1, "Initial memory allocation of write buffer", -1);
    mccptr->write_buf_size = MAX_BUFFER_SIZE_FOR_MONITOR;
  }
  else
  {
    MY_MALLOC(mccptr->read_buf, mccptr_buf_size, "mccptr->read_buf", -1);
    mccptr->read_buf_size = mccptr_buf_size;
    int sz = mccptr_buf_size + (sizeof(parent_child) * 2);
    NSTL1(NULL, NULL, "malloc write buf at first time with size = %d", sz);
    MY_MALLOC(mccptr->write_buf, mccptr_buf_size + (sizeof(parent_child) * 2), "mccptr->write_buf", -1);
    mccptr->write_buf_size = mccptr_buf_size + (sizeof(parent_child) * 2);
  }

  /* Set flags to call kill_all_children function and terminate test with respect to epoll error or 
   * event logger gets epoll errors
   * 
   * 1) In case of epoll error occuring on processes
   * 
   * NVM:
   * In case of NVM we set flags to not call kill_all_children or exit if event or error occurs for NVM
   * 
   * NS-parent/Controller/Generator-parent:
   * NS-parent:        In standalone test must call kill_all_children and exit 
   *                   if error occur hence SHOULD not set flags.
   *
   * Generator-parent: In case of generator if parent dies then children must die, 
   *                   so in CLIENT_LOADER and standalone case we will not set these bits.
   *
   * Controller:       Setting bits here for only MASTER as we dont want to kill genartoars 
   *                   if CONTINUE_ON_GEN_FAIL is enable
   *
   *************************************************************************************************  
   * 
   * 2) In case of event logger, 
   * 
   * NVM:
   * Since it is NVM, we need to set do not call kill all childern
   * If error on connection with event logger, we need to exit.  
   *
   * NS-parent/Controller/Generator-parent: 
   * If master or client or parent is getting error on event log connection, 
   * we need to kill all child and exit, So do not get any flags
   *************************************************************************************************
   * 
   * 3) In case of tool we are setting flags, currently tools are connected through parent only later 
   * we can support tools which can connect through NVMs.   
   * */
   /*In case of NDC we do not call kill_all_children or exit if event or error occurs*/
   if ((mccptr->con_type == NS_LPS_TYPE) || (mccptr->con_type == NS_STRUCT_TYPE_TOOL) || (mccptr->con_type == NS_STRUCT_TYPE_LOG_MGR_COM) || (mccptr->con_type == NS_NDC_TYPE) || (mccptr->con_type == NS_NDC_DATA_CONN) || (mccptr->con_type == NS_LPS_TYPE)) //set for tools and log mgr

   {
     mccptr->flags |= (NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN + NS_MSG_COM_DO_NOT_CALL_EXIT);
   }
   else if (my_port_index != 255) //NVM
   {
      mccptr->flags |= NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN;
   } 
   else //Controller/NS-parent/Generator-parent/NVMs
   {
     if (((global_settings->nvm_fail_continue == 1) && ((mccptr->con_type == NS_STRUCT_TYPE_NVM_PARENT_DATA_COM) || 
      (mccptr->con_type == NS_STRUCT_TYPE_NVM_PARENT_COM))) ||
       ((loader_opcode == MASTER_LOADER) && (global_settings->con_test_gen_fail_setting.mode == 1) && (mccptr->con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM)))
       mccptr->flags |= (NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN + NS_MSG_COM_DO_NOT_CALL_EXIT);
   }

}

//Accepting connection from tools on data thread.
void accept_connection_from_tools_dh()
{
  socklen_t len = sizeof(struct sockaddr_in);
  struct sockaddr_in their_addr;
  int fd;
  int num_tool_epfd, count, timeout;
  Msg_com_con *tool_msg_com_con;
  struct epoll_event *pfds = NULL;
  struct epoll_event data_th_pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called.");
  NSDL2_MESSAGES(NULL, NULL, "Waiting for connections from tool");

  MY_MALLOC(tool_msg_com_con, sizeof (Msg_com_con), "tool_msg_com_con", -1);
 
  if ((num_tool_epfd = epoll_create(1)) == -1)
  {
    NSTL1(NULL, NULL, "Error in creating epoll for tool connection. err = %s\n", nslb_strerror(errno));
    NS_EXIT(-1, "Error in creating epoll for tool connection. err = %s", nslb_strerror(errno));
  }

  // Allocate for num_connections events
  MY_MALLOC(pfds, sizeof(struct epoll_event), "epoll event", -1);

  data_th_pfd.events = EPOLLIN;
  data_th_pfd.data.fd = data_handler_listen_fd;

  if (epoll_ctl(num_tool_epfd, EPOLL_CTL_ADD, data_handler_listen_fd, &data_th_pfd) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Could not add parent fd in EPOLL. err = %s\n", nslb_strerror(errno));
    if(close(num_tool_epfd) < 0)
    {
      NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
      return;
    }
    FREE_AND_MAKE_NULL(pfds, "pfds", -1);
    return;
  }

  timeout = TIMEOUT_FOR_ACCEPT_FROM_TOOLS;

  count = epoll_wait(num_tool_epfd, pfds, 1, timeout);
  
  if(count < 0)
  {
    NSTL1(NULL, NULL, "Error in epoll for tools. error = %s", nslb_strerror(errno));
    if(close(num_tool_epfd) < 0)
    {
      NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
      return;
    }
    FREE_AND_MAKE_NULL(pfds, "pfds", -1);

    return;
  }
  else if(count > 0)
  {
    fd = accept(data_handler_listen_fd, (struct sockaddr *)&their_addr, (socklen_t *)&len);
    if(fd < 0)
    {
      NSTL1_OUT(NULL, NULL, "FATAL ERROR: Tool request can not be processed due to error in accept. err = %s\n", nslb_strerror(errno));
      return;
    }
  }
  else
  {
    NSTL1_OUT(NULL, NULL, "timeout(%d millisec) while waiting for connections from tool\n", timeout);
    return;
  }
 
  NSTL1(NULL, NULL, "Connection accepted from tool IP = %s, fd = %d",
            nslb_sock_ntop((const struct sockaddr *)&their_addr), fd); // log IP

  if(close(num_tool_epfd) < 0)
  {
     NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
     return;
  }

  FREE_AND_MAKE_NULL(pfds, "pfds", -1);

  fcntl(fd, F_SETFL, O_NONBLOCK);
  
  init_msg_con_struct(tool_msg_com_con, fd, CONNECTION_TYPE_OTHER, nslb_sock_ntop((const struct sockaddr *)&their_addr), NS_STRUCT_TYPE_TOOL);
  ADD_SELECT_MSG_COM_CON((char *)tool_msg_com_con, fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
  sprintf(tool_msg_com_con->conn_owner, "DATA_HANDLER_TO_TOOL");
}

void accept_connection_from_child()
{
  NSDL1_MESSAGES(NULL, NULL, "Method called. con_type=%d", NS_STRUCT_TYPE_CHILD);
  accept_connection_v1(NS_STRUCT_TYPE_CHILD);
}

void accept_connection_from_tools()
{
  NSDL1_MESSAGES(NULL, NULL, "Method called. con_type=%d", NS_STRUCT_TYPE_TOOL);
  accept_connection_v1(NS_STRUCT_TYPE_TOOL);
}
/*bug 92660: added argument conn_type, to differenciate b/w event from CM child or tool*/
void accept_connection_v1(int conn_type)
{
  socklen_t len = sizeof(struct sockaddr_in);
  struct sockaddr_in their_addr;
  int fd;
  int num_tool_epfd, count, timeout;
  Msg_com_con *tool_msg_com_con;
  struct epoll_event *pfds = NULL;
  struct epoll_event parent_pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called.");
  NSDL2_MESSAGES(NULL, NULL, "Waiting for connections from tool");

  MY_MALLOC(tool_msg_com_con, sizeof (Msg_com_con), "tool_msg_com_con", -1);
 
  if ((num_tool_epfd = epoll_create(1)) == -1)
  {
    NSTL1(NULL, NULL, "Error in creating epoll for tool connection. err = %s\n", nslb_strerror(errno));
    NS_EXIT(-1, "Error in creating epoll for tool connection. err = %s", nslb_strerror(errno));
  }

  NSDL1_MESSAGES(NULL, NULL, "epoll_create done");
  // Allocate for num_connections events
  MY_MALLOC(pfds, sizeof(struct epoll_event), "epoll event", -1);

  int loc_listen_fd;
  /*bug 92660: check if connection is from CM Child*/
  if(NS_STRUCT_TYPE_CHILD == conn_type)
    loc_listen_fd = parent_listen_fd;
  else
   loc_listen_fd = listen_fd;

  parent_pfd.events = EPOLLIN;
  parent_pfd.data.fd = loc_listen_fd;

  if (epoll_ctl(num_tool_epfd, EPOLL_CTL_ADD, loc_listen_fd, &parent_pfd) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Could not add parent fd in EPOLL. err = %s\n", nslb_strerror(errno));
    if(close(num_tool_epfd) < 0)
    {
      NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
      return;
    }
    FREE_AND_MAKE_NULL(pfds, "pfds", -1);
    return;
  }

  NSDL1_MESSAGES(NULL, NULL, "epoll_ctl done. loc_listen_fd=%d", loc_listen_fd);
  timeout = TIMEOUT_FOR_ACCEPT_FROM_TOOLS;

  count = epoll_wait(num_tool_epfd, pfds, 1, timeout);
  
  NSDL1_MESSAGES(NULL, NULL, "count=%d", count);
  if(count < 0)
  {
    NSTL1(NULL, NULL, "Error in epoll for tools. error = %s", nslb_strerror(errno));
    if(close(num_tool_epfd) < 0)
    {
      NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
      return;
    }
    FREE_AND_MAKE_NULL(pfds, "pfds", -1);

    return;
  }
  else if(count > 0)
  {
    fd = accept(loc_listen_fd, (struct sockaddr *)&their_addr, (socklen_t *)&len);
    if(fd < 0)
    {
      NSTL1_OUT(NULL, NULL, "FATAL ERROR: Tool request can not be processed due to error in accept. err = %s\n", nslb_strerror(errno));
      return;
    }
  }
  else
  {
    NSDL1_MESSAGES(NULL, NULL, "timeout(%d millisec) while waiting for connections from tool\n", timeout);
    NSTL1_OUT(NULL, NULL, "timeout(%d millisec) while waiting for connections from tool\n", timeout);
    return;
  }
 
  NSTL1(NULL, NULL, "Connection accepted from tool IP = %s, fd = %d",
            nslb_sock_ntop((const struct sockaddr *)&their_addr), fd); // log IP
  NSDL1_MESSAGES(NULL, NULL, "Connection accepted from tool IP = %s, fd = %d", nslb_sock_ntop((const struct sockaddr *)&their_addr), fd);
  if(close(num_tool_epfd) < 0)
  {
     NSTL1_OUT(NULL, NULL, "Unable to close epoll_fd with err = %s\n", nslb_strerror(errno));
     return;
  }

  FREE_AND_MAKE_NULL(pfds, "pfds", -1);

  fcntl(fd, F_SETFL, O_NONBLOCK);
  
  init_msg_con_struct(tool_msg_com_con, fd, CONNECTION_TYPE_OTHER, nslb_sock_ntop((const struct sockaddr *)&their_addr), conn_type);
  ADD_SELECT_MSG_COM_CON((char *)tool_msg_com_con, fd, EPOLLIN | EPOLLERR | EPOLLHUP, CONTROL_MODE);
  sprintf(tool_msg_com_con->conn_owner, "PARENT_TO_TOOL");
}

void
free_msg_com_con(Msg_com_con *ptr, int num)
{
  int i;
  Msg_com_con *mccptr = ptr;
  
  if (!ptr) return;

  for (i = 0; i < num; i++, ptr++) {
    FREE_AND_MAKE_NULL_EX(ptr->read_buf, ptr_buf_size, "mccptr->read_buf", i);
    FREE_AND_MAKE_NULL_EX(ptr->write_buf, ptr_buf_size + (sizeof(parent_child) * 2), "mccptr->write_buf", i);
    FREE_AND_MAKE_NULL_EX(ptr->ip,  strlen(ptr->ip) + 1, "mccptr->ip", i);
  }
    FREE_AND_MAKE_NULL_EX(mccptr, sizeof (Msg_com_con) * num, "mccptr", -1);
}


// this MACRO is used to set mode_str for contol and data connection.
#define SET_DATA_CONTROL_STR(mode) \
  char mode_str[16];               \
  (mode == DATA_MODE)?strcpy(mode_str,"data"):strcpy(mode_str, "control");

/*===========================================================
this function will fetch the gen_id from received msg to confirm control/data connection is coming from one of geneartor 

1. first check if gen_id is between 0 and used geneartor count else return -1.
2. check if msg->opcode is match for child_registration else return -1.
3. check if msg->token match with gen_id token in generator_entry table else return -1.
4. if all check pass then return gen_id 
=============================================================*/
int process_child_registration(Msg_com_con *mccptr, ChildRegistration *msg, int mode)
{
  // set mode string for control or data connection
  SET_DATA_CONTROL_STR(mode)

  NSDL1_MESSAGES(NULL, NULL, "Method Called, mccptr = %p",mccptr);

  // check if gen_id is between 0 and used geneartor count
  if((msg->gen_id < 0) || (msg->gen_id >= sgrp_used_genrator_entries)){
    NSTL1_OUT(NULL, NULL, "Returning generator index -1 because of received generator index = %d is out of used"
                          " generator range %d for %s connection.",
                          msg->gen_id, sgrp_used_genrator_entries, mode_str);
    return -1;
  }

  // check msg->opcode is match for child_registration
  if(CHILD_REGISTRATION != msg->opcode){
     NSTL1_OUT(NULL, NULL, "Returning generator index -1 because of opcode mismatched, opcode = %d from gen = %s , for %s connection.",
                            msg->opcode, msg_com_con_to_str(mccptr), mode_str);
     return -1;
  }
  
  // check msg->token match with gen_id token in generator_entry table
  if(!strcmp(msg->token, (char *)generator_entry[msg->gen_id].token))
  {
    NSDL1_MESSAGES(NULL, NULL, "Generator token match returning index = %d, token = %s from gen= %s , for %s connection.\n",
                  msg->gen_id, (char *)generator_entry[msg->gen_id].token, msg_com_con_to_str(mccptr), mode_str);
    // return gen_id if all check pass 
    return msg->gen_id;
  }
  NSTL1_OUT(NULL, NULL, "Returning generator index -1 because of generator token mismatched, expected geneartor token = %s,"
             " received geneartor token = %s, received generator index = %d, generator fd = %d for %s connection.\n",
             (char *)generator_entry[msg->gen_id].token, msg->token, msg->gen_id, mccptr->fd, mode_str);
  // fail status if opcode match but token does not match
  return -1;
}

/*=================================================================
This method will be used for reading the received msg fro the event coming for data/control connection from generator

return following
   0 if msg is partially recieved
   1 if gen reg is successful
  -1 gen is already killed
  -2 reg msg is invalid 
==================================================================*/

int read_child_reg_msg(Msg_com_con *mccptr, const struct sockaddr *their_addr, int mode, int num_nvm_epfd)
{
  // set mode string for control or data connection
  SET_DATA_CONTROL_STR(mode)

  NSDL1_MESSAGES(NULL, NULL, "Method Called, mccptr = %p, for %s connection.",mccptr, mode_str);

  int msg_size=0;
  int gen_id;
  ChildRegistration *msg;
  Msg_com_con *child_mccptr;

  // read received message mccptr and assign to msg
  msg = (ChildRegistration *)read_msg(mccptr, &msg_size, mode);
  
  if(msg == NULL)
    return 0;

  /* NC: In case of adding generators in message communication table, we need to add generator index
  * as per generator table, therefore we need to match gen_id and update index with same*/
  if((gen_id = process_child_registration(mccptr, msg, mode)) < 0)
  {
    //generator index not found.
    return -2; 
  }

  NSDL2_MESSAGES(NULL, NULL, "Generator ip match,hence add generator id = %d for %s connection", gen_id, mode_str);
  
  if(mode == DATA_MODE)
    child_mccptr = &g_dh_msg_com_con[gen_id];
  else
    child_mccptr = &g_msg_com_con[gen_id];
   
  /*If connection came from generator ,what is already killed as failed to start test,then close its fd */
  if(generator_entry[gen_id].flags & IS_GEN_KILLED)
  {
    child_mccptr->flags |= NS_MSG_COM_CON_IS_CLOSED;
    NSTL1_OUT(NULL, NULL, "Received child registration msg which is already killed. fd = %d, gen id = %d,"
                          " for gen %s for %s connection.",
                          mccptr->fd, gen_id, msg_com_con_to_str(mccptr), mode_str);
    return -1;
  }
  // if CONTROL_MODE then set generator flag to ACTIVE
  if(mode == CONTROL_MODE)
    generator_entry[gen_id].flags |= IS_GEN_ACTIVE;
    
  NSTL1( NULL, NULL, "Received success child registration message. So add connection fd = %d, into g%s_msg_com_con"
                     " at index = %d, for gen %s for %s connection.",
                     mccptr->fd, (mode == DATA_MODE)?"_dh":"" ,gen_id,  msg_com_con_to_str(mccptr), mode_str);

  init_msg_con_struct(child_mccptr, mccptr->fd, CONNECTION_TYPE_CHILD_OR_CLIENT, nslb_sock_ntop((const struct sockaddr *)their_addr), NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM);
  sprintf(child_mccptr->conn_owner, "[%s] CONTROLLER_TO_GENERATOR", mode_str);
  child_mccptr->nvm_index = gen_id;
  del_conn_for_epoll(mccptr->fd, num_nvm_epfd, mode);
  ADD_SELECT_MSG_COM_CON((char *)child_mccptr, mccptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP, mode);

  // returning ack to generator 
  write_msg(mccptr, (char *)&msg, sizeof(ChildRegistration), 0 , mode);
  
  return 1;
}

void add_conn_for_epoll(int fd, Msg_com_con *mccptr, int num_nvm_epfd, struct epoll_event *pfds, const struct sockaddr *their_addr, int mode)
{
  SET_DATA_CONTROL_STR(mode)

  MY_MALLOC(mccptr, sizeof (Msg_com_con), "registration mesage from generator", -1);
  
  init_msg_con_struct(mccptr, fd, CONNECTION_TYPE_CHILD_OR_CLIENT, nslb_sock_ntop((const struct sockaddr *)their_addr), NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM);
 // Add fd for epoll wait()
  pfds->events = EPOLLIN | EPOLLERR | EPOLLHUP;
  pfds->data.ptr = mccptr;

  if(epoll_ctl(num_nvm_epfd, EPOLL_CTL_ADD, fd, pfds) == -1) 
  {
      NS_EXIT(-1, "Could not add connection fd = %d  in EPOLL for %s connection. err = %s", fd, mode_str, nslb_strerror(errno)); 
   // Also add connection string
  }
}

void del_conn_for_epoll(int fd, int num_nvm_epfd, int mode)
{
  SET_DATA_CONTROL_STR(mode)
  struct epoll_event pfd;

  bzero(&pfd, sizeof(struct epoll_event));

  if(epoll_ctl(num_nvm_epfd, EPOLL_CTL_DEL, fd, &pfd) == -1)
  {
      NSTL1(NULL, NULL, "Could not delete connection fd = %d  in EPOLL for %s connection. err = %s", fd, mode_str, nslb_strerror(errno));
   // Also add connection string
  }
}

/**
 * listen_fd: fd to accept on
 * num_connections: Num connections to accept
 *
 * NOTE: right now we block at accept.

   Used by parent and master
 */

/* Netomni changes:
 * 1) Terminology changes
 *    In MASTER-MODE we used terms master-client.
 *    In netomni we call master == CONTROLLER
 *                       client == GENERATOR
 * 2) Struct changes
 *    Earlier we were using client struct in current
 *    code generator_entry struct is used.
 * */

/*===================================================================================
This function is used to accept connection from generator to controller for control
and data connection.
=====================================================================================*/
void wait_for_child_registration_control_and_data_connection(int max_connections , int mode)
{
  int i, index = 0; //index must be initialized with 0
  socklen_t len = sizeof(struct sockaddr_in);
  struct sockaddr_in their_addr;
  int fd;
  int timeout;
  int num_nvm_epfd;
  u_ns_ts_t tu1 = get_ms_stamp();
  u_ns_ts_t tu2;
  struct epoll_event *pfds = NULL;
  struct epoll_event parent_pfd;
  int count;
  time_t epoll_start_time, epoll_end_time;
  char path_buf[MAX_VAR_SIZE + 1];
  path_buf[0] = '\0';
  char err_buf[MAX_VAR_SIZE + 1];
  int num_pending_registrations =0;
  Msg_com_con *mccptr;

  // local variables for use in according to Connection type.
  int parent_listen_fd;
  Msg_com_con *child_mccptr = NULL ;
  Msg_com_con *listen_msg_com_con_dh_or_parent;

  NSDL1_MESSAGES(NULL, NULL, "Method called, max_connection = %d, mode = %d", max_connections, mode);
  SET_DATA_CONTROL_STR(mode)

  int num_connections = max_connections;

  // allocating memory to child_mccptr for total num connections.
  MY_MALLOC_AND_MEMSET(child_mccptr, sizeof (Msg_com_con) * num_connections, mode_str , -1);

  //setting NVM/generator fd an index to -1, so if in case any one left due to timeout will be discarded.
  for(i = 0; i < num_connections; i++)
  {
    child_mccptr[i].fd = -1;
    child_mccptr[i].nvm_index = -1;
  }
 
  MY_MALLOC_AND_MEMSET(listen_msg_com_con_dh_or_parent, sizeof (Msg_com_con), "tool_msg_com_con", -1);

  // set listen_fd, num_msg_com_con and g_msg_com_con as per connection type(data/control)
  if(mode == DATA_MODE)
  {
    parent_listen_fd = data_handler_listen_fd;
  //  num_data_handler_msg_com_con = num_connections;
    g_dh_msg_com_con = child_mccptr;
    dh_listen_msg_com_con = listen_msg_com_con_dh_or_parent;
  }
  else
  {
    parent_listen_fd = listen_fd;
  //  num_msg_com_con = num_connections;
    g_msg_com_con = child_mccptr;
    parent_listen_msg_com_con = listen_msg_com_con_dh_or_parent;
  }

  listen_msg_com_con_dh_or_parent->con_type = NS_STRUCT_TYPE_LISTEN;
  listen_msg_com_con_dh_or_parent->fd = parent_listen_fd;

 // reducing all the killed generator as we will not get registeration request from these geneartors,
 // but in case if register request will be received. it will be ignored.
  num_connections -= g_data_control_var.total_killed_gen;

  //assigning  num_pending_registrations equal to num_connections.
  num_pending_registrations = num_connections;
  
  NSDL1_MESSAGES(NULL, NULL, "Number of child/generators = %d, g_avgtime_size = %d, Parent_fd = %d for %s connection",
                              num_connections, g_avgtime_size, parent_listen_fd, mode_str);

  // IN NC case.
  if (loader_opcode == MASTER_LOADER)
  {
    NSTL1(NULL, NULL, "Waiting for connections from %d generators for %s connection", num_connections, mode_str);
    NSTL1(NULL, NULL, "If controller_mode is enabled then take TCPDUmp for %s connection, here mode is %d",
                      mode_str, global_settings->nc_tcpdump_settings->cntrl_mode);
    //call tcpdump shell to check Controller stats of network
    //If ENABLE_NC_TCPDUMP is with controller_mode value 1 then take tcpdump
    if(IS_LAST_TCPDUMP_DUR_ENDED && IS_ENABLE_NC_TCPDUMP(ALWAYS))
    {
      snprintf(path_buf, MAX_VAR_SIZE, "%s/logs/TR%d/ns_logs/tcpdump", g_ns_wdir, testidx);
      NSTL1(NULL, NULL, "Taking TCPDUMP on Controller at path %s for %s connection", path_buf, mode_str);
      g_tcpdump_started_time =  get_ms_stamp();
      nslb_start_tcp_dump(g_cavinfo.NSAdminIP, parent_port_number, global_settings->nc_tcpdump_settings->tcpdump_duration, path_buf);
    }
  }
  else
  {
    NSTL1(NULL, NULL, "Waiting for %s connections from %d children", mode_str, num_connections);
  }

  NSDL2_MESSAGES(NULL, NULL, "Waiting for %s connections from %d child/generators", mode_str, num_connections);

  // creating epoll for parent/NVM
  if ((num_nvm_epfd = epoll_create(1)) == -1)
  {
    NSTL1(NULL, NULL, "Error in creating epoll for parent/NVM for %s connection. err = %s", nslb_strerror(errno), mode_str);
    NS_EXIT(-1, "Error in creating epoll for parent/NVM for %s connection. err = %s", nslb_strerror(errno), mode_str);
  }

  //Allocating memory for parent Epoll
  MY_MALLOC(pfds, sizeof(struct epoll_event) * num_connections, "epoll event", -1);

  parent_pfd.events = EPOLLIN;
  parent_pfd.data.fd = parent_listen_fd;

  //adding parent listen fd in epoll
  if (epoll_ctl(num_nvm_epfd, EPOLL_CTL_ADD, parent_listen_fd, &parent_pfd) == -1) 
  {
    NSTL1(NULL, NULL, "Could Not add parent fd in EPOLL for %s connection. err = %s", nslb_strerror(errno), mode_str);
    NS_EXIT(-1, "Could Not add parent fd in EPOLL for %s connection. err = %s", nslb_strerror(errno), mode_str);
  }

  //ContinueTestOnGenFailure setting.
  ContinueTestOnGenFailure *CTOGF_setting = &global_settings->con_test_gen_fail_setting;
  
  // over all timeout for accepting connection from all NVM/generator
  timeout = global_settings->parent_child_con_timeout;


  //run until all connections are accepted or timeout reach
  while((num_pending_registrations > 0)  && (timeout > 0))
  {
    //clear epoll for every connection 
    memset(pfds, 0, sizeof(struct epoll_event) * num_connections);
    
    // epoll with reduce time  
    epoll_start_time = time(NULL);
    //collect all events 
    count = epoll_wait(num_nvm_epfd, pfds, num_connections, timeout * 1000);
    epoll_end_time = time(NULL);
    timeout -= epoll_end_time - epoll_start_time;
   
    //if connection fails 
    if(count < 0)
    {
      if (errno == EINTR){
        NSTL1(NULL, NULL, "Epoll wait of NVM/GEN connection break due to interrupt for %s connection."
                          " Error = %s. Retrying for [%d]", nslb_strerror(errno), index, mode_str);
        continue;
      }
      else {
        kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
        NS_EXIT(-1, "NVM failed to make connection with parent. Error = %s.", nslb_strerror(errno));
      }
    }
    else if(count == 0)
    { /* no event received and epoll timeout*/
      
      NSTL1(NULL, NULL,"No event received,Continuing on epoll for timeout for %s connection", mode_str); 
      continue;
    }

    //processing the epoll event 
    int event_idx;
    for(event_idx = 0; event_idx < count; event_idx++)
    {
      // if received event fd is on listen fd for data/control connection
      if(pfds[event_idx].data.fd == parent_listen_fd)
      {
        //accept connection
        fd = nslb_accept(parent_listen_fd, (struct sockaddr *)&their_addr, (socklen_t *)&len, err_buf);
        // if fail in accept connection 
        if (fd < 0)
        {
          kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
          NS_EXIT(-1, "FATAL ERROR: TEST RUN CANCELLED due to error in accept for %s connection. %s\n", mode_str, err_buf);
        }
        write_log_file(NS_START_INST, "Accepted connection from IP = %s (%d out of %d) for %s connection",
                         nslb_sock_ntop((const struct sockaddr *)&their_addr), index+1, num_connections, mode_str);
        NSTL1(NULL, NULL, "Accepted connection from child/generator IP = %s, fd = %d for %s connection",
                         nslb_sock_ntop((const struct sockaddr *)&their_addr), fd, mode_str);
        NSDL2_MESSAGES(NULL, NULL, "Accepted connection from child/generator IP = %s, fd = %d for %s connection",
                         nslb_sock_ntop((const struct sockaddr *)&their_addr), fd, mode_str); // log IP
       
        //make connection non blocking 
        if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
        {
          NS_EXIT(-1, "FATAL ERROR: TEST RUN CANCELLED due to error in making conn fd non blocking fd = %d in %s connection, error = %s\n", fd, mode_str, nslb_strerror(errno));
        }
       
        // in case of NC we add connection in epoll for further processing 
        if (loader_opcode == MASTER_LOADER)
        {
          add_conn_for_epoll(fd, mccptr, num_nvm_epfd, pfds, (const struct sockaddr *)&their_addr, mode);
        }
        else //in case of netstorm we hadn't implemented child registration, so we assume on connection NVM has connected with us.
        {
          init_msg_con_struct(&child_mccptr[index], fd, CONNECTION_TYPE_CHILD_OR_CLIENT, nslb_sock_ntop((const struct sockaddr *)&their_addr), NS_STRUCT_TYPE_NVM_PARENT_COM);
          sprintf(child_mccptr[index].conn_owner, "[%s] PARENT_TO_NVM", mode_str);
          ADD_SELECT_MSG_COM_CON((char *)&child_mccptr[index], fd, EPOLLIN | EPOLLERR | EPOLLHUP, mode);
          num_pending_registrations--;
        }
        index++;
      }
      else //This is for child registration message from gen
      {
        if (loader_opcode == MASTER_LOADER)
        {
          int ret;
          mccptr = pfds[event_idx].data.ptr;
          //In all cases except partial msg we must do it
          if ((ret = read_child_reg_msg(mccptr, (const struct sockaddr *)&their_addr, mode, num_nvm_epfd)) < 0)
          {
            NSTL1_OUT(NULL, NULL, "child registration failed, ret = %d from child/generator IP = %s, fd = %d for %s connection ",
                     ret, nslb_sock_ntop((const struct sockaddr *)&their_addr), fd, mode_str);
        //    remove from epoll
            epoll_ctl(num_nvm_epfd, EPOLL_CTL_DEL, fd, pfds);
            close(mccptr->fd);
            FREE_AND_MAKE_NULL(mccptr, "mccptr", -1);
          }
          if(ret == 1) 
          { // successfull reg done
            num_pending_registrations--;
          }
        }
        else
        {
          NSTL1_OUT(NULL, NULL, "code should not come here.");
        }
      }
    }
  }

  // calculate total connected NVM/generator
  int num_connected;
  num_connected = num_connections - num_pending_registrations;
  NSTL1_OUT(NULL,NULL, "num_connected = %d, num_pending_registrations = %d, max_connections =%d, num_connections= %d",
                        num_connected, num_pending_registrations, max_connections, num_connections);
  //if connected is less then epected/total NVM/generator
  if(num_connected < max_connections)
  {
    // In  case of NVM 
    if (loader_opcode != MASTER_LOADER)
    {
      NSTL1_OUT(NULL, NULL,"Timeout(%d Sec) while waiting for %s connections from NVMs. Number of connections accepted so far = %d\n", timeout, mode_str, num_connected);

      if(mode == DATA_MODE) // if data connection is less than control connection we have to stop test.
        kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);

      NS_EXIT(-1, "kill all children");
    }
    else // IN case of geneartor
    { 
      NSTL1(NULL, NULL, "Timeout(%d Sec) while waiting for control connections from generator."
                        " Number of connections accepted so far = %d" , timeout, num_connected);
      //in case of contorl connection in NC
      if(mode == CONTROL_MODE)
      {
       //if continue test with 100% generator is enabled is scenario then kill test if any one gen fails otherwise continue
        if((CTOGF_setting->percent_started == 100) || (num_connected < num_gen_expected))
        {
          NS_EXIT(-1, "Failed to registered generators %d%% of generators in %d seconds for %s connection",
                       CTOGF_setting->percent_started, CTOGF_setting->start_timeout, mode_str);
        }
        NSTL1(NULL,NULL,"Continuing the test with %d generators.", num_connected);
      }
      else// in case of data connection in NC 
      {
        if(num_pending_registrations > 0)
        {
          kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
          NS_EXIT(-1, "kill all children");
        }
      }
    }
  }  
  // if any gen fail or killed then change num_connected geneartors  and killed gen in control mode only.
  if(mode == CONTROL_MODE)
  {
    g_data_control_var.num_connected = num_connected;
    g_data_control_var.total_killed_gen += num_pending_registrations;
  }
  //In case of one nvm (mostly in nde case) we are saving msg_com_con_ptr for nvm0 for reading in epoll wait
  if((loader_opcode == STAND_ALONE) && (index == 1))
  {
    if(mode == DATA_MODE)
      g_dh_msg_com_con_nvm0 = &child_mccptr[index - 1];
    else
      g_msg_com_con_nvm0 = &child_mccptr[index - 1];
    NSDL2_MESSAGES(NULL, NULL, "%s = %p", (mode == DATA_MODE)?"g_dh_msg_com_con_nvm0":"g_msg_com_con_nvm0" ,&child_mccptr[index - 1]);
  }

  if(close(num_nvm_epfd) < 0)
  {
    NSTL1(NULL, NULL, "Unable to close epoll_fd for %s connection with err = %s", mode_str, nslb_strerror(errno));
    NS_EXIT(-1, "Unable to close epoll_fd for %s connection with err = %s", mode_str, nslb_strerror(errno));
  }

  FREE_AND_MAKE_NULL(pfds, "pfds", -1);

  tu2 = get_ms_stamp();

  if (loader_opcode == MASTER_LOADER){
    NSTL1(NULL, NULL, "All connections from %d child/generators received in %llu msec for %s connection", g_data_control_var.num_connected, tu2 - tu1, mode_str);
  }

  ADD_SELECT_MSG_COM_CON((char*)listen_msg_com_con_dh_or_parent, parent_listen_fd, EPOLLIN, mode);
  
  NSDL2_MESSAGES(NULL, NULL, "All %s connections from %d child/generators received", mode_str, num_connections);
}
  
inline void add_select_msg_com_con_v2(char *file, int line, char * func, char* data_ptr, int fd, int event, int flag)
{
  struct epoll_event pfd;
  static int structure_dumped = 0;
  int epfd; 
  NSDL1_MESSAGES(NULL, NULL, "Method called. Adding fd = %d for event = %x "
                             "Caller = [%s]-[%d]-[%s]", fd, event, file, line, func);
  if(my_port_index != 255)
  {
    epfd = v_epoll_fd;
  }
  else
  {
    if(flag == DATA_MODE){
      NSTL2(NULL, NULL, "add_select_msg_com_con_v2 Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd);
      epfd = g_dh_msg_com_epfd;
    }
    else
      epfd = g_msg_com_epfd;
  }

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;

  
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    if((!g_progress_delay_read) && (structure_dumped == 0) && (my_port_index == 255))
    {
      NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured in parent/thread process, add_select_parent() - with fd %d "
                                 "EPOLL_CTL_ADD: err = %s", fd, nslb_strerror(errno));
      NSTL1_OUT(NULL, NULL, "EPOLL ERROR occured in parent/thread add_select_parent() - with fd %d "
                            "EPOLL_CTL_ADD: err = %s\n", fd, nslb_strerror(errno));
      dump_monitor_table();
      structure_dumped = 1;
    }
    if (my_port_index != 255){
      NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured in child process[%d], add_select_parent() - with fd %d "
                                 "EPOLL_CTL_ADD: err = %s", my_child_index, fd, nslb_strerror(errno));
      NS_EXIT(-1, "EPOLL ERROR occured in child process[%d], add_select_parent() - with fd %d "
                  "EPOLL_CTL_ADD: err = %s", my_child_index, fd, nslb_strerror(errno));
    } 
     
    return;
  }
  if(g_progress_delay_read)
  {
    NSTL1(NULL, NULL, "[GENERATOR_DELAY] EPOLL ADD Due to Progress Report Delay from Generator = '%d' with fd '%d'",
                       ((Msg_com_con *)data_ptr)->nvm_index, fd);

  }
}

inline void add_select_msg_com_con(char* data_ptr, int fd, int event)
{
  add_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, data_ptr, fd, event, CONTROL_MODE);
}

inline void remove_select_msg_com_con_v2(char *file, int line, char * func, int fd, int th_flag)
{
  struct epoll_event pfd;
  //static int structure_dumped = 0;

  int epfd;

  if(my_port_index != 255)
  { 
    epfd = v_epoll_fd; 
  }
  else
  {
    if(th_flag == DATA_MODE)
      epfd = g_dh_msg_com_epfd;
    else        // CONTROL_MODE
      epfd = g_msg_com_epfd;
  }

  NSDL1_MESSAGES(NULL, NULL, "Method called. Removing fd = %d from select, my_child_index = %d, "
                             "Caller = [%s]-[%d]-[%s], epfd = %d", fd, my_child_index, file, line, func, epfd);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  if (fd == -1) return;

  if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &pfd) == -1)
  {
    if(my_port_index != 255){
      NSTL1_OUT(NULL, NULL, "EPOLL ERROR occured in child process = '%d', remove_select_parent_v2() "
                            "Caller = [%s]-[%d]-[%s], -epfd %d with fd %d EPOLL_CTL_DEL: err = %s", 
                            my_child_index, file, line, func, epfd, fd, nslb_strerror(errno));
    }
    else{
      NSTL1_OUT(NULL, NULL, "EPOLL ERROR occured in parent process, remove_select_parent_v2() "
                            "Caller = [%s]-[%d]-[%s], -epfd %d with fd %d EPOLL_CTL_DEL: err = %s", 
                            file, line, func, epfd, fd, nslb_strerror(errno));
    } 
  }
}

inline void remove_select_msg_com_con(int fd)
{
  remove_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, fd, g_msg_com_epfd);
}

inline void mod_select_msg_com_con_v2(char *file, int line, char * func, char* data_ptr, int fd, int event, int th_flag) {
  struct epoll_event pfd;
  int structure_dumped = 0;
  int epfd;

  if(my_port_index != 255)
  { 
    epfd = v_epoll_fd; 
  }
  else
  {
    if(th_flag == DATA_MODE)
    {
      NSTL2(NULL, NULL, "mod_select_msg_com_con_v2 Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd); 
      epfd = g_dh_msg_com_epfd;
    }
    else
      epfd = g_msg_com_epfd;
  }

  NSDL3_MESSAGES(NULL, NULL, "Method called. Moding %d for event=%x, my_child_index = %d, epfd = %d "
                             "Caller = [%s]-[%d]-[%s]", fd, event, my_child_index, epfd, file, line, func);
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.ptr = (void*) data_ptr;
  if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &pfd) == -1) {
    NSDL3_MESSAGES(NULL, NULL, "Error occured in parent process on epfd %d with fd %d epoll mod: err = %s", epfd, fd, nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "\nError occured in parent process on epfd %d with fd %d epoll mod: err = %s\n", epfd, fd, nslb_strerror(errno));
    if(structure_dumped == 0)
    {
      dump_monitor_table();
      structure_dumped = 1;
    }
    return;
  }
}

inline void mod_select_msg_com_con(char* data_ptr, int fd, int event) {
  mod_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, data_ptr, fd, event, CONTROL_MODE);
}

void close_parent_listen_fd()
{
  /*bug 92660: add if() before closing fd*/
  if(listen_fd > 0)
    close(listen_fd);
  if(data_handler_listen_fd > 0)
    close(data_handler_listen_fd);
}

/*============================================================================ 
   Issue: 
     We are facing issue of Generator discard and according to our
     obervation it can be due to low bandwidth on single connection (~12Mbps)
     Since we are using same connection (DATA Connection) for Percentile as
     well as for Progress Report so it can be big enough and introduce
     a delay, due which Generator can discarded.

   Resolution:
     To overcome this issue we are decided to compress Controller and Generator 
     message

   Side-Effect:
     It can cause to increase CPU utilization and hence reduce performace also

   Check whether message is compressed OR not ? 
   IF compressed then decompressed it and return buffer with message header 
 ===========================================================================*/
static char *chk_and_decomp_read_msg(Msg_com_con *mccptr, int *size) 
{
  char err_msg[ERROR_MSG_SIZE + 1];
  int msg_hdr_len = sizeof(MsgHdr);
  parent_child *msg = (parent_child *)mccptr->read_buf;
   
  NSDL3_MESSAGES(NULL, NULL, "Method called. Opcode = %d, msg_flag = %d, msg_len = %d, size = %d", 
      msg->opcode, msg->msg_flag, msg->msg_len, *size);

  // Currently compression is supported for 2 opcodes and only for MASTER 
  if ((loader_opcode != MASTER_LOADER) || ((msg->opcode != PROGRESS_REPORT) && (msg->opcode != PERCENTILE_REPORT)))
    return mccptr->read_buf;

  //If compressed then decompress it
  if (msg->msg_flag & MSG_FLAG_COMPRESSED) 
  {
    long start_time_in_msecs = get_ms_stamp();
    ns_nvm_scratch_buf_len = NSLB_INIT_OUTPUT_SIZE;  //Input as Delta size, Output as decompressed size
    
    // 20MB
    if (nslb_decompress(((char *)msg + msg_hdr_len), (*size - msg_hdr_len), &ns_nvm_scratch_buf, (size_t *)&ns_nvm_scratch_buf_size, 
       (size_t *)&ns_nvm_scratch_buf_len, global_settings->data_comp_type, err_msg, ERROR_MSG_SIZE)) 
    {
      NSTL1(NULL, NULL, "Error: unable to decompress message. Message Headers: msg_len = %d, "
          "opcode = %d, child_id = %d, ns_version = %d, gen_rtc_idx = %d, testidx = %d, DecompressionTime(msec) = %ld", 
           msg->msg_len, msg->opcode, msg->child_id, msg->ns_version, msg->gen_rtc_idx, msg->testidx, 
           (get_ms_stamp() - start_time_in_msecs));
      return NULL;
    }

    //Grow size of mccptr->read_buf and copy uncomppress data into this 
    if ((mccptr->read_buf_size - msg_hdr_len) < ns_nvm_scratch_buf_len) 
    {
      int new_size = msg_hdr_len + ns_nvm_scratch_buf_len + 4096; 

      NSTL1(NULL, NULL, "Re-allocating mccptr->read_buf for dcompression, "
          "MccptrReadBufOldSize = %d, MccptrReadBufNewSize = %d, MsgHdrSize = %d, "
          "CompressedMsgSize = %d, DeCompressedMsgSize = %d, DecompressionTime(msecs) = %ld, "
          "MessageHeaders: {msg_len = %d, opcode = %d, child_id = %d, "
          "NSVersion = %d, gen_rtc_idx = %d, testidx = %d}", 
           mccptr->read_buf_size, new_size, msg_hdr_len, (msg->msg_len - msg_hdr_len + 4),
           ns_nvm_scratch_buf_len, (get_ms_stamp() - start_time_in_msecs), msg->msg_len, msg->opcode,
           msg->child_id, msg->ns_version, msg->gen_rtc_idx, msg->testidx);

      MY_REALLOC_EX(mccptr->read_buf, new_size, mccptr->read_buf_size, "Increasing mccptr->read_buf for decompression", -1);
      mccptr->read_buf_size = new_size;
      msg = (parent_child *)mccptr->read_buf;
    }

    memcpy(((char *)msg + msg_hdr_len), ns_nvm_scratch_buf, ns_nvm_scratch_buf_len);
    
    *size = msg_hdr_len + ns_nvm_scratch_buf_len;  
    msg->msg_len = *size - 4;
  }

  return (mccptr->read_buf);  
}

/**
 * All noblock read and write functions for parent <-> child
 * communication.
 */


/**
 * Returns
     - buf on succesful read
     - NULL on partial read.
     - Exits in error
 */

/* Neeraj - sz is not used */

char *
read_msg(Msg_com_con *mccptr, int *sz, int thread_flag)
{
  int bytes_read;  // Bytes read in one read call
  //int size_tmp;
  int fd = mccptr->fd;
  if (fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for %s.. returning.", msg_com_con_to_str(mccptr));
    return NULL;  // Issue - this is misleading as it means read is not complete
  }

  if (!(mccptr->state & NS_STATE_READING)) // Method called for first time to read message
  {
    NSDL1_MESSAGES(NULL, NULL, "Method called to read message for the first time. %s", msg_com_con_to_str(mccptr));
    mccptr->read_offset = 0;
    mccptr->read_bytes_remaining = -1;
  }
  else
    NSDL1_MESSAGES(NULL, NULL, "Method called to read message which was not read completly. offset = %d, bytes_remaining = %d, %s", 
              mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));


  if (mccptr->read_offset < sizeof(int))  // Message length is not yet read
  {
    while (1) /* Reading msg length */
    {
      NSDL2_MESSAGES(NULL, NULL, "Reading message length. Size received so far = %d, %s", mccptr->read_offset, msg_com_con_to_str(mccptr));

      if((bytes_read = read(fd, mccptr->read_buf + mccptr->read_offset, sizeof(int) - mccptr->read_offset)) < 0)
      {
        if (errno == EAGAIN)
        {
          NSDL2_MESSAGES(NULL, NULL, "Complete message length is not available for read. offset = %d, %s", 
                    mccptr->read_offset, msg_com_con_to_str(mccptr));
          mccptr->state |= NS_STATE_READING; // Set state to reading message
          return NULL;//NS_EAGAIN_RECEIVED;
        } else if (errno == EINTR) {   /* this means we were interrupted */
          NSDL2_MESSAGES(NULL, NULL, "Interrupted. continuing");
          continue;
        }
        else
        {
          NSTL1_OUT(NULL, NULL, "Error in reading message length, %s. error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
          gen_conn_failure_monitor_handling(mccptr->fd);
          CLOSE_MSG_COM_CON_EXIT(mccptr, thread_flag);
          return NULL; /* This is to handle closed connection from tools */
        }
      }
      if (bytes_read == 0) {
        /*Receiving 0 bytes on fd means connection has been closed from remote side, this isnt an error case. Therefore printing error can be misleading, hence do not print error in log statements*/
        NSDL2_MESSAGES(NULL, NULL, "Connection (%s) closed from other side. mccptr->flags = %X", msg_com_con_to_str(mccptr), mccptr->flags);

        //memset netcloud ip data monitor data when generator connection breaks.
        if (thread_flag == DATA_MODE)
          gen_conn_failure_monitor_handling(mccptr->fd);

        NSTL1(NULL, NULL, "Connection (%s) closed from other side.", msg_com_con_to_str(mccptr));
        if (mccptr->type != CONNECTION_TYPE_OTHER) 
          NSTL1_OUT(NULL, NULL, "Connection (%s) closed by other side.\n", msg_com_con_to_str(mccptr));  
        CLOSE_MSG_COM_CON_EXIT(mccptr, thread_flag);
        return NULL; /* This is to handle closed connection from tools */
      }
      mccptr->read_offset += bytes_read;
      mccptr->total_bytes_recieved += bytes_read;
      if(mccptr->read_offset == sizeof(int))
      {
        NSDL2_MESSAGES(NULL, NULL, "Complete message length is read. msg_len = %d", ((parent_child *)(mccptr->read_buf))->msg_len);
        mccptr->read_bytes_remaining = ((parent_child *)(mccptr->read_buf))->msg_len; // message size without msg length

        if(mccptr->read_bytes_remaining > (mccptr->read_buf_size - mccptr->read_offset))
        {
          int new_size = mccptr->read_offset + mccptr->read_bytes_remaining;
          NSTL1(NULL, NULL, "Re-allocating mccptr->read_buf as available size is %d "
                            "but going to reading data of size  %d. new_size = %d, read_offset = %d\n", 
                             (mccptr->read_buf_size - mccptr->read_offset), mccptr->read_bytes_remaining, new_size,
                             mccptr->read_offset);
          MY_REALLOC_EX(mccptr->read_buf, new_size, mccptr->read_buf_size, "mccptr->read_buf increasing.", -1);
          mccptr->read_buf_size = new_size;
        }
        break;
      }
    }
  }

  while (mccptr->read_bytes_remaining) /* Reading rest of the message */
  {
    NSDL2_MESSAGES(NULL, NULL, "Reading rest of the message. offset = %d, bytes_remaining = %d, %s",
              mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));

    /* TODO: Vivek: Handle if mccptr->read_buf size get lesser than mccptr->read_bytes_remaining */
    if((bytes_read = read(fd, mccptr->read_buf + mccptr->read_offset, mccptr->read_bytes_remaining)) <= 0)
    {
      if(errno == EAGAIN)
      {
        NSDL2_MESSAGES(NULL, NULL, "Complete message is not available for read. offset = %d, bytes_remaining = %d, %s", 
                  mccptr->read_offset, mccptr->read_bytes_remaining, msg_com_con_to_str(mccptr));
        mccptr->state |= NS_STATE_READING; // Set state to reading message
        return NULL;// NS_EAGAIN_RECEIVED | NS_READING;
      }
      else
      { 
        if (bytes_read == 0) { 
          /*In case of reading 0 bytes print success instead of error string*/ 
          NSTL1_OUT(NULL, NULL, "Connection (%s) closed from other side.", msg_com_con_to_str(mccptr));
        } else {
          NSTL1_OUT(NULL, NULL, "Error in reading msg, %s. error = %s", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        }    
        gen_conn_failure_monitor_handling(mccptr->fd);
        CLOSE_MSG_COM_CON_EXIT(mccptr, thread_flag);
        return NULL; /* This is to handle closed connection from tools */
      }
    }
    mccptr->read_offset += bytes_read;
    mccptr->read_bytes_remaining -= bytes_read;
    mccptr->total_bytes_recieved += bytes_read;
  }

  NSDL2_MESSAGES(NULL, NULL, "Complete message read. Total message size read = %d, %s", 
            mccptr->read_offset, msg_com_con_to_str(mccptr));
  mccptr->state &= ~NS_STATE_READING; // Clear state as reading message is complete
  *sz = mccptr->read_offset;

  //For JMeter msg structure are different and it's opcode are own so we retrun from here.
  if (mccptr->con_type == NS_JMETER_DATA_CONN)
    return mccptr->read_buf;

  return chk_and_decomp_read_msg(mccptr, sz);
}

// do_not_call_kill_all_children - Not used. Need to remove
int write_msg(Msg_com_con *mccptr, char *buf, int size, int do_not_call_kill_all_children, int thread_flag) 
{
  int fd = mccptr->fd;
  int bytes_writen;
  char *msg_ptr;
  int new_size = 0;
  int queued_size_limit;  // For event logging
  //int copy_was_done = 0;

  NSDL1_MESSAGES(NULL, NULL, "Method called. Size = %d, fd = %d, ip = %s", size, fd, mccptr->ip);

  if (fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for %s.. returning.", msg_com_con_to_str(mccptr));
    return 0;  // Return 0 to indicate write is nor partial
  }

 // If buf is passed then it means method is called for sending message for the first time
  NSDL2_MESSAGES(NULL, NULL, "mccptr->state = %d", mccptr->state);

  if ((buf != NULL) && (mccptr->state & NS_STATE_WRITING))
  {
    new_size = mccptr->write_offset + mccptr->write_bytes_remaining + size;
    
    if(new_size < 0)
    {
      //Buffer size exceeding the maximum value of integer ( 0x7FFFFFFF in HEX , 2147483647 in Decimal)
      //Here if flag is not set then error will be logged and flag will be set so that it can be logged only once until flag reset
      //Flag will reset when all data has been written and write_offset is zero.
      if(!(mccptr->flags & NS_MSG_COM_CON_BUF_SIZE_EXCEEDED))
      { 
         NSTL1(NULL, NULL, "Error: mccptr write buffer size exceeding from maximum value of integer, "
                           "write_offset = %d, write_bytes_remaining = %d, size = %d, thread_flag = %d, my_child_index",
                            mccptr->write_offset, mccptr->write_bytes_remaining, size, thread_flag, my_child_index);

        mccptr->flags |= NS_MSG_COM_CON_BUF_SIZE_EXCEEDED;
      }
      return 0;
    }
    
    if ((new_size) > mccptr->write_buf_size) {

      MY_REALLOC_EX(mccptr->write_buf, new_size, mccptr->write_buf_size, "mccptr->write_buf increasing.", -1);
      mccptr->write_buf_size = new_size;
    }

    memcpy(mccptr->write_buf + mccptr->write_offset + mccptr->write_bytes_remaining, buf, size); /* append in the end */
    mccptr->write_bytes_remaining += size;

    msg_ptr = mccptr->write_buf;

  } else if (buf != NULL) {
    msg_ptr = buf;
    mccptr->write_bytes_remaining = size;
    mccptr->write_offset = 0;
    mccptr->flags &= ~NS_MSG_COM_CON_BUF_SIZE_EXCEEDED;
  } else {                      /* buf == NULL */
    msg_ptr = mccptr->write_buf;
  }
  
  /*Some Conditional Event logs What should i do i dont have opcode filled here*/

  while (mccptr->write_bytes_remaining)
  {
    if (!buf) {
      NSDL2_MESSAGES(NULL, NULL, "Sending rest of the message. offset = %d, bytes_remaining = %d, %s", 
                mccptr->write_offset, mccptr->write_bytes_remaining, msg_com_con_to_str(mccptr));
    } else {
      NSDL2_MESSAGES(NULL, NULL, "Sending the message. bytes_remaining = %d, %s", 
                      mccptr->write_bytes_remaining, msg_com_con_to_str(mccptr));
    }

    if ((bytes_writen = write (fd, msg_ptr + mccptr->write_offset, mccptr->write_bytes_remaining)) < 0)
    {
      queued_size_limit = g_avgtime_size * mccptr->overflow_count;

      if(mccptr->write_bytes_remaining >= queued_size_limit) {
         
        mccptr->overflow_count++;
        
          /* Issue - We were facing an issue that we are not abe to write
	  *         so as we get EAGAIN we are forcing it to EPOLLIN/EPOLLOUT */          
         if(mccptr->overflow_count > 2) {
	    //mod_select_msg_com_con((char *)mccptr, fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT);
            MOD_SELECT_MSG_COM_CON((char *)mccptr, fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT, thread_flag);
         }
      }
  
      if (errno == EAGAIN) // No more data is available at this time, so set state and return
      {
        if (!(mccptr->state & NS_STATE_WRITING)) {
          MOD_SELECT_MSG_COM_CON((char *)mccptr, fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT, thread_flag);
          //19 Aug, 2017: This is case when write_buf_size is less then write_bytes_remaining.
          if (mccptr->write_bytes_remaining > mccptr->write_buf_size) {
            MY_REALLOC_EX(mccptr->write_buf, mccptr->write_bytes_remaining, mccptr->write_buf_size, "mccptr->write_buf increasing.", -1);
            mccptr->write_buf_size = mccptr->write_bytes_remaining;
          } 
          bcopy(msg_ptr + mccptr->write_offset, mccptr->write_buf, 
                mccptr->write_bytes_remaining);
          mccptr->write_offset = 0;
        }

        mccptr->state |= NS_STATE_WRITING; // Set state to writing message
        return 0;//On receiving EAGAIN return success rather fail status .
      }
      else
      {
        NSDL2_MESSAGES(NULL, NULL, "Error in write (MsgCom = %s) due to error = %s", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Error in write (MsgCom = %s) due to error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
        CLOSE_MSG_COM_CON_EXIT(mccptr, thread_flag);
        return -1; /* This is to handle unable to write to tools */
      }
    }
    /* what if byte_writen == 0 */
    if (bytes_writen == 0) {
      NSDL2_MESSAGES(NULL, NULL, "write returned = 0 for %s", msg_com_con_to_str(mccptr));
      continue;
    }
    mccptr->write_offset += bytes_writen;
    mccptr->write_bytes_remaining -= bytes_writen;
    mccptr->total_bytes_sent += bytes_writen;
  }

  /* we are done writing */
  // Reset FLAG as we have done writing
  mccptr->overflow_count = 1; 
  if (mccptr->state & NS_STATE_WRITING) {
    mccptr->state &= ~NS_STATE_WRITING;
    MOD_SELECT_MSG_COM_CON((char *)mccptr, fd, EPOLLIN | EPOLLERR | EPOLLHUP, thread_flag);
  }
  NSDL2_MESSAGES(NULL, NULL, "Exiting method");
  return 0;
}

void send_end_test_ack_msg(int conn_mode)
{
  EndTestRunMsg end_test_ack_msg; // Acknowledment message

  end_test_ack_msg.opcode = END_TEST_RUN_ACK_MESSAGE;
  end_test_ack_msg.testidx = testidx;
  end_test_ack_msg.child_id = g_parent_idx;
  end_test_ack_msg.msg_len = sizeof(end_test_ack_msg) - sizeof(int);
  Msg_com_con *mccptr = (conn_mode == DATA_MODE)?g_dh_master_msg_com_con:g_master_msg_com_con;
  NSTL1(NULL, NULL, " Sending END_TEST_RUN_ACK_MESSAGE to master, opcode = %d ", end_test_ack_msg.opcode);
  NSDL1_MESSAGES(NULL, NULL, " Sending END_TEST_RUN_ACK_MESSAGE to master, opcode = %d ", end_test_ack_msg.opcode);
  write_msg(mccptr, (char *)(&end_test_ack_msg), sizeof(EndTestRunMsg), 0, conn_mode);
}

void send_end_test_msg(char *msg, int status)
{
  EndTestRunMsg end_test_msg;
  int i;

  NSDL1_MESSAGES(NULL, NULL, "Method called, status = %d, loader_opcode = %d", status, loader_opcode);
  NSTL1(NULL, NULL, "Method called, status = %d, loader_opcode = %d", status, loader_opcode);
  memset(&end_test_msg, 0, sizeof(EndTestRunMsg));
  end_test_msg.opcode = END_TEST_RUN_MESSAGE;
  end_test_msg.testidx = testidx;
  end_test_msg.child_id = g_parent_idx;
  end_test_msg.status = status;
  end_test_msg.msg_len = sizeof(end_test_msg) - sizeof(int); 
  if ((status == USE_ONCE_ERROR) || (status == MEMPOOL_EXHAUST)) { //USEONCE or POOL_EXHAUST
    NSDL1_MESSAGES(NULL, NULL, "message = %s", msg);
    sprintf(end_test_msg.error_msg, "%s", msg);
  }

  if (loader_opcode == MASTER_LOADER)
  {
    for (i = 0; i < sgrp_used_genrator_entries; i++) 
    {
      if (g_dh_msg_com_con[i].fd == -1) {
        if (g_dh_msg_com_con[i].ip) {
          NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", 
                  msg_com_con_to_str(&g_dh_msg_com_con[i]));
        }
      } else {
        NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s", i, end_test_msg.opcode, 
                msg_com_con_to_str(&g_dh_msg_com_con[i]));
        write_msg(&g_dh_msg_com_con[i], (char *)&end_test_msg, sizeof(EndTestRunMsg), 0, DATA_MODE);
      }

      if (g_msg_com_con[i].fd == -1) {
        if (g_msg_com_con[i].ip) {
          NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", 
                  msg_com_con_to_str(&g_msg_com_con[i]));
        }
      } else {
        NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s", i, end_test_msg.opcode, 
                msg_com_con_to_str(&g_msg_com_con[i]));
        write_msg(&g_msg_com_con[i], (char *)&end_test_msg, sizeof(EndTestRunMsg), 0, CONTROL_MODE);
      }

    }
  } else{ //CLIENT_LOADER
    write_msg(g_dh_master_msg_com_con, (char *)(&end_test_msg), sizeof(EndTestRunMsg), 0, DATA_MODE);
  }
}
// This method is used only to send message which need only opcode
void send_msg_to_master(int fd, int opcode, int th_mode)
{
  parent_child send_msg;
  EndTestRunMsg end_test_msg;

  NSDL3_MESSAGES(NULL, NULL, "Sending message to master. opcode = %d", opcode);
  NSTL1(NULL, NULL, "(Generator -> Master) opcode = %d", opcode);

  if(fd == -1) {
    NSTL1(NULL, NULL, "Connection from master has been broken. opcode = %d", opcode);
    return;
  }

  /*fig bug 9352 where message not recive to master when one generator is killed in running test*/
  if (opcode == END_TEST_RUN_MESSAGE) {
    end_test_msg.opcode = opcode;
    end_test_msg.child_id = g_parent_idx;
    end_test_msg.msg_len = sizeof(end_test_msg) - sizeof(int);
    if(th_mode == DATA_MODE)
      write_msg(g_dh_master_msg_com_con, (char *)(&end_test_msg), sizeof(end_test_msg), 0, DATA_MODE);
    else
      write_msg(g_master_msg_com_con, (char *)(&end_test_msg), sizeof(end_test_msg), 0, CONTROL_MODE);
      
  } else {
    send_msg.child_id = g_parent_idx;
    send_msg.abs_ts = time(NULL) * 1000;
    send_msg.opcode = opcode;
    send_msg.testidx = testidx;
    send_msg.gen_rtc_idx = g_rtc_msg_seq_num;
    send_msg.avg_time_size = g_avgtime_size - ip_avgtime_size;
    send_msg.msg_len = sizeof(send_msg) - sizeof(int);
    NSDL3_MESSAGES(NULL, NULL, "send_msg.avg_time_size = %d, ip_avgtime_size = %d, g_rtc_msg_seq_num = %d", 
                                                             send_msg.avg_time_size, ip_avgtime_size, g_rtc_msg_seq_num);
    /* NC: In case of URL normalization, on controller we require maximum number of NVM 
     * hence each generator need to send number of NVMs. */
    if (opcode == START_MSG_BY_CLIENT) 
      send_msg.num_nvm_per_generator = global_settings->num_process;
    if (th_mode == DATA_MODE)
      write_msg(g_dh_master_msg_com_con, (char *)(&send_msg), sizeof(send_msg), 0, DATA_MODE);
    else
      write_msg(g_master_msg_com_con, (char *)(&send_msg), sizeof(send_msg), 0, CONTROL_MODE);
  }
}

/*******************************************************************************************************************
 |  • NAME:     
 |      forward_dh_msg_to_master_ex() - forwarding msg to master on data connection   
 |
 |  • DESCRIPTION:      
 |      This function will do following task -
 |      1. From release 4.4.0 we are sending compress data for progress report and percentile report generator to controller.
 |      2. For other opcodes data will be send without compress.
 |  
 |  • ARGUMENTS:
 |      1. fd ->     master data connection fd
 |      2. msg ->    Message to send gen to controller msg contain MSG_HDR and message
 |      3. size ->   size of msg and MSG_HDR
 |      4. offset -> Offset will be use only in compression only for (PR & Percentile report)   
 |      5. data_comp -> Flag for data compression (By default compression type is BR Compression)
 |         i).  Compression will be enabled through keyword PROGRESS_REPORT_QUEUE 
 |         ii). Below are results of BR and GZIP compression 
 |
 |   -----------------------------------------------------------------------------------------------------------
 |  |ComressionType |OrignalSize(Bytes) |CompressedSize(Bytes) |CompressionTime(msecs) |DecompressionTime(msecs)|
 |   ----------------------------------------------------------------------------------------------------------- 
 |  |BR             |163513920          |160237                       |1049                   |488                     |
 |  |GZIP          |163513920          |160697                |1250                   |608                     |
 |  |               |                   |                      |                       |                        |
 |  |BR             |163513920         |160254                |1150                   |393                     | 
 |  |GZIP           |163513920         |166165                |1259                   |536                     |
 |  |               |                   |                      |                       |                        |
 |  |BR             |21961972           |22087                 |286                    |41                      |
 |  |GZIP           |21833844           |21471                 |300                    |51                      |
 |  |               |                   |                      |                       |                        | 
 |  |BR             |22026036           |21471                 |286                    |41                      |
 |  |GZIP           |21449460           |21712                 |300                    |51                      |
 |  |               |                   |                      |                       |                        |
 |   -----------------------------------------------------------------------------------------------------------
 |  • RETURN VALUE:
 |      nothing
 ******************************************************************************************************************/

inline void
forward_dh_msg_to_master_ex(int fd, parent_msg *msg, int size, int offset, char data_comp)
{
  avgtime *amsg = &(msg->top.avg);
  long start_time_in_msecs;

  //For trace log only as due to compression amsg can not used
  double avg_elapsed = amsg->elapsed;
  int avg_complete = amsg->complete;
  double avg_abs_ts = amsg->abs_ts;

  NSDL2_MESSAGES(NULL, NULL, "Forwarding message to master. fd = %d, size = %d, "
    "opcode = %d, data_comp_type = %d, loader_opcode = %d",
     fd, size, msg->top.internal.opcode, global_settings->data_comp_type, loader_opcode);

  //This is only for transfer parent_child data
  if (loader_opcode == CLIENT_LOADER) {
    msg->top.internal.child_id = g_parent_idx;
    //NC: In progress report and finish report message, generator test run number is required 
    msg->top.internal.testidx = testidx;
    msg->top.internal.abs_ts = (time(NULL)) * 1000;
    msg->top.internal.msg_flag &= ~MSG_FLAG_COMPRESSED;   //Clear only compression bit
 
    if(msg->top.internal.opcode == PROGRESS_REPORT || msg->top.internal.opcode == PERCENTILE_REPORT)
    {
      if (data_comp)   //Checking for compression or not
      {
        char err_msg[ERROR_MSG_SIZE];  //using for error msg in case of compression failure
            
        /*****************************************************************************************
         Using scratch buffer for compressed data
         size-> is avg size 
         ns_nvm_scratch_buf_size -> size of scratch buffer malloc with (size + MSG_BUF_DELTA_SIZE)
         ns_nvm_scratch_buf_len -> output length of compressed data
         ****************************************************************************************/
        ns_nvm_scratch_buf_len = 0;   //Size of compressed data
        start_time_in_msecs = get_ms_stamp();  

        NSDL2_MESSAGES(NULL, NULL, "Going to compressed data, offset = %d", offset);
        //10MB
        if ((nslb_compress(((char *)msg + offset), (size - offset), &ns_nvm_scratch_buf, 
            (size_t *)&ns_nvm_scratch_buf_size, (size_t *)&ns_nvm_scratch_buf_len, data_comp, err_msg, ERROR_MSG_SIZE))) 
        {
          NSTL1(NULL, NULL, "(Generator:%d -> Master), Error in compression, so sending data without compression. Opcode = %d,"
                            " Failed time(msecs) = [%lld], Total size to compress = %d", msg->top.internal.opcode, 
                            (get_ms_stamp() - start_time_in_msecs), (size - offset));
        } 
        else if (ns_nvm_scratch_buf_len > (size - offset)) {
          NSTL1(NULL, NULL, "(Generator:%d -> Master), Compression data(%d) is more than Uncompressed Data(%d), "
             "so sending data without compression. Opcode = %d,"
             " CompressionTime(msecs) = [%ld]", 
               ns_nvm_scratch_buf_len, (size - offset), msg->top.internal.opcode, 
               (get_ms_stamp() - start_time_in_msecs));
        } 
        else {  // Compression done successfully
          NSTL2(NULL, NULL, "(Generator:%d -> Master), Compression successfull for opcode = %d, "
            "orignal size = %d, compressed size = %d, CompressionTime(msecs) = %ld", 
             g_parent_idx, msg->top.internal.opcode, (size - offset), 
             ns_nvm_scratch_buf_len, (get_ms_stamp() - start_time_in_msecs));

          //Copying compressed data & size into avg   
          memcpy((char *)msg + offset, ns_nvm_scratch_buf, ns_nvm_scratch_buf_len);
          size = ns_nvm_scratch_buf_len + offset;

          //Set compression flag 
          msg->top.internal.msg_flag |= MSG_FLAG_COMPRESSED;
        }
      }
 
      if (msg->top.internal.opcode == PROGRESS_REPORT)
      {
        amsg->elapsed += gen_delayed_samples;
        NSTL1(NULL, NULL, "PROGRESS_REPORT: (Generator:%d -> Master), SampleID = %d, IsLastSample = %d, "
                          "SampeBirthTime(ms) = %.0lf, CurSampleID = %d, gen_delayed_samples = %u, PR_PacketSize = %d, "
                          "RemainingBytesToWrite = %d",
                           g_parent_idx, avg_elapsed, avg_complete, avg_abs_ts, cur_sample,
                           gen_delayed_samples, size, g_dh_master_msg_com_con->write_bytes_remaining); 
      }
    }
    NSDL2_MESSAGES(NULL, NULL, "Done!!");
  }

  NSDL2_MESSAGES(NULL, NULL, "Send to Master, size = %d", size);

  msg->top.internal.msg_len = size - sizeof(int);
  NSTL1(NULL, NULL, "Forwarding message to master, Opcode = %d, cur_sample = %u, Total size to send = %d, "
                    "remaining data on connection = %d", msg->top.internal.opcode, cur_sample, size,
                     g_dh_master_msg_com_con->write_bytes_remaining);

  write_msg(g_dh_master_msg_com_con, (char *)msg, size, 0, DATA_MODE);
}

inline void
forward_dh_msg_to_master(int fd, parent_msg *msg, int size)
{
  forward_dh_msg_to_master_ex(fd, msg, size, 0, 0);
} 
 
inline void
forward_msg_to_master(int fd, parent_msg *msg, int size)
{
  NSDL3_MESSAGES(NULL, NULL, "Forwarding message to master. opcode = %d", msg->top.internal.opcode);
  //This is only for transfer parent_child data
  if (loader_opcode == CLIENT_LOADER) {
    msg->top.internal.child_id = g_parent_idx;
    //NC: In progress report and finish report message, generator test run number is required 
    msg->top.internal.testidx = testidx;
    msg->top.internal.abs_ts = (time(NULL)) * 1000;
  }
  msg->top.internal.msg_len = size - sizeof(int);
  NSTL1(NULL, NULL, "Forwarding message to master, Opcode = %d, Total size to send = %d, remaining data on connection = %d", msg->top.internal.opcode, size, g_master_msg_com_con->write_bytes_remaining);
  write_msg(g_master_msg_com_con, (char *)msg, size, 0, CONTROL_MODE);
}

char * 
msg_com_con_to_str(Msg_com_con *pcon)
{
  static char msg_com_con_str[256];
  sprintf(msg_com_con_str, "fd = %d, IP = %s, State = 0x%x, Connection = %s, write_bytes_remaining = %d", 
                            pcon->fd, pcon->ip, pcon->state, pcon->conn_owner, pcon->write_bytes_remaining);
  return(msg_com_con_str);
}

// Method to check gen id using ip
int get_gen_id_from_ip(char *gen_ip)
{
  char *rem_port;
  int gen_id;
  if ((rem_port = strrchr(gen_ip, '.')) != NULL)
      *rem_port = '\0';
   if ((gen_id = find_generator_idx_using_ip (gen_ip, 0)) == -1)
   {
      NSDL1_MESSAGES(NULL, NULL, "Generator id mistmatch hence returning");
      return -1;
   }
  return gen_id;
}

void 
close_msg_com_con_v2(char *file, int line, char * func, Msg_com_con *mccptr, int th_flag)
{
  int gen_id;
  int epfd;
  NSDL1_MESSAGES(NULL, NULL, "Method called. Closing connection for sock '%s' and "
                             "Caller = [%s]-[%d]-[%s]", msg_com_con_to_str(mccptr), file, line, func);

  if (my_port_index != 255)
    epfd = v_epoll_fd;
  else
  {
    if (th_flag == DATA_MODE)
    {
      epfd = g_dh_msg_com_epfd;
      if(loader_opcode == MASTER_LOADER)
      { 
        gen_id = mccptr->nvm_index;
        if(gen_id != -1) {
          generator_entry[gen_id].flags &= ~IS_GEN_ACTIVE; 
          strcpy(generator_entry[gen_id].test_end_time_on_gen, get_relative_time());
        }
      }
    }
    else 
     epfd = g_msg_com_epfd;
  }

  NSTL1(NULL, NULL, "Method called. Closing connection for sock '%s' and "
                    "epfd '%d'", msg_com_con_to_str(mccptr), epfd);
  /* close fd and remove from epoll */
  //Set nvm_index of the NVM as per v_port_table based on ip & port
  //set_nvm_index_after_finish_report(mccptr);

  /* do not remove ip here because it is used for logging purpose */
  REMOVE_SELECT_MSG_COM_CON(mccptr->fd, th_flag);
  mccptr->state = 0;
  if (close(mccptr->fd) < 0) 
    NSTL1_OUT(NULL, NULL, "Error in closing %s, error = %s\n", msg_com_con_to_str(mccptr), nslb_strerror(errno));
  mccptr->fd = -1;
  FREE_AND_MAKE_NULL_EX(mccptr->read_buf, g_avgtime_size, "mccptr->read_buf", -1);
  FREE_AND_MAKE_NULL_EX(mccptr->write_buf, g_avgtime_size + (sizeof(parent_child) * 2), "mccptr->write_buf", -1);
  mccptr->flags |= NS_MSG_COM_CON_IS_CLOSED;/* Set flag to indicate connection is closed. Currently used for continue on generator error */
}

void close_msg_com_con(Msg_com_con *mccptr)
{
  close_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, mccptr, CONTROL_MODE);
}


/* This method will not exit when conncetion from tools are closed. */
void close_msg_com_con_and_exit(Msg_com_con *mccptr, char *function_name, int line_num, char *file_name)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called. %s", msg_com_con_to_str(mccptr));
  // We must close msg_com_con before kill_all_children() is called
  // to make sure it does not send message to client which had error
  close_msg_com_con(mccptr);
  NSDL1_MESSAGES(NULL, NULL, "mccptr->flags = 0x%x, mccptr->type = %d", mccptr->flags, mccptr->type);

  // For connections from tools (Other type) - Both flags are set
  // For connections from NVM - Both flags are NOT set
  // For connections from client to master on client side - Both flags are NOT set
  // For connections from client to master on master side - 
  //     Conttinue on generator error is OFF - Both flags are NOT set
  //     Conttinue on generator error is ON - Both flags are set
  // 

  if (!(mccptr->flags & NS_MSG_COM_DO_NOT_CALL_EXIT))  // Used mainly for connection from tools
  { 
    NS_EXIT(-1, "mccptr->flag=0");
  }

  NSDL1_MESSAGES(NULL, NULL, "Method exited");
}

/**
 * This method is called to finish leftover write if any.
 * Generally called before pausing.
 */
void complete_leftover_write(Msg_com_con *mccptr, int th_flag)
{
  int cnt = 0, ret;
  char cmd[1024] = {0}; 
  char err_msg[1024] = "\0";
  NSTL1(NULL, NULL, "Going to send pending data on connection to parent/master = %s, wait_for_write = %d", 
                     msg_com_con_to_str(mccptr), global_settings->wait_for_write);
  NSDL3_MESSAGES(NULL, NULL, "Method called. %s", msg_com_con_to_str(mccptr));
  if (mccptr->state & NS_STATE_WRITING) {
    NSDL3_MESSAGES(NULL, NULL, "State is writing");
    while (cnt < global_settings->wait_for_write) {
      usleep(1000 * 1000);
      if ((write_msg(mccptr, NULL, 0, 0, th_flag) == 0) && (mccptr->write_bytes_remaining == 0))
        break;
      cnt++;
      NSTL1(NULL, NULL, "Waiting for writting after %d count on connection = (%s)", cnt, msg_com_con_to_str(mccptr));
    }
  }
  if (mccptr->state & NS_STATE_WRITING) { /* Unable to finish write */
    NSTL1(NULL, NULL, "Unable to finish write on connection (%s)", msg_com_con_to_str(mccptr));
    NSDL3_MESSAGES(NULL, NULL, "Unable to finish write to %s", msg_com_con_to_str(mccptr));
    sprintf(cmd, "%s/bin/get_user_stats.sh %s/logs/TR%d/system_stats.log.%d",
                                 g_ns_wdir, g_ns_wdir, testidx, my_child_index);
    ret = nslb_system(cmd,1,err_msg);
    if(ret != 0) {
      NSTL1_OUT(NULL, NULL,"Unable to run command = %s\n", cmd);
    }
    CLOSE_MSG_COM_CON_EXIT(mccptr, th_flag);
    return;
  }
}

inline void add_select_msg_com_con_ex(char *file, int line, char * func, char* data_ptr, int fd, int event)
{
  NSTL2(NULL, NULL, "add_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd);
  NSDL2_MESSAGES(NULL, NULL, "add_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd);
  add_select_msg_com_con(data_ptr, fd, event);
}

inline void remove_select_msg_com_con_ex(char *file, int line, char * func, int fd)
{
  NSTL2(NULL, NULL, "remove_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] fd = %d, \n", file, line, func, fd);
  NSDL2_MESSAGES(NULL, NULL, "remove_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] fd = %d, \n", file, line, func, fd);
  remove_select_msg_com_con(fd);
}

inline void mod_select_msg_com_con_ex(char *file, int line, char * func, char* data_ptr, int fd, int event)
{
  NSTL2(NULL, NULL, "mod_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd);
  NSDL2_MESSAGES(NULL, NULL, "mod_select_msg_com_con_ex Caller = [%s]-[%d]-[%s] ptr =  %p fd = %d, \n", file, line, func, data_ptr, fd);
  mod_select_msg_com_con(data_ptr, fd, event);
}


void end_test_run_msg_to_client(Msg_com_con *mccptr)
{
  EndTestRunMsg end_test_msg;
  NSDL1_MESSAGES(NULL, NULL, "Method called, %s, loader_opcode = %d", msg_com_con_to_str(mccptr), loader_opcode);

  memset(&end_test_msg, 0, sizeof(EndTestRunMsg));
  end_test_msg.opcode = END_TEST_RUN_MESSAGE;
  end_test_msg.testidx = testidx;
  end_test_msg.msg_len = sizeof(end_test_msg) - sizeof(int); 

  write_msg(mccptr, (char *)(&end_test_msg), sizeof(EndTestRunMsg), 0, CONTROL_MODE);
}

int read_gen_token(int fd, char *token, int token_size, int timeout)
{
  int read_bytes = 0;
  int ret = 0;
  int retry = 10 * timeout; //30 Sec
  char token_buf[token_size + 1];
  
  NSDL1_MESSAGES(NULL, NULL, "Method called,  token_size  = %d , timeout = %d", token_size, timeout);
 
  while(retry > 0)
  {
    if((ret = read(fd, token_buf + read_bytes, (token_size - read_bytes))) == 0)
    {
      NSDL1_MESSAGES(NULL, NULL, "Connection closed from peer side");
      return -1;
    }
    if(ret > 0)
    {
      read_bytes += ret;
      NSDL1_MESSAGES(NULL, NULL, "buffer = %s, read_bytes = %d", token_buf, read_bytes);
      //if(read_bytes > token_size)
        //return -1;
       
      if(read_bytes == token_size)
      {
        NSDL1_MESSAGES(NULL, NULL, "Token Received");
        memcpy(token, token_buf, token_size); 
        return 0;
      }
    }
    usleep(100 * 1000); //100 ms
    retry--;
  }
  return -1; 
}

int get_gen_id_from_ip_token(char *gen_ip, int fd)
{
   int gen_id;
   int idx;
   char token[2*MD5_DIGEST_LENGTH +1];

   NSDL1_MESSAGES(NULL, NULL, "Method called , gen_ip = %s , fd = %d", gen_ip, fd);
   //match ip , if not match then return -1;
   if ((gen_id = get_gen_id_from_ip(gen_ip)) == -1)
   {
     //NSDL1_MESSAGES(NULL, NULL, "Generator id mistmatch hence returning");
      return -1;
   }
   //read token , timeout 30 sec
   if((read_gen_token(fd, token, 2*MD5_DIGEST_LENGTH , 30)) == -1)
   {
     NSDL1_MESSAGES(NULL, NULL, "Failed to read token hence returning");
     return -1;
   }
   token[2*MD5_DIGEST_LENGTH] = '\0';


   //match token , if match return gen_idx else try next;
   if(!strcmp(token, (char *)generator_entry[gen_id].token))
   {
     NSDL1_MESSAGES(NULL, NULL, "Generator ip & token match returning index = %d", gen_id);
//     write(fd, token, 2*MD5_DIGEST_LENGTH); 
     return gen_id;
   }
   /*Search Next*/
   for (idx = gen_id + 1; idx < sgrp_used_genrator_entries; idx++)
   {
     if(!strcmp(gen_ip, generator_entry[idx].resolved_IP) && !strcmp(token, (char *)generator_entry[idx].token))
     {
       NSDL1_MESSAGES(NULL, NULL, "Generator ip & token match returning index = %d", idx);
  //     write(fd, token, 2*MD5_DIGEST_LENGTH);
       return idx;
     }
   }
   return -1;
}
