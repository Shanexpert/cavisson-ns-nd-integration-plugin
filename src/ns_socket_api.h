/*
 * external file for socket api defns
 */
#ifndef NS_SOCKET_API_H
#define NS_SOCKET_API_H

enum {LOCAL_ADDRESS, LOCAL_HOSTNAME, LOCAL_PORT, REMOTE_ADDRESS, REMOTE_HOSTNAME, REMOTE_PORT};

#if RUN_TIME_PARSER
extern int ns_sock_create_socket (char *s_desc, char* type, ...);
extern int ns_sock_send (char *socket_desc, char* bufName,...);
extern int ns_sock_receive_ex (char *socket_desc, char* buf_name, ...);
extern int ns_sock_receive (char *socket_desc, char* buf_name, ...);
#else
extern int ns_sock_create_socket (char *s_desc, char* type, char* lh, char*rh, char*bl);
extern int ns_sock_send (char *socket_desc, char* bufName, char *targetHostName, char* flags) ;
extern int ns_sock_receive_ex (char *socket_desc, char* buf_name, char *recv_flags, char *recv_size, char *recv_terminator, char* recv_mismatch, char* recv_rec_size);
extern int ns_sock_receive (char *socket_desc, char* buf_name, char *recv_flags);
#endif

extern int ns_sock_close_socket (char *s_desc);
extern int ns_sock_accept_connection (char *oldSocket, char *newSocket );
extern char *ns_sock_get_socket_attrib (char *socket_desc , int attribute);
extern void ns_sock_set_accept_timeout (long seconds, long u_sec);
extern void ns_sock_set_connect_timeout (long seconds, long u_sec);
extern void ns_sock_set_recv_timeout (long seconds, long u_sec);
extern void ns_sock_set_recv2_timeout (long seconds, long u_sec);
extern void ns_sock_set_send_timeout (long seconds, long u_sec);

#endif  //NS_SOCKET_API_H
