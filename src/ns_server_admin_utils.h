
// also in amf.h
#ifndef _LF_
#define  _LF_ __LINE__, (char *)__FUNCTION__
#endif

#ifndef __SERVER_ADMIN_UTIL_H__
#define __SERVER_ADMIN_UTIL_H__

#include <stdio.h>
#include <sys/epoll.h>
#include "v1/topolib_structures.h"

#define MAX_BUF_SIZE 1024
#define MAX_ONCE_WRITE 16384   //16 K
#define MAX_LINE_LEN 10000 
#define MAX_MSG_SIZE 5120
#define MAX_BUFFER_SIZE 256

//#define HEART_BEAT_CON_INIT            0
#define HEART_BEAT_CON_CONNECTED       1
#define HEART_BEAT_CON_CONNECTING      2
#define HEART_BEAT_CON_SENDING         3     
#define HEART_BEAT_CON_STOPPED         4
#define HEART_BEAT_CON_RUNTIME_INIT    5

#define HEART_BEAT_CON_SEND_MSG_BUF_SIZE 3 * 1024

#define HEART_BEAT_CON_NOT_REMOVE_FROM_EPOLL    0
#define HEART_BEAT_CON_REMOVE_FROM_EPOLL        1

#define SERVER_ADMIN_FAILURE -1
#define SERVER_ADMIN_SUCCESS  0
#define SERVER_ADMIN_CONDITIONAL_SUCCESS  1
#define SERVER_ADMIN_SECURE_CMON 2
#define SERVER_ADMIN_CONN_FAILURE -2

/*
typedef struct
{
  char con_type; // Must be first field as we are adding this into parent's epoll wait
  char *server_ip;
  char *user_name;
  char *password;
  char *java_home;
  char *install_dir;
  char *machine_type;    // Linux, LinuxEx, AIX_Shared_LPAR), AIX_Dedicated_LPAR, Solaris, Windows)
  char *cmon_settings;
  char is_ssh;
  char is_agent_less;
  char used;         // selected
  char *partial_buf;  
  char *origin_cmon; //Added for Heroku, this is the origin cmon server name or ip address as given by origin cmon to proxy cmon
*/
 /* Control connection fd from NS to CavMonAgent. This is made once and used for sending following messages:
  * test_run_starts:MON_TEST_RUN=1234;ProgressMsec=10000
  * test_run_running:MON_TEST_RUN=<TR Number>;ProgressMsec=<ProgressInteravalInMS>;SeqNum=<seq_num>
  * end_test_run:MON_TEST_RUN=<TR Number> */
/*  int control_fd;    
  int con_state;  // Here we are storing connection state for haert beat connection
  int bytes_remaining;
  int send_offset;
  int cntrl_conn_retry_attempts; //control connection (heart beat) retry attempts
  int is_cntrl_conn_b_nb;         //control connection is blocking(1) or non-blocking(0)
} ServerInfo; 
*/

typedef struct
{
  int fd;
  //char *partial_buffer;
  ServerInfo *server_index_ptr;
  int cmd_output_size;
  char *cmd_output;
} ServerCptr;

extern char *cmon_settings;
extern int search_server_in_topo_servers_list(char *tiername, char *server);
//extern FILE *open_file(char* file_name, char* mode);
//extern void fill_ServerInfo(char *fields[], ServerInfo *local_server);
extern int add_server_in_server_list(char *server_name,char *tiername,int topo_idx); 
//extern int fill_server_info_in_ignorecase(char *server);
//extern void read_server_file();
extern int find_tier_idx_and_server_idx(char *server, char *cs_ip, char *origin_cmon, char hv_separator, int hpd_port, int *topo_server_idx,int *tier_idx);
//extern void free_servers_list();

//Heart Beat
extern void handle_server_ctrl_conn_event(struct epoll_event *pfds, int i, void *ptr);

extern void send_testrun_starts_msg_to_all_cavmonserver_used();
extern void send_testrun_running_msg_to_all_cavmonserver_used(int seq_num);
extern void send_end_msg_to_all_cavmonserver_used();

extern char *get_ns_wdir();
extern unsigned short server_prt;
char encoded_install_dir[MAX_LINE_LEN + MAX_LINE_LEN +1];

//extern void debug_log(int type, int line, char *fname, char *format, ...);

extern int kw_set_enable_data_conn_hb(char *keyword, char *buf, char *err_msg, int runtime_flag);
extern void send_hb_on_data_conn();
extern int g_last_hb_send;
extern int g_count_to_send_hb;
extern int g_enable_mon_data_conn_hb;

extern inline void add_select_cntrl_conn();
#endif
