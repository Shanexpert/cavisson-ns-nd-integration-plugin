#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

static struct in6_addr ipv6,start_ip_bin,end_ip_bin;
static char buf[INET6_ADDRSTRLEN];
static char temp[5],ipv6_exp_addr[50];
static int  value[8],value_start_ip[8],value_end_ip[8];
static int i,j,start,total_ips;



void print_usage()
{
  printf("Usage: One of the following...\n"); 
  printf(" IP Version type can be 1 for ipv4 and 2 for ipv6 \n"); 
  printf("To list IP's from an start to end IP         : nsu_seq_ip ip_version_type  start_ip end_ip\n");  
  printf("To list some number of IP's from an start ip : nsu_seq_ip ip_version_type start_ip num_ip\n");  
  printf("To list end_ip given start_ip & num_ip       : nsu_seq_ip -v ip_version_type -e start_ip num_ip\n");  
  printf("To list start_ip given end_ip & num_ip       : nsu_seq_ip -v ip_version_type -s end_ip num_ip\n");  
  printf("To list Nth forward IP for start_ip and N    : nsu_seq_ip -v ip_version_type -f start_ip num_ip\n");  
  printf("To list Nth backward IP for start_ip and N   : nsu_seq_ip -v ip_version_type -b start_ip num_ip\n");  
  printf("To get the number of IP's in a range         : nsu_seq_ip -v ip_version_type -n start_ip end_ip\n");  
}


int print_seq(char * start_ip,int total_ips )
{
  if (inet_pton(AF_INET6, start_ip, &ipv6)==1){
    for (i=0,j=0;i<16;i=i+2,j++)
    {
      sprintf(temp,"%02x%02x",ipv6.s6_addr[i],ipv6.s6_addr[i+1]);
      value[j]=strtol(temp,0,16);

    }
	//no_ips=atoi(argv[2]);
      
    for (start=0;start<=total_ips -1 ;start++)
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
   char network_id[256] , end_network_id[256] ;
  if (inet_pton(AF_INET6,start_ip,&start_ip_bin)!=1){
    printf("Couldn't parse '%s' as an IPv6 address\n",start_ip);
    return 1;
  }

  if (inet_pton(AF_INET6,end_ip,&end_ip_bin)!=1){
    printf("Couldn't parse '%s' as an IPv6 address\n",end_ip);
    return 1;
  }

  //extrating network id 
  sprintf(network_id,"%02x%02x",start_ip_bin.s6_addr[0],start_ip_bin.s6_addr[1]);
  sprintf(end_network_id, "%02x%02x",end_ip_bin.s6_addr[0],end_ip_bin.s6_addr[1]);

  
  if (strcmp(network_id,end_network_id))
  {
    printf("network id mismatch \n");
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
      printf("\nStart IP cannot be greater than End IP \n");
      return 1;
    }

 
  }

  total_ips=value_end_ip[7]-value_start_ip[7];    
  total_ips = total_ips+1; //Need to count the start ip also	
  return 0;
}


unsigned int get_cmd_value(char *cmd)
{
  FILE *app = NULL;
  char buffer[128] = "\0";
  unsigned int value = 0;
  app = popen(cmd, "r");
  if(app == NULL)
  {
    perror("Error: Error in running command !");
    exit(-1);
  }
  fgets(buffer, 128, app);
  pclose(app);
  sscanf(buffer, "%u", &value); 
  return(value);
}

void print_ip_dot_format(unsigned int ip)
{

  int  xpart,tpart,ypart,zpart,wpart;
  xpart = ip / 16777216;
  tpart = ip % 16777216;
  ypart = tpart / 65536;
  tpart = tpart % 65536;
  zpart = tpart / 256;
  wpart = tpart % 256;

  printf("%d.%d.%d.%d\n", xpart, ypart, zpart, wpart);
}


void list_dest_ip(unsigned int strt_ip, int num_ip)
{
  unsigned int ip = strt_ip + num_ip ;
  print_ip_dot_format(ip);
}

void list_num_ip(unsigned long strt_ip, unsigned long end_ip)
{
  unsigned long ip = end_ip - strt_ip + 1 ;
  printf("%lu\n", ip);
}

void list_ip_till_end_ip(unsigned int strt_ip, unsigned int end_ip)
{
  unsigned int ip = strt_ip;
  while(ip <= end_ip) 
  {
    print_ip_dot_format(ip);
    ip++;
  }

}

void list_ip_till_num_ip(unsigned int strt_ip, int num_ip)
{
  int i = 0;
  unsigned int ip = strt_ip;
  while(i < num_ip)
  {
    print_ip_dot_format(ip);
    i++;
    ip++;
  }
}


int main(int argc, char *argv[])
{
  char check_format[] = "/home/cavisson/work/bin/nsi_check_format";
  char cmd[256] = "\0";
  unsigned long  strt_ip = 0;
  unsigned long end_ip = 0;
  unsigned int num_ip;
  int opt;
  int ip_version_type; 
  char *ip_address;
  int ip_num;
  int arg_passed = 0;

  if(argc < 4 )
  {
    print_usage();
    return 1;
  }

  //Parse command arguments

  while ((opt = getopt (argc, argv, "v:e:s:f:b:n:")) != -1)
  {
    arg_passed ++;
    switch(opt)
    {
      //checking ip version type 
      case 'v' :
        ip_version_type = atoi(optarg);
      break;

      case 'e' :
         
         //this option will list end_ip provided start_ip & num_ip 
         ip_address = optarg;
         sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address);
         //extrating numeric value of start ip here
         
         strt_ip = get_cmd_value(cmd);
         ip_num = atoi(argv[optind]);
         
         //getting number of ip here 
         sprintf(cmd, "%s -v %d -N %d", check_format, ip_version_type, ip_num); 
         num_ip = get_cmd_value(cmd) -1;
         //list end ip using start_ip and number_of_ip(s)
         list_dest_ip(strt_ip, num_ip);
      break;

      case 's' :
       
        //with this option we can list start_ip given end_ip & num_ip
        ip_address = optarg;
        sprintf(cmd, "%s -v %d -I %s",check_format, ip_version_type, ip_address);
        strt_ip = get_cmd_value(cmd);
        //extract number of ips
        ip_num = atoi(argv[optind]); 
        sprintf(cmd, "%s -v %d -N %d", check_format, ip_version_type, ip_num);
        num_ip = 1- get_cmd_value(cmd);
        list_dest_ip(strt_ip, num_ip);

      break;

      case 'f' :

        //with this option we can list next nth ip given start ip and n
        ip_address = optarg; 
        sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address);
        strt_ip = get_cmd_value(cmd);
        ip_num = atoi(argv[optind]);
        sprintf(cmd, "%s -v %d -N %d", check_format, ip_version_type, ip_num);
        num_ip = get_cmd_value(cmd);
        list_dest_ip(strt_ip,  num_ip);
      break;

      case 'b' :

        //option will list list nth backward IP given start_ip and N
        ip_address = optarg;
        sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address);
        strt_ip = get_cmd_value(cmd);
        ip_num = atoi(argv[optind]);
        sprintf(cmd, "%s -v %d -N %d", check_format, ip_version_type, ip_num);
        num_ip = 0 - get_cmd_value(cmd);
        list_dest_ip(strt_ip,  num_ip);
      break;

      case 'n' :
        
        //option get the number of IP's in a range
        ip_address = optarg;
        sprintf(cmd, "%s -v %d -I  %s", check_format, ip_version_type, ip_address);
        strt_ip = get_cmd_value(cmd);
        ip_address = argv[optind];
        sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address );
        end_ip  = get_cmd_value(cmd);
        list_num_ip(strt_ip, end_ip);
      break;
 
    }
  }

  if (arg_passed == 0)
  {
   
    ip_version_type = atoi(argv[1]);
    if (ip_version_type == 1)
    { 
      ip_address = argv[2] ;
      sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address);
      strt_ip = get_cmd_value(cmd);
      ip_address = argv[3];
      //Bug 17045
      if(strchr(ip_address,'.'))
      { 	
        sprintf(cmd, "%s -v %d -I %s", check_format, ip_version_type, ip_address);
        end_ip  = get_cmd_value(cmd);
        if (end_ip)
        {
          list_ip_till_end_ip(strt_ip, end_ip);
        }
      }
      else 
      {  
        num_ip = atoi(argv[3]);
        sprintf(cmd, "%s -v %d -N %d", check_format, ip_version_type, num_ip);
        num_ip = get_cmd_value(cmd);
        if (num_ip) {
          list_ip_till_num_ip(strt_ip, num_ip);
        }
      }
    }
    else if (ip_version_type == 2)
    {
      if (((ip_address = strstr(argv[3] ,"::")) != NULL) || ((ip_address = strstr(argv[3], ":")) != NULL)) {
        if (!find_total_ips(argv[2], argv[3]) )
          print_seq(argv[2],total_ips);
      }
      else
        print_seq(argv[2],atoi(argv[3]));
    }
  }
  return 0;
}
