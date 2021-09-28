/*H**********************************************************************
* FILENAME :        ns_tls_utils.c             
*
* DESCRIPTION :
*       For creating, initializing and deallocating thread storage buffer. 
*
* PUBLIC FUNCTIONS :
*       void    ns_tls_init(int)
*       void    ns_tls_init_seg_vector(int)
*       void    ns_tls_free()
*
* NOTES :
*       These functions are used for initializing TLS buffer and
*       segment vector. It also deallocate the allocated TLS.
*
* 
* AUTHOR :    Sharad Jain        START DATE :    25 Apr 19
*
* CHANGES :
*
* VERSION DATE    WHO     DETAIL
* 0.1     25Apr19 SHJ     Initial version
*
*********************************************************************H*/




#include <stdio.h>
#include <string.h>
#include "ns_alloc.h"
#include "util.h"
#include "ns_log.h"

#include "ns_tls_utils.h"
#include "ns_trace_level.h"
#include "nslb_alloc.h"

/* Global variable for managing thread local storage. Bug - 57213 */
__thread NSTLS g_tls = {0};  // Must be set to 0/NULL


/* Function to initialize NSTLS thread local storage buffer */
void ns_tls_init(int buffer_size)
{
  //This buffer is for logs parallel with actual feature; debug_log, trace_log etc.
  if(g_tls.log_buffer_size < buffer_size)
  {
    g_tls.log_buffer_size = buffer_size;
    /* realloc is used inplace of MY_REALLOC as later is calling END_TEST_RUN when realloc gets failed */
    g_tls.log_buffer = realloc(g_tls.log_buffer, g_tls.log_buffer_size);
    if(g_tls.log_buffer == (void *) 0)
    {
      /* To Do: In case if realloc gets failed what action to be performed */
      fprintf(stderr, "CavErr[1000002]: Unable to allocate memory for thread local storage (TLS) (size = %d)\n", buffer_size);
      exit(-1);
    }
  }
  
  NSDL2_MISC(NULL, NULL, "Method called, size = %d", buffer_size);
  if(g_tls.buffer_size < buffer_size)
  { 
    g_tls.buffer_size = buffer_size;
    /* realloc is used inplace of MY_REALLOC as later is calling END_TEST_RUN when realloc gets failed */
    g_tls.buffer = realloc(g_tls.buffer, g_tls.buffer_size); 
    if(g_tls.buffer == (void *) 0)
    {
      /* To Do: In case if realloc gets failed what action to be performed */
      fprintf(stderr, "CavErr[1000002]: Unable to allocate memory for thread local storage (TLS) (size = %d)\n", buffer_size);
      exit(-1);
    }
    NSDL2_MISC(NULL, NULL, "TLS buffer allocated %p, size = %d", g_tls.buffer, buffer_size);
  }
  else
  {
    NSTL1(NULL, NULL, "TLS buffer buffer size(%d) >= buffer size(%d), no need to alloacate", g_tls.buffer, buffer_size);    
  }
} 

/* Function to initialize NSTLS thread local segment vector and free_array.
   It checks for the size required to allocate is less than segment
   vector size in order to do ALLOC */
void ns_tls_init_seg_vector(int seg_vector_size)
{  
  NSDL2_MISC(NULL, NULL, "Method called, size = %d", seg_vector_size);
  if(g_tls.ns_iovec.tot_size < seg_vector_size)
  {
    NS_REALLOC_IOVEC(g_tls.ns_iovec, seg_vector_size); 
  }
}

/* Deallocating buffer in NSTLS structure */
void ns_tls_free()
{
  NSDL2_MISC(NULL, NULL, "Method called");
  FREE_AND_MAKE_NULL(g_tls.buffer, "g_tls.buffer", -1);
  FREE_AND_MAKE_NULL(g_tls.log_buffer, "g_tls.log_buffer", -1);
  FREE_AND_MAKE_NULL(g_tls.ns_iovec.vector, "g_tls.ns_iovec.vector", -1);
  FREE_AND_MAKE_NULL(g_tls.ns_iovec.flags, "g_tls.ns_iovec.flags", -1);
}
