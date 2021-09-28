/***********Header***************
nsi_comp_ctrl: Tool to perform start|stop operation on 
               nsa_log_mgr|logging writer

Usage        : nsi_comp_ctrl  --testrun(-t) <TestRun> [--component(-c)  [--operation(-o) [--trace_level(-l) --trace_log_size(-s)
************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_msg_def.h"
#include "ns_event_log.h"
#include "nslb_signal.h"
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE_SIZE 50
static int fd;

// Function to print status messages recieved from netstorm
static void print_status_msg(parent_child *req_msg, char status, char *reply_msg){
   char tmp_op[50];
   char tmp_comp[50];

   req_msg->component == COMP_NLM? strcpy(tmp_comp, "nsa_log_manager"):strcpy(tmp_comp, "logging writer");
   req_msg->operation == COMP_START? strcpy(tmp_op, "started"):strcpy(tmp_op, "stopped");

   if(status){
     fprintf(stderr, "%s is not %s. Reply from component - %s\n", tmp_comp, tmp_op, reply_msg);
   }else{
     fprintf(stdout, "%s %s successfully. Reply from component - %s\n", tmp_comp, tmp_op, reply_msg);
   }
}

static int change_component_trace_level(char *comp, char *base_dir, int trace_level, int trace_log_size){
  
   char pid_file[MAX_LINE_SIZE] = "";
   int pid = 0;
   char trace_file[MAX_LINE_SIZE];
   FILE *fp = NULL;
   char tmp_buf[MAX_LINE_SIZE] = "";
  
   if(!strcasecmp(comp, "nlm")){
     sprintf(pid_file, "%s/.nlm.pid", base_dir);
     sprintf(tmp_buf, "NLM_TRACE_LEVEL %d %d", trace_level, trace_log_size);
   }
   else if(!strcasecmp(comp, "nlw")){
     sprintf(pid_file, "%s/.nlw.pid", base_dir);
     sprintf(tmp_buf, "NLW_TRACE_LEVEL %d %d", trace_level, trace_log_size);
   }
   else if(!strcasecmp(comp, "nlr")){
     sprintf(pid_file, "%s/.nlr.pid", base_dir);
     sprintf(tmp_buf, "NLR_TRACE_LEVEL %d %d", trace_level, trace_log_size);
   }
   else if(!strcasecmp(comp, "ndp")){
     sprintf(pid_file, "%s/.ndp.pid", base_dir);
     sprintf(tmp_buf, "NDP_TRL_LOG %d %d", trace_level, trace_log_size);
   }
   else if(!strcasecmp(comp, "nddbu")){
     sprintf(pid_file, "%s/.ndu_db_upload.pid", base_dir);
     sprintf(tmp_buf, "NDDBU_TRACE_LEVEL %d %d", trace_level, trace_log_size);
   }
   else if(!strcasecmp(comp, "nsdbu")){
     sprintf(pid_file, "%s/.nsu_db_upload.pid", base_dir);
     sprintf(tmp_buf, "NSDBU_TRACE_LEVEL %d %d", trace_level, trace_log_size);
   }

   if((fp = fopen(pid_file, "r")) == NULL){
     fprintf(stderr, "Error: Unable to open pid file %s for component %s.\n", pid_file, comp);
     return -1;
   }

  if(fscanf(fp, "%d", &pid) < 0)
  {
     fprintf(stderr, "Error: error in reading content of pid file %s.\n", pid_file);
     return -1; 
  }
  fclose(fp);

  strcpy(trace_file, "/tmp/trace_level.conf");
  if((fp = fopen(trace_file, "a")) != NULL){
    fwrite(tmp_buf, strlen(tmp_buf), 1, fp); 
    fclose(fp);
  }else{
    fprintf(stderr, "Unable to open trace level file %s in append mode.\n", trace_file);
    return -1;
  } 

  if(pid > 1)
  {
    if(kill(pid, TRACE_LEVEL_CHANGE_SIG) < 0)
    {
      fprintf(stderr, "Error: error in sending signal to pid %d\n", pid);
      return -1; 
    }
  }
  else
  {
    fprintf(stderr, "Error: Invalid pid = %d\n", pid);
    return -1; 
  }
  
  sleep(10);

  if(unlink(trace_file)){
    fprintf(stderr, "Error: Unable to delete trace conf file %s.\n", trace_file);
  }

  return 0;
}
// Function to get the msg from netstorm
size_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
  size_t ret = 0, count = 0;

  while(1)
  {
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

static void print_usage(char *bin_name)
{
  fprintf(stderr, "Usage: nsi_comp_ctrl  --testrun(-t) <TestRun> [--component(-c)  [--operation(-o) [--trace_level(-l) --trace_log_size(-s)");
  fprintf(stderr, "\nWhere:\n"
                  "\t--testrun: Testrun number\n"
                  "\t--component: Component name eg. NLM, NLW etc.\n"
                  "\t--operation: Operation may be START, STOP, CHANGETRACELEVEL\n"
                  "\t--trace_level: New trace level\n"
                  "\t--trace_log_size: Trace log file size\n");

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
  int trace_level = 0;
  int trace_level_flag = 0;
  int trace_log_size;
  int trace_log_size_flag = 0;

  int ns_port;
  int ns_pid;
  int c;

  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"operation", 1, NULL, 'o'},
                               {"component", 1, NULL, 'c'},
                               {"trace_level", 1, NULL, 'l'},
                               {"trace_log_size",  1, NULL, 's'},
                               {0, 0, 0, 0}
                             };

  while ((c = getopt_long(argc, argv, "t:o:c:l:s:", longopts, NULL)) != -1)
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
        if((strcasecmp(operation, "START")) &&  (strcasecmp(operation, "STOP")) && (strcasecmp(operation, "CHANGETRACELEVEL"))){
          fprintf(stderr, "Operation should be START, STOP or CHANGETRACELEVEL only\n");
          exit(-1);
        }
        break;
      case 'c':
        strcpy(component, optarg);
        component_flag++;
        break;
      case 'l':
        trace_level = atoi(optarg);
        trace_level_flag  = 1;
        if(trace_level < 0 || trace_level > 4){
          fprintf(stderr, "Trace level %d should be in between 0 and 4\n", trace_level); 
          print_usage(argv[0]);
        }
        break;
      case 's':
        trace_log_size = atoi(optarg);
        trace_log_size_flag = 1;
        if(trace_log_size < 0){
          fprintf(stderr, "Trace log size %d can't be negative\n", trace_log_size); 
          print_usage(argv[0]);
        }
        break;
      case ':':
      case '?':
        print_usage(argv[0]);
        break;
      }
  } /* while */


  if(!(test_run_flag && component_flag && operation_flag)){
    fprintf(stderr,"Missing mandatory argument. All the arguments are mandatory\n");
    print_usage(argv[0]);
  }

  if(!strcasecmp(operation,"ChangeTraceLevel") && (!trace_log_size_flag || !trace_level_flag))
  {
     fprintf(stderr, "Error: If operation is Change Trace Level trace level and trace log size ahould be given.\n");
     print_usage(argv[0]);
  }
 
  parent_child control_msg; 
  ns_comp_ctrl_rep rep_msg;
  char err_msg[1024];
  char base_dir[1024] = "";
  int rcv_amt;

  if (getenv("NS_WDIR") != NULL){
    sprintf(base_dir, "%s/logs/TR%d", getenv("NS_WDIR"), test_run_num);
  }
  else{
     fprintf(stdout, "Cant get NS_WDIR environment variable, setting base directory to work.\n");
     sprintf(base_dir, "/home/cavisson/work/logs/TR%d", test_run_num);
  }
 //Get the ns_port number from test run number
  ns_port = get_server_port(test_run_num);

  if(ns_port == -1){
    fprintf(stderr, "Unable to get the port no for test_run_num = %d\n", test_run_num);
    exit(-1);
  }
  ns_pid = get_ns_pid(test_run_num); /* This call will exit if failed. */

  printf("ns_pid === [%d],  is_test_running = [%d], port == [%d]\n" , ns_pid, is_test_run_running(ns_pid), ns_port);
 
  if(is_test_run_running(ns_pid) != 0) // If test run number is not running then exit
  {
    fprintf(stderr, "Test run number %d is not running\n", test_run_num);
    exit(-1);
  }

  if(trace_level){ 
    if(change_component_trace_level(component, base_dir, trace_level, trace_log_size) == -1){
       fprintf(stderr, "Error in changing trace level of component %s\n.", component);
       exit(-1);
    }
    return 0;
  }
  // make connection with netstorm on ns_port
  if((fd = nslb_tcp_client_ex("127.0.0.1", ns_port, 10, err_msg)) < 0){
    fprintf(stderr, "%s", err_msg);
    exit(-1);
  }

  // Handle Interrupt 
  (void) signal( SIGINT, handle_control_msg_sigint);
  control_msg.opcode = NS_COMP_CNTRL_MSG;


  // set operation
  if (!strcasecmp(operation, "START")){
    control_msg.operation = COMP_START;
  }
  else if (!strcasecmp(operation, "STOP")){
    control_msg.operation = COMP_STOP;
  }

  // Set component
  if (!strcasecmp(component, "NLM")){
    control_msg.component = COMP_NLM;
  }
  else if (!strcasecmp(component, "NLW")){
    control_msg.component = COMP_NLW;
  }

  // Send mesaage to parent(in case of netcloud it will be controller)
  if (send(fd, &control_msg, sizeof(control_msg), 0) <= 0) {
    fprintf(stderr, "Unable to send message to netstorm errno = %d, error: %s\n", errno, nslb_strerror(errno));
    perror("unable to send client sock");
    close(fd);
    exit(-1);
  }

  // recieve response message 
  if ((rcv_amt = my_recv (fd, &rep_msg, sizeof(ns_comp_ctrl_rep), 0)) <= 0) {
    fprintf(stderr, "Unable to get message size errno = %d, error: %s\n",
                     errno, nslb_strerror(errno));
    exit(-1);
  }

  // print status messages recieved
  print_status_msg(&control_msg, rep_msg.status, rep_msg.reply_msg);

  close(fd);
 
  return 0;
}
