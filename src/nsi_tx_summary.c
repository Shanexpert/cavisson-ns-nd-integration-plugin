#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <regex.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "nslb_sock.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"

#define MAX_MESSAGE_SIZE 64*1024

#define MAX_FILENAME_LENGTH 1024

static int fd; // netstorm server socket file descriptor 
static int show_not_run_tx = 0; // flag for not running transactions
static char tr_dir[4096]; // Test Run directory
static int test_run_num = -1; // Test Run Number
static int ns_port = -1;  // TCP port of Netstorm parent process of running test run
int interval = -1; // Flag for interval time
static int g_buffer_size = 0;
static char *g_buffer = NULL;

static int get_tokens_netcloud(char *read_buf, char *fields[], char *token, int max_flds)
{
  int totalFlds = 0;
  char *ptr;
  char *token_ptr;

  ptr = read_buf;
  while((token_ptr = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
    if(totalFlds > max_flds)
    {
      //fprintf(stderr, "Total fields are more than max fields (%d), remaining fields are ignored\n", max_flds);
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}


/*
Purpose  : This method is to get offline data from the file tranddetail.dat and transnotrun.dat
Argument :  None
Return Value	: None
   Shows file content on console (stdout)
*/
static void get_offline_data()
{
  FILE *fp;
  char buffer[MAX_MESSAGE_SIZE + 1] = "\0";
  char fname[4096] = "\0";
  char cmd_buffer[1024] = "\0";

  if(show_not_run_tx == 1)
    sprintf(fname,"%s/trans_not_run.dat", tr_dir);
  else
  {
    sprintf(cmd_buffer, "ls %s/transdetail.dat >/dev/null 2>&1" , tr_dir);
    
    // To search trans_detail.dat in logs/TRXXX directory
    int status = system(cmd_buffer);
    
    // This check for trans_detail.dat and transdetail.dat file, because previously netstorm creates transdetail.dat
    // So now it work for both if trans_detail.dat not present then it reads data from transdetail.dat
    if(status == 0)
      sprintf(fname, "%s/transdetail.dat", tr_dir);
    else
      sprintf(fname, "%s/trans_detail.dat", tr_dir);

  }  

  if((fp = fopen(fname, "r")) == NULL)
  {
    fprintf(stderr, "Offline data not available. Error in opening file : %s\n", fname);
    exit(-1);
  }
  while(fgets(buffer, MAX_MESSAGE_SIZE, fp) != NULL)
  {
    printf("%s", buffer);  // Since fgets doest not remove new line ...
  } 
  fclose(fp);
}

/************************************************************************
 *
 * my_recv()
 * This function handles partial reads in recv() system call
 * recv() man page says,
 *
 *        "The receive calls normally return any data available, up to the requested
 *         amount, rather than waiting for receipt of the full amount requested."
 *
 * This method calls the recv() in a loop until all data is read from the socket.
 *
 ************************************************************************/
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

/*
Purpose  : This method is to send and receive data from netstorm
Argument : None, uses g_buffer 
Return Value	: None
Note: This method can be called in a loop. Once the data is over, the
      size of the data (first 4 bytes) will be zero.
      Data is sent from netstorm, source file: trans_util.c
*/
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

/*
Purpose  : This method is to handle CTRL C
Argument :  SIGNAL 
Return Value	: None
  
*/
void
handle_tx_summary_sigint( int sig )
{
  close(fd);
  exit(0);
}

static void usage(char *err_msg)
{
  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "nsi_tx_summary -t <test run number> -g <generator name> -m <mode> [ -n -i <interval> -o <offline mode>]\n");
  fprintf(stderr, "Where:\n");
  fprintf(stderr, "\t-t <Test Run Number> Test Run Number for which transaction data is required. "
	  "This is mandatory argument\n");
  fprintf(stderr, "\t-g <Generator Name> Generator Name for which transaction data is required.\n");
  fprintf(stderr, "\t-m <Mode>.For comparing transaction detail of Gen and Controller TR.Value can be 0 or 1\n");
  fprintf(stderr, "\t With mode option gen test number can be given with comma seprated..\n");
  fprintf(stderr, "\t-n To show not run transaction list. Default is to show running transaction details.\n");
  fprintf(stderr, "\t-i <interval in secs> Interval if data is to be shown repeatedly. If not specified, "
	  "data will be shown once\n");
  fprintf(stderr, "\t-o To show data from data file (offline mode). Default is online. In online mode, data "
	  "is queried from running test cavisson process if test is running or from data file if test is no longer running\n");
  fprintf(stderr, "\n");

  exit(-1);
}

/*
Purpose  : This method is to get online data from the file tranddetail.dat if test run running, else get data from                      trans_not_run.dat file
Argument :  None
Return Value	: None
   Shows file content on console (stdout)
*/
static void get_data_in_online_mode()
{
  int ns_pid, ret;
  parent_child get_tx_summary_msg;
  //char buffer[MAX_MESSAGE_SIZE + 1];
  
  ns_port = get_server_port(test_run_num);
  if(ns_port == -1)
  {
    fprintf(stderr, "Unable to get port of running test run number\n");
    exit(-1);
  }

  ns_pid = get_ns_pid(test_run_num); /* This call will exit if failed. */
/*   if(ns_pid == -1) */
/*   { */
/*     fprintf(stderr, "Unable to get pid\n");  */
/*     exit(-1); */
/*   } */

  if(is_test_run_running(ns_pid) != 0) // If test run number is not running then get offline data from files
  {
    get_offline_data(); 
    return;
  }

  // to get data online
  if ((fd = nslb_tcp_client("127.0.0.1", ns_port)) < 0) {
  //    perror("Unable to create client sock");
    fprintf(stderr, "Check if Netstorm with the supplied TestRun number is running\n");
    exit(-1);
  }

  (void) signal( SIGINT, handle_tx_summary_sigint);

  while(1) 
  {
    /* NetCloud: In release 3.9.3, now tool will send TX message once to NS and will fetch transaction
     * data. Next it prints it on stdout.
     * */
    if (show_not_run_tx == 1)
      get_tx_summary_msg.opcode = GET_MISSING_TX_DATA;
    else
      get_tx_summary_msg.opcode = GET_ALL_TX_DATA;
      get_tx_summary_msg.msg_len = sizeof(get_tx_summary_msg) - sizeof(int);

    if (send(fd, &get_tx_summary_msg, sizeof(get_tx_summary_msg), 0) <= 0) {
      fprintf(stderr, "Unable to send message to netstorm errno = %d, error: %s\n", errno, nslb_strerror(errno));
      //perror("unable to send client sock");
      close(fd);
      exit(-1);
    }

    /* Bug#4512: Keep receiving data until get_data() return -2, which it does when the last
     * message of 0 bytes has been received 
     */
    while(1)
    { 
      ret = get_data();
      if (ret == -2)
        break;
      
      if(ret != 0)
      {
        fprintf(stderr, "Unable to fetch data from Netstorm\n");
        close(fd);
        exit(-1);
      }
      printf ("%s", g_buffer);
    }

    if (interval == -1)
      break;
    else
      sleep(interval);
  }
  close(fd);

}

int
main(int argc, char *argv[])
{
  int c;
  char *ptr; 
  extern char *optarg;
  int offline_flag = 0;
  int trflag = 0;
  int mode_flag = 0;
  int gen_trflag = 0;
  char test_run[100] ="\0";
  char test_gen_name[10000]="\0";
  int totalFlds = 0;
  char *fields[100];
  char *gen_field[100];
  int i = 0;
  int j;
  int matched = 0;
	int gen_count = 0;
  FILE *gen_tr_fptr = NULL;
  char gen_tr[100][100];
  char gen_name[100][100];
  char file_data[MAX_FILENAME_LENGTH];
  char net_cloud_file_path[100];
  struct stat s;
  int err;
  
  //ptr = getenv("NS_WDIR");
  set_ns_wdir();

  while ((c = getopt(argc, argv, "t:i:g:m:on")) != -1) {
    switch (c) {
    case 't':
      strcpy(test_run, optarg);
      trflag = 1;
      break;

    case 'i':
      interval = atoi(optarg);
      break;
    
    case 'g':
      strcpy(test_gen_name, optarg);
      gen_trflag = 1;
      break;

    case 'm':
      mode_flag = atoi(optarg);
      break;

    case 'o':
      offline_flag = 1;
      break;
    
    case 'n':
      show_not_run_tx = 1;
      break;

    case '?':
      usage("Invalid arguments");
    }
  }


  
  // Check if Test Run is specified or not
  //
  if(!trflag)
      usage("Test run number not specified");

  if(trflag)
  {
    if(test_run[0] == '\0' )
      usage("Test run number not specified");
    test_run_num = atoi(test_run);
  }

  // Check if Generator Test Run is specified or not
  if(gen_trflag && !mode_flag)
  {
    if(test_gen_name[0] == '\0' )
      usage("Generator name not specified");
  }
  
  // Check need to be put, as in stanalone test it will nor read nc data file
  // Reading NetCloud.data file, and use of get_tokens fetch gen name and gen TR
  if(gen_trflag || mode_flag)
  {
    sprintf(net_cloud_file_path, "%s/logs/TR%d/NetCloud/NetCloud.data", g_ns_wdir, test_run_num);
    if ((gen_tr_fptr = fopen(net_cloud_file_path, "r")) == NULL)
    {
      fprintf(stderr, "Error in opening file %s.\n", net_cloud_file_path);
      return -1;
    }
    while (fgets(file_data, MAX_FILENAME_LENGTH, gen_tr_fptr) != NULL)
    {
      if(!strncmp (file_data, "NETCLOUD_GENERATOR_TRUN ", strlen("NETCLOUD_GENERATOR_TRUN "))) 
      {
        ptr = file_data;
        ptr = ptr + strlen("NETCLOUD_GENERATOR_TRUN ");
        get_tokens(ptr, gen_field, "|", 10);
        strcpy(gen_tr[i], gen_field[0]);
        strcpy(gen_name[i], gen_field[1]);
        i++;
      } 
    }
  // Keeping the count for no of generarator in netclod data file
    gen_count=i;
  }
  // In case mode is comapre mode then there is two cases possible(Compare mode will be treated as offline always)
  //         A: geneartor TR is provided sepratted by comma
  //         B: Generator TR is not provided it means ALL generators with Controller
  if(mode_flag)
  {
    if(gen_trflag) //If gen TR are given
    {
      totalFlds = get_tokens_netcloud(test_gen_name, fields, ",", 100);
      // In case generatot TR single given in case of mode 1 option, then error will be given
      if (totalFlds <= 1){
        usage("At least two generator name must be given");
      }
      for(i=0; i < totalFlds; i++)
      {
        // In case controller TR with -t option and generartor TR are same with -g option
        // Then generator equals to controller and fetch the controller data
        // If wants Controller data then GUI will send Overall
        if(!strcmp(fields[i], "Overall"))
        {
          fprintf(stdout, "Generator=TR%s\n", test_run);
          sprintf(tr_dir, "%s/logs/TR%s/", g_ns_wdir, test_run);
          matched = 1;
        }
        // Fetch genearator data
        else
        {
          for(j=0; j<gen_count; j++)
          {
            matched = 0;
            if(!strcmp(fields[i], gen_name[j]))
            {
              fprintf(stdout, "Generator=%s\n", fields[i]);
              sprintf(tr_dir, "%s/logs/TR%s/NetCloud/%s/TR%s", g_ns_wdir, test_run, gen_name[j], gen_tr[j]);
              matched = 1;
              err = stat(tr_dir, &s);
              break;
            }
          }
        }
        if(-1 == err || matched == 0) 
        {
          if(matched == 0 || ENOENT == errno)
          {
            fprintf(stdout, "Generator=TR%s\n", fields[i]);
            fprintf(stderr, "ERROR:selected TR%s details not found\n", fields[i]);
            exit(0);
          }
        }               
        get_offline_data();
      }
    } 
    else
    {
      //gen flag is not given it means ALL generators and Controller TR need to print
      fprintf(stdout, "Generator=TR%s\n", test_run);
      sprintf(tr_dir, "%s/logs/TR%s/", g_ns_wdir, test_run);
      get_offline_data();
      //  Get gen test runs and gen name
      for(j=0;j<gen_count;j++)
      {
        sprintf(tr_dir, "%s/logs/TR%s/NetCloud/%s/TR%s", g_ns_wdir, test_run, gen_name[j], gen_tr[j]);
        fprintf(stdout, "Generator=TR%s\n", gen_tr[j]);
        get_offline_data();
      }
    }
    exit(0);
  } 

  //sprintf(tr_dir, "%s/logs/TR%s", ptr?ptr:"/home/cavisson/work", test_run);
  if(gen_trflag ) //For generator test
  {
    for(j=0;j<gen_count;j++)
    {
      if(!strcmp(test_gen_name, gen_name[j]))
      {
        sprintf(tr_dir, "%s/logs/TR%d/NetCloud/%s/TR%s", g_ns_wdir, test_run_num, gen_name[j], gen_tr[j]);
      }
    }
  }
  else //For controller and Standalone test
    sprintf(tr_dir, "%s/logs/TR%d", g_ns_wdir, test_run_num);

  // NetCloud Changes: In release 3.9.3, in case of fetching transaction data for generators we will call get_offline_data function
  // Otherwise in case of controller or standalone, we will get data depending on offline_flag
  if (gen_trflag)
    get_offline_data(); // To read transaction detail summary offline, means from trans_detail.dat and trans_not_run.dat
  else if(offline_flag == 0) // Get data in online mode 
    get_data_in_online_mode(); 
  else
    get_offline_data(); // To read transaction detail summary offline, means from trans_detail.dat and trans_not_run.dat

  exit(0);
}
	
