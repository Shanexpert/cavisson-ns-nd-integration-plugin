#ifndef CDR_MEM_H
#define CDR_MEM_H

/* File: cdr_mem.h
 * Purpose: To handle memory for cavisson data retention
 */

#include "cdr_log.h"

#include <stdlib.h>
#include <stdio.h>


#define CDR_MALLOC(ptr, size, msg) \
{ \
  char msg_buff[512]; \
  \
  if (size < 0) { \
    sprintf(msg_buff, "Trying to malloc -ve size. file: %s, line: %d, func: %s, size: %ld",  \
      __FILE__,__LINE__,(char *)__FUNCTION__, (long)(size));  \
  \
    fprintf(stderr, "%s\n", msg_buff);  \
    CDRTL1("Error: %s", msg_buff); \
    ptr = NULL; \
  } else if(size == 0) { \
    sprintf(msg_buff, "Trying to malloc 0 size. file: %s, line: %d, func: %s, size: %ld", \
          __FILE__,__LINE__,(char *)__FUNCTION__, (long)(size));  \
    CDRTL1("Error: %s", msg_buff); \
    ptr = NULL; \
  } else {\
    ptr = malloc(size); \
    if(ptr == NULL) { \
      fprintf(stderr, "malloc error: Out of memory\n"); \
      CDRTL1("Error: Out of memory"); \
      exit(1);  \
    } \
    CDRTL4("CDR_MALLOC done. size: %d, msg: %s", size, msg); \
  } \
}

#define CDR_REALLOC(ptr, size, msg)  \
{ \
  char msg_buff[512]; \
  \
  if (size < 0) { \
    sprintf(msg_buff, "Trying to realloc -ve size. file: %s, line: %d, func: %s, size: %ld",  \
      __FILE__,__LINE__,(char *)__FUNCTION__, (long)(size));  \
  \
    fprintf(stderr, "%s\n", msg_buff);  \
    CDRTL1("Error: %s", msg_buff); \
    ptr = NULL; \
  } else if(size == 0) { \
    sprintf(msg_buff, "Trying to relloc 0 size. file: %s, line: %d, func: %s, size: %ld", \
          __FILE__,__LINE__,(char *)__FUNCTION__, (long)(size));  \
    CDRTL1("Error: %s", msg_buff); \
    ptr = NULL; \
  } else {\
    ptr = realloc(ptr, size); \
    if(ptr == NULL) { \
      fprintf(stderr, "realloc error: Out of memory\n"); \
      CDRTL1("Error: Out of memory"); \
      exit(1);  \
    } \
    CDRTL4("CDR_REALLOC done. size: %d, msg: %s", size, msg); \
  } \
}

#endif
