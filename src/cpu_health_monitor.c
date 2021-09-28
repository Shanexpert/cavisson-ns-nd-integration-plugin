#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define LINE_LENGTH 512
#define BUF_SIZE 4

static void
handle_sigterm( int sig )
{
  exit(0);
}

int main(int argc, char** argv) {
  FILE* cpufile;
  FILE* runqfile;
  char read_buf[LINE_LENGTH];
  int send_fd;
  char send_buf[sizeof(int)];
  struct sockaddr_in svr_addr;
  int addrlen = sizeof(svr_addr);
  int cpu_busy;
  float rq_length_input;
  int rq_length;
  int old_uptime;
  int old_idletime;
  int uptime;
  int idletime;
  int window_uptime;
  int window_idletime;
  int window_busytime;
  float uptime_input;
  float idletime_input;
  int send_msg;
  int netstorm_pid;
  int i;

  if (argc != 3) {
    printf("usage: ./sender SERVER_IP_ADDRESS SERVER_PORT_NUMBER\n");
    exit(0);
  }

  netstorm_pid = getppid();
  signal(SIGINT, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGTERM, handle_sigterm);

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
    //Do parent check evevry 10 sec and run the main loop every minute
    for (i=0; i < 6; i++) {
	if ((kill (netstorm_pid, 0) == -1) && ((errno == ESRCH) || (errno == EPERM))) {
	    fprintf(stderr, "NetStorm master process got killed.\n");
	    exit (1);
	}
        sleep(10);
    }
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

    cpu_busy = window_busytime * 100 / window_uptime;
    
    if ((runqfile = fopen("/proc/loadavg", "r")) == NULL) {
      fprintf(stderr, "could not open file /proc/loadavg\n");
      exit(-1);
    }
    if (fgets(read_buf, LINE_LENGTH, runqfile) == NULL) {
      fprintf(stderr, "error reading from file /proc/loadavg\n");
      exit(-1);
    }
    sscanf(read_buf, "%f", &rq_length_input);
    rq_length = rq_length_input * 100;

    if (((rq_length > 75) && (cpu_busy > 95)) ||
	((rq_length > 100) && (cpu_busy > 75)))
      send_msg = 1;
    else
      send_msg = 0;
    
    printf("nsa_hmon: cpubusy is %d and runq_length is %d, sending %d to the server\n", cpu_busy, rq_length, send_msg);
    send_msg = htonl(send_msg);
    memcpy(send_buf, &send_msg, sizeof(int));
    if (sendto(send_fd, send_buf, BUF_SIZE, 0, (struct sockaddr *)&svr_addr, addrlen) <= 0) {
      fprintf(stderr, "error in sending data\n");
      exit(-1);
    };

    fclose(cpufile);
    fclose(runqfile);
  }
}

    
