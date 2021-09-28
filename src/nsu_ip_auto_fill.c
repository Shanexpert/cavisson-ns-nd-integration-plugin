/******************************************************************
 * Name    :    ns_autofill.c
 * Author  :    Sanjay
 * Purpose :    It will auto fill endip, totalip, netbit, netid
 * Usage:  nsu_ip_auto_fill netid netbits start_ip end_ip total_ip vlanid gateway interface
           *    Minimum two values must be known
		   *    1. start-ip AND 2. end-ip OR totalip
		   *    vlan-id and gateway will not be autofiiled
		   *    Unknown values will be specified as -
 * Modification History:
 *   02/05/06: Sanjay - Initial Version
 *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipmgmt_utils.h"
#include "nsu_ipv6_utils.h"

int main(int argc, char *argv[])
{
  char start_ip[50], end_ip[50], total_ip[20], netbits[20], netid[50], vlanid[20], gateway[50], interface[20], ip_type[10];
  int ret, type=1;

  if(argc != 10){
    printf("ERROR: Invalid arguments\n");
    printf("Usage: %s netid netbits start_ip end_ip total_ip vlanid gateway interface ip_type\n", argv[0]);
    return 1;
  }
  
  strcpy(netid, argv[1]);
  strcpy(netbits, argv[2]);
  strcpy(start_ip, argv[3]);
  strcpy(end_ip, argv[4]);
  strcpy(total_ip, argv[5]);
  strcpy(vlanid, argv[6]);
  strcpy(gateway, argv[7]);
  strcpy(interface, argv[8]);
  strcpy(ip_type, argv[9]);

  if ( !strcmp(ip_type,"-") ){
    type=check_ip_type(start_ip);
    if ( type == IPV4_ADDR )
      strcpy(ip_type, "IPv4");
    else if ( type == IPV6_ADDR )
      strcpy(ip_type, "IPv6");
    else
      printf("Error: Invalid IP");
  }

  if ( !strcasecmp(ip_type,"IPv4"))
    ret = ns_autofill(start_ip, end_ip, total_ip, netbits, netid, vlanid, gateway, interface);
  else
    ret = ns_autofill_ipv6(start_ip, end_ip, total_ip, netbits, netid, vlanid, gateway, interface);
 
  if (ret)
    return 1;

  printf("netid|netbits|start_ip|end_ip|total_ip|vlanid|gateway|interface|ip_type\n");
  printf("%s|%s|%s|%s|%s|%s|%s|%s|%s\n",  netid, netbits, start_ip, end_ip, total_ip, vlanid, gateway, interface, ip_type);
  return 0;
}
