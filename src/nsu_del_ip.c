/******************************************************************
 * Name    :    nsu_del_ip.c
 * Author  :    Sanjay
 * Purpose :    Delete allias IP for client and server
 * Usage:  nsu_del_ip [-s] [-a|start_ip...]
           * -s flag means, given IP's need to be deleted on server else on client
		   * -a means delete all IP's
		   * start_ip gives the start_ip of the IP range to be deleted
           * Complete assigned range would be deleted.
		   * More than one start_ip's to specify multiple ranges may be gievn
           * If start ip does not match any given range, an error maesage 
           * will be given out for that start-ip ("No IP range assigned with the
           * given start IP"). 
 * Modification History:
 *   10/05/06: Sanjay - Initial Version
 *   29/06/06: Sanjay - Update all functions to support source based routing.
 *   08/07/06: Sanjay - Bug fixed in delete_all() function.     
 *                    - Update code for get the primary ip at runtime and fill in-memory data structure.
 *                    - Add code to syncronize ip entries file on NetOcean.
 *   24/07/06: Sanjay - Updated for support multiple interface
  *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ipmgmt_utils.h"
#include "nsu_ipv6_utils.h"

void print_usage(char *s);
int delete_all(int entity);
static char work_dir[512];
char rlogin[5] = "ssh";
char admin_ip[20], server_addr[20];
char nsadmin_ethx[20], sradmin_ethx[20], config[50];


int
delete_ipv6_range(int entity, char *start_ip_to_del)
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
    //ip entries file is now no longer Controller specific. Hence changing it to /home/cavisson/work
    sprintf(ipv6_entries_file,"/home/cavisson/work/sys/ip_entries");
    //re-write ip_entries file
    if (write_ip_entries (ipv6_entries_file)){
      printf("ERROR: In writing file %s\n",ipv6_entries_file);
      return 1;
    }

  return 0;
}

int
delete_range(int entity, char *start_ip)
{
  int i, total, nbits;
  unsigned int sip, eip, nid,gw, pip, exip, vid;
  IPRange* iprange_table;
  char end_ip[20], interface[20];


  //Convert start_ip to unsgined int 
  if (ns_ip_addr (start_ip, &sip)) {
    printf("ERROR: Invalid start IP \n");
    return 1;
  }

  //Match in the IP range of the entity
  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }

  for (i = 0; i < total; i++) {
    if (sip == iprange_table[i].start_ip)
      break;
  }

  //If NO IP range with given start ip,
  //Give error and return

  if (i == total){
    printf("ERROR: No IP range assigned with the given start IP %s\n",start_ip);
    return 1;
  }

  //Initialize eip (end-ip), gw (gateway), nid (net-id), nbits(netbits), exip (exclude ip), vidi (vlanid), pip (primary ip)
  sip = iprange_table[i].start_ip;
  eip = iprange_table[i].end_ip;
  gw = iprange_table[i].gateway;
  nid = iprange_table[i].net_id;
  nbits = iprange_table[i].netbits;
  exip = iprange_table[i].exclude_ip;
  vid = iprange_table[i].vlan_id;
  pip = iprange_table[i].primary_ip;
  strcpy(interface, iprange_table[i].LoadIF);

  //Convert in string format
  strcpy(end_ip, ns_char_ip(eip));

  if (!pip) { //If no primary IP for the range
    //Delete all ip n ip range using cmd
    // nsi_del_ip entity ip start_ip end_ip vlan_id netbits exclude_ip
    // using cmd 'ip addr del ip-address/netbits dev load_interface.valn_id'
    // example: ip addr del 192.168.0.7/24 dev eth0.5
    if(do_del_ip(entity, sip, eip, vid, nbits, exip, nid, gw, interface)){
        printf("ERROR: Deleting IP failed \n");
        return 1;
    }
    //Delete ip_entries 
    delete_IPRange_table_entry(entity, sip);
  } else {
    //A primary IP is associacted with this range
    //removing promary IP will remove all other IPs on this interface and netid
    // nsi_del_ip entity primary_ip primary_ip vlan-id netbits -
    if(do_del_ip(entity, sip, eip, vid, nbits, 0, nid, gw, interface)){
        printf("ERROR: Deleting IP failed \n");
        return 1;
    }
    delete_IPRange_table_entry(entity, sip);
    total--; //One entry is deleted
    //It is possible that some other IP from other address ranges might have deleted because of 
    //deleting promary IP
    //For all other address ranges in same netid and vlan-id and interface for same entity,
    //issuee nsi_add_ip  entity  start_ip end-ip vlan-id netbits exclude-ip cmd to recreate such IPs
    //And set the primary if IPs are created for new range
    for (i = 0; i < total; i++) {
      if ((nid == iprange_table[i].net_id) && (vid == iprange_table[i].vlan_id && !strcmp(interface, iprange_table[i].LoadIF))) {
        if (do_add_ip (entity, iprange_table[i].start_ip, iprange_table[i].end_ip,
                vid, nbits, iprange_table[i].exclude_ip, &pip, iprange_table[i].net_id, iprange_table[i].gateway, interface)) {
            printf("ERROR: Adding IP failed for removed primary IP\n");
            return 1;
        }

        //If primary == sip, save as primary
        //And set the primary if IPs are creaeated for nay range
        if(pip == iprange_table[i].start_ip)
         iprange_table[i].primary_ip = pip;

      }//End if
    }//End for loop
  }// End else

  //re-write ip_entries file
   if (write_ip_entries ()){
     printf("ERROR: write_ip_entries() failed\n");
     return 1;
   }

   return 0;
}

int main(int argc, char *argv[])
{
int entity=NS_CLIENT;
int argnum = 1, i;
int deleteall = 0;
FILE *fp_conf;
char  read_buf[MAX_LINE_LENGTH],temp[20];
unsigned int pip;
int ip_type;

   if (argc < 2) {
	print_usage(argv[0]); //define this function
	exit(1);
   }
//process cmd line arguments
   if (!strcasecmp ("-s", argv[argnum])) {
	entity=NS_SERVER;
 	argnum++;
   }

   if (argv[argnum] == '\0')
   {
     print_usage(argv[0]);
     exit (1);
   }
 

   if (!strcasecmp ("-a", argv[argnum])) {
	deleteall=1;
 	argnum++;
   }

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

    //Read current IP entries intructure
 
   if (getenv("NS_WDIR") != NULL)
     strcpy(work_dir, getenv("NS_WDIR"));
   else{
     printf("ERROR: Did not found netstorm working directory\n");
     exit(1);
   }
   //Read ip_entries file
   if (read_ip_entries ()){
     printf("ERROR: read_ip_entries() failed\n");
     exit(1);
   }
   for (i = 0; i < total_client_iprange_entries; i++) {
	    do_get_primary (0, cIPrangeTable[i].start_ip, cIPrangeTable[i].netbits, cIPrangeTable[i].vlan_id, &pip, cIPrangeTable[i].LoadIF);
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


    if (deleteall) {
      if (delete_all(entity))
        exit(1);
    }
    else {
      ip_type = check_ip_type(argv[argnum]);
      if ( ip_type == IPV4_ADDR ) {
        for (i = argnum; i < argc; i++){ 
          if (delete_range(entity, argv[i]))
          exit(1);
        }
      }
      else if (ip_type == IPV6_ADDR) {
        for (i = argnum; i < argc; i++){
          if (delete_ipv6_range(entity, argv[i]))
           exit(1);
        }
      } 
    }
   printf("Successfully deleted\n");
   if(entity){
     strcat(work_dir,"/bin/nsi_sync_netocean");
     system(work_dir);
   }
   return 0;
}

void
print_usage(char *str)
{
  printf("Usage: %s [-s] [-a|start_ip...]\n", str);
}

// Comparision function for qsort, i have changed the logic for descending sort
int compfunc(const void *x, const void *y)
{
    IPRange *r1, *r2;
   
    r1 = (IPRange *)x;
    r2 = (IPRange *)y;

    if ((r1->primary_ip) && !(r2->primary_ip))
        return -1;
    else if (!(r1->primary_ip) && (r2->primary_ip))
        return 1;
    else 
        return 0;
}

int delete_all(int entity)
{
    int total, i;
    IPRange *iprange_table;
    char *ns_admin_ip, *sr_admin_ip, *ns_netbits, *sr_netbits;
    unsigned int ns_pip, sr_pip, sradmin_ip, nsadmin_ip;

    //Find admin ips and netbits for both entity
    ns_admin_ip = strtok(admin_ip,"/");
    ns_netbits = strtok(NULL,"/");
    sr_admin_ip = strtok(server_addr,"/");
    sr_netbits = strtok(NULL,"/");

    //Convert admin ips into unsigned number
    ns_ip_addr(sr_admin_ip, &sradmin_ip);
    ns_ip_addr(ns_admin_ip, &nsadmin_ip);

    //Find pip of NS Admin IP
    do_get_primary (NS_CLIENT, nsadmin_ip, atoi(ns_netbits), NS_NO_VLAN, &ns_pip, nsadmin_ethx);
    //Find pip of SR Admin IP
    do_get_primary (NS_SERVER, sradmin_ip, atoi(sr_netbits), NS_NO_VLAN, &sr_pip, sradmin_ethx);
 
    //Make sure, Admin IPs are primaries.
    if (!strcmp(config, "NS>NO")) {
       if ( !((nsadmin_ip == ns_pip) && (sradmin_ip == sr_pip))) {
          printf("ERROR: Admin IPs are not primary \n");
          return 1;
       }
    }

    //Get admin netid
    if (entity) {
       get_netid (sradmin_ip, atoi(sr_netbits));
    } else {
       get_netid (nsadmin_ip, atoi(ns_netbits));
    }

    //Check self
    if (entity == NS_CLIENT) {
      total = total_client_iprange_entries;
      iprange_table = cIPrangeTable;
    } else {
      total = total_server_iprange_entries;
      iprange_table = sIPrangeTable;
    }

    //Sort entries with primaries first.
    qsort(iprange_table, total, sizeof(IPRange), compfunc);
	
    //Iterate entries table
    for(i=0; i < total; i++)
    {
      //If marked for deletion, continue
      if(iprange_table[i].status)
        continue;
      //Delete all entries one by one
      if(do_del_ip(entity, iprange_table[i].start_ip, iprange_table[i].end_ip, iprange_table[i].vlan_id, iprange_table[i].netbits, iprange_table[i].exclude_ip, iprange_table[i].net_id, iprange_table[i].gateway, iprange_table[i].LoadIF)){
          printf("ERROR: Deleting IP failed \n");  
          return 1;
      }
      delete_IPRange_table_entry(entity, iprange_table[i].start_ip);
      i--;
      total--;
    }

   //re-write ip_entries file
   if (write_ip_entries ()){
     printf("ERROR: write_ip_entries()\n");
     return 1;
   } 
    return 0;
}
