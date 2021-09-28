/************************************************************************************
 * File Name            : ns_connection_pool.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 20 November 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : In new design connections are kept in a connection pool 
 *                        which can be accessed by each virtual user.
 *                         - Contains all connection function 
 *                         - Allocate memory chunk for connections and assign 
 *                           connection whenever free connection slot required
 *                         - Dynamic allocation of connections
 *                         - Create link-list for saving free, reuse and in-use cptr 
 *                           list
 *                         - Maximum connection will vary with respect to browser 
 *                           settings, logic remains keyword driven  
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <execinfo.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "url.h"
#include "nslb_sock.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "poi.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_vuser.h"
#include "nslb_util.h"
#include "ns_url_resp.h"
#include "ns_connection_pool.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"


/*Global Connection pointer*/
connection* gFreeConnHead = NULL;
connection* gFreeConnTail = NULL;

connection* total_conn_list_head = NULL;
connection* total_conn_list_tail = NULL;

// TODO - Add these in ND Daignostics group
static unsigned long total_free_cptrs = 0;
static unsigned long total_allocated_cptrs = 0;


/* Function used to allocate connections dynamically
 * Allocate connections_chunk with size of INIT_CONN_BUFFER * size of connection structure
 * Link connection list and made last entry NULL
 * Updated connection structure members
 * Use global pointer to connect different connection chunks, need to save link list
 * Update gFreeConnTail to update connection pool with free 
 *
 * Returns: Head node of connection pool list
 * */
static connection* allocate_connection_pool()
{
  int i;
  connection* connections_chunk = NULL;
  timer_type *timer_chunk = NULL;
  timer_type *timer_ptr = NULL;

  NSDL2_CONN(NULL, NULL, "Method called. Sizeof connection structure = %d. Allocating connection pool %d connections and size of %d bytes", sizeof(connection), INIT_CONN_BUFFER, (INIT_CONN_BUFFER * sizeof(connection)));

  // Doing memset to make fiels 0/NULL as it will be faster than doing field by filed
  MY_MALLOC_AND_MEMSET(connections_chunk, (INIT_CONN_BUFFER * sizeof(connection)), "connections_chunk", -1);  

  // Not doing memset as only two fields need to be set to NULL
  MY_MALLOC(timer_chunk, INIT_CONN_BUFFER * sizeof(timer_type), "timer_chunk", -1);
  //Counters to keep record of free ans allocated connections
  total_free_cptrs += INIT_CONN_BUFFER;
  total_allocated_cptrs += INIT_CONN_BUFFER;
  NSDL2_CONN(NULL, NULL, "Total free connections: total_free_cptrs = %d, total allocated connections: total_allocated_cptrs = %d", total_free_cptrs, total_allocated_cptrs);

  timer_ptr = timer_chunk;

  for(i = 0; i <INIT_CONN_BUFFER; i++)
  {
    /* Linking connection entries within a pool and making last entry NULL*/
    if(i < (INIT_CONN_BUFFER - 1)) {
      connections_chunk[i].next_free = (struct connection *)&connections_chunk[i+1];
    }
    
    connections_chunk[i].con_type = NS_STRUCT_TYPE_CPTR;
    // connections_chunk[i].conn_state = CNST_FREE; commented as CNST_FREE is 0
    
    connections_chunk[i].timer_ptr = timer_ptr;
    timer_ptr++;
    connections_chunk[i].timer_ptr->timer_type = -1;

    /* following should be zero since it server for all protocols (SMTP/POP3);
     * marks the init state 
    connections_chunk[i].proto_state = 0; //ST_SMTP_INITIALIZATION;
    */

    /* Net Diagnostics intialize nd flow path instance */
    connections_chunk[i].nd_fp_instance = -1;
    connections_chunk[i].list_member = NS_ON_FREE_LIST; //Initially connection is residing in free list
    connections_chunk[i].ssl_cert_id = -1; 
    connections_chunk[i].ssl_key_id = -1; 
  }

  /* Connection pointer total_conn_list_head and total_conn_list_tail 
   * declared in netstorm.c
   * Used to connect different connection chunks, need to save link list*/
  if (total_conn_list_head == NULL)
    total_conn_list_head = total_conn_list_tail = connections_chunk;
  else {
    total_conn_list_tail->next_in_list = (struct connection*)connections_chunk;
    total_conn_list_tail = connections_chunk;
  }

  // total_conn_list_tail->next_in_list = NULL; commented as it is done in memset
  total_conn_list_tail->chunk_size = INIT_CONN_BUFFER;
  gFreeConnTail = &connections_chunk[INIT_CONN_BUFFER - 1];
  NSDL2_CONN(NULL, NULL, "connections_chunk = %p", connections_chunk);

  return(connections_chunk);
}

static inline void add_in_inuse_list(connection *free, VUser *vptr)
{
  NSDL2_CONN(vptr, free, "Method called. cptr = %p, vptr = %p, inuse list: Head_node = %p, Tail_node = %p", free, vptr, vptr->head_cinuse, vptr->tail_cinuse);
  
  if (vptr->head_cinuse) {
    //need to make linked list
    vptr->tail_cinuse->next_inuse = (struct connection *)free;
    free->prev_inuse = (struct connection *)vptr->tail_cinuse; // Prev is the current tail
    vptr->tail_cinuse = free;
  } else  {
    /*If NULL then both head and tail points to same cptr*/
    vptr->head_cinuse = vptr->tail_cinuse = (connection *)free;
    free->prev_inuse = NULL; // Prev is NULL as it head of the linked list
  }
  //When we malloc connection pool at that time connection pointer must be in free connection list, therefore we need to reset 
  if (free->list_member & NS_ON_FREE_LIST) {
    NSDL2_CONN(vptr, free, "Reset cptr pointer from NS_ON_FREE_LIST");
    free->list_member &= ~NS_ON_FREE_LIST; // Remove from free list
  }
  free->list_member |= NS_ON_INUSE_LIST; // Mark it in use list
  free->next_inuse = NULL;

  NSDL1_CONN(vptr, NULL, "Populate connection inuse list on vptr: head_node = %p, tail_node = %p, prev_node = %p, next_node = %p", vptr->head_cinuse, vptr->tail_cinuse, free->prev_inuse, free->next_inuse);
}

/*Function used to set Keep-ALive settings for each SGRP group*/
static inline void set_user_connection_ka(VUser *vptr, connection *cptr)
{
  NSDL2_CONN(vptr, cptr, "Group id %d, runprof_table_shr_mem[%d].gset.ka_pct = %d", 
              vptr->group_num, vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.ka_pct);

  /* set KA/MKA */
 
  // If ka_pct is 100 -> KA (no need to do random number
  // If ka_pct is 0 -> NKA (no need to do random number
  // Else check using random number
  
  if (((runprof_table_shr_mem[vptr->group_num].gset.ka_pct) != 0) && 
          (((runprof_table_shr_mem[vptr->group_num].gset.ka_pct) == 100) ||
                 ((rand() % 100) < (runprof_table_shr_mem[vptr->group_num].gset.ka_pct))))
  {
    cptr->connection_type = KA;
    cptr->num_ka =
        runprof_table_shr_mem[vptr->group_num].gset.num_ka_min +
        ((runprof_table_shr_mem[vptr->group_num].gset.num_ka_range > 0) ?
         (rand() % runprof_table_shr_mem[vptr->group_num].gset.num_ka_range):0);

    if (cptr->num_ka <= 0)
      cptr->num_ka = runprof_table_shr_mem[vptr->group_num].gset.num_ka_min +
        (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range)/2;
  } else {
    cptr->connection_type = NKA;
    cptr->num_ka = 0;
  }

  NSDL2_CONN(NULL, cptr, "grp id = %d, Setting cptr->connection_type = %d, cptr->num_ka = %d",
                vptr->group_num, cptr->connection_type, cptr->num_ka);
}

/* Function used to get free connection from connection pool
 * TASK:
 *      - If gFreeConnHead global pointer is FREE then we need 
 *      to allocate connection from pool else we update its 
 *      value with next connection in list
 *      - Updated Keep-Alive settings per SGRP 
 *      - Set vptr pointer in connection struct
 *      - Maintain in-use connection list, these connections are 
 *      neither close nor in reuse state
 *
 * */
inline connection*
get_free_connection_slot(VUser *vptr)
{
  connection *free;
 
  NSDL2_CONN(vptr, NULL, "Method called. vptr = %p", vptr);

  
  /* If gFreeVuserHead is NULL then realloc connection pool otherwise draw connection
   * from existing pool*/
  if (gFreeConnHead == NULL) 
  {
    NSDL1_CONN(vptr, NULL, "Allocating connection pool");
    gFreeConnHead = allocate_connection_pool();
    if(gFreeConnHead == NULL) // If we are not able to allocate connection pool
    {
      fprintf(stderr, "Connection allocation failed\n");
      end_test_run();
    }
  }

  //Update free counter for connection slot
  total_free_cptrs--;
  NSDL2_CONN(NULL, NULL, "Updated free connection counter: total_free_cptrs = %d, total_allocated_cptrs = %d", total_free_cptrs, total_allocated_cptrs);

  /* Connection Pool Design: Traverse connection list and update gFreeConnHead and update
   * vptr pointer of connection struct*/
  free = gFreeConnHead;
  gFreeConnHead = (connection*)free->next_free; // Move free head to next cptr on pool

  if(gFreeConnHead == NULL) // If we took the last cptr of the pool, then both head and tail need to be set to NULL
  {
    NSDL1_CONN(vptr, NULL, "Last connection fetch from pool, then both head and tail need to be set to NULL");
    gFreeConnTail = NULL;
  }

  NSDL1_CONN(vptr, NULL, "Next connection = %p", (connection*)free->next_free);

  free->next_free = NULL; // Set next_free to NULL. May not be important ??
  free->request_type = -1;

  //Set vptr ...
  free->vptr = vptr; // Set vptr to which this connection pointer belongs

  set_user_connection_ka(vptr, free); /* Set value of KA */

  SET_HTTP_VERSION()
  NSDL2_HTTP2(NULL, NULL, "HTTP mode for in cptr is [%d] ", free->http_protocol);

  // Since this cptr will be used for making connection, it need to be put in the in-use connection list
  // In use is doubly linked list, so both prev and next need to be set
  add_in_inuse_list(free, vptr);

  NSDL1_CONN(vptr, NULL, "Exiting get_free_connection_slot %p",free);
  return free;
}

static inline void
remove_from_glb_reuse_list(connection* cptr, VUser *vptr) 
{
  connection* cur_next, *cur_prev;
  NSDL2_CONN(vptr, cptr, "Method called: cptr=%p", cptr);
 
  // Remove from global reuse list
  cur_next = (connection *)cptr->next_reuse;
  cur_prev = (connection *)cptr->prev_reuse;
  NSDL2_CONN(vptr, cptr, "Global reuse list: previous connection = %p and next connection in list = %p", cur_prev, cur_next);
 
  if (cur_next)
    cur_next->prev_reuse = (struct connection *)cur_prev;
  else
    vptr->tail_creuse = cur_prev;

  if (cur_prev)
    cur_prev->next_reuse = (struct connection *)cur_next;
  else
    vptr->head_creuse = cur_next;

  cptr->list_member &= ~NS_ON_GLB_REUSE_LIST;
  NSDL2_CONN(vptr, cptr, "Updated VUser connection list: head_node = %p, tail_node = %p", vptr->head_creuse, vptr->tail_creuse);
}

static inline void remove_from_svr_reuse_list(connection* cptr, VUser* vptr)
{
  HostSvrEntry *hptr;
  connection* cur_next, *cur_prev;

  NSDL2_CONN(NULL, cptr, "Method called: cptr=%p", cptr);

  hptr = vptr->hptr + cptr->gServerTable_idx;
  NSDL2_CONN(NULL, cptr, "HostServerEntry node =%p, gServerTable_idx = %d", hptr, cptr->gServerTable_idx);

  cur_next = (connection *)cptr->next_svr;
  cur_prev = (connection *)cptr->prev_svr;
  NSDL2_CONN(vptr, cptr, "Server reuse list: previous connection = %p and next connection in list = %p", cur_prev, cur_next);

  if (cur_next)
    cur_next->prev_svr = (struct connection *)cur_prev;
  else
    hptr->svr_con_tail = cur_prev;

  if (cur_prev)
    cur_prev->next_svr = (struct connection *)cur_next;
  else
    hptr->svr_con_head = cur_next;

  cptr->list_member &= ~NS_ON_SVR_REUSE_LIST;
  NSDL2_CONN(vptr, cptr, "Updated HostSvrEntry list: head_node = %p, tail_node = %p", hptr->svr_con_head, hptr->svr_con_tail);
}

static inline void
remove_from_inuse_list(connection* cptr, VUser* vptr)
{
  connection* cur_next, *cur_prev;
  NSDL2_CONN(vptr, cptr, "Method called: cptr=%p, vptr = %p", cptr, vptr);

  // Remove from vuser inuse list
  cur_next = (connection *)cptr->next_inuse;
  cur_prev = (connection *)cptr->prev_inuse;
  NSDL2_CONN(vptr, cptr, "Inuse connection list: previous connection = %p and next connection in list = %p", cur_prev, cur_next); 

  if (cur_next)
    cur_next->prev_inuse = (struct connection *)cur_prev;
  else
    vptr->tail_cinuse = cur_prev;

  if (cur_prev)
    cur_prev->next_inuse = (struct connection *)cur_next;
  else
    vptr->head_cinuse = cur_next;

  cptr->list_member &= ~NS_ON_INUSE_LIST;
  NSDL2_CONN(vptr, cptr, "Updated VUser inuse connection list: head_node = %p, tail_node = %p", vptr->head_cinuse, vptr->tail_cinuse);
}

//remove an specified element from host server vuser reuse list
static inline void remove_ifon_svr_reuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called");

  if (!(cptr->list_member & NS_ON_SVR_REUSE_LIST)) {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on Server reuse list of the Vuser.", cptr);
  }
  else {
    NSDL2_CONN(vptr, cptr, "Removing cptr from server reuse list");
    remove_from_svr_reuse_list(cptr, vptr);
  }
  NSDL1_CONN(vptr, cptr, "Exiting remove_ifon_svr_reuse_list");
}

//remove an specified element from vuser reuse list
static inline void remove_ifon_glb_reuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called");

  if (!(cptr->list_member & NS_ON_GLB_REUSE_LIST)) {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on Global reuse list of the Vuser.", cptr);
  } 
  else {
    NSDL2_CONN(vptr, cptr, "Removing cptr from global reuse link list");
    remove_from_glb_reuse_list(cptr, vptr);
  }
  NSDL1_CONN(vptr, cptr, "Exiting remove_ifon_glb_reuse_list");
}

//remove an specified element from vuser inuse list  
static inline void remove_ifon_inuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called, cptr = %p, vptr = %p", cptr, vptr);

  if (!(cptr->list_member & NS_ON_INUSE_LIST)) {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on connection inuse list of the Vuser.", cptr);
  } 
  else {
    NSDL2_CONN(vptr, cptr, "Removing cptr from inuse connection link list");
    remove_from_inuse_list(cptr, vptr);
  }
  NSDL1_CONN(vptr, cptr, "Exiting remove_ifon_inuse_list");
}

void
log_stack_trace ()
{
  void *array[100];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 100);
  strings = backtrace_symbols (array, size);

  for (i = 0; i < size; i++)
    NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__,
                   "Trace for method [%d]: [%s]", i, strings[i]);
     //fprintf(stderr, "print_trace: %s\n", strings[i]);

  free (strings);
}

/*Function used to free connections slot*/
inline void
free_connection_slot(connection* cptr, u_ns_ts_t now)
{
  static long long cptr_url_free_count = 0; //This is for debug only.

  NSDL2_CONN(NULL, cptr, "Method called. cptr=%p at %u", cptr, now);
  

  // 7 Jan 2017: This check is added as a safetynet if free_connection_slot is called with some timer, Althouth we have fixed in 
  // retry_connection. #BugId: 22851
  if (cptr->timer_ptr->timer_type >= 0){
    char buffer[MAX_LINE_LENGTH]; 
    log_stack_trace();
    NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__,
                   "free_connection_slot() called for connection (%p) with timer %d . Ignored. cptr  = %s", 
					cptr, cptr->timer_ptr->timer_type, cptr_to_string(cptr, buffer, MAX_LINE_LENGTH));
    dis_timer_del(cptr->timer_ptr);
  }


  if (cptr->list_member & NS_ON_FREE_LIST)
  {
    NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__,
                   "free_connection_slot() called for connection (%p) which is already in connection free list. Ignored ", cptr);
    return;
  }  
  //Update free counter of connection slot
  total_free_cptrs++;
  NSDL2_CONN(NULL, NULL, "Increment free connection counter: total_free_cptrs = %d, total_allocated_cptrs = %d", total_free_cptrs, total_allocated_cptrs);

  // Remove from in global reuse list if ..
  remove_ifon_glb_reuse_list(cptr);
  // Remove from in server reuse list if ..
  remove_ifon_svr_reuse_list(cptr);
  // Remove from in use list if in inuse list
  remove_ifon_inuse_list(cptr);
  /*add free cptr at head of connection pool*/
  NSDL2_CONN(NULL, cptr, "Last connection entry, gFreeConnTail = %p", gFreeConnTail);

  if(gFreeConnTail)
  {
    gFreeConnTail->next_free = (struct connection *)cptr;
  } else {
    NSDL2_CONN(NULL, cptr, "Something went wrong while freeing connection pool\n");
    gFreeConnHead = cptr;
  }

  gFreeConnTail = (connection *)cptr;
  cptr->next_free = NULL;
  cptr->list_member = NS_ON_FREE_LIST; //While freeing the connection pointer we need to set its flag to FREE_LIST
  cptr->nd_fp_instance = -1;
  cptr->conn_state = CNST_FREE;
  cptr->request_type = -1;
  cptr->flags = 0;
  cptr->conn_fd = -1;
  //TODO Free conn_link of DNS connection.
  cptr->conn_link = NULL;

  if(cptr->url)
  {
    FREE_CPTR_URL(cptr);
    cptr_url_free_count++;
  }
  cptr->url_num = NULL;
  // Free HTTP2 data for cptr
  if (cptr->http_protocol == HTTP_MODE_HTTP2) //TODO:ADD MACRO For HTTP2
    free_http2_data(cptr);
  NSDL2_CONN(NULL, NULL, "Free Connection available, gFreeConnTail =%p, cptr_url_free_count = %lld", gFreeConnTail, cptr_url_free_count);
}

//remove an specified element from global vuser resue list
static inline void
check_and_remove_from_inuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called: cptr=%p, vptr = %p", cptr, vptr);

  // Check to make sure it is in global reuse list
  if (!(cptr->list_member & NS_ON_INUSE_LIST)) {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on Connection inuse list of the Vuser", cptr);
    print_core_events((char*)__FUNCTION__, __FILE__,
                      "Connection slot %p is not on Connection inuse list of the Vuser", cptr);
  }
  else
  {
    NSDL1_CONN(vptr, cptr, "Remove cptr from inuse connection list");
    remove_from_inuse_list(cptr, vptr);
  }

  NSDL1_CONN(vptr, cptr, "Exiting check_and_remove_from_inuse_list.");
}

//remove an element from global vuser list
//add at the tail of per host_server list and
//also at the tail of global list of this vuser
inline void
add_to_reuse_list(connection* cptr)
{
  HostSvrEntry *hptr;
  VUser *vptr;
  connection *cur_tail;
  action_request_Shr *parent_url_num = (cptr->url_num)?cptr->url_num->parent_url_num:NULL;

  NSDL2_CONN(NULL, cptr, "Method called, cptr=%p parent_url_num = %p url_num = %p", cptr, parent_url_num, cptr->url_num);

  vptr = cptr->vptr;

  // Step1 - Remove from in use list
  NSDL2_CONN(NULL, cptr, "Search and remove cptr in inuse connection link list");

  check_and_remove_from_inuse_list(cptr);
  /*bug 54315: reset vptr->last_cptr to NULL*/
  if((cptr == vptr->last_cptr) && (HTTP_MODE_HTTP2 == cptr->http_protocol))
  {
    vptr->last_cptr = NULL;
  }
  NSDL2_CONN(NULL, cptr, "vptr->last_cptr=%p", vptr->last_cptr);
  // Step2 - Add to global reuse list of the vuser
  NSDL2_CONN(NULL, cptr, "Add cptr in reuse connection link list, VUser: head_node = %p, tail_node = %p", vptr->head_creuse, vptr->tail_creuse);
  //We are freeing parameterized urls only bcz it was allocted dynamically.
  //Bug 64306 - NC_MON | Netstorm core is getting formed in add_to_reuse_list method
  FREE_CPTR_PARAM_URL(cptr);
  if(parent_url_num)
  {
    cptr->url_num = parent_url_num;
  }
  // TODO - Add check if in global reuse list or not.
  if ((cptr->list_member & NS_ON_GLB_REUSE_LIST)) {
    print_core_events((char*)__FUNCTION__, __FILE__, "Connection slot %p is on Global reuse list of the Vuser.", cptr);
  }
  cur_tail = vptr->tail_creuse;
  if (cur_tail)
    cur_tail->next_reuse = (struct connection *)cptr;
  else
    vptr->head_creuse = cptr;

  cptr->prev_reuse = (struct connection *)cur_tail;
  cptr->next_reuse = NULL;  //Always goin on tail so always do NULL
  vptr->tail_creuse = cptr;

  cptr->list_member |= NS_ON_GLB_REUSE_LIST;
  NSDL2_CONN(NULL, cptr, "Populate reuse connection link list on vptr: head_node = %p, tail_node = %p", vptr->head_creuse, vptr->tail_creuse);
  NSDL2_CONN(NULL, cptr, "Populate reuse connection link list on cptr: prev_reuse_node = %p, next_reuse_node = %p", cptr->prev_reuse, cptr->next_reuse);


  // Step3 - Add to host_server specific reuse list
  
  //hptr = vptr->hptr + get_svr_ptr(&(cptr->url_num->proto.http), vptr)->idx;
  hptr = vptr->hptr + cptr->gServerTable_idx;
  NSDL2_CONN(NULL, cptr, "Add cptr in server link list,HostServerEntry: hptr = %p,vptr->hptr = %p, cptr->gServerTable_idx = %d", hptr, vptr->hptr, cptr->gServerTable_idx);
  /* Code below is commented since cptr->url_num might have been freed in case of
   * redirection. which can cause Segmentation Faults. */
  /* Save the index also so that we can use it. This helps in the case */
  /* of auto fetch embedded - when we have freed the contents of hptr */
  /* cptr->gServerTable_idx = get_svr_ptr(&(cptr->url_num->proto.http), vptr)->idx; */

  // TODO - Add check if in server reuse list or not.
  if ((cptr->list_member & NS_ON_SVR_REUSE_LIST)) {
    print_core_events((char*)__FUNCTION__, __FILE__, "Connection slot %p is on Server reuse list of the Vuser.", cptr);
  }

  cur_tail = hptr->svr_con_tail;
  if (cur_tail)
    cur_tail->next_svr = (struct connection*)cptr;
  else
    hptr->svr_con_head = cptr;

  cptr->prev_svr = (struct connection *)cur_tail;
  cptr->next_svr = NULL;
  hptr->svr_con_tail = cptr;
  cptr->list_member |= NS_ON_SVR_REUSE_LIST;
  NSDL2_CONN(NULL, cptr, "Populate hptr link list on vptr: head_node = %p, tail_node = %p", hptr->svr_con_head, hptr->svr_con_tail);
  NSDL2_CONN(NULL, cptr, "Populate server connection link list on cptr: prev_svr_node = %p, next_svr_node = %p", cptr->prev_svr, cptr->next_svr);

  NSDL1_CONN(NULL, cptr, "Exiting add_to_reuse_list");
}

//remove an specified element from global vuser resue list
static inline void
check_and_remove_from_glb_reuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called: cptr=%p", cptr);

  // Check to make sure it is in global reuse list
  if (!(cptr->list_member & NS_ON_GLB_REUSE_LIST)) {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on Global reuse list of the Vuser", cptr);
    print_core_events((char*)__FUNCTION__, __FILE__,
                      "Connection slot %p is not on Global reuse list of the Vuser", cptr);
    // return; // Commented and add else to make this method inline
  }
  else
  {
    NSDL1_CONN(vptr, cptr, "Remove cptr from global reuse list");
    remove_from_glb_reuse_list(cptr, vptr);
  }

  NSDL1_CONN(vptr, cptr, "Exiting check_and_remove_from_glb_reuse_list.");
}

//remove an specified element from hosr server vuser resue list
static inline void check_and_remove_from_svr_reuse_list(connection* cptr)
{
  VUser *vptr = cptr->vptr;

  NSDL2_CONN(vptr, cptr, "Method called: cptr=%p, vptr = %p", cptr, vptr);

  if (!(cptr->list_member & NS_ON_SVR_REUSE_LIST)) 
  {
    NSDL1_CONN(vptr, cptr,
                      "Connection slot %p is not on Server reuse list of the Vuser.", cptr);
    //print_core_events((char*)__FUNCTION__, __FILE__,
    //                  "Connection slot %p is not on Server reuse list of the Vuser.", cptr);
    //return;
  }
  else
  {
    NSDL2_CONN(vptr, cptr, "Removing connection from server reuse list");
    remove_from_svr_reuse_list(cptr, vptr);
  } 
  NSDL1_CONN(vptr, cptr, "Exiting check_and_remove_from_svr_reuse_list");
}

//remove an element from server host reuse list
//also remove from global reuse list
//removes from the head of server host list
inline connection*
remove_head_svr_reuse_list(VUser *vptr, HostSvrEntry* svr_ptr)
{
  connection* cptr;

  NSDL2_CONN(vptr, NULL, "Method called for svr=%p", svr_ptr);

  cptr = svr_ptr->svr_con_head;

  if (cptr == NULL) {
      NSDL1_CONN(vptr, NULL, "There is no element to remove from head of svr reuse list svr=%p", svr_ptr);
    return NULL;
  }

  check_and_remove_from_glb_reuse_list(cptr);
  check_and_remove_from_svr_reuse_list(cptr);
  add_in_inuse_list(cptr, vptr);

  NSDL1_CONN(vptr, NULL, "Exiting from method, cptr = %p", cptr);

  return cptr;
}

//remove an element from global vuser list
//also remove from host server reuse list
//removes from the head of global list
inline connection *
remove_head_glb_reuse_list(VUser *vptr)
{
  connection* cptr;

  NSDL2_CONN(vptr, NULL, "Method called");

  cptr = vptr->head_creuse;
  if (cptr == NULL) {
    NSDL1_CONN(vptr, NULL, "There is no element to remove from head of glb reuse list");
    return NULL;
  }

  check_and_remove_from_glb_reuse_list(cptr);
  check_and_remove_from_svr_reuse_list(cptr);
  add_in_inuse_list(cptr, vptr);

  NSDL1_CONN(vptr, NULL, "Exiting remove_head_glb_reuse_list, cptr = %p", cptr);

  return cptr;
}

//Function used to remove connection entries from global resuse and sever list on vuser
inline connection *
remove_from_all_reuse_list(connection *cptr)
{

  NSDL2_CONN(NULL, cptr, "Method called");

  check_and_remove_from_glb_reuse_list(cptr);
  check_and_remove_from_svr_reuse_list(cptr);

  NSDL1_CONN(NULL, cptr, "Exiting remove_head_glb_reuse_list, cptr = %p", cptr);

  return cptr;
}

int validate_browser_connection_values(int max_con_per_svr_http1_0, int max_con_per_svr_http1_1, int max_proxy_per_svr_http1_0, int max_proxy_per_svr_http1_1, int max_con_per_vuser, char *browser_name)
{
  NSDL4_PARSING(NULL, NULL, " Method called. max_con_per_svr_http1_0 = %d, max_con_per_svr_http1_1 = %d, max_proxy_per_svr_http1_0 = %d, max_proxy_per_svr_http1_1 = %d, max_con_per_vuser = %d, browser_name = %s", max_con_per_svr_http1_0, max_con_per_svr_http1_1, max_proxy_per_svr_http1_0, max_proxy_per_svr_http1_1, max_con_per_vuser, browser_name);

  //Maximum connection per server or proxy connection per server shouldnt be greater than max_con_per_vuser
  if (max_con_per_svr_http1_0 > max_con_per_vuser) 
  {
    NSDL2_PARSING(NULL, NULL, "Invalid settings, maximum connection per server cannot be greater than max con per vuser");
    return -1; 
  } 
  else if (max_con_per_svr_http1_1 > max_con_per_vuser) 
  {
    NSDL2_PARSING(NULL, NULL, "Invalid settings, maximum connection per server cannot be greater than max con per vuser");
    return -2;
  }
  else if (max_proxy_per_svr_http1_0 > max_con_per_vuser) 
  {
    NSDL2_PARSING(NULL, NULL, "Invalid settings, maximum proxy per server cannot be greater than max con per vuser");
    return -1;
  }
  else if (max_proxy_per_svr_http1_1 > max_con_per_vuser) 
  {
    NSDL2_PARSING(NULL, NULL, "Invalid settings, maximum proxy per server cannot be greater than max con per vuser");
    return -2;
  }
  return 0; 
}

/* Keyword parsing: G_MAX_CON_PER_VUSER <group> <mode> <max_con_per_svr_http_1.0> <max_con_per_svr_http_1.1> <max_proxy_per_svr_http_1.0> <max_proxy_per_svr_http_1.1> <max_con_per_vuser>
 * */
int kw_set_g_max_con_per_vuser(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag) 
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];
  char max_con_per_server_http1_0[MAX_DATA_LINE_LENGTH];
  char max_con_per_server_http1_1[MAX_DATA_LINE_LENGTH];
  char max_proxy_per_server_http1_0[MAX_DATA_LINE_LENGTH];
  char max_proxy_per_server_http1_1[MAX_DATA_LINE_LENGTH];
  char max_conn_per_vuser[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int status, num, conn_mode, max_con_per_svr_http_1_0, max_con_per_svr_http_1_1, max_proxy_per_http_1_0, max_proxy_per_http_1_1, max_con_per_usr;
  //Adding default value
  mode[0] = 0;
  max_con_per_server_http1_0[0] = 8;
  max_con_per_server_http1_1[0] = 8;
  max_proxy_per_server_http1_0[0] = 8;
  max_proxy_per_server_http1_1[0] = 8;
  max_conn_per_vuser[0] = 8;            

  NSDL4_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  //Check number of arguments
  num = sscanf(buf, "%s %s %s %s %s %s %s %s %s", keyword, sgrp_name, mode, max_con_per_server_http1_0, max_con_per_server_http1_1, max_proxy_per_server_http1_0, max_proxy_per_server_http1_1, max_conn_per_vuser, tmp);

  //Validation check, arugements must be numeric
  if (ns_is_numeric(mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
  }
  conn_mode = atoi(mode);
  //Connection mode should be either 0 or 1
  if ((conn_mode < 0) || (conn_mode > 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_3);
  }
  gset->max_con_mode = conn_mode;
  //If connection mode is 0 then number of arguments should be 8 
  if (gset->max_con_mode == 0)
  {   
    if (num != 8) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_1);
    }
  } else { //For mode 1 number of arguments must be 3
    if(num != 3)
    {
      NSDL2_PARSING(NULL, NULL, "Error: In mode 1 user is not suppose to provide other fields");
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_1);
    } 
  }  

  /* If mode is equal to 0 then we need to set values as per keyword*/  
  if ((gset->max_con_mode == 0))
  {
    NSDL2_PARSING(NULL, NULL, "Setting default value for connection settings", buf);
    //Validations for max_con_per_server_http1_0
    if (ns_is_numeric(max_con_per_server_http1_0) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
    } 
    max_con_per_svr_http_1_0 = atoi(max_con_per_server_http1_0);
    //max_con_per_server_http1_0 should be greater than 0
    if ((max_con_per_svr_http_1_0 <= 0) || (max_con_per_svr_http_1_0 > 1024)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011036, "");
    }
    gset->max_con_per_svr_http1_0 = max_con_per_svr_http_1_0;
    
    //Validations for max_con_per_server_http1_1
    if (ns_is_numeric(max_con_per_server_http1_1) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
    }
    max_con_per_svr_http_1_1 = atoi(max_con_per_server_http1_1);
    //max_con_per_server_http1_0 should be greater than 0
    if ((max_con_per_svr_http_1_1 <= 0) || (max_con_per_svr_http_1_1 > 1024)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011036, "");
    }
    gset->max_con_per_svr_http1_1 = max_con_per_svr_http_1_1;
    
    //Validations for max_proxy_per_server_http1_0
    if (ns_is_numeric(max_proxy_per_server_http1_0) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
    }
    max_proxy_per_http_1_0 = atoi(max_proxy_per_server_http1_0);
    //max_con_per_server_http1_0 should be greater than 0
    if ((max_proxy_per_http_1_0 <= 0) || (max_proxy_per_http_1_0 > 1024)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011036, "");
    }
    
    gset->max_proxy_per_svr_http1_0 = max_proxy_per_http_1_0;

    //Validations for max_proxy_per_server_http1_1
    if (ns_is_numeric(max_proxy_per_server_http1_1) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
    }
    max_proxy_per_http_1_1 = atoi(max_proxy_per_server_http1_1);
    //max_con_per_server_http1_1 should be greater than 0
    if ((max_proxy_per_http_1_1 <= 0)|| (max_proxy_per_http_1_1 > 1024)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011036, "");
    }
    gset->max_proxy_per_svr_http1_1 = max_proxy_per_http_1_1;
    
    //validation for maximum connection per user
    if (ns_is_numeric(max_conn_per_vuser) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011035, CAV_ERR_MSG_2);
    }
    max_con_per_usr = atoi(max_conn_per_vuser);
    //max_con_per_vuser should be greater than 0
    if ((max_con_per_usr <= 0) || (max_con_per_usr > 1024)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011036,"");
    }
    gset->max_con_per_vuser = max_con_per_usr;

    //Maximum connection per server and proxy connection per server shouldnt be greater than max_con_per_vuser
    status = validate_browser_connection_values(gset->max_con_per_svr_http1_0, gset->max_con_per_svr_http1_1, gset->max_proxy_per_svr_http1_0, gset->max_proxy_per_svr_http1_1, gset->max_con_per_vuser, "keyword G_MAX_CON_PER_VUSER");
 
    if (status == -1)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011297, "keyword G_MAX_CON_PER_VUSER", gset->max_proxy_per_svr_http1_0, gset->max_con_per_vuser);
    }
    else 
     if (status == -2)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_CON_PER_VUSER_USAGE, CAV_ERR_1011298, "keyword G_MAX_CON_PER_VUSER", gset->max_proxy_per_svr_http1_1, gset->max_con_per_vuser);
      }
  } 
  else 
  {
    NSDL2_PARSING(NULL, NULL, "Need to provide browser settings");
    //Set using browser keyword
  } 
  return 0;
}
