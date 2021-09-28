#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include "ns_exit.h"

#define MAX_BUFFER_SIZE 16384
#define MAX_FRAME_SIZE 1600

#define RMI_REQUEST_URL 1
#define JBOSS_CONNECT_URL 2
#define RMI_CONNECT_URL 3
#define PING_ACK_URL 4
#define ANY_URL 5

FILE* capture_fptr;


char *get_type (unsigned char type)
{
  static char buf[64];
  buf[0] = 0;
  
  if (type & 0x01)
    strcat(buf,"F:");
  if (type & 0x02)
    strcat(buf,"S:");
  if (type & 0x04)
    strcat(buf,"R:");
  if (type & 0x08)
    strcat(buf,"P:");
  if (type & 0x10)
    strcat(buf,"A:");
  if (type & 0x20)
    strcat(buf,"U:");
  strcat(buf, "\0");
  return buf;
}


char* is_syn(char* buf) {
  return strchr(buf, 'S');
}


char* is_fin(char* buf) {
  return strchr(buf, 'F');
}


void handle_sig_int(int sig_num) {
  fclose(capture_fptr);
  NS_EXIT(0, "");
}


int get_request_line(unsigned char** buffer, unsigned char** recv_buffer, int* length) {
  while (*length) {
    **buffer = **recv_buffer;
    (*buffer)++;
    (*recv_buffer)++;
    (*length)--;
    if (*((*buffer)-1) == '\n') {
      **buffer = '\0';
      return 1;
    }
  }
  return 0;
}


int main(int argc, char** argv) {
  int fd;
  unsigned char buf[MAX_BUFFER_SIZE];
  unsigned char* ip_packet;
  unsigned char* tcp_packet;
  unsigned char* data;
  int amt_recv;
  unsigned short ip_header_length;
  char source[32], dest[32];
  struct in_addr source_ip;
  struct in_addr dest_ip;
  unsigned short tcp_header_length;
  unsigned short tcp_source_port;
  unsigned short tcp_dest_port;
  unsigned char tcp_type;
  struct in_addr server_ip;
  int data_length;
  char c;
  unsigned short port_num;
  int nbytes = -1;
  int reading_content = 0;
  unsigned char request_line[1024];
  unsigned char* request_line_ptr = request_line;
  int i;
  int new_page = 1;
  int page_num = 1;
  char* type_buf;
  unsigned int initial_sequence_num, next_expected_num, seq_num;
    
  if (argc != 7) {
    printf("Usage: ./ws_capture -c [Capture File] -s [Server IP Address] -p [Server Port Number]\n");
    exit(0);
  }

  while ((c = getopt(argc, argv, "c:p:s:")) != -1) {
    switch (c) {
    case 'c':
      if ((capture_fptr = fopen(optarg, "w+")) == NULL) {
	perror("fopen");
	NS_EXIT(-1, "Error in creating file %s\n", optarg);
      }
      break;
    case 's':
      if (inet_aton(optarg, &server_ip) == 0) {
	perror("inet_aton");
	NS_EXIT(-1,  "Error in the ip address %s\n", optarg);
      }
      break;
    case 'p':
      for (i =0; optarg[i]; i++) {
	if (!isdigit(optarg[i])) {
	  NS_EXIT(-1, "Port must be a number\n");
	}
      }
      port_num = atoi(optarg);
      break;
    case ':':
    case '?':
      printf("Usage: ./ws_capture -c [Capture File] -s [Server IP Address] -p [Server Port Number]\n");
      exit(0);
    }
  }

  if (optind != argc) {
    printf("Usage: ./ws_capture -c [Capture File] -s [Server IP Address] -p [Server Port Number]\n");
    exit(0);
  }

  signal(SIGINT, handle_sig_int);
  
  fd = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL));

  while (1) {
    amt_recv = recvfrom(fd, buf, MAX_BUFFER_SIZE, 0, NULL, NULL);
    assert(amt_recv <= MAX_FRAME_SIZE);

    // Filter out packets that are not of type IP
    if (!((buf[12] == (unsigned char)0x08) && (buf[13] == (unsigned char)0x00))) {
      continue;
    }

    ip_packet = buf + 14;

    // Filter out packets that are not of type TCP
    if (ip_packet[9] != 0x06)
      continue;

    ip_header_length = ip_packet[0];
    ip_header_length &= 0x000F;
    memcpy(&source_ip.s_addr, ip_packet + 12, 4);
    strcpy(source, inet_ntoa(source_ip));
    memcpy(&dest_ip.s_addr, ip_packet + 16, 4);
    strcpy(dest, inet_ntoa(dest_ip));

    // Filter out the user inputted server
    if (dest_ip.s_addr != server_ip.s_addr)
      continue;
    
    tcp_packet = ip_packet + (4*ip_header_length);
    memcpy(&tcp_source_port, tcp_packet, 2);
    tcp_source_port = ntohs(tcp_source_port);
    memcpy(&tcp_dest_port, tcp_packet + 2, 2);
    tcp_dest_port = ntohs(tcp_dest_port);

    // Filter out the user inputted port
    if (tcp_dest_port != port_num)
      continue;
    
    tcp_header_length = tcp_packet[12];
    tcp_header_length &= 0x00F0;
    tcp_header_length >>= 4;
    tcp_type = 0x3F & tcp_packet[13];

    type_buf = get_type(tcp_type);

    if (is_syn(type_buf)) {
      memcpy(&initial_sequence_num, tcp_packet+4, 4);
      initial_sequence_num = ntohl(initial_sequence_num);
      next_expected_num = initial_sequence_num + 1;
      continue;
    }

    memcpy(&seq_num, tcp_packet+4, 4);
    seq_num = ntohl(seq_num);

    if (seq_num != next_expected_num) {
      printf("the capture is erroneous, should run the capture program again\n");
      NS_EXIT(0, "");
    }

    if (is_fin(type_buf))
      next_expected_num ++;

    data = tcp_packet + (tcp_header_length*4);
    data_length = amt_recv - (data - buf);

    next_expected_num += data_length;

    while (data_length) {
      if (!reading_content) {
	if (get_request_line(&request_line_ptr, &data, &data_length)) {
	  if (new_page) {
   	    fprintf(capture_fptr, "--Page <Page %d>\n----\n", page_num++);
	    new_page = 0;
	  }
	  //	  printf("header - %s", request_line);
	  fputs(request_line, capture_fptr);
	  request_line_ptr = request_line;
	}
	else 
	  break;
	  
	if (!strncasecmp(request_line, "content-length:", 15)) {
	  nbytes = atol(&request_line[15]);
	  continue;
	}
	if (!strncasecmp(request_line, "\r\n", 2)) {
	  if (nbytes != -1)
	    reading_content = 1;
	  else {
	    fputs("----\n", capture_fptr);
	    new_page = 1;
	  }
	  continue;
	}	
      } else {
	if (data_length >= nbytes) {
	  fwrite(data, sizeof(char), nbytes, capture_fptr);
	  fputs("\n----\n", capture_fptr);
	  new_page = 1;
	  data += nbytes;
	  data_length -= nbytes;
	  reading_content = 0;
	  nbytes = -1;
	} else {
	  fwrite(data, sizeof(char), data_length, capture_fptr);
	  data_length = 0;
	  nbytes -= data_length;
	}
      }
    }
  }
}
