
#ifndef SOCK_H
#define SOCK_H
extern int tcp_listen(int local_port, int backlog, char *err_msg);
extern int tcp_listen_ex(int local_port, int backlog, char *ip_addr, char *err_msg);
extern int tcp_listen6(int local_port, int backlog);
extern int tcp_client(char *server_name, int default_port);
//extern int tcp_client_ex(char *server_name, int default_port, char *err_msg);
extern int tcp_client_ex(char *server_name, int default_port, int timeout_val, char *err_msg);
extern int udp_server (unsigned short port, int no_delay, char *);
extern int udp_server6 (unsigned short port, int no_delay, char *);
extern int udp_client(char *server_name, int default_port, int no_delay);
extern inline int Tcp_listen(int local_port, int backlog, char *err_msg);
extern inline int Tcp_listen_ex(int local_port, int backlog, char *ip_addr, char *err_msg);
extern inline int Tcp_listen6(int local_port, int backlog);
extern inline void Fcntl (int fd, int cmd, long arg);  //SS: Function not used
extern int ns_fill_sockaddr (struct sockaddr_in6 * saddr, char *server_name, int default_port);
extern int ns_fill_sockaddr_ex (struct sockaddr_in6 * saddr, char *server_name, int default_port, char *err_msg);
extern char * sock_ntop(const struct sockaddr *sa);
extern char * sockaddr_to_ip(const struct sockaddr *sa, int give_port);
extern unsigned int ns_get_sigfig_addr (struct sockaddr_in6 *saddr);
extern char * ns_split_host_port (char *svr, int *hport);
extern char* get_src_addr(int fd);
extern char* get_src_addr_ex(int fd, int give_port);
extern char* get_dest_addr(int fd);
extern char* get_dest_addr_ex(int fd, int give_port);
extern char *get_tcpinfo(int fd);
#define SIN6_LEN
#define SA  struct sockaddr
#endif
