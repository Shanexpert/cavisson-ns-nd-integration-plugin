#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "poi.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "ipmgmt_utils.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "ns_vuser.h"
#include "nslb_util.h"
#include "ns_exit.h"

extern int add_select(void* cptr, int fd, int event);
extern unsigned long long gettsc(void);

static int max_fd_tbl_sz;
static int max_fd_sz;
SOCK_data *fd_table;

void set_high_perf_mode(char* value)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  global_settings->high_perf_mode = atoi(value);

  if(global_settings->high_perf_mode)
    printf("High performance mode is set to %d\n", global_settings->high_perf_mode);
}

void set_max_sock(char* value)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called. value = %s", value);
  //this method is checking if the user enter negative value or any other special character
  //it will display error message and exit
  if(ns_is_numeric(value))
  {
    global_settings->max_sock = atoi(value); 
  }
  else
  {
    NS_EXIT(-1, "Value of MAX_SOC is not numeric");
  }
  //Add limit from 1 to 2147483647 for MAX_SOC keyword
  if ((global_settings->max_sock < 1 ) || (global_settings->max_sock > 2147483647))
  {
    NS_EXIT(-1, "MAX_SOC value cannot be less than 1 or greater than 2147483647");
  }
  NSDL2_SOCKETS(NULL, NULL, "Max socket per child = %d", global_settings->max_sock);
  
  // if(global_settings->max_sock != 10000)
    // printf("Num Sock per child = %d\n", global_settings->max_sock);
}

void inline unbind_sock (int fd)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  struct sockaddr_in c_addr;
  bzero((char *) &c_addr, sizeof(c_addr));
  c_addr.sin_family = AF_UNSPEC;
  if (connect(fd, (struct sockaddr *)&c_addr, sizeof(c_addr)) < 0)
  {
    perror("ERROR unbind");
    NS_EXIT(1, "");
  }
}

//get an srcip elemnet from head
int get_sock_from_list (int net_idx, connection *cptr)
{
  NSDL2_SOCKETS(NULL, cptr, "Method called");
  int fd, grp_idx;
  SOCK_data *cur_next, *el;

  // printf("Method starts - get_sock_from_list(net_idx = %d, cptr = %x)\n", net_idx, (unsigned int)cptr);
  
  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    GroupSettings *gset_ptr = &runprof_table_shr_mem[grp_idx].gset;  
    if (!gset_ptr->master_src_ip_table[net_idx].sock_head)
    {
      printf("Could not alloc Sock \n");
      return -1;
    }

    el = gset_ptr->master_src_ip_table[net_idx].sock_head;
    cur_next = el->next;
    if (cur_next)
      gset_ptr->master_src_ip_table[net_idx].sock_head = cur_next;
    else
      gset_ptr->master_src_ip_table[net_idx].sock_head = gset_ptr->master_src_ip_table[net_idx].sock_tail = NULL;

    el->cptr = cptr;
    fd = el->fd;
    if(!(global_settings->optimize_ether_flow & OPTIMIZE_CLOSE_BY_RST))
      unbind_sock (fd);
    if(set_socketopt_for_quickack(fd, 0) < 0) end_test_run();
    return el->fd;
  }
  return -1;
}

//add an element to tail
//Used at the time of freeing user slot
void add_sock_to_list(int fd, int shut, GroupSettings *gset)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  SOCK_data *el;
  int net_idx, ret;

  // printf("Method starts - add_sock_to_list(fd = %d, shut = %d)\n", fd, shut);
  if (shut)
  {
#if 0
    ret = shutdown(fd, SHUT_RDWR);
    //ret = shutdown(fd, 99);
    if (ret < 0)
    {
      perror("ERROR shutdown");
      //exit(1);  // Anil - Is exit to be done?
    }
#endif

    if(global_settings->optimize_ether_flow & OPTIMIZE_CLOSE_BY_RST)
    {
      unbind_sock (fd);
    }
    else 
    {
      ret = shutdown(fd, SHUT_RDWR);
      if (ret < 0)
      {
        perror("ERROR shutdown");
        //exit(1);  // Anil - Is exit to be done?
      }
    }
  }

  el = &fd_table[fd];
  el->cptr = NULL;
  net_idx = el->net_idx;

  // printf("add_sock_to_list() - Adding socket for netx_idx = %d\n", net_idx);
  // printf("add_sock_to_list() - sock_tail = %x\n", (unsigned int)(g_master_src_ip_table[net_idx].sock_tail));

  if (gset->master_src_ip_table[net_idx].sock_tail)
    gset->master_src_ip_table[net_idx].sock_tail->next = el;
  else
    gset->master_src_ip_table[net_idx].sock_head = el;

  el->next = NULL;
  gset->master_src_ip_table[net_idx].sock_tail = el;
}

static int prepare_socket(int net_idx)
{
  int fd;
  int SO_on;
  int test_bind;
  int bind_done;
  struct sockaddr_in6 sin;
  IP_data *user_ip;

  NSDL2_SOCKETS(NULL, NULL, "Method called, net_idx = %d\n", net_idx);

  user_ip = get_src_ip(NULL, net_idx);
  fd = socket( user_ip->ip_addr.sin6_family, SOCK_STREAM, 0 );

  if (fd == 0)                  /* we dont want fd to be since checking in epoll */
    fd = socket( user_ip->ip_addr.sin6_family, SOCK_STREAM, 0 );

  if ( fd < 0 )
  {
    fprintf(stderr, "socket() failed!!\n");
    end_test_run();
  }

  // Anil - Do we need to do this based on DISABLE_REUSEADDR ?
  SO_on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &SO_on, sizeof(int)) < 0)
  {
    fprintf(stderr, "Setting REUSE AADRR\n");
    end_test_run();
  }
  //Close by RST is done differently for HPM case
  //if(set_socketopt_for_close_by_rst(fd) < 0) end_test_run();
  //Done at the time get_socket_from_list
  //if(set_socketopt_for_quickack(fd, 0) < 0) end_test_run();

  /*if (!nslb_fill_sockaddr(&(sin), "192.168.101.100", 0)) {
  printf("address reolution failed\n");
  exit(1);
  }*/

  bind_done = 0;
  test_bind = 0;
  while (!bind_done)
  {
    test_bind++;
    //sin.sin_addr.s_addr = ip_addr;
    //memcpy ((char *)&(sin), (char *) &(ip_addr), sizeof(struct sockaddr_in6));
    memcpy ((char *)&(sin), (char *) &(user_ip->ip_addr), sizeof(struct sockaddr_in6));
    if (total_ip_entries)
    {
      //printf("Asking pid %d: addr = 0x%x port=%d\n", getpid(), cptr->sin.sin_addr.s_addr, vptr->user_ip->port);
      sin.sin6_port = htons(user_ip->port);
      user_ip->port++;
      if (user_ip->port > v_port_table[my_port_index].max_port)
      {
        NS_EXIT (1, "Use more IP's. Run of socket ports");
      }
    }
    if ( (bind( fd, (struct sockaddr*) &sin, sizeof(sin))) < 0 )
    {
      if (errno != EADDRINUSE)
      {
        //if (fp) fprintf (fp, " INUSE\n");
        perror( "bind" );
        NS_EXIT(1, "bind failed address is %s", nslb_sock_ntop((struct sockaddr *)&sin));
      }
      else if (((test_bind % 100) == 0) /*|| (test_bind == 1)*/)
      {
        printf("bind (%d): err=%d serr= %s binding attempt %d addr =%s \n", getpid(), errno,
        nslb_strerror(errno), test_bind, nslb_sock_ntop((struct sockaddr *)&sin));
      }
      //if (fp) fprintf (fp, " ERR\n");
    }
    else
    {
      //if (fp) fprintf (fp, " DONE\n");
      bind_done = 1;
    }
  }

  /*
  struct sockaddr_in sockname;
  int socksize;
  getsockname(cptr->conn_fd, (struct sockaddr *)&sockname, &socksize);
  printf("after bind, (pid=%d) IP=0x%x port: %hd\n", getpid(), htonl(sockname.sin_addr.s_addr), ntohs(sockname.sin_port));
  */

  /* Set the file descriptor to no-delay mode. */
  if ( fcntl( fd, F_SETFL, O_NDELAY ) < 0 )
  {
    NS_EXIT(1, "Setting fd to no-delay failed");
    perror( "NODELAY");
  }
  return fd;
}

//Initialize srcip list of netstorm  child
void init_sock_list(int epfd, struct epoll_event * events, int maxevents)
{
  int fd, num, i, net_idx = 0, grp_idx;

  NSDL2_SOCKETS(NULL, NULL, "Method called, epfd = %d. Num Sock per child = %d", epfd, global_settings->max_sock);

  for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
  {
    GroupSettings *gset_ptr = &(runprof_table_shr_mem[grp_idx].gset);  
    if(gset_ptr->master_src_ip_table == NULL)
    {
      NS_EXIT(-1, "Error: init_sock_list() - High Performace Mode cannot be used without IPs created by IP Management\n.Use IPs created                 by IP Management in the scenario file and try again.");
    }
    num = global_settings->max_sock;
 
    max_fd_sz = num;
    max_fd_tbl_sz = num + 1000;
    NSDL2_SOCKETS(NULL, NULL, "Max fd table size = %d", max_fd_tbl_sz);
    MY_MALLOC (fd_table, max_fd_tbl_sz * sizeof (SOCK_data), "fd_table", -1);
 
    //sock_head = sock_tail = NULL;
    for (i = 0; i < max_fd_tbl_sz; i++)
    {
      fd_table[i].fd = -1;
      fd_table[i].cptr = NULL;
      fd_table[i].net_idx = -1;
    }
    for (i = 0; i < num; i++)
    {
      fd = prepare_socket(net_idx);
      if (fd >= max_fd_tbl_sz)
      {
        NS_EXIT (1, "return fd =%d, more than fd table size", fd);
      }
      fd_table[fd].con_type = NS_STRUCT_TYPE_CPTR;
      fd_table[fd].fd = fd;
      fd_table[fd].cptr = NULL;
      fd_table[fd].net_idx = net_idx;
      fd_table[fd].slf_idx = i; 
      if (add_select(&fd_table[fd], fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0)
      {
        NS_EXIT(1, "Set Select failed on WRITE EVENT");
      }
      //ret = read (fd, buf, 16);
      //perror ("read from init_sock");
      add_sock_to_list(fd, 0, gset_ptr);
      net_idx++;
      net_idx %= gset_ptr->g_max_net_idx;
    }
  }
#if 0
  // All fd's are opened
  epoll_wait(epfd, events, maxevents, 1000);
#endif
}
