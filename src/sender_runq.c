#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define LINE_LENGTH 512
#define BUF_SIZE 4

int main(int argc, char** argv) {
  FILE* runqfile;
  char read_buf[LINE_LENGTH];
  float rq_length;
  int send_rq_length;
  int send_fd;
  char send_buf[sizeof(int)];
  struct sockaddr_in svr_addr;
  int addrlen = sizeof(svr_addr);

  if (argc != 3) {
    printf("usage: ./sender SERVER_IP_ADDRESS SERVER_PORT_NUMBER\n");
    exit(0);
  }

  if ((send_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "error in creating socket\n");
    close(send_fd);
    exit(-1);
  }

  memset(&svr_addr, 0, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = inet_addr(argv[1]);
  svr_addr.sin_port = htons(atoi(argv[2]));

  while (1) {
    sleep(60);
    if ((runqfile = fopen("/proc/loadavg", "r")) == NULL) {
      fprintf(stderr, "could not open file /proc/loadavg\n");
      exit(-1);
    }
    if (fgets(read_buf, LINE_LENGTH, runqfile) == NULL) {
      fprintf(stderr, "error reading from file /proc/loadavg\n");
      exit(-1);
    }
    sscanf(read_buf, "%f", &rq_length);
    send_rq_length = rq_length * 100;
    printf("sender_runq: sending %d to the server\n", send_rq_length);
    send_rq_length = htonl(send_rq_length);
    memcpy(send_buf, &send_rq_length, sizeof(int));
    if (sendto(send_fd, send_buf, BUF_SIZE, 0, (struct sockaddr *)&svr_addr, addrlen) <= 0) {
      fprintf(stderr, "error in sending data\n");
      exit(-1);
    };
    fclose(runqfile);
  }
}
    
    
