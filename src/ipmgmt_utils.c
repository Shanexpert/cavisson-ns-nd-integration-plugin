/******************************************************************
 * Name    :    ipmgmt_utils.c
 * Author  :    Sanjay
 * Purpose :    Contain functions that will be used by all IP management utilies
 * Modification History:
 *   01/05/06: Sanjay - Initial Version
 *   05/05/06  Sanjay - Added write_ip_entries() function
 *   10/05/06: Sanjay - Added function delete_range() and delete_IPRange_table_entry()
 *   08/07/06: Sanjay - Update functions write_ip_entries() and read_ip_entries().Now script will not read or write the
                        primary IPs in ip_entries file.
 *   24/07/06: Sanjay - Updated read_ip_entries, write_ip_entries, do_add_ip, do_del_ip
                        function for support multiple interface
 *****************************************************************/

/*Handling Load IP addresses and route on reboot:
 If for some reason Netstorm or Netocean need to be rebooted, all the IP and route assignemnet  will disappear. For handling this, We will maintain ip_entries file on Netstorm and Netocean. ip_entries file  will be syncronized on netocean when IP will be added or deleted on Netocean. There is a script nsi_on_reboot on Netstorm and Netocean. When machine will be rebooted, It will get information from ip_entries file and reassigned all IPs and related route for that particuler machine (Netstorm/Netocean).*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "nsu_ipv6_utils.h"
#include "ipmgmt_utils.h"
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "nslb_cav_conf.h"
#include "nslb_util.h"

//static void Create_client_iprange_entry(int *row_num);
//static void Create_server_iprange_entry(int *row_num);

IPRange *cIPrangeTable = NULL;
IPRange *sIPrangeTable = NULL;
AssignedIpInfo assignedIPinfo;

int total_client_iprange_entries = 0; //current IP entres for client (netstorm)
int total_server_iprange_entries = 0; //current IP entres for server (netocean)
static int max_client_iprange_entries; //max buf in IP entres for client (netstorm)
static int max_server_iprange_entries; //max buf in IP entres for server (netocean)

unsigned int ns_netmask[32]= {
        0x00000000,
        0x80000000,
        0xC0000000,
        0xE0000000,
        0xF0000000,
        0xF8000000,
        0xFC000000,
        0xFE000000,
        0xFF000000,
        0xFF800000,
        0xFFC00000,
        0xFFE00000,
        0xFFF00000,
        0xFFF80000,
        0xFFFC0000,
        0xFFFE0000,
        0xFFFF0000,
        0xFFFF8000,
        0xFFFFC000,
        0xFFFFD000,
        0xFFFFF000,
        0xFFFFF800,
        0xFFFFFC00,
        0xFFFFFE00,
        0xFFFFFF00,
        0xFFFFFF80,
        0xFFFFFFC0,
        0xFFFFFFE0,
        0xFFFFFFF0,
        0xFFFFFFF8,
        0xFFFFFFFC,
        0xFFFFFFFE
};

//void * Realloc(char *ptr, int size, char *msg);

static void * Realloc(char *ptr, int size, char *msg)
{
	void *p;
	p = realloc((void *)ptr, size);
	return p;
}

//an unsigned int address out.
//return 0 on success and -1 on failure
//Example: addr may be 192.168.1.18 out would be 0xC0A80112

int
ns_ip_addr (char * addr, unsigned int * out)
{
  int a = -1, b = -1 , c = -1, d = -1;
  //Read all 4 octects, make sure each octet is
  sscanf(addr, "%d.%d.%d.%d", &a, &b, &c, &d);
  //between 0-255. get these ocetets values in a,b,c,d
  if ( !((a >= 0 && a <= 255) && (b >= 0 && b <= 255) && (c >= 0 && c <= 255) && (d >= 0 && d <= 255)) ){
    //printf("ERROR: Invalid IP\n ");
    return -1;
  }
  //a is first octet and d is last.
  *out = d+ 256*c + 256*256*b + 256*256*256*a;
  return 0;
}

//converts unsigned int address to dotted notation
//Example: addr may be 0xC0A80112 return would be 192.168.1.18
//buffer is statically allocated. would be overwritten on next call.
char * ns_char_ip (unsigned int addr)
{
  static char str_address[16];
  unsigned int a, b, c,d;
  a = (addr >>24) & 0x000000FF;
  b = (addr >>16) & 0x000000FF;
  c = (addr >>8) & 0x000000FF;
  d = (addr) & 0x000000FF;
  sprintf(str_address, "%d.%d.%d.%d", a,b,c,d);
  return str_address;
}

//returns string vlanid
char * ns_char_vlanid (int vlan_id)
{
static char str_vlan[16];

    if (vlan_id == NS_NO_VLAN)
	strcpy(str_vlan, "-");
    else
    	sprintf (str_vlan, "%d", vlan_id);

    return str_vlan;
}

//ns_get_netid takes netid in dotted notaion, netbits
//and returns the netid in unsigedn int out.
//Return value is 0 on success and -1 if netbits are
//not between 1-30, -2 if netid has octets with more than 255-3 if
//netid is not not valid

int
ns_get_netid (char* netid, int netbits, unsigned int *out)
{
  int a,b,c,d;

  if ((netbits < 1 ) || (netbits >30))
    return -1;

  //Read all 4 octects, make sure each octet is
  sscanf(netid,"%d.%d.%d.%d", &a, &b, &c, &d);
  //between 0-255. get these ocetets values in a,b,c,d
  if ( !((a >= 0 && a <= 255) && (b >= 0 && b <= 255) && (c >= 0 && c <= 255) && (d >= 0 && d <= 255)) ){
    printf("ERROR: Invalid netid\n");
    return -2;
  }
  //a is first octet and d is last.
  *out = d+ 256*c + 256*256*b + 256*256*256*a;
  //Make sure all hostbits are 0.
  if ((*out) & ~ns_netmask[netbits])
    return -3;

  return 0;
}

//returns netbits based on IETF defined address classes.
//retrun 8, 16,24 on success and 0 on failure
int
ns_get_netbits (unsigned int addr)
{
  //First bit 0 means class A
  if ((addr & 0X80000000) == 0x00000000)
    return 8;
  //First 2 bit 10 means class B
  else if ((addr & 0XC0000000) == 0x80000000)
    return 16;
  //First 3 bit 110 means class B
  else if ((addr & 0XE0000000) == 0xC0000000)
    return 24;
  else
   return 0;
}

//checks ip range sip-eip is within netid
//return 1 if valid, else 0
int
is_valid_net_range (unsigned int nid, unsigned int nbits, unsigned int sip, unsigned int eip)
{
  unsigned int Snip, Enip, num=1;
  Snip = nid +1;
  //num contains max hosts in the netid.
  num = num << (32 - nbits);

  //Note that nid starts from 0 hostid and last host id with all 1's is not valid
  //host id with all 1's is braodcast address
  Enip = nid + num - 2;

  if (IS_INCLUSIVE (sip, eip, Snip, Enip))
    return 1;
  else
    return 0;
}

//checks net-id IP ranges for net/nbits is exclsuive with other netid/nbits or
//they are same nets.
//return 1 if valid, else 0
int
is_netid_consistent (unsigned int nip, unsigned int nbits)
{
  unsigned int Snip1, Enip1, Snip2, Enip2, num1, num2;
  int i, j, total;
  IPRange *iprange_table;

  for (i = 0; i < 2; i++) {
    if (i == 0) {
      total = total_client_iprange_entries;
      iprange_table = cIPrangeTable;
    } else {
      total = total_server_iprange_entries;
      iprange_table = sIPrangeTable;
    }
    for (j = 0; j < total; j++) {
      num1 = 1;
      num2 = 1;
      Snip1 = nip;
      Snip2 = iprange_table[j].net_id;
      //num contains max hosts in the netid.
      num1 = num1 << (32-nbits);
      num2 = num2 << (32- (iprange_table[j].netbits));

      //Note that nip starts from 0 hostid and last host id with all 1's is included for compete net range
      //host id with all 1's is braodcast address
      Enip1 = Snip1 + num1 - 1;
      Enip2 = Snip2 + num2 - 1;

      if ((Snip1 == Snip2) && (nbits == iprange_table[j].netbits))
        continue;
      else if (IS_EXCLUSIVE (Snip1, Enip1, Snip2, Enip2))
        continue;
      else {
        printf("ERROR: Net-id IP ranges for netid/nbits not Exclusive with others on %s\n", i?"Server":"Client");
        return 0;

      }
    }// End inner for loop
  }
  return 1;
}

//checks IP range being assigned irsn unique. Check it with
//ip range for other ip ranges at clinet and server for same netid and
//also with any gatewasys at clinet and server.
//Futrtheri new gateway  as part of this entry should not be inclusive in other IP ranges.
//return 1 if valid, else 0
int
is_iprange_exclusive (unsigned int sip, unsigned int eip, unsigned int gateway, unsigned int nid)
{
  int i, j, total;
  IPRange *iprange_table;
  char str[20];

  for (i = 0; i < 2; i++) {
    if (i == 0) {
      total = total_client_iprange_entries;
      iprange_table = cIPrangeTable;
    } else {
      total = total_server_iprange_entries;
      iprange_table = sIPrangeTable;
    }
    for (j = 0; j < total; j++) {
      strcpy(str, ns_char_ip(iprange_table[j].start_ip));
      if (nid == iprange_table[j].net_id ) {
        if (!IS_EXCLUSIVE (sip, eip, iprange_table[j].start_ip, iprange_table[j].end_ip)) {
	  printf("ERROR: IP range being assigned is not unique with assigned IP range %s-%s on %s\n", str, ns_char_ip(iprange_table[j].end_ip), i?"Server":"Client");
	  return 0;
	}
      }
      if (IS_INCLUSIVE (iprange_table[j].gateway, iprange_table[j].gateway, sip, eip)) {
        printf("ERROR: IP range being assigned is not unique with assigned gateways %s on %s\n", ns_char_ip(iprange_table[j].gateway), i?"Server":"Client");
	return 0;
      }
      if (IS_INCLUSIVE (gateway, gateway, iprange_table[j].start_ip, iprange_table[j].end_ip)) {
        printf("ERROR: New gateway being assigned is not unique with assigned IP ranges %s-%s on %s\n", str, ns_char_ip(iprange_table[j].end_ip), i?"Server":"Client");
        return 0;
      }
    }//Inner for loop
  }
  return 1;
}


//checks Gateway specification validity. Checks
//for same netid and entity, same gateway
//if Gatweay given -  Gateway is part of the netid - NOT FOR THIS VERSION
//AND clinet/server both do not have same netid
//return 1 if valid, else 0
int
is_gateway_consistent (int entity, unsigned int gateway, unsigned int nid)
{
  int j, total;
  IPRange *iprange_table;

  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }
  for (j = 0; j < total; j++) {
    if (nid == iprange_table[j].net_id ) {
      if (gateway == NS_NO_GATEWAY) {
      	if ((iprange_table[j].gateway != NS_NO_GATEWAY) && (iprange_table[j].gateway != NS_SELF_GATEWAY))  {
          printf("ERROR: Gateway being assigned is not consistent with already assigned gateway for same netid\n");
	  return 0;
	}
      } else {
       	if (gateway != iprange_table[j].gateway ) {
          printf("ERROR: Gateway being assigned is not consistent with already assigned gateway for same netid\n");
          return 0;
	}
      }
    }
  }

  if (gateway != NS_NO_GATEWAY) {
    if (entity == NS_SERVER) {
      total = total_client_iprange_entries;
      iprange_table = cIPrangeTable;
    } else {
      total = total_server_iprange_entries;
      iprange_table = sIPrangeTable;
    }
    //Check if the opposite entity also has the same netid
    for (j = 0; j < total; j++) {
      if (nid == iprange_table[j].net_id ) {
	printf("ERROR: %s has also same netid. If Gateway given, then Client and Server both can't be same netid\n", entity?"Client":"Server");
	return 0;
      }
    }
  }
  return 1;
}


//netid can be only on one load interface
//Required for source based routing
//return 1 if valid, else 0
int
is_netid_only_on_one_load_interface (int entity, unsigned int nid, char *interface)
{
  int j, total;
  IPRange *iprange_table;

  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }

  for (j = 0; j < total; j++) {
    if ((nid == iprange_table[j].net_id )  && (strcmp (interface, iprange_table[j].LoadIF))) {
          printf("ERROR: IP's of one netid cannot spread across load interfaces.\n");
	  printf ("Use bodning if netid need to be shared across interfaces. \n");
	  printf ("Otherwise use IP's of different netid's\n");
	  return 0;
    }
  }

  return 1;
}

//return 1, it is new netid, else return 0
int is_new_netid( int entity, unsigned int nid)
{
  int self=0, j, total;
  IPRange *iprange_table;

  //Check self
  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }
  for (j = 0; j < total; j++) {
    if (nid == iprange_table[j].net_id ) {
      self = 1;
      break;
    }
  }

  if (self)
    return 0;
  else
    return 1;
}

//return 1, if yes else 0
int is_last_iprange_of_netid (int entity, unsigned int start_ip, unsigned int netid)
{
  int no_of_netid=0, j, total;
  IPRange *iprange_table;
  //Match in the IP range of the entity
  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }

  for (j = 0; j < total; j++) {
    if (netid == iprange_table[j].net_id ) {
      no_of_netid++;
    }
  }

  if (no_of_netid == 1)
    return 1;
  else
    return 0;
}

//return 1, if yes else 0
int is_last_iprange_of_vlanid (int entity, unsigned int start_ip, unsigned int vlan_id, char *LoadIF)
{
  int no_of_netid=0, j, total;
  IPRange *iprange_table;
  //Match in the IP range of the entity
  if (entity == NS_CLIENT) {
    total = total_client_iprange_entries;
    iprange_table = cIPrangeTable;
  } else {
    total = total_server_iprange_entries;
    iprange_table = sIPrangeTable;
  }

  for (j = 0; j < total; j++) {
    if ((vlan_id == iprange_table[j].vlan_id ) && (!strcmp (LoadIF, iprange_table[j].LoadIF ))) {
      no_of_netid++;
    }
  }

  if (no_of_netid == 1)
    return 1;
  else
    return 0;
}

/*************************
Atleast two inputs

Start-ip AND

End-ip OR total-ip

If (end-ip not given) Calculate total ip

If (total-ip not given) Calculate end-ip

If (netbits not given) netbits = get_netbits (start-ip)

If (net-id not given) net-id = get_netid (start-ip, netbits)

check, if start and end-ip;s are part of net-id

get_netbits(start ip) {

//Use the info in point 3 below

}

Get netid (start_ip, netbits) {

Create 32 bit netmask with first netbits 1 and rest 0. and AND with start-ip

}
********************************/

unsigned int
get_netid (unsigned int start_ip, int netbits)
{
  unsigned int net_id;
  //Create 32 bit netmask with first netbits 1 and rest 0. and AND with start-ip
  net_id = start_ip & ns_netmask[netbits];
  return net_id;
}

//All args are input/output
//returns 1 on failure and 0 on success
int
ns_autofill (char *start_ip, char *end_ip, char *total_ip, char *netbits, char *netid, char *vlanid, char *gateway, char *interface)
{
unsigned int startip, endip, totalip, nbits, nid, gw;
//int flag_is_end_ip=0, match = 1, i = 0; //Anuj, changed for warning
int flag_is_end_ip=0;
//char LoadIF[64], temp[50], *load_if[100], read_buf[50]; //Anuj, changed for warning
char LoadIF[64], temp[50], read_buf[50];
char use_first_ip_as_gateway[20], dut_layer[20];
FILE *fp_conf;
char work_dir[1024], ip_properties[50];

  if (!strcmp (start_ip, "-")) {
    printf("ERROR: Start IP must be provided\n");
    return 1;
  }else if (ns_ip_addr(start_ip, &startip)){
    printf("ERROR: Invalid start IP \n");
    return 1;
  }

  if (strcmp (end_ip, "-")) {
    if (ns_ip_addr(end_ip, &endip)){
      printf("ERROR: Invalid end IP \n");
      return 1;
    }
    flag_is_end_ip=1;
    if (startip > endip) {
      printf("ERROR: Start IP is greater than end IP \n");
      return 1;
    }
  }

  if (strcmp (total_ip, "-")) {
    totalip=atoi(total_ip);
    //Num IP should be greater than zero
    if (totalip <= 0){
      printf("ERROR: Invalid num ip\n");
      return 1;
    }
    if(flag_is_end_ip == 0){ //If end-ip not given
      endip = startip + totalip - 1;
      strcpy(end_ip,ns_char_ip(endip));
    } else {
      //Both end_ip and totalip given, check for consistemcy
      if (endip-startip+1 != totalip) {
      	printf("ERROR: total ip and IP range does not match\n");
        return 1;
      }
    }
  }else {
    if (flag_is_end_ip == 0){
      //End IP and num IP both have not given
      printf("End IP or num ip must be provided\n");
      return 1;
    } else {
      //If total-ip not given but end_ip given
      totalip = endip - startip + 1;
      sprintf(total_ip,"%d",totalip);
    }
  }

  if (strcmp (netbits, "-")) {
    nbits=atoi(netbits);
    if (nbits < 1 || nbits > 31) {
      printf("ERROR: netbits can be between 1-31 only\n");
      return 1;
    }
  } else {
    //If netbits not given
    nbits = ns_get_netbits (startip);
    if (!netbits) {
      printf("ERROR: netbits can't be deduced from start ip \n");
      return 1;
    }
    sprintf(netbits,"%d",nbits);
  }

  if (strcmp (netid, "-")) {
      if (ns_get_netid(netid, nbits, &nid)) {
        printf("ERROR: Invalid netid/netbits\n");
	return 1;
      }
  } else {
      //If net-id not given
      nid = get_netid (startip, nbits);
      strcpy(netid, ns_char_ip(nid));
  }

  if (strcmp(vlanid, "-") && (atoi(vlanid) < 0)) {
    printf("ERROR: Invalid vlanid. vlanid should be >=0 \n");
    return 1;
  }
  //Get NS work directory
  if (getenv("NS_WDIR") != NULL)
      strcpy(work_dir, getenv("NS_WDIR"));
  else{
      printf("ERROR: Did not found netstorm working directory\n");
      return 1;
  }

  sprintf(ip_properties, "%s/sys/ip_properties", work_dir);
  //Open config file
  if ((fp_conf = fopen(ip_properties, "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file %s\n", ip_properties);
    perror("fopen");
    return 1;
  }
  // read config information
  while (nslb_fgets(read_buf, MAX_LINE_LENGTH, fp_conf, 0)) {
    if (!strncmp(read_buf, "DUT_LAYER", 9)) {
           sscanf(read_buf,"%s %s", temp, dut_layer);
    } else if (!strncmp(read_buf, "USE_FIRST_IP_AS_GATEWAY", 23)) {
           sscanf(read_buf,"%s %s", temp, use_first_ip_as_gateway);
    }
  }
  fclose(fp_conf);


  if (strcmp (gateway, "-")) {
	  if (!strcmp(dut_layer, "2")) {
		  printf("ERROR: Can not specify gateway, DUT_LAYER is set to 2\n");
          return 1;
      }
      if (ns_ip_addr (gateway, &gw)){
    	  printf("ERROR: Invalid gateway\n");
    	  return 1;
      }
      //If Gateway is at start or end ip boundry, adjust range to exclude gateway.
      if (gw == startip && startip == endip){
          printf("ERROR: Invalid specification, Start IP, End Ip and Gateway can't be same\n");
          return 1;
      }
      if (gw == startip) {
    	  startip++;
    	  totalip--;
          strcpy(start_ip, ns_char_ip(startip));
          sprintf(total_ip, "%d", totalip);
      }

      if (gw == endip) {
    	  endip--;
    	  totalip--;
          sprintf(total_ip, "%d", totalip);
          strcpy(end_ip, ns_char_ip(endip));
      }
  } else if (!strcmp(dut_layer, "3") && !strcmp(use_first_ip_as_gateway, "1")) {
	  gw = nid + 1;
		  //If Gateway is at start or end ip boundry, adjust range to exclude gateway.
      if (gw == startip && startip == endip){
          printf("ERROR: Invalid specification, Start IP, End Ip and Gateway can't be same\n");
          return 1;
      }
      if (gw == startip) {
    	  startip++;
    	  totalip--;
          strcpy(start_ip, ns_char_ip(startip));
          sprintf(total_ip, "%d", totalip);
      }

      if (gw == endip) {
    	  endip--;
    	  totalip--;
          sprintf(total_ip, "%d", totalip);
          strcpy(end_ip, ns_char_ip(endip));
      }
	  strcpy(gateway, ns_char_ip(gw));
  } else if (!strcmp(dut_layer, "3") && !strcmp(use_first_ip_as_gateway, "0")) {
      printf("ERROR: Gateway required, Layer 3 device is between Netstorm and Netocean\n");
      return 1;
  }


  //Check, if start and end-ip are part of net-id
  if (!(is_valid_net_range (nid, nbits, startip, endip))){
     printf("ERROR: Start ip and end ip is not part of the specified net id \n");
     return 1;
  }

  //Validate interface
  //Open config file
  if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
    perror("fopen");
    return 1;
  }
  // read config information
  while (nslb_fgets(read_buf, MAX_LINE_LENGTH, fp_conf, 0)) {
    if (!strncmp(read_buf, "NSLoadIF", 8)) {
           sscanf(read_buf,"%s %s", temp, LoadIF);
    }
  }
  fclose(fp_conf);

  //Check valid interface
#if 0
  /this check removed as entity not available at the moment
  if (strcmp (interface, "-")) {
    for (load_if[i] = strtok(LoadIF,"|"); load_if[i] != NULL; load_if[i] = strtok(NULL, "|") ) {
      match = strcmp(load_if[i], interface);
      if (match == 0)
        break;
      i++;
    }
    if(match) {
      printf("ERROR: Interface %s not configured as Load Interface \n", interface);
      return 1;
    }
  } else {
	  strcpy(interface, strtok(LoadIF,"|"));
  }
#endif

  if (!strcmp (interface, "-"))
	  strcpy(interface, strtok(LoadIF,"|"));
  return 0;
}


// IPRange Table Creation function
//On success row num contains the newly created row-index of clinet IP Range
void
Create_client_iprange_entry(int *row_num)
{
  if (total_client_iprange_entries == max_client_iprange_entries) {
    cIPrangeTable = (IPRange *)Realloc((char *)cIPrangeTable,
           (max_client_iprange_entries + DELTA_IPRANGE_ENTRIES) * sizeof(IPRange),
           "montable");
    max_client_iprange_entries += DELTA_IPRANGE_ENTRIES;
  }
  *row_num = total_client_iprange_entries++;
}

//On success row num contains the newly created row-index of server IP Range
void
Create_server_iprange_entry(int *row_num)
{
  if (total_server_iprange_entries == max_server_iprange_entries) {
    sIPrangeTable = (IPRange *)Realloc((char *)sIPrangeTable,
            (max_server_iprange_entries + DELTA_IPRANGE_ENTRIES) * sizeof(IPRange),
            "montable");
    max_server_iprange_entries += DELTA_IPRANGE_ENTRIES;
  }
  *row_num = total_server_iprange_entries++;
}


/*NetStorm would have a new directory sys at the level of bin. This directory would not be overwritten on NetStorm upgrade.
Sys/ip_entries keeps the IP assignment onformation for Netstomr and NetOcean.
The format of this file is
Entity|netid|netbits|start-ip|last-ip|total-ip|vlan-id|gateway|primary-ip|exclude-ip
Entity is C, if entry for NetStorm and is S, if the entry id for NetOcean.
read_ip_entries(0 reads this file and create and fills in clinet and server IP entries table
*/

//this is dummy varibale . Need to remove after testing . 
int read_ip_entries ()
{
  FILE *fp;
  int row_num, i ,ip_type , row_num6;
  char read_buf[1024], *ptr[100], *start_ip; 
  int server_entry=0, client_entry=0;
  //ip_entries file is now no longer controller_specific . From now on ip_entries file will only be created in /home/netstorm/work/
  char file[] = "/home/cavisson/work/sys/ip_entries"; 
  //unsigned int net_id, netbits, last_ip, num_ip, vlan_id, gateway, primary_ip, exclude_ip;
  //Open input file
  if ((fp = fopen(file, "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file %s\n", file);
    perror(file);
    return 1;
  }

  memset (&assignedIPinfo, 0, sizeof(assignedIPinfo));

  while (nslb_fgets(read_buf, MAX_LINE_LENGTH, fp, 0)) {
    i = 0;
    char *line_ptr = read_buf;
    
    while ((*line_ptr == ' ') || (*line_ptr == '\t')) line_ptr++;

    if (strchr(line_ptr, '#')) 
      continue;
 
    for ( ptr[i] = strtok(line_ptr, "|"); ptr[i] != NULL; ptr[i] = strtok(NULL, "|") ) {
      i++;
      start_ip = ptr[1]; 
    }
    if ( i != 10) {
      printf("ERROR: ip_entries file is not in specified format \n");
      return 1;
    }
    if (!strcmp(ptr[0], "C")){
      client_entry = 1;
      server_entry = 0;
    }else if (!strcmp(ptr[0], "S")){
      server_entry = 1;
      client_entry = 0;
    }
  
    //Create cIPrangeTable and sIPrangeTable using ip_entries
    ip_type = check_ip_type(start_ip);
   
    if (ip_type ==  IPV4_ADDR) {
      
      if (client_entry) {
        assignedIPinfo.client_ipv4 = 1;
        Create_client_iprange_entry(&row_num);
        //Fill in table
        ns_ip_addr(ptr[1], &cIPrangeTable[row_num].net_id);
        cIPrangeTable[row_num].netbits = atoi(ptr[2]);
        ns_ip_addr(ptr[3], &cIPrangeTable[row_num].start_ip);
        ns_ip_addr(ptr[4], &cIPrangeTable[row_num].end_ip);
        cIPrangeTable[row_num].num_ip = atoi(ptr[5]);
        if (strcmp(ptr[6],"-"))
          cIPrangeTable[row_num].vlan_id=atoi(ptr[6]);
        else
          cIPrangeTable[row_num].vlan_id = NS_NO_VLAN;
        if (strcmp(ptr[7],"-"))
          ns_ip_addr(ptr[7], &cIPrangeTable[row_num].gateway);
        else
          cIPrangeTable[row_num].gateway = NS_NO_GATEWAY;
        if (strcmp(ptr[8],"-"))
          ns_ip_addr(ptr[8], &cIPrangeTable[row_num].exclude_ip);
      else
         cIPrangeTable[row_num].exclude_ip = 0;
        strcpy(cIPrangeTable[row_num].LoadIF, ptr[9]);
      if (cIPrangeTable[row_num].LoadIF[strlen(ptr[9]) - 1] == '\n')
         cIPrangeTable[row_num].LoadIF[strlen(ptr[9]) - 1] = '\0';

      cIPrangeTable[row_num].status = 0;

     } else if (server_entry) {
       assignedIPinfo.server_ipv4 = 1; 
       Create_server_iprange_entry(&row_num);
       //Fill in table
       ns_ip_addr(ptr[1], &sIPrangeTable[row_num].net_id);
       sIPrangeTable[row_num].netbits = atoi(ptr[2]);
       ns_ip_addr(ptr[3], &sIPrangeTable[row_num].start_ip);
       ns_ip_addr(ptr[4], &sIPrangeTable[row_num].end_ip);
       sIPrangeTable[row_num].num_ip = atoi(ptr[5]);
       if (strcmp(ptr[6],"-"))
         sIPrangeTable[row_num].vlan_id = atoi(ptr[6]);
       else
         sIPrangeTable[row_num].vlan_id = NS_NO_VLAN;
       if (strcmp(ptr[7],"-"))
         ns_ip_addr(ptr[7], &sIPrangeTable[row_num].gateway);
       else
         sIPrangeTable[row_num].gateway = NS_NO_GATEWAY;
       if (strcmp(ptr[8],"-"))
         ns_ip_addr(ptr[8], &sIPrangeTable[row_num].exclude_ip);
       else
         sIPrangeTable[row_num].exclude_ip = 0;
       strcpy(sIPrangeTable[row_num].LoadIF, ptr[9]);
       if (sIPrangeTable[row_num].LoadIF[strlen(ptr[9]) - 1] == '\n')
         sIPrangeTable[row_num].LoadIF[strlen(ptr[9]) - 1] = '\0';

       sIPrangeTable[row_num].status = 0;

     } else {
       printf("ERROR: Invalid Entity\n");
       fclose(fp);
       return 1;
    }
  }
  else if (ip_type ==  IPV6_ADDR)
  {
    if (client_entry) {
      assignedIPinfo.client_ipv6 = 1;
      Create_client_ipv6range_entry(&row_num6);
      //Fill in table
      strcpy(cIPV6rangeTable[row_num6].net_id, ptr[1]);
      strcpy(cIPV6rangeTable[row_num6].netbits, ptr[2]);
      strcpy(cIPV6rangeTable[row_num6].start_ip, ptr[3]);
      strcpy(cIPV6rangeTable[row_num6].end_ip, ptr[4]);
      strcpy(cIPV6rangeTable[row_num6].num_ip, ptr[5]);
      if (strcmp(ptr[6],"-"))
        strcpy(cIPV6rangeTable[row_num6].vlan_id, ptr[6]);
      else
        strcpy(cIPV6rangeTable[row_num6].vlan_id, NS_NO_VLAN_IPV6);
      if (strcmp(ptr[7],"-"))
        strcpy(cIPV6rangeTable[row_num6].gateway, ptr[7]);
      else
        strcpy(cIPV6rangeTable[row_num6].gateway, NS_NO_GATEWAY_IPV6);
      if (strcmp(ptr[8],"-"))
        strcpy(cIPV6rangeTable[row_num6].exclude_ip, ptr[8]);
      else
        strcpy(cIPV6rangeTable[row_num6].exclude_ip,"-");
      strcpy(cIPV6rangeTable[row_num6].LoadIF, ptr[9]);
      if (cIPV6rangeTable[row_num6].LoadIF[strlen(ptr[9]) - 1] == '\n')
         cIPV6rangeTable[row_num6].LoadIF[strlen(ptr[9]) - 1] = '\0';

      cIPV6rangeTable[row_num6].status = 0;
    }
    else if (server_entry) {
      assignedIPinfo.server_ipv6 = 1; 
      Create_server_ipv6range_entry(&row_num6);
       //Fill in table
      strcpy(sIPV6rangeTable[row_num6].net_id, ptr[1]);
      strcpy(sIPV6rangeTable[row_num6].netbits, ptr[2]);
      strcpy(sIPV6rangeTable[row_num6].start_ip, ptr[3]);
      strcpy(sIPV6rangeTable[row_num6].end_ip, ptr[4]);
      strcpy(sIPV6rangeTable[row_num6].num_ip, ptr[5]);
      if (strcmp(ptr[6],"-"))
        strcpy(sIPV6rangeTable[row_num6].vlan_id, ptr[6]);
      else
        strcpy(sIPV6rangeTable[row_num6].vlan_id, NS_NO_VLAN_IPV6);
      if (strcmp(ptr[7],"-"))
        strcpy(sIPV6rangeTable[row_num6].gateway, ptr[7]);
      else
        strcpy(sIPV6rangeTable[row_num6].gateway, NS_NO_GATEWAY_IPV6);
      if (strcmp(ptr[8],"-"))
        strcpy(sIPV6rangeTable[row_num6].exclude_ip, ptr[8]);
      else
        strcpy(sIPV6rangeTable[row_num6].exclude_ip,"-");
      strcpy(sIPV6rangeTable[row_num6].LoadIF, ptr[9]);
      if (sIPV6rangeTable[row_num6].LoadIF[strlen(ptr[9]) - 1] == '\n')
        sIPV6rangeTable[row_num6].LoadIF[strlen(ptr[9]) - 1] = '\0';

      sIPV6rangeTable[row_num6].status = 0;

    } else {
       printf("ERROR: Invalid Entity\n");
       fclose(fp);
       return 1;
     }
  }

   }//End while
   fclose(fp);
   return 0;
}


int write_ip_entries ()
{
  FILE *fp;
  int row_num;
  char net_id[50], start_ip[50], end_ip[50], gateway[50];
  char exclude_ip[50], vlan_id[20], netbits[10], num_ip[10];
  // IP entries file is now no longer Controller specific . It will now only be created in /home/netstorm/work
  char file[] = "/home/cavisson/work/sys/ip_entries";

  if ((fp = fopen(file, "w")) == NULL) {
    fprintf(stderr, "ERROR: in opening file %s\n", file);
    perror("fopen");
    return 1;
  }

 
  if (total_client_iprange_entries ) { //current IPv4  entres for client (netstorm)
 
    for (row_num=0; row_num < total_client_iprange_entries ; row_num++) {

      strcpy(net_id, ns_char_ip(cIPrangeTable[row_num].net_id));
      snprintf(netbits, sizeof(unsigned int), "%u",  cIPrangeTable[row_num].netbits);
      strcpy(start_ip, ns_char_ip(cIPrangeTable[row_num].start_ip));
      strcpy(end_ip, ns_char_ip(cIPrangeTable[row_num].end_ip));
      snprintf(num_ip, sizeof(unsigned int), "%u",  cIPrangeTable[row_num].num_ip);

      if (cIPrangeTable[row_num].vlan_id != NS_NO_VLAN)
        sprintf(vlan_id, "%d", cIPrangeTable[row_num].vlan_id);
      else
        strcpy(vlan_id, "-");

      if (cIPrangeTable[row_num].gateway != NS_NO_GATEWAY)
        strcpy(gateway, ns_char_ip(cIPrangeTable[row_num].gateway));
      else
        strcpy(gateway, "-");

      if (cIPrangeTable[row_num].exclude_ip)
        strcpy(exclude_ip, ns_char_ip(cIPrangeTable[row_num].exclude_ip));
      else
        strcpy(exclude_ip, "-");

      fprintf(fp, "C|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", net_id, netbits, start_ip, end_ip, num_ip, vlan_id, gateway, exclude_ip, cIPrangeTable[row_num].LoadIF);
    }
  }

  if (total_server_iprange_entries) { // current ipv4 server entries  
    for (row_num=0; row_num < total_server_iprange_entries ; row_num++) {
      strcpy(net_id, ns_char_ip(sIPrangeTable[row_num].net_id));
      snprintf(netbits, sizeof(unsigned int), "%u",  sIPrangeTable[row_num].netbits);
      strcpy(start_ip, ns_char_ip(sIPrangeTable[row_num].start_ip));
      strcpy(end_ip, ns_char_ip(sIPrangeTable[row_num].end_ip));
      snprintf(num_ip, sizeof (unsigned int) , "%u",  sIPrangeTable[row_num].num_ip);

      if (sIPrangeTable[row_num].vlan_id != NS_NO_VLAN)
        sprintf(vlan_id, "%d", sIPrangeTable[row_num].vlan_id);
      else
        strcpy(vlan_id, "-");

      if (sIPrangeTable[row_num].gateway != NS_NO_GATEWAY)
        strcpy(gateway, ns_char_ip(sIPrangeTable[row_num].gateway));
      else
        strcpy(gateway, "-");

      if (sIPrangeTable[row_num].exclude_ip)
        strcpy(exclude_ip, ns_char_ip(sIPrangeTable[row_num].exclude_ip));
      else
        strcpy(exclude_ip, "-");

      fprintf(fp, "S|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", net_id, netbits, start_ip, end_ip, num_ip, vlan_id, gateway, exclude_ip, sIPrangeTable[row_num].LoadIF);
   }
  }

  if (total_client_ipv6range_entries) { //IPv6 entries for client (Netstorm)
    for (row_num=0; row_num < total_client_ipv6range_entries ; row_num++) {
      strcpy(net_id, cIPV6rangeTable[row_num].net_id);
      strcpy(netbits, cIPV6rangeTable[row_num].netbits);
      strcpy(start_ip, cIPV6rangeTable[row_num].start_ip);
      strcpy(end_ip, cIPV6rangeTable[row_num].end_ip);
      strcpy(num_ip, cIPV6rangeTable[row_num].num_ip);

      if (strcmp(cIPV6rangeTable[row_num].vlan_id, NS_NO_VLAN_IPV6) == 0)
        sprintf(vlan_id, "%s", cIPV6rangeTable[row_num].vlan_id);
      else
        strcpy(vlan_id, "-");

      if (strcmp(cIPV6rangeTable[row_num].gateway, NS_NO_GATEWAY_IPV6) == 0)
        strcpy(gateway, cIPV6rangeTable[row_num].gateway);
      else
        strcpy(gateway, "-");

      if (strcmp(cIPV6rangeTable[row_num].exclude_ip, "-") == 0)
        strcpy(exclude_ip, cIPV6rangeTable[row_num].exclude_ip);
      else
        strcpy(exclude_ip, "-");

      fprintf(fp, "C|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", net_id, netbits, start_ip, end_ip, num_ip, vlan_id, gateway, exclude_ip,
           cIPV6rangeTable[row_num].LoadIF);
    }
  }
  if (total_server_ipv6range_entries) { //IPV6 entries for client (Netocean)
    for (row_num=0; row_num < total_server_ipv6range_entries ; row_num++) {
      strcpy(net_id, sIPV6rangeTable[row_num].net_id);
      strcpy(netbits, sIPV6rangeTable[row_num].netbits);
      strcpy(start_ip, sIPV6rangeTable[row_num].start_ip);
      strcpy(end_ip, sIPV6rangeTable[row_num].end_ip);
      strcpy(num_ip, sIPV6rangeTable[row_num].num_ip);

      if (strcmp(sIPV6rangeTable[row_num].vlan_id, NS_NO_VLAN_IPV6) == 0)
        sprintf(vlan_id, "%s", sIPV6rangeTable[row_num].vlan_id);
      else
        strcpy(vlan_id, "-");

      if (strcmp(sIPV6rangeTable[row_num].gateway, NS_NO_GATEWAY_IPV6) == 0)
        strcpy(gateway, sIPV6rangeTable[row_num].gateway);
      else
        strcpy(gateway, "-");

      if (strcmp(sIPV6rangeTable[row_num].exclude_ip, "-") == 0)
        strcpy(exclude_ip, sIPV6rangeTable[row_num].exclude_ip);
      else
        strcpy(exclude_ip, "-");

      fprintf(fp, "S|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", net_id, netbits, start_ip, end_ip, num_ip, vlan_id, gateway,
            exclude_ip, sIPV6rangeTable[row_num].LoadIF);
    }
  }

  fclose(fp);
  return 0;
}


void
delete_IPRange_table_entry(int entity, unsigned int sip)
{
int i, total, row_index;
IPRange* iprange_table;

    if (entity == NS_CLIENT) {
	    total = total_client_iprange_entries;
	    iprange_table = cIPrangeTable;
    } else {
	    total = total_server_iprange_entries;
	    iprange_table = sIPrangeTable;
    }

    for (row_index = 0; row_index < total; row_index++) {
	if (sip == iprange_table[row_index].start_ip)
		break;
    }

    //Copy one entry up
    for (i = row_index; i+1 < total; i++) {
	iprange_table[i] = iprange_table[i+1];
    }

    //We shifted one entry up
    //Last entry in empty now
    if (entity == NS_CLIENT) {
	   total_client_iprange_entries--;
    } else {
	   total_server_iprange_entries--;
    }
}

//Rteurn s 1 on failure 0 on success
//Calls nsi_add_ip and reads prirmay ip from stdout
int
do_add_ip (int entity, unsigned int start_ip, unsigned int end_ip,
	int vid, int nbits, unsigned int exclude_ip, unsigned int *pip, unsigned int nid,
	unsigned int gw, char *load )
{
    char work_dir[1024];
    char cmd[1024]="";
    char output[4096], primary_ip[4096], st_ip[20], en_ip[20], gateway[20], netid[20];
    unsigned int admin_netid; //Admin netid: client or server
    //FILE *app = NULL;
    int status;

    if (getenv("NS_WDIR") != NULL)
     	strcpy(work_dir, getenv("NS_WDIR"));
    else{
     	printf("ERROR: Did not found netstorm working directory\n");
     	return 1;
    }

    strcpy(st_ip, ns_char_ip(start_ip));
    strcpy(en_ip, ns_char_ip(end_ip));

    if (gw == NS_NO_GATEWAY) {
        strcpy(gateway, "-");
    } else {
        strcpy(gateway, ns_char_ip(gw));
    }

    //If address beiing added in admin netid, do not create src based route
    ns_get_netid(entity?g_cavinfo.SRAdminIP : g_cavinfo.NSAdminIP,
		 entity? g_cavinfo.SRAdminNetBits : g_cavinfo.NSAdminNetBits , &admin_netid);
    admin_netid = get_netid(admin_netid, entity? g_cavinfo.SRAdminNetBits : g_cavinfo.NSAdminNetBits);
    if ( nid != admin_netid)
        strcpy(netid, ns_char_ip(nid));
    else
        strcpy(netid, "-");

    //pass netid and gw in dottet notaion to nsi_add_ip
    sprintf(cmd, "%s/bin/nsi_add_ip %s %s %s %s %d %s %s %s %s %d", work_dir, entity?"S":"C",
	    st_ip, en_ip,
	    ns_char_vlanid (vid), nbits,
	    exclude_ip?ns_char_ip(exclude_ip):"-",
	    netid, gateway, load, IPV4_ADDR);
#if 0
    if (is_new_netid(entity, nid)) {
        if (gw == NS_NO_GATEWAY) {
            strcpy(gateway, "SELF");
	} else {
            strcpy(gateway, ns_char_ip(gw));
	}
	strcpy(netid, ns_char_ip(nid));
	//pass netid and gw in dottet notaion to nsi_add_ip
	sprintf(cmd, "%s/bin/nsi_add_ip %s %s %s %s %d %s %s %s %s", work_dir, entity?"S":"C",
	    st_ip, en_ip,
	    ns_char_vlanid (vid), nbits,
	    exclude_ip?ns_char_ip(exclude_ip):"-",
	    netid, gateway, load);
    } else {
        //pass - - for netid and gw to nsi_add_ip
        sprintf(cmd, "%s/bin/nsi_add_ip %s %s %s %s %d %s %s %s %s", work_dir, entity?"S":"C",
	    st_ip, en_ip,
	    ns_char_vlanid (vid), nbits,
	    exclude_ip?ns_char_ip(exclude_ip):"-",
	    "-", "-", load);
    }
#endif

    memset(primary_ip,0x0,4096);
    memset(output,0x0,4096);
    primary_ip[0] = '\0';

    status = nslb_run_and_get_cmd_output(cmd, 4096, output);
    if (status == -1) {
      fprintf(stderr, "Error in execution of cmd %s \n", cmd);
      exit(-1);
    }
    if(strlen(output) == 0){
           strcpy(primary_ip,output);
        } else {
           strcat(primary_ip,output);
        }

     if(!strncmp(primary_ip, "Error:",6) ){
         printf("%s",primary_ip);
         return 1;
      }

    status = strlen(primary_ip);
    primary_ip[status] = '\0';
   
    memset(output , 0x0, 4096);
    /*
    if(!strcmp(primary_ip, "You must be logged in as cavisson user to execute this command\n") ){
         printf("ERROR: You must be logged in as cavisson user to execute this command\n");
         return 1;
    }
    //return with error if server is not available : arun 03/13/08
    if(!strcmp(primary_ip, "Server is not available !\n") ){
         printf("ERROR: Server is not available !\n");
         return 1;
    }
    */
    //If primary == sip, save as primary
    //And set the primary if IPs are creaeated for nay range
    ns_ip_addr (primary_ip, pip);

   /* if(pclose(app) == -1)
    	printf("ERROR : pclose() FAILED\n");
   */
    return 0;
}




//Rteurn s 1 on failure 0 on success
//Calls nsi_del_ip and reads prirmay ip from stdout
int
do_del_ip (int entity, unsigned int start_ip, unsigned int end_ip,
	unsigned int vid, int nbits, unsigned int exclude_ip, unsigned int nid,
	unsigned int gw, char *load )
 {
    char work_dir[1024];
    char cmd[1024]="",gateway[20],netid[20];
    char temp[MAX_MSG], output[MAX_MSG], st_ip[20], en_ip[20], vlan_id[20];
    FILE *app = NULL;
    int status, del_vlan = 0;
    unsigned int admin_netid;

    if (getenv("NS_WDIR") != NULL)
     	strcpy(work_dir, getenv("NS_WDIR"));
    else{
     	printf("ERROR: Did not found netstorm working directory\n");
     	return 1;
    }

    strcpy(st_ip, ns_char_ip(start_ip));
    strcpy(en_ip, ns_char_ip(end_ip));

    if(vid != NS_NO_VLAN) {
         sprintf(vlan_id, "%d", vid);
         del_vlan = is_last_iprange_of_vlanid(entity, start_ip,  vid, load);
    } else {
         strcpy(vlan_id, "-");
    }

    if (is_last_iprange_of_netid(entity, start_ip,  nid)) {
        if (gw == NS_NO_GATEWAY) {
             strcpy(gateway, "SELF");
	} else {
             strcpy(gateway, ns_char_ip(gw));
	}
        //If address beiing deleted  in admin netid, do not delete  src based route
        ns_get_netid(entity?g_cavinfo.SRAdminIP : g_cavinfo.NSAdminIP,
		 entity? g_cavinfo.SRAdminNetBits : g_cavinfo.NSAdminNetBits , &admin_netid);
        admin_netid = get_netid(admin_netid, entity? g_cavinfo.SRAdminNetBits : g_cavinfo.NSAdminNetBits);

        if ( nid != admin_netid)
            strcpy(netid, ns_char_ip(nid));
        else
            strcpy(netid, "-");

        //pass netid and gw in dottet notaion to nsi_add_ip
        sprintf(cmd, "%s/bin/nsi_del_ip %s %s %s %s %d %s %s %s %s %d %d", work_dir, entity?"S":"C",
        st_ip, en_ip, vlan_id, nbits,
        exclude_ip?ns_char_ip(exclude_ip):"-", netid, gateway, load, del_vlan , IPV4_ADDR);
    } else {
        //pass - - for netid and gw to nsi_add_ip
        sprintf(cmd, "%s/bin/nsi_del_ip %s %s %s %s %d %s %s %s %s %d %d", work_dir, entity?"S":"C",
        st_ip, en_ip, vlan_id, nbits,
        exclude_ip?ns_char_ip(exclude_ip):"-","-","-", load, del_vlan, IPV4_ADDR);
    }

    memset(output,0x0,MAX_MSG);
    memset(temp,0x0,MAX_MSG);
    output[0] = '\0';

    app = popen(cmd, "r");
    if(app == NULL){
         printf("ERROR: popen failed for command %s. Error: %s\n", cmd, nslb_strerror(errno));
         return 1;
    }

    while (!feof(app)) {
    	status = fread(temp, 1, MAX_MSG, app);
        if(status <= 0){
           pclose(app);
           //printf("fread() NO DATA");
           memset(temp,0x0,MAX_MSG);
           return 0;
        }
        if(strlen(temp) == 0){
           strcpy(output,temp);
        } else {
           strcat(output,temp);
	}
	memset(temp,0x0,MAX_MSG);
    }

    if(!strcmp(output, "You must be logged in as cavisson user to execute this command\n") ){
         printf("ERROR: You must be logged in as cavisson user to execute this command\n");
         return 1;
    }

    if(pclose(app) == -1)
    	printf("ERROR : pclose() FAILED\n");

    return 0;
}

//Returns 1 on failure 0 on success
//Calls nsi_get_primary and reads primary ip from stdout
int do_get_primary (int entity, unsigned int start_ip, int nbits, int vid, unsigned int *pip, char *eth)
{
    char cmd[1024]="", work_dir[512];
    char temp[MAX_MSG], primary_ip[MAX_MSG], vlan_id[32];
    FILE *app = NULL;
    int status;

    if (vid != NS_NO_VLAN)
        sprintf(vlan_id, "%d", vid);
    else
        strcpy(vlan_id, "-");

    //Get NS work directory
    if (getenv("NS_WDIR") != NULL)
        strcpy(work_dir, getenv("NS_WDIR"));
    else{
        printf("ERROR: Did not found netstorm working directory\n");
        return 1;
    }

    sprintf(cmd, "%s/bin/nsi_get_primary %s %s/%d %s %s", work_dir, entity?"S":"C",
	ns_char_ip(start_ip), nbits, vlan_id, eth);

    memset(primary_ip,0x0,MAX_MSG);
    memset(temp,0x0,MAX_MSG);
    primary_ip[0] = '\0';

    app = popen(cmd, "r");
    if(app == NULL){
        printf("ERROR: popen failed for command %s. Error: %s\n", cmd, nslb_strerror(errno));
        return 1;
    }

    while (!feof(app)) {
    	status = fread(temp, 1, MAX_MSG, app);
        if(status <= 0){
            pclose(app);
            printf("ERROR: fread() NO DATA");
            memset(temp,0x0,MAX_MSG);
            return 1;
        }
        if(strlen(temp) == 0){
            strcpy(primary_ip,temp);
        } else {
            strcat(primary_ip,temp);
	}
	memset(temp,0x0,MAX_MSG);
    }

    if (strcmp(primary_ip, "0"))
        ns_ip_addr (primary_ip, pip);
    else
	*pip=atoi(primary_ip);

    if(pclose(app) == -1)
    	printf("ERROR : pclose() FAILED\n");

    return 0;
}

