/************************************************************************************************************
 *  Name            : ns_pause_test.c
 *  Purpose         : To pause a running test run 
 *  Initial Version : Friday, September 25 2009 
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

#include "ns_msg_def.h"
#include "util.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "nslb_msg_com.h"

extern void exit_on_sig_alarm(int sig);

static void usage()
{
  fprintf(stderr, "Usage:\n    nsu_pause_test -t <testrun> [-d <HH:MM:SS>]\n");
   exit(-1);
}

int main(int argc, char *argv[])
{
  int sock_fd, t_flag = 0, u_flag = 0, r_flag = 0, duration = 0, trnum, ns_uid, my_user_id;
  int alarm_time = 15;  // default timeout is 15 seconds; will be overwritten if given with -i
  struct passwd *user_info;
  Pause_resume msg;
  pid_t ns_pid;
  int server_port = -1;
  char err_msg[1024]="\0";
  char c;
  char user_name[128+1];
  char role[10];
  char *user_buf = NULL;
  char *role_buf = NULL;
  char *test_owner = NULL;

  memset(&msg, 0, sizeof(Pause_resume));
  msg.child_id = 255;  

  while ((c = getopt(argc, argv, "d:i:t:u:r:")) != -1) {
    switch(c) {
    case 'd':
      duration = get_time_from_format(optarg); 
      if(duration <= 0)
      {
        fprintf(stderr, "Pause is not allowed for time '%s'\n", optarg);
        exit(-1);
      }
      break;

    case 'i':
      alarm_time = atoi(optarg);
      break;

    case 't':
      t_flag++;
      trnum = atoi(optarg);    
      break;

  case 'u':
      u_flag++;
      strncpy(user_name,optarg,128);
      user_buf = user_name;    
      break;

  case 'r':
      r_flag++;
      strncpy(role,optarg,10);
      role_buf = role;
      break;

    default:
      usage();
    }
  }

  if(t_flag == 1)
  {
    ns_pid = get_ns_pid(trnum); 

    if(is_test_run_running(ns_pid) != 0)
    {
      fprintf(stderr, "Error: Test run %d is not running.\n", trnum);
      exit(-1);
    }
  }
  else
  {
    fprintf(stderr, "One test run must be given.\n");
    exit(-1);
  }

  if(u_flag > 1)
  {
    fprintf(stderr, "Only one user must be given.\n");
    exit(-1);
  }
  
  if(r_flag > 1)
  {
    fprintf(stderr, "Only one role must be given.\n");
    exit(-1);
  }

  if (( r_flag && !u_flag ) || (!r_flag && u_flag ))
  {
    fprintf(stderr, "Role(-r) should be provided with User(-u) option\n");
    exit(-1);
  }
  if (r_flag && strcmp(role_buf,"admin") && strcmp(role_buf,"normal"))
  {
    fprintf(stderr, "Role %s is not authorized to pause the test\n", role_buf);
    exit(-1);
  }
  
  ns_uid = get_process_uid(ns_pid);
  my_user_id = getuid();

  if(my_user_id == 0)
  {
    fprintf(stderr, "Usage:\n root is not allowed to execute this command\n");
    exit(-1);
  }

  if(user_buf == NULL)
  {
     user_info = getpwuid(my_user_id);
     user_buf =  user_info->pw_name;
  }
  
  test_owner = get_test_owner(trnum);
  // root check is done because GUI runs all cmd using root
  if(strcmp(user_buf, "admin") && strcmp(user_buf, "root") && (!r_flag || strcmp(role_buf,"admin")) && (my_user_id != ns_uid || strcmp(user_buf,test_owner)))
  {
    fprintf(stderr, "User '%s' is not authorized to pause test TR%d\n", user_buf,trnum);
    exit(-1);
  }

  server_port = get_server_port(trnum);
  if(server_port == -1)
  {
    fprintf(stderr, "Error: NetStorm Port is not available. It may possible test is in before start phase.\n");
    exit(-1);
  }

  //printf("server_port = %d\n", server_port);
  msg.opcode = PAUSE_SCHEDULE;
  msg.time = duration/1000; 
  strncpy(msg.cmd_owner, user_buf, 128);
  msg.msg_from = MSG_FROM_CMD;
  //printf("data = %d\n", msg.data);

  if(alarm_time > 0)
  {
    alarm(alarm_time);
    (void) signal( SIGALRM, exit_on_sig_alarm);
  }

  sock_fd = nslb_tcp_client_ex("127.0.0.1", server_port, 30, err_msg);
  if(sock_fd < 0)
  {
    fprintf(stderr, "%s\n", err_msg);
    exit(-1);
  }
  //printf("sock_fd = %d\n", sock_fd);
  msg.msg_len = sizeof(Pause_resume) - sizeof(int);
  if (write(sock_fd, &msg, sizeof(Pause_resume)) == -1)
  {
     fprintf(stderr, "Error: Unable to send message to netstorm.\n");
     exit (-1);
  }

  read_msg_from_fd(sock_fd);
  close(sock_fd);
  return 0;
}
