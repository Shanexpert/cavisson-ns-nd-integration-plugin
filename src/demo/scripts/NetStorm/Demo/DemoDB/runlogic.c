#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"


extern int init_script();
extern int exit_script();

typedef void FlowReturn;

// Note: Following extern declaration is used to find the list of used flows. Do not delete/edit it
// Start - List of used flows in the runlogic
extern FlowReturn db_create_flow();
extern FlowReturn db_insert_flow();
extern FlowReturn db_select_flow();
extern FlowReturn db_update_flow();
extern FlowReturn db_bindparameter_flow();
// End - List of used flows in the runlogic


void runlogic()
{
    NSDL2_RUNLOGIC(NULL, NULL, "Executing init_script()");

    init_script();

    NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - DemoDB");
    {

        NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - DBCreate");
        {
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_create_flow");
            db_create_flow();
        }

        NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - DBInsert");
        {
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_insert_flow");
            db_insert_flow();
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_select_flow");
            db_select_flow();
        }

        NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - DBUpdate");
        {
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_update_flow");
            db_update_flow();
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_select_flow");
            db_select_flow();
        }

        NSDL2_RUNLOGIC(NULL, NULL, "Executing sequence block - DBInsertALL");
        {
            NSDL2_RUNLOGIC(NULL, NULL, "Executing flow - db_bindparameter_flow");
            db_bindparameter_flow();
        }
    }

    NSDL2_RUNLOGIC(NULL, NULL, "Executing ns_exit_session()");
    ns_exit_session();
}
