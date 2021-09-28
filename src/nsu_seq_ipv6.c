/******************************************************************
 * Name    :    nsu_seq_ipv6.c
 * Author  :    Abhay
 * Purpose :    Generates sequence of ipv6 address from given start ip
******************************************************************/



#include <arpa/inet.h>
#include <stdio.h>
#include<string.h>
#include<stdlib.h>

static struct in6_addr ipv6,start_ip_bin,end_ip_bin;
static char buf[INET6_ADDRSTRLEN];
static char temp[5],ipv6_exp_addr[50];
static int  value[8],value_start_ip[8],value_end_ip[8];
static int i,j,start,total_ips;


int print_seq(char * start_ip,int total_ips )

{
  if (inet_pton(AF_INET6, start_ip, &ipv6)==1){
    for (i=0,j=0;i<16;i=i+2,j++)
    {
      sprintf(temp,"%02x%02x",ipv6.s6_addr[i],ipv6.s6_addr[i+1]);
      value[j]=strtol(temp,0,16);

    }
	//no_ips=atoi(argv[2]);
      
    for (start=0;start<=total_ips;start++)
    {
      strcpy(ipv6_exp_addr,"\0");
      for (i=0;i<8;i++)
      {
        sprintf(temp,"%04x:",value[i]);
        strcat(ipv6_exp_addr,temp);
      }
    
      ipv6_exp_addr[strlen(ipv6_exp_addr)-1]='\0';
      if (value[7]+1<=65535)
        value[7]+=1;
        else
        {value[7]=0;
         if (value[6]+1<=65535)
           value[6]+=1;
           else
           {value[6]=0;
            if (value[5]+1<=65535)
              value[5]+=1;
              else
              {value[5]=0;
               if (value[4]+1<=65535)
                  value[4]+=1;
                  else
                  {value[4]=0;
                   if (value[3]+1<=65535)
                     value[3]+=1;
                     else
                     {value[3]=0;
                       if (value[2]+1<=65535)
                          value[2]+=1;
                          else
                          {value[2]=0;
                           if (value[1]+1<=65535)
                             value[1]+=1;
                             else
                             {value[1]=0;
                              if (value[0]+1<=65535)
                                value[0]+=1;
                             } 
                          }
                      } 
                  } 
               } 
            } 
         }

      inet_pton(AF_INET6,ipv6_exp_addr, &ipv6);
      printf("%s\n",inet_ntop(AF_INET6,&ipv6, buf, sizeof buf));

    }
  }
  else
  {
    printf("Couldn't parse '%s' as an IPv6 address\n",start_ip);
  }

  return 0;
}

//function to calculate number of ips when start_ip and end_ip is given
//returns 0 on success and 1 on failure

int find_total_ips( char * start_ip, char * end_ip )
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
      printf("\nstart ip cannot be greator than end ip");
      return 1;
    }

  }

  total_ips=value_end_ip[7]-value_start_ip[7];    
	
  return 0;
}

int main(int argc,char * argv[])
{
  if (argc != 3)
  {  
    printf("Usage: %s <startip>  <endip>\n", argv[0]);
    return 1; 
  }
  
  if ( !find_total_ips(argv[1], argv[2]) )
  {
    print_seq(argv[1],total_ips);
    return 0;
  }
  else 
    return 1;

}

                           
