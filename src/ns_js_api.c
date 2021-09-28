/*###################################################################################
#####################################################################################
##  Name      :    ns_js_api.c			                                   ##
##  Author    :    Devendar Jain,Ayush Kumar & Sourabh Jain	                   ## 
##  Purpose   :    To execute java script inside the Netstorm script               ##
##                 using duktape library.                                          ##
##										   ##
##  KeyPoints :	1.API named “ns_exec_js” that will take JS as a input		   ##
##		  and provide the result.					   ##
##		2.API ns_exec_js have options to take buffer, file		   ##
##		  and parameter as input.					   ##
##		3.API ns_exec_js have options to save result in buffer, file 	   ##
##		  and parameter.						   ##
##		4.API named “ns_js_error” to get error message in case of any 	   ##
##		  error occurred during execution.				   ##
##										   ##
##  Req Doc CVS Path : /docs/Products/NetStorm/TechDocs/NetStormCore/Req	   ##
##                                                                                 ##
##  Usage   :    int ns_exec_js(int input_type, char *input, int output_type,	   ##
##				char *output, int  size); 			   ##
##		 char* ns_js_error()						   ##	
##                                                                                 ##
##  Modification History:                                                          ##
##  Initial Version                                                                ##
##  14/09/2020 : Devendar Jain,Ayush Kumar & Sourabh Jain  		           ##
##                                                                                 ##
#####################################################################################
###################################################################################*/

/*#################################################################################
Possible uses of API:								 ##	
										 ##	
int ns_exec_js(0, inp_buff, 0, out_buff, 1024)					 ##
int ns_exec_js(1, "inp_param", 0, out_buff, 1024)				 ##
int ns_exec_js(2, "/home/cavisson/js_inp.txt", 0, out_buff, 1024)		 ##
int ns_exec_js(0, inp_buff, 1, "out_param", 0)					 ##
int ns_exec_js(1, "inp_param", 1, "out_param", 0)				 ##
int ns_exec_js(2, "/home/cavisson/js_inp.txt", 1, "out_param", 0)		 ##
int ns_exec_js(0, inp_buff, 2, "/home/cavisson/js_out.txt", 0)			 ##
int ns_exec_js(1, "inp_param", 2, "/home/cavisson/js_out.txt", 0)		 ##
int ns_exec_js(2, "/home/cavisson/js_inp.txt", 2, "/home/cavisson/js_out.txt", 0)##
#################################################################################*/
#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "ns_string.h"
#include "ns_log.h"
#include "duktape.h"
#include "ns_log.h"
#include "url.h"
#include "ns_tls_utils.h"
#include "nslb_util.h"
#include "ns_alloc.h"

static char js_err_msg[1024 + 1];
static int js_err_mlen = 1024;

int ns_exec_js(int inp_type, char *inp, int out_type, char *out, int size)
{
  char *inp_buf, *out_buf;
  char param_name[NS_PB_MAX_PARAM_LEN + 1];
  int fd;
  int var_hashcode;
  long len;
  struct stat fst;
  NSDL2_API(NULL, NULL, "Method Called, inp_type = %d, input = %s, out_type = %d",
                         inp_type, inp, out_type);

  VUser *vptr = TLS_GET_VPTR();
  js_err_msg[0] = '\0';

//checks if provided input and output are valid and exist.
  if(!inp || inp[0] == '\0')
  {
    snprintf(js_err_msg, js_err_mlen, "Error: Provided input is null or empty.");
    return -1;
  }

  if((inp_type < NS_ARG_IS_BUF) || (inp_type > NS_ARG_IS_FILE))
  {
    snprintf(js_err_msg, js_err_mlen, "Error: Provided input type is invalid.");
    return -1;
  }

  if((out_type < NS_ARG_IS_BUF) || (out_type > NS_ARG_IS_FILE))
  {
    snprintf(js_err_msg, js_err_mlen, "Error: Provided output type is invalid.");
    return -1;
  }

  if(!out || (!out[0] && out_type != NS_ARG_IS_BUF))
  {
    snprintf(js_err_msg, js_err_mlen, "Error: Provided output is null or empty.");
    return -1;
  }

  //checking if given input parameter is a valid parameter. 
  if(inp_type == NS_ARG_IS_PARAM)
  {
    var_hashcode = vptr->sess_ptr->var_hash_func(inp, strlen(inp));
    NSDL3_API(NULL, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

    if(var_hashcode == -1)
    {
      snprintf(js_err_msg, js_err_mlen, "Error: Provided input parameter is not a valid NS parameter.");
      return -1;
    }
  }

  //checking if given output parameter is a valid parameter. 
  if(out_type == NS_ARG_IS_PARAM)
  {
    var_hashcode = vptr->sess_ptr->var_hash_func(out, strlen(out));
    NSDL3_API(NULL, NULL, "Found end of variable. var_hashcode = %d", var_hashcode);

    if(var_hashcode == -1)
    {
      snprintf(js_err_msg, js_err_mlen, "Error: Provided output parameter is not a valid NS parameter.");
      return -1;
    }
  }

//Filling input values to the variables
  switch(inp_type)
  {
    case NS_ARG_IS_BUF:
         inp_buf = inp;
         break;

    case NS_ARG_IS_PARAM:
         snprintf(param_name, NS_PB_MAX_PARAM_LEN, "{%s}", inp);
         inp_buf = ns_eval_string_flag(param_name, 0, &len);
         break;

    case NS_ARG_IS_FILE:
         if((stat(inp, &fst) == -1) || (!fst.st_size))
         {
           snprintf(js_err_msg, js_err_mlen, "Error: Provided file input is not available or zero size.");
           return -1;
         }
         
         int fd = open(inp, O_RDONLY|O_CLOEXEC);
         if(fd <= 0)
         {
           snprintf(js_err_msg, js_err_mlen, "Error in opening file %s. Err[%d] = %s", inp, errno, nslb_strerror(errno));
           return -1;
         }
         MY_MALLOC(inp_buf, fst.st_size + 1, "ns_execute_js_query() input buffer", -1);
         nslb_read_file_and_fill_buf(fd, inp_buf, fst.st_size);
         close(fd);
         break;
   }

  NSDL2_API(NULL, NULL, "input = %s", inp_buf);

//using duktap functinality to execute java script inside the Netstorm script 
  duk_context *ctx = duk_create_heap_default();
  
  if(!ctx)
  {
    snprintf(js_err_msg, js_err_mlen, "Error in creating duk context to execute JS query.");
    if(inp_type == NS_ARG_IS_FILE)
      FREE_AND_MAKE_NULL(inp_buf, "free input buffer", -1);
    
    return -1;
  }

  duk_eval_string(ctx, inp_buf);
  out_buf = (char *)duk_get_string(ctx, -1);

//checking if we get output from duktape functionality    
  if(!out_buf)
  { 
    snprintf(js_err_msg, js_err_mlen, "Error in getting output string from duk context.");
    if(inp_type == NS_ARG_IS_FILE)
      FREE_AND_MAKE_NULL(inp_buf, "free input buffer", -1);
    duk_destroy_heap(ctx);
    return -1;
  }


  NSDL2_API(NULL, NULL, "Output = %s", out_buf);

//Filling output of JS to output variables.
  switch(out_type)
  {
    case NS_ARG_IS_BUF:
         strncpy(out, out_buf, size);
         out[size] = '\0';
         break;

    case NS_ARG_IS_PARAM:
         ns_save_string(out_buf, out);
         break;

    case NS_ARG_IS_FILE:
         fd = open(out, O_CREAT|O_APPEND|O_TRUNC|O_WRONLY|O_CLOEXEC, 00666);
         if(fd <= 0)
         {
           snprintf(js_err_msg, js_err_mlen, "Error in opening file %s. Error = %s", out, nslb_strerror(errno));
           if(inp_type == NS_ARG_IS_FILE)
             FREE_AND_MAKE_NULL(inp_buf, "free input buffer", -1);
           duk_destroy_heap(ctx); 
           return -1;
         }
         write(fd, out_buf, strlen(out_buf));
         close(fd);
         break;
  }
//free input buffer if input type is file.
  if(inp_type == NS_ARG_IS_FILE)
    FREE_AND_MAKE_NULL(inp_buf, "free input buffer", -1);
  if (!ctx)
    duk_destroy_heap(ctx);

 return 0;
 }

//API to print error if ns_exec_js fails on any step.
char* ns_js_error()
{
  NSDL2_API(NULL, NULL, "Method Called");
  return js_err_msg;
}
