#include <stdio.h>
#include <string.h>

#include "ns_string.h"

void tr069_cpe_flow()
{
double cpe_svc_time = 0.1;  

    ns_start_transaction("TR069Inform");
    ns_tr069_cpe_invoke_inform();  
    ns_end_transaction("TR069Inform", NS_AUTO_STATUS);

    ns_tr069_wait(cpe_svc_time);

    ns_start_transaction("TR069InviteRpc");
    int rpc_name = ns_tr069_cpe_invite_rpc();
    ns_end_transaction("TR069InviteRpc", NS_AUTO_STATUS);

    ns_tr069_wait(cpe_svc_time);

    ns_start_transaction("TR069Session");

    while(rpc_name != NS_TR069_END_OF_SESSION)
    {

        switch (rpc_name)
        {
            case NS_TR069_GET_RPC_METHODS:
              ns_start_transaction("TR069GetRPCMethod"); 
              rpc_name = ns_tr069_cpe_execute_get_rpc_methods();
              ns_end_transaction("TR069GetRPCMethod", NS_AUTO_STATUS);
              break;

            case  NS_TR069_GET_PARAMETER_NAMES:
              ns_start_transaction("TR069GetParameterNames");
              rpc_name = ns_tr069_cpe_execute_get_parameter_names();
              ns_end_transaction("TR069GetParameterNames", NS_AUTO_STATUS);
              break;

          case NS_TR069_GET_PARAMETER_VALUES:
              ns_start_transaction("TR069GetParameterValues");
              rpc_name = ns_tr069_cpe_execute_get_parameter_values();
              ns_end_transaction("TR069GetParameterValues", NS_AUTO_STATUS);
              break;

          case NS_TR069_GET_PARAMETER_ATTRIBUTES:
              ns_start_transaction("TR069GetParameterAttributes");
              rpc_name = ns_tr069_cpe_execute_get_parameter_attributes();
              ns_end_transaction("TR069GetParameterAttributes", NS_AUTO_STATUS);
              break;   

          case NS_TR069_SET_PARAMETER_VALUES:
              ns_start_transaction("TR069SetParameterValues");
              rpc_name = ns_tr069_cpe_execute_set_parameter_values();
              ns_end_transaction("TR069SetParameterValues", NS_AUTO_STATUS);
              break;

          case NS_TR069_SET_PARAMETER_ATTRIBUTES:
              ns_start_transaction("TR069SetParameterAttributes");
              rpc_name = ns_tr069_cpe_execute_set_parameter_attributes();
              ns_end_transaction("TR069SetParameterAttributes", NS_AUTO_STATUS);
              break;

          case NS_TR069_ADD_OBJECT:
              ns_start_transaction("TR069AddObject");
              rpc_name = ns_tr069_cpe_execute_add_object();
              ns_end_transaction("TR069AddObject", NS_AUTO_STATUS);
              break;

          case NS_TR069_DELETE_OBJECT:
              ns_start_transaction("TR069DeleteObject");
              rpc_name = ns_tr069_cpe_execute_delete_object();
              ns_end_transaction("TR069DeleteObject", NS_AUTO_STATUS);
              break;

          case NS_TR069_DOWNLOAD:
              ns_start_transaction("TR069Download");
              rpc_name = ns_tr069_cpe_execute_download();
              ns_end_transaction("TR069Download", NS_AUTO_STATUS);
              break; 

          case NS_TR069_REBOOT:
              ns_start_transaction("TR069Reboot");
              rpc_name = ns_tr069_cpe_execute_reboot();
              ns_end_transaction("TR069Reboot", NS_AUTO_STATUS);
              break;

          case NS_TR069_END_OF_SESSION:
              break;

          default:
              fprintf(stderr, "Error: Invalid RPC method (%d)\n", rpc_name);
              break;  
        } // End of switch

        ns_tr069_wait(cpe_svc_time);
    } // End of while

    ns_end_transaction("TR069Session", NS_AUTO_STATUS);
}
