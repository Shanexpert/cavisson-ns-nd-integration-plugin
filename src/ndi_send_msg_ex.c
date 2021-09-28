#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "nslb_util.h"
#include "nslb_sock.h"

#define NDC_CONF "ndc.conf"
#define MAX_BUFFER_LEN 512
#define MAX_LOG_FILE_SIZE 1024*1024

char log_buf[MAX_BUFFER_LEN];

static void usage(char *err) 
{
  fprintf(stdout, "Error: %s\n", err);
  fprintf(stdout, "Usage:\n     ndi_send_msg <server> <port> <message>\n");
  exit(1);
}
 
void check_log_file_size(char *log_path)
{
  int status;
  struct stat stat_buf;
  char log_path_prev[1024];

  snprintf(log_path_prev, 1024, "%s.prev", log_path);
  if((stat(log_path, &stat_buf) == 0) && (stat_buf.st_size > MAX_LOG_FILE_SIZE)) 
    status = rename(log_path, log_path_prev);
  if(status < 0)
  {  
    fprintf(stderr, "Error in moving '%s' file to '%s' file, err = %s\n", log_path, log_path_prev, nslb_strerror(errno));
  }
}

void dump_log_data(FILE *out, char *log_data)
{
  char ts_buf[MAX_BUFFER_LEN];
  time_t rawtime = time(NULL);
  if (out == NULL)
    return ;
  if (rawtime == -1)
  {
     fprintf(out, "time failed |%s \n",log_data);
     return;
  }
  struct tm *ptm = localtime(&rawtime);
  if (ptm == NULL) 
  {
    fprintf(out, " localtime failed |%s \n",log_data);
    return;
  }
  strftime(ts_buf, MAX_BUFFER_LEN, "%d/%m/%Y %H:%M:%S", ptm);
  fprintf(out, "%s |%s\n",ts_buf,log_data);
  return ;
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
int send_message(char *ip_addr, int server_port, char *msg, FILE *log_fp)
{
  int fd;
  char err_msg[1024] = "\0";

  /* Here we are creating the connection to the ip and port given.
   * And the file descriptor is returned.
   * If file descriptor is -1 it means connection was not formed with the ip and port given.
   */

  snprintf (log_buf, MAX_BUFFER_LEN, "Going to Make Connection with ip = %s port  = %d", ip_addr, server_port);
  dump_log_data(log_fp, log_buf);

  fd =  nslb_tcp_client_ex(ip_addr, server_port, 10000, err_msg);
  
  if(fd == -1)
  {

    fprintf(stderr, "\nError: Error in connection with Server, host = %s, port = %d, Error = '%s'\n", ip_addr, server_port, err_msg);
    snprintf (log_buf, MAX_BUFFER_LEN, "Error: Error in connection with Server, host = %s, port = %d, Error = '%s'", ip_addr, server_port, err_msg);
    dump_log_data(log_fp, log_buf);
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
      { 
	snprintf (log_buf, MAX_BUFFER_LEN, "Error in sending message. Error = '%s' ",nslb_strerror(errno));
        dump_log_data(log_fp, log_buf);
        fprintf(stderr,"\nError: Failed to send message. Error = '%s'\n", nslb_strerror(errno));
        return -1;
      }
      
    }

    write_offset += bytes_sent;
    write_bytes_remaining -= bytes_sent;
  }
  return fd;
}


int main(int argc, char *argv[])
{
  char fname[MAX_BUFFER_LEN] = "";
  char ndc_port[MAX_BUFFER_LEN] = "";
  char ndc_ip[MAX_BUFFER_LEN] = "127.0.0.1";
  char *ptr = NULL;
  int ret = 0;
  int fd = -1;
  char log_path[512];
  char ndc_root[512] = "";
  FILE *log_fp = NULL;
  int len = 0;

  len = snprintf(ndc_root, 512, "%s", getenv("NS_WDIR"));

  if(ndc_root[0] == 0)
  {
    fprintf(stderr, "Error : NS_WDIR variable not set \n");
    return -1;
  }

  snprintf(ndc_root + len, 512 - len, "/ndc" );

  if(argc == 4) // debug mode is off
  {
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
    fd = send_message(argv[1], atoi(argv[2]), argv[3], log_fp);
  /*Calling send message function*/
    if(fd == -1)
      return -1;
  }

  else if(argc == 5) // debug mode is provided
  {
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
    
    if (atoi(argv[4])) //if debug mode is activated other than 0
    {
      snprintf(log_path, 512, "%s/logs/ndi_send_msg_ex.log", ndc_root);
      check_log_file_size(log_path); //check log file size
      log_fp = fopen(log_path, "a+");
      if (log_fp == NULL)
      {
        fprintf (stderr, "Unable to open %s/logs/ndi_send_msg_ex.log", ndc_root);
	return -1;
      }
      snprintf(log_buf,MAX_BUFFER_LEN,"Log File open succeded"); 
      dump_log_data(log_fp,log_buf);
    }

    fd = send_message(argv[1], atoi(argv[2]), argv[3], log_fp);
  /*Calling send message function*/
    if(fd == -1)
      return -1;
  }

  else if(argc == 3)
  {
    if(!ns_is_numeric(argv[1]))
    {
      usage("\nServer port is not numeric\n");
    }

    if ( strlen(argv[2]) == 0 )
    {
      fprintf(stderr,"\nError: Empty string not allowed to send\n");
      exit(1);
    }
    
    fd = send_message(ndc_ip, atoi(argv[1]), argv[2], log_fp);

    if(fd == -1)
      return -1;
  }
  else if(argc == 2)
  {
    if ( strlen(argv[1]) == 0 )
    {
      fprintf(stderr,"\nError: Empty string not allowed to send\n");
      exit(1);
    }
    
    sprintf(fname, "%s/ndc/conf/%s", getenv("NS_WDIR"), NDC_CONF);

    ret = nslb_parse_keyword(fname, "PORT", ndc_port);

    if(ret == -1)
    {
      if(nslb_parse_keyword(fname, "NDC_PORT", ndc_port) == -1)
      {
        fprintf(stderr,"\nError: Keyword not found in conf file %s. Error = '%s'\n", fname, nslb_strerror(errno));
        exit(1);
      }
    }
    
    if((ptr = strchr(ndc_port, ':')))
    {
      *ptr = '\0';
      ptr++;
      sprintf(ndc_ip, "%s", ndc_port);
      sprintf(ndc_port, "%s", ptr);
    }
    else
      sprintf(ndc_port, "%s", ndc_port);

    if(!ns_is_numeric(ndc_port))
    { 
      usage("\nServer port is not numeric\n");
    }

    fd = send_message(ndc_ip, atoi(ndc_port), argv[1], log_fp);
  /*Calling send message function*/
    if(fd == -1)
      return -1;
  }
  else
    usage("Invalid number of arguments");

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
 	  if (log_fp)
	    fclose(log_fp);
          return 0;
        }
        if(byte_read < 0)
          continue;

        read_buf[byte_read] = '\0';
        fprintf(stdout, "%s", read_buf);
        snprintf(log_buf, MAX_BUFFER_LEN, "Received Message =  %s", read_buf); 
        dump_log_data(log_fp, log_buf);

        if(strchr(read_buf, '\n'))
        {
          close(fd);
 	  if (log_fp)
	    fclose(log_fp);
          return 0;
        }
      }
      else if(pfds.events & EPOLLHUP)
      {
        printf("Find epoll hand event\n");
        return -1;
      }
    }
  }

  //fprintf(stdout, "\nMessage successfully send to lps having host = %s\t and port = %d\n", argv[1], atoi(argv[2]));
  return 0;
}
