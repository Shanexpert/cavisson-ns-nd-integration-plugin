/* 
 * Name: tr069_inform
 * Purpose: Send Inform, wait for reply, then send empty Post
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"


void tr069_inform()
{
double wait_time = .1; // .1 Seconds  (100 milli seconds)

    ns_start_transaction("TR069Inform");
    ns_tr069_cpe_invoke_inform();
    ns_end_transaction("TR069Inform", NS_AUTO_STATUS);

    ns_tr069_wait(wait_time);

    ns_start_transaction("TR069InviteRpc");
    int rpc_name = ns_tr069_cpe_invite_rpc();

    NSDL2_RUNLOGIC(NULL, NULL, "Next RPC Method name = %d", rpc_name);

    ns_end_transaction("TR069InviteRpc", NS_AUTO_STATUS);

    // Save RPC method to be executed in RpcMethod parameter to use in runlogic
    ns_set_int_val("RpcMethod", rpc_name);

    // IsSessionInProgress is set to indicate runlogic that tr069 session is complete
    if(rpc_name == NS_TR069_END_OF_SESSION)
      ns_set_int_val("IsSessionInProgress", 0);
    else
      ns_set_int_val("IsSessionInProgress", 1);

      ns_tr069_wait(think_time);
}
