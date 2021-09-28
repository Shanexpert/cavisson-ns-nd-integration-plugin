#include "ns_cache_include.h"
#include "ns_vuser_tasks.h"
#include "ns_page_think_time.h"
#include "ns_session.h"
#include "ns_vuser_ctx.h"
#include "ns_js.h"
#include "ns_click_script.h"
#include "ns_child_thread_util.h"
#include "ns_vuser_thread.h"
#include "ns_sync_point.h"
#include "ns_script_parse.h"
#include "ns_rbu_page_stat.h"
#include "ns_websocket_reporting.h"

#include "tr069/src/ns_tr069_acs_con.h"
#include "ns_rbu.h"
#include "ns_websocket.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_rbu_domain_stat.h"
#include "ns_jmeter.h"
#include "ns_sockjs.h"
#include "ns_xmpp.h"
#include "ns_socket.h"
#include "ns_rdp_api.h"   /*bug 79149*/


//int g_script_mode = NS_SCRIPT_MODE_0;

/* Notes:
     There will be only one task per user

*/

static VUser* task_head = NULL;
static VUser* task_tail = NULL;
unsigned int vut_task_overwrite_count = 0;
unsigned int vut_task_count = 0;

// This method will store the page information in the linked list
void vut_add_task(VUser *vptr, int operation)
{

  NSDL2_HTTP(vptr, NULL, "Method called. operation = %d", operation);
  if(vptr->operation != VUT_NO_TASK)
  {
    vut_task_overwrite_count++;
    NSTL1(vptr, NULL, "Adding task (%d) for a user which already has pending task (%d) total overwrite task(%u)",
                                             operation, vptr->operation, vut_task_overwrite_count);
    vptr->operation = operation;
    return; // We should not add this in the task linked list as this user is already in the list
  }

  vptr->operation = operation;

  /*User will not be paused in ramp down*/ 
  if(!(vptr->flags & NS_VUSER_RAMPING_DOWN))
  { 
    if(vptr->vuser_state == NS_VUSER_PAUSED || vptr->flags & NS_VUSER_PAUSE){
      
      if(vptr->flags & NS_VUSER_PAUSE) //User was in User Context , pause the VUser
        pause_vuser(vptr);
      
       if(operation != VUT_PAGE_THINK_TIME) // In case of Page Think Time we will add the task also
        return;
    }
  }
  vptr->task_next = NULL;
  vut_task_count++;

  if(task_head == NULL)
  {
    NSDL2_HTTP(vptr, NULL, "List is Empty...Adding the new task at the head");
    task_head = vptr;
  }
  else
  {
    task_tail->task_next = vptr;
  }
  task_tail = vptr;

  NSDL2_HTTP(vptr, NULL, "New Task Added in the List...Exiting Method. Total tasks in queue = %d", vut_task_count);
}

// Called from child main loop before epoll_wait()
// This method will execute all tasks of a all users  which are queued so far
// Note that this method may call other method which can queue more tasks. These
// tasks will be executed in the next call of this method

#define NS_CHECK_PAUSED(vptr)\
{\
  if(!(vptr->flags & NS_VUSER_RAMPING_DOWN) && vptr->vuser_state == NS_VUSER_PAUSED)\
  {\
    NSDL2_HTTP(NULL, NULL, "User is in paused state. Execute next task");\
    vptr->operation = operation;\
    vptr = vptr_next;\
    continue;\
  }\
}
void vut_execute(u_ns_ts_t now)
{
  VUser *vptr, *vptr_next;
  int task_count = vut_task_count;

  NSDL2_HTTP(NULL, NULL, "Method called. Total tasks in queue = %d, task_head = %p", vut_task_count, task_head);

  if(task_head == NULL)
    return;

  vptr = task_head;

  // Sine method may call other method which can queue more tasks, we make head/tail NULL
  // so that new tasks are queues using NULL head
  task_head = task_tail = NULL;
  vut_task_count = 0;

  while(task_count != 0)
  {
    NSDL2_HTTP(NULL, NULL, "vptr = %p, Total tasks in next queue = %d, task left for current queue = %d", vptr, vut_task_count, task_count);

    if(vptr == NULL)
    {
      NSTL1(NULL, NULL, "Error: vptr is NULL. Pending tasks = %d", task_count);
      return;
    }

    task_count--;
    vptr_next = vptr->task_next;
    vptr->task_next = NULL;  // Sat Sep 24 06:24:36 PDT 2011 Shakti
  
    // We need to save operation in a variable and then set vptr->operation to no task
    // This is required as execeution of task may add another task
    int operation = vptr->operation;
    
    vptr->operation = VUT_NO_TASK;

    TLS_SET_VPTR(vptr);
    // Get next node in the linked list
    if(operation == VUT_WEB_URL) 
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_HTTP(NULL, NULL, "Executing Task Web Url for page id = %d", vptr->next_pg_id);
      nsi_web_url(vptr, now);
    }
    // Get next node in the linked list
    else if(operation == VUT_RBU_WEB_URL)
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_HTTP(NULL, NULL, "Executing Task Web Url in RBU mode for page id = %d", vptr->next_pg_id);
      nsi_rbu_web_url(vptr, now);
    }  
    else if(operation == VUT_WS_SEND)
    {
      vptr->ws_status = NS_REQUEST_OK;
      NS_CHECK_PAUSED(vptr);
      NSDL2_WS(NULL, NULL, "Executing Task WebSocket send api for send id = %d", vptr->ws_send_id);
      if(nsi_websocket_send(vptr) == -1) 
      {
        vptr->ws_status = NS_REQUEST_WRITE_FAIL;
        ws_avgtime->ws_error_codes[NS_REQUEST_WRITE_FAIL]++;
        NSDL2_WS(NULL, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_WRITE_FAIL]);
        NSDL2_HTTP(NULL, NULL, "WebSocket Send request failed for send id %d", vptr->ws_send_id);
        if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) 
           switch_to_vuser_ctx(vptr, "WebSocket Send request failed");
        else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
           send_msg_to_njvm(vptr->mcctptr,NS_NJVM_API_WEB_WEBSOCKET_SEND_REP, -1);
      }
      NSDL2_HTTP(NULL, NULL, "Executing Task websocket send api for send id = %d, done", vptr->ws_send_id);
    }
    else if(operation == VUT_WS_CLOSE)
    {
      vptr->ws_status = NS_REQUEST_OK;
      NSDL2_HTTP(NULL, NULL, "Executing Task websocket close api for close id = %d", vptr->ws_close_id);
      NS_CHECK_PAUSED(vptr);
      if(nsi_websocket_close(vptr) == -1)
      {
        NSDL2_WS(NULL, NULL, "WebSocket Close request failed for close id %d", vptr->ws_close_id);
      }
    }
    else if(operation == VUT_CLICK_ACTION) // Click and Script APIs
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_HTTP(NULL, NULL, "Executing Task Click Action Url for page id = %d", vptr->next_pg_id);
      nsi_click_action(vptr, now);
    }   
    else if(operation == VUT_PAGE_THINK_TIME)
    {
      //Page Id does not have any relevance here as user would be in thinking state
      //NSDL2_HTTP(NULL, NULL, "Executing Page Think Time for Page_id = %d", vptr->next_pg_id);
      NSDL2_HTTP(NULL, NULL, "Executing Page Think Time");
      nsi_page_think_time(vptr, now);
      NSDL2_HTTP(NULL, NULL, "after Page Think Time");
    }
    else if(operation == VUT_END_SESSION)        //Also used when session is aborted on Page Failure
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_SCHEDULE(NULL, NULL, "Executing End session");
      // Note - This can add task for web url
      nsi_end_session(vptr, now); // in ns_session.c
    }
    else if(operation == VUT_TR069_WAIT_TIME)
    {
      NSDL2_HTTP(NULL, NULL, "Executing Tr069 wait time");
      nsi_tr069_wait_time(vptr, now); 
    }
    else if(operation == VUT_SYNC_POINT)
    {
      NSDL2_HTTP(NULL, NULL, "Executing sync point");
      nsi_send_sync_point_msg(vptr); 
    }
    else if(operation == VUT_NO_TASK)
    {
      print_core_events((char*)__FUNCTION__, __FILE__, "No Task for user.");
    }
    else if(operation == VUT_RBU_WEB_URL_END)
    {
      //Handle all close_connection
      NS_CHECK_PAUSED(vptr);
      NSDL2_HTTP(NULL, NULL, "Executing Task VUT_RBU_WEB_URL_END for page id = %d, is_incomplete_har_file = %d", 
                              vptr->next_pg_id, vptr->httpData->rbu_resp_attr->is_incomplete_har_file);
      
      //RBU processing of data on web url end will be done inside ns_rbu_handle_web_url_end()
      ns_rbu_handle_web_url_end(vptr, now);
    }   
    else if(operation == VUT_SWITCH_TO_NVM)
    {
      NSDL2_HTTP(NULL, NULL, "Switch to User Context,  page id = %d", vptr->next_pg_id);
      if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      {
        NSDL3_HTTP(vptr, NULL, "Switching to vuser context");
        switch_to_vuser_ctx(vptr, "SwitcToVUser");
      }
    }   
    else if(operation == VUT_SOCKJS_CLOSE)
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_HTTP(NULL, NULL, "Executing Task sockJs close api for close id = %d", vptr->sockjs_close_id);
      if(nsi_sockjs_close(vptr) == -1)
      {
        NSDL2_HTTP(NULL, NULL, "sockJs Close request failed for close id %d", vptr->sockjs_close_id);
        vptr->sockjs_status = NS_REQUEST_ERRMISC;
        switch_to_vuser_ctx(vptr, "SockJS Close request failed");
      }
    }
    else if(operation == VUT_XMPP_SEND)
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_XMPP(NULL, NULL, "Executing Task XMPP send api");
      vptr->xmpp_status = nsi_xmpp_send(vptr,now);
      do_xmpp_complete(vptr);
    }
    else if(operation == VUT_XMPP_LOGOUT)
    {
      NS_CHECK_PAUSED(vptr);
      NSDL2_XMPP(NULL, NULL, "Executing Task XMPP send api");
      vptr->xmpp_status = nsi_xmpp_logout(vptr,now);
      do_xmpp_complete(vptr);
    }
    else if(operation == VUT_SOCKET_SEND)
    {
      vptr->page_status = NS_REQUEST_OK;
      NS_CHECK_PAUSED(vptr);
      NSDL2_WS(NULL, NULL, "Executing Task Socket send api for send id = %d", vptr->next_pg_id);
      if(nsi_socket_send(vptr) == -1)
      {
        vptr->page_status = NS_REQUEST_WRITE_FAIL;
        if (IS_TCP_CLIENT_API_EXIST) 
        {
          fill_tcp_client_avg(vptr, NUM_SEND_FAILED, 0);
          fill_tcp_client_failure_avg(vptr, NS_REQUEST_WRITE_FAIL);
        }

        NSDL2_HTTP(NULL, NULL, "Socket Send request failed for send id %d", vptr->next_pg_id);
        if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
           switch_to_vuser_ctx(vptr, "Socket Send request failed");
        //else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
          // send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_SOCKET_SEND_REP, -1);
      }
    }
    else if(operation == VUT_SOCKET_READ)
    {
      //Every read will be treated as a fresh read, so resetting before reading data
      connection *cptr;
      NS_SOCKET_GET_CPTR(cptr, vptr);
      cptr->bytes = vptr->bytes = 0;
      cptr->body_offset = 0;   //Must set as we are using incase of DELIMITER read endpolicy

      if(url_resp_buff)
        url_resp_buff[0] = 0; 
    }
    else if(operation == VUT_SOCKET_CLOSE)
    {
      vptr->page_status = NS_REQUEST_OK;
      NSDL2_HTTP(NULL, NULL, "Executing Task socket close api for close id = %d", vptr->next_pg_id);
      NS_CHECK_PAUSED(vptr);
      if(nsi_socket_close(vptr) == -1)
      {
        vptr->page_status = NS_REQUEST_ERRMISC;

        if (IS_TCP_CLIENT_API_EXIST) 
          fill_tcp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);

        if (IS_UDP_CLIENT_API_EXIST)
          fill_udp_client_failure_avg(vptr, NS_REQUEST_ERRMISC);

        NSDL2_SOCKETS(NULL, NULL, "Socket Close request failed for close id %d with page_status = %d",
                      vptr->next_pg_id, vptr->page_status);

        if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
          switch_to_vuser_ctx(vptr, "SocketCloseError: nsi_socket_close()");
        else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
          send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_CLOSE_REP, -1);
        /*else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
          send_msg_to_njvm(vptr->mcctptr,NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP, -1);*/
      }
    }
    else if(operation == VUT_SOCKET_OPT)
    {
      NSDL2_WS(NULL, NULL, "Socket Operation done");
      vptr->page_status = NS_REQUEST_OK;
      if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
        switch_to_vuser_ctx(vptr, "Socket Operation Done");
    }
    else if(operation == VUT_RDP) /*bug 79149*/
    {
      NSDL3_RDP(vptr, NULL, "RDP operation = %d vptr = %p", VUT_RDP, vptr);
      NS_CHECK_PAUSED(vptr);
      vptr->first_page_url = vptr->cur_page->first_eurl; 
      NSDL3_RDP(vptr, NULL, "calling nsi_rdp_execute_ex. vptr->first_page_url = %p vptr[%p]->xwp = %p", vptr->first_page_url, vptr, vptr->xwp);
      nsi_rdp_execute_ex(vptr);
      NSDL3_RDP(vptr, NULL, " now vptr[%p]->xwp = %p", vptr, vptr->xwp);
    }
    else
    {
      print_core_events((char*)__FUNCTION__, __FILE__, "Error: Invalid task operation = %d\n", operation);
    }
    vptr = vptr_next;
    NSDL2_HTTP(NULL, NULL, "Method called. task_count = %d", task_count);
  }

  NSDL2_HTTP(NULL, NULL, "Task List Complete");
}

/* ns_click_action() - This is run in virtual user context */
int ns_click_action(VUser *vptr, int page_id, int clickaction_id)
{
  int ret;

  vptr->next_pg_id = page_id;
  vptr->httpData->clickaction_id = clickaction_id;
  vut_add_task(vptr, VUT_CLICK_ACTION);
  // We will move to nvm context in case of USER_CONTEXT only as for other types we are already in NVM context
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    switch_to_nvm_ctx(vptr, "ClickAction");
    ret = vptr->page_status;
    return ret;
  }
  return 0;
}

int post_ns_click_action(VUser *vptr)
{

  ClickActionTableEntry_Shr *ca;
  char *att[NUM_ATTRIBUTE_TYPES];
  int ret;
   /* Find the click action table entry for ca_idx */
  ca = &(clickaction_table_shr_mem[vptr->httpData->clickaction_id]);

  memset(att, 0, sizeof(att));
  read_attributes_array_from_ca_table(vptr, att, ca);

  /* RBU: In case of RBU script above task VUT_CKICK_ACTION will create dummy connection and fill required data structure i.e only setup work done.
     Page is not executed there so execute page here
  */
  NSDL2_RBU(NULL, NULL, "enable_rbu = %d, APINAME = %s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu, att[APINAME]);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
  {
    ContinueOnPageErrorTableEntry_Shr *ptr;
    ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];
    /* Shibani: resolve bug id 10613 - In click and script ns_web_url API is replaced by ns_browser API*/
    if(!strcmp(att[APINAME], "ns_browser"))
    {
      ret = ns_rbu_execute_page(vptr, vptr->next_pg_id);
      NSDL2_RBU(NULL, NULL, "ret status = %d", ret);
    }
    else
    {
      ret = ns_rbu_click_execute_page(vptr, vptr->next_pg_id, vptr->httpData->clickaction_id);
      // For Failure we have handled through macro, for success we will handle inside
      /*if (!strcmp(att[APINAME], "ns_get_num_domelement")) {
        ret = vptr->httpData->rbu_resp_attr->num_domelement;
      }
      else if (!strcmp(att[APINAME], "ns_execute_js")) {
        ret = vptr->httpData->rbu_resp_attr->executed_js_result;
      } */
      NSDL2_RBU(NULL, NULL, "ret status = %d", ret);
    }
     // We are freeing free_attributes_array here. 
    // This array is only used in ns_rbu_click_execute_page function where it was allocated separtely.
    free_attributes_array(att);
   }
  return(ret);
}
   
/* nsi_click_action() - This is run in nvm from vut_execute() */
void nsi_click_action(VUser *vptr, u_ns_ts_t now)
{
  int create_new_page; //Flag if click action results in new page
  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
  {
    create_new_page = handle_click_actions (vptr, vptr->next_pg_id, vptr->httpData->clickaction_id);
    if(create_new_page) free_dom_and_js_ctx(vptr);
  }
  else // RBU - Always hiting new page
    create_new_page = 1;

  if(create_new_page)
  {
    nsi_web_url(vptr, now);
  }
  else
  {
    if(vptr->sess_ptr->script_type != NS_SCRIPT_TYPE_JAVA)
      switch_to_vuser_ctx(vptr, "ClickActionOver");
  }
  post_ns_click_action(vptr);

}

inline void nsi_web_url_int(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_API(vptr, NULL, "Method called, group_num = %d, enable_rbu = %d, now = %ld", vptr->group_num, 
                         runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu, now);

  //Setup for page to be stated
  on_page_start(vptr, now);

  //Manish: if script is RBU then we need to set make dummy connection and set different variable 
  //Execute page
  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
    execute_page(vptr, vptr->next_pg_id, now);
  else
    ns_rbu_setup_for_execute_page(vptr, vptr->next_pg_id, now); 

  NSDL2_API(vptr, NULL, "Exiting Method. page_id = %d", vptr->next_pg_id);
}

void nsi_web_url(VUser *vptr, u_ns_ts_t now)
{   
  int next_pg_id = vptr->next_pg_id;
  int ret;

  vptr->cur_page = vptr->sess_ptr->first_page + next_pg_id;

  NSDL2_API(vptr, NULL, "Method called. page_id = %d, page_name = %s", next_pg_id, vptr->cur_page->page_name);
  NSDL2_API(vptr, NULL, "group_num = %d, enable_rbu = %d", vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu);

/*  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu && vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    vptr->page_instance ++; // incrementing page instance here . This will not affect value of page instance in case of web url .  
    NSDL2_API(vptr, NULL, "RBU is enabled with Java-type Script.");
    ns_rbu_setup_for_execute_page(vptr, vptr->next_pg_id, now);
    //send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_URL_REP, 0);
    return;
  } */
 
  CHECK_FOR_SYNC_POINT(vptr->cur_page);
  nsi_web_url_int(vptr, now);
}

int nsi_rbu_web_url(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_API(vptr, NULL, "Method called %p", vptr);
  nsi_web_url(vptr, now);
  int ret = ns_rbu_execute_page(vptr, vptr->next_pg_id);
  return ret;
}

// API called from script ns_web_url() will call this
// We are passing page_id as it will be called from script directly
int ns_web_url_ext(VUser *vptr, int page_id)
{
  NSDL2_API(vptr, NULL, "Method called. page_id = %d", page_id);

  //For all the pages in the script.c
  //Add to the end of linked list, information related to page 
  //which includes vptr, page_id, operation, think_time

  // Since vptr is in shared memory, it is accessible to NVM, User Context and separate thread

  // In Legacy Mode - Add task and return
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_LEGACY 
		|| 
    vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    vptr->next_pg_id = page_id;
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      vut_add_task(vptr, VUT_WEB_URL);
    else
      vut_add_task(vptr, VUT_RBU_WEB_URL);
  }
  // In User Context Mode, Add task, switch to nvm contxt
  //   wait for api to be complete
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vptr->next_pg_id = page_id;
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      vut_add_task(vptr, VUT_WEB_URL);
    else
      vut_add_task(vptr, VUT_RBU_WEB_URL);

    NSDL2_API(vptr, NULL, "Waiting for web url api to be over");

    switch_to_nvm_ctx(vptr, "WebUrlStart");
    NSDL2_API(vptr, NULL, "Task VUT_WEB_URL done for user %p", vptr);

    action_request_Shr *url_num = vptr->first_page_url;
    if( url_num->request_type == XMPP_REQUEST)
    {
      NSDL2_API(vptr, NULL, "XMPP Login done for user %p", vptr);
      xmpp_update_login_stats(vptr);
      return(vptr->xmpp_status);
    }
    return(vptr->page_status);
  }
  // In Separate Thread Mode, Send message to NVM and wait for reply
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_API(vptr, NULL, "Sending message to NVM and waiting for reply, page_id = %d", page_id);
    /*static int tlt_no_url = 0; fprintf(stderr, "total no url = %d\n", ++tlt_no_url);*/
    Ns_web_url_req web_url;
    web_url.opcode = NS_API_WEB_URL_REQ;
    web_url.page_id = page_id;
    int ret = vutd_send_msg_to_nvm(VUT_WEB_URL, (char *)(&web_url), sizeof(Ns_web_url_req));
    NSDL2_API(vptr, NULL, "take the response and take response page_id = %d", page_id);
    return (ret);
  }
  return 0;
}

