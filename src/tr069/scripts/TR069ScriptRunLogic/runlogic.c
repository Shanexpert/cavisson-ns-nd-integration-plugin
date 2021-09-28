#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"


extern int init_script();
extern int exit_script();

typedef void FlowReturn;

// Note: Following extern declaration is used to find the list of used flows. Do not delete/edit it
// Start - List of used flows in the runlogic
extern FlowReturn tr069_inform();
extern FlowReturn tr069_execute_get_rpc_methods();
extern FlowReturn tr069_execute_set_parameter_values();
extern FlowReturn tr069_execute_get_parameter_values();
extern FlowReturn tr069_execute_get_parameter_names();
extern FlowReturn tr069_execute_set_parameter_attributes();
extern FlowReturn tr069_execute_get_parameter_attributes();
extern FlowReturn tr069_execute_add_object();
extern FlowReturn tr069_execute_delete_object();
extern FlowReturn tr069_execute_reboot();
extern FlowReturn tr069_execute_download();
extern FlowReturn tr069_unsupported_rpc();
// End - List of used flows in the runlogic


void runlogic()
{
    NSDL2_RUNLOGIC(NULL, NULL, "Executing init_script()");

    init_script();

    NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - TR069Main");
    {
        NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_inform");
        tr069_inform();

        NSDL2_RUNLOGIC(NULL, NULL, "Executing while block - TR069CheckSessionDone. NS Variable = IsSessionInProgress");
        {

            NSDL2_RUNLOGIC(NULL, NULL, "NS Variable value for block - TR069CheckSessionDone = %d", ns_get_int_val("IsSessionInProgress"));
            while(ns_get_int_val("IsSessionInProgress"))
            {

                NSDL2_RUNLOGIC(NULL, NULL, "Executing switch block - TR069ExecuteRPC. NS Variable = RpcMethod");
                {

                    NSDL2_RUNLOGIC(NULL, NULL, "NS Variable value for block - TR069ExecuteRPC = %d", ns_get_int_val("RpcMethod"));
                    switch(ns_get_int_val("RpcMethod"))
                    {
                        case 3:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_get_rpc_methods (case value = 3)");
                            tr069_execute_get_rpc_methods();
                            break;
                        case 4:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_set_parameter_values (case value = 4)");
                            tr069_execute_set_parameter_values();
                            break;
                        case 5:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_get_parameter_values (case value = 5)");
                            tr069_execute_get_parameter_values();
                            break;
                        case 6:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_get_parameter_names (case value = 6)");
                            tr069_execute_get_parameter_names();
                            break;
                        case 7:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_set_parameter_attributes (case value = 7)");
                            tr069_execute_set_parameter_attributes();
                            break;
                        case 8:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_get_parameter_attributes (case value = 8)");
                            tr069_execute_get_parameter_attributes();
                            break;
                        case 9:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_add_object (case value = 9)");
                            tr069_execute_add_object();
                            break;
                        case 10:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_delete_object (case value = 10)");
                            tr069_execute_delete_object();
                            break;
                        case 11:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_reboot (case value = 11)");
                            tr069_execute_reboot();
                            break;
                        case 12:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_execute_download (case value = 12)");
                            tr069_execute_download();
                            break;
                        default:
                            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - tr069_unsupported_rpc (case value = 0)");
                            tr069_unsupported_rpc();
                            break;
                    }
                }
            }
        }
    }

    NSDL2_RUNLOGIC(NULL, NULL, "Executing ns_exit_session()");
    ns_exit_session();
}
