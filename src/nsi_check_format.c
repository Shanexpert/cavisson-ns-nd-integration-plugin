/**********************************************************************************************************************
  File 		:nsu_check_format
  Author	:Sanjana Joshi/Nishi Mishra
  Date 		:28th April 2016
  Synopsis	:This program will validate IP address format
 		 convert the IP address to numeric for the purpose
		 of calculating range of IP's. It validate the IP
		 based on the IP version type
		 IPv4 => 1 (version type)
		 Ipv6 => 2 (version type)
  exit status	: O for success
		  1 for failure
  Modified By	: Tanmay Prakash
  Description	: Provided support for IPv6 in this program
		  It will now convert last octet of IPv6 address to numeric form for
 		  calculating the range and also validate IPv6
		  format and version.Modified the usage to provide more information to user
  Date		: 05th May 2016


  Usage	:-
     -> To check IP address format:
        nsi_check_format -i ip-address -v ip-version 
     -> To check IP address format and output numeric IP on stdout : 
        nsi_check_format -I ip-address -v ip_version
     -> To check number format: 
        nsi_check_format -n number -v ip_version
     -> To check ip:num_ip format: 
        nsi_check_fromat -x ip-num -v ip_version
     -> To check number format and output the number on stdout:  
        nsi_check_format -N number -v ip_version 
     -> To check ip:num_ip format and output numeric IP and number on stdout, space separated: 
        nsi_check_fromat -X ip-num -v ip_version
**********************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>


#define _MAX_ERR_BUF_SIZE_ 2048

void usage(char *errstr)
{
  if(errstr && errstr[0])
    fprintf(stderr, "\nError: %s\n", errstr);

  fprintf(stderr, "USAGE:\n"
           "-v <version type: 1 for IPv4; 2 for IPv6>\n"
           "-i <IP_address: will not print to console>\n"
           "-I <IP_address: will print to console>\n"
           "-n <No.of IP's: will not print to console>\n"
           "-N <No.of IP's: will print to console>\n"
           "-x <Give both start IP and number of IP>\n"
           "-X <Give both start IP and number of IP, output will print on console>\n"
           "-r <Range of start ip and end ip>\n"
           "-R <Range of start ip and end ip, output will print ion console>\n"
           "    Note: '-v' argument is mandatory, you must specify the version type of IP\n"
           "Start IP and number of IP must be hyphen separated, so as for range start ip and end ip must be hyphen separate\n\n"
           "    E.g.:nsi_check_format -v 1 -X 192.168.12.2-3 <start_ip-num_ip>\n"
           "         nsi_check_format -v 1 <IP version > -N  6 <number>\n"
           "         nsi_check_format -v 2 -R 2001::3-2001::6 <start_ip-end_ip>\n");

  exit(1);
}


int convert_ipv4_to_numeric(char *ip_address, unsigned long *num_ip) 
{
  char tmp_buf[1024]; 
  unsigned long num=0, value=0;
  char *tok,*ptr;
  
  strcpy(tmp_buf, ip_address); 
  tmp_buf[strlen(ip_address)] = '\0';

  tok=strtok(tmp_buf, "."); /* looks for the '.' in between numbers */

  while( tok != NULL)
  {
    value = strtoul(tok, &ptr, 0); /* puts the first number in 'val' */
    num = (num << 8) + value; /* stores it, moving over 8 bits each time */
    tok = strtok(NULL, "."); /* gets the next number */
  }
 
  *num_ip = num;
  return 0;
}


/*************************************************************************************************
  
Tanmay	:This function convert the last Octet of IPv6 address to Numeric, the purpose of
	 converting it to numeric is to calculate the range of IP's
  	IPv6 address is made of 128 bit, divided into 8 Block of 16 bit:
                      _______________________________________
                     |____|____|____|____|____|____|____|____|<- convert last octet to numeric
                       1    2    3    4    5    6    7    8
                   
E.g. IPv6 address	   = 2001:0db8:85a3:0000:0000:8a2e:0370:7334
     Numeric of last octet = 57701172
**************************************************************************************************/
static void convert_ipv6_to_numeric(char *ip_address, unsigned long *num_ip)
{ 
  //int inet_check;
  char str[INET6_ADDRSTRLEN];
  unsigned char buf[sizeof(struct in6_addr)];
  int domain = AF_INET6;
  //inet_check = inet_pton(domain, ip_address, buf);
  
  inet_pton(domain, ip_address, buf);
  unsigned int addr[4];
  memcpy(addr, buf, sizeof(buf));
  *num_ip = ntohl(addr[3]); 

  //printable format IP address
  inet_ntop(domain, buf, str, INET6_ADDRSTRLEN);
}


/**************************************************************************
Validates ip address to a particular family type 
**************************************************************************/
int validate_address(char *ip_address , int *ip_type)
{
  struct addrinfo hint, *res = NULL;
  int ret;

  memset(&hint, '\0', sizeof hint);

  hint.ai_family = PF_UNSPEC;
  hint.ai_flags = AI_NUMERICHOST;

  ret = getaddrinfo(ip_address, NULL, &hint, &res);
  if (ret) {
      puts(gai_strerror(ret));
      return 1;
  }
  if(res->ai_family == AF_INET) {
      *ip_type = 4;
  } else if (res->ai_family == AF_INET6) {
      *ip_type = 6;
  } else {
      *ip_type = 1;
    return 1;
  }

 freeaddrinfo(res);
 return 0;
}


/**************************************************************************************
 This function tokenizes ip address and range and displays it in console if required 
***************************************************************************************/
int check_ip_and_range(char *ip_address, int ip_version_type, int is_display, char *errbuf)
{
  //char *ptr = NULL; 
  char ip[256];
  char *range; 
  int ip_type , ret ;
  char *tok; 
  unsigned long num_ip;
  int dot = 0 ; 

 
  if (ip_version_type == 1)
  {
    //copying ip_address to ip 
    strcpy(ip, ip_address);
    tok = ip ; 
    while (*tok != '\0')
    {
      if (*tok != '.' )
      {
        tok++;
      } else {
        dot ++;
        tok++;
      }
    }

    if (dot != 3)
    {
      return 1; 
    }
  
    // seperate ip and range 
    range = strstr(ip, "-");
    if (range == NULL)
    {
      printf("Error invalid format\n");
      return 1; 
    }
    if (*range != '\0')
    {
      *range = '\0';
      range = range +1; 
    }

    ret = validate_address(ip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "unable to validate as ipv4 address");
      usage(errbuf);
      return 1;
    }
    else
    {
      convert_ipv4_to_numeric(ip , &num_ip);
    }
    

    if (is_display)
    {
      printf("%lu %s\n", num_ip , range);
    }

  }
  else if (ip_version_type == 2) {
    strcpy(ip, ip_address);
    tok = ip;
    if (*tok == ':' &&  *(tok + 1) == ':')
    {
      tok+= 2;
      if (isalnum(*tok) == 0)
        return 1;
    }

    // seperate ip and range 
    range = strstr(ip, "-");
    if (range == NULL)
    {
      sprintf(errbuf,"Invalid format \n ");
      usage(errbuf); 
      return 1; 
    }
    else if (*range != '\0')
    {
      *range = '\0';
      range = range +1; 
    }

    ret = validate_address(ip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as a valid ipv6 address ");
      usage(errbuf);
      return 1;
    }
    else{
      convert_ipv6_to_numeric(ip, &num_ip);
    }

    if (is_display)
    {
      if (ip_type == 6)
          printf("%lu %s\n", num_ip , range);
        //printf("%s %s \n", ip, range );

    }
 }


//TODO::Check for ipv6 

return 0;
}


/************************************************************************
 This function takes input as address and convert ip address in numeric 
 format . 
 Returns 0 as sucess and 1 as error 
*************************************************************************/
int validate_number (char *ip_address, int ip_version_type, int is_display)
{
  char *ptr = ip_address;
  if (ip_version_type == 1 || ip_version_type == 2) {
    while (*ptr != '\0')
    {
      if (*ptr < '0' || *ptr > '9') {
        return 1;
      }
      ptr ++;
    }

    if (is_display)
    {
      printf("%d \n", atoi(ip_address));
    }
  }
return 0;
}


/*****************************************************************************************
This function takes ip address , ip version and is_display as input and returns
0 as sucess  and 1 as error  
   Input : ip_addrss(ipv4/ipv6), ipv version , 0/1. 1 for displaying in console
   output: 0 sucess 
           1 error 
This function Validates ip address based on ip_version type 
******************************************************************************************/
int validate_ip(char *ip_address, int ip_version_type, int is_display, char *errbuf)
{
  int ret;
  int ip_type; 
  unsigned long num_ip; 
  int inet_check;
  //char str[INET6_ADDRSTRLEN];
  unsigned char buf[sizeof(struct in6_addr)];
 
  if (ip_version_type == 1) { //this means ip address passed is an ipv4 address .
    int domain=AF_INET;
    inet_check = inet_pton(domain, ip_address, buf);
    if (inet_check <= 0)
    {
      if (inet_check == 0){
        sprintf(errbuf, "IP address is not in valid format or not valid");
        usage(errbuf);
      }
      else{
        sprintf(errbuf,"inet_pton error, invalid IP family");
        usage(errbuf);
      }
      exit(1);
    }

     //validate the IP address family
      ret = validate_address(ip_address, &ip_type);
     //printf("ip_type = [%d] \n", ip_type);
     if (ret == 1)
     {  
        sprintf(errbuf,"Unable to validate as IPv4 address");
        usage(errbuf);
        return 1;
     }
     else
     {
       convert_ipv4_to_numeric(ip_address , &num_ip);
     }

     if (is_display)
     {
       if (ip_type == 4)
         printf("%lu \n", num_ip);
       else {
         printf("[%s] ip address is not a valid address of IPv4 type  \n", ip_address);
         return 1;  
       }
     }
  }
  else if (ip_version_type == 2) { //this means ip address passes is an ipv6 address .
    
    int domain = AF_INET6;
    inet_check = inet_pton(domain, ip_address, buf);
    if (inet_check <= 0)
    {
      if (inet_check == 0){
        sprintf(errbuf,"IP address is not in valid format or not valid");
        usage(errbuf);
      }
      else{
        sprintf(errbuf,"inet_pton error, invalid IP family");
        usage(errbuf);
      }
      exit(1);
    }

    ret = validate_address(ip_address, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as a valid IPv6 address");
      usage(errbuf);
      return 1;
    }
    else
    {
      convert_ipv6_to_numeric(ip_address, &num_ip);
    }
        


    if (is_display)
    {
      if (ip_type == 6)
          printf("%lu \n", num_ip);
        //printf("%s \n", ip_address);
      else {
        printf("[%s] ip address is not a valid address of IPV6 type \n", ip_address);
        return 1;
      }
    }
  }
return 0;
}


/**********************************************************************************
This function seprates start_ip and endip and validates whether they belong to 
ipv4 or ipv6 type further  converts it in numeric form .
***********************************************************************************/  
int validate_ip_range (char *ip_address, int ip_version_type, int is_display, char *errbuf)
{
  char *sip;  //points to start ip 
  char *eip; // points to end ip 
  int ret ;
  unsigned long start_ip;
  unsigned long end_ip;
  int ip_type;

  //ip_address contains value of start and end ip we tokenize their value
  //and use these values furthure to validate them 

  sip = ip_address; 

  //evaluating end ip
 
  if ((eip =strchr(ip_address, '-')) != NULL)
  {
    *eip = '\0';
     eip = eip +1;
  }
  
  //validate sip and end_ip
  if ( ip_version_type == 1 ) {  
    ret = validate_address(sip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as ipv4 address");
      usage(errbuf);
      return 1;
    }
    else
    {
      convert_ipv4_to_numeric(sip , &start_ip);
    } 

    ret = validate_address(eip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as ipv4 address");
      usage(errbuf);
      return 1;
    }
    else
    {
      convert_ipv4_to_numeric(eip , &end_ip);
    }

    if (is_display)
    {
      if (ip_type == 4)
        printf("%lu %lu \n", start_ip , end_ip);
      else {
        printf("[%s] ip address is not a valid address of IPv4 type  \n", ip_address);
        return 1;
      }
    }
  }
  else if (ip_version_type == 2) {
    ret =  validate_address(sip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as ipv6 address");
      usage(errbuf);
      return 1;
    }
    else{
     convert_ipv6_to_numeric(sip, &start_ip);
    }

    ret = validate_address(eip, &ip_type);
    if (ret == 1)
    {
      sprintf(errbuf, "Unable to validate as ipv6 address");
      usage(errbuf);
      return 1;
    }
    else
    {
      convert_ipv6_to_numeric(eip , &end_ip);
    }

    if (is_display)
    {
      if (ip_type == 6)
         printf("%lu %lu \n", start_ip , end_ip);
        //printf("%s %s \n", sip , eip);
      else {
        printf("[%s] ip address is not a valid address of IPv6 type  \n", ip_address);
        return 1;
      }
    }
  }


return 0;
}


/**********************************************************************************
			Argument Parsing using getopt
***********************************************************************************/
int main(int argc, char *argv[])
{
  int opt = 0 ;  
  char *ip_address;
  int ip_version_type;
  int ret ;
  char errbuf[_MAX_ERR_BUF_SIZE_ + 1];

  if(argc < 4)
  {
    sprintf(errbuf, "Insufficient number of argument is present, please refer the usage\n");
    usage(errbuf);
    exit(1);
  }

  while ((opt = getopt (argc, argv, "v:i:I:r:R:x:X:n:N:")) != -1)
  {
    
    switch (opt)
    {
      case 'v':
        ip_version_type = atoi(optarg);
      break;
      
      case 'i':
        ip_address = optarg;
       //calling function to validate ip 
       ret = validate_ip(ip_address, ip_version_type, 0, errbuf);
       if (ret == 1)
         exit (1);
      break;

      case 'I':
        ip_address = optarg;
        //Calling function to validate ip 
        ret = validate_ip(ip_address, ip_version_type , 1, errbuf);
        if (ret == 1)
        exit (1);
      break;

      case 'n':
        ip_address = optarg;
        //Calling function to convert numeric ip 
        ret = validate_number(ip_address, ip_version_type, 0);
        if (ret == 1)
          exit(1);
        break;

     case 'N':
       ip_address = optarg;
       //Calling function to convert numeric ip 
       ret = validate_number(ip_address, ip_version_type, 1);
       if (ret == 1)
         exit(1);
       break;

     case 'x':
       ip_address = optarg;
       //Calling function to convert numeric ip 
       ret = check_ip_and_range(ip_address, ip_version_type, 0, errbuf);
       if (ret == 1)
         exit (1);
       break;
      
      case 'X':
        ip_address = optarg;
         //Calling function to ccheck ip range  
        ret = check_ip_and_range(ip_address, ip_version_type, 1, errbuf);
        if (ret == 1)
          exit (-1);
        break;
    
       case 'r':
         ip_address = optarg;
         //calling function to validate ip range  
         ret = validate_ip_range(ip_address, ip_version_type, 0, errbuf);
         if (ret == 1)
         {
           exit(1);
         }
       break;

       case 'R':
         ip_address = optarg;
         //calling function to validate ip range
         ret = validate_ip_range(ip_address, ip_version_type, 1, errbuf);
         if (ret == 1)
         {
           exit (1);
         } 
       break;

       case '?':
         usage("");
         break;

       default :
         usage("");
         break;
    }
  }
return 0; 
}
