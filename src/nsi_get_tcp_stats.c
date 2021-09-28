/***************************************************************************************************************************
 * Name                   : ns_run_phases.c
 * Purpose                : Shows data fro TCP STATS
 * Usage                  : nsi_get_tcp_stats [-c|-s] [-r] [--all|--cur|--trans]
 * Author                 : Arun Nishad
 * Intial version date    : Saturday, November 12 2008 
 * Last modification date : 
 * O/P Looks Like         : 
 
From-StateName   From-StateId    To-StateName   To-StateId   TransCount
Established         1              FinWait1            4         0
Established         1              CloseWait           8         0
Established         1              Closed              7         0
SynSent             2              SynRcvd             3         0
SynSent             2              Closed              7         0
SynSent             2              Established         1         0
SynRcvd             3              Closed              7         0
SynRcvd             3              FinWait1            4         0
SynRcvd             3              Established         1         0
FinWait1            4              FinWait2            5         0
FinWait1            4              Closing             11        0
FinWait1            4              TimeWait            6         0
FinWait1            4              Closed              7         0
FinWait2            5              TimeWait            6         0
TimeWait            6              Closed              7         0
Closed              7              Listen              10        0
Closed              7              SynSent             2         0
CloseWait           8              LastAck             9         0
CloseWait           8              Closed              7         0
LastAck             9              Closed              7         0
Listen              10             SynRcvd             3         0
Listen              10             SynSent             2         0
Closing             11             TimeWait            6         0



  TcpStateName   StateId   CurCount
Established          1          0
SynSent              2          1
SynRcvd              3          0
FinWait1             4          0
FinWait2             5          0
TimeWait             6          0
Closed               7          0
CloseWait            8          0
LastAck              9          0
Listen               10         0
Closing              11         0

****************************************************************************************************************************/

// We should use some other name for NS_KER_FC9 as this will be supported in all future versions of FC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include "nslb_cav_conf.h"
#include "cavmodem.h"

#define MAX_LENGTH 80


#ifdef ENABLE_TCP_STATES

int v_cavmodem_fd = 0;
static unsigned long data_array[12][12];
char line[MAX_LENGTH + 1] = "\0";

char *statsname[]= {
                     "Unused", "Established", "SynSent", "SynRcvd", "FinWait1", "FinWait2",
                     "TimeWait", "Closed",  "CloseWait","LastAck","Listen","Closing"
                   };
 
int from_statname_id_arr[] = { 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 6, 7, 7, 8, 8, 9, 10 ,10 ,11};
int to_statname_id_arr[] = { 4, 8, 7, 3, 7, 1, 7, 4, 1, 5, 11, 6, 7, 6, 7, 10, 2, 9, 7, 7, 3, 2, 6};

static void usage()
{
  printf( "Usage: nsi_get_tcp_stats [-c|-s] [-r] [--all|--cur|--trans]\n");
  exit(1);
}


// Need to use this method from ns_wan_env.c later
static int ns_cavmodem_init()
{

// Disabled this code be Neeraj on May 11, 2011 as there is no need. Bug Id 2408
#if 0
  int max = 20480; // This is the max modems allowed in the kernel. If kernel is tuned, change this also
  char *ptr;

  ptr = getenv("NS_MAX_MODEM");
  if (ptr) max = atoi(ptr);

  // NS_MAX_MODEM is set to 0 or already opened, return.
  if((max == 0) || (v_cavmodem_fd))
  {
    printf("Skipping open. max = %d, v_cavmodem_fd = %d\n", max, v_cavmodem_fd);
    return 0;
  }
#endif

  if ((v_cavmodem_fd = open("/dev/cavmodem", O_RDWR|O_CLOEXEC)) == -1)
  {
    perror("cavmodem open");
    printf("netstorm: failed to open /dev/cavmodem");
    return -1;
  }
  return 0;
}

static char *get_file_value(char *file)
{
  FILE *fp;

  if((fp = fopen(file,"r")) == NULL)
  {
    printf("\nError: %s not Found\n",file);
    return("NA");
  }
  
  fgets(line, MAX_LENGTH, fp);
  fclose(fp);
  return(line);
}

//to get the TCP value from file /proc/net/sockstat
static unsigned long tcp_file_value()
{
  FILE *fp;
  char *value;

  if((fp = fopen("/proc/net/sockstat","r")) == NULL)
  {
    printf("\nError: /proc/net/sockstat  not Found\n");
    return 0;
  }
  while(fgets(line, MAX_LENGTH, fp))
  {
    if(!strncmp (line, "TCP:", 4))
    {
      int i = 1;
      value = strtok(line, " ");
      while(i++ < 7)
       value = strtok(NULL," ");
      fclose(fp);
      return(atol(value));
    }
  }
  return 0;
}


// fn to check tcp stat is enable or not 
static int is_tcp_stats_enabled()
{
  int ctrl_file_value;
  char *ctrl_file = get_file_value("/proc/driver/cavmodem/cv_tcpStCtrl");

  if( !strcmp(ctrl_file, "NA")) 
    exit(0);

  ctrl_file_value = atoi(ctrl_file);

  if(ctrl_file_value == 1)
    return 0;
  else if(ctrl_file_value == 0)
    printf("TCP Stats are not enabled in /proc/driver/cavmodem/cv_tcpStCtrl.\n");
  else
    printf("Error: /proc/driver/cavmodem/cv_tcpStCtrl file not in proper format");

  return 1;
}

static int set_option(char *file_name, char *arg1_val, char *arg2_val, char *arg3_val)
{
  char sr_mode[2];
  char cmd1[MAX_LENGTH],cmd2[MAX_LENGTH];

  if(is_tcp_stats_enabled())
    exit(1);

  nslb_init_cav();

  if(!strcmp(arg1_val, "-s"))
  {
    if(strncmp(g_cavinfo.config, "NS>NO", 5))
    {
      printf("Error: This command can not run on Server, Machine Configuration is %s in /home/cavisson/etc/cav.conf.",g_cavinfo.config); 
      exit(1);
    }
    sprintf(cmd1, "ping -c 1 -W 1 %s >/dev/null 2>&1", g_cavinfo.SRAdminIP);
     
    if(system(cmd1))
    {
      printf( "Error: There is no connectivity between Client Admin IP = %s and Server Admin IP = %s",g_cavinfo.NSAdminIP,g_cavinfo.SRAdminIP);
      exit(1);
    }

    if(!strcmp(arg2_val, "USER"))
       strcpy(sr_mode, " ");
    else if(!strcmp(arg2_val, "RAW"))
       strcpy(sr_mode, "-r");

    // FromGUI command is run with full path, do not prefix with $NS_WDIR
    //     /home/cavisson/work/bin/nsi_get_tcp_stats -c -r --trans 2>&1

    if(file_name[0] == '/') // Command name running with full path
      sprintf(cmd2, "ssh %s %s %s --%s", g_cavinfo.SRAdminIP, file_name, sr_mode, arg3_val);
    else
      sprintf(cmd2, "ssh %s /home/cavisson/work/bin/%s %s --%s", g_cavinfo.SRAdminIP, file_name, sr_mode, arg3_val);

    if(system(cmd2))
    {
      printf("Error: Error in getting the TCP States stats from the Server %s\n",g_cavinfo.SRAdminIP);
      exit(1);
    }
    return 1;
  }
  return 0;
}

static char *write_cur_header(char *result_mode)
{
  if(!strncmp(result_mode, "USER", 4))
    return("  TcpStateName   StateId   CurCount\n");
  else if(!strncmp(result_mode, "RAW", 3))
    return("TcpStateName|StateId|CurCount\n");
}

static char *write_trans_header(char *result_mode)
{
  if(!strncmp(result_mode, "USER", 4))
    return("From-StateName   From-StateId    To-StateName   To-StateId   TransCount\n");
  else if(!strncmp(result_mode, "RAW", 3))
    return("From-StateName|From-StateId|To-StateName|To-StateId|TransCount\n");
}

//Function to write cur values
static void write_cur_values(char *result_mode)
{
  int idx = 1;
  unsigned long file_value;
  
  char buf_data[20*128] = "\0";

  strcpy(buf_data, write_cur_header(result_mode));

  while (idx < 12)
  {
    char buf[128] = "\0";

    if (idx == 6 )
      file_value = tcp_file_value();
    else
      file_value = data_array[idx][idx];

    if(!strncmp(result_mode, "USER", 4))
      sprintf(buf, "%-20s %-10d %lu\n", statsname[idx], idx, file_value);
    else if(!strncmp(result_mode, "RAW", 4))
      sprintf(buf, "%s|%d|%lu\n", statsname[idx], idx, file_value);

    strcat(buf_data, buf);
    idx++;
  }
  printf("%s", buf_data);
}

static void write_trans_values(char *result_mode)
{
  int id = 0, src_id, dest_id;

  char buf_data[20*128] = "\0";

  strcpy(buf_data, write_trans_header(result_mode));
  //while (id < (sizeof(from_statname_id_arr)/4))
  while (id < 23)
  {
    char buf[128] = "\0";

    src_id = from_statname_id_arr[id];
    dest_id = to_statname_id_arr[id];

    if(src_id == dest_id) 
    { 
      ++id;
      continue;
    }

    if(!strncmp(result_mode, "USER", 4))
      sprintf(buf, "%-20s%-15d%-20s%-10d%lu\n", statsname[src_id], src_id, statsname[dest_id], dest_id, data_array[src_id][dest_id]); 
    else if(!strncmp(result_mode, "RAW", 3))
      sprintf(buf, "%s|%d|%s|%d|%lu\n", statsname[src_id], src_id, statsname[dest_id], dest_id, data_array[src_id][dest_id]);

    strcat(buf_data, buf);
    ++id;
  } 
  printf("%s", buf_data);
}


static int get_tcp_stats(int arg_count, char *arg_list[])
{
  int ret;
  char opt;
  char state_mode[6] = "all";
  char entity[] = "-c";
  char result_mode[] = "USER";
  struct option longopts[] = {
                              {"c", 0, NULL, 'c'},
                              {"r", 0, NULL, 'r'},
                              {"s", 0, NULL, 's'},
                              {"all", 0, NULL, 'A'},
                              {"trans", 0, NULL, 'T'},
                              {"cur", 0, NULL, 'C'},
                              {0, 0, 0, 0}
                             };
 
  if(arg_count > 4)
    usage();
 
  while((opt = getopt_long(arg_count, arg_list, "crsATC", longopts, NULL)) != -1)
  {
   switch(opt)
   {
    case 'c':
     strcpy(entity, "-c");
     break;
    case 'r':
     strcpy(result_mode, "RAW");
     break;
    case 's':
     strcpy(entity, "-s");
     break;
    case 'A':
     strcpy(state_mode, "all");
     break;
    case 'T':
     strcpy(state_mode, "trans");
     break;
    case 'C':
     strcpy(state_mode, "cur");
     break;
    default :
     usage();
     break;
    }
  }

  ret = ns_cavmodem_init();
  if(ret != 0){
    exit(-1);
  }  
    
  ret = ioctl(v_cavmodem_fd, CAV_GET_STATS, data_array);
  if (ret)
  {
    perror("CAV_GET_STATS ioctl");
    fprintf(stderr, "netstorm: CAV_GETSTATS failed RET %d\n", ret);
    return -1;
  }

#if 0

  int k ;
  int l ;
  for(k = 1; k<12; k++)
  {
    for(l = 1; l<12; l++)
    {
      printf("data_array[%d][%d] = %lu\t", k, l, data_array[k][l]);
      if(l%4 == 0)
        printf("\n\n");
    }
    printf("\n\n");
  }

#endif

  //with -s option return after set_option fn called,because command will run on server
  if(set_option (arg_list[0], entity, result_mode, state_mode))
    return 0;

  if(!strncmp(state_mode, "all", 3))
  {
   write_trans_values(result_mode);
   printf("\n");
   write_cur_values(result_mode);
  }
  else if(!strncmp(state_mode, "cur", 3))
   write_cur_values(result_mode);

  else if(!strncmp(state_mode, "trans", 4))
   write_trans_values(result_mode);
}

#endif

int main(int argc, char *argv[])
{

#ifdef ENABLE_TCP_STATES
   get_tcp_stats(argc, argv);
#else
   fprintf(stderr, "TCP state stats are not supported.\n");
   exit(-1);
#endif
   return 0;
}
