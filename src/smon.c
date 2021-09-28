/******************************************************************
 * Name    :    smon.c
 * Purpose :    This file contains methods related to the library
                that maintains a dynamic table for each connection
                with the server. It sends data to the server in the
                following format init_monitor frequency
                pgm_path exec_option. And receives data from the server.
 * Author  :    Amitabh Modak
 * Note    :
 * Intial version date   : 30/06/05 by Amitabh Modak
 * Last modification date: 12/04/08 by Archana
*****************************************************************/

#define MAX_SMON_MSG_SIZE 4096
//#define FAILURE -1
//#define SUCCESS 0

#include <stdio.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <regex.h>

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
#include "tmr.h"
#include "timing.h"
#include "nslb_sock.h"
#include "nslb_cav_conf.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_event_log.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_check_monitor.h"
#include "smon.h"
#include "ns_user_monitor.h"
#include "ns_mon_log.h"
#include "ns_string.h"
#include "ns_server_admin_utils.h"
#include "ns_event_id.h"
#define NS_EXIT_VAR
#include "ns_exit.h"
// -Achint -Add 23/02/2007--
#define RUN_EVERY_TIME 1 //For linux to get create server data
#define RUN_ONLY_ONCE 2 // Fow window to get create server data
//-- End

typedef struct MonTable
{
  char con_type; // Must be first field
  int fd; // fd must be the 1st field as we are using this struct as epoll_event.data.ptr
  int monitor_id;
  char *server;
  char *pgm_path;
  int is_data_filled;
  Long_data *data;
  char *data_buf;
  int dindex;
  int exec_option;
  int idx_in_send_datapoint; // -Achint 23/02/2007--To take index of windows data in send data point
  char cmd_args[1024]; // - Achint 02/06/2007 -- Option Command Arguments
} MonTable;

MonTable *monTable;

typedef struct MonTypeTable
{
  char *monitor_name;
  char *pgm_path;
  int num_data;
  int exec_option;
  int mon_count; // No. of monitors used in the scenario file : Anuj 18/12/07
  void (*print_monito_func)(FILE*fp1, FILE* fp2, MonTable *mptr);
  void (*fill_monito_func)(MonTable *mptr);
} MonTypeTable;

extern void fprint2f(FILE *fp1, FILE *fp2, char* format, ...);
static int filldata(MonTable *mptr);
static int checkfornewline(MonTable *mptr);
static int receive(MonTable *mptr);

static void print_no_system_stats_gp(FILE* fp1, FILE* fp2, MonTable *mptr);
static inline void fill_no_system_stats_gp(MonTable *mptr);
//void log_no_system_stats_gp();

static void print_no_network_stats_gp(FILE* fp1, FILE* fp2, MonTable *mptr);
static inline void fill_no_network_stats_gp(MonTable *mptr);
//void log_no_network_stats_gp ();

// server stat Group start
static void print_window_perfmon(FILE* fp1, FILE* fp2, MonTable *mptr);
static void fill_window_server_stats_gp(MonTable *mptr);
void log_window_server_stats_gp (MonTable *mptr);
// The server stat for linux get filled in the rstat.c
// server stat Group end

// tunnel stat Group start
static void print_tunnel_stats_gp(FILE* fp1, FILE* fp2, MonTable *mptr);
static void fill_tunnel_stats_gp(MonTable *mptr);
void log_tunnel_stats_gp ();
// tunnel stat Group end

int total_tunnels = 0;
char server_name[100];

MonTypeTable monTypeTable[] =
  {
    {"linux", "/opt/cavisson/monitors/bin/linmon", 14, RUN_EVERY_TIME, 0, print_no_system_stats_gp, fill_no_system_stats_gp},
    {"tcp", "/opt/cavisson/monitors/bin/tcpmon", 10, RUN_EVERY_TIME, 0, print_no_network_stats_gp, fill_no_network_stats_gp},
    {"windows_perfmon", "c:/opt/cavisson/monitors/bin/nsi_perfmon", 27, RUN_ONLY_ONCE, 0, print_window_perfmon, fill_window_server_stats_gp},// Achint - 23/02/2007 - for windows
    {"nc_mon", "/opt/netchannel/bin/nc_mon", 40, RUN_EVERY_TIME, 0, print_tunnel_stats_gp, fill_tunnel_stats_gp}
  };//Achint - 01/05/2007 - for Netchannel

int num_mon_type_entries = 4;

//MonTable is dynamic suzed table. In the begining montable will have buffer for 5 entries
#define INIT_MONTABLE_ENTRIES 5
//When more rows need to be created and current buffer run short, tables buffer is expanded by 5 more
#define DELTA_MONTABLE_ENTRIES 5

int total_montable_entries = 0; //This is the size of the monTable
static int max_montable_entries;  //This is the max buffer available in monTable

#ifdef NS_DEBUG_ON
static char *cm_to_str(MonTable *monTable)
{
  static char cm_buf[2048];
  sprintf(cm_buf, "Name = %s, fd = %d, dindex = %d, cmd_args = %s", monTable->pgm_path, monTable->fd, monTable->dindex, monTable->cmd_args);
  return(cm_buf);
}
#endif

//data which contains only digits & spaces is accepted
static int validate_data(MonTable *mptr)
{
  int i = 0;
  char *buffer = mptr->data_buf;
  NSDL2_MON(NULL, NULL, "Method called. buffer = %s", buffer);

  if(strstr(buffer, "Error:"))
  {
    /* Commented by arun as following task will be done by ns_monitor_log
    fprintf(stderr, "Received error from monitor '%s'\n", mptr->pgm_path);
    fprintf(stderr, "%s\n", buffer);
    error_log("Received error from monitor '%s'", mptr->pgm_path);
    error_log(buffer);
    */
    ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR, buffer);
    mptr->dindex = 0;
    mptr->is_data_filled = 0;
    return 1;
  }
  while(i < strlen(buffer))
  {
    if(buffer[i] != ' ' &&  buffer[i] != '.' && !(isdigit(buffer[i])))
    {
      /* Commented by Neeraj on Aug 25, 08 as this error comes every 10 secs
         on the console and TestRunOutput e.g. vmstat headers (macys)
      fprintf(stderr, "Received invalid data from monitor '%s'\n", mptr->pgm_path);
      fprintf(stderr, "%s\n", buffer);
      error_log("Received invalid data from monitor '%s'", mptr->pgm_path);
      error_log(buffer);
      */
      ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_INV_DATA, EVENT_MAJOR,
					"Received invalid data from monitor: %s", buffer);
      mptr->dindex = 0;
      mptr->is_data_filled = 0;
      return 1;
    }
    i++;
  }
 return 0;
}
// Montable Creation function
//On success row num contains the newly created row-index of MonTable
static void
Create_montable_entry(int *row_num)
{
  NSDL2_MON(NULL, NULL, "Method called");
  if (total_montable_entries == max_montable_entries)
  {
    MY_REALLOC(monTable, (max_montable_entries + DELTA_MONTABLE_ENTRIES) * sizeof(MonTable), "montable", -1);
    max_montable_entries += DELTA_MONTABLE_ENTRIES;
  }
  *row_num = total_montable_entries++;
}

// To be used for reading keywords in scenario file
// buf may contain a pattern of the format MONITOR monitor-name host-ip:port [pgm-path]
// pgm-path field is optional.
// Make sure function does not modify buf.If the first field of the buf is not MONITOR, function returns
// immediately with 0. Othewise does more processing as described below
// and returns 1 after processing.
void
monitor_config(char *keyword, char *buf)
{
  char host[64], key[64], mname[64], pgm_path[256];
  //int mon_row = 0;
  int num; //mon_id, dbuf_len;
  char tunnel1[64 + 1] = "";
  char tunnel2[64 + 1] = "";
  char tunnel3[64 + 1] = "";
  char tunnel4[64 + 1] = "";
  char tunnel5[64 + 1] = "";
  char tunnel6[64 + 1] = ""; // This only for checking. we are supporting max only 5 tunnels
  char cmd_args[1024] = "";

  NSDL2_MON(NULL, NULL, "Method called");
  if (strcasecmp(keyword, "MONITOR") != 0) return;

  pgm_path[0] = '\0'; // Make it empty so that if we do not get from keyword, it is empty string

  num = sscanf(buf, "%s %s %s %s", key, mname, host, pgm_path);
  if (num < 3 )
  {
    NS_EXIT(-1, "monitor_config(): Need atleast TWO fields after key MONITOR\n");
  }
  if(!strcmp (mname, "nc_mon"))
  {
    num = sscanf(buf, "%s %s %s %s %s %s %s %s %s", key, mname, host, tunnel1, tunnel2, tunnel3, tunnel4, tunnel5, tunnel6);
    if (num < 4)
    {
      NS_EXIT(1, "Atleast one tunnel name is required\n");
    }

    if ( num > 8)
    {
      NS_EXIT(1, "There are morethen five tunnels are given.\n");
    }
    pgm_path[0] = '\0'; // Make it empty. Presently assuming no path with nc_mon
    sprintf(cmd_args, "%s %s %s %s %s %s", tunnel1, tunnel2, tunnel3, tunnel4, tunnel5, tunnel6);
    total_tunnels = num - 3;
  }
  add_monitor(mname, host, pgm_path, -1, cmd_args);
}

void add_monitor(char *mname, char *host, char *pgm_path, int send_data_point_idx, char *cmd_args)
{
  int mon_row = 0;
  int mon_id, dbuf_len; //num;
  int server_index;
  ServerCptr server_mon_ptr;

  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < num_mon_type_entries; mon_id++)
  {
    if (strcmp (mname, monTypeTable[mon_id].monitor_name) == 0)
      break;
  }
  if (mon_id == num_mon_type_entries)
  {
    fprintf(stderr, "monitor_config(): unknown monitor (%s) ignorning MONITOR entry\n", mname);
    return;
  }

  /* Validate server entry in server.dat */
  //if(search_in_server_list(host, &server_mon_ptr, NULL, NULL, global_settings->hierarchical_view_vector_separator, 0, &server_index) == -1) 
  
  {
    NS_EXIT(-1, "Server (%s) not present in topolgy, for monitor(%s) .\n", host, pgm_path);
  }

  Create_montable_entry(&mon_row);
  monTable[mon_row].con_type = NS_STRUCT_S_MON;
  monTable[mon_row].monitor_id = mon_id;
  MY_MALLOC(monTable[mon_row].server, strlen(host) + 1, "montable server", mon_row);
  strcpy(monTable[mon_row].server, host);
  MY_MALLOC(monTable[mon_row].data, sizeof(Long_data) * monTypeTable[mon_id].num_data, "montable data", mon_row);
  //dbuf_len = 16 * monTypeTable[mon_id].num_data;

  //We set dbuf_len to a big size because there may come a big error message from custom monitor
  dbuf_len = MAX_SMON_MSG_SIZE;
  MY_MALLOC(monTable[mon_row].data_buf, dbuf_len, "montable data buf", mon_row);
  monTable[mon_row].dindex = 0;
  monTable[mon_row].idx_in_send_datapoint = send_data_point_idx;
  monTable[mon_row].exec_option = monTypeTable[mon_id].exec_option;

  //pgm_path overridden
  if (pgm_path[0] != '\0')
  {
    MY_MALLOC(monTable[mon_row].pgm_path, strlen(pgm_path) + 1, "montable pgm_path", mon_row);
    strcpy(monTable[mon_row].pgm_path, pgm_path);
  }
  else
  {
    monTable[mon_row].pgm_path = monTypeTable[mon_id].pgm_path;
  }
  monTable[mon_row].is_data_filled = 0;
  strcpy(monTable[mon_row].cmd_args, cmd_args);
  monTable[mon_row].idx_in_send_datapoint = monTypeTable[mon_id].mon_count++; //Anuj: 18/12/07
}

// This method will give the total no of monitors and put it's id in mon_id //Anuj: 18/12/07
int get_mon_count(char *mname, int *mon_id)
{
  int id;
  NSDL2_MON(NULL, NULL, "Method called");
  for (id = 0; id < num_mon_type_entries; id++)
  {
    if (strcmp (mname, monTypeTable[id].monitor_name) == 0)
    {
      *mon_id = id;
      return (monTypeTable[id].mon_count);
    }
  }
  fprintf(stderr, "Error: get_mon_count() - No monitor has been found for '%s' monitor\n", mname);
  return -1;
}

// This method will give the total no of monitors //Anuj: 18/12/07
char *get_mon_server_name(int mon_id, int *index)
{
  int start_index;

  NSDL2_MON(NULL, NULL, "Method called, mon_id = %d", mon_id);
  if(*index == -1)
    start_index = 0;
  else
    start_index = *index;

  for ( ; start_index < total_montable_entries; start_index++)
  {
    if (mon_id == monTable[start_index].monitor_id)
    {
      *index = start_index + 1;
      return (monTable[start_index].server);
    }
  }
  fprintf(stderr, "Error : get_mon_server_name() - No Monitor name has been found for mon_id = %d\n", mon_id);
  return NULL;
}

// To be used for reading keywords in scenario file
// buf may contain a pattern of the format NO_MONITOR monitor-name ...
// Simpler form of MONITOR keyword
// Make sure function does not modify buf.If the first field of the buf is not MONITOR, function returns
// immediately with 0. Othewise does more processing as described below
// and returns 1 after processing.
void smonitor_config(char *keyword, char *buf)
{
  char *mname, mon_entry[1024];
  //int mon_row = 0;
  //int mon_id, dbuf_len;

  NSDL2_MON(NULL, NULL, "Method called");
  strncpy (mon_entry, buf, 1024);
  mon_entry[1023] = '\0';

  mname = strtok (mon_entry, " ");

  if (!mname || strcasecmp(mname, "NETOCEAN_MONITOR") != 0) return;

  while ((mname = strtok (NULL, " ")))
  {
    add_monitor(mname, g_cavinfo.SRAdminIP, "", -1, "");

/****
    for (mon_id = 0; mon_id < num_mon_type_entries; mon_id++)
    {
      if (strcmp (mname, monTypeTable[mon_id].monitor_name) == 0)
        break;
    }
    if (mon_id == num_mon_type_entries)
    {
      fprintf(stderr, "monitor_config(): unknown monitor (%s) ignorning MONITOR entry\n", mname);
      return;
    }

    Create_montable_entry(&mon_row);
    monTable[mon_row].monitor_id = mon_id;
    monTable[mon_row].server = g_cavinfo.SRAdminIP;
    monTable[mon_row].data = (int *)Malloc ((sizeof(int) * monTypeTable[mon_id].num_data), "montable data");
    dbuf_len = 16 * monTypeTable[mon_id].num_data;
    monTable[mon_row].data_buf = (char *)Malloc ((dbuf_len), "montable data buf");
    monTable[mon_row].dindex = 0;
    monTable[mon_row].pgm_path = monTypeTable[mon_id].pgm_path;
    monTable[mon_row].is_data_filled = 0;
***/

  }
}

//make all connections at a time. As connect is time consuming task, once all connection is ready send msg will be faster
static void make_connections(int frequency)
{
  int mon_id;
  MonTable *mptr = monTable;
  char err_msg[1024]="\0";

  NSDL2_MON(NULL, NULL, "Method called, frequency = %d", frequency);

  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_GENERAL, EVENT_INFO,
			       "Making connection to server (%s).", mptr->server);
    if ((mptr->fd = nslb_tcp_client_ex(mptr->server, 7891, 10, err_msg)) < 0)
    {
      ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR,
				 "%s"
				 " Data will not be available for this monitor.",
				 err_msg);
      continue;
    }
    //making non-blocking
    if (fcntl(mptr->fd, F_SETFL, O_NONBLOCK) < 0)
    {
      ns_monitor_log(EL_CDF, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR,
						 "Error in making connection non blocking.");
      NS_EXIT(-1, "Error in making connection non blocking.");
      //return -1;
    }

    else
      ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_GENERAL, EVENT_INFO,
				 "Connection established to server (%s).", mptr->server);
  }
}

void add_select_monitor()
{
  int mon_id;
  MonTable *mptr = monTable;

  NSDL2_MON(NULL, NULL, "Method called");

  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    NSDL2_MON(NULL, NULL, "mptr->pgm_path = %s fd = %d", mptr->pgm_path, mptr->fd);

    if(mptr->fd >0)
      add_select_msg_com_con((char *)mptr, mptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  }
}

static void send_msg_to_create_server(int frequency)
{
  int mon_id;
  MonTable *mptr = monTable;

  NSDL2_MON(NULL, NULL, "Method called, frequency = %d", frequency);

  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    char SendMsg[2048]="\0";
    NSDL2_MON(NULL, NULL, "mptr->pgm_path = %s fd = %d", mptr->pgm_path, mptr->fd);

    //send msg if connection is there
    if(mptr->fd >0)
    {
       sprintf(SendMsg,"init_monitor:MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_WDIR=%s\n", mptr->pgm_path, mptr->cmd_args, mptr->exec_option, frequency, testidx, g_cavinfo.NSAdminIP, mptr->server, g_ns_wdir);

      //NSDL3_MON("Sending message to Server(%s). Sent msg buffer is %s", mptr->server, SendMsg);
      ns_monitor_log(EL_D, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_GENERAL, EVENT_INFO,
					      "Sending message. Sent msg buffer=%s", SendMsg);
      if (send(mptr->fd, SendMsg, strlen(SendMsg), 0)!=strlen(SendMsg))
      {
        //sending msg failed for this monitor, close fd & send for nxt monitor
        ns_monitor_log(EL_D, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR,
						"Send failure.");
        close(monTable[mon_id].fd);
        monTable[mon_id].fd = -1;
        continue;
      }
      //add_select_msg_com_con((char *)mptr, mptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);
    }
  }
}

//Called by NetStorm parent for setting up monitors
//Iterate over whole monTable, Create TCP client connection to servers for each row
//Fill up fd in each row  Send init message to server
//Format of this message is same as told in task2 For any error just output a message and return
void monitor_setup(int frequency)
{
  NSDL2_MON(NULL, NULL, "Method called");

  //make all connection first
  make_connections(frequency);
  //now send msg to createserver
  send_msg_to_create_server(frequency);
}

static inline void close_monitor_connection(MonTable *mptr)
{
  NSDL2_MON(NULL, NULL, "Method called. CustomMonitor => %s", cm_to_str(mptr));
  if(mptr->fd >= 0)
  {
    close(mptr->fd);
    mptr->fd = -1;
  }
}

//Sets all fd's in monTable in the rfd fd_set.Returns max_fd of heighest fd added. starting max_fd is input arg
#if 0
inline int set_monitor_fd (fd_set * rfd, int max_fd)
{
  int mon_id;
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++)
  {
    if(monTable[mon_id].fd > 0)
    {
      //printf("setting fd = %d for mon=%d\n", monTable[mon_id].fd, mon_id);
      FD_SET (monTable[mon_id].fd,rfd);
      if(monTable[mon_id].fd > max_fd)  max_fd = monTable[mon_id].fd;
    }
  }
  return max_fd;
}
#endif

//Check if any of the fd in monTable is set in the rfd set.
//If set, receive the message. This message will be
//the data message as explained task2.  Read comple message
//till newline character.
//convert all data elements into numbers and store in the
//data array in monTable and set is_data_filled flag for the
//corresponding row
//void handle_if_monitor_fd(fd_set *rfd)
int handle_if_monitor_fd(void *ptr)
{
  MonTable *mptr = (MonTable *) ptr;

  NSDL2_MON(NULL, NULL, "mptr->fd =  %d", mptr->fd);
  //printf("monid=%d and fd=%d\n", mon_id, mptr->fd);
  if (receive(mptr) == FAILURE)
  {
    //printf("closing con %d\n", mptr->fd);
    ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR,
    			   "Receive failure.");
    remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, mptr->fd);
    close_monitor_connection(mptr);
    mptr->is_data_filled = 0;
  }
  return 1;
}

//Iterate over all monTable and
//call, print function of monTypeTable,
//if is_data_filled flag is set.
//Unset is_data_filled flag after calling print function
void print_if_monitor_data(FILE* fp1, FILE*fp2)
{
  int mon_id;
  MonTable *mptr = monTable;
  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if (mptr->is_data_filled)
    {
      monTypeTable[mptr->monitor_id].print_monito_func(fp1, fp2, mptr);
      mptr->is_data_filled = 0;
    }
  }
}

void fill_if_monitor_data()                    // this fn will all the 4 monitor groups : Anuj
{
  int mon_id;
  MonTable *mptr = monTable;
  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if (mptr->is_data_filled)
    {
      monTypeTable[mptr->monitor_id].fill_monito_func(mptr);
    }
  }
}

// Fill NetOcean System Stats
static void fill_no_system_stats_gp(MonTable *mptr)
{
  int i;
  int group_vector_idx =  mptr->idx_in_send_datapoint;
  NO_system_stats_gp *no_system_stats_gp_local_ptr = no_system_stats_gp_ptr + group_vector_idx;
  Long_data *ptr = (Long_data *)no_system_stats_gp_local_ptr;
  
  NSDL2_MON(NULL, NULL, "Method called");
  // Anuj - Verification of all indexes : verified 20/09/07
  for (i = 0; i < monTypeTable[mptr->monitor_id].num_data; i++) {
    GDF_COPY_VECTOR_DATA(no_system_stats_gp_idx, i, group_vector_idx, 0, mptr->data[i], *ptr); ptr++;
  }
}

// Change for GDF : Anuj 13/09/07,
// Fill System Stats for Windows Servers
static void fill_window_server_stats_gp (MonTable *mptr)
{
  Server_stats_gp *server_stats_gp_local_ptr = server_stats_gp_ptr + mptr->idx_in_send_datapoint;

  NSDL2_MON(NULL, NULL, "Method called");
  server_stats_gp_local_ptr->cpuUser        = (mptr->data[0]);
  server_stats_gp_local_ptr->cpuSys         = (mptr->data[1]);
  server_stats_gp_local_ptr->cpuTotalBusy   = (mptr->data[2]); // 25/05/2007- Achint cpuIdle is changed to cpuTotalBusy
  //server_stats_gp_local_ptr->cpuNice      = htonl(mptr->data[3]);

  server_stats_gp_local_ptr->pageIn         = (mptr->data[4]);
  server_stats_gp_local_ptr->pageOut        = (mptr->data[5]);

  server_stats_gp_local_ptr->swapIn         = (mptr->data[6]);
  server_stats_gp_local_ptr->swapOut        = (mptr->data[7]);

  server_stats_gp_local_ptr->diskIn         = (mptr->data[8]);
  server_stats_gp_local_ptr->diskOut        = (mptr->data[9]);

  server_stats_gp_local_ptr->interrupts     = (mptr->data[10]);
  //server_stats_gp_local_ptr->freeMem      = htonl(mptr->data[11]);

  server_stats_gp_local_ptr->loadAvg1m      = (mptr->data[12]);
  server_stats_gp_local_ptr->loadAvg5m      = (mptr->data[13]);
  server_stats_gp_local_ptr->loadAvg15m     = (mptr->data[14]);

  //server_stats_gp_local_ptr->ActiveOpens  = htonl(mptr->data[15]);
  //server_stats_gp_local_ptr->PassiveOpens = htonl(mptr->data[16]);
  //server_stats_gp_local_ptr->AttemptFails = htonl(mptr->data[17]);
  //server_stats_gp_local_ptr->EstabResets  = htonl(mptr->data[18]);
  //server_stats_gp_local_ptr->CurrEstab    = htonl(mptr->data[19]);

  server_stats_gp_local_ptr->InSegs         = (mptr->data[20]);
  server_stats_gp_local_ptr->OutSegs        = (mptr->data[21]);

  //server_stats_gp_local_ptr->RetransSegs  = htonl(mptr->data[22]);
  server_stats_gp_local_ptr->InErrs         = (mptr->data[23]);
  server_stats_gp_local_ptr->OutRsts        = (mptr->data[24]);
  server_stats_gp_local_ptr->collisions     = (mptr->data[25]);

  server_stats_gp_local_ptr->v_swtch        = (mptr->data[26]);
}

int do_summary_min_max (int nelements, SummaryMinMax *data)
{
  FILE *fp;
  int i; //j=0;
  char buf[128];
  Msg_data_hdr *msg_hdr_ptr = (Msg_data_hdr *) msg_data_ptr;
  char *ptr;
  Long_data value;

  NSDL2_MON(NULL, NULL, "Method called");
  for (i = 0; i < nelements; i++)
  {
    data[i].cum_sum = 0;
    data[i].num_samples = 0;
    data[i].avg = 0;
    data[i].max = 0;
    data[i].min = 0xFFFFFFFF;
  }
  // argument is NULL so that fflush flushes all open output streams
  fflush(NULL); // Moved from bottom to top. Check with Anil if it is OK
  sprintf (buf, "%s/logs/TR%d/rtgMessage.dat", g_ns_wdir, testidx);
  fp = fopen(buf, "r");
  if (!fp) return -1;

  //printf ("reading rtgMessage\n");
  while  (fread (msg_data_ptr, msg_data_size, 1, fp) == 1)
  {
    if ((msg_hdr_ptr->opcode) != 1) continue;
    for (i = 0; i < nelements; i++)
    {
      data[i].num_samples++;
      //ptr = (char *)local_buff_ptr + data[i].offset;
      //ptr = (char *)&dp + data[i].offset;
      ptr = msg_data_ptr + data[i].offset;
      if(ptr == NULL)
      {
        NS_EXIT(-1, "Error: ptr is NULL");
      }

      value = *((Long_data *)ptr);
      //      value = ntohl(value);
      data[i].cum_sum += value; // Can this overflow??
      if (data[i].min > value) data[i].min = value;
      if (data[i].max < value) data[i].max = value;
    /*if (i == 1 )
        printf ("CS sample=%llu value = %d\n", data[i].num_samples, value);
      if (i == 2 )
        printf ("Int sample=%llu value = %d\n", data[i].num_samples, value);*/
    }
  }
  for (i = 0; i < nelements; i++)
  {
    if (data[i].num_samples == 0)
      data[i].avg = 0;
    else
      data[i].avg = data[i].cum_sum / data[i].num_samples;
  }
  return 0;
}

//static inline void fill_server_stats_gp (MonTable *mptr)      // Server Stats for Window (perfmon)

// Abhishek 9/10/2006 - Add this function to print data in data_point
// Start
void print_server_stat(FILE * fp1, FILE* fp2, int num_servers)
{
  int i;
  Server_stats_gp *server_stats_gp_local_ptr = server_stats_gp_ptr;

  NSDL2_MON(NULL, NULL, "Method called");
  for(i = 0; i < num_servers; i++)
  {
    fprint2f(fp1, fp2,"    Server Stats - %s: ", server_stat_ip[i]);
    fprint2f(fp1, fp2,"Intr/s=%0.0f, Cswitch/s=%0.0f, LoadAvg=%6.2f %6.2f %6.2f\n",
    ( server_stats_gp_local_ptr->interrupts),
    ( server_stats_gp_local_ptr->v_swtch),
    (double)((double)( server_stats_gp_local_ptr->loadAvg1m)/100.0),
    (double)((double)( server_stats_gp_local_ptr->loadAvg5m)/100.0),
    (double)((double)(server_stats_gp_local_ptr->loadAvg15m)/100.0));
    fprint2f(fp1, fp2,"      CPU: User=%6.2f System=%6.2f Busy=%6.2f, Paging/s: In=%0.0f Out=%0.0f, Swapping/s: In=%0.0f Out=%0.0f\n",
    (double)((double)(server_stats_gp_local_ptr->cpuUser)/100.0),
    (double)((double)(server_stats_gp_local_ptr->cpuSys)/100.0),
    (double)((double)(server_stats_gp_local_ptr->cpuTotalBusy)/100.0),
    ( server_stats_gp_local_ptr->pageIn),
    ( server_stats_gp_local_ptr->pageOut),
    ( server_stats_gp_local_ptr->swapIn),
    ( server_stats_gp_local_ptr->swapOut));

    fprint2f(fp1, fp2,"      Ethernet Pkts/s: In=%0.0f InErrs=%0.0f OutErrs=%0.0f Collisions=%0.0f Out=%0.0f\n",
    (server_stats_gp_local_ptr->InSegs),
    (server_stats_gp_local_ptr->InErrs),
    (server_stats_gp_local_ptr->OutRsts),
    (server_stats_gp_local_ptr->collisions),
    (server_stats_gp_local_ptr->OutSegs));

    server_stats_gp_local_ptr++;
  }
}
// End

static void print_no_system_stats_gp(FILE * fp1, FILE* fp2, MonTable *mptr)
{
  //int i=0, length = monTypeTable[mptr->monitor_id].num_data;

  NSDL2_MON(NULL, NULL, "Method called");
  fprint2f(fp1, fp2, "    Server System Stats - %s: CPU: User=%'6.2f Sys=%'6.2f Busy=%'6.2f, Paging/s: In=%'d Out=%'d, Swap (KB/s): In=%'d Out=%'d\n", mptr->server, (double)((double)mptr->data[0]/100.0), (double)((double)mptr->data[1]/100.0), (double)((double)mptr->data[2]/100.0), mptr->data[3], mptr->data[4], mptr->data[5], mptr->data[6]);
  fprint2f(fp1, fp2, "        Ctxt/s=%'0.0f, Free Mem(KB)=%'0.0f, Disk: In=%'0.0f Out=%'0.0f, Load Avg: 1Min=%'6.2f 5Min=%'6.2f 15Min=%'6.2f\n", mptr->data[7], mptr->data[8], mptr->data[9], mptr->data[10], (double)((double)mptr->data[11]/100.0), (double)((double)mptr->data[12]/100.0), (double)((double)mptr->data[13]/100.0));
  /*while(i<length)
  {
    //printf("%s %s %d\n",a,b,data[i]);
    printf("%d ",mptr->data[i]);
    i++;
  }
  printf("\n"); */
}

// Function for filliling the data in the structure of Tunnel_stats_gp
static void fill_tunnel_stats_gp (MonTable *mptr)
{
  if(tunnel_stats_gp_ptr == NULL) return;

  NSDL2_MON(NULL, NULL, "Method called");
  int i;
  int j = 0;
  Tunnel_stats_gp *tunnel_stats_gp_local_ptr = tunnel_stats_gp_ptr;
  for (i = 0; i < total_tunnels; i++)
  {
    tunnel_stats_gp_local_ptr->send_throughput = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->recv_throughput = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->send_pps        = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->recv_pps        = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->send_pps_drop   = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->recv_pps_drop   = (mptr->data[j++]);
    tunnel_stats_gp_local_ptr->send_latency    = (mptr->data[j++]);
    //tunnel_stats_gp_local_ptr->recv_latency    = htonl(mptr->data[j++]);
    j++;                        // incremented the 'j' since we are getting 8 data for tunnels but 1 data is commented,
                                // so when uncomment the commented data, then comment the 'j++' :Anuj 03/10/07
    tunnel_stats_gp_local_ptr++;
  }
}

// Print Tunnel Stats in progress and console
static void print_tunnel_stats_gp(FILE * fp1, FILE* fp2, MonTable *mptr)
{
  int i = 0;
  char buff[1024] = "";
  char *tunnel;

  NSDL2_MON(NULL, NULL, "Method called");
  strcpy(buff, mptr->cmd_args);
  tunnel = strtok(buff, " ");
  for(i = 0; i < total_tunnels; i++)
  {
    fprint2f(fp1, fp2, "    NetChannel (%s) Tunnel: Throughput (Kbps) (Tx/Rx)=%6.3f/%6.3f, Packets/Sec (Tx/Rx)=%6.3f/%6.3f, Packets Drop/Sec (Tx/Rx)=%6.3f/%6.3f Latency (Tx/Rx) = %6.3f/%6.3f\n",
    tunnel,
    (double)((double)mptr->data[i*8 + 0]/1024),
    (double)((double)mptr->data[i*8 + 1]/1024),
    (double)((double)mptr->data[i*8 + 2]/100.0),
    (double)((double)mptr->data[i*8 + 3]/100.0),
    (double)((double)mptr->data[i*8 + 4]/100.0),
    (double)((double)mptr->data[i*8 + 5]/100.0),
    (double)((double)mptr->data[i*8 + 6]/100.0),
    (double)((double)mptr->data[i*8 + 7]/100.0));
    tunnel = strtok(NULL, " ");
  }
}

// Chnaged by Anuj for GDF
// NetOcean TCP (Network) Stats
static inline void fill_no_network_stats_gp (MonTable *mptr)
{
  NSDL2_MON(NULL, NULL, "Method called");
  int group_vector_idx =  mptr->idx_in_send_datapoint;
  NO_network_stats_gp *no_network_stats_gp_local_ptr = no_network_stats_gp_ptr + group_vector_idx;
  int i;
  Long_data *ptr = (Long_data *)no_network_stats_gp_local_ptr;

  for (i = 0; i < monTypeTable[mptr->monitor_id].num_data; i++) {
    GDF_COPY_VECTOR_DATA(no_network_stats_gp_idx, i, group_vector_idx, 0, mptr->data[i], *ptr); ptr++;
  }
}

static void print_no_network_stats_gp(FILE* fp1, FILE* fp2, MonTable *mptr)
{
  //int i=0, length = monTypeTable[mptr->monitor_id].num_data;
  NSDL2_MON(NULL, NULL, "Method called");

  fprint2f(fp1, fp2, "    Server Network Stats (%s) (per sec): ActiveOpens=%'0.0f PassiveOpens=%'0.0f AttemptFails=%'0.0f EstabResets=%'0.0f\n", mptr->server, mptr->data[0], mptr->data[1], mptr->data[2], mptr->data[3]);
  fprint2f(fp1, fp2, "        CurrEstab (Total)=%'0.0f InSegs=%'0.0f OutSegs=%'0.0f RetransSegs=%'0.0f InErrs=%'0.0f OutRsts=%'0.0f\n", mptr->data[4], mptr->data[5], mptr->data[6], mptr->data[7], mptr->data[8], mptr->data[9]);
  /*while(i<length)
  {
    //printf("%s %s %d\n",a,b,data[i]);
    printf("%d ",mptr->data[i]);
    i++;
  }
  printf("\n"); */
}

static void print_window_perfmon (FILE* fp1, FILE* fp2, MonTable *mptr)
{
  printf("Only for testing, Will not show any data\n");
}

//retun SUCCESS if all OK else returns FAILURE
static int receive(MonTable *mptr)
{
  char *RecvMsg;
  //fd_set fdRcv;
  //struct timeval timeout;
  int  maxlen;
  int lengthrecv;
  //char *buffer = NULL;

  //maxlen = (monTypeTable[mptr->monitor_id].num_data * 16) - mptr->dindex;
  maxlen = MAX_SMON_MSG_SIZE - mptr->dindex;
  RecvMsg = mptr->data_buf + mptr->dindex;

  NSDL2_MON(NULL, NULL, "Method called. Before receive data: %s", cm_to_str(mptr));
  while(1)
  {
    lengthrecv = recv(mptr->fd, RecvMsg, maxlen, 0);
    if(lengthrecv <= 0)
    {
      if (errno == EAGAIN)
        return SUCCESS;
      else
        return FAILURE;
    }
    else
    {
      RecvMsg[lengthrecv] = 0;
      mptr->dindex += lengthrecv;

#ifdef NS_DEBUG_ON
      if(group_default_settings->module_mask & MM_MON)
        ns_monitor_log(EL_D, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_GENERAL, EVENT_INFO,
						"Received data (%s).", RecvMsg);
#endif
      return (checkfornewline(mptr));
    }
  }
}

static int checkfornewline(MonTable *mptr)
{
  int i = 0, j = 0, done = 0;
  int length = mptr->dindex;
  char *RecvMsg = mptr->data_buf;

  NSDL2_MON(NULL, NULL, "Method called");
  while(i <  length)
  {
    if(RecvMsg[i] == '\n')
    {
      RecvMsg[i] = '\0';
      if (i != 0)
      {
        //printf ("filling data (%s)\n", RecvMsg);
        filldata (mptr);
        done = 1;
      }
      i++;
      while (i+j < length)
      {
        mptr->data_buf[j] = mptr->data_buf[i+j];
        j++;
      }
      length = mptr->dindex = j;
      if (!done)
      {
        i = 0; j= 0;
        continue;
      }
      else
      {
        return SUCCESS;
      }
    }
    i++;
  }
  if (length == (monTypeTable[mptr->monitor_id].num_data * 16))
  {
    ns_monitor_log(EL_F, 0, 0, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_INV_DATA, EVENT_WARNING,
			       "Server (%s) Monitor (%s) got much Longer data than expected (%d).",
				mptr->server, monTypeTable[mptr->monitor_id].monitor_name, length);
    return FAILURE;
  }
  return SUCCESS;
}

static int filldata(MonTable *mptr)
{
  int i = 0;
  int  max = monTypeTable[mptr->monitor_id].num_data;
  Long_data *data = mptr->data;
  char *ptr;
  char *buffer = mptr->data_buf;

  NSDL2_MON(NULL, NULL, "Method called. Buffer = %s", buffer);

  //validates data 
  if(validate_data(mptr))
    return SUCCESS;
  ptr = strtok(buffer, " ");
  while(ptr)
  {
    if (i >= max)
    {
/*      fprintf(stderr, "Warning: Server (%s) Monitor (%s) got more than expected (%d) data elements\n",
      mptr->server, monTypeTable[mptr->monitor_id].monitor_name, max); 
*/
      ns_monitor_log(EL_DF, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_INV_DATA, EVENT_WARNING,
					       "Got (%d) more than expected (%d) data elements",
      i, max);
      break;
    }
    data[i++] = atof(ptr);
    ptr = strtok(NULL, " ");
  }
  if (i < max)
  {
/*    fprintf(stderr, "Warning: Server (%s) Monitor (%s) got less (%d) than expected (%d) data elements\n",
    mptr->server, monTypeTable[mptr->monitor_id].monitor_name, i, max);
*/
    ns_monitor_log(EL_DF, DM_METHOD, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_INV_DATA, EVENT_WARNING,
					     "Got (%d) less than expected (%d) data elements",
    i, max);
  }
  else
  {
    mptr->is_data_filled = 1;
  }
  return 0;
}

//This method is to stop connections of all MONITOR, NETOCEAN_MONITOR at end of test run.
//To stop connection of monitor send 'end_monitor' message to create server(cav mon server)
inline void stop_all_monitors()
{
  int mon_id;
  MonTable *mptr = monTable;
  char *buffer="end_monitor\n";

  NSDL2_MON(NULL, NULL, "Method called");

  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if(mptr->fd != -1)
    {
      if (send(mptr->fd, buffer, strlen(buffer), 0) != strlen(buffer))
        ns_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, mptr->server, mptr->pgm_path, mptr->fd, EID_DATAMON_ERROR, EVENT_MAJOR,
					"Error in sending end_monitor message for monitor to cav mon server '%s'",
					 mptr->server);

      close_monitor_connection(mptr);
    }
  }
}

//added by Atul - for rtg changes
int is_no_tcp_present()
{
  int mon_id;
  MonTable *mptr = monTable;
  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if (strcmp ("tcp", monTypeTable[mptr->monitor_id].monitor_name) == 0)
      return 1;
  }
  return 0;
}

int is_no_linux_present()
{
  int mon_id;
  MonTable *mptr = monTable;
  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if (strcmp ("linux", monTypeTable[mptr->monitor_id].monitor_name) == 0)
      return 1;
  }
  return 0;
}

char *get_tunnels()
{
  int mon_id;
  MonTable *mptr = monTable;
  NSDL2_MON(NULL, NULL, "Method called");
  for (mon_id = 0; mon_id < total_montable_entries; mon_id++, mptr++)
  {
    if (strcmp ("nc_mon", monTypeTable[mptr->monitor_id].monitor_name) == 0)
      return mptr->cmd_args;
  }
  return NULL;
}
