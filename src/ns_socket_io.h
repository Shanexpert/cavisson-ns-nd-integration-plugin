#ifndef NS_SOCKET_IO_H
#define NS_SOCKET_IO_H

#define  NS_SOCKET_IO_READ_SUCCESS     0
#define  NS_SOCKET_IO_READ_CONTINUE    1
#define  NS_SOCKET_IO_READ_ERROR       2

extern void handle_recv(connection *cptr, u_ns_ts_t now);

#endif
