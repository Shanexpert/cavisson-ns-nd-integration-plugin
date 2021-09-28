#ifndef _NS_DOS_SYN_ATTACK_H_ 
#define _NS_DOS_SYN_ATTACK_H_

#include <stdio.h>
#include <stdlib.h>
#include <netinet/tcp.h>   //Provides declarations for tcp header
#include <netinet/ip.h>    //Provides declarations for ip header
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>        //For getopt


//#include "../ns_log.h"
//#include "../ns_string.h"
//#include "../ns_event_id.h"
//#include "../ns_event_log.h"
//#include "../netstorm.h"


#define MAX_PACKET_SIZE 4096

//VUser *vptr;

//Needed for checksum calculation
typedef struct pseudo_header {   
  unsigned int source_address;
  unsigned int dest_address;
  unsigned char placeholder;
  unsigned char protocol;
  unsigned short tcp_length;
  //char tcp[28];
  struct tcphdr tcp;
}sudo_hdr;

extern void ns_dos_syn_attack_ext(VUser *vptr, char *source_ip_add, char *dest_ip_add, unsigned short dest_port, int num_attack);

#endif
