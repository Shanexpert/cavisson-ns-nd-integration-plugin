
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ns_string.h"

extern char db_result_filename[];

int db_log_result(char *tablename, char *operation, char *buf, int status)
{
  time_t tloc;
  struct  tm *lt;
  static  char cur_date_time[30] = "";
  static char tmp_buf[512] = "";


  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_date_time, "Error|Error");
  else
  {
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d",
                           lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000,
                           lt->tm_hour, lt->tm_min, lt->tm_sec);
  }

  sprintf(tmp_buf, "%s,%d_%d,%d,%s,%s,%s,%s", 
                    cur_date_time, ns_get_nvmid(), 
                    ns_get_userid(), ns_get_sessid(), 
                    tablename?tablename:"NULL", 
                    operation?operation:"NULL", 
                    buf?buf:"NULL",
                    (status==0)?"Success":"Fail");

  ns_save_data_ex(db_result_filename, 1, tmp_buf);
  fprintf(stdout, "%s\n", tmp_buf);

  return 0;

}
