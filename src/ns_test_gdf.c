#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h> /* not needed on IRIX */
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_error_codes.h"
#include "util.h"
#include "ns_test_gdf.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "ns_alloc.h"

int no_tcp_monitor = 0;
int no_linux_monitor = 0;

int total_tunnels = 0;
char tunnelNames[1000] = ""; // Comma separated names of tunnels from argument

int total_tx_entries = 0;
TxTableEntry_Shr *tx_table_shr_mem =  NULL;

int no_of_host = 0;
char *server_stat_ip[100];

int testidx = 5500; // Test Run Number
char g_test_start_time[32] = "01/01/2007 12:00:00";
char g_ns_wdir[255];

Globals globals;

void htonll(long long in, unsigned long *out)
{
  long *l_long;
  long *u_long;
  l_long = (long *)&in;
  u_long = (long *)((char *)&in + 4);
  *out = htonl(*l_long);
  *(out + 1) = htonl(*u_long);
}

int is_no_tcp_present()
{
  return no_tcp_monitor;
}

int is_no_linux_present()
{
  return no_linux_monitor;
}

static void set_tunnels(char *buff)
{
  char *buffer[MAX_LINE_LENGTH];
  int j = 0;

  //strcpy(tunnelNames, buff);
  for ( buffer[j] = strtok(buff, ","); buffer[j] != NULL; buffer[j] = strtok(NULL, ",") )
  {
    strcat(tunnelNames, buffer[j]);
    strcat(tunnelNames, " ");
    j++;
  }
  total_tunnels = j;
}

char *get_tunnels()
{
  printf("tunnelNames = %s\n", tunnelNames);
  return(tunnelNames);
}

void set_server_stats(char *buff[])
{
//   server_stat_ip = malloc(sizeof(char *) * no_of_host);
   int j = 0;
   for(j = 0; j < no_of_host; j++)
   {
//     printf("name2 = %s , length = %d,no_of_host =%d \n ", buff[j], strlen(buff[j]), no_of_host);
     MY_MALLOC(server_stat_ip[j], strlen(buff[j]) + 1, "server_stat_ip[j]", j);
     strcpy(server_stat_ip[j], buff[j]);
     printf("name3 = %s\n", server_stat_ip[j]);
   }
}

void set_trans(char *buff[])
{
  int i;
  MY_MALLOC(tx_table_shr_mem, sizeof(TxTableEntry_Shr) * total_tx_entries, "tx_table_shr_mem", -1);

  for ( i = 0; i < total_tx_entries; i++)
  {
    MY_MALLOC(tx_table_shr_mem[i].name, strlen(buff[i]), "tx_table_shr_mem[i].name", i);
    strcpy(tx_table_shr_mem[i].name, buff[i]);
    printf("Transname = %s\n", tx_table_shr_mem[i].name);
  }
}

void read_custom_file(char *filename)
{

  char buffer[4096] = "";
  char file[1024]= "";
  FILE *fp;

  printf("filename = %s\n", filename);
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scenarios dir*/
  sprintf(file, "%s/%s/%s/%s/%s", GET_NS_TA_DIR(),  g_project_name, g_subproject_name, "scripts", filename);
  printf("file = %s\n", file);

  buffer[0] = '\0';

  fp = fopen(file, "r+");

  if(!fp)
  {
    printf("enable to open file %s\n", file);
    exit(-1);
  }

  while(fgets(buffer, 4096, fp) != NULL )
  {
    if(strncmp(buffer, "CUSTOM_MONITOR", strlen("CUSTOM_MONITOR")) == 0)
    {
      custom_config("CUSTOM_MONITOR", buffer, NULL, 0, NORMAL_CM_TABLE, NULL, NULL, 0, -1, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0);
    }
  }
  if(fp)
    fclose(fp);
}


int main(int argc, char *argv[])
{
  int i,j = 0;

  char filename[MAX_LINE_LENGTH];
  char *buffer[100];
  char *temparg;

  if(argc < 2)
  {
    printf("Usage: ./ns_gdf <input GDF filename> [TestRun=<testRunNum>] [CUSTOM=<filename>] [NO=tcp] [NO=linux] [Tunnels=<tunnelname1>,<tunnelname2>,.] [SS=<server1>,....] [Trans=<t1>,<t2>,...]\n");
    exit(-1);
  }

  globals.debug = 1;

  if (getenv("NS_WDIR") != NULL)
    strcpy(g_ns_wdir, getenv("NS_WDIR"));
  else
    strcpy(g_ns_wdir, "/home/cavisson/work");

  /*bug id: 101320: ToDo: TBD with DJA*/
  nslb_set_ta_dir(g_ns_wdir);
  strcpy(filename, argv[1]);
  memset(buffer, 0, sizeof(buffer));

  for (i = 2; i < argc; i++)
  {
    MY_MALLOC(temparg, sizeof(argv[i]), "temparg", i);
    strcpy(temparg, argv[i]);

    if(!strncmp(temparg, "CUSTOM=", strlen("CUSTOM=")))
    {
      temparg += strlen("CUSTOM=");
      read_custom_file(temparg);
    }

    if(!strncmp(temparg, "TestRun=", strlen("TestRun=")))
    {
      temparg += strlen("TestRun=");
      testidx = atoi(temparg);
    }

    if(!strncmp(temparg, "NO=", strlen("NO=")))
    {
      temparg += strlen("NO=");
      if(! strncmp(buffer[1], "tcp", strlen("tcp")))
        no_tcp_monitor = 1;
      if(! strncmp(buffer[1], "linux", strlen("linux")))
        no_linux_monitor = 1;
    }
    if(!strncmp(temparg, "Tunnels=", strlen("Tunnels=")))
    {
      temparg += strlen("Tunnels=");
      set_tunnels(temparg);
    }
    if(!strncmp(temparg, "SS=", strlen("SS=")))
    {
      temparg += strlen("SS=");
      printf("temparg = %s\n", temparg);
      no_of_host = get_tokens(temparg, buffer, "," );
     for(j = 0; j < no_of_host; j++)
     {
       printf("name1 = %s\n", buffer[j]);

     }

      set_server_stats(buffer);
    }
    if(!strncmp(temparg, "Trans=", strlen("Trans=")))
    {
      temparg += strlen("Trans=");
      total_tx_entries = get_tokens(temparg , buffer, "," );
      set_trans(buffer);
    }
  }

  globals.progress_secs = 1000;
  create_tmp_gdf();
  process_gdf(filename);

  process_custom_gdf();
  close_gdf(write_gdf_fp);
  allocMsgBuffer();
  create_testrun_gdf(0);

  return 0;
}
