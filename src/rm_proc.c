
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/socket.h>
#include <linux/cavmodem.h>
#include <netinet/tcp.h>
#define TCP_BWEMU_MODEM 13

#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "ns_exit.h"
main(int argc, char *argv[])
{
int v_cavmodem_fd, i;
int max=20480;
int modem_id;


	if (argc > 1) 
		max = atoi(argv[1]);

	if ((v_cavmodem_fd = open("/dev/cavmodem", O_RDWR|O_CLOEXEC)) == -1) { 
		perror("cavmodem open");
		NS_EXIT(1, "netstorm: failed to open /dev/cavmodem");
        } 

	for (i = 0; i < max; i++) {
	  //		printf("Closing modem = %d\n", i);
		modem_id = i;
            	if (ioctl(v_cavmodem_fd, CAV_CLOSE_ALL_MODEMS, &(modem_id))) {
			perror("CAV_OPEN_MODEM ioctl");
			NS_EXIT(1, "netstorm: CAV_CLOSE_MODEM  %d  failed", i);
            	} 
	}
}
