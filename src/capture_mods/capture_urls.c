#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COMMAND_LENGTH 10
#define SESSION_LENGTH 50

int main (int argc, char ** argv) {
  int num_arg = 1;
  char* pid_file;
  char* url_file;
  char* pid_buf = NULL;
  int moz_pid;
  char command[COMMAND_LENGTH];
  char session[SESSION_LENGTH];
  char session_input[SESSION_LENGTH];
  int capture_mode;
  char remove_buf[30];
  FILE* file_ptr;
  int buf_size;
  int url_file_fd;

  if (argc != 5) {
    printf("<usage> ./capture -pid <filename for Mozilla pid> -url <filename for urls to be captured to>\n");
    exit(0);
  }
    
  while(1) {
    if (strcmp(argv[num_arg], "-pid") == 0) {
      pid_file = argv[++num_arg];
      num_arg++;
      if (num_arg == argc)
	break;
      else
	continue;
    }
    if (strcmp(argv[num_arg], "-url") == 0) {
      url_file = argv[++num_arg];
      num_arg++;
      if (num_arg == argc)
	break;
      else
	continue;
    }
    printf("<usage> ./capture -pid <filename for Mozilla pid> -url <filename for urls to be captured to>\n");
    exit(0);
  }
  
  if (setenv("CAVISSON_PID_FILE", pid_file, 1)) {
    printf("capture_urls: error in setting the env variable CAVISSON_PID_FILE\n");
    exit(-1);
  }
  
  if (setenv("CAVISSON_URL_FILE", url_file, 1)) {
    printf("capture_urls: error in setting the env variable CAVISSON_URL_FILE\n");
    exit(-1);
  }

  if ((url_file_fd = open(url_file, O_TRUNC | O_RDWR | O_CREAT | O_APPEND, 0666))  == -1) {
    printf("capture_urls: error in opening the url dump file\n");
    exit(-1);
  }
  
  if (fcntl(url_file_fd, F_SETFD, 0) == -1) {
    printf("capture_urls: error in fcntl on url dump file\n");
    exit(-1);
  }

  printf("Please wait while I start Mozilla\n");

  //  if (system("./dist/bin/mozilla > /dev/null &") == -1) {
  if (system("./dist/Embed/run-mozilla.sh ./dist/Embed/TestGtkEmbed > /dev/null &") == -1) {
    printf("capture_urls: error in running Mozilla\n");
    exit(-1);
  }
  sleep(7);

  /*  while (1) {
    sleep(1);
    printf(".");
    if (++sleep_time == 10)
      break;
      }*/

  if ((file_ptr = fopen(pid_file, "r"))) {
    getline(&pid_buf, &buf_size, file_ptr);
    moz_pid = atoi(pid_buf);
    free(pid_buf);
  }

  capture_mode = 1;

  printf("enter in the name of the first session: ");
  memset(session_input, 0, SESSION_LENGTH);
  memset(session, 0, SESSION_LENGTH);
  scanf("%s", session_input);
  sprintf(session, "--Session <%s>\n", session_input);
  if (write(url_file_fd, session, strlen(session)) != strlen(session)) {
    printf("capture_urls: failed in writing to url file\n");
    exit(-1);
  }
  
  while (1) {
    memset(command, 0, COMMAND_LENGTH);
    printf("capture: ");
    scanf("%s", command);

    if (strcmp(command, "help") == 0) {
      printf("commands:\n");
      printf("capture_on - turn mozilla capture mode on\n");
      printf("capture_off - turn mozilla capture mode off\n");
      printf("capture_status - tells you whether the capture status is on or off\n");
      printf("quit - quit the program\n");
      continue;
    }

    if (strcmp(command, "capture_on") == 0) {
      if (capture_mode) {
	printf("capture mode is already on.  You must turn off your current capture session\n");
	continue;
      }
      printf("Enter the name of your new session: ");
      memset(session_input, 0, SESSION_LENGTH);
      memset(session, 0, SESSION_LENGTH);
      scanf("%s", session_input);
      sprintf(session, "--Session <%s>\n", session_input);
      if (write(url_file_fd, session, strlen(session)) != strlen(session)) {
	printf("capture_urls: failed in writing to url file\n");
	exit(-1);
      }
      
      if (kill(moz_pid, SIGHUP) == -1) {
	printf("error is sending signal to mozilla\n");
	exit (-1);
      }
      capture_mode = 1;
      continue;
    }

    if (strcmp(command, "capture_off") == 0) {
      if (capture_mode) {
	if (kill(moz_pid, SIGHUP) == -1) {
	  printf("error is sending signal to mozilla\n");
	  exit (-1);
	}
      }
      capture_mode = 0;
      continue;
    }

    if (strcmp(command, "capture_status") == 0) {
      if (capture_mode)
	printf("Capturing is on\n");
      else
	printf("Capturing is off\n");
      continue;
    }

    if (strcmp(command, "quit") == 0) {
      if (capture_mode) {
	if (kill(moz_pid, SIGHUP) == -1) {
	  printf("error is sending signal to mozilla\n");
	  exit (-1);
	}
	if (kill(moz_pid, SIGILL) == -1) {
	  printf("error is sending signal to mozilla\n");
	  exit (-1);
	}
      }
      break;
    }

    printf("unknown command.  Type 'help' for list of commands\n");
  }

  sprintf(remove_buf, "rm -f %s", pid_file);
  close(url_file_fd);
  system(remove_buf);

  return 0;
}
