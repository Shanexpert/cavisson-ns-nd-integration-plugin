#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "init_cav.h"
#include "nslb_util.h"

//char g_ns_wdir[CAV_INFO_ELEMENT_SIZE];
extern char g_ns_wdir[];
Cav_Info g_cavinfo;

void
init_cav()
{
char *ptr;
FILE *fp_conf;
char read_buf[1024], temp[1024];

  set_ns_wdir();
   //Open config file
  if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) 
  {
    fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
    perror("fopen");
    exit (1);
  }

  memset(&g_cavinfo, 0, sizeof(Cav_Info));

  // read interface name for client/server and IP address of server
  while (nslb_fgets(read_buf, 1024, fp_conf, 0))
  {
    if (!strncmp(read_buf, "CONFIG", 6))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.config);
    else if (!strncmp(read_buf, "NSLoadIF", 8))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.NSLoadIF);
    else if (!strncmp(read_buf, "SRLoadIF", 8))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.SRLoadIF);
    else if (!strncmp(read_buf, "NSAdminIP", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.NSAdminIP);
    else if (!strncmp(read_buf, "SRAdminIP", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.SRAdminIP);
    else if (!strncmp(read_buf, "NSAdminIF", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.NSAdminIF);
    else if (!strncmp(read_buf, "SRAdminIF", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.SRAdminIF);
    else if (!strncmp(read_buf, "NSAdminGW", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.NSAdminGW);
    else if (!strncmp(read_buf, "SRAdminGW", 9))
      sscanf(read_buf,"%s %s", temp, g_cavinfo.SRAdminGW);
  }
   
  // NS Related keywords are only for NS>NO and NS machines
  if ((!strcmp (g_cavinfo.config, "NS>NO")) || (!strcmp (g_cavinfo.config, "NS")) || (!strcmp (g_cavinfo.config, "NC")))
  {
    //Get NS admin netid
    if (!(ptr = strtok(g_cavinfo.NSAdminIP, "/"))) 
    {
      fprintf(stderr, "ERROR: NSAdminIP (/home/cavisson/etc/cav.conf) not in IP/netbits format\n");
      exit (1);
    }
    if (!(ptr = strtok(NULL, "/"))) 
    {
        fprintf(stderr, "ERROR: NSAdminIP (/home/cavisson/etc/cav.conf) not in IP/netbits format\n");
        exit (1);
    }
    g_cavinfo.NSAdminNetBits = atoi(ptr);
  }
  
  // SR (NetOcean) Related keywords are only for NS>NO machine
  if (!strcmp(g_cavinfo.config, "NS>NO"))
  {
    //Get NS admin netid
    if (!(ptr = strtok(g_cavinfo.SRAdminIP, "/"))) 
    {
      fprintf(stderr, "ERROR: SRAdminIP (/home/cavisson/etc/cav.conf) not in IP/netbits format\n");
      exit (1);
    }
    if (!(ptr = strtok(NULL, "/"))) 
    {
      fprintf(stderr, "ERROR: SRAdminIP (/home/cavisson/etc/cav.conf) not in IP/netbits format\n");
      exit (1);
    }
    g_cavinfo.SRAdminNetBits = atoi(ptr);
  }
  
  fclose(fp_conf);
}
