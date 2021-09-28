/******************************************************************
 * Name    :    nsu_add_ip.c
 * Author  :    Sanjay
 * Purpose :    Assign allias IP for client and server
 * Usage:  nsu_assign_ip [-s|-h]   network_id/netbits|ip_range|vlan_id|gateway
           * -h flag displays usage
		   * -s flag suggest that IP's need to be assigned to NetOcean (instead of NetStorm)
		   * ip_range may be specified as startip:num-ip or startip - endip
 * Modification History:
 *   06/05/06: Sanjay - Initial Version
 *   29/06/06: Sanjay - Update to support source based routing.
 *   08/07/06: Sanjay - Update code for not writting primary IP  of assiging IPs in ip_entires file.
 *                    - Add code to syncronize ip entries file on NetOcean. 
 *   24/07/06: Sanjay - Updated for support multiple interface
  *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ipmgmt_utils.h"
#include "nslb_cav_conf.h"
#include "nsu_ipv6_utils.h"


static char start_ip[50], end_ip[50]="-", num_ip[20]="-", netbits[20], netid[50], vlanid[40], gateway[50], interface[20];
int ip_type;


//return 0 on success and 1 on failure
int 
ns_add_ip_entry (int entity) 
{
  unsigned int sip, eip, nip=0, nbits, nid, vid, gw, exip, pip;
  char work_dir[1024];
  int  row_num;

  //convert all addresses to unsigned int form and
  //complete the missing data
  
    if (ns_autofill (start_ip, end_ip, num_ip, netbits, netid, vlanid, gateway, interface)){
	return 1;
    }

    if (entity < 0 || entity > 1){
        printf("ERROR: Entity is invalid\n");
    	return 1;
    }
    
    ns_ip_addr (start_ip, &sip);

    nip = atoi(num_ip);

    ns_ip_addr (end_ip, &eip);

    nbits = atoi (netbits); 	
    
    ns_get_netid(netid, nbits, &nid);
    
    if (strcmp(vlanid, "-")){
    	vid = atoi(vlanid);
        if ( strcmp(vlanid, ns_char_vlanid(vid))){
           printf("ERROR: Invalid vlanid. vlanid should be >=0 \n");
           return 1;
        }
    } else
    	vid = NS_NO_VLAN;

    if (!strcmp (gateway, "-"))
      	gw =(unsigned int) NS_NO_GATEWAY;
    else 
	ns_ip_addr (gateway, &gw);


   //Read ip_entries file
   if (getenv("NS_WDIR") != NULL)
     strcpy(work_dir, getenv("NS_WDIR"));
   else{
     printf("ERROR: Did not found netstorm working directory\n");
     return 1;
   }

   if (read_ip_entries ()){
     printf("ERROR: read_ip_entries() failed\n");
     return 1;
   }

   //Do input validation

   //Check if the netid is consistent with other netid's
   if (!is_netid_consistent (nid, nbits)){
     printf("ERROR: Net id not consistent\n");
     return 1;
   }
	
   //Check if new IP range is exclusive
   if (!is_iprange_exclusive (sip, eip, gw, nid)){
     printf("ERROR: Ip range not exclusive\n");
     return 1;
   }

   //Check if new gateway is consistent with earlier settings
   if (!is_gateway_consistent (entity, gw, nid)){
     printf("ERROR: Gateway not consistent\n");
     return 1;
   }

   //Check if one netid is one one ineterface only
   if (!is_netid_only_on_one_load_interface (entity, nid, interface)){
     printf("ERROR: NetId cannot be split across load interfaces\n");
     return 1;
   }

   //Chcek if gateway IP should be excluded (if gateway address falls within IP range to be assigned)
   if (IS_INCLUSIVE(gw, gw, sip, eip))
     exip = gw;
   else
     exip = 0;

   //Call command nsi_add_ip save the outout of the cmd as primary
   if (do_add_ip (entity, sip, eip, vid, nbits, exip, &pip, nid, gw, interface)) {
    	printf("ERROR: Adding IP failed \n");  
    	return 1;
   }

   //Add entry
   if (!entity) {
      Create_client_iprange_entry(&row_num);
      //Fill in table
      cIPrangeTable[row_num].net_id = nid;
      cIPrangeTable[row_num].netbits = nbits;
      cIPrangeTable[row_num].start_ip = sip;
      cIPrangeTable[row_num].end_ip = eip;
      cIPrangeTable[row_num].num_ip = nip;
      cIPrangeTable[row_num].vlan_id = vid;
      cIPrangeTable[row_num].gateway = gw; 
      cIPrangeTable[row_num].exclude_ip = exip;
      strcpy(cIPrangeTable[row_num].LoadIF, interface);
    } else {
       Create_server_iprange_entry(&row_num);
       //Fill in table
       sIPrangeTable[row_num].net_id = nid;
       sIPrangeTable[row_num].netbits = nbits;
       sIPrangeTable[row_num].start_ip = sip;
       sIPrangeTable[row_num].end_ip = eip;
       sIPrangeTable[row_num].num_ip = nip;
       sIPrangeTable[row_num].vlan_id = vid;
       sIPrangeTable[row_num].gateway = gw;
       sIPrangeTable[row_num].exclude_ip = exip;
       strcpy(sIPrangeTable[row_num].LoadIF, interface);
    } 

   //re-write ip_entries file
   if (write_ip_entries ()){
     printf("ERROR:  write_ip_entries() failed\n");
     return 1;
   }
    
   printf("IP assignment completed!\n");
   if (entity){
     strcat(work_dir,"/bin/nsi_sync_netocean");
     system(work_dir);
   }
   return 0;
}
/*--------------------------------------------------------------------------------
//Purpose: to add ipv6 entries
//Arguments: entity 
//Return:0 for success 1 for error 

---------------------------------------------------------------------------------*/
int ns_add_ipv6_entry (int entity)
{ 
  char exclude_ip[50]="-";  //exclude IP
  char primary_ip[50]="-";  //primary IP
  char work_dir[1024];
  int  row_num;

  //complete the missing data
  if (ns_autofill_ipv6 (start_ip, end_ip, num_ip, netbits, netid, vlanid, gateway, interface)){
    return 1;
  }

  //check value of entity should be 0 or 1
  if (entity < 0 || entity > 1){
    printf("ERROR: Entity is invalid\n");
    return 1;
  }

  //Read ipv6_entries file
  if (getenv("NS_WDIR") != NULL)
    strcpy(work_dir, getenv("NS_WDIR"));
  else{
    printf("ERROR: Did not found netstorm working directory\n");
    return 1;
  }

  if (read_ip_entries ()){
    printf("ERROR: read_ip_entries() failed\n");
    return 1;
  }

  //check for duplicate entries 
  if (is_ipv6range_exclusive (entity, start_ip, end_ip)){
    printf("ERROR: Ip range not exclusive\n");
    return 1;
  }

  //Call command nsi_add_ipv6 to add ips  
  if (do_add_ipv6 (entity, start_ip, end_ip, vlanid, netbits, exclude_ip, primary_ip, netid, gateway, interface)) {
    printf("ERROR: Adding IP failed \n");
    return 1;
  }

  //Add entry
  if (!entity) {
    Create_client_ipv6range_entry(&row_num);
    //Fill in table
    strcpy(cIPV6rangeTable[row_num].net_id, netid);
    strcpy(cIPV6rangeTable[row_num].netbits, netbits);
    strcpy(cIPV6rangeTable[row_num].start_ip, start_ip);
    strcpy(cIPV6rangeTable[row_num].end_ip, end_ip);
    strcpy(cIPV6rangeTable[row_num].num_ip, num_ip);
    strcpy(cIPV6rangeTable[row_num].vlan_id, vlanid);
    strcpy(cIPV6rangeTable[row_num].gateway, gateway);
    strcpy(cIPV6rangeTable[row_num].exclude_ip, exclude_ip);
    strcpy(cIPV6rangeTable[row_num].LoadIF, interface);
  } 
  else {
    Create_server_ipv6range_entry(&row_num);
    //Fill in table
    strcpy(sIPV6rangeTable[row_num].net_id, netid);
    strcpy(sIPV6rangeTable[row_num].netbits, netbits);
    strcpy(sIPV6rangeTable[row_num].start_ip, start_ip);
    strcpy(sIPV6rangeTable[row_num].end_ip, end_ip);
    strcpy(sIPV6rangeTable[row_num].num_ip, num_ip);
    strcpy(sIPV6rangeTable[row_num].vlan_id, vlanid);
    strcpy(sIPV6rangeTable[row_num].gateway, gateway);
    strcpy(sIPV6rangeTable[row_num].exclude_ip, exclude_ip);  
    strcpy(sIPV6rangeTable[row_num].LoadIF, interface);
  }

  //re-write ipv6_entries file
  if (write_ip_entries()){
     printf("ERROR: write_ip_entries() failed\n");
     return 1;
  } 

  printf("\nIP Assignment completed!\n");
  if (entity){
     strcat(work_dir,"/bin/nsi_sync_netocean");
     system(work_dir);
   }

  return 0;
} 



void print_usage(char *str);

int main(int argc, char *argv[])
{
  int entity=NS_CLIENT;
  int result, argnum = 1, i = 0, match = 1;
  char *arg[100], config[50], read_buf[50], temp[50];
  char *ptr;
  FILE *fp_conf;
  char LoadIF[64], fname[1024];
   
  nslb_init_cav();


#if 0
//Open config file
  if ((fp_conf = fopen(/home/cavisson/etc/cav.conf, "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /etc/cav.conf\n");
    perror("fopen");
    return 1;
  }
  // read config information
  while (fgets(read_buf, MAX_LINE_LENGTH, fp_conf)) {
    if (!strncmp(read_buf, "CONFIG", 6)){
      sscanf(read_buf,"%s %s", temp, config);
    }else if (!strncmp(read_buf, "NSLoadIF", 8)) {
           sscanf(read_buf,"%s %s", temp, LoadIF);
    }else if (!strncmp(read_buf, "SRLoadIF", 8)){
           sscanf(read_buf,"%s %s", temp, SRLoadIF);
    }
  }
#endif

  //For NS+NO, NO IP Management allowed.
  if (!strcmp(g_cavinfo.config, "NS+NO")) {
    printf("ERROR: Test Configuration (NS+NO). NO IP Management allowed\n");
    return 1;
  }

 
  if (argc < 2) {
    print_usage(argv[0]); 
    exit(1);
  } 
 
  //process cmd line arguments
  if (!strcasecmp ("-h", argv[1])) {
    print_usage(argv[0]);
    exit(0);
  }

  //Open properties file
  sprintf (fname, "%s/bin/sys/ip_properties", g_ns_wdir);
  if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /etc/cav.conf\n");
    perror("fopen");
    return 1;
  }
  // read reserved netid information
  while (fgets(read_buf, MAX_LINE_LENGTH, fp_conf)) {
    if (!strncmp(read_buf, "RESERVED_NETID", 14)){
      sscanf(read_buf,"%s %s", temp, config);
      break;
    }
  }

  fclose(fp_conf);


  if (!strcasecmp ("-s", argv[argnum])) {
    entity=NS_SERVER;
    argnum++;
	strcpy(LoadIF, g_cavinfo.SRLoadIF);
  } else
	strcpy(LoadIF, g_cavinfo.NSLoadIF);

  //
  for ( arg[i] = strtok(argv[argnum],"|"); arg[i] != NULL; arg[i] = strtok(NULL, "|") )
  {
    //printf("%s\n",trim(ip[i]));
    i++;
  }

  if ( i != 7)
  {
    print_usage(argv[0]);
     exit(1);
  }

  if(strchr(arg[0], '/')){
    strcpy(netid, strtok(arg[0],"/"));  
    ptr = strtok(NULL,"/");
    if (ptr)
      strcpy(netbits, ptr);
    else {
      printf("ERROR: Netid and netbits must be separated with forward slash \n");
      return 1;
    }
  }else {
    printf("ERROR: Netid and netbits must be separated with forward slash \n");
    return 1;
  }

  //For reserved netid allowed.
  if (!strcmp(config, netid)) {
    printf("ERROR: %s is a reserved netid and cnnot be used for IP assignment.\n", netid);
    return 1;
  }

  //For start_ip
  strcpy(start_ip, arg[1]);
  if ( (strcmp(start_ip, "-") == 0) || (start_ip == NULL))
  {
     printf("start ip cannot be \"-\" or NULL \n");
     return 1;
  }
 
  //For end_ip and ip range 

  strcpy(end_ip, arg[2]);

  strcpy(num_ip,arg[3]);
 
  if ((strcmp(end_ip, "-") == 0) && ((strcmp(num_ip, "-")==0) ))
  {
    printf("Error : Both end_ip and num_ip can not be -\n");
    printf("Either provide range or end_ip\n");
    return 1;
  }


  strcpy(vlanid, arg[4]);
  strcpy(gateway, arg[5]);
  strcpy(interface, arg[6]);

  //Check valid interface
  i = 0;
  for ( arg[i] = strtok(LoadIF,"|"); arg[i] != NULL; arg[i] = strtok(NULL, "|") )
  {
    match = strcmp(arg[i], interface);
    if (match == 0)
      break;
    i++;
  }
  if(match) {
	  if (!strcmp(interface, "-") && i == 1)
		  strcpy(interface, arg[0]);
	  else if(!strcmp(interface, "-") && i > 1) {
		  printf("ERROR: Must provide interafce name\n");
		  return 1;
	  } else {
          printf("ERROR: Interface %s not configured as Load Interface \n", interface);
          return 1;
	  }
  }

  ip_type=check_ip_type(netid);
  if ( ip_type == IPV4_ADDR )
    result = ns_add_ip_entry(entity); 
  else if ( ip_type == IPV6_ADDR )
    result = ns_add_ipv6_entry(entity);
      
return result;
}


void
print_usage(char *str)
{
  printf("Usage: %s [-s|-h]   network_id/netbits|start_ip|end_ip|num ip|vlan_id|gateway|interface\n\n", str);
  printf("  -s < Server : flag suggest that IP's need to be assigned to NetOcean (instead of NetStorm) >\n");
  printf("  -h < Help : This Flag is used to print usage>\n");
  printf("   ip_range may be specified as Start_IP-End_IP\n");
}


