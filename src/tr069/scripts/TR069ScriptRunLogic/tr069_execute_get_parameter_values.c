/* 
 * Name: tr069_cpe_execute_get_parameter_values
 * Purpose: Execute get parameter values RPC and get next rpc to execute
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"

void tr069_execute_get_parameter_values()
{
double wait_time = .1; // .1 Seconds  (100 milli seconds)

    ns_start_transaction("TR069GetParameterValues");
    int rpc_name = ns_tr069_cpe_execute_get_parameter_values();
    ns_end_transaction("TR069GetParameterValues", NS_AUTO_STATUS);

    // Save RPC method to be executed in RpcMethod parameter to use in runlogic
    ns_set_int_val("RpcMethod", rpc_name);

    // IsSessionInProgress is set to indicate runlogic that tr069 session is complete
    if(rpc_name == NS_TR069_END_OF_SESSION)
      ns_set_int_val("IsSessionInProgress", 0);
    else
      ns_set_int_val("IsSessionInProgress", 1);

      ns_tr069_wait(wait_time);
}
