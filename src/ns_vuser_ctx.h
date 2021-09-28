/************************************************************************************************
 * File Name            : ns_vuser_ctx.h 
 * Author(s)            : 
 * Date                 : 19 May 2011
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Declares script mode Directives and Functions
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/

#ifndef _NS_VUSER_CTX_H_
#define _NS_VUSER_CTX_H_

//<<<<<<<<<<<<<<<<<<<<<<<<< P R E P R O C E S S O R   D I R E C T I V E S >>>>>>>>>>>>>>>>>>>>>>>

// Script execution mode
#define NS_SCRIPT_MODE_LEGACY           0 // Execute in the current way
#define NS_SCRIPT_MODE_USER_CONTEXT     1 // Execute in user context
#define NS_SCRIPT_MODE_SEPARATE_THREAD  2 // Execute as separate thread
#define NS_SCRIPT_MODE_SEPARATE_PROCESS 3 // Execute as separate process

#define NS_VUSER_NVM_CTX                0
#define NS_VUSER_USER_CTX               1

#define NS_MIN_STACK_SIZE_FOR_DEBUG 256 // Min stack required run in debug mode in KB
#define NS_MIN_STACK_SIZE_FOR_NON_DEBUG 16 // Min stack required run in non debug mode in KB
#define NS_MIN_JMETER_STACK_SIZE 32

// We are doing it 32 because when any event is locked from an api called from flow, like ns_end_transaction, event api 
// needs arround 10k of stack itself. We run it with 16k it is also corrupting stack. We run the test with 32 k, with 100 // and 1000 users, it is working fine.
// Reverting back above changes beacuse event log needs more than 32k, so we made the buffers used in event log static
#define NS_DEFAULT_STACK_SIZE_FOR_NON_DEBUG 16 // Default stack set if not given in scenario in KB
#define NS_RBU_STACK_SIZE_FOR_NON_DEBUG     64 // Default stack set if not given in scenario in KB

extern int kw_set_g_script_mode(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int switch_to_nvm_ctx(VUser *vptr, char *msg);
extern int create_vuser_ctx(VUser *vptr);
extern int free_vuser_ctx(VUser *vptr);
extern inline int switch_to_nvm_ctx_ext(VUser *vptr, char *msg);
extern inline int switch_to_vuser_ctx(VUser *vptr, char *msg);
/***********************************************************************************************/
#endif
