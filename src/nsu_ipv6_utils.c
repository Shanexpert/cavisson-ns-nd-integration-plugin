/******************************************************************
 * Name    :    ipv6_utils.c
 * Author  :    Abhay
 * Purpose :    Functions for ipv6 management
******************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "nsu_ipv6_utils.h"
#include "nslb_util.h"

IPV6Range *cIPV6rangeTable = NULL;
IPV6Range *sIPV6rangeTable = NULL;

int total_client_ipv6range_entries = 0; //current IPV6 entries for client (netstorm)
int total_server_ipv6range_entries = 0; //current IPV6 entries for server (netocean)
static int max_client_ipv6range_entries; //max buf in IPV6 entries for client (netstorm)
static int max_server_ipv6range_entries; //max buf in IPV6 entries for server (netocean)

struct in6_addr start_ip_bin,end_ip_bin;
char buf[INET6_ADDRSTRLEN];
char temp[5],ipv6_exp_addr[50],ipv6_std_addr[50],ipv6_end_ip[50];
static int  value[8],value_start_ip[8],value_end_ip[8];
//static int sip[8] , eip[8] ; 
int i,j=0,start,num_ips;


/*------------------------------------------------------------------
Function to allocate memory for IPV6 Range Tables
Returns pointer to the allocated memory
-------------------------------------------------------------------*/
static void * Realloc(char *ptr, int size, char *msg)
{
        void *p;
        p = realloc((void *)ptr, size);
        return p;
}


/*-----------------------------------------------------------------
Function to check if given ip is ipv4 or ipv6
Returns 0 on succes else -1 on failure

-------------------------------------------------------------------*/

int check_ip_type(char *addr)
{
  struct addrinfo hint, *res = NULL;
  int ret;

  memset(&hint, '\0', sizeof hint);
  hint.ai_family = PF_UNSPEC;
  hint.ai_flags = AI_NUMERICHOST;
  ret = getaddrinfo(addr, NULL, &hint, &res);
  if (ret) {
    puts(gai_strerror(ret));
    return -1;
  }

  if (res->ai_family == AF_INET) {
    //printf("%s is an ipv4 address\n",addr);
    freeaddrinfo(res);
    return IPV4_ADDR;
  }

  else if (res->ai_family == AF_INET6) {
   // printf("%s is an ipv6 address\n",addr);
    freeaddrinfo(res);
    return IPV6_ADDR;
  }
 
  else {
    printf("%s is an is unknown address format %d\n",addr,res->ai_family);
    freeaddrinfo(res);
    return -1;
  }
return -1;
}


/*-------------------------------------------------------------------------------
  Purpose  : function to calculate end_ip when start_ip and number of ips are given
  Arguments: variable description::
  Return   : 0 for success -1 for error 

--------------------------------------------------------------------------------*/

int find_end_ipv6 (char * start_ip, int num_ips, char *end_ip)
{

  if (inet_pton(AF_INET6,start_ip,&start_ip_bin)!=1){
    printf("Couldn't parse '%s' as an IPv6 address\n",start_ip);
               return -1;
  }

  for (i=0,j=0;i<16;i=i+2,j++)
  {
    sprintf(temp,"%02x%02x",start_ip_bin.s6_addr[i],start_ip_bin.s6_addr[i+1]);
    value[j]=strtol(temp,0,16);
  }
  
  for (i=0;i<num_ips-1;i++)
  {
    if( value[7] + 1 <= 65535 )
       value[7] += 1;
    else
      {
       value[7]=0;
       if(value[6]+1<=65535)
         value[6]+=1;
       else
       {
         value[6]=0;
         if(value[5]+1<=65535)
            value[5]+=1;
         else
         {
	   value[5]=0;
           if(value[4]+1<=65535)
              value[4]+=1;
           else
           {
	     value[4]=0;
             if(value[3]+1<=65535)
                value[3]+=1;
             else
             {
               value[3]=0;
               if(value[2]+1<=65535)
                  value[2]+=1;
               else
               {
                 value[2]=0;
                 if(value[1]+1<=65535)
                   value[1] += 1; 
		   else {
                     value[1]=0;
                     if(value[0]+1<=65535)
                       value[0]+=1;
                   }
               }
             }
           }
         }
       }
    }
  }

  for (i=0;i<8;i++)
  {
    sprintf(temp,"%04x:",value[i]);
    strcat(ipv6_end_ip,temp);
  }

  ipv6_end_ip[strlen(ipv6_end_ip)-1]='\0';
  inet_pton(AF_INET6,ipv6_end_ip,&end_ip_bin);
  inet_ntop(AF_INET6,&end_ip_bin, buf, sizeof buf);
  strcpy(end_ip,buf);
  return 0;
}


/*---------------------------------------------------------------------
Function to calculate number of ips when start_ip and end_ip is given
Returns 0 on success and 1 on failure

-----------------------------------------------------------------------*/
int find_total_ips( char * start_ip, char * end_ip, char*total_ip)
{
  if (inet_pton(AF_INET6,start_ip,&start_ip_bin)!=1){
    printf("Couldn't parse '%s' as an IPv6 address\n",start_ip);
    return 1;
  }
   
  if (inet_pton(AF_INET6,end_ip,&end_ip_bin)!=1){
    printf("Couldn't parse '%s' as an IPv6 address\n",end_ip);
    return 1;
  }

  for (i=0,j=0; i<16; i=i+2,j++)
  {
    sprintf(temp,"%02x%02x",start_ip_bin.s6_addr[i],start_ip_bin.s6_addr[i+1]);
    value_start_ip[j]=strtol(temp,0,16);
  }
    
  for (i=0,j=0; i<16; i=i+2,j++)
  {
    sprintf(temp,"%02x%02x",end_ip_bin.s6_addr[i],end_ip_bin.s6_addr[i+1]);
    value_end_ip[j]=strtol(temp,0,16);
  }

  for (i=0; i<8; i++)
  {
    if (value_start_ip[i]>value_end_ip[i]){
      printf("\nstart ip cannot be greater than end ip \n");
      return 1;
    }

  }

  num_ips=value_end_ip[7] - value_start_ip[7] + 1;
  sprintf(total_ip,"%d",num_ips);

  return 0;
}
               
/*-------------------------------------------------------------------------
//Function to complete the missing data
//Return 0 on success else -1 on failure

----------------------------------------------------------------------------*/

int ns_autofill_ipv6 (char *start_ip, char *end_ip, char *total_ip, char *netbits,
                      char *netid, char *vlanid, char *gateway, char *interface)
{
  char LoadIF[64], temp[50], read_buf[50];
  char use_first_ip_as_gateway[20], dut_layer[20];
  FILE *fp_conf;
  char work_dir[1024];
  char ip_properties[50];
  unsigned int totalip;
  char *env_ptr=NULL;


  // check for start ip
  if ( !strcmp (start_ip, "-") || !strcmp (netbits, "-") || !strcmp (netid, "-") ) {
    printf("ERROR: Network ID, Netbits and Start IP must be provided\n");
    return -1;
  }

  if (check_ip_type(netid) != IPV6_ADDR){
    printf("ERROR: Invalid Netid\n");
    return -1;
  } 

  if (check_ip_type(start_ip) != IPV6_ADDR){
    printf("ERROR: Invalid Start IP\n");
    return -1;
  } 
  //netbits should be between 1-128
  if ( atoi(netbits) < 1 || atoi(netbits) > 128 ){
    printf("ERROR: netbits should be between 1-128 only\n");
    return -1;
  }

  if ( !strcmp(end_ip, "-") && !strcmp (total_ip, "-") ){
    printf("ERROR: Both End IP and Number of IPs cannot be Null\n");
    return -1;
  }

  // check for end ip
  if (!strcmp(end_ip, "-")) 
    find_end_ipv6(start_ip, atoi(total_ip), end_ip);

  // check for total number of ips
  if (strcmp (total_ip, "-")) {
    //Num IP should be greater than zero
    if (atoi(total_ip) <= 0){
      printf("ERROR: Invalid num ip %s\n",total_ip);
      return -1;
    }
  }
  else {
    if (find_total_ips( start_ip, end_ip, total_ip )) {
      return -1;
    }
  }

  
  if (strcmp(vlanid, "-") && (atoi(vlanid) < 0)) {
    printf("ERROR: Invalid vlanid. vlanid should be >=0 \n");
    return -1;
  }  

  env_ptr = getenv("NS_WDIR"); 
  //Get NS work directory
  if(env_ptr != NULL){
    strcpy(work_dir, env_ptr);
  }
  else {
    printf("ERROR: Did not found netstorm working directory\n");
    return -1;
  }
  sprintf(ip_properties, "%s/sys/ip_properties", work_dir);

  //Open config file
  if ((fp_conf = fopen(ip_properties, "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /sys/ip_properties\n");
    perror("fopen");
    return -1;
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
      return -1;
    }
      
    //If Gateway is at start or end ip boundry, adjust range to exclude gateway.
    if ( !strcmp(gateway,start_ip) && !strcmp(start_ip,end_ip) ){
      printf("ERROR: Invalid specification, Start IP, End Ip and Gateway can't be same\n");
      return -1;
    }

    if ( !strcmp(gateway,start_ip) ) {
      find_end_ipv6(start_ip, 1, start_ip);
      totalip=atoi(total_ip);
      totalip--;
      sprintf(total_ip, "%d", totalip);
      
    }

/*TODO:
    if (gw == endip) {
      endip--;
      totalip--;
      sprintf(total_ip, "%d", totalip);
      strcpy(end_ip, ns_char_ip(endip));
      }
*/

  } else if (!strcmp(dut_layer, "3") && !strcmp(use_first_ip_as_gateway, "1")) {
      find_end_ipv6(gateway, 1, gateway);

      //If Gateway is at start or end ip boundry, adjust range to exclude gateway.
      if ( !strcmp(gateway,start_ip) && !strcmp(start_ip,end_ip) ){
          printf("ERROR: Invalid specification, Start IP, End Ip and Gateway can't be same\n");
          return -1;
      } 

      if ( !strcmp(gateway,start_ip) ) {
        find_end_ipv6(start_ip, 1, start_ip);
        totalip=atoi(total_ip);
        totalip--;
        sprintf(total_ip, "%d", totalip);

      }

/*TODO:
        if (gw == endip) {
          endip--;
          totalip--;
          sprintf(total_ip, "%d", totalip);
          strcpy(end_ip, ns_char_ip(endip));
      }
*/  

        
  } else if (!strcmp(dut_layer, "3") && !strcmp(use_first_ip_as_gateway, "0")) {
      printf("ERROR: Gateway required, Layer 3 device is between Netstorm and Netocean\n");
      return -1;
  }

  

  //Validate interface
  //Open config file
  if ((fp_conf = fopen("/home/cavisson/etc/cav.conf", "r")) == NULL) {
    fprintf(stderr, "ERROR: in opening file /home/cavisson/etc/cav.conf\n");
    perror("fopen");
    return -1;
  }
  // read config information
  while (nslb_fgets(read_buf, MAX_LINE_LENGTH, fp_conf, 0)) {
    if (!strncmp(read_buf, "NSLoadIF", 8)) {
           sscanf(read_buf,"%s %s", temp, LoadIF);
    }
  }
  fclose(fp_conf);

  if (!strcmp (interface, "-"))
    strcpy(interface, strtok(LoadIF,"|"));

  return 0;

}  


/*-------------------------------------------------------------------------------------------------
Function to add ips by calling the shell nsi_add_ipv6
Returns 1 on failure 0 on success

--------------------------------------------------------------------------------------------------*/

int do_add_ipv6 ( int entity, char *start_ip, char *end_ip, char *vlanid, char *netbits, char *exclude_ip, 
                  char *primary_ip, char *netid, char *gateway, char *interface)
{ 
  char work_dir[1024];
  char cmd[1024]=""; 
  char output[4096]="";
  char cmd_output[4096]="";
  int app;
  
  if (getenv("NS_WDIR") != NULL)
    strcpy(work_dir, getenv("NS_WDIR"));
  else {
    printf("ERROR: Did not found netstorm working directory\n");
    return 1;
  }  
  sprintf(cmd, "%s/bin/nsi_add_ip %s %s %s %s %s %s %s %s %s %d",work_dir, entity?"S":"C",
          start_ip, end_ip, vlanid, netbits, exclude_ip, netid, gateway, interface, IPV6_ADDR);

  app = nslb_run_and_get_cmd_output(cmd, 4096, output);
  if (app == -1) {
    fprintf(stderr, "Error in execution of cmd %s \n", cmd);
    exit(-1);
  }

  if (strlen(output) == 0){
    strcpy(cmd_output,output);
  }
  else {
    strcat(cmd_output,output);
  }
 
  if (!strncmp(cmd_output, "Error:",6) ){
    printf("%s",cmd_output);
    return 1;
  }

  memset(output , 0x0, 4096);
  return 0;

}

/*-------------------------------------------------------------------------------
  Function to create Client IPV6 Range Table 
  On success row num contains the newly created row-index of clinet IPV6 Range

--------------------------------------------------------------------------------*/

void Create_client_ipv6range_entry(int *row_num)
{
  if (total_client_ipv6range_entries == max_client_ipv6range_entries) {
    cIPV6rangeTable = (IPV6Range *)Realloc((char *)cIPV6rangeTable,
                      (max_client_ipv6range_entries + DELTA_IPV6RANGE_ENTRIES) * sizeof(IPV6Range),
                       "montable");
    max_client_ipv6range_entries += DELTA_IPV6RANGE_ENTRIES;
  }

  *row_num = total_client_ipv6range_entries++;
}

/*------------------------------------------------------------------------------
  Function to create Server IPV6 Range Table
  On success row num contains the newly created row-index of server IPV6 Range

--------------------------------------------------------------------------------*/
void Create_server_ipv6range_entry(int *row_num)
{
  if (total_server_ipv6range_entries == max_server_ipv6range_entries) {
    sIPV6rangeTable = (IPV6Range *)Realloc((char *)sIPV6rangeTable,
                      (max_server_ipv6range_entries + DELTA_IPV6RANGE_ENTRIES) * sizeof(IPV6Range),
                      "montable");
    max_server_ipv6range_entries += DELTA_IPV6RANGE_ENTRIES;
  }

  *row_num = total_server_ipv6range_entries++;
}

/*-------------------------------------------------------------------------
Function to delete IPV6 Range Table Entry which is later used to update the 
file /home/cavisson/work/sys/ip_entries

---------------------------------------------------------------------------*/

    
void
delete_IPV6Range_table_entry(int entity, char *start_ip)
{
int i, total, row_index;
IPV6Range* ipv6range_table;

  if (entity == NS_CLIENT) {
    total = total_client_ipv6range_entries;
    ipv6range_table = cIPV6rangeTable;
  }
  else {
    total = total_server_ipv6range_entries;
    ipv6range_table = sIPV6rangeTable;
  }

  for (row_index = 0; row_index < total; row_index++) {
    if (strcasecmp(start_ip,ipv6range_table[row_index].start_ip) == 0 )
    break;
  }

  //Copy one entry up
  for (i = row_index; i+1 < total; i++) {
    ipv6range_table[i] = ipv6range_table[i+1];
  }

  //We have shifted one entry up
  //Last entry is empty now
  if (entity == NS_CLIENT) 
    total_client_ipv6range_entries--;
  else
    total_server_ipv6range_entries--;
  
}


/*-----------------------------------------------------------------------------
Function to delete assigned IPs
Returns -1 on failure 0 on success
Calls Shell nsi_del_ipv6 

------------------------------------------------------------------------------*/
int
do_del_ipv6 (int entity, char *start_ip, char *end_ip, char *vid, char *nbits, 
             char *exclude_ip, char *nid, char *gw, char *load )
{
  char work_dir[1024];
  char cmd[1024]="";
  //char gateway[20],netid[20];
  char temp[MAX_MSG], output[MAX_MSG];
  //char st_ip[20], en_ip[20], vlan_id[20];
  FILE *app = NULL;
  int status;
  //int del_vlan = 0;
  //unsigned int admin_netid;

  if (getenv("NS_WDIR") != NULL)
    strcpy(work_dir, getenv("NS_WDIR"));
  else {
    printf("ERROR: Did not found netstorm working directory\n");
    return 1;
  }
  sprintf(cmd, "%s/bin/nsi_del_ip %s %s %s %s %d %s %s %s %s %d %d", work_dir, entity?"S":"C", start_ip, end_ip, vid, atoi(nbits), exclude_ip, "-", "-", load, atoi(vid) , IPV6_ADDR);
    
  memset(output,0x0,MAX_MSG);
  memset(temp,0x0,MAX_MSG);
  output[0] = '\0';

  app = popen(cmd, "r");
  if (app == NULL){
    printf("ERROR: popen failed\n");
    return 1;
  }

  while (!feof(app)) {
    status = fread(temp, 1, MAX_MSG, app);
    if (status <= 0){
      pclose(app);
      //printf("fread() NO DATA");
      memset(temp,0x0,MAX_MSG);
      return 0;
    }
    if (strlen(temp) == 0){
           strcpy(output,temp);
    } else {
        strcat(output,temp);
    }
        memset(temp,0x0,MAX_MSG);
  }

  if (!strcmp(output, "You must be logged in as cavisson user to execute this command\n") ){
    printf("ERROR: You must be logged in as cavisson user to execute this command\n");
    return 1;
  }

  if (pclose(app) == -1)
    printf("ERROR : pclose() FAILED\n");

    return 0;
}

int is_ipv6range_exclusive (int entity, char *start_ip, char *end_ip)
{
  IPV6Range *IPrangeTable;
  int i , total ;
  unsigned long previous_sip[2] ;  // stores allocated start ip 
  unsigned long previous_eip[2] ; // stores allocated end ip 
  unsigned long  sip[2] ; // stores current start ip  
  unsigned long  eip[2] ; // stores current endip
  unsigned char buf[sizeof(struct in6_addr)];
  unsigned int addr[4];
  int domain = AF_INET6;

  
  if (!entity) { 
    total = total_client_ipv6range_entries;
    IPrangeTable = cIPV6rangeTable; 
  }
  else 
  {
    total = total_server_ipv6range_entries;
    IPrangeTable = sIPV6rangeTable;
  }

  //evaluating curreny start ip 
   
  inet_pton(domain, start_ip, buf);
  memcpy(addr, buf, sizeof(buf));

  sip[0] = ntohl(addr[0]);
  sip[0] = sip[0] << 32;
  sip[0] = sip[0] | ntohl(addr[1]);

  sip[1] = ntohl(addr[2]);
  sip[1] = sip[1] << 32;
  sip[1] = sip[1] | ntohl(addr[3]);
   
  //evaluating current end ip 
  inet_pton(domain, end_ip, buf);
  memcpy(addr, buf, sizeof(buf));
  eip[0] = ntohl(addr[0]);
  eip[0] = eip[0] << 32;
  eip[0] = eip[0] | ntohl(addr[1]);

  eip[1] = ntohl(addr[2]);
  eip[1] = eip[1] << 32;
  eip[1] = eip[1] | ntohl(addr[3]);


  for (i = 0; i< total; i++)
  {
    //step 1: evaluating values  for previously assigned start ip
    inet_pton(domain,  IPrangeTable[i].start_ip, buf);
    memcpy(addr, buf, sizeof(buf));
    previous_sip[0] = ntohl(addr[0]);  
    previous_sip[0] = previous_sip[0] << 32;
    previous_sip[0] = previous_sip[0] | ntohl(addr[1]);
    
    previous_sip[1] = ntohl(addr[2]);
    previous_sip[1] = previous_sip[1] << 32;
    previous_sip[1] = previous_sip[1] | ntohl(addr[3]);

   //step 2:: evaluating values for previsouly assigned end ip 
    inet_pton(domain,  IPrangeTable[i].end_ip, buf);
    memcpy(addr, buf, sizeof(buf));
    previous_eip[0] = ntohl(addr[0]);
    previous_eip[0] = previous_eip[0] << 32;
    previous_eip[0] = previous_eip[0] | ntohl(addr[1]);

    previous_eip[1] = ntohl(addr[2]);
    previous_eip[1] = previous_eip[1] << 32;
    previous_eip[1] = previous_eip[1] | ntohl(addr[3]);

    
    //Case 1 ) if start ip is equal to previous assigned start ip 
    if (sip[0] == previous_sip[0] && sip[1] == previous_sip[1])
    {
      printf("ERROR: IP range being assigned is not unique with assigned IP range %s-%s on %s\n", start_ip, IPrangeTable[i].end_ip, entity?"Server":"Client");
      return 1; 
    }

    //case 2) if start ip lies in range of assigned ip 
    if (sip[0] == previous_sip[0]) {
      if (((sip[1] < previous_sip[1]) && (eip[1] < previous_sip[1])) || ((sip[1] > previous_eip[1]) && (eip[1] > previous_eip[1]))) {
        return 0;
      }
      else {
        printf("ERROR: IP range being assigned is not unique with assigned IP range %s-%s on %s\n", start_ip, IPrangeTable[i].end_ip, entity?"Server":"Client");
        return 1;
     }
   }
    
    // case 3 ) if end ip is equal to end ip 
    if (eip[0] ==  previous_eip[0] && eip[1] == previous_eip[1]){
      printf("ERROR: IP range being assigned is not unique with assigned IP range %s-%s on %s\n", start_ip, IPrangeTable[i].end_ip, entity?"Server":"Client");
      return 1; 
    }

    // case 4 ) if start ip is equal to previously assigned end ip 
    if (sip[0] == previous_eip[0] && sip[1] == previous_eip[1]) {
      printf("ERROR: IP range being assigned is not unique with assigned IP range %s-%s on %s\n", start_ip, IPrangeTable[i].end_ip, entity?"Server":"Client");
      return 1;
    }
   
 }
return 0; 
}
