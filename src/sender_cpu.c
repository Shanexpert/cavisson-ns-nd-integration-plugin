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
  FILE* cpufile;
  char read_buf[LINE_LENGTH];
  int send_fd;
  char send_buf[sizeof(int)];
  struct sockaddr_in svr_addr;
  int addrlen = sizeof(svr_addr);
  int send_cpu_busy;
  int old_uptime;
  int old_idletime;
  int uptime;
  int idletime;
  int window_uptime;
  int window_idletime;
  int window_busytime;
  float uptime_input;
  float idletime_input;

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

  if ((cpufile = fopen("/proc/uptime", "r")) == NULL) {
    fprintf(stderr, "could not open file /proc/loaduptime\n");
    exit(-1);
  }

  if (fgets(read_buf, LINE_LENGTH, cpufile) == NULL) {
    fprintf(stderr, "error reading from file /proc/loadavg\n");
    exit(-1);
  }

  sscanf(read_buf, "%f %f\n", &uptime_input, &idletime_input);
  old_uptime = uptime_input;
  old_idletime = idletime_input;
  
  fclose(cpufile);

  while (1) {
    sleep(60);

    if ((cpufile = fopen("/proc/uptime", "r")) == NULL) {
      fprintf(stderr, "could not open file /proc/loaduptime\n");
      exit(-1);
    }
    
    if (fgets(read_buf, LINE_LENGTH, cpufile) == NULL) {
      fprintf(stderr, "error reading from file /proc/loadavg\n");
      exit(-1);
    }
    
    sscanf(read_buf, "%f %f\n", &uptime_input, &idletime_input);
    uptime = uptime_input;
    idletime = idletime_input;

    window_uptime = uptime - old_uptime;
    window_idletime = idletime - old_idletime;
    window_busytime = window_uptime - window_idletime;

    send_cpu_busy = window_busytime * 100 / window_uptime;
    printf("sender_cpu: sending %d to server\n", send_cpu_busy);
    send_cpu_busy = htonl(send_cpu_busy);

    memcpy(send_buf, &send_cpu_busy, sizeof(int));
    if (sendto(send_fd, send_buf, BUF_SIZE, 0, (struct sockaddr *)&svr_addr, addrlen) <= 0) {
      fprintf(stderr, "error in sending data\n");
      exit(-1);
    };

    old_uptime = uptime;
    old_idletime = idletime;

    fclose(cpufile);
  }
}
