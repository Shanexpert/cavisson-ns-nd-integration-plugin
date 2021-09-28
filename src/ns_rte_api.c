/************************************************************************************
 * Name      : nsi_rte_api.c 
 * Purpose   : This file contains functions related to rte protocol 
 * Author(s) : Devendra Jain/Atul Sharma 
 * Date      : 12 January 2017 
 * Copyright : (c) Cavisson Systems
 * Modification History :
 ***********************************************************************************/
#include "ns_script_parse.h"
#include "ns_url_req.h"
#include "ns_vuser_trace.h"
#include "ns_vuser_tasks.h"
#include "ns_url_hash.h"
#include "ns_trace_log.h"
#include "ns_log.h"
#include "ns_rte_api.h"
#include "ns_rte_ssh.c"
#include "ns_rte_3270.c"

int nsi_rte_init(ns_rte *rte , int protocol, int terminal)
{

  NSDL2_RTE(NULL, NULL, "Method Called protocol = %d", protocol);
  switch(protocol)
  {
    case RTE_PROTO_SSH:
      rte->protocol = protocol;
      rte->terminal = terminal;
      rte->connect = ns_rte_ssh_connect;
      rte->login = ns_rte_ssh_login;
      rte->type = ns_rte_ssh_send_text;
      rte->wait_text = ns_rte_ssh_wait_text;
      rte->wait_sync = ns_rte_ssh_wait_sync;
      rte->disconnect = ns_rte_ssh_disconnect;
      rte->config = ns_rte_ssh_config;
    break;
    case RTE_PROTO_3270:
      rte->protocol = protocol;
      rte->terminal = terminal;
      rte->connect = ns_rte_3270_connect;
      rte->login = ns_rte_3270_login;
      rte->type = ns_rte_3270_send_text;
      rte->wait_text = ns_rte_3270_wait_text;
      rte->wait_sync = ns_rte_3270_wait_sync;
      rte->disconnect = ns_rte_3270_disconnect;
      rte->config = ns_rte_3270_config;
    break;
  }
  return 0;
}

int nsi_rte_config(ns_rte *rte, char *input)
{
  NSDL2_RTE(NULL, NULL, "Method Called input = %s", input);
  if(!rte)
  {
    NSDL2_RTE(NULL,NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->config(rte, input);
}


int nsi_rte_connect(ns_rte *rte, char *host, char *user, char *password)
{
  NSDL2_RTE(NULL, NULL, "Method Called host = %s, user = %s, password = %s", host, user, password);

  if(!rte)
  {
    NSDL2_RTE(NULL,NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->connect(rte,host,user,password);
}

int nsi_rte_login(ns_rte *rte)
{
  NSDL2_RTE(NULL, NULL,"Method Called");
  if(!rte)
  {
    NSDL2_RTE(NULL, NULL,"NULL rte received");
    NSTL1(NULL, NULL,"NULL rte received");
    return -1;
  }
  return rte->login(rte);
}

int nsi_rte_type(ns_rte *rte, char *input)
{
  NSDL2_RTE(NULL, NULL, "Method Called, input = %s", input);
  if(!rte)
  {
    NSDL2_RTE(NULL, NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->type(rte,input);
}

int nsi_rte_wait_sync(ns_rte *rte)
{
  NSDL2_RTE(NULL, NULL, "Method Called");

  if(!rte)
  {
    NSDL2_RTE(NULL, NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->wait_sync(rte);
}


int nsi_rte_wait_text(ns_rte *rte, char *text,int timeout)
{
  NSDL2_RTE(NULL, NULL, "Method Called, text = %s, timeout = %d", text, timeout);

  if(!rte)
  {
    NSDL2_RTE(NULL, NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->wait_text(rte,text,timeout);
}

int nsi_rte_disconnect(ns_rte *rte)
{
  
  NSDL2_RTE(NULL, NULL, "Method Called");
  if(!rte)
  {
    NSDL2_RTE(NULL, NULL,"NULL rte received");
    NSTL1(NULL,NULL,"NULL rte received");
    return -1;
  }
  return rte->disconnect(rte);
}

/*-------------------------------------------------------------------------------------------------
* Function name  - ns_rte_post_proc
*                 
* Purpose        - This method will close all rte related vnc instances at the time of test end
*
* Output         - On success:  0
*          	 - On failure: -1
*  .rte_params.csv will be present at script path
---------------------------------------------------------------------------------------------------*/
int ns_rte_post_proc()
{
  char cmd_buf [2048 + 1]="";
  char rte_param_path [512 + 1]="";
  int gp_idx;

  cmd_buf [0] = 0;
  rte_param_path [0] = 0;


  NSDL2_RTE(NULL, NULL, "Method called");



  for(gp_idx = 0; gp_idx < total_runprof_entries; gp_idx++)
  {
     if(!runprof_table_shr_mem[gp_idx].gset.rte_settings.enable_rte ||
        !runprof_table_shr_mem[gp_idx].gset.rte_settings.rte.terminal )
       continue;
    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
    sprintf(rte_param_path, "%s/%s/.rte_params.csv", GET_NS_TA_DIR(), 
                             get_sess_name_with_proj_subproj_int(runprof_table_shr_mem[gp_idx].sess_ptr->sess_name, 
                             runprof_table_shr_mem[gp_idx].sess_ptr->sess_id, "/"));


    NSDL3_RTE(NULL, NULL, "rte_param_path = %s, scen_group_name = %s, sess_name = %s", rte_param_path,
                           runprof_table_shr_mem[gp_idx].scen_group_name, runprof_table_shr_mem[gp_idx].sess_ptr->sess_name);

    sprintf(cmd_buf, "vnc_id=`cat %s |awk '{printf $1\",\"}'` "
                     " && nsu_auto_gen_prof_and_vnc -o stop -N $vnc_id >/dev/null 2>&1", rte_param_path);

    NSDL2_RTE(NULL, NULL, "cmd_buf = %s", cmd_buf);

    system(cmd_buf);
  }
  
 return 0;
}

int ns_rte_on_test_start()
{

  char cmd_buff[128 + 1];
  //Stop all RTE running vncs
  NSDL2_RTE(NULL, NULL, "Method Called");
 

  #ifdef NS_DEBUG_ON
    sprintf(cmd_buff, "nsu_auto_gen_prof_and_vnc -o init_rte -D >/dev/null 2>&1" );
  #else
    sprintf(cmd_buff, "nsu_auto_gen_prof_and_vnc -o init_rte >/dev/null 2>&1" );
  #endif

  NSDL2_RTE(NULL, NULL,"Command to stop vnc = %s", cmd_buff);
  system(cmd_buff);

  return 0;
}

//RTE think time as sleep
int ns_rte_page_think_time_as_sleep(VUser *vptr, int page_think_time)
{
  NSDL2_RTE(vptr, NULL, "Method called. vptr = %p, cptr = %p, page_think_time = %d milli-secs", vptr, vptr->last_cptr, page_think_time);

  if(page_think_time <= 0)
  {
    NSDL2_RTE(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
    return 0;
  }

  vptr->pg_think_time = page_think_time;
  vut_add_task(vptr, VUT_PAGE_THINK_TIME);
  NSDL2_RTE(vptr, NULL, "Waiting for think timer as sleep to be over");
  if(vptr->flags & NS_VPTR_FLAGS_USER_CTX) /*bug 79149*/
    switch_to_nvm_ctx(vptr, "PageThinkTimeStart");

  return 0;
}



