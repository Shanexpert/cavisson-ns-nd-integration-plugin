/***********Header***************
ndi_comp_ctrl: Tool to perform start|stop operation on 
               

Usage        : ndi_comp_ctrl -t <test_run_num> -o <operation(START|STOP) -c <component> -p <ndc_port(optional)>

************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include "nslb_util.h"
#include "nslb_sock.h"
#include <unistd.h>

static int fd;

// Function to print status messages recieved from netstorm
static void print_status_msg(char *rep_msg){

   char operation[10];
   char component[50];
   char status[1024];
   //int testrun_num;
   char *tmp_rep = rep_msg;
   char *tmp_ptr;
   int len;
   char *fields[4];
   int num_tok;
   char tmp_op[10];

   if((tmp_ptr = strchr(tmp_rep, ':')) != NULL){
      len = tmp_ptr - tmp_rep;
      tmp_ptr++;
      if(strncmp(tmp_rep, "nd_control_rep", len)){
        fprintf(stderr, "Response %s is not in correct format", rep_msg);
        exit(-1);
      }        
   }

   num_tok = get_tokens(tmp_ptr, fields, ";" , 5);

   if(num_tok != 4){
     fprintf(stderr, "Missing fields in response msg - %s\n", rep_msg);
     exit(-1);
   }


   if((tmp_ptr = strchr(fields[0], '=')) != NULL){
     tmp_ptr++;
     strcpy(operation, tmp_ptr); 
   }
   if((tmp_ptr = strchr(fields[1], '=')) != NULL){
     tmp_ptr++;
     strcpy(component, tmp_ptr); 
   }
   if((tmp_ptr = strchr(fields[2], '=')) != NULL){
     tmp_ptr++;
     //testrun_num = atoi(tmp_ptr);   set but not used so commenting
   }
   if((tmp_ptr = strchr(fields[3], '=')) != NULL){
     tmp_ptr++;
     strcpy(status, tmp_ptr); 
   }

   strcasecmp(operation, "start") == 0? strcpy(tmp_op, "started"):strcpy(tmp_op, "stopped");

   if(!strncasecmp(status, "ok", 2)){
     fprintf(stdout, "%s %s successfully. Reply from component - %s\n", component, tmp_op, status);
   }else{
     fprintf(stderr, "%s is not %s. Reply from component - %s\n", component, tmp_op, status);
   }
}

// Function to get the msg from netstorm
size_t my_recv(int sockfd, void *buf, size_t len)
{
  size_t ret = 0, remain_bytes = len;
  int offset = 0;

  while(remain_bytes)
  {
   // ret = recv(sockfd, buf + count, len - count, flags);
    ret = read(sockfd, buf+offset, remain_bytes);
    if(ret < 0) {
      if(errno == EINTR)
        continue;
      else  {
        break;
      }
    }
    
    if(ret == 0) {
      //fprintf(stderr, "Connection is closed from other side. We assume that we have read all data");
      break;
    }
    offset += ret;
    remain_bytes -= ret;
  }

  return (len-remain_bytes);
}

static void print_usage(char *bin_name)
{
  fprintf(stderr,"Usage: %s -t <test run number> -o <operation(start|stop)> -c <component> -p <ndc_port(optional)>\n", bin_name);
  exit(-1);
}

void
handle_control_msg_sigint( int sig )
{
  close(fd);
  exit(0);
}

int main(int argc, char *argv[])
{
  int test_run_num = 0;
  char operation[256]  = "";
  char component[256] = "";

  extern char *optarg;
  int operation_flag = 0;
  int component_flag = 0;
  int test_run_flag = 0;
  int ndc_port_flag = 0;
  int ndc_port = 7892;
  int ns_pid;
  int c;

  //parent_child get_control_msg;

  while ((c = getopt(argc, argv, "t:o:c:p:")) != -1)
  {
    switch (c)
    {
      case 't':
        test_run_num = atoi(optarg);
        test_run_flag++;
        if(test_run_num <= 0){
         fprintf(stderr,"test_run_num %d should be greater than 0\n", test_run_num);
         exit(-1);
        }
        break;
      case 'o':
        strcpy(operation, optarg);
        operation_flag++;
        if((strcasecmp(operation, "START")) &&  (strcasecmp(operation, "STOP"))){
          fprintf(stderr, "Operation should be START or STOP only\n");
          exit(-1);
        }
        break;
      case 'c':
        strcpy(component, optarg);
        component_flag++;
        break;

      case 'p':
        ndc_port = atoi(optarg);
        ndc_port_flag++;
        break;
      case ':':
      case '?':
        print_usage(argv[0]);
        break;
      }
  } /* while */

 if(!(test_run_flag && component_flag && operation_flag)){
   fprintf(stderr,"Missing mandatory argument. Except ndc_port all arguments are mandatory\n");
   print_usage(argv[0]);
 }
 
  char err_msg[1024];
  int rcv_amt;
  char req_buf[2048];
  char rep_msg[2048];
  int msg_len;
  char *req_ptr = req_buf;

  ns_pid = get_ns_pid(test_run_num); /* This call will exit if failed. */
 
  if(is_test_run_running(ns_pid) != 0) // If test run number is not running then exit
  {
    fprintf(stderr, "Test run number %d is not running\n", test_run_num);
    exit(-1);
  }

  // make connection with netstorm on ns_port
  if((fd = nslb_tcp_client_ex("127.0.0.1", ndc_port, 10, err_msg)) < 0){
    fprintf(stderr, "%s", err_msg);
    exit(-1);
  }

  // Handle Interrupt 
  (void) signal( SIGINT, handle_control_msg_sigint);
  //control_msg.opcode = NDC_COMP_CNTRL_MSG;

  sprintf(req_ptr, "nd_control_req:action=%s;component=%s;test_run_num=%d\n", operation, component, test_run_num);

  // Send message to parent(in case of netcloud it will be controller)

  msg_len = strlen(req_ptr);
  if (send(fd, req_ptr, msg_len, 0) <= 0) {
    fprintf(stderr, "Unable to send message to netstorm errno = %d, error: %s\n", errno, nslb_strerror(errno));
    perror("unable to send client sock");
    close(fd);
    exit(-1);
  }
    
  // recieve response message 
  if ((rcv_amt = my_recv (fd, rep_msg, sizeof(rep_msg))) <= 0) {
    fprintf(stderr, "Unable to get message size errno = %d, error: %s\n",
                     errno, nslb_strerror(errno));
    exit(-1);
  }

  // print status messages recieved
  print_status_msg(rep_msg);

  close(fd);
 
  return 0;
}
