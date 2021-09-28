#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"

extern int init_script();

typedef void FlowReturn;

//extern FlowReturn tr069_cpe_flow();
extern FlowReturn tr069_flow();


void runlogic() {

  init_script();

  //tr069_cpe_flow();
  tr069_flow();

  ns_exit_session();
}
