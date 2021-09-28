#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include<sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include "nslb_util.h"
#include "nslb_sock.h"

static void usage(char *err) {
 fprintf(stdout, "Error: %s\n", err);
 fprintf(stdout, "Usage:\n     ndi_send_msg <server> <port> <message>\n");
 exit(1);

}

static int ns_epoll_init(int max_fds)
{
  int v_epoll_fd;
  if ((v_epoll_fd = epoll_create(max_fds)) == -1) {
    fprintf(stderr, "ns_epoll_init, epoll_create: err = %s", nslb_strerror(errno));
    exit (-1);
  }
  return(v_epoll_fd);
}

inline int add_select(int v_epoll_fd, int fd, int event)
{
  struct epoll_event pfd;

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.fd = fd;
  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
    fprintf(stderr, "add_select epoll add: err = %s", nslb_strerror(errno));
    return -1;
  }
  return 0;
}

/* This function will take three arguments:
 * Ip address, server port and the message that is to be send.
 */
int send_message(char *ip_addr, int server_port, char *msg)
{
  int fd;
  char err_msg[1024] = "\0";

  /* Here we are creating the connection to the ip and port given.
   * And the file descriptor is returned.
   * If file descriptor is -1 it means connection was not formed with the ip and port given.
   */

  fd =  nslb_tcp_client_ex(ip_addr, server_port, 10000, err_msg);
 // printf("\nFD : %d\n",fd);
  if(fd == -1)
  {
    fprintf(stderr, "\nError: Error in connection with Server, host = %s, port = %d, Error = '%s'\n", ip_addr, server_port, err_msg);
    return -1;
  }

  /* Here the length of the message is stored in a variable write_bytes_remaining.
   */
  int len = strlen(msg);
  int write_bytes_remaining = len;
  int write_offset = 0;
  int bytes_sent = 0;
  int data = len;

  /* Now the loop will be executed here and will continue till all the bytes are send.
   */
  while(write_bytes_remaining) {

    if(data > write_bytes_remaining)
       data = write_bytes_remaining;

    if ((bytes_sent = send (fd, msg + write_offset, data, 0)) < 0)//Sending the message
    {
      if(errno == EINTR)
        continue;
      else
        fprintf(stderr,"\nError: Failed to send message. Error = '%s'\n", nslb_strerror(errno));
        return -1; 
    }

    write_offset += bytes_sent;
    write_bytes_remaining -= bytes_sent;

  }
  return fd;
}


int main(int argc, char *argv[])
{
  if(argc != 4)
    usage("Invalid number of arguments");

  //Check Port is numeric or not
  if(!ns_is_numeric(argv[2]))
  {
    usage("\nServer port is not numeric\n");   
  }
  
  if ( strlen(argv[3]) == 0 )
  {
    fprintf(stderr,"\nError: Empty string not allowed to send\n"); 
    exit(1);
  }
  int fd = send_message(argv[1], atoi(argv[2]), argv[3]);
  
  if(fd == -1)
    return -1;

  //wait for response
  static struct epoll_event pfds;
  int epoll_fd;
  int pfds_idx; 
  int event_count;
  int byte_read;
  char read_buf[10 * 1024];
  
  epoll_fd = ns_epoll_init(1);
  add_select(epoll_fd, fd,  EPOLLIN | EPOLLHUP);
  for(;;)
  {
    event_count = epoll_wait(epoll_fd, &pfds, 1, 10000);
    if(event_count < 0 ) {
      continue;
    }
    for(pfds_idx = 0; pfds_idx < event_count; pfds_idx++)
    {
      if(pfds.events & EPOLLIN)
      {
        byte_read = read(fd , read_buf, 10 * 1024 - 1);
        if(byte_read == 0) 
        {
          close(fd);
          return 0;
        }
        if(byte_read < 0)
          continue;

        read_buf[byte_read] = '\0';
        fprintf(stderr, "%s", read_buf);
      }
      else if(pfds.events & EPOLLHUP)
      {
        printf("Find epoll hand event\n");
        return -1;
      }
    }
  }
  return 0;
}
