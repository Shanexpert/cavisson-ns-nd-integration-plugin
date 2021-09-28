#include<stdio.h>
#include<string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <regex.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "nslb_sock.h"
#include "nslb_util.h"
static int fd;
static int g_buffer_size;
static char *g_buffer = NULL;
static void usage(char *err_msg)
{
  fprintf(stderr, "%s\n", err_msg);
  exit(-1);
}

void GenUsage()
{
  fprintf(stdout, "Mandatory options are missing.\nnsi_stop_generator -t <tr_number> -g <gen_name> -d\n"  
                  "  -t => [specify the Controller Test Run number. It is mandatory option]\n"
                  "  -g => [specify the generator name to kill particular generator]\n"
                  "  -d => [is used for display used generators]\n"
                  "-g and -d both cannot be used together.\n");
  exit(-1);
}

void GenUsage1()
{

  fprintf(stdout , "Please use only one option -g [GENERATOR_NAME to kill generator] OR -d [To see list of active generators].\n");
  exit(-1);
}
void
handle_stop_gen_sigint( int sig )
{
  close(fd);
  exit(0);
}

size_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
  size_t ret = 0, count = 0;

  while(1)
  {
    //fprintf(stdout, "count = %d, buf = %s, len = %d\n", count, buf, len);
    ret = recv(sockfd, buf + count, len - count, flags);
    if(ret <= 0) // Error in recv
      break;

    if(ret > (len - count)) // Some error occured, returned bytes should not be more than requested
      return -1;

    count += ret;
    if(count >= len) // read all the data upto len bytes as requested by the caller of this function
      break;
  }

  if(count == 0) // Error 
    return -1;

  return count;
}

static int get_data(void)
{
  //int fd = -1;
  int rcv_amt;
  int size;

  /* Read 4 bytes first for size */
  if ((rcv_amt = my_recv (fd, &size, sizeof(int), 0)) <= 0) {
    fprintf(stderr, "Unable to get message size errno = %d, error: %s\n",
                     errno, nslb_strerror(errno));
    return -1;
  }
  if(size == 0) /* Data is over */
    return -2;

  if (g_buffer_size == 0)
  {
    g_buffer = malloc(size + 1);
    g_buffer_size = size;
  }
  else if (size > g_buffer_size)
  {
    g_buffer = realloc(g_buffer, size + 1);
    g_buffer_size = size;
  }

  if ((rcv_amt = my_recv (fd, g_buffer, size, 0)) <= 0) {
    fprintf(stderr, "Unable to get message of size = %d, errno = %d, error: %s\n",
                     size, errno, nslb_strerror(errno));;
    //perror("unable to rccv client sock");
    return -1;
  }
  g_buffer[rcv_amt] = '\0';
  return 0;
}

char *get_ns_wdir()
{ 
  char *nswdir = NULL;

  nswdir = getenv("NS_WDIR");
  if(nswdir)
    return(nswdir);
  else
    return("/home/cavisson/work");
}
 
void ns_process_generator(char *gen_name, int tr_number, int display_Active_GenFlag)
{
  int ns_pid, ret, ns_port;
  ns_comp_ctrl_rep stop_gen_msg;
  struct stat stat_buf;
  char Netcloud_fpath[256];
  char cur_user_cmd[] = "id -u";
  char cur_user_uid[128];

  ns_port = get_server_port(tr_number);
  if(ns_port == -1)
  {
    fprintf(stderr, "Unable to get port of running test run number.\n");
    exit(-1);
  }

  ns_pid = get_ns_pid(tr_number); /* This call will exit if failed. */
  
  if(nslb_run_cmd_and_get_last_line(cur_user_cmd, 128, cur_user_uid) != 0)
  {
    fprintf(stderr, "Unable to get current user.\n");
    exit(-1);
  }
   
  if(is_test_run_running(ns_pid) != 0) // If test run number is not running.
  {
    fprintf(stderr, "Test is not running.\n");
    return;
  }
  
  //check Netcloud.data file exist or not.  
  sprintf(Netcloud_fpath, "%s/logs/TR%d/NetCloud/NetCloud.data", get_ns_wdir(), tr_number);
  if(stat(Netcloud_fpath, &stat_buf))
  {
    fprintf(stderr, "Running test is not Netcloud test.\n");
    exit(-1);
  }
  
  if((fd = nslb_tcp_client("127.0.0.1", ns_port)) < 0) {
    fprintf(stderr, "Check if Netstorm with the supplied TestRun number is running.\n");
    exit(-1);
  }

  (void) signal(SIGINT, handle_stop_gen_sigint);
  if(display_Active_GenFlag == 1)
  {
    stop_gen_msg.opcode = SHOW_ACTIVE_GENERATOR;
    stop_gen_msg.msg_len = sizeof(stop_gen_msg) - sizeof(int);	
  }
  else
  { 
    if((atoi(cur_user_uid)) == (get_process_uid(ns_pid)) || (atoi(cur_user_uid)) == 0)
    {
      stop_gen_msg.opcode = STOP_PARTICULAR_GEN;
      stop_gen_msg.msg_len = sizeof(stop_gen_msg) - sizeof(int);
      strcpy(stop_gen_msg.reply_msg, gen_name);
    }
    else
    {
      fprintf(stderr, "You are not the owner of the Test.\n");
      exit(-1);
    }
  }

  if (send(fd, &stop_gen_msg, sizeof(stop_gen_msg), 0) <= 0) 
  {
    fprintf(stderr, "Unable to send message to netstorm errno = %d, error: %s\n", errno, nslb_strerror(errno));
    close(fd);
    exit(-1);
  }

  while(1)
  {
    ret = get_data();
    if (ret == -2)
      break;

    if(ret != 0)
    {
      fprintf(stderr, "Unable to fetch data from Netstorm.\n");
      close(fd);
      exit(-1);
    }
    printf ("%s\n", g_buffer);
  }
}

int main(int argc, char *argv[])
{
  char gen_name[64];
  int tr_number, tr_flg, gname_flg, c, display_Active_GenFlag = 0;

  if(argc > 5) {
    fprintf(stdout, "Please provide valid options like: nsi_stop_generator -t <CONTROLLER_TR> -g <GEN_NAME>\n");
    exit(-1);
  }
    
  while ((c = getopt(argc, argv, "t:g:d")) != -1) {
    switch (c) {
    case 't':
      tr_number = atoi(optarg);
      tr_flg = 1;
      break;

    case 'g':
      strcpy(gen_name, optarg);
      gname_flg = 1;
      break;

    case 'd':
      display_Active_GenFlag = 1;
      break;

     case '?':
      usage("Invalid arguments.");
    }
  }
  if(tr_flg != 1)
    GenUsage(); 
  if(gname_flg == 1 && display_Active_GenFlag ==1)
    GenUsage1(); 
 
  if(gname_flg == 1 || display_Active_GenFlag ==1)
    ns_process_generator(gen_name, tr_number, display_Active_GenFlag);
  else
  {
    fprintf(stdout, "Please provide valid options ex. nsi_stop_generator -t TR_NUMBER [-g GEN_NAME OR -d].\n");
    exit(-1);
  } 
  return 0;
}
