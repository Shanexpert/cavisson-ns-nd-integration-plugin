#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "ns_alloc.h"
#include "nslb_util.h"
#include "nslb_server_admin.h"
//#include "netomni/src/core/ni_scenario_distribution.h"

#define MAKE_COMMAND_FOR_CMON \
  nslb_replace_strings(cmd_args, ":", "%3A");\
  cnt = 0;\
  len = strlen(cmd_args);\
  while(cmd_args[cnt] != ' ' && cnt< len)\
    cnt++;\
  if(cnt< len)\
    cmd_args[cnt]=':';

#if 0
typedef struct gen_attr
{ 
  char IP[128]; 
  char agentport[6];
  char work[512]; 
  int mode;
  int gen_id;
  int testidx;
}gen_attr;

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
  int num, bandwidthAvail;

  if((ptr = strchr(gen_health_buf, '\n')))
    *ptr = '\0';

  printf("%s, gen_health_buf = %s", gen_and_ip, gen_health_buf);

  if(!strncmp(gen_health_buf, "INVALID", 7))
  {
    printf("Error: Stopping test as blade path '%s' is invalid for generator %s",
                           generator_entry[gen_id].work, gen_and_ip);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as blade path '%s' is invalid for generator %s",
                                           generator_entry[gen_id].work, gen_and_ip);
    return -1;
  }

  num = get_tokens(gen_health_buf, comma_fields, ",", 10);
  if(!num && (gen_health_buf[0] == '\0'))
  {
    printf("Error: %s is not healthy. Please check disk may be full.", gen_and_ip);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "%s is not healthy. Please check disk may be full", gen_and_ip);
    return -1;
  }

  if(num < 5 || num > 6)
  {
    printf("Error: Invalid number of arguments [%d] in health file of %s", num, gen_and_ip);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Unable to get generator %s health information"
                        " (Build Version, Memory, Disk, Bandwidth)", gen_and_ip);
    return -1;
  }

  buildVersion = comma_fields[0];
  testRun = comma_fields[1];
  diskAvail = comma_fields[2];
  cpuAvail = comma_fields[3];
  memAvail = comma_fields[4];
  bandwidthAvail = atoi(comma_fields[5]);

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

  printf("buildVersion = %s, testRun = %s, diskAvail = %s, cpuAvail = %s, memAvail = %s, "
                 "bandwidthAvail = %d, homeDiskAvail = %s, rootDiskAvail = %s", 
                 buildVersion, testRun, diskAvail, cpuAvail, memAvail, bandwidthAvail, homeDiskAvail, 
                 rootDiskAvail);
  
  if(atoi(testRun))
  {
    printf("Error: Stopping test as testrun '%s' is already running on %s", testRun, gen_and_ip);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as testrun '%s' is already running on %s",
             testRun, gen_and_ip);
    return -1;
  }
/*
  if(checkgenhealth.minDiskAvailability)
  {
    int ret = 0;
    // For running a netcloud test what should be the minimum memory on / if /home is mounted ?? 
    if(rootDiskAvail && (atoi(rootDiskAvail) < 5)) {
      printf("Error: Stopping test as root disk available Space '%sGB' on %s is less than threshold disk Space '5GB'",
                            rootDiskAvail, gen_and_ip);
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as root disk available Space '%sGB' on"
               " %s is less than threshold disk Space '5GB'", rootDiskAvail, gen_and_ip);
      ret = 1;
    }

    if(homeDiskAvail && (atoi(homeDiskAvail) < checkgenhealth.minDiskAvailability)) {
      printf("Error: Stopping test as home disk available Space '%sGB' on %s is less than threshold disk Space '%dGB'",
                homeDiskAvail, gen_and_ip, checkgenhealth.minDiskAvailability); 
      snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as home disk available Space"
               " '%sGB' on %s is less than threshold disk Space '%dGB'", homeDiskAvail, gen_and_ip,
               checkgenhealth.minDiskAvailability);
      ret = 1;
    }
    if(ret)
      return -1;
  }

  if(checkgenhealth.minCpuAvailability && (atoi(cpuAvail) > checkgenhealth.minCpuAvailability))
  {
    printf("Error: Stopping test as CPU utilization pct '%s%%' on %s is exceeded from threshold CPU '%d%%'",
              cpuAvail, gen_and_ip, checkgenhealth.minCpuAvailability);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as CPU utilization pct '%s%%' on"
             " %s is exceeded from threshold CPU '%d%%'", cpuAvail, gen_and_ip, checkgenhealth.minCpuAvailability);
    return -1;
  }
 
  if(checkgenhealth.minMemAvailability && (atoi(memAvail) < checkgenhealth.minMemAvailability))
  {
    printf("Error: Stopping test as available memory '%sGB' on %s is less than threshold memory '%dGB'",
              memAvail, gen_and_ip,  checkgenhealth.minMemAvailability);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as available memory '%sGB' on %s"
             " is less than threshold memory '%dGB'", memAvail, gen_and_ip, checkgenhealth.minMemAvailability);
    return -1;
  }

  if(checkgenhealth.minBandwidthAvailability && (bandwidthAvail > 0) && (bandwidthAvail < checkgenhealth.minBandwidthAvailability))
  {
    printf("Error: Stopping test as available bandwidth '%dMbps' on %s is less than threshold bandwidth '%dMbps'",
              bandwidthAvail, gen_and_ip, checkgenhealth.minBandwidthAvailability);
    snprintf(generator_entry[gen_id].gen_keyword, 24000, "Stopping test as available bandwidth '%dMbps' on"
             " %s is less than threshold bandwidth '%dMbps'", bandwidthAvail, gen_and_ip, checkgenhealth.minBandwidthAvailability);
    return -1;
  }
*/
  return 0; //Success
}
#endif

#define MAX_THREAD 512
#define DELTA_GEN_ENTRY_SIZE 128

typedef struct GenEntries{
 char name[1024];
 char ip[32];
 char work[32];
 int work_flag;
 int status;
 int num_gen; 
}GenEntries;
 
//int check_gen_health(char *gen_name, char *gen_ip, char* gen_ctrl)
void check_gen_health(GenEntries *gen_table)
{
  char cmd_args[4096];
  ServerCptr server_ptr;
  ServerInfo server_info;
  int cnt, len;
  
  memset(&server_ptr, 0, sizeof(ServerCptr));
  memset(&server_info, 0, sizeof(ServerInfo));
  server_ptr.server_index_ptr = &server_info;
  server_ptr.server_index_ptr->server_ip = gen_table->ip;
  server_ptr.server_index_ptr->topo_server_idx = 7891;

  //sprintf(cmd_args, "/home/cavisson/%s/bin/nsu_check_health /home/cavisson/%s", gen_table->work, gen_table->work);
  sprintf(cmd_args, "nsu_get_version -v env_var:NS_WDIR=/home/cavisson/%s", gen_table->work);

  MAKE_COMMAND_FOR_CMON
  if (nslb_run_users_command(&server_ptr, cmd_args))
  {
    gen_table->status = 0; //UNAVAILABLE
    gen_table->ip[0] = '\n';
  }  
  else
  {
    gen_table->status = 1; //AVAILABLE
    strcpy(gen_table->ip, server_ptr.cmd_output);
  }
}

void *check_gen_health_thread(void *args)
{
  GenEntries *gen_table = (GenEntries *)args;
  int lol_num_gen = gen_table->num_gen;

  for (int i = 0; i < lol_num_gen; i++, gen_table++)
    check_gen_health(gen_table); 
  return NULL; 
}

int main(int argc, char *argv[])
{
  char generator[1024] = {0};
  char *field[20] = {0}, *ptr = NULL, *next = NULL;
  int total_gen_flds, len = 0, i;
  GenEntries *gen_table = NULL;
  char *default_ctrl = "work", *ctrl = NULL;
  int num_gen_entries = 0, max_gen_entries = 0;
//New code change for thread  
  static pthread_t thread[MAX_THREAD];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//End  
  
  if (argc != 2)
    exit (-1);
  
  ptr = argv[1];
  do
  {
     next = strchr(ptr, ',');
      
     if(next)
     { 
       len = next-ptr;
       strncpy(generator, ptr, len);
       generator[len] = '\0';	
       next++;  
       ptr = next;
     }else
     {
       strcpy(generator, ptr);
     }
     
     total_gen_flds = get_tokens_ex(generator, field, "|", 4); 
      if(total_gen_flds != 2)
      {
        //TODO error msg
         exit (-1);
       }
       if (num_gen_entries == max_gen_entries)
       {
          max_gen_entries += DELTA_GEN_ENTRY_SIZE;   //++128 size
          NSLB_REALLOC_EX(gen_table, max_gen_entries * sizeof(GenEntries),  num_gen_entries * sizeof(GenEntries), "GeneratorEntry", -1, NULL); 
       }
       if ((ctrl = strrchr(field[0], ':')) != NULL)
       {
         *ctrl = '\0';
         ctrl++;
         gen_table[num_gen_entries].work_flag = 1;
       }else
       {
         ctrl = default_ctrl;
         gen_table[num_gen_entries].work_flag = 0;
       }
       strcpy(gen_table[num_gen_entries].name, field[0]);
       strcpy(gen_table[num_gen_entries].ip, field[1]);
       strcpy(gen_table[num_gen_entries].work, ctrl);
       gen_table[num_gen_entries].status = -1;
       gen_table[num_gen_entries].num_gen = 0;
       num_gen_entries++;
       //ret = check_gen_health(field[0], field[1], ctrl);
      //TODO: add thread
  }while (next);
  
  //check_all_thread_done();
  int all_thread_gen_enties = num_gen_entries / MAX_THREAD;
  int remaining_gen_enties = num_gen_entries % MAX_THREAD;
  //TODO Need to provide data in log file.
  //freopen("/dev/null", "a", stdout);
  freopen("/dev/null", "a", stdout);
  for(i = 0; i < num_gen_entries; i += gen_table[i].num_gen)
  {
    gen_table[i].num_gen = all_thread_gen_enties + (i < remaining_gen_enties)?1:0;
    
    int ret = pthread_create(&thread[i], &attr, check_gen_health_thread, (void *)&gen_table[i]);

    if (ret)
    {
      exit(-1);
    }
  }
  pthread_attr_destroy(&attr);
   
  int remaining_time = 60;
  int count = 0;
  //int retry_count = 5;
  while(1)
  {
    count = 0;
    for (i = 0; i< num_gen_entries; i++)
    {
      if (gen_table[i].status != -1)
        count++;
      //fprintf(stderr, "Generator Status: Gen_name [%s], Gen_IP [%s], Gen_status [%d]\n", gen_table[i].name, gen_table[i].ip, gen_table[i].status);
    }
    if (count == num_gen_entries) 
      break;
    if(!remaining_time)
      break;
    remaining_time -= 1; 
    //fprintf(stderr, "Retrying to check server health. Count = %d", retry_count--); 
    sleep(1);
  }
  for(i = 0; i< num_gen_entries; i++)
  {
    if (gen_table[i].status == -1)
    {
      gen_table[i].status = 1;
      gen_table[i].ip[0] = '\n';
    }
    if (gen_table[i].work_flag)
      fprintf(stderr, "%s:%s|%d|%s", gen_table[i].name, gen_table[i].work,  gen_table[i].status, gen_table[i].ip);
    else
      fprintf(stderr, "%s|%d|%s", gen_table[i].name,  gen_table[i].status, gen_table[i].ip);
  }
  exit (0);
}

