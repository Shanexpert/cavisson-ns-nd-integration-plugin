#ifndef NS_SOCK_LIST_H
#define NS_SOCK_LIST_H

#include <sys/epoll.h>
#include "util.h"

typedef struct SOCK_data
{
  //struct in_addr ip_addr; changed to ipv6 below 5/29/07
  char con_type;
  int fd;
  void *cptr;
  struct SOCK_data *next;
  unsigned short net_idx;
  int slf_idx;
} SOCK_data;

//void init_sock_list();
extern void init_sock_list(int epfd, struct epoll_event * events, int maxevents);
extern int get_sock_from_list ();
extern SOCK_data *fd_table;
extern void set_high_perf_mode(char* value);
extern void set_max_sock(char* value);
#endif
