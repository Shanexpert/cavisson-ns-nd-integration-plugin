#include <stdio.h>
#include <time.h>
#include "nslb_util.h"
#include <stdlib.h>
#ifdef TEST
int main(int argc, char* argv[])
{
  int test_id;
  int start_test_id = INIT_TEST_RUN_VAL;

  if(argc == 2)
    start_test_id = atoi(argv[1]);

  test_id = nslb_get_testid_ex(start_test_id);
  printf("%d\n",test_id);
  return 0;
} 
#endif
