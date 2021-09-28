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
#include "ns_exit.h"
#define MAX_BUFFER_SIZE 16384
#define MAX_FRAME_SIZE 1600

#define RMI_REQUEST_URL 1
#define JBOSS_CONNECT_URL 2
#define RMI_CONNECT_URL 3
#define PING_ACK_URL 4
#define ANY_URL 5

FILE* capture_fptr;
int resp_length = 0;

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
  return buf;
}

void handle_sig_int(int sig_num) {
  if (resp_length) {
    fprintf(capture_fptr, "---- RCV_BYTES:1,%d\n", resp_length);
    resp_length = 0;
  } else
    fprintf(capture_fptr, "----\n");
  fclose(capture_fptr);
  exit(0);
}

void parse_data(unsigned char* data, int data_length) {
  int i;
  for (i=0; i < data_length; i++) {
    if ((i+1)%16)
      fprintf(capture_fptr, "%02x ", data[i]);
    else
      fprintf(capture_fptr, "%02x\n", data[i]);
  }
  fprintf(capture_fptr, "\n");
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
  char url_type = ANY_URL;
  unsigned short svr_rmi_connect_port= 0;
    
  if (argc != 5) {
    printf("Usage: ./rmi_capture -c [Capture File] -s [Server IP Address]\n");
    exit(0);
  }

  while ((c = getopt(argc, argv, "c:s:")) != -1) {
    switch (c) {
    case 'c':
      if ((capture_fptr = fopen(optarg, "w+")) == NULL) {
	NS_EXIT(-1, "Error in creating file %s", optarg);
	perror("fopen");
      }
      break;
    case 's':
      if (inet_aton(optarg, &server_ip) == 0) {
	NS_EXIT(-1, "Error in the ip address %s", optarg);
	perror("inet_aton");
      }
      break;
    case ':':
    case '?':
      printf("Usage: ./rmi_capture -c [Capture File] -s [Server IP Address]\n");
      exit(0);
    }
  }

  if (optind != argc) {
    printf("Usage: ./rmi_capture -c [Capture File] -s [Server IP Address]\n");
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

    // Filter out the user inputed filter    
    if ((source_ip.s_addr != server_ip.s_addr) && (dest_ip.s_addr != server_ip.s_addr))
      continue;

    tcp_packet = ip_packet + (4*ip_header_length);
    memcpy(&tcp_source_port, tcp_packet, 2);
    tcp_source_port = ntohs(tcp_source_port);
    memcpy(&tcp_dest_port, tcp_packet + 2, 2);
    tcp_dest_port = ntohs(tcp_dest_port);
    tcp_header_length = tcp_packet[12];
    tcp_header_length &= 0x00F0;
    tcp_header_length >>= 4;
    tcp_type = 0x3F & tcp_packet[13];

    data = tcp_packet + (tcp_header_length*4);
    data_length = amt_recv - (data - buf);

    printf("recieved message: tcp_type:%02x, source:%s:%hd, dest:%s:%hd\n", tcp_type, source, tcp_source_port, dest, tcp_dest_port);

    switch (url_type) {
    case ANY_URL:
      // FOR THE JBOSS_CONNECT URL
      if ((tcp_type == 0x02) && (server_ip.s_addr == dest_ip.s_addr) && (tcp_dest_port == 1099)) {
	if (resp_length) {
	  fprintf(capture_fptr, "---- RCV_BYTES:1,%d\n", resp_length);
	  resp_length = 0;
	}
	fprintf(capture_fptr, "---- JBOSS_CONNECT\n");
	fprintf(capture_fptr, "HOST: %s:%hd\n", inet_ntoa(server_ip), tcp_dest_port);
	fprintf(capture_fptr, "----\n");
	url_type = JBOSS_CONNECT_URL;
	continue;
      }
      // FOR THE RMI_CONNECT URL
      if ((tcp_type == 0x02) && (server_ip.s_addr == dest_ip.s_addr) && (tcp_dest_port != 1099)) {
	if (resp_length) {
	  fprintf(capture_fptr, "---- RCV_BYTES:1,%d\n", resp_length);
	  resp_length = 0;
	}
	fprintf(capture_fptr, "---- RMI_CONNECT\n");
	if (tcp_dest_port == 4444)
	  fprintf(capture_fptr, "HOST: %s:%hd\n", inet_ntoa(server_ip), tcp_dest_port);
	else
	  fprintf(capture_fptr, "PREVIOUS_HOST\n");
	fprintf(capture_fptr, "----\n");
	svr_rmi_connect_port = tcp_dest_port;
	url_type = RMI_CONNECT_URL;
	continue;
      }
      // FOR THE RMI_REQUEST URL
      if ((tcp_type == 0x18) && (server_ip.s_addr == dest_ip.s_addr) && (data[0] == 0x50)) {
	if (resp_length) {
	  fprintf(capture_fptr, "---- RCV_BYTES:1,%d\n", resp_length);
	  resp_length = 0;
	}
	fprintf(capture_fptr, "---- RMI_REQUEST\n");
	parse_data(data, data_length);
	resp_length = 0;
	url_type = RMI_REQUEST_URL;
	continue;
      }
      // FOR THE RMI_REQUEST URL
      if ((tcp_type == 0x18) && (server_ip.s_addr == source_ip.s_addr)) {
	resp_length += data_length; 
	continue;
      }
      //FOR THE PING_ACK URL
      if ((tcp_type == 0x18) && (server_ip.s_addr == dest_ip.s_addr) && (data[0] == 0x52)) {
	if (resp_length) {
	  fprintf(capture_fptr, "---- RCV_BYTES:1,%d\n", resp_length);
	  resp_length = 0;
	}
	fprintf(capture_fptr, "---- PING_ACK_REQUEST\n");
	fprintf(capture_fptr, "----\n");
	url_type = PING_ACK_URL;
      }
      // all other packets
      break;
    case JBOSS_CONNECT_URL:
      if ((tcp_type == 0x12) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == 1099))
	continue;
      if ((tcp_type == 0x10) && (server_ip.s_addr == dest_ip.s_addr) && (tcp_dest_port == 1099))
	continue;
      if ((tcp_type == 0x18) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == 1099))
	continue;
      if ((tcp_type == 0x11) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == 1099)) {
	url_type = ANY_URL;
	continue;
      }
      if ((tcp_type == 0x19) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == 1099)) {
	url_type = ANY_URL;
	continue;
      }
      NS_EXIT(-1, "recieved invalid message in state JBOSS_CONNECT_URL, tcp_type:%02x, source:%s:%hd, dest:%s:%hd", tcp_type, source, tcp_source_port, dest, tcp_dest_port);
    case RMI_CONNECT_URL:
      if ((tcp_type == 0x12) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == svr_rmi_connect_port))
	continue;
      if (tcp_type == 0x10)
	continue;
      if ((tcp_type == 0x18) && (server_ip.s_addr == dest_ip.s_addr) && (tcp_dest_port == svr_rmi_connect_port) && (data_length == 7) &&
	  (data[0] == 0x4a) && (data[1] == 0x52) && (data[2] == 0x4d) && (data[3] == 0x49))
	continue;
      if ((tcp_type == 0x18) && (server_ip.s_addr == source_ip.s_addr) && (tcp_source_port == svr_rmi_connect_port) && (data[0] == 0x4e)) {
	svr_rmi_connect_port = 0;
	url_type = ANY_URL;
	continue;
      }
      NS_EXIT(-1, "recieved invalid message in state RMI_CONNECT_URL, tcp_type:%02x, source:%s:%hd, dest:%s:%hd, data[0]:%02x", tcp_type, source, tcp_source_port, dest, tcp_dest_port, data[0]);
    case RMI_REQUEST_URL:
      if (tcp_type == 0x10)
	continue;
      if ((tcp_type == 0x18) && (server_ip.s_addr == dest_ip.s_addr)) {
	parse_data(data, data_length);
	continue;
      }
      if ((tcp_type == 0x18) && (server_ip.s_addr == source_ip.s_addr)) {
	resp_length += data_length; 
	url_type = ANY_URL;
	continue;
      }
      NS_EXIT(-1, "recieved invalid message in state RMI_CONNECT_URL");
    case PING_ACK_URL:
      if ((tcp_type == 0x18) && (server_ip.s_addr == source_ip.s_addr) && (data[0] == 0x53)) {
	url_type = ANY_URL;
	continue;
      }
      NS_EXIT(-1, "recieved invalid message in state RMI_CONNECT_URL");
    }
    //		printf("eth_type is %hd, ip_header_length: %hd, ip_type: %hd, ip_source_addr: %s, ip_dest_addr: %s, tcp_type: %s, tcp_header_length: %hd, tcp_source_port: %hd, tcp_dest_port: %hd\n", eth_type, ip_header_length, ip_type, src, dst, get_type(tcp_type), tcp_header_length, tcp_source_port, tcp_dest_port);
  }
}
