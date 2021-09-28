/********************************************************************
* master process that will fire up
* clients on other computers
*******************************************************************/


/* Pending

1. call kill_all_childer() in case of error

*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <linux/unistd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <regex.h>
#include <signal.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "logging.h"
#include "sock.h"
#include "ns_log.h"

#include "ns_msg_com_util.h"
#include "ns_alloc.h"
#include "nslb_cav_conf.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nslb_util.h"
#include "netomni/src/core/ni_script_parse.h"
#include "nslb_static_var_use_once.h"
#include "ns_parent.h"
#include "ns_gdf.h"
#include "ns_runtime_changes.h"			
#include "ns_trace_level.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_master_agent.h"
#include "output.h"
#include "ns_page_dump.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_global_dat.h"
#include "nslb_server_admin.h"
#include "ns_percentile.h"
#include "ns_exit.h"
#include "ns_test_init_stat.h"
#include "ns_data_handler_thread.h"
#include "ns_runtime.h"
#include "ns_kw_usage.h"
#include "ns_error_msg.h"
#include "ns_vuser_runtime_control.h"

#define MAX_CONF_LINE_LENGTH 16*1024
#define MASTER_CONF  "sys/master.conf"
#define SESSION_RATE_MULTIPLE 1000
#define RETRY_DELAY 100000 //this value in ms will be passed in usleep for delay before retry
#define MAX_GENERATOR_ENTRIES 255
#define CHECK_GENERATOR_HEALTH_TIMEOUT 60

extern int first_time;
//extern char qty_msg_buff[RTC_QTY_BUFFER_SIZE];
int g_rtc_msg_seq_num = 0;
//int g_rtc_msg_seq_num = 0; //sequence number to track response from child.
//int rtc_failed_msg_rev = 0;
int total_client_started = 0;
//u_ns_ts_t local_rtc_epoll_start_time = 0;
int client_delay = 100; // 100ms default for now
// Array of client fds
int *clientfd;  // add this in Client_data later
char err_buff[1024] = ""; //buffer to filling error msg and printing on the stdout.
extern int create_client_table_entry(int *);
extern int master_init(char *user_conf);
extern void wait_for_start_msg(int udp_fp);
//int total_killed_generator;  //means how many generator is ignored
//extern void end_test_run( void );
extern int g_rtc_start_idx;
void upd_client_data(int *idx, struct sockaddr_in addr);

Master_agent send_data;
static int rnum = 0;
int phase_end_msg_flag;

//#define NS_MAX_EPOLL_FD 1024
//static struct timeval timeout, kill_timeout;
//static int nc_epfd = 0;
//static int rcv_amt;
#define LINE_LENGTH 1024
//nsu_server_admin tool is now replaced by function
//ServerCptr server_ptr;
extern char controller_ns_build_ver[128];
//only used by master
pthread_mutex_t gen_thread_cnt = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gen_cnt = PTHREAD_MUTEX_INITIALIZER;
int num_fail_generator = 0;
unsigned long gen_started_mask[4];
int gen_updated_timeout[MAX_GENERATOR_ENTRIES];
int g_start_sch_msg_seq_num;
extern void handle_fparam_rtc_done(int ignore_nvm);

void
handle_master_sigint( int sig )
{
  NSDL3_MESSAGES(NULL, NULL, "Received sig = %d", sig);
  print2f_always(rfp, "Stopping netstorm...\n");
  /* Fix for Bug#3952: In case of controller mode sigterm_received was never set to 1
   * Therefore generator machines never used to receive start last phase msg from controller
   * */
  sigterm_received = 1;
  send_msg_to_all_clients(FINISH_TEST_RUN_MESSAGE, 0);
}
#if 0
void shutdown_master(char *buf)
{
  //we are setting total client entries to total client started because kill all children will kill all started clients only.
  total_client_entries = total_client_started;
  //this line should not be changed, used in nsu_start_test to show error on GUI.
  print2f_always(rfp, "Controller is shutting down due to following error:\n");
  print2f_always(rfp, "\t%s\n", buf);
  //fflush(stderr);
  kill_all_children();
  exit(-1);
}
#endif
void shutdown_master_connections()
{
  int i;
  if (clientfd) {
    for (i = 0; i < total_client_entries; i++) {
      if (clientfd[i] != -1)
	close(clientfd[i]);
    }
  }
}

/* Exiting because connection making failed */
void shutdown_master_connect_failed()
{
  NSTL1(NULL, NULL, "Master is shuting down : Unable to connect to client(s)\n");
  fflush(stderr);
  shutdown_master_connections();
  NS_EXIT(-1, "Master is shuting down : Unable to connect to client(s)");
}

int
read_clients(char *buf)
{
  int port = 7893, num;
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  unsigned int ip_addr;

  //printf("Processing CLIENT %s\n", buf);
  num = sscanf(buf, "%s %s %d", keyword, text, &port);
  if ((num < 2) || (num > 3))
  {
    NSTL1_OUT(NULL, NULL,"read_keywords(): Need two or three fields after key CLIENT\n");
    return -1;
  }
  if (create_client_table_entry(&rnum) == FAILURE)
    return -1;
  if ((ip_addr = inet_addr(text)) < 0)
  {
    NSTL1_OUT(NULL, NULL,"read_keywords(): Invalid CLIENT address, ignoring <%s>\n", text);
  }
  else clients[rnum].ip_addr = ip_addr;

  clients[rnum].port = (unsigned short)port;
  return 0;
}

//To set the eth variable value
void upd_collect_no_eth_data()
{
  int i, j;
  for(i = 0; i < (total_client_entries - 1); i++ )
  {
    for(j = (i+1); j < total_client_entries; j++ )
    {
      if(clients[i].ip_addr == clients[j].ip_addr)
        clients[j].collect_no_eth_data = 1;
    }
  }
}

void
read_master_keywords(FILE* fp)
{

  char buf[MAX_CONF_LINE_LENGTH+1];
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  int  num;

  while (nslb_fgets(buf, MAX_CONF_LINE_LENGTH, fp, 0) != NULL)
  {
    buf[strlen(buf)-1] = '\0';
    if((buf[0] == '#') || (buf[0] == '\0'))
      continue;
    if ((num = sscanf(buf, "%s %s", keyword, text)) != 2)
    {
      printf("read_keywords(): At least two fields required  <%s>\n", buf);
      continue;
    }
    else
    {
      if (strncasecmp(keyword, "CLIENT", strlen(keyword)) == 0)
        read_clients(buf);
      else if(strncasecmp(keyword, "PORT", strlen("PORT")) == 0)
      {
/*         if ((num = sscanf(buf, "%s %d", keyword, &master_port)) != 2) */
/*         { */
/*           NSTL1_OUT(NULL, NULL,"read_keywords(): Need ONE fields after key PORT\n"); */
/*           continue; */
/*         } */
        // printf("master port=%d\n", master_port);
      }
      else if(strncasecmp(keyword, "DELAY_CLIENT", strlen(keyword)) == 0)
      {
        if ((num = sscanf(buf, "%s %d", keyword, &client_delay)) != 2)
        {
          NSTL1_OUT(NULL, NULL,"read_keywords(): Need ONE fields after key CLIENT_DELAY\n");
          continue;
        }
      }
    }
  } //End while
}

void read_master_conf_file(char *filename)
{
  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL)
  {
    sprintf(err_buff,"Error: in Opening file: %s\n", filename);
    //shutdown_master(err_buff);
  }
  read_master_keywords(fp);
  upd_collect_no_eth_data();
  fclose(fp);
}

void set_connection_with_clients(char *user_conf)
{
  struct sockaddr_in *clientaddr;
  int i;
  u_ns_ts_t tu1, tu2;
  int debug_on = 0;
 
  /*Run same commad on all clients*/
  if(strstr(argv0, "netstorm.debug"))
     debug_on = 1; // this flag will tell to agent

  if (total_client_entries == 0)
  {
    sprintf(err_buff, "Error: No Clients are defined in master.conf file.\n ");
    //shutdown_master(err_buff);
  }

  MY_MALLOC (clientfd, total_client_entries*sizeof(int), "clientfd", -1);
  /* memset'ing to -1 so we can makeout if they are not open */
  memset(clientfd, -1, sizeof (int) * total_client_entries); 
  MY_MALLOC (clientaddr, total_client_entries*sizeof(struct sockaddr_in), "clientaddr", -1);
  memset(clientaddr, 0, total_client_entries*sizeof(struct sockaddr_in));

  printf("Making connections to %d clients... \n", total_client_entries);
  tu1 = get_ms_stamp();
  for (i = 0; i < total_client_entries; i++)
  {
    if ((clientfd[i] = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("ns_master:  Error in creating TCP socket for client connection.  Aborting...\n");
      shutdown_master_connect_failed();
      break;
    }

    /* Prepare to get connected to the TCP server */
    memset(&clientaddr[i], 0, sizeof(clientaddr[i]));
    clientaddr[i].sin_family = AF_INET;
    clientaddr[i].sin_port = htons(clients[i].port);
    clientaddr[i].sin_addr.s_addr = clients[i].ip_addr;

    /* Now get connected to the TCP server */
    if (connect(clientfd[i], (struct sockaddr *)&clientaddr[i], sizeof(clientaddr[i])) < 0)
    {
      perror("ns_master:  Error in making connection to client.  Aborting...\n");
      printf("client[%d] %s, port = %d \n", i, inet_ntoa(clientaddr[i].sin_addr), 
             clients[i].port);
      //      close(clientfd[i]);
      shutdown_master_connect_failed();
      break;
    } else 
	NSDL2_CHILD(NULL, NULL, "Client[%d] %s Connected.\n", i, inet_ntoa(clientaddr[i].sin_addr));
  }
  tu2 = get_ms_stamp();
  printf("Connection made successfully to all clients (in %llu msec)\n", tu2 - tu1);

  printf("Sending data to %d clients... \n", total_client_entries);
  tu1 = get_ms_stamp();
  for (i = 0; i < total_client_entries; i++)
  {
    memset(&send_data, 0, sizeof(send_data));
    //fill values in struct to be send
    strcpy(send_data.scen_name, user_conf);
    send_data.opcode = OPCODE_INIT;
    send_data.port = parent_port_number; /* in master, master is like parent */
    send_data.event_logger_port = event_logger_port_number; 
    send_data.debug_on = debug_on;
    send_data.collect_no_eth_data = clients[i].collect_no_eth_data;
    /* First client takes max delay, last client takes 0 */
/*     send_data.wait_for_msec = client_delay * (total_client_entries - (i + 1)); */

    //Send struct to agent
    if (send(clientfd[i], &send_data, sizeof(send_data), 0) <= 0)
    {
      perror("ns_master: Error sending structure to clients\n");
      break;
    } else 
	NSDL2_CHILD(NULL, NULL, "Sent data to client[%d] %s.\n", i, inet_ntoa(clientaddr[i].sin_addr));

    total_client_started++;
/*     s2 = get_ms_stamp(); */
/*     if (i < (total_client_entries - 1) && (s2 - s1) < client_delay) { */
/*       printf("Sleeping for %u (s1 = %u, s2 = %u, client_delay = %u)\n",  */
/* 	     (client_delay - (s2 - s1)), s1, s2, client_delay); */
/*       usleep ((client_delay - (s2 - s1)) * 1000); */
/*     } */
  }
  tu2 = get_ms_stamp();
  printf("Sent data successfully to clients (in %llu msec)\n", tu2 - tu1);
  
  /* Close open sockets */
  shutdown_master_connections();
}

void my_handler_ignore(int data)
{
}

/* Need to ship testrun.gdf of each generator to controller*/
void send_testrun_gdf_to_controller()
{
  char cmd_args[MAX_LINE_LEN];
  char file_name[MAX_LINE_LEN];
  char path[MAX_LINE_LEN];
  int ret = 0;
  int cnt = 0;
  int len = 0;
  //Gaurav: the server structure nsu_server_admin functions
  ServerCptr server_ptr;
  memset(&server_ptr, 0, sizeof(ServerCptr));
  MY_MALLOC_AND_MEMSET(server_ptr.server_index_ptr, (sizeof(ServerInfo)), "Server Index ptr", -1);
  MY_MALLOC(server_ptr.server_index_ptr->server_ip, (sizeof(char) * 20), "Server IP", -1);

  NSDL1_MESSAGES(NULL, NULL, "Method called");

  //Create directory on controller
  sprintf(cmd_args, "mkdir -p %s/logs/TR%s/NetCloud/%s/%s/", g_controller_wdir, g_controller_testrun, 
                     global_settings->event_generating_host,  global_settings->tr_or_partition);
  sprintf(server_ptr.server_index_ptr->server_ip, "%s", master_ip);
  
  nslb_replace_strings(cmd_args, ":", "%3A");
  cnt = 0;
  len = strlen(cmd_args);
  while(cmd_args[cnt] != ' ' && cnt < len)
    cnt++;
  if(cnt < len)
    cmd_args[cnt]=':';

  NSDL2_MESSAGES(NULL, NULL, "Creating generator directory on controller, cmd_args = %s", cmd_args);
  ret = nslb_run_users_command(&server_ptr, cmd_args);
  if (ret != 0) 
  { 
    NSDL2_MESSAGES(NULL, NULL, "Unable to create generator directory on controller");
    NS_EXIT(ret, CAV_ERR_1014011, global_settings->event_generating_host, master_ip);
  }

  //Ship testrun.gdf file 
  sprintf(file_name, "%s/logs/%s/testrun.gdf", g_ns_wdir, global_settings->tr_or_partition);
  sprintf(path, "%s/logs/TR%s/NetCloud/%s/%s/", g_controller_wdir, g_controller_testrun,
                 global_settings->event_generating_host,  global_settings->tr_or_partition);
  ret = nslb_ftp_file(&server_ptr, file_name, path, 0);

  NSDL2_MESSAGES(NULL, NULL, "Send testrun.gdf to controller, file_name = %s, path = %s\n", file_name, path);

  if (ret != 0) 
  { 
    NSDL2_MESSAGES(NULL, NULL, "Unable to send testrun.gdf to controller");
    NS_EXIT(ret, CAV_ERR_1000024, "testrun.gdf", master_ip);
  }

  FREE_AND_MAKE_NULL(server_ptr.cmd_output, "serveradmin.cmd_output", -1);
  server_ptr.cmd_output_size = 0;
  //Change owner from test run number
  sprintf(cmd_args, "chown -R %s %s/logs/TR%s/NetCloud/%s/TR%d/", getenv("NS_CONTROLLER_USR_GRP_NAME"), getenv("NS_CONTROLLER_WDIR"), 
         getenv("NS_CONTROLLER_TEST_RUN"), global_settings->event_generating_host, testidx);

  nslb_replace_strings(cmd_args, ":", "%3A");
  cnt = 0;
  len = strlen(cmd_args);
  while(cmd_args[cnt] != ' ' && cnt < len)
    cnt++;
  if(cnt < len)
    cmd_args[cnt]=':';

  ret = nslb_run_users_command(&server_ptr, cmd_args);
  if (ret != 0) 
  { 
    NSDL2_MESSAGES(NULL, NULL, "Unable to change owner of generator directory on controller");
    NS_EXIT(ret, CAV_ERR_1014012, global_settings->event_generating_host, master_ip);
  }
  nslb_clean_cmon_var(&server_ptr);
}

void write_generator_table_in_csv()
{
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  char buf[LINE_LENGTH];
  FILE *fp;
  int j; 
  char ptr[128];
  char *token_ptr;
 
  sprintf(buf, "logs/%s/reports/csv/generator_table.csv", global_settings->tr_or_common_files);

  if (!(fp = fopen(buf, "a+"))) {
    //NSTL1(NULL, NULL, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, testidx);
    //perror("netstorm");
    NS_EXIT(-1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }

  for (j = 0; j < sgrp_used_genrator_entries; j++) 
  {
    //Remove IPV4: from resolved IP field 
    strcpy(ptr, generator_entry[j].resolved_IP);
    if((token_ptr = strtok(ptr, ":")) != NULL) {
      token_ptr = strtok(NULL, " ");
    }
    fprintf(fp, "%s,%d,%s,%s,%s,%s\n", generator_entry[j].gen_name, j, generator_entry[j].work, generator_entry[j].IP, token_ptr, generator_entry[j].agentport);
  }
  fclose(fp);
}

//Function used to find generator index for given generator ip and test run number
//Returns corresponding generator index in success case else -1
int find_generator_idx_using_ip (char *gen_ip, int testidx)
{
  int idx;
  NSDL1_MESSAGES(NULL, NULL, "Method called, gen_ip = %s, testidx = %d", gen_ip, testidx);
  NSTL1(NULL, NULL, "Method called, gen_ip = %s, testidx = %d", gen_ip, testidx);
  for (idx = 0; idx < sgrp_used_genrator_entries; idx++)
  {
    if (gen_ip) { 
      if (!strcmp(gen_ip, generator_entry[idx].resolved_IP))
      {
        NSDL1_MESSAGES(NULL, NULL, "Generator ip match returning index = %d", idx);
        return idx;
      }
    } else if (testidx != 0) {
      if (testidx == generator_entry[idx].testidx)
      {
        NSDL1_MESSAGES(NULL, NULL, "Generator testidx match returning index = %d", idx);
        return idx;
      }

    }
  }
  return -1; //Not found
}

static char use_gen_file_name[MAX_DATA_LINE_LENGTH];//Used generator file name
//Function used to find controller type, here we need to find whether machine is external or internal
//In case of external controller we need to run external generators. Hence get external used generator list

static void ignore_sigchild(int data)
{ 
  NSDL2_MESSAGES(NULL, NULL, "Ignoring signal %d for getting controller type.", data);
} 

int find_controller_type(char *type)
{
  char sys_cmd[MAX_COMMAND_SIZE];
  char controller_type[10];
  FILE *app = NULL;
  char temp[1024];
  //int ret;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called");

  sprintf(sys_cmd, "%s/bin/nsi_get_controller_type", g_ns_wdir);
  
  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, ignore_sigchild);

  if ((app = popen(sys_cmd, "r")) == NULL){
    NSTL1(NULL, NULL, "Error while fetching controller type errno = '%s'", nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000019, sys_cmd, errno, nslb_strerror(errno));
  }

  NSDL2_MESSAGES(NULL, NULL, "popen the file");
  while (nslb_fgets(temp, 1024, app, 0) != NULL)
  {
    strcpy(controller_type, temp);
  }
  pclose(app);
  (void) signal( SIGCHLD, prev_handler);
  
  NSDL1_MESSAGES(NULL, NULL, "controller_type = %s", controller_type);

  if(!strncmp(controller_type, "Internal", 8)) 
    strcpy(type, "Internal");
  else
    strcpy(type, "External");
  return(0);
} 

/* Function used read used generator list file and fill generator structure*/
static void read_and_fill_used_generators()
{
  int /* total_flds, */ rnum;
  char file_name[MAX_DATA_LINE_LENGTH];
  char line[2024];
  char *field[20];
  FILE* fp = NULL;
  char *ptr; 
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  //Used generator file path
  sprintf(file_name, "%s/.tmp/.controller/%s", g_ns_wdir, use_gen_file_name);
  
  if((fp = fopen(file_name, "r")) == NULL)
  {
    //NSTL1(NULL, NULL, "Used generator file (%s) does not exist. Hence exiting...", file_name);
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }

  //File Format: generator_name|IP|port|work
  while (nslb_fgets(line, 2024, fp, 0) != NULL)
  {
    //total_flds = get_tokens(line, field, "|", 20);
    get_tokens(line, field, "|", 20);
    create_generator_table_entry(&rnum);
    //Copy value to struct 
    strcpy((char *)generator_entry[rnum].gen_name, field[0]);
    strcpy(generator_entry[rnum].IP, field[1]);
    strcpy(generator_entry[rnum].work, field[2]);
    strcpy(generator_entry[rnum].agentport, field[3]);
    if((ptr = strchr(generator_entry[rnum].agentport, '\n')) != NULL)
    *ptr = '\0';
    NSDL1_MESSAGES(NULL, NULL, "generator_entry[rnum].gen_name = %s, generator_entry[rnum].IP = %s, generator_entry[rnum].work = %s, generator_entry[rnum].agentport = %s", generator_entry[rnum].gen_name, generator_entry[rnum].IP, generator_entry[rnum].work, generator_entry[rnum].agentport);
  }  
  fclose(fp);
}


// For external controller we need to read used generator list 
// and need to fill generator structure.
void extract_external_controller_data()
{
  char type[9];

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  if(ni_make_tar_option & DO_NOT_CREATE_TAR)
  {
    write_log_file(NS_GEN_VALIDATION, "For external controller, need to read used-generator list and fill generator entries");
    NSDL2_MESSAGES(NULL, NULL, "For external controller, need to read used-generator list and fill generator structure.");

    char proj_subproj_file[1024]; //buffer to store project-subproject name

    //Initialize generator structure
    init_generator_entry(); 
    //In case of internal/external controller we need to find controller type and then read generator list wrt controller type
    find_controller_type(type);
    if(!strncmp(type, "Internal", 8)) 
      strcpy(use_gen_file_name, "internal_used_generator_list.dat");
    else
      strcpy(use_gen_file_name, "external_used_generator_list.dat");
    
    //read used-generator file and fill generator structure
    read_and_fill_used_generators();

    //Update sgrp_used_genrator_entries 
    sgrp_used_genrator_entries = total_generator_entries;
    NSDL2_MESSAGES(NULL, NULL, "Number of used generators, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);

    //save scenario project and subproject
    sprintf(proj_subproj_file, "%s/%s", g_project_name, g_subproject_name);     
    //copy scenario name in global vars. 
    strcpy(netomni_proj_subproj_file, proj_subproj_file);
    sprintf(netomni_scenario_file, "%s.conf", g_scenario_name);
  }
}

void close_gen_pct_file(int idx) 
{

  NSDL1_MESSAGES(NULL, NULL, "Method calledo, Generator id = %d", idx);

  if(generator_entry[idx].pct_fd == -1)
  {
    NSTL1_OUT(NULL, NULL, "PctMessage.dat file pointer is not allocated for %d generator\n", idx);
    return;
  }
  close(generator_entry[idx].pct_fd);
  generator_entry[idx].pct_fd = -1;
}

void create_new_ver_of_gen_pctMessage_dat(int idx)
{
  char buf[1024 + 1];

  NSDL2_MESSAGES(NULL, NULL, "Method called, closing old pct_msg_file_fd = %d", generator_entry[idx].pct_fd);

  if(generator_entry[idx].pct_fd > 0)
    close(generator_entry[idx].pct_fd);

  sprintf(buf, "logs/TR%d/NetCloud/%s/TR%d/%lld/pctMessage.dat.%d",
                testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx,
                g_partition_idx, testrun_pdf_and_pctMessgae_version+1);

  NSDL2_MESSAGES(NULL, NULL, "Opening file fname = %s", buf);
  generator_entry[idx].pct_fd = open(buf, O_CREAT|O_CLOEXEC|O_WRONLY |O_LARGEFILE| O_APPEND, 0666);

  if (generator_entry[idx].pct_fd < 0)
  { 
    NS_EXIT(-1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
  }
  
  NSDL2_MESSAGES(NULL, NULL, "Generator's pct_msg_file_fd = %d", generator_entry[idx].pct_fd);
}

void dump_pdf_data_into_file(int idx)
{
  char buf[1024 + 1];
  struct stat info;

  pdf_data_hdr *pdf_hdr = (pdf_data_hdr *)parent_pdf_addr;

  if(!testrun_pdf_and_pctMessgae_version)
    sprintf(buf, "logs/TR%d/NetCloud/%s/TR%d/%lld/pctMessage.dat", 
                  testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx, g_partition_idx);
  else
    sprintf(buf, "logs/TR%d/NetCloud/%s/TR%d/%lld/pctMessage.dat.%d", 
                  testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx, g_partition_idx, testrun_pdf_and_pctMessgae_version);

  if((stat(buf, &info) == 0) && ((info.st_size + total_pdf_data_size_8B) > 2147483648)) //2GB
  {
    NSTL1(NULL, NULL, "Existing pctMessage.dat size exceed maximum size limit 2GB. Creating new file." );
    is_new_tx_add = 1;
  }

  if(is_new_tx_add)
  {
    testrun_pdf_ts = pdf_hdr->abs_timestamp;
    create_new_ver_of_gen_pctMessage_dat(idx);
  }
}

void create_gen_pct_file(int idx) 
{
  NSDL1_MESSAGES(NULL, NULL, "Method called. idx = %d", idx);

  char buf[4096];
  struct stat info;
  NSDL1_MESSAGES(NULL, NULL, "Method called");

  sprintf(buf, "%s/logs/TR%d/NetCloud/%s/TR%d", g_ns_wdir, testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx);
  if(stat( buf, &info ) != 0 )
  {
    NSTL1_OUT(NULL, NULL, "Cannot access %s. errno = %d, error = %s", buf, errno, nslb_strerror(errno));
    return;
  }
  strcpy(generator_entry[idx].gen_path, buf);

  if(g_partition_idx == 0)
    sprintf(buf, "logs/TR%d/NetCloud/%s/TR%d/pctMessage.dat", testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx);
  else
    sprintf(buf, "logs/TR%d/NetCloud/%s/TR%d/%lld/pctMessage.dat", testidx, generator_entry[idx].gen_name, generator_entry[idx].testidx, g_partition_idx);

  generator_entry[idx].pct_fd = open(buf, O_CREAT|O_CLOEXEC|O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); 

  if (generator_entry[idx].pct_fd == -1)
  {
    NS_EXIT(-1, CAV_ERR_1000006, buf, errno, nslb_strerror(errno)); 
  }
}

static int parse_check_generator_health_args(char *gen_health_buf, char *gen_and_ip, int gen_id)
{
  char *buildVersion = NULL;
  char *testRun = NULL;
  char *diskAvail = NULL;
  char *homeDiskAvail = NULL;
  char *rootDiskAvail = NULL;
  char *cpuAvail = NULL;
  char *memAvail = NULL;
  char *comma_fields[16];
  char *ptr = NULL;
  char *bandwidthAvail = NULL;
  char *numcpuAvail = NULL;
  int num;

  if((ptr = strchr(gen_health_buf, '\n')))
    *ptr = '\0';

  NSDL1_MESSAGES(NULL, NULL, "Method called, %s, gen_health_buf = %s", gen_and_ip, gen_health_buf);
  NSTL1(NULL, NULL, "%s, gen_health_buf = %s", gen_and_ip, gen_health_buf);

  if(!strncmp(gen_health_buf, "INVALID", 7))
  {
    NSTL1_OUT(NULL, NULL, "Ignoring %s as blade path '%s' is invalid",
                           gen_and_ip, generator_entry[gen_id].work);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as blade path '%s' is invalid",
                                           gen_and_ip, generator_entry[gen_id].work);
    return -1;
  }

  num = get_tokens(gen_health_buf, comma_fields, ",", 10);

  //Number of argument must be atleast 7
  if(num < 7)
  {
    NSTL1_OUT(NULL, NULL, "Ignoring %s as invalid number of arguments [%d] in health check %s", gen_and_ip, num, gen_health_buf);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as health information"
                        " (Build Version, Memory, Disk, Bandwidth, Number of Cpu's) is invalid", gen_and_ip);
    return -1;
  }

  buildVersion = comma_fields[0];
  testRun = comma_fields[1];
  diskAvail = comma_fields[2];
  cpuAvail = comma_fields[3];
  memAvail = comma_fields[4];
  bandwidthAvail = comma_fields[5];
  numcpuAvail = comma_fields[6];

  NSDL3_MESSAGES(NULL, NULL, "buildVersion = %s, testRun = %s, diskAvail = %s, cpuAvail = %s, memAvail = %s, "
                 "bandwidthAvail = %s, homeDiskAvail = %s, rootDiskAvail = %s, numcpuAvail = %s ",
                 buildVersion, testRun, diskAvail, cpuAvail, memAvail, bandwidthAvail, homeDiskAvail,
                 rootDiskAvail, numcpuAvail);

  //step1 - check build version (mandatory)
  if(strcmp(buildVersion, controller_ns_build_ver)) 
  {
    NS_DUMP_WARNING("Generator build version '%s' on %s is not same with controller '%s'",
                       buildVersion, gen_and_ip, controller_ns_build_ver);
    NSTL1(NULL, NULL, "Generator build version '%s' on %s is not same with controller '%s'",
                       buildVersion, gen_and_ip, controller_ns_build_ver);
  }
 
  //step2: Check test is running or not (mandatory)
  if(atoi(testRun))
  {
    NSTL1_OUT(NULL, NULL, "Ignoring %s as testrun '%s' is already running", gen_and_ip, testRun);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as testrun '%s' is already running",
             gen_and_ip, testRun);
    return -1;
  }

  //step3: Check Disk avaliablity (its optional) 
  if(checkgenhealth.minDiskAvailability)
  {
    int ret = 0;
    //if '/' + '/home' disk space is given
    if((homeDiskAvail = strchr(diskAvail, '+')) != NULL) {
      *homeDiskAvail = '\0';
      homeDiskAvail++;
      rootDiskAvail = diskAvail;
    }
    else
     homeDiskAvail = diskAvail;

    //if /home mount point is not available then check with /
    if(*homeDiskAvail == '\0')
      homeDiskAvail = rootDiskAvail;
    
    /* For running a netcloud test what should be the minimum memory on / if /home is mounted ?? */
    if(rootDiskAvail && (atoi(rootDiskAvail) < 5)) {
      NSTL1_OUT(NULL, NULL, "Ignoring %s as root disk available Space '%sGB' is less than threshold disk Space '5GB'",
                             gen_and_ip, rootDiskAvail);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as root disk available Space '%sGB'"
               " is less than threshold disk Space '5GB'", gen_and_ip, rootDiskAvail);
      ret = 1;
    }

    if(homeDiskAvail && (atoi(homeDiskAvail) < checkgenhealth.minDiskAvailability)) {
      NSTL1_OUT(NULL, NULL, "Ignoring %s as home disk available Space '%sGB' is less than threshold disk Space '%dGB'",
                gen_and_ip, homeDiskAvail, checkgenhealth.minDiskAvailability);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as home disk available Space"
               " '%sGB' is less than threshold disk Space '%dGB'", gen_and_ip, homeDiskAvail,
               checkgenhealth.minDiskAvailability);
      ret = 1;
    }
    if(ret)
      return -1;
  }

  //step4 : Check cpu availability (Its optional)
  if(checkgenhealth.minCpuAvailability && (atoi(cpuAvail) > checkgenhealth.minCpuAvailability))
  {
    if(atoi(cpuAvail) > checkgenhealth.minCpuAvailability)
    {
      NSTL1_OUT(NULL, NULL, "Ignoring %s as CPU utilization pct '%s%%' is exceeded from threshold CPU '%d%%'",
              gen_and_ip, cpuAvail, checkgenhealth.minCpuAvailability);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as CPU utilization pct '%s%%' "
             "is exceeded from threshold CPU '%d%%'", gen_and_ip, cpuAvail, checkgenhealth.minCpuAvailability);
      return -1;
    }
  }

  //step5 : Check memory availability (Its optional) 
  if(checkgenhealth.minMemAvailability)
  {
    if(atoi(memAvail) < checkgenhealth.minMemAvailability)
    {
      NSTL1_OUT(NULL, NULL, "Ignoring %s as available memory '%sGB' is less than threshold memory '%dGB'",
                gen_and_ip, memAvail,  checkgenhealth.minMemAvailability);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as available memory '%sGB'"
               " is less than threshold memory '%dGB'", gen_and_ip, memAvail, checkgenhealth.minMemAvailability);
      return -1;
    }
  }

  //step6: Check bandwidth availability (Its optional)
  //Bug 55886: bandwidth for Vms are -1/0; skip those generators
  if(checkgenhealth.minBandwidthAvailability)
  {
    int bandwidth = atoi(bandwidthAvail);
    if(bandwidth > 0 && (bandwidth < checkgenhealth.minBandwidthAvailability))
    {
      NSTL1_OUT(NULL, NULL, "Ignoring %s as available bandwidth '%dMbps' is less than threshold bandwidth '%sMbps'",
                gen_and_ip, bandwidthAvail, checkgenhealth.minBandwidthAvailability);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as available bandwidth '%sMbps'"
               " is less than threshold bandwidth '%dMbps'", gen_and_ip, bandwidthAvail, checkgenhealth.minBandwidthAvailability);
      return -1;
    }
  }

  //step7: Check number of cpu availability (Its optional)
  int num_cpu = atoi(numcpuAvail);
  if(num_cpu > 0)
  {
    generator_entry[gen_id].num_cpus = num_cpu;
    NIDL(4, "Num_Cpus = %d", generator_entry[gen_id].num_cpus);
  }
  else
  {
    NSTL1_OUT(NULL, NULL, "Ignoring %s as available cpus '%s'.",
              gen_and_ip, numcpuAvail);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Ignoring %s as available cpus '%s'.",
             gen_and_ip , numcpuAvail);
    return -1;
  }

  return 0; //Success
}


//Create generator name directory in NetCloud directory
void create_generator_dir_by_name()
{
  int i;
  char netcloud_cmd[1024];
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  for (i = 0; i < sgrp_used_genrator_entries; i++)
  {
    sprintf(netcloud_cmd, "mkdir -p %s/logs/TR%d/NetCloud/%s", g_ns_wdir, testidx, generator_entry[i].gen_name);
    NSDL4_MESSAGES(NULL, NULL, "Creating generator directory, running command %s", netcloud_cmd);
    if(system(netcloud_cmd) != 0)
    {
      //NSTL1(NULL, NULL, "Error in creating generator %s directory in NetCloud directory\n", generator_entry[i].gen_name);
      NS_EXIT(-1, CAV_ERR_1000019, netcloud_cmd, errno, nslb_strerror(errno));
    }

    //[Test Initialization Status]:creating generator directories inside TR/ns_logs/TestInitStatus/Generators
    sprintf(netcloud_cmd, "mkdir -p %s/logs/TR%d/ready_reports/TestInitStatus/Generators/%s", g_ns_wdir, testidx,
                           generator_entry[i].gen_name);
    NSDL4_MESSAGES(NULL, NULL, "Creating generator directory, running command %s", netcloud_cmd);
    if(system(netcloud_cmd) != 0)
    {
      NSTL1(NULL, NULL, "Error in creating generator %s directory in ns_logs directory\n", generator_entry[i].gen_name);
      NS_EXIT(-1, CAV_ERR_1000019, netcloud_cmd, errno, nslb_strerror(errno));
    }
  }
  NSDL1_MESSAGES(NULL, NULL, "Method exited");
}

//Enhancement 66273: to get generator testrun number
static void get_generator_testrun(int gen_id, char *test_or_session)
{
  char testrunnum[16];
  char generator_path[1024];
  FILE *gen_fp = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method called, gen_id = %d", gen_id);
  sprintf(generator_path, "%s/logs/TR%d/ready_reports/TestInitStatus/Generators/%s/TestRunNumber", 
                                g_ns_wdir, testidx, generator_entry[gen_id].gen_name);
  if((gen_fp = fopen(generator_path, "r")) == NULL)
  {
    test_or_session[0] = '\0';
    NSDL1_MESSAGES(NULL, NULL, "Failed to read generator test run number from path %s, error: %s", generator_path,
                       nslb_strerror(errno));
  }
  else
  {
    fread(testrunnum, 1, 15, gen_fp);
    fclose(gen_fp);
    generator_entry[gen_id].testidx = atoi(testrunnum);
    snprintf(test_or_session, 31, "%s %s", (global_settings->continuous_monitoring_mode?"Session":"Testrun"), testrunnum);
  }

  //TODO: saving generator cavisson.netstorm.log on controller on exit
  sprintf(generator_path, "%s/logs/TR%d/%s_cavisson.netstorm.log", g_ns_wdir, testidx, generator_entry[gen_id].gen_name);
  gen_fp = fopen(generator_path, "w");

  if(generator_entry[gen_id].gen_keyword[0]) {
    if(gen_fp != NULL) {
      print2f_always(gen_fp, "%s", generator_entry[gen_id].gen_keyword);
      fclose(gen_fp);
    }
  }

  NSDL2_MESSAGES(NULL, NULL, "Method exit, gen_id = %d, test_or_session = %s", gen_id, test_or_session);
}
/* Netomni Scenario Distribution
 *
 * Add new function
 * ship_scen_data_to_all_generators()
 * It will take list of generators and find the IP address
 * and then ship file in a loop
 * using following command:
 * $ nsu_server_admin_int -s 192.168.1.66 -F /tmp/x -D /tmp
 *
 * FTPing file /tmp/x to server 192.168.1.66 at destination directory /tmp
 * Making connection to server 192.168.1.66
 *
 * File FTPed successfully
 * Then we need to run command using server admin
 * to untar files
 * nsu_server_admin_int -s 192.168.1.66 -c 'ls -ltr'
 * Then we need to run test
 * using server admin
 * nsu_server_admin_int -s 192.168.1.66 -c 'nsu_start_test -n xxxxx'*/
//#define MAX_COMMAND_SIZE 4096
#define ERR_MSG_SIZE  1024*1024
//Third argument "int j" is added in this function to get index of generator_entry, because we need generator_ip, generator_port & generator_controller for file "NetCloud.data".
static int chk_command_op (ServerCptr *ptr, int j)
{
  char net_cloud_data_path[1024];
  FILE *net_cloud_fp = NULL;
  char *net_cloud_tr_ptr = NULL;
  char test_or_session[32];
 
  NSDL1_MESSAGES(NULL, NULL, "Method called");

  
  /* Creating file NetCloud/NetCloud.data inside TR dir.
   * File format is: 
   * KEYWORD TESTRUN|GEN NAME|GEN IP|CAVMON PORT|/home/cavisson/work6
   * Eg: NETCLOUD_GENERATOR_TRUN 33617|192.168.1.66|7891|/home/cavisson/work6
   * Which is further used to delete TestRuns from generators. 
   *
   * Here format of err_msg is:
   * Making connection to server 192.168.1.66
   *
   * Netstorm started successfully. TestRunNumber is 34209
   */
  //sprintf(err_msg, "%s", ptr->cmd_output);
  if(j != -1)
  {
    NSTL1(NULL, NULL, "cmd_output = %s", ptr->cmd_output);
    snprintf(generator_entry[j].gen_keyword, 24000, "%s", ptr->cmd_output);
    if((net_cloud_tr_ptr = strstr(ptr->cmd_output, "Netstorm started successfully. TestRunNumber is")) == NULL)
    {
      get_generator_testrun(j, test_or_session);
      snprintf(generator_entry[j].gen_keyword, 24000, "Error in starting %s on server %s, error:\n%s", test_or_session, 
                                              generator_entry[j].IP, ptr->cmd_output);
      /*
      if(global_settings->con_test_gen_fail_setting.percent_started == 100) {
        //NSTL1_OUT(NULL, NULL, "Error in starting %s on server %s", test_or_session, generator_entry[j].IP);
        NS_EL_2_ATTR(EID_NS_INIT, -1, -1, EVENT_CORE, EVENT_INFORMATION, __FILE__, (char*)__FUNCTION__,
                         "Error in starting %s on server %s", test_or_session, generator_entry[j].IP);
        if(test_or_session[0]){
          NS_EXIT(-1, CAV_ERR_1014013, generator_entry[j].gen_name, test_or_session, generator_entry[j].gen_keyword);
        }
        else{
          NS_EXIT(-1, CAV_ERR_1014026, generator_entry[j].gen_name, generator_entry[j].gen_keyword);
        }
      }*/
      pthread_mutex_lock(&gen_cnt);
      num_fail_generator++;
      pthread_mutex_unlock(&gen_cnt);
      NSTL1(NULL, NULL, "Skipping server %s.", generator_entry[j].IP);
      return(-1);
    }
   
    net_cloud_tr_ptr += 48;
    char *ptr = strchr(net_cloud_tr_ptr, '\n');
    if(ptr != NULL)
      *ptr = '\0';
    else
    {
      //Remove new line character
      net_cloud_tr_ptr[strlen(net_cloud_tr_ptr) - 1] = '\0';
    }

    sprintf(net_cloud_data_path, "%s/logs/TR%d/NetCloud/NetCloud.data", g_ns_wdir, testidx);

    if (net_cloud_tr_ptr != NULL)
    {
      if ((net_cloud_fp = fopen(net_cloud_data_path, "a+")) == NULL)
      {
        NSTL1_OUT(NULL, NULL, "Error in opening NetCloud.data file.\n");
        return(-1);
      }
      //KEYWORD GEN_TESTRUN|GEN IP|CAVMON PORT|WORK
      fprintf(net_cloud_fp, "NETCLOUD_GENERATOR_TRUN %s|%s|%s|%s|%s|%s|%d|%s\n", net_cloud_tr_ptr, generator_entry[j].gen_name, generator_entry[j].IP, generator_entry[j].agentport, generator_entry[j].work, generator_entry[j].resolved_IP, j, generator_entry[j].location);
      fclose(net_cloud_fp);
      //Save test run number and generator path per generator. 
      generator_entry[j].testidx = atoi(net_cloud_tr_ptr);
    }
  }

  return(0);
}

typedef struct gen_attr
{
  char gen_name[GENERATOR_NAME_LEN];
  char IP[128]; 
  char agentport[6];
  char work[512]; 
  int mode;
  int gen_id;
  int testidx;
}gen_attr;

#define INC_THREAD_COUNT(gen_id)\
  if(nslb_set_bitflag(gen_started_mask, gen_id))\
    NSDL1_PARENT(NULL, NULL, "Thread is already set");\

#define DEC_THREAD_COUNT(gen_id)\
  if(nslb_reset_bitflag(gen_started_mask, gen_id))\
    NSDL1_PARENT(NULL, NULL, "Thread is already reset");\

#define CHECK_ALL_THREAD_DONE nslb_check_all_reset_bits(gen_started_mask)

#define MAKE_COMMAND_FOR_CMON \
  nslb_replace_strings(cmd_args, ":", "%3A");\
  cnt = 0;\
  len = strlen(cmd_args);\
  while(cmd_args[cnt] != ' ' && cnt< len)\
    cnt++;\
  if(cnt< len)\
    cmd_args[cnt]=':';

void *start_gen_processing(void *args)
{
  char sys_cmd[MAX_COMMAND_SIZE + 1];
  char cmd_args[4096];
  //ftp buffers
  char file_name[MAX_LINE_LEN]={0};
  char path[MAX_LINE_LEN];
  char gen_and_ip[MAX_BUF_SIZE + 1];
  gen_attr *a = (gen_attr *)args;
  int cnt = 0;
  int len = 0;
  int ret = 0;
  int ret_val;
  double size;
  time_t start_time;
  char addKW[1024] = "";
  ServerCptr server_ptr;
  server_ptr.cmd_output_size = 0;
  memset(&server_ptr, 0, sizeof(ServerCptr));
  MY_MALLOC_AND_MEMSET(server_ptr.server_index_ptr, (sizeof(ServerInfo)), "Server index ptr", -1);
  MY_MALLOC(server_ptr.server_index_ptr->server_ip, (sizeof(char) * 20), "Server ip", -1);
  server_ptr.server_index_ptr->topo_server_idx = atoi(a->agentport);
  
  struct stat buf;

  // Initializing thread local storage
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);

  NSDL1_MESSAGES(NULL, NULL,"[Callback] STARTED Gen Name = %s, Gen IP = %s, Agent Port = %s, Mode = %d,"
                            " Gen Work = %s, Gen id = %d, testidx = %d, a = %p", a->gen_name, a->IP, a->agentport,
                             a->mode, a->work, a->gen_id, a->testidx, a);   

  GeneratorEntry *gen_ptr = &generator_entry[a->gen_id];
  sprintf(server_ptr.server_index_ptr->server_ip, "%s", a->IP);
  snprintf(gen_and_ip, MAX_BUF_SIZE, "<generator:%s ip:%s>", a->gen_name, a->IP);

  switch(a->mode)
  {
    case CHECK_GENERATOR_HEALTH:

      sprintf(cmd_args, "%s/bin/nsu_check_health %s", a->work, a->work);
      //sprintf(cmd_args, "cat %s/.nsu_check_health", a->work);
      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      MAKE_COMMAND_FOR_CMON
      ret = nslb_run_users_command(&server_ptr, cmd_args);
      if(ret != 0)
      {
        if(ret == SERVER_ADMIN_CONN_FAILURE)
        {
          NSTL1_OUT(NULL, NULL, "Cmon is not running on %s", gen_and_ip);
          snprintf(gen_ptr->gen_keyword, 24000, "Cavisson Monitoring Agent on %s is not running", gen_and_ip);
          goto TLS_FREE;
        }
        sprintf(cmd_args, "%s/bin/nsu_check_health %s", a->work, a->work);
        MAKE_COMMAND_FOR_CMON
        ret = nslb_run_users_command(&server_ptr, cmd_args);
        if(ret != 0)
        {
          NSTL1_OUT(NULL, NULL, "\nEither cmon not running or command failed '%s' on %s", cmd_args, gen_and_ip);
          snprintf(gen_ptr->gen_keyword, 24000, "Unable to get generator %s health information"
                     " (Build Version, Memory, Disk, Bandwidth)", gen_and_ip);
          goto TLS_FREE;
        }
      }

      /***************************************************************************************************
       * Format of file
       * version,testrun,freediskspace,cpuUtilizationpct,memAvailable,bandwidthAvailable
       * fields[0],fields[1],fields[2],fields[3],fields[4],fields[5]
       * total fields = 6; may increase in future
       * ret = 0; success
       *        1; failure
       *
       ***************************************************************************************************/
      char check_buf[MAX_BUF_SIZE + 1];
      snprintf(check_buf, MAX_BUF_SIZE, "%s", server_ptr.cmd_output);
      if((ret = parse_check_generator_health_args(check_buf, gen_and_ip, a->gen_id)) != 0)
        goto TLS_FREE;

      NSTL1_OUT(NULL, NULL, "(Master <- Generator:%s) Health check successful", gen_and_ip);
      break;

      case FTP_DATA_FILE_RTC:

      //FTP scripts file to respective IP address
      //If Test run number given   
      //Here WORK will be Controller's work
      //Verify whether tar file exists or not
      sprintf(sys_cmd, "%s/logs/TR%d/.controller/nc_%s_rtc.tar.lz4", g_ns_wdir, testidx, a->gen_name);
      if (stat(sys_cmd, &buf))
      {
        NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s.", sys_cmd);
        ret = 0;
        goto TLS_FREE;
      }
      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(file_name, "%s/logs/TR%d/.controller/nc_%s_rtc.tar.lz4", g_ns_wdir, testidx, a->gen_name);
      else
        sprintf(file_name, "%s/.tmp/.controller/nc_%s_rtc.tar.lz4", g_ns_wdir, a->gen_name);

      sprintf(path, "%s/logs/TR%d/runtime_changes", a->work, a->testidx);

      NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
      NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

      ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
      if(ret != 0)
      {
        NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                          gen_and_ip, server_ptr.cmd_output);
        usleep(RETRY_DELAY);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_%s_rtc.tar.lz4' to %s port:%s, error:%s",
                                a->gen_name, gen_and_ip, a->agentport, server_ptr.cmd_output);
          goto TLS_FREE;
        }
      }
      NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s", file_name, gen_and_ip);
      break;

      case EXTRACT_DATA_FILE_RTC:

      //Verify whether tar file exists or not
      sprintf(sys_cmd, "%s/logs/TR%d/.controller/nc_%s_rtc.tar.lz4", g_ns_wdir, testidx, a->gen_name);
      if (stat(sys_cmd, &buf))
      {
        NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s", sys_cmd);
        snprintf(gen_ptr->gen_keyword, 24000, "Unable to get status of file %s", sys_cmd);
        ret = 0;
        goto TLS_FREE;
      }
      /*Cmd to untar the file*/
      sprintf(cmd_args, "tar -I%s/thirdparty/bin/lz4 -xf %s/logs/TR%d/runtime_changes/nc_%s_rtc.tar.lz4 -C %s/logs/TR%d/runtime_changes",
                        a->work, a->work, a->testidx, a->gen_name, a->work, a->testidx);

      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);
      NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      MAKE_COMMAND_FOR_CMON

      ret = nslb_run_users_command(&server_ptr, cmd_args);
      if(ret)
      {
        NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_%s_rtc.tar.lz4' on %s, error %s",
                              a->gen_name, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_%s_rtc.tar.lz4' on %s, error %s",
                                              a->gen_name, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        ret = -1;
        goto TLS_FREE;
      }
      NSTL1(NULL, NULL, "Command run successfully '%s' to %s", cmd_args, gen_and_ip);
      break;

    case RUNNING_SCENARIO_ON_GENERATORS:

      //Next run scenario on respective generators : nsu_start_test -n scenario_name -m master_ip:port
      //Friday, September 28 2012 - Added generator id in -m option. Now format is:r
      //Controller_ip:port:genrator_id:event_logger_port(optional)
      //Passing the -d option to nii_start_test as we need this controllers directory for shipping the generators TR data to the controller.
      //Added option -u to pass controller's user and group name to all generators, which is require to change ownership of genrator TR's at controller 
      //Added option -t to pass controller's testrun number to all generators, which is used to print controller details in global.dat file of each generator 
      /* Friday, 23 May 2014 - Added g_partition_idx in -m option. Now format is : 
       *                               Controller_ip:port:genrator_id:event_logger_port(optional):partition_idx
       * partition_idx will be '0' in non partition mode & 'yyyymmddhhmmss' in partition mode 
         
         Thursday, 28 Dec 2017 - Added config and Subconfig to -m option : Bug 39240 - when subconfig is NVSM then rpf.csv is mandatory
                                   in generators, config and subconfig is set only in controller hence passing as argument to generator.
       */

      if((g_set_args_type == KW_ARGS) || (g_set_args_type == KW_FILE))
        sprintf(addKW, "-C KW_FILE=additional_kw.conf");

      sprintf(cmd_args, "%s/bin/nii_start_test -n %s/%s_%s -m %s:%hu:%d:%hu:%lld:%d:%s:%s:%hu -w %s -d %s"
                        " -u cavisson:cavisson -t %d -e %lu -g %d %s", a->work, netomni_proj_subproj_file, a->gen_name,
                         netomni_scenario_file, global_settings->ctrl_server_ip, parent_port_number,
                         a->gen_id, event_logger_port_number, g_partition_idx, g_tomcat_port,
                         g_cavinfo.SUB_CONFIG, a->gen_name, g_dh_listen_port, a->work, g_ns_wdir,
                         testidx, global_settings->unix_cav_epoch_diff,
                         global_settings->disable_use_of_gen_spec_kwd_file, addKW);

      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);
      NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      write_log_file(NS_UPLOAD_GEN_DATA, "Starting test on %s", gen_and_ip);
      MAKE_COMMAND_FOR_CMON

      time(&start_time);
      ret = nslb_run_users_command(&server_ptr, cmd_args);
      if(ret != 0)
      {
        NSTL1_OUT(NULL, NULL, "\nError in running cmd = %s on %s", cmd_args, gen_and_ip);
      }

      ret = chk_command_op (&server_ptr, a->gen_id);

      if(ret != 0)
      {
        /* On the failure we are copying the generators netstorm log (webapps/netstorm/logs/$USER_NAME.netstorm.log) to the controllers log
         * directory. In case TestRunOutput file is created in generator TR then one need to check issue on generator itself because netstorm log
         * file gets removed. 
         */
        FILE *fp = NULL;
        sprintf(sys_cmd, "%s/logs/TR%d/%s_cavisson.netstorm.log", g_ns_wdir, testidx, a->gen_name);
        fp = fopen(sys_cmd, "w");

        if(server_ptr.cmd_output && (server_ptr.cmd_output[0] != '\0')) {
          if(fp != NULL)
          {
            fprintf(fp, "%s", server_ptr.cmd_output);
            fclose(fp); 
          } 
        }
        else
        {
          sprintf(cmd_args, "cat %s/webapps/netstorm/logs/cavisson.netstorm.log", a->work);

          NSTL1_OUT(NULL, NULL, "Running cmd %s >%s/logs/TR%d/%s_cavisson.netstorm.log", cmd_args, g_ns_wdir,
                                 testidx, a->gen_name);

          usleep(RETRY_DELAY * 50); //Waiting 5 seconds as test got killed
          MAKE_COMMAND_FOR_CMON
          ret_val = nslb_run_users_command(&server_ptr, cmd_args);

          if(ret_val == 0)
          {
            if(fp != NULL)
            {
              print2f_always(fp, "%s", server_ptr.cmd_output);
              fclose(fp);
            }
          }
        }
        NSTL1_OUT(NULL, NULL, "\nError in starting test %d at %s, port:%s", gen_ptr->testidx,
                                gen_and_ip, a->agentport);
        goto TLS_FREE;

      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Started test on %s took %llu secs", gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Command run successfully '%s' to %s took %llu secs", cmd_args, gen_and_ip, time(NULL) - start_time);
      break;

    case UPLOAD_ALL_GEN_DATA:
    //case CHECK_JMETER_ENABLED:

      if(g_script_or_url == 100)
      {
        sprintf(cmd_args, "cat %s/.jmeterFile", a->work);
        MAKE_COMMAND_FOR_CMON
        ret = nslb_run_users_command(&server_ptr, cmd_args);
        if(ret != 0)
        {
          snprintf(gen_ptr->gen_keyword, 24000, "\nEither cmon not running or command failed '%s' on %s",
                  cmd_args, gen_and_ip);
          NSTL1_OUT(NULL, NULL, "\nEither cmon not running or command failed '%s' on %s", cmd_args, gen_and_ip);
          goto TLS_FREE;
        }

        if(server_ptr.cmd_output && server_ptr.cmd_output[0] == '0')
        {
          NSTL1_OUT(NULL, NULL, "Error: Stopping test as JMETER_HOME is not set on %s", gen_and_ip);
          snprintf(gen_ptr->gen_keyword, 24000, "Stopping test as JMETER_HOME is not set on %s", gen_and_ip);
          ret = -1;
          goto TLS_FREE;
        }
      }

      //ship nc_common.tar.lz4
      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(sys_cmd, "%s/logs/TR%d/.controller/nc_common_%s.tar.lz4", g_ns_wdir, testidx, ip_and_controller_tr);
      else
        // In case of script mode 1, URL based script, tar file wont be present
        sprintf(sys_cmd, "%s/.tmp/.controller/.controller/nc_common_%s.tar.lz4", g_ns_wdir, ip_and_controller_tr);
       if (stat(sys_cmd, &buf))
       {
          NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s.", sys_cmd);
          snprintf(gen_ptr->gen_keyword, 24000, "Unable to get status of file %s", sys_cmd);
          ret = 0;
          goto TLS_FREE;
       }

      strncpy(file_name,sys_cmd,strlen(sys_cmd));
      sprintf(path, "%s", a->work);
      NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
      NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

      time(&start_time);
      size = (double)buf.st_size/(INIT_BIGBUFFER);
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploading scripts of size (%.3fM) to %s", size, gen_and_ip);
      ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
      if(ret != 0)
      {
        NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                           gen_and_ip, server_ptr.cmd_output);
        usleep(RETRY_DELAY);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_common_%s.tar.lz4' to %s port:%s, error:%s",
                               ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          snprintf(gen_ptr->gen_keyword, 24000, "Error in transferring data 'nc_common_%s.tar.lz4' to %s, "
                  "port:%s, error:%s", ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          goto TLS_FREE;
        }
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploaded scripts of size (%.3fM) to %s took %llu secs",
                                         size, gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s took %llu secs", file_name, gen_and_ip,
                        time(NULL) - start_time);

      //Extract nc_common.tar.lz4
      sprintf(cmd_args, "%s/bin/nii_extract_generator_data -c nc_common_%s.tar.lz4 env_var:NS_WDIR=%s", a->work, ip_and_controller_tr, a->work);

      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s\n", cmd_args, gen_and_ip);
      NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      write_log_file(NS_UPLOAD_GEN_DATA, "Extracting file-parameter data archive (for unique and use-once) on %s", gen_and_ip);
      MAKE_COMMAND_FOR_CMON

      time(&start_time);
      ret = nslb_run_users_command(&server_ptr, cmd_args);
      NSTL1(NULL, NULL, "Running cmd %s output %s", cmd_args, server_ptr.cmd_output);
      if(ret || (server_ptr.cmd_output && !strncmp(server_ptr.cmd_output, "FAILURE", 7)))
      {
        NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_common_%s.tar.lz4' on %s, error %s",
                              ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_common_%s.tar.lz4' on %s, error %s",
                              ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        ret = -1;
        goto TLS_FREE;
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Extracted file-parameter data archive (for unique and use-once) on %s took %llu secs",
                                      gen_and_ip, time(NULL) - start_time);
      // case FTP_SCRIPTS:

      //FTP scripts file to respective IP address
      //If Test run number given   
      //Here WORK will be Controller's work
      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(sys_cmd, "%s/logs/TR%d/.controller/nc_common_rel_%s.tar.lz4", g_ns_wdir, testidx, ip_and_controller_tr);
      else
        // In case of script mode 1, URL based script, tar file wont be present
        sprintf(sys_cmd, "%s/.tmp/.controller/.controller/nc_common_rel_%s.tar.lz4", g_ns_wdir, ip_and_controller_tr);
       if (stat(sys_cmd, &buf))
       {
          NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s.", sys_cmd);
          snprintf(gen_ptr->gen_keyword, 24000, "Unable to get status of file %s", sys_cmd);
          ret = 0;
          goto TLS_FREE;
       }

      strncpy(file_name,sys_cmd,strlen(sys_cmd));
      sprintf(path, "%s", a->work);
      NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
      NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

      time(&start_time);
      size = (double)buf.st_size/(INIT_BIGBUFFER);
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploading scripts of size (%.3fM) to %s", size, gen_and_ip);
      ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
      if(ret != 0)
      {
        NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                           gen_and_ip, server_ptr.cmd_output);
        usleep(RETRY_DELAY);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_common_rel_%s.tar.lz4' to %s port:%s, error:%s",
                               ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          snprintf(gen_ptr->gen_keyword, 24000, "Error in transferring data 'nc_common_rel_%s.tar.lz4' to %s, "
                  "port:%s, error:%s", ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          goto TLS_FREE;
        }
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploaded scripts of size (%.3fM) to %s took %llu secs",
                                         size, gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s took %llu secs", file_name, gen_and_ip,
                        time(NULL) - start_time);

      //case EXTRACT_SCRIPTS:

      /* Cmd to untar the scripts */
      sprintf(cmd_args, "%s/bin/nii_extract_generator_data -s nc_common_rel_%s.tar.lz4 env_var:NS_WDIR=%s", a->work, ip_and_controller_tr, a->work);

      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);
      NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      write_log_file(NS_UPLOAD_GEN_DATA, "Extracting scripts archive on %s", gen_and_ip);
      MAKE_COMMAND_FOR_CMON

      time(&start_time);
      ret = nslb_run_users_command(&server_ptr, cmd_args);
      NSTL1(NULL, NULL, "Running cmd %s output %s", cmd_args, server_ptr.cmd_output);
      if(ret || (server_ptr.cmd_output && !strncmp(server_ptr.cmd_output, "FAILURE", 7)))
      {
        NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_common_rel_%s.tar.lz4' on %s, error %s",
                            ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_common_rel_%s.tar.lz4' on %s, error %s", 
                                      ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        ret = -1;
        goto TLS_FREE;
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Extracted scripts archive on %s took %llu secs", gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Command run successfully '%s' to %s took %llu secs", cmd_args, gen_and_ip, time(NULL) - start_time);

   //case FTP_SCENARIO:

      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(file_name, "%s/logs/TR%d/.controller/nc_%s_rel_%s.tar.lz4", g_ns_wdir, testidx, a->gen_name, ip_and_controller_tr);
      else
        sprintf(file_name, "%s/.tmp/.controller/nc_%s_rel_%s.tar.lz4", g_ns_wdir, a->gen_name, ip_and_controller_tr);

      sprintf(path, "%s", a->work);
      if (stat(file_name, &buf))
      {
        NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s.", file_name);
        ret = 0;
        goto TLS_FREE;
      }
      NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
      NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

      time(&start_time);
      size = (double)buf.st_size/(INIT_BIGBUFFER);
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploading scenario file (also USE-ONCE and UNIQUE data) of size (%.3fM) to %s",
                                         size, gen_and_ip);
      ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
      if(ret != 0)
      {
        NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                          gen_and_ip, server_ptr.cmd_output);
        usleep(RETRY_DELAY);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_%s_rel_%s.tar.lz4' to %s, port:%s, error:%s",
                                 a->gen_name , ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          snprintf(gen_ptr->gen_keyword, 24000, "Error in transferring data 'nc_%s_rel_%s.tar.lz4' to %s,"
                  " port:%s, error:%s", a->gen_name, ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
          goto TLS_FREE;
        }
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Uploaded scenario file (also USE-ONCE and UNIQUE data) of "
                                         "size (%.3fM) to %s took %llu secs", size, gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s took %llu secs", file_name,
                         gen_and_ip, time(NULL) - start_time);

   //case EXTRACT_SCENARIO:

      /*Cmd to untar the file*/
      sprintf(cmd_args, "%s/bin/nii_extract_generator_data -f nc_%s_rel_%s.tar.lz4 env_var:NS_WDIR=%s", 
                         a->work, a->gen_name, ip_and_controller_tr, a->work);

      NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);
      NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

      write_log_file(NS_UPLOAD_GEN_DATA, "Extracting scenario archive on %s", gen_and_ip);
      MAKE_COMMAND_FOR_CMON
      time(&start_time);
      ret = nslb_run_users_command(&server_ptr, cmd_args);
      NSTL1(NULL, NULL, "Running cmd %s output %s", cmd_args, server_ptr.cmd_output);
      if(ret || (server_ptr.cmd_output && !strncmp(server_ptr.cmd_output, "FAILURE", 7)))
      {
        NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_%s_rel_%s.tar.lz4' on %s, error %s",
                              a->gen_name, ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_%s_rel_%s.tar.lz4' on %s, error %s",
                              a->gen_name, ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
        ret = -1;
        goto TLS_FREE;
      }
      write_log_file(NS_UPLOAD_GEN_DATA, "Extracted scenario archive on %s took %llu secs", gen_and_ip, time(NULL) - start_time);
      NSTL1(NULL, NULL, "Command run successfully '%s' to %s took %llu secs", cmd_args, gen_and_ip, time(NULL) - start_time);

    //case FTP_ABS_DATA_FILE:

      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(file_name, "%s/logs/TR%d/.controller/nc_common_abs_%s.tar.lz4", g_ns_wdir, testidx, ip_and_controller_tr);
      else
        sprintf(file_name, "%s/.tmp/.controller/nc_common_abs_%s.tar.lz4", g_ns_wdir, ip_and_controller_tr);

      if (!stat(file_name, &buf))
      {
        NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
        NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

        time(&start_time);
        sprintf(path, "%s", a->work);
        size = (double)buf.st_size/(INIT_BIGBUFFER);
        write_log_file(NS_UPLOAD_GEN_DATA, "Uploading file parameter data (absolute) of size (%.3fM) to %s", size, gen_and_ip);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                            gen_and_ip, server_ptr.cmd_output);
          usleep(RETRY_DELAY);
          ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
          if(ret != 0)
          {
            NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_common_abs_%s.tar.lz4' to %s, port:%s, error:%s", 
                                   ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
            snprintf(gen_ptr->gen_keyword, 24000, "Error in transferring data 'nc_common_abs_%s.tar.lz4' to %s, port:%s, error:%s",
                                                   ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
            goto TLS_FREE;
          }
        }
        write_log_file(NS_UPLOAD_GEN_DATA, "Uploaded file parameter data (absolute) of size (%.3fM) to %s took %llu secs",
                                            size, gen_and_ip, time(NULL) - start_time);
        NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s took %llu secs", file_name, gen_and_ip, time(NULL) - start_time);

    //case EXTRACT_ABS_DATA_FILE:

        /*Cmd to untar the file*/
        sprintf(cmd_args, "%s/bin/nii_extract_generator_data -F nc_common_abs_%s.tar.lz4 env_var:NS_WDIR=%s",
                           a->work, ip_and_controller_tr, a->work);

        NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);
        NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

        write_log_file(NS_UPLOAD_GEN_DATA, "Extracting file-parameter data archive (absolute) on %s", gen_and_ip);
        MAKE_COMMAND_FOR_CMON

        time(&start_time);
        ret = nslb_run_users_command(&server_ptr, cmd_args);
        NSTL1(NULL, NULL, "Running cmd %s output %s", cmd_args, server_ptr.cmd_output);
        if(ret || (server_ptr.cmd_output && !strncmp(server_ptr.cmd_output, "FAILURE", 7)))
        {
          NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_common_abs_%s.tar.lz4' on %s, error %s",
                                 ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
          snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_common_abs_%s.tar.lz4' on %s, error %s",
                                 ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
          ret = -1;
          goto TLS_FREE;
        }
        write_log_file(NS_UPLOAD_GEN_DATA, "Extracted file-parameter data archive (absolute) on %s took %llu secs",
                                           gen_and_ip, time(NULL) - start_time);
        NSTL1(NULL, NULL, "Command run successfully '%s' to %s took %llu secs", cmd_args, gen_and_ip, time(NULL) - start_time);
      }

    //case FTP_DIV_DATA_FILE:

      if ((testidx) && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) //If test run not generated then tar should be fetch from .tmp folder
        sprintf(file_name, "%s/logs/TR%d/.controller/nc_%s_abs_%s.tar.lz4",
                            g_ns_wdir, testidx, a->gen_name, ip_and_controller_tr);
      else
        sprintf(file_name, "%s/.tmp/.controller/nc_%s_abs_%s.tar.lz4", g_ns_wdir, a->gen_name, ip_and_controller_tr);
      if(stat(file_name,&buf))
      {
         NSDL3_MESSAGES(NULL, NULL, "Unable to get status of file %s", file_name);
         NSTL1(NULL, NULL, "Unable to get status of file %s for %s", file_name, gen_and_ip);
         snprintf(gen_ptr->gen_keyword, 24000, "Unable to get status of file %s", file_name);
         ret = 0;
         goto TLS_FREE;
      }
      else
      {
        sprintf(path, "/home/cavisson/");

        NSDL3_MESSAGES(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);
        NSTL1(NULL, NULL, "Uploading filename = %s, path = %s on %s", file_name, path, gen_and_ip);

        time(&start_time);
        size = (double)buf.st_size/(INIT_BIGBUFFER);
        write_log_file(NS_UPLOAD_GEN_DATA, "Uploading file parameter data (for unique and use-once)"
                                           " of size (%.3fM) to %s", size, gen_and_ip);
        ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
        if(ret != 0)
        {
          NSTL1(NULL, NULL, "Retrying upload filename = %s, path = %s on %s, error:%s", file_name, path,
                            gen_and_ip, server_ptr.cmd_output);
          usleep(RETRY_DELAY);
          ret = nslb_ftp_file(&server_ptr, file_name, path, 0);
          if(ret != 0)
          {
            NSTL1_OUT(NULL, NULL, "Error in transferring data 'nc_%s_abs_%s.tar.lz4' to %s port:%s, error:%s",
                                     a->gen_name, ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
            snprintf(gen_ptr->gen_keyword, 24000, "Error in transferring data 'nc_%s_abs_%s.tar.lz4' to %s, port:%s, error:%s",
                                     a->gen_name, ip_and_controller_tr, gen_and_ip, a->agentport, server_ptr.cmd_output);
            goto TLS_FREE;
          }
        }
        write_log_file(NS_UPLOAD_GEN_DATA, "Uploaded file parameter data (for unique and use-once) of "
                                           "size (%.3fM) to %s took %llu secs", size, gen_and_ip, time(NULL) - start_time);
        NSTL1(NULL, NULL, "Uploaded successfully filename = '%s' to %s took %llu secs", file_name, gen_and_ip, time(NULL) - start_time);

    //case EXTRACT_DIV_DATA_FILE:


        /*Cmd to untar the file*/
        sprintf(cmd_args, "%s/bin/nii_extract_generator_data -d nc_%s_abs_%s.tar.lz4 env_var:NS_WDIR=%s",
                          a->work, a->gen_name, ip_and_controller_tr, a->work);

        NSDL3_MESSAGES(NULL, NULL, "Running cmd %s on %s\n", cmd_args, gen_and_ip);
        NSTL1(NULL, NULL, "Running cmd %s on %s", cmd_args, gen_and_ip);

        write_log_file(NS_UPLOAD_GEN_DATA, "Extracting file-parameter data archive (for unique and use-once) on %s", gen_and_ip);
        MAKE_COMMAND_FOR_CMON

        time(&start_time);
        ret = nslb_run_users_command(&server_ptr, cmd_args);
        NSTL1(NULL, NULL, "Running cmd %s output %s", cmd_args, server_ptr.cmd_output);
        if(ret || (server_ptr.cmd_output && !strncmp(server_ptr.cmd_output, "FAILURE", 7)))
        {
          NSTL1_OUT(NULL, NULL, "Error in extracting data 'nc_%s_abs_%s.tar.lz4' on %s, error %s",
                                a->gen_name, ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
          snprintf(gen_ptr->gen_keyword, 24000, "Error in extracting data 'nc_%s_abs_%s.tar.lz4' on %s, error %s",
                                a->gen_name, ip_and_controller_tr, gen_and_ip, (!ret)?server_ptr.cmd_output+8:server_ptr.cmd_output);
          ret = -1;
          goto TLS_FREE;
        }
        write_log_file(NS_UPLOAD_GEN_DATA, "Extracted file-parameter data archive (for unique and use-once) on %s took %llu secs",
                                           gen_and_ip, time(NULL) - start_time);
        NSTL1(NULL, NULL, "Command run successfully '%s' to %s took %llu secs", cmd_args, gen_and_ip, time(NULL) - start_time);
      }
      break;

    default:
      NSTL1_OUT(NULL, NULL, "Invalid mode of operation = %d for %s", a->mode, gen_and_ip);
      snprintf(gen_ptr->gen_keyword, 24000, "Invalid mode of operation = %d for %s", a->mode, gen_and_ip);
      ret = -1;
      goto TLS_FREE;
  }
   goto TLS_FREE;
  

TLS_FREE:

  if(ret)
    gen_ptr->flags |= IS_GEN_FAILED;

  nslb_clean_cmon_var(&server_ptr);

  pthread_mutex_lock(&gen_thread_cnt);
  DEC_THREAD_COUNT(a->gen_id);
  pthread_mutex_unlock(&gen_thread_cnt);
  #if NS_DEBUG_ON
  NSDL1_MESSAGES(NULL, NULL, "[Callback] FINISHED gen idx = %d, bitflag = %s, mode = %d",
                              a->gen_id, nslb_show_bitflag(gen_started_mask), a->mode);
  #endif
  TLS_FREE_AND_EXIT(&ret);
}

static void set_killed_generator(int killed_gen_idx, int mode)
{
  int i, idx, grp_idx = 0;

  NSDL4_MESSAGES(NULL, NULL, "Method called, killed_gen_idx = %d", killed_gen_idx);
  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.total_killed_gen++)
  if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      for (idx = 0; idx < scen_grp_entry[grp_idx].num_generator; idx++)
      {
        NSDL4_MESSAGES(NULL, NULL, "killed gen index = %d, scen_grp_entry[%d].generator_id_list[%d] = %d",
                       killed_gen_idx, grp_idx, idx, scen_grp_entry[grp_idx].generator_id_list[idx]);
        if(scen_grp_entry[grp_idx].generator_id_list[idx] == killed_gen_idx)
        {
          if(mode == CHECK_GENERATOR_HEALTH)
            runProfTable[i].num_generator_kill_per_grp++;
          else
            runprof_table_shr_mem[i].num_generator_kill_per_grp++;
        }
        NSDL4_MESSAGES(NULL, NULL, "i = %d, mode = %d kill_gen_per_grp_shr = %d", i, mode,
        (mode == CHECK_GENERATOR_HEALTH)?runProfTable[i].num_generator_kill_per_grp:runprof_table_shr_mem[i].num_generator_kill_per_grp);
      }
      grp_idx += scen_grp_entry[grp_idx].num_generator;
    }
  }
  NSDL2_MESSAGES(NULL, NULL, "Method exitted, killed generator count = %d", g_data_control_var.total_killed_gen);
}

void mark_gen_timeout(int gen_idx, pthread_t thread_id)
{
  generator_entry[gen_idx].flags |= IS_GEN_KILLED;
  generator_entry[gen_idx].flags |= IS_GEN_INACTIVE;
  g_data_control_var.total_killed_gen++;
  DEC_THREAD_COUNT(gen_idx);
  pthread_cancel(thread_id);
}

int run_command_in_thread (int mode, int runtime)
{
  int i, ret/*, total_gen*/;
  u_ns_ts_t start_time;
  static int first_time = 1;
  static pthread_t *thread = NULL;
  static gen_attr *threadargs = NULL;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  ContinueTestOnGenFailure *gen_settings = &global_settings->con_test_gen_fail_setting;
  int timeout_val;

  /*Bug 69202: Malloc and filling gen info should be only once and should be reused
               As only mode changes, hence update mode and create thread
               Now, this should happpen only once out of 22 times mode ops
  */
  if(first_time || runtime) //Create memory only once
  {
    MY_MALLOC (thread, sgrp_used_genrator_entries * sizeof(pthread_t), "thread pthread_t", -1);
    MY_MALLOC (threadargs, sgrp_used_genrator_entries * sizeof(gen_attr), "threadargs, gen_attr", -1);
    memset(threadargs, 0, sgrp_used_genrator_entries * sizeof(gen_attr));

    for(i = 0; i < sgrp_used_genrator_entries; i++)
    {
      strcpy(threadargs[i].gen_name, (char *)generator_entry[i].gen_name);
      strcpy(threadargs[i].IP, generator_entry[i].IP);
      strcpy(threadargs[i].agentport, generator_entry[i].agentport);
      threadargs[i].gen_id = i;
      strcpy(threadargs[i].work, generator_entry[i].work);
      threadargs[i].testidx = generator_entry[i].testidx;
      gen_updated_timeout[i] = 0;
    }
    if(mode != CHECK_GENERATOR_HEALTH)
      first_time = 0;
  }

  //memset(gen_started_mask, 0, sizeof(unsigned long) * 4);

  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    /*IS_GEN_RETURN will not be set on 100% generator testcase*/
    if(generator_entry[i].flags & (IS_GEN_KILLED|IS_GEN_INACTIVE))
      continue;

    threadargs[i].mode = mode;
    NSDL3_MESSAGES(NULL, NULL, "Thread_Idx = %d, Gen Name = %s, Gen IP = %s, Agent Port = %s, Mode = %d,"
                               " Gen Work = %s, Gen id = %d, testidx = %d, &threadargs = %p", i, threadargs[i].gen_name,
                               threadargs[i].IP, threadargs[i].agentport, threadargs[i].mode, 
                               threadargs[i].work, threadargs[i].gen_id, threadargs[i].testidx, &threadargs[i]);

    generator_entry[i].flags &= ~IS_GEN_FAILED;
    INC_THREAD_COUNT(i);
    ret = pthread_create(&thread[i], &attr, start_gen_processing, (void *)&threadargs[i]);
    if (ret)
    {
      NSDL1_MESSAGES(NULL, NULL, "ERROR: Return value from pthread_create() is %d\n", ret);
      if(!runtime)
        NS_EXIT(-1, CAV_ERR_1014014); 
    }
  }

  pthread_attr_destroy(&attr);
  start_time = get_ms_stamp();  //Generators Timeout: start time 
  NSDL2_MESSAGES(NULL, NULL, "Timeout = %d, Start_time = %u", gen_settings->start_timeout, start_time);

  if (mode == CHECK_GENERATOR_HEALTH)
     timeout_val = CHECK_GENERATOR_HEALTH_TIMEOUT;
  else
     timeout_val = gen_settings->start_timeout;

  NSDL4_MESSAGES(NULL, NULL,"Generator Timeout = %d", timeout_val);
  while(1)
  {
    if(CHECK_ALL_THREAD_DONE)
    {
      NSDL1_MESSAGES(NULL, NULL, "All thread returned from callback");
      NSTL1(NULL, NULL, "All threads returned from callback");
      break;
    }
    sleep(1);

    // In case of runtime there is no need to perform/check timeout, so continue....
    if(runtime)
       continue;

    for(i = 0; i < sgrp_used_genrator_entries; i++)
    {
      if(!(generator_entry[i].flags & IS_GEN_KILLED))
      {
        if(nslb_check_bit_set(gen_started_mask, i))
        {
          //now = get_ms_stamp(); //Generators Timeout: Current time
          gen_updated_timeout[i] = (get_ms_stamp() - start_time)/1000;
          NSDL4_MESSAGES(NULL, NULL,"Running generator timeout gen_updated_timeout[%d] = %d", i, gen_updated_timeout[i]);
          NSTL1(NULL, NULL,"Running generator timeout gen_updated_timeout[%d] = %d", i, gen_updated_timeout[i]);
        }
         
        if(gen_updated_timeout[i] >= timeout_val)
        {
          NSTL1(NULL, NULL, "Generator '%s' got timed out[%d]. Hence marking it killed",
                generator_entry[i].gen_name, timeout_val);
          NS_DUMP_WARNING("Generator '%s' got timed out[%d]. Hence marking it killed",
                         generator_entry[i].gen_name, timeout_val);
          mark_gen_timeout(i,thread[i]);
        }
      }
    }
  }

  ret = 0;
  for(i = 0; i < sgrp_used_genrator_entries ; i++)
  {
    NSDL4_MESSAGES(NULL, NULL, "id = %d, flags = %X", i, generator_entry[i].flags);
    if(!((generator_entry[i].flags & IS_GEN_KILLED) ^ IS_GEN_KILLED))
      continue;

    if(!((generator_entry[i].flags & IS_GEN_FAILED) ^ IS_GEN_FAILED))
    {
      if(runtime)
      {
        ret = -1; 
        break;     
      }
      generator_entry[i].flags |= IS_GEN_KILLED;
      generator_entry[i].flags |= IS_GEN_INACTIVE;

      set_killed_generator(i, mode);
      write_log_file((mode == CHECK_GENERATOR_HEALTH)?NS_GEN_VALIDATION:NS_UPLOAD_GEN_DATA, "%s",
                     generator_entry[i].gen_keyword);

      NSDL4_MESSAGES(NULL, NULL, "id = %d, flags = %X, Killed Gen = %d, Expected Gen = %d", i,
                           generator_entry[i].flags, g_data_control_var.total_killed_gen, num_gen_expected);
    }
  }

  if ((mode == RUNNING_SCENARIO_ON_GENERATORS) && (num_fail_generator == sgrp_used_genrator_entries))
  {
    NS_EXIT(-1, CAV_ERR_1014025);
  }
  if((sgrp_used_genrator_entries - g_data_control_var.total_killed_gen) < num_gen_expected)
  {
    NS_EXIT(-1, CAV_ERR_1014013, (sgrp_used_genrator_entries - g_data_control_var.total_killed_gen),
                 num_gen_expected);
  }
  if((mode == RUNNING_SCENARIO_ON_GENERATORS) || (mode == CHECK_GENERATOR_HEALTH) || runtime) //Free only once
  {
    FREE_AND_MAKE_NULL(thread, "Freeing Thread pointer", -1);
    FREE_AND_MAKE_NULL(threadargs, "Freeing Thread pointer callback arguments structure", -1);
  }
  return ret;
}

void ship_scen_data_to_all_generators()
{
  sighandler_t prev_handler;
  //char buff[2048];
  //struct stat s;
  int len;

  NSDL1_MESSAGES(NULL, NULL, "Method called");
  
  prev_handler = signal(SIGCHLD, SIG_IGN); 

  len = strlen(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);
  snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Transferring metadata files to generators)");
  update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);
  if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)
  {
    //Check for jmeter directory
    //if(g_script_or_url == 100)
    //   run_command_in_thread(CHECK_JMETER_ENABLED, 0);
    //TODO: copy only 4 tar i.e nc_common_rel.tar.lz4, nc_common_abs.tar.lz4, nc_<genName>_rel.tar.lz4, nc_<genName>_abs.tar.lz4
    snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Transferring scripts/scenarios/absolute/divided absolute data to generators)");
    update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);

    run_command_in_thread(UPLOAD_ALL_GEN_DATA , 0);
    
    
    /******** Comment Section Start */
    //FTP nc_common_rel.tar.lz4 contains norm_csv, user_monitor, scripts, site_keyword, cert file
   /* run_command_in_thread(FTP_SCRIPTS, 0);

    //Extract nc_common_rel.tar.lz4
    run_command_in_thread(EXTRACT_SCRIPTS, 0);

    snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Transferring scenario to generators)");
    update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);

    //FTP nc_<genName>_rel.tar.lz4, generator tar consist of scenario configuration file
    run_command_in_thread(FTP_SCENARIO, 0);

    //Extract nc_<genName>_rel.tar.lz4
    run_command_in_thread(EXTRACT_SCENARIO, 0);

    sprintf(buff, "%s/logs/TR%d/.controller/nc_common_abs.tar.lz4", g_ns_wdir, testidx);
    if(!stat(buff, &s))
    {
      snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Transferring absolute data to generators)");
      update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);
      //FTP nc_common_rel.tar.lz4 i.e. Additional files or directory to generator's
      run_command_in_thread(FTP_ABS_DATA_FILE, 0);

      //Extract nc_common_rel.tar.lz4
      run_command_in_thread(EXTRACT_ABS_DATA_FILE, 0);
    }
    else
    {
      NSTL1(NULL, NULL, "Unable to get status of file nc_common_abs.tar.lz4");
    }
 
     
    snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Transferring divided absolute data for unique and use-once to generators)");
    update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);

    //FTP nc_<genName>_abs.tar.lz4
    run_command_in_thread(FTP_DIV_DATA_FILE, 0);

    //Extract nc_<genName>_abs.tar.lz4
    run_command_in_thread(EXTRACT_DIV_DATA_FILE, 0); */

    /******* Comment section Ends ***********************/
  }
  else
  {
    NSTL1(NULL, NULL, "Disable shipping of scenario, scripts and data files on all generator");
  }
  snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (Starting test on generators)");
  update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);
  //Running Scenario on respective Generators
  run_command_in_thread(RUNNING_SCENARIO_ON_GENERATORS, 0);
  snprintf(testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc + len, 1024 - len, " (%d of %d Generators are active)",
            sgrp_used_genrator_entries - g_data_control_var.total_killed_gen, sgrp_used_genrator_entries);
  update_summary_desc(NS_UPLOAD_GEN_DATA, testInitStageTable[NS_UPLOAD_GEN_DATA].stageDesc);

  /*delete all tar.lz4 files inside TR/.controller*/
  if(memory_based_fs_mode)
    clean_up_tmpfs_file(!DLT_OLD_TESTRUN);

  (void) signal(SIGCHLD, prev_handler);
}

static void make_csv_tar()
{
  sighandler_t prev_handler;
  NSDL1_PARSING(NULL, NULL, "Method called");
  
  prev_handler = signal(SIGCHLD, my_handler_ignore); 
  
  char tar_path[2048];
  char cmd[2048];
  char err_msg[1024] = "\0";
  
  write_log_file(NS_UPLOAD_GEN_DATA, "Compressing scripts and metadata files");
  /*bug id: 101320: moving .meta_data_files to $NS_WDIR/.tmp as its not a test assets*/
  sprintf(tar_path, "%s/.tmp/.meta_data_files/", controller_dir); 
  NSDL1_PARSING(NULL, NULL, "tar_path  =  %s", tar_path);
  mkdir_ex(tar_path);
 
  //make cmd to copy csv files from common to webapps/scripts/.meta_data_files directory
  sprintf(cmd, "cp %s/logs/TR%d/common_files/reports/csv/*.csv %s && cd %s; tar hrf %s/nc_common.tar .tmp/.meta_data_files; cd - >/dev/null", g_ns_wdir, testidx, tar_path, controller_dir, controller_dir);
  NSDL1_PARSING(NULL, NULL, "Going to run command %s", cmd);
  nslb_system(cmd,1,err_msg);

  sprintf(cmd, "%s/thirdparty/bin/lz4 -fq %s/nc_common.tar > %s/nc_common_%s.tar.lz4", g_ns_wdir, controller_dir, 
                controller_dir, ip_and_controller_tr);
  if(nslb_system(cmd,1,err_msg))
  {
    NS_EXIT(-1, CAV_ERR_1000023, cmd, errno, nslb_strerror(errno));
  }
  
  sprintf(cmd, "%s/thirdparty/bin/lz4 -fq %s/nc_common_rel.tar > %s/nc_common_rel_%s.tar.lz4", g_ns_wdir, controller_dir, 
                controller_dir, ip_and_controller_tr);
  if(nslb_system(cmd,1,err_msg))
  {
    NS_EXIT(-1, CAV_ERR_1000023, cmd, errno, nslb_strerror(errno));
  }
  
  (void) signal(SIGCHLD, prev_handler);
}

int master_init(char *user_conf)
{
  if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)
    make_csv_tar();

  ship_scen_data_to_all_generators();

  return 0;
}

/**************************************** RTC FUNCTION FOR MESSAGE COMMUNICATION ***************************************************/
//This function is used by generators/parent to pause its NVM
//on receiving RTC_PAUSE message from controller. Whereas parent send pause to NVMs 
inline int process_pause_for_rtc(int opcode, int gen_rtc_idx, User_trace *msg)
{
  int i, j;
  parent_child send_msg;
  send_msg.opcode = rtcdata->opcode = opcode;
  g_rtc_msg_seq_num = gen_rtc_idx;
  send_msg.gen_rtc_idx = ++(rtcdata->msg_seq_num);
  send_msg.ns_version =
  (loader_opcode != CLIENT_LOADER)?(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG) == RUNTIME_QUANTITY_FLAG):(msg?msg->rtc_qty_flag:0);
  global_settings->rtc_pause_seq = send_msg.gen_rtc_idx; //Pause Seq Number

  NSDL1_MESSAGES(NULL, NULL, "Method called, received RTC_PAUSE message, g_rtc_msg_seq_num = %d, rtcdata->msg_seq_num = %d",
                              g_rtc_msg_seq_num , rtcdata->msg_seq_num);

  NSTL1(NULL, NULL, "%s RTC_PAUSE(138) to stop processing",
              (loader_opcode == MASTER_LOADER)?"(Master -> Generator)":"(Parent -> NVM)");

  //For Generator: Set timer for before entering epoll wait
  if(loader_opcode == CLIENT_LOADER)
  {
    rtcdata->epoll_start_time = get_ms_stamp();
    if(msg && msg->rtc_qty_flag)
    {
      SET_RTC_FLAG(RUNTIME_QUANTITY_FLAG);
      strcpy(rtcdata->msg_buff, msg->reply_msg);
    }
    phase_end_msg_flag = 0;
    //else scheduling rtc
  }

  for (i = 0; i < global_settings->num_process; i++) 
  {
    if ((loader_opcode == MASTER_LOADER)?(generator_entry[i].flags & IS_GEN_INACTIVE):(g_msg_com_con[i].fd == -1)) 
    {
      if (g_msg_com_con[i].ip) 
      {
        NSDL3_MESSAGES(NULL, NULL, "Connection with the child is already" 
                                   "closed so not sending the msg %s for control connection", 
                           msg_com_con_to_str(&g_msg_com_con[i])); 
      }
    } 
    else
    {
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to child id = %d, opcode = %d, %s for control connection", i, opcode, 
                       msg_com_con_to_str(&g_msg_com_con[i]));
      send_msg.msg_len = sizeof(send_msg) - sizeof(int);
      if((write_msg(&g_msg_com_con[i], (char *)&send_msg, sizeof(send_msg), 0, CONTROL_MODE)) != RUNTIME_SUCCESS) {
        //TODO: we should send resume to paused NVMs
        if(i)
        {
          send_msg.opcode = RTC_RESUME;
          for(j = i-1; j >= 0; j--)
          {
            if(loader_opcode == MASTER_LOADER) { 
              if((write_msg(&g_msg_com_con[j], (char *)&send_msg, sizeof(parent_child), 0, CONTROL_MODE)))
                NSTL1(NULL, NULL, "(Master -> Generator) Failed to send RTC_RESUME to paused generators");
            }
            else {
                global_settings->rtc_pause_seq = 0;
                NSTL1(NULL, NULL, "Sending sigrtmin to NVM=%d to resume processing", g_msg_com_con[j].nvm_index);
            }
            DEC_CHECK_RTC_RETURN(g_msg_com_con[j].nvm_index, 0)
          }
          rtcdata->cur_state = RTC_RESUME_STATE;
        }
        break;
      }
      else {
        if(!CHECK_RTC_FLAG(RUNTIME_FPARAM_FLAG))
          INC_RTC_MSG_COUNT(g_msg_com_con[i].nvm_index);
      }
    } 
  }
  //No message sent to child
  if (CHECK_ALL_RTC_MSG_DONE && (!CHECK_RTC_FLAG(RUNTIME_FPARAM_FLAG)))
  {
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("RTC cannot be applied as failed to pause all NVMs")
    SET_RTC_FLAG(RUNTIME_FAIL);
    return 0;
  }
  rtcdata->cur_state = RTC_PAUSE_STATE;
  return 0;
}

//This function is used by generators to resume its NVM
//on receiving RTC_RESUME message on failure of RTC on controller or other generator failed
inline void process_resume_from_rtc(int seq_num)
{
  int i;
  NSDL1_MESSAGES(NULL, NULL, "Method called, received RTC_RESUME message from Controller/Parent");

  NSTL1(NULL, NULL, "%s RTC_RESUME(139) to resume processing, seq_num = %d",
              (loader_opcode == MASTER_LOADER)?"(Master -> Generator)":"(Parent -> NVM)", seq_num);

  if(seq_num)  //In case of EPOLL timeout/error no need to update g_rtc_msg_seq_num
    g_rtc_msg_seq_num = seq_num;

  if(rtc_resume_from_pause() != 0)
    return;
  
  global_settings->rtc_pause_seq = 0;
  for (i = 0; i < global_settings->num_process; i++)
  {
    if (g_msg_com_con[i].fd != -1)
    {
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to child id = %d, %s for control connection", i, msg_com_con_to_str(&g_msg_com_con[i]));
      INC_RTC_MSG_COUNT(g_msg_com_con[i].nvm_index);
    }
  }
  check_and_send_next_phase_msg();
  rtcdata->cur_state = RTC_RESUME_STATE;
}

// This method is used to send runtime change message from generator
void send_rtc_msg_to_controller(int fd, int opcode, char *msg, int grp_idx)
{
  User_trace send_msg;

  NSDL1_MESSAGES(NULL, NULL, "Method called, sending message to controller. opcode = %d, grp_idx = %d, rtcdata->msg_seq_num = %d", 
                                             opcode, grp_idx, rtcdata->msg_seq_num);
  NSTL1(NULL, NULL, "(Generator -> Master), opcode = %d, msg = %s, rtcdata->msg_seq_num = %d", opcode, msg, rtcdata->msg_seq_num);
  memset(&send_msg, 0, sizeof(User_trace)); 
  send_msg.opcode = opcode;
  if(opcode == NC_RTC_FAILED_MESSAGE)
  {
    send_msg.reply_status = -1;
    send_msg.opcode = NC_RTC_APPLIED_MESSAGE;
  }
  send_msg.testidx = testidx;
  send_msg.child_id = g_generator_idx;
  send_msg.ns_version = grp_idx;
  send_msg.gen_rtc_idx = g_rtc_msg_seq_num;
  //NSDL1_MESSAGES(NULL, NULL, "send_msg.ns_version = %d, buff= %s", send_msg.ns_version, buff);
  NSDL1_MESSAGES(NULL, NULL, "send_msg.ns_version = %d", send_msg.ns_version);
  snprintf(send_msg.reply_msg, sizeof(send_msg.reply_msg), "%s", msg);
  //sprintf(send_msg.group_name, "%s", buff);
  send_msg.msg_len = sizeof(send_msg) - sizeof(int);
  //TODO AYUSH
  if((write_msg(g_master_msg_com_con, (char *)(&send_msg), sizeof(send_msg), 0, CONTROL_MODE)))
    NSTL1(NULL, NULL, "Failed to send NC_RTC_APPLIED_MESSAGE(141) to master");
}

void process_nc_get_schedule_detail (int opcode, int grp_idx, char *msg, int *len, char *buff_ptr)
{
  int available_qty_to_remove[MAX_NVM_NUM];  //Stores users/sessios currently existing per NVM - available to remove
  int proc_index; 
  int cur_users = 0;
  int non_ramped_up = 0;

  NSDL1_MESSAGES(NULL, NULL, "Method called, received message from controller for group = %d. opcode = %d, group_name = %s", 
                              grp_idx, opcode, buff_ptr);

  //Memset array
  memset(available_qty_to_remove, 0, sizeof(available_qty_to_remove));

  //Get available quantity to remove ramped_up users/sessions
  runtime_get_nvm_quantity(grp_idx, REMOVE_RAMPED_UP_VUSERS, available_qty_to_remove);

  //Add current users
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    cur_users += available_qty_to_remove[proc_index];  

  //Memset array
  memset(available_qty_to_remove, 0, sizeof(available_qty_to_remove));

  //Get available quantity to remove not_ramped_up users/sessions
  runtime_get_nvm_quantity(grp_idx, REMOVE_NOT_RAMPED_UP_VUSERS, available_qty_to_remove);

  //Add current users
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    non_ramped_up += available_qty_to_remove[proc_index];  

  NSTL1(NULL, NULL, "Creating schedule detail response for group = %s(%d), Ramped-up Users/Sess = %d," 
                    " Non-Ramped-up Users/Sess = %d", buff_ptr, grp_idx, cur_users, non_ramped_up);
  NSDL2_MESSAGES(NULL, NULL, "cur_users = %d non_ramped_up = %d.", cur_users, non_ramped_up);
  //TODO: Jagat Check for the size of msg length, It should not overflow
  *len += sprintf(msg + *len, "%s-CUR_USER_OR_SESS:%d;NON_RAMPED_USER:%d,", buff_ptr, cur_users, non_ramped_up); 
  NSDL2_MESSAGES(NULL, NULL, "len = %d, msg = %s", *len, msg);
}

//This function called by Generator
void process_rtc_qty_schedule_detail(int opcode, char *tool_msg)
{
  char msg[USER_TRACE_REPLY_SIZE + 1]; //Using USER_TRACE_REPLY_SIZE because it will use User_trace struct to send message
  char err_msg[USER_TRACE_REPLY_SIZE + 1]; 
  char *ptr  = NULL;
  int grp_idx, len = 0;
  //int opcode = *((int *)tool_msg + 1);
  //char *buff = tool_msg + 8; // skip -> 4 byte for tool_msg len and 4 byte for opcode
  char *buff_ptr = tool_msg;

  //Null terminate messgae at max len
  //buff[(*((int *)tool_msg)) - 4] = '\0';
  //char *buff_ptr = buff;

  NSDL1_MESSAGES(NULL, NULL, "Method Called, where opcode = %d, buff_ptr = %s", opcode, buff_ptr);
  NSTL1(NULL, NULL, "Processing RTC_PAUSE(138) and making schedule response, msg = %s", buff_ptr);

  //Gaurav: substring of buffer as buff_ptr
  while(*buff_ptr != '\0')
  {
    if((ptr = strchr(buff_ptr, ':')) != NULL)
    { 
      *ptr = '\0';
      ptr++;
    }
    if(ptr == NULL) 
      break;
   
    NSDL1_MESSAGES(NULL, NULL, "buff_ptr = %s", buff_ptr);
    grp_idx = find_grp_idx_for_rtc(buff_ptr, err_msg);
    if(grp_idx == NS_GRP_IS_INVALID)
    {
      NSDL1_MESSAGES(NULL, NULL, "grp_idx = %d not found, err_msg = %s", grp_idx, err_msg);
      continue;
    }
    NSDL1_MESSAGES(NULL, NULL, "grp_idx = %d", grp_idx);
    process_nc_get_schedule_detail(opcode, grp_idx, msg, &len, buff_ptr);
    buff_ptr = ptr;
  }
  NSDL1_MESSAGES(NULL, NULL, "msg = %s", msg); 
  send_rtc_msg_to_controller(master_fd, opcode, msg, grp_idx);
}

#define ERROR_MSG \
  print_core_events((char*)__FUNCTION__, __FILE__,  "Got wrong message(%s) from generator. It should not happen", msg->reply_msg); \
  RUNTIME_UPDATE_LOG(msg->reply_msg); \
  return; 

inline char *get_gen_id_name_ip(int child_id, char *buf)
{
  NSDL1_RUNTIME(NULL, NULL, "child_id = %d", child_id);
  if((loader_opcode == MASTER_LOADER) && child_id >= 0 && child_id < sgrp_used_genrator_entries)
    snprintf(buf, 256, "%d,%s,%s", child_id, generator_entry[child_id].gen_name, generator_entry[child_id].IP);
  else
    snprintf(buf, 256, "%d", child_id);
  NSDL1_RUNTIME(NULL, NULL, "buf = %s", buf);
  return buf;
}

void process_nc_schedule_detail_response_msg(int gen_fd, User_trace *msg)
{
  char *ptr, *grp_ptr;
  char *start_ptr, *buff;
  char *fields[10];
  char err_msg[1024];
  int id = msg->ns_version;
  int i, cur_vuser_sess, rem_not_ramped_usr;
  //static int rec_all_gen_msg;

  NSDL1_MESSAGES(NULL, NULL, "Method called, gen_fd = %d, grp_idx = %d, gen_id = %d", gen_fd, msg->ns_version, msg->child_id);

  NSTL1(NULL, NULL, "(Master <- Generator:%s), RTC_PAUSE_DONE(145) where fd = %d, msg = %s, group = %s",
                    get_gen_id_name_ip(msg->child_id, err_msg), gen_fd, msg->reply_msg, msg->group_name);

  //rec_all_gen_msg++;  //Received all messages from generator

  buff = msg->reply_msg;
  while (*buff != '\0') 
  {
    if((start_ptr = strchr(buff, ',')) != NULL) 
    {
      *start_ptr = '\0';
      start_ptr++; 
      if(start_ptr == NULL) {
        NSDL1_MESSAGES(NULL, NULL, "This is last comma not going to break");
        break;
      }
      get_tokens_ex2(buff, fields, ";", 10);
      ptr = strchr(fields[0], ':');
      if (ptr == NULL) {
        ERROR_MSG
      }
      ptr++;
      cur_vuser_sess = atoi(ptr);
  
      ptr = strchr(fields[1], ':');
      if (ptr == NULL)
      {
        ERROR_MSG
      } 
      ptr++;
      rem_not_ramped_usr = atoi(ptr);
  
      if((grp_ptr = strchr(buff, '-')) != NULL)
      {
        *grp_ptr = '\0';
      } else {
        NSTL1_OUT(NULL, NULL, "RTC: Cannot get group name with - in buff = %s", buff);
        return;
      }
      NSDL2_RUNTIME(NULL, NULL, "cur_vuser_sess = %d, rem_not_ramped_usr = %d, buff = %s", cur_vuser_sess, rem_not_ramped_usr, buff);
      /*Store generator values in scenario group table with respect to generator id*/
      id = find_grp_idx_for_rtc(buff, err_msg);
      if(id == NS_GRP_IS_INVALID)
      {
        NSTL1_OUT(NULL, NULL, "process_nc_schedule_detail_response_msg(): id = %d not found, err_msg = %s", id, err_msg);
        break;
      }
      NSDL2_RUNTIME(NULL, NULL, "grp_id = %d", id);

      runprof_table_shr_mem[id].quantity += cur_vuser_sess;
      for (i = 0; i < runprof_table_shr_mem[id].num_generator_per_grp; i++)
      {
        if (runprof_table_shr_mem[id].running_gen_value[i].id == msg->child_id) 
        {
          runprof_table_shr_mem[id].running_gen_value[i].cur_vuser_sess = cur_vuser_sess;
          runprof_table_shr_mem[id].running_gen_value[i].rem_not_ramped_usr = rem_not_ramped_usr;
          NSTL1(NULL, NULL, "For generator %s, Values stored at index %d, Group = %s, Ramped-up Users/Sess = %d, "
                            "Non-Ramped-up User/Sess = %d", 
                             get_gen_id_name_ip(runprof_table_shr_mem[id].running_gen_value[i].id, err_msg), i,
                             runprof_table_shr_mem[id].scen_group_name, cur_vuser_sess, rem_not_ramped_usr);
          break;
        }
      }
    } else {
      NSDL2_RUNTIME(NULL, NULL, "RTC: Delimeter ',' not found in buff = %s", buff);
      break;
    }
    buff = start_ptr;
  }

  //Check received from all the generators
  if(CHECK_ALL_RTC_MSG_DONE)
  {
    NSTL1(NULL, NULL, "Got all RTC_PAUSE_DONE(145) from all generators");
    if((parse_qty_buff_and_distribute_on_gen()))
    {
      global_settings->pause_done = 0;
      rtcdata->cur_state = RTC_RESUME_STATE;
      send_msg_to_all_clients(RTC_RESUME, 0);
      return;
    }
  }
}

/* This function is called by generators to apply RTC changes on receiving "NC_APPLY_RTC_MESSAGE" msg
 * Here the message string contains the keyword, here we will apply the keyword, validate and verify changes.
 * In case of success send NC_RTC_APPLIED_MESSAGE to controller otherwise in case of error send NC_RTC_APPLIED_MESSAGE 
 */
void process_nc_apply_rtc_message (int controller_fd, User_trace *vuser_trace_msg)
{
  char err_msg[4096]; //Currently we are using 4K buffer for reporting error message, here we have limitation 
                      // in User_trace reply message buffer is of 8K.
  char curr_time_buffer[100]; //To get current date and time
  int first_time = -1;//Require to update start index once while reading message
  int rtc_msg_len = 0;
  int rtc_rec_counter;
  if(vuser_trace_msg->gen_rtc_idx)
    g_rtc_msg_seq_num = vuser_trace_msg->gen_rtc_idx;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called, received NC_APPLY_RTC_MESSAGE message from controller");

  NSTL1(NULL, NULL, "(Generator <- Master) NC_APPLY_RTC_MESSAGE(140), msg = %s", vuser_trace_msg->reply_msg);
  
  err_msg[0] = '\0';
  //RTC Validation
  if (ns_parent_state == NS_PARENT_ST_INIT ) {
    sprintf(err_msg, "%s: Cannot apply runtime changes before start phase", 
               nslb_get_cur_date_time(curr_time_buffer, 0));
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA", 
                           "Error in applying runtime changes. Error = %s", err_msg);
    send_rtc_msg_to_controller(master_fd, NC_RTC_FAILED_MESSAGE, err_msg, -1);
    return;
  } else if(ns_parent_state == NS_PARENT_ST_TEST_OVER) {
    sprintf(err_msg, "%s: Cannot apply during post processing phase", 
              nslb_get_cur_date_time(curr_time_buffer, 0));
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA", 
                           "Error in applying runtime changes. Error = %s", err_msg);
    send_rtc_msg_to_controller(master_fd, NC_RTC_FAILED_MESSAGE, err_msg, -1);
    return;
  }
  //Parse message keyword
  NSTL1(NULL, NULL, "Going to distribute quantity on generator where rtc_qty_flag = %d, keyword = %s",
                    vuser_trace_msg->rtc_qty_flag, vuser_trace_msg->reply_msg);
  if(vuser_trace_msg->rtc_qty_flag == 1) {
    parse_qty_buff_for_generator(vuser_trace_msg->reply_msg, vuser_trace_msg->cmd_fd, vuser_trace_msg->rtc_qty_flag);
  }
  else
    read_runtime_keyword(vuser_trace_msg->reply_msg, err_msg, NULL, &first_time, vuser_trace_msg->cmd_fd, 0, 0, &rtc_rec_counter, &rtc_msg_len/*runtime id*/);
  /*In run time changes for page dump, if test is started with level 0 and 
   * afetr some time page dump is eanbled in online mode, then we need to show 
   * page dump link in reports. So need to updatea summary.top file
   * to show page_dump*/
  if ((get_max_tracing_level() > TRACE_DISABLE) && (get_max_trace_dest() > 0)) {
    NSDL2_MESSAGES(NULL, NULL, "Need to update summary.top");
    update_summary_top_field(4, "Available_New", 0);
  }

  /*PageDump: Need to divide number of sessions among NVMs*/
  divide_session_per_proc();
}

#if 0
#define SEND_MSG_TO_ACTIVE_GEN(index, msg) \
if ((g_msg_com_con[index].con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM) && generator_entry[index].flags & IS_GEN_ACTIVE) {\
  if (flag == 1) \
    strcpy(send_msg.reply_msg, generator_entry[index].gen_keyword); \
  else\
    strcpy(send_msg.reply_msg, msg); \
  NSDL3_MESSAGES(NULL, NULL, "Sending msg to generator id = %d, opcode = %d, %s", idx, opcode, msg_com_con_to_str(&g_msg_com_con[index])); \
  NSTL1(NULL, NULL, "Sending msg to generator id = %d, opcode = %d, send_msg.reply_msg = %s", index, opcode, send_msg.reply_msg); \
  send_msg.msg_len = sizeof(User_trace) - sizeof(int);\
  write_msg(&g_msg_com_con[index], (char *)&send_msg, sizeof(User_trace), 0, CONTROL_MODE); \
}
/* Function used to send RTC message to all generators  comment by Gaurav on 25/01/2017*/
static void send_rtc_msg_to_all_gen(int opcode, char *msg, int flag, int grp_idx, int runtime_id, int runtime_idx)
{
  int idx, gen_count;
  User_trace send_msg;

  NSDL1_MESSAGES(NULL, NULL, "Method called, opcode = %d, msg = %s, grp_idx = %d, runtime_id = %d", opcode, msg, grp_idx, runtime_id);

  //Send message to generators which belong to generator
  if (grp_idx == -1) //In case of ALL send message to all generators
    gen_count = sgrp_used_genrator_entries;
  else//Send message to generators 
    gen_count = runprof_table_shr_mem[grp_idx].num_generator_per_grp;

  //Create User_trace message
  memset(&send_msg, 0, sizeof(User_trace));
  send_msg.opcode = opcode;
  //Fill group index
  send_msg.grp_idx = grp_idx;
  send_msg.gen_rtc_idx = runtime_idx;
  //Fill runtime_id
  send_msg.cmd_fd = runtime_id;
  // Here we are looping for sgrp_used_genrator_entries, at time of accept_and_timeout generators were added 
  // before monitors and tools. Hence this should work.
  // Send keyword to all generators
  for(idx = 0; idx < gen_count; idx++)
  {
    if (grp_idx != -1)
    {
      int id = runprof_table_shr_mem[grp_idx].running_gen_value[idx].id;
      SEND_MSG_TO_ACTIVE_GEN(id, msg);
    } else {
      SEND_MSG_TO_ACTIVE_GEN(idx, msg);
    }
  }
}
#endif

/* Function used to send RTC message to all generators*/
void send_nc_apply_rtc_msg_to_all_gen(int opcode, char *msg, int flag, int runtime_id)
{
  int i; 
  User_trace send_msg;

  NSDL1_MESSAGES(NULL, NULL, "Method called, opcode = %d, msg = %s, runtime_id = %d, rtc_qty_flag = %X",
                             opcode, msg, runtime_id, CHECK_RTC_FLAG(RUNTIME_SET_ALL_FLAG));

  send_msg.opcode = opcode;
  send_msg.cmd_fd = runtime_id;
  send_msg.gen_rtc_idx = ++rtcdata->msg_seq_num;
  send_msg.rtc_qty_flag = (CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)?1:0);

  for(i = 0; i < sgrp_used_genrator_entries; i++) {
    /* How to handle partial write as this method is the last called ?? */
    if (generator_entry[i].flags & IS_GEN_INACTIVE) {
      if (g_msg_com_con[i].ip)
        NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));  
    } else {
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s", i, opcode, msg_com_con_to_str(&g_msg_com_con[i]));
 
      //Check if gen_keyword buff is not null. Then set send bit, copy data into send messgae and send
      if(generator_entry[i].gen_keyword[0] != '\0') 
      {
        if (flag == 1)
          strcpy(send_msg.reply_msg, generator_entry[i].gen_keyword); 
        else
          strcpy(send_msg.reply_msg, msg); 

        generator_entry[i].flags |= SCEN_DETAIL_MSG_SENT;
        NSDL3_MESSAGES(NULL, NULL, "Sending msg to generator id = %d, opcode = %d, send_msg.reply_msg = %s, send_msg.rtc_qty_flag = %d", 
                                    i, opcode, send_msg.reply_msg, send_msg.rtc_qty_flag); 
        NSTL1(NULL, NULL, "Sending msg to generator id = %d, opcode = %d, send_msg.reply_msg = %s", 
                           i, opcode, send_msg.reply_msg); 
        send_msg.msg_len = sizeof(User_trace) - sizeof(int);
        if((write_msg(&g_msg_com_con[i], (char *)&send_msg, sizeof(User_trace), 0, CONTROL_MODE)) == RUNTIME_SUCCESS){
          if(!g_start_sch_msg_seq_num)
            INC_RTC_MSG_COUNT(g_msg_com_con[i].nvm_index);
        }
      }
      /*For every generator, updating these two variables*/
      send_msg.reply_msg[0] = '\0'; 
    }
  }
  if(!g_start_sch_msg_seq_num && CHECK_RTC_FLAG(RUNTIME_SCHEDULE_FLAG))
    g_start_sch_msg_seq_num = send_msg.gen_rtc_idx;
  
  rtcdata->cur_state = RTC_START_STATE;
}

/* Function used to read RTC success message from generator
 * Append generator messages in runtime_changes.log file*/
int process_nc_rtc_applied_message(int gen_fd, User_trace *msg)
{
  char genInfo[257];

  NSDL1_MESSAGES(NULL, NULL, "Method called, gen_fd = %d, msg->gen_rtc_idx = %d, rtcdata->msg_seq_num = %d,"
                             " g_start_sch_msg_seq_num = %d", gen_fd, msg->gen_rtc_idx, rtcdata->msg_seq_num,
                             g_start_sch_msg_seq_num);
  NSTL1(NULL, NULL, "(Master <- Generator:%s) NC_RTC_APPLIED_MESSAGE(141) where fd = %d, msg = %s",
                    get_gen_id_name_ip(msg->child_id, genInfo), gen_fd, msg->reply_msg);

  if(((msg->gen_rtc_idx != rtcdata->msg_seq_num) &&
    ((msg->gen_rtc_idx < g_start_sch_msg_seq_num) || (msg->gen_rtc_idx > rtcdata->msg_seq_num))) || (rtcdata->cur_state == RESET_RTC_STATE))
  {
    NSDL2_MESSAGES(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d",
                               msg->gen_rtc_idx, rtcdata->msg_seq_num);
    NSTL1(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d, schedule start seq = %d",
                      msg->gen_rtc_idx, rtcdata->msg_seq_num, g_start_sch_msg_seq_num);
    return 0;
  }

  if(msg->gen_rtc_idx == rtcdata->msg_seq_num)
  { 
    DEC_CHECK_RTC_RETURN(msg->child_id, 0);
    g_start_sch_msg_seq_num = 0;
  }

  rtcdata->opcode = msg->opcode;
  RUNTIME_UPDATE_LOG(msg->reply_msg);

  if(msg->reply_status == -1)
    SET_RTC_FLAG(RUNTIME_FAIL);

  if(CHECK_ALL_RTC_MSG_DONE)
  {
    rtcdata->cur_state = RESET_RTC_STATE;
    if(CHECK_RTC_FLAG(RUNTIME_FAIL))
    {
      RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("Runtime changes not applied")
    }
    else
    {
      SET_RTC_FLAG(RUNTIME_PASS);
      RUNTIME_UPDATE_LOG("Runtime changes applied Successfully");
      NS_EL_2_ATTR(EID_RUNTIME_CHANGES_OK, -1,
                                  -1, EVENT_CORE, EVENT_INFORMATION,
                                  "NA",
                                  "NA",
                                  "Runtime changes applied Successfully");
      RUNTIME_UPDATION_CLOSE_FILES;
      NSTL1(NULL, NULL, "Runtime changes applied Successfully");
    }
    if(!CHECK_RTC_FLAG(RUNTIME_ALERT_FLAG)) 
      apply_resume_rtc(0);
    else
    {
      send_rtc_msg_to_invoker(rtcdata->invoker_mccptr, APPLY_ALERT_RTC, NULL, 0);
      RUNTIME_UPDATION_RESET_FLAGS
    }
  } 
  return 0;
}

/* Function used to read RTC fail message from generator
 * Append generator messages in runtime_changes.log file
 * Set flag to send RESUME_MSG to all generators once all events are read
 * */
void process_nc_rtc_failed_message(int gen_fd, User_trace *msg, int runtime_flag)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, gen_fd = %d, runtime_flag = %d", gen_fd, runtime_flag);
  NSTL1(NULL, NULL, "Received RTC_FAILED_MESSAGE from generator fd = %d, msg = %s, runtime state = %d", gen_fd,
                     msg->reply_msg, rtcdata->cur_state);
  if (loader_opcode == MASTER_LOADER)
  { 
    RUNTIME_UPDATE_LOG(msg->reply_msg);
    //rec_all_gen_msg++; //Received message from a generator
    //rtc_failed_msg_rev = 1; 
  }
/*
  else 
  { //In case generator fails it send message to controller
    if(rtcdata->cur_state != RESET_RTC_STATE)  // if runtime change is in reset state then do not call
    {
      //on_failure_resume_gen_update_rtc_log(msg->reply_msg); 
      //Reset runtime changes state
      rtcdata->cur_state = RESET_RTC_STATE;
      //there is no conf in case of generator
      //delete_runtime_changes_conf_file();
      rtcdata->progress_flag = -1; //In case of RTC failure for NS/NC we are setting this flag to RTC Failure(-1)
      global_settings->pause_done = 0;
    }
  }
*/
}

#if 0
//Funtion used to add generator fds in new epoll created by controller
static inline int  nc_add_select(char* data_ptr, int fd, int event)
{
  struct epoll_event pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called. Adding fd = %d for event = %x", fd, event);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;

  if (epoll_ctl(nc_epfd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    NSDL2_MESSAGES(NULL, NULL, "EPOLL ERROR occured in controller, nc_add_select() - EPOLL_CTL_ADD: err = %s", nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "\nEPOLL ERROR occured in controller, nc_add_select() - EPOLL_CTL_ADD: err = %s\n", nslb_strerror(errno));
    NSTL1(NULL, NULL, "EPOLL ERROR occured in controller, nc_add_select() - EPOLL_CTL_ADD: err = %s", nslb_strerror(errno));
    return 1;//error case
  }
  return 0;//success case
}

//Funtion used to remove generator fds in new epoll created by controller
static inline void nc_remove_select(int fd)
{
  struct epoll_event pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called. Removing fd = %d from select, my_port_index = %d", fd, my_port_index);
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  if (fd == -1) return;
  if (epoll_ctl(nc_epfd, EPOLL_CTL_DEL, fd, &pfd) == -1)
  {
    NSTL1_OUT(NULL, NULL, "nc_remove_select() - EPOLL_CTL_DEL: err = %s\n", nslb_strerror(errno));
    return;
  }
}

//Function used to create epoll between controller and generators for RTC, message communication
int nc_create_epoll_wait_for_controller()
{
  int idx, ret;
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  if ((nc_epfd = epoll_create(sgrp_used_genrator_entries)) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Unable to epoll_create. Hence returning...");
    NSTL1(NULL, NULL, "RTC: Unable to epoll_create. Hence returning...");
    return 1;
  }

  NSTL1(NULL, NULL, "RTC: Controller created epoll, num_generators = %d", sgrp_used_genrator_entries);

  for(idx = 0; idx < sgrp_used_genrator_entries; idx++)
  {
    NSDL1_MESSAGES(NULL, NULL, "Add g_msg_com_con[%d].con_type = %d", idx, g_msg_com_con[idx].con_type);
    //Add only active generator fd's in epoll
    if (g_msg_com_con[idx].con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM)
    {  
      NSDL1_MESSAGES(NULL, NULL, "Add g_msg_com_con[%d].fd = %d", idx, g_msg_com_con[idx].fd);
      if(g_msg_com_con[idx].fd < 0){
        NSTL1(NULL, NULL, "Add g_msg_com_con[%d].fd = %d", idx, g_msg_com_con[idx].fd);
        continue; 
      }
      if ((ret = nc_add_select((char *)&g_msg_com_con[idx], g_msg_com_con[idx].fd, EPOLLIN | EPOLLERR | EPOLLHUP)) == 1)
        return 1;//Error occurred    
    }
  }
  return 0;//Success case
}

//Function used to remove generator fd from epoll
void remove_epoll_from_controller()
{
  int idx;
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  for(idx = 0; idx < sgrp_used_genrator_entries; idx++)
  {
    NSDL1_MESSAGES(NULL, NULL, "Add g_msg_com_con[%d].con_type = %d", idx, g_msg_com_con[idx].con_type);
    //TODO: How to handle if generator is killed during RTC
    if (g_msg_com_con[idx].con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM && (generator_entry[idx].flags & IS_GEN_ACTIVE))
    {
      NSDL1_MESSAGES(NULL, NULL, "Add g_msg_com_con[%d].fd = %d", idx, g_msg_com_con[idx].fd);
      nc_remove_select(g_msg_com_con[idx].fd);
    }
  }
}
static inline void default_case_msg(Msg_com_con *mccptr)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "Recieved invalid message from generator. gen_id = %d, opcode = %d, from ip = %s",
          msg->top.internal.child_id, msg->top.internal.opcode, mccptr->ip);
}

static int process_killed_generator(/*int grp_idx,*/ int num_active_gen)
{
  int idx;
  int num_gen_participated = 0;
 /* if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    if (rec_all_gen_msg == (num_active_gen - total_killed_generator)) {
      NSDL3_MESSAGES(NULL, NULL, "rec_all_gen_msg = %d, num_active_gen = %d, total_killed_gen = %d", rec_all_gen_msg, num_active_gen, total_killed_generator);
      return 0;
    }
  }*/
  //check for send + active bit and get total generators participate in RTC message
  //if total rec msg are >= total send message then got messages from all generators
  for(idx = 0; idx < sgrp_used_genrator_entries; idx++)
  {
    if((generator_entry[idx].flags & IS_GEN_ACTIVE) && (generator_entry[idx].flags & SCEN_DETAIL_MSG_SENT))
    {
      num_gen_participated++;
      //if (rec_all_gen_msg == (num_active_gen - total_killed_generator)) {
      //  NSDL3_MESSAGES(NULL, NULL, "rec_all_gen_msg = %d, num_active_gen = %d, total_killed_gen = %d", rec_all_gen_msg, num_active_gen, total_killed_generator);
       // return 0;
      //}
    }
  }
 // NSDL3_MESSAGES(NULL, NULL, "num_gen_participated = %d, rec_all_gen_msg = %d", num_gen_participated, rec_all_gen_msg);
  //if(rec_all_gen_msg >= num_gen_participated)
  {
    num_gen_participated = 0;
    //rec_all_gen_msg = 0;
    return 0;
  }
  
  /*
  // Is used of Schedule by group 
  else
  {
    // When RUNTIME_CHANGE_QUANTITY_SETTINGS keyword parsed and goes to all generator
    if(grp_idx == -1)
    {
      if (rec_all_gen_msg == (num_active_gen - total_killed_generator))
      { 
        NSDL3_MESSAGES(NULL, NULL, "rec_all_gen_msg = %d, num_active_gen = %d, total_killed_generator = %d", rec_all_gen_msg, num_active_gen, total_killed_generator);
        return 0;
      }
    }
    // when QUANTITY keyword goes to all generator
    else {
      if(rec_all_gen_msg == (runprof_table_shr_mem[grp_idx].num_generator_per_grp - runprof_table_shr_mem[grp_idx].num_generator_kill_per_grp))      {
        NSDL3_MESSAGES(NULL, NULL, "rec_all_gen_msg = %d, runprof_table_shr_mem[grp_idx].num_generator_per_grp = %d, runprof_table_shr_mem[grp_idx].num_generator_kill_per_grp = %d", rec_all_gen_msg, runprof_table_shr_mem[grp_idx].num_generator_per_grp, runprof_table_shr_mem[grp_idx].num_generator_kill_per_grp);
        return 0;
      }
    }
  }*/
  return 1;
}

//Function used by controller, it enters epoll wait and expect messages from all generator
//On receiving messages from all generator we willl break the epoll_wait loop
int wait_for_all_generator()
{
  int cnt, i;
  int epoll_timeout;
  char epoll_timeout_cnt = 0;
  struct epoll_event *epev = NULL;
  //rec_all_gen_msg = 0;//Need to reset generator messages count
  NSDL1_MESSAGES(NULL, NULL, "Method called, parent_epoll_timeout = %d, parent_timeout_max_cnt = %d, dump_parent_on_timeout = %d",
					   global_settings->parent_epoll_timeout,
					   global_settings->parent_timeout_max_cnt,
					   global_settings->dump_parent_on_timeout);	

  //if (grp_idx == -1)
  //  num_active_gen = sgrp_used_genrator_entries;
  //else
  //  num_active_gen += runprof_table_shr_mem[grp_idx].num_generator_per_grp;

  //NSTL1(NULL, NULL, "RTC: Total number of active generators = %d for group idx = %d", num_active_gen, grp_idx);  
  NSTL1(NULL, NULL, "RTC: Total number of active generators = %d", g_data_control_var.num_active);  
  MY_MALLOC(epev, sizeof(struct epoll_event) * NS_MAX_EPOLL_FD, "epoll event", -1);

  if (epev == NULL)
  {
    NSTL1_OUT(NULL, NULL, "%s:%d Malloc failed.. exiting.", __FUNCTION__, __LINE__);
    //kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
    return 1;
  }

  kill_timeout.tv_sec  = global_settings->parent_epoll_timeout/1000; 
  kill_timeout.tv_usec = global_settings->parent_epoll_timeout%1000;
  timeout = kill_timeout;

  while(1)
  {
    epoll_timeout = kill_timeout.tv_sec * 1000 + kill_timeout.tv_usec / 1000;

    NSDL2_MESSAGES(NULL, NULL, "After calculate_parent_epoll_timeout: epoll_timeout = %d", epoll_timeout);

    memset(epev, 0, sizeof(struct epoll_event) * NS_MAX_EPOLL_FD);
    NSDL1_MESSAGES(NULL, NULL, "Timeout is sec=%lu usec=%lu at %lu", timeout.tv_sec, timeout.tv_usec, get_ms_stamp());

    cnt = epoll_wait(nc_epfd, epev, NS_MAX_EPOLL_FD, epoll_timeout);
    NSDL2_MESSAGES(NULL, NULL, "cnt = %d", cnt);

    u_ns_ts_t local_rtc_epoll_end_time = get_ms_stamp();
    //In case of epoll timeout we need to 
    if((local_rtc_epoll_end_time - local_rtc_epoll_start_time) >= epoll_timeout)
    {
      NSTL1(NULL, NULL, "RTC Epoll Timeout: start time %llu, end time %llu, epoll_timeout = %d", 
                   local_rtc_epoll_start_time, local_rtc_epoll_end_time, epoll_timeout);  
      //rtc_failed_msg_rev = 1;
      return 1;   
    } 

    if (cnt > 0)
    {
      /* Reset epoll timeout count to 0 as we has to track only continuesly timeout*/
      epoll_timeout_cnt = 0;
      for (i = 0; i < cnt; i++)
      {
        Msg_com_con *mccptr = NULL;

        void *event_ptr = NULL;

        event_ptr = epev[i].data.ptr;
        if (event_ptr)
          mccptr = (Msg_com_con *)epev[i].data.ptr;
        if (epev[i].events & EPOLLERR) {
          NSDL3_MESSAGES(NULL, NULL, "EPOLLERR occured on sock %s. error = %s", 
                    msg_com_con_to_str(mccptr), nslb_strerror(errno));
          close_msg_com_con_and_exit(mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
          continue;
        }
        if (epev[i].events & EPOLLHUP) {
          NSDL3_MESSAGES(NULL, NULL, "EPOLLHUP occured on sock %s. error = %s", 
                    msg_com_con_to_str(mccptr), nslb_strerror(errno));
          close_msg_com_con_and_exit(mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
          continue;
        }

        /* partial write 
        if (epev[i].events & EPOLLOUT){
          if (mccptr->state & NS_STATE_WRITING)
            write_msg(mccptr, NULL, 0, 0);
          else {
            NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we got EPOLLOUT event on fd = %d", g_msg_com_con[i].fd);
          }
        }*/

        msg = NULL;
        if (epev[i].events & EPOLLIN) 
          msg = (parent_msg *)read_msg(mccptr, &rcv_amt, CONTROL_MODE);

        if (msg == NULL) continue;

        NSDL3_MESSAGES(NULL, NULL, "msg->top.internal.opcode = %d", msg->top.internal.opcode);  

        switch (msg->top.internal.opcode)
        {
          case NC_RTC_APPLIED_MESSAGE: 
            //From Generators. Controller appends success message in RTC log files 
            process_nc_rtc_applied_message(mccptr->fd, (User_trace *)&(msg->top.internal));
            break;

          case NC_RTC_FAILED_MESSAGE: 
            //From Generators. Controller appends failure message in RTC log files 
            process_nc_rtc_failed_message(mccptr->fd, (User_trace *)&(msg->top.internal), 1);
            break;

          case NC_SCHEDULE_DETAIL_RESPONSE: 
            //From Generators. Controller appends failure message in RTC log files 
            process_nc_schedule_detail_response_msg(mccptr->fd, (User_trace *)&(msg->top.internal));
            break;

          default:
            default_case_msg(mccptr);
            break;

        } // End of switch
      }  // End of for()
    } // End of if
    else if (cnt == 0)
    {
      epoll_timeout_cnt++;
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
				"Parent epoll_wait timeout.  cur timeout count  = %d max timeout count = %d",
				 epoll_timeout_cnt, global_settings->parent_timeout_max_cnt);

      /* Continue if timeout count is not continueusly reached to max_cnt*/
      if(epoll_timeout_cnt <= global_settings->parent_timeout_max_cnt) {
	 continue;
      } 
      else 
      {
      	NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                  __FILE__, (char*)__FUNCTION__,
                                 "Parent epoll_wait timeout cur count reached to its max limit = %d ",
			     global_settings->parent_timeout_max_cnt);	
        continue;
      }
    }
    else if (errno == EBADF)
      perror("Bad nc_epfd");
    else if (errno == EFAULT)
      perror("The memory area");
    else if (errno == EINVAL)
      perror("nc_epfd is not valid");
    else
    {
      if (errno != EINTR)
        perror("epoll_wait() failed");
      else
        NSDL3_MESSAGES(NULL, NULL, "epoll_wait() interrupted");
    }

    /* This function is added, when any generator is ignored in a running test and we want to apply RTC
       then in this case we want to try get message from all generators(including ignored generator). 
       Now, we want to wait for a message from only active generator in a running test.
       TODO: handling for individual generator for each group */

    if(rtcdata->quantity_flag && !process_killed_generator(g_data_control_var.num_active))
      break;
  }
  //In generator table 
  //1. reset send bit
  //2. reset send buffer 
  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    generator_entry[i].flags &= ~SCEN_DETAIL_MSG_SENT;
    memset(generator_entry[i].send_buff, 0, 4096);
  }
  return 0;
}

/* Function is used by controller to send runtime change keyword to generators
 * Here keyword will be send to all generator. 
 * Controller need to wait in epoll-wait for getting event from generators
 * If RTC applied successfully then send next keyword else send resume message to all
 * */
//jagat:TODO: How to send grp idx and runtime idx:
int send_rtc_settings_to_generator(char *keyword, int flag, int runtime_id)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, keyword = %s, flag = %d, runtime_id = %d", keyword, flag, runtime_id); 
  send_nc_apply_rtc_msg_to_all_gen(NC_APPLY_RTC_MESSAGE, keyword, flag, runtime_id);  
  //send_rtc_msg_to_all_gen(NC_APPLY_RTC_MESSAGE, keyword, flag, grp_idx, runtime_id, runtime_idx);  
  local_rtc_epoll_start_time = get_ms_stamp(); 
  NSTL1(NULL, NULL, "RTC: Epoll start time = %d", local_rtc_epoll_start_time);
  wait_for_all_generator();
  //On receiving all events and we have one/more failure messages then controller will send resume message to all generators
  return rtc_failed_msg_rev;
}
static char *nc_get_cur_time()
{
  time_t    tloc;
  struct  tm *lt, tm_struct;
  static  char cur_time[100];

  (void)time(&tloc);
  if((lt = nslb_localtime(&tloc, &tm_struct, 1)) == (struct tm *)NULL)
    strcpy(cur_time, "Error");
  else
    sprintf(cur_time, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_time);
}

//In order epoll_create fail on controller we need to send resume message to generators
void on_failure_resume_gen_update_rtc_log(char *err)
{
  char runtime_log[512];
  char err_msg[1024];
  FILE *fp;

  NSDL1_MESSAGES(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "RTC: Error in applying runtime changes. Hence sending resume message to all generators");

  sprintf(runtime_log, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, testidx);

  if ((fp = fopen(runtime_log, "a+")) == NULL) 
  {
    sprintf(err_msg, "Error in opening file %s. Error=%s", runtime_log, nslb_strerror(errno));
    NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA", 
                           "%s", err_msg); 
   
    NSTL1_OUT(NULL, NULL, "%s", err_msg);
    return;
  }

  fprintf(fp, "%s\n", err);

  NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA", 
                           "Error in applying runtime changes. Error = %s", err); 

  fprintf(fp, "%s: Runtime Updation Failed\n", nc_get_cur_time());  
  NSTL1_OUT(NULL, NULL, "%s, Runtime Updation Failed", err);

  fclose(fp);
  if(loader_opcode == MASTER_LOADER) { 
    send_msg_to_all_clients(RTC_RESUME, 0);
    User_trace msg;
    msg.opcode = QUANTITY_RESUME_RTC;
    process_rtc_quantity_resume_schedule(&msg);
    rtcdata->quantity_flag = 0;
  }
}
#endif

/* Function is used to create generator id list for each scenario group*/
void update_generator_list()
{
  int k, idx, grp_id = 0;
  NSDL1_MESSAGES(NULL, NULL, "Method called, total number of scenario groups = %d", total_runprof_entries);

  for(k = 0; k < total_runprof_entries; k++) 
  {
    NSDL2_MESSAGES(NULL, NULL, "For group index = %d, tool scenario group_id = %d, number of generator = %d", 
                     k, grp_id, scen_grp_entry[grp_id].num_generator);
    //In case of controller num_generator_per_group is not filled while copying into share memory
    runprof_table_shr_mem[k].num_generator_per_grp = scen_grp_entry[grp_id].num_generator;
    //runprof_table_shr_mem[k].num_generator_kill_per_grp = 0;//At initial time set variable to zero
    //Allocate memory for int array (generator_id_list)
    MY_MALLOC_AND_MEMSET(runprof_table_shr_mem[k].running_gen_value, (runprof_table_shr_mem[k].num_generator_per_grp * sizeof(RunningGenValue)), "Malloc running_gen_value", -1);

    NSDL2_MESSAGES(NULL, NULL, "List of generator ids, for group %s at index = %d having total %d generators", 
                     runprof_table_shr_mem[k].scen_group_name, k, runprof_table_shr_mem[k].num_generator_per_grp);
    //Fill generator id list for each group
    for (idx = 0; idx < runprof_table_shr_mem[k].num_generator_per_grp; idx++)
    {
      runprof_table_shr_mem[k].running_gen_value[idx].id = scen_grp_entry[grp_id].generator_id_list[idx];  
      NSDL4_MESSAGES(NULL, NULL, "runprof_table_shr_mem[%d].generator_id_list[%d] = %d", 
                       k, idx, runprof_table_shr_mem[k].running_gen_value[idx].id);
    }
    //In order to get list of ids from unique scenario group
    grp_id = grp_id + scen_grp_entry[grp_id].num_generator;
  }
} 
#if 0
static int nc_divide_usr_wrt_generator(int number_of_generators, double pct, int grp_idx, char *err_msg)
{
  int for_all, left_over;
  int i, j = 0, k = 0;
  int flag = 0;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called, number_of_generators = %d, pct = %lf, grp_idx = %d", number_of_generators, pct, grp_idx);
#if 0
  if (pct < number_of_generators)
  {
    NSDL1_MESSAGES(NULL, NULL, "pct = %d, number_of_generators = %d", (int)pct, number_of_generators);
    sprintf(err_msg, "Number of users/sessions cannot be less than total number of generators used in a group");
    return RUNTIME_ERROR;
  } 
#endif
  //Calculate quantity to distribute to each generator and remainder
  for_all = (int)pct / number_of_generators;
  left_over = (int)pct % number_of_generators;
  NSDL3_MESSAGES(NULL, NULL, "for_all = %d, left_over = %d", for_all, left_over);

  for (i = 0; i < number_of_generators; i++) {
    if (generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags & IS_GEN_ACTIVE)
    {
      runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity = for_all;
      NSDL1_MESSAGES(NULL, NULL, "runprof_table_shr_mem[%d].generator_id_list[%d] = %d", grp_idx, i, runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity);
    }
  }
  for ( j = 0; j < left_over; j++)
  {
    while(k < number_of_generators)
    {
      if (generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags & IS_GEN_ACTIVE)
      {  
        runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity++;
        NSDL1_MESSAGES(NULL, NULL, "runprof_table_shr_mem[%d].generator_id_list[%d] = %d", 
                         grp_idx, k, runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity);
        flag = 1;
      }
      if (flag)
      {
        k = k + 1;
        break;
      }
    }
  }
  return RUNTIME_SUCCESS;
}
#endif
static void runtime_get_gen_quantity(int grp_idx, int ramped_up_users_or_sess, int available_qty_to_remove[], int gen_count)
{
  int i; 
  for (i = 0; i < gen_count; i++) 
  //for (i = 0; i < global_settings->num_process; i++)
  {
    NSDL3_MESSAGES(NULL, NULL,"grp_idx = %d, i = %d, id = %d, flag = %d, ramped_up_users_or_sess = %d", grp_idx, i, runprof_table_shr_mem[grp_idx].running_gen_value[i].id, generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags, ramped_up_users_or_sess);
    if (generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags & IS_GEN_ACTIVE)
    {
      if(ramped_up_users_or_sess == REMOVE_RAMPED_UP_VUSERS) { 
        available_qty_to_remove[i] = runprof_table_shr_mem[grp_idx].running_gen_value[i].cur_vuser_sess;
        NSDL3_MESSAGES(NULL, NULL,"available_qty_to_remove[%d] = %d, cur_vuser_sess = %d", i, available_qty_to_remove[i], runprof_table_shr_mem[grp_idx].running_gen_value[i].cur_vuser_sess);
      }
      else
        available_qty_to_remove[i] = runprof_table_shr_mem[grp_idx].running_gen_value[i].rem_not_ramped_usr;
    }
    if(available_qty_to_remove[i] == 0)
      runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come--;
  } 
}

static int nc_runtime_distribute_removed_quantity(char orig_gen_ids[], int gen_count, int grp_idx, int quantity, 
                          int gen_quantity[], int ramped_up_users_or_sess, int session_mode)
{
  int for_all, proc_index = 0;
  int total_leftover_qty_to_remove = 0;  //users/sessions which are not yet distributed
  int redistribution_flag = 0;
  int gens_left_for_distribution = 0;   
  int assigned_qty;
  int available_qty_to_remove[MAX_NVM_NUM] = {0};  //Stores users/sessios currently existing per NVM - available to remove
  char gen_idx[MAX_NVM_NUM];  
 
  NSDL3_MESSAGES(NULL, NULL,"Method Called. grp_idx=%d, quantity=%d, ramped_up_users_or_sess=%d\n", grp_idx, quantity, ramped_up_users_or_sess);

  //Making local copy of gen array
  memcpy(gen_idx, orig_gen_ids, MAX_NVM_NUM);
  quantity = abs(quantity);  
  //1. Divide equal quantity among gens
  total_leftover_qty_to_remove = quantity;
  for_all = (quantity) / gen_count;

  NSDL2_RUNTIME(NULL, NULL, "Process wise distributed quantity for group=%d...for_all=%d total_leftover=%d", grp_idx, for_all, total_leftover_qty_to_remove);

  //2. Get available quantity to remove ramped_up/not_ramped_up users/sessions
  if(session_mode != SESSION_RTC)
     runtime_get_gen_quantity(grp_idx, ramped_up_users_or_sess, available_qty_to_remove, runprof_table_shr_mem[grp_idx].num_generator_per_grp);

  do 
  {
    NSDL2_RUNTIME(NULL, NULL, "Generator's available for distribution=%d", gen_count);
    if(redistribution_flag)
    {
      for_all = (total_leftover_qty_to_remove)/gen_count;
      redistribution_flag = 0;
      NSDL2_RUNTIME(NULL, NULL, "REDISTRIBUTION set...After redistibution quantity=%d among %d Generators..for_all = %d", 
                                                  total_leftover_qty_to_remove, gen_count, for_all);
    }
    if (!for_all) break;

    for(proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    {
      NSDL2_RUNTIME(NULL, NULL, "**** Generator id %d ****", proc_index);
      //3. Checking whether this NVM is set for distributed quantity
      if(gen_idx[proc_index])
      {
        //3. Calculate quantity which can be assigned to the NVM i.e. (Users/session ramped_up/not_ramped_up - Quantity already set to remove)
      
        assigned_qty = for_all + gen_quantity[proc_index];
      
        NSDL2_RUNTIME(NULL, NULL, "assigned_qty=%d,  available_qty_to_remove[%d]=%d, gen_quantity[%d]=%d",  
                                      assigned_qty, proc_index, available_qty_to_remove[proc_index], proc_index, gen_quantity[proc_index]);

        //If users to delete < users remaning in the NVM
        if ((assigned_qty >= available_qty_to_remove[proc_index]) && (session_mode != SESSION_RTC))
        {
          total_leftover_qty_to_remove -= (available_qty_to_remove[proc_index] - gen_quantity[proc_index]);
          gen_quantity[proc_index] = available_qty_to_remove[proc_index];
          gen_idx[proc_index] = 0;
          gen_count--;
          if (assigned_qty > available_qty_to_remove[proc_index])
            redistribution_flag = 1;
          else
          NSDL2_RUNTIME(NULL, NULL, "GENERATOR CAPACITY FULL"); 
          NSDL2_RUNTIME(NULL, NULL, "quantity left for distribution=%d", total_leftover_qty_to_remove);
          //proc_index = 0;     //As requires redistribution, resetting proc_index=0 to start for all the NVMs
        }
        else
        {
          gen_quantity[proc_index] += for_all;
          total_leftover_qty_to_remove -= for_all;
          NSDL2_RUNTIME(NULL, NULL, "total_leftover_qty_to_remove = %d, gen_quantity[%d] = %d", 
                                     total_leftover_qty_to_remove, proc_index, gen_quantity[proc_index]);
        }
      } 
    }  //end for
  } while (redistribution_flag && gen_count);

  //Redistribute leftover quantity
  NSDL2_RUNTIME(NULL, NULL, "------> Users left to REDISTRIBUTE=%d gen_count=%d", total_leftover_qty_to_remove, gen_count);

  gens_left_for_distribution = gen_count;
  while(total_leftover_qty_to_remove && gens_left_for_distribution)
  {
    for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    {
     if(gen_idx[proc_index])
      {
        if((gen_quantity[proc_index] < available_qty_to_remove[proc_index]) && (session_mode != SESSION_RTC))
        {  
          gen_quantity[proc_index]++;
          total_leftover_qty_to_remove--;
          NSDL2_RUNTIME(NULL, NULL, "gen_quantity[%d]=%d, total_leftover_qty_to_remove=%d",  
                             proc_index, gen_quantity[proc_index], total_leftover_qty_to_remove);
        }
        else
        { 
          if(session_mode != SESSION_RTC)
             gen_idx[proc_index] = 0;
          gen_count--;
          gens_left_for_distribution--;
          NSDL2_RUNTIME(NULL, NULL, "Nvm's left for redistribution = %d", gens_left_for_distribution);
        }
      }

      NSDL2_RUNTIME(NULL, NULL, "NVM %d, gen_quantity=%d gens_left_for_distribution=%d, total_leftover_qty_to_remove=%d", 
                      proc_index, gen_quantity[proc_index], gens_left_for_distribution, total_leftover_qty_to_remove);

      //Break when either users not left for redisribution or no gens left for redistribution
      if (!(total_leftover_qty_to_remove && gens_left_for_distribution))
        break;
    }
  }
  
  NSDL2_RUNTIME(NULL, NULL, "Distributed Quantity:");
  // To be executed in case of Session RTC only. Distribute remaining session among generators
  if(session_mode == SESSION_RTC)
  {
     for (for_all = 0, proc_index=0; for_all < total_leftover_qty_to_remove; proc_index++, for_all++)
     {
        if(gen_idx[proc_index])
           gen_quantity[proc_index]++;
     }
     total_leftover_qty_to_remove =- for_all;   
  }
  
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method");
  return total_leftover_qty_to_remove;
}

static void nc_set_distributed_quantity_reframe_keyword(int grp_idx, int gen_quantity[], int runtime_operation, int gen_count, int grp_type, char *buf, int gen_rate[], int rate_flag, char *text)
{
  NSDL2_RUNTIME(NULL, NULL, "Method called. buf = %s, grp_idx = %d, gen_count = %d, rate_flag = %d, text = %s", 
                             buf, grp_idx, gen_count, rate_flag, text);
  int i;
  char *start_ptr;
  char rate_buf[512] = {0};

  if ((start_ptr = strstr(buf, "SETTINGS")) != NULL)
    NSDL2_RUNTIME(NULL, NULL, "start_ptr = %s", start_ptr);

  for (i = 0; i < gen_count; i++)
  {
    if (generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags & IS_GEN_ACTIVE)
    {
       idx = runprof_table_shr_mem[grp_idx].running_gen_value[i].id;
       if(!generator_entry[idx].msg_len)
         generator_entry[idx].msg_len += sprintf(generator_entry[idx].gen_keyword + generator_entry[idx].msg_len,
                       "RUNTIME_CHANGE_QUANTITY_SETTINGS %d %d\n", global_settings->runtime_increase_quantity_mode,
                       global_settings->runtime_decrease_quantity_mode);
       if (grp_type == TC_FIX_USER_RATE) {
         runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity = (abs(gen_quantity[i])/SESSION_RATE_MULTIPLIER); 
         NSDL2_RUNTIME(NULL, NULL, "For gen index = %d, grp idx = %d, quantity = %d", i,
                       runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity);
         generator_entry[idx].msg_len += sprintf(generator_entry[idx].gen_keyword + generator_entry[idx].msg_len,
                                     "QUANTITY %s %s %0.3f %s\n", runprof_table_shr_mem[grp_idx].scen_group_name,
                                     (runtime_operation == 1)?"INCREASE":"DECREASE",
                                     (double)runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity, start_ptr);
       } else {
         runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity = gen_quantity[i];
         NSDL2_RUNTIME(NULL, NULL, "i = %d, grp_idx = %d, gen_quantity = %d, gen_rate = %d, idx = %d", 
                                    i, grp_idx, gen_quantity[i], gen_rate[i], idx); 
         if(rate_flag) {
           runprof_table_shr_mem[grp_idx].running_gen_value[i].rate_val = gen_rate[i];
           sprintf(rate_buf, "SETTINGS RATE %d %s", abs((int)runprof_table_shr_mem[grp_idx].running_gen_value[i].rate_val), text);
           NSDL2_RUNTIME(NULL, NULL, "rate_buf = %s", rate_buf); 
         }

         NSDL2_RUNTIME(NULL, NULL, "Before: msg_len = %d, gen_keyword = %s, addr of gen_keyword = %p", generator_entry[idx].msg_len,
                                   generator_entry[idx].gen_keyword, generator_entry[idx].gen_keyword);
         generator_entry[idx].msg_len += sprintf(generator_entry[idx].gen_keyword + generator_entry[idx].msg_len,
                                      "QUANTITY %s %s %d %s\n", runprof_table_shr_mem[grp_idx].scen_group_name,
                                      (runtime_operation == 1)?"INCREASE":"DECREASE",
                                      abs((int)runprof_table_shr_mem[grp_idx].running_gen_value[i].quantity),
                                      ((rate_flag == 1)?rate_buf:start_ptr));
       }
       NSDL2_RUNTIME(NULL, NULL, "msg_len = %d, gen_keyword = %s, addr of gen_keyword = %p", generator_entry[idx].msg_len,
                                 generator_entry[idx].gen_keyword, generator_entry[idx].gen_keyword);
    }
  }
}
#define THOUSAND 1000.00
inline static void fill_output_msg(int grp_idx, int quantity, int quantity_left_to_remove,  char *err_msg, int grp_type)
{
  NSDL1_RUNTIME(NULL, NULL, "Method called");
  Schedule *cur_schedule;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    cur_schedule = scenario_schedule;
  else
    cur_schedule = &(group_schedule[grp_idx]);

  int phase_idx = cur_schedule->phase_idx;

  if (phase_idx < 0)
    phase_idx = 0;
  NSDL3_RUNTIME(NULL, NULL, "Applying RTC in Phase Name = %s, phase_idx=%d", cur_schedule->phase_array[cur_schedule->phase_idx].phase_name, cur_schedule->phase_idx);

  if (grp_type == TC_FIX_USER_RATE)
    sprintf(err_msg,"%.3f Session(s) rate per minute %s group '%s' in phase '%s'\n",
                 abs(quantity - quantity_left_to_remove)/THOUSAND,   //No. of Sessions
                 ((quantity > 0)? "Increased in":"Decreased from"),
                 (runprof_table_shr_mem[grp_idx].scen_group_name),
                 cur_schedule->phase_array[phase_idx].phase_name);   //Phase Name      
  else
     sprintf(err_msg,"%d User(s) %s group '%s' in phase '%s'\n",
                 abs(quantity - quantity_left_to_remove),   //No. of Users
                 ((quantity > 0)? "Added in":"Removed from"),
                 (runprof_table_shr_mem[grp_idx].scen_group_name),
                 cur_schedule->phase_array[phase_idx].phase_name);   //Phase Name                
}

static int nc_divide_usr_wrt_generator(char gen_idx[], int gen_count, int grp_idx, int quantity, int gen_quantity[], int gen_rate[], char *buf, int *rate_flag, char *text)
{
  int for_all, balance, j, proc_index, len = 0;
  int rate_for_all = 0, rate_for_balance = 0;
  char rate_val[28] = {0};
  char *start_ptr = NULL;
  char *ptr = NULL;

  NSDL1_MESSAGES(NULL, NULL, "Method Called. buf = %s, grp_idx=%d, quantity=%d", buf, grp_idx, quantity);
 
  //buf = QUANTITY G1 INCREASE 200.0 SETTINGS RATE 100 M LINEARLY
  if ((start_ptr = strstr(buf, "RATE")) != NULL) {
    NSDL2_RUNTIME(NULL, NULL, "start_ptr = %s", start_ptr);
    start_ptr += 5;
    if((ptr = strchr(start_ptr, ' ')) != NULL) {
      ptr++;
      len = ptr - start_ptr;
      strncpy(rate_val, start_ptr, len);
      strcpy(text, ptr);
    }
    *rate_flag = 1;
  }

  for_all = (quantity) / gen_count;
  balance = (quantity) % gen_count;

  if(*rate_flag) {
    NSDL2_MESSAGES(NULL, NULL, "rate_val = %d", atoi(rate_val));
    rate_for_all = atoi(rate_val) / gen_count;
    rate_for_balance = atoi(rate_val) % gen_count;
  }
  
  NSDL2_MESSAGES(NULL, NULL, "Distribution for added quantity. for_all=%d, balance=%d, rate_for_all = %d, rate_for_balance = %d", 
                              for_all, balance, rate_for_all, rate_for_balance);

  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    if(gen_idx[proc_index])
       gen_quantity[proc_index] += for_all;
    if((*rate_flag) && gen_idx[proc_index]) {
      gen_rate[proc_index] += rate_for_all;
    }
  }

  for (j = 0, proc_index=0; j < balance; proc_index++, j++)
  {
    if(gen_idx[proc_index])
       gen_quantity[proc_index]++;
  }

  for(j = 0, proc_index=0; j < rate_for_balance; proc_index++, j++) {
    if(gen_idx[proc_index]) {
      gen_rate[proc_index]++;
    }
  }
  NSDL3_MESSAGES(NULL, NULL, "Exiting Method");
  return RUNTIME_SUCCESS;
}


static int nc_runtime_distribute_quantity(char gen_idx[], int gen_count, int grp_idx, int quantity, char *err_msg, int runtime_operation, char *buf, int session_mode)
{
  int quantity_left_to_remove;
  int gen_quantity[MAX_NVM_NUM] = {0};
  int gen_rate[MAX_NVM_NUM] = {0};
  int ret, rate_flag = 0;
  int grp_type = get_group_mode(grp_idx);
  int group_not_started = 0;
  char text[512];
  Schedule *schedule;

  NSDL3_MESSAGES(NULL, NULL, "Method called, gen_count = %d, grp_idx = %d, quantity = %d, runtime_operation = %d", gen_count, grp_idx, quantity, runtime_operation);

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    schedule = scenario_schedule;
  else
    schedule = &group_schedule[grp_idx];
  if((schedule->phase_array[0].phase_type == SCHEDULE_PHASE_START) && (schedule->phase_array[0].phase_status != PHASE_IS_COMPLETED))
    group_not_started = 1;
  if(runtime_operation == INCREASE_USERS_OR_SESSIONS)
  {
    if(schedule->phase_array[schedule->phase_idx].phase_status == PHASE_IS_COMPLETED) {
      NSTL1(NULL, NULL, "gen_count = %d, grp_idx = %d, phase_status = %d, total_done_msgs_need_to_come = %d, phase_idx = %d", 
                         gen_count, grp_idx, schedule->phase_array[schedule->phase_idx].phase_status,
                         runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come, schedule->phase_idx);
      runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come -= gen_count;
    }

    ret = nc_divide_usr_wrt_generator(gen_idx, gen_count, grp_idx, quantity, gen_quantity, gen_rate, buf, &rate_flag, text);
    if (!ret)
      nc_set_distributed_quantity_reframe_keyword(grp_idx, gen_quantity, runtime_operation, runprof_table_shr_mem[grp_idx].num_generator_per_grp, grp_type, buf, gen_rate, rate_flag, text);
    else
      return RUNTIME_ERROR;
  } 
  else 
  {
    if (global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_0) {
      quantity_left_to_remove = nc_runtime_distribute_removed_quantity(gen_idx, gen_count, grp_idx, quantity,
                                                             gen_quantity, REMOVE_RAMPED_UP_VUSERS, session_mode);
      nc_set_distributed_quantity_reframe_keyword(grp_idx, gen_quantity, runtime_operation, runprof_table_shr_mem[grp_idx].num_generator_per_grp, grp_type, buf, gen_rate, 0, NULL);
      NSDL2_MESSAGES(NULL, NULL, "Quantity left to remove = %d", quantity_left_to_remove);
      
    } else if (global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_1) {
      if(!group_not_started)
        quantity_left_to_remove = nc_runtime_distribute_removed_quantity(gen_idx, gen_count, grp_idx, quantity,
                                                             gen_quantity, REMOVE_RAMPED_UP_VUSERS, session_mode);
      NSDL2_MESSAGES(NULL, NULL, "Quantity left to remove = %d", quantity_left_to_remove);

      //memset(gen_quantity, 0, sizeof(gen_quantity));
      if (quantity_left_to_remove)
        quantity_left_to_remove = nc_runtime_distribute_removed_quantity(gen_idx, gen_count, grp_idx, quantity_left_to_remove, 
                                                              gen_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, session_mode);
      nc_set_distributed_quantity_reframe_keyword(grp_idx, gen_quantity, runtime_operation, gen_count, grp_type, buf, gen_rate, 0, NULL);
      NSDL2_MESSAGES(NULL, NULL, "Quantity which cannot be removed = %d", quantity_left_to_remove);

    } else if (global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_2) {
      quantity_left_to_remove = nc_runtime_distribute_removed_quantity(gen_idx, gen_count, grp_idx, quantity, gen_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, session_mode);
      NSDL2_RUNTIME(NULL, NULL, "Quantity left to remove = %d", quantity_left_to_remove);

      //memset(gen_quantity, 0, sizeof(gen_quantity));
      if (!group_not_started && quantity_left_to_remove)
        quantity_left_to_remove = nc_runtime_distribute_removed_quantity(gen_idx, gen_count, grp_idx, quantity_left_to_remove,
                                                             gen_quantity, REMOVE_RAMPED_UP_VUSERS, session_mode);
      nc_set_distributed_quantity_reframe_keyword(grp_idx, gen_quantity, runtime_operation, gen_count, grp_type, buf, gen_rate, 0, NULL);
      NSDL2_RUNTIME(NULL, NULL, "Quantity which cannot be removed = %d", quantity_left_to_remove, buf);

    }
  } 
  fill_output_msg(grp_idx, quantity, -quantity_left_to_remove, err_msg, grp_type);
  return RUNTIME_SUCCESS;
}

static void runtime_get_gen_cnt_serving_group(char gen_ids[], int *gen_count, int grp_idx)
{
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method called.");
  //Need to mark active generators for scenario group
  for (i = 0; i < runprof_table_shr_mem[grp_idx].num_generator_per_grp; i++)
  {
    if (generator_entry[runprof_table_shr_mem[grp_idx].running_gen_value[i].id].flags & IS_GEN_ACTIVE)      
    {  
      gen_ids[i] = 1;
      *gen_count += 1; 
    }
  }
  NSDL2_MESSAGES(NULL, NULL, "gen_count = %d", *gen_count);
}

/* Function used to distribute users/sessions among active generators
 * */
int distribute_quantity_among_generators(char *buf, char *err_msg, int runtime_id, int first_time, int *index)
{
  int grp_idx, runtime_operation;
  int quantity;
  char gen_idx[MAX_NVM_NUM] = {0};//Maximum number of generators 254 
  int gen_count = 0;
  int ret;
  int session_mode = 0; //Default value

  NSDL1_MESSAGES(NULL, NULL, "Method Called buf=%s, runtime_id = %d, first_time = %d", buf, runtime_id, first_time);
  
  if(sigterm_received)
  { 
    strcpy(err_msg, "Cannot change users/sessions after test-run is stopped by the user");
    return RUNTIME_ERROR;
  } 
    
  ret = parse_keyword_runtime_quantity(buf, err_msg, &grp_idx, &quantity, &runtime_operation, runtime_id, index, &session_mode);
  if(ret == RUNTIME_ERROR) return ret;
  
  //2. Get number of active generators serving this group.
  runtime_get_gen_cnt_serving_group(gen_idx, &gen_count, grp_idx);

  if(gen_count == 0)
  {
    NSDL2_MESSAGES(NULL, NULL, "Cannot apply runtime changes to group=%s as Generator(s)'s running for this group is/are over",
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
    sprintf(err_msg, "Cannot apply runtime changes to group=%s as as Generator(s)'s running for this group is/are over",
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
    return RUNTIME_ERROR;
  }
  if(!first_time) 
  { 
    g_rtc_start_idx = *index;
  }

  /*runtime_schedule[*index].start_idx = g_rtc_start_idx;  
  runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id++;  
  runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come += gen_count;*/
  
  NSDL2_RUNTIME(NULL, NULL, "runtime_schedule[*index].start_idx = %d, index = %d, gen_count = %d, g_rtc_start_idx = %d, runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id = %d, runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come = %d", runtime_schedule[*index].start_idx, *index, gen_count, g_rtc_start_idx, runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id, runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come);

  //3. Distribute quantity and reframe generator keyword
  ret = nc_runtime_distribute_quantity(gen_idx, gen_count, grp_idx, quantity, err_msg, runtime_operation, buf, session_mode);

  if(ret == RUNTIME_ERROR)
    return ret;

  runtime_schedule[*index].start_idx = g_rtc_start_idx;  
  runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id++;  
  runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come += gen_count;
 
  return RUNTIME_SUCCESS;
} 

/* Function used to process PAUSE acknowledgement message
 * RTC_PAUSE_DONE            145
 **/ 
int process_pause_done_message(int fd, User_trace *msg)
{
  char genInfo[257];
  NSDL2_MESSAGES(NULL, NULL, "Method called, child_id = %d, opcode = %d, fd = %d,"
                             "gen_rtc_idx = %d, rtcdata->msg_seq_num = %d", msg->child_id,
                             msg->opcode, fd, msg->gen_rtc_idx, rtcdata->msg_seq_num);


  //TODO: handling for group in case of seperate generator for each group or for ALL */
  //if generator got killed in between the processing of runtime change
  /* TODO: replace below code with child bitmask 
     Two cases might observe here
     1. PAUSE_DONE received before child failure
     2. PAUSE_DONE received after child failure
  */
  
  NSDL3_MESSAGES(NULL, NULL, "%s%s) RTC_PAUSE_DONE(145) fd = %d, rtcdata->cur_state = %d",
                    (loader_opcode == MASTER_LOADER)?"(Master <- Generator:":"(Parent <- NVM:",
                    get_gen_id_name_ip(msg->child_id, genInfo), fd, rtcdata->cur_state);

  NSTL1(NULL, NULL, "%s%s) RTC_PAUSE_DONE(145) fd = %d, rtcdata->cur_state = %d",
                    (loader_opcode == MASTER_LOADER)?"(Master <- Generator:":"(Parent <- NVM:",
                    get_gen_id_name_ip(msg->child_id, genInfo), fd, rtcdata->cur_state);

  if(msg->gen_rtc_idx != rtcdata->msg_seq_num)
  {
    NSDL2_MESSAGES(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d", msg->gen_rtc_idx, rtcdata->msg_seq_num); 
    NSTL1(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d", msg->gen_rtc_idx, rtcdata->msg_seq_num); 
    return 0;
  }
  DEC_CHECK_RTC_RETURN(msg->child_id, 0)
  rtcdata->opcode = msg->opcode;

  if(rtcdata->cur_state == RESET_RTC_STATE)
  {
    NSTL1(NULL, NULL, "ERROR: RTC_PAUSE_DONE(145), rtc state = %d, which is always"
                      " be set in case of failure. This will not processed", rtcdata->cur_state);
    return 0;
  }

  if((loader_opcode == MASTER_LOADER) && CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG))
    process_nc_schedule_detail_response_msg(fd, msg);

  if(CHECK_ALL_RTC_MSG_DONE)
  {
    rtcdata->cur_state = RESET_RTC_STATE;//Reset the runtime change state
    NSTL1(NULL, NULL, "Got all RTC_PAUSE_DONE messages from generators/child");
    if (loader_opcode == CLIENT_LOADER)
    {
      if(CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG))
        process_rtc_qty_schedule_detail(msg->opcode, rtcdata->msg_buff);
      else
        send_msg_to_master(master_fd, msg->opcode, CONTROL_MODE);
    } 
    else //Controller and NS (standalone) parent 
    {
      NSDL3_MESSAGES(NULL, NULL, "Got all RTC_PAUSE_DONE messages from generators/child, rtcdata->rtclog_fp = %p", rtcdata->rtclog_fp);
      NSTL1(NULL, NULL, "Applying runtime changes.");
      apply_runtime_changes(0);
    }
  }
  return 0;
}

int process_resume_done_message(int fd, int opcode, int child_id, int gen_rtc_idx)
{
  char time[0xff];
  int rtc_idx, rtc_flag = 0;
  char phase_name[50];
  char genInfo[257];

  NSDL2_MESSAGES(NULL, NULL, "Method called, num_process = %d, child_id = %d, opcode = %d, fd = %d, "
                             "child_sequence_num = %d, parent_sequence_num = %d", 
                              global_settings->num_process, child_id, opcode, fd, gen_rtc_idx, rtcdata->msg_seq_num);

  NSTL1(NULL, NULL, "%s%s) RTC_RESUME_DONE(146) fd = %d, rtcdata->cur_state = %d", 
                    (loader_opcode == MASTER_LOADER)?"(Master <- Generator:":"(Parent <- NVM:", 
                    get_gen_id_name_ip(child_id, genInfo), fd, rtcdata->cur_state);

  if (gen_rtc_idx != rtcdata->msg_seq_num)
  { 
    NSDL2_MESSAGES(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d", gen_rtc_idx, rtcdata->msg_seq_num);
    NSTL1(NULL, NULL, "Unknown request found, gen_rtc_idx = %d, rtcdata->msg_seq_num = %d", gen_rtc_idx, rtcdata->msg_seq_num);
    return 0;
  } 
  DEC_CHECK_RTC_RETURN(child_id, 0)
  rtcdata->opcode = opcode;

  NSDL3_MESSAGES(NULL, NULL, "%s%s) RTC_RESUME_DONE(146) fd = %d, rtcdata->cur_state = %d",
                    (loader_opcode == MASTER_LOADER)?"(Master <- Generator:":"(Parent <- NVM:",
                    get_gen_id_name_ip(child_id, genInfo), fd, rtcdata->cur_state);


  if (CHECK_ALL_RTC_MSG_DONE)
  {
    if(rtcdata->cur_state == RESET_RTC_STATE) {
      NSTL1(NULL, NULL, "ERROR: RTC_RESUME_DONE(146): Here rtc state will be = %d, which is always "
                        "be set in case of failure. So Sending QUANTITY_RESUME_RTC(155) and returning", rtcdata->cur_state );  
      
      return 0;   
    }
    rtcdata->cur_state = RESET_RTC_STATE;//Reset the runtime change state
    NSTL1(NULL, NULL, "Got all RTC_RESUME_DONE messages from generators/child");
    if (loader_opcode == CLIENT_LOADER)
      send_msg_to_master(master_fd, opcode, CONTROL_MODE);

    for(rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    { 
      NSDL3_MESSAGES(NULL, NULL, "rtc_idx = %d, rtc_state = %d", rtc_idx, runtime_schedule[rtc_idx].rtc_state);
      if(runtime_schedule[rtc_idx].rtc_state == RTC_NEED_TO_PROCESS)
      {
        convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
        sprintf(phase_name, "RTC_PHASE_%d", runtime_schedule[rtc_idx].rtc_id);
        NSDL3_MESSAGES(NULL, NULL, "***rtc_idx = %d, rtc_id = %d", rtc_idx, runtime_schedule[rtc_idx].rtc_id);
        runtime_schedule[rtc_idx].rtc_state = RTC_RUNNING;
        rtc_flag = 1;
      }
    }
    if(rtc_flag)
      log_phase_time(!PHASE_IS_COMPLETED, 6, phase_name, time);

    RUNTIME_UPDATION_RESPONSE
  }
  return 0;
}

void rtc_log_on_epoll_timeout(int epoll_timeout)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, epoll_timeout = %d", epoll_timeout);

  u_ns_ts_t rtc_epoll_diff;
  //Set end time for epoll 
  u_ns_ts_t rtc_epoll_end_time = get_ms_stamp();  

  rtc_epoll_diff = rtc_epoll_end_time - rtcdata->epoll_start_time;

  //If runtime changes took longer time and epoll timeout then we need to resume test
  if(rtc_epoll_diff < epoll_timeout)
  {
    //TODO: TBD
    if(!(rtc_epoll_diff % 1000)) //print only for second
      NSTL1(NULL, NULL, "RTC Epoll: Started time(ms) = %llu, End time(ms) = %llu, Timeout(ms) = %d, "
                        "difference(ms) = %llu, rtc state = %llu", rtcdata->epoll_start_time, rtc_epoll_end_time,
                        epoll_timeout, rtc_epoll_diff, rtcdata->cur_state);
    return;
  }

  NSTL1(NULL, NULL, "RTC Epoll: Timeout(ms) = %d reached, hence resume generator/child processes, "
                    "difference = %llu, rtc state = %llu", epoll_timeout, rtc_epoll_diff, rtcdata->cur_state);

  //Either this is standalone or controller
  if ((rtcdata->cur_state == RTC_PAUSE_STATE) || (rtcdata->cur_state == RTC_START_STATE))
  {
    if(CHECK_RTC_FLAG(RUNTIME_FPARAM_FLAG))
    {
      sprintf(rtcdata->err_msg, "RTC Epoll: Timeout(%d ms), %s not received from generator/child", epoll_timeout, 
                                (loader_opcode != MASTER_LOADER)?"RTC_PAUSE_DONE(145)":"RTC_RESUME_DONE(146)");
      RUNTIME_UPDATION_LOG_ERROR(rtcdata->err_msg, 0);
      handle_fparam_rtc_done(-2);
      return;
    }

    if (loader_opcode != CLIENT_LOADER)
    {
      if (loader_opcode == MASTER_LOADER)
      {
        NSTL1(NULL, NULL, "RTC Epoll: Timeout(%d ms) in applying runtime changes. Sending resume"
                          " message to all generators", epoll_timeout);
        //on_failure_resume_gen_update_rtc_log("Error in applying runtime changes. Hence sending resume message to all generators");
        rtcdata->cur_state = RTC_RESUME_STATE;
        send_msg_to_all_clients(RTC_RESUME, 0);
      }
      else
      {
        NSTL1(NULL, NULL, "RTC Epoll: Timeout in applying runtime changes. Sending resume message to all NVMs");
        //on_failure_resume_gen_update_rtc_log("Error in applying runtime changes. Hence sending resume message to all NVMs");
        process_resume_from_rtc(g_rtc_msg_seq_num);
      }
      RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("RTC: Epoll timeout, pause done message was not received from generator(s)/ NVM(s).");
      global_settings->pause_done = 0;
      rtcdata->test_paused_at = 0;
    }
    else //For Generator
    {  
      char err_msg[1024];
      sprintf(err_msg, "Epoll timeout on generator %s, error while applying runtime changes.", global_settings->event_generating_host);
      NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1, -1, EVENT_CORE, EVENT_INFORMATION, "NA", "NA",
                     "Epoll timeout on generator %s, error while applying runtime changes.", global_settings->event_generating_host);
      send_rtc_msg_to_controller(master_fd, NC_RTC_FAILED_MESSAGE, err_msg, -1);       
    }
  }
  else
  {
    NSTL1(NULL, NULL, "RTC: Resume done message was not received from generator(s)/ NVM(s).");          
    //In case of RTC failure for NS/NC we are setting this flag to RTC Failure(-1).
    SET_RTC_FLAG(RUNTIME_PASS);
    rtcdata->cur_state = RESET_RTC_STATE;
    RUNTIME_UPDATION_RESPONSE
  }
  RESET_RTC_BITMASK;
}

//Function used to check whether killed generator belongs to given scenario group 
//Here generator and group index will be provided and we need to search index in generator id list
//Return Value: In case of success it returns group index else returns -1
int find_group_idx_using_gen_idx(int gen_id, int grp_id)
{
  int idx;
  NSDL1_MESSAGES(NULL, NULL, "Method called, generator id = %d, scenario group id = %d", gen_id, grp_id);
  for(idx = 0; idx < runprof_table_shr_mem[grp_id].num_generator_per_grp; idx++)
  {
    if (runprof_table_shr_mem[grp_id].running_gen_value[idx].id == gen_id)
    {
      NSDL2_MESSAGES(NULL, NULL, "Generator id found in list hence returning..");
      return grp_id;
    } 
  }  
  NSDL2_MESSAGES(NULL, NULL, "Generator id not found in list. Hence returning -1.");
  return -1;
}

/*Find maximum number of NVMs running per generator
*/
void find_max_num_nvm_per_generator(Msg_com_con *mccptr, parent_msg *msg, int num_started)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, maximum number of NVM per generator = %d", 
                 global_settings->max_num_nvm_per_generator);

  generator_entry[msg->top.internal.child_id].total_nvms = msg->top.internal.num_nvm_per_generator;
  if(num_started == 1)
    global_settings->max_num_nvm_per_generator = msg->top.internal.num_nvm_per_generator;

  NSTL1(NULL, NULL, "NVM Details received from Generator:  Index = %d, Name = %s, IP = %s, Number of NVM = %d, "
                    "Max Number of NVM = %d", msg->top.internal.child_id, generator_entry[msg->top.internal.child_id].gen_name, 
                    mccptr->ip, msg->top.internal.num_nvm_per_generator, global_settings->max_num_nvm_per_generator);

  if (global_settings->max_num_nvm_per_generator < msg->top.internal.num_nvm_per_generator) 
  {
    NSTL1(NULL, NULL, "Maximum number of NVMs are less than number of NVMs received from "
                      "generator. Hence updating maximum number of NVMs to [%d]", 
                      msg->top.internal.num_nvm_per_generator);
    global_settings->max_num_nvm_per_generator = msg->top.internal.num_nvm_per_generator;
  }
} 

/*In case of controller we need to create used unused file*/
void ni_create_data_files_frm_last_files()
{
  int j;
  char err_msg[1024];
  NSDL2_VARS(NULL, NULL, "Method called, total_api_entries = %d", total_api_entries);
  for(j = 0; j < total_api_entries; j++)
  {
    NSDL2_VARS(NULL, NULL, "api_table[%d].sequence = %d", j, api_table[j].sequence);
    if ((api_table[j].sequence == USEONCE) && (!api_table[j].is_sql_var))
      if (nslb_uo_create_data_file_frm_last_file(api_table[j].data_fname, (api_table[j].first_data_line - 1), err_msg, j) == -2)
        NSTL1_OUT(NULL, NULL, "%s", err_msg);
  }  
}

// keyword parsing usages 
static void check_use_of_gen_specific_kwd_file(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE <mode>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is check sys/gen_specific_site_keywords.default file is used or not in generator's.\n");
  NSTL1_OUT(NULL, NULL, "    Mode: Mode for enable/disable. It can only be 0, 1\n");
  NSTL1_OUT(NULL, NULL, "      0 - Disable.\n");
  NSTL1_OUT(NULL, NULL, "      1 - Enable.(default)\n");
  NS_EXIT(-1, "%s\nUsage: DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE <mode>", err);
}

int kw_check_use_of_gen_specific_kwd_file(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;

  num = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);

  if(num != 2)
  {
    check_use_of_gen_specific_kwd_file("Invalid number of arguments");
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    check_use_of_gen_specific_kwd_file("DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE mode is not numeric");
  }
  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    check_use_of_gen_specific_kwd_file("DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE mode is not valid");
  }

  global_settings->disable_use_of_gen_spec_kwd_file = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->disable_use_of_gen_spec_kwd_file = %d", global_settings->disable_use_of_gen_spec_kwd_file);

  return 0;
}

#if 0
void wait_for_start_msg(int udp_fd)
{
  parent_msg  msg;
  struct sockaddr_in addr;
  socklen_t addr_len;
  int i = 0;
  int rcv_amt;
  int max_fd, cnt;
  fd_set udp_rfdset;
  struct timeval timeout;
  int count = total_client_started;
  u_ns_ts_t tu1, tu2;
  //printf("count= %d\n", count);
  //this is done to reset struct clients for childs and not for agents.
  memset(clients, 0, ((sizeof(clients))*total_client_entries));
  addr_len = sizeof(struct sockaddr);

  // Set total timeout of 20 sceconds to get start message from all cleintds
  timeout.tv_sec = 20;
  timeout.tv_usec = 0;
  printf("Waiting for start messages from %d clients.\n", total_client_started);
  tu1 = get_ms_stamp();
  while(count != 0)
  {
    FD_ZERO(&udp_rfdset);
    FD_SET(udp_fd, &udp_rfdset);
    max_fd = udp_fd;
    cnt = select (max_fd +1, &udp_rfdset, NULL, NULL, &timeout);
    // Note - If select does not timeout, it will update timeout with remaing time
    if(cnt > 0)
    {
      if ((rcv_amt = recvfrom (udp_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&addr,  &addr_len)) <= 0)
        printf("Error in receving data\n");

      if(msg.top.internal.opcode == START_MSG_BY_CLIENT)
      {
        upd_client_data(&i, addr);
        count--;
	  NSDL2_CHILD(vptr, cptr, "Receving start message from client[%s]\n", inet_ntoa(addr.sin_addr));

        //printf("count= %d\n", count);
      }
      else
      {
        printf("Unexpected message received while waiting for Start Msg from clinet.\n");
        printf("  Opcode = %d, Client IP = %s, Port = %d\n", msg.top.internal.opcode, inet_ntoa(addr.sin_addr), (int)addr.sin_port);
      }
    }
    else if(cnt == 0)
    {
      sprintf(err_buff, "TimeOut: While waiting for start MSG from Client, about to kill all Clients\n");
      shutdown_master(err_buff);    
    }
    else
    {
      if (errno != EINTR)
        sprintf(err_buff, "Error: While waiting for start MSG from Client, about to kill all Clients\n");
      shutdown_master(err_buff);    
    }
  }
  tu2 = get_ms_stamp();
  printf("Start messages recieved successfully from %d clients (in %lu msec).\n", total_client_started, tu2 - tu1);

  //here we r calculating how many Childs are given and how many are started.if the value is not same then we should kill thoes which r running.
  if(total_client_started != total_client_entries)
  {
    sprintf(err_buff, "Error: Not all Clients started, about to kill all running Clients\n");
    shutdown_master(err_buff);
  }
}

void upd_client_data(int *idx, struct sockaddr_in addr)
{
  //printf("In udp_client_data\n");
  //if(clients[*idx].ip_addr == 0)
  //{
   // printf("IP =%s, port = %d, idx = %d\n", inet_ntoa(addr.sin_addr), (int)addr.sin_port, *idx);
    clients[*idx].ip_addr = addr.sin_addr.s_addr;
    clients[*idx].port = addr.sin_port;
    (*idx)++;
  //}
  //printf("Returnin from udp_client_data \n");
}
#endif

/**
master_close()
{

  for (i=0; i<total_client_entries; i++) {
    close(clientfd[i]);
  }
  free(clientfd);
  free(clientaddr);
  close(curfd);
  if (rfp) fclose(rfp);
  return(0);

}
**/

