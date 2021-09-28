/*
 * This file have API which are used for internal testing only
 */

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
#define MAX_READ_SIZE 64*1024
static void write_msg_on_fd_num_block(int fd, char **buf, int arr_len, int delay) {

  int i;
  for(i = 0; i < arr_len; i++)
  {
    int len = strlen(buf[i]);
    int write_bytes_remaining = len;
    int data = len;
    int write_offset = 0;
    int bytes_writen = 0;
  
    //fprintf(stderr, "buf = %s, len = %d\n", buf[i], len);
    //fprintf(stderr, "data = %d\n", data);
    while(write_bytes_remaining) {
      //printf("Reamining = %d\n", write_bytes_remaining);
      if(data > write_bytes_remaining)
         data = write_bytes_remaining;
         
      //if ((bytes_writen = write (fd, buf + write_offset, write_bytes_remaining)) < 0)
      //if ((bytes_writen = write (fd, buf + write_offset, data)) < 0)
      if ((bytes_writen = send (fd, buf[i] + write_offset, data, 0)) < 0)
         continue;
      else if(bytes_writen == 0)
         return;
 
      write_offset += bytes_writen;
      write_bytes_remaining -= bytes_writen;
      /*Doing defoult sleep time 1, because its make core in case of num_partial 0, 
       *We are doing interval 0 in cale of num_partial 0 */
      usleep(delay * 1000);
    }
  }
}

char *ns_send_data_with_delay(char *ip,int port, int arr_len, char **req_buf, int delay)
{

  int red_client_fd;
  char err_msg[1024] = "\0";

  red_client_fd =  nslb_tcp_client_ex(ip, port, red_connect_time_out, err_msg);

  if(red_client_fd == -1)
  {
    fprintf(stderr, "Error in connection with Server, ip = %s, port = %d, %s\n", ip, port, err_msg);
    return NULL;
  }

  //fprintf(stderr, "buf = %s, len = %d\n", req_buf[0], arr_len);
  write_msg_on_fd_num_block(red_client_fd, req_buf, arr_len, delay);
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

  MY_MALLOC(read_msg, MAX_READ_SIZE, "read_msg", -1); 
 
  server_epoll_fd = epoll_init(fds_count);

  if(add_select_ex(server_epoll_fd, red_client_fd,  EPOLLIN | EPOLLHUP) == -1)
  {
    NS_EXIT(-1, "add_select_ex() failed.");
  }
  
  event_count =  epoll_wait(server_epoll_fd, &pfds, fds_count, 10000);
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
    bytes_read = read (red_client_fd, read_msg + total_read, MAX_READ_SIZE - total_read);
    if (bytes_read < 0)
    {
      if (errno == EAGAIN)
      {
        fprintf(stderr, "Read: Got EAGAIN");
        close(red_client_fd);
        red_client_fd = -1; 
        return NULL;
        //return NULL;
      } else if (errno == EINTR) {   /* this means we were interrupted */
        close(red_client_fd);
        red_client_fd = -1; 
        return NULL;
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
    close(red_client_fd);
    red_client_fd = -1; 
    return read_msg;
  }
  close(red_client_fd);
  red_client_fd = -1; 
  return NULL;
}

