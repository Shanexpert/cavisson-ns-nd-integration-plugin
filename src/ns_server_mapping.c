#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "init_cav.h"
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
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_cookie.h"
#include <curl/curl.h>
#include "decomp.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_user_monitor.h"
#include "ns_gdf.h"
#include "ns_alloc.h" 
#include "ns_event_log.h"
#include "ns_trans.h"
#include "nslb_time_stamp.h"
#include "ipmgmt_utils.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "nslb_sock.h"
#include "logging.h"
#include "ns_trans.h"
#include "ns_page_think_time.h"
#include "ns_replay_access_logs.h"
#include "ns_auto_fetch_embd.h"
#include "ns_url_req.h"
#include "ns_string.h"
#include "ns_server_mapping.h"
#include "ns_sock_com.h"


#if 0
static PerHostSvrTableEntry_Shr* find_actual_server_shr(char* serv, int grp_idx, int host_idx)
{
  int s;

  NSDL4_HTTP(NULL, NULL, "Method called - serv %s grp_idx %d host_idx %d", serv, grp_idx, host_idx);
 
  PerGrpHostTableEntry_Shr *grp_host_ptr;

  FIND_GRP_HOST_ENTRY_SHR(grp_idx, host_idx, grp_host_ptr); 
  if(!grp_host_ptr)
  {
    NSDL2_HTTP(NULL, NULL, "grp_host_ptr is NULL");
    return NULL;
  }

  PerHostSvrTableEntry_Shr *svr_table_ptr = grp_host_ptr->server_table;

  if(!serv)
    return(&svr_table_ptr[0]);   

  for(s = 0; s < grp_host_ptr->total_act_svr_entries; s++) 
  {
      if(!strcmp(svr_table_ptr[s].server_name, serv))
       return(&svr_table_ptr[s]);
  }
  return NULL;
}
#endif

/* 
 * function to save host URL and port at a given redirection depth
 * into a user defined variable in the vptr
 * inputs 
 * none
 * outputs
 * none
 * called from - connection_close()
 * Inputs 
 * cptr
 * Outputs
 * errors
 * Algo
 *
 */
inline void save_current_url (connection *cptr)
{
  //char url_extract[MAX_LINE_LENGTH];
  char *hostname;
  //char request_buf[MAX_LINE_LENGTH];
  VUser* vptr = cptr->vptr;

  NSDL4_HTTP(vptr, cptr, "Method called");

    /* save redirected host and port if the user wants to change the recorded
    * server to this one for a certain page and thereafter, at this
    * redirection depth
     */
  hostname = cptr->url_num->index.svr_ptr->server_hostname;
#if 1
    if (vptr->svr_map_change) {
      NSDL2_HTTP(vptr, cptr, "svr_map_change %p svr_map_change->flag %d change_on_depth %d vptr->redirect_count %d, hostname %s",
          vptr->svr_map_change,vptr->svr_map_change->flag,
          vptr->svr_map_change->change_on_depth,vptr->redirect_count, hostname);
    }
#endif
    if ( !(vptr->svr_map_change && vptr->svr_map_change->flag && (vptr->svr_map_change->change_on_depth
          == vptr->redirect_count) )) {
      NSDL4_HTTP(vptr, cptr, "Returning svr_map_change = %p," 
                             "flag = %d, change_on_depth = %d, redirect_count = %d",
                             vptr->svr_map_change, vptr->svr_map_change?vptr->svr_map_change->flag:999,
                             vptr->svr_map_change?vptr->svr_map_change->change_on_depth:999, 
                             vptr->redirect_count);
      return;
    }
 
  vptr->svr_map_change->port = cptr->url_num->index.svr_ptr->server_port;

  NSDL2_HTTP(vptr, cptr, "host = %s, port = %d", hostname, vptr->svr_map_change->port);

  //assign cur_vptr here for use in this api
  //cur_vptr = vptr;
  TLS_SET_VPTR(vptr);
  ns_save_string(hostname, vptr->svr_map_change->var_name);
  NSDL2_HTTP(vptr, cptr,"saved %s in var %s",hostname, vptr->svr_map_change->var_name);

  /* reset to 0 to avoid getting here more than once
   * NOTE: set this to 0 at the end of the page too to avoid saving the
   *  hostname/URL in another page if we dont succeed in the page where the the
   *  redirection happens
   * is called 
   */

  vptr->svr_map_change->flag = 0;
  return;
}

/* 
 * User api to save the redirected hostname at given depth into a var "var"
 *
 * inputs
 * type -- not used currently
 * depth
 * redirection depth at which the hostname should be saved
 * var
 * var name in which hostname is saved (should have been declared earlier by
 * calling ns_decl_var(var)
 * vptr 
 * VUser pointer
 * outputs
 * none
 * errors
 * none
 *
 */
int ns_setup_save_url_ext(int type, int depth, char *var,  VUser *vptr)
{
  int len =0, old_len =0;
  char * tmp_var_ptr;

  NSDL4_HTTP(NULL, NULL, "Method called: type %d depth %d var %s", type, depth, var);
  //allocate memory if not allocated yet
  if (!vptr->svr_map_change) { 
    MY_MALLOC(vptr->svr_map_change, sizeof(ServerMapChange), "svr_map_change", -1);
    vptr->svr_map_change->var_name = NULL;
  }
  // initialize fields
  vptr->svr_map_change->flag = 1;
  vptr->svr_map_change->change_on_depth = 0;
  vptr->svr_map_change->port = 0;
  len = strlen(var);
  tmp_var_ptr = vptr->svr_map_change->var_name;

  if (tmp_var_ptr)
    old_len = strlen(tmp_var_ptr);

  // should be NULL when we're called first time
  if (!tmp_var_ptr) {
    MY_MALLOC(tmp_var_ptr , len + 1, "malloc var_name to store variable name for server remapping", 1);
  }else{ //could only be non NULL only if we were called before in this session
    if (len  > old_len)
      MY_REALLOC_EX(tmp_var_ptr, (len + 1), (old_len + 1), "realloc var_name to store variable name for server remapping", 1);// added previous length of tmp variable pointer.
  }
  bzero(tmp_var_ptr, len+1);
  strncpy(tmp_var_ptr, var, len);
  vptr->svr_map_change->var_name = tmp_var_ptr;
  vptr->svr_map_change->change_on_depth = depth;
  NSDL2_API(NULL, NULL, "var_name %s change_on_depth %d",vptr->svr_map_change->var_name, vptr->svr_map_change->change_on_depth);
  return(0);
}


/*
 *  function  can do the following - 
 * force the mapping - hostA--> hostB to 
 * hostX --> hostY
 * if hostA is not supplied, the api can retrieve this hostname from the vptr if
 * it was previously saved during redirection (in order to do this, the user
 * must have set it up using the api - ns_setup_save_url).
 * this api should then be called by the user in pre_page() of another page to extract the saved
 * hostname and use this as the recorded host for the rest of the session
 * (unless changed again) 
 *
 * Inputs 
 *
 * vptr 
 * rec_host - recorded host whose mapping is to be changed. should be in host:port format
 * map_to_server - actual server to which above should be mapped. should be in host:port format
 * Outputs
 * None
 * Errors
 * Algo
 *
 * extract ports from the inputs and save the hosts and ports separately
 * if recorded host is not supplied, extract this from vptr if saved earlier,
 * else use current recorded host and change its mapping to point to RHS
 * supplied.
 * if the server to map to is not given, use current mapping for the recorded
 * server obtained above.
 * find the pointer to host and server to map to in the server entry tables.
 * change the current rec server --> actual in the user table to use the new
 * mapping 
 * the RHS (map_to) should be present in the SERVER_HOST entry 
 * the LHS (rec_host) should be present in the ADD_RECORDED host entry.
 *
 */

int ns_force_server_mapping_ext(VUser *vptr, char *rec_host, char* map_to_server)
{
  IW_UNUSED(char *current_host_name = NULL);
  char *p;
  char  temp[MAX_LINE_LENGTH], tmp_rec_host[MAX_LINE_LENGTH], tmp_map_to_server[MAX_LINE_LENGTH];
  int rec_host_port =0, map_to_server_port =0;
  //int idx;
  int rec_host_idx =0, use_current_host =0,
      use_saved_host =0;
  //current and new rec hosts 
  SvrTableEntry_Shr* svr_ptr_current, *svr_ptr_new;  
  PerHostSvrTableEntry_Shr *grp_svr_host_new; // ptr to the mapped server
  connection *next;
  HostSvrEntry *hptr;
  u_ns_ts_t now = get_ms_stamp();

  //extract port before hand for both recorded and actual servers, if given
  NSDL4_HTTP(NULL, NULL, 
      "Method called rec_host %s map_to_server %s", rec_host, map_to_server);
  if (map_to_server) { //extract port
    strcpy(tmp_map_to_server, map_to_server);
    if  ( (p = strstr(tmp_map_to_server, ":")) ) { 
      p = p + 1;
      map_to_server_port = atoi(p);
    }else{
      NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
          "mapped server %s is not in host:port format", map_to_server);
      return (-1);
    }

   /* send the hostname in host:port format later to search the tables, as the
    * name is saved in this format for actual servers now 
    */   
#if 0 
    // Null terminate the hostname in order to search the tables
    *(p-1) = 0;
    map_to_server = tmp_map_to_server;
#endif

    NSDL4_HTTP(NULL, NULL, "map_to_server %s map_to_server_port %d",
        map_to_server, map_to_server_port);
  }
  if (rec_host) { //extract port
    strcpy(tmp_rec_host, rec_host);
    if ( (p = strstr(tmp_rec_host, ":")) ) { 
      p = p + 1;
      rec_host_port = atoi(p);
    }else{
      NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
          "rec host %s is not in host:port format", rec_host);
      return (-1);
    }
    // Null terminate the hostname in order to search the tables
    *(p-1) = 0;
    rec_host = tmp_rec_host;
    NSDL4_HTTP(NULL, NULL, "rec_host %s rec_host_port %d", rec_host, rec_host_port);
  }


  /* case 1 : recorded server is NULL -
  * if the mapped server is also NULL, use the saved recorded server and the existing mapping for this.
  * it doesnt make sense to call this if there is no saved hostname as there
  * will not be any change in the mapping  since the RHS is also
  * NULL.
  * 
  * case 2: recorded server is NULL, but mapped server is not NULL.
  * use the given mapping for the saved /current recorded server
  * in both these cases, we need to check saved rec server 
   */
  if (!rec_host) {
      if ( !vptr->svr_map_change || vptr->svr_map_change->flag ) { // no saved host
        if (!map_to_server) {   //both NULL and no saved host either - error
          NSEL_MAJ (NULL, NULL, ERROR_ID, ERROR_ATTR, 
              "Trying to force map saved host but ns_setup_save_url() not done or hostname was not saved");
          return(-1);
        }
      }else{
        use_saved_host = 1;
      }
    // if hostname was saved, use this, else use the current recorded host
    if (use_saved_host) {
      // get saved hostname
      sprintf(temp, "{%s}", vptr->svr_map_change->var_name);
      rec_host = ns_eval_string(temp);
      rec_host_port  = vptr->svr_map_change->port;
      if (!rec_host) {
        NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
            "saved hostname appears to be present, but couldnt evaluate host from var name in vptr %s", 
            vptr->svr_map_change->var_name);
        return(-1);
      }
      NSDL4_HTTP(NULL, NULL, "Found saved hostname in vptr rec_host %s rec_host_port %d", rec_host, rec_host_port);
    }else{
      NSDL4_HTTP(NULL, NULL, "No saved hostname found in vptr - using current host for forced mapping");
      use_current_host =1;
    }
  }

  //current recorded host
  svr_ptr_current = vptr->cur_page->first_eurl->index.svr_ptr;
  NSDL2_HTTP(NULL, NULL, "svr_ptr_current->server_name = %s", svr_ptr_current->server_hostname);

  if (!use_current_host) {
    // recorded host can be NULL at this point - only if map_to is not NULL
    // use_current_host is 1 for this case
    unsigned short rec_server_port;
    int hostname_len = find_host_name_length_without_port(rec_host, &rec_server_port);
    if ( (rec_host_idx = find_gserver_shr_idx(rec_host, rec_host_port, hostname_len)) == -1) {
      NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
          "couldn't find saved rec server to force map in the server table - hostname %s port %d",
          rec_host, rec_host_port);
      return(-1);
    }
    svr_ptr_new = &gserver_table_shr_mem[rec_host_idx];
  }else{
    svr_ptr_new = svr_ptr_current;
    rec_host_idx = svr_ptr_current->idx;
  }

  /* if a server to map to was supplied, get the index for this, otherwise use the
  * same index as the recorded server - this will pick whatever this recorded
  * host was mapped to in our tables
   */
  
  grp_svr_host_new = find_actual_server_shr(map_to_server, vptr->group_num, rec_host_idx);
  if (!grp_svr_host_new) {

      NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
          "couldn't find map_to server in actual server table - server name %s port %d",
          map_to_server,  map_to_server_port);
      return(-1);
    }
  
  NSDL2_HTTP(NULL, NULL, "Current rec_host_idx = %d, server_name:port = %s:%d, New rec_host server_name:port = %s", 
                          rec_host_idx, svr_ptr_new->server_hostname, svr_ptr_new->server_port, grp_svr_host_new->server_name);

  // the RHS that is current is contained in ustable[] at this index - get name to print
  #ifdef NS_DEBUGON
  if (!vptr->ustable[svr_ptr_current->idx].svr_ptr) {
    vptr->ustable[svr_ptr_current->idx].svr_ptr = find_actual_server_shr(NULL, vptr->group_num, svr_ptr_current->idx);
  }
  current_host_name = vptr->ustable[svr_ptr_current->idx].svr_ptr->server_name;
  #endif
  /* 
  * our current mapping is - A --> B
  * If the new rec server we want to use is C (given or saved), and this needs to be force mapped to
  * actual server D, then
  * replace B with D
  * so the result is -- A --> D
  * the entry for D may not be in vptr->ustable[] as it may not have happened yet.
  * So we use the global table of mappings set up in the begining.
   */

  //Now we are filling new recorded host on same index where old recorded host was filled.
  //vptr->ustable[svr_ptr_current->idx].svr_ptr = grp_svr_host_new;
  vptr->ustable[svr_ptr_new->idx].svr_ptr = grp_svr_host_new;

  NSDL2_HTTP(NULL, NULL, 
      "changed server mapping from %s:%d (recorded) -> %s (current actual) to -> %s:%d (actual of new recorded host)",
      svr_ptr_new->server_hostname,svr_ptr_new->server_port,
      current_host_name? current_host_name:"No mapping",
      grp_svr_host_new->server_name, ntohs(grp_svr_host_new->saddr.sin6_port) );

  //close all connections that dont use this rec server mapping
  // get pointer to the new rec-server entry in the host server table
  hptr = vptr->hptr + svr_ptr_new->idx;
  next = hptr->svr_con_head;
  /*
  * loop through the connections for this host and close the open ones, as
  * the mapping for the host has changed. the open ones point to the old
  * rec-server
   */
  while (next) {
    if (next->conn_fd != -1) {
      //idx = next->gServerTable_idx;   //cant use cptr->url_num for the host as
      // the url_num gets freed if redirected
      NSDL2_HTTP(NULL, next, 
          "closing  open connection due to rec server re-mapping: new rec-server %s old idx %d hostname %s",
          svr_ptr_new->server_hostname, next->gServerTable_idx , svr_ptr_current->server_hostname);
      close_fd_and_release_cptr(next, NS_FD_CLOSE_REMOVE_RESP, now); 
    }
    next = (connection *)next->next_reuse;
  } //while
  return(0);
}

