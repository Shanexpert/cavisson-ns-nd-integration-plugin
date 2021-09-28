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

 char red_buf[1024];

  while(1)
  {
    if((bytes_sent = read (fd, red_buf, 1023)) == -1)
    {
     // printf("\nbytes_sent=%d\n", bytes_sent);
      continue;
    }
    //printf("\nbytes_sent=%d\n", bytes_sent);
    break;
  }

  if(bytes_sent == 0)
  {
    fprintf(stderr, "\nConnection closed by NDCollector");
  }
  else{
    red_buf[bytes_sent] = '\0';
    fprintf(stdout, "%s", red_buf);
  }

  close(fd);
  return 0;
}


int main(int argc, char *argv[])
{
  //Checking Number of arguments
  //int i;
  //printf("argc = %d\n", argc);
  //for(i = 0; i < argc; i++)
   // printf("argv[%d] = %s\n", i, argv[i]);
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
  /*Calling send message function*/
  if(send_message(argv[1], atoi(argv[2]), argv[3]) == -1)
    return -1;

  //fprintf(stdout, "\nMessage successfully send to lps having host = %s\t and port = %d\n", argv[1], atoi(argv[2]));
  return 0;
}

