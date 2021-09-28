#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <string.h>
#include <time.h>
#include <ctype.h>
#include "ns_string.h" 
#include "ns_log.h" 

extern char* ns_eval_string_flag_internal(char* string, int encode_flag, long *size, VUser *api_vptr);

/* ns_parm_sprintf() function is like the standard c function sprintf
* except that the formatted string is written to a netstorm parameter
* of a string buffer
* On Success: returns 0
* On Failure: returns -1
*/

/*
* On success: Returns the number of elements in parameter array.
* On failure: return -1
*/
int
ns_paramarr_len_internal(const char* paramArrayName, VUser *my_vptr)
{
  char buf[1024];
  char param_len;
  int var_hashcode;
  char * var_value;
  long len;

  NSDL2_API(NULL, NULL, "Method called. Parameter = %s", paramArrayName);
 
  if(paramArrayName == NULL) {
   fprintf(stderr, "Passed Parameter(paramArrayName) is NULL\n");
   return -1;
  } 

  // Check if var name is a valid variable using hash code
  var_hashcode = my_vptr->sess_ptr->var_hash_func(paramArrayName, strlen(paramArrayName));
  NSDL3_API(NULL, NULL, "var_hashcode = %d", var_hashcode);
  if (var_hashcode == -1) {
    fprintf(stderr, "Invalid parameter name. Parameter name:%s\n", paramArrayName);
    return -1;
  }

  sprintf(buf, "{%s_count}", paramArrayName);
  //var_value = ns_eval_string(buf);  
  var_value = ns_eval_string_flag_internal(buf, 0, &len, my_vptr);

  NSDL2_API(NULL, NULL, "Count of variable=%s", buf);
  param_len = atoi(var_value);

  NSDL2_API(NULL, NULL, "Parameter(%s) length=%d", paramArrayName, param_len);
  if(param_len == 0) {
    NSDL3_API(NULL, NULL, "Parameter (%s) is a scalar paramter, returning 1", paramArrayName);
    return 1;
  }
  if(param_len == 1) {
   fprintf(stderr, "Error Parameter (%s) is a vector parameter but count is 1. It should be > 1", paramArrayName);
   return 1;
  }

  return param_len;

}

int
ns_paramarr_len(const char* paramArrayName)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(NULL, NULL, "Method called. Parameter = %s", paramArrayName);
  return ns_paramarr_len_internal(paramArrayName, vptr);
}

inline char *
ns_paramarr_idx_or_random_internal(const char * paramArrayName, unsigned int index, int random_flag, VUser *my_vptr)
{
  int num_elements = 0;  
  static __thread char *var_value = NULL;

  char ns_paramarr_buf[1024];
  long len;
  
  NSDL2_API(NULL, NULL, "Method called with Parameter name:%s and index:%d", paramArrayName, index);

  num_elements = ns_paramarr_len_internal(paramArrayName, my_vptr); // This method will check for valid var name
  NSDL2_API(NULL, NULL, "index=%d; number of elements =%d", index, num_elements);

  // TODO - (Ask Anil) - If var name is not correct, what should be returned?
  if(num_elements == -1) {
    return NULL;
  }
  /* If random_flag is set, then pic up the random index value.*/ 
  if(random_flag == 1) {
    //This will returns the random number between 1 to num_elements
     index  = 1+(int) (((float)num_elements)*rand()/(RAND_MAX+1.0));
  }

  if((index <= 0) || (index > num_elements)) {
    NSDL2_API(NULL, NULL, "Parameter index (%d) is invalid value for paramter %s. It should be between 1 and %d\n", index, paramArrayName, num_elements);
  } 
  if ((num_elements == 1) && (index == 1)) // Variable is scalar and index is also 1
    sprintf(ns_paramarr_buf, "{%s}", paramArrayName);
  else
    sprintf(ns_paramarr_buf, "{%s_%d}", paramArrayName, index); //variable is vactor

  //if ns_eval_string failed then what should be do
  var_value = ns_eval_string_flag_internal(ns_paramarr_buf, 0, &len, my_vptr); 
  NSDL4_API(NULL, NULL,"variable value=%s", var_value);
  return var_value; 

}

static inline char *
ns_paramarr_idx_or_random(const char * paramArrayName, unsigned int index, int random_flag)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called with Parameter name:%s and index:%d", paramArrayName, index);
  return ns_paramarr_idx_or_random_internal(paramArrayName, index, random_flag, vptr); 
}

/*
* ns_paramarr_idx() returns a string containing the value
* of the parameter at the specified position in a parameter array
* If the parameter does not exist, then return value is the concatenation i
* of "{", paramArrayName, "_", index and "}"
* example "{myParam_4}"
* On success: Return a string buffer containing the value.
* On failure: Return NULL.
* Arguments: Parameter array and index value
*/

char *
ns_paramarr_idx(const char * paramArrayName, unsigned int index) {
  int random_flag = 0; //0 for non random and 1 for random
  return(ns_paramarr_idx_or_random(paramArrayName, index, random_flag));
}

/*ns_paramarr_random() returns a string containing the value 
* of the parameter in a parameter array. The value is returned 
* from the parameter at a position chosen randomly by NetStorm.
* If the parameter does not exist, then return value is the concatenation of 
* of "{", paramArrayName, "_", index and "}"
* example "{myParam_4}"
* On success: Return a string buffer containing the value.
* On failure: Return NULL.
* Arguments: Param array
*/

char *
ns_paramarr_random(const char *paramArrayName)
{
 // pass index 0 as second argument to pic up the random index value
  int random_flag = 1; //0 for non random and 1 for random
  int index = 0;

  return(ns_paramarr_idx_or_random(paramArrayName, index, random_flag));
}

/*ns_check_reply_size() c api is used to compare the response 
* size with value1 and value2.
*Arguments:
* mode: mode value. currently NotBetweenMinMax mode is supported only.
* value1: user defined value1 that is use to compare with response size. 
* value2: user defined value2 that is used to compare with response size.
*Returns value:
* 0 : on success if the response size is as per mode.
* 1 : if response/reply size is small.
* 2 : if response/reply size is big. 
* -1 : on error
*/
int ns_check_reply_size(int mode, int value1, int value2)
{  
  VUser *vptr = TLS_GET_VPTR();
  

  int blen = vptr->bytes; //response size
  //if size is not tobig or tosmall then its as per mode
  int ret_val = SIZE_AS_PER_MODE; //default OK 

  NSDL3_API(vptr, NULL, "Method called. mode = %d, value1 = %d, value2= %d, response size is = %d", mode, value1, value2, blen);
  
  if(value1 < 0 || value2 < 0) {
    printf("value1(%d) or value2(%d) can not be negative.\n", value1, value2);
    return -1;
  }

  //code is written like that more mode value can be added in future.
  if(mode == NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX)
  {
    if(value2 <= value1) {
      printf("value2(%d) must be greater than value1(%d) for mode NotBetweenMinMax.\n", value2, value1);
      return -1;
    }
    /* check the response[reply] size with value */
    NSDL3_API(NULL, NULL, "Response size is = [%d]", blen);
    if(blen > value2)
      ret_val = SIZE_TO_BIG; 
    else if(blen < value1)
      ret_val = SIZE_TO_SMALL;  
  } else {
    printf("Invalid mode, mode value is = %d.\n", mode); 
    return -1;
  }
  return ret_val;
}
