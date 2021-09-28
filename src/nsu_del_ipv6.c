/******************************************************************
 * Name    :    nsu_del_ipv6.c
 * Purpose :    Delete allias IPV6 for client and server
 * Usage:  nsu_del_ipv6 [-s] [start_ip...]
           * -s flag means, given IP's need to be deleted on server else on client
		   * start_ip gives the start_ip of the IP range to be deleted
           * Complete assigned range would be deleted.
		   * More than one start_ip's to specify multiple ranges may be gievn
           * If start ip does not match any given range, an error maesage 
           * will be given out for that start-ip ("No IP range assigned with the
           * given start IP"). 
*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nsu_ipv6_utils.h"

void print_usage(char *s);
//int delete_all(int entity);
static char work_dir[512];
char rlogin[5] = "ssh";
char admin_ip[50], server_addr[50];
char nsadmin_ethx[20], sradmin_ethx[20], config[50];


int
delete_range(int entity, char *start_ip_to_del)
{
  int i, total;
  char start_ip[50], end_ip[50], net_id[50], netbits[10], gateway[50], primary_ip[50], exclude_ip[50], vlan_id[10];
  IPV6Range* ipv6range_table;
  char ipv6_entries_file[512], interface[20];


  //Match in the IPV6 range of the entity
  if (entity == NS_CLIENT) {
    total = total_client_ipv6range_entries;
    ipv6range_table = cIPV6rangeTable;
  } else {
    total = total_server_ipv6range_entries;
    ipv6range_table = sIPV6rangeTable;
  }

  for (i = 0; i < total; i++) {
    if (strcasecmp(start_ip_to_del, ipv6range_table[i].start_ip) == 0)
      break;
  }

  //If NO IPV6 range with given start ip,
  //Give error and return

  if (i == total){
    printf("ERROR: No IPV6 range assigned with the given start IP %s\n",start_ip_to_del);
    return 1;
  }

  //Initialize eip (end-ip), gw (gateway), nid (net-id), nbits(netbits), exip (exclude ip), vidi (vlanid), pip (primary ip)
  strcpy(start_ip, ipv6range_table[i].start_ip);
  strcpy(end_ip, ipv6range_table[i].end_ip);
  strcpy(gateway, ipv6range_table[i].gateway);
  strcpy(net_id, ipv6range_table[i].net_id);
  strcpy(netbits, ipv6range_table[i].netbits);
  strcpy(exclude_ip, ipv6range_table[i].exclude_ip);
  strcpy(vlan_id, ipv6range_table[i].vlan_id);
  strcpy(primary_ip, ipv6range_table[i].primary_ip);
  strcpy(interface, ipv6range_table[i].LoadIF);

  if (do_del_ipv6(entity, start_ip, end_ip, vlan_id, netbits, exclude_ip, net_id, gateway, interface)){
   	printf("ERROR: Deleting IP failed \n");  
        return 1;
  }
    //Delete ipv6_entries 
    delete_IPV6Range_table_entry(entity, start_ip);
    sprintf(ipv6_entries_file,"%s/sys/ipv6_entries",work_dir);
    //re-write ip_entries file
    if (write_ipv6_entries (ipv6_entries_file)){
      printf("ERROR: In writing file %s\n",ipv6_entries_file);
      return 1;
    }

  return 0;
}

int main(int argc, char *argv[])
{
int entity=NS_CLIENT;
int argnum = 1, i;
int deleteall = 0;
char ipv6_entries_file[1024];
FILE *fp_conf;
char  read_buf[MAX_LINE_LENGTH],temp[20];
//char pip[50];

  if (argc < 2) {
    print_usage(argv[0]); 
    exit(1);
  }
//process cmd line arguments
  if (!strcasecmp ("-s", argv[argnum])) {
    entity=NS_SERVER;
    argnum++;
  } 

/*  if (!strcasecmp ("-a", argv[argnum])) {
    deleteall=1;
    argnum++;
  }
*/
   //Open config file
  if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
    perror("fopen");
    return 1;
  }
  // read interface name for client/server and IP address of server
  while (fgets(read_buf, MAX_LINE_LENGTH, fp_conf)) {
    if (!strncmp(read_buf, "CONFIG", 6)){
           sscanf(read_buf,"%s %s", temp, config);
    }if (!strncmp(read_buf, "NSAdminIP", 9)){
           sscanf(read_buf,"%s %s", temp, admin_ip);
    }else if (!strncmp(read_buf, "SRAdminIP", 9)){
           sscanf(read_buf,"%s %s", temp, server_addr);
    }else if (!strncmp(read_buf, "NSAdminIF", 9)){
           sscanf(read_buf,"%s %s", temp, nsadmin_ethx);
    }else if (!strncmp(read_buf, "SRAdminIF", 9)){
           sscanf(read_buf,"%s %s", temp, sradmin_ethx);
    }
  }
  
  //For NS+NO, NO IP Management allowed.
  if (!strcmp(config, "NS+NO")) {
    printf("ERROR: Test Configuration. NO IP Management allowed\n");
    return 1;
  }

  //Read current IP entries in structure
  if (getenv("NS_WDIR") != NULL)
    strcpy(work_dir, getenv("NS_WDIR"));
  else{
    printf("ERROR: Did not found netstorm working directory\n");
    exit(1);
  }
  
  //Read ipv6_entries file
  sprintf(ipv6_entries_file,"%s/sys/ipv6_entries",work_dir);
  if (read_ipv6_entries (ipv6_entries_file)){
     printf("ERROR: In reading file %s\n",ipv6_entries_file);
     exit(1);
  }
/*
  for (i = 0; i < total_client_ipv6range_entries; i++) {
    do_get_primary (0, cIPV6rangeTable[i].start_ip, cIPV6rangeTable[i].netbits, cIPrangeTable[i].vlan_id, &pip, cIPrangeTable[i].LoadIF);
    if ( cIPrangeTable[i].start_ip == pip )
      cIPrangeTable[i].primary_ip=pip;
    else
      cIPrangeTable[i].primary_ip=0;
  }
  for (i = 0; i < total_server_iprange_entries; i++) {
    do_get_primary (1, sIPrangeTable[i].start_ip, sIPrangeTable[i].netbits, sIPrangeTable[i].vlan_id, &pip, sIPrangeTable[i].LoadIF);
    if ( sIPrangeTable[i].start_ip == pip )
      sIPrangeTable[i].primary_ip=pip;
    else
      sIPrangeTable[i].primary_ip=0;
  }
*/

  if (deleteall) {
   // if (delete_all(entity))
      exit(1);
  }
  else {
    for (i = argnum; i < argc; i++) 
      if (delete_range(entity, argv[i]))
        exit(1);
  }
  printf("Successfully deleted\n");
/*
  if (entity){
    strcat(work_dir,"/bin/nsi_sync_netocean");
    system(work_dir);
  }
*/

  return 0;
}

void
print_usage(char *str)
{
  printf("Usage: %s [-s] [start_ip...]\n", str);
}
