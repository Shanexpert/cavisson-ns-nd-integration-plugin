/******************************************************************
 * Name    : ns_vuser_ctx.c
 * Author  : Neeraj Jain/Shalu
 * Purpose : This file contains methods related to running script in user context and
             parsing of G_SCRIPT_MODE keyword
 * Note:

******************************************************************/
#include "ns_cache_include.h"
#include "ns_vuser_tasks.h"

#include <ucontext.h>

#include "ns_vuser_ctx.h"
#include "ns_tls_utils.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

/* G_SCRIPT_MODE <group name> <mode> <stack_size> */
int kw_set_g_script_mode(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char sgrp_name[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num; 
  int mode = 0;
  char cmode[MAX_DATA_LINE_LENGTH];
  cmode[0]=0;
  char cstack_size[MAX_DATA_LINE_LENGTH];
  cstack_size[0] = 0;
  char cfree_stack[MAX_DATA_LINE_LENGTH];
  cfree_stack[0] = 0;
  int stack_size = 0;
  int free_stack = 0;

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, sgrp_name, cmode,  cstack_size, cfree_stack, tmp);

  if((num < 3) || (num > 5)) {
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(cmode) == 0)
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_2);

  mode = atoi(cmode);
  if(mode < 0 || mode > 3)
  {
     NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_3);
  }

  if(mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    if(num >= 4)
    {
      if(ns_is_numeric(cstack_size) == 0)
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_2);
      }
    }
    else
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_1);
    if(num == 5)
    {
      if(ns_is_numeric(cfree_stack) == 0)
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_2);

      free_stack = atoi(cfree_stack); 
      if(free_stack < 0 || free_stack > 1)
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011233, CAV_ERR_MSG_3);
    }

    stack_size = atoi(cstack_size); // Stack size is in KB
  }
  else if (mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    if(global_settings->init_thread == 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SCRIPT_MODE_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_4);
    }
    global_settings->is_thread_grp = 1;
  }
  else if(num >= 4)
  {
    NSTL1(NULL, NULL, "Warning: stack size is only with mode 1. Ignored");
    NS_DUMP_WARNING("Stack size for keyword 'G_SCRIPT_MODE' can use only in user context mode.");
  }

  gset->script_mode = mode;
  gset->stack_size = stack_size * 1024; // Convert KB to bytes
  gset->free_stack = free_stack; 

  return 0;
}

inline int switch_to_nvm_ctx_ext(VUser *vptr, char *msg) {
  NSDL2_SCHEDULE(vptr, NULL, "Method called. UserId = %d:%d, Msg = %s", my_child_index, vptr->user_index, msg);
  vut_add_task(vptr, VUT_SWITCH_TO_NVM);
  switch_to_nvm_ctx(vptr, msg);
  return 0;
}
/* Code for user context */

// Variable to save nvm context
static ucontext_t g_main_ctx;
/* High level design for running C script in user context

  On start of User:
    NVM runs in NVM context and starts the new user.
  On new session:
    NVM starts running script in user context by calling create_vuser_ctx().  
    C Script now runs in user context till any API is called which needs to be run in NVM context.
    Currently following two API are run in NVM context:
      ns_web_url
      ns_page_think_time
  On Calling of API which need to run in NVM context:    
    When script is calling any of these API, we will switch context from user to NVM by switch_to_nvm_ctx()
    Now NVM runs scripts APIs in NVM context.
  On completion of API:
    Once these APIs are done, NVM will switch context back to User context by switch_to_vuser_ctx().
  
  One sample flow is:
  NVM context --(On New Session)--> User Context --(ns_web_url start)--> NVM Context --(ns_web_url done)--> User Context ...
*/
/* Switch for nvm context from virtual user context */

inline int switch_to_nvm_ctx(VUser *vptr, char *msg) {
  NSDL2_SCHEDULE(vptr, NULL, "Method called. UserId = %d:%d, Msg = %s", my_child_index, vptr->user_index, msg);

  NS_VPTR_SET_NVM_CONTEXT(vptr);
  // if(swapcontext(&(vptr->ctxptr->ctx), &(vptr->ctxptr->nvm_ctx)) < 0) {
  if(swapcontext(&(vptr->ctxptr->ctx), &(g_main_ctx)) < 0) {
  char user_id[64]; // Attributes are in string format
    sprintf(user_id, "%d:%u", my_child_index, vptr->user_index);
    NS_EL_1_ATTR(EID_VUSER_CTX, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                user_id,
                                "Error in switching to nvm context using swapcontext(). Error = %s",
                                nslb_strerror(errno));
    NS_VPTR_SET_USER_CONTEXT(vptr);
    return -1;
  
  }
  // Bug 111105: When Session is completed we have ON this flag(on_session_completion).
  // This is done so that Vuser can call the exit script to execute the exit_script script
  if(vptr->flags & NS_VPTR_FLAGS_SESSION_COMPLETE)
  {
     NSDL2_API(vptr, NULL, "Task %s done for user %p", vptr);
     vptr->flags &= ~NS_VPTR_FLAGS_SESSION_COMPLETE;
     ns_exit_session();
     return 0; 
  }

  return 0;  
}

/* Switch for virtual user context from NVM context */

inline int switch_to_vuser_ctx(VUser *vptr, char *msg) {

  NSDL2_SCHEDULE(vptr, NULL, "Method called. UserId = %d:%d, Msg = %s", my_child_index, vptr->user_index, msg);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->operation = VUT_USER_CONTEXT;
    return -1;//donot switch the context
  }
  NS_VPTR_SET_USER_CONTEXT(vptr);

  TLS_SET_VPTR(vptr);  // set as we need this in API called from script

  //  When successful, swapcontext() does not return. (But we may return later, in case oucp is activated, in  which  case
  //  it looks like swapcontext() returns 0.  On error, it return -1 and set errno appropriately.

  if(swapcontext(&(g_main_ctx), &(vptr->ctxptr->ctx)) < 0) {
  char user_id[64]; // Attributes are in string format
    sprintf(user_id, "%d:%u", my_child_index, vptr->user_index);
    NS_EL_1_ATTR(EID_VUSER_CTX, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                user_id,
                                "Error in switching to user context using swapcontext(). Error = %s",
                                nslb_strerror(errno));
    NS_VPTR_SET_NVM_CONTEXT(vptr);
    return -1;
  
  }
  return 0;
}


/* Create virtual user context and switch to user context */

inline int create_vuser_ctx(VUser *vptr) {

  int loc_stack_size = runprof_table_shr_mem[vptr->group_num].gset.stack_size;
  NSDL2_SCHEDULE(vptr, NULL, "Method called. UserId = %d:%d. Stack Size = %d", 
                              my_child_index, vptr->user_index, loc_stack_size); 

  // gets the current context of the calling process, storing it in the ctx
  if(getcontext(&(vptr->ctxptr->ctx)) < 0) {
  char user_id[64]; // Attributes are in string format
    sprintf(user_id, "%d:%u", my_child_index, vptr->user_index);
    NS_EL_1_ATTR(EID_VUSER_CTX, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                user_id,
                                "Error in getting current context using getcontext(). Error = %s",
                                nslb_strerror(errno));
  
    return -1;  
  }
  
  // Allocate stack for virtual user context
  vptr->ctxptr->ctx.uc_stack.ss_size = loc_stack_size; 

  if(vptr->ctxptr->stack == NULL){
    MY_MALLOC(vptr->ctxptr->stack, loc_stack_size, "User stack", -1);
  }
  else
    NSDL2_SCHEDULE(vptr, NULL, "Not allocating stack as stack is to be reused");

  vptr->ctxptr->ctx.uc_stack.ss_sp = vptr->ctxptr->stack;

  vptr->ctxptr->ctx.uc_link = &(g_main_ctx);

  // Make context for virtual user. Starting function for script execution is runlogic() with no arguments
  makecontext(&(vptr->ctxptr->ctx), (void (*)(void))runprof_table_shr_mem[vptr->group_num].gset.runlogic_func_ptr, 0);

  /* Something is wrong with MAN page of makecontext Method is void and it says When successful, makecontext() returns 0 */
 /*if(makecontext(&(vptr->ctxptr->ctx), (void (*)(void))vptr->sess_ptr->runlogic_func_ptr, 1, vptr) < 0) {
  char user_id[64]; // Attributes are in string format
    sprintf(user_id, "%d:%u", my_port_index, vptr->user_index);
    NS_EL_1_ATTR(EID_VUSER_CTX, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                user_id,
                                "Error in making context for virutal user using makecontext(). Error = %s",
                                nslb_strerror(errno));
  
    return -1;  
  }*/

  return(switch_to_vuser_ctx(vptr, "NewUser"));
    
}

// Free user context to free stack after session is complete
inline int free_vuser_ctx(VUser *vptr) {

  NSDL2_SCHEDULE(vptr, NULL, "Method called. UserId = %d:%d", my_child_index, vptr->user_index);
  NSDL2_SCHEDULE(vptr, NULL, "free_stack = %d", runprof_table_shr_mem[vptr->group_num].gset.free_stack);
  if(runprof_table_shr_mem[vptr->group_num].gset.free_stack == 1)
  {
    NSDL2_SCHEDULE(vptr, NULL, "Method called.Freeing stack. UserId = %d:%d", my_child_index, vptr->user_index);
    FREE_AND_MAKE_NULL_EX(vptr->ctxptr->stack, runprof_table_shr_mem[vptr->group_num].gset.stack_size, "User stack", -1);// Added stack size allocated to user in bytes
  }
  else
    NSDL2_SCHEDULE(vptr, NULL, "Method called. NOT Freeing. UserId = %d:%d", my_child_index, vptr->user_index);
  return 0;
}

