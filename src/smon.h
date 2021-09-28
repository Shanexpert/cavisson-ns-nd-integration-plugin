#ifndef _smon_h
#define _smon_h

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h> /* not needed on IRIX */
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    unsigned long long cum_sum;
    unsigned long long num_samples;
    unsigned int min;
    unsigned int max;
    unsigned int avg;
    unsigned int offset; //into data_point
} SummaryMinMax;

extern int total_tunnels;

extern void monitor_config(char *keyword, char *buf);
extern void smonitor_config(char *keyword, char *buf);
extern void monitor_setup(int frequency);
extern inline int set_monitor_fd (fd_set *rfd, int max_fd);
extern int handle_if_monitor_fd(void *ptr);
extern void print_if_monitor_data(FILE* fp1, FILE* fp2);
extern void fill_if_monitor_data();
extern void unset_monitor_fd();
extern void print_server_stat(FILE * fp1, FILE* fp2, int num_servers);
extern int do_summary_min_max (int nelements, SummaryMinMax *data);
extern void add_monitor(char *mname, char *host, char *pgm_path, int send_data_point_idx, char *cmd_args);
extern int is_no_linux_present();
extern int is_no_tcp_present();
extern char *get_tunnels();
extern int get_mon_count(char *mname, int *mon_id);
extern char *get_mon_server_name(int mon_id, int *index);
extern inline void stop_all_monitors();
extern void add_select_monitor();
#endif
