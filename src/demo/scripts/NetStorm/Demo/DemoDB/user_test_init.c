
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"


char db_result_filename[64] = "\0";
FILE *fp = NULL;
int user_test_init(void)
{
  sprintf(db_result_filename, "%s/logs/TR%d/db_result.csv", getenv("NS_WDIR"), ns_get_testid());

  fp = fopen (db_result_filename, "w");
  if (fp)
  {
    fprintf(fp, "Timestamp,nvmid_userid,sessionid,tablename,operation,productid,productname,productcategory,quantity,unitprice,querystatus\n");
    fclose(fp);
    fp = NULL;
  }
  return 0;
}
