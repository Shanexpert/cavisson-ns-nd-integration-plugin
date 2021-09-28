/************************************************************************************************************
 *  Name            : ns_pause_test.c
 *  Purpose         : To pause a running test run 
 *  Initial Version : Saturday, July 16 2011
 *  Modification    : -
 *  Usage           : nsu_pause_test -t <testrun> [-d <HH:MM:SS>] [ -i < time in seconds>]
 *                    time with -i option means we have to exit after this time
 *
 ***********************************************************************************************************/

#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include "nslb_sock.h"
#include "nslb_util.h"
#include "ns_msg_def.h"

//#include "ns_vuser_trace.h"

static void exit_on_sig_alarm(int sig)
{
  fprintf(stderr, "Fail: Error in starting virutal user trace. Timeout in getting response from netstorm\n");
  exit (-1);
}

static void usage(char *err_msg)
{
  fprintf(stderr, "%s", err_msg);
  fprintf(stderr, "Usage:\n    nsu_user_trace -t <test run number> -g <group>\n");
  exit(-1);
}

static int read_msg_from_parent(int my_fd, int echo_or_not)
{
  int read_bytes_remaining = sizeof(User_trace);
  char read_buf[read_bytes_remaining + 1];
  int read_offset = 0;
  int bytes_read = 0;

  memset(read_buf, 0, read_bytes_remaining + 1);
  while (read_bytes_remaining) /* Reading rest of the message */
  {
     bytes_read = read (my_fd, read_buf + read_offset, read_bytes_remaining);

     if(bytes_read == 0)
     {
       return 0;
     }

     if(bytes_read < 0)
     {
       continue;
     }

     read_bytes_remaining -= bytes_read;
     read_offset += bytes_read;
  }
  
  if(echo_or_not) //Need to show msg on console
    fprintf(stdout, "%s\n", ((User_trace *)read_buf)->reply_msg);

  return(((User_trace *)read_buf)->reply_status); 
}
                                              
int main(int argc, char *argv[])
{
  int sock_fd, t_flag = 0, grp_flag = 0;
  // duration = 0;
  char grp[256] = "\0";
  User_trace msg;
  int alarm_time = 15;  // default timeout is 15 seconds; will be overwritten if given with -i
  int server_port = -1;
  char err_msg[1024]="\0";
  char c;
  int trnum;
  //int debug_level;
  pid_t ns_pid;
  int ret = 0;

  while ((c = getopt(argc, argv, "g:t:i:D:")) != -1) {
    switch(c) {

    case 'g':
      grp_flag++;
      strcpy(grp, optarg);
      break;

    case 'i':
      alarm_time = atoi(optarg);
      break;

    case 't':
      t_flag++;
      trnum = atoi(optarg);
      break;

    case 'D':
      break;

    default:
      usage("Invalid argument");
    }
  }

  if(!grp_flag)
  {
    usage("Group name argument is missing");
  }

  if(!strcmp(grp, "ALL")){
    usage("Group name can not be ALL");
  }

  if(t_flag == 1)
  {
    ns_pid = get_ns_pid(trnum); 

    if(is_test_run_running(ns_pid) != 0)
    {
      fprintf(stderr, "Error: Test run %d is not running.\n", trnum);
      exit (-1);
    }
  }
  else
  {
    usage("Test run argument is missing");
  }

  server_port = get_server_port(trnum);
  if(server_port == -1)
  {
    fprintf(stderr, "Error: NetStorm Port is not available. It may be possible that test is in before start phase.\n");
    exit (-1);
  }

  //printf("server_port = %d\n", server_port);
  memset(&msg, 0, sizeof(User_trace));
  msg.child_id = 255;
  msg.opcode = VUSER_TRACE_REQ;
  msg.msg_len = sizeof(msg) - sizeof(int);
  strcpy(msg.grp_name, grp);
  msg.testidx = trnum;
  //printf("data = %d\n", msg.data);

  if(alarm_time > 0)
  {
    alarm(alarm_time);
    (void) signal( SIGALRM, exit_on_sig_alarm);
  }

  sock_fd = nslb_tcp_client_ex("127.0.0.1", server_port, 10, err_msg);
  if(sock_fd < 0)
  {
    fprintf(stderr, "%s\n", err_msg);
    exit (-1);
  }
  //printf("sock_fd = %d\n", sock_fd);

  if (write(sock_fd, &msg, sizeof(User_trace)) == -1)
  {
    fprintf(stderr, "Error: Unable to send message to netstorm.\n");
    exit (-1);
  }

  // Read reply from netstorm and echo output on the console
  //if(read_msg_from_parent(sock_fd, 1) != 0)
  //{
   // fprintf(stderr, "Error: No reply from netstorm.\n");
    //exit (-1);
  //}

  ret = read_msg_from_parent(sock_fd, 1);
  close(sock_fd);
  return ret;
}
