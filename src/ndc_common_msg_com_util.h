#ifndef NDC_COMMON_MSG_COM_UTIL_H
#define NDC_COMMON_MSG_COM_UTIL_H

#include <stdlib.h>
#include "lps/lps_log.h"
#define ERROR_BUF_SIZE 255
#define DELTA_PATTERN_ENTRIES 5
#define LPS_LOGS 0x01
#define NDC_LOGS 0x02


#define MY_MALLOC(new, size, msg, index) {                              \
    if (size < 0)                                                       \
      {                                                                 \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", (int )(size), index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        LPSDL1(NULL, "Trying to malloc a 0 size for index %d", index); \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          fprintf(stderr, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, index); \
        }                                                               \
        LPSDL1(NULL, "MY_MALLOC'ed (%s) done. ptr = $%p$, size = %d for index %d", msg, new, (int)size, index); \
      }                                                                 \
  }

#define MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
      fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int )(size), index); \
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
        fprintf(stderr, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, (int )index); \
      }  \
      LPSDL1(NULL, "MY_REALLOC'ed (%s) done. ptr = $%p$, size = %d for index %d", msg, buf, (int)size, index); \
    } \
}

#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    LPSDL1(NULL, "MY_FREE'ed (%s) done. Freeing ptr = $%p$ for index %d", msg, to_free, index); \
    free((void*)to_free);  \
    to_free = NULL; \
  } \
}

#define MALLOC_AND_COPY(src, dest, size, msg, index)   \
{                        \
  MY_MALLOC(dest, size, msg, index);  \
  strcpy(dest, src);     \
}

#define REALLOC_AND_COPY(src, dest, size, msg, index)   \
{                        \
  MY_REALLOC(dest, size, msg, index);  \
  strcpy((char *)dest, (char *)src);     \
}

inline int add_select(int v_epoll_fd, int fd, int event);
extern int create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern int create_table_entry_ndc(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern int create_ctrl_con_entry_table(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern int create_nd_con_table_entry(int *row_num, int *total, int *max, int size, char *name);
extern void remove_select(int v_epoll_fd, int fd);
extern int send_message(int fd, char *msg, int len);
extern void setSoKeepalive(int fd);
extern int mod_select(int v_epoll_fd, int fd, int event);
extern int ns_log_event(char *severity, char *event_msg, int fd);
extern char *decode(char *input);
#endif
