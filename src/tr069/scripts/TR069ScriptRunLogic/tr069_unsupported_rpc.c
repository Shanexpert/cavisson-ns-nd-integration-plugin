/* 
 * Name: tr069_unsupported_rpc
 * Purpose: Dummy flow to execute on end of session
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"

void tr069_unsupported_rpc()
{
double wait_time = .1; // .1 Seconds  (100 milli seconds)

    fprintf(stderr, "Error: Unsupported RPCMethod (%s) requested by ACS.\n", ns_eval_string("{TR069RPCMethodNameSP}"));
    ns_tr069_wait(wait_time);
}
