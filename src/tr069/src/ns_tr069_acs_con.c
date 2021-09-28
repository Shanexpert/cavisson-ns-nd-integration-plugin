#include "ns_tr069_includes.h"
#include "../../ns_vuser_trace.h"
#include "../../ns_group_data.h"

/* create a listener connection for the cpe to receive RFC */

#define TR069_CPE_PORT_AUTO  -1
#define TR069_CPE_IP_AUTO    NULL

static FILE *tr069_trace_fp  = NULL;
static struct sockaddr_in tr069_sin;

inline void tr069_init_addr_for_admin_ip() {

  NSDL2_HTTP (NULL, NULL, "Method called, g_cavinfo.NSAdminIP = [%s]", g_cavinfo.NSAdminIP);
  
  tr069_sin.sin_family = AF_INET;
  inet_pton (AF_INET, g_cavinfo.NSAdminIP, &(tr069_sin.sin_addr));
}


static void tr069_log_trace(VUser *vptr, char* url) {

  char trace_file_name[1024];
  
  sprintf(trace_file_name, "%s/logs/TR%d/tr069_cpe_trace.log", g_ns_wdir, testidx);

  if(tr069_trace_fp == NULL) {
    tr069_trace_fp = fopen(trace_file_name, "a+");
    if(!tr069_trace_fp) {
      fprintf(stderr, "Unable to open file [%s] for writing.\n", trace_file_name);
      return;
    }
  }

  fprintf(tr069_trace_fp, "%d,%u,%s\n", my_port_index, vptr->user_index, url);
  fflush(tr069_trace_fp);
}

int cpe_add_listener_socket (VUser *vptr, char *hostname,  unsigned short port, char *url);
// int ns_tr069_register_rfc (char *ip, int port, char *url) {
// Ip is NULL, if NS need to assign IP
// port is -1, netstorm need to assign it
// url is just the url path (absolute url e.g. /cpe/index.html" 
// url is not used by NS. It may be used in later
//

int ns_tr069_register_rfc_ext (VUser *vptr, char *ip, unsigned short port, char *url) {
   connection* cptr;
/* 
 * need to check if we already have a listener socket for this cpe 
 *  and if so return error. There can be max 2 cptrs per cpe . one for 
 * rfc, and other if a connection is established to an ACS
 */
  // Loop through cptrs for this user  and check for existing listener

  NSDL2_TR069 (vptr, NULL, "Method called");

  // Check if already listening or not as this API will get called many times in
  // same session or across sessions
  /* TODO: Manpreet: How to find cptr having CNST_LISTENING state?
   * Earlier here we were using vptr->first_cptr
   * */ 

  for (cptr = vptr->head_cinuse; cptr != NULL; cptr = (connection *)cptr->next_inuse) {
    if (cptr->conn_state == CNST_LISTENING) {
      NSDL2_TR069 (NULL, NULL, "Already listening.");
      return(0);
    }
  }

  // Used to track number of acs connections queued up
  vptr->num_requests = 0;
  cpe_add_listener_socket (vptr, ip, port, url);
  return 0;
}

/* 
 * wait indefinitely or with timeout (ms) 
 * terminate wait if a RFC occurs. You can also use setitimer(), but
 * w/o a callback mechanism which is triggered when an event is recvd, 
 * the only way is to check the flag in a loop
 */
#if 0
int ns_tr069_wait(unsigned int timeout, unsigned short event) {

  uint_t total =0, incr =500;

  NSDL2_TR069 (NULL, NULL, "Method called, ip = [%s], port = [%d]", ip, port);

  for (;;) {
    usleep(incr);  // sleep for 0.5 ms
    if (tr69_get_rfc())
	return(1);		//event occured
    if (!timeout) continue; //indefinite wait
    total += incr;	  
     if (total >= timeout)
	return(0);
  }
}
#endif


// Wait  timer callback
static void tr069_wait_timer(ClientData client_data, u_ns_ts_t now)
{
  VUser *vptr;

  vptr = client_data.p; // p point to vptr

  NSDL2_TR069(vptr, NULL, "Method Called.");

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    // Wakeup the ns_page_think_time() API
    NSDL3_SCHEDULE(vptr, NULL, "TR069 wait time is over. Changing to vuser context");
    switch_to_vuser_ctx(vptr, "TR069WaitTimeOrRFCOver"); 
    return;
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_LEGACY)
  {
    // TODO: Add code for other modes
    return;
  }
}

// This is like session pacing timer but used differently in tr069 
// as user is always in same session
inline int nsi_tr069_wait_time(VUser *vptr, u_ns_ts_t now)
{
ClientData cd;

  NSDL2_TR069(vptr, NULL, "Method Called. vptr = %p, wait time = %d milli-seconds", vptr, vptr->pg_think_time);

  vptr->timer_ptr->actual_timeout = vptr->pg_think_time;

  cd.p = vptr;
  (void) dis_timer_think_add(AB_TIMEOUT_STHINK, vptr->timer_ptr, now, tr069_wait_timer, cd, 0);
  return 0;
}

int ns_tr069_get_rfc_ext(VUser *vptr, int wait_time) {

  NSDL2_TR069(NULL, NULL, "Method called. wait_time = %d seconds", wait_time);

  // Check if user is marked for ramp down
  // If yes, then call user cleanup as we need to remove this used from the system
  if (vptr->flags & NS_VUSER_RAMPING_DOWN)
  {
    NSDL2_TR069(NULL, NULL, "User is ramping down. So ending session");
    vut_add_task(vptr, VUT_END_SESSION);
    switch_to_nvm_ctx(vptr, "TR069UserRampedDown");
    // It will never return from here as NVM will close session and never switch to User context
    return 0; 
  }

  if(vptr->httpData->flags & NS_TR069_EVENT_GOT_RFC)
  {
    NSDL2_TR069(NULL, NULL, "Got RFC from ACS");
    // DO NOT clear NS_TR069_GOT_RFC bit in flags here
    // It will be clear after sending inform (TODO)
    vptr->num_requests = 0; // Reset number of acs connection pending
    return NS_TR069_RFC_FROM_ACS;
  }

  if(vptr->httpData->flags & NS_TR069_EVENT_VALUE_CHANGE_ACTIVE)
  {
    NSDL2_TR069(NULL, NULL, "Got active value change");
    return NS_TR069_VALUE_CHANGE_ACTIVE;
  }

  // Set state to waiting as it is like in session pacing
  // This is done so that ramp down can be done
  VUSER_ACTIVE_TO_WAITING(vptr);

  // If reboot request came, then we need to simulate reboot by starting
  // session with ACS with some delay.

  wait_time = wait_time * 1000; // Convert seconds to milli-seconds

  if(vptr->httpData->flags & NS_TR069_EVENT_M_REBOOT)
  {
    // This is the delay to simulate the time taken to reboot.
    NSDL2_TR069(NULL, NULL, "global_settings->tr069_reboot_min_time = %d, global_settings->tr069_reboot_max_time = %d", global_settings->tr069_reboot_min_time, global_settings->tr069_reboot_max_time);
    wait_time = (double)global_settings->tr069_reboot_min_time + (double)(((double)global_settings->tr069_reboot_max_time - ((double)global_settings->tr069_reboot_min_time - 1)) * (rand()/(RAND_MAX + (double)global_settings->tr069_reboot_max_time))); 
    NSDL2_TR069(NULL, NULL, "Reboot request came from ACS. Wait time %d milli secs", wait_time);
  }

  if(vptr->httpData->flags & NS_TR069_EVENT_M_DOWNLOAD)
  {
    //This is the delay to simulate the time taken to download FW.
    NSDL2_TR069(NULL, NULL, "global_settings->tr069_download_min_time = %d, global_settings->tr069_download_max_time = %d", global_settings->tr069_download_min_time, global_settings->tr069_download_max_time);
    wait_time = (double)global_settings->tr069_download_min_time + (double)(((double)global_settings->tr069_download_max_time - ((double)global_settings->tr069_download_min_time - 1)) * (rand()/(RAND_MAX + (double)global_settings->tr069_download_max_time))); 
    NSDL2_TR069(NULL, NULL, "FW download request came from ACS. Wait time %d milli secs", wait_time);
  }

  // Save in virtual user trace
  if(NS_IF_TRACING_ENABLE_FOR_USER){
    //ut_add_internal_page_node(vptr, "TR069WaitForRFC", "NA", "TR069", wait_time * 1000);
    ut_add_internal_page_node(vptr, "TR069WaitForRFC", "NA", "TR069", wait_time);
  }

  if(wait_time <= 0) // wait till we get RFC from ACS
  {
    // Switch to NVM context
    NSDL2_HTTP(vptr, NULL, "Waiting for RFC from ACS");
    switch_to_nvm_ctx(vptr, "TR069WaitForRFCStart");
    VUSER_WAITING_TO_ACTIVE(vptr);
    // It will come here after we get RFC from ACS. NVM will do switch to user to context
    NSDL2_TR069(NULL, NULL, "Got RFC from ACS");
    
    vptr->num_requests = 0; // Reset number of acs connection pending
    return NS_TR069_RFC_FROM_ACS; 
  }

  // wait time is there, so start a timer
  vptr->pg_think_time = wait_time; // Convert seconds to milli-seconds
  vptr->httpData->flags |= NS_TR069_EVENT_WAITING_FOR_RFC_FROM_ACS; // Waiting with timeout
  vut_add_task(vptr, VUT_TR069_WAIT_TIME);
  NSDL2_HTTP(vptr, NULL, "Waiting for TR069 wait time (%d secs) to be over or RFC from ACS", wait_time);
  switch_to_nvm_ctx(vptr, "TR069WaitTimeOrRFCStart");
  VUSER_WAITING_TO_ACTIVE(vptr);

  if(vptr->httpData->flags & NS_TR069_EVENT_GOT_RFC)
  {
    NSDL2_TR069(NULL, NULL, "Got RFC from ACS");
    // DO NOT clear NS_TR069_GOT_RFC bit in flags here
    // It will be clear after sending inform
    vptr->num_requests = 0; // Reset number of acs connection pending
    return NS_TR069_RFC_FROM_ACS;
  }
  else
  {
    NSDL2_TR069(NULL, NULL, "TR069 wait is over");
    //if(!(vptr->httpData->flags & NS_TR069_EVENT_M_DOWNLOAD) || !(vptr->httpData->flags & NS_TR069_EVENT_TRANSFER_COMPLETE))
    if(!(vptr->httpData->flags & NS_TR069_EVENT_M_DOWNLOAD)){
      vptr->httpData->flags |= NS_TR069_EVENT_PERIODIC;
    }
    vptr->num_requests = 0; // Reset number of acs connection pending
    return NS_TR069_WAIT_IS_OVER; 
  }
}

inline void tr069_proc_rfc_from_acs(connection *cptr)
{
VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_TR069(NULL, cptr, "Method called. Vuser State = %s", vuser_states[vptr->vuser_state]);

  vptr->httpData->flags |= NS_TR069_EVENT_GOT_RFC;  // RFC DONE

  if(!(vptr->vuser_state & NS_VUSER_SESSION_THINK))
  {
    NSDL2_TR069(NULL, cptr, "User is not in waiting state. So setting flag and returning");
    return;
  } 

  // Check if CPE is waiting with timer or not
  if(vptr->httpData->flags & NS_TR069_EVENT_WAITING_FOR_RFC_FROM_ACS) 
  {
    NSDL2_TR069(NULL, cptr, "Cancelling timer for rfc wait");
    // Cancel timer so that callback gets called
    vptr->timer_ptr->actual_timeout = get_ms_stamp();
    dis_timer_reset(0, vptr->timer_ptr);  // Reset with 0
  }
  else
  {
    NSDL2_TR069(NULL, cptr, "Switching to User context");
    switch_to_vuser_ctx(vptr, "TR069WaitForRFCEnd");
  }

}

static void tr069_set_url(VUser *vptr, connection *cptr, char *url) {

  char url_loc[1024];
  char *save_url;

  if(url) {
    save_url = url;
  } else {
    save_url = url_loc;
    sprintf(save_url, "http://%s/ns_cpe_url", nslb_get_src_addr_ex(cptr->conn_fd, 1));
  }


  // Save in virtual user trace
  if(NS_IF_TRACING_ENABLE_FOR_USER){
    ut_add_internal_page_node(vptr, "TR069Listen", save_url, "TR069", 0);
  }

  tr069_set_param_values(vptr, "InternetGatewayDevice.ManagementServer.ConnectionRequestURL", -1, save_url, -1);

  tr069_log_trace(vptr, save_url);
}

#if 0
/* Get IP Port from url saved in TR069CpeUrlDP*/
static void tr069_get_url(VUser *vptr, char *ip, int *port) {

  int req_type;
  char *ptr;
  char req_line[512];
  char url_string[1024 + 1];

  //char *url = ns_eval_string("{TR069CpeUrlDP}");
  char *url =  tr069_get_full_param_values_str(vptr, "InternetGatewayDevice.ManagementServer.ConnectionRequestURL", -1);
  
  if(url && url[0] != '\0') {
    strncpy(url_string, url, 1024);
    url_string[1024] = '\0';
    parse_url(url_string, NULL, &req_type, ip, req_line);
    ptr = index(ip, ':');
    if(ptr) {
      *port = atoi(ptr + 1);
      *ptr  = '\0';
    } else {
      ip[0] = '\0';
    }
  }

  NSDL2_TR069(NULL, NULL, "Extracted IP = [%s], Port = [%d]", ip, *port);
}
#endif

/* Create a listener socket for the RFC from ACS
 * TODO: How to handle Ramp Down Phase as we will have to free rfc_url_num 
 */
int cpe_add_listener_socket (VUser *vptr, char *hostname,  unsigned short port, char *url) {
  action_request_Shr *rfc_url_num;
  connection *new_cptr = NULL;

  NSDL2_HTTP (NULL, NULL, "Method called");
 
  // TODO: Use static url num 
  MY_MALLOC(rfc_url_num, sizeof(action_request_Shr), "rfc_url_num", -1);
  memset(rfc_url_num, 0, sizeof(action_request_Shr));

  rfc_url_num->request_type = CPE_RFC_REQUEST;

  new_cptr =  get_free_connection_slot(vptr);
  if (new_cptr == NULL) {
    fprintf(stderr, "%s: could not get new cptr for RFC\n", (char*)__FUNCTION__);
    return(1);
  }

  new_cptr->url_num = rfc_url_num;
  new_cptr->request_type = rfc_url_num->request_type;
  //SET_URL_NUM_IN_CPTR(new_cptr, rfc_url_num);
  //new_cptr->url_num = NULL;

  new_cptr->conn_state = CNST_LISTENING;


  /* 1. For first time we need to pic IP/Port from ip entries which
   *    already set in user_ip or given by user in  ns_tr069_register_rfc API.
   * 2. For second time we need to get it from decalare var TR069CpeUrlDP 
   */

  char ip_from_var[512] = "\0";
  int port_from_var = -1;

  if(hostname == TR069_CPE_IP_AUTO) {
#if 0  // we want to frsh listen whenever start a test dont take from data tree
    tr069_get_url(vptr, ip_from_var, &port_from_var);
#endif
    if(ip_from_var[0] == '\0') {
      NSDL2_HTTP (NULL, NULL, "Using vuser picked ip, total_ip_entries = %d", total_ip_entries);
      if(!total_ip_entries) {
        memcpy(&(new_cptr->cur_server), (struct sockaddr*)&(tr069_sin), sizeof(struct sockaddr));
      } else {
        memcpy(&(new_cptr->cur_server), &(vptr->user_ip->ip_addr), sizeof(struct sockaddr));
      }
    } else {
      if (!nslb_fill_sockaddr(&(new_cptr->cur_server), ip_from_var, port_from_var)) {
        fprintf(stderr, "Error: Host <%s> specified by Host header is not a valid hostname. Exiting \n",
                       hostname);
        end_test_run();
      }

    }
  } else {
    NSDL2_HTTP (NULL, NULL, "Using %s:%d", hostname, port);
    if (!nslb_fill_sockaddr(&(new_cptr->cur_server), hostname, port)) {
      fprintf(stderr, "Error: Host <%s> specified by Host header is not a valid hostname. Exiting \n",
                       hostname);
      end_test_run();
    }
  }

  // If TR069CpeUrlDP have empty then it is first time
  u_ns_ts_t now = get_ms_stamp();
  start_listen_socket(new_cptr, now, NULL);  //existing routine

  // We need to save  IP/Port into decalare var as next time we pick same IP/Port for this CPE 
  tr069_set_url(vptr, new_cptr, url);

  // vptr->http flags &= ~NS_TR069_GOT_RFC; 
  return 0;
}

/*Accep connection from ACS*/
int tr069_accept_connection(connection *cptr, u_ns_ts_t now) {

  socklen_t len = sizeof(struct sockaddr_in);
  struct sockaddr_in their_addr;

  int fd;
  action_request_Shr *rfc_url_num = NULL; 
  connection *new_cptr = NULL;
  VUser *vptr = cptr->vptr;

  NSDL2_HTTP (NULL, cptr, "Method called");

  if (cptr->url_num->request_type == CPE_RFC_REQUEST) {
    NSDL3_SOCKETS(NULL, cptr, "Method called CPE RFC conn fd = %d", cptr->conn_fd);

    //if we exceeded the limit for accepting connections for this CPE, reject this one
/*
    if (((VUser*)(cptr->vptr))->num_requests > CPE_MAX_RFC_REQUESTS) {
      NSDL3_SOCKETS(NULL, cptr, "CPE rejecting RFC. outstanding requests %d max %d", 
                                 ((VUser*)(cptr->vptr))->num_requests,  CPE_MAX_RFC_REQUESTS);
      goto cancel_action;
    }
*/

    //if((fd = accept(cptr->conn_fd, NULL, 0)) < 0) {
    if((fd = accept(cptr->conn_fd, (struct sockaddr *)&their_addr, (socklen_t *)&len)) < 0) {
      if(errno == EAGAIN) 
      {
        fprintf(stderr, "Error: CPE RFC accept failed with EAGAIN: err = %s\n", strerror(errno));
        return 0;
      }
      fprintf(stderr, "Error: CPE RFC accept failed: err = %s\n", strerror(errno));
      return -1;
    }

    NSDL4_SOCKETS(NULL, NULL, "CPE RFC Connection accepted from ACS Server IP = %s, fd = %d",
                 nslb_sock_ntop((const struct sockaddr *)&their_addr), fd); // log IP

    if ( fcntl( fd, F_SETFL, O_NDELAY ) < 0 ) {
      fprintf(stderr, "CPE RFC fcntl failed: err = %s", strerror(errno));
      close(fd);
      return -1;
    }
    //new_url_num->request_type = HTTP_RESPONSE;
    new_cptr =  get_free_connection_slot(vptr);
    if (new_cptr == NULL) {
      fprintf(stderr, "accept_connection: could not get new cptr  for RFC\n");
      close(fd);
      return -1;
    }

    /* need to respond back to the ACS , but we need to leave the listening connection as is */
    //get  a new cptr
    MY_MALLOC(rfc_url_num, sizeof(action_request_Shr), "rfc_url_num", -1);
    memset(rfc_url_num, 0, sizeof(action_request_Shr));

    new_cptr->url_num = rfc_url_num; 
    new_cptr->conn_fd = fd;
    new_cptr->conn_state = CNST_REQ_READING;
    new_cptr->bytes_left_to_send = -1;
    // Following are used to keep in buffer if partial when reading request header
    new_cptr->content_length = 0;
    new_cptr->cur_hdr_value = NULL;
    
    if (add_select(new_cptr, new_cptr->conn_fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
      fprintf(stderr, "accept_connection: Error- Set Select failed on READ EVENT for CPE RFC \n");
      return -1;
    }
    return 0;

  } else {
    NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Invalid request type %d", cptr->url_num->request_type);
    return -1;
  }
/*
cancel_action:
  // DO we need some clean up
  
  close_fd(cptr, 1, now);
  FREE_AND_MAKE_NULL(rfc_url_num, "rfc_url_num for ACS cptr", -1);
  return -1;
*/
}

