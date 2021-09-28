#if (Ubuntu && RELEASE == 1204)
#include <postgresql/9.1/server/postgres.h>
#include <postgresql/9.1/server/fmgr.h>
#elif (Redhat)
#include </usr/pgsql-12/include/server/postgres.h>
#include </usr/pgsql-12/include/server/fmgr.h>
#elif (Ubuntu && RELEASE == 1604)
#include <postgresql/9.5/server/postgres.h>
#include <postgresql/9.5/server/fmgr.h>
#else
#include <postgresql/12/server/postgres.h>
#include <postgresql/12/server/fmgr.h>
#endif
#include <string.h>
#include <stdio.h>

/* Any change in this macro must be reflected in nsi_db_get_obj_data shell script. Look for function calculate_percentile_scale in that file. */
#define PERC_ARRAY_SIZE 120000
/*

  In FC8, following error comes: 
  psql:insert_aggregate.sql:13: ERROR:  incompatible 
  library "/var/lib/pgsql/percentile.so": missing magic block
  So added following 3 lines in percentile.c file:
*/
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

typedef struct {
  int num_elements;
  int array[PERC_ARRAY_SIZE];
} percentile_t;

PG_FUNCTION_INFO_V1(percentile_add);
         
Datum
percentile_add(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int32 arg2 = PG_GETARG_INT32(1);
  // FILE* debug_file = fopen("/tmp/debug_file", "a+");
  
  // fprintf(debug_file, "arg is 0x%x, arg2 is %d\n", (unsigned int) arg, arg2);

  if (arg == (percentile_t*) 0) {
    arg = (percentile_t*) malloc(sizeof(percentile_t));
    if(arg == NULL) PG_RETURN_POINTER(arg); // Added on Oct 5th, 2009
    memset(arg, 0, sizeof(percentile_t));
  } 
  
  // fprintf(debug_file, "arg is now 0x%x\n", (unsigned int) arg);

  /* If arg2 is < 0 this means there is something wrong while calculating
   * time stamps (end time is smaller than start time). Here, we bypass it
   * because it will cause corruption and eventual postgres coredumps. */
  if (arg2 < 0)
  {
      FILE* debug_file = fopen("/tmp/percentile_error.log", "a+");
      if(debug_file) 
      {
        fprintf(debug_file, "Sample is < 0, value = %d\n", arg2);
        fclose(debug_file);
      }
  }
  else
  {
    (arg->num_elements)++;
    if (arg2 < PERC_ARRAY_SIZE)
      (arg->array[arg2])++;
    else /* Response time for the sessions can be more than PERC_ARRAY_SIZE, We put it into last bucket */
    {
      (arg->array[PERC_ARRAY_SIZE - 1])++;
    }
  }


  // fclose(debug_file);
  PG_RETURN_POINTER(arg);
}

PG_FUNCTION_INFO_V1(median_percentile_final);

Datum
median_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;
  //  FILE* debug_file = fopen("/tmp/debug_file", "a+");

  if (arg) {
    pct_num = arg->num_elements * 50 / 100;
    //    fprintf(debug_file, "pct_num is %d\n", pct_num);

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }
    free(arg);
  }

  //  fclose(debug_file);

  PG_RETURN_INT32(i);
}

PG_FUNCTION_INFO_V1(eighty_percentile_final);

Datum
eighty_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;

  if (arg) {
    pct_num = arg->num_elements * 80 / 100;

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }
    free(arg);
  }

  PG_RETURN_INT32(i);
}

PG_FUNCTION_INFO_V1(ninety_percentile_final);

Datum
ninety_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;

  if (arg) {
    pct_num = arg->num_elements * 90 / 100;

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }

    free(arg);
  }

  PG_RETURN_INT32(i);
}

PG_FUNCTION_INFO_V1(ninety_five_percentile_final);

Datum
ninety_five_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;

  if (arg) {
    pct_num = arg->num_elements * 95 / 100;

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }

    free(arg);
  }

  PG_RETURN_INT32(i);
}

PG_FUNCTION_INFO_V1(ninety_nine_percentile_final);

Datum
ninety_nine_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;

  if (arg) {
    pct_num = arg->num_elements * 99 / 100;

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }

    free(arg);
  }

  PG_RETURN_INT32(i);
}

PG_FUNCTION_INFO_V1(eighty_five_percentile_final);

Datum
eighty_five_percentile_final(PG_FUNCTION_ARGS)
{
  percentile_t* arg = (percentile_t*) PG_GETARG_POINTER(0);
  int pct_num;
  int i = 0;
  int total_add = 0;

  if (arg) {
    pct_num = arg->num_elements * 85 / 100;

    for (; i < PERC_ARRAY_SIZE; i++) {
      total_add += arg->array[i];
      
      if (total_add >= pct_num)
	break;
    }

    free(arg);
  }

  PG_RETURN_INT32(i);
}
