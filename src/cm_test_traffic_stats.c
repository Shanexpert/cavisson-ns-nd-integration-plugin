/*
#############################################################################################################################
#cm_test_traffic_stats:
# Purpose: This is a monitor which will always run on ND env. It was designed to get HTTP data from running TR mainly from NC Test. It will connect to NS Parent. And parent will send its HTTP data it.
# If multiple TR's are coming on a controller then this will aggregate data of all TR depending on its data type.
# Total of 41 data elements will be coming. And we need to do operations like:
# sum -> sample,rate,cumulative datatypes
# avg -> times datatype 
# min -> times datatype
# max -> times datatype
# count -> times datatype
#
# Note: Progress interval of different test are different. This case has not been tested properly.
#
# gcc cm_test_traffic_stats.c -g -fgnu89-inline -I./libnscore -I./topology/src/topolib -I/usr/include/postgresql/ -DUbuntu -DRELEASE=1604 -DENABLE_TCP_STATES -DPG_BULKLOAD -lodbc -o ../bin/cm_test_traffic_stats ../obj/topology_debug/src/libnstopo/libnstopo_debug.a ../obj/nscore_debug/libnscore/libnscore_debug.a -L/usr/lib64 -lm -DTESTING
#############################################################################################################################
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

#include "cm_test_traffic_stats.h"
#include "nslb_sock.h"
#include "nslb_util.h"
#include "nslb_log.h"

#define MSG_LEN  		4
#define OVERALL_AGGREGATION	1
#define TR_AGGREGATION		2

TestTrafficInfo *tts_head = NULL;

static int min_prog_intvl = 0x7FFFFFFF;   //Maximum integer value
static int g_time_out = 5;
static char w_dir[128];
TestTrafficStatsRes overall_data;  //Overall data for all TR
static int debug = 0;
static FILE *dlog_fp;
static char *cmon_home;

static void display_help_and_exit(char *err_msg)
{
  printf("Event:1.0:Info|%s\n", err_msg);
  printf("-t: TestRun. If provided, ftech data for that particular testRun, else fetch data for all running TR\n");
  printf("-c: Controller Name. It is mandatory\n");
  printf("-D: Debug Log. Level can be 1, 2, 3, 4\n");
  printf("-i: Interval Secs. Provided to sleep in seconds\n");
  exit(-1);
}


static int open_debug_log(char *err_msg)
{
  char dlog_file[1024 + 1];
  int fd;

  sprintf(dlog_file, "%s/logs/cm_test_traffic.logs", cmon_home);

  umask(0);
  fd = open(dlog_file, O_WRONLY|O_CLOEXEC|O_CREAT|O_APPEND, 0666);
  if(ENOENT == errno)
  {
    sprintf(err_msg, "Error:'%s', [System Exception] = %s", dlog_file, nslb_strerror(errno));
    return FAILURE;
  }
  if(fd < 0 )
  {
    sprintf(err_msg, "Error: Unable to open debug log file '%s', [System Exception-(%d)] = %s", dlog_file, errno, nslb_strerror(errno));
    return FAILURE;
  }

  dlog_fp = fdopen(fd, "a");
  if(dlog_fp == NULL)
  {
    sprintf(err_msg, "Error: Unable to open debug log file '%s', [System Exception-(%d)] = %s", dlog_file, errno, nslb_strerror(errno));
    return -1;
  }

  return 1;
}

static inline void debug_log(int level, char *file, int line, char *fname, char *format, ...)
{
  va_list ap;
  int wbytes = 0, log_wbytes = 0;
  char buffer[1024 + 1] = "\0";
  char time_buf[100];

  if((debug & level) == 0) return;

  wbytes = sprintf(buffer, "\n%s|%s|%d|%s|", nslb_get_cur_date_time(time_buf, 1), file, line, fname);
  va_start(ap, format);
  log_wbytes = vsnprintf(buffer + wbytes , 1024 - wbytes, format, ap);
  va_end(ap);
  buffer[1024] = 0;

  if(wbytes < 0)
    log_wbytes = strlen(buffer) - wbytes;

  if(dlog_fp)
  {
    if((fwrite(buffer, log_wbytes + wbytes, 1, dlog_fp)) < 0)
    {
      fprintf(stderr, "Error: [Write Operation Failure] - Unable to write to debug log file for nsi_rtc_invoker .\n");
      exit (EXIT_FAILURE);
    }
    fflush(dlog_fp);
  }
}


static void free_from_list(TestTrafficInfo *node)
{
  TestTrafficInfo *prev = NULL; 
  TestTrafficInfo *cur = tts_head;
  
  while(cur != NULL)
  {
    if(cur == node)
    {
      if(prev == NULL)
        tts_head = cur->next;
      else
        prev->next = cur->next;

      free(cur);
      return;
    }
    prev = cur;
    cur = cur->next;
  }
}


static int make_connection(char *err_msg, int trnum)
{
  char ns_port_buf[PORT_LEN + 1];
  char ns_port_file[MAX_PATH_LEN + 1];
  char err_buf[1024];
  FILE *fp;
  int fd = -1;
  int ns_port = 0;
  
  debug_log(DLOG_L1, _NLF_, "Method called");

  sprintf(ns_port_file, "%s/logs/TR%d/NSPort", w_dir, trnum);
  debug_log(DLOG_L1, _NLF_, "ns_port_file = %s", ns_port_file);

  if(!(fp = fopen(ns_port_file, "r")))
  {
    sprintf(err_msg, "Error: [Connection Failure] - In making connection with NS, unable to opening NSPort file %s", ns_port_file);
    return -1;
  }

  if(fgets(ns_port_buf,  PORT_LEN + 1, fp) != NULL) // should we != NULL for success ?
    ns_port = atoi(ns_port_buf);

  fclose(fp);

  debug_log(DLOG_L1, _NLF_, "path of ns port file = %s, ns_port = %d, g_time_out = %d", ns_port_file, ns_port, g_time_out);
  if(ns_port == 0)
  {
    debug_log(DLOG_L1, _NLF_, "nsPort is 0. Hence not making connection for TR %d", trnum);
    return -1;
  }

  /* Increase timeout value from 10 to 60 seconds  */ 
  if((fd = nslb_tcp_client_ex("127.0.0.1", ns_port, g_time_out, err_buf)) <= 0)
  {
    sprintf(err_msg, "Error: %s", err_buf);
    return -1;
  }

  debug_log(DLOG_L1, _NLF_, "Returning fd = %d , after nslb_tcp_client", fd);

  return fd;
}

//Weighted avg using count of time_data type
static inline void avg(double *d1, double d2, int c1, int c2)  
{
  *d1 = ((*d1 * c1) + (d2 * c2)) / (c1 + c2);
  return;
}

static inline void min(double *d1, double d2)
{
  if(d2 < *d1)
    *d1 = d2;
  return;
}

static inline void max(double *d1, double d2)
{
  if(d2 > *d1)  
    *d1 = d2;
  return;
}

static inline void sum(double *d1, double d2)
{
  *d1 = *d1 + d2;
  return;
}
/*bug  103688  */ 
static inline void sum_int(int *d1, int d2)
{
  *d1 = *d1 + d2;
}

#ifdef TESTING
static TestTrafficStatsRes dummy_data(double value)
{
  TestTrafficStatsRes resp_data;

  resp_data.http_response.url_req = value;
  resp_data.http_response.url_sent = value;
  resp_data.http_response.tries = value;
  resp_data.http_response.succ = value;
  resp_data.http_response.response.avg_time = value;
  resp_data.http_response.response.min_time = value;
  resp_data.http_response.response.max_time = value;
  resp_data.http_response.response.succ = value;
  resp_data.http_response.succ_response.avg_time = value;
  resp_data.http_response.succ_response.min_time = value;
  resp_data.http_response.succ_response.max_time = value;
  resp_data.http_response.succ_response.succ = value;
  resp_data.http_response.fail_response.avg_time = value;
  resp_data.http_response.fail_response.min_time = value;
  resp_data.http_response.fail_response.max_time = value;
  resp_data.http_response.fail_response.succ = value;
  resp_data.http_response.dns.avg_time = value;
  resp_data.http_response.dns.min_time = value;
  resp_data.http_response.dns.max_time = value;
  resp_data.http_response.dns.succ = value;
  resp_data.http_response.conn.avg_time = value;
  resp_data.http_response.conn.min_time = value;
  resp_data.http_response.conn.max_time = value;
  resp_data.http_response.conn.succ = value;
  resp_data.http_response.ssl.avg_time = value;
  resp_data.http_response.ssl.min_time = value;
  resp_data.http_response.ssl.max_time = value;
  resp_data.http_response.ssl.succ = value;
  resp_data.http_response.frst_byte_rcv.avg_time = value;
  resp_data.http_response.frst_byte_rcv.min_time = value;
  resp_data.http_response.frst_byte_rcv.max_time = value;
  resp_data.http_response.frst_byte_rcv.succ = value;
  resp_data.http_response.dwnld.avg_time = value;
  resp_data.http_response.dwnld.min_time = value;
  resp_data.http_response.dwnld.max_time = value;
  resp_data.http_response.dwnld.succ = value;
  resp_data.http_response.cum_tries = value;
  resp_data.http_response.cum_succ = value;
  resp_data.http_response.failure = value; 
  resp_data.http_response.http_body_throughput = value;
  resp_data.http_response.tot_http_body = value;

  return resp_data;
}
#endif

static void reset_data(TestTrafficStatsRes *tts_data)
{
  //memset(&(tts_data->http_response), (int)value, sizeof(HttpResponse));

  tts_data->http_response.url_req = NAN_VALUE;
  tts_data->http_response.url_sent = NAN_VALUE;
  tts_data->http_response.tries = NAN_VALUE;
  tts_data->http_response.succ = NAN_VALUE;
  tts_data->http_response.response.avg_time = NAN_VALUE;
  tts_data->http_response.response.min_time = NAN_VALUE;
  tts_data->http_response.response.max_time = NAN_VALUE;
  tts_data->http_response.response.succ = NAN_VALUE;
  tts_data->http_response.succ_response.avg_time = NAN_VALUE;
  tts_data->http_response.succ_response.min_time = NAN_VALUE;
  tts_data->http_response.succ_response.max_time = NAN_VALUE;
  tts_data->http_response.succ_response.succ = NAN_VALUE;
  tts_data->http_response.fail_response.avg_time = NAN_VALUE;
  tts_data->http_response.fail_response.min_time = NAN_VALUE;
  tts_data->http_response.fail_response.max_time = NAN_VALUE;
  tts_data->http_response.fail_response.succ = NAN_VALUE;
  tts_data->http_response.dns.avg_time = NAN_VALUE;
  tts_data->http_response.dns.min_time = NAN_VALUE;
  tts_data->http_response.dns.max_time = NAN_VALUE;
  tts_data->http_response.dns.succ = NAN_VALUE;
  tts_data->http_response.conn.avg_time = NAN_VALUE;
  tts_data->http_response.conn.min_time = NAN_VALUE;
  tts_data->http_response.conn.max_time = NAN_VALUE;
  tts_data->http_response.conn.succ = NAN_VALUE;
  tts_data->http_response.ssl.avg_time = NAN_VALUE;
  tts_data->http_response.ssl.min_time = NAN_VALUE;
  tts_data->http_response.ssl.max_time = NAN_VALUE;
  tts_data->http_response.ssl.succ = NAN_VALUE;
  tts_data->http_response.frst_byte_rcv.avg_time = NAN_VALUE;
  tts_data->http_response.frst_byte_rcv.min_time = NAN_VALUE;
  tts_data->http_response.frst_byte_rcv.max_time = NAN_VALUE;
  tts_data->http_response.frst_byte_rcv.succ = NAN_VALUE;
  tts_data->http_response.dwnld.avg_time = NAN_VALUE;
  tts_data->http_response.dwnld.min_time = NAN_VALUE;
  tts_data->http_response.dwnld.max_time = NAN_VALUE;
  tts_data->http_response.dwnld.succ = NAN_VALUE;
  tts_data->http_response.cum_tries = NAN_VALUE;
  tts_data->http_response.cum_succ = NAN_VALUE;
  tts_data->http_response.failure.sample = 0;  /*bug 103688*/
  tts_data->http_response.failure.count = 0; 
  tts_data->http_response.http_body_throughput = NAN_VALUE;
  tts_data->http_response.tot_http_body = NAN_VALUE;
}

static void aggregate_test_traffic_data(TestTrafficStatsRes *resp_1, TestTrafficStatsRes resp_2, int trnum, char flag)
{
  if(flag == TR_AGGREGATION)
  {
    //Timestamp is same, it means same data is received again from a TR
    if((*resp_1).abs_timestamp == resp_2.abs_timestamp)
    {
      debug_log(DLOG_L1, _NLF_, "Same data is received from TR %d", trnum);
      return;
    }
  }

  (*resp_1).abs_timestamp = resp_2.abs_timestamp;  //Updating timestamp for next sample comparison
  sum(&(resp_1->http_response.url_req), resp_2.http_response.url_req);      //URL request started/sec
  sum(&(resp_1->http_response.url_sent), resp_2.http_response.url_sent);    //URL request sent/sec
  sum(&(resp_1->http_response.tries), resp_2.http_response.tries);          //URL Hits: Total tries in a sampling period
  sum(&(resp_1->http_response.succ), resp_2.http_response.succ);            //Success URL Response

  //Response
  avg(&(resp_1->http_response.response.avg_time), resp_2.http_response.response.avg_time, (*resp_1).http_response.response.succ, resp_2.http_response.response.succ);
  min(&(resp_1->http_response.response.min_time), resp_2.http_response.response.min_time);
  max(&(resp_1->http_response.response.max_time), resp_2.http_response.response.max_time);
  sum(&(resp_1->http_response.response.succ), resp_2.http_response.response.succ);
  
  //Succ Response
  avg(&(resp_1->http_response.succ_response.avg_time), resp_2.http_response.succ_response.avg_time, (*resp_1).http_response.succ_response.succ, resp_2.http_response.succ_response.succ);
  min(&(resp_1->http_response.succ_response.min_time), resp_2.http_response.succ_response.min_time);
  max(&(resp_1->http_response.succ_response.max_time), resp_2.http_response.succ_response.max_time);
  sum(&(resp_1->http_response.succ_response.succ), resp_2.http_response.succ_response.succ);

  //Fail Response
  avg(&(resp_1->http_response.fail_response.avg_time), resp_2.http_response.fail_response.avg_time, (*resp_1).http_response.fail_response.succ, resp_2.http_response.fail_response.succ);
  min(&(resp_1->http_response.fail_response.min_time), resp_2.http_response.fail_response.min_time);
  max(&(resp_1->http_response.fail_response.max_time), resp_2.http_response.fail_response.max_time);
  sum(&(resp_1->http_response.fail_response.succ), resp_2.http_response.fail_response.succ);
 
  //DNS llokup time
  avg(&(resp_1->http_response.dns.avg_time), resp_2.http_response.dns.avg_time, (*resp_1).http_response.dns.succ, resp_2.http_response.dns.succ);
  min(&(resp_1->http_response.dns.min_time), resp_2.http_response.dns.min_time);
  max(&(resp_1->http_response.dns.max_time), resp_2.http_response.dns.max_time);
  sum(&(resp_1->http_response.dns.succ), resp_2.http_response.dns.succ);

  //URL Connection Time
  avg(&(resp_1->http_response.conn.avg_time), resp_2.http_response.conn.avg_time, (*resp_1).http_response.conn.succ, resp_2.http_response.conn.succ);
  min(&(resp_1->http_response.conn.min_time), resp_2.http_response.conn.min_time);
  max(&(resp_1->http_response.conn.max_time), resp_2.http_response.conn.max_time);
  sum(&(resp_1->http_response.conn.succ), resp_2.http_response.conn.succ);
 
  //SSL Handshake
  avg(&(resp_1->http_response.ssl.avg_time), resp_2.http_response.ssl.avg_time, (*resp_1).http_response.ssl.succ, resp_2.http_response.ssl.succ);
  min(&(resp_1->http_response.ssl.min_time), resp_2.http_response.ssl.min_time);
  max(&(resp_1->http_response.ssl.max_time), resp_2.http_response.ssl.max_time);
  sum(&(resp_1->http_response.ssl.succ), resp_2.http_response.ssl.succ);

  //First Byte Received
  avg(&(resp_1->http_response.frst_byte_rcv.avg_time), resp_2.http_response.frst_byte_rcv.avg_time, (*resp_1).http_response.frst_byte_rcv.succ, resp_2.http_response.frst_byte_rcv.succ);
  min(&(resp_1->http_response.frst_byte_rcv.min_time), resp_2.http_response.frst_byte_rcv.min_time);
  max(&(resp_1->http_response.frst_byte_rcv.max_time), resp_2.http_response.frst_byte_rcv.max_time);
  sum(&(resp_1->http_response.frst_byte_rcv.succ), resp_2.http_response.frst_byte_rcv.succ);

  //Download Time
  avg(&(resp_1->http_response.dwnld.avg_time), resp_2.http_response.dwnld.avg_time, (*resp_1).http_response.dwnld.succ, resp_2.http_response.dwnld.succ);
  min(&(resp_1->http_response.dwnld.min_time), resp_2.http_response.dwnld.min_time);
  max(&(resp_1->http_response.dwnld.max_time), resp_2.http_response.dwnld.max_time);
  sum(&(resp_1->http_response.dwnld.succ), resp_2.http_response.dwnld.succ);

  //cumulative data. Just updating values for TR aggregation, and adding up for Overall aggregation
  if(flag == TR_AGGREGATION)
  {
    resp_1->http_response.cum_tries = resp_2.http_response.cum_tries;   			//Total URL tries since start of test
    resp_1->http_response.cum_succ = resp_2.http_response.cum_succ;   			//Total URL succeed 
  }
  else
  {
    sum(&(resp_1->http_response.cum_tries), resp_2.http_response.cum_tries);
    sum(&(resp_1->http_response.cum_succ), resp_2.http_response.cum_succ);
  }

  //sum(&(resp_1->http_response.failure), resp_2.http_response.failure);       			//Total URL Failure
  /*bug  103688  */
  sum_int((int*) &(resp_1->http_response.failure.sample), (int)resp_2.http_response.failure.sample);       //Total URL Failure
  sum_int(&(resp_1->http_response.failure.count), resp_2.http_response.failure.count);       			//Total URL Failure
  sum(&(resp_1->http_response.http_body_throughput), resp_2.http_response.http_body_throughput);   //Total HTTP body throughput
  sum(&(resp_1->http_response.tot_http_body), resp_2.http_response.tot_http_body);			//Total HTTP body size
}


static TestTrafficInfo * search_for_node(int trnum)
{
  TestTrafficInfo *cur = tts_head;
  
  while(cur != NULL)
  {
    if(cur->test_run == trnum)
      return cur;

    cur = cur->next;
  }
  return NULL;
}

//Adding new node at head
static TestTrafficInfo * add_node_in_tr_list(int prog_intvl, int fd, int trnum, double timestamp, double seq_no)
{
  TestTrafficInfo *node = NULL;
  
  node = (TestTrafficInfo *)malloc(sizeof(TestTrafficInfo));
  if(node == NULL)
  {
    //cannot allocate memory. Log and Exit.
  }
  
  node->next = tts_head;
  tts_head = node;

  node->con_fd = fd;
  node->test_run = trnum;
  reset_data(&(node->tts_data));
  node->data_filled = 0;
  node->tts_data.abs_timestamp = timestamp;
  node->tts_data.progress_interval = prog_intvl;
  node->tts_data.seq_no = seq_no;

  return node;
}


static void aggregate_overall_data()
{
  TestTrafficInfo *node = tts_head;
  
  while(node != NULL)
  {
    if(node->data_filled)
    {
      if(overall_data.http_response.url_req != overall_data.http_response.url_req)    //-nan data
        memcpy(&overall_data, &(node->tts_data), sizeof(TestTrafficStatsRes));
      else
        aggregate_test_traffic_data(&(overall_data), node->tts_data, node->test_run, OVERALL_AGGREGATION);
    }

    node = node->next;
  }
}


static int receive(TestTrafficStatsRes *read_buffer, int con_fd, int trnum)
{
  int buf_size = 4; //1st four bytes of message is length.
  int buflen = 0, rbytes;
  int read_buf_int = 4; //1st four bytes of message is length.
  int read_buf_size;
  
  while(read_buf_int > 0)
  {
    if((rbytes = read(con_fd, read_buffer + buflen, read_buf_int)) < 0)
    {
      if(errno == ENETRESET || errno == ECONNRESET ) //In case parent dies after accepting the connection.
      {
        debug_log(DLOG_L1, _NLF_,"Error: Test is Stopped. read bytes = %d, buff_len = %d, read_buf_int = %d, errno(%d) = (%s)",
                                    rbytes, buflen, read_buf_int, errno, nslb_strerror(errno));
      }
      else if(errno == EAGAIN ) //In case parent is not responding.
      {
        debug_log(DLOG_L1, _NLF_,"Error: TimeOut: Netstorm is not responding read bytes = %d"
                                 "buff_len = %d, read_buf_int = %d, errno(%d) = (%s)",
                                  rbytes, buflen, read_buf_int, errno, nslb_strerror(errno));
      }
      else
      {
        debug_log(DLOG_L1, _NLF_,"Error: [Peer Connection Reset]. read bytes = %d, errno(%d) = (%s)", rbytes, errno, nslb_strerror(errno));
      }
      return READ_ERROR;
    }
    //bytes_read == 0    /connection_Closed
    else if(rbytes == 0)
    {
      debug_log(DLOG_L1, _NLF_, "Connection closed from Netstorm Parent for TR %d", trnum);
      return CONNECTION_CLOSED;
    }

    buflen += rbytes;
    read_buf_int -= rbytes;
  }
  //read_buf[buflen] = '\0';
  memcpy(&read_buf_size, read_buffer, 4);

  debug_log(DLOG_L1, _NLF_, "First 4 bytes of Message = [%d]", read_buf_size);

  buflen = sizeof(int);
  while(read_buf_size > 0)
  {
    if( read_buf_size > 1024)
      buf_size = 1024;
    else
      buf_size = read_buf_size;

    if((rbytes = read(con_fd, (char*)(read_buffer) + buflen, buf_size)) < 0)
    {
      debug_log(DLOG_L1, _NLF_,"Error in reading message from NS. read bytes = %d, errno(%d) = (%s)",
                        rbytes, errno, nslb_strerror(errno));
      return READ_ERROR;
    }

    else if(rbytes == 0)
    {
      debug_log(DLOG_L1, _NLF_, "Connection closed from Netstorm Parent for TR %d", trnum);
      return CONNECTION_CLOSED;
    }

    debug_log(DLOG_L1, _NLF_, "Message recived from NS");
    read_buf_size -= rbytes;
    buflen += rbytes;
  }

  return READ_SUCCESSFUL;
}

static int get_data(int fd, int trnum, char flag, TestTrafficInfo *node)
{
  int prog_intvl;
  TestTrafficStatsRes resp_data;

  if(receive(&resp_data, fd, trnum) <= 0)
    return CONNECTION_CLOSED;

 /* resp_data = dummy_data(value);
  value++;

  resp_data.abs_timestamp = resp_data.seq_no = value;
  resp_data.progress_interval = 10000;
*/ 
  if(flag == INIT_TIME)  // Need to ignore data, only save the progress interval
  {
    //First data will be progressIntvl
    prog_intvl = (resp_data.progress_interval / 1000);
    debug_log(DLOG_L1, _NLF_, "Progress Interval for TR%d is %d ", trnum, prog_intvl);
    if(min_prog_intvl > prog_intvl)  //Setting Minimum prog interval
      min_prog_intvl = prog_intvl;

    node = add_node_in_tr_list(prog_intvl, fd, trnum, resp_data.abs_timestamp, resp_data.seq_no); 
  }

  
  if(node->tts_data.http_response.url_req != node->tts_data.http_response.url_req)    //-nan data
    memcpy(&(node->tts_data), &(resp_data), sizeof(TestTrafficStatsRes));
  else
    aggregate_test_traffic_data(&(node->tts_data), resp_data, trnum, TR_AGGREGATION);

  node->data_filled = 1;

  return READ_SUCCESSFUL;
}

//Provide list of list space separated
int get_list_of_running_TR(char *tr_buf)
{
  char line[200];
  int test_run_no;
  FILE* output;
  int len = 0;
  
  tr_buf[0] = '\0';
  line[0] = '\0';
  if((output = popen("nsu_show_test_logs -RL","r")) == NULL)
  {
    //pclose(output);
    return -1;
  }

  /*Sample Output:
  TestRun	Status	Start Date and Time	Elapsed Time	Owner		Project/Subproject/Scenario
  4861	Running	05/20/19 15:19:49	00:01:50	cavisson	default/default/anshul_scenario 

  First line starts with TestRun*/

  while(fgets(line, 256, output))
  {
    if(line[0] == 'T' || line[0] == 't')    
      continue;

    sscanf(line, "%d", &test_run_no);
    if(len < 256)
      len += sprintf(tr_buf + len, "%d ", test_run_no);
    else 
      break;
  }
  pclose(output);

  if(len != 0)
    tr_buf[len-1] = '\0';

  return 0;
}

static int send_message(int fd, int trnum)
{
  int opcode = TEST_TRAFFIC_STATS;
  int msgsize = 0;
  char buffer[1024];
  buffer[0] = '\0';
  //int size = sprintf(buffer + OPCODE_LEN + MSG_LEN, "START:http_test_traffic_stat");

  //we did +1 to store null byte at the end of message.
  msgsize = sizeof(int);

  //Fill length (payload + opcode) in msg_buf
  memcpy(buffer, &msgsize, sizeof(int));
  //Fill opcode in msg_buf
  //message size is sizeof(message) + opcode
  memcpy(buffer + MSG_LEN, &opcode, sizeof(int));

  //buflen = sizeof message  +  4(opcode) + 4(length)
  msgsize = msgsize + MSG_LEN;

  debug_log(DLOG_L2, _NLF_, "Going to send opcode message to TR %d", trnum);
  if(write(fd, buffer, msgsize) < 0)
  {
    debug_log(DLOG_L1, _NLF_, "Error in sending Message to TR %d, Error: %s", trnum, nslb_strerror(errno));
    return -1;
  }
  return 0;
}


static void process_data(char *err_msg, int trnum)
{
  int i, con_fd = -1;
  char tr_buf[256];
  char *tr_list[MAX_TR_LIST];
  TestTrafficInfo *node;

  tr_buf[0] = '\0';

  if(trnum == 0)
  {
    if(get_list_of_running_TR(tr_buf) < 0)
    {
      debug_log(DLOG_L1, _NLF_, "Error in running nsu_show_test_logs command");
      return;
    }
  }
  else
    sprintf(tr_buf, "%d", trnum);

  //tokenise tr_buf 
  int count = get_tokens(tr_buf, tr_list, " ", MAX_TR_LIST);

  for(i = 0; i < count; i++)
  {
    int trnum = atoi(tr_list[i]);
    node = search_for_node(trnum);
    if(node == NULL)   //New TR, Need to add in structure
    {
      con_fd = make_connection(err_msg, trnum);
      if(con_fd > 0)
      {
        send_message(con_fd, trnum);
        get_data(con_fd, trnum, INIT_TIME, NULL);     //No need to handle error case as if error then TR would not be added in node.
      }
      else
        debug_log(DLOG_L1, _NLF_, "Error in making connection for TR. %s", err_msg);
      continue;
    }
    else
    {
      send_message(node->con_fd, trnum);
      if(get_data(node->con_fd, node->test_run, RUNTIME, node) == CONNECTION_CLOSED)   //Connection closed from TR. Testrun Maybe Over.
        free_from_list(node);
    }
  }
}


//Logic: If this is a valid controller name then, netstorm binary must be present in its bin directory.
static int verify_controller_name(char *cntrlr_name, char *err_msg)
{
  char file[256];
  struct stat st;

  sprintf(file, "/home/cavisson/%s/bin/netstorm", cntrlr_name);

  if(!stat(file, &st))  //File present
    return 1;

  sprintf(err_msg, "File (%s) is not present.", file);
  return -1;
}


static void show_data(char *vector_name)
{
  char data_buffer[1024];

  sprintf(data_buffer, "0:%s|%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %lf %lf "
         "%lf %lf %d %lf %lf\n", 
         vector_name, overall_data.http_response.url_req, overall_data.http_response.url_sent, overall_data.http_response.tries, overall_data.http_response.succ, 
         overall_data.http_response.response.avg_time, overall_data.http_response.response.min_time, overall_data.http_response.response.max_time, overall_data.http_response.response.succ, 
         overall_data.http_response.succ_response.avg_time, overall_data.http_response.succ_response.min_time, overall_data.http_response.succ_response.max_time, overall_data.http_response.succ_response.succ, 
         overall_data.http_response.fail_response.avg_time, overall_data.http_response.fail_response.min_time, overall_data.http_response.fail_response.max_time, overall_data.http_response.fail_response.succ,
         overall_data.http_response.dns.avg_time, overall_data.http_response.dns.min_time, overall_data.http_response.dns.max_time, overall_data.http_response.dns.succ,
         overall_data.http_response.conn.avg_time, overall_data.http_response.conn.min_time,  overall_data.http_response.conn.max_time, overall_data.http_response.conn.succ,
         overall_data.http_response.ssl.avg_time, overall_data.http_response.ssl.min_time, overall_data.http_response.ssl.max_time, overall_data.http_response.ssl.succ,
         overall_data.http_response.frst_byte_rcv.avg_time, overall_data.http_response.frst_byte_rcv.min_time, overall_data.http_response.frst_byte_rcv.max_time, overall_data.http_response.frst_byte_rcv.succ,
	 overall_data.http_response.dwnld.avg_time, overall_data.http_response.dwnld.min_time, overall_data.http_response.dwnld.max_time, overall_data.http_response.dwnld.succ,
	 overall_data.http_response.cum_tries, overall_data.http_response.cum_succ, overall_data.http_response.failure.count, overall_data.http_response.http_body_throughput, overall_data.http_response.tot_http_body);


  debug_log(DLOG_L1, _NLF_, "Going to send data: %s", data_buffer);
  if(printf("%s\n", data_buffer) < 0)
    debug_log(DLOG_L1, _NLF_, "Error in printing Data");

  fflush(stdout);
}


static void set_debug(int level)
{
  if(level == 1)
    debug = 0x000F;
  else if(level == 2)
    debug = 0x00FF;
  else if(level == 3)
    debug = 0x0FFF;
  else if(level == 4)
    debug = 0xFFFF;
  else
    debug = 0x0000;
}


static void create_vector_name(char *tier_name, char *vector_name)
{
  char server_name[256];
  char hostbuffer[256];
  char *IPbuffer;
  struct hostent *host_entry;
  int herr;
  int hostname;
  char separator[5] = ">";

  server_name[0] = '\0';
  
  if(getenv("MON_CAVMON_SERVER_NAME"))
    strcpy(server_name, getenv("MON_CAVMON_SERVER_NAME"));
  else
  {
    // To retrieve hostname 
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if(hostname != -1)
    {
      // To retrieve host information 
      host_entry = nslb_gethostbyname2(hostbuffer, AF_INET, &herr);
      if(host_entry)
      {
        // To convert an Internet network 
        // address into ASCII string 
        IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
        strcpy(server_name, IPbuffer);
      }
    }

    if(server_name[0] == '\0')
      strcpy(server_name, "Default");
  } 

  if(getenv("MON_VECTOR_SEPARATOR"))
    strcpy(separator, getenv("MON_VECTOR_SEPARATOR"));

  sprintf(vector_name, "%s%s%s", tier_name, separator, server_name);
}


int is_test_over(int mon_test_run, int mon_partition_idx, char *running_tr_dir)
{
  struct stat st;

  if(mon_test_run == -1)   //For standalone
    return 0;

  if(!stat(running_tr_dir, &st))  //File present
    return 1;

  return -1;   //Test Over
}


int main(int argc, char *argv[])
{
  int opt, trnum = 0;
  int level, interval, i, count;
  char tr_buf[256], running_tr_dir[512];
  char *tr_list[MAX_TR_LIST];
  TestTrafficInfo *node, *prev;
  unsigned long start_time, end_time;
  int time_left;
  char err_msg[1024];
  char tier_name[256], vector_name[512];
  char *tmp_ptr;
  int mon_test_run = -1, freq;
  long mon_partition_idx = -1;
  char controller_name[128];

  err_msg[0] = '\0';
  tier_name[0] = '\0';
 
  if(!(tmp_ptr = getenv("MON_FREQUENCY")))
    interval = atoi(tmp_ptr)/1000;
  else
    interval = 10;  //If no -i option is provided
  
  while((opt = getopt(argc, argv, "t:T:c:D:i:X:L:")) != -1 )
  {
    switch(opt)
    {
      case 't':
        if(!strcasecmp(optarg, "ALL"))
          trnum = 0;
        else if(!ns_is_numeric(optarg))
          sprintf(err_msg, "TestRun (%s) provided is not numeric", optarg);
        else if(optarg[0] == '0')
          sprintf(err_msg, "TestRun (%s) cannot start with 0", optarg);
        else
          trnum = atoi(optarg);
        break;

      case 'T':
        strcpy(tier_name, optarg);
        break;

      case 'c':
        if(verify_controller_name(optarg, err_msg) > 0)
        {
          strcpy(controller_name, optarg);
          sprintf(w_dir, "/home/cavisson/%s", optarg);
        }
        break;

      case 'D':
        level = atoi(optarg);
        set_debug(level);
        break;

      case 'i':
        if(!ns_is_numeric(optarg))  //Not numeric. Hence setting it to default 60s.
          interval = 10;
        else
          interval = atoi(optarg);
        break;

      case 'X':                  //Not handling these cases
      case 'L': break;

      default:
        sprintf(err_msg, "Error: incorrect usages");
    }

    if(err_msg[0] != '\0')    //Exit if any error found
      display_help_and_exit(err_msg);
  }

//#################################
  //Setting mandatory arguements
  min_prog_intvl = interval;
  time_left = interval; 
 
  if(!(cmon_home = getenv("CAV_MON_HOME")))
  {
    //sprintf(err_msg,"Error: Environment variable CAV_MON_HOME not set");
    //display_help_and_exit(err_msg);
    cmon_home = "/home/cavisson/monitors/";
  }

  //setting running tr dir
  if(cmon_home)
  {
    if(getenv("MON_TEST_RUN"))
    {
      mon_test_run = atoi(getenv("MON_TEST_RUN"));
   
      if(getenv("MON_PARTITION_IDX"))
        mon_partition_idx = atol(getenv("MON_PARTITION_IDX"));

      if(mon_partition_idx == -1 || mon_partition_idx == 0)
        sprintf(running_tr_dir, "%s/logs/running_tests/%d", cmon_home, mon_test_run);
      else
        sprintf(running_tr_dir, "%s/logs/running_tests/%d_%ld", cmon_home, mon_test_run, mon_partition_idx);
    }
  }

  if(debug > 0)
    open_debug_log(err_msg);

  if(w_dir[0] == '\0')
    sprintf(err_msg, "Controller Name is mandatory");

  if(err_msg[0] != '\0')    //Exit if any error found
    display_help_and_exit(err_msg);

  //Set Work_dir. 
  if(setenv("NS_WDIR", w_dir, 1) < 0)
  {
    sprintf(err_msg, "Error in setting working dir (%s) in environment. Error: %s", w_dir, nslb_strerror(errno));
    display_help_and_exit(err_msg);
  }

//####################################
  //check if TR is running or not
  if(get_list_of_running_TR(tr_buf) < 0)
  {
    sprintf(err_msg, "Error in running command nsu_show_test_logs");
    display_help_and_exit(err_msg);
  }

  count = get_tokens(tr_buf, tr_list, " ", MAX_TR_LIST);
  if(trnum != 0)
  {
    for(i=0; i<count; i++)
    {
      int TR = atoi(tr_list[i]);
      if(TR == trnum)
        break;
    }
  

    if(i == count)  //Loop reaches end and could not find TR
    {
      sprintf(err_msg, "Testrun %d is not running. Hence Exiting", trnum);
      display_help_and_exit(err_msg);
    }
  }
  else if(count == 0)
  {
    debug_log(DLOG_L1, _NLF_, "No TestRun is running on Controller %s", w_dir);
    exit(-1);
  }
  
//#################################### 
  if(tier_name[0] == '\0')
    strcpy(tier_name, controller_name);

  //Create vector name
  create_vector_name(tier_name, vector_name);


//####################################
  //Forever loop
  while(1)
  {
    //Checking if test is over
    if(is_test_over(mon_test_run, mon_partition_idx, running_tr_dir) < 0)
      exit(1);
    
    //Handling when progress_report of different TR's are different. to aggregate at TR level.
    //This loop is to process data of TR whose progress report is different from this monitor's interval.
    /*If this monitor progress interval is 60, and TR's interval is 20, then aggregation of 3 samples should be done at TR level aggregation.*/
    freq = 0;
    do
    {
      //Sleep for the remaining time
      debug_log(DLOG_L1, _NLF_, "Going to sleep for tiem: %d, min_prog_interval: %d", time_left, min_prog_intvl);
      sleep(time_left);

      start_time = (unsigned long)time(NULL);                 //start time

      process_data(err_msg, trnum);                           //get TR and make connection and get data.

      end_time =  (unsigned long)time(NULL); 
      time_left = min_prog_intvl - (end_time - start_time);   //Returned time in seconds
      freq++;
    }while((interval - (freq * min_prog_intvl) > 0));

    reset_data(&overall_data);                  //Reset data
    aggregate_overall_data();  	                //Overall Aggregation of all TR
    show_data(vector_name);                     //Display data

    //checking for stopped TR and if stopped, need to remove from the list.
    node = tts_head;
    prev = NULL;
    while(node != NULL)
    {
      if(node->data_filled == 0)  // Data is not filled. It means TR was stopped.
      {
        TestTrafficInfo *tmp_node = node;
        if(prev == NULL)
          tts_head = node->next;
        else
          prev->next = node->next; 
  
        free(tmp_node);
      }
      else
      {
        reset_data(&(node->tts_data));    //Reset data
        node->data_filled = 0;
      }

      prev = node;
      node = node->next;
    }
  }
}
