#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <stdlib.h>

#include "ns_string.h"
#include "nslb_sock.h"
#include "ns_alloc.h"
#include "ns_exit.h"
//#include "ns_child_msg_com.h"
//#include "ns_sock_com.h"

static int red_connect_time_out = 10000;
static int red_rep_time_out = 5000;

//extern void end_test_run();

/*#define MY_MALLOC(new, size, msg, index) {                              \
    if (size < 0)                                                       \
      {                                                                 \
        fprintf(stderr, "Trying to malloc a negative size (%d) for index %d\n", (int )(size), index); \
      }                                                                 \
    else if (size == 0)                                                 \
      {                                                                 \
        new = NULL;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        new = (void *)malloc( size );                                   \
        if ( new == (void*) 0 )                                         \
        {                                                               \
          fprintf(stderr, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, index); \
        }                                                               \
      }                                                                 \
  }
*/

/* Function:- ns_epoll_init
              Initialize the epoll
   Input   :-  
   Outout  :-  
   Issues  :- */

int epoll_init(int max_fds)
{
  int v_epoll_fd;
  //LPSDL1(NULL, "Method called, Create epoll with total no = %d ", max_fds);
  if ((v_epoll_fd = epoll_create(max_fds)) == -1) {
    //LPSDL1(NULL, "ns_epoll_init, epoll_create: err = %s", nslb_strerror(errno));
    NS_EXIT (-1, "epoll_create() failed.");
  }
  //LPSDL1(NULL, "Method exiting with  v_epoll_fd = %d ", v_epoll_fd); 
  return(v_epoll_fd);
}


/* Function:- add_select
              Add fd in epoll
   Input   :-  fd, 
               event, on which event you wont to add fd
   Outout  :-  
   Issues  :- */
inline int
add_select_ex(int v_epoll_fd, int fd, int event)
{
  struct epoll_event pfd;

  //LPSDL1(NULL,"Method called. Adding %d for event=%x", fd, event);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.fd = fd;
  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
    //hpd_error_log(0, 0, _FL_, "add_select", NULL, "epoll add: err = %s", nslb_strerror(errno));
    //LPSDL1(NULL, "add_select epoll add: err = %s", nslb_strerror(errno));
    return -1;
  }
  return 0;
}


void ns_set_red_connect_time_out(int time_out)
{
  red_connect_time_out = time_out;
}
void ns_set_red_rep_time_out(int time_out)
{
  red_rep_time_out = time_out;
}



static void write_msg_on_fd(int fd, char *buf, int num_partial) {

  int len = strlen(buf);
  int write_bytes_remaining = len;
  int data = len;
  int write_offset = 0;
  int bytes_writen = 0;
  short interval = 0;
 
  //fprintf(stderr, "buf = %s, len = %d\n", buf, len);
  if(num_partial)
  {
    data = len / num_partial;
    interval = 2;
    if(!data)
      data = len;
  }
  

  //fprintf(stderr, "data = %d\n", data);
  while(write_bytes_remaining) {
    //printf("Reamining = %d\n", write_bytes_remaining);
    if(data > write_bytes_remaining)
       data = write_bytes_remaining;
       
    //if ((bytes_writen = write (fd, buf + write_offset, write_bytes_remaining)) < 0)
    //if ((bytes_writen = write (fd, buf + write_offset, data)) < 0)
    if ((bytes_writen = send (fd, buf + write_offset, data, 0)) < 0)
       continue;
    else if(bytes_writen == 0)
       return;

    write_offset += bytes_writen;
    write_bytes_remaining -= bytes_writen;
    /*Doing defoult sleep time 1, because its make core in case of num_partial 0, 
     *We are doing interval 0 in cale of num_partial 0 */
    sleep(interval);
  }

}

//#define MAX_READ_LENGTH 64 * 1024
#define MAX_READ_LENGTH 1024
char *ns_red_client(char *ip, int port, char *req_buf, int num_partial)
{
  int red_client_fd;
  char err_msg[1024] = "\0";

  red_client_fd =  nslb_tcp_client_ex(ip, port, red_connect_time_out, err_msg);

  if(red_client_fd == -1)
  {
    fprintf(stderr, "Error in connection with RED server, ip = %s, port = %d, %s\n", ip, port, err_msg);
    return NULL;
  }

  write_msg_on_fd(red_client_fd, req_buf, num_partial);
  /*if (send(red_client_fd, req_buf, strlen(req_buf), 0) != strlen(req_buf))
  {
    fprintf(stderr, "Fail: Request to RED Server ip = %s\n", ip);
    close(red_client_fd);
    red_client_fd = -1;
    return NULL; //error
  }*/

  static int server_epoll_fd;
  unsigned int fds_count = 1;
  struct epoll_event pfds; 
  int event_count;
  int bytes_read = 0;
  int total_read = 0;
  char *read_msg;
  char *tmp_ptr;

  MY_MALLOC(read_msg, MAX_READ_LENGTH, "read_msg", -1); 
 
  server_epoll_fd = epoll_init(fds_count);

  if(add_select_ex(server_epoll_fd, red_client_fd,  EPOLLIN | EPOLLHUP) == -1)
  {
    NS_EXIT(-1, "add_select_ex() failed.");
  }
  
  event_count =  epoll_wait(server_epoll_fd, &pfds, fds_count, red_rep_time_out);
  if ( event_count < 0 ) {
    if (errno != EINTR)
      fprintf(stderr, "RED Server select/epoll_wait: err = %s", nslb_strerror(errno));
      return NULL;;
  }
  //fprintf(stderr, "fiend event on fd red_client_fd = %d, pfds.events = %d, EPOLLIN = %d\n", 
  //            pfds.data.fd, pfds.events, EPOLLIN);
  if(pfds.events & EPOLLIN && pfds.data.fd == red_client_fd)
  {
  //fprintf(stderr, "fiend event on fd red_client_fd = %d\n", pfds.data.fd);
    while(1)
    {
      bytes_read = read (red_client_fd, read_msg + total_read, MAX_READ_LENGTH - total_read);
      if (bytes_read == 0)
      { 
        //Connection closed
        //printf("conn closed by client\n");
        close(red_client_fd);
        red_client_fd = -1;
        return NULL; //error
      }
      else if (bytes_read < 0)
      {
        if (errno == EAGAIN)
        {
          fprintf(stderr, "Read: Got EAGAIN");
          continue;
          //return NULL;
        } else if (errno == EINTR) {   /* this means we were interrupted */
          continue;
        }
        else
        {
          fprintf(stderr, "Error in reading starting 4 bytes. error = %s, FD = %d\n", nslb_strerror(errno), red_client_fd);
          close(red_client_fd);
          red_client_fd = -1;
          return NULL; //error
        }
      }
 
      read_msg[bytes_read] = '\0';
      total_read += bytes_read;
      if((tmp_ptr = strchr(read_msg, '\n')))
      {
        close(red_client_fd);
        red_client_fd = -1; 
        return read_msg;
      }
    }
  }
  close(red_client_fd);
  red_client_fd = -1; 
  return NULL;
}


char *ns_red_client_ex(char *ip, int port, char *req_buf)
{
  return (ns_red_client(ip, port, req_buf, 0));
}

char *ns_red_client_partial_ex(char *ip, int port, char *req_buf, int num_partial)
{
  return (ns_red_client(ip, port, req_buf, num_partial));
}

void ns_set_red_connect_time_out_ex(int time_out)
{
  ns_set_red_connect_time_out(time_out);
}

void ns_set_red_rep_time_out_ex(int time_out)
{
  ns_set_red_rep_time_out(time_out);
}
