
#ifndef __NS_MSG_COM_UTIL__
#define __NS_MSG_COM_UTIL__
#include <semaphore.h>
#include "ns_struct_con_type.h"

/* Return val */
/*These states are not been used hence discontinuing their usage*/
//#define NS_EAGAIN_RECEIVED	(1 << 0)
//#define NS_READING		(1 << 1)
//#define NS_WRITING		(1 << 2)

/* Status */
#define NS_STATE_READING	(1 << 0)
#define NS_STATE_WRITING	(1 << 1)
#define NS_STATE_DONE		(1 << 2)
#define NS_READING_DONE		(1 << 3)
#define NS_WRITING_DONE		(1 << 4)
#define NS_CONNECTING		  (1 << 5)
#define NS_CONNECTED	    (1 << 6)
#define NS_DATA_CONN_MADE   (1 << 7)


#define CONNECTION_TYPE_CHILD_OR_CLIENT         0
#define CONNECTION_TYPE_OTHER                   1
#define PARENT_OR_THREAD_BACKLOG                256
#define EVENT_LOG_BACKLOG                       10000
// #define MAX_MSG_SIZE    17708
// #define NS_MAX_CHILDS	1024

// Structure for Message communication connection for child, parent and master
struct Vuser;
typedef struct Msg_com_con
{
  char con_type;  // Connection type // Must be first field as we are using this after epoll wait to fine the type of struct and take action based on type
  int fd; 
  unsigned char state; /* Present state of the read/write */
  /* This flags is to log event if More samples are accumulated due to EAGAIN, must be initialized with 1*/ 
  unsigned char overflow_count;
  signed char type; /* 0 for child/client and 1 for other types eg. tools. */

  char *ip; // Ip address in Ver:IP.port format (e.g IPV4:192.168.18.1.12345)
  /* Read related */
  char *read_buf;
  int read_buf_size;
  int read_offset;
  int read_bytes_remaining;
  

  /* Write related */
  char *write_buf;
  int write_offset;
  int write_buf_size;
  int write_bytes_remaining;
  int nvm_index;   //Store the index of NVM as per v_port_table. 
                   //Set when NVM has send FINISH REPORT
  long total_bytes_sent;   // total bytes sent on this connection
  long total_bytes_recieved;  // total bytes received on this connection

  struct VUser *vptr; //For vptr of thread
  pthread_t thread_id; // Thread id
  int thread_nvm_fd; // fd form thread to NVM - Used by thread
  int nvm_thread_fd; // fd form NVM to thread - Used by NVM
  unsigned int flags; //Flag used in case of error or event occur in NS

/*run_thread will be used for CType Script in Thread mode 
  and prev will be used to traverse free list in Java Type Script*/
  union
  {
    sem_t run_thread; // flag for thread is executr script
    struct Msg_com_con *prev; // For make thread pool
    int32_t sgrp_id;          // In case of JMeter we need SGRP Group Id to show group wiase data
  };
  union
  {
    struct Msg_com_con *next; // For make thread pool
    void *jmeter_tx_id_mapping; //Using only for JMeter Tx record
  };
  char conn_owner[64]; //This defines connection owner or relationship of process which establish connection with peer
} Msg_com_con;

extern Msg_com_con *g_msg_com_con;
extern Msg_com_con *g_dh_msg_com_con;
extern int g_msg_com_epfd;
extern int v_epoll_fd;
extern Msg_com_con *g_master_msg_com_con;
extern Msg_com_con *g_dh_master_msg_com_con;

char * read_msg(Msg_com_con *pcon, int *sz, int thread_flag);
int write_msg(Msg_com_con *mccptr, char *buf, int size, int do_not_call_kill_all_children, int thread_flag);
void init_msg_con_struct(Msg_com_con *pcon, int fd, signed char type, char *ip, char conn_type);

int connect_to_parent();
int connect_to_parent_dh();
void accept_and_timeout(int num_connections);
extern void wait_for_child_registration_control_and_data_connection(int num_connections , int mode);
void accept_connection_from_tools();
void accept_connection_from_tools_dh();
int init_parent_listner_socket_new(int *lfd, unsigned short port);
int init_parent_listner_socket_new_v2(int *lfd, unsigned short port, int flag);
void close_parent_listen_fd();

int send_msg_to_all_clients(int opcode, int called_from_kill_all_childern);
void send_msg_to_master(int fd, int opcode, int th_flag);
void close_msg_com_con(Msg_com_con *pcon);
void close_msg_com_con_v2(char *file, int line, char * func, Msg_com_con *mccptr, int epfd);
void remove_select_msg_com_con_v2(char *file, int line, char * func, int fd, int epfd);
void mod_select_msg_com_con_v2(char *file, int line, char * func, char* data_ptr, int fd, int event, int epfd);
void add_select_msg_com_con_v2(char *file, int line, char * func, char* data_ptr, int fd, int event, int epfd);
void close_msg_com_con_and_exit(Msg_com_con *mccptr, char *function_name, int line_num, char *file_name);
char * msg_com_con_to_str(Msg_com_con *pcon);
void complete_leftover_write(Msg_com_con *mccptr, int th_flag);

inline void forward_msg_to_master(int fd, parent_msg *msg, int size);
inline void forward_dh_msg_to_master(int fd, parent_msg *msg, int size);
inline void forward_dh_msg_to_master_ex(int fd, parent_msg *msg, int size, int offset, char data_comp);
int read_gen_token(int fd, char *token, int token_size, int timeout);

extern inline void add_select_msg_com_con(char* data_ptr, int fd, int event);
extern inline void remove_select_msg_com_con(int fd);

int connect_to_master(void);
extern inline int connect_to_event_logger(char *ip, unsigned short port);
extern int connect_to_event_logger_ex(char *ip, unsigned short port, int mode);

extern Msg_com_con *g_msg_com_con_nvm0;
extern int epfd;
extern int *client_fds;
// extern int tcp_fd;
extern unsigned short parent_port_number;
extern unsigned short g_dh_listen_port; //added for DH thread
extern unsigned short event_logger_port_number;
extern unsigned short master_port;
extern unsigned short dh_master_port;
extern int listen_fd;
extern int data_handler_listen_fd;
extern int g_dh_msg_com_epfd;
extern int dh_master_fd;
extern char *read_msg_ex(Msg_com_con *mccptr, int *sz);
extern int get_gen_id_from_ip(char *gen_ip);
extern inline void mod_select_msg_com_con(char* data_ptr, int fd, int event);
extern int get_gen_id_from_ip_token(char *gen_ip, int fd);

#define NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN  0x00000001 // value 1
#define NS_MSG_COM_DO_NOT_CALL_EXIT               0x00000002 // value 2
#define NS_MSG_COM_CON_IS_CLOSED                  0x00000004 // value 4
#define NS_MSG_COM_CON_BUF_SIZE_EXCEEDED          0x00000008 // value 4

extern int connect_to_event_logger_nb(char *ip, unsigned short port);
extern void send_end_test_msg(char *msg, int status);
//Add to send END_TEST_RUN_ACK_MESSAGE
extern void send_end_test_ack_msg(int conn_mode);
extern inline void add_select_msg_com_con_ex(char *file, int line, char * func, char* data_ptr, int fd, int event);
extern inline void remove_select_msg_com_con_ex(char *file, int line, char * func, int fd);
extern inline void mod_select_msg_com_con_ex(char *file, int line, char * func, char* data_ptr, int fd, int event);
extern void check_if_need_to_realloc_connection_read_buf (Msg_com_con *mccptr, int nvm_id, int old_avg_size, int type);
extern void end_test_run_msg_to_client(Msg_com_con *mccptr);
extern void check_if_need_to_realloc_connection_read_buf (Msg_com_con *mccptr, int nvm_id, int old_avg_size, int type);
void accept_connection_v1(int conn_type); /*bug 92660*/
void accept_connection_from_child();
extern void add_conn_for_epoll(int fd, Msg_com_con *mccptr, int num_nvm_epfd, struct epoll_event *pfds, const struct sockaddr *their_addr, int mode);
extern void del_conn_for_epoll(int fd, int num_nvm_epfd, int mode);
extern int read_child_reg_msg(Msg_com_con *mccptr, const struct sockaddr *their_addr, int mode, int num_nvm_epfd);
#define CLOSE_MSG_COM_CON(mccptr, flag)     close_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, mccptr, flag);
#define REMOVE_SELECT_MSG_COM_CON(mccptr, flag)     remove_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, mccptr, flag);
#define MOD_SELECT_MSG_COM_CON(data_ptr, fd, event, flag) mod_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, data_ptr, fd, event, flag);
#define ADD_SELECT_MSG_COM_CON(data_ptr, fd, event, flag)     add_select_msg_com_con_v2(__FILE__, __LINE__, (char *)__FUNCTION__, data_ptr, fd, event, flag);
#define CLOSE_MSG_COM_CON_EXIT(mccptr, flag)\
{\
  CLOSE_MSG_COM_CON(mccptr, flag);\
  if (!(mccptr->flags & NS_MSG_COM_DO_NOT_CALL_KILL_ALL_CHILDREN) && flag == DATA_MODE)\
    kill_all_children(__FILE__, __LINE__, (char *)__FUNCTION__);\
  if (!(mccptr->flags & NS_MSG_COM_DO_NOT_CALL_EXIT))\
    NS_EXIT(-1, "Connection (%s) closed from other side. Connection flags = %X", msg_com_con_to_str(mccptr), mccptr->flags);\
}
#endif
