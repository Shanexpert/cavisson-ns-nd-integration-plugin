#ifndef _NS_TLS_UTILS_H
#define _NS_TLS_UTILS_H

#include <setjmp.h>
#include "ns_iovec.h"

/* Defines for thread local storage. Bug - 57213 */

#define VUSER_THREAD_LOCAL_BUFFER_SIZE 8000+1
#define VUSER_THREAD_BUFFER_SIZE 64000+1

/*Multiple threads are used while starting test
  Need to differentiate between the caller*/
#define IS_PARENT          1   /*Parent before creation DH thread*/
#define IS_NVM             2   /*NVM not in thread mode*/
#define IS_NVM_THREAD      3   /*NVM in thread mode*/
#define IS_DATA_HANDLER    4   /*Data handler thread*/
#define IS_PARENT_AFTER_DH 5   /*Parent after creation of DH thread*/
#define IS_DB_THREAD       6   /*Database creation handler thread*/

/* This structure is used for Thread local storage */
typedef struct
{
   char *buffer; // All thread local static buffer will now be allocated dynamically using this element.
   int buffer_size; // to maintain and get the size of buffer at any instant of time.
   char *log_buffer;
   int log_buffer_size;
   jmp_buf jmp_buffer; //to save and restore the environment during thread waiting on semaphore
   unsigned char caller_type;
   void *thread_data;
   void *vptr;
   NSIOVector ns_iovec;
}NSTLS;

/* To initialize and destroy thread local storage. Bug - 57213 */
extern void ns_tls_init(int buffer_size);
extern void ns_tls_free();
/* For initializing segment vector */
extern void ns_tls_init_seg_vector(int seg_vector_size);
extern __thread NSTLS g_tls;

/* Macro for freeing thread local storage  */
#define TLS_FREE_AND_RETURN(X)	\
	ns_tls_free();	\
	return X;

#define TLS_FREE_AND_EXIT(X) \
	ns_tls_free();	\
        pthread_exit(X);

#define set_thread_specific_data(X) g_tls.thread_data = X
#define get_thread_specific_data g_tls.thread_data

#define SET_CALLER_TYPE(X) g_tls.caller_type = X

#define ISCALLER_PARENT          (g_tls.caller_type == IS_PARENT)
#define ISCALLER_NVM             (g_tls.caller_type == IS_NVM)
#define ISCALLER_NVM_THREAD      (g_tls.caller_type == IS_NVM_THREAD)
#define ISCALLER_DATA_HANDLER    (g_tls.caller_type == IS_DATA_HANDLER)
#define ISCALLER_PARENT_AFTER_DH (g_tls.caller_type == IS_PARENT_AFTER_DH)
#define ISCALLER_DB_THREAD       (g_tls.caller_type == IS_PARENT_DB_THREAD)

#ifdef CAV_MAIN
#define TLS_SET_VPTR(vptr) \
{\
  g_tls.vptr = vptr; \
  cav_main_set_global_vars(vptr->sm_mon_info); \
  NSDL2_MISC(NULL, NULL, "g_tls.vptr = %p, vptr = %p", g_tls.vptr, vptr);\
}

#define TLS_GET_VPTR()   g_tls.vptr;  cav_main_set_global_vars(((VUser*)(g_tls.vptr))->sm_mon_info)

#else
#define TLS_SET_VPTR(vptr) \
{\
  NSDL2_MISC(NULL, NULL, "g_tls.vptr = %p, vptr = %p", g_tls.vptr, vptr);\
  g_tls.vptr = vptr; \
}\

#define TLS_GET_VPTR()  g_tls.vptr

#endif


#endif
